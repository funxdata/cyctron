#pragma once

#ifndef CT_ENABLE_LOG
#define CT_ENABLE_LOG 1
#endif

#ifndef CT_ENABLE_CUSTOM_CALLOC
#define CT_ENABLE_CUSTOM_CALLOC 0
#endif

#ifndef CT_ENABLE_CUSTOM_LOG
#define CT_ENABLE_CUSTOM_LOG 0  // Let user define their own CT_LOG
#endif

#ifndef CT_ENABLE_TCPIP
#define CT_ENABLE_TCPIP 0  // Built-in network stack
#endif

#ifndef CT_ENABLE_LWIP
#define CT_ENABLE_LWIP 0  // LWIP network stack
#endif

#ifndef CT_ENABLE_FREERTOS_TCP
#define CT_ENABLE_FREERTOS_TCP 0  // Amazon FreeRTOS-TCP network stack
#endif

#ifndef CT_ENABLE_RL
#define CT_ENABLE_RL 0  // ARM MDK network stack
#endif

#ifndef CT_ENABLE_SOCKET
#define CT_ENABLE_SOCKET !CT_ENABLE_TCPIP
#endif

#ifndef CT_ENABLE_POLL
#define CT_ENABLE_POLL 0
#endif

#ifndef CT_ENABLE_EPOLL
#define CT_ENABLE_EPOLL 0
#endif

#ifndef CT_ENABLE_FATFS
#define CT_ENABLE_FATFS 0
#endif

#ifndef CT_ENABLE_SSI
#define CT_ENABLE_SSI 0
#endif

#ifndef CT_ENABLE_IPV6
#define CT_ENABLE_IPV6 0
#endif

#ifndef CT_IPV6_V6ONLY
#define CT_IPV6_V6ONLY 0  // IPv6 socket binds only to V6, not V4
#endif

#ifndef CT_ENABLE_MD5
#define CT_ENABLE_MD5 1
#endif

#ifndef CT_ENABLE_WINSOCK
#define CT_ENABLE_WINSOCK 1
#endif

#ifndef CT_ENABLE_DIRLIST
#define CT_ENABLE_DIRLIST 0
#endif

#ifndef CT_ENABLE_CUSTOM_RANDOM
#define CT_ENABLE_CUSTOM_RANDOM 0
#endif

#ifndef CT_ENABLE_CUSTOM_MILLIS
#define CT_ENABLE_CUSTOM_MILLIS 0
#endif

#ifndef CT_ENABLE_PACKED_FS
#define CT_ENABLE_PACKED_FS 0
#endif

#ifndef CT_ENABLE_ASSERT
#define CT_ENABLE_ASSERT 0
#endif

#ifndef CT_IO_SIZE
#define CT_IO_SIZE 512
#endif

#ifndef CT_MAX_RECV_SIZE
#define CT_MAX_RECV_SIZE (3UL * 1024UL * 1024UL)
#endif

#ifndef CT_DATA_SIZE
#define CT_DATA_SIZE 32
#endif

#ifndef CT_MAX_HTTP_HEADERS
#define CT_MAX_HTTP_HEADERS 30
#endif

#ifndef CT_HTTP_INDEX
#define CT_HTTP_INDEX "index.html"
#endif

#ifndef CT_PATH_MAX
#ifdef PATH_MAX
#define CT_PATH_MAX PATH_MAX
#else
#define CT_PATH_MAX 128
#endif
#endif

#ifndef CT_SOCK_LISTEN_BACKLOG_SIZE
#define CT_SOCK_LISTEN_BACKLOG_SIZE 128
#endif

#ifndef CT_DIRSEP
#define CT_DIRSEP '/'
#endif

#ifndef CT_ENABLE_POSIX_FS
#define CT_ENABLE_POSIX_FS 0
#endif

#ifndef CT_INVALID_SOCKET
#define CT_INVALID_SOCKET (-1)
#endif

#ifndef CT_SOCKET_TYPE
#define CT_SOCKET_TYPE int
#endif

#ifndef CT_SOCKET_ERRNO
#define CT_SOCKET_ERRNO errno
#endif

#if CT_ENABLE_EPOLL
#define CT_EPOLL_ADD(c)                                                    \
  do {                                                                     \
    struct epoll_event ev = {EPOLLIN | EPOLLERR | EPOLLHUP, {c}};          \
    epoll_ctl(c->mgr->epoll_fd, EPOLL_CTL_ADD, (int) (size_t) c->fd, &ev); \
  } while (0)
#define CT_EPOLL_MOD(c, wr)                                                \
  do {                                                                     \
    struct epoll_event ev = {EPOLLIN | EPOLLERR | EPOLLHUP, {c}};          \
    if (wr) ev.events |= EPOLLOUT;                                         \
    epoll_ctl(c->mgr->epoll_fd, EPOLL_CTL_MOD, (int) (size_t) c->fd, &ev); \
  } while (0)
#else
#define CT_EPOLL_ADD(c)
#define CT_EPOLL_MOD(c, wr)
#endif

#ifndef CT_ENABLE_PROFILE
#define CT_ENABLE_PROFILE 0
#endif

#ifndef CT_ENABLE_TCPIP_DRIVER_INIT
#define CT_ENABLE_TCPIP_DRIVER_INIT 1
#endif

#ifndef CT_TCPIP_IP
#define CT_TCPIP_IP CT_IPV4(0, 0, 0, 0)
#endif

#ifndef CT_TCPIP_MASK
#define CT_TCPIP_MASK CT_IPV4(0, 0, 0, 0)
#endif

#ifndef CT_TCPIP_GW
#define CT_TCPIP_GW CT_IPV4(0, 0, 0, 0)
#endif

#ifndef CT_SET_MAC_ADDRESS
#define CT_SET_MAC_ADDRESS(mac)
#endif

#ifndef CT_SET_WIFI_CONFIG
#define CT_SET_WIFI_CONFIG(data)
#endif

#ifndef CT_ENABLE_TCPIP_PRINT_DEBUG_STATS
#define CT_ENABLE_TCPIP_PRINT_DEBUG_STATS 0
#endif

#ifndef CT_ENABLE_CHACHA20
#define CT_ENABLE_CHACHA20 1
#endif
