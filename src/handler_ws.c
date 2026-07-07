#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mongoose.h"
#include "log.h" 
#include "server.h"
#include "calldyn.h"
#include "global.h"

// =====================================================
// 内部函数声明
// =====================================================

static int call_soket_demo(const char *func_name, const char *json_in, char **json_out);

// =====================================================
// 调用 soket_demo 动态库
// =====================================================

static int call_soket_demo(const char *func_name, const char *json_in, char **json_out) {
    char ffi_path[512];
    
    // 构建动态库路径
    snprintf(ffi_path, sizeof(ffi_path), 
             "%s%ssoket_demo%s", 
             LIBARY_DIR, PATH_SEP, FFI_EXT);
    
    log_debug("[WS] Calling: %s from %s", func_name, ffi_path);
    
    return call_local_dyn_libffi(ffi_path, func_name, json_in, json_out);
}

// =====================================================
// WebSocket 事件回调
// =====================================================

void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_WS_OPEN: {
            log_info("[WS] Connection opened from %s", 
                    c->peer_ip ? c->peer_ip : "unknown");
            
            // 发送连接成功消息
            char response[128];
            snprintf(response, sizeof(response), 
                    "{\"code\":0,\"msg\":\"connected\",\"peer\":\"%s\"}", 
                    c->peer_ip ? c->peer_ip : "unknown");
            struct mg_ws_message connect_msg;
            connect_msg.data.buf = response;
            connect_msg.data.len = strlen(response);
            connect_msg.flags = MG_WS_OP_TEXT;
            mg_ws_send(c, connect_msg.data.buf, connect_msg.data.len, connect_msg.flags);
            break;
        }

        case MG_EV_WS_MSG: {
            struct mg_ws_message *msg = (struct mg_ws_message *) ev_data;
            
            // 提取消息内容
            char *json_in = NULL;
            if (msg->data.len > 0) {
                json_in = (char *)malloc(msg->data.len + 1);
                if (json_in) {
                    memcpy(json_in, msg->data.buf, msg->data.len);
                    json_in[msg->data.len] = '\0';
                    log_info("[WS] Received: %.*s", (int)msg->data.len, json_in);
                }
            }
            
            // 如果消息为空，默认调用 status
            const char *func_name = "process_status";
            const char *json_to_send = json_in ? json_in : "{}";
            
            // 从 JSON 中提取 action 字段
            if (json_in && strlen(json_in) > 0) {
                char *action_start = strstr(json_in, "\"action\"");
                if (action_start) {
                    char *colon = strchr(action_start, ':');
                    if (colon) {
                        char *quote1 = strchr(colon, '"');
                        if (quote1) {
                            char *quote2 = strchr(quote1 + 1, '"');
                            if (quote2) {
                                int len = quote2 - quote1 - 1;
                                if (len > 0 && len < 64) {
                                    static char action_buf[64];
                                    memcpy(action_buf, quote1 + 1, len);
                                    action_buf[len] = '\0';
                                    func_name = action_buf;
                                    log_info("[WS] Action: %s", func_name);
                                }
                            }
                        }
                    }
                }
            }
            
            // 调用动态库
            char *json_out = NULL;
            int rc = call_soket_demo(func_name, json_to_send, &json_out);
            
            // 发送响应
            if (rc == 0 && json_out) {
                struct mg_ws_message resp_msg;
                resp_msg.data.buf = json_out;
                resp_msg.data.len = strlen(json_out);
                resp_msg.flags = MG_WS_OP_TEXT;
                mg_ws_send(c, resp_msg.data.buf, resp_msg.data.len, resp_msg.flags);
                free(json_out);
            } else {
                const char *error = "{\"code\":-1,\"msg\":\"call failed\"}";
                struct mg_ws_message err_msg;
                err_msg.data.buf = error;
                err_msg.data.len = strlen(error);
                err_msg.flags = MG_WS_OP_TEXT;
                mg_ws_send(c, err_msg.data.buf, err_msg.data.len, err_msg.flags);
                if (json_out) free(json_out);
            }
            
            free(json_in);
            break;
        }

        case MG_EV_CLOSE: {
            log_info("[WS] Connection closed from %s", 
                    c->peer_ip ? c->peer_ip : "unknown");
            break;
        }

        case MG_EV_ERROR: {
            log_error("[WS] Error: %s", (char *)ev_data);
            break;
        }

        default:
            break;
    }
}

// =====================================================
// WebSocket 接受处理
// =====================================================

void ws_accept_manual(struct mg_connection *c, struct mg_http_message *hm) {
    // 检查是否为 WebSocket 升级请求
    if (mg_http_is_websocket(hm)) {
        c->is_websocket = 1;
        c->fn = ws_ev_handler;
        
        // 发送 HTTP 101 Switching Protocols 响应
        mg_http_reply(c, 101,
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n",
            mg_websocket_accept(hm));
        
        log_info("[WS] Handshake accepted from %s", 
                c->peer_ip ? c->peer_ip : "unknown");
    } else {
        // 处理普通 HTTP 请求，返回 WebSocket 信息
        mg_http_reply(c, 200, 
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"code\":0,\"msg\":\"WebSocket server\",\"endpoint\":\"ws://%s/ws\"}",
            c->peer_ip ? c->peer_ip : "localhost");
    }
}

// =====================================================
// HTTP API 接口（用于测试调用 soket_demo）
// =====================================================

void ev_handler_ws_api(struct mg_connection *c, int ev, void *ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;
    
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    
    // 获取 URI
    char uri[128] = {0};
    int ulen = hm->uri.len < (int)sizeof(uri) - 1 ? (int)hm->uri.len : (int)sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, ulen);
    uri[ulen] = '\0';
    
    // 获取请求体
    char *json_in = NULL;
    if (hm->body.len > 0) {
        json_in = (char *)malloc(hm->body.len + 1);
        if (json_in) {
            memcpy(json_in, hm->body.buf, hm->body.len);
            json_in[hm->body.len] = '\0';
        }
    }
    
    // 从 URI 提取函数名
    // 格式: /ws/api/init -> process_init
    //       /ws/api/status -> process_status
    //       /ws/api/create -> process_create
    //       /ws/api/update -> process_update
    //       /ws/api/msg -> process_msg
    //       /ws/api/pause -> process_pause
    //       /ws/api/resume -> process_resume
    //       /ws/api/close -> process_close
    char func_name[64] = "process_status";
    char *api_start = strstr(uri, "/ws/api/");
    if (api_start) {
        char *action = api_start + 8; // 跳过 "/ws/api/"
        if (strlen(action) > 0) {
            snprintf(func_name, sizeof(func_name), "process_%s", action);
            log_info("[WS_API] Action: %s -> %s", action, func_name);
        }
    }
    
    // 调用动态库
    char *json_out = NULL;
    int rc = call_soket_demo(func_name, json_in ? json_in : "{}", &json_out);
    
    free(json_in);
    
    if (rc != 0 || !json_out) {
        mg_http_reply(c, 500,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"code\":-1,\"msg\":\"call failed\"}");
        if (json_out) free(json_out);
        return;
    }
    
    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%s", json_out);
    
    free(json_out);
}