# HTTP API

默认监听：`http://0.0.0.0:44944`（可用 `start -port <port>` 修改）。

所有响应默认带 CORS：

```
Access-Control-Allow-Origin: *
```

`OPTIONS` 预检返回 204，并允许：

```
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, FFI-Symbol, ffi-symbol
Access-Control-Max-Age: 86400
```

## 1. 路由总表

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| OPTIONS | `*` | CORS 预检 |
| GET | `/soket` | 升级为 WebSocket（见 [websocket.md](./websocket.md)） |
| GET | `/sse` | SSE 事件流（见 [sse.md](./sse.md)） |
| POST | `/api/push` | 推送一条事件到 WS/NNG |
| GET | `/api/status` | 服务状态 |
| POST | `/api/iceoryx` | 推送 iceoryx 类型事件 |
| POST | `/api/log` | 推送 log 类型事件 |
| GET/HEAD | 其它带后缀 URI | 静态文件（`web/`） |
| 任意 | `/<name>` | 若 `./libary/<name>.<ext>` 存在 → libary 插件 |
| 任意 | 其它 | 回退到 libary handler（找不到插件返回 404） |

## 2. 状态与推送 API

### 2.1 GET /api/status

响应：

```json
{
  "websocket": "connected",
  "push_count": 12,
  "status": "running"
}
```

- `websocket`：`connected` / `disconnected`（当前连接状态）。
- `push_count`：WebSocket 推送计数（不含 NNG）。

### 2.2 POST /api/push

请求体作为事件数据，转发到 WebSocket 与全局 NNG PUB。

```bash
curl -X POST http://127.0.0.1:44944/api/push \
  -d '{"message":"hello"}'
```

响应：

```json
{"success":true,"message":"Pushed to WebSocket"}
```

### 2.3 POST /api/iceoryx

请求体作为 `iceoryx` 类型事件推送，最大 2048 字节。

```bash
curl -X POST http://127.0.0.1:44944/api/iceoryx \
  -d '{"temperature":23.5}'
```

响应：

```json
{"success":true,"size":20}
```

### 2.4 POST /api/log

请求体作为 `log` 类型事件推送。

```json
{"level":"info","message":"Log message"}
```

响应：`{"success":true}`。

## 3. 静态文件

- 根路径 `/` 与无后缀 URI → `web/index.html`。
- 带后缀 URI → `web/<uri>`，例如 `/assets/js/app.js` → `web/assets/js/app.js`。
- 只处理 `GET`/`HEAD`。
- 含 `..` 的 URI 返回 400。
- MIME 由 mongoose 内置表 + 额外映射（`svg`、`json`、`js`、`mjs`）处理。

## 4. libary 插件

`/<name>` 对应 `./libary/<name>.<ext>`：

- Windows：`.dll`
- Linux：`.so`
- macOS：`.dylib`

请求方法与 `FFI-Symbol` 决定导出函数，详见 [libary.md](./libary.md)。

## 5. 错误格式

常见错误：

```json
{"error":404,"message":"Not Found"}
{"error":404,"message":"Plugin Not Found"}
{"error":400,"message":"Bad Request"}
{"error":405,"message":"Method Not Allowed"}
{"error":503,"message":"SSE plugin unavailable"}
```

说明：

- 静态文件 404 可能返回 mongoose 的纯文本 `Not found`。
- libary 插件调用失败（加载失败/符号不存在/返回非 0）统一返回 404 `Plugin Not Found`。
- SSE 的 405/503 返回 JSON。

## 6. 请求大小与头部限制

来自 `include/config.h`：

| 配置 | 默认值 |
| --- | --- |
| `CT_IO_SIZE` | 512 |
| `CT_MAX_RECV_SIZE` | 3 MB |
| `CT_MAX_HTTP_HEADERS` | 30 |
| `CT_PATH_MAX` | PATH_MAX 或 128 |

插件输出缓冲为 64 KB（`calldyn.c`）。
