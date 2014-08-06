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

/* JSON_checker.c */

/* 2007-08-24 */

/*
Copyright (c) 2005 JSON.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

The Software shall be used for Good, not Evil.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*----------------------------------------------------------------------------*/

#define MAX_NAME_LEN 256

#define true  1
#define false 0
#define __   -1     /* the universal error code */

/*
    Characters are mapped into these 31 character classes. This allows for
    a significant reduction in the size of the state transition table.
*/
enum classes {
    C_SPACE,  /* space */
    C_WHITE,  /* other whitespace */
    C_LCURB,  /* {  */
    C_RCURB,  /* } */
    C_LSQRB,  /* [ */
    C_RSQRB,  /* ] */
    C_COLON,  /* : */
    C_COMMA,  /* , */
    C_QUOTE,  /* " */
    C_BACKS,  /* \ */
    C_SLASH,  /* / */
    C_PLUS,   /* + */
    C_MINUS,  /* - */
    C_POINT,  /* . */
    C_ZERO ,  /* 0 */
    C_DIGIT,  /* 123456789 */
    C_LOW_A,  /* a */
    C_LOW_B,  /* b */
    C_LOW_C,  /* c */
    C_LOW_D,  /* d */
    C_LOW_E,  /* e */
    C_LOW_F,  /* f */
    C_LOW_L,  /* l */
    C_LOW_N,  /* n */
    C_LOW_R,  /* r */
    C_LOW_S,  /* s */
    C_LOW_T,  /* t */
    C_LOW_U,  /* u */
    C_ABCDF,  /* ABCDF */
    C_E,      /* E */
    C_ETC,    /* everything else */
    NR_CLASSES
};

static int ascii_class[128] = {
/*
    This array maps the 128 ASCII characters into character classes.
    The remaining Unicode characters should be mapped to C_ETC.
    Non-whitespace control characters are errors.
*/
    __,      __,      __,      __,      __,      __,      __,      __,
    __,      C_WHITE, C_WHITE, __,      __,      C_WHITE, __,      __,
    __,      __,      __,      __,      __,      __,      __,      __,
    __,      __,      __,      __,      __,      __,      __,      __,

    C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
    C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
    C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

    C_ETC,   C_ABCDF, C_ABCDF, C_ABCDF, C_ABCDF, C_E,     C_ABCDF, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

    C_ETC,   C_LOW_A, C_LOW_B, C_LOW_C, C_LOW_D, C_LOW_E, C_LOW_F, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
    C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB, C_ETC,   C_ETC
};

/*
    The state codes.
*/
enum states {
    GO,  /* start    */
    OK,  /* ok       */
    OB,  /* object   */
    KE,  /* key      */
    CO,  /* colon    */
    VA,  /* value    */
    AR,  /* array    */
    ST,  /* string   */
    ES,  /* escape   */
    U1,  /* u1       */
    U2,  /* u2       */
    U3,  /* u3       */
    U4,  /* u4       */
    MI,  /* minus    */
    ZE,  /* zero     */
    IN,  /* integer  */
    FR,  /* fraction */
    E1,  /* e        */
    E2,  /* ex       */
    E3,  /* exp      */
    T1,  /* tr       */
    T2,  /* tru      */
    T3,  /* true     */
    F1,  /* fa       */
    F2,  /* fal      */
    F3,  /* fals     */
    F4,  /* false    */
    N1,  /* nu       */
    N2,  /* nul      */
    N3,  /* null     */
    NR_STATES
};

