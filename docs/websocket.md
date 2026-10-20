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
{"cmd":"bind","uuid":"task-001","lib":"libsoket_demo"}
```

- `uuid` 必填，任务唯一标识。
- `lib` 可选，默认 `libsoket_demo`。
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

## 4. 事件格式

服务端推送的主要类型：

| type | data 示例 | 说明 |
| --- | --- | --- |
| `system` | `{"status":"online"}` | 连接状态 |
| `realtime` | `{"action":"bind","uuid":"task-001","status":"success"}` | 任务生命周期 |
| `msg` | `"hello world"` | 消息 |
| `echo` | `{"message":"...","len":11}` | 非 JSON 回显 |
| `error` | `{"message":"..."}` | 命令错误 |
| `log` / `iceoryx` / `http_api` | 自定义 | API 推送 |

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
  ws.send(JSON.stringify({ cmd: "bind", uuid: "task-001", lib: "libsoket_demo" }));
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

- 单连接：多客户端会互相覆盖。
- 任一连接关闭会触发 `rt_cleanup`，销毁全部任务。
- `msg` 未定向到具体任务。
- JSON 解析基于字符串匹配，不支持空格、转义、嵌套。
- 心跳/超时逻辑（`rt_run`）尚未接入主循环。
- 断线重连不会自动恢复任务。

优化计划见 `todo/socket.md`（`socket` 分支）。
