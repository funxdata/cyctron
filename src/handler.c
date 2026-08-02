#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "event_pusher.h"
#include "handler_ws.h"

extern struct mg_connection *g_ws_conn;
extern pthread_mutex_t g_ws_mutex;

static void reply_json(struct mg_connection *c, int code, const char *json) {
    mg_http_reply(c, code,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%s", json);
}

void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_HTTP_MSG: {
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            char method[16];
            int len = hm->method.len < sizeof(method) - 1 ? (int)hm->method.len : sizeof(method) - 1;
            memcpy(method, hm->method.buf, len);
            method[len] = '\0';
            
            // ====== OPTIONS ======
            if (strcmp(method, "OPTIONS") == 0) {
                mg_http_reply(c, 204,
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, FFI-Symbol, ffi-symbol\r\n"
                    "Access-Control-Max-Age: 86400\r\n",
                    "");
                return;
            }
            
            // ====== WebSocket ======
            if (hm->uri.len == 6 && strncmp(hm->uri.buf, "/soket", 6) == 0) {
                mg_ws_upgrade(c, hm, NULL);
                c->fn = ws_ev_handler;
                printf("Upgrading connection to WebSocket: %p\n", c);
                
                pthread_mutex_lock(&g_ws_mutex);
                g_ws_conn = c;
                pthread_mutex_unlock(&g_ws_mutex);
                
                event_push_with_type("system", "{\"status\":\"connected\",\"message\":\"WebSocket client connected\"}");
                event_push_message("Welcome to Cyctron WebSocket Server");
                return;
            }
            
            // ====== HTTP API ======
            if (hm->uri.len >= 10 && strncmp(hm->uri.buf, "/api/push", 10) == 0) {
                if (strcmp(method, "POST") == 0) {
                    char message[1024] = "{\"message\":\"Hello from HTTP API\"}";
                    if (hm->body.len > 0) {
                        size_t n = hm->body.len < sizeof(message)-1 ? hm->body.len : sizeof(message)-1;
                        memcpy(message, hm->body.buf, n);
                        message[n] = '\0';
                    }
                    event_push_with_type("http_api", message);
                    log_info("[API] Pushed: %s", message);
                    reply_json(c, 200, "{\"success\":true,\"message\":\"Pushed to WebSocket\"}");
                    return;
                }
            }
            
            if (hm->uri.len >= 11 && strncmp(hm->uri.buf, "/api/status", 11) == 0) {
                if (strcmp(method, "GET") == 0) {
                    char response[256];
                    bool has_conn = event_pusher_has_connection();
                    uint64_t count = event_pusher_get_push_count();
                    snprintf(response, sizeof(response),
                        "{\"websocket\":%s,\"push_count\":%lu,\"status\":\"running\"}",
                        has_conn ? "connected" : "disconnected", count);
                    reply_json(c, 200, response);
                    return;
                }
            }
            
            if (hm->uri.len >= 12 && strncmp(hm->uri.buf, "/api/iceoryx", 12) == 0) {
                if (strcmp(method, "POST") == 0) {
                    if (hm->body.len > 0) {
                        char body_data[2048];
                        size_t n = hm->body.len < sizeof(body_data)-1 ? hm->body.len : sizeof(body_data)-1;
                        memcpy(body_data, hm->body.buf, n);
                        body_data[n] = '\0';
                        event_push_with_type("iceoryx", body_data);
                        log_info("[API] Iceoryx data pushed: %zu bytes", hm->body.len);
                        char resp[64];
                        snprintf(resp, sizeof(resp), "{\"success\":true,\"size\":%zu}", hm->body.len);
                        reply_json(c, 200, resp);
                    } else {
                        reply_json(c, 400, "{\"error\":\"empty body\"}");
                    }
                    return;
                }
            }
            
            if (hm->uri.len >= 10 && strncmp(hm->uri.buf, "/api/log", 8) == 0) {
                if (strcmp(method, "POST") == 0) {
                    char body[1024] = "{\"level\":\"info\",\"message\":\"Log message\"}";
                    if (hm->body.len > 0) {
                        size_t n = hm->body.len < sizeof(body)-1 ? hm->body.len : sizeof(body)-1;
                        memcpy(body, hm->body.buf, n);
                        body[n] = '\0';
                    }
                    event_push_with_type("log", body);
                    log_info("[API] Log pushed");
                    reply_json(c, 200, "{\"success\":true}");
                    return;
                }
            }
            
            // ====== 静态文件 ======
            if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
                ev_handler_static(c, ev, ev_data);
                return;
            }
            
            // ====== /os ======
            if (hm->uri.len >= 3 && strncmp(hm->uri.buf, "/os", 3) == 0) {
                event_push_with_type("os", "{\"operation\":\"os_request\"}");
                ev_handler_os(c, ev, ev_data);
                return;
            }
            
            // ====== Content-Type 路由 ======
            char content_type[128] = "";
            const struct mg_str *ctype_hdr = mg_http_get_header(hm, "Content-Type");
            if (ctype_hdr) {
                size_t n = ctype_hdr->len < sizeof(content_type)-1 ? ctype_hdr->len : sizeof(content_type)-1;
                memcpy(content_type, ctype_hdr->buf, n);
                content_type[n] = '\0';
            }
            
            switch (parse_content_type(content_type)) {
                case CT_DATABASE: 
                    event_push_with_type("database", "{\"operation\":\"query\"}");
                    ev_handler_database(c, ev, ev_data);
                    return;
                case CT_OS:       
                    event_push_with_type("os", "{\"operation\":\"os_call\"}");
                    ev_handler_os(c, ev, ev_data); 
                    return;
                case CT_LIBARY:   
                    event_push_with_type("libary", "{\"operation\":\"library_call\"}");
                    ev_handler_libary(c, ev, ev_data); 
                    return;
                case CT_CHAT:     
                    event_push_with_type("chat", "{\"operation\":\"chat_message\"}");
                    ev_handler_chat(c, ev, ev_data); 
                    return;
                default:
                    reply_json(c, 404, "{\"error\":404,\"message\":\"Not Found\"}");
                    return;
            }
            break;
        }

        // WebSocket 事件由 ws_ev_handler 处理
        case MG_EV_WS_OPEN:
        case MG_EV_WS_MSG:
        case MG_EV_CLOSE:
            break;

        default:
            break;
    }
}
