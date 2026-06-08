/**
 * @brief 调用本地动态库函数（JSON 输入输出）
 *
 * @param file_path 动态库路径，例如 "./libary/add.so"
 * @param func_name 库中函数名，例如 "add_json"
 * @param input_json 输入 JSON 字符串，例如 "{\"a\":10,\"b\":20}"
 * @param output_json 输出缓冲区，用于存储返回 JSON
 * @param out_size 输出缓冲区大小
 * @return 0 调用成功, -1 调用失败
 *
 * 说明：
 *   - 库函数必须遵循接口：
 *       void func(const char* input_json, char* output_json, int out_size);
 *   - 输入输出都是 JSON 字符串
 */
#pragma once

int call_local_dyn_libffi(const char *ffi_path,
                          const char *func_name,
                          const char *json_in,
                          char **json_out);