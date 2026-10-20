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

    log_debug("[Static] %s -> %s", uri, file_path);

    struct mg_http_serve_opts opts = {0};
    opts.root_dir = WEB_ROOT;
    opts.mime_types = "svg=image/svg+xml,json=application/json,"
                      "js=text/javascript,mjs=text/javascript";
    opts.fs = &mg_fs_posix;
    mg_http_serve_file(c, hm, file_path, &opts);
}
