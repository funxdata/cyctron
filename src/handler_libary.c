#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "calldyn.h"
#include "global.h"

void ev_handler_libary(struct mg_connection *c, int ev, void *ev_data) {
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
    const char *clean_uri = uri[0] == '/' ? uri + 1 : uri;
    snprintf(ffi_path, sizeof(ffi_path),
             "%s%s%s%s", LIBARY_DIR,PATH_SEP, clean_uri, FFI_EXT);

    char *json_in = NULL;
    if (hm->body.len > 0) {
        json_in = malloc(hm->body.len + 1);
        memcpy(json_in, hm->body.buf, hm->body.len);
        json_in[hm->body.len] = '\0';
    }
    
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
    snprintf(final_symbol, sizeof(final_symbol),"%s_%s",method,ffi_symbol);
    char *json_out = NULL;
    int rc = call_local_dyn_libffi(ffi_path, final_symbol, json_in, &json_out);

    free(json_in);
    if (rc != 0 || !json_out) {
        mg_http_reply(c, 404,
            "Content-Type: application/json\r\n",
            "{\"error\":404,\"message\":\"Plugin Not Found\"}");
        return;
    }

    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n",
        "%s", json_out);

    free(json_out);
}
