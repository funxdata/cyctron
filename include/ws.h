#pragma once

#define WEBSOCKET_OP_CONTINUE 0
#define WEBSOCKET_OP_TEXT 1
#define WEBSOCKET_OP_BINARY 2
#define WEBSOCKET_OP_CLOSE 8
#define WEBSOCKET_OP_PING 9
#define WEBSOCKET_OP_PONG 10

#include "http.h"

struct ct_ws_message {
  struct ct_str data;  // Websocket message data
  uint8_t flags;       // Websocket message flags
};

struct ct_connection *ct_ws_connect(struct ct_mgr *, const char *url,
                                    ct_event_handler_t fn, void *fn_data,
                                    const char *fmt, ...);
void ct_ws_upgrade(struct ct_connection *, struct ct_http_message *,
                   const char *fmt, ...);
size_t ct_ws_send(struct ct_connection *, const void *buf, size_t len, int op);
size_t ct_ws_wrap(struct ct_connection *, size_t len, int op);
size_t ct_ws_printf(struct ct_connection *c, int op, const char *fmt, ...);
size_t ct_ws_vprintf(struct ct_connection *c, int op, const char *fmt,
                     va_list *);