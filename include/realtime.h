#ifndef REALTIME_H
#define REALTIME_H

#include <stdint.h>
#include <stddef.h>
#include <ev.h>

// ============ 枚举定义 ============

// 实时任务状态
typedef enum {
    RT_TASK_IDLE = 0,
    RT_TASK_RUNNING,
    RT_TASK_PAUSED,
    RT_TASK_STOPPED,
    RT_TASK_ERROR = -1
} rt_task_status_t;

// 实时任务类型
typedef enum {
    RT_TYPE_DATA_PROCESS = 0,
    RT_TYPE_SCHEDULER,
    RT_TYPE_MONITOR,
    RT_TYPE_CUSTOM
} rt_task_type_t;

// ============ 前向声明 ============

typedef struct rt_task rt_task_t;
typedef struct rt_manager rt_manager_t;

// ============ 任务结构 ============

struct rt_task {
    // 基础信息
    char task_id[64];
    char socket_id[64];
    rt_task_type_t type;
    rt_task_status_t status;
    
    // 用户数据
    void *user_data;
    
    // 管理器指针
    rt_manager_t *manager;
    
    // libev 事件
    ev_timer timer;
    ev_io io_watcher;
    ev_async async_watcher;
    
    // 回调函数
    void (*on_tick)(rt_task_t *task);
    void (*on_data)(rt_task_t *task, void *data, size_t len);
    void (*on_cleanup)(rt_task_t *task);
    
    // 链表指针
    rt_task_t *next;
};

// ============ 管理器结构 ============

struct rt_manager {
    struct ev_loop *loop;       // libev 事件循环（使用 struct ev_loop 而不是 ev_loop）
    rt_task_t *tasks;           // 任务链表
    int task_count;             // 任务数量
    int running;                // 运行状态
    void (*on_task_complete)(rt_task_t *task);
};

// ============ API 函数 ============

// 初始化管理器
rt_manager_t* rt_init(void);

// 创建任务
rt_task_t* rt_create_task(rt_manager_t *mgr, 
                          const char *socket_id,
                          rt_task_type_t type);

// 生命周期管理
int rt_start_task(rt_task_t *task);
int rt_stop_task(rt_task_t *task);
int rt_pause_task(rt_task_t *task);
int rt_resume_task(rt_task_t *task);

// 数据交互
int rt_send_data(rt_task_t *task, void *data, size_t len);

// 回调设置
void rt_task_set_tick(rt_task_t *task, void (*callback)(rt_task_t*));
void rt_task_set_data(rt_task_t *task, void (*callback)(rt_task_t*, void*, size_t));
void rt_task_set_cleanup(rt_task_t *task, void (*callback)(rt_task_t*));

// 查询
rt_task_status_t rt_get_task_status(const rt_task_t *task);

// 运行事件循环
void rt_run(rt_manager_t *mgr);
void rt_stop(rt_manager_t *mgr);

// 清理
void rt_cleanup(rt_manager_t *mgr);

// 广播
void rt_broadcast(rt_manager_t *mgr, const char *data, size_t len);

#endif // REALTIME_H