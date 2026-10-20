// test_sse.c - SSE 格式化与 sse_demo 插件单元测试
//
// 覆盖：
//   - sse_format_event 正常输出/超长
//   - process_init / sse_next / sse_status / sse_reset
//   - after_id 续传
//   - 未知符号
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/calldyn.h"
#include "../include/handler_sse.h"

#ifdef _WIN32
#define SSE_PLUGIN "./libary/sse_demo.dll"
#else
#define SSE_PLUGIN "./libary/sse_demo.so"
#endif

static int passed = 0, failed = 0;

static void check(int cond, const char *name, const char *detail) {
    if (cond) {
        passed++;
        printf("  [PASS] %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s  (%s)\n", name, detail ? detail : "");
    }
}

static char *call(const char *symbol, const char *json_in, int *rc) {
    char *out = NULL;
    *rc = call_local_dyn_libffi(SSE_PLUGIN, symbol, json_in, &out);
    return out;
}

static int contains(const char *s, const char *needle) {
    return s && strstr(s, needle) != NULL;
}

static long extract_id(const char *json) {
    const char *p = json ? strstr(json, "\"id\":") : NULL;
    if (!p) return -1;
    return strtol(p + 5, NULL, 10);
}

static void test_format(void) {
    printf("== sse_format_event ==\n");

    char buf[256];
    int n = sse_format_event(buf, sizeof(buf), 7, "tick", "{\"seq\":7}");
    check(n > 0, "format returns length", "");
    check(contains(buf, "id: 7\n"), "contains id line", buf);
    check(contains(buf, "event: tick\n"), "contains event line", buf);
    check(contains(buf, "data: {\"seq\":7}\n\n"), "contains data + blank line", buf);

    char small[16];
    n = sse_format_event(small, sizeof(small), 123456789, "tick", "{}");
    check(n == -1, "truncation returns -1", "");
}

static void test_plugin(void) {
    printf("== sse_demo plugin ==\n");
    int rc;
    char *out;

    out = call("process_init", "{\"config\":{\"interval_ms\":123,\"event\":\"tick\"}}", &rc);
    check(rc == 0 && contains(out, "\"ok\":true") && contains(out, "\"interval_ms\":123"),
          "process_init returns config", out);
    free(out);

    out = call("sse_next", "{\"after_id\":0}", &rc);
    long id1 = extract_id(out);
    check(rc == 0 && id1 == 1 && contains(out, "\"event\":\"tick\"") &&
          contains(out, "\"data\""),
          "first sse_next id=1", out);
    free(out);

    out = call("sse_next", "{\"after_id\":0}", &rc);
    long id2 = extract_id(out);
    check(rc == 0 && id2 == id1 + 1, "second sse_next increments id", out);
    free(out);

    out = call("sse_next", "{\"after_id\":100}", &rc);
    long id3 = extract_id(out);
    check(rc == 0 && id3 > 100, "after_id is honored", out);
    free(out);

    out = call("sse_status", "{}", &rc);
    check(rc == 0 && contains(out, "\"ok\":true") && contains(out, "\"seq\""),
          "sse_status returns seq", out);
    free(out);

    out = call("sse_reset", "{}", &rc);
    check(rc == 0 && contains(out, "\"seq\":0"), "sse_reset resets seq", out);
    free(out);

    out = call("no_such_symbol", "{}", &rc);
    check(rc == -2 && out == NULL, "unknown symbol -> -2", "");
    free(out);
}

int main(void) {
    printf("========== test_sse ==========\n");
    test_format();
    test_plugin();
    printf("==============================\n");
    printf("Passed: %d  Failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
