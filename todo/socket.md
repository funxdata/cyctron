# TODO: soket 实时通道优化

> 状态：**准备阶段（仅需求拆解、设计与任务清单，未开始改代码）**
> 决策：继续使用 **NNG + IPC**，不引入其他消息框架。
> 本轮目标：把“任务锁 + 请求-调用模型 + 测试”拆解成可执行的文档。

## 1. 需求记录

### 1.1 本轮确认的需求

- [*] 1.1 继续使用 `ipc://` + NNG（PUB/SUB）作为实时通道，不更换框架。
- [*] 1.2 增加任务锁：控制同一任务、同一动态库上的 FFI 调用并发。
- [*] 1.3 建立“一次 socket 请求 → 一次或多次 FFI 调用”的请求-响应模型。
- [*] 1.4 支持一个请求内调用多个 FFI；每次调用可独立控制锁、超时、参数与结果。
- [*] 1.5 在 `tests/` 中补充锁、批量调用、并发、超时、错误隔离等单元测试。
- [*] 1.6 在 `ts/` 中补充对应的 soket 测试文件与统一断言入口。
- [*] 1.7 先完成拆解与分析，全部工作项记录到本文件（本轮）。

### 1.2 待确认

- [ ] 1.8 批量调用默认串行还是允许并发（本文默认 `concurrency = 1` 串行）。
- [ ] 1.9 锁等待超时后的默认值（本文默认 `2000 ms`）。
- [ ] 1.10 同一插件多实例（多个 uuid）是否必须支持（影响 Rust runtime 改造）。
- [ ] 1.11 旧 `cmd: bind/msg/status/remove` 需要兼容多久（本文建议保留一个版本）。

## 2. 现状链路

```
浏览器 / Deno
  │  ws://host:44944/soket
  ▼
src/handler.c                 /soket → mg_ws_upgrade，记录 g_ws_conn
  ▼
src/handler_ws.c              解析 cmd：bind / remove / status / msg / ping
  ├─ bind   → src/realtime.c  rt_create_task + rt_start_task
  │            └─ libffi 调插件 process_init
  │            └─ 每任务 NNG PUB listen ipc:///tmp/cyctron_<uuid>.sock
  ├─ msg    → event_pusher.c  广播（未定向到具体任务）
  └─ remove → rt_remove_task
  ▼
examples/soket_demo (Rust)   NNG SUB dial → message_loop → process_data → println
  ▼
src/event_pusher.c           WS 单连接 + 全局 NNG PUB ipc:///tmp/cyctron.sock
```

### 2.1 关键文件

| 文件 | 职责 |
| --- | --- |
| `src/handler.c` | HTTP/WS 总路由，`/soket` 升级并记录连接 |
| `src/handler_ws.c` | WS 命令解析、bind/remove/status/msg 处理 |
| `src/realtime.c` | 任务表、NNG PUB、插件 `process_init` 调用 |
| `src/event_pusher.c` | WS 推送、全局 NNG PUB、JSON 转义 |
| `src/calldyn.c` | `dlopen` + libffi 调用插件 |
| `include/realtime.h` | 任务结构与调度 API |
| `examples/soket_demo` | Rust 侧 runtime、NNG SUB、消息处理 |
| `ts/socket_*.ts` | 浏览器/Deno 侧 WS 客户端与交互测试 |

### 2.2 已核实的代码事实

| 事实 | 位置 |
| --- | --- |
| `rt_create_task` 忽略传入的 `socket_path`，自行生成 `/tmp/cyctron_<uuid>.sock` | `src/realtime.c:86-88,115-117` |
| `handler_ws.c` 传入的却是 `/tmp/cyctron/<uuid>.sock` | `src/handler_ws.c:58-65` |
| `handle_msg` 只广播，从未调用 `rt_send_msg` | `src/handler_ws.c:103-120` |
| `MG_EV_CLOSE` 直接 `rt_cleanup`，清空全部任务 | `src/handler_ws.c:206-218` |
| `g_ws_conn` 是单个全局连接指针 | `src/handler_ws.c:11`、`src/event_pusher.c:74-86` |
| `rt_run` 已实现但项目内无任何调用点 | `src/realtime.c:297`，`src/main.c` 只有 `mg_mgr_poll` |
| `rt_init` 只在 WS 打开/首次 bind 时调用 | `src/handler_ws.c:61-63,188` |
| `event_pusher_cleanup` 没有退出路径调用 | `src/event_pusher.c:63` |
| `nng_sendmsg` 返回值未处理 | `src/event_pusher.c:47` |
| 无扩展名时固定拼 `./core/lib%s.so`，Windows 也如此 | `src/calldyn.c:146-152` |
| `/api/log` 用 `len >= 10` 比较 8 字节前缀 | `src/handler.c:105` |
| 当前没有任务级或库级调用锁 | `src/realtime.c`、`src/calldyn.c` |
| `soket_demo` 只导出 `process_init` | `examples/soket_demo/src/ffi/base.rs` |
| 旧 realtime 测试使用已删除的 `RT_TYPE_CUSTOM` 与旧签名 | `tests/test_basic.c` 等 |

