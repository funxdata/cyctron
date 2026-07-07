#include "../include/realtime.h"
#include <stdio.h>

static int tick_count = 0;

void on_tick(rt_task_t *task) {
    tick_count++;
    printf("[TEST] %s tick #%d\n", task->task_id, tick_count);
}

int main() {
    printf("========== Test: Basic Lifecycle ==========\n");

    rt_manager_t *mgr = rt_init();
    if (!mgr) {
        printf(" rt_init failed\n");
        return 1;
    }
    printf(" Manager created\n");

    rt_task_t *task = rt_create_task(mgr, "socket_001", RT_TYPE_CUSTOM);
    if (!task) {
        printf(" task create failed\n");
        rt_cleanup(mgr);
        return 1;
    }
    printf(" Task created: %s\n", task->task_id);

    rt_task_set_tick(task, on_tick);
    rt_start_task(task);
    printf(" Task started\n");

    printf(" Running loop...\n");
    for (int i = 0; i < 10; i++) {
        rt_run(mgr);
    }

    rt_stop_task(task);
    rt_cleanup(mgr);

    printf("========== Test Complete ==========\n");
    printf("tick_count = %d\n", tick_count);

    return tick_count > 0 ? 0 : 1;
}