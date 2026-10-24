// src/ws_message.c
#include "ws_message.h"

int ws_message_limit(void) {
    return WS_MAX_MESSAGE_BYTES;
}

int ws_message_check_size(size_t len) {
    return len > (size_t)WS_MAX_MESSAGE_BYTES ? -1 : 0;
}
