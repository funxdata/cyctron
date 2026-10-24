// src/event_format.c
#include "event_format.h"
#include "event_pusher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void event_escape_json(const char *in, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (!in) { out[0] = '\0'; return; }

    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        const char *rep = NULL;
        switch (*p) {
            case '"':  rep = "\\\""; break;
            case '\\': rep = "\\\\"; break;
            case '\n': rep = "\\n";  break;
            case '\r': rep = "\\r";  break;
            case '\t': rep = "\\t";  break;
            default:
                if (*p < 0x20) continue;
                break;
        }
        if (rep) {
            size_t n = strlen(rep);
            if (o + n >= out_size) break;
            memcpy(out + o, rep, n);
            o += n;
        } else {
            if (o + 1 >= out_size) break;
            out[o++] = (char)*p;
        }
    }
    out[o] = '\0';
}

char *event_escape_alloc(const char *in) {
    if (!in) in = "";
    size_t cap = strlen(in) * 2 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    event_escape_json(in, out, cap);
    return out;
}

int event_format_with_type(const char *type, const char *data, char **out) {
    if (!out) return -1;
    *out = NULL;
    if (!type || !data) return -1;

    char *etype = event_escape_alloc(type);
    if (!etype) return -1;

    size_t need = strlen(etype) + strlen(data) + 64;
    char *buf = (char *)malloc(need);
    if (!buf) {
        free(etype);
        return -1;
    }

    snprintf(buf, need,
             "{\"type\":\"%s\",\"data\":%s,\"timestamp\":%ld}",
             etype, data, (long)time(NULL));

    free(etype);
    *out = buf;
    return 0;
}

int event_format_message(const char *msg, char **out) {
    if (!out) return -1;
    *out = NULL;
    if (!msg) return -1;

    char *emsg = event_escape_alloc(msg);
    if (!emsg) return -1;

    size_t need = strlen(emsg) + 64;
    char *buf = (char *)malloc(need);
    if (!buf) {
        free(emsg);
        return -1;
    }

    snprintf(buf, need,
             "{\"type\":\"message\",\"message\":\"%s\",\"timestamp\":%ld}",
             emsg, (long)time(NULL));

    free(emsg);
    *out = buf;
    return 0;
}

int event_format_json_string(const char *type, const char *str, char **out) {
    if (!out) return -1;
    *out = NULL;
    if (!type || !str) return -1;

    char *etype = event_escape_alloc(type);
    char *estr = event_escape_alloc(str);
    if (!etype || !estr) {
        free(etype);
        free(estr);
        return -1;
    }

    size_t need = strlen(etype) + strlen(estr) + 64;
    char *buf = (char *)malloc(need);
    if (!buf) {
        free(etype);
        free(estr);
        return -1;
    }

    snprintf(buf, need,
             "{\"type\":\"%s\",\"data\":\"%s\",\"timestamp\":%ld}",
             etype, estr, (long)time(NULL));

    free(etype);
    free(estr);
    *out = buf;
    return 0;
}
