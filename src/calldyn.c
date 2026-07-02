#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef NO_LIBFFI
#include <calldyn.h>
#endif

#define PLUGIN_JSON_BUF_SIZE 65536

typedef int (*plugin_fn_t)(const char* in, char* out);

int call_local_dyn_libffi(const char *ffi_path, const char *func_name, const char *json_in, char **json_out)
{
    if (!ffi_path || !func_name || !json_out) return -100;
    *json_out = NULL;
    DL_HANDLE handle = dlopen(ffi_path, RTLD_NOW
    #ifndef _WIN32
     | RTLD_LOCAL
    #endif
    );
    if (!handle) {
        fprintf(stderr, "[ffi] load library failed: %s\n", dlerror());
        return -1;
    }
    
    void *fn_ptr = dlsym(handle, func_name);
    if (!fn_ptr) {
        fprintf(stderr, "[ffi] dlsym failed: %s\n", dlerror());
        dlclose(handle);
        return -2;
    }
    
    char *buf = (char*)malloc(PLUGIN_JSON_BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "[ffi] malloc failed\n");
        dlclose(handle);
        return -3;
    }
    memset(buf, 0, PLUGIN_JSON_BUF_SIZE);
#ifndef NO_LIBFFI
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
    dlclose(handle);
    return rc;

#else
    fprintf(stderr, "[ffi] libffi not available on Windows\n");
    free(buf);
    dlclose(handle);
    return -998;
#endif
}