#include "realtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
#endif

// ============ 内部函数 ============

static int task_counter = 0;

// 生成唯一任务ID
static void generate_task_id(char *id, size_t size) {
    static uint64_t counter = 0;
    
    uint64_t val;
#ifdef _WIN32
    // Windows 下使用 InterlockedIncrement
    val = (uint64_t)InterlockedIncrement((long*)&counter);
#else
    // Linux 下使用 GCC 原子操作
    val = __sync_fetch_and_add(&counter, 1);
#endif
    
    snprintf(id, size, "task_%lu_%lu", 
             (unsigned long)time(NULL), 
             (unsigned long)val);
}

// ============ libev 回调函数 ============

// 定时器回调
static void timer_callback(struct ev_loop *loop, ev_timer *w, int revents) {
    rt_task_t *task = (rt_task_t *)w->data;
    if (!task || task->status != RT_TASK_RUNNING) return;
    
    if (task->on_tick) {
        task->on_tick(task);
    }
}

// IO回调
static void io_callback(struct ev_loop *loop, ev_io *w, int revents) {
    rt_task_t *task = (rt_task_t *)w->data;
    if (!task || task->status != RT_TASK_RUNNING) return;
    
    if (task->on_data) {
        task->on_data(task, NULL, 0);
    }
}

// 异步回调
static void async_callback(struct ev_loop *loop, ev_async *w, int revents) {
    rt_task_t *task = (rt_task_t *)w->data;
    if (!task) return;
    
    if (task->on_data) {
        task->on_data(task, NULL, 0);
    }
}

// ============ API 实现 ============

// 初始化管理器
rt_manager_t* rt_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    static int wsa_initialized = 0;
    if (!wsa_initialized) {
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("[RT] WSAStartup failed\n");
            return NULL;
        }
        wsa_initialized = 1;
        printf("[RT] WSAStartup successful\n");
    }
#endif

    rt_manager_t *mgr = (rt_manager_t *)calloc(1, sizeof(rt_manager_t));
    if (!mgr) {
        printf("[RT] Failed to allocate manager\n");
        return NULL;
    }
    
    // 使用 ev_loop_new 代替 ev_default_loop
    mgr->loop = ev_loop_new(EVFLAG_AUTO);
    if (!mgr->loop) {
        printf("[RT] Failed to create event loop\n");
        free(mgr);
        return NULL;
    }
    
    mgr->tasks = NULL;
    mgr->task_count = 0;
    mgr->running = 1;
    mgr->on_task_complete = NULL;
    
    printf("[RT] Init (libev loop created)\n");
    return mgr;
}

// 创建任务
rt_task_t* rt_create_task(rt_manager_t *mgr, 
                          const char *socket_id,
                          rt_task_type_t type) {
    if (!mgr) {
        printf("[RT] Manager is NULL\n");
        return NULL;
    }
    
    rt_task_t *task = (rt_task_t *)calloc(1, sizeof(rt_task_t));
    if (!task) {
        printf("[RT] Failed to allocate task\n");
        return NULL;
    }
    
    // 生成任务ID
    generate_task_id(task->task_id, sizeof(task->task_id));
    
    // 复制socket ID
    if (socket_id) {
        strncpy(task->socket_id, socket_id, sizeof(task->socket_id) - 1);
    } else {
        strcpy(task->socket_id, "unknown");
    }
    
    task->type = type;
    task->status = RT_TASK_IDLE;
    task->user_data = NULL;
    task->manager = mgr;
    task->next = NULL;
    task->on_tick = NULL;
    task->on_data = NULL;
    task->on_cleanup = NULL;
    
    // 初始化 libev watchers
    ev_timer_init(&task->timer, timer_callback, 0.0, 0.0);
    task->timer.data = task;
    
    ev_io_init(&task->io_watcher, io_callback, 0, EV_READ);
    task->io_watcher.data = task;
    
    ev_async_init(&task->async_watcher, async_callback);
    task->async_watcher.data = task;
    
    // 添加到任务链表
    task->next = mgr->tasks;
    mgr->tasks = task;
    mgr->task_count++;
    
    printf("[RT] Created task: %s (socket: %s, type: %d)\n", 
           task->task_id, task->socket_id, type);
    
    return task;
}

// 启动任务
int rt_start_task(rt_task_t *task) {
    if (!task) {
        printf("[RT] Task is NULL\n");
        return -1;
    }
    
    if (task->status == RT_TASK_RUNNING) {
        printf("[RT] Task %s already running\n", task->task_id);
        return 0;
    }
    
    task->status = RT_TASK_RUNNING;
    
    // 启动定时器（每100ms执行一次）
    ev_timer_set(&task->timer, 0.1, 0.1);
    ev_timer_start(task->manager->loop, &task->timer);
    
    // 启动异步监听
    ev_async_start(task->manager->loop, &task->async_watcher);
    
    printf("[RT] Started task: %s\n", task->task_id);
    return 0;
}

