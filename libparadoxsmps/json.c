#define _POSIX_C_SOURCE 200809L // for strdup with strict -std=c99

#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PJValue {
    PJType type;
    union {
        bool boolean;
        double number;
        char *string;
        struct {
            PJValue **items;
            size_t count, capacity;
        } array;
        struct {
            char **keys;
            PJValue **values;
            size_t count, capacity;
        } object;
    } as;
};

// --- Construction ---

static PJValue *pj_alloc(PJType type) {
    PJValue *v = (PJValue *)calloc(1, sizeof(PJValue));
    v->type = type;
    return v;
}

PJValue *pj_new_null(void) { return pj_alloc(PJ_NULL); }

PJValue *pj_new_bool(bool b) {
    PJValue *v = pj_alloc(PJ_BOOL);
    v->as.boolean = b;
    return v;
}

PJValue *pj_new_number(double n) {
    PJValue *v = pj_alloc(PJ_NUMBER);
    v->as.number = n;
    return v;
}

PJValue *pj_new_string(const char *s) {
    PJValue *v = pj_alloc(PJ_STRING);
    v->as.string = s ? strdup(s) : strdup("");
    return v;
}

PJValue *pj_new_array(void) { return pj_alloc(PJ_ARRAY); }
PJValue *pj_new_object(void) { return pj_alloc(PJ_OBJECT); }

void pj_free(PJValue *v) {
    if (!v)
        return;
    switch (v->type) {
    case PJ_STRING:
        free(v->as.string);
        break;
    case PJ_ARRAY:
        for (size_t i = 0; i < v->as.array.count; i++)
            pj_free(v->as.array.items[i]);
        free(v->as.array.items);
        break;
    case PJ_OBJECT:
        for (size_t i = 0; i < v->as.object.count; i++) {
            free(v->as.object.keys[i]);
            pj_free(v->as.object.values[i]);
        }
        free(v->as.object.keys);
        free(v->as.object.values);
        break;
    default:
        break;
    }
    free(v);
}

// --- Array ---

void pj_array_append(PJValue *arr, PJValue *item) {
    if (!arr || arr->type != PJ_ARRAY) {
        pj_free(item);
        return;
    }
    if (arr->as.array.count == arr->as.array.capacity) {
        arr->as.array.capacity = arr->as.array.capacity ? arr->as.array.capacity * 2 : 4;
        arr->as.array.items = (PJValue **)realloc(arr->as.array.items, arr->as.array.capacity * sizeof(PJValue *));
    }
    arr->as.array.items[arr->as.array.count++] = item;
}

size_t pj_array_size(const PJValue *arr) { return (arr && arr->type == PJ_ARRAY) ? arr->as.array.count : 0; }

PJValue *pj_array_get(const PJValue *arr, size_t index) {
    if (!arr || arr->type != PJ_ARRAY || index >= arr->as.array.count)
        return NULL;
    return arr->as.array.items[index];
}

// --- Object (linear, insertion-ordered -- see json.h's own comment on why) ---

void pj_object_set(PJValue *obj, const char *key, PJValue *value) {
    if (!obj || obj->type != PJ_OBJECT) {
        pj_free(value);
        return;
    }
    for (size_t i = 0; i < obj->as.object.count; i++) {
        if (strcmp(obj->as.object.keys[i], key) == 0) {
            pj_free(obj->as.object.values[i]);
            obj->as.object.values[i] = value;
            return;
        }
    }
    if (obj->as.object.count == obj->as.object.capacity) {
        obj->as.object.capacity = obj->as.object.capacity ? obj->as.object.capacity * 2 : 4;
        obj->as.object.keys = (char **)realloc(obj->as.object.keys, obj->as.object.capacity * sizeof(char *));
        obj->as.object.values =
            (PJValue **)realloc(obj->as.object.values, obj->as.object.capacity * sizeof(PJValue *));
    }
    obj->as.object.keys[obj->as.object.count] = strdup(key);
    obj->as.object.values[obj->as.object.count] = value;
    obj->as.object.count++;
}

bool pj_object_has(const PJValue *obj, const char *key) { return pj_object_get(obj, key) != NULL; }

PJValue *pj_object_get(const PJValue *obj, const char *key) {
    if (!obj || obj->type != PJ_OBJECT)
        return NULL;
    for (size_t i = 0; i < obj->as.object.count; i++)
        if (strcmp(obj->as.object.keys[i], key) == 0)
            return obj->as.object.values[i];
    return NULL;
}