## 3. 问题清单

### P0：正确性 / 阻塞使用 / 安全

- [ ] 3.1 **socket 路径不一致**：`handler_ws.c` 传入 `/tmp/cyctron/<uuid>.sock`，
  `realtime.c` 忽略入参并生成 `/tmp/cyctron_<uuid>.sock`。
- [ ] 3.2 **消息未定向发送**：`handle_msg` 只广播，所有任务都收到同一条消息。
- [ ] 3.3 **插件缺少生命周期/调用符号**：只有 `process_init`；
  `rt_stop_task` 仅改状态。
- [ ] 3.4 **断线误清全部任务**：任一 WS 断开都触发 `rt_cleanup` 并重置 `g_rt_manager`。
- [ ] 3.5 **单连接模型**：`g_ws_conn` 只保存最后一个连接。
- [ ] 3.6 **跨线程调用 mongoose**：`event_push` 可能在插件/调度线程调用 `mg_ws_send`。
- [ ] 3.7 **Windows 插件路径错误**：固定拼 `./core/lib%s.so`。
- [ ] 3.8 **库名/uuid 未校验（安全）**：可加载任意动态库、路径注入。
- [ ] 3.9 **心跳逻辑未运行**：`rt_run` 无调用点。
- [ ] 3.10 **NNG 发送失败泄漏**：`nng_sendmsg` 失败未释放消息。
- [ ] 3.11 **没有任务锁**：同一任务/同一 `.so` 的并发 FFI 调用没有串行化，
  插件内 `static`/`OnceLock` 状态存在数据竞争（如 `libary_demo` 的 `ITEMS`、
  `soket_demo` 的 `RuntimeState`）。

### P1：健壮性 / 可维护性

- [ ] 3.12 **手写 JSON 提取脆弱**：不支持空格、转义、数字、嵌套。
- [ ] 3.13 **固定缓冲区静默截断**：消息 2048、字段 1024、事件 2048。
- [ ] 3.14 **锁覆盖不全**：任务表读取无锁，`rt_remove_task` 解锁后释放。
- [ ] 3.15 **bind 失败残留任务**。
- [ ] 3.16 **错误响应不统一**：缺统一 code 与请求 id。
- [ ] 3.17 **事件格式两套**：`action` 与 `type` 并存。
- [ ] 3.18 **JSON 拼接未转义**：`rt_broadcast`、`handle_msg`。
- [ ] 3.19 **user_data 释放约定不明**。
- [ ] 3.20 **强转依赖结构体布局**：`(rt_task_node_t*)task`。
- [ ] 3.21 **任务 id 可能重复**。
- [ ] 3.22 **初始化失败被吞**：`event_pusher_init` 总返回 true。
- [ ] 3.23 **/api/log 路由判断错误**。
- [ ] 3.24 **推送计数不完整**。
- [ ] 3.25 **二进制 WS 消息未处理**。
- [ ] 3.26 **日志噪音/敏感信息**。

### P2：体验 / 扩展

- [ ] 3.27 **Rust runtime 为进程级单例**，无法多 uuid。
- [ ] 3.28 **IPC 路径硬编码 `/tmp`**（保留 NNG，但需平台适配）。
- [ ] 3.29 **全局 PUB 无订阅者/背压管理**。
- [ ] 3.30 **插件响应未回传**：`process_data` 结果只 `println`。
- [ ] 3.31 **测试缺失**：旧 realtime 测试未接入 ctest，无 WS 协议级测试。
- [ ] 3.32 **客户端示例不可自动化**、硬编码 host/port/uuid。
- [ ] 3.33 **文档缺失**：消息字段表、错误码表、锁语义。
- [ ] 3.34 **残留字段/接口**：`rt_task_t.next`、旧 `ws_rt_*`。