// 停止任务
int rt_stop_task(rt_task_t *task) {
    if (!task) return -1;
    
    if (task->status == RT_TASK_STOPPED) {
        printf("[RT] Task %s already stopped\n", task->task_id);
        return 0;
    }
    
    task->status = RT_TASK_STOPPED;
    
    // 停止所有 watchers
    ev_timer_stop(task->manager->loop, &task->timer);
    ev_io_stop(task->manager->loop, &task->io_watcher);
    ev_async_stop(task->manager->loop, &task->async_watcher);
    
    printf("[RT] Stopped task: %s\n", task->task_id);
    return 0;
}

// 暂停任务
int rt_pause_task(rt_task_t *task) {
    if (!task) return -1;
    
    if (task->status != RT_TASK_RUNNING) {
        printf("[RT] Task %s not running\n", task->task_id);
        return -1;
    }
    
    task->status = RT_TASK_PAUSED;
    ev_timer_stop(task->manager->loop, &task->timer);
    
    printf("[RT] Paused task: %s\n", task->task_id);
    return 0;
}

// 恢复任务
int rt_resume_task(rt_task_t *task) {
    if (!task) return -1;
    
    if (task->status != RT_TASK_PAUSED) {
        printf("[RT] Task %s not paused\n", task->task_id);
        return -1;
    }
    
    task->status = RT_TASK_RUNNING;
    ev_timer_start(task->manager->loop, &task->timer);
    
    printf("[RT] Resumed task: %s\n", task->task_id);
    return 0;
}

// 发送数据到任务
int rt_send_data(rt_task_t *task, void *data, size_t len) {
    if (!task || !data) return -1;
    
    if (task->status != RT_TASK_RUNNING) {
        printf("[RT] Task %s not running, cannot send data\n", task->task_id);
        return -1;
    }
    
    // 使用异步通知机制发送数据
    ev_async_send(task->manager->loop, &task->async_watcher);
    
    printf("[RT] Data sent to task: %s\n", task->task_id);
    return 0;
}

// 设置定时回调
void rt_task_set_tick(rt_task_t *task, void (*callback)(rt_task_t*)) {
    if (task) {
        task->on_tick = callback;
        printf("[RT] Tick callback set for task: %s\n", task->task_id);
    }
}

// 设置数据回调
void rt_task_set_data(rt_task_t *task, void (*callback)(rt_task_t*, void*, size_t)) {
    if (task) {
        task->on_data = callback;
        printf("[RT] Data callback set for task: %s\n", task->task_id);
    }
}

// 设置清理回调
void rt_task_set_cleanup(rt_task_t *task, void (*callback)(rt_task_t*)) {
    if (task) {
        task->on_cleanup = callback;
        printf("[RT] Cleanup callback set for task: %s\n", task->task_id);
    }
}

// 获取任务状态
rt_task_status_t rt_get_task_status(const rt_task_t *task) {
    return task ? task->status : RT_TASK_ERROR;
}

// 运行事件循环
void rt_run(rt_manager_t *mgr) {
    if (!mgr || !mgr->running) return;
    
    // 运行 libev 事件循环（每次运行 100ms）
    ev_run(mgr->loop, EVRUN_ONCE);
}

// 停止事件循环
void rt_stop(rt_manager_t *mgr) {
    if (mgr) {
        mgr->running = 0;
        printf("[RT] Stopping manager\n");
    }
}

// 销毁任务（内部使用）
static void rt_destroy_task(rt_task_t *task) {
    if (!task) return;
    
    // 停止任务
    rt_stop_task(task);
    
    // 执行清理回调
    if (task->on_cleanup) {
        task->on_cleanup(task);
    }
    
    // 清理用户数据
    if (task->user_data) {
        free(task->user_data);
        task->user_data = NULL;
    }
    
    free(task);
    printf("[RT] Task destroyed\n");
}

// 清理所有任务
void rt_cleanup(rt_manager_t *mgr) {
    if (!mgr) return;
    
    rt_task_t *task = mgr->tasks;
    while (task) {
        rt_task_t *next = task->next;
        rt_destroy_task(task);
        task = next;
    }
    
    mgr->tasks = NULL;
    mgr->task_count = 0;
    mgr->running = 0;
    
    // 销毁事件循环
    if (mgr->loop) {
        ev_loop_destroy(mgr->loop);
        mgr->loop = NULL;
    }
    
    free(mgr);
    printf("[RT] Cleanup complete\n");
}

// 广播消息到所有任务
void rt_broadcast(rt_manager_t *mgr, const char *data, size_t len) {
    if (!mgr || !data) return;
    
    rt_task_t *task = mgr->tasks;
    while (task) {
        if (task->status == RT_TASK_RUNNING) {
            rt_send_data(task, (void *)data, len);
        }
        task = task->next;
    }
    
    printf("[RT] Broadcasted to all tasks\n");
}
