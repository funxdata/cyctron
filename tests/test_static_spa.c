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

    printf("== page routes (browser navigation) ==\n");
    check(static_should_fallback("/chat", "text/html") == 1, "/chat");
    check(static_should_fallback("/chat/inner", "text/html") == 1, "/chat/inner");
    check(static_should_fallback("/workspace", "text/html") == 1, "/workspace");
    check(static_should_fallback("/setting/info", "text/html") == 1, "/setting/info");
    check(static_should_fallback("/chat", "text/html,application/xhtml+xml") == 1,
          "/chat with compound Accept");

    printf("== non-HTML requests ==\n");
    check(static_should_fallback("/chat", "*/*") == 0, "/chat with */*");
    check(static_should_fallback("/chat", "application/json") == 0,
          "/chat with json Accept");
    check(static_should_fallback("/no_such_lib", "*/*") == 0,
          "unknown plugin with */*");

    printf("== excluded ==\n");
    check(static_should_fallback(NULL, "text/html") == 0, "NULL");
    check(static_should_fallback("", "text/html") == 0, "empty");
    check(static_should_fallback("/", "text/html") == 0, "root");
    check(static_should_fallback("/index.html", "text/html") == 0, "index.html has extension");
    check(static_should_fallback("/assets/app.js", "text/html") == 0, "resource has extension");
    check(static_should_fallback("/assets/nothere/", "text/html") == 0, "assets dir never falls back");
    check(static_should_fallback("/soket", "text/html") == 0, "/soket");
    check(static_should_fallback("/sse", "text/html") == 0, "/sse");
    check(static_should_fallback("/sse?x=1", "text/html") == 0, "/sse?x=1");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
