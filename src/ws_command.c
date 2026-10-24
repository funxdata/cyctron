// src/ws_command.c
#include "ws_command.h"
#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WS_COMMAND_MAX_PAYLOAD (1024 * 1024)

static void set_err(char *err, size_t size, const char *msg) {
    if (err && size) snprintf(err, size, "%s", msg ? msg : "");
}

int ws_command_parse(const char *payload, size_t len, ws_command_t *out,
                     char *err, size_t err_size) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (err && err_size) err[0] = 0;

    if (!payload || len == 0 || len > WS_COMMAND_MAX_PAYLOAD) {
        set_err(err, err_size, "invalid payload size");
        return -1;
    }

    struct mg_str json = mg_str_n(payload, len);
    out->cmd = mg_json_get_str(json, "$.cmd");
    if (!out->cmd || !out->cmd[0]) {
        set_err(err, err_size, "missing cmd");
        ws_command_free(out);
        return -1;
    }

    out->uuid = mg_json_get_str(json, "$.uuid");
    out->lib = mg_json_get_str(json, "$.lib");
    out->data = mg_json_get_str(json, "$.data");
    return 0;
}

void ws_command_free(ws_command_t *cmd) {
    if (!cmd) return;
    free(cmd->cmd);
    free(cmd->uuid);
    free(cmd->lib);
    free(cmd->data);
    memset(cmd, 0, sizeof(*cmd));
}
