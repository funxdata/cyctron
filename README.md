# cyctron

本地运行的 HTTP + WebSocket + SSE 服务，核心业务通过 **动态库插件** 扩展。

- 默认监听 `0.0.0.0:44944`
- C11 + CMake，跨 Windows / Linux / macOS
- 插件通过 libffi 调用，支持 Rust / C / C++ 动态库
- 事件推送：WebSocket、SSE、NNG IPC

## 核心能力

| 能力 | 端点 | 说明 |
| --- | --- | --- |
| 插件路由 | `/<name>` | 从 `./libary/<name>.<ext>` 加载，按方法+`FFI-Symbol` 调符号 |
| 静态文件 | `/`、`/assets/*` | 优先 `web/index.html`，否则内置首页；带后缀/`/assets` 映射 `web/` |
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

产物：`.dytes/cyctron.exe`（Linux / macOS 为 `.dytes/cyctron`）。

### 2. 构建并放置插件

```bash
# libary CRUD 示例
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
cp examples/libary_demo/target/release/libary_demo.dll .dytes/libary/     # Windows
cp examples/libary_demo/target/release/libibary_demo.so .dytes/libary/libary_demo.so  # Linux

# SSE 事件源示例
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
cp examples/sse_demo/target/release/sse_demo.dll .dytes/libary/           # Windows
cp examples/sse_demo/target/release/libsse_demo.so .dytes/libary/sse_demo.so        # Linux
```

### 3. 启动

```bash
cd .dytes
./cyctron.exe start            # Windows
./cyctron start                # Linux / macOS
# 或指定端口
./cyctron.exe start -port 44944

# 停止（在 .dytes 目录执行，读取 cyctron.pid）
./cyctron.exe stop
```

### 4. 验证

```bash
# 首页（内置 HTML）
curl -I http://127.0.0.1:44944/

# libary 插件
curl "http://127.0.0.1:44944/libary_demo?id=1"

# SSE（Ctrl+C 退出）
curl -N http://127.0.0.1:44944/sse?interval_ms=200
```

## 目录结构

```
.dytes/         本地测试环境（构建产物、libary、web 夹具、dytes test 目录；不入库）
include/        对外头文件（路由、插件、WS、SSE、realtime）
src/            服务实现
src/core/       通用工具（URI、JSON 校验）
crates/         第三方子模块（mongoose、nng、libev、log、libffi）
examples/       Rust 插件示例（rust_demo、soket_demo、libary_demo、sse_demo）
tests/          C 单元/集成/压力测试
libary/         运行时能力库目录（从仓库根启动时使用；本地测试在 .dytes/libary）
plugins/        设计插件目录（由 chatllm 管理，cyctron 不读取）
ts/             Deno 接口测试与客户端示例
todo/           模块设计与完成记录
docs/           系统文档
```

## 文档

完整文档见 [docs/README.md](./docs/README.md)。

| 文档 | 内容 |
| --- | --- |
| [docs/architecture.md](./docs/architecture.md) | 架构、线程模型、请求/事件流 |
| [docs/http-api.md](./docs/http-api.md) | HTTP 路由、静态文件、错误 |
| [docs/libary.md](./docs/libary.md) | 插件协议（libary 路径、C ABI） |
| [docs/sse.md](./docs/sse.md) | SSE 端点、事件格式、续传 |
| [docs/websocket.md](./docs/websocket.md) | `/soket` 协议与命令 |
| [docs/realtime-nng.md](./docs/realtime-nng.md) | realtime 任务与 NNG |
| [docs/configuration.md](./docs/configuration.md) | 端口、目录、平台路径 |
| [docs/build.md](./docs/build.md) | 构建与平台依赖 |
| [docs/testing.md](./docs/testing.md) | C/TS/压力测试 |
| [docs/development.md](./docs/development.md) | 扩展插件/路由、todo 流程 |
| [docs/troubleshooting.md](./docs/troubleshooting.md) | 常见问题排查 |

## 测试

```bash
# C 测试（插件放在 .dytes/libary）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# TS 接口测试（先在 .dytes 启动服务）
cd .dytes && ./cyctron.exe start -port 44944
deno run --allow-net ts/libary_request.ts 44944
deno run --allow-net ts/sse_request.ts 44944
```

说明：

- `libary`、`sse` 测试仅在 `.dytes/libary` 存在对应插件时注册。
- 静态文件测试使用 `.dytes/web` 夹具。
- 压力测试结果写入 `docs/results/stress.jsonl`（随仓库归档）；
  已归档样例与初步分析见 [docs/results/README.md](./docs/results/README.md)。
- 旧 realtime/插件测试已清理；当前测试文件均已接入 ctest（`static` 仅非 Windows，插件缺失时 `libary`/`sse` 跳过）。

## 分支

所有功能已合并到 `main`；`developer`、`libary`、`socket`、`sse` 仅保留历史记录。

## 平台说明

- Windows：使用 MSYS2 MinGW 或 MSVC；Release 为 GUI 子系统（无控制台）。
- Windows 静态 libffi：`crates/libffi-packages/libffi.a`。
- Linux/macOS：通过 `pkg-config` 查找 libffi。

## 许可

MIT，见 [LICENSE](./LICENSE)。
