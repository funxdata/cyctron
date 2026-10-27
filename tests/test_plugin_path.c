// test_plugin_path.c - plugins 目录优先 / libary 回退的路径解析测试
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

    /* 1. ./plugins/<name>/<name><ext> 优先 */
    MKDIR("plugins");
    MKDIR("plugins" PATH_SEP "demo");
    make_path(path, sizeof(path), PLUGINS_DIR, "demo", "demo" FFI_EXT);
    check(write_file(path) == 0, "create plugins/demo/demo");
    check(plugin_resolve_path("demo", found, sizeof(found)) == 0,
          "resolve demo");
    make_path(path, sizeof(path), PLUGINS_DIR, "demo", "demo" FFI_EXT);
    check(strcmp(found, path) == 0, "demo resolves to plugins dir");

    /* 2. 目录里 DLL 名和包名不一致时扫描目录 */
    MKDIR("plugins" PATH_SEP "renamed");
    make_path(path, sizeof(path), PLUGINS_DIR, "renamed", "actual" FFI_EXT);
    check(write_file(path) == 0, "create plugins/renamed/actual");
    check(plugin_resolve_path("renamed", found, sizeof(found)) == 0,
          "resolve renamed");
    check(strstr(found, "actual" FFI_EXT) != NULL,
          "renamed resolves to scanned library");

    /* 3. plugins 下没有时回退 ./libary/<name><ext> */
    MKDIR("libary");
    make_path(path, sizeof(path), LIBARY_DIR, "fallback" FFI_EXT, NULL);
    check(write_file(path) == 0, "create libary/fallback");
    check(plugin_resolve_path("fallback", found, sizeof(found)) == 0,
          "resolve fallback");
    check(strncmp(found, LIBARY_DIR PATH_SEP, strlen(LIBARY_DIR PATH_SEP)) == 0,
          "fallback resolves to libary dir");

    /* 4. 未安装：返回 -1，并给出 libary 候选路径 */
    check(plugin_resolve_path("missing", found, sizeof(found)) == -1,
          "missing plugin returns -1");
    make_path(path, sizeof(path), LIBARY_DIR, "missing" FFI_EXT, NULL);
    check(strcmp(found, path) == 0, "missing keeps libary candidate path");

    /* 5. 非安全名（含斜杠）不使用 plugins 目录 */
    MKDIR("plugins" PATH_SEP "evil");
    make_path(path, sizeof(path), PLUGINS_DIR, "evil", "evil" FFI_EXT);
    check(write_file(path) == 0, "create plugins/evil/evil");
    check(plugin_name_is_safe("evil") == 1, "safe name accepted");
    check(plugin_name_is_safe("evil/x") == 0, "slash rejected");
    check(plugin_name_is_safe("../evil") == 0, "traversal rejected");
    check(plugin_resolve_path("evil/x", found, sizeof(found)) == -1,
          "unsafe name does not resolve from plugins");

    /* 清理 */
    make_path(path, sizeof(path), PLUGINS_DIR, "demo", "demo" FFI_EXT);
    remove(path);
    RMDIR("plugins" PATH_SEP "demo");
    make_path(path, sizeof(path), PLUGINS_DIR, "renamed", "actual" FFI_EXT);
    remove(path);
    RMDIR("plugins" PATH_SEP "renamed");
    make_path(path, sizeof(path), PLUGINS_DIR, "evil", "evil" FFI_EXT);
    remove(path);
    RMDIR("plugins" PATH_SEP "evil");
    RMDIR("plugins");
    make_path(path, sizeof(path), LIBARY_DIR, "fallback" FFI_EXT, NULL);
    remove(path);
    RMDIR("libary");

    if (g_failed) {
        printf("plugin_path: %d failure(s)\n", g_failed);
        return 1;
    }
    printf("plugin_path: all checks passed\n");
    return 0;
}
