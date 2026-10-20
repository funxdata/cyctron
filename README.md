# cyctron

本地运行的 HTTP + WebSocket + SSE 服务，核心业务通过 **动态库插件** 扩展。

- 默认监听 `0.0.0.0:44944`
- C11 + CMake，跨 Windows / Linux / macOS
- 插件通过 libffi 调用，支持 Rust / C / C++ 动态库
- 事件推送：WebSocket、SSE、NNG IPC

## 核心能力

| 能力 | 端点 | 说明 |
| --- | --- | --- |
| HTTP API | `/api/*` | 状态查询、事件推送 |
| libary 插件 | `/<name>` | 映射到 `./libary/<name>.<ext>`，按方法+`FFI-Symbol` 调符号 |
| 静态文件 | `/`、`/assets/*` | `web/` 目录，index.html 回退 |
| WebSocket | `/soket` | 实时任务命令（bind/msg/status/remove） |
| SSE | `/sse` | 服务器事件流，支持 `Last-Event-ID` 续传 |
| NNG IPC | `ipc:///tmp/cyctron.sock` | 全局事件 PUB；每任务独立 PUB |

## 快速开始

### 1. 构建

```bash
git submodule update --init --recursive

# Linux / macOS
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows / MinGW
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

产物：项目根目录 `cyctron`（Windows 为 `cyctron.exe`）。

### 2. 构建并放置插件

```bash
# libary CRUD 示例
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
cp examples/libary_demo/target/release/libary_demo.dll libary/     # Windows
cp examples/libary_demo/target/release/libibary_demo.so libary/libary_demo.so  # Linux

# SSE 事件源示例
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
cp examples/sse_demo/target/release/sse_demo.dll libary/           # Windows
cp examples/sse_demo/target/release/libsse_demo.so libary/sse_demo.so        # Linux
```

### 3. 启动

```bash
./cyctron start
# 或指定端口
./cyctron start -port 44944
```

### 4. 验证

```bash
# 服务状态
curl http://127.0.0.1:44944/api/status

# libary 插件
curl "http://127.0.0.1:44944/libary_demo?id=1"

# SSE（Ctrl+C 退出）
curl -N http://127.0.0.1:44944/sse?interval_ms=200
```

## 目录结构

```
include/        对外头文件（路由、插件、WS、SSE、realtime）
src/            服务实现
src/core/       通用工具（URI）
crates/         第三方子模块（mongoose、nng、libev、log、libffi）
examples/       Rust 插件示例（rust_demo、soket_demo、libary_demo、sse_demo）
tests/          C 单元/集成/压力测试
ts/             Deno 接口测试与客户端示例
todo/           模块设计与完成记录
docs/           系统文档
```

## 文档

完整文档见 [docs/README.md](./docs/README.md)。

| 文档 | 内容 |
| --- | --- |
| [docs/architecture.md](./docs/architecture.md) | 架构、线程模型、请求/事件流 |
| [docs/http-api.md](./docs/http-api.md) | HTTP 路由、API、静态文件、错误 |
| [docs/libary.md](./docs/libary.md) | libary 插件协议与 C ABI |
| [docs/sse.md](./docs/sse.md) | SSE 端点、事件格式、续传 |
| [docs/websocket.md](./docs/websocket.md) | `/soket` 协议与命令 |
| [docs/realtime-nng.md](./docs/realtime-nng.md) | realtime 任务与 NNG |
| [docs/configuration.md](./docs/configuration.md) | 端口、目录、编译宏 |
| [docs/build.md](./docs/build.md) | 构建与平台依赖 |
| [docs/testing.md](./docs/testing.md) | C/TS/压力测试 |
| [docs/development.md](./docs/development.md) | 扩展插件/路由、todo 流程 |
| [docs/troubleshooting.md](./docs/troubleshooting.md) | 常见问题排查 |

## 测试

```bash
# C 测试（需先放置对应插件）
cmake --build build
ctest --test-dir build --output-on-failure

# TS 接口测试（先启动服务）
deno run --allow-net ts/libary_request.ts 44944
deno run --allow-net ts/sse_request.ts 44944
```

说明：

- `libary`、`sse` 测试仅在对应插件存在时注册。
- 静态文件测试需要 `web/` 资源。
- 压力测试（`tests/test_nng_stress.c`、`tests/stress_report.h`）位于 `socket` 分支，
  结果写入 `test-results/stress.jsonl`。

## 分支

| 分支 | 内容 |
| --- | --- |
| `main` | 集成主线 |
| `developer` | 历史开发线 |
| `libary` | libary 插件与测试 |
| `socket` | soket 压力测试与实时通道优化 |
| `sse` | SSE 端点与 `sse_demo` |

## 平台说明

- Windows：使用 MSYS2 MinGW 或 MSVC；Release 为 GUI 子系统（无控制台）。
- Windows 静态 libffi：`crates/libffi-packages/libffi.a`。
- Linux/macOS：通过 `pkg-config` 查找 libffi。

## 许可

MIT，见 [LICENSE](./LICENSE)。
