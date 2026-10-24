// test_ws_call.c - unit tests for ws_call_parse / free
#include "../include/ws_call.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static int parse(const char *json, ws_call_req_t *req, char *err, size_t err_size) {
    return ws_call_parse(json, strlen(json), req, err, err_size);
}

static void test_parse_ok(void) {
    const char *json =
        "{\"id\":\"req-1\",\"cmd\":\"call\",\"lib\":\"chatllm\","
        "\"symbol\":\"post_chat\",\"args\":{\"prompt\":\"hi\"},"
        "\"timeout_ms\":120000}";
    ws_call_req_t req;
    char err[128] = {0};

    printf("== parse: valid ==\n");
    check(parse(json, &req, err, sizeof(err)) == 0, "returns 0");
    check(req.id && strcmp(req.id, "req-1") == 0, "id");
    check(req.lib && strcmp(req.lib, "chatllm") == 0, "lib");
    check(req.symbol && strcmp(req.symbol, "post_chat") == 0, "symbol");
    check(req.args && strstr(req.args, "\"prompt\"") != NULL, "args raw JSON");
    check(req.timeout_ms == 120000, "timeout_ms");
    ws_call_req_free(&req);
}

static void test_parse_defaults(void) {
    const char *json = "{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"s\"}";
    ws_call_req_t req;
    char err[128] = {0};

    printf("== parse: defaults ==\n");
    check(parse(json, &req, err, sizeof(err)) == 0, "returns 0");
    check(req.args && strcmp(req.args, "{}") == 0, "args defaults to {}");
    check(req.timeout_ms == 0, "timeout_ms defaults to 0");
    ws_call_req_free(&req);
}

static void test_parse_missing(void) {
    ws_call_req_t req;
    char err[128] = {0};

    printf("== parse: missing fields ==\n");
    check(parse("{}", &req, err, sizeof(err)) == WS_CALL_ERR_PARAM, "empty object -> -1");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\"}", &req, err, sizeof(err)) == WS_CALL_ERR_PARAM, "missing lib/symbol -> -1");
    ws_call_req_free(&req);
    check(parse("{\"lib\":\"l\",\"symbol\":\"s\"}", &req, err, sizeof(err)) == WS_CALL_ERR_PARAM, "missing id -> -1");
    ws_call_req_free(&req);
}

static void test_parse_keeps_id_on_error(void) {
    ws_call_req_t req;
    char err[128] = {0};
    int rc = parse("{\"id\":\"keep-me\",\"lib\":\"bad/name\",\"symbol\":\"s\"}",
                   &req, err, sizeof(err));

    printf("== parse: id kept for error response ==\n");
    check(rc == WS_CALL_ERR_DENIED, "bad lib -> -5");
    check(req.id && strcmp(req.id, "keep-me") == 0, "id preserved");
    ws_call_req_free(&req);
}

static void test_req_free_null(void) {
    printf("== free: null-safe ==\n");
    ws_call_req_free(NULL);
    ws_call_resp_free(NULL);
    check(1, "no crash");
}

int main(void) {
    printf("=== test_ws_call ===\n");
    test_parse_ok();
    test_parse_defaults();
    test_parse_missing();
    test_parse_keeps_id_on_error();
    test_req_free_null();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
