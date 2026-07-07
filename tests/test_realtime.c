#include "../include/realtime.h"
#include <stdio.h>

void on_tick(rt_task_t *task) {
    static int count = 0;
    printf("[TEST] %s tick %d\n", task->task_id, ++count);
}

int main() {
    printf("=== TEST START ===\n");
    
    rt_manager_t *mgr = rt_init();
    if (!mgr) return 1;
    
    // 修复：传入 3 个参数
    rt_task_t *t1 = rt_create_task(mgr, "Task1", RT_TYPE_CUSTOM);
    rt_task_t *t2 = rt_create_task(mgr, "Task2", RT_TYPE_CUSTOM);
    
    t1->on_tick = on_tick;
    t2->on_tick = on_tick;
    
    rt_start_task(t1);
    rt_start_task(t2);
    
    printf("Running for 5 seconds...\n");
    for (int i = 0; i < 50; i++) {
        rt_run(mgr);
    }
    
    rt_stop_task(t1);
    rt_stop_task(t2);
    
    rt_cleanup(mgr);
    printf("=== TEST END ===\n");
    return 0;
}