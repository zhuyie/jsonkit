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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/*----------------------------------------------------------------------------*/

struct json_value {
	json_value_type type;
};

typedef struct json_string {
	json_value_type type;
	unsigned int trailing : 1;
	unsigned int capacity : 31;
	union {
		struct {
			unsigned int len;
			char str[1];
		} trailing_str;
		struct {
			char *ptr;
			unsigned int len;
		} str;
	};
} json_string;

typedef struct json_number {
	json_value_type type;
	double dbl;
} json_number;

typedef struct _json_name {
	unsigned int len;
	unsigned int hash;
	char str[1];
} _json_name;

typedef struct _json_object_item {
	_json_name *name;
	json_value *value;
} _json_object_item;

typedef struct json_object {
	json_value_type type;
	unsigned int capacity;
	_json_object_item *items;
	unsigned int size;
} json_object;

typedef struct json_array {
	json_value_type type;
	unsigned int capacity;
	json_value **values;
	unsigned int size;
} json_array;

static double _NaN();
static unsigned int _new_capacity(unsigned int capacity);
static _json_name* _name_alloc(
	const char *str, 
	unsigned int len, 
	unsigned int hash
	);
static void _name_free(_json_name *name);
static void _hash_string(
	const char *str, 
	unsigned int *len, 
	unsigned int *hash
	);
static unsigned int _name_to_index(
	json_object *object, 
	const char *name, 
	unsigned int len, 
	unsigned int hash
	);
static unsigned int _str_to_index(const char *str, unsigned int len);

/*----------------------------------------------------------------------------*/

json_value_type json_type(json_value *v)
{
	assert(v);
	return v->type;
}

/*----------------------------------------------------------------------------*/

json_value* json_string_alloc(const char *str, unsigned int len)
{
	json_string *string;
	unsigned int extra_cb;

	assert(str);
	
	if (len == (unsigned int)-1)
		len = (unsigned int)strlen(str);
	if (len > INT_MAX)
		return NULL;

	if (len < sizeof(char*))
		extra_cb = 0;
	else
		extra_cb = len - sizeof(char*) + 1;

	string = (json_string*)malloc(sizeof(json_string) + extra_cb);
	if (!string)
		return NULL;
		
	string->type = json_type_string;
	string->trailing = 1;
	string->capacity = (unsigned short)(sizeof(char*) + extra_cb);
	string->trailing_str.len = len;
	memcpy(string->trailing_str.str, str, len);
	string->trailing_str.str[len] = '\0';

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
	char *ptr;

	assert(v);
	assert(str);

	if (v->type != json_type_string)
		return NULL;

	if (len == (unsigned int)-1)
		len = (unsigned int)strlen(str);
	if (len > INT_MAX)
		return NULL;

	if (len >= string->capacity) {
		ptr = string->trailing ? NULL : string->str.ptr;
		ptr = realloc(ptr, len + 1);
		if (!ptr)
			return NULL;
		
		string->trailing = 0;
		string->capacity = len + 1;
		string->str.ptr = ptr;
	}

	if (string->trailing) {
		ptr = string->trailing_str.str;
		memcpy(ptr, str, len);
		ptr[len] = '\0';
		string->trailing_str.len = len;
	} else {
		ptr = string->str.ptr;
		memcpy(ptr, str, len);
		ptr[len] = '\0';
		string->str.len = len;
	}

	return v;
}

unsigned int json_string_len(json_value *v)
{
	json_string *string = (json_string*)v;
	assert(string);

	if (v->type != json_type_string)
		return (unsigned int)-1;

	if (string->trailing)
		return string->trailing_str.len;
	else
		return string->str.len;
}

/*----------------------------------------------------------------------------*/

