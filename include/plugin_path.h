// include/plugin_path.h
// 插件动态库路径解析：设计插件优先从 ./plugins/<name>/ 加载，找不到时回退 ./libary/。
#ifndef PLUGIN_PATH_H
#define PLUGIN_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 校验插件名是否安全（非空，仅允许 [A-Za-z0-9_-]）。
 * 只有安全名字才会去 ./plugins/<name>/ 下查找，避免 URI 中的路径片段
 * 被拼进设计插件安装目录。
 */
int plugin_name_is_safe(const char *name);

/**
 * 解析插件动态库路径。
 *
 * 查找顺序：
 *   1. ./plugins/<name>/<name><ext>
 *   2. ./plugins/<name>/lib<name><ext>
 *   3. ./plugins/<name>/ 下第一个扩展名匹配的动态库
 *   4. ./libary/<name><ext>（旧目录，兼容 sys 包与历史安装）
 *
 * @param name      插件名，如 "cdrimex"
 * @param out       输出缓冲区，未找到时仍写入 ./libary/<name><ext> 候选路径
 * @param out_size  输出缓冲区大小
 * @return 0 找到可用文件；-1 参数错误或未找到
 */
int plugin_resolve_path(const char *name, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_PATH_H
