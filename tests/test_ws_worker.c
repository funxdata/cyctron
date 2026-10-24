// tests/test_ws_worker.c - worker queue, ordering, timeout, stats
#include "../include/ws_worker.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

static int passed = 0, failed = 0;
static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

static pthread_mutex_t t_mutex = PTHREAD_MUTEX_INITIALIZER;
static int t_inflight = 0;
static int t_max_inflight = 0;
static int t_delay_ms = 40;

static int exec_sleep(const char *lib_path, const char *symbol,
                      const char *args, char **out) {
    (void)lib_path;
    (void)symbol;
    (void)args;

    pthread_mutex_lock(&t_mutex);
    t_inflight++;
    if (t_inflight > t_max_inflight) t_max_inflight = t_inflight;
    pthread_mutex_unlock(&t_mutex);

    SLEEP_MS(t_delay_ms);

    pthread_mutex_lock(&t_mutex);
    t_inflight--;
    pthread_mutex_unlock(&t_mutex);

    if (out) {
        const char *json = "{\"code\":200}";
        size_t n = strlen(json);
        char *p = (char *)malloc(n + 1);
        if (p) memcpy(p, json, n + 1);
        *out = p;
    }
    return 0;
}

/* Stub required by ws_worker.c default executor. */
int call_local_dyn_libffi(const char *path, const char *fn,
                          const char *in, char **out) {
    (void)path; (void)fn; (void)in;
    if (out) *out = NULL;
    return -1;
}

#define MAX_RESULTS 128
static char *g_results[MAX_RESULTS];
static int g_result_count = 0;

static void on_done(unsigned long conn_id, const char *json, void *user) {
    (void)conn_id;
    (void)user;
    if (g_result_count >= MAX_RESULTS) return;
    size_t n = strlen(json);
    char *p = (char *)malloc(n + 1);
    if (!p) return;
    memcpy(p, json, n + 1);
    g_results[g_result_count++] = p;
}

static void clear_results(void) {
    for (int i = 0; i < g_result_count; i++) free(g_results[i]);
    g_result_count = 0;
}

static void wait_for_results(int expected, int timeout_ms) {
    int waited = 0;
    while (g_result_count < expected && waited < timeout_ms) {
        ws_worker_drain(on_done, NULL);
        SLEEP_MS(10);
        waited += 10;
    }
    ws_worker_drain(on_done, NULL);
}

static int submit(unsigned long conn_id, const char *id, long timeout_ms) {
    return ws_worker_submit(conn_id, id, "chatllm", "post_chat", "{}", timeout_ms);
}

static void test_serial_same_conn(void) {
    printf("== same connection serial ==\n");
    t_max_inflight = 0;
    t_delay_ms = 40;

    check(submit(100, "a1", 2000) == 0, "submit a1");
    check(submit(100, "a2", 2000) == 0, "submit a2");
    wait_for_results(2, 3000);

    check(g_result_count == 2, "two results");
    check(t_max_inflight == 1, "max inflight == 1 (serial)");
    check(g_result_count == 2 && strstr(g_results[0], "\"ok\":true") != NULL,
          "response ok");
    clear_results();
}

static void test_parallel_different_conns(void) {
    printf("== different connections parallel ==\n");
    t_max_inflight = 0;
    t_delay_ms = 80;

    check(submit(201, "b1", 3000) == 0, "submit b1");
    check(submit(202, "b2", 3000) == 0, "submit b2");
    wait_for_results(2, 3000);

    check(g_result_count == 2, "two results");
    check(t_max_inflight >= 2, "max inflight >= 2 (parallel)");
    clear_results();
}

static void test_per_conn_limit(void) {
    printf("== per-connection limit ==\n");
    t_delay_ms = 100;

    int rejected = 0;
    for (int i = 0; i < WS_WORKER_MAX_PER_CONN + 1; i++) {
        char id[32];
        snprintf(id, sizeof(id), "c%d", i);
        if (submit(300, id, 5000) != 0) rejected++;
    }
    check(rejected == 1, "17th submission rejected");

    wait_for_results(WS_WORKER_MAX_PER_CONN, 8000);
    check(g_result_count == WS_WORKER_MAX_PER_CONN, "16 results");
    clear_results();
}

static void test_queue_timeout(void) {
    printf("== queue timeout ==\n");
    t_delay_ms = 200;

    check(submit(500, "d1", 2000) == 0, "submit slow d1");
    check(submit(501, "d2", 2000) == 0, "submit slow d2");
    SLEEP_MS(30);
    check(submit(502, "d3", 1) == 0, "submit timeout d3");

    wait_for_results(3, 5000);
    check(g_result_count == 3, "three results");

    int timeout_seen = 0;
    for (int i = 0; i < g_result_count; i++) {
        if (strstr(g_results[i], "\"code\":-7") != NULL) timeout_seen = 1;
    }
    check(timeout_seen, "timed out call returns -7");
    clear_results();
}

static void test_stats(void) {
    printf("== stats ==\n");
    ws_worker_stats_t stats;
    ws_worker_stats(&stats);

    check(stats.total >= 23, "total counted");
    check(stats.ok >= 4, "ok counted");
    check(stats.timeouts >= 1, "timeouts counted");
    check(stats.busy >= 1, "busy counted");
    check(stats.max_us >= stats.total_us / (stats.total ? stats.total : 1),
          "max_us sane");
}

int main(void) {
    printf("=== test_ws_worker ===\n");

    ws_worker_set_exec(exec_sleep);
    check(ws_worker_init() == 0, "init");

    test_serial_same_conn();
    test_parallel_different_conns();
    test_per_conn_limit();
    test_queue_timeout();
    test_stats();

    ws_worker_shutdown();
    clear_results();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
