#include "json.h"

void test()
{
	size_t s0 = sizeof(json_value);
	size_t s1 = sizeof(json_string);
	size_t s2 = sizeof(json_number);
	size_t s3 = sizeof(json_object);
	size_t s4 = sizeof(json_array);

	json_value *v, *v2;
	json_value_type type;
	const char *str;
	unsigned int len;
	unsigned int size;
	double num;
	int boolean;

	v = json_string_alloc("hello", (unsigned int)-1);
	str = json_string_get(v);
	len = json_string_len(v);
	v = json_string_set(v, "abcd", (unsigned int)-1);
	str = json_string_get(v);
	len = json_string_len(v);
	v = json_string_set(v, "xxxxyyyyzzzz", 12);
	str = json_string_get(v);
	len = json_string_len(v);
	num = json_number_get(v);
	json_free(v);

	v = json_number_alloc(100);
	num = json_number_get(v);
	v = json_number_set(v, 99.99);
	str = json_string_get(v);
	json_free(v);

	v = json_boolean_alloc(1);
	boolean = json_boolean_get(v);
	v = json_boolean_set(v, 0);
	json_free(v);

	v = json_null_alloc();
	type = json_type(v);
	json_free(v);

	v = json_object_alloc();
	size = json_object_size(v);
	v2 = json_object_get(v, "foo");
	json_free(v);

	v = json_array_alloc();
	size = json_array_size(v);
	v2 = json_array_get(v, 0);
	json_free(v);
}

int main(int argc, char **argv)
{
	test();
	return 0;
}
