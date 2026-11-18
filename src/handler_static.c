#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mongoose.h"
#include "server.h"
#include "uri.h"
#include "log.h"
#include "global.h"

#define INDEX_HTML "index.html"

// 内置首页：安装目录不再需要 web/index.html。
// 依次加载 pages 的模板引擎、路由与 init 入口，保持与页面同源。
static const char *INDEX_HTML_CONTENT =
    "<!DOCTYPE html>\n"
    "<html lang=\"zh-cn\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width,minimum-scale=1,maximum-scale=1,initial-scale=1,user-scalable=no\">\n"
    "  <link rel=\"icon\" href=\"https://book.funxdata.com/public/funxdata.ico\">\n"
    "  <title>Piksel</title>\n"
    "</head>\n"
    "<body>\n"
    "  <div id=\"asides\" class=\"vg-asides\"></div>\n"
    "  <div id=\"app\"></div>\n"
    "  <div id=\"popup\"></div>\n"
    "  <script type=\"module\" src=\"/pages/browser.js\"></script>\n"
    "  <script type=\"module\" src=\"/pages/router.js\"></script>\n"
    "  <script type=\"module\" src=\"/init/init.js\"></script>\n"
    "</body>\n"
    "</html>\n";

static void reply_index_html(struct mg_connection *c) {
    mg_http_reply(c, 200,
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n",
        "%s", INDEX_HTML_CONTENT);
}

// 优先返回磁盘上的 web/index.html；不存在时才使用内置首页
static void serve_index_html(struct mg_connection *c, struct mg_http_message *hm) {
    struct stat st;
    if (stat(WEB_ROOT "/" INDEX_HTML, &st) == 0 && S_ISREG(st.st_mode)) {
        struct mg_http_serve_opts opts = {0};
        opts.root_dir = WEB_ROOT;
        opts.mime_types = "svg=image/svg+xml,json=application/json,"
                          "js=text/javascript,mjs=text/javascript";
        opts.fs = &mg_fs_posix;
        mg_http_serve_file(c, hm, WEB_ROOT "/" INDEX_HTML, &opts);
        return;
    }
    reply_index_html(c);
}

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

// 判断无后缀 URI 是否允许 SPA fallback（仅浏览器 HTML 导航）。
// /assets、/soket、/sse 以及带后缀的资源不参与回退。
int static_should_fallback(const char *uri, const char *accept) {
    if (!uri || uri[0] == '\0' || strcmp(uri, "/") == 0) return 0;
    if (has_file_extension(uri)) return 0;
    if (strcmp(uri, "/assets") == 0 || strncmp(uri, "/assets/", 8) == 0) return 0;
    if (strcmp(uri, "/soket") == 0) return 0;
    if (strcmp(uri, "/sse") == 0 || strncmp(uri, "/sse?", 5) == 0) return 0;
    return accept && strstr(accept, "text/html") != NULL;
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

    char accept[128] = "";
    const struct mg_str *accept_hdr = mg_http_get_header(hm, "Accept");
    if (accept_hdr && accept_hdr->len > 0) {
        size_t n = accept_hdr->len < sizeof(accept) - 1
                 ? accept_hdr->len
                 : sizeof(accept) - 1;
        memcpy(accept, accept_hdr->buf, n);
        accept[n] = '\0';
    }

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

    // 首页：优先磁盘 web/index.html，不存在时回退内置 HTML
    if (strcmp(file_path, WEB_ROOT "/" INDEX_HTML) == 0) {
        serve_index_html(c, hm);
        return;
    }
    // 无后缀页面路由：web/<uri>/index.html 存在则直接服务；否则 HTML 导航回退首页
    if (static_should_fallback(uri, accept)) {
        struct stat st;
        if (stat(file_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            log_debug("[Static] SPA fallback: %s -> index", uri);
            serve_index_html(c, hm);
            return;
        }
    }

    log_debug("[Static] %s -> %s", uri, file_path);

    struct mg_http_serve_opts opts = {0};
    opts.root_dir = WEB_ROOT;
    opts.mime_types = "svg=image/svg+xml,json=application/json,"
                      "js=text/javascript,mjs=text/javascript";
    opts.fs = &mg_fs_posix;
    mg_http_serve_file(c, hm, file_path, &opts);
}
