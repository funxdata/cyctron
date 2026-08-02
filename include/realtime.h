#ifndef REALTIME_H
#define REALTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RT_TASK_BIND = 0,
    RT_TASK_REMOVE = 1,
    RT_TASK_STATUS = 2,
} rt_task_type_t;

typedef enum {
    RT_TASK_IDLE = 0,
    RT_TASK_RUNNING = 1,
    RT_TASK_PAUSED = 2,
    RT_TASK_STOPPED = 3,
    RT_TASK_ERROR = -1,
} rt_task_status_t;

typedef struct rt_task {
    char task_id[64];
    char socket_id[128];
    char uuid[64];
    rt_task_type_t type;
    rt_task_status_t status;
    void *user_data;
    struct rt_task *next;
    void *manager;
    void (*on_tick)(struct rt_task*);
    void (*on_data)(struct rt_task*, void*, size_t);
    void (*on_cleanup)(struct rt_task*);
} rt_task_t;

// 不透明指针
typedef struct rt_manager rt_manager_t;

// API 函数
rt_manager_t* rt_init(void);
void rt_cleanup(rt_manager_t *mgr);

rt_task_t* rt_create_task(rt_manager_t *mgr, const char *uuid, const char *socket_path);
int rt_start_task(rt_task_t *task, const char *lib_name);
int rt_stop_task(rt_task_t *task);
int rt_remove_task(rt_task_t *task);
rt_task_t* rt_find_task_by_uuid(rt_manager_t *mgr, const char *uuid);

void rt_task_set_tick(rt_task_t *task, void (*callback)(rt_task_t*));
void rt_task_set_data(rt_task_t *task, void (*callback)(rt_task_t*, void*, size_t));
void rt_task_set_cleanup(rt_task_t *task, void (*callback)(rt_task_t*));

rt_task_status_t rt_get_task_status(const rt_task_t *task);
int rt_get_task_count(rt_manager_t *mgr);
void rt_dump_tasks(rt_manager_t *mgr);

void rt_run(rt_manager_t *mgr);
void rt_stop(rt_manager_t *mgr);
void rt_broadcast(rt_manager_t *mgr, const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // REALTIME_H

