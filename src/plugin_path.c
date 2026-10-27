// src/plugin_path.c
// 插件动态库路径解析。
//
// 设计插件（design_plugins）安装到 ./plugins/<name>/，其中的动态库文件名
// 通常与包名一致；sys 能力库仍安装到 ./libary/。为了让两类插件共用同一套
// HTTP / SSE / WebSocket 调用链，这里统一解析出最终可加载的路径：
// 先找 ./plugins/<name>/，找不到再回退 ./libary/<name><ext>。

#include "plugin_path.h"
#include "global.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

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

// 在插件目录中查找动态库：先按包名约定，再扫描目录取第一个匹配文件。
static int find_library_in_dir(const char *dir, const char *name,
                               char *out, size_t out_size) {
    char candidate[PLUGIN_PATH_MAX];
    int n;

    n = snprintf(candidate, sizeof(candidate), "%s%s%s%s",
                 dir, PATH_SEP, name, FFI_EXT);
    if (n > 0 && (size_t) n < sizeof(candidate) && file_exists(candidate)) {
        snprintf(out, out_size, "%s", candidate);
        return 0;
    }

    n = snprintf(candidate, sizeof(candidate), "%s%slib%s%s",
                 dir, PATH_SEP, name, FFI_EXT);
    if (n > 0 && (size_t) n < sizeof(candidate) && file_exists(candidate)) {
        snprintf(out, out_size, "%s", candidate);
        return 0;
    }

#ifdef _WIN32
    char pattern[PLUGIN_PATH_MAX];
    n = snprintf(pattern, sizeof(pattern), "%s\\*%s", dir, FFI_EXT);
    if (n > 0 && (size_t) n < sizeof(pattern)) {
        struct _finddata_t fd;
        intptr_t h = _findfirst(pattern, &fd);
        if (h != -1) {
            do {
                if (fd.attrib & _A_SUBDIR) continue;
                snprintf(out, out_size, "%s%s%s", dir, PATH_SEP, fd.name);
                _findclose(h);
                return 0;
            } while (_findnext(h, &fd) == 0);
            _findclose(h);
        }
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *entry;
        size_t ext_len = strlen(FFI_EXT);
        while ((entry = readdir(d)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len > ext_len &&
                strcmp(entry->d_name + len - ext_len, FFI_EXT) == 0) {
                snprintf(out, out_size, "%s%s%s", dir, PATH_SEP, entry->d_name);
                closedir(d);
                return 0;
            }
        }
        closedir(d);
    }
#endif

    return -1;
}

int plugin_resolve_path(const char *name, char *out, size_t out_size) {
    if (!name || !*name || !out || out_size == 0) return -1;
    out[0] = '\0';

    if (plugin_name_is_safe(name)) {
        char dir[PLUGIN_PATH_MAX];
        int n = snprintf(dir, sizeof(dir), "%s%s%s", PLUGINS_DIR, PATH_SEP, name);
        if (n > 0 && (size_t) n < sizeof(dir) &&
            find_library_in_dir(dir, name, out, out_size) == 0) {
            return 0;
        }
    }

    snprintf(out, out_size, "%s%s%s%s", LIBARY_DIR, PATH_SEP, name, FFI_EXT);
    return file_exists(out) ? 0 : -1;
}
