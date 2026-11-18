#include "realtime.h"
#include "event_pusher.h"
#include "calldyn.h"
#include "plugin_path.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

// ============ 调度中心状态 ============

typedef struct rt_task_node {
    rt_task_t task;
    struct rt_task_node *next;
    nng_socket nng_sock;
    int nng_initialized;
    time_t created_at;
} rt_task_node_t;

static struct {
    rt_task_node_t *tasks;
    int count;
    int running;
    pthread_mutex_t mutex;
} g_scheduler = {
    .tasks = NULL,
    .count = 0,
    .running = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

static uint64_t g_counter = 0;

// ============ 内部函数 ============

static void gen_id(char *id, size_t size) {
    uint64_t n = __sync_fetch_and_add(&g_counter, 1);
    snprintf(id, size, "task_%llu_%llu",
             (unsigned long long)time(NULL), (unsigned long long)n);
}

// 调用者必须持有 g_scheduler.mutex
static rt_task_node_t* find_node_locked(const char *uuid) {
    rt_task_node_t *n = g_scheduler.tasks;
    while (n) {
        if (strcmp(n->task.uuid, uuid) == 0) return n;
        n = n->next;
    }
    return NULL;
}

static void send_rt_event(const char *action, const char *uuid, const char *status) {
    char euuid[128];
    event_escape_json(uuid ? uuid : "", euuid, sizeof(euuid));
    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"action\":\"%s\",\"uuid\":\"%s\",\"status\":\"%s\"}",
             action, euuid, status ? status : "");
    event_push_with_type("realtime", msg);
}

// ============ 调度 API ============

rt_manager_t* rt_init(void) {
    pthread_mutex_lock(&g_scheduler.mutex);
    if (!g_scheduler.running) {
        g_scheduler.running = 1;
        g_scheduler.count = 0;
        g_scheduler.tasks = NULL;
        log_info("[Scheduler] Init");
    }
    pthread_mutex_unlock(&g_scheduler.mutex);
    return (rt_manager_t*)&g_scheduler;
}

rt_task_t* rt_create_task(rt_manager_t *mgr, const char *uuid, const char *socket_path) {
    (void)mgr;
    (void)socket_path;  // 不再使用传入的路径
    if (!uuid) return NULL;
    
    pthread_mutex_lock(&g_scheduler.mutex);
    
    if (find_node_locked(uuid)) {
        pthread_mutex_unlock(&g_scheduler.mutex);
        log_warn("[Scheduler] Task %s exists", uuid);
        return NULL;
    }
    
    rt_task_node_t *node = calloc(1, sizeof(rt_task_node_t));
    if (!node) {
        pthread_mutex_unlock(&g_scheduler.mutex);
        return NULL;
    }
    
    memset(&node->nng_sock, 0, sizeof(nng_socket));
    node->nng_initialized = 0;
    node->created_at = time(NULL);
    
    gen_id(node->task.task_id, sizeof(node->task.task_id));
    strncpy(node->task.uuid, uuid, sizeof(node->task.uuid) - 1);
    node->task.uuid[sizeof(node->task.uuid) - 1] = '\0';
    
    // 直接生成 socket 路径: /tmp/cyctron_<uuid>.sock
    snprintf(node->task.socket_id, sizeof(node->task.socket_id), "/tmp/cyctron_%s.sock", uuid);
    node->task.socket_id[sizeof(node->task.socket_id) - 1] = '\0';
    
    node->task.status = RT_TASK_IDLE;
    node->task.manager = mgr;
    node->task.conn_id = 0;
    node->task.user_data = NULL;
    node->task.on_tick = NULL;
    node->task.on_data = NULL;
    node->task.on_cleanup = NULL;
    
    node->next = g_scheduler.tasks;
    g_scheduler.tasks = node;
    g_scheduler.count++;

    char task_id[sizeof(node->task.task_id)];
    char socket_id[sizeof(node->task.socket_id)];
    memcpy(task_id, node->task.task_id, sizeof(task_id));
    memcpy(socket_id, node->task.socket_id, sizeof(socket_id));
    pthread_mutex_unlock(&g_scheduler.mutex);

    log_info("[Scheduler] Created: %s (uuid: %s, socket: %s)",
             task_id, uuid, socket_id);
    send_rt_event("create", uuid, "created");
    return &node->task;
}

