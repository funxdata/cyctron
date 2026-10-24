// test_ws_call_errors.c - rc mapping and error response
#include "../include/ws_call.h"
#include "../crates/mongoose/mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static void test_map_rc(void) {
    printf("== map rc ==\n");
    check(ws_call_map_rc(0) == WS_CALL_OK, "0 -> 0");
    check(ws_call_map_rc(-100) == WS_CALL_ERR_PARAM, "-100 -> -1");
    check(ws_call_map_rc(-1) == WS_CALL_ERR_LIB, "-1 -> -2 (library)");
    check(ws_call_map_rc(-2) == WS_CALL_ERR_SYMBOL, "-2 -> -3 (symbol)");
    check(ws_call_map_rc(-3) == WS_CALL_ERR_INVOKE, "-3 -> -4 (malloc/invoke)");
    check(ws_call_map_rc(-999) == WS_CALL_ERR_INVOKE, "-999 -> -4 (ffi prep)");
    check(ws_call_map_rc(1) == WS_CALL_ERR_INVOKE, "plugin nonzero -> -4");
}

static void test_build_error(void) {
    ws_call_resp_t resp;
    struct mg_str json;
    char *id = NULL, *err = NULL;

    printf("== build error ==\n");
    check(ws_call_build_error("a\"b", WS_CALL_ERR_LIB, "line1\nline2", &resp) == 0,
          "build returns 0");
    check(resp.code == WS_CALL_ERR_LIB, "resp.code");
    check(strstr(resp.json, "\"type\":\"call\"") != NULL, "type");
    check(strstr(resp.json, "\"ok\":false") != NULL, "ok=false");
    check(strstr(resp.json, "\"code\":-2") != NULL, "code=-2");

    json = mg_str(resp.json);
    id = mg_json_get_str(json, "$.id");
    err = mg_json_get_str(json, "$.error");
    check(id && strcmp(id, "a\"b") == 0, "id escaped/roundtrip");
    check(err && strcmp(err, "line1\nline2") == 0, "error escaped/roundtrip");

    free(id);
    free(err);
    ws_call_resp_free(&resp);
}

static void test_build_error_null_id(void) {
    ws_call_resp_t resp;

    printf("== build error: null id ==\n");
    check(ws_call_build_error(NULL, WS_CALL_ERR_PARAM, "bad", &resp) == 0,
          "null id accepted");
    check(strstr(resp.json, "\"id\":\"\"") != NULL, "empty id");
    ws_call_resp_free(&resp);
}

int main(void) {
    printf("=== test_ws_call_errors ===\n");
    test_map_rc();
    test_build_error();
    test_build_error_null_id();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
