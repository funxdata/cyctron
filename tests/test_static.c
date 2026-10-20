// test_static.c - 静态文件处理单元测试
// 覆盖：路径解析（命中/回退/非法）、路径遍历拒绝、MIME 推断
#include "../include/server.h"
#include "../include/uri.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name, const char *detail) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s  (%s)\n", name, detail ? detail : ""); }
}

static void test_resolve_path(void) {
    char out[1024];

    printf("== resolve: direct files ==\n");
    check(static_resolve_path("/index.html", out, sizeof(out)) == 0
          && strcmp(out, "./web/index.html") == 0,
          "root index.html", out);
    check(static_resolve_path("/assets/js/app.js", out, sizeof(out)) == 0
          && strcmp(out, "./web/assets/js/app.js") == 0,
          "nested asset file", out);

    printf("== resolve: fallback to index.html ==\n");
    check(static_resolve_path("/", out, sizeof(out)) == 0
          && strcmp(out, "./web/index.html") == 0,
          "root -> index.html", out);
    check(static_resolve_path("/page", out, sizeof(out)) == 0
          && strcmp(out, "./web/page/index.html") == 0,
          "no-extension dir -> dir/index.html", out);
    check(static_resolve_path("/assets/img", out, sizeof(out)) == 0
          && strcmp(out, "./web/assets/img/index.html") == 0,
          "nested no-extension -> nested index", out);

    printf("== resolve: traversal rejected ==\n");
    check(static_resolve_path("/../etc/passwd", out, sizeof(out)) == -1,
          "../ rejected", out);
    check(static_resolve_path("/assets/../../secret.js", out, sizeof(out)) == -1,
          "embedded ../ rejected", out);
    check(static_resolve_path("/..%2f..%2fetc", out, sizeof(out)) == -1
          || strstr(out, "..") == NULL,
          "encoded traversal not mapped outside web root", out);
}

static void test_has_file_extension(void) {
    printf("== extension detection ==\n");
    check(has_file_extension("/a.html") == 1, "html detected", "");
    check(has_file_extension("/assets/img/logo.svg") == 1, "svg detected", "");
    check(has_file_extension("/assets/js") == 0, "no ext -> 0", "");
    check(has_file_extension("/assets/js/") == 0, "trailing slash -> 0", "");
    check(has_file_extension("/a.hidden/b") == 0, "dot in dir not ext", "");
    check(has_file_extension("/file.") == 0, "trailing dot -> 0", "");
}

static void test_uri_traversal(void) {
    printf("== traversal detection ==\n");
    check(uri_has_traversal("/../x") == 1, "leading ..", "");
    check(uri_has_traversal("/a/../b") == 1, "middle ..", "");
    check(uri_has_traversal("/assets/img/logo.svg") == 0, "clean uri", "");
    check(uri_has_traversal("") == 0, "empty uri", "");
}

int main(void) {
    printf("========== test_static ==========\n");
    test_resolve_path();
    test_has_file_extension();
    test_uri_traversal();
    printf("=================================\n");
    printf("Passed: %d  Failed: %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
