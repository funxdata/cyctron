// test_uri_utils.c - 纯函数测试：URI 扩展名与路径遍历检查
#include "uri.h"
#include "server.h"

#include <stdio.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) {
        passed++;
        printf("  [PASS] %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s\n", name);
    }
}

int main(void) {
    printf("=== test_uri_utils ===\n");

    check(has_file_extension("/assets/app.js") == 1, "js extension");
    check(has_file_extension("/assets/app") == 0, "no extension");
    check(has_file_extension("/assets/.hidden") == 1, "dot file counts as extension");
    check(has_file_extension("/assets/file.") == 0, "trailing dot");
    check(has_file_extension("/a.b/c") == 0, "dot before slash");

    check(uri_has_traversal("../etc/passwd") == 1, "leading traversal");
    check(uri_has_traversal("/a/../b") == 1, "embedded traversal");
    check(uri_has_traversal("/a..b") == 1, "double dot in segment");
    check(uri_has_traversal("/a/b") == 0, "normal path");
    check(uri_has_traversal(NULL) == 0, "NULL safe");

    printf("Passed: %d  Failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
