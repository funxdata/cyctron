// include/ws_worker.h
// Background worker for WebSocket "call" requests.
#ifndef WS_WORKER_H
#define WS_WORKER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS_WORKER_MAX_QUEUE 64
#define WS_WORKER_MAX_PER_CONN 16
#define WS_WORKER_THREADS 2

typedef struct {
    unsigned long long total;
    unsigned long long ok;
    unsigned long long failed;
    unsigned long long timeouts;
    unsigned long long busy;
    unsigned long long total_us;
    unsigned long long max_us;
} ws_worker_stats_t;

/* Executes one FFI call. `lib_path` is already resolved. */
typedef int (*ws_worker_exec_fn)(const char *lib_path, const char *symbol,
                                 const char *args, char **out);

/* Called from the main loop for each completed call. */
typedef void (*ws_worker_done_fn)(unsigned long conn_id, const char *json,
                                  void *user);

int ws_worker_init(void);
void ws_worker_shutdown(void);

/* Inject an executor (tests). Call before submit / init. */
void ws_worker_set_exec(ws_worker_exec_fn fn);

/**
 * Queue a call. Returns 0 on success, -1 when the queue is full / invalid.
 * The response is built asynchronously and fetched via ws_worker_drain().
 */
int ws_worker_submit(unsigned long conn_id, const char *id, const char *lib,
                     const char *symbol, const char *args, long timeout_ms);

/* Drain completed responses (main loop only). Returns number delivered. */
int ws_worker_drain(ws_worker_done_fn cb, void *user);

void ws_worker_stats(ws_worker_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WS_WORKER_H */
