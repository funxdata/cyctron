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
#include "plugin_path.h"
#include "uri.h"

extern struct mg_connection *g_ws_conn;
extern pthread_mutex_t g_ws_mutex;

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
                // 先切换到 WS 事件处理器：mg_ws_upgrade 成功时会在内部
                // 同步触发 MG_EV_WS_OPEN，由 ws_ev_handler 完成注册与欢迎消息。
                c->fn = ws_ev_handler;
                mg_ws_upgrade(c, hm, NULL);
                if (!c->is_websocket) {
                    // 缺少 Sec-WebSocket-Key 时 mg_ws_upgrade 已回复 426 并 draining
                    c->fn = ev_handler;
                    log_warn("[WS] upgrade failed conn=%llu",
                             (unsigned long long)c->id);
                }
                return;
            }
            
            // ====== SSE ======
            if (hm->uri.len == 4 && strncmp(hm->uri.buf, "/sse", 4) == 0) {
                ev_handler_sse(c, ev, ev_data);
                return;
            }

            // 去掉 query，构造用于路由判断的路径
            char uri_path[512];
            size_t ulen = hm->uri.len < sizeof(uri_path) - 1
                        ? hm->uri.len : sizeof(uri_path) - 1;
            memcpy(uri_path, hm->uri.buf, ulen);
            uri_path[ulen] = '\0';
            char *q = strchr(uri_path, '?');
            if (q) *q = '\0';

            int get_or_head = strcmp(method, "GET") == 0 ||
                              strcmp(method, "HEAD") == 0;

            // ====== 明确的静态资源优先：带扩展名或 /assets 前缀 ======
            if (get_or_head &&
                (has_file_extension(uri_path) ||
                 strcmp(uri_path, "/assets") == 0 ||
                 strncmp(uri_path, "/assets/", 8) == 0)) {
                ev_handler_static(c, ev, ev_data);
                return;
            }

            // ====== 插件路由（任意方法） ======
            // 能力库从 ./libary/<name>.<ext> 加载
            {
                const char *clean = uri_path[0] == '/' ? uri_path + 1 : uri_path;
                if (*clean && !uri_has_traversal(uri_path)) {
                    char lib_path[600];
                    if (plugin_resolve_path(clean, lib_path, sizeof(lib_path)) == 0) {
                        event_push_with_type("libary", "{\"operation\":\"library_call\"}");
                        ev_handler_libary(c, ev, ev_data);
                        return;
                    }
                }
            }

            // ====== 无后缀 GET/HEAD：页面 / 目录 index / 404 ======
            if (get_or_head) {
                ev_handler_static(c, ev, ev_data);
                return;
            }

            // ====== 其余方法统一路由到 libary 动态库 ======
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
