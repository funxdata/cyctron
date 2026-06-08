
struct ct_dns {
    const char *url;          // DNS server URL
    struct ct_connection *c;  // DNS server connection
};

struct ct_addr {
    uint8_t ip[16];    // Holds IPv4 or IPv6 address, in network byte order
    uint16_t port;     // TCP or UDP port in network byte order
    uint8_t scope_id;  // IPv6 scope ID
    bool is_ip6;       // True when address is IPv6 address
};

