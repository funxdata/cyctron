# 架构总览

## 1. 定位

cyctron 是一个单进程、本地运行的 HTTP + WebSocket 服务，核心能力是：

- 接收 HTTP 请求并交由动态库插件处理；
- 通过 WebSocket 接收实时任务命令；
- 通过 SSE 向浏览器持续推送事件；
- 使用 NNG IPC 在 C 进程与 Rust 插件之间传递事件/任务。

## 2. 进程与线程模型

```
主线程（唯一的事件循环）
  └─ mg_mgr_poll(&mgr, 50)          mongoose 事件循环，50ms 一轮
       ├─ HTTP 请求  → ev_handler
       ├─ MG_EV_POLL → SSE 轮询 / 心跳
       ├─ MG_EV_CLOSE→ SSE 连接清理
       └─ WebSocket 事件 → ws_ev_handler

插件线程（由插件自己创建）
  └─ Rust NNG SUB / 消息循环（如 soket_demo）

NNG 事件通道
  ├─ ipc:///tmp/cyctron.sock        全局事件 PUB（event_pusher）
  └─ ipc:///tmp/cyctron_<uuid>.sock 每任务事件 PUB（realtime）
```

关键约束：

- mongoose 连接对象只能在主线程操作；HTTP/SSE/WS 的发送都在主线程完成。
- 插件调用是同步 FFI：调用期间会阻塞主线程，插件实现应避免长时间占用。
- 当前 WebSocket 只维护一个客户端连接（`g_ws_conn`），多客户端会互相覆盖。

## 3. 目录与模块

```
include/                 对外头文件
  config.h               编译期配置宏
  global.h               平台路径（web/libary、扩展名）
  server.h               路由处理函数声明
  handler_ws.h           WebSocket 处理
  handler_sse.h          SSE 处理
  realtime.h             任务调度 API
  event_pusher.h         事件推送 API
  calldyn.h              动态库调用 API

src/
  main.c                 命令行入口、mongoose 主循环
  handler.c              HTTP/WS/SSE 总路由
  handler_static.c       静态文件
  handler_libary.c       libary 插件调用
  handler_ws.c           WebSocket 命令
  handler_sse.c          SSE 连接与事件
  calldyn.c              dlopen + libffi 调用（含句柄缓存）
  event_pusher.c         WS + NNG 事件推送
  realtime.c             任务表与 NNG PUB
  core/uri.c             URI 扩展名判断

crates/                  第三方子模块
  mongoose/              HTTP/WS/JSON/工具
  nng/                   NNG IPC
  libev/                 事件循环（构建依赖）
  log/                   日志
  libffi-packages/       Windows 静态 libffi
```

## 4. 请求数据流

### 4.1 HTTP → 插件

```
客户端
  │ POST /libary_demo  (Content-Type: application/libary, FFI-Symbol: add)
  ▼
ev_handler (handler.c)
  ├─ OPTIONS/WS/SSE/API 特殊路由
  ├─ 探测 ./libary/<uri>.<ext> 是否存在
  ▼
ev_handler_libary (handler_libary.c)
  ├─ 路径遍历检查
  ├─ 组装 json_in（query 或 body）
  ├─ 符号名 = <小写 method>_<FFI-Symbol|handle>
  ▼
call_local_dyn_libffi (calldyn.c)
  ├─ dlopen（句柄缓存）
  ├─ libffi 调用 int fn(const char *in, char *out)
  └─ 输出缓冲 64KB
  ▼
JSON 响应（200 或 404 Plugin Not Found）
```

### 4.2 SSE

```
GET /sse
  ▼
ev_handler_sse
  ├─ 校验 GET、插件 ./libary/sse_demo.<ext> 存在
  ├─ 写 text/event-stream 响应头
  └─ 注册连接（最多 32）
  ▼
MG_EV_POLL（每 50ms）
  ├─ 每 interval_ms 调 sse_next 拉取事件
  ├─ 发送 id/event/data
  └─ 每 15s 发送 ": ping"
```

### 4.3 WebSocket → realtime

```
ws://host:44944/soket
  ▼
ws_ev_handler
  ├─ bind   → rt_create_task + rt_start_task → 插件 process_init
  ├─ msg    → 事件广播（当前实现）
  ├─ status → 任务数
  └─ remove → rt_remove_task
  ▼
realtime.c
  ├─ 每任务 NNG PUB listen ipc:///tmp/cyctron_<uuid>.sock
  └─ 参数 {"config":{"uuid":...,"nng_url":...}}
```

## 5. 事件推送

`event_pusher.c` 提供：

- `event_push(data)`：向当前 WebSocket 客户端发送，并向全局 NNG PUB 发布；
- `event_push_with_type(type, data)`：包成 `{"type":...,"data":...,"timestamp":...}`；
- `event_push_json_string(type, str)`：把字符串作为 JSON 字符串值推送；
- `event_escape_json`：JSON 字符串转义。

全局 NNG PUB 监听 `ipc:///tmp/cyctron.sock`，供外部订阅者接收所有事件。

## 6. 已知限制

- WebSocket 单连接、断开时清理全部 realtime 任务。
- `rt_run`（心跳/超时）尚未接入主循环。
- PUB/SUB 为尽力而为语义；高突发流量下可能丢消息。
- 插件调用在主线程同步执行，慢插件会阻塞其他请求。
- `handler_ws.c` 的 JSON 提取是字符串匹配，不支持空格/转义/嵌套。
- `stop`/`reload` 子命令仍是 TODO。

更完整的优化计划见 `todo/socket.md`（`socket` 分支）。
