#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

// =========================
// Windows fallback 版本
// =========================

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out)
{
    (void)ffi_path;
    (void)func_name;
    (void)json_in;

    if (json_out) *json_out = NULL;

    fprintf(stderr,
        "[calldyn_libffi] Windows fallback: libffi not supported\n");

    return -1;
}

#else

// =========================
// Linux / macOS 真实 libffi 实现
// =========================

#include <ffi.h>
#include <dlfcn.h>

#define PLUGIN_JSON_BUF_SIZE 65536

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out)
{
    if (!ffi_path || !func_name || !json_out) return -100;

    *json_out = NULL;

    // 1. 加载动态库
    void *handle = dlopen(ffi_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "[ffi] dlopen failed: %s\n", dlerror());
        return -1;
    }

    // 2. 获取函数
    void *fn_ptr = dlsym(handle, func_name);
    if (!fn_ptr) {
        fprintf(stderr, "[ffi] dlsym failed: %s\n", dlerror());
        dlclose(handle);
        return -2;
    }

    // 3. 分配输出 buffer
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        dlclose(handle);
        return -3;
    }
    memset(buf, 0, PLUGIN_JSON_BUF_SIZE);

    // 4. libffi 调用准备
    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    int rc = 0;

    args[0] = &ffi_type_pointer;
    args[1] = &ffi_type_pointer;

    values[0] = (void*)&json_in;
    values[1] = &buf;

    if (ffi_prep_cif(&cif,
                     FFI_DEFAULT_ABI,
                     2,
                     &ffi_type_sint,
                     args) != FFI_OK)
    {
        fprintf(stderr, "[ffi] cif prep failed\n");
        free(buf);
        dlclose(handle);
        return -4;
    }

    // 5. 调用
    ffi_call(&cif, fn_ptr, &rc, values);

    *json_out = buf;

    // 6. 清理
    dlclose(handle);

    return rc;
}

#endif