---
name: cyctron-dev
description: cyctron 项目开发约定：CMake 构建、ctest 测试、插件（libary/plugins）、SSE/WebSocket 路由与目录布局。修改 C 源码、构建脚本或插件前先读本技能。
---

# cyctron 开发技能

本地 HTTP + WebSocket + SSE 服务，C11 + CMake，跨 Windows / Linux / macOS。
核心业务通过动态库插件（libffi）扩展，默认监听 `0.0.0.0:44944`。

## 目录速查

| 路径 | 内容 |
| --- | --- |
| `src/main.c` | 启动、命令行、主循环（`mg_mgr_poll` + `ws_drain_call_results`） |
| `src/handler.c` | 总路由：OPTIONS → WS → SSE → `/api/*` → 插件 → 静态 |
| `src/handler_libary.c` | 插件 HTTP 调用（`<method>_<FFI-Symbol>`） |
| `src/handler_sse.c` | `/sse` 长连接，主循环轮询推送 |
| `src/handler_ws.c` | `/soket` 命令（bind/msg/status/remove/call） |
| `src/ws_worker.c` | FFI 调用线程池，结果由主循环 drain |
| `src/calldyn.c` | dlopen 缓存 + libffi 调用 |
| `src/plugin_path.c` | `./plugins/<name>/` 优先，`./libary/<name><ext>` 回退 |
| `include/global.h` | 平台路径/扩展名宏（`WEB_ROOT`、`LIBARY_DIR` 等） |
| `tests/` | C 测试，`tests/CMakeLists.txt` 注册 ctest |
| `examples/` | Rust 插件示例（libary_demo、sse_demo、soket_demo、rust_demo） |
| `docs/` | 系统文档；`issues/` 记录已知问题 |

## 构建与测试

```bash
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows / MinGW：

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

- 产物在**项目根目录**（`cyctron.exe` / `cyctron`），由 `RUNTIME_OUTPUT_DIRECTORY` 指定。
- `build*/` 目录全部是本地生成物，**不要提交**。
- 插件缺失时相关 `add_test` 不会注册（`tests/CMakeLists.txt` 用 `if(EXISTS ...)`）。
- 插件契约：导出 `int <method>_handle(const char *in, char *out)`，`out` 为 64 KB 缓冲区。

## 修改约定

1. 新增源文件要同时加入 `CMakeLists.txt` 的 `CORE_SOURCES`。
2. 新增路由保持在 `handler.c` 的顺序：OPTIONS → WS → SSE → API → 插件 → 静态。
3. 拼 JSON 用 `event_escape_json` / `mg_json_*`，不要直接拼接用户输入。
4. 失败路径要释放 `calldyn` 返回的 `json_out`（`malloc`，调用方 `free`）。
5. 文件名/URI 拼接前先做 `..` 与字符白名单校验（`uri_has_traversal`、`plugin_name_is_safe`）。
6. 提交前跑 `git diff --check`、构建和受影响的 ctest 子集。

## 常见坑

- Windows LLP64：`uint64_t` 不能用 `%lu`，用 `PRIu64` 或 `%llu`。
- mongoose 不是线程安全的：所有 `mg_*` 发送应回到主循环线程。
- `mg` 的连接指针在 `MG_EV_CLOSE` 后失效，连接表里要做失效清理。
- `docs/build.md`、`README.md` 与实际产物位置/CI 变量可能漂移，以代码为准。
