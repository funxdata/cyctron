// tests/test_ws_parse_legacy.c - JSON parsing for bind/remove/status/msg
#include "../include/ws_command.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static int parse(const char *json, ws_command_t *req) {
    char err[128] = {0};
    return ws_command_parse(json, strlen(json), req, err, sizeof(err));
}

static void test_spaces(void) {
    ws_command_t req;
    const char *json = "{\"cmd\": \"bind\", \"uuid\": \"u1\", \"lib\": \"l1\"}";

    printf("== spaces ==\n");
    check(parse(json, &req) == 0, "returns 0");
    check(req.cmd && strcmp(req.cmd, "bind") == 0, "cmd");
    check(req.uuid && strcmp(req.uuid, "u1") == 0, "uuid");
    check(req.lib && strcmp(req.lib, "l1") == 0, "lib");
    check(req.data == NULL, "data is NULL");
    ws_command_free(&req);
}

static void test_escaped_data(void) {
    ws_command_t req;
    const char *json = "{\"cmd\":\"msg\",\"uuid\":\"u1\",\"data\":\"a\\\"b\\nc\\\\d\"}";

    printf("== escaped data ==\n");
    check(parse(json, &req) == 0, "returns 0");
    check(req.data && strcmp(req.data, "a\"b\nc\\d") == 0, "data unescaped");
    ws_command_free(&req);
}

static void test_missing_fields(void) {
    ws_command_t req;

    printf("== missing fields ==\n");
    check(parse("{\"uuid\":\"u1\"}", &req) == -1, "missing cmd -> -1");
    ws_command_free(&req);
    check(parse("{\"cmd\":123}", &req) == -1, "non-string cmd -> -1");
    ws_command_free(&req);
    check(parse("{\"cmd\":\"bind\"}", &req) == 0, "missing uuid -> parse ok");
    check(req.uuid == NULL, "uuid NULL");
    ws_command_free(&req);
}

static void test_invalid(void) {
    ws_command_t req;

    printf("== invalid ==\n");
    check(parse("{oops", &req) == -1, "bad json -> -1");
    ws_command_free(&req);
    check(parse("", &req) == -1, "empty -> -1");
    ws_command_free(&req);
    check(ws_command_parse("{}", 2 * 1024 * 1024, &req, NULL, 0) == -1, "oversized -> -1");
    ws_command_free(&req);
    ws_command_free(NULL);
    check(1, "free(NULL) safe");
}

int main(void) {
    printf("=== test_ws_parse_legacy ===\n");
    test_spaces();
    test_escaped_data();
    test_missing_fields();
    test_invalid();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