int rt_start_task(rt_task_t *task, const char *lib_name) {
    if (!task) return -1;
    const char *lib = lib_name ? lib_name : "soket_demo";
    if (!plugin_name_is_safe(lib)) {
        log_error("[Scheduler] invalid lib name: %s", lib);
        return -1;
    }
    if (task->status == RT_TASK_RUNNING) {
        log_warn("[Scheduler] Task %s already running", task->uuid);
        return 0;
    }
    
    rt_task_node_t *node = (rt_task_node_t*)task;
    char socket_url[256];
    
    // 创建 NNG 监听
    if (!node->nng_initialized) {
        int rv = nng_pub0_open(&node->nng_sock);
        if (rv != 0) {
            log_error("[Scheduler] nng_pub0_open failed: %s", nng_strerror(rv));
            return -1;
        }
        
        snprintf(socket_url, sizeof(socket_url), "ipc://%s", task->socket_id);
        
        rv = nng_listen(node->nng_sock, socket_url, NULL, 0);
        if (rv != 0) {
            log_error("[Scheduler] nng_listen %s failed: %s", socket_url, nng_strerror(rv));
            nng_close(node->nng_sock);
            return -1;
        }
        
        node->nng_initialized = 1;
        log_info("[Scheduler] NNG listening on %s", socket_url);
    }
    
    // 构建调用参数
    char json_in[512];

    snprintf(socket_url, sizeof(socket_url), "ipc://%s", task->socket_id);
    
    snprintf(json_in, sizeof(json_in), 
             "{\"config\":{\"uuid\":\"%s\",\"nng_url\":\"%s\"}}",
             task->uuid, socket_url);
    
    log_info("[Scheduler] Starting %s with %s", task->uuid, lib);
    
    char *json_out = NULL;
    int ret = call_local_dyn_socklibffi(lib, json_in, &json_out);
    
    if (ret == 0) {
        task->status = RT_TASK_RUNNING;
        log_info("[Scheduler] Started: %s", task->uuid);
        if (json_out) {
            log_info("[Scheduler] Output: %s", json_out);
            free(json_out);
        }
        send_rt_event("bind", task->uuid, "running");
        return 0;
    }
    
    log_error("[Scheduler] Start %s failed: %d", task->uuid, ret);
    if (json_out) free(json_out);
    task->status = RT_TASK_ERROR;
    return -1;
}

// 从链表摘除后调用；不持有 g_scheduler.mutex
static void node_destroy(rt_task_node_t *node, const char *action) {
    if (!node) return;
    if (node->nng_initialized) {
        nng_close(node->nng_sock);
        node->nng_initialized = 0;
    }
    node->task.status = RT_TASK_STOPPED;
    log_info("[Scheduler] Removed: %s", node->task.uuid);
    if (action) send_rt_event(action, node->task.uuid, "stopped");
    if (node->task.on_cleanup) node->task.on_cleanup(&node->task);
    if (node->task.user_data) {
        free(node->task.user_data);
        node->task.user_data = NULL;
    }
    free(node);
}

