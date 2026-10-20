#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "event_pusher.h"
#include <sys/stat.h>
#include "global.h"
#include "handler_ws.h"
#include "handler_sse.h"

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
            
            // ====== SSE ======
            if (hm->uri.len == 4 && strncmp(hm->uri.buf, "/sse", 4) == 0) {
                ev_handler_sse(c, ev, ev_data);
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
            
            // ====== libary 插件路由（任意方法，含 GET/HEAD） ======
            // URI 对应 ./libary/<name>.so 存在时优先交给 libary handler
            {
                char probe[512];
                size_t plen = hm->uri.len < sizeof(probe) - 1 ? hm->uri.len : sizeof(probe) - 1;
                memcpy(probe, hm->uri.buf, plen);
                probe[plen] = '\0';
                char *q = strchr(probe, '?');
                if (q) *q = '\0';

                const char *clean = probe[0] == '/' ? probe + 1 : probe;
                if (*clean && !uri_has_traversal(probe)) {
                    char lib_path[600];
                    snprintf(lib_path, sizeof(lib_path), "%s%s%s%s",
                             LIBARY_DIR, PATH_SEP, clean, FFI_EXT);
                    struct stat st;
                    if (stat(lib_path, &st) == 0) {
                        event_push_with_type("libary", "{\"operation\":\"library_call\"}");
                        ev_handler_libary(c, ev, ev_data);
                        return;
                    }
                }
            }

            // ====== 静态文件（GET/HEAD） ======
            if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
                ev_handler_static(c, ev, ev_data);
                return;
            }

            // ====== 其余请求统一路由到 libary 动态库 ======
            event_push_with_type("libary", "{\"operation\":\"library_call\"}");
            ev_handler_libary(c, ev, ev_data);
            break;
        }

        // WebSocket 事件由 ws_ev_handler 处理
        case MG_EV_WS_OPEN:
        case MG_EV_WS_MSG:
            break;

        // SSE 长连接：主循环轮询与关闭清理
        case MG_EV_POLL:
            sse_on_poll(c);
            break;
        case MG_EV_CLOSE:
            sse_on_close(c);
            break;

        default:
            break;
    }
}
