#ifndef CALLDYN_LIBFFI_H
#define CALLDYN_LIBFFI_H

#ifdef __cplusplus
extern "C" {
#endif

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out);

#ifdef __cplusplus
}
#endif

#endif