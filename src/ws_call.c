// src/ws_call.c
#include "ws_call.h"
#include "mongoose.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int ws_safe_name(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

static int is_object_or_null(const char *s) {
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '{') return 1;
    if (strncmp(s, "null", 4) == 0) {
        const char *p = s + 4;
        while (*p && isspace((unsigned char)*p)) p++;
        return *p == 0;
    }
    return 0;
}

static void set_err(char *err, size_t size, const char *msg) {
    if (err && size) snprintf(err, size, "%s", msg ? msg : "");
}

int ws_call_parse(const char *payload, size_t len, ws_call_req_t *out,
                  char *err, size_t err_size) {
    if (!out) return WS_CALL_ERR_PARAM;
    memset(out, 0, sizeof(*out));
    if (err && err_size) err[0] = 0;

    if (!payload || len == 0 || len > WS_CALL_MAX_PAYLOAD) {
        set_err(err, err_size, "invalid payload size");
        return WS_CALL_ERR_PARAM;
    }

    struct mg_str json = mg_str_n(payload, len);
    char *id = mg_json_get_str(json, "$.id");
    char *lib = mg_json_get_str(json, "$.lib");
    char *symbol = mg_json_get_str(json, "$.symbol");

    /* Keep id (if any) so the caller can build an error response. */
    out->id = id;

    if (!id || !lib || !symbol) {
        free(lib);
        free(symbol);
        set_err(err, err_size, "missing id/lib/symbol");
        return WS_CALL_ERR_PARAM;
    }

    if (strlen(id) > WS_CALL_MAX_NAME || strlen(lib) > WS_CALL_MAX_NAME ||
        strlen(symbol) > WS_CALL_MAX_NAME) {
        free(lib);
        free(symbol);
        set_err(err, err_size, "id/lib/symbol too long");
        return WS_CALL_ERR_PARAM;
    }

    if (!ws_safe_name(lib) || !ws_safe_name(symbol)) {
        free(lib);
        free(symbol);
        set_err(err, err_size, "invalid lib or symbol");
        return WS_CALL_ERR_DENIED;
    }

    struct mg_str args_tok = mg_json_get_tok(json, "$.args");
    char *args = NULL;
    if (args_tok.len > 0 && args_tok.buf) {
        args = (char *)malloc(args_tok.len + 1);
        if (args) {
            memcpy(args, args_tok.buf, args_tok.len);
            args[args_tok.len] = 0;
        }
    }
    if (!args) args = dup_str("{}");
    if (!args) {
        free(lib);
        free(symbol);
        set_err(err, err_size, "out of memory");
        return WS_CALL_ERR_PARAM;
    }

    if (!is_object_or_null(args)) {
        free(args);
        free(lib);
        free(symbol);
        set_err(err, err_size, "args must be an object or null");
        return WS_CALL_ERR_PARAM;
    }

    out->lib = lib;
    out->symbol = symbol;
    out->args = args;
    out->timeout_ms = mg_json_get_long(json, "$.timeout_ms", 0);
    return WS_CALL_OK;
}

static char *json_escape_dup(const char *in) {
    if (!in) in = "";
    size_t len = strlen(in);
    size_t cap = len * 2 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    const char bs = (char)92; /* backslash */
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == 34) {          /* double quote */
            out[o++] = bs;
            out[o++] = '"';
        } else if (c == 92) {   /* backslash */
            out[o++] = bs;
            out[o++] = bs;
        } else if (c == 10) {   /* LF */
            out[o++] = bs;
            out[o++] = 'n';
        } else if (c == 13) {   /* CR */
            out[o++] = bs;
            out[o++] = 'r';
        } else if (c == 9) {    /* TAB */
            out[o++] = bs;
            out[o++] = 't';
        } else if (c < 0x20) {
            continue;
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = 0;
    return out;
}

int ws_call_build_ok(const char *id, const char *lib, const char *symbol,
                     unsigned long long elapsed_ms, const char *result_json,
                     ws_call_resp_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    char *eid = json_escape_dup(id ? id : "");
    char *elib = json_escape_dup(lib ? lib : "");
    char *esym = json_escape_dup(symbol ? symbol : "");
    if (!eid || !elib || !esym) {
        free(eid);
        free(elib);
        free(esym);
        return -1;
    }

    const char *result = (result_json && result_json[0]) ? result_json : "null";
    size_t need = strlen(eid) + strlen(elib) + strlen(esym) + strlen(result) + 256;
    char *buf = (char *)malloc(need);
    if (!buf) {
        free(eid);
        free(elib);
        free(esym);
        return -1;
    }

    snprintf(buf, need,
             "{\"type\":\"call\",\"id\":\"%s\",\"ok\":true,\"code\":0,"
             "\"lib\":\"%s\",\"symbol\":\"%s\",\"elapsed_ms\":%llu,"
             "\"result\":%s,\"timestamp\":%ld}",
             eid, elib, esym, elapsed_ms, result, (long)time(NULL));

    out->json = buf;
    out->code = WS_CALL_OK;

    free(eid);
    free(elib);
    free(esym);
    return 0;
}

int ws_call_build_error(const char *id, int code, const char *message,
                        ws_call_resp_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    char *eid = json_escape_dup(id ? id : "");
    char *emsg = json_escape_dup(message ? message : "");
    if (!eid || !emsg) {
        free(eid);
        free(emsg);
        return -1;
    }

    size_t need = strlen(eid) + strlen(emsg) + 160;
    char *buf = (char *)malloc(need);
    if (!buf) {
        free(eid);
        free(emsg);
        return -1;
    }

    snprintf(buf, need,
             "{\"type\":\"call\",\"id\":\"%s\",\"ok\":false,\"code\":%d,"
             "\"error\":\"%s\",\"timestamp\":%ld}",
             eid, code, emsg, (long)time(NULL));

    out->json = buf;
    out->code = code;

    free(eid);
    free(emsg);
    return 0;
}

int ws_call_map_rc(int ffi_rc) {
    switch (ffi_rc) {
        case 0:    return WS_CALL_OK;
        case -100: return WS_CALL_ERR_PARAM;
        case -1:   return WS_CALL_ERR_LIB;
        case -2:   return WS_CALL_ERR_SYMBOL;
        default:   return WS_CALL_ERR_INVOKE;
    }
}

void ws_call_req_free(ws_call_req_t *req) {
    if (!req) return;
    free(req->id);
    free(req->lib);
    free(req->symbol);
    free(req->args);
    memset(req, 0, sizeof(*req));
}

void ws_call_resp_free(ws_call_resp_t *resp) {
    if (!resp) return;
    free(resp->json);
    memset(resp, 0, sizeof(*resp));
}
