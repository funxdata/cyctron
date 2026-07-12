// include/calldyn.h
#ifndef CALLDYN_H
#define CALLDYN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 使用 libffi 调用动态库函数
 * @param ffi_path   动态库路径
 * @param func_name  函数名
 * @param json_in    输入 JSON
 * @param json_out   输出 JSON（需要调用者 free）
 * @return 0 成功，负数失败
 */
int call_local_dyn_libffi(const char *ffi_path, const char *func_name, 
                          const char *json_in, char **json_out);

/**
 * 调用 NNG 动态库的 process_init 函数
 * 动态库从 ./core/ 目录加载
 * @param lib_name   库名（如 "libsoket_demo" 或 "libsoket_demo.so"）
 * @param json_in    输入 JSON
 * @param json_out   输出 JSON（需要调用者 free）
 * @return 0 成功，负数失败
 */
int call_local_dyn_socklibffi(const char *lib_name, const char *json_in, char **json_out);

#ifdef __cplusplus
}
#endif

#endif // CALLDYN_H
      