## 4. 设计决策

- [*] 4.1 **通道不变**：继续使用 NNG，任务通道用 `ipc://`；Unix 默认
  `/tmp/cyctron_<uuid>.sock`，Windows 使用 `ipc://cyctron_<uuid>`（必要时回退
  `tcp://127.0.0.1:<port>`，但仍由 NNG 承载）。
- [*] 4.2 **同步请求-响应**：WS 收到一条请求后，在请求处理上下文内完成一次或多次
  FFI 调用，再把结果作为响应返回；事件推送继续走 NNG PUB。
- [*] 4.3 **锁分层**：全局加载锁（dlopen 缓存）→ 任务锁（uuid）→ 库锁（.so 路径）。
- [*] 4.4 **默认串行**：同一任务的调用默认串行；批量调用默认按数组顺序执行。
- [*] 4.5 **兼容旧命令**：`bind/msg/status/remove` 保留，新能力通过 `call`/`batch` 提供。

## 5. 任务锁设计（本轮重点）

### 5.1 目标

1. 同一任务（uuid）的 FFI 调用默认串行，避免插件内状态竞争。
2. 同一动态库被多个任务共享时，库级锁保护库内 `static` 状态。
3. 每次调用可独立选择锁模式、超时和失败策略。
4. 锁等待可超时，超时返回明确错误码，不阻塞整个服务。
5. 批量调用可以按顺序执行多个 FFI，并对每个调用单独控制。

### 5.2 锁层级

| 层级 | 作用域 | 实现 | 持有时间 |
| --- | --- | --- | --- |
| L0 加载锁 | 进程级 | 复用/扩展现有 `g_dl_mutex` | 仅 `dlopen`/缓存插入期间 |
| L1 任务锁 | 每个 uuid | `pthread_mutex_t` + 超时封装 | 一次调用或一批调用的执行期 |
| L2 库锁 | 每个 `.so` 路径 | 每个库一个 `pthread_mutex_t` | 单次 FFI 调用期间 |

固定获取顺序：**L0 → L1 → L2**。
批量调用逐个获取/释放，不在两次调用之间同时持有多个 L1/L2，避免死锁。

### 5.3 锁模式

| 模式 | 行为 | 适用场景 |
| --- | --- | --- |
| `serial`（默认） | 阻塞等待锁，直到超时 | 绝大多数插件调用 |
| `try` | 非阻塞；锁被占用立即返回 `-6 busy` | 前端轮询、避免排队 |
| `none` | 不加任务锁/库锁 | 确认插件线程安全的高吞吐场景 |

### 5.4 超时

- 每次调用可带 `timeout_ms`，默认 `2000`，`0` 表示不等待（等同 `try`）。
- 使用 `pthread_mutex_timedlock`（MinGW winpthreads 支持）或封装的
  `ct_mutex_lock_timeout()`；Windows 需验证。
- **限制**：FFI 调用一旦进入插件函数无法强制中断；超时只覆盖“等锁”阶段。
  执行中的调用会继续运行，文档与响应需明确标注 `timed_out_stage = "lock"`。

### 5.5 锁状态与统计

每个任务/库维护：

```c
typedef struct {
    uint64_t calls;          // 总调用数
    uint64_t failures;       // 失败数
    uint64_t lock_timeouts;  // 等锁超时数
    uint64_t busy_rejects;   // try 模式拒绝数
    uint64_t total_us;       // 累计耗时
    uint64_t max_us;         // 最大单次耗时
} ct_lock_stats_t;
```

`status` 响应增加：当前持锁任务、等待队列长度、上述统计。

### 5.6 错误码

| code | 含义 |
| --- | --- |
| 0 | 成功 |
| -1 | 参数错误 |
| -2 | 任务不存在 |
| -3 | 插件加载/符号失败 |
| -4 | 插件返回失败 |
| -5 | 安全拒绝（非法 lib/uuid） |
| -6 | 锁忙（`try` 模式） |
| -7 | 等锁超时 |
| -8 | 批量中部分失败（`stop_on_error=false` 时汇总） |

