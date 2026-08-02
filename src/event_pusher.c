#include "event_pusher.h"
#include "log.h"
#include "mongoose.h"
#include "handler_ws.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

static bool g_initialized = false;
static uint64_t g_push_count = 0;
static nng_socket g_nng_sock = NNG_SOCKET_INITIALIZER;
static int g_nng_initialized = 0;

static void nng_init_internal(void) {
    if (g_nng_initialized) return;

    int rv = nng_pub0_open(&g_nng_sock);
    if (rv != 0) {
        log_error("[NNG] nng_pub0_open failed: %s", nng_strerror(rv));
        return;
    }

    const char *ipc_path = "ipc:///tmp/cyctron.sock";
    rv = nng_listen(g_nng_sock, ipc_path, NULL, 0);
    if (rv != 0) {
        log_error("[NNG] nng_listen %s failed: %s", ipc_path, nng_strerror(rv));
        nng_close(g_nng_sock);
        return;
    }

    g_nng_initialized = 1;
    log_info("[NNG] Listening on %s", ipc_path);
}

static void nng_push_internal(const char *data) {
    if (!g_nng_initialized || !data) return;
    
    nng_msg *msg = NULL;
    if (nng_msg_alloc(&msg, 0) != 0) return;
    if (nng_msg_append(msg, data, strlen(data)) != 0) {
        nng_msg_free(msg);
        return;
    }
    nng_sendmsg(g_nng_sock, msg, 0);
}

bool event_pusher_init(void) {
    if (g_initialized) {
        log_warn("[EventPusher] Already initialized");
        return true;
    }
    
    g_initialized = true;
    g_push_count = 0;
    nng_init_internal();
    log_info("[EventPusher] Initialized");
    return true;
}

void event_pusher_cleanup(void) {
    if (!g_initialized) return;
    
    if (g_nng_initialized) {
        nng_close(g_nng_sock);
        g_nng_initialized = 0;
    }
    g_initialized = false;
    log_info("[EventPusher] Cleaned up (total pushes: %lu)", g_push_count);
}

void event_push(const char *data) {
    if (!g_initialized || !data) return;
    
    pthread_mutex_lock(&g_ws_mutex);
    if (g_ws_conn && g_ws_conn->is_websocket) {
        mg_ws_send(g_ws_conn, data, strlen(data), WEBSOCKET_OP_TEXT);
        g_push_count++;
        log_debug("[EventPusher] WS: %s", data);
    }
    pthread_mutex_unlock(&g_ws_mutex);
    
    nng_push_internal(data);
}

void event_push_with_type(const char *type, const char *data) {
    if (!type || !data) return;
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "{\"type\":\"%s\",\"data\":%s,\"timestamp\":%ld}",
             type, data, time(NULL));
    event_push(buffer);
}

void event_push_message(const char *msg) {
    if (!msg) return;
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "{\"type\":\"message\",\"message\":\"%s\",\"timestamp\":%ld}",
             msg, time(NULL));
    event_push(buffer);
}

void event_push_json(const char *json) {
    if (!json) return;
    event_push(json);
}

bool event_pusher_has_connection(void) {
    pthread_mutex_lock(&g_ws_mutex);
    bool has = (g_ws_conn && g_ws_conn->is_websocket);
    pthread_mutex_unlock(&g_ws_mutex);
    return has;
}

uint64_t event_pusher_get_push_count(void) {
    return g_push_count;
}

