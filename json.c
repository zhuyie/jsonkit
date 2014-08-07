/*
 jsonkit ( https://github.com/zhuyie/jsonkit )

 Copyright (c) 2014, zhuyie
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 * Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/*----------------------------------------------------------------------------*/

struct json_value {
    json_alloc_func alloc_func;
    unsigned char type;
};

typedef struct json_string {
    json_alloc_func alloc_func;
    unsigned char type;
    /* We provide a initial string to alloc the json_string. after that, we can 
       modify the json_string but rarely. Trailing mode can optimize this case. 
       In trailing mode, we only alloc 1 memory block, both for the json_string struct and the string data. */
    unsigned char trailing;
    unsigned short trailing_extra_cb;
    unsigned int len;
    union {
        struct {
            char str[1];
        } trailing_str;
        struct {
            char *ptr;
            unsigned int capacity;
        } str;
    };
} json_string;

typedef struct json_number {
    json_alloc_func alloc_func;
    unsigned char type;
    double dbl;
} json_number;

typedef struct _json_object_item {
    int sorted_index;
    unsigned int name_len;
    unsigned int name_hash;
    const char *name_str;
    json_value *value;
} _json_object_item;

typedef struct json_object {
    json_alloc_func alloc_func;
    unsigned char type;
    int capacity;
    _json_object_item *items;
    int size;
} json_object;

typedef struct json_array {
    json_alloc_func alloc_func;
    unsigned char type;
    unsigned int capacity;
    json_value **values;
    unsigned int size;
} json_array;

static void* _default_alloc_func(
    void *ptr, 
    size_t osize, 
    size_t nsize
    );
static double _NaN();
static unsigned int _new_capacity(
    unsigned int capacity
    );
static int _object_item_init(
    const char *name_str, 
    unsigned int name_len, 
    unsigned int name_hash,
    json_value *value,
    _json_object_item *item,
    json_alloc_func alloc_func
    );
static void _object_item_cleanup(
    _json_object_item *item,
    json_alloc_func alloc_func
    );
static int _object_item_index_lower_bound(
    _json_object_item *items,
    int count,
    unsigned int hash
    );
static void _hash_string(
    const char *str, 
    unsigned int *len, 
    unsigned int *hash
    );
static int _name_to_index(
    json_object *object, 
    const char *name, 
    unsigned int len, 
    unsigned int hash,
    int *lower_bound
    );
static unsigned int _str_to_index(
    const char *str, 
    unsigned int len
    );

/*----------------------------------------------------------------------------*/

json_value_type json_type(json_value *v)
{
    assert(v);
    return v->type;
}

json_alloc_func json_get_alloc_func(json_value *v)
{
    assert(v);
    return v->alloc_func;
}

/*----------------------------------------------------------------------------*/

/* count of bytes inside the json_string structure which can be used by trailing mode */
const static unsigned int _json_string_builtin_string_cb = 
    (unsigned int)(sizeof(json_string) - offsetof(json_string, trailing_str));

json_value* json_string_alloc(const char *str, unsigned int len, json_alloc_func alloc_func)
{
    json_string *string;
    unsigned int extra_cb;

    assert(str);
    
    if (!alloc_func)
        alloc_func = _default_alloc_func;

    if (len == (unsigned int)-1)
        len = (unsigned int)strlen(str);
    if (len == UINT_MAX)
        return NULL;

    if (len < USHRT_MAX) {
        /* trailing mode */
        if (len < _json_string_builtin_string_cb)
            extra_cb = 0;
        else
            extra_cb = len - _json_string_builtin_string_cb + 1;

        string = (json_string*)alloc_func(NULL, 0, sizeof(json_string) + extra_cb);
        if (!string)
            return NULL;

        memcpy(string->trailing_str.str, str, len);
        string->trailing_str.str[len] = '\0';

        string->alloc_func = alloc_func;
        string->type = json_type_string;
        string->trailing = 1;
        string->trailing_extra_cb = extra_cb;
        string->len = len;

    } else {
        /* normal mode */
        string = (json_string*)alloc_func(NULL, 0, sizeof(json_string));
        if (!string)
            return NULL;

        string->str.ptr = alloc_func(NULL, 0, len + 1);
        if (!string->str.ptr) {
            alloc_func(string, sizeof(json_string), 0);
            return NULL;
        }
        memcpy(string->str.ptr, str, len);
        string->str.ptr[len] = '\0';

        string->alloc_func = alloc_func;
        string->type = json_type_string;
        string->trailing = 0;
        string->trailing_extra_cb = 0;
        string->len = len;
        string->str.capacity = len;
    }

    return (json_value*)string;
}

