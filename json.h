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

#ifndef _JSONKIT_JSON_H_
#define _JSONKIT_JSON_H_

/*----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum json_value_type {
    json_type_string = 1,
    json_type_number,
    json_type_true,
    json_type_false,
    json_type_null,
    json_type_object,
    json_type_array
} json_value_type;

struct json_value;
typedef struct json_value json_value;

json_value_type json_type(json_value *v);

json_value*  json_string_alloc(const char *str, unsigned int len);
const char*  json_string_get(json_value *v);
json_value*  json_string_set(json_value *v, const char *str, unsigned int len);
unsigned int json_string_len(json_value *v);
json_value*  json_string_resize(json_value *v, unsigned int len, char ch);
json_value*  json_string_concat(json_value *v, const char *str, unsigned int len);

json_value*  json_number_alloc(double number);
double       json_number_get(json_value *v);
#define      json_number_get_type(v, type)    (type)json_number_get(v)
json_value*  json_number_set(json_value *v, double dbl);

json_value*  json_boolean_alloc(int boolean);
int          json_boolean_get(json_value *v);
json_value*  json_boolean_set(json_value *v, int boolean);

json_value*  json_null_alloc();

json_value*  json_object_alloc();
unsigned int json_object_size(json_value *v);
const char*  json_object_name_by_index(json_value *v, unsigned int index);
json_value*  json_object_value_by_index(json_value *v, unsigned int index);
json_value*  json_object_get(json_value *v, const char *name);
json_value*  json_object_set(json_value *v, const char *name, json_value *value);
json_value*  json_object_erase(json_value *v, const char *name);

json_value*  json_array_alloc();
unsigned int json_array_size(json_value *v);
json_value*  json_array_get(json_value *v, unsigned int index);
json_value*  json_array_set(json_value *v, unsigned int index, json_value *value);
#define      json_array_append(array, value)    json_array_set(array, json_array_size(array), value)
json_value*  json_array_erase(json_value *v, unsigned int index);

json_value*  json_clone(json_value *v);

void         json_free(json_value *v);

json_value*  json_dotget(json_value *v, const char *dotname);
const char*  json_dotget_string(json_value *v, const char *dotname);
double       json_dotget_number(json_value *v, const char *dotname);
int          json_dotget_boolean(json_value *v, const char *dotname);
json_value*  json_dotget_object(json_value *v, const char *dotname);
json_value*  json_dotget_array(json_value *v, const char *dotname);


typedef struct json_write_config {
    int compact;    /* compact mode */
    int indent;     /* indent levels(number of spaces) */
    int crlf;       /* \r\n or \n */
    int (*write)(const char *buf, int len);
} json_write_config;

int json_write(json_value *v, json_write_config config);


struct json_parser;
typedef struct json_parser json_parser;

typedef struct json_parser_config {
    const char *json_str;
    unsigned int json_str_len;
} json_parser_config;

json_parser* json_parser_alloc(int depth, json_parser_config config);
int          json_parser_char(json_parser *parser, int next_char);
json_value*  json_parser_done(json_parser *parser);
void         json_parser_free(json_parser *parser);

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------*/

#endif  /* _JSONKIT_JSON_H_ */
