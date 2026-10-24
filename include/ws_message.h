// include/ws_message.h
#ifndef WS_MESSAGE_H
#define WS_MESSAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS_MAX_MESSAGE_BYTES (1024 * 1024) /* 1 MiB */

/** Maximum accepted WebSocket text message size. */
int ws_message_limit(void);

/** 0 when `len` is acceptable, -1 when it exceeds the limit. */
int ws_message_check_size(size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WS_MESSAGE_H */
