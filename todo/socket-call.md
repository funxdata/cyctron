# TODO: WebSocket `call` 细化任务与单元测试

> 状态：**1.0.8 已落地基础 `call`；本文件把 F-05 的剩余需求拆成可独立验收的小任务。**
> 关联：`docs/websocket.md` 3.5、`todo/socket.md`、`feature/005-chat-ui-socket.md`（dytes_sys_operations）。
> 约定：每个小功能点都必须有对应单元测试；测试优先纯函数，避免依赖真实网络与插件。

---

## 0. 已完成（1.0.8）

| 项 | 实现 | 测试现状 |
| --- | --- | --- |
| `call` 命令 | `src/handler_ws.c`：解析 `id/lib/symbol/args`，白名单，`call_local_dyn_libffi`，回包 | 手工 WebSocket 脚本；无 C 单测 |
| 动态 WS 消息缓冲 | `MG_EV_WS_MSG` 按 `msg->data.len` 分配 | 无 |
| 动态事件缓冲 | `src/event_pusher.c` 三个 push 函数动态拼接 | 无 |
| SPA fallback | `src/handler_static.c` 无后缀路径回退 `web/index.html` | 无 |
| `elapsed_ms` | `GetTickCount64` / `clock_gettime` | 无 |

> 结论：功能可用，但**测试与可测性不足**。下面的任务以“先抽纯函数，再补测试”为主线。

---

## 1. 任务总表

| ID | 功能点 | 优先级 | 主要文件 | 单元测试 |
| --- | --- | --- | --- | --- |
| SC-00 | 抽出 `ws_call` 纯函数模块 | P0 | `src/ws_call.c`、`include/ws_call.h` | `tests/test_ws_call.c` |
| SC-01 | `call` 参数校验补强 | P0 | `src/ws_call.c` | `tests/test_ws_call_parse.c` |
| SC-02 | 错误码细分与统一 | P0 | `src/ws_call.c`、`src/calldyn.c` | `tests/test_ws_call_errors.c` |
| SC-03 | 响应 JSON 组装与转义 | P0 | `src/ws_call.c` | `tests/test_ws_call_response.c` |
| SC-04 | WS 消息大小上限与 OOM 防护 | P1 | `src/handler_ws.c` | `tests/test_ws_message_limit.c` |
| SC-05 | 动态事件缓冲压力与释放 | P1 | `src/event_pusher.c` | `tests/test_event_push_dynamic.c` |
| SC-06 | SPA fallback 安全与缓存 | P1 | `src/handler_static.c` | `tests/test_static_spa.c` |
| SC-07 | 旧命令统一 JSON 解析 | P1 | `src/handler_ws.c` | `tests/test_ws_parse_legacy.c` |
| SC-08 | 多连接与任务归属 | P2 | `src/handler_ws.c`、`src/realtime.c` | `tests/test_ws_multi_conn.c` |
| SC-09 | 断线只清理本连接任务 | P2 | `src/handler_ws.c` | `tests/test_ws_close_ownership.c` |
| SC-10 | FFI 线程化与结果队列 | P2 | `src/handler_ws.c`、新 `src/ws_worker.c` | `tests/test_ws_worker.c` |
| SC-11 | 服务端超时与取消 | P2 | `src/ws_worker.c` | `tests/test_ws_call_timeout.c` |
| SC-12 | 并发策略与队列上限 | P2 | `src/ws_worker.c` | `tests/test_ws_call_concurrency.c` |
| SC-13 | 指标与慢调用日志 | P3 | `src/handler_ws.c` | `tests/test_ws_call_metrics.c` |
| SC-14 | 测试接入 ctest | P0 | `tests/CMakeLists.txt` | — |
| SC-15 | 文档与示例更新 | P3 | `docs/websocket.md`、`ts/` | `ts/socket_call.ts` |

---

## 2. 任务详情

### SC-00 抽出 `ws_call` 纯函数模块（P0）

**目标**：把解析、校验、响应组装从 `handle_ws_message` 中剥离，形成可单测的纯函数。

**新增文件**：

- `include/ws_call.h`
- `src/ws_call.c`

**建议 API**：