## 6. 请求-调用模型

### 6.1 单次调用

```json
// 请求
{
  "id": "req-1",
  "cmd": "call",
  "uuid": "task-001",
  "lib": "soket_demo",
  "symbol": "process_msg",
  "args": { "data": "hello" },
  "lock": "task",
  "mode": "serial",
  "timeout_ms": 2000
}

// 响应
{
  "id": "req-1",
  "ok": true,
  "code": 0,
  "call_id": "req-1#0",
  "elapsed_ms": 3,
  "data": { "task_uuid": "task-001", "result": "..." }
}
```

### 6.2 批量调用

```json
{
  "id": "req-2",
  "cmd": "batch",
  "uuid": "task-001",
  "concurrency": 1,
  "stop_on_error": false,
  "calls": [
    { "call_id": "c1", "lib": "soket_demo", "symbol": "process_msg",
      "args": { "data": "a" }, "lock": "task", "mode": "serial", "timeout_ms": 1000 },
    { "call_id": "c2", "lib": "soket_demo", "symbol": "process_status",
      "args": {}, "lock": "lib", "mode": "try", "timeout_ms": 0 }
  ]
}
```

响应的 `results` 按 `call_id` 返回，每个结果独立包含
`ok/code/error/elapsed_ms/data`；`stop_on_error=true` 时后续调用标记 `skipped`。

### 6.3 调用控制字段

| 字段 | 默认 | 说明 |
| --- | --- | --- |
| `lib` | 任务绑定库 | 允许批量跨库 |
| `symbol` | 无 | 插件导出函数名，如 `process_msg` |
| `args` | `null` | 原样序列化为插件入参 |
| `lock` | `task` | `task` / `lib` / `none` |
| `mode` | `serial` | `serial` / `try` / `none` |
| `timeout_ms` | 2000 | 等锁超时 |
| `retry` | 0 | 失败重试次数（仅 `-6/-7`） |
| `continue_on_error` | true | 批量中单次失败是否继续 |

### 6.4 处理流程

```
WS 收到 call/batch
  → 解析并校验 id/uuid/lib/symbol（白名单）
  → 查找任务（不存在则 -2）
  → 对每个 call：
      获取任务锁（按 lock/mode/timeout）
      获取库锁（按 lock/mode/timeout）
      calldyn 调用 symbol（复用 dlopen 缓存）
      释放库锁 → 释放任务锁
      记录统计与日志
  → 汇总 results，按 id 返回 WS 响应
  → 同时把关键事件推送到任务 NNG 通道
```

### 6.5 插件侧约定

- 默认直接调用插件导出的符号（与 libary 插件一致），入参为 `args` 的 JSON。
- 建议 `soket_demo` 补齐：
  `process_msg`、`process_call`、`process_status`、`process_pause`、
  `process_resume`、`process_close`。
- `process_call` 可选通用入口：`{ "symbol": "...", "args": {...} }`，
  内部 dispatch；批量调用可不使用它。
- 插件输出缓冲区仍为 64 KB（与 `calldyn.c` 一致），超限返回 `-4`。

## 7. 需要修改/新增的文件

| 文件 | 改动 |
| --- | --- |
| `include/realtime.h` | 任务锁/库锁字段、统计结构、`rt_call`/`rt_batch_call` API |
| `src/realtime.c` | 锁实现、超时封装、调用执行、统计、`rt_run` 接入 |
| `src/handler_ws.c` | 新增 `call`/`batch` 解析与校验，统一响应 |
| `src/handler.c` | 保持 `/soket` 与 `/api/*` 兼容，修正 `/api/log` 长度判断 |
| `src/calldyn.c` | 按 symbol 调用、平台扩展名、调用耗时统计 |
| `src/event_pusher.c` | 响应/事件推送、`nng_sendmsg` 失败处理、多连接 |
| `include/calldyn.h` | 新增按符号调用的 API 声明 |
| `examples/soket_demo/src/ffi/base.rs` | 补齐控制符号与 `process_call` |
| `examples/soket_demo/src/core/runtime.rs` | 按 uuid 的实例表（若确认多实例） |
| `tests/CMakeLists.txt` | 注册新测试 |
| `tests/test_task_lock.c` | 新增：锁串行/超时/try |
| `tests/test_batch_call.c` | 新增：批量调用与部分失败 |
| `tests/test_call_error.c` | 新增：参数/符号/超时错误 |
| `tests/test_lock_order.c` | 新增：交叉调用无死锁 |
| `tests/test_concurrency.c` | 新增：并发压力与统计 |
| `tests/stress_report.h` | 已落地：压力测试结果记录（JSONL、跨平台计时） |
| `tests/test_nng_stress.c` | 已落地：NNG PAIR1 / PUB/SUB IPC 压力测试 |
| `ts/socket_call.ts` | 新增：单调用断言 |
| `ts/socket_batch.ts` | 新增：批量调用断言 |
| `ts/socket_lock.ts` | 新增：锁模式/超时/busy 断言 |
| `ts/socket_error.ts` | 新增：错误码断言 |
| `ts/socket_concurrency.ts` | 新增：并发断言 |
| `ts/socket_request.ts` | 新增：基础协议与旧命令兼容 |
| `ts/socket_all.ts` | 新增：统一运行入口 |

