// src/handler_sse.c - Server-Sent Events 端点
//
// 设计要点：
//   - 事件源来自 ./libary/sse_demo.<ext> 的 sse_next 导出函数
//   - 所有发送都在 mongoose 主循环线程内完成（MG_EV_POLL）
//   - 每个客户端独立维护 last_id，支持 Last-Event-ID 续传
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "global.h"
#include "calldyn.h"
#include "handler_sse.h"

#define SSE_DEFAULT_INTERVAL_MS 200
#define SSE_MIN_INTERVAL_MS 50
#define SSE_MAX_INTERVAL_MS 5000
#define SSE_KEEPALIVE_MS 15000
#define SSE_DATA_MAX 4096
#define SSE_EVENT_BUF 8192

typedef struct {
    struct mg_connection *c;
    long last_id;            // 已发送的最后事件 id
    uint64_t last_poll_ms;   // 上次拉取事件的时间
    uint64_t last_ping_ms;   // 上次心跳时间
    int interval_ms;         // 拉取间隔
    int active;
} sse_client_t;

static sse_client_t g_sse_clients[SSE_MAX_CLIENTS];

static void sse_plugin_path(char *out, size_t out_size) {
    snprintf(out, out_size, "%s%s%s%s", LIBARY_DIR, PATH_SEP, "sse_demo", FFI_EXT);
}

static sse_client_t *sse_find(struct mg_connection *c) {
    for (int i = 0; i < SSE_MAX_CLIENTS; i++) {
        if (g_sse_clients[i].active && g_sse_clients[i].c == c) {
            return &g_sse_clients[i];
        }
    }
    return NULL;
}

int sse_format_event(char *out, size_t out_size, long id,
                     const char *event, const char *data) {
    if (!out || out_size == 0) return -1;
    if (!event || !*event) event = "message";
    if (!data) data = "null";

    int n = snprintf(out, out_size, "id: %ld\nevent: %s\ndata: %s\n\n",
                     id, event, data);
    if (n < 0 || (size_t) n >= out_size) return -1;
    return n;
}

// 从插件拉取并发送一条事件；返回发送字节数，0 表示无新事件，-1 失败
static int sse_send_event(sse_client_t *cl) {
    char plugin[512];
    sse_plugin_path(plugin, sizeof(plugin));

    char json_in[64];
    snprintf(json_in, sizeof(json_in), "{\"after_id\":%ld}", cl->last_id);

    char *json_out = NULL;
    int rc = call_local_dyn_libffi(plugin, "sse_next", json_in, &json_out);
    if (rc != 0 || !json_out) {
        free(json_out);
        log_warn("[SSE] sse_next failed (rc=%d, plugin=%s)", rc, plugin);
        return -1;
    }

    struct mg_str out = mg_str(json_out);
    long id = mg_json_get_long(out, "$.id", 0);
    if (id <= cl->last_id) {
        free(json_out);
        return 0;
    }

    char *event = mg_json_get_str(out, "$.event");
    struct mg_str data = mg_json_get_tok(out, "$.data");

    char data_buf[SSE_DATA_MAX];
    size_t dlen = data.len < sizeof(data_buf) - 1 ? data.len : sizeof(data_buf) - 1;
    if (dlen > 0 && data.buf) {
        memcpy(data_buf, data.buf, dlen);
    }
    data_buf[dlen] = '\0';
    if (dlen == 0) {
        strcpy(data_buf, "null");
    }

    char buf[SSE_EVENT_BUF];
    int n = sse_format_event(buf, sizeof(buf), id,
                             event ? event : "message", data_buf);
    if (n > 0) {
        mg_send(cl->c, buf, (size_t) n);
        cl->last_id = id;
    }

    mg_free(event);
    free(json_out);
    return n > 0 ? n : -1;
}

void sse_on_poll(struct mg_connection *c) {
    sse_client_t *cl = sse_find(c);
    if (!cl) return;

    uint64_t now = mg_millis();

    if (now - cl->last_poll_ms >= (uint64_t) cl->interval_ms) {
        cl->last_poll_ms = now;
        sse_send_event(cl);
    }

    if (now - cl->last_ping_ms >= SSE_KEEPALIVE_MS) {
        cl->last_ping_ms = now;
        mg_send(c, ": ping\n\n", 7);
    }
}

void sse_on_close(struct mg_connection *c) {
    sse_client_t *cl = sse_find(c);
    if (!cl) return;
    log_info("[SSE] client disconnected (last_id=%ld)", cl->last_id);
    memset(cl, 0, sizeof(*cl));
}

void ev_handler_sse(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        if (!(hm->method.len == 3 && memcmp(hm->method.buf, "GET", 3) == 0)) {
            mg_http_reply(c, 405,
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "{\"error\":405,\"message\":\"Method Not Allowed\"}");
            return;
        }

        char plugin[512];
        sse_plugin_path(plugin, sizeof(plugin));
        FILE *fp = fopen(plugin, "rb");
        if (!fp) {
            log_error("[SSE] plugin not found: %s", plugin);
            mg_http_reply(c, 503,
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "{\"error\":503,\"message\":\"SSE plugin unavailable\"}");
            return;
        }
        fclose(fp);

        int slot = -1;
        for (int i = 0; i < SSE_MAX_CLIENTS; i++) {
            if (!g_sse_clients[i].active) { slot = i; break; }
        }
        if (slot < 0) {
            mg_http_reply(c, 503,
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "{\"error\":503,\"message\":\"Too many SSE clients\"}");
            return;
        }

        // Last-Event-ID: 支持断线续传
        long last_id = 0;
        struct mg_str *hdr = mg_http_get_header(hm, "Last-Event-ID");
        if (hdr && hdr->len > 0) {
            char tmp[32];
            size_t n = hdr->len < sizeof(tmp) - 1 ? hdr->len : sizeof(tmp) - 1;
            memcpy(tmp, hdr->buf, n);
            tmp[n] = '\0';
            last_id = strtol(tmp, NULL, 10);
            if (last_id < 0) last_id = 0;
        }

        int interval = SSE_DEFAULT_INTERVAL_MS;
        char q[32];
        if (mg_http_get_var(&hm->query, "interval_ms", q, sizeof(q)) > 0) {
            int v = atoi(q);
            if (v < SSE_MIN_INTERVAL_MS) v = SSE_MIN_INTERVAL_MS;
            if (v > SSE_MAX_INTERVAL_MS) v = SSE_MAX_INTERVAL_MS;
            interval = v;
        }

        // SSE 不能使用 mg_http_reply（会带 Content-Length），手动写响应头
        mg_printf(c,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "X-Accel-Buffering: no\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n");

        sse_client_t *cl = &g_sse_clients[slot];
        memset(cl, 0, sizeof(*cl));
        cl->c = c;
        cl->last_id = last_id;
        cl->interval_ms = interval;
        cl->last_poll_ms = mg_millis();
        cl->last_ping_ms = mg_millis();
        cl->active = 1;

        // 立即推送一条，客户端无需等待首个 interval
        sse_send_event(cl);

        log_info("[SSE] client connected (slot=%d, last_id=%ld, interval=%d)",
                 slot, last_id, interval);
        return;
    }

    if (ev == MG_EV_POLL) {
        sse_on_poll(c);
    } else if (ev == MG_EV_CLOSE) {
        sse_on_close(c);
    }
}