void pj_object_remove(PJValue *obj, const char *key) {
    if (!obj || obj->type != PJ_OBJECT)
        return;
    for (size_t i = 0; i < obj->as.object.count; i++) {
        if (strcmp(obj->as.object.keys[i], key) == 0) {
            free(obj->as.object.keys[i]);
            pj_free(obj->as.object.values[i]);
            for (size_t j = i + 1; j < obj->as.object.count; j++) {
                obj->as.object.keys[j - 1] = obj->as.object.keys[j];
                obj->as.object.values[j - 1] = obj->as.object.values[j];
            }
            obj->as.object.count--;
            return;
        }
    }
}

size_t pj_object_size(const PJValue *obj) { return (obj && obj->type == PJ_OBJECT) ? obj->as.object.count : 0; }

const char *pj_object_key_at(const PJValue *obj, size_t index) {
    if (!obj || obj->type != PJ_OBJECT || index >= obj->as.object.count)
        return NULL;
    return obj->as.object.keys[index];
}

PJValue *pj_object_value_at(const PJValue *obj, size_t index) {
    if (!obj || obj->type != PJ_OBJECT || index >= obj->as.object.count)
        return NULL;
    return obj->as.object.values[index];
}

const char *pj_object_first_key(const PJValue *obj) { return pj_object_key_at(obj, 0); }

// --- Accessors ---

PJType pj_type(const PJValue *v) { return v ? v->type : PJ_NULL; }

const char *pj_get_string(const PJValue *v, const char *fallback) {
    return (v && v->type == PJ_STRING) ? v->as.string : fallback;
}

double pj_get_number(const PJValue *v, double fallback) { return (v && v->type == PJ_NUMBER) ? v->as.number : fallback; }

bool pj_get_bool(const PJValue *v, bool fallback) { return (v && v->type == PJ_BOOL) ? v->as.boolean : fallback; }

// ---------------------------------------------------------------------------
// JSONC comment stripping -- mirrors tools/paradoxsmps/json_to_header.py's
// strip_jsonc_comments() exactly (line comments, block comments, respecting
// quoted strings so a ';' or '//' inside a note name string is never
// misread -- this format never needs anything more exotic than \" escapes).
// ---------------------------------------------------------------------------

static char *strip_jsonc_comments(const char *text) {
    size_t len = strlen(text);
    char *out = (char *)malloc(len + 1);
    size_t o = 0;
    bool in_string = false;
    size_t i = 0;
    while (i < len) {
        char c = text[i];
        if (in_string) {
            out[o++] = c;
            if (c == '\\' && i + 1 < len) {
                out[o++] = text[i + 1];
                i += 2;
                continue;
            }
            if (c == '"')
                in_string = false;
            i++;
            continue;
        }
        if (c == '"') {
            in_string = true;
            out[o++] = c;
            i++;
            continue;
        }
        if (c == '/' && i + 1 < len && text[i + 1] == '/') {
            while (i < len && text[i] != '\n')
                i++;
            continue;
        }
        if (c == '/' && i + 1 < len && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(text[i] == '*' && text[i + 1] == '/'))
                i++;
            i += 2;
            continue;
        }
        out[o++] = c;
        i++;
    }
    out[o] = '\0';
    return out;
}

// ---------------------------------------------------------------------------
// Parser -- standard recursive-descent JSON, operating on the
// comment-stripped buffer.
// ---------------------------------------------------------------------------

typedef struct {
    const char *s;
    size_t pos, len;
    char *errbuf;
    size_t errbuf_size;
    bool failed;
} Parser;

static void skip_ws(Parser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->s[p->pos]))
        p->pos++;
}

static void set_error(Parser *p, const char *msg) {
    if (p->failed)
        return;
    p->failed = true;
    if (p->errbuf && p->errbuf_size)
        snprintf(p->errbuf, p->errbuf_size, "%s at offset %zu", msg, p->pos);
}

static PJValue *parse_value(Parser *p);

static bool parse_literal(Parser *p, const char *lit) {
    size_t n = strlen(lit);
    if (p->pos + n <= p->len && strncmp(p->s + p->pos, lit, n) == 0) {
        p->pos += n;
        return true;
    }
    return false;
}