## 8. 实施阶段（确认后开始）

### 阶段 0：评审

- [ ] 8.1 评审本文件的锁层级、协议字段、错误码与默认值
- [ ] 8.2 确认 1.8–1.11 的开放问题

### 阶段 1：锁基础设施

- [ ] 8.3 新增 `ct_mutex_lock_timeout()` 跨平台封装并验证 MinGW
- [ ] 8.4 在任务节点上增加任务锁、库锁注册表、统计结构
- [ ] 8.5 实现 `serial` / `try` / `none` 三种模式
- [ ] 8.6 修复任务表无锁读取与释放窗口

### 阶段 2：请求-调用模型

- [ ] 8.7 新增 `call` 命令：单次 FFI 调用 + 统一响应
- [ ] 8.8 新增 `batch` 命令：多调用、`concurrency`、`stop_on_error`
- [ ] 8.9 `calldyn` 支持按 symbol 调用并返回耗时
- [ ] 8.10 校验 `lib`/`uuid`/`symbol` 白名单，拒绝路径注入
- [ ] 8.11 `rt_send_msg` 接入任务 NNG 通道，响应与事件分离

### 阶段 3：生命周期与并发

- [ ] 8.12 任务归属连接，断线只清理本连接任务
- [ ] 8.13 WS 发送改为事件队列/主循环，消除跨线程 `mg_ws_send`
- [ ] 8.14 补齐心跳、`rt_run` 接入、超时策略
- [ ] 8.15 插件 `process_close/pause/resume/status` 接入

### 阶段 4：Rust 插件

- [ ] 8.16 `soket_demo` 补齐控制符号与 `process_call`
- [ ] 8.17 按 uuid 的 runtime 实例表（若确认多实例）
- [ ] 8.18 插件响应通过 NNG 回传，而不只是 `println`

### 阶段 5：测试与文档

- [ ] 8.19 新增/迁移 C 单元测试并接入 ctest
- [ ] 8.20 新增 TS 测试文件与 `socket_all.ts`
- [ ] 8.21 更新 README：协议表、错误码、锁语义、限制
- [ ] 8.22 在 Windows/MinGW 与 Linux 各跑一遍回归

## 9. 测试计划

### 9.1 C 单元测试（`tests/`）

| 文件 | 用例 |
| --- | --- |
| `test_task_lock.c` | 同任务并发调用串行；不同任务并行；`try` 返回 `-6`；等锁超时 `-7`；统计计数正确 |
| `test_batch_call.c` | 单请求多调用顺序执行；`stop_on_error` 两种行为；`skipped` 标记；跨库调用 |
| `test_call_error.c` | 非法 JSON、缺 uuid、未知 lib/symbol、插件返回非 0、输出超限 |
| `test_lock_order.c` | 两个任务交叉调用两个库，固定超时内无死锁；批量调用不交叉持锁 |
| `test_concurrency.c` | N 线程并发 call/batch，断言零崩溃、统计守恒、无泄漏 |
| `test_ws_protocol.c` | 真实 WS 连接驱动 call/batch，断言响应 id/code/结果 |

### 9.2 TS 测试（`ts/`）

