// test_plugin_path.c - libary 插件路径解析测试
#include "plugin_path.h"
#include "global.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0755)
#define RMDIR(p) rmdir(p)
#endif

static int g_failed = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failed++;
    }
}

static int write_file(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fputs("x", fp);
    fclose(fp);
    return 0;
}

static void make_path(char *out, size_t size, const char *a,
                      const char *b, const char *c) {
    snprintf(out, size, "%s%s%s%s%s", a, PATH_SEP, b, c ? PATH_SEP : "", c ? c : "");
}

int main(void) {
    char found[512];
    char path[512];

    MKDIR("libary");

    /* 1. ./libary/<name><ext> */
    make_path(path, sizeof(path), LIBARY_DIR, "demo" FFI_EXT, NULL);
    check(write_file(path) == 0, "create libary/demo");
    check(plugin_resolve_path("demo", found, sizeof(found)) == 0,
          "resolve demo");
    check(strcmp(found, path) == 0, "demo resolves to libary dir");

    /* 2. ./libary/lib<name><ext> 回退 */
    make_path(path, sizeof(path), LIBARY_DIR, "liblib_demo" FFI_EXT, NULL);
    check(write_file(path) == 0, "create libary/liblib_demo");
    check(plugin_resolve_path("lib_demo", found, sizeof(found)) == 0,
          "resolve lib_demo");
    check(strstr(found, "liblib_demo" FFI_EXT) != NULL,
          "lib_demo resolves to lib prefixed library");

    /* 3. 未安装：返回 -1，并给出 libary 候选路径 */
    check(plugin_resolve_path("missing", found, sizeof(found)) == -1,
          "missing plugin returns -1");
    make_path(path, sizeof(path), LIBARY_DIR, "missing" FFI_EXT, NULL);
    check(strcmp(found, path) == 0, "missing keeps libary candidate path");

    /* 4. 非安全名不参与路径拼接 */
    check(plugin_name_is_safe("evil") == 1, "safe name accepted");
    check(plugin_name_is_safe("evil/x") == 0, "slash rejected");
    check(plugin_name_is_safe("evil\\x") == 0, "backslash rejected");
    check(plugin_name_is_safe("../evil") == 0, "traversal rejected");
    check(plugin_resolve_path("evil/x", found, sizeof(found)) == -1,
          "unsafe name returns -1");
    check(found[0] == '\0', "unsafe name leaves out empty");

    /* 清理 */
    make_path(path, sizeof(path), LIBARY_DIR, "demo" FFI_EXT, NULL);
    remove(path);
    make_path(path, sizeof(path), LIBARY_DIR, "liblib_demo" FFI_EXT, NULL);
    remove(path);
    RMDIR("libary");

    if (g_failed) {
        printf("plugin_path: %d failure(s)\n", g_failed);
        return 1;
    }
    printf("plugin_path: all checks passed\n");
    return 0;
}
