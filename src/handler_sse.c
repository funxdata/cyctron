// src/handler_sse.c - Server-Sent Events 端点
//
// 支持两种事件源：
//   1. 默认：./libary/sse_demo.<ext> 的 sse_next，输出 {"id","event","data"}
//   2. 通用：GET /sse?lib=<name>&symbol=<fn>&args=<json>
//      周期调用 ./libary/<name>.<ext> 的 <fn>，把返回 JSON 原样作为 data
//
// 所有发送都在 mongoose 主循环线程内完成（MG_EV_POLL）。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#define SSE_ARGS_MAX 1024
#define SSE_LIB_MAX 64
#define SSE_SYMBOL_MAX 128

typedef struct {
    struct mg_connection *c;
    long last_id;            // 默认模式：插件返回的事件 id
    uint64_t last_poll_ms;   // 上次拉取事件的时间
    uint64_t last_ping_ms;   // 上次心跳时间
    int interval_ms;         // 拉取间隔
    int active;

    // 通用模式：调用指定 lib/symbol
    int custom;
    int once;                // 1 = 只调用一次（适用于弹窗类接口）
    int done;                // once 调用完成标记
    long seq;
    char lib[SSE_LIB_MAX];
    char symbol[SSE_SYMBOL_MAX];
    char args[SSE_ARGS_MAX];
} sse_client_t;

static sse_client_t g_sse_clients[SSE_MAX_CLIENTS];

static int sse_safe_name(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char) *s;
        if (!(isalnum(c) || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static void sse_lib_path(const char *lib, char *out, size_t out_size) {
    snprintf(out, out_size, "%s%s%s%s", LIBARY_DIR, PATH_SEP, lib, FFI_EXT);
}

static void sse_plugin_path(char *out, size_t out_size) {
    sse_lib_path("sse_demo", out, out_size);
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

// 通用模式：调用指定插件符号，把输出原样作为 data
static int sse_send_custom(sse_client_t *cl) {
    char plugin[512];
    sse_lib_path(cl->lib, plugin, sizeof(plugin));

    char *json_out = NULL;
    int rc = call_local_dyn_libffi(plugin, cl->symbol,
                                   cl->args[0] ? cl->args : NULL, &json_out);
    if (rc != 0 || !json_out) {
        free(json_out);
        log_warn("[SSE] %s/%s failed (rc=%d, plugin=%s)",
                 cl->lib, cl->symbol, rc, plugin);
        return -1;
    }

    char data_buf[SSE_DATA_MAX];
    size_t dlen = strlen(json_out);
    if (dlen >= sizeof(data_buf)) dlen = sizeof(data_buf) - 1;
    memcpy(data_buf, json_out, dlen);
    data_buf[dlen] = '\0';

    long id = ++cl->seq;
    char buf[SSE_EVENT_BUF];
    int n = sse_format_event(buf, sizeof(buf), id, cl->symbol, data_buf);
    if (n > 0) mg_send(cl->c, buf, (size_t) n);

    free(json_out);
    if (n > 0 && cl->once) cl->done = 1;
    return n > 0 ? n : -1;
}

// 默认模式：从 sse_demo 拉取并发送一条事件
// 返回发送字节数，0 表示无新事件，-1 失败
static int sse_send_event(sse_client_t *cl) {
    if (cl->done) return 0;

    if (cl->custom) {
        return sse_send_custom(cl);
    }

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
    log_info("[SSE] client disconnected (last_id=%ld, lib=%s, symbol=%s)",
             cl->last_id, cl->lib, cl->symbol);
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

        // 通用模式参数：lib / symbol / args
        char lib[SSE_LIB_MAX] = "sse_demo";
        char symbol[SSE_SYMBOL_MAX] = "sse_next";
        char args[SSE_ARGS_MAX] = "";

        char tmp[SSE_ARGS_MAX];
        if (mg_http_get_var(&hm->query, "lib", tmp, sizeof(tmp)) > 0) {
            snprintf(lib, sizeof(lib), "%s", tmp);
        }
        if (mg_http_get_var(&hm->query, "symbol", tmp, sizeof(tmp)) > 0) {
            snprintf(symbol, sizeof(symbol), "%s", tmp);
        }
        if (mg_http_get_var(&hm->query, "args", tmp, sizeof(tmp)) > 0) {
            snprintf(args, sizeof(args), "%s", tmp);
        }

        // once=1：只调用一次（文件/目录选择等弹窗接口）
        char once_buf[8];
        int once = 0;
        if (mg_http_get_var(&hm->query, "once", once_buf, sizeof(once_buf)) > 0) {
            once = atoi(once_buf) != 0;
        }

        if (!sse_safe_name(lib) || !sse_safe_name(symbol)) {
            mg_http_reply(c, 400,
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "{\"error\":400,\"message\":\"Invalid lib or symbol\"}");
            return;
        }

        char plugin[512];
        sse_lib_path(lib, plugin, sizeof(plugin));
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

        // Last-Event-ID: 默认模式支持断线续传
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
        cl->custom = !(strcmp(lib, "sse_demo") == 0 && strcmp(symbol, "sse_next") == 0);
        cl->once = once;
        cl->done = 0;
        cl->seq = 0;
        snprintf(cl->lib, sizeof(cl->lib), "%s", lib);
        snprintf(cl->symbol, sizeof(cl->symbol), "%s", symbol);
        snprintf(cl->args, sizeof(cl->args), "%s", args);

        // 立即推送一条，客户端无需等待首个 interval
        sse_send_event(cl);

        log_info("[SSE] client connected (slot=%d, lib=%s, symbol=%s, custom=%d, once=%d, interval=%d)",
                 slot, cl->lib, cl->symbol, cl->custom, cl->once, interval);
        return;
    }

    if (ev == MG_EV_POLL) {
        sse_on_poll(c);
    } else if (ev == MG_EV_CLOSE) {
        sse_on_close(c);
    }
}
