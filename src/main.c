#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "mongoose.h"
#include "server.h"
#include "event_pusher.h"

static void print_help(const char *prog) {
    printf("Usage:\n");
    printf("  %s start [-port <port>]   Start HTTP + WS server\n", prog);
    printf("  %s stop Stop the server\n", prog);
    printf("  %s reload Reload the server\n", prog);
    printf("  %s help Show this help\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-help") == 0) {
        print_help(argv[0]);
        return 0;
    } 
    else if (strcmp(argv[1], "start") == 0) {
        const char *host = "0.0.0.0";
        const char *port = "44944";

        // 解析可选参数
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
                port = argv[i + 1];
                i++;
            }
        }

        char addr[128];
        snprintf(addr, sizeof(addr), "http://%s:%s", host, port);

        // 初始化事件推送
        if (!event_pusher_init()) {
            log_info("Failed to initialize event pusher\n");
            return 1;
        }

        struct mg_mgr mgr;
        mg_mgr_init(&mgr);

        if (mg_http_listen(&mgr, addr, ev_handler, NULL) == NULL) {
            log_info("Error starting HTTP+WS server on %s\n", addr);
            return 1;
        }

        log_info("HTTP + WS server started on %s\n", addr);

        while (1) {
            mg_mgr_poll(&mgr, 50);
        }
    } 
    else if (strcmp(argv[1], "stop") == 0) {
        log_info("Stopping server... (TODO: implement IPC or PID file)\n");
    } 
    else if (strcmp(argv[1], "reload") == 0) {
        log_info("Reloading server... (TODO: implement reload logic)\n");
    } 
    else {
        log_info("Unknown command: %s\n", argv[1]);
        log_info(argv[0]);
        return 1;
    }

    return 0;
}

