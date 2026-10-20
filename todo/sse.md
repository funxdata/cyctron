# TODO: SSE 事件流支持

> 状态：**实施中（先文档，后代码）**
> 目标：在现有 HTTP 服务中增加 SSE（Server-Sent Events）端点，
> 事件由 Rust 动态库通过 libffi 提供，配套 C 单元测试与 Deno 接口测试。

## 1. 需求记录

- [*] 1.1 新增 `todo/sse.md`，按既有 dev 流程整理需求、设计与测试
- [*] 1.2 创建 C 端 SSE 处理文件并接入路由
- [*] 1.3 创建 `tests/test_sse.c` 单元测试与测试说明
- [*] 1.4 创建 Rust 库 `examples/sse_demo`，提供适合 SSE 的事件生成接口
- [*] 1.5 编译 Rust 库并拷贝到 `./libary/`
- [*] 1.6 启动 cyctron，在 `ts/` 中新增 FFI/SSE 调用测试
- [*] 1.7 跑通 C 测试、TS 测试并记录结果

## 2. 现状与约束

| 事实 | 位置/说明 |
| --- | --- |
| HTTP 服务基于 mongoose，主循环 `mg_mgr_poll(&mgr, 50)` | `src/main.c:58-60` |
| 所有 HTTP 请求先进入 `ev_handler`，再按 URI/Content-Type 分发 | `src/handler.c:22-164` |
| 插件从 `./libary/<name>.<ext>` 通过 libffi 调用 | `src/handler_libary.c`、`src/calldyn.c` |
| libffi 输出缓冲区 64 KB，返回 `int` | `src/calldyn.c:31`（`PLUGIN_JSON_BUF_SIZE`） |
| mongoose 提供 `mg_json_get_long/str/tok` | `crates/mongoose/mongoose.h:3021-3032` |
| `mg_http_reply` 会写 `Content-Length`，不适合 SSE | `crates/mongoose/mongoose.c:2070-2086` |
| `MG_EV_POLL` 每个连接每轮 poll 都会触发 | `crates/mongoose/mongoose.h:1643` |
| 现有 WebSocket 通道为单连接 `g_ws_conn` | `src/handler_ws.c:11` |

约束：

- 不引入新的网络框架，继续使用 mongoose 的 HTTP 连接。
- SSE 事件源复用现有 libary 插件机制（libffi + `./libary/`）。
- SSE 长连接必须在主循环线程内发送，避免跨线程操作 mongoose 连接。

## 3. SSE 协议设计

### 3.1 端点

```
GET /sse[?interval_ms=200][&event=tick]
Accept: text/event-stream
Last-Event-ID: <可选，断线重连时携带>
```

响应头（手写，不使用 `mg_http_reply`）：

```
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
X-Accel-Buffering: no
Access-Control-Allow-Origin: *

```

### 3.2 事件格式

```
id: 12
event: tick
data: {"seq":12,"ts":1760000000,"message":"sse tick #12"}

```

- `id`：单调递增整数，用于 `Last-Event-ID` 续传。
- `event`：事件类型，默认 `tick`，可由查询参数指定。
- `data`：单行 JSON（插件返回的 `data` 字段原样输出）。
- 每个事件以空行结束；心跳使用注释行 `: ping`。

### 3.3 心跳与重连

- 每 15 秒发送一次 `: ping\n\n`，避免代理断开空闲连接。
- 客户端重连时带 `Last-Event-ID: N`，服务端从 `N+1` 开始拉取。
- 拉取间隔默认 200 ms，可通过 `?interval_ms=` 调整（限制 50–5000）。

### 3.4 错误响应

| 场景 | 响应 |
| --- | --- |
| 非 GET | 405 `{"error":405,"message":"Method Not Allowed"}` |
| 插件不存在/加载失败 | 503 `{"error":503,"message":"SSE plugin unavailable"}` |
| 连接数超限 | 503 `{"error":503,"message":"Too many SSE clients"}` |
| 事件拉取失败 | 已建立连接时发送 `event: error`，不中断流 |

## 4. C 端设计

### 4.1 文件

