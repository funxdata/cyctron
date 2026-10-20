# cyctron 文档

本目录是 cyctron 的系统文档，按“先总览、后模块”的顺序组织。

## 阅读顺序

| 文档 | 内容 |
| --- | --- |
| [architecture.md](./architecture.md) | 总体架构、线程模型、请求/事件数据流 |
| [http-api.md](./http-api.md) | HTTP 路由、API、静态文件、CORS 与错误格式 |
| [libary.md](./libary.md) | libary 动态库插件协议（URI、FFI-Symbol、C ABI） |
| [sse.md](./sse.md) | SSE 事件流端点、事件格式、重连与 `sse_demo` |
| [websocket.md](./websocket.md) | `/soket` WebSocket 协议与命令 |
| [realtime-nng.md](./realtime-nng.md) | realtime 任务调度与 NNG IPC 通道 |
| [configuration.md](./configuration.md) | 端口、目录、编译宏、平台路径 |
| [build.md](./build.md) | 依赖、CMake、Windows/Linux/macOS 构建 |
| [testing.md](./testing.md) | C/TS 测试、插件准备、压力测试与结果记录 |
| [development.md](./development.md) | 扩展插件/路由、todo 流程、分支约定 |
| [troubleshooting.md](./troubleshooting.md) | 常见问题与排查步骤 |

## 快速索引

- 想跑起来：看 [build.md](./build.md) 的“快速构建”，再执行 `./cyctron start`
- 想写插件：看 [libary.md](./libary.md) 与 [development.md](./development.md)
- 想接入事件流：看 [sse.md](./sse.md) 与 [websocket.md](./websocket.md)
- 想调试任务调度：看 [realtime-nng.md](./realtime-nng.md)
- 测试失败/插件加载失败：看 [troubleshooting.md](./troubleshooting.md)

## 模块待办

各模块的详细设计过程与完成记录保留在仓库根目录的 `todo/`：

- `todo/libary.md`：libary 插件与测试
- `todo/static.md`：静态文件服务
- `todo/sse.md`：SSE 端点与 `sse_demo`
- `todo/socket.md`：soket 实时通道优化（位于 `socket` 分支）

## 文档维护约定

- 新增模块时，在 `docs/` 增加对应文件，并更新本索引与根 `README.md` 的导航。
- 协议字段、错误码、目录约定发生变化时，同步更新对应文档。
- 已知限制与未完成项用“限制”或“待办”小节明确标出，避免与实现混淆。