static char *parse_string_raw(Parser *p) {
    if (p->s[p->pos] != '"') {
        set_error(p, "expected string");
        return NULL;
    }
    p->pos++;
    size_t start = p->pos;
    size_t cap = 32, len = 0;
    char *buf = (char *)malloc(cap);
    while (p->pos < p->len && p->s[p->pos] != '"') {
        char c = p->s[p->pos];
        if (c == '\\' && p->pos + 1 < p->len) {
            p->pos++;
            char esc = p->s[p->pos];
            char decoded;
            switch (esc) {
            case 'n':
                decoded = '\n';
                break;
            case 't':
                decoded = '\t';
                break;
            case 'r':
                decoded = '\r';
                break;
            case '"':
                decoded = '"';
                break;
            case '\\':
                decoded = '\\';
                break;
            case '/':
                decoded = '/';
                break;
            case 'b':
                decoded = '\b';
                break;
            case 'f':
                decoded = '\f';
                break;
            case 'u': {
                // \uXXXX -- this schema never needs non-ASCII, but parse and
                // pass through as best-effort UTF-8 for a codepoint <= 0xFFFF
                // rather than crashing on it.
                if (p->pos + 4 < p->len) {
                    char hex[5] = {p->s[p->pos + 1], p->s[p->pos + 2], p->s[p->pos + 3], p->s[p->pos + 4], 0};
                    unsigned int cp = (unsigned int)strtoul(hex, NULL, 16);
                    p->pos += 4;
                    if (len + 4 >= cap) {
                        cap *= 2;
                        buf = (char *)realloc(buf, cap);
                    }
                    if (cp < 0x80) {
                        buf[len++] = (char)cp;
                    } else if (cp < 0x800) {
                        buf[len++] = (char)(0xC0 | (cp >> 6));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[len++] = (char)(0xE0 | (cp >> 12));
                        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    }
                    p->pos++;
                    continue;
                }
                decoded = 'u';
                break;
            }
            default:
                decoded = esc;
            }
            if (len + 1 >= cap) {
                cap *= 2;
                buf = (char *)realloc(buf, cap);
            }
            buf[len++] = decoded;
            p->pos++;
            continue;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = c;
        p->pos++;
    }
    if (p->pos >= p->len) {
        set_error(p, "unterminated string");
        free(buf);
        return NULL;
    }
    p->pos++; // closing quote
    (void)start;
    buf[len] = '\0';
    return buf;
}

static PJValue *parse_object(Parser *p) {
    PJValue *obj = pj_new_object();
    p->pos++; // '{'
    skip_ws(p);
    if (p->pos < p->len && p->s[p->pos] == '}') {
        p->pos++;
        return obj;
    }
    while (true) {
        skip_ws(p);
        char *key = parse_string_raw(p);
        if (!key) {
            pj_free(obj);
            return NULL;
        }
        skip_ws(p);
        if (p->pos >= p->len || p->s[p->pos] != ':') {
            set_error(p, "expected ':'");
            free(key);
            pj_free(obj);
            return NULL;
        }
        p->pos++;
        skip_ws(p);
        PJValue *val = parse_value(p);
        if (!val) {
            free(key);
            pj_free(obj);
            return NULL;
        }
        pj_object_set(obj, key, val);
        free(key);
        skip_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ',') {
            p->pos++;
            continue;
        }
        if (p->pos < p->len && p->s[p->pos] == '}') {
            p->pos++;
            break;
        }
        set_error(p, "expected ',' or '}'");
        pj_free(obj);
        return NULL;
    }
    return obj;
}

static PJValue *parse_array(Parser *p) {
    PJValue *arr = pj_new_array();
    p->pos++; // '['
    skip_ws(p);
    if (p->pos < p->len && p->s[p->pos] == ']') {
        p->pos++;
        return arr;
    }
    while (true) {
        skip_ws(p);
        PJValue *val = parse_value(p);
        if (!val) {
            pj_free(arr);
            return NULL;
        }
        pj_array_append(arr, val);
        skip_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ',') {
            p->pos++;
            continue;
        }
        if (p->pos < p->len && p->s[p->pos] == ']') {
            p->pos++;
            break;
        }
        set_error(p, "expected ',' or ']'");
        pj_free(arr);
        return NULL;
    }
    return arr;
}

static PJValue *parse_value(Parser *p) {
    skip_ws(p);
    if (p->pos >= p->len) {
        set_error(p, "unexpected end of input");
        return NULL;
    }
    char c = p->s[p->pos];
    if (c == '{')
        return parse_object(p);
    if (c == '[')
        return parse_array(p);
    if (c == '"') {
        char *s = parse_string_raw(p);
        if (!s)
            return NULL;
        PJValue *v = pj_new_string(s);
        free(s);
        return v;
    }
    if (parse_literal(p, "true"))
        return pj_new_bool(true);
    if (parse_literal(p, "false"))
        return pj_new_bool(false);
    if (parse_literal(p, "null"))
        return pj_new_null();
    if (c == '-' || isdigit((unsigned char)c)) {
        char *end = NULL;
        double n = strtod(p->s + p->pos, &end);
        if (end == p->s + p->pos) {
            set_error(p, "invalid number");
            return NULL;
        }
        p->pos = (size_t)(end - p->s);
        return pj_new_number(n);
    }
    set_error(p, "unexpected character");
    return NULL;
}

