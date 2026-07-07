#include "../include/realtime.h"
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define GET_TIME() GetTickCount()
#else
    #include <sys/time.h>
    #define GET_TIME() ({ \
        struct timeval tv; \
        gettimeofday(&tv, NULL); \
        (tv.tv_sec * 1000 + tv.tv_usec / 1000); \
    })
#endif

int total_ticks = 0;

void on_tick(rt_task_t *task) {
    total_ticks++;
}

int main() {
    printf("========== Test 5: Performance ==========\n");
    
    #define NUM_TASKS 10  // 使用宏定义
    
    rt_manager_t *mgr = rt_init();
    if (!mgr) return 1;
    printf(" Manager created\n");
    
    rt_task_t *tasks[NUM_TASKS];
    for (int i = 0; i < NUM_TASKS; i++) {
        char socket_id[32];
        snprintf(socket_id, sizeof(socket_id), "perf_%d", i);
        tasks[i] = rt_create_task(mgr, socket_id, RT_TYPE_CUSTOM);
        rt_task_set_tick(tasks[i], on_tick);
        rt_start_task(tasks[i]);
    }
    printf(" %d tasks created and started\n", NUM_TASKS);
    
    printf(" Running 1 second...\n");
    unsigned long start_time = GET_TIME();
    for (int i = 0; i < 10; i++) rt_run(mgr);
    unsigned long end_time = GET_TIME();
    unsigned long elapsed = end_time - start_time;
    
    printf("\n Performance Results:\n");
    printf("  Total ticks: %d\n", total_ticks);
    printf("  Elapsed time: %lu ms\n", elapsed);
    printf("  Ticks per second: %.2f\n", (float)total_ticks / (elapsed / 1000.0));
    printf("  Tasks: %d\n", NUM_TASKS);
    
    for (int i = 0; i < NUM_TASKS; i++) rt_stop_task(tasks[i]);
    rt_cleanup(mgr);
    printf("========== Test 5 Complete ==========\n\n");
    return 0;
}