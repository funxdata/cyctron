#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "mongoose.h"
#include "server.h"
#include "uri.h"
#include "log.h"

#define WEB_ROOT "./web"

void ev_handler_static(struct mg_connection *c, int ev, void *ev_data) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    char uri[512];
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, uri);
    int len = hm->uri.len < sizeof(uri) - 1? (int) hm->uri.len: sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, len);
    uri[len] = '\0';
    printf("URI = [%s]\n", uri);
    // 判断是否有文件后缀
    if (has_file_extension(uri)) {
        snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, uri);
    } else {
        snprintf(file_path, sizeof(file_path), "%s/index.html", WEB_ROOT);
    }
    printf("file_path = %s\n", file_path);
    struct mg_http_serve_opts opts = {0};
    opts.root_dir = ".";           // 可选
    opts.mime_types = NULL;        // 使用内置
    opts.extra_headers = NULL;
    opts.page404 = NULL;
    opts.fs = NULL;                // 默认 posix
    mg_http_serve_file(c, hm, file_path, &opts);
    return;
}

