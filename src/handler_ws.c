#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <pthread.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "realtime.h"
#include "event_pusher.h"
#include "calldyn.h"
#include "ws_call.h"
#include "ws_message.h"
#include "ws_command.h"
#include "ws_worker.h"

struct mg_connection *g_ws_conn = NULL;
pthread_mutex_t g_ws_mutex = PTHREAD_MUTEX_INITIALIZER;

static rt_manager_t *g_rt_manager = NULL;

#define WS_MAX_CONNS 8
static struct mg_connection *g_ws_conns[WS_MAX_CONNS];
static pthread_mutex_t g_ws_conns_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ws_conn_register(struct mg_connection *c) {
    pthread_mutex_lock(&g_ws_conns_mutex);
    for (int i = 0; i < WS_MAX_CONNS; i++) {
        if (g_ws_conns[i] == c) {
            pthread_mutex_unlock(&g_ws_conns_mutex);
            return;
        }
    }
    for (int i = 0; i < WS_MAX_CONNS; i++) {
        if (!g_ws_conns[i]) { g_ws_conns[i] = c; break; }
    }
    pthread_mutex_unlock(&g_ws_conns_mutex);
    log_info("[WS] register conn=%lu", c->id);
}

static void ws_conn_unregister(struct mg_connection *c) {
    pthread_mutex_lock(&g_ws_conns_mutex);
    for (int i = 0; i < WS_MAX_CONNS; i++) {
        if (g_ws_conns[i] == c) g_ws_conns[i] = NULL;
    }
    pthread_mutex_unlock(&g_ws_conns_mutex);

    pthread_mutex_lock(&g_ws_mutex);
    if (g_ws_conn == c) {
        g_ws_conn = NULL;
        for (int i = WS_MAX_CONNS - 1; i >= 0; i--) {
            if (g_ws_conns[i]) { g_ws_conn = g_ws_conns[i]; break; }
        }
    }
    pthread_mutex_unlock(&g_ws_mutex);
}

// ============================================
//  命令处理器
// ============================================
static void send_event_to(struct mg_connection *c, const char *action,
                          const char *uuid, const char *status) {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"action\":\"%s\",\"uuid\":\"%s\",\"status\":\"%s\"}",
             action, uuid ? uuid : "", status ? status : "");
    event_push_with_type_to(c, "realtime", msg);
}

static void handle_bind(struct mg_connection *c, const ws_command_t *req) {
    if (!req->uuid || !req->uuid[0]) {
        event_push_json_string_to(c, "error", "Missing uuid for bind");
        return;
    }

    const char *lib = (req->lib && req->lib[0]) ? req->lib : "libsoket_demo";

    char socket_path[128];
    snprintf(socket_path, sizeof(socket_path), "/tmp/cyctron/%s.sock", req->uuid);

    if (!g_rt_manager) {
        g_rt_manager = rt_init();
    }

    rt_task_t *task = rt_create_task(g_rt_manager, req->uuid, socket_path);
    if (task) {
        task->conn_id = c->id;
        int ret = rt_start_task(task, lib);
        send_event_to(c, "bind", req->uuid, ret == 0 ? "success" : "failed");
        log_info("[WS] Bind %s: %s", req->uuid, ret == 0 ? "success" : "failed");
    } else {
        event_push_json_string_to(c, "error", "Failed to create task");
        log_error("[WS] Failed to create task for %s", req->uuid);
    }
}

static void handle_remove(struct mg_connection *c, const ws_command_t *req) {
    if (!req->uuid || !req->uuid[0]) {
        event_push_json_string_to(c, "error", "Missing uuid for remove");
        return;
    }

    rt_task_t *task = g_rt_manager
        ? rt_find_task_by_uuid(g_rt_manager, req->uuid)
        : NULL;
    if (task) {
        rt_remove_task(task);
        send_event_to(c, "remove", req->uuid, "success");
        log_info("[WS] Removed %s", req->uuid);
    } else {
        send_event_to(c, "remove", req->uuid, "not_found");
        log_warn("[WS] Task %s not found", req->uuid);
    }
}

