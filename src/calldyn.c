// src/calldyn.c
#include "calldyn.h"
#include "global.h"
#include "plugin_path.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#define DL_HANDLE HMODULE
#define dlopen(path, flags) LoadLibraryA(path)
#define dlsym(handle, name) GetProcAddress((HMODULE)handle, name)
#define dlclose(handle) FreeLibrary((HMODULE)handle)
#define dlerror() "windows error"
#else
#include <dlfcn.h>
#define DL_HANDLE void*
#endif

#ifndef PLUGIN_JSON_BUF_SIZE
// 插件输出缓冲区：1 MB。插件 C ABI 没有长度参数，缓冲区由宿主分配并保证
// 至少有 PLUGIN_JSON_BUF_SIZE 字节可写。
#define PLUGIN_JSON_BUF_SIZE (1024 * 1024)
#endif

typedef int (*plugin_fn_t)(const char* in, char* out);

// dlopen 句柄缓存：动态库常驻，保证库内 static 状态（自增 id 等）
// 在多次调用间保持；也避免每次请求重复 dlopen 的开销。
// 全平台使用同一把锁；路径动态分配，避免长路径被截断。
#define DL_CACHE_MAX 32

typedef struct {
    char *path;
    DL_HANDLE handle;
} dl_cache_entry_t;

static dl_cache_entry_t g_dl_cache[DL_CACHE_MAX];
static int g_dl_cache_count = 0;
static pthread_mutex_t g_dl_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *dup_path(const char *path) {
    size_t n = strlen(path) + 1;
    char *copy = (char *)malloc(n);
    if (copy) memcpy(copy, path, n);
    return copy;
}

static DL_HANDLE dl_open_library(const char *path) {
    return dlopen(path, RTLD_NOW
#ifndef _WIN32
     | RTLD_LOCAL
#endif
    );
}

static DL_HANDLE dl_get(const char *path) {
    DL_HANDLE handle = NULL;

    pthread_mutex_lock(&g_dl_mutex);

    for (int i = 0; i < g_dl_cache_count; i++) {
        if (strcmp(g_dl_cache[i].path, path) == 0) {
            handle = g_dl_cache[i].handle;
            pthread_mutex_unlock(&g_dl_mutex);
            return handle;
        }
    }

    if (g_dl_cache_count >= DL_CACHE_MAX) {
        log_warn("[ffi] dlopen cache full (%d), library not cached: %s",
                 DL_CACHE_MAX, path);
        handle = dl_open_library(path);
        pthread_mutex_unlock(&g_dl_mutex);
        return handle;
    }

    handle = dl_open_library(path);
    if (handle) {
        char *copy = dup_path(path);
        if (!copy) {
            dlclose(handle);
            handle = NULL;
        } else {
            g_dl_cache[g_dl_cache_count].path = copy;
            g_dl_cache[g_dl_cache_count].handle = handle;
            g_dl_cache_count++;
        }
    }

    pthread_mutex_unlock(&g_dl_mutex);
    return handle;
}

int call_local_dyn_libffi(const char *ffi_path, const char *func_name, const char *json_in, char **json_out)
{
    if (!ffi_path || !func_name || !json_out) return -100;
    *json_out = NULL;

    DL_HANDLE handle = dl_get(ffi_path);
    if (!handle) {
        fprintf(stderr, "[ffi] load library failed: %s\n", dlerror());
        return -1;
    }
    void *fn_ptr = dlsym(handle, func_name);
    if (!fn_ptr) {
        fprintf(stderr, "[ffi] dlsym failed: %s (symbol=%s)\n", dlerror(), func_name);
        return -2;
    }
    
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc failed\n");
        return -3;
    }
    // 只初始化首字节：插件未写入时输出为空串，避免每次调用都写满 1 MB。
    // 插件必须写入以 '\0' 结尾的 JSON，并自行保证不超过 PLUGIN_JSON_BUF_SIZE。
    buf[0] = '\0';

    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    int rc = 0;

    const char *in = json_in;
    char *out = buf;

    args[0] = &ffi_type_pointer;
    args[1] = &ffi_type_pointer;

    values[0] = &in;
    values[1] = &out;

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args) == FFI_OK)
    {
        ffi_call(&cif, fn_ptr, &rc, values);
    }
    else
    {
        rc = -999;
    }

    // 失败时不把缓冲区交给调用方：统一在 calldyn 内释放，*json_out 保持 NULL。
    // 这样调用方只需在返回 0 时 free(*json_out)，避免失败路径泄漏。
    if (rc != 0) {
        free(buf);
        return rc;
    }

    *json_out = buf;
    return 0;
}

static ffi_type *ffi_size_type(void) {
    return sizeof(size_t) == 8 ? &ffi_type_uint64 : &ffi_type_uint32;
}

