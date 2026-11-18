// src/core/json_validate.c
// 轻量 JSON 结构校验：不分配内存，只判断输入是否为单个完整 JSON 值。

#include "json_validate.h"

#include <ctype.h>
#include <string.h>

#define JSON_MAX_DEPTH 64

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

static int parse_string(const char **pp, const char *end) {
    const char *p = *pp;
    if (p >= end || *p != '"') return 0;
    p++;
    while (p < end) {
        unsigned char c = (unsigned char) *p;
        if (c == '"') {
            *pp = p + 1;
            return 1;
        }
        if (c < 0x20) return 0;
        if (c == '\\') {
            p++;
            if (p >= end) return 0;
            char e = *p;
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' ||
                e == 'n' || e == 'r' || e == 't') {
                p++;
            } else if (e == 'u') {
                p++;
                for (int i = 0; i < 4; i++, p++) {
                    if (p >= end || !isxdigit((unsigned char) *p)) return 0;
                }
            } else {
                return 0;
            }
        } else {
            p++;
        }
    }
    return 0;
}

static int parse_number(const char **pp, const char *end) {
    const char *p = *pp;
    if (p < end && *p == '-') p++;
    if (p >= end) return 0;

    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        p++;
        while (p < end && isdigit((unsigned char) *p)) p++;
    } else {
        return 0;
    }

    if (p < end && *p == '.') {
        p++;
        if (p >= end || !isdigit((unsigned char) *p)) return 0;
        while (p < end && isdigit((unsigned char) *p)) p++;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (p >= end || !isdigit((unsigned char) *p)) return 0;
        while (p < end && isdigit((unsigned char) *p)) p++;
    }

    *pp = p;
    return 1;
}

static int parse_value(const char **pp, const char *end, int depth);

static int parse_object(const char **pp, const char *end, int depth) {
    const char *p = *pp;
    if (p >= end || *p != '{') return 0;
    p++;
    p = skip_ws(p, end);
    if (p < end && *p == '}') {
        *pp = p + 1;
        return 1;
    }

    for (;;) {
        p = skip_ws(p, end);
        if (!parse_string(&p, end)) return 0;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return 0;
        p++;
        if (!parse_value(&p, end, depth + 1)) return 0;
        p = skip_ws(p, end);
        if (p >= end) return 0;
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            *pp = p + 1;
            return 1;
        }
        return 0;
    }
}

static int parse_array(const char **pp, const char *end, int depth) {
    const char *p = *pp;
    if (p >= end || *p != '[') return 0;
    p++;
    p = skip_ws(p, end);
    if (p < end && *p == ']') {
        *pp = p + 1;
        return 1;
    }

    for (;;) {
        if (!parse_value(&p, end, depth + 1)) return 0;
        p = skip_ws(p, end);
        if (p >= end) return 0;
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == ']') {
            *pp = p + 1;
            return 1;
        }
        return 0;
    }
}

static int parse_value(const char **pp, const char *end, int depth) {
    if (depth > JSON_MAX_DEPTH) return 0;

    const char *p = skip_ws(*pp, end);
    if (p >= end) return 0;

    switch (*p) {
        case '{':
            if (!parse_object(&p, end, depth)) return 0;
            *pp = p;
            return 1;
        case '[':
            if (!parse_array(&p, end, depth)) return 0;
            *pp = p;
            return 1;
        case '"':
            if (!parse_string(&p, end)) return 0;
            *pp = p;
            return 1;
        case 't':
            if (end - p >= 4 && memcmp(p, "true", 4) == 0) {
                *pp = p + 4;
                return 1;
            }
            return 0;
        case 'f':
            if (end - p >= 5 && memcmp(p, "false", 5) == 0) {
                *pp = p + 5;
                return 1;
            }
            return 0;
        case 'n':
            if (end - p >= 4 && memcmp(p, "null", 4) == 0) {
                *pp = p + 4;
                return 1;
            }
            return 0;
        default:
            if (!parse_number(&p, end)) return 0;
            *pp = p;
            return 1;
    }
}

int json_validate(const char *s, size_t len) {
    if (!s || len == 0) return 0;

    const char *p = s;
    const char *end = s + len;
    if (!parse_value(&p, end, 0)) return 0;
    p = skip_ws(p, end);
    return p == end;
}