static void handle_status(struct mg_connection *c) {
    int count = g_rt_manager ? rt_get_task_count(g_rt_manager) : 0;
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"action\":\"status\",\"count\":%d}", count);
    event_push_with_type_to(c, "realtime", msg);
    log_info("[WS] Status: %d tasks", count);
    if (count > 0) rt_dump_tasks(g_rt_manager);
}

static void handle_msg(struct mg_connection *c, const ws_command_t *req) {
    if (!req->uuid || !req->uuid[0]) {
        event_push_json_string_to(c, "error", "Missing uuid for msg");
        return;
    }

    const char *msg_data = req->data ? req->data : "";
    rt_task_t *task = g_rt_manager
        ? rt_find_task_by_uuid(g_rt_manager, req->uuid)
        : NULL;
    if (task) {
        event_push_json_string_to(c, "msg", msg_data);
        send_event_to(c, "msg", req->uuid, "sent");
        log_info("[WS] Msg sent to %s: %s", req->uuid, msg_data);
    } else {
        send_event_to(c, "msg", req->uuid, "not_found");
        log_warn("[WS] Task %s not found", req->uuid);
    }
}

// ============================================
//  call：一次 socket 请求 → 一次 FFI 调用
// ============================================
static void handle_call(struct mg_connection *c, const char *data, size_t len) {
    ws_call_req_t req;
    char err[128] = {0};
    int code = ws_call_parse(data, len, &req, err, sizeof(err));

    if (code != WS_CALL_OK) {
        ws_call_resp_t resp;
        if (ws_call_build_error(req.id, code, err, &resp) == 0) {
            event_push_to(c, resp.json);
            ws_call_resp_free(&resp);
        }
        ws_call_req_free(&req);
        return;
    }

    if (ws_worker_submit(c->id, req.id, req.lib, req.symbol, req.args,
                         req.timeout_ms) != 0) {
        log_warn("[WS] call queue full id=%s", req.id ? req.id : "");
        ws_call_resp_t resp;
        if (ws_call_build_error(req.id, -6, "worker busy", &resp) == 0) {
            event_push_to(c, resp.json);
            ws_call_resp_free(&resp);
        }
    } else {
        log_debug("[WS] call queued id=%s conn=%lu", req.id ? req.id : "", c->id);
    }

    ws_call_req_free(&req);
}

static struct mg_connection *ws_conn_find(unsigned long conn_id) {
    struct mg_connection *found = NULL;
    pthread_mutex_lock(&g_ws_conns_mutex);
    for (int i = 0; i < WS_MAX_CONNS; i++) {
        if (g_ws_conns[i] && g_ws_conns[i]->id == conn_id) {
            found = g_ws_conns[i];
            break;
        }
    }
    pthread_mutex_unlock(&g_ws_conns_mutex);
    log_debug("[WS] find conn=%lu -> %s", conn_id, found ? "hit" : "miss");
    return found;
}

static void ws_call_done(unsigned long conn_id, const char *json, void *user) {
    (void)user;
    struct mg_connection *c = ws_conn_find(conn_id);
    if (c) {
        event_push_to(c, json);
    } else {
        log_warn("[WS] call done for closed conn=%lu (dropped)", conn_id);
    }
}

void ws_drain_call_results(void) {
    ws_worker_drain(ws_call_done, NULL);
}

