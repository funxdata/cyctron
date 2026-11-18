# TODO: soket 实时通道待办

> 已完成的锁分层、请求-调用模型、worker 线程化、断线按连接清理等已从本文移除。

## 1. 待确认

- [ ] 1.8 批量调用默认串行还是允许并发（当前没有 `batch` 命令）。
- [ ] 1.9 锁等待超时后的默认值（当前调用使用 `timeout_ms`）。
- [ ] 1.10 同一插件多实例（多个 uuid）是否必须支持（Rust runtime 仍是进程级单例）。
- [ ] 1.11 旧 `cmd: bind/msg/status/remove` 的兼容期限。

## 2. 未完成问题

- [ ] 2.1 socket 路径不一致：`handler_ws.c` 传入 `/tmp/cyctron/<uuid>.sock`，
  `rt_create_task` 忽略该参数并生成 `/tmp/cyctron_<uuid>.sock`。
- [ ] 2.2 `msg` 未定向到具体任务：`handle_msg` 只向当前连接发事件，任务 NNG 未收到定向消息。
- [ ] 2.3 Rust `soket_demo` 仍只保证 `process_init`，缺少 `process_pause/resume/status/close/call`。
- [ ] 2.4 客户端 `uuid` 未校验，可拼入 socket 路径（见 `issues/005`）。
- [ ] 2.5 NNG 发送失败时消息所有权/释放未处理。
- [ ] 2.6 任务级锁未实现：worker 只按连接串行，不保证同一任务/同一库的并发策略。
- [ ] 2.7 `bind` 失败后任务未回滚，任务表残留 IDLE/ERROR 任务。
- [ ] 2.8 错误响应不统一：`bind/msg` 使用自定义事件，`call` 使用 `code` 字段。
- [ ] 2.9 事件格式两套：`action`（realtime）与 `type`（event_pusher）并存。
- [ ] 2.10 `send_event_to` 的 `uuid`/`status` 未 JSON 转义（见 `issues/005`）。
- [ ] 2.11 `user_data` 释放约定不明确（`node_destroy` 直接 `free`）。
- [ ] 2.12 `(rt_task_node_t*)task` 依赖结构体首成员布局，缺少显式容器转换。
- [ ] 2.13 `event_pusher_init` 总是返回 true，初始化失败被吞。
- [ ] 2.14 推送计数只统计 WebSocket，不统计 NNG。
- [ ] 2.15 二进制 WS 消息未处理（按文本解析）。
- [ ] 2.16 日志包含完整插件输入/输出，可能泄露敏感数据。
- [ ] 2.17 Rust runtime 为进程级单例，无法多 uuid。
- [ ] 2.18 IPC 路径硬编码 `/tmp`，Windows 需平台适配。
- [ ] 2.19 全局 PUB 无订阅者/背压管理。
- [ ] 2.20 插件响应未回传：`process_init` 结果只记录日志。
- [ ] 2.21 客户端示例（`ts/demo_socket.ts`）交互式、硬编码 host/port/uuid。
- [ ] 2.22 残留字段/接口：`rt_task_t.next` 等。