const char* json_string_get(json_value *v)
{
    json_string *string = (json_string*)v;
    assert(string);

    if (v->type != json_type_string)
        return NULL;

    return string->trailing ? string->trailing_str.str : string->str.ptr;
}

json_value* json_string_set(json_value *v, const char *str, unsigned int len)
{
    json_string *string = (json_string*)v;
    unsigned int capacity;
    char *ptr;
    size_t osize;

    assert(v);
    assert(str);

    if (v->type != json_type_string)
        return NULL;

    if (len == (unsigned int)-1)
        len = (unsigned int)strlen(str);
    if (len == UINT_MAX)
        return NULL;

    /* check if we have enough space to hold the new string */
    if (string->trailing) {
        capacity = _json_string_builtin_string_cb + string->trailing_extra_cb - 1;
    } else {
        capacity = string->str.capacity;
    }
    if (len > capacity) {
        if (string->trailing) {
            ptr = NULL;
            osize = 0;
        } else {
            ptr = string->str.ptr;
            osize = capacity + 1;
        }
        ptr = string->alloc_func(ptr, osize, len + 1);  /* realloc */
        if (!ptr)
            return NULL;
        
        string->trailing = 0;
        string->str.ptr = ptr;
        string->str.capacity = len;
    }

    ptr = string->trailing ? string->trailing_str.str : string->str.ptr;
    memcpy(ptr, str, len);
    ptr[len] = '\0';
    string->len = len;

    return v;
}

unsigned int json_string_len(json_value *v)
{
    json_string *string = (json_string*)v;
    assert(string);

    if (v->type != json_type_string)
        return (unsigned int)-1;

    return string->len;
}

json_value* json_string_resize(json_value *v, unsigned int len, char ch)
{
    json_string *string = (json_string*)v;
    unsigned int capacity;
    char *ptr;
    size_t osize;
    unsigned int i;

    assert(v);

    if (v->type != json_type_string)
        return NULL;
    if (len == UINT_MAX)
        return NULL;
    
    /* check if we have enough space to hold the new string */
    if (string->trailing) {
        capacity = _json_string_builtin_string_cb + string->trailing_extra_cb - 1;
    } else {
        capacity = string->str.capacity;
    }
    if (len > capacity) {
        if (string->trailing) {
            ptr = NULL;
            osize = 0;
        } else {
            ptr = string->str.ptr;
            osize = capacity + 1;
        }
        ptr = string->alloc_func(ptr, osize, len + 1);  /* realloc */
        if (!ptr)
            return NULL;

        string->trailing = 0;
        string->str.ptr = ptr;
        string->str.capacity = len;
    }

    ptr = string->trailing ? string->trailing_str.str : string->str.ptr;
    for (i = string->len; i < len; ++i) {
        ptr[i] = ch;  /* new characters are filled with ch */
    }
    ptr[len] = '\0';
    string->len = len;

    return v;
}

