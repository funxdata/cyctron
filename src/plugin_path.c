// src/plugin_path.c
// 插件动态库路径解析。
//
// cyctron 只从 ./libary/ 加载能力库；设计插件调用已迁移到 chatllm，
// 不再读取 ./plugins/。
//
// 查找顺序：
//   1. ./libary/<name><ext>
//   2. ./libary/lib<name><ext>（Linux 上的 sys 库约定）

#include "plugin_path.h"
#include "global.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define PLUGIN_PATH_MAX 512

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int plugin_name_is_safe(const char *name) {
    if (!name || !*name) return 0;
    for (const char *p = name; *p; p++) {
        unsigned char c = (unsigned char) *p;
        if (!(isalnum(c) || c == '_' || c == '-')) return 0;
    }
    return 1;
}

int plugin_resolve_path(const char *name, char *out, size_t out_size) {
    if (!name || !*name || !out || out_size == 0) return -1;
    out[0] = '\0';

    if (!plugin_name_is_safe(name)) return -1;

    int n = snprintf(out, out_size, "%s%s%s%s",
                     LIBARY_DIR, PATH_SEP, name, FFI_EXT);
    if (n <= 0 || (size_t) n >= out_size) {
        out[0] = '\0';
        return -1;
    }
    if (file_exists(out)) return 0;

    // libary 回退：Linux 上的 sys 库按约定安装为 lib<name>.so
    char lib_path[PLUGIN_PATH_MAX];
    n = snprintf(lib_path, sizeof(lib_path), "%s%slib%s%s",
                 LIBARY_DIR, PATH_SEP, name, FFI_EXT);
    if (n > 0 && (size_t) n < sizeof(lib_path) && file_exists(lib_path)) {
        snprintf(out, out_size, "%s", lib_path);
        return 0;
    }

    // 未找到时保留首个候选（<name><ext>），与 plugin_path.h 的约定一致
    return -1;
}
