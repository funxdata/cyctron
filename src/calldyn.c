// src/calldyn.c
#include "calldyn.h"
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

#define PLUGIN_JSON_BUF_SIZE 65536

typedef int (*plugin_fn_t)(const char* in, char* out);

// dlopen 句柄缓存：动态库常驻，保证库内 static 状态（自增 id 等）
// 在多次调用间保持；也避免每次请求重复 dlopen 的开销。
// 快路径无锁读（表只增不改，写入端有锁 + 全量写屏障保证可见性）
#define DL_CACHE_MAX 32

typedef struct {
    char path[256];
    DL_HANDLE handle;
} dl_cache_entry_t;

static dl_cache_entry_t g_dl_cache[DL_CACHE_MAX];
static volatile int g_dl_cache_count = 0;
#ifndef _WIN32
static pthread_mutex_t g_dl_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static DL_HANDLE dl_get(const char *path) {
    // 快路径：无锁查表（条目一旦写入不再修改，count 只增）
    int n = g_dl_cache_count;
    for (int i = 0; i < n; i++) {
        if (strcmp(g_dl_cache[i].path, path) == 0) {
            return g_dl_cache[i].handle;
        }
    }

    // 慢路径：加锁加载并插入
#ifndef _WIN32
    pthread_mutex_lock(&g_dl_mutex);
#endif
    DL_HANDLE handle = NULL;
    // 双重检查：等锁期间可能已被其他线程插入
    for (int i = 0; i < g_dl_cache_count; i++) {
        if (strcmp(g_dl_cache[i].path, path) == 0) {
            handle = g_dl_cache[i].handle;
            break;
        }
    }
    if (!handle && g_dl_cache_count < DL_CACHE_MAX) {
        handle = dlopen(path, RTLD_NOW
        #ifndef _WIN32
         | RTLD_LOCAL
        #endif
        );
        if (handle) {
            snprintf(g_dl_cache[g_dl_cache_count].path,
                     sizeof(g_dl_cache[0].path), "%s", path);
            g_dl_cache[g_dl_cache_count].handle = handle;
            __sync_synchronize();
            g_dl_cache_count++;
        }
    }
#ifndef _WIN32
    pthread_mutex_unlock(&g_dl_mutex);
#endif
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
        fprintf(stderr, "[ffi] dlsym failed: %s\n", dlerror());
        return -2;
    }
    
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc failed\n");
        return -3;
    }
    memset(buf, 0, PLUGIN_JSON_BUF_SIZE);

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
    
    *json_out = buf;

    return rc;
}

/**
 * 调用 NNG 动态库的 process_init 函数
 * @param lib_name  库名（如 "libsoket_demo" 或 "libsoket_demo.so"）
 * @param json_in   输入 JSON
 * @param json_out  输出 JSON（需要调用者 free）
 * @return 0 成功，负数失败
 */
int call_local_dyn_socklibffi(const char *lib_name, const char *json_in, char **json_out)
{
    if (!lib_name || !json_in || !json_out) return -100;
    *json_out = NULL;
    
    // 构建动态库路径（从 ./core/ 目录加载）
    char ffi_path[256];
    if (strstr(lib_name, ".so") || strstr(lib_name, ".dylib") || strstr(lib_name, ".dll")) {
        snprintf(ffi_path, sizeof(ffi_path), "./core/%s", lib_name);
    } else {
        snprintf(ffi_path, sizeof(ffi_path), "./core/lib%s.so", lib_name);
    }
    
    log_info("[CALDYN] Loading NNG library: %s", ffi_path);
    log_info("[CALDYN] json_in: %s", json_in);
    
    // 调用底层 libffi
    return call_local_dyn_libffi(ffi_path, "process_init", json_in, json_out);
}

