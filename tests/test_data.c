#include "../include/realtime.h"
#include <stdio.h>
#include <string.h>

void on_tick(rt_task_t *task) {
    // 每 5 次 tick 发送一次数据
    static int tick_counter = 0;
    tick_counter++;
    if (tick_counter % 5 == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Data from tick #%d", tick_counter);
        printf("[SEND] Sending: %s\n", msg);
        rt_send_data(task, msg, strlen(msg) + 1);
    }
}

void on_data(rt_task_t *task, void *data, size_t len) {
    if (data && len > 0) {
        printf("[RECV] Received: %s (len=%zu)\n", (char*)data, len);
    }
}

int main() {
    printf("========== Test 4: Data Sending ==========\n");
    
    rt_manager_t *mgr = rt_init();
    if (!mgr) return 1;
    printf(" Manager created\n");
    
    rt_task_t *task = rt_create_task(mgr, "data_socket", RT_TYPE_CUSTOM);
    if (!task) return 1;
    printf(" Task created\n");
    
    rt_task_set_tick(task, on_tick);
    rt_task_set_data(task, on_data);
    rt_start_task(task);
    printf(" Task started\n");
    
    // 运行 2 秒（20 次 tick）
    printf(" Running 2 seconds (sending data every 5 ticks)...\n");
    for (int i = 0; i < 20; i++) {
        rt_run(mgr);
    }
    
    rt_stop_task(task);
    rt_cleanup(mgr);
    printf("========== Test 4 Complete ==========\n\n");
    return 0;
}