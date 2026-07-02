/**
 * @brief 调用本地动态库函数（JSON 输入输出）
 *
 * @param ffi_path 动态库路径，例如 "./libary/add.so"
 * @param func_name 库中函数名，例如 "add_json"
 * @param json_in 输入 JSON 字符串，例如 "{\"a\":10,\"b\":20}"
 * @param json_out 输出 JSON 字符串指针（需要 free 释放）
 * @return 0 调用成功, 负数表示错误码
 *
 * 说明：
 *   - 库函数必须遵循接口：
 *       int func(const char* input_json, char* output_json);
 *   - 输入输出都是 JSON 字符串
 *   - 输出缓冲区由 calldyn 内部动态分配，调用者需要 free
 */
#pragma once

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