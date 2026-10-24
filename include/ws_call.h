// include/ws_call.h
// Pure helpers for the WebSocket "call" command: parse, map rc, build JSON.
#ifndef WS_CALL_H
#define WS_CALL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call-layer error codes (kept stable for clients). */
enum {
    WS_CALL_OK = 0,
    WS_CALL_ERR_PARAM = -1,
    WS_CALL_ERR_LIB = -2,
    WS_CALL_ERR_SYMBOL = -3,
    WS_CALL_ERR_INVOKE = -4,
    WS_CALL_ERR_DENIED = -5
};

#define WS_CALL_MAX_NAME 128
#define WS_CALL_MAX_PAYLOAD (1024 * 1024)

typedef struct {
    char *id;
    char *lib;
    char *symbol;
    char *args;       /* raw JSON text, defaults to "{}" */
    long timeout_ms;
} ws_call_req_t;

typedef struct {
    char *json;       /* full {"type":"call",...} payload; caller frees */
    int code;         /* 0 on success, negative otherwise */
} ws_call_resp_t;

/**
 * Parse a call request.
 * Returns 0 on success, WS_CALL_ERR_* / -1 on failure.
 * On failure `out` may still contain `id` (when it was parsed) so callers can
 * build an error response; always call ws_call_req_free() afterwards.
 */
int ws_call_parse(const char *payload, size_t len, ws_call_req_t *out,
                  char *err, size_t err_size);

/** Build a successful response. result_json NULL/empty is encoded as null. */
int ws_call_build_ok(const char *id, const char *lib, const char *symbol,
                     unsigned long long elapsed_ms, const char *result_json,
                     ws_call_resp_t *out);

/** Build an error response. */
int ws_call_build_error(const char *id, int code, const char *message,
                        ws_call_resp_t *out);

/** Map a call_local_dyn_libffi() return code to a call-layer code. */
int ws_call_map_rc(int ffi_rc);

void ws_call_req_free(ws_call_req_t *req);
void ws_call_resp_free(ws_call_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* WS_CALL_H */
