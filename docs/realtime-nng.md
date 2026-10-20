# realtime 与 NNG

realtime 模块管理“实时任务”，每个任务对应一个动态库实例和一个 NNG PUB 通道，
用于把消息/事件推送给插件进程。

## 1. 任务模型

```c
typedef struct rt_task {
    char task_id[64];      // 内部生成
    char socket_id[128];   // NNG IPC 路径
    char uuid[64];         // 客户端提供
    rt_task_type_t type;   // BIND / REMOVE / STATUS
    rt_task_status_t status;
    void *user_data;
    void *manager;
    void (*on_tick)(rt_task_t*);
    void (*on_data)(rt_task_t*, void*, size_t);
    void (*on_cleanup)(rt_task_t*);
} rt_task_t;
```

状态：

| 状态 | 含义 |
| --- | --- |
| `RT_TASK_IDLE` | 已创建，未启动 |
| `RT_TASK_RUNNING` | 插件 `process_init` 成功 |
| `RT_TASK_PAUSED` | 预留 |
| `RT_TASK_STOPPED` | 已停止/移除 |
| `RT_TASK_ERROR` | 启动失败 |

## 2. API

```c
rt_manager_t *rt_init(void);
void rt_cleanup(rt_manager_t *mgr);

rt_task_t *rt_create_task(rt_manager_t *mgr, const char *uuid, const char *socket_path);
int rt_start_task(rt_task_t *task, const char *lib_name);
int rt_stop_task(rt_task_t *task);
int rt_remove_task(rt_task_t *task);
rt_task_t *rt_find_task_by_uuid(rt_manager_t *mgr, const char *uuid);

void rt_task_set_tick(rt_task_t *task, void (*cb)(rt_task_t*));
void rt_task_set_data(rt_task_t *task, void (*cb)(rt_task_t*, void*, size_t));
void rt_task_set_cleanup(rt_task_t *task, void (*cb)(rt_task_t*));

rt_task_status_t rt_get_task_status(const rt_task_t *task);
int rt_get_task_count(rt_manager_t *mgr);
void rt_dump_tasks(rt_manager_t *mgr);

void rt_run(rt_manager_t *mgr);
void rt_stop(rt_manager_t *mgr);
void rt_broadcast(rt_manager_t *mgr, const char *data, size_t len);
```

## 3. NNG 通道

### 3.1 全局事件通道

`event_pusher.c` 创建：

```
ipc:///tmp/cyctron.sock
```

类型：PUB。所有 `event_push*` 事件都会发布到这里，供外部订阅者消费。

### 3.2 每任务通道

`rt_start_task` 为每个任务创建 PUB 并监听：

```
ipc:///tmp/cyctron_<uuid>.sock
```

然后把地址通过 `process_init` 传给插件：

```json
{"config":{"uuid":"task-001","nng_url":"ipc:///tmp/cyctron_task-001.sock"}}
```

插件侧通常用 SUB `dial` 该地址并订阅全部消息（Rust 示例见 `examples/soket_demo`）。

### 3.3 平台差异

- Unix：`ipc:///tmp/...sock`。
- Windows：NNG IPC 使用命名管道，建议使用 `ipc://cyctron_<uuid>`；
  当前实现仍拼 `/tmp` 路径，跨平台时需要调整。

## 4. 生命周期

```
bind
  ├─ rt_create_task：分配节点、生成 task_id、socket_id
  ├─ rt_start_task：nng_pub0_open + nng_listen + 调插件 process_init
  └─ 状态 RUNNING，推送 realtime/bind 事件

remove
  ├─ 从任务表摘除
  ├─ nng_close
  └─ 调 on_cleanup / 释放节点

WebSocket 关闭
  └─ rt_cleanup：销毁所有任务（当前行为）
```

## 5. 事件推送

```c
event_push("{\"type\":\"pong\"}");
event_push_with_type("realtime", "{\"action\":\"bind\",\"uuid\":\"task-001\"}");
event_push_json_string("msg", "hello");
```

`event_push` 同时：

1. 向当前 WebSocket 连接发送；
2. 向全局 NNG PUB 发布。

## 6. 已知限制

- `rt_create_task` 忽略调用方传入的 `socket_path`，自行生成
  `/tmp/cyctron_<uuid>.sock`；与 `handler_ws.c` 传入的路径不一致。
- `rt_run`（心跳/超时）没有调用点，`heartbeat_timeout`、`max_restarts` 未生效。
- `rt_get_task_count`、`rt_find_task_by_uuid` 未加锁。
- 任务表释放后指针失效，调用方需避免持有过期 `rt_task_t*`。
- PUB/SUB 是尽力而为：突发流量下可能丢消息（压测中观察到高丢失率）。
- Rust `soket_demo` 的 runtime 是进程级单例，同一插件无法运行多个 uuid。
- 插件目前只保证导出 `process_init`，暂停/关闭/状态查询尚未接入。

## 7. 调试

查看服务日志：

```
[Scheduler] Created: task_... (uuid: ..., socket: /tmp/cyctron_....sock)
[Scheduler] NNG listening on ipc:///tmp/cyctron_....sock
[Scheduler] Started: ...
```

订阅全局事件：

```bash
# 使用 nngcat（如果构建了 nng 工具）
./build-sse/crates/nng/tools/nngcat/nngcat.exe --sub --dial ipc:///tmp/cyctron.sock
```

或在 Rust 中使用 `nng_sub0_open` + `nng_dial` + 订阅空前缀。

## 8. 测试

- `tests/test_nng_ipc.c`：PUB/SUB 演示（无限循环，不注册 ctest）。
- `socket` 分支提供 `tests/test_nng_stress.c`：
  PAIR1 可靠通道 + PUB/SUB 尽力而为吞吐测试，结果写入 `test-results/stress.jsonl`。
- `tests/test_realtime.c` 等旧测试使用已移除的 API，尚未迁移。
