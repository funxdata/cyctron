#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "realtime.h"
#include "event_pusher.h"
#include "calldyn.h"

struct mg_connection *g_ws_conn = NULL;
pthread_mutex_t g_ws_mutex = PTHREAD_MUTEX_INITIALIZER;

static rt_manager_t *g_rt_manager = NULL;

// ============================================
//  JSON 提取辅助（通用）
// ============================================
static char* json_get_string(const char *json, const char *key) {
    static char value[512];
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    const char *start = strstr(json, search_key);
    if (!start) return NULL;
    
    start += strlen(search_key);
    const char *end = strchr(start, '"');
    if (!end) return NULL;
    
    size_t len = end - start;
    if (len >= sizeof(value)) len = sizeof(value) - 1;
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

static int extract_field(const char *data, const char *key, char *buf, size_t size) {
    char *val = json_get_string(data, key);
    if (!val) return -1;
    strncpy(buf, val, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

static void send_event(const char *action, const char *uuid, const char *status) {
    char msg[256];
    snprintf(msg, sizeof(msg), 
             "{\"action\":\"%s\",\"uuid\":\"%s\",\"status\":\"%s\"}",
             action, uuid ? uuid : "", status ? status : "");
    event_push_with_type("realtime", msg);
}

// ============================================
//  命令处理器
// ============================================
static void handle_bind(const char *data) {
    char uuid[64] = {0}, lib[64] = {0};
    if (extract_field(data, "uuid", uuid, sizeof(uuid)) != 0) {
        event_push_with_type("error", "{\"message\":\"Missing uuid for bind\"}");
        return;
    }
    if (extract_field(data, "lib", lib, sizeof(lib)) != 0) {
        strcpy(lib, "libsoket_demo");
    }
    
    char socket_path[128];
    snprintf(socket_path, sizeof(socket_path), "/tmp/cyctron/%s.sock", uuid);
    
    if (!g_rt_manager) {
        g_rt_manager = rt_init();
    }
    
    rt_task_t *task = rt_create_task(g_rt_manager, uuid, socket_path);
    if (task) {
        int ret = rt_start_task(task, lib);
        send_event("bind", uuid, ret == 0 ? "success" : "failed");
        log_info("[WS] Bind %s: %s", uuid, ret == 0 ? "success" : "failed");
    } else {
        event_push_with_type("error", "{\"message\":\"Failed to create task\"}");
        log_error("[WS] Failed to create task for %s", uuid);
    }
}

static void handle_remove(const char *data) {
    char uuid[64] = {0};
    if (extract_field(data, "uuid", uuid, sizeof(uuid)) != 0) {
        event_push_with_type("error", "{\"message\":\"Missing uuid for remove\"}");
        return;
    }
    
    rt_task_t *task = rt_find_task_by_uuid(g_rt_manager, uuid);
    if (task) {
        rt_remove_task(task);
        send_event("remove", uuid, "success");
        log_info("[WS] Removed %s", uuid);
    } else {
        send_event("remove", uuid, "not_found");
        log_warn("[WS] Task %s not found", uuid);
    }
}

static void handle_status(void) {
    int count = g_rt_manager ? rt_get_task_count(g_rt_manager) : 0;
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"action\":\"status\",\"count\":%d}", count);
    event_push_with_type("realtime", msg);
    log_info("[WS] Status: %d tasks", count);
    if (count > 0) rt_dump_tasks(g_rt_manager);
}

static void handle_msg(const char *data) {
    char uuid[64] = {0}, msg_data[1024] = {0};
    if (extract_field(data, "uuid", uuid, sizeof(uuid)) != 0) {
        event_push_with_type("error", "{\"message\":\"Missing uuid for msg\"}");
        return;
    }
    extract_field(data, "data", msg_data, sizeof(msg_data));
    
    rt_task_t *task = rt_find_task_by_uuid(g_rt_manager, uuid);
    if (task) {
        event_push_with_type("msg", msg_data);
        send_event("msg", uuid, "sent");
        log_info("[WS] Msg sent to %s: %s", uuid, msg_data);
    } else {
        send_event("msg", uuid, "not_found");
        log_warn("[WS] Task %s not found", uuid);
    }
}

// ============================================
//  消息处理入口
// ============================================
static void handle_ws_message(const char *data, size_t len) {
    // ping
    if (strcmp(data, "ping") == 0) {
        event_push("{\"type\":\"pong\",\"timestamp\":\"now\"}");
        return;
    }
    
    // status (text)
    if (strcmp(data, "status") == 0) {
        event_push_with_type("system", "{\"status\":\"running\"}");
        return;
    }
    
    // 非 JSON
    if (data[0] != '{') {
        char json_data[512];
        snprintf(json_data, sizeof(json_data), 
                 "{\"message\":\"%.*s\",\"len\":%zu}", 
                 (int)(len < 400 ? len : 400), data, len);
        event_push_with_type("echo", json_data);
        return;
    }
    
    // 提取 cmd
    char cmd[64] = {0};
    if (extract_field(data, "cmd", cmd, sizeof(cmd)) != 0) {
        event_push_with_type("error", "{\"message\":\"Missing cmd field\"}");
        return;
    }
    
    log_info("[WS] Command: %s", cmd);
    
    if (strcmp(cmd, "bind") == 0) {
        handle_bind(data);
    } else if (strcmp(cmd, "remove") == 0) {
        handle_remove(data);
    } else if (strcmp(cmd, "status") == 0) {
        handle_status();
    } else if (strcmp(cmd, "msg") == 0) {
        handle_msg(data);
    } else {
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"cmd\":\"%s\",\"status\":\"unknown\"}", cmd);
        event_push_with_type("error", resp);
        log_warn("[WS] Unknown command: %s", cmd);
    }
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
            
            log_info("[WS] Connected");
            if (!g_rt_manager) g_rt_manager = rt_init();
            
            event_push_with_type("system", "{\"status\":\"online\"}");
            event_push_message("WebSocket client connected");
            break;
        }

        case MG_EV_WS_MSG: {
            struct mg_ws_message *msg = (struct mg_ws_message *) ev_data;
            char data[2048] = {0};
            size_t len = msg->data.len < sizeof(data)-1 ? msg->data.len : sizeof(data)-1;
            memcpy(data, msg->data.buf, len);
            
            log_info("[WS] Received: %s", data);
            handle_ws_message(data, len);
            break;
        }

        case MG_EV_CLOSE: {
            pthread_mutex_lock(&g_ws_mutex);
            g_ws_conn = NULL;
            pthread_mutex_unlock(&g_ws_mutex);
            
            log_info("[WS] Closed");
            if (g_rt_manager) {
                rt_cleanup(g_rt_manager);
                g_rt_manager = NULL;
            }
            event_push_with_type("system", "{\"status\":\"offline\"}");
            break;
        }
        
        default:
            break;
    }
}

