// include/calldyn.h
#ifndef CALLDYN_H
#define CALLDYN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 插件 ABI v2 返回码与输出缓冲限制 */
#define CYCTRON_FFI_OK        0
#define CYCTRON_FFI_ENOSPC    (-1001) /* 输出不足，*out_len 为所需字节数 */
#define CYCTRON_FFI_INITIAL_OUTPUT (1024 * 1024)      /* 首次分配 1 MB */
#define CYCTRON_FFI_MAX_OUTPUT     (16 * 1024 * 1024) /* 扩容上限 16 MB */

/**
 * 使用 libffi 调用动态库函数（插件 ABI v2）。
 *
 * 插件导出：
 *   int fn(const char *json_in, size_t json_in_len,
 *          char *json_out, size_t json_out_cap, size_t *json_out_len);
 *
 * - 成功返回 0，*json_out_len 为写入的 JSON 字节数（不含结尾 NUL）；
 * - 输出不足返回 CYCTRON_FFI_ENOSPC，*json_out_len 为所需字节数，
 *   宿主会扩容后重试一次；
 * - 其它负值为插件错误。
 *
 * 宿主返回 0 时 *json_out 由调用者 free；失败时 *json_out 保持 NULL。
 *
 * @param ffi_path   动态库路径
 * @param func_name  导出函数名（通常为 <method>_<suffix>_v2）
 * @param json_in    输入 JSON，可为 NULL（此时 json_in_len 为 0）
 * @param json_out   输出 JSON
 * @param json_len   可选，返回实际输出长度
 * @return 0 成功，负数失败
 */
int call_local_dyn_libffi_v2(const char *ffi_path, const char *func_name,
                             const char *json_in, char **json_out,
                             size_t *json_len);

/**
 * 使用 libffi 调用动态库函数（旧 2 参数 ABI，保留给 soket/realtime 链路）
 * @param ffi_path   动态库路径
 * @param func_name  函数名
 * @param json_in    输入 JSON
 * @param json_out   输出 JSON；仅返回 0 时写入，调用者负责 free；失败时保持 NULL
 * @return 0 成功，负数失败
 */
int call_local_dyn_libffi(const char *ffi_path, const char *func_name, 
                          const char *json_in, char **json_out);

/**
 * 调用 NNG 动态库的 process_init 函数
 * 动态库从 ./core/ 目录加载
 * @param lib_name   库名（如 "soket_demo"、"libsoket_demo.so"）
 * @param json_in    输入 JSON
 * @param json_out   输出 JSON（需要调用者 free）
 * @return 0 成功，负数失败
 */
int call_local_dyn_socklibffi(const char *lib_name, const char *json_in, char **json_out);

#ifdef __cplusplus
}
#endif

#endif // CALLDYN_H
      
