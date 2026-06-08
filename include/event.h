#pragma once

struct ct_connection;
typedef void (*ct_event_handler_t)(struct ct_connection *, int ev,
                                   void *ev_data);
void ct_call(struct ct_connection *c, int ev, void *ev_data);
void ct_error(struct ct_connection *c, const char *fmt, ...);

enum {
  CT_EV_ERROR,      // Error                        char *error_message
  CT_EV_OPEN,       // Connection created           NULL
  CT_EV_POLL,       // mg_mgr_poll iteration        uint64_t *uptime_millis
  CT_EV_RESOLVE,    // Host name is resolved        NULL
  CT_EV_CONNECT,    // Connection established       NULL
  CT_EV_ACCEPT,     // Connection accepted          NULL
  CT_EV_TLS_HS,     // TLS handshake succeeded      NULL
  CT_EV_READ,       // Data received from socket    long *bytes_read
  CT_EV_WRITE,      // Data written to socket       long *bytes_written
  CT_EV_CLOSE,      // Connection closed            NULL
  CT_EV_HTTP_HDRS,  // HTTP headers                 struct ct_http_message *
  CT_EV_HTTP_MSG,   // Full HTTP request/response   struct ct_http_message *
  CT_EV_WS_OPEN,    // Websocket handshake done     struct ct_http_message *
  CT_EV_WS_MSG,     // Websocket msg, text or bin   struct ct_ws_message *
  CT_EV_WS_CTL,     // Websocket control msg        struct ct_ws_message *
  CT_EV_MQTT_CMD,   // MQTT low-level command       struct ct_mqtt_message *
  CT_EV_MQTT_MSG,   // MQTT PUBLISH received        struct ct_mqtt_message *
  CT_EV_MQTT_OPEN,  // MQTT CONNACK received        int *connack_status_code
  CT_EV_SNTP_TIME,  // SNTP time received           uint64_t *epoch_millis
  CT_EV_WAKEUP,     // mg_wakeup() data received    struct ct_str *data
  CT_EV_USER        // Starting ID for user events
};