// test_ipc_nng.c - NNG IPC 测试程序
// 
// 功能说明：
//   这个程序用于测试 NNG (nanomsg-next-gen) 库的 IPC 通信功能。
//   它创建一个发布者 (Publisher) 和一个订阅者 (Subscriber) 线程，
//   通过 ipc:// 在同一台机器上进行进程间通信。
//
// 编译方法：
//   gcc -o ./build/test/test_nng_ipc ./tests/test_nng_ipc.c \
//       -I./crates/nng/include \
//       -L./build/crates/nng \
//       -lnng \
//       -Wl,-rpath,./build/crates/nng \
//       -lpthread
//
// 运行方法：
//   ./build/test/test_nng_ipc
//
// 可选参数：
//   ./build/test/test_nng_ipc ipc:///tmp/test.sock    # Unix 域套接字（默认）
//   ./build/test/test_nng_ipc tcp://127.0.0.1:44945   # TCP 本地回环
//
// 预期输出：
//   [Pub] ✅ Publisher socket created
//   [Pub] 📡 Listening on ipc:///tmp/test.sock
//   [Sub] ✅ Connected to ipc:///tmp/test.sock
//   [Sub] 📡 Listening for messages...
//   [Pub] 📤 Sent [1]: {"type":"test","count":0,...}
//   [Sub] 📥 Received: {"type":"test","count":0,...}
//   ... (每秒发送一条消息，直到按 Ctrl+C 停止)
//
// 注意：
//   1. ipc:// 是 Unix 域套接字，用于同一台机器上的进程间通信
//   2. 按 Ctrl+C 优雅退出程序

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <nng/nng.h>

static volatile int g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// 订阅者线程函数
void *subscriber_thread(void *arg) {
    const char *url = (const char *)arg;
    nng_socket sock;
    int rv;
    
    if ((rv = nng_sub0_open(&sock)) != 0) {
        fprintf(stderr, "[Sub] nng_sub0_open: %s\n", nng_strerror(rv));
        return NULL;
    }
    
    if ((rv = nng_dial(sock, url, NULL, 0)) != 0) {
        fprintf(stderr, "[Sub] nng_dial %s: %s\n", url, nng_strerror(rv));
        nng_close(sock);
        return NULL;
    }
    
    // 订阅所有消息
    if ((rv = nng_sub0_socket_subscribe(sock, "", 0)) != 0) {
        fprintf(stderr, "[Sub] nng_sub0_socket_subscribe: %s\n", nng_strerror(rv));
        nng_close(sock);
        return NULL;
    }
    
    printf("[Sub] ✅ Connected to %s\n", url);
    printf("[Sub] 📡 Listening for messages...\n");
    
    while (g_running) {
        nng_msg *msg = NULL;
        if ((rv = nng_recvmsg(sock, &msg, 0)) == 0) {
            char *data = (char *)nng_msg_body(msg);
            size_t len = nng_msg_len(msg);
            printf("[Sub] 📥 Received: %.*s\n", (int)len, data);
            nng_msg_free(msg);
        }
    }
    
    nng_close(sock);
    printf("[Sub] 🛑 Stopped\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    nng_socket pub_sock;
    int rv;
    // 默认使用 IPC（Unix 域套接字）
    const char *url = "ipc:///tmp/cyctron_test.sock";
    int counter = 0;
    pthread_t sub_thread;
    
    if (argc > 1) {
        url = argv[1];
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("========================================\n");
    printf("  NNG IPC 测试\n");
    printf("========================================\n\n");
    
    // 1. 创建发布者套接字
    if ((rv = nng_pub0_open(&pub_sock)) != 0) {
        fprintf(stderr, "[Pub] nng_pub0_open: %s\n", nng_strerror(rv));
        return 1;
    }
    printf("[Pub] ✅ Publisher socket created\n");
    
    // 2. 监听 IPC 地址
    if ((rv = nng_listen(pub_sock, url, NULL, 0)) != 0) {
        fprintf(stderr, "[Pub] nng_listen %s: %s\n", url, nng_strerror(rv));
        nng_close(pub_sock);
        return 1;
    }
    printf("[Pub] 📡 Listening on %s\n", url);
    
    // 3. 启动订阅者线程
    if (pthread_create(&sub_thread, NULL, subscriber_thread, (void *)url) != 0) {
        fprintf(stderr, "Failed to create subscriber thread\n");
        nng_close(pub_sock);
        return 1;
    }
    
    sleep(1);
    
    printf("\n[Pub] 🔄 Sending messages...\n");
    printf("[Pub] Press Ctrl+C to stop...\n");
    printf("----------------------------------------\n");
    
    // 4. 发送消息循环
    while (g_running) {
        char msg[512];
        time_t now = time(NULL);
        
        snprintf(msg, sizeof(msg),
                 "{\"type\":\"test\",\"count\":%d,\"time\":%ld,\"message\":\"Hello from IPC\"}",
                 counter++,
                 now);
        
        nng_msg *nmsg = NULL;
        if ((rv = nng_msg_alloc(&nmsg, 0)) != 0) {
            fprintf(stderr, "[Pub] nng_msg_alloc: %s\n", nng_strerror(rv));
            break;
        }
        
        if ((rv = nng_msg_append(nmsg, msg, strlen(msg))) != 0) {
            fprintf(stderr, "[Pub] nng_msg_append: %s\n", nng_strerror(rv));
            nng_msg_free(nmsg);
            break;
        }
        
        if ((rv = nng_sendmsg(pub_sock, nmsg, 0)) != 0) {
            fprintf(stderr, "[Pub] nng_sendmsg: %s\n", nng_strerror(rv));
            nng_msg_free(nmsg);
            break;
        }
        
        printf("[Pub] 📤 Sent [%d]: %s\n", counter, msg);
        sleep(1);
    }
    
    printf("\n🛑 Shutting down...\n");
    g_running = 0;
    pthread_join(sub_thread, NULL);
    nng_close(pub_sock);
    printf("✅ Done\n");
    
    return 0;
}