| 文件 | 用例 |
| --- | --- |
| `socket_request.ts` | 连接、ping/status、旧命令兼容 |
| `socket_call.ts` | 单调用成功/失败/超时 |
| `socket_batch.ts` | 多调用、部分失败、`stop_on_error` |
| `socket_lock.ts` | `serial` 排队、`try` busy、`none` 并发 |
| `socket_error.ts` | 错误码表逐项断言 |
| `socket_concurrency.ts` | 多请求并发、结果按 id 对齐 |
| `socket_all.ts` | 顺序运行以上脚本并汇总 |

### 9.3 测试约束

- 测试插件使用 `soket_demo` 与一个可控的 `libary_demo`（已有），避免依赖网络。
- 所有锁测试带超时，失败时输出当前持锁者与等待队列。
- 压力测试默认 8 线程 × 1000 次，可通过环境变量调整。
- 测试结束检查：任务数归零、NNG 关闭、无残留线程。

### 9.4 压力测试与结果记录（已落地）

- 结果文件：`test-results/stress.jsonl`（可用 `STRESS_REPORT_DIR` 覆盖）
- 每行一条 JSON，字段：
  `timestamp` / `platform` / `suite` / `case` / `threads` / `iterations` /
  `duration_ms` / `ops_per_sec` / `failures` / `status` / `notes` / `pid`
- 已接入 ctest：
  - `libary_stress`：FFI 串行 20000 次 + 8 线程混合 40000 次
  - `nng_stress`：PAIR1 可靠通道 5000 条（断言零丢失）
  - `nng_stress`：PUB/SUB 尽力而为 5000 条（记录丢失率，不作为失败）
- 运行：
  - `mingw32-make -C build test`（Windows/MinGW）
  - `ctest --test-dir build -R "stress" --output-on-failure`
- 结果查看：`tail -n 20 test-results/stress.jsonl`
- 基线（Windows MinGW Release，2026-10-21）：
  - `ffi/serial_get`：约 197k ops/s，0 失败
  - `ffi/concurrent_mixed`：约 1.2k ops/s，0 失败
  - `nng/pair1_ipc_throughput`：约 46k msg/s，0 丢失
  - `nng/pubsub_ipc_best_effort`：约 92% 丢失（PUB/SUB 尽力而为语义）
- 结论：PUB/SUB 在高突发流量下不可靠；soket 任务通道若要求零丢失，
  需改用 PAIR1 或在上层增加确认/重传。该结论纳入 4.4 设计评审。
- 待补（依赖阶段 2）：
  - `test_lock_stress.c`：任务锁/库锁串行、try、超时
  - `test_batch_stress.c`：单请求多调用并发压力
  - `ts/socket_concurrency.ts`：WS 层并发与结果对齐

## 10. 验收标准

- [ ] 10.1 同一请求可包含 1..N 个 FFI 调用，响应按 `call_id` 返回。
- [ ] 10.2 每个调用可独立指定 `lock`、`mode`、`timeout_ms`。
- [ ] 10.3 同任务默认串行；`try` 模式在锁占用时立即返回 `-6`；
  等锁超时返回 `-7`。
- [ ] 10.4 两个任务交叉调用两个库不会死锁。
- [ ] 10.5 批量调用支持部分失败与 `stop_on_error`。
- [ ] 10.6 新测试在 ctest 与 deno 中全部通过，重复运行稳定。
- [ ] 10.7 Windows 与 Linux 行为一致（IPC 地址按平台生成）。
- [ ] 10.8 旧 `bind/msg/status/remove` 仍可用。

## 11. 风险与限制

- [ ] 11.1 FFI 调用不可中断：超时只能覆盖等锁阶段，需在文档与响应中注明。
- [ ] 11.2 锁顺序必须固定为 L0→L1→L2；批量调用逐个持锁，避免同时持有多个互斥锁。
- [ ] 11.3 Win32 下 `pthread_mutex_timedlock` 依赖 winpthreads，需在 MinGW 实测。
- [ ] 11.4 响应体可能随批量数量增大，需要设置批量上限（建议 64 次/请求）。
- [ ] 11.5 若确认多实例，Rust `OnceLock` 改造会影响现有 `process_init` 语义。
- [ ] 11.6 协议新增字段采用双写兼容，旧客户端在一个版本内不受影响。
- [ ] 11.7 PUB/SUB 实测在突发流量下丢失率超过 90%：soket 事件通道若要求可靠投递，
  需改用 PAIR1 或增加确认/重传，基线见 `test-results/stress.jsonl`。
