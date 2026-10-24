// test_ws_call_parse.c - validation matrix for ws_call_parse
#include "../include/ws_call.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static int parse(const char *json, ws_call_req_t *req) {
    char err[128] = {0};
    return ws_call_parse(json, strlen(json), req, err, sizeof(err));
}

static void test_names(void) {
    ws_call_req_t req;

    printf("== names ==\n");
    check(parse("{\"id\":\"r\",\"lib\":\"../escape\",\"symbol\":\"s\"}", &req) == WS_CALL_ERR_DENIED,
          "lib with slash -> -5");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"post chat\"}", &req) == WS_CALL_ERR_DENIED,
          "symbol with space -> -5");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\",\"lib\":\"\",\"symbol\":\"s\"}", &req) == WS_CALL_ERR_DENIED,
          "empty lib -> -5");
    ws_call_req_free(&req);
}

static void test_lengths(void) {
    char json[512];
    char id[300];
    ws_call_req_t req;

    memset(id, 'a', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"id\":\"%s\",\"lib\":\"l\",\"symbol\":\"s\"}", id);

    printf("== lengths ==\n");
    check(parse(json, &req) == WS_CALL_ERR_PARAM, "id > 128 -> -1");
    ws_call_req_free(&req);
}

static void test_args(void) {
    ws_call_req_t req;

    printf("== args ==\n");
    check(parse("{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"s\",\"args\":[]}", &req) == WS_CALL_ERR_PARAM,
          "array args -> -1");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"s\",\"args\":\"x\"}", &req) == WS_CALL_ERR_PARAM,
          "string args -> -1");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"s\",\"args\":null}", &req) == 0,
          "null args -> 0");
    ws_call_req_free(&req);
    check(parse("{\"id\":\"r\",\"lib\":\"l\",\"symbol\":\"s\",\"args\":{\"a\":1}}", &req) == 0,
          "object args -> 0");
    ws_call_req_free(&req);
}

static void test_payload(void) {
    ws_call_req_t req;
    char err[64] = {0};

    printf("== payload ==\n");
    check(ws_call_parse(NULL, 0, &req, err, sizeof(err)) == WS_CALL_ERR_PARAM,
          "null payload -> -1");
    ws_call_req_free(&req);
    check(ws_call_parse("{}", 0, &req, err, sizeof(err)) == WS_CALL_ERR_PARAM,
          "zero length -> -1");
    ws_call_req_free(&req);
    check(ws_call_parse("{}", WS_CALL_MAX_PAYLOAD + 1, &req, err, sizeof(err)) == WS_CALL_ERR_PARAM,
          "oversized length -> -1");
    ws_call_req_free(&req);
}

int main(void) {
    printf("=== test_ws_call_parse ===\n");
    test_names();
    test_lengths();
    test_args();
    test_payload();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
