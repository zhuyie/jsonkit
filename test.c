#include "json.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void test_string();
static void test_number();
static void test_boolean();
static void test_null();
static void test_object();
static void test_array();
static void test_dotget_clone();
static void test_write();
static void test_parser();

int main(int argc, char **argv)
{
    test_string();
    test_number();
    test_boolean();
    test_null();
    test_object();
    test_array();
    test_dotget_clone();
    test_write();
    test_parser();
    return 0;
}

static void test_string()
{
    json_value *v;
    json_value_type type;
    const char *str;
    unsigned int len;
    int boolean;

    v = json_string_alloc("hello", (unsigned int)-1, NULL);
    assert(v);
    type = json_type(v);
    assert(type == json_type_string);

    str = json_string_get(v);
    assert(str && strcmp(str, "hello") == 0);
    len = json_string_len(v);
    assert(len == 5);
    
    v = json_string_set(v, "abcd", (unsigned int)-1);
    assert(v);
    str = json_string_get(v);
    assert(str && strcmp(str, "abcd") == 0);
    len = json_string_len(v);
    assert(len == 4);

    v = json_string_set(v, "xxxxyyyyzzzz", 12);
    assert(v);
    str = json_string_get(v);
    assert(str && strcmp(str, "xxxxyyyyzzzz") == 0);
    len = json_string_len(v);
    assert(len == 12);

    v = json_string_resize(v, 20, ' ');
    assert(v);
    assert(json_string_len(v) == 20);
    v = json_string_resize(v, 5, ' ');
    assert(v);
    assert(json_string_len(v) == 5);

    v = json_string_concat(v, "hello_json", -1);
    assert(v);

    boolean = json_boolean_get(v);
    assert(boolean == -1);

    json_free(v);
}

static void test_number()
{
    json_value *v;
    json_value_type type;
    double num;
    unsigned short u2;
    const char *str;

    v = json_number_alloc(42, NULL);
    assert(v);
    type = json_type(v);
    assert(type == json_type_number);

    num = json_number_get(v);
    assert(num == 42.0);
    u2 = json_number_get_type(v, unsigned short);
    assert(u2 == 42);
    v = json_number_set(v, 99.99);
    assert(v);
    num = json_number_get(v);
    assert(num == 99.99);

    str = json_string_get(v);
    assert(str == NULL);

    json_free(v);
}

static void test_boolean()
{
    json_value *v;
    json_value_type type;
    int boolean;

    v = json_boolean_alloc(1, NULL);
    assert(v);
    
    type = json_type(v);
    assert(type == json_type_true);
    boolean = json_boolean_get(v);
    assert(boolean == 1);
    
    v = json_boolean_set(v, 0);
    assert(v);

    type = json_type(v);
    assert(type == json_type_false);
    boolean = json_boolean_get(v);
    assert(boolean == 0);
    
    json_free(v);
}

static void test_null()
{
    json_value *v;
    json_value_type type;

    v = json_null_alloc(NULL);
    assert(v);
    type = json_type(v);
    assert(type == json_type_null);
    json_free(v);
}

static void test_object()
{
    json_value *v, *v2;
    json_value_type type;
    unsigned int size;
    int i;
    const char* names[10] = { "5", "8", "2", "0", "1", "3", "7", "4", "6", "9" };

    v = json_object_alloc(NULL);
    assert(v);
    type = json_type(v);
    assert(type == json_type_object);

    size = json_object_size(v);
    assert(size == 0);

    v2 = json_object_get(v, "foo");
    assert(v2 == NULL);

    for (i = 0; i < 10; ++i) {
        v2 = json_number_alloc(i + 1, NULL);
        assert(v2);
        v = json_object_set(v, names[i], v2);
        assert(v);
    }
    size = json_object_size(v);
    assert(size == 10);

    v2 = json_object_get(v, "8");
    assert(v2 != NULL);
    assert(json_type(v2) == json_type_number);
    assert(json_number_get(v2) == 2);

    v2 = json_string_alloc("master yoda", (unsigned int)-1, NULL);
    assert(v2);
    v = json_object_set(v, "3", v2);
    assert(v != NULL);
    v2 = json_object_get(v, "3");
    assert(v2 != NULL);
    assert(json_type(v2) == json_type_string);
    assert(strcmp(json_string_get(v2), "master yoda") == 0);

    v = json_object_erase(v, "5");
    assert(v);
    size = json_object_size(v);
    assert(size == 9);

    v2 = json_object_get(v, "9");
    assert(v2);
    assert(json_type(v2) == json_type_number);
    assert(json_number_get(v2) == 10);

    json_free(v);
}

