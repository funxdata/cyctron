// test_nng_stress.c - NNG IPC 压力测试
//
// 场景一：PAIR1 可靠通道吞吐（断言零丢失，作为 IPC 基线）
// 场景二：PUB/SUB 尽力而为吞吐（记录丢失率；PUB/SUB 语义允许丢弃，不作为失败）
// 结果同时打印并追加写入 $STRESS_REPORT_DIR/stress.jsonl。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "stress_report.h"

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

#define MSG_COUNT 5000
#define RECV_TIMEOUT_MS 500
#define SEND_TIMEOUT_MS 2000
#define DRAIN_TIMEOUT_MS 5000
#define PEER_WARMUP_MS 100

typedef struct {
    const char *url;
    int expected;
    volatile int received;
    volatile int failures;
    volatile int stop;
    volatile int ready;
} recv_ctx_t;

static void make_url(char *buf, size_t size, const char *tag) {
#ifdef _WIN32
    snprintf(buf, size, "ipc://cyctron_stress_%s_%d", tag, (int) STRESS_GETPID());
#else
    snprintf(buf, size, "ipc:///tmp/cyctron_stress_%s_%d.sock", tag, (int) STRESS_GETPID());
#endif
}

// ---------------- PAIR1：可靠通道 ----------------
static void *pair_recv_worker(void *arg) {
    recv_ctx_t *ctx = (recv_ctx_t *) arg;
    nng_socket s;

    int rv = nng_pair1_open(&s);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pair open failed: %s\n", nng_strerror(rv));
        ctx->failures++;
        return NULL;
    }

    nng_socket_set_ms(s, NNG_OPT_RECVTIMEO, RECV_TIMEOUT_MS);
    rv = nng_dial(s, ctx->url, NULL, 0);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pair dial %s failed: %s\n", ctx->url, nng_strerror(rv));
        ctx->failures++;
        nng_close(s);
        return NULL;
    }
    ctx->ready = 1;

    while (!ctx->stop && ctx->received < ctx->expected) {
        nng_msg *msg = NULL;
        rv = nng_recvmsg(s, &msg, 0);
        if (rv == 0) {
            if (msg) nng_msg_free(msg);
            ctx->received++;
        } else if (rv == NNG_ETIMEDOUT) {
            continue;
        } else {
            fprintf(stderr, "[nng-stress] pair recv failed: %s\n", nng_strerror(rv));
            ctx->failures++;
            break;
        }
    }

    nng_close(s);
    return NULL;
}

static int run_pair_test(void) {
    char url[256];
    make_url(url, sizeof(url), "pair");

    nng_socket peer;
    int rv = nng_pair1_open(&peer);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pair open failed: %s\n", nng_strerror(rv));
        return 1;
    }

    nng_socket_set_ms(peer, NNG_OPT_SENDTIMEO, SEND_TIMEOUT_MS);
    rv = nng_listen(peer, url, NULL, 0);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pair listen %s failed: %s\n", url, nng_strerror(rv));
        nng_close(peer);
        return 1;
    }

    recv_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.url = url;
    ctx.expected = MSG_COUNT;

    pthread_t th;
    if (pthread_create(&th, NULL, pair_recv_worker, &ctx) != 0) {
        fprintf(stderr, "[nng-stress] pthread_create failed\n");
        nng_close(peer);
        return 1;
    }

    while (!ctx.ready) nng_msleep(5);
    nng_msleep(PEER_WARMUP_MS);

    const char *payload = "{\"type\":\"stress\",\"data\":\""
                          "0123456789012345678901234567890123456789"
                          "0123456789012345678901234567890123456789\"}";
    const size_t payload_len = strlen(payload);

    int send_fail = 0;
    double t0 = stress_now_ms();
    for (int i = 0; i < MSG_COUNT; i++) {
        nng_msg *msg = NULL;
        if (nng_msg_alloc(&msg, 0) != 0) { send_fail++; continue; }
        if (nng_msg_append(msg, payload, payload_len) != 0 ||
            nng_sendmsg(peer, msg, 0) != 0) {
            nng_msg_free(msg);
            send_fail++;
        }
    }

    double deadline = stress_now_ms() + DRAIN_TIMEOUT_MS;
    while (ctx.received < MSG_COUNT && stress_now_ms() < deadline) {
        nng_msleep(5);
    }
    double elapsed = stress_now_ms() - t0;

    ctx.stop = 1;
    pthread_join(th, NULL);
    nng_close(peer);

    long lost = MSG_COUNT - ctx.received;
    if (lost < 0) lost = 0;
    long failures = send_fail + lost + ctx.failures;
    double ops = elapsed > 0 ? (double) ctx.received * 1000.0 / elapsed : 0.0;

    printf("  [pair1] url=%s sent=%d received=%d lost=%ld send_fail=%d\n",
           url, MSG_COUNT, ctx.received, lost, send_fail);
    printf("  [pair1] elapsed=%.1f ms (%.0f msg/s)\n", elapsed, ops);

    stress_result_t report = {
        .suite = "nng",
        .case_name = "pair1_ipc_throughput",
        .threads = 2,
        .iterations = MSG_COUNT,
        .duration_ms = elapsed,
        .failures = failures,
        .status = failures == 0 ? "PASS" : "FAIL",
        .notes = "reliable pair1, zero-loss expected"
    };
    stress_report_write(&report);

    if (failures != 0) {
        printf("  [FAIL] pair1 stress: %ld failures\n", failures);
        return 1;
    }
    printf("  [PASS] pair1 stress: %d messages, %.0f msg/s\n", ctx.received, ops);
    return 0;
}

