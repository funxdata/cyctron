# 测试

## 1. 测试分层

| 层 | 位置 | 运行方式 | 依赖 |
| --- | --- | --- | --- |
| C 单元/集成 | `tests/*.c` | ctest | 对应插件产物 |
| TS 接口 | `ts/*.ts` | deno run | 已启动的 cyctron |
| 压力测试 | `tests/test_*stress*.c` | ctest / 直接运行 | 插件 + mongoose/nng |
| Rust 插件 | `examples/*/tests` | cargo test | 无 |

## 2. 准备插件

C 测试与接口测试需要先把插件放入约定目录：

```bash
# libary 插件
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
cp examples/libary_demo/target/release/libary_demo.dll libary/          # Windows
cp examples/libary_demo/target/release/libibary_demo.so libary/libary_demo.so  # Linux

# SSE 插件
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
cp examples/sse_demo/target/release/sse_demo.dll libary/                # Windows
cp examples/sse_demo/target/release/libsse_demo.so libary/sse_demo.so   # Linux
```

插件缺失时，相关 `add_test` 不会注册（CMake 里用 `if(EXISTS ...)` 判断）。

## 3. C 测试

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

单个测试：

```bash
ctest --test-dir build -R "^sse$" --output-on-failure
ctest --test-dir build -R "^libary" --output-on-failure
```

### 3.1 已接入 ctest 的测试

| 名称 | 文件 | 覆盖 |
| --- | --- | --- |
| `libary` | `tests/test_libary.c` | libary_demo 全符号、边界、未知符号 |
| `libary_stress` | `tests/test_libary_stress.c` | 串行 20000 + 8 线程混合 |
| `sse` | `tests/test_sse.c` | SSE 格式化、sse_demo 调用、续传 |
| `static`（非 Windows） | `tests/test_static.c` | 静态路径解析、404、遍历拒绝 |
| `calldyn`（非 Windows + 插件存在） | `tests/test_calldyn.c` | FFI 插件联调 |

### 3.2 仅构建、不注册

| 文件 | 原因 |
| --- | --- |
| `tests/test_nng_ipc.c` | 演示程序，无限循环 |
| `tests/test_dyn_socket.c` | 与 soket_demo 导出符号不匹配 |
| `tests/test_basic.c`、`test_data.c`、`test_multiple.c`、`test_pause_resume.c`、`test_performance.c`、`test_realtime.c` | 使用旧版 realtime API，尚未迁移 |

## 4. TS 接口测试

先启动服务：

```bash
./cyctron start -port 44944
```

再运行：

```bash
# libary 插件接口
deno run --allow-net ts/libary_request.ts 44944

# 静态文件（需要 web/ 资源）
deno run --allow-net ts/static_request.ts 44944

# SSE
deno run --allow-net ts/sse_request.ts 44944

# WebSocket（交互式）
deno run --allow-net ts/demo_socket.ts
```

| 文件 | 覆盖 |
| --- | --- |
| `ts/libary_request.ts` | GET 无参/带参、POST/PATCH/UPDATE/PUT/DELETE、FFI-Symbol 负例 |
| `ts/static_request.ts` | 静态资源内容一致性、404、遍历 |
| `ts/sse_request.ts` | 响应头、事件、id 递增、data JSON、续传、405 |
| `ts/socket_*.ts` | WS 连接、bind/msg/status/remove |

## 5. 压力测试

`socket` 分支提供统一压力测试与结果记录：

| 文件 | 内容 |
| --- | --- |
| `tests/stress_report.h` | JSONL 结果记录、跨平台计时 |
| `tests/test_nng_stress.c` | PAIR1 可靠吞吐 + PUB/SUB 尽力而为 |
| `tests/test_libary_stress.c` | FFI 串行/并发（已记录结果） |

结果文件：`test-results/stress.jsonl`（可用 `STRESS_REPORT_DIR` 覆盖）。

每行字段：

```json
{
  "timestamp": "2026-10-21T...",
  "platform": "windows",
  "suite": "nng",
  "case": "pair1_ipc_throughput",
  "threads": 2,
  "iterations": 5000,
  "duration_ms": 92.4,
  "ops_per_sec": 54110,
  "failures": 0,
  "status": "PASS",
  "notes": "reliable pair1, zero-loss expected",
  "pid": 12345
}
```

查看：

```bash
tail -n 20 test-results/stress.jsonl
```

`sse` 分支不包含压力测试目标；需要时从 `socket` 分支获取。

## 6. CI

- Linux：安装 `criterion`、`pkg-config`，构建后运行测试。
- macOS：构建，临时关闭测试。
- Windows：MSVC + Ninja 构建。

注意：CI 中的 `cyctron_tests`/`-DBUILD_TESTS` 是旧配置，当前 CMake 使用 `enable_testing()` + `tests/CMakeLists.txt`。

## 7. 测试约定

- 测试前确认插件存在于 `./libary/` 或 `./core/`。
- SSE/WS 测试使用真实端口，建议用非默认端口避免冲突。
- 压力测试注意 CI 时间，单个测试建议 < 120 秒。
- 不在测试中修改仓库文件；运行产物写到 `test-results/` 或临时目录。