```c
typedef struct {
    char *id;        // 调用方释放
    char *lib;
    char *symbol;
    char *args;      // 原始 JSON 文本，缺省 "{}"
    long timeout_ms;
} ws_call_req_t;

typedef struct {
    char *json;      // 完整 {"type":"call",...}
    int code;        // 0 / -1 / -2 / -4 / -5
} ws_call_resp_t;

int ws_call_parse(const char *payload, ws_call_req_t *out, char *err, size_t err_size);
int ws_call_build_ok(const char *id, const char *lib, const char *symbol,
                     long elapsed_ms, const char *plugin_out, ws_call_resp_t *out);
int ws_call_build_error(const char *id, int code, const char *message,
                        ws_call_resp_t *out);
void ws_call_req_free(ws_call_req_t *req);
void ws_call_resp_free(ws_call_resp_t *resp);
```

**改动**：`handler_ws.c` 的 `handle_call` 只负责“解析 → 调用 FFI → 组装 → event_push”。

**测试**：`tests/test_ws_call.c`

- 解析合法请求；
- 缺 id/lib/symbol；
- 非法 JSON；
- `args` 缺省；
- 释放函数不崩溃、无泄漏（配合 ASan 可选）。

**验收**：`handle_call` 行数显著下降；纯函数覆盖上述用例。

---

### SC-01 `call` 参数校验补强（P0）

**目标**：避免异常输入导致崩溃或资源浪费。

**改动**：

- `id` 长度上限（建议 128），超长返回 `-1`；
- `lib` / `symbol` 长度上限（建议 128）；
- `lib` / `symbol` 白名单（已有）：`[A-Za-z0-9_-]`；
- `args` 必须是 JSON 对象或 `null`（拒绝数组/字符串/数字，避免插件解析失败）；
- 请求体大小上限（建议 1 MB，见 SC-04）；
- 重复 `id` 的处理策略（同连接内：拒绝并返回 `-1`，或允许但记录告警）。

**测试**：`tests/test_ws_call_parse.c`

| 用例 | 输入 | 期望 |
| --- | --- | --- |
| 正常 | 完整字段 | `0` |
| 缺 id | 无 id | `-1` |
| 缺 lib | 无 lib | `-1` |
| 缺 symbol | 无 symbol | `-1` |
| id 超长 | 129 字符 | `-1` |
| lib 含 `/` | `../x` | `-5` |
| symbol 含空格 | `post chat` | `-5` |
| args 数组 | `[]` | `-1` |
| args 缺省 | 无 args | `0`，args=`{}` |
| 非法 JSON | `{oops` | `-1` |

**验收**：全部用例通过；无未释放内存。

---

### SC-02 错误码细分与统一（P0）

**目标**：区分“库不存在 / 符号不存在 / 调用失败 / 输出超限”，便于前端提示与排查。

**改动**：

- `calldyn` 增加细分返回（或 `ws_call` 在调用前 `dlopen`/`dlsym` 检查）；
- 映射表：

| 场景 | code |
| --- | --- |
| 参数错误 | -1 |
| 库不存在 / 加载失败 | -2 |
| 符号不存在 | -3 |
| 调用失败 / 插件返回非 0 | -4 |
| 输出超限 | -4（附 `error`） |
| 安全拒绝 | -5 |
| 锁忙 / 超时 | -6 / -7（后续） |

**测试**：`tests/test_ws_call_errors.c`

- 不存在的库；
- 不存在的符号；
- 插件返回错误；
- 输出超过 64 KB；
- 非法 lib / symbol。

**验收**：每个场景返回稳定错误码与 `error` 文本。

---

### SC-03 响应 JSON 组装与转义（P0）

**目标**：保证 id / error / result 的 JSON 合法性。

**改动**：

- `id` 使用 `event_escape_json`；
- `result` 原样嵌入（插件输出必须为合法 JSON；否则用 `null` 并返回 `-4`）；
- 响应字段固定：`type/id/ok/code/lib/symbol/elapsed_ms/result/timestamp`；
- 失败响应固定：`type/id/ok/code/error/timestamp`。

**测试**：`tests/test_ws_call_response.c`

- id 含引号/反斜杠/中文；
- result 为对象/数组/null/字符串；
- 超长 result；
- 失败响应结构。

**验收**：所有响应可被 `mg_json_parse` 解析。