- `include/handler_sse.h`：路由处理、轮询/关闭钩子、格式化函数声明。
- `src/handler_sse.c`：连接表、SSE 头、事件拉取、心跳、格式化。

### 4.2 连接表

```c
#define SSE_MAX_CLIENTS 32
typedef struct {
    struct mg_connection *c;
    long last_id;          // 已发送的最后事件 id
    uint64_t last_poll_ms; // 上次拉取时间
    uint64_t last_ping_ms; // 上次心跳时间
    int interval_ms;       // 拉取间隔
    int active;
} sse_client_t;
```

- 表只在主循环线程访问，无需加锁。
- `MG_EV_HTTP_MSG`：校验方法、解析 `Last-Event-ID`/`interval_ms`、写响应头、注册。
- `MG_EV_POLL`：遍历表，按间隔调用插件、发送事件；按 15 s 发心跳。
- `MG_EV_CLOSE`：清理对应槽位。

### 4.3 插件调用

```c
int call_local_dyn_libffi("./libary/sse_demo.<ext>", "sse_next",
                          "{\"after_id\":N}", &json_out);
```

插件输出：

```json
{"id": 12, "event": "tick", "data": {"seq": 12, "ts": 1760000000}}
```

C 端解析：

- `mg_json_get_long(out, "$.id", 0)` → `id`
- `mg_json_get_str(out, "$.event")` → 事件名（失败则用 `message`）
- `mg_json_get_tok(out, "$.data")` → 原样作为 `data:` 内容

若 `id <= last_id` 或输出为空，本次不发送。

### 4.4 路由接入

在 `ev_handler` 的 `MG_EV_HTTP_MSG` 分支增加：

```c
if (hm->uri.len == 4 && strncmp(hm->uri.buf, "/sse", 4) == 0) {
    ev_handler_sse(c, ev, ev_data);
    return;
}
```

同时在 `MG_EV_POLL`、`MG_EV_CLOSE` 调用 `sse_on_poll(c)`、`sse_on_close(c)`。

### 4.5 格式化函数（可单测）

```c
int sse_format_event(char *out, size_t out_size, long id,
                     const char *event, const char *data);
```

输出包含 `id:`、`event:`、`data:` 与结尾空行；超长时返回 -1。

## 5. Rust 库设计

### 5.1 工程

```
examples/sse_demo/
  Cargo.toml        # crate-type = ["cdylib"], serde_json
  src/lib.rs
```

### 5.2 导出符号

| 符号 | 入参 | 输出 |
| --- | --- | --- |
| `process_init` | `{"config":{"interval_ms":200,"event":"tick"}}` | `{"ok":true,"interval_ms":200,"event":"tick"}` |
| `sse_next` | `{"after_id":N}` | `{"id":N+1,"event":"tick","data":{...}}` |
| `sse_status` | `{}` | `{"ok":true,"seq":N,"interval_ms":200,"event":"tick"}` |
| `sse_reset` | `{}` | `{"ok":true,"seq":0}` |

### 5.3 行为

- 全局状态用 `Mutex<State>` 保护：`seq: u64`、`interval_ms: u32`、`event: String`。
- `sse_next` 的 `id = max(after_id + 1, seq + 1)`，并更新 `seq`。
- `data` 至少包含 `seq`、`ts`（Unix 秒）、`message`。
- 输出固定为紧凑 JSON，字段顺序 `id,event,data`（手工 `format!`，避免 serde_json 重排）。
- 输出缓冲区 64 KB，超长截断并返回 -1。
- 空/非法 JSON 按默认参数处理。

### 5.4 编译与放置

```
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
copy examples/sse_demo/target/release/sse_demo.dll libary/   # Windows
cp    examples/sse_demo/target/release/libsse_demo.so libary/sse_demo.so  # Linux
```

`./libary/` 已被 `.gitignore` 忽略，产物不入库。

## 6. 文件清单

