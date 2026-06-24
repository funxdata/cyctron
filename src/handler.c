#include <stdio.h>
#include <string.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"

void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_HTTP_MSG: {
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            char method[16];
            int len = hm->method.len < sizeof(method) - 1 ? (int)hm->method.len : sizeof(method) - 1;
            memcpy(method, hm->method.buf, len);
            method[len] = '\0';
            
            // ====== OPTIONS 预检请求 ======
            if (strcmp(method, "OPTIONS") == 0) {
                mg_http_reply(c, 204, 
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, FFI-Symbol, ffi-symbol\r\n"
                    "Access-Control-Max-Age: 86400\r\n",
                    "");
                return;
            }
            
            // ====== WebSocket 升级 ======
            if (hm->uri.len == 6 && strncmp(hm->uri.buf, "/soket", 6) == 0) {
                mg_ws_upgrade(c, hm, NULL);
                printf("Upgrading connection to WebSocket: %p\n", c);
                return;
            } 
            
            // ====== 处理静态文件 ======
            if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
                ev_handler_static(c, ev, ev_data);
                return;
            }
            
            // ====== 处理 /os 路径 ======
            if (hm->uri.len >= 3 && strncmp(hm->uri.buf, "/os", 3) == 0) {
                ev_handler_os(c, ev, ev_data);
                return;
            }
            
            // ====== 根据 Content-Type 路由 ======
            char content_type[128] = "";
            const struct mg_str *ctype_hdr = mg_http_get_header(hm, "Content-Type");
            if (ctype_hdr) {
                size_t n = ctype_hdr->len < sizeof(content_type)-1 ? ctype_hdr->len : sizeof(content_type)-1;
                memcpy(content_type, ctype_hdr->buf, n);
                content_type[n] = '\0';
            }
            
            ct_content_type ctype = parse_content_type(content_type);
            
            switch (ctype) {
                case CT_DATABASE: 
                    ev_handler_database(c, ev, ev_data);
                    return;
                    
                case CT_OS:       
                    ev_handler_os(c, ev, ev_data); 
                    return;
                    
                case CT_LIBARY:   
                    ev_handler_libary(c, ev, ev_data); 
                    return;
                    
                case CT_CHAT:     
                    ev_handler_chat(c, ev, ev_data); 
                    return;
                    
                default:
                    mg_http_reply(c, 404, 
                        "Access-Control-Allow-Origin: *\r\n"
                        "Content-Type: application/json\r\n",
                        "{\"error\":404,\"message\":\"Not Found\"}");
                    return;
            }
            break;
        }

        // WebSocket 事件
        case MG_EV_WS_OPEN: {
            printf("WS Opened: %p\n", c);
            break;
        }

        case MG_EV_WS_MSG: {
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            printf("WS Msg received: len=%d\n", (int)wm->data.len);
            mg_ws_send(c, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
            break;
        }

        case MG_EV_CLOSE: {
            if (c->is_websocket) {
                printf("WS Closed: %p\n", c);
            }
            break;
        }

        default:
            break;
    }
}