---

### SC-04 WS 消息大小上限与 OOM 防护（P1）

**目标**：防止超大 WS 消息拖垮服务。

**改动**：

- 定义 `WS_MAX_MESSAGE_BYTES`（建议 1 MB）；
- 超过上限：关闭连接或返回 `error`；
- `malloc` 失败：记录日志并关闭连接，不继续处理。

**测试**：`tests/test_ws_message_limit.c`

- 1 B / 2 KB / 1 MB / 1 MB+1；
- 非法长度（0）；
- 分配失败路径（可用注入）。

**验收**：边界值行为稳定，无崩溃。

---

### SC-05 动态事件缓冲压力与释放（P1）

**目标**：验证 `event_push_with_type` / `event_push_message` / `event_push_json_string` 的动态分配。

**改动**：

- 统一使用 `event_escape_alloc`；
- 所有路径 `free`；
- 可选的推送计数与丢弃计数。

**测试**：`tests/test_event_push_dynamic.c`

| 用例 | 数据 | 期望 |
| --- | --- | --- |
| 小消息 | 10 B | 正常 |
| 2 KB 边界 | 2048 B | 不截断 |
| 64 KB | 64 KB | 不截断 |
| 1 MB | 1 MB | 不截断 |
| 转义 | 引号/换行/中文 | JSON 合法 |
| NUL 字符 | 含 `\0` | 按字符串结束处理 |

**验收**：输出长度与输入匹配；无泄漏（可选 ASan）。

---

### SC-06 SPA fallback 安全与缓存（P1）

**目标**：保证 fallback 只作用于页面路由，不影响资源与安全。

**改动**：

- fallback 仅对无后缀、非 `/api/*`、非 `/soket`、非 `/sse` 生效；
- 保留 `..` 拒绝；
- 为 `index.html` 增加 `Cache-Control: no-cache`，避免更新后旧页面；
- 资源文件（`.js/.css`）仍返回 404，不 fallback。

**测试**：`tests/test_static_spa.c`

- `/chat` → `web/index.html`；
- `/chat?uid=...` → `web/index.html`；
- `/chat_ui/chat.js` → 文件；
- `/missing.js` → 404；
- `/../secret` → 400；
- `/api/status` → 交给 API 处理。

**验收**：路由与资源行为分离；无路径穿越。

---

### SC-07 旧命令统一 JSON 解析（P1）

**目标**：`bind` / `msg` / `status` / `remove` 也改用 `mg_json_*`，支持空格、转义、嵌套。

**改动**：`handler_ws.c` 的 `extract_field` 逐步替换。

**测试**：`tests/test_ws_parse_legacy.c`

- `{"cmd": "bind", "uuid": "u1", "lib": "l1"}`（含空格）；
- `msg` 的 `data` 含引号/换行；
- 缺 `cmd`；
- 未知命令。

**验收**：旧命令行为不变，解析更稳健。

---

### SC-08 多连接与任务归属（P2）

**目标**：支持多个 WebSocket 连接，各自持有任务。

**改动**：

- `g_ws_conn` → 连接列表/连接 ID；
- `rt_task_t` 增加 `conn_id`；
- 推送按连接定向；
- 关闭只清理本连接任务。

**测试**：`tests/test_ws_multi_conn.c`

- 两个连接各自 bind；
- 一个连接关闭，另一个任务仍在；
- 推送只到目标连接。

**验收**：多窗口互不干扰。

---

### SC-09 断线只清理本连接任务（P2）

**目标**：落实 `todo/socket.md` 3.4。

**改动**：`MG_EV_CLOSE` 按 `conn_id` 清理。

**测试**：`tests/test_ws_close_ownership.c`

- 连接 A 关闭后，连接 B 的任务数与状态不变；
- 任务表无残留。

**验收**：断线不影响其他连接。

---

### SC-10 FFI 线程化与结果队列（P2）

**目标**：`call` 不再阻塞 mongoose 主循环。

**改动**：

- 新增 `src/ws_worker.c`：工作线程 + 请求队列 + 完成队列；
- 主循环 `mg_mgr_poll` 后 drain 完成队列并 `mg_ws_send`；
- 需要处理跨线程发送（mongoose 非线程安全）。

