// tests/test_ws_message_limit.c - WebSocket message size limit
#include "../include/ws_message.h"
#include <stdio.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

int main(void) {
    const size_t limit = (size_t)WS_MAX_MESSAGE_BYTES;

    printf("=== test_ws_message_limit ===\n");
    printf("== limit ==\n");
    check(ws_message_limit() == WS_MAX_MESSAGE_BYTES, "limit is 1 MiB");
    check(limit == 1024u * 1024u, "macro is 1 MiB");

    printf("== boundaries ==\n");
    check(ws_message_check_size(0) == 0, "0 accepted");
    check(ws_message_check_size(1) == 0, "1 accepted");
    check(ws_message_check_size(limit - 1) == 0, "limit-1 accepted");
    check(ws_message_check_size(limit) == 0, "limit accepted");
    check(ws_message_check_size(limit + 1) == -1, "limit+1 rejected");
    check(ws_message_check_size(limit * 2) == -1, "2*limit rejected");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