// ---------------- PUB/SUB：尽力而为通道 ----------------
static void *sub_recv_worker(void *arg) {
    recv_ctx_t *ctx = (recv_ctx_t *) arg;
    nng_socket sub;

    int rv = nng_sub0_open(&sub);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] sub open failed: %s\n", nng_strerror(rv));
        ctx->failures++;
        return NULL;
    }

    nng_sub0_socket_subscribe(sub, "", 0);
    nng_socket_set_ms(sub, NNG_OPT_RECVTIMEO, RECV_TIMEOUT_MS);

    rv = nng_dial(sub, ctx->url, NULL, 0);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] sub dial %s failed: %s\n", ctx->url, nng_strerror(rv));
        ctx->failures++;
        nng_close(sub);
        return NULL;
    }
    ctx->ready = 1;

    while (!ctx->stop && ctx->received < ctx->expected) {
        nng_msg *msg = NULL;
        rv = nng_recvmsg(sub, &msg, 0);
        if (rv == 0) {
            if (msg) nng_msg_free(msg);
            ctx->received++;
        } else if (rv == NNG_ETIMEDOUT) {
            continue;
        } else {
            fprintf(stderr, "[nng-stress] sub recv failed: %s\n", nng_strerror(rv));
            ctx->failures++;
            break;
        }
    }

    nng_close(sub);
    return NULL;
}

static int run_pubsub_test(void) {
    char url[256];
    make_url(url, sizeof(url), "pubsub");

    nng_socket pub;
    int rv = nng_pub0_open(&pub);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pub open failed: %s\n", nng_strerror(rv));
        return 1;
    }

    nng_socket_set_ms(pub, NNG_OPT_SENDTIMEO, SEND_TIMEOUT_MS);
    rv = nng_listen(pub, url, NULL, 0);
    if (rv != 0) {
        fprintf(stderr, "[nng-stress] pub listen %s failed: %s\n", url, nng_strerror(rv));
        nng_close(pub);
        return 1;
    }

    recv_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.url = url;
    ctx.expected = MSG_COUNT;

    pthread_t th;
    if (pthread_create(&th, NULL, sub_recv_worker, &ctx) != 0) {
        fprintf(stderr, "[nng-stress] pthread_create failed\n");
        nng_close(pub);
        return 1;
    }

    while (!ctx.ready) nng_msleep(5);
    nng_msleep(300);

    const char *payload = "{\"type\":\"stress\",\"data\":\""
                          "0123456789012345678901234567890123456789"
                          "0123456789012345678901234567890123456789\"}";
    const size_t payload_len = strlen(payload);

    int send_fail = 0;
    double t0 = stress_now_ms();
    for (int i = 0; i < MSG_COUNT; i++) {
        nng_msg *msg = NULL;
        if (nng_msg_alloc(&msg, 0) != 0) { send_fail++; continue; }
        if (nng_msg_append(msg, payload, payload_len) != 0 ||
            nng_sendmsg(pub, msg, 0) != 0) {
            nng_msg_free(msg);
            send_fail++;
        }
    }

    double deadline = stress_now_ms() + DRAIN_TIMEOUT_MS;
    while (ctx.received < MSG_COUNT && stress_now_ms() < deadline) {
        nng_msleep(5);
    }
    double elapsed = stress_now_ms() - t0;

    ctx.stop = 1;
    pthread_join(th, NULL);
    nng_close(pub);

    long lost = MSG_COUNT - ctx.received;
    if (lost < 0) lost = 0;
    double loss_rate = MSG_COUNT > 0 ? (double) lost * 100.0 / MSG_COUNT : 0.0;
    double ops = elapsed > 0 ? (double) ctx.received * 1000.0 / elapsed : 0.0;

    printf("  [pubsub] url=%s sent=%d received=%d lost=%ld (%.1f%%) send_fail=%d\n",
           url, MSG_COUNT, ctx.received, lost, loss_rate, send_fail);
    printf("  [pubsub] elapsed=%.1f ms (%.0f msg/s), best-effort\n", elapsed, ops);

    char notes[256];
    snprintf(notes, sizeof(notes),
             "best-effort pub/sub; lost=%ld (%.1f%%), send_fail=%d",
             lost, loss_rate, send_fail);

    // PUB/SUB 允许丢弃，丢包只记录为观察值，不作为测试失败；
    // 只有连接/发送/接收错误才判失败。
    int hard_fail = send_fail + ctx.failures;
    stress_result_t report = {
        .suite = "nng",
        .case_name = "pubsub_ipc_best_effort",
        .threads = 2,
        .iterations = MSG_COUNT,
        .duration_ms = elapsed,
        .failures = lost,
        .status = hard_fail == 0 ? "PASS" : "FAIL",
        .notes = notes
    };
    stress_report_write(&report);

    if (hard_fail != 0) {
        printf("  [FAIL] pubsub stress: %d transport failures\n", hard_fail);
        return 1;
    }
    printf("  [PASS] pubsub stress: received %d/%d (lost %.1f%% is allowed)\n",
           ctx.received, MSG_COUNT, loss_rate);
    return 0;
}

int main(void) {
    printf("========== test_nng_stress ==========\n");

    int rc_pair = run_pair_test();
    int rc_pubsub = run_pubsub_test();

    if (rc_pair != 0 || rc_pubsub != 0) {
        printf("nng stress: FAILED (pair=%d pubsub=%d)\n", rc_pair, rc_pubsub);
        return 1;
    }
    printf("nng stress: all scenarios completed\n");
    return 0;
}
