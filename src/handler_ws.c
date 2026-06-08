#include <stdio.h>
#include <string.h>
#include "mongoose.h"
#include "log.h" 
#include "server.h"

void ws_accept_manual(struct mg_connection *c, struct mg_http_message *hm) {
    // 标记为 WebSocket
    c->is_websocket = 1;
    c->fn = ws_ev_handler;

    // 发送 HTTP 101 Switching Protocols 响应
    mg_http_reply(c, 101,
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n\r\n",
        "");
}

// WebSocket 事件回调
void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_WS_OPEN:
            log_info("[WS] Connection opened");
            break;

        case MG_EV_WS_MSG: {
            struct mg_ws_message *msg = (struct mg_ws_message *) ev_data;

            char data[1024] = {0};
            size_t len = msg->data.len < sizeof(data)-1 ? msg->data.len : sizeof(data)-1;
            memcpy(data, msg->data.buf, len);

            log_info("[WS] message: %s", data);

            // 回传给客户端（可选）
            // ct_ws_send(c, msg->data.buf, msg->data.len, msg->flags);
            break;
        }

        case MG_EV_CLOSE:
            log_info("[WS] Connection closed");
            break;

        default:
            break;
    }
}
