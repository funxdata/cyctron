// test_ws_call_response.c - successful response JSON
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

static void test_build_ok(void) {
    ws_call_resp_t resp;
    struct mg_str json;
    char *id = NULL, *lib = NULL, *symbol = NULL;

    printf("== build ok ==\n");
    check(ws_call_build_ok("id-1", "chatllm", "post_chat", 12,
                           "{\"code\":200,\"data\":{}}", &resp) == 0,
          "build returns 0");
    check(resp.code == WS_CALL_OK, "resp.code=0");
    check(strstr(resp.json, "\"ok\":true") != NULL, "ok=true");
    check(strstr(resp.json, "\"code\":0") != NULL, "code=0");
    check(strstr(resp.json, "\"elapsed_ms\":12") != NULL, "elapsed_ms");
    check(strstr(resp.json, "\"result\":{\"code\":200") != NULL, "result raw");

    json = mg_str(resp.json);
    id = mg_json_get_str(json, "$.id");
    lib = mg_json_get_str(json, "$.lib");
    symbol = mg_json_get_str(json, "$.symbol");
    check(id && strcmp(id, "id-1") == 0, "id roundtrip");
    check(lib && strcmp(lib, "chatllm") == 0, "lib roundtrip");
    check(symbol && strcmp(symbol, "post_chat") == 0, "symbol roundtrip");

    free(id);
    free(lib);
    free(symbol);
    ws_call_resp_free(&resp);
}

static void test_result_null(void) {
    ws_call_resp_t resp;

    printf("== result null ==\n");
    check(ws_call_build_ok("id", "l", "s", 0, NULL, &resp) == 0, "NULL result");
    check(strstr(resp.json, "\"result\":null") != NULL, "encoded as null");
    ws_call_resp_free(&resp);

    check(ws_call_build_ok("id", "l", "s", 0, "", &resp) == 0, "empty result");
    check(strstr(resp.json, "\"result\":null") != NULL, "empty encoded as null");
    ws_call_resp_free(&resp);
}

static void test_escape(void) {
    ws_call_resp_t resp;
    struct mg_str json;
    char *id = NULL;

    printf("== escape ==\n");
    check(ws_call_build_ok("a\"b\c", "l", "s", 0, NULL, &resp) == 0,
          "quotes/backslash");
    json = mg_str(resp.json);
    id = mg_json_get_str(json, "$.id");
    check(id && strcmp(id, "a\"b\c") == 0, "id roundtrip");
    free(id);
    ws_call_resp_free(&resp);
}

int main(void) {
    printf("=== test_ws_call_response ===\n");
    test_build_ok();
    test_result_null();
    test_escape();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