**测试**：`tests/test_ws_worker.c`

- 入队/出队；
- 主循环 drain；
- 任务完成顺序；
- 关闭时清理。

**验收**：长调用期间 `status` 仍能及时响应。

---

### SC-11 服务端超时与取消（P2）

**目标**：`timeout_ms` 生效；超时返回 `-7`。

**改动**：

- 工作线程记录开始时间；
- 超时后标记取消（FFI 本身不可中断，返回 `timed_out_stage=exec`）；
- 响应带 `timed_out` 字段。

**测试**：`tests/test_ws_call_timeout.c`

- 超时前完成；
- 超时后返回 `-7`；
- 取消与结果竞争。

**验收**：超时语义明确，不悬挂。

---

### SC-12 并发策略与队列上限（P2）

**目标**：同连接串行、跨连接并行；队列满时拒绝。

**改动**：

- 每连接请求队列；
- `WS_MAX_PENDING_CALLS`（建议 16）；
- 队满返回 `-6`（busy）。

**测试**：`tests/test_ws_call_concurrency.c`

- 同连接并发 10 个 call，按序完成；
- 跨连接并行；
- 队列满拒绝；
- 取消后队列释放。

**验收**：并发行为可预测，无死锁。

---

### SC-13 指标与慢调用日志（P3）

**目标**：可观测性。

**改动**：

- 统计：总调用、成功、失败、超时、busy、平均/最大耗时；
- `status` 响应附带统计；
- 超过阈值（如 5s）记录 warn。

**测试**：`tests/test_ws_call_metrics.c`

- 计数守恒；
- 最大耗时不小于平均；
- 慢调用触发日志（可用注入计时）。

**验收**：`status` 可读，计数准确。

---

### SC-14 测试接入 ctest（P0）

**目标**：新增测试可在 Windows/Linux 构建并注册。

**改动**：`tests/CMakeLists.txt`

- 新增纯函数测试（无插件依赖）：`test_ws_call`、`test_ws_call_parse`、`test_ws_call_errors`、`test_ws_call_response`、`test_event_push_dynamic`、`test_static_spa`（Windows 也可）；
- 需要插件的集成测试（`test_ws_worker`、`test_ws_call_timeout`、`test_ws_call_concurrency`）按插件存在与否注册；
- 设置 `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`、`TIMEOUT`；
- 结果写入 `test-results/`（与现有压力测试一致）。

