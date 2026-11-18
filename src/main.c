#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "log.h"
#include "mongoose.h"
#include "server.h"
#include "event_pusher.h"
#include "handler_ws.h"
#include "ws_worker.h"
#include "realtime.h"

#define PID_FILE "cyctron.pid"

static volatile sig_atomic_t g_running = 1;

static void on_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_help(const char *prog) {
    printf("Usage:\n");
    printf("  %s start [-port <port>]   Start HTTP + WS server\n", prog);
    printf("  %s stop                   Stop the server (uses %s)\n", prog, PID_FILE);
    printf("  %s reload                 Reload the server (not supported)\n", prog);
    printf("  %s help                   Show this help\n", prog);
}

static int write_pid_file(void) {
    FILE *fp = fopen(PID_FILE, "w");
    if (!fp) return -1;
#ifdef _WIN32
    fprintf(fp, "%ld\n", (long)_getpid());
#else
    fprintf(fp, "%ld\n", (long)getpid());
#endif
    fclose(fp);
    return 0;
}

static int stop_server(void) {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) {
        log_error("cyctron is not running (missing %s)", PID_FILE);
        return 1;
    }

    long pid = 0;
    int valid = fscanf(fp, "%ld", &pid) == 1 && pid > 0;
    fclose(fp);
    if (!valid) {
        log_error("invalid pid file: %s", PID_FILE);
        remove(PID_FILE);
        return 1;
    }

#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) {
        log_error("failed to open process %ld (error %lu)",
                  pid, (unsigned long)GetLastError());
        return 1;
    }
    BOOL ok = TerminateProcess(h, 0);
    CloseHandle(h);
    if (!ok) {
        log_error("failed to stop process %ld", pid);
        return 1;
    }
#else
    if (kill((pid_t)pid, SIGTERM) != 0) {
        log_error("failed to stop process %ld: %s", pid, strerror(errno));
        return 1;
    }
#endif

    remove(PID_FILE);
    log_info("Stopped cyctron (pid=%ld)", pid);
    return 0;
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

    if (strcmp(argv[1], "start") == 0) {
        const char *host = "0.0.0.0";
        const char *port = "44944";

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-port") == 0) {
                if (i + 1 >= argc) {
                    log_error("-port requires a value");
                    return 1;
                }
                errno = 0;
                char *end = NULL;
                long value = strtol(argv[i + 1], &end, 10);
                if (errno == ERANGE || !end || *end != '\0' ||
                    value < 1 || value > 65535) {
                    log_error("invalid port: %s (expected 1-65535)", argv[i + 1]);
                    return 1;
                }
                port = argv[i + 1];
                i++;
            }
        }

        char addr[128];
        snprintf(addr, sizeof(addr), "http://%s:%s", host, port);

        signal(SIGINT, on_signal);
#ifdef SIGTERM
        signal(SIGTERM, on_signal);
#endif

        if (!event_pusher_init()) {
            log_error("failed to initialize event pusher");
            return 1;
        }
        if (ws_worker_init() != 0) {
            log_error("failed to initialize WebSocket worker");
            event_pusher_cleanup();
            return 1;
        }

        struct mg_mgr mgr;
        mg_mgr_init(&mgr);

        if (mg_http_listen(&mgr, addr, ev_handler, NULL) == NULL) {
            log_error("failed to start HTTP+WS server on %s", addr);
            ws_worker_shutdown();
            event_pusher_cleanup();
            mg_mgr_free(&mgr);
            return 1;
        }

        if (write_pid_file() != 0) {
            log_warn("failed to write %s; `stop` command will not work", PID_FILE);
        }

        log_info("HTTP + WS server started on %s", addr);

        while (g_running) {
            mg_mgr_poll(&mgr, 50);
            ws_drain_call_results();
        }

        log_info("Shutting down...");
        ws_worker_shutdown();
        rt_cleanup(NULL);
        event_pusher_cleanup();
        mg_mgr_free(&mgr);
        remove(PID_FILE);
        return 0;
    }

    if (strcmp(argv[1], "stop") == 0) {
        return stop_server();
    }

    if (strcmp(argv[1], "reload") == 0) {
        log_error("reload is not supported (no config file); restart the server instead");
        return 1;
    }

    log_error("unknown command: %s", argv[1]);
    print_help(argv[0]);
    return 1;
}
