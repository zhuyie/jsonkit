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
#ifdef _MSC_VER
/* warning C4996: '_snprintf': This function or variable may be unsafe. Consider using _snprintf_s instead. 
   To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details. */
  #define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <string.h>
#include <assert.h>

/*----------------------------------------------------------------------------*/

#define BUF_LEN 4096

typedef struct context {
    json_write_config *config;
    int written;
    int level;
    unsigned int buf_size;
    char buf[BUF_LEN];
} context;

static int _json_write(json_value *v, context *ctx);
static int _flush(context *ctx);

int json_write(json_value *v, json_write_config config)
{
    context ctx;

    assert(v);
    assert(config.write);

    ctx.config = &config;
    ctx.written = 0;
    ctx.level = 0;
    ctx.buf_size = 0;
    
    if (!_json_write(v, &ctx))
        return 0;

    if (!_flush(&ctx))
        return 0;

    return ctx.written;
}

/*----------------------------------------------------------------------------*/

static int _write(const char *str, int len, context *ctx)
{
    int n;

    while (len) {
        if (ctx->buf_size == BUF_LEN) {
            if (ctx->config->write(ctx->buf, BUF_LEN) != BUF_LEN)
                return 0;
            ctx->buf_size = 0;
            ctx->written += BUF_LEN;
        }

        n = BUF_LEN - ctx->buf_size;
        if (n > len)
            n = len;
        
        memcpy(ctx->buf + ctx->buf_size, str, n);
        str += n;
        len -= n;
        ctx->buf_size += n;
        ctx->written += n;
    }

    return 1;
}

static int _flush(context *ctx)
{
    if (ctx->buf_size) {
        if (ctx->config->write(ctx->buf, ctx->buf_size) != ctx->buf_size)
            return 0;
        ctx->written += ctx->buf_size;
        ctx->buf_size = 0;
    }
    return 1;
}

static int _write_lineend_indent(context *ctx)
{
    int res;

    if (ctx->config->compact)
        return 1;

    res = ctx->config->crlf ? _write("\r\n", 2, ctx) : _write("\n", 1, ctx);
    if (!res)
        return res;

    if (ctx->config->indent > 0 && ctx->level > 0) {
        static const char* indent_str[8] = {
            " ",
            "  ",
            "   ",
            "    ",
            "     ",
            "      ",
            "       ",
            "        "
        };
        int count = ctx->config->indent * ctx->level, i;
        if (count <= 8) {
            res = _write(indent_str[count - 1], count, ctx);
        } else if (ctx->config->indent <= 8) {
            for (i = 0; i < ctx->level; ++i) {
                res = _write(indent_str[ctx->config->indent - 1], ctx->config->indent, ctx);
                if (!res)
                    break;
            }
        } else {
            for (i = 0; i < count; ++i) {
                res = _write(" ", 1, ctx);
                if (!res)
                    break;
            }
        }
    }

    return res;
}

static int _write_string(const char *string, context *ctx)
{
    const char *p = string;
    char c[2];

    if (!_write("\"", 1, ctx))
        return 0;
    
    /* escaping */
    c[0] = '\\';
    while ((c[1] = *p++)) {
        switch (c[1]) {
        case '\"':
        case '\\':
        case '/':
            goto MyLabel;
        case '\b':
            c[1] = 'b';
            goto MyLabel;
        case '\f':
            c[1] = 'f';
            goto MyLabel;
        case '\n':
            c[1] = 'n';
            goto MyLabel;
        case '\r':
            c[1] = 'r';
            goto MyLabel;
        case '\t':
            c[1] = 't';
            
        MyLabel:
            if (string + 1 != p) {
                if (!_write(string, (int)(p - 1 - string), ctx))
                    return 0;
            }
            if (!_write(c, 2, ctx))
                return 0;
            string = p; 
            
            break;
        }
    }

    if (string + 1 != p) {
        if (!_write(string, (int)(p - 1 - string), ctx))
            return 0;
    }

    if (!_write("\"", 1, ctx))
        return 0;
    
    return 1;
}

static int _write_number(double number, context *ctx)
{
    char tmp[30];
    int len;
    
#ifdef _MSC_VER
    len = _snprintf(tmp, sizeof(tmp), "%f", number);
#else
    len = snprintf(tmp, sizeof(tmp), "%f", number);
#endif
    if (len <= 0)
        return 0;

    /* truncate trailing zeros */
    while (len) {
        if (tmp[len - 1] != '0' && tmp[len - 1] != '.')
            break;
        len--;
    }
    if (!len)  /* 0.000000 */
        len = 1;

    return _write(tmp, len, ctx);
}

static int _write_object(json_value *v, context *ctx)
{
    unsigned int size, i;
    const char *name;
    json_value *value;

    if (!_write("{", 1, ctx))
        return 0;

    size = json_object_size(v);
    if (size > 0) {
        ctx->level += 1;

        if (!_write_lineend_indent(ctx))
            return 0;

        for (i = 0; i < size; ++i) {
            name = json_object_name_by_index(v, i);
            assert(name);
            value = json_object_value_by_index(v, i);
            assert(value);
            if (!_write_string(name, ctx))
                return 0;
            if (!_write(":", 1, ctx))
                return 0;
            if (!ctx->config->compact && !_write(" ", 1, ctx))
                return 0;
            if (!_json_write(value, ctx))
                return 0;

            if (i < size - 1) {
                if (!_write(",", 1, ctx))
                    return 0;
                if (!_write_lineend_indent(ctx))
                    return 0;
            }
        }

        ctx->level -= 1;

        if (!_write_lineend_indent(ctx))
            return 0;
    }

    if (!_write("}", 1, ctx))
        return 0;

    return 1;
}

static int _write_array(json_value *v, context *ctx)
{
    unsigned int size, i;
    json_value *child;

    if (!_write("[", 1, ctx))
        return 0;

    size = json_array_size(v);
    if (size > 0) {
        ctx->level += 1;

        if (!_write_lineend_indent(ctx))
            return 0;

        for (i = 0; i < size; ++i) {
            child = json_array_get(v, i);
            assert(child);
            if (!_json_write(child, ctx))
                return 0;

            if (i < size - 1) {
                if (!_write(",", 1, ctx))
                    return 0;
                if (!_write_lineend_indent(ctx))
                    return 0;
            }
        }
        
        ctx->level -= 1;

        if (!_write_lineend_indent(ctx))
            return 0;
    }

    if (!_write("]", 1, ctx))
        return 0;

    return 1;
}

static int _json_write(json_value *v, context *ctx)
{
    json_value_type t;
    int res = 0;

    t = json_type(v);
    switch (t)
    {
    case json_type_string:
        res = _write_string(json_string_get(v), ctx);
        break;

    case json_type_number:
        res = _write_number(json_number_get(v), ctx);
        break;

    case json_type_true:
        res = _write("true", 4, ctx);
        break;

    case json_type_false:
        res = _write("false", 5, ctx);
        break;

    case json_type_null:
        res = _write("null", 4, ctx);
        break;

    case json_type_object:
        res = _write_object(v, ctx);
        break;

    case json_type_array:
        res = _write_array(v, ctx);
        break;

    default:
        assert(0);
    }

    return res;
}
