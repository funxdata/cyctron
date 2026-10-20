// tests/stress_report.h - 压力测试结果记录（header-only）
//
// 用法：
//   #include "stress_report.h"
//   stress_result_t r = { .suite = "ffi", .case_name = "serial", ... };
//   stress_report_write(&r);
//
// 结果追加写入 $STRESS_REPORT_DIR/stress.jsonl（默认 ./test-results），
// 每行一个 JSON 对象，便于后续用 jq / Deno 汇总。
#ifndef STRESS_REPORT_H
#define STRESS_REPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define STRESS_MKDIR(path) _mkdir(path)
#define STRESS_GETPID() _getpid()
#define STRESS_PLATFORM "windows"
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define STRESS_MKDIR(path) mkdir(path, 0755)
#define STRESS_GETPID() getpid()
#if defined(__APPLE__)
#define STRESS_PLATFORM "macos"
#else
#define STRESS_PLATFORM "linux"
#endif
#endif

typedef struct {
    const char *suite;       // 测试套件，如 "ffi" / "nng"
    const char *case_name;   // 用例名，如 "serial" / "concurrent"
    int threads;             // 并发线程数，串行为 1
    long iterations;         // 总调用/消息数
    double duration_ms;      // 耗时（毫秒）
    long failures;           // 失败数
    const char *status;      // "PASS" / "FAIL"
    const char *notes;       // 附加说明（可为 NULL）
} stress_result_t;

// 单调时钟（毫秒）。MinGW/Linux 使用 clock_gettime；
// MSVC 没有 clock_gettime，回退到 QueryPerformanceCounter。
static double stress_now_ms(void) {
#if defined(_WIN32) && !defined(__GNUC__)
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double) count.QuadPart * 1000.0 / (double) freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
#endif
}

static void stress_json_escape(const char *in, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (!in) { out[0] = '\0'; return; }

    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *) in; *p; p++) {
        const char *rep = NULL;
        char tmp[8];
        switch (*p) {
            case '"':  rep = "\\\""; break;
            case '\\': rep = "\\\\"; break;
            case '\n': rep = "\\n";  break;
            case '\r': rep = "\\r";  break;
            case '\t': rep = "\\t";  break;
            default:
                if (*p < 0x20) {
                    snprintf(tmp, sizeof(tmp), "\\u%04x", *p);
                    rep = tmp;
                }
                break;
        }
        if (rep) {
            size_t n = strlen(rep);
            if (o + n >= out_size) break;
            memcpy(out + o, rep, n);
            o += n;
        } else {
            if (o + 1 >= out_size) break;
            out[o++] = (char) *p;
        }
    }
    out[o] = '\0';
}

static void stress_timestamp(char *buf, size_t size) {
    if (!buf || size == 0) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

// 返回结果文件路径；失败返回空字符串
static void stress_report_path(char *out, size_t out_size) {
    const char *dir = getenv("STRESS_REPORT_DIR");
    if (!dir || !*dir) dir = "test-results";

    STRESS_MKDIR(dir);
    snprintf(out, out_size, "%s/stress.jsonl", dir);
}

static void stress_report_write(const stress_result_t *r) {
    if (!r || !r->suite || !r->case_name) return;

    char path[512];
    stress_report_path(path, sizeof(path));
    if (!path[0]) return;

    char ts[32];
    stress_timestamp(ts, sizeof(ts));

    char suite[128], case_name[128], status[32], notes[512];
    stress_json_escape(r->suite, suite, sizeof(suite));
    stress_json_escape(r->case_name, case_name, sizeof(case_name));
    stress_json_escape(r->status ? r->status : "UNKNOWN", status, sizeof(status));
    stress_json_escape(r->notes ? r->notes : "", notes, sizeof(notes));

    double ops = r->duration_ms > 0
        ? (double) r->iterations * 1000.0 / r->duration_ms
        : 0.0;

    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f,
            "{\"timestamp\":\"%s\",\"platform\":\"%s\",\"suite\":\"%s\","
            "\"case\":\"%s\",\"threads\":%d,\"iterations\":%ld,"
            "\"duration_ms\":%.3f,\"ops_per_sec\":%.1f,"
            "\"failures\":%ld,\"status\":\"%s\",\"notes\":\"%s\","
            "\"pid\":%d}\n",
            ts, STRESS_PLATFORM, suite, case_name, r->threads, r->iterations,
            r->duration_ms, ops, r->failures, status, notes, (int) STRESS_GETPID());
        fclose(f);
    }

    printf("[stress-report] %s/%s status=%s threads=%d iterations=%ld "
           "duration=%.1fms ops=%.0f/s failures=%ld -> %s\n",
           suite, case_name, status, r->threads, r->iterations,
           r->duration_ms, ops, r->failures,
           path[0] ? path : "(no report file)");
    fflush(stdout);
}

#endif // STRESS_REPORT_H
