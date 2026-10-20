# 故障排查

## 1. 服务启动

### 端口被占用

```
Error starting HTTP+WS server on http://0.0.0.0:44944
```

- 换端口：`./cyctron start -port 44945`
- 查占用：`netstat -ano | findstr :44944`（Windows）/
  `lsof -i :44944`（Linux/macOS）

### 启动后立即退出

- 确认在项目根目录运行（插件/静态目录是相对路径）。
- 检查 `event_pusher_init` 日志；NNG 监听失败只记录错误，不会阻止 HTTP 启动。
- Windows Release 使用 `-mwindows`，无控制台；用 Debug 构建或在命令行观察日志。

## 2. libary 插件

### 返回 404 `Plugin Not Found`

按顺序检查：

1. 文件是否存在：`./libary/<uri>.<dll|so|dylib>`。
2. 扩展名是否与平台一致（Windows `.dll`）。
3. 符号是否存在：`<小写 method>_<FFI-Symbol>`。
   - `POST /libary_demo` + `FFI-Symbol: add` → `post_add`
   - 不带头 → `post_handle`
4. 插件返回值是否为 0。
5. 插件是否写入了以 `\0` 结尾的 JSON。

查看日志：

```
[CALDYN] Loading library: ...
[CALDYN] Failed to load library: ...
[ffi] dlsym failed: ...
```

### Windows 加载失败

- 架构必须一致（x64）。
- 插件依赖的 DLL 需在 PATH 或同目录。
- Rust 插件需要 MSVC 运行时（或使用 `-C target-feature=+crt-static` 静态链接）。

### 中文/UTF-8 乱码

- Windows 下 CMake 已设置 `/utf-8` 或 `-fexec-charset=UTF-8`。
- 插件输出必须是 UTF-8。

## 3. 静态文件

### 访问 `/` 返回 404

- 确认 `web/index.html` 存在。
- 无后缀 URI 会回退到 `web/index.html`。
- 带后缀 URI 直接映射到 `web/<uri>`。

### 路径遍历被拒绝

含 `..` 的 URI 返回 400，这是预期行为。

## 4. SSE

### 返回 503 `SSE plugin unavailable`

- 确认 `./libary/sse_demo.<ext>` 存在。
- 先构建 `examples/sse_demo` 并拷贝到 `./libary/`。

### 连接建立但收不到事件

- 检查插件 `sse_next` 是否返回 `id` 大于 `after_id`。
- 查看日志中的 `[SSE] sse_next failed`。
- 用 `curl -N` 直接观察原始流。
- 代理可能缓冲：已发送 `X-Accel-Buffering: no`，仍需在网关关闭缓冲。

### 事件被截断

- 单事件 `data` 上限约 4 KB（C 端缓冲）。
- 插件输出总缓冲 64 KB。

### `Last-Event-ID` 不生效

- 请求头名称必须是 `Last-Event-ID`。
- 服务端只接受数字；非数字按 0 处理。
- 服务重启后 id 从插件状态重新开始。

## 5. WebSocket / realtime

### 连接后没有任务事件

- 先发送 `{"cmd":"bind","uuid":"task-001","lib":"libsoket_demo"}`。
- 确认 `./core/` 下存在对应插件（`call_local_dyn_socklibffi` 从 `./core/` 加载）。
- 查看 `[Scheduler]` 日志。

### 多个客户端互相干扰

当前只支持单 WS 连接；后连接会覆盖 `g_ws_conn`，任一连接关闭会清理全部任务。
这是已知限制，优化计划见 `todo/socket.md`。

### RT_TASK_STOPPED 但插件仍在运行

`rt_stop_task` 只修改状态，不会通知插件；需要插件导出 `process_close` 并接入。

## 6. NNG

### `nng_listen ipc:///tmp/... failed`

- Unix：确认 `/tmp` 可写。
- Windows：NNG 的 IPC 使用命名管道，`/tmp` 路径可能不符合预期；
  建议改用 `ipc://cyctron_<uuid>`。
- 检查是否已有同名 socket 文件/管道残留。

### PUB/SUB 丢消息

PUB/SUB 是尽力而为语义；突发流量或订阅未建立时会丢。
压测基线见 `test-results/stress.jsonl`（`socket` 分支）。

### 订阅端收不到

- 确认订阅时使用了空前缀（订阅所有主题）。
- 确认先启动 PUB 再启动 SUB，或等待订阅建立。

## 7. 构建

### `libffi.a not found`

- Windows 需要 `crates/libffi-packages/libffi.a`。
- 若文件缺失，从 `developer` 分支恢复：

```bash
git checkout developer -- crates/libffi-packages
```

### `fatal error: nng/nng.h: No such file`

```bash
git submodule update --init --recursive
```

### CMake 找不到生成器

- Windows 使用 `-G "MinGW Makefiles"`，确保 MSYS2 的 `mingw64/bin` 在 PATH。
- Linux/macOS 使用 `-G Ninja`，先安装 ninja。

### 链接错误：重复定义/缺少符号

- 确认 `CMakeLists.txt` 中源文件没有重复添加。
- 测试目标需要显式链接 `${FFI_LIBRARIES}` 与 mongoose。

## 8. 测试

### ctest `Not Run`，提示找不到可执行文件

先构建全部目标，而不是只构建主程序：

```bash
cmake --build build -j4
```

### `libary` / `sse` 测试未注册

对应插件不存在时，CMake 不会 `add_test`：

```bash
ls libary/
```

### TS 测试连接失败

- 先启动服务，并确认端口一致。
- `deno run --allow-net ts/xxx.ts <port>`。
- SSE 脚本会保持长连接，超时后通过 AbortController 退出。

## 9. 日志定位

| 前缀 | 模块 |
| --- | --- |
| `[WS]` | WebSocket 命令与连接 |
| `[SSE]` | SSE 连接、事件、插件错误 |
| `[Scheduler]` | realtime 任务 |
| `[NNG]` | 全局事件 PUB |
| `[EventPusher]` | 事件推送 |
| `[CALDYN]` | 动态库加载与调用 |
| `[Static]` | 静态文件映射 |
| `[API]` | HTTP API 推送 |

开启更详细日志：Debug 构建，或调整 `crates/log` 的级别。
