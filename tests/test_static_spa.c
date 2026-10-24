// tests/test_static_spa.c - SPA fallback routing rules
#include "../include/server.h"
#include <stdio.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

int main(void) {
    printf("=== test_static_spa ===\n");

    printf("== page routes ==\n");
    check(static_should_fallback("/chat") == 1, "/chat");
    check(static_should_fallback("/chat/inner") == 1, "/chat/inner");
    check(static_should_fallback("/workspace") == 1, "/workspace");
    check(static_should_fallback("/setting/info") == 1, "/setting/info");

    printf("== excluded ==\n");
    check(static_should_fallback(NULL) == 0, "NULL");
    check(static_should_fallback("") == 0, "empty");
    check(static_should_fallback("/") == 0, "root");
    check(static_should_fallback("/index.html") == 0, "index.html has extension");
    check(static_should_fallback("/assets/app.js") == 0, "resource has extension");
    check(static_should_fallback("/api") == 0, "/api");
    check(static_should_fallback("/api/status") == 0, "/api/status");
    check(static_should_fallback("/soket") == 0, "/soket");
    check(static_should_fallback("/sse") == 0, "/sse");
    check(static_should_fallback("/sse?x=1") == 0, "/sse?x=1");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
