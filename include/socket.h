#ifndef WEBSOCKET_RT_H
#define WEBSOCKET_RT_H

#include "mongoose.h"
#include "realtime.h"

// WebSocket实时连接上下文
typedef struct {
    char socket_id[64];         // WebSocket连接ID
    rt_task_t *task;            // 关联的实时任务
    struct mg_connection *conn; // WebSocket连接
    int connected;              // 连接状态
    void *user_data;            // 用户自定义数据
} ws_rt_context_t;

// 初始化WebSocket实时系统
int ws_rt_init(ev_loop *loop);

// 处理WebSocket连接打开
void ws_rt_on_open(struct mg_connection *c);

// 处理WebSocket消息
void ws_rt_on_message(struct mg_connection *c, struct mg_ws_message *msg);

// 处理WebSocket连接关闭
void ws_rt_on_close(struct mg_connection *c);

// 发送消息到指定连接
int ws_rt_send(const char *socket_id, const char *data, size_t len);

// 广播消息到所有连接
void ws_rt_broadcast(const char *data, size_t len);

// 关闭指定连接
void ws_rt_close(const char *socket_id);

#endif // WEBSOCKET_RT_H