| 文件 | 类型 | 说明 |
| --- | --- | --- |
| `todo/sse.md` | 文档 | 本文件 |
| `include/handler_sse.h` | 新增 | SSE 路由/格式化声明 |
| `src/handler_sse.c` | 新增 | SSE 实现 |
| `src/handler.c` | 修改 | `/sse` 路由与 POLL/CLOSE 转发 |
| `CMakeLists.txt` | 修改 | 加入 `src/handler_sse.c` |
| `examples/sse_demo/Cargo.toml` | 新增 | Rust cdylib |
| `examples/sse_demo/src/lib.rs` | 新增 | SSE 事件生成 |
| `tests/test_sse.c` | 新增 | 插件调用与格式化单元测试 |
| `tests/CMakeLists.txt` | 修改 | 注册 `sse` 测试 |
| `ts/sse_request.ts` | 新增 | SSE 接口/重连测试 |

## 7. 实施阶段

### 阶段 1：文档与骨架

- [*] 7.1 完成本文件
- [*] 7.2 新增 `include/handler_sse.h`、`src/handler_sse.c`
- [*] 7.3 接入 `handler.c` 与 `CMakeLists.txt`

### 阶段 2：Rust 事件源

- [*] 7.4 创建 `examples/sse_demo`
- [*] 7.5 实现 `process_init`/`sse_next`/`sse_status`/`sse_reset`
- [*] 7.6 编译并拷贝到 `./libary/`

### 阶段 3：测试

- [*] 7.7 `tests/test_sse.c` + ctest
- [*] 7.8 `ts/sse_request.ts`
- [*] 7.9 构建、启动、跑通全部测试（ctest 4/4，TS 9/9）

## 8. 测试计划

### 8.1 C 单元测试 `tests/test_sse.c`

| 用例 | 断言 |
| --- | --- |
| `sse_format_event` | 输出含 `id:`/`event:`/`data:`，以空行结束 |
| `sse_format_event` 超长 | 返回 -1，不越界 |
| `process_init` | 返回 0，输出 `ok:true` |
| `sse_next` 连续调用 | id 单调递增，`event` 与 `data` 存在 |
| `sse_next` 带 `after_id` | 返回 id > after_id |
| `sse_status` | 返回当前 `seq` |
| 未知符号 | 返回 -2 |

### 8.2 TS 接口测试 `ts/sse_request.ts`

| 用例 | 断言 |
| --- | --- |
| 建立 SSE 连接 | 200 + `text/event-stream` |
| 接收事件 | 至少 2 条，`id` 递增，`event=tick` |
| `data` 解析 | 每条 `data` 可 JSON 解析且含 `seq` |
| 心跳 | 观察到 `: ping`（可选，等待较长） |
| 断线重连 | 带 `Last-Event-ID`，首条 id 大于上次 |
| 方法限制 | POST `/sse` 返回 405 |

### 8.3 运行方式

```
# C
mingw32-make -C build-sse test

# TS（先启动 cyctron）
./cyctron.exe start -port 44944
deno run --allow-net ts/sse_request.ts 44944
```

## 9. 验收标准

- [*] 9.1 `GET /sse` 返回 `text/event-stream`，持续收到 `tick` 事件
- [*] 9.2 事件 id 单调递增，`data` 是合法 JSON
- [*] 9.3 `Last-Event-ID` 可从指定位置续传
- [ ] 9.4 心跳与多客户端互不干扰（心跳 15s、多客户端场景待补测）
- [*] 9.5 Rust 库产物位于 `./libary/`，C 端可加载
- [*] 9.6 `tests/test_sse.c` 通过 ctest
- [*] 9.7 `ts/sse_request.ts` 全部断言通过
- [*] 9.8 Windows/MinGW 已验证（`cyctron.exe` + `sse_demo.dll`）；Linux 路径按同一逻辑生成，待 CI 验证

## 10. 风险与限制

- [ ] 10.1 SSE 长连接占用 mongoose 连接；限制最大客户端数（32）
- [ ] 10.2 事件拉取在主循环线程执行，插件调用过慢会阻塞其他请求；
  后续可将 FFI 调用移到工作线程 + 事件队列
- [ ] 10.3 代理可能缓冲 SSE，需 `X-Accel-Buffering: no` 与 `no-cache`
- [ ] 10.4 `Last-Event-ID` 只保证单进程内序号，重启后从插件状态重新开始
- [ ] 10.5 插件输出超过 64 KB 会被截断，需在 Rust 侧限制单事件大小
