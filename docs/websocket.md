# WebSocket（/soket）

cyctron 提供一个 WebSocket 端点用于实时任务命令与事件推送。

## 1. 连接

```
ws://<host>:44944/soket
```

握手成功后服务端会发送：

```json
{"type":"system","data":{"status":"online"},"timestamp":...}
{"type":"message","message":"WebSocket client connected","timestamp":...}
```

当前实现只保存一个全局连接（`g_ws_conn`），后连接的客户端会覆盖前一个。

## 2. 文本命令

| 发送 | 响应 |
| --- | --- |
| `ping` | `{"type":"pong","timestamp":"now"}` |
| `status` | `{"type":"system","data":{"status":"running"}}` |
| 非 JSON 文本 | `{"type":"echo","data":{"message":"...","len":N}}` |

## 3. JSON 命令

统一使用 `cmd` 字段：

### 3.1 bind

```json
{"cmd":"bind","uuid":"task-001","lib":"soket_demo"}
```

- `uuid` 必填，任务唯一标识。
- `lib` 可选，默认 `soket_demo`。
- 服务端创建 realtime 任务、监听 NNG PUB，并调用插件的 `process_init`。
- 结果事件：

```json
{"type":"realtime","data":{"action":"bind","uuid":"task-001","status":"success"}}
```

### 3.2 msg

```json
{"cmd":"msg","uuid":"task-001","data":"hello world"}
```

当前实现把消息作为 `msg` 类型事件广播，尚未按 uuid 定向发送。

### 3.3 status

```json
{"cmd":"status"}
```

响应事件包含任务数：

```json
{"type":"realtime","data":{"action":"status","count":1}}
```

### 3.4 remove

```json
{"cmd":"remove","uuid":"task-001"}
```

移除任务、关闭对应 NNG 通道。

### 3.5 call

一次请求 → 一次 FFI 调用，按 `id` 返回结果。适用于 chatllm 等无状态能力库。

```json
{"id":"req-1","cmd":"call","lib":"chatllm","symbol":"post_chat",
 "args":{"prompt":"你好"},"timeout_ms":120000}
```

- `lib`：库名，仅允许 `[A-Za-z0-9_-]`；从 `libary/<lib>.<ext>` 加载。
- `symbol`：插件导出符号基名，如 `post_chat`；worker 实际调用 `post_chat_v2`。
- `args`：必须是完整 JSON 对象或 `null`，原样传给插件；缺省 `{}`。服务端会校验整个 payload 的 JSON 结构。
- `timeout_ms`：超时提示。仍在队列中时超过该值会直接返回 `-7`；插件开始执行后
  无法安全抢占，但执行完成后若耗时超过 `timeout_ms` 也会标记为 `-7`。

成功响应：

```json
{"type":"call","id":"req-1","ok":true,"code":0,
 "lib":"chatllm","symbol":"post_chat","elapsed_ms":578,
 "result":{"code":200,"data":{"content":"..."},"msg":"success"}}
```

调用层失败：

```json
{"type":"call","id":"req-1","ok":false,"code":-5,"error":"Invalid lib or symbol"}
```

| code | 含义 |
| --- | --- |
| 0 | 成功（业务状态看 `result.code`） |
| -1 | 缺少 id / lib / symbol |
| -2 | 库加载、符号查找或调用失败 |
| -4 | 内存分配失败 |
| -5 | lib / symbol 非法 |
| -6 | worker busy（队列满或单连接并发超限） |
| -7 | timeout（排队超时或执行耗时超过 `timeout_ms`） |
| -100 | 参数错误（lib / symbol / args） |

> 插件业务失败（如 `result.code = 400102`）仍是 `ok:true`，调用方需检查 `result.code`。
> `call` 由 worker 线程执行，结果通过主循环回推；同一连接的调用会串行执行。

## 4. 事件格式

服务端推送的主要类型：

| type | data 示例 | 说明 |
| --- | --- | --- |
| `system` | `{"status":"online"}` | 连接状态 |
| `realtime` | `{"action":"bind","uuid":"task-001","status":"success"}` | 任务生命周期 |
| `msg` | `"hello world"` | 消息 |
| `call` | `{"id":"req-1","ok":true,...}` | FFI 调用响应 |
| `echo` | `{"message":"...","len":11}` | 非 JSON 回显 |
| `error` | `{"message":"..."}` | 命令错误 |
| `log` / `iceoryx` / `http_api` | 自定义 | API 推送 |

> 事件 `data` 必须是合法 JSON 值；服务端会校验，非法数据会被丢弃并记录 WARN。

示例：

```json
{"type":"realtime","data":{"action":"bind","uuid":"task-001","status":"success"},"timestamp":1760000000}
```

## 5. 与 realtime/NNG 的关系

- `bind` → `rt_create_task` + `rt_start_task`。
- 每个任务监听 `ipc:///tmp/cyctron_<uuid>.sock`。
- 插件通过 `process_init` 收到：

```json
{"config":{"uuid":"task-001","nng_url":"ipc:///tmp/cyctron_task-001.sock"}}
```

- 任务状态：`IDLE/RUNNING/PAUSED/STOPPED/ERROR`。

详见 [realtime-nng.md](./realtime-nng.md)。

## 6. 客户端示例

```ts
const ws = new WebSocket("ws://127.0.0.1:44944/soket");

ws.onopen = () => {
  ws.send(JSON.stringify({ cmd: "bind", uuid: "task-001", lib: "soket_demo" }));
};

ws.onmessage = (e) => {
  const msg = JSON.parse(e.data);
  console.log(msg.type, msg.data);
};
```

仓库中的示例：

- `ts/socket_init.ts`：连接与发送
- `ts/socket_dyn.ts`：bind/msg/status/remove
- `ts/demo_socket.ts`：交互式测试工具

## 7. 限制与已知问题

- 最多 32 个 WS 连接；全局事件推送到最近一个 `g_ws_conn`。
- 连接关闭只清理该连接的任务（`rt_remove_tasks_by_conn`）。
- `msg` 未定向到具体任务。
- 事件推送使用动态缓冲，不再受 2048 字节限制。
- `call` 由 worker 线程执行，同一连接的调用串行；长调用不会阻塞主循环。
- `rt_run` 保留空实现，心跳/超时循环未启用。
- 断线重连不会自动恢复任务。

优化计划见 `todo/socket.md`。
