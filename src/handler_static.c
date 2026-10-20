#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "mongoose.h"
#include "server.h"
#include "uri.h"
#include "log.h"
#include "global.h"

void ev_handler_static(struct mg_connection *c, int ev, void *ev_data) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    char uri[512];
    int len = hm->uri.len < sizeof(uri) - 1 ? (int) hm->uri.len : sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, len);
    uri[len] = '\0';

    if (uri_has_traversal(uri)) {
        mg_http_reply(c, 400,
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: application/json\r\n",
            "{\"error\":400,\"message\":\"Bad Request\"}");
        return;
    }

    char file_path[1024];
    if (has_file_extension(uri)) {
        snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, uri);
    } else {
        snprintf(file_path, sizeof(file_path), "%s/index.html", WEB_ROOT);
    }

    struct mg_http_serve_opts opts = {0};
    opts.root_dir = ".";
    opts.mime_types = NULL;
    opts.extra_headers = NULL;
    opts.page404 = NULL;
    opts.fs = NULL;
    mg_http_serve_file(c, hm, file_path, &opts);
}
