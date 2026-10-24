// include/ws_command.h
// Parse legacy WebSocket commands (bind / remove / status / msg) with JSON.
#ifndef WS_COMMAND_H
#define WS_COMMAND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *cmd;    /* command name, required */
    char *uuid;   /* optional */
    char *lib;    /* optional */
    char *data;   /* msg payload as unescaped string, optional */
} ws_command_t;

/**
 * Parse a legacy command payload.
 * Returns 0 on success, -1 on invalid JSON / missing cmd / oversized payload.
 */
int ws_command_parse(const char *payload, size_t len, ws_command_t *out,
                     char *err, size_t err_size);

void ws_command_free(ws_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* WS_COMMAND_H */
