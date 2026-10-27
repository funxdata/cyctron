// src/ws_worker.c
#include "ws_worker.h"
#include "ws_call.h"
#include "calldyn.h"
#include "plugin_path.h"
#include "log.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct ws_job {
    unsigned long conn_id;
    char *id;
    char *lib;
    char *symbol;
    char *args;
    long timeout_ms;
    unsigned long long submit_ms;
    struct ws_job *next;
} ws_job_t;

typedef struct ws_done {
    unsigned long conn_id;
    char *json;
    struct ws_done *next;
} ws_done_t;

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static ws_job_t *g_head = NULL;
static ws_job_t *g_tail = NULL;
static ws_done_t *g_done_head = NULL;
static ws_done_t *g_done_tail = NULL;
static int g_queue_len = 0;
static int g_running = 0;
static int g_inited = 0;
static pthread_t g_threads[WS_WORKER_THREADS];
static int g_thread_count = 0;
static ws_worker_exec_fn g_exec = call_local_dyn_libffi;
#define WS_CONN_LOCKS 16
static ws_worker_stats_t g_stats;
static int g_conn_pending[WS_CONN_LOCKS];

static pthread_mutex_t g_conn_locks[WS_CONN_LOCKS];

static void init_conn_locks(void) {
    for (int i = 0; i < WS_CONN_LOCKS; i++) {
        pthread_mutex_init(&g_conn_locks[i], NULL);
    }
}

static unsigned long long now_ms(void) {
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000u +
           (unsigned long long)(ts.tv_nsec / 1000000);
#endif
}

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void job_free(ws_job_t *job) {
    if (!job) return;
    free(job->id);
    free(job->lib);
    free(job->symbol);
    free(job->args);
    free(job);
}

static void push_done(unsigned long conn_id, char *json) {
    if (!json) return;
    ws_done_t *d = (ws_done_t *)calloc(1, sizeof(*d));
    if (!d) {
        free(json);
        return;
    }
    d->conn_id = conn_id;
    d->json = json;

    pthread_mutex_lock(&g_mutex);
    if (g_done_tail) g_done_tail->next = d;
    else g_done_head = d;
    g_done_tail = d;
    pthread_mutex_unlock(&g_mutex);
}

static void record_result(int mapped, unsigned long long elapsed, int timed_out) {
    pthread_mutex_lock(&g_mutex);
    if (mapped == WS_CALL_OK) {
        g_stats.ok++;
    } else {
        g_stats.failed++;
        if (timed_out) g_stats.timeouts++;
    }
    g_stats.total_us += elapsed;
    if (elapsed > g_stats.max_us) g_stats.max_us = elapsed;
    pthread_mutex_unlock(&g_mutex);
}

static void *worker_loop(void *arg) {
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&g_mutex);
        while (!g_head && g_running) {
            pthread_cond_wait(&g_cond, &g_mutex);
        }
        if (!g_head && !g_running) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }

        ws_job_t *job = g_head;
        g_head = job->next;
        if (!g_head) g_tail = NULL;
        g_queue_len--;
        pthread_mutex_unlock(&g_mutex);

        unsigned long long start = now_ms();
        long waited = (long)(start - job->submit_ms);
        int timed_out = 0;
        int rc = -1;
        char *out = NULL;

        if (job->timeout_ms > 0 && waited > job->timeout_ms) {
            timed_out = 1;
            rc = -7;
        } else {
            char path[512];
            /* 未找到时 plugin_resolve_path 仍写入 ./libary/<lib><ext> 候选路径，
             * 交给执行器 dlopen，保持“库不存在 → rc=-1”的旧语义。 */
            plugin_resolve_path(job->lib, path, sizeof(path));
            pthread_mutex_t *lock = &g_conn_locks[job->conn_id % WS_CONN_LOCKS];
            pthread_mutex_lock(lock);
            rc = g_exec ? g_exec(path, job->symbol, job->args, &out) : -1;
            pthread_mutex_unlock(lock);
        }

        unsigned long long elapsed = now_ms() - start;
        int mapped = timed_out ? -7 : ws_call_map_rc(rc);

        ws_call_resp_t resp;
        memset(&resp, 0, sizeof(resp));
        if (mapped == WS_CALL_OK) {
            ws_call_build_ok(job->id, job->lib, job->symbol, elapsed, out, &resp);
        } else {
            char msg[128];
            if (timed_out) {
                snprintf(msg, sizeof(msg), "call timed out in queue");
            } else {
                snprintf(msg, sizeof(msg), "ffi rc=%d", rc);
            }
            ws_call_build_error(job->id, mapped, msg, &resp);
        }

        if (resp.json) {
            log_debug("[WS] call done id=%s code=%d elapsed=%llums",
                      job->id, mapped, elapsed);
            push_done(job->conn_id, resp.json);
            resp.json = NULL;
            record_result(mapped, elapsed, timed_out);
        }

        pthread_mutex_lock(&g_mutex);
        int slot = (int)(job->conn_id % WS_CONN_LOCKS);
        if (g_conn_pending[slot] > 0) g_conn_pending[slot]--;
        pthread_mutex_unlock(&g_mutex);
        ws_call_resp_free(&resp);

        if (elapsed > 5000) {
            log_warn("[WS] slow call %s.%s %llums",
                     job->lib, job->symbol, elapsed);
        }

        free(out);
        job_free(job);
    }
    return NULL;
}