static void test_array()
{
    json_value *v, *v2;
    json_value_type type;
    unsigned int size;
    unsigned int i;

    v = json_array_alloc(NULL);
    assert(v);
    type = json_type(v);
    assert(type == json_type_array);

    size = json_array_size(v);
    assert(size == 0);
    v2 = json_array_get(v, 0);
    assert(v2 == NULL);

    for (i = 0; i < 100; ++i) {
        v2 = json_boolean_alloc(i % 2, NULL);
        assert(v2);
        v = json_array_set(v, i, v2);
        assert(v);
    }
    size = json_array_size(v);
    assert(size == 100);

    v2 = json_array_get(v, 42);
    assert(v2);
    assert(json_type(v2) == json_type_false);

    v2 = json_string_alloc("hello world", (unsigned int)-1, NULL);
    assert(v2);
    v = json_array_set(v, 2, v2);
    assert(v);
    v2 = json_array_get(v, 2);
    assert(v2);
    assert(json_type(v2) == json_type_string);

    v = json_array_erase(v, 10);
    assert(v);
    size = json_array_size(v);
    assert(size == 99);

    json_free(v);
}

static void test_dotget_clone()
{
    json_value *object, *array, *v;
    unsigned int i;
    int boolean;

    object = json_object_alloc(NULL);
    assert(object);
    array = json_array_alloc(NULL);
    assert(array);
    for (i = 0; i < 10; ++i) {
        v = json_boolean_alloc(i % 2, NULL);
        assert(v);
        v = json_array_set(array, i, v);
        assert(v);
    }
    v = json_object_set(object, "abc", array);
    assert(v);

    v = json_dotget(object, "abc");
    assert(v && json_type(v) == json_type_array);
    v = json_dotget(object, "abc.[3]");
    assert(v && json_type(v) == json_type_true);
    v = json_dotget(object, "abc.xxx");
    assert(v == NULL);
    v = json_dotget(object, "");
    assert(v == NULL);

    boolean = json_dotget_boolean(object, "abc.[3]");
    assert(boolean == 1);
    boolean = json_dotget_boolean(object, "abc.[8]");
    assert(boolean == 0);
    boolean = json_dotget_boolean(object, "abc.[999]");
    assert(boolean == -1);

    v = json_clone(object, NULL);
    assert(v);
    json_free(v);

    json_free(object);
}

char buf[8192];
unsigned int buf_size = 0;
static int my_write(const char *data, int len)
{
    assert(len + buf_size <= 8192);
    memcpy(buf + buf_size, data, len);
    buf_size += len;
    return len;
}

static void test_write()
{
    json_value *object, *v, *v1, *v2;
    unsigned int i;
    json_write_config write_config;

    object = json_object_alloc(NULL);
    assert(object);
    
    v = json_string_alloc("bar\"123\"", (unsigned int)-1, NULL);
    assert(v);
    object = json_object_set(object, "foo\txxx\\yyy\nzzz", v);
    assert(object);
    
    v = json_number_alloc(0, NULL);
    assert(v);
    object = json_object_set(object, "num1", v);
    assert(object);
    
    v = json_number_alloc(99.99, NULL);
    assert(v);
    object = json_object_set(object, "num2", v);
    assert(object);
    
    v = json_boolean_alloc(0, NULL);
    assert(v);
    object = json_object_set(object, "bool", v);
    assert(object);
    
    v = json_null_alloc(0);
    assert(v);
    object = json_object_set(object, "null", v);
    assert(object);
    
    v = json_array_alloc(NULL);
    assert(v);
    object = json_object_set(object, "empty_array", v);
    assert(object);
    
    v = json_object_alloc(NULL);
    assert(v);
    object = json_object_set(object, "empty_object", v);
    assert(object);
    
    v = json_array_alloc(NULL);
    assert(v);
    for (i = 0; i < 10; ++i) {
        v1 = json_object_alloc(NULL);
        assert(v1);
        v2 = json_boolean_alloc(i % 2, NULL);
        assert(v2);
        v1 = json_object_set(v1, "test", v2);
        assert(v1);

        v = json_array_set(v, i, v1);
        assert(v);
    }
    object = json_object_set(object, "array", v);
    assert(object);

    write_config.compact = 0;
    write_config.crlf = 1;
    write_config.indent = 4;
    write_config.write = my_write;
    json_write(object, write_config);
    printf("%s\n", buf);

    json_free(object);
}

static const char *testJSON = "{ \"foo\": \"bar\", \"null\": null, \"number\": 99.99, \"array\": [ true ] }";
/*
static int my_read(unsigned int index, int len, char *buf)
{
    unsigned int json_len = (unsigned int)strlen(testJSON);
    assert(index + len <= json_len);
    memcpy(buf, testJSON + index, len);
    return 1;
}
*/
static void test_parser()
{
    json_parser_config config;
    json_parser *parser;
    size_t i, len;
    int n;
    json_value *res;

    config.json_str = testJSON;
    config.json_str_len = 0;
    parser = json_parser_alloc(20, config);
    assert(parser);

    for (i = 0, len = strlen(testJSON); i < len; ++i) {
        int next_char = testJSON[i];
        n = json_parser_char(parser, next_char);
        assert(n);
    }
    
    res = json_parser_done(parser);
    assert(res);
    json_free(res);

    json_parser_free(parser);
}
