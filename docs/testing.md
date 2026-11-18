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
cp examples/libary_demo/target/release/libary_demo.dll .dytes/libary/          # Windows
cp examples/libary_demo/target/release/libibary_demo.so .dytes/libary/libary_demo.so  # Linux

# SSE 插件
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
cp examples/sse_demo/target/release/sse_demo.dll .dytes/libary/                # Windows
cp examples/sse_demo/target/release/libsse_demo.so .dytes/libary/sse_demo.so   # Linux
```

插件缺失时，相关 `add_test` 不会注册（CMake 里用 `if(EXISTS ...)` 判断）。

## 3. C 测试

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
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
| `plugin_path` | `tests/test_plugin_path.c` | libary 路径解析、`lib<name>` 回退、非法名字 |
| `libary` | `tests/test_libary.c` | libary_demo 全符号、边界、未知符号 |
| `libary_stress` | `tests/test_libary_stress.c` | 串行 20000 + 8 线程混合 |
| `sse` | `tests/test_sse.c` | SSE 格式化、sse_demo 调用、续传 |
| `static`（非 Windows） | `tests/test_static.c` | 静态路径解析、404、遍历拒绝 |
| `static_spa` | `tests/test_static_spa.c` | SPA fallback 判定 |
| `json_validate` | `tests/test_json_validate.c` | JSON 结构校验（对象/数组/字符串/数字/尾随垃圾） |
| `uri_utils` | `tests/test_uri_utils.c` | URI 扩展名、路径遍历检查 |
| `ws_call` / `ws_call_parse` / `ws_call_errors` / `ws_call_response` | `tests/test_ws_call*.c` | `call` 解析、错误码、响应 JSON |
| `ws_message_limit` | `tests/test_ws_message_limit.c` | WebSocket 消息大小限制 |
| `ws_parse_legacy` | `tests/test_ws_parse_legacy.c` | 旧命令 JSON 解析 |
| `ws_worker` | `tests/test_ws_worker.c` | worker 队列、并发、超时、统计 |
| `event_push_dynamic` | `tests/test_event_push_dynamic.c` | 事件格式化 |
| `realtime_ownership` | `tests/test_realtime_ownership.c` | 任务按连接清理 |

### 3.2 已移除的旧测试

旧 realtime API 测试（`test_basic.c`、`test_data.c`、`test_multiple.c`、
`test_pause_resume.c`、`test_performance.c`、`test_realtime.c`）以及不再匹配
当前 ABI/插件的 `test_calldyn.c`、`test_dyn_socket.c`、`test_nng_ipc.c` 已删除。

## 4. TS 接口测试

先启动服务：

```bash
cd .dytes
./cyctron.exe start -port 44944    # Windows
./cyctron start -port 44944        # Linux / macOS
```

再运行：

```bash
# libary 插件接口
deno run --allow-net ts/libary_request.ts 44944

# 静态文件（使用 .dytes/web 夹具）
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

统一压力测试与结果记录：

| 文件 | 内容 |
| --- | --- |
| `tests/stress_report.h` | JSONL 结果记录、跨平台计时 |
| `tests/test_nng_stress.c` | PAIR1 可靠吞吐 + PUB/SUB 尽力而为 |
| `tests/test_libary_stress.c` | FFI 串行/并发（已记录结果） |

结果文件：`docs/results/stress.jsonl`（可用 `STRESS_REPORT_DIR` 覆盖）。

已归档结果：[results/stress.jsonl](./results/stress.jsonl)；
初步分析见 [results/README.md](./results/README.md)。

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
tail -n 20 docs/results/stress.jsonl
```

所有压力测试目标都在 `main`；测试结果统一写入 `docs/results/stress.jsonl`。

## 6. CI

- 发布 workflow `dytes-release.yml` 在 Windows / Linux / macOS 上构建，
  使用 `-DBUILD_TESTS=OFF`，不在发布流程中运行测试。
- 本地或独立测试任务使用默认 `BUILD_TESTS=ON`：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 7. 测试约定

- 测试前确认插件存在于 `.dytes/libary/` 或 `./core/`。
- SSE/WS 测试使用真实端口，建议用非默认端口避免冲突。
- 压力测试注意 CI 时间，单个测试建议 < 120 秒。
- 压力测试会追加写入 `docs/results/stress.jsonl`；其他测试产物写到临时目录。
