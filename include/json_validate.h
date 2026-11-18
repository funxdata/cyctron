#ifndef JSON_VALIDATE_H
#define JSON_VALIDATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 校验 s[0..len) 是否为单个完整 JSON 值（允许首尾空白）。
 * 不分配内存，递归深度上限 64。
 *
 * @return 1 合法；0 非法或参数错误
 */
int json_validate(const char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* JSON_VALIDATE_H */