**验收**：

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
mingw32-make -C build test
# 或
ctest --test-dir build --output-on-failure
```

所有新测试通过或按条件跳过。

---

### SC-15 文档与示例更新（P3）

- `docs/websocket.md`：已补 `call`；后续补错误码细分、超时、并发策略；
- `ts/socket_call.ts`：单调用断言；
- `ts/socket_call_error.ts`：错误码断言；
- `ts/socket_call_all.ts`：统一入口。

**测试**：上述 TS 脚本连接真实 cyctron 运行。

**验收**：文档与实现一致；TS 脚本可重复执行。

---

## 3. 测试文件规划

| 文件 | 类型 | 依赖 | 说明 |
| --- | --- | --- | --- |
| `tests/test_ws_call.c` | 单元 | 无 | 纯函数解析/组装/释放 |
| `tests/test_ws_call_parse.c` | 单元 | 无 | 参数校验矩阵 |
| `tests/test_ws_call_errors.c` | 单元 | 可选插件 | 错误码 |
| `tests/test_ws_call_response.c` | 单元 | 无 | JSON 结构与转义 |
| `tests/test_ws_message_limit.c` | 单元 | 无 | 消息大小边界 |
| `tests/test_event_push_dynamic.c` | 单元 | 无 | 动态缓冲 |
| `tests/test_static_spa.c` | 单元 | 无 | SPA fallback / 路径安全 |
| `tests/test_ws_parse_legacy.c` | 单元 | 无 | 旧命令解析 |
| `tests/test_ws_multi_conn.c` | 集成 | 网络 | 多连接 |
| `tests/test_ws_close_ownership.c` | 集成 | 网络 | 断线归属 |
| `tests/test_ws_worker.c` | 集成 | 插件 | 线程化 |
| `tests/test_ws_call_timeout.c` | 集成 | 插件 | 超时 |
| `tests/test_ws_call_concurrency.c` | 集成 | 插件 | 并发 |
| `tests/test_ws_call_metrics.c` | 单元 | 无 | 统计 |

---

## 4. 推荐实施顺序

1. **SC-00 + SC-01 + SC-02 + SC-03**：抽纯函数、补解析/错误码/响应测试（无需插件，Windows 可跑）；
2. **SC-14**：把上述测试接入 ctest；
3. **SC-04 + SC-05 + SC-06**：边界与缓冲测试；
4. **SC-07**：旧命令解析统一；
5. **SC-08 + SC-09**：多连接与断线归属；
6. **SC-10 + SC-11 + SC-12**：线程化、超时、并发；
7. **SC-13 + SC-15**：指标与文档。

---

## 5. 验收总则

- 每个任务合并前必须包含对应单元测试；
- 纯函数测试在 Windows/Linux 都可运行；
- 集成测试在插件缺失时跳过，不阻塞构建；
- `ctest --output-on-failure` 全绿；
- 文档与错误码表同步更新；
- 不引入新的全局状态泄漏（ASan 可选）。

---

## 0.1 已完成（cyctron 1.0.9，2026-10-24）

- **SC-00**：新增 `include/ws_call.h` / `src/ws_call.c`，`handler_ws.c` 的 `handle_call` 只负责调用链。
- **SC-01**：`id/lib/symbol` 长度上限 128；`lib/symbol` 白名单；`args` 仅允许对象或 `null`；payload 上限 1 MB。
- **SC-02**：`ws_call_map_rc` 映射 `call_local_dyn_libffi` 返回码：`-1` → `-2`（库加载）、`-2` → `-3`（符号）、其余 → `-4`（调用）。
- **SC-03**：`ws_call_build_ok` / `ws_call_build_error` 统一 JSON 结构与转义。
- **SC-14**：4 个测试接入 ctest：
  - `tests/test_ws_call.c`（15 项）
  - `tests/test_ws_call_parse.c`（11 项）
  - `tests/test_ws_call_errors.c`（16 项）
  - `tests/test_ws_call_response.c`（15 项）

验证：

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
ctest --test-dir build -R "ws_call" --output-on-failure
# 100% tests passed, 0 tests failed out of 4
```

- 发布：`core/cyctron/1.0.9/cyctron.exe`（2,300,676 bytes）。
- 行为变更：符号不存在由 `-2` 改为 `-3`；库加载失败为 `-2`；调用失败为 `-4`。
- 运行回归：`post_dialogue_list` 正常；非法符号返回 `-3`；非法 lib 返回 `-5`；业务错误仍在 `result.code`。

---

## 0.2 已完成（cyctron 1.0.10，2026-10-24）

- **SC-04**：新增 `include/ws_message.h` / `src/ws_message.c`；WS 消息上限 1 MiB；超限发送 `error` 后 graceful close；OOM 同样关闭连接。
- **SC-05**：新增 `include/event_format.h` / `src/event_format.c`；`event_escape_json` 与动态格式化函数从 `event_pusher.c` 抽出；2 KB / 64 KB / 1 MB 不截断。
- **SC-06**：新增 `static_should_fallback()`；仅页面路由回退；`/api/*`、`/soket`、`/sse`、带后缀资源不回退；`index.html` 增加 `Cache-Control: no-cache`。
- 新增 3 个测试（共 45 项）并接入 ctest：
  - `tests/test_ws_message_limit.c`（8）
  - `tests/test_event_push_dynamic.c`（23）
  - `tests/test_static_spa.c`（14）

验证：

```bash
ctest --test-dir build -R "ws_call|event_push_dynamic|ws_message_limit|static_spa" --output-on-failure
# 7/7 passed
```

- 发布：`core/cyctron/1.0.10/cyctron.exe`（2,306,019 bytes）。
- 运行回归：`/chat` 200 + `Cache-Control: no-cache`；`/missing.js` 404；`/chat_ui/chat.js` 200；超大 WS 消息收到 `{"type":"error","data":"message too large"}` 后连接关闭。

---

## 0.3 已完成（cyctron 1.0.11，2026-10-24）

