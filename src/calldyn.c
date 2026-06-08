#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

typedef int (*plugin_fn_t)(const char* json_in, char* json_out);

#define PLUGIN_JSON_BUF_SIZE 65536

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out)
{
    if (!ffi_path || !func_name || !json_out) return -100;

    *json_out = NULL;

    void *handle = dlopen(ffi_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "[ffi] dlopen failed: %s\n", dlerror());
        return -1;
    }

    dlerror(); // 清空错误
    plugin_fn_t fn = (plugin_fn_t)dlsym(handle, func_name);
    const char *err = dlerror();
    if (err || !fn) {
        fprintf(stderr, "[ffi] dlsym(%s) failed: %s\n",
                func_name, err ? err : "unknown");
        dlclose(handle);
        return -2;
    }

    // 分配 buffer
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc buffer failed\n");
        dlclose(handle);
        return -3;
    }
    memset(buf, 0, PLUGIN_JSON_BUF_SIZE);

    // 调用插件
    int rc = fn(json_in ? json_in : "{}", buf);
    if (rc != 0) {
        fprintf(stderr, "[ffi] plugin returned error: %d\n", rc);
        free(buf);
        dlclose(handle);
        return rc;
    }

    *json_out = buf;
    dlclose(handle);
    return 0;
}