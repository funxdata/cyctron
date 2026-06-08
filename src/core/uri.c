#include <string.h>
#include "uri.h"

// uri是否包含文件扩展名（如 .html .png）
int has_file_extension(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *dot   = strrchr(path, '.');

    if (!dot) return 0;                 // 没有 .
    if (slash && dot < slash) return 0; // . 在 / 前面，不算扩展名
    if (*(dot + 1) == '\0') return 0;   // 以 . 结尾

    return 1;
}
