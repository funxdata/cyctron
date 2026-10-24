// tests/test_event_push_dynamic.c - dynamic event formatting
#include "../include/event_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static void test_small(void) {
    char *out = NULL;

    printf("== with_type: small ==\n");
    check(event_format_with_type("log", "{\"a\":1}", &out) == 0, "returns 0");
    check(out && strstr(out, "\"type\":\"log\"") != NULL, "type");
    check(out && strstr(out, "\"data\":{\"a\":1}") != NULL, "data");
    check(out && strstr(out, "\"timestamp\":") != NULL, "timestamp");
    free(out);
}

static void test_large(size_t n, const char *label) {
    char *data = (char *)malloc(n + 1);
    char *out = NULL;

    if (!data) {
        check(0, label);
        return;
    }
    memset(data, 'x', n);
    data[n] = 0;

    check(event_format_with_type("log", data, &out) == 0, label);
    check(out && strlen(out) >= n, "not truncated");
    check(out && strstr(out, data) != NULL, "payload present");

    free(out);
    free(data);
}

static void test_escape(void) {
    char *out = NULL;

    printf("== escape ==\n");
    check(event_format_message("a\"b\nc\\d", &out) == 0, "message returns 0");
    check(out && strstr(out, "a\\\"b\\nc\\\\d") != NULL, "quotes/newline/backslash");
    free(out);
}

static void test_json_string(void) {
    char *out = NULL;

    printf("== json string ==\n");
    check(event_format_json_string("error", "x\"y", &out) == 0, "returns 0");
    check(out && strstr(out, "\"data\":\"x\\\"y\"") != NULL, "escaped data");
    free(out);
}

static void test_null(void) {
    char *out = NULL;

    printf("== null args ==\n");
    check(event_format_with_type(NULL, "{}", &out) == -1, "null type");
    check(event_format_with_type("t", NULL, &out) == -1, "null data");
    check(event_format_message(NULL, &out) == -1, "null message");
    check(event_format_json_string(NULL, "x", &out) == -1, "null json type");
    check(event_format_json_string("e", NULL, &out) == -1, "null json data");
    check(event_format_with_type("t", "{}", NULL) == -1, "null out");
}

int main(void) {
    printf("=== test_event_push_dynamic ===\n");
    test_small();
    test_large(2048, "2 KB");
    test_large(64 * 1024, "64 KB");
    test_large(1024 * 1024, "1 MB");
    test_escape();
    test_json_string();
    test_null();
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