int call_local_dyn_libffi_v2(const char *ffi_path, const char *func_name,
                             const char *json_in, char **json_out,
                             size_t *json_len)
{
    if (!ffi_path || !func_name || !json_out) return -100;
    *json_out = NULL;
    if (json_len) *json_len = 0;

    DL_HANDLE handle = dl_get(ffi_path);
    if (!handle) {
        fprintf(stderr, "[ffi] load library failed: %s\n", dlerror());
        return -1;
    }
    void *fn_ptr = dlsym(handle, func_name);
    if (!fn_ptr) {
        fprintf(stderr, "[ffi] dlsym failed: %s (symbol=%s)\n", dlerror(), func_name);
        return -2;
    }

    size_t in_len = json_in ? strlen(json_in) : 0;
    size_t cap = CYCTRON_FFI_INITIAL_OUTPUT;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc failed\n");
        return -3;
    }
    buf[0] = '\0';

    /* ENOSPC 时按插件返回的所需长度扩容，最多重试一次。 */
    for (int attempt = 0; attempt < 2; attempt++) {
        size_t out_len = 0;
        size_t *out_len_ptr = &out_len;
        const char *in = json_in;
        char *out = buf;
        size_t out_cap = cap;

        ffi_type *args[5] = {
            &ffi_type_pointer,
            ffi_size_type(),
            &ffi_type_pointer,
            ffi_size_type(),
            &ffi_type_pointer,
        };
        void *values[5] = { &in, &in_len, &out, &out_cap, &out_len_ptr };

        ffi_cif cif;
        int rc = -999;
        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 5, &ffi_type_sint, args) != FFI_OK) {
            free(buf);
            return -999;
        }
        ffi_call(&cif, fn_ptr, &rc, values);

        if (getenv("CYCTRON_FFI_DEBUG")) {
            fprintf(stderr, "[ffi] %s rc=%d out_len=%zu cap=%zu\n",
                    func_name, rc, out_len, cap);
        }

        if (rc == CYCTRON_FFI_ENOSPC) {
            size_t need = out_len + 1;
            if (out_len == 0 || need > CYCTRON_FFI_MAX_OUTPUT || need <= cap) {
                free(buf);
                return -4;
            }
            char *grown = (char *)realloc(buf, need);
            if (!grown) {
                free(buf);
                return -3;
            }
            buf = grown;
            cap = need;
            continue;
        }
        if (rc != 0) {
            free(buf);
            return rc;
        }

        /* ABI 违规兜底：插件未设置长度或写满缓冲区时截断并补 NUL。 */
        if (out_len >= cap) out_len = cap - 1;
        buf[out_len] = '\0';
        *json_out = buf;
        if (json_len) *json_len = out_len;
        return 0;
    }

    free(buf);
    return -4;
}

/**
 * 调用 NNG 动态库的 process_init 函数
 * @param lib_name  库名，可带扩展名（如 "soket_demo"、"libsoket_demo.so"）
 * @param json_in   输入 JSON
 * @param json_out  输出 JSON（需要调用者 free）
 * @return 0 成功，负数失败
 */
int call_local_dyn_socklibffi(const char *lib_name, const char *json_in, char **json_out)
{
    if (!lib_name || !json_in || !json_out) return -100;
    *json_out = NULL;

    /* 去掉常见扩展名，得到平台无关的库名基名 */
    char base[256];
    size_t len = strlen(lib_name);
    static const char *exts[] = { ".so", ".dylib", ".dll" };
    size_t cut = len;
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        size_t n = strlen(exts[i]);
        if (len > n && strcmp(lib_name + len - n, exts[i]) == 0) {
            cut = len - n;
            break;
        }
    }
    size_t copy = cut < sizeof(base) - 1 ? cut : sizeof(base) - 1;
    memcpy(base, lib_name, copy);
    base[copy] = '\0';

    if (!base[0] || !plugin_name_is_safe(base)) {
        log_error("[CALDYN] invalid library name: %s", lib_name);
        return -100;
    }

    char ffi_path[1024];
    /* 统一按 libary 查找；历史 soket 插件仍可放在 ./core/ */
    if (plugin_resolve_path(base, ffi_path, sizeof(ffi_path)) != 0) {
        snprintf(ffi_path, sizeof(ffi_path), "./core/%s%s", base, FFI_EXT);
        FILE *fp = fopen(ffi_path, "rb");
        if (!fp) {
            snprintf(ffi_path, sizeof(ffi_path), "./core/lib%s%s", base, FFI_EXT);
        } else {
            fclose(fp);
        }
    }

    log_info("[CALDYN] Loading NNG library: %s", ffi_path);
    log_info("[CALDYN] json_in: %s", json_in);

    // 调用底层 libffi
    return call_local_dyn_libffi(ffi_path, "process_init", json_in, json_out);
}