int ws_worker_init(void) {
    pthread_once(&g_once, init_conn_locks);

    pthread_mutex_lock(&g_mutex);
    if (g_inited) {
        pthread_mutex_unlock(&g_mutex);
        return 0;
    }
    g_running = 1;
    g_inited = 1;
    memset(&g_stats, 0, sizeof(g_stats));

    for (int i = 0; i < WS_WORKER_THREADS; i++) {
        if (pthread_create(&g_threads[i], NULL, worker_loop, NULL) != 0) {
            log_error("[WS] failed to create worker %d", i);
            break;
        }
        g_thread_count++;
    }
    if (g_thread_count == 0) {
        g_running = 0;
        g_inited = 0;
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    pthread_mutex_unlock(&g_mutex);
    log_info("[WS] worker started (%d thread(s))", g_thread_count);
    return 0;
}

void ws_worker_shutdown(void) {
    pthread_mutex_lock(&g_mutex);
    if (!g_inited) {
        pthread_mutex_unlock(&g_mutex);
        return;
    }
    g_running = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);

    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(g_threads[i], NULL);
    }
    g_thread_count = 0;

    pthread_mutex_lock(&g_mutex);
    ws_job_t *job = g_head;
    while (job) {
        ws_job_t *next = job->next;
        job_free(job);
        job = next;
    }
    g_head = g_tail = NULL;
    g_queue_len = 0;
    memset(g_conn_pending, 0, sizeof(g_conn_pending));

    ws_done_t *d = g_done_head;
    while (d) {
        ws_done_t *next = d->next;
        free(d->json);
        free(d);
        d = next;
    }
    g_done_head = g_done_tail = NULL;
    g_inited = 0;
    pthread_mutex_unlock(&g_mutex);
    log_info("[WS] worker stopped");
}

void ws_worker_set_exec(ws_worker_exec_fn fn) {
    g_exec = fn;
}

int ws_worker_submit(unsigned long conn_id, const char *id, const char *lib,
                     const char *symbol, const char *args, long timeout_ms) {
    if (!id || !lib || !symbol) return -1;
    if (ws_worker_init() != 0) return -1;

    ws_job_t *job = (ws_job_t *)calloc(1, sizeof(*job));
    if (!job) return -1;

    job->conn_id = conn_id;
    job->id = dup_str(id);
    job->lib = dup_str(lib);
    job->symbol = dup_str(symbol);
    job->args = dup_str(args ? args : "{}");
    job->timeout_ms = timeout_ms;
    job->submit_ms = now_ms();

    if (!job->id || !job->lib || !job->symbol || !job->args) {
        job_free(job);
        return -1;
    }

    pthread_mutex_lock(&g_mutex);
    int slot = (int)(conn_id % WS_CONN_LOCKS);
    if (g_queue_len >= WS_WORKER_MAX_QUEUE ||
        g_conn_pending[slot] >= WS_WORKER_MAX_PER_CONN) {
        g_stats.busy++;
        pthread_mutex_unlock(&g_mutex);
        job_free(job);
        return -1;
    }

    if (g_tail) g_tail->next = job;
    else g_head = job;
    g_tail = job;
    g_queue_len++;
    g_conn_pending[slot]++;
    g_stats.total++;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int ws_worker_drain(ws_worker_done_fn cb, void *user) {
    if (!cb) return 0;

    pthread_mutex_lock(&g_mutex);
    ws_done_t *d = g_done_head;
    g_done_head = g_done_tail = NULL;
    pthread_mutex_unlock(&g_mutex);

    int n = 0;
    while (d) {
        ws_done_t *next = d->next;
        cb(d->conn_id, d->json, user);
        free(d->json);
        free(d);
        n++;
        d = next;
    }
    return n;
}

void ws_worker_stats(ws_worker_stats_t *out) {
    if (!out) return;
    pthread_mutex_lock(&g_mutex);
    *out = g_stats;
    pthread_mutex_unlock(&g_mutex);
}