- **SC-07**：新增 `include/ws_command.h` / `src/ws_command.c`；`bind` / `remove` / `status` / `msg` 统一使用 `mg_json_*` 解析，支持空格、转义、嵌套字段。
- `handler_ws.c` 删除 `extract_field`，`handle_bind` / `handle_remove` / `handle_msg` 改为接收 `ws_command_t`。
- 新增 `tests/test_ws_parse_legacy.c`（15 项）：空格、转义 data、缺 cmd、非字符串 cmd、缺 uuid、非法 JSON、超长 payload、`free(NULL)`。

验证：

```bash
ctest --test-dir build -R "ws_call|event_push_dynamic|ws_message_limit|static_spa|ws_parse_legacy" --output-on-failure
# 8/8 passed（117 项）
```

- 发布：`core/cyctron/1.0.11/cyctron.exe`（2,309,174 bytes）。
- 运行回归：带空格的 `{"cmd": "status"}` 正常；`msg` 的转义 data 正常；`call` 不受影响。

---

## 0.4 已完成（cyctron 1.0.12，2026-10-24）

- **SC-09**：`rt_task_t` 增加 `conn_id`；`handle_bind` 写入 `c->id`；新增 `rt_remove_tasks_by_conn()`；`MG_EV_CLOSE` 只删除本连接任务，不再 `rt_cleanup` 全部任务。
- **SC-08（基础）**：`handler_ws.c` 增加连接表（最多 8 个）；`event_pusher` 增加定向推送 `event_push_to` / `event_push_with_type_to` / `event_push_message_to` / `event_push_json_string_to`；`call` 响应、错误、生命周期事件、ping/status/echo 均只发给发起连接。
- 新增 `tests/test_realtime_ownership.c`（11 项）：按 conn 删除任务、其他连接任务保留、未知 conn 不误删、cleanup。
- 集成 TS 测试：
  - `ws_multi_conn.ts`：连接 A/B 各 bind 一个任务，关闭 A 后 `status` 从 2 变为 1（PASS）。
  - `ws_call_target.ts`：A 发起 `call`，A 收到 1 条、B 收到 0 条（PASS）。

验证：

```bash
ctest --test-dir build -R "ws_call|event_push_dynamic|ws_message_limit|static_spa|ws_parse_legacy|realtime_ownership" --output-on-failure
# 9/9 passed（128 项）
```

- 发布：`core/cyctron/1.0.12/cyctron.exe`（2,313,295 bytes）。
- 已知限制：连接表上限 8；超过后仍可通信但关闭时不会回退 `g_ws_conn`；全局 `event_push`（NNG 事件）仍发最后一个连接。

---

## 0.5 已完成（cyctron 1.0.13，2026-10-24）

- **SC-10**：新增 `include/ws_worker.h` / `src/ws_worker.c`；`call` 改为工作线程执行，主循环 `ws_drain_call_results()` 取完成队列并定向回包。
- **SC-11**：排队超时返回 `-7`（FFI 执行阶段不可中断，文档已注明）；慢调用（>5s）记录 warn。
- **SC-12**：2 个工作线程；同连接按 `conn_id` 哈希锁串行，跨连接并行；全局队列上限 64、每连接 pending 上限 16，超限返回 `-6`（worker busy）。
- **SC-13**：`ws_worker_stats()` 统计 total/ok/failed/timeouts/busy/total_us/max_us。
- 新增 `tests/test_ws_worker.c`（22 项）：同连接串行、跨连接并行、每连接上限、排队超时 -7、统计。
- 集成验证：
  - 5 个并发 `post_dialogue_list` 全部返回；
  - 慢 `post_chat` 执行期间 `status` 约 0.8s 返回（不阻塞）；
  - 两连接任务归属与定向推送仍通过。

验证：

```bash
ctest --test-dir build -R "ws_call|event_push_dynamic|ws_message_limit|static_spa|ws_parse_legacy|realtime_ownership|ws_worker" --output-on-failure
# 10/10 passed（150 项）
```

- 发布：`core/cyctron/1.0.13/cyctron.exe`（2,328,268 bytes）。
- 已知限制：FFI 执行阶段不可中断，超时只覆盖排队；连接锁按 `conn_id % 16` 哈希，冲突会串行；工作线程数固定 2。
