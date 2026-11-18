// include/plugin_path.h
// 插件动态库路径解析：只从 ./libary/ 加载能力库。
// 设计插件调用已迁移到 chatllm，cyctron 不再读取 ./plugins/。
#ifndef PLUGIN_PATH_H
#define PLUGIN_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 校验插件名是否安全（非空，仅允许 [A-Za-z0-9_-]）。
 * 用于阻止 URI 中的路径片段被拼进 libary 目录。
 */
int plugin_name_is_safe(const char *name);

/**
 * 解析插件动态库路径（仅 ./libary/）。
 *
 * 查找顺序：
 *   1. ./libary/<name><ext>
 *   2. ./libary/lib<name><ext>
 *
 * @param name      插件名，如 "soket_demo"
 * @param out       输出缓冲区；未找到时写入 ./libary/<name><ext> 候选路径
 * @param out_size  输出缓冲区大小
 * @return 0 找到可用文件；-1 参数错误、名字非法或未找到
 */
int plugin_resolve_path(const char *name, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_PATH_H
