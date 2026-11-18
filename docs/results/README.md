# 压力测试结果与分析

## 1. 数据来源

- 原始数据：[stress.jsonl](./stress.jsonl)（JSONL，每行一次 ctest 结果）。
- 采集环境：Windows，MinGW/GCC 16.1.0，`libary_demo` 插件（ABI v2）。
- 采集时间：2026-11-18 04:38 / 05:08 UTC（本地 12:38 / 13:08）。
- 当前归档只包含 `libary_stress`（FFI）结果；NNG 压测结果可另行追加到同一 JSONL。
- 复现命令：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R "^libary_stress$" --output-on-failure
```

`test_libary_stress` 和 `test_nng_stress` 默认把结果追加到
`docs/results/stress.jsonl`（可用 `STRESS_REPORT_DIR` 覆盖）；本目录即结果归档位置，
运行测试会直接更新 `stress.jsonl`。

## 2. 结果汇总

| 时间 (UTC) | 用例 | 线程 | 迭代 | 耗时 (ms) | 吞吐 (ops/s) | 失败 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-11-18T04:38:41Z | `serial_get` | 1 | 20000 | 253.316 | 78,952.9 | 0 |
| 2026-11-18T04:39:04Z | `concurrent_mixed` | 8 | 40000 | 22,495.973 | 1,778.1 | 0 |
| 2026-11-18T05:08:09Z | `serial_get` | 1 | 20000 | 312.901 | 63,918.0 | 0 |
| 2026-11-18T05:08:41Z | `concurrent_mixed` | 8 | 40000 | 31,933.676 | 1,252.6 | 0 |

- `serial_get`：单线程只读，约 **64k–79k ops/s**。
- `concurrent_mixed`：8 线程混合读写（get/post/patch），总吞吐约 **1.25k–1.78k ops/s**，
  单线程等效约 **156–222 ops/s**。
- 两次运行波动：`serial_get` 约 **-19%**，`concurrent_mixed` 约 **-30%**，
  并发结果对运行环境更敏感。
- 并发总吞吐约为单线程只读的 **2%**；注意两者工作负载不同（只读 vs 混合读写），
  不能直接当作纯扩展性对比。

## 3. 初步观察

1. **并发没有扩展**：8 线程总吞吐远低于单线程只读，且每线程等效吞吐明显下降。
2. **全局串行点**：`calldyn.c` 的 `dl_get()` 对每次调用都持有同一把 `g_dl_mutex`
   做缓存查找，是宿主侧的全局串行点。
3. **每次调用分配 1 MB**：`call_local_dyn_libffi_v2()` 每次 `malloc/free` 初始
   1 MB 输出缓冲，8 线程下会放大分配器和内存带宽竞争。
4. **插件全局读写锁**：`libary_demo` 使用 `RwLock<HashMap>`，`post`/`patch` 持写锁；
   混合读写负载会放大锁竞争。
5. **工作负载差异**：`serial_get` 是纯读，`concurrent_mixed` 中约一半是写操作
   （`post`/`patch`），目前还缺少「8 线程只读」和「8 线程只写」的对照数据。

## 4. 后续分析方向

1. 增加多线程只读/只写基准，分离锁竞争与分配开销。
2. 评估线程本地输出缓冲，避免每次调用 `malloc/free` 1 MB。
3. 评估 `dl_get` 的无锁读或按路径分片锁（注意 Windows 内存模型）。
4. 缩小 `libary_demo` 写锁范围或改用分片结构。
5. 在 CI 中归档 JSONL 并设置吞吐阈值（例如 serial ≥ 50k ops/s）。