PJValue *pj_parse(const char *text, char *errbuf, size_t errbuf_size) {
    char *stripped = strip_jsonc_comments(text);
    Parser p = {stripped, 0, strlen(stripped), errbuf, errbuf_size, false};
    PJValue *result = parse_value(&p);
    if (result) {
        skip_ws(&p);
        if (p.pos != p.len) {
            set_error(&p, "trailing content after JSON value");
            pj_free(result);
            result = NULL;
        }
    }
    free(stripped);
    if (!result && errbuf && errbuf_size && errbuf[0] == '\0')
        snprintf(errbuf, errbuf_size, "parse error");
    return result;
}

// ---------------------------------------------------------------------------
// Serializer
// ---------------------------------------------------------------------------

typedef struct {
    char *buf;
    size_t len, cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->cap = 256;
    sb->len = 0;
    sb->buf = (char *)malloc(sb->cap);
    sb->buf[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        while (sb->len + n + 1 > sb->cap)
            sb->cap *= 2;
        sb->buf = (char *)realloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static void sb_append_char(StrBuf *sb, char c) {
    char s[2] = {c, '\0'};
    sb_append(sb, s);
}

static void sb_append_indent(StrBuf *sb, int depth) {
    for (int i = 0; i < depth; i++)
        sb_append(sb, "  ");
}

static void sb_append_json_string(StrBuf *sb, const char *s) {
    sb_append_char(sb, '"');
    for (const char *c = s; *c; c++) {
        switch (*c) {
        case '"':
            sb_append(sb, "\\\"");
            break;
        case '\\':
            sb_append(sb, "\\\\");
            break;
        case '\n':
            sb_append(sb, "\\n");
            break;
        case '\t':
            sb_append(sb, "\\t");
            break;
        case '\r':
            sb_append(sb, "\\r");
            break;
        default:
            sb_append_char(sb, *c);
        }
    }
    sb_append_char(sb, '"');
}

static void serialize_value(StrBuf *sb, const PJValue *v, bool pretty, int depth) {
    if (!v) {
        sb_append(sb, "null");
        return;
    }
    char numbuf[64];
    switch (v->type) {
    case PJ_NULL:
        sb_append(sb, "null");
        break;
    case PJ_BOOL:
        sb_append(sb, v->as.boolean ? "true" : "false");
        break;
    case PJ_NUMBER:
        // Integral values print without a trailing ".0" -- this schema's
        // own numbers (durations, offsets, etc.) are always whole numbers.
        if (v->as.number == (double)(long long)v->as.number)
            snprintf(numbuf, sizeof(numbuf), "%lld", (long long)v->as.number);
        else
            snprintf(numbuf, sizeof(numbuf), "%g", v->as.number);
        sb_append(sb, numbuf);
        break;
    case PJ_STRING:
        sb_append_json_string(sb, v->as.string);
        break;
    case PJ_ARRAY:
        if (v->as.array.count == 0) {
            sb_append(sb, "[]");
            break;
        }
        sb_append_char(sb, '[');
        if (pretty)
            sb_append_char(sb, '\n');
        for (size_t i = 0; i < v->as.array.count; i++) {
            if (pretty)
                sb_append_indent(sb, depth + 1);
            serialize_value(sb, v->as.array.items[i], pretty, depth + 1);
            if (i + 1 < v->as.array.count)
                sb_append_char(sb, ',');
            if (pretty)
                sb_append_char(sb, '\n');
        }
        if (pretty)
            sb_append_indent(sb, depth);
        sb_append_char(sb, ']');
        break;
    case PJ_OBJECT:
        if (v->as.object.count == 0) {
            sb_append(sb, "{}");
            break;
        }
        sb_append_char(sb, '{');
        if (pretty)
            sb_append_char(sb, '\n');
        for (size_t i = 0; i < v->as.object.count; i++) {
            if (pretty)
                sb_append_indent(sb, depth + 1);
            sb_append_json_string(sb, v->as.object.keys[i]);
            sb_append(sb, pretty ? ": " : ":");
            serialize_value(sb, v->as.object.values[i], pretty, depth + 1);
            if (i + 1 < v->as.object.count)
                sb_append_char(sb, ',');
            if (pretty)
                sb_append_char(sb, '\n');
        }
        if (pretty)
            sb_append_indent(sb, depth);
        sb_append_char(sb, '}');
        break;
    }
}

char *pj_serialize(const PJValue *v, bool pretty) {
    StrBuf sb;
    sb_init(&sb);
    serialize_value(&sb, v, pretty, 0);
    return sb.buf;
}
