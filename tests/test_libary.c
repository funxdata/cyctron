// test_libary.c - libary_demo 动态库单元测试
// 覆盖：GET 无参/带参/不存在、POST 空/正常、PATCH、UPDATE、PUT、DELETE、
//       边界（NULL 入参、非法 JSON、未知符号、空输出缓冲）
#include "../include/calldyn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN "./libary/libary_demo.so"

static int passed = 0, failed = 0;

static void check(int cond, const char *name, const char *detail) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s  (%s)\n", name, detail ? detail : ""); }
}

// 调用并返回 json_out（调用者 free），rc 通过参数带回
static char *call(const char *symbol, const char *json_in, int *rc) {
    char *out = NULL;
    *rc = call_local_dyn_libffi(PLUGIN, symbol, json_in, &out);
    return out;
}

static int out_contains(const char *out, const char *needle) {
    return out && strstr(out, needle);
}

static void test_get(void) {
    printf("== GET ==\n");
    int rc;
    char *out;

    out = call("get_handle", NULL, &rc);
    check(rc == 0 && out_contains(out, "\"count\":3") && out_contains(out, "alpha"),
          "GET no params -> 3 items", out);
    free(out);

    out = call("get_handle", "{\"id\":1}", &rc);
    check(rc == 0 && out_contains(out, "\"id\":1") && out_contains(out, "alpha"),
          "GET id=1 -> alpha", out);
    free(out);

    out = call("get_handle", "{\"id\":2}", &rc);
    check(rc == 0 && out_contains(out, "beta"), "GET id=2 -> beta", out);
    free(out);

    out = call("get_handle", "{\"id\":99}", &rc);
    check(rc == 0 && out_contains(out, "not_found"), "GET id=99 -> not_found", out);
    free(out);

    out = call("get_handle", "{\"foo\":\"bar\"}", &rc);
    check(rc == 0 && out_contains(out, "\"foo\":\"bar\""), "GET extra params echoed", out);
    free(out);
}

static void test_post(void) {
    printf("== POST ==\n");
    int rc;
    char *out;

    out = call("post_handle", NULL, &rc);
    check(rc == 0 && out_contains(out, "empty_body"), "POST null body -> empty_body", out);
    free(out);

    out = call("post_handle", "{}", &rc);
    check(rc == 0 && out_contains(out, "empty_body"), "POST {} -> empty_body", out);
    free(out);

    out = call("post_handle", "{\"name\":\"delta\",\"type\":\"js\"}", &rc);
    check(rc == 0 && out_contains(out, "\"ok\":true") && out_contains(out, "\"id\":4")
          && out_contains(out, "delta"),
          "POST create -> id assigned + echoed", out);
    free(out);

    out = call("post_handle", "{\"name\":\"eps\"}", &rc);
    check(rc == 0 && out_contains(out, "\"id\":5"), "POST second create -> next id", out);
    free(out);
}

static void test_patch(void) {
    printf("== PATCH ==\n");
    int rc;
    char *out;

    out = call("patch_handle", NULL, &rc);
    check(rc == 0 && out_contains(out, "missing_id"), "PATCH null -> missing_id", out);
    free(out);

    out = call("patch_handle", "{\"id\":1}", &rc);
    check(rc == 0 && out_contains(out, "empty_patch"), "PATCH no patch -> empty_patch", out);
    free(out);

    out = call("patch_handle", "{\"id\":1,\"patch\":{\"name\":\"renamed\"}}", &rc);
    check(rc == 0 && out_contains(out, "\"ok\":true") && out_contains(out, "renamed")
          && out_contains(out, "\"type\":\"img\""),
          "PATCH merges fields, keeps others", out);
    free(out);

    out = call("patch_handle", "{\"id\":99,\"patch\":{\"x\":1}}", &rc);
    check(rc == 0 && out_contains(out, "not_found"), "PATCH missing id -> not_found", out);
    free(out);
}

static void test_update_put(void) {
    printf("== UPDATE / PUT ==\n");
    int rc;
    char *out;

    out = call("update_handle", NULL, &rc);
    check(rc == 0 && out_contains(out, "empty_body"), "UPDATE null -> empty_body", out);
    free(out);

    out = call("update_handle", "{\"id\":2,\"name\":\"replaced\"}", &rc);
    check(rc == 0 && out_contains(out, "UPDATE") && out_contains(out, "replaced"),
          "UPDATE replaces item", out);
    free(out);

    out = call("put_handle", "{\"id\":3,\"name\":\"putted\"}", &rc);
    check(rc == 0 && out_contains(out, "PUT") && out_contains(out, "putted"),
          "PUT same semantics", out);
    free(out);

    out = call("update_handle", "{\"id\":99,\"name\":\"x\"}", &rc);
    check(rc == 0 && out_contains(out, "not_found"), "UPDATE missing -> not_found", out);
    free(out);
}

static void test_delete(void) {
    printf("== DELETE ==\n");
    int rc;
    char *out;

    out = call("delete_handle", NULL, &rc);
    check(rc == 0 && out_contains(out, "missing_id"), "DELETE null -> missing_id", out);
    free(out);

    out = call("delete_handle", "{\"id\":1}", &rc);
    check(rc == 0 && out_contains(out, "\"deleted\":{\"id\":1}"), "DELETE id=1 -> ok", out);
    free(out);

    out = call("delete_handle", "{\"id\":1}", &rc);
    check(rc == 0 && out_contains(out, "not_found"), "DELETE again -> not_found", out);
    free(out);
}

static void test_edges(void) {
    printf("== 边界 ==\n");
    int rc;
    char *out;

    out = call("get_handle", "not a json", &rc);
    check(rc == 0 && out_contains(out, "\"method\":\"GET\"") && out_contains(out, "\"params\":null"),
          "invalid json treated as no params", out);
    free(out);

    out = call("no_such_symbol", "{}", &rc);
    check(rc == -2 && out == NULL, "unknown symbol -> -2", "");
    free(out);

    rc = call_local_dyn_libffi("./libary/nope.so", "get_handle", "{}", &out);
    check(rc == -1, "missing plugin -> -1", "");
    free(out);
}

int main(void) {
    printf("========== test_libary ==========\n");
    test_get();
    test_post();
    test_patch();
    test_update_put();
    test_delete();
    test_edges();
    printf("=================================\n");
    printf("Passed: %d  Failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
