// test_libary_stress.c - libary_demo FFI 压力测试
// 串行 + 多线程并发调用动态库，验证零失败并输出吞吐
#include "../include/calldyn.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define PLUGIN "./libary/libary_demo.dll"
#else
#define PLUGIN "./libary/libary_demo.so"
#endif
#define SERIAL_N 20000
#define THREADS 8
#define PER_THREAD 5000

static int call_once(const char *symbol, const char *json_in) {
    char *out = NULL;
    int rc = call_local_dyn_libffi(PLUGIN, symbol, json_in, &out);
    free(out);
    return rc;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

typedef struct {
    int failures;
} worker_result_t;

static void *worker(void *arg) {
    worker_result_t *res = (worker_result_t *) arg;
    res->failures = 0;

    char body[64];
    for (int i = 0; i < PER_THREAD; i++) {
        // 混合读/写负载
        switch (i % 4) {
            case 0:
                if (call_once("get_handle", NULL) != 0) res->failures++;
                break;
            case 1:
                if (call_once("get_handle", "{\"id\":2}") != 0) res->failures++;
                break;
            case 2:
                snprintf(body, sizeof(body), "{\"name\":\"s%d\",\"type\":\"t\"}", i);
                if (call_once("post_handle", body) != 0) res->failures++;
                break;
            case 3:
                if (call_once("patch_handle",
                        "{\"id\":3,\"patch\":{\"name\":\"p\"}}") != 0) res->failures++;
                break;
        }
    }
    return NULL;
}

int main(void) {
    printf("========== test_libary_stress ==========\n");
    int rc;
    int hard_fail = 0;

    // ---------- 串行 ----------
    {
        double t0 = now_ms();
        for (int i = 0; i < SERIAL_N; i++) {
            rc = call_once("get_handle", NULL);
            if (rc != 0) { hard_fail++; break; }
        }
        double dt = now_ms() - t0;
        if (hard_fail) {
            printf("  [FAIL] serial: rc=%d at iter %d\n", rc, SERIAL_N);
            return 1;
        }
        printf("  [PASS] serial %d calls in %.1f ms (%.0f ops/s)\n",
               SERIAL_N, dt, SERIAL_N / (dt / 1000.0));
    }

    // ---------- 并发 ----------
    {
        pthread_t th[THREADS];
        worker_result_t results[THREADS];

        double t0 = now_ms();
        for (int i = 0; i < THREADS; i++) {
            pthread_create(&th[i], NULL, worker, &results[i]);
        }
        for (int i = 0; i < THREADS; i++) {
            pthread_join(th[i], NULL);
        }
        double dt = now_ms() - t0;

        int total_fail = 0;
        for (int i = 0; i < THREADS; i++) total_fail += results[i].failures;

        int total_calls = THREADS * PER_THREAD;
        if (total_fail != 0) {
            printf("  [FAIL] concurrent: %d/%d failures\n", total_fail, total_calls);
            return 1;
        }
        printf("  [PASS] concurrent %d threads x %d calls in %.1f ms (%.0f ops/s)\n",
               THREADS, PER_THREAD, dt, total_calls / (dt / 1000.0));
    }

    printf("========================================\n");
    printf("stress: all passed\n");
    return 0;
}
