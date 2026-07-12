// include/handler_ws.h
#ifndef HANDLER_WS_H
#define HANDLER_WS_H

#include "mongoose.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// 全局WebSocket连接（供 event_pusher 使用）
extern struct mg_connection *g_ws_conn;
extern pthread_mutex_t g_ws_mutex;

// WebSocket 处理函数
void ws_accept_manual(struct mg_connection *c, struct mg_http_message *hm);
void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data);

#ifdef __cplusplus
}
#endif

#endif // HANDLER_WS_H
