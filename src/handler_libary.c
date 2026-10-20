#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "calldyn.h"
#include "global.h"

// 解析 query string 中的数字参数（如 ?id=2），成功返回 0
static int query_get_long(const char *query, const char *key, long *out) {
    size_t klen = strlen(key);
    const char *p = query;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            char *end = NULL;
            long v = strtol(p + klen + 1, &end, 10);
            if (end != p + klen + 1) { *out = v; return 0; }
            return -1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return -1;
}

// 从 hm->query（mongoose 已剥离的 query string，如 "id=2"）取数字参数，
// 成功返回分配的 JSON 字符串（调用者 free）；无有效参数返回 NULL
static char *query_to_json(struct mg_http_message *hm) {
    if (hm->query.len == 0) return NULL;

    char query[256];
    size_t qlen = hm->query.len < sizeof(query) - 1 ? hm->query.len : sizeof(query) - 1;
    memcpy(query, hm->query.buf, qlen);
    query[qlen] = '\0';

    long id = 0;
    if (query_get_long(query, "id", &id) == 0) {
        char *buf = malloc(32);
        snprintf(buf, 32, "{\"id\":%ld}", id);
        return buf;
    }
    // 其他 query 参数：取第一个值原样传递
    const char *v = strchr(query, '=');
    if (v && *(v + 1)) {
        char *buf = malloc(strlen(v + 1) + 8);
        snprintf(buf, strlen(v + 1) + 8, "{\"q\":\"%s\"}", v + 1);
        return buf;
    }
    return NULL;
}

// 组装传给插件的 JSON：优先 query 参数，其次请求体，都没有则 NULL
static char *build_json_in(struct mg_http_message *hm) {
    char *q = query_to_json(hm);
    if (q) return q;

    if (hm->body.len == 0) return NULL;
    char *body = malloc(hm->body.len + 1);
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';
    return body;
}

void ev_handler_libary(struct mg_connection *c, int ev, void *ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    char uri[256], method[16], ffi_path[512];
    int mlen = hm->method.len < (int)sizeof(method) - 1
             ? (int)hm->method.len
             : (int)sizeof(method) - 1;
    memcpy(method, hm->method.buf, mlen);
    method[mlen] = '\0';
    for (int i = 0; method[i]; i++) {
        method[i] = (char)tolower((unsigned char)method[i]);
    }

    int ulen = hm->uri.len < (int)sizeof(uri) - 1
             ? (int)hm->uri.len
             : (int)sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, ulen);
    uri[ulen] = '\0';

    if (uri_has_traversal(uri)) {
        mg_http_reply(c, 400,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":400,\"message\":\"Bad Request\"}");
        return;
    }

    // URI → 插件名：/libary_demo → ./libary/libary_demo.so
    // （query 参数由 hm->query 提供，不在 uri 里）
    const char *clean_uri = uri[0] == '/' ? uri + 1 : uri;
    if (!*clean_uri) {
        mg_http_reply(c, 404,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":404,\"message\":\"Not Found\"}");
        return;
    }
    snprintf(ffi_path, sizeof(ffi_path), "%s%s%s%s", LIBARY_DIR, PATH_SEP, clean_uri, FFI_EXT);

    char *json_in = build_json_in(hm);

    // FFI-Symbol 头覆盖默认符号；默认 <method>_handle
    const struct mg_str *ffi_symbol_str = mg_http_get_header(hm, "FFI-Symbol");
    char ffi_symbol[128] = "handle";
    if (ffi_symbol_str && ffi_symbol_str->len > 0) {
        size_t len = ffi_symbol_str->len < sizeof(ffi_symbol) - 1
               ? ffi_symbol_str->len
               : sizeof(ffi_symbol) - 1;
        memcpy(ffi_symbol, ffi_symbol_str->buf, len);
        ffi_symbol[len] = '\0';
    }
    char final_symbol[256];
    snprintf(final_symbol, sizeof(final_symbol), "%s_%s", method, ffi_symbol);

    char *json_out = NULL;
    int rc = call_local_dyn_libffi(ffi_path, final_symbol, json_in, &json_out);

    free(json_in);
    if (rc != 0 || !json_out) {
        mg_http_reply(c, 404,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":404,\"message\":\"Plugin Not Found\"}");
        return;
    }

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%s", json_out);

    free(json_out);
}