// ============================================
//  消息处理入口
// ============================================
static void handle_ws_message(struct mg_connection *c, const char *data, size_t len) {
    // ping
    if (strcmp(data, "ping") == 0) {
        event_push_to(c, "{\"type\":\"pong\",\"timestamp\":\"now\"}");
        return;
    }
    
    // status (text)
    if (strcmp(data, "status") == 0) {
        event_push_with_type_to(c, "system", "{\"status\":\"running\"}");
        return;
    }
    
    // 非 JSON
    if (data[0] != '{') {
        char esc[4096];
        event_escape_json(data, esc, sizeof(esc));
        char json_data[4224];
        snprintf(json_data, sizeof(json_data),
                 "{\"message\":\"%s\",\"len\":%zu}",
                 esc, len);
        event_push_with_type_to(c, "echo", json_data);
        return;
    }
    
    ws_command_t req;
    char err[128] = {0};
    if (ws_command_parse(data, len, &req, err, sizeof(err)) != 0) {
        event_push_json_string_to(c, "error", err[0] ? err : "missing cmd");
        return;
    }

    log_info("[WS] Command: %s", req.cmd);
    
    if (strcmp(req.cmd, "bind") == 0) {
        handle_bind(c, &req);
    } else if (strcmp(req.cmd, "remove") == 0) {
        handle_remove(c, &req);
    } else if (strcmp(req.cmd, "status") == 0) {
        handle_status(c);
    } else if (strcmp(req.cmd, "msg") == 0) {
        handle_msg(c, &req);
    } else if (strcmp(req.cmd, "call") == 0) {
        handle_call(c, data, len);
    } else {
        char ecmd[128];
        event_escape_json(req.cmd, ecmd, sizeof(ecmd));
        char resp[288];
        snprintf(resp, sizeof(resp), "{\"cmd\":\"%s\",\"status\":\"unknown\"}", ecmd);
        event_push_with_type_to(c, "error", resp);
        log_warn("[WS] Unknown command: %s", req.cmd);
    }

    ws_command_free(&req);
}

void ws_on_upgrade(struct mg_connection *c) {
    ws_conn_register(c);

    pthread_mutex_lock(&g_ws_mutex);
    g_ws_conn = c;
    pthread_mutex_unlock(&g_ws_mutex);

    if (!g_rt_manager) g_rt_manager = rt_init();

    event_push_with_type_to(c, "system", "{\"status\":\"online\"}");
    event_push_message_to(c, "WebSocket client connected");
    log_info("[WS] Connected conn=%lu", c->id);
}

// ============================================
//  WebSocket 事件处理
// ============================================
void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_WS_OPEN: {
            pthread_mutex_lock(&g_ws_mutex);
            g_ws_conn = c;
            pthread_mutex_unlock(&g_ws_mutex);
            ws_conn_register(c);

            log_info("[WS] Connected");
            if (!g_rt_manager) g_rt_manager = rt_init();
            
            event_push_with_type_to(c, "system", "{\"status\":\"online\"}");
            event_push_message_to(c, "WebSocket client connected");
            break;
        }

        case MG_EV_WS_MSG: {
            struct mg_ws_message *msg = (struct mg_ws_message *) ev_data;
            size_t len = msg->data.len;

            if (ws_message_check_size(len) != 0) {
                log_warn("[WS] message too large: %zu (limit %d)",
                         len, ws_message_limit());
                event_push_json_string_to(c, "error", "message too large");
                c->is_draining = 1;
                break;
            }

            char *data = (char *)malloc(len + 1);
            if (!data) {
                log_error("[WS] out of memory for %zu bytes", len);
                c->is_draining = 1;
                break;
            }
            memcpy(data, msg->data.buf, len);
            data[len] = 0;

            log_info("[WS] Received %zu bytes", len);
            handle_ws_message(c, data, len);
            free(data);
            break;
        }

        case MG_EV_CLOSE: {
            ws_conn_unregister(c);

            log_info("[WS] Closed conn=%lu", c->id);
            if (g_rt_manager) {
                int removed = rt_remove_tasks_by_conn(g_rt_manager, c->id);
                if (removed > 0) {
                    log_info("[WS] Removed %d task(s) for conn %lu",
                             removed, c->id);
                }
            }
            break;
        }
        
        default:
            break;
    }
}

