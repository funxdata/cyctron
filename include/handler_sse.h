#pragma once

#include <stddef.h>

struct mg_connection;

// SSE 最大并发客户端数
#define SSE_MAX_CLIENTS 32

// SSE 路由处理（MG_EV_HTTP_MSG）
void ev_handler_sse(struct mg_connection *c, int ev, void *ev_data);

// 主循环钩子：由 ev_handler 在 MG_EV_POLL / MG_EV_CLOSE 转发
void sse_on_poll(struct mg_connection *c);
void sse_on_close(struct mg_connection *c);

// 格式化一条 SSE 事件（供单元测试复用）
// 返回写入字节数，失败返回 -1
int sse_format_event(char *out, size_t out_size, long id,
                     const char *event, const char *data);