json_value* json_number_alloc(double number)
{
	json_number *v;

	v = (json_number*)malloc(sizeof(json_number));
	if (!v)
		return NULL;

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

json_value* json_boolean_alloc(int boolean)
{
	json_value *v;

	v = (json_value*)malloc(sizeof(json_value));
	if (!v)
		return NULL;

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

json_value* json_null_alloc()
{
	json_value *v;

	v = (json_value*)malloc(sizeof(json_value));
	if (!v)
		return NULL;

	v->type = json_type_null;

	return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_object_alloc()
{
	json_object *object;

	object = (json_object*)malloc(sizeof(json_object));
	if (!object)
		return NULL;

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

	if (v->type == json_type_object && index < object->size)
		return object->items[index].name->str;
	else
		return NULL;
}

json_value* json_object_value_by_index(json_value *v, unsigned int index)
{
	json_object *object = (json_object*)v;
	assert(object);

	if (v->type == json_type_object && index < object->size)
		return object->items[index].value;
	else
		return NULL;
}

static
json_value* _json_object_get(json_value *v, const char *name, unsigned int len)
{
	json_object *object = (json_object*)v;
	unsigned int hash, index;

	assert(object);
	assert(name);

	if (v->type != json_type_object)
		return NULL;

	_hash_string(name, &len, &hash);
	index = _name_to_index(object, name, len, hash);
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
	unsigned int len, hash, index;

	assert(object);
	assert(name);
	assert(value);

	if (v->type != json_type_object)
		return NULL;

	_hash_string(name, &len, &hash);
	index = _name_to_index(object, name, len, hash);
	
	if (index < object->size) {
		if (object->items[index].value == value)
			return NULL;
		json_free(object->items[index].value);
		object->items[index].value = value;
		return v;

	} else {
		_json_name *n;
		unsigned int c;
		_json_object_item *p;

		if (object->size == object->capacity) {
			c = _new_capacity(object->capacity);
			p = (_json_object_item*)realloc(object->items, sizeof(_json_object_item) * c);
			if (!p)
				return NULL;
			object->capacity = c;
			object->items = p;
		}

		n = _name_alloc(name, len, hash);
		if (!n)
			return NULL;

		index = object->size;
		object->items[index].name = n;
		object->items[index].value = value;

		object->size += 1;

		return v;
	}
}

json_value* json_object_erase(json_value *v, const char *name)
{
	json_object *object = (json_object*)v;
	unsigned int len, hash, i, index;

	assert(object);
	assert(name);

	if (v->type != json_type_object)
		return NULL;

	_hash_string(name, &len, &hash);
	index = _name_to_index(object, name, len, hash);
	if (index >= object->size)
		return NULL;

	_name_free(object->items[index].name);
	json_free(object->items[index].value);

	for (i = index + 1; i < object->size; ++i)
		object->items[i - 1] = object->items[i];
	
	object->size -= 1;
	
	return v;
}

/*----------------------------------------------------------------------------*/

json_value* json_array_alloc()
{
	json_array *array;

	array = (json_array*)malloc(sizeof(json_array));
	if (!array)
		return NULL;
	
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
	assert(value);

	if (v->type != json_type_array || index > array->size)
		return NULL;

	if (index == array->size) {
		unsigned int c;
		json_value **p;

		c = _new_capacity(array->capacity);
		p = (json_value**)realloc(array->values, sizeof(json_value*) * c);
		if (!p)
			return NULL;
		array->values = p;
		array->capacity = c;

		array->values[index] = value;
		array->size += 1;

		return v;
	
	} else {
		if (array->values[index] == value)
			return NULL;
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
			free(string->str.ptr);
		free(string);
		break;

	case json_type_number:
	case json_type_true:
	case json_type_false:
	case json_type_null:
		free(v);
		break;

	case json_type_object:
		object = (json_object*)v;
		for (i = 0; i < object->size; ++i) {
			_name_free(object->items[i].name);
			json_free(object->items[i].value);
		}
		free(object->items);
		free(object);
		break;

	case json_type_array:
		array = (json_array*)v;
		for (i = 0; i < array->size; ++i)
			json_free(array->values[i]);
		free(array->values);
		free(array);
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

	while (c = *p) {
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
		index = _str_to_index(dotname + 1, (unsigned int)(p - dotname - 3));
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

static double _NaN()
{
	/* assuming sizeof(unsigned)=4 && sizeof(double)=8 and Little-Endian */
	unsigned int nan[2] = { 0xffffffff, 0x7fffffff };
	return *(double*)nan;
}

static unsigned int _new_capacity(unsigned int capacity)
{
	if (capacity == 0)
		capacity = 2;
	else if (capacity < 32)
		capacity *= 2;
	else
		capacity += 16;

	return capacity;
}

static _json_name* _name_alloc(const char *str, unsigned int len, unsigned int hash)
{
	_json_name *name;

	assert(str);
	assert(len);

	name = (_json_name*)malloc(sizeof(_json_name) + len);
	if (!name)
		return NULL;

	name->len = len;
	name->hash = hash;
	memcpy(name->str, str, len);
	name->str[len] = '\0';

	return name;
}

static void _name_free(_json_name *name)
{
	free(name);
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
			*hash = ((*hash << 5) + *hash) + c; /* hash * 33 + c */
		}
	} else {
		while (c = *p++) {
			*hash = ((*hash << 5) + *hash) + c; /* hash * 33 + c */
		}
		*len = (unsigned int)(p - str - 1);  /* return the length */
	}
}

static unsigned int _name_to_index(
	json_object *object, 
	const char *name, 
	unsigned int len, 
	unsigned int hash
	)
{
	unsigned int i;
	
	assert(object);
	assert(name);

	for (i = 0; i < object->size; ++i) {
		if (object->items[i].name->len == len &&
			object->items[i].name->hash == hash &&
			memcmp(object->items[i].name->str, name, len) == 0)
		{
			return i;
		}
	}
	return (unsigned int)-1;
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
		
		if (i != 0)
			num *= 10;
		num += c - '0';
	}

	return num;
}
