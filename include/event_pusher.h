#ifndef EVENT_PUSHER_H
#define EVENT_PUSHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化和清理
bool event_pusher_init(void);
void event_pusher_cleanup(void);

// 核心推送函数
void event_push(const char *data);
void event_push_with_type(const char *type, const char *data);

// 便捷推送函数
void event_push_message(const char *msg);
void event_push_json(const char *json);

// 状态查询
bool event_pusher_has_connection(void);
uint64_t event_pusher_get_push_count(void);

#ifdef __cplusplus
}
#endif

#endif // EVENT_PUSHER_H


