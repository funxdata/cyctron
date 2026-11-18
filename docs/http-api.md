# HTTP 路由

默认监听：`http://0.0.0.0:44944`（可用 `start -port <port>` 修改）。

cyctron 的核心职责是**调用动态库**：

| 场景 | 通道 |
| --- | --- |
| 快速同步调用 | HTTP `/<name>` |
| 需要等待/持续产出 | SSE `/sse` |
| 需要双向确认 | WebSocket `/soket` |

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
| GET/HEAD | 其它带后缀 URI、`/assets/*` | 静态文件（`web/`） |
| 任意 | `/<name>` | 若 `./libary/<name>.<ext>` 存在 → 插件 |
| 任意 | 其它 | 回退到插件 handler（找不到插件返回 404） |

## 2. 静态文件

- `/`、`/index.html`：优先读取磁盘 `web/index.html`，文件不存在时才返回内置首页。
- 带后缀 URI 映射到 `web/<uri>`，例如 `/assets/js/app.js` → `web/assets/js/app.js`；
  文件不存在返回 404。
- `/assets` 前缀始终按静态资源处理，不参与插件探测，也不做 SPA fallback。
- 无后缀路径先尝试 libary 插件；插件不存在时再按静态页面处理：
  - 仅当请求 `Accept` 含 `text/html`，且 `web/<uri>/index.html` 不存在时，
    回退到首页（SPA fallback）；
  - 其他无后缀请求返回 404。
- 只处理 `GET`/`HEAD`。
- 含 `..` 的 URI 返回 400。
- MIME 由 mongoose 内置表 + 额外映射（`svg`、`json`、`js`、`mjs`）处理。

## 3. 插件

`/<name>` 从 `./libary/<name>.<ext>` 加载（兼容 `lib<name>.<ext>`）：

- Windows：`.dll`
- Linux：`.so`
- macOS：`.dylib`

请求方法与 `FFI-Symbol` 决定导出函数，详见 [libary.md](./libary.md)。

## 4. 错误格式

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

## 5. 请求大小与插件输出

- 请求体上限由 mongoose 的默认接收缓冲决定。
- 插件输出缓冲初始 1 MB；ABI v2 下插件返回 `-1001`（ENOSPC）时自动扩容至 16 MB 重试。