static int state_transition_table[NR_STATES][NR_CLASSES] = {
/*
    The state transition table takes the current state and the current symbol,
    and returns either a new state or an action. An action is represented as a
    negative number. A JSON text is accepted if at the end of the text the
    state is OK and if the stack is empty.

                 white                                      1-9                                   ABCDF  etc
             space |  {  }  [  ]  :  ,  "  \  /  +  -  .  0  |  a  b  c  d  e  f  l  n  r  s  t  u  |  E  |*/
/*start  GO*/ {GO,GO,-6,__,-5,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ok     OK*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*object OB*/ {OB,OB,__,-9,__,__,__,__,ST,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*key    KE*/ {KE,KE,__,__,__,__,__,__,ST,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*colon  CO*/ {CO,CO,__,__,__,__,-2,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*value  VA*/ {VA,VA,-6,__,-5,__,__,__,ST,__,__,__,MI,__,ZE,IN,__,__,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*array  AR*/ {AR,AR,-6,__,-5,-7,__,__,ST,__,__,__,MI,__,ZE,IN,__,__,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*string ST*/ {ST,__,ST,ST,ST,ST,ST,ST,-4,ES,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST},
/*escape ES*/ {__,__,__,__,__,__,__,__,ST,ST,ST,__,__,__,__,__,__,ST,__,__,__,ST,__,ST,ST,__,ST,U1,__,__,__},
/*u1     U1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U2,U2,U2,U2,U2,U2,U2,U2,__,__,__,__,__,__,U2,U2,__},
/*u2     U2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U3,U3,U3,U3,U3,U3,U3,U3,__,__,__,__,__,__,U3,U3,__},
/*u3     U3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U4,U4,U4,U4,U4,U4,U4,U4,__,__,__,__,__,__,U4,U4,__},
/*u4     U4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ST,ST,ST,ST,ST,ST,ST,ST,__,__,__,__,__,__,ST,ST,__},
/*minus  MI*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ZE,IN,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*zero   ZE*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,FR,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*int    IN*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,FR,IN,IN,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*frac   FR*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,__,FR,FR,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*e      E1*/ {__,__,__,__,__,__,__,__,__,__,__,E2,E2,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ex     E2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*exp    E3*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*tr     T1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T2,__,__,__,__,__,__},
/*tru    T2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T3,__,__,__},
/*true   T3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*fa     F1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F2,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*fal    F2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F3,__,__,__,__,__,__,__,__},
/*fals   F3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F4,__,__,__,__,__},
/*false  F4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*nu     N1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N2,__,__,__},
/*nul    N2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N3,__,__,__,__,__,__,__,__},
/*null   N3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__},
};

/*
    These modes can be pushed on the stack.
*/
typedef enum modes {
    MODE_DONE = 1,
    MODE_ARRAY,
    MODE_OBJECT,
    MODE_OBJECT_KEY,
    MODE_OBJECT_VALUE,
} modes;

typedef struct json_parser_stack_item {
    modes mode;
    json_value *value;
    unsigned int name_begin;
    unsigned int name_len;
    unsigned int value_begin;
} json_parser_stack_item;

struct json_parser {
    int depth;
    json_parser_config config;
    unsigned int char_index;
    int state;
    int top;
    json_parser_stack_item *stack;
};

static int _push(
    json_parser *parser, 
    modes mode
    );
static int _pop(
    json_parser *parser, 
    modes mode
    );
static int _change_state(
    json_parser *parser, 
    int next_state
    );

/*----------------------------------------------------------------------------*/

json_parser* json_parser_alloc(int depth, json_parser_config config)
{
/*
    json_parser_alloc starts the checking process by constructing a json_parser
    object. It takes a depth parameter that restricts the level of maximum
    nesting.

    To continue the process, call json_parser_char for each character in the
    JSON text, and then call json_parser_done to obtain the final result.
    These functions are fully reentrant.
*/
    json_parser *parser;

    assert(depth > 1);
    parser = (json_parser*)malloc(sizeof(json_parser));
    if (!parser)
        return NULL;
    parser->depth = depth;
    memcpy(&parser->config, &config, sizeof(json_parser_config));
    parser->state = GO;
    parser->char_index = 0;
    parser->top = -1;
    parser->stack = (json_parser_stack_item*)calloc(depth, sizeof(json_parser_stack_item));
    if (!parser->stack) {
        free(parser);
        return NULL;
    }

    _push(parser, MODE_DONE);

    return parser;
}

int json_parser_char(json_parser *parser, int next_char)
{
/*
    After calling json_parser_alloc, call this function for each character (or
    partial character) in your JSON text. It can accept UTF-8, UTF-16, or
    UTF-32. It returns true if things are looking ok so far. If it rejects the
    text, it returns false.
*/
    int next_class, next_state;
/*
    Determine the character's class.
*/
    if (next_char < 0) {
        return false;
    }
    if (next_char >= 128) {
        next_class = C_ETC;
    } else {
        next_class = ascii_class[next_char];
        if (next_class <= __) {
            return false;
        }
    }
/*
    Get the next state from the state transition table.
*/
    next_state = state_transition_table[parser->state][next_class];
    if (next_state >= 0) {
/*
    Change the state.
*/
        if (!_change_state(parser, next_state))
            return false;
    } else {
/*
    Or perform one of the actions.
*/
        switch (next_state) {
/* empty } */
        case -9:
            if (!_change_state(parser, OK))
                return false;
            if (!_pop(parser, MODE_OBJECT_KEY) || !_pop(parser, MODE_OBJECT))
                return false;
            break;

/* } */ case -8:
            if (!_change_state(parser, OK))
                return false;
            if (!_pop(parser, MODE_OBJECT_VALUE) || !_pop(parser, MODE_OBJECT))
                return false;
            break;

/* ] */ case -7:
            if (!_change_state(parser, OK))
                return false;
            if (!_pop(parser, MODE_ARRAY))
                return false;
            break;

/* { */ case -6:
            if (!_change_state(parser, OB))
                return false;
            if (!_push(parser, MODE_OBJECT) || !_push(parser, MODE_OBJECT_KEY))
                return false;
            break;

/* [ */ case -5:
            if (!_change_state(parser, AR))
                return false;
            if (!_push(parser, MODE_ARRAY))
                return false;
            break;

/* " */ case -4:
            switch (parser->stack[parser->top].mode) {
            case MODE_OBJECT_KEY:
                if (!_change_state(parser, CO))
                    return false;
                break;
            case MODE_ARRAY:
            case MODE_OBJECT_VALUE:
                if (!_change_state(parser, OK))
                    return false;
                break;
            default:
                return false;
            }
            break;

/* , */ case -3:
            switch (parser->stack[parser->top].mode) {
            case MODE_OBJECT_VALUE:
/*
    A comma causes a flip from object_value mode to object_key mode.
*/
                if (!_change_state(parser, KE))
                    return false;
                if (!_pop(parser, MODE_OBJECT_VALUE) || !_push(parser, MODE_OBJECT_KEY))
                    return false;
                break;
            case MODE_ARRAY:
                if (!_change_state(parser, VA))
                    return false;
                break;
            default:
                return false;
            }
            break;

/* : */ case -2:
/*
    A colon causes a flip from object_key mode to object_value mode.
*/
            if (!_change_state(parser, VA))
                return false;
            if (!_pop(parser, MODE_OBJECT_KEY) || !_push(parser, MODE_OBJECT_VALUE))
                return false;
            break;
/*
    Bad action.
*/
        default:
            return false;
        }
    }

    parser->char_index++;

    if (parser->config.json_str)
        parser->config.json_str_len++;

    return true;
}

json_value* json_parser_done(json_parser *parser)
{
/*
    The json_parser_done function should be called after all of the characters
    have been processed, but only if every call to json_parser_char returned
    true. This function returns a new json_value object if the JSON
    text was accepted.
*/
    json_value *result;

    if (parser->state != OK || parser->top != 0)
        return NULL;
    if (parser->stack[0].mode != MODE_DONE || !parser->stack[0].value)
        return NULL;

    result = parser->stack[0].value;
    _pop(parser, MODE_DONE);

    return result;
}

void json_parser_free(json_parser *parser)
{
    int i;

    if (!parser)
        return;

    for (i = parser->top; i >=0 ; --i) {
        json_free(parser->stack[i].value);
    }
    free(parser->stack);
    free(parser);
}

/*----------------------------------------------------------------------------*/

static json_value* _create_string_value(
    json_parser_config *config, 
    unsigned int index, 
    unsigned int len
    )
{
    if (config->json_str && index + len <= config->json_str_len) {
        return json_string_alloc(config->json_str + index, len);
    }
    return NULL;
}

static json_value* _create_number_value(
    json_parser_config *config, 
    unsigned int index, 
    unsigned int len
    )
{
    char tmp[50];
    double dbl;

    if (config->json_str && len > 0 && len < 50 && index + len <= config->json_str_len) {
        memcpy(tmp, config->json_str + index, len);
        tmp[len] = '\0';
        dbl = atof(tmp);
        return json_number_alloc(dbl);
    }
    return NULL;
}

static unsigned int _get_object_name(
    json_parser_config *config,
    unsigned int index,
    unsigned int len,
    char name[MAX_NAME_LEN]
    )
{
    if (config->json_str && len > 0 && len < MAX_NAME_LEN && index + len <= config->json_str_len) {
        memcpy(name, config->json_str + index, len);
        name[len] = '\0';
        return len;
    }
    return 0;
}

static int _push(json_parser *parser, modes mode)
{
/*
    Push a mode onto the stack. Return false if there is overflow.
*/
    json_value *v;
    json_parser_stack_item *top_stack_item;

    parser->top += 1;
    if (parser->top >= parser->depth) {
        return false;
    }

    top_stack_item = parser->stack + parser->top;
    top_stack_item->mode = mode;
    top_stack_item->value = NULL;
    top_stack_item->name_begin = 0;
    top_stack_item->name_len = 0;
    top_stack_item->value_begin = 0;

    if (mode == MODE_ARRAY) {
        v = json_array_alloc();
        if (!v)
            return false;
        top_stack_item->value = v;
    } else if (mode == MODE_OBJECT) {
        v = json_object_alloc();
        if (!v)
            return false;
        top_stack_item->value = v;
    }

    return true;
}

static int _pop(json_parser *parser, modes mode)
{
/*
    Pop the stack, assuring that the current mode matches the expectation.
    Return false if there is underflow or if the modes mismatch.
*/
    json_parser_stack_item *top_stack_item, *parent_stack_item;
    json_value *v, *parent;
    modes parent_mode;
    char name[MAX_NAME_LEN];

    if (parser->top < 0 || parser->stack[parser->top].mode != mode) {
        return false;
    }

    top_stack_item = parser->stack + parser->top;

    if (parser->top > 0) {
        parent_stack_item = parser->stack + parser->top - 1;
        v = top_stack_item->value;
        parent = parent_stack_item->value;
        parent_mode = parent_stack_item->mode;
        
        if (mode == MODE_ARRAY || mode == MODE_OBJECT) {
            assert(v);
            if (parent_mode == MODE_OBJECT_VALUE || parent_mode == MODE_DONE) {
                assert(parent == NULL);
                /* copy to parent level */
                parent_stack_item->value = v;
            } else if (parent_mode == MODE_ARRAY) {
                assert(parent && json_type(parent) == json_type_array);
                /* insert v into the array */
                if (!json_array_set(parent, json_array_size(parent), v))
                    return false;
            } else {
                assert(0);
                return false;
            }

        } else if (mode == MODE_OBJECT_KEY) {
            if (parent && parent_mode == MODE_OBJECT) {
                /* copy to parent level */
                parent_stack_item->name_begin = top_stack_item->name_begin;
                parent_stack_item->name_len = top_stack_item->name_len;
            } else {
                assert(0);
                return false;
            }

        } else if (mode == MODE_OBJECT_VALUE) {
            assert(v);
            if (parent && parent_mode == MODE_OBJECT) {
                /* insert v into the object */
                if (!_get_object_name(&parser->config, 
                        parent_stack_item->name_begin, 
                        parent_stack_item->name_len, 
                        name)
                    )
                    return false;
                if (!json_object_set(parent, name, v))
                    return false;
            } else {
                assert(0);
                return false;
            }
        }
    }

    memset(top_stack_item, 0, sizeof(json_parser_stack_item));
    parser->top -= 1;
    
    return true;
}

static int _change_state(json_parser *parser, int next_state)
{
    json_parser_stack_item *top_stack_item = parser->stack + parser->top;
    int value_end = 0;
    json_value *v = NULL, *parent;

    if (next_state == OK) {
        if (parser->state == N3) {
            /* null */
            value_end = 1;
            v = json_null_alloc();
        } else if (parser->state == T3) {
            /* true */
            value_end = 1;
            v = json_boolean_alloc(1);
        } else if (parser->state == F4) {
            /* false */
            value_end = 1;
            v = json_boolean_alloc(0);
        } else if (parser->state == ST) {
            /* end of string in object_value or array */
            assert(top_stack_item->value_begin > 0);
            value_end = 1;
            v = _create_string_value(
                    &parser->config, 
                    top_stack_item->value_begin, 
                    parser->char_index - top_stack_item->value_begin
                );
        }
    } else if (next_state == ST) {
        if (parser->state == OB || parser->state == KE) {
            /* begin of string in object_name */
            assert(top_stack_item->mode == MODE_OBJECT_KEY);
            top_stack_item->name_begin = parser->char_index + 1;
        } else if (parser->state == VA) {
            /* begin of string in object_value */
            assert(top_stack_item->mode == MODE_OBJECT_VALUE);
            top_stack_item->value_begin = parser->char_index + 1;
        } else if (parser->state == AR) {
            /* begin of string in array */
            assert(top_stack_item->mode == MODE_ARRAY);
            top_stack_item->value_begin = parser->char_index + 1;
        }
    } else if (next_state == CO) {
        if (parser->state == ST) {
            /* end of string in object_name */
            assert(top_stack_item->mode == MODE_OBJECT_KEY);
            top_stack_item->name_len = parser->char_index - top_stack_item->name_begin;
        }
    }

    if (parser->state == VA || parser->state == AR) {
        if (next_state == MI || next_state == ZE || next_state == IN) {
            /* begin of number */
            assert(top_stack_item->mode == MODE_OBJECT_VALUE || top_stack_item->mode == MODE_ARRAY);
            top_stack_item->value_begin = parser->char_index;
        }
    } else if (parser->state == FR || parser->state == IN || parser->state == ZE || parser->state == E3) {
        if (next_state == OK || next_state == KE || next_state == VA) {
            /* end of number */
            assert(top_stack_item->value_begin > 0);
            value_end = 1;
            v = _create_number_value(
                    &parser->config,
                    top_stack_item->value_begin,
                    parser->char_index - top_stack_item->value_begin
                );
        }
    }

    if (value_end) {
        if (!v)
            return false;
        
        if (top_stack_item->mode == MODE_ARRAY) {
            /* insert v into the array */
            parent = top_stack_item->value;
            assert(parent && json_type(parent) == json_type_array);
            if (!json_array_set(parent, json_array_size(parent), v))
                return false;
        
        } else if (top_stack_item->mode == MODE_OBJECT_VALUE) {
            /* record v into the stack */
            assert(top_stack_item->value == NULL);
            top_stack_item->value = v;
        
        } else {
            assert(0);
            json_free(v);
            return false;
        }
    }

    parser->state = next_state;
    return true;
}