int rt_remove_task(rt_task_t *task) {
    if (!task) return -1;
    rt_task_node_t *node = (rt_task_node_t *) task;
    int found = 0;

    pthread_mutex_lock(&g_scheduler.mutex);
    rt_task_node_t **pp = &g_scheduler.tasks;
    while (*pp) {
        if (*pp == node) {
            *pp = node->next;
            g_scheduler.count--;
            found = 1;
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&g_scheduler.mutex);

    if (!found) return -1;
    node_destroy(node, "remove");
    return 0;
}

int rt_stop_task(rt_task_t *task) {
    if (!task) return -1;
    if (task->status == RT_TASK_STOPPED) return 0;
    task->status = RT_TASK_STOPPED;
    log_info("[Scheduler] Stopped: %s", task->uuid);
    send_rt_event("stop", task->uuid, "stopped");
    return 0;
}

rt_task_t* rt_find_task_by_uuid(rt_manager_t *mgr, const char *uuid) {
    (void)mgr;
    pthread_mutex_lock(&g_scheduler.mutex);
    rt_task_node_t *n = find_node_locked(uuid);
    rt_task_t *task = n ? &n->task : NULL;
    pthread_mutex_unlock(&g_scheduler.mutex);
    return task;
}

int rt_send_msg(rt_task_t *task, const char *data) {
    if (!task || !data) return -1;
    if (task->status != RT_TASK_RUNNING) return -2;

    char euuid[128], edata[1024];
    event_escape_json(task->uuid, euuid, sizeof(euuid));
    event_escape_json(data, edata, sizeof(edata));

    char msg[1152];
    snprintf(msg, sizeof(msg), "{\"target\":\"%s\",\"data\":\"%s\"}", euuid, edata);
    event_push_with_type("msg", msg);
    log_info("[Scheduler] Msg sent to %s", task->uuid);
    return 0;
}

void rt_broadcast(rt_manager_t *mgr, const char *data, size_t len) {
    (void)mgr;
    if (!data) return;

    /* 锁内只做快照，事件发送放到锁外，避免 scheduler → ws 的锁嵌套。 */
    pthread_mutex_lock(&g_scheduler.mutex);
    int cap = g_scheduler.count;
    char (*uuids)[64] = cap > 0 ? malloc((size_t) cap * sizeof(*uuids)) : NULL;
    int n = 0;
    if (uuids) {
        for (rt_task_node_t *cur = g_scheduler.tasks; cur && n < cap; cur = cur->next) {
            if (cur->task.status == RT_TASK_RUNNING) {
                memcpy(uuids[n], cur->task.uuid, sizeof(uuids[n]));
                uuids[n][sizeof(uuids[n]) - 1] = '\0';
                n++;
            }
        }
    }
    pthread_mutex_unlock(&g_scheduler.mutex);

    for (int i = 0; i < n; i++) {
        char msg[512];
        snprintf(msg, sizeof(msg), "{\"target\":\"%s\",\"data\":%.*s}",
                 uuids[i], (int) len, data);
        event_push_with_type("broadcast", msg);
    }
    free(uuids);
    log_info("[Scheduler] Broadcasted");
}

rt_task_status_t rt_get_task_status(const rt_task_t *task) {
    if (!task) return RT_TASK_ERROR;
    pthread_mutex_lock(&g_scheduler.mutex);
    rt_task_status_t status = task->status;
    pthread_mutex_unlock(&g_scheduler.mutex);
    return status;
}

int rt_get_task_count(rt_manager_t *mgr) {
    (void)mgr;
    pthread_mutex_lock(&g_scheduler.mutex);
    int count = g_scheduler.count;
    pthread_mutex_unlock(&g_scheduler.mutex);
    return count;
}

void rt_run(rt_manager_t *mgr) {
    (void)mgr;
    /* 保留 API 兼容：心跳/超时循环当前未启用（没有可靠的心跳来源）。 */
}

void rt_stop(rt_manager_t *mgr) {
    (void)mgr;
    pthread_mutex_lock(&g_scheduler.mutex);
    g_scheduler.running = 0;
    pthread_mutex_unlock(&g_scheduler.mutex);
    log_info("[Scheduler] Stopped");
}


int rt_remove_tasks_by_conn(rt_manager_t *mgr, uint64_t conn_id) {
    (void)mgr;
    rt_task_node_t *victims = NULL;

    pthread_mutex_lock(&g_scheduler.mutex);
    rt_task_node_t **pp = &g_scheduler.tasks;
    while (*pp) {
        rt_task_node_t *cur = *pp;
        if (cur->task.conn_id == conn_id) {
            *pp = cur->next;
            cur->next = victims;
            victims = cur;
            g_scheduler.count--;
        } else {
            pp = &cur->next;
        }
    }
    pthread_mutex_unlock(&g_scheduler.mutex);

    int removed = 0;
    while (victims) {
        rt_task_node_t *next = victims->next;
        node_destroy(victims, "remove");
        victims = next;
        removed++;
    }
    return removed;
}

void rt_cleanup(rt_manager_t *mgr) {
    (void)mgr;
    pthread_mutex_lock(&g_scheduler.mutex);
    rt_task_node_t *list = g_scheduler.tasks;
    g_scheduler.tasks = NULL;
    g_scheduler.count = 0;
    g_scheduler.running = 0;
    pthread_mutex_unlock(&g_scheduler.mutex);

    while (list) {
        rt_task_node_t *next = list->next;
        const char *action = (list->task.status == RT_TASK_RUNNING) ? "cleanup" : NULL;
        node_destroy(list, action);
        list = next;
    }
    log_info("[Scheduler] Cleanup done");
}

void rt_task_set_tick(rt_task_t *task, void (*cb)(rt_task_t*)) {
    if (task) task->on_tick = cb;
}
void rt_task_set_data(rt_task_t *task, void (*cb)(rt_task_t*, void*, size_t)) {
    if (task) task->on_data = cb;
}
void rt_task_set_cleanup(rt_task_t *task, void (*cb)(rt_task_t*)) {
    if (task) task->on_cleanup = cb;
}

void rt_dump_tasks(rt_manager_t *mgr) {
    (void)mgr;
    pthread_mutex_lock(&g_scheduler.mutex);
    log_info("[Scheduler] %d tasks", g_scheduler.count);
    rt_task_node_t *n = g_scheduler.tasks;
    int idx = 0;
    while (n) {
        log_info("[Scheduler] [%d] %s | %s | %d", 
                 idx++, n->task.task_id, n->task.uuid, n->task.status);
        n = n->next;
    }
    pthread_mutex_unlock(&g_scheduler.mutex);
}

