#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>

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

// 统一插件函数签名
typedef int (*plugin_fn_t)(const char* in, char* out);

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out)
{
    if (!ffi_path || !func_name || !json_out) return -100;

    *json_out = NULL;

    // =========================
    // 1️ 加载动态库
    // =========================
    DL_HANDLE handle = dlopen(ffi_path, RTLD_NOW
#ifndef _WIN32
                              | RTLD_LOCAL
#endif
    );
    if (!handle) {
        fprintf(stderr, "[ffi] load library failed: %s\n", dlerror());
        return -1;
    }

    // =========================
    // 2️ 获取函数指针
    // =========================
    void *fn_ptr = dlsym(handle, func_name);
    if (!fn_ptr) {
        fprintf(stderr, "[ffi] dlsym failed: %s\n", dlerror());
        dlclose(handle);
        return -2;
    }

    // =========================
    // 3️ 准备输出 buffer
    // =========================
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc failed\n");
        dlclose(handle);
        return -3;
    }
    memset(buf, 0, PLUGIN_JSON_BUF_SIZE);

    // =========================
    // 4 libffi 调用
    // =========================
    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    int rc = 0;

    args[0] = &ffi_type_pointer;   // const char*
    args[1] = &ffi_type_pointer;   // char* (输出)

    values[0] = (void*)&json_in;
    values[1] = &buf;

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args) != FFI_OK) {
        fprintf(stderr, "[ffi] cif prep failed\n");
        free(buf);
        dlclose(handle);
        return -4;
    }

    ffi_call(&cif, fn_ptr, &rc, values);

    *json_out = buf;

    // =========================
    // 5️⃣ 卸载动态库
    // =========================
    dlclose(handle);

    return rc;
}