#pragma once

# include "arch.h"  // For size_t

struct ct_queue{
    char *buf;
    size_t size;

}

struct ct_mgr {
    struct mg_connection *conns;  // List of active connections
    struct mg_dns dns4;           // DNS for IPv4
    struct mg_dns dns6;           // DNS for IPv6
    int dnstimeout;               // DNS resolve timeout in milliseconds
    bool use_dns6;                // Use DNS6 server by default, see #1532
    unsigned long nextid;         // Next connection ID
    void *userdata;               // Arbitrary user data pointer
    void *tls_ctx;                // TLS context shared by all TLS sessions
    uint16_t mqtt_id;             // MQTT IDs for pub/sub
    void *active_dns_requests;    // DNS requests in progress
    struct mg_timer *timers;      // Active timers
    int epoll_fd;                 // Used when MG_EPOLL_ENABLE=1
    struct mg_tcpip_if *ifp;      // Builtin TCP/IP stack only. Interface pointer
    size_t extraconnsize;         // Builtin TCP/IP stack only. Extra space
    MG_SOCKET_TYPE pipe;          // Socketpair end for mg_wakeup()
  #if MG_ENABLE_FREERTOS_TCP
    SocketSet_t ss;  // NOTE(lsm): referenced from socket struct
  #endif
};

struct ct_connection {
    struct ct_connection *next;     // Linkage in struct mg_mgr :: connections
    struct ct_mgr *mgr;             // Our container
    struct ct_addr loc;             // Local address
    struct ct_addr rem;             // Remote address
    void *fd;                       // Connected socket, or LWIP data
    unsigned long id;               // Auto-incrementing unique connection ID
    struct ct_iobuf recv;           // Incoming data
    struct ct_iobuf send;           // Outgoing data
    struct ct_iobuf prof;           // Profile data enabled by MG_ENABLE_PROFILE
    struct ct_iobuf rtls;           // TLS only. Incoming encrypted data
    ct_event_handler_t fn;          // User-specified event handler function
    void *fn_data;                  // User-specified function parameter
    ct_event_handler_t pfn;         // Protocol-specific handler function
    void *pfn_data;                 // Protocol-specific function parameter
    char data[MG_DATA_SIZE];        // Arbitrary connection data
    void *tls;                      // TLS specific data
    unsigned is_listening : 1;      // Listening connection
    unsigned is_client : 1;         // Outbound (client) connection
    unsigned is_accepted : 1;       // Accepted (server) connection
    unsigned is_resolving : 1;      // Non-blocking DNS resolution is in progress
    unsigned is_arplooking : 1;     // Non-blocking ARP resolution is in progress
    unsigned is_connecting : 1;     // Non-blocking connect is in progress
    unsigned is_tls : 1;            // TLS-enabled connection
    unsigned is_tls_hs : 1;         // TLS handshake is in progress
    unsigned is_udp : 1;            // UDP connection
    unsigned is_websocket : 1;      // WebSocket connection
    unsigned is_mqtt5 : 1;          // For MQTT connection, v5 indicator
    unsigned is_hexdumping : 1;     // Hexdump in/out traffic
    unsigned is_draining : 1;       // Send remaining data, then close and free
    unsigned is_closing : 1;        // Close and free the connection immediately
    unsigned is_full : 1;           // Stop reads, until cleared
    unsigned is_tls_throttled : 1;  // Last TLS write: MG_SOCK_PENDING() was true
    unsigned is_resp : 1;           // Response is still being generated
    unsigned is_readable : 1;       // Connection is ready to read
    unsigned is_writable : 1;       // Connection is ready to write
  };