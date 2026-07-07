#include "../include/realtime.h"
#include <stdio.h>

int tick_counts[3] = {0, 0, 0};

// 定时回调 - 不同任务独立计数
void on_tick_task1(rt_task_t *task) {
    tick_counts[0]++;
    printf("[T1] Tick #%d\n", tick_counts[0]);
}

void on_tick_task2(rt_task_t *task) {
    tick_counts[1]++;
    printf("[T2] Tick #%d\n", tick_counts[1]);
}

void on_tick_task3(rt_task_t *task) {
    tick_counts[2]++;
    printf("[T3] Tick #%d\n", tick_counts[2]);
}

int main() {
    printf("========== Test 2: Multiple Tasks ==========\n");
    
    rt_manager_t *mgr = rt_init();
    if (!mgr) return 1;
    printf(" Manager created\n");
    
    // 创建 3 个任务
    rt_task_t *tasks[3];
    tasks[0] = rt_create_task(mgr, "socket_001", RT_TYPE_CUSTOM);
    tasks[1] = rt_create_task(mgr, "socket_002", RT_TYPE_CUSTOM);
    tasks[2] = rt_create_task(mgr, "socket_003", RT_TYPE_CUSTOM);
    
    // 设置不同的回调
    rt_task_set_tick(tasks[0], on_tick_task1);
    rt_task_set_tick(tasks[1], on_tick_task2);
    rt_task_set_tick(tasks[2], on_tick_task3);
    
    // 启动所有任务
    for (int i = 0; i < 3; i++) {
        rt_start_task(tasks[i]);
    }
    printf(" All 3 tasks started\n");
    
    // 运行 0.5 秒
    printf(" Running 0.5 seconds...\n");
    for (int i = 0; i < 5; i++) {
        rt_run(mgr);
    }
    
    // 统计
    printf("\n Statistics:\n");
    for (int i = 0; i < 3; i++) {
        printf("  Task %d: %d ticks\n", i+1, tick_counts[i]);
    }
    
    // 清理
    for (int i = 0; i < 3; i++) {
        rt_stop_task(tasks[i]);
    }
    rt_cleanup(mgr);
    printf("========== Test 2 Complete ==========\n\n");
    return 0;
}