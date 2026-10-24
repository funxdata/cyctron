#include <stdio.h>
#include <string.h>
#include "mongoose.h"
#include "server.h"
#include "uri.h"
#include "log.h"
#include "global.h"

#define INDEX_HTML "index.html"

// 解析静态文件请求 URI 到磁盘相对路径（导出供单元测试）
// 规则：含 ".." 拒绝；无后缀 → index.html；带后缀 → 原样映射
// 返回 0 成功，-1 表示非法请求
int static_resolve_path(const char *uri, char *out, size_t out_size) {
    if (uri_has_traversal(uri)) return -1;

    if (!has_file_extension(uri)) {
        // 目录或根路径 → index.html
        if (uri[0] == '\0' || strcmp(uri, "/") == 0) {
            snprintf(out, out_size, "%s", WEB_ROOT "/" INDEX_HTML);
        } else {
            snprintf(out, out_size, "%s%s/%s", WEB_ROOT, uri, INDEX_HTML);
        }
        return 0;
    }

    // 带后缀：/assets/js/app.js → web/assets/js/app.js
    snprintf(out, out_size, "%s%s", WEB_ROOT, uri);
    return 0;
}

// 判断无后缀 URI 是否允许 SPA fallback（仅页面路由）。
// /api/*、/soket、/sse 以及带后缀的资源不参与回退。
int static_should_fallback(const char *uri) {
    if (!uri || uri[0] == '\0' || strcmp(uri, "/") == 0) return 0;
    if (has_file_extension(uri)) return 0;
    if (strcmp(uri, "/api") == 0 || strncmp(uri, "/api/", 5) == 0) return 0;
    if (strcmp(uri, "/soket") == 0) return 0;
    if (strcmp(uri, "/sse") == 0 || strncmp(uri, "/sse?", 5) == 0) return 0;
    return 1;
}

void ev_handler_static(struct mg_connection *c, int ev, void *ev_data) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    char uri[512];
    int len = hm->uri.len < sizeof(uri) - 1 ? (int) hm->uri.len : sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, len);
    uri[len] = '\0';

    // 去掉 query string（静态资源不应带）
    char *q = strchr(uri, '?');
    if (q) *q = '\0';

    if (uri_has_traversal(uri)) {
        mg_http_reply(c, 400,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":400,\"message\":\"Bad Request\"}");
        return;
    }

    char file_path[1024];
    if (static_resolve_path(uri, file_path, sizeof(file_path)) != 0) {
        mg_http_reply(c, 400,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":400,\"message\":\"Bad Request\"}");
        return;
    }

    // SPA fallback：页面路由（/chat 等）对应的 index.html 不存在时，
    // 回退到 web/index.html，由前端 init 从 URL 恢复路由。
    if (static_should_fallback(uri) &&
        strcmp(file_path, WEB_ROOT "/" INDEX_HTML) != 0) {
        FILE *fp = fopen(file_path, "rb");
        if (!fp) {
            snprintf(file_path, sizeof(file_path), "%s/%s", WEB_ROOT, INDEX_HTML);
            log_debug("[Static] SPA fallback: %s -> %s", uri, file_path);
        } else {
            fclose(fp);
        }
    }

    log_debug("[Static] %s -> %s", uri, file_path);

    struct mg_http_serve_opts opts = {0};
    opts.root_dir = WEB_ROOT;
    opts.mime_types = "svg=image/svg+xml,json=application/json,"
                      "js=text/javascript,mjs=text/javascript";
    opts.fs = &mg_fs_posix;
    if (strcmp(file_path, WEB_ROOT "/" INDEX_HTML) == 0) {
        opts.extra_headers = "Cache-Control: no-cache\r\n";
    }
    mg_http_serve_file(c, hm, file_path, &opts);
}
