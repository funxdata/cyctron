#include "../include/realtime.h"
#include <stdio.h>

int tick_count = 0;

void on_tick(rt_task_t *task) {
    tick_count++;
    printf("[TICK] #%d\n", tick_count);
}

int main() {
    printf("========== Test 3: Pause and Resume ==========\n");
    
    rt_manager_t *mgr = rt_init();
    if (!mgr) return 1;
    printf(" Manager created\n");
    
    rt_task_t *task = rt_create_task(mgr, "test_socket", RT_TYPE_CUSTOM);
    if (!task) return 1;
    printf(" Task created\n");
    
    rt_task_set_tick(task, on_tick);
    rt_start_task(task);
    printf(" Task started\n");
    
    // 运行 0.3 秒（3 次 tick）
    printf(" Running 3 ticks...\n");
    for (int i = 0; i < 3; i++) {
        rt_run(mgr);
    }
    printf(" Current tick count: %d\n", tick_count);
    
    // 暂停
    printf("  Pausing task...\n");
    rt_pause_task(task);
    
    // 运行 0.3 秒（应该没有 tick）
    printf(" Running 3 ticks (should be paused)...\n");
    int before_pause = tick_count;
    for (int i = 0; i < 3; i++) {
        rt_run(mgr);
    }
    printf(" Tick count unchanged: %d (should be same)\n", tick_count);
    
    if (tick_count == before_pause) {
        printf(" Pause successful - no new ticks\n");
    } else {
        printf(" Pause failed - ticks still running\n");
    }
    
    // 恢复
    printf("  Resuming task...\n");
    rt_resume_task(task);
    
    // 运行 0.3 秒（应该恢复 tick）
    printf(" Running 3 ticks (should resume)...\n");
    for (int i = 0; i < 3; i++) {
        rt_run(mgr);
    }
    printf(" Final tick count: %d\n", tick_count);
    
    rt_stop_task(task);
    rt_cleanup(mgr);
    printf("========== Test 3 Complete ==========\n\n");
    return 0;
}