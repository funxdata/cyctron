# SSE 事件流

SSE（Server-Sent Events）用于向浏览器/客户端持续推送事件。
事件源是一个 libary 插件（`sse_demo`），由 C 端在主循环中按间隔拉取。

## 1. 端点

```
GET /sse[?interval_ms=200]
Accept: text/event-stream
Last-Event-ID: <可选>
```

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `interval_ms` | 200 | 拉取间隔，限制 50–5000 ms |
| `Last-Event-ID` | 0 | 断线重连时从该 id 之后继续 |

响应头：

```
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
X-Accel-Buffering: no
Access-Control-Allow-Origin: *
```

## 2. 事件格式

```
id: 12
event: tick
data: {"seq":12,"ts":1760000000,"message":"sse tick #12"}

```

- `id`：单调递增整数。
- `event`：事件类型，默认 `tick`（由插件配置）。
- `data`：单行 JSON。
- 空行表示事件结束。
- 空闲时每 15 秒发送注释行 `: ping`。

## 3. 重连

客户端断线后携带上一次的 `Last-Event-ID` 重新连接：

```js
const es = new EventSource("/sse");
es.onmessage = (e) => { /* ... */ };
```

服务端会把该值作为 `after_id` 传给插件，插件返回更大的 id。

> 说明：id 在单进程/插件生命周期内单调递增；服务重启后从插件的初始状态重新开始。

## 4. 插件协议

插件文件：`./libary/sse_demo.<ext>`（Windows `.dll`）。

| 符号 | 入参 | 输出 |
| --- | --- | --- |
| `process_init` | `{"config":{"interval_ms":200,"event":"tick"}}` | `{"ok":true,"interval_ms":200,"event":"tick"}` |
| `sse_next` | `{"after_id":N}` | `{"id":N+1,"event":"tick","data":{...}}` |
| `sse_status` | `{}` | `{"ok":true,"seq":N,...}` |
| `sse_reset` | `{}` | `{"ok":true,"seq":0}` |

C 端只调用 `sse_next`，并从输出中取 `id`、`event`、`data`。

输出约束：

- 紧凑 JSON，字段顺序固定为 `id,event,data`。
- 单事件 `data` 超过 4 KB 会被截断（C 端缓冲）。
- 返回值 `0` 表示成功；非 0 视为失败，记录日志。

## 5. 服务端实现

`src/handler_sse.c`：

- 最多 32 个并发客户端（`SSE_MAX_CLIENTS`）。
- 每个客户端维护 `last_id`、`last_poll_ms`、`last_ping_ms`、`interval_ms`。
- 连接注册后立即推送一条事件。
- 在 `MG_EV_POLL` 中拉取事件和发送心跳。
- 所有发送都在 mongoose 主循环线程内完成。
- 客户端断开时在 `MG_EV_CLOSE` 中清理槽位。

路由接入在 `src/handler.c`：

```c
if (hm->uri.len == 4 && strncmp(hm->uri.buf, "/sse", 4) == 0) {
    ev_handler_sse(c, ev, ev_data);
    return;
}
```

## 6. 错误响应

| 场景 | 状态 | 响应 |
| --- | --- | --- |
| 非 GET | 405 | `{"error":405,"message":"Method Not Allowed"}` |
| 插件不存在 | 503 | `{"error":503,"message":"SSE plugin unavailable"}` |
| 客户端超限 | 503 | `{"error":503,"message":"Too many SSE clients"}` |

## 7. 使用示例

```bash
# 终端观察事件
curl -N http://127.0.0.1:44944/sse?interval_ms=100

# 带续传
curl -N -H "Last-Event-ID: 5" http://127.0.0.1:44944/sse
```

Deno/浏览器：

```ts
const res = await fetch("http://127.0.0.1:44944/sse?interval_ms=100", {
  headers: { Accept: "text/event-stream" },
});
const reader = res.body!.getReader();
// 按 \n\n 分块解析 id/event/data
```

## 8. 限制

- 插件调用在主循环同步执行；插件慢会拖慢其他 HTTP 请求。
- SSE 连接占用 mongoose 连接，最多 32 个。
- 不支持二进制事件，`data` 必须是文本。
- 代理/网关需要禁用缓冲（已发送 `X-Accel-Buffering: no`）。

## 9. 测试

```bash
# C 单元测试
ctest --test-dir build -R "^sse$" --output-on-failure

# 接口测试（先启动服务）
./cyctron start
deno run --allow-net ts/sse_request.ts 44944
```

覆盖：响应头、事件数量、id 递增、data JSON、`Last-Event-ID` 续传、POST 405。
