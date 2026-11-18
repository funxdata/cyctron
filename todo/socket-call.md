# TODO: WebSocket `call` 后续

`call` 的纯函数解析、错误码、响应 JSON、消息大小限制、worker 线程、队列上限、
超时标记与 ctest 覆盖已完成；以下为剩余项。

## 待办

- [ ] SC-11 服务端超时与取消：已实现排队超时和执行耗时标记（返回 `-7`），
  但插件执行中无法安全取消；需要确认是否引入进程隔离或可中断 ABI。
- [ ] SC-15 文档与示例：补齐 `ts/socket_call.ts` 调用示例；
  `docs/websocket.md` 的错误码与 worker 说明已更新。