json_value* json_string_concat(json_value *v, const char *str, unsigned int len)
{
    unsigned int old_len;
    char *ptr;

    assert(v);
    assert(str);

    if (v->type != json_type_string)
        return NULL;

    if (len == (unsigned int)-1)
        len = (unsigned int)strlen(str);
    
    if (len) {
        old_len = json_string_len(v);
        v = json_string_resize(v, old_len + len, ' ');
        if (v) {
            ptr = (char*)json_string_get(v);
            assert(ptr);
            memcpy(ptr + old_len, str, len);
        }
    }

    return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_number_alloc(double number, json_alloc_func alloc_func)
{
    json_number *v;

    if (!alloc_func)
        alloc_func = _default_alloc_func;

    v = (json_number*)alloc_func(NULL, 0, sizeof(json_number));
    if (!v)
        return NULL;

    v->alloc_func = alloc_func;
    v->type = json_type_number;
    v->dbl = number;

    return (json_value*)v;
}

double json_number_get(json_value *v)
{
    json_number *number = (json_number*)v;
    assert(number);

    if (v->type == json_type_number)
        return number->dbl;
    else
        return _NaN();
}

json_value* json_number_set(json_value *v, double dbl)
{
    json_number *number = (json_number*)v;
    assert(number);

    if (v->type == json_type_number) {
        number->dbl = dbl;
        return v;
    } else {
        return NULL;
    }
}

/*----------------------------------------------------------------------------*/

json_value* json_boolean_alloc(int boolean, json_alloc_func alloc_func)
{
    json_value *v;

    if (!alloc_func)
        alloc_func = _default_alloc_func;

    v = (json_value*)alloc_func(NULL, 0, sizeof(json_value));
    if (!v)
        return NULL;

    v->alloc_func = alloc_func;
    v->type = boolean ? json_type_true : json_type_false;

    return v;
}

int json_boolean_get(json_value *v)
{
    assert(v);

    if (v->type == json_type_true)
        return 1;
    else if (v->type == json_type_false)
        return 0;
    else
        return -1;
}

json_value* json_boolean_set(json_value *v, int boolean)
{
    assert(v);

    if (v->type == json_type_true || v->type == json_type_false) {
        v->type = boolean ? json_type_true : json_type_false;
        return v;
    } else {
        return NULL;
    }
}

/*----------------------------------------------------------------------------*/

json_value* json_null_alloc(json_alloc_func alloc_func)
{
    json_value *v;

    if (!alloc_func)
        alloc_func = _default_alloc_func;

    v = (json_value*)alloc_func(NULL, 0, sizeof(json_value));
    if (!v)
        return NULL;

    v->alloc_func = alloc_func;
    v->type = json_type_null;

    return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_object_alloc(json_alloc_func alloc_func)
{
    json_object *object;

    if (!alloc_func)
        alloc_func = _default_alloc_func;

    object = (json_object*)alloc_func(NULL, 0, sizeof(json_object));
    if (!object)
        return NULL;

    object->alloc_func = alloc_func;
    object->type = json_type_object;
    object->capacity = 0;
    object->items = NULL;
    object->size = 0;

    return (json_value*)object;
}

unsigned int json_object_size(json_value *v)
{
    json_object *object = (json_object*)v;
    assert(object);
    
    if (v->type == json_type_object)
        return object->size;
    else
        return (unsigned int)-1;
}

const char* json_object_name_by_index(json_value *v, unsigned int index)
{
    json_object *object = (json_object*)v;
    assert(object);

    if (v->type == json_type_object && index < (unsigned int)object->size)
        return object->items[index].name_str;
    else
        return NULL;
}

json_value* json_object_value_by_index(json_value *v, unsigned int index)
{
    json_object *object = (json_object*)v;
    assert(object);

    if (v->type == json_type_object && index < (unsigned int)object->size)
        return object->items[index].value;
    else
        return NULL;
}

static
json_value* _json_object_get(json_value *v, const char *name, unsigned int len)
{
    json_object *object = (json_object*)v;
    unsigned int hash;
    int index, lower_bound;

    assert(object);
    assert(name);

    if (v->type != json_type_object)
        return NULL;

    _hash_string(name, &len, &hash);
    index = _name_to_index(object, name, len, hash, &lower_bound);
    if (index >= object->size)
        return NULL;

    return object->items[index].value;
}

json_value* json_object_get(json_value *v, const char *name)
{
    return _json_object_get(v, name, (unsigned int)-1);
}

json_value* json_object_set(json_value *v, const char *name, json_value *value)
{
    json_object *object = (json_object*)v;
    unsigned int len = (unsigned int)-1, hash;
    int index, lower_bound, i;

    assert(object);
    assert(name);

    if (v->type != json_type_object || !value)
        return NULL;

    _hash_string(name, &len, &hash);
    index = _name_to_index(object, name, len, hash, &lower_bound);
    
    if (index < object->size) {
        /* assign a new value to a exist key */
        assert(object->items[index].value);
        json_free(object->items[index].value);
        object->items[index].value = value;
        return v;

    } else {
        /* insert a new key/value pair */
        assert(index == object->size);
        assert(lower_bound >= 0 && lower_bound <= object->size);

        if (object->size == object->capacity) {
            unsigned int capacity;
            _json_object_item *items;
            
            capacity = _new_capacity(object->capacity);
            items = (_json_object_item*)object->alloc_func(  /* realloc */
                object->items, 
                sizeof(_json_object_item) * object->capacity, 
                sizeof(_json_object_item) * capacity
                );
            if (!items)
                return NULL;

            object->capacity = capacity;
            object->items = items;
        }

        if (!_object_item_init(name, len, hash, value, 
                object->items + object->size, object->alloc_func)) {
            return NULL;
        }

        for (i = object->size; i > lower_bound; --i)
            object->items[i].sorted_index = object->items[i - 1].sorted_index;
        object->items[lower_bound].sorted_index = object->size;

        object->size += 1;

        return v;
    }
}

json_value* json_object_erase(json_value *v, const char *name)
{
    json_object *object = (json_object*)v;
    unsigned int len = (unsigned int)-1, hash;
    int i, index, lower_bound;

    assert(object);
    assert(name);

    if (v->type != json_type_object)
        return NULL;

    _hash_string(name, &len, &hash);
    index = _name_to_index(object, name, len, hash, &lower_bound);
    if (index >= object->size)
        return NULL;

    _object_item_cleanup(object->items + index, v->alloc_func);

    for (i = index + 1; i < object->size; ++i)
        object->items[i - 1] = object->items[i];
    
    object->size -= 1;
    
    return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_array_alloc(json_alloc_func alloc_func)
{
    json_array *array;

    if (!alloc_func)
        alloc_func = _default_alloc_func;

    array = (json_array*)alloc_func(NULL, 0, sizeof(json_array));
    if (!array)
        return NULL;
    
    array->alloc_func = alloc_func;
    array->type = json_type_array;
    array->capacity = 0;
    array->values = NULL;
    array->size = 0;

    return (json_value*)array;  
}

unsigned int json_array_size(json_value *v)
{
    json_array *array = (json_array*)v;
    assert(array);
    
    if (v->type == json_type_array)
        return array->size;
    else
        return (unsigned int)-1;
}

json_value* json_array_get(json_value *v, unsigned int index)
{
    json_array *array = (json_array*)v;
    assert(array);
    
    if (v->type == json_type_array && index < array->size)
        return array->values[index];
    else
        return NULL;
}

json_value* json_array_set(json_value *v, unsigned int index, json_value *value)
{
    json_array *array = (json_array*)v;
    assert(array);

    if (v->type != json_type_array || index > array->size || !value)
        return NULL;

    if (index == array->size) {
        /* insert a new value */
        if (array->size == array->capacity) {
            unsigned int c;
            json_value **p;

            c = _new_capacity(array->capacity);
            p = (json_value**)array->alloc_func(  /* realloc */
                array->values, 
                sizeof(json_value*) * array->capacity,
                sizeof(json_value*) * c
                );
            if (!p)
                return NULL;
            array->values = p;
            array->capacity = c;
        }

        array->values[index] = value;
        array->size += 1;

        return v;
    
    } else {
        /* assign a new value to a exist index */
        assert(array->values[index]);
        json_free(array->values[index]);
        array->values[index] = value;
        return v;
    }
}

json_value* json_array_erase(json_value *v, unsigned int index)
{
    json_array *array = (json_array*)v;
    unsigned int i;

    assert(array);

    if (v->type != json_type_array || index >= array->size)
        return NULL;

    json_free(array->values[index]);
    
    for (i = index + 1; i < array->size; ++i)
        array->values[i - 1] = array->values[i];
    
    array->size -= 1;
    
    return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_clone(json_value *v, json_alloc_func alloc_func)
{
    json_value *clone = NULL;
    json_string *string;
    json_number *number;
    json_object *object;
    json_array  *array;
    unsigned int i;

    assert(v);

    switch (v->type)
    {
    case json_type_string:
        string = (json_string*)v;
        clone = json_string_alloc(
            string->trailing ? string->trailing_str.str : string->str.ptr, 
            string->len, 
            alloc_func
            );
        break;

    case json_type_number:
        number = (json_number*)v;
        clone = json_number_alloc(number->dbl, alloc_func);
        break;

    case json_type_true:
    case json_type_false:
        clone = json_boolean_alloc(v->type == json_type_true, alloc_func);
        break;

    case json_type_null:
        clone = json_null_alloc(alloc_func);
        break;

    case json_type_object:
        object = (json_object*)v;
        clone = json_object_alloc(alloc_func);
        if (clone) {
            for (i = 0; i < (unsigned int)object->size; ++i) {
                json_value *child_clone = json_clone(object->items[i].value, alloc_func);
                if (!child_clone) {
                    json_free(clone);
                    clone = NULL;
                    break;
                }
                if (!json_object_set(clone, object->items[i].name_str, child_clone)) {
                    json_free(child_clone);
                    json_free(clone);
                    clone = NULL;
                    break;
                }
            }
        }
        break;

    case json_type_array:
        array = (json_array*)v;
        clone = json_array_alloc(alloc_func);
        if (clone) {
            for (i = 0; i < array->size; ++i) {
                json_value *child_clone = json_clone(array->values[i], alloc_func);
                if (!child_clone) {
                    json_free(clone);
                    clone = NULL;
                    break;
                }
                if (!json_array_set(clone, i, child_clone)) {
                    json_free(child_clone);
                    json_free(clone);
                    clone = NULL;
                    break;
                }
            }
        }
        break;

    default:
        assert(0);
    }

    return clone;
}

/*----------------------------------------------------------------------------*/

void json_free(json_value *v)
{
    json_string *string;
    json_object *object;
    json_array  *array;
    unsigned int i;

    if (!v)
        return;

    switch (v->type)
    {
    case json_type_string:
        string = (json_string*)v;
        if (!string->trailing)
            v->alloc_func(string->str.ptr, string->str.capacity + 1, 0);
        v->alloc_func(string, sizeof(json_string) + string->trailing_extra_cb, 0);
        break;

    case json_type_number:
        v->alloc_func(v, sizeof(json_number), 0);
        break;

    case json_type_true:
    case json_type_false:
    case json_type_null:
        v->alloc_func(v, sizeof(json_value), 0);
        break;

    case json_type_object:
        object = (json_object*)v;
        for (i = 0; i < (unsigned int)object->size; ++i)
            _object_item_cleanup(object->items + i, v->alloc_func);
        v->alloc_func(object->items, sizeof(_json_object_item) * object->capacity, 0);
        v->alloc_func(object, sizeof(json_object), 0);
        break;

    case json_type_array:
        array = (json_array*)v;
        for (i = 0; i < array->size; ++i)
            json_free(array->values[i]);
        v->alloc_func(array->values, sizeof(json_value*) * array->capacity, 0);
        v->alloc_func(array, sizeof(json_array), 0);
        break;

    default:
        assert(0);
    }
}

/*----------------------------------------------------------------------------*/

json_value* json_dotget(json_value *v, const char *dotname)
{
    const char *p = dotname;
    char c;
    json_value *child;
    unsigned int index;

    assert(v);
    assert(dotname);

    if (v->type != json_type_array && v->type != json_type_object)
        return NULL;

    while ((c = *p)) {
        if (c == '.')
            break;
        p++;
    }

    if (v->type == json_type_array) {
        /* 0 1 2 3 4 5
           [ x ] . y
           ^     ^
           |     |
        dotname  p  */
        if (p - 3 < dotname)
            return NULL;
        if (*dotname != '[' || *(p - 1) != ']')
            return NULL;
        index = _str_to_index(dotname + 1, (unsigned int)(p - dotname - 2));
        child = json_array_get(v, index);
        if (!child)
            return NULL;

        if (c == '.')
            return json_dotget(child, p + 1);
        else  /* c == '\0' */
            return child;

    } else {          /* json_type_object */
        /* 0 1 2 3 4 5 6
           f o o . b a r
           ^     ^
           |     |
        dotname  p  */
        if (p - 1 < dotname)
            return NULL;
        child = _json_object_get(v, dotname, (unsigned int)(p - dotname));
        if (!child)
            return NULL;

        if (c == '.')
            return json_dotget(child, p + 1);
        else  /* c == '\0' */
            return child;
    }
}

const char* json_dotget_string(json_value *v, const char *dotname)
{
    json_value *p = json_dotget(v, dotname);
    return p ? json_string_get(p) : NULL;
}

double json_dotget_number(json_value *v, const char *dotname)
{
    json_value *p = json_dotget(v, dotname);
    return p ? json_number_get(p) : _NaN();
}

int json_dotget_boolean(json_value *v, const char *dotname)
{
    json_value *p = json_dotget(v, dotname);
    return p ? json_boolean_get(p) : -1;
}

json_value* json_dotget_object(json_value *v, const char *dotname)
{
    json_value *p = json_dotget(v, dotname);
    return (p && json_type(p) == json_type_object) ? p : NULL;
}

json_value* json_dotget_array(json_value *v, const char *dotname)
{
    json_value *p = json_dotget(v, dotname);
    return (p && json_type(p) == json_type_array) ? p : NULL;
}

/*----------------------------------------------------------------------------*/

static void* _default_alloc_func(void *ptr, size_t osize, size_t nsize)
{
    (void)osize;
    return realloc(ptr, nsize);
}

static double _NaN()
{
    /* assuming sizeof(unsigned)=4 && sizeof(double)=8 and Little-Endian */
    unsigned int nan[2] = { 0xffffffff, 0x7fffffff };
    return *(double*)nan;
}

static unsigned int _new_capacity(unsigned int capacity)
{
    if (capacity == 0)
        capacity = 4;
    else if (capacity < 1024)
        capacity *= 2;
    else
        capacity += 1024;

    return capacity;
}

static int _object_item_init(
    const char *name_str, 
    unsigned int name_len, 
    unsigned int name_hash, 
    json_value *value, 
    _json_object_item *item,
    json_alloc_func alloc_func
    )
{
    assert(name_str);
    assert(name_len);
    assert(value);
    assert(item);
    assert(alloc_func);

    item->sorted_index = -1;

    item->name_str = alloc_func(NULL, 0, name_len + 1);
    if (!item->name_str)
        return 0;
    item->name_len = name_len;
    item->name_hash = name_hash;
    memcpy((void*)item->name_str, name_str, name_len);
    *(char*)(item->name_str + name_len) = '\0';

    item->value = value;

    return 1;
}

static void _object_item_cleanup(_json_object_item *item, json_alloc_func alloc_func)
{
    assert(item);
    assert(alloc_func);

    alloc_func((void*)item->name_str, item->name_len + 1, 0);
    item->name_str = NULL;
    item->name_len = 0;
    item->name_hash = 0;
    
    json_free(item->value);
    item->value = NULL;
}

static int _object_item_index_lower_bound(
    _json_object_item *items,
    int count,
    unsigned int hash
    )
{
    _json_object_item *first, *middle;
    int len, half;

    first = items;
    len = count;

    while (len > 0) {
        half = len >> 1;
        middle = first + half;
        assert(middle->sorted_index >= 0 && middle->sorted_index < count);
        if (items[middle->sorted_index].name_hash < hash) {
            first = middle + 1;
            len = len - half - 1;
        } else {
            len = half;
        }
    }

    assert(first >= items && first <= items + count);
    return (int)(first - items);
}

static void _hash_string(const char *str, unsigned int *len, unsigned int *hash)
{
    const char *p;
    char c;
    unsigned int i;
    
    assert(str);
    assert(len);
    assert(hash);
    
    /* djb2 hash */
    *hash = 5381;
    p = str;
    if (*len != (unsigned int)-1) {
        for (i = 0; i < *len; ++i) {
            c = *p++;
            *hash = ((*hash << 5) + *hash) + c;
        }
    } else {
        while ((c = *p++)) {
            *hash = ((*hash << 5) + *hash) + c;
        }
        *len = (unsigned int)(p - str - 1);  /* return the length */
    }
}

static int _name_to_index(
    json_object *object, 
    const char *name, 
    unsigned int len, 
    unsigned int hash,
    int *lower_bound
    )
{
    int index, i;
    
    assert(object);
    assert(name);
    assert(lower_bound);

    *lower_bound = _object_item_index_lower_bound(object->items, object->size, hash);
    assert(*lower_bound >= 0 && *lower_bound <= object->size);

    for (i = *lower_bound; i < object->size; ++i) {
        index = object->items[i].sorted_index;
        if (object->items[index].name_hash != hash)
            break;
        if (object->items[index].name_len == len && memcmp(object->items[index].name_str, name, len) == 0)
            return index;
    }

    return object->size;
}

static unsigned int _str_to_index(const char *str, unsigned int len)
{
    unsigned int i, num = (unsigned int)-1;
    char c;

    assert(str);
    assert(len);

    for (i = 0; i < len; ++i) {
        c = str[i];
        if (c < '0' || c > '9')
            return (unsigned int)-1;
        
        if (i == 0)
            num = 0;
        else
            num *= 10;
        num += c - '0';
    }

    return num;
}
