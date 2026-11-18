// test_json_validate.c - 纯函数测试：完整 JSON 结构校验
#include "json_validate.h"

#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(const char *s, int expected, const char *name) {
    int got = json_validate(s, strlen(s));
    if (got == expected) {
        passed++;
        printf("  [PASS] %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s (got %d, expected %d)\n", name, got, expected);
    }
}

int main(void) {
    printf("=== test_json_validate ===\n");

    check("{}", 1, "empty object");
    check("[]", 1, "empty array");
    check("null", 1, "null");
    check("true", 1, "true");
    check("false", 1, "false");
    check("123", 1, "integer");
    check("-1.25e+3", 1, "number with exponent");
    check("\"hello\\n\\u4e2d\"", 1, "escaped string");
    check("{\"a\":[1,2,{\"b\":null}]}", 1, "nested value");
    check("  { \"a\" : 1 }  ", 1, "surrounding whitespace");

    check("{1,2}", 0, "non-string key");
    check("{\"a\":1,}", 0, "trailing comma");
    check("{}garbage", 0, "trailing garbage");
    check("[1,2", 0, "unterminated array");
    check("\"unterminated", 0, "unterminated string");
    check("", 0, "empty input");

    printf("Passed: %d  Failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
