# 动态库插件（libary）

插件是 cyctron 的核心扩展机制：HTTP 请求按 URI 映射到动态库，再通过 libffi
调用库导出的 C ABI 函数。

设计插件的调用已迁移到 chatllm，本服务只从 `./libary/` 加载能力库：

1. **能力库**：`./libary/<name>.<ext>`；
2. 兼容 Linux 的 `lib<name>.<ext>` 命名。

## 1. 目录与扩展名

| 平台 | 库目录 | 扩展名 |
| --- | --- | --- |
| Windows | `.\libary` | `.dll` |
| Linux | `./libary` | `.so` |
| macOS | `./libary` | `.dylib` |

目录与扩展名由 `include/global.h` 定义，路径解析集中在 `src/plugin_path.c`。
`./libary/` 被 `.gitignore` 忽略，插件产物不入库。`./plugins/` 由 chatllm 管理，
cyctron 不再读取。

## 2. URI 与符号映射

请求：

```
POST /libary_demo
Content-Type: application/libary
FFI-Symbol: add
```

映射：

| 部分 | 规则 |
| --- | --- |
| 插件路径 | `./libary/<name><ext>` → 回退 `./libary/lib<name><ext>` |
| 方法 | 转小写，作为前缀 |
| 符号 | `<method>_<FFI-Symbol>_v2`；`FFI-Symbol` 缺省为 `test`，即 `<method>_test_v2` |
| 示例 | `POST` + `add` → `post_add` |

`FFI-Symbol` 头不区分大小写（服务端读取 `FFI-Symbol`）。

## 3. 参数规则

`handler_libary.c` 的组装顺序：

1. 若存在 query string，优先解析：
   - `?id=2` → `{"id":2}`
   - 其他参数 → `{"q":"<第一个值>"}`
2. 否则若存在请求体，原样作为 JSON 传入。
3. 都没有则传 `NULL`。

插件需要对空/非法 JSON 做容错。

## 4. C ABI

```c
int <method>_<symbol>_v2(
    const char *json_in, size_t json_in_len,
    char *json_out, size_t json_out_cap, size_t *json_out_len);
```

- `json_in`：UTF-8 JSON，可能为 `NULL`（此时 `json_in_len == 0`）。
- `json_out`：宿主提供的输出缓冲区，首次 1 MB。
- `json_out_len`：插件写入实际 JSON 字节数（不含结尾 `\0`）。
- 返回值：
  - `0`：成功；
  - `-1001`（`CYCTRON_FFI_ENOSPC`）：输出不足，`*json_out_len` 写入所需字节数，
    宿主会扩容（上限 16 MB）后重试一次；
  - 其它负值：失败，HTTP 返回 404 `Plugin Not Found`。

服务端处理：

- `dlopen` 句柄缓存（最多 32 个），库常驻，库内 `static` 状态在多次调用间保留。
- 调用是同步的，运行在 mongoose 主线程（WebSocket `call` 走 worker 线程）。
- 输出超过 16 MB 时返回错误，不再静默截断。

## 5. 响应与错误

成功：

```
HTTP/1.1 200 OK
Content-Type: application/json
Access-Control-Allow-Origin: *

<插件输出的 JSON>
```

失败：

```json
{"error":404,"message":"Plugin Not Found"}
```

以下情况都会返回 404：

- 动态库不存在或加载失败；
- 符号不存在；
- 插件返回非 0。

URI 含 `..` 返回 400。

## 6. 示例：libary_demo

`examples/libary_demo` 是一个 Rust cdylib，导出：

| 方法 | 符号 | 行为 |
| --- | --- | --- |
| GET | `get_test_v2` | 无参返回列表；`{"id":N}` 返回单条；不存在返回 `not_found` |
| POST | `post_test_v2` | 空 body → `empty_body`；否则创建并自增 id |
| PATCH | `patch_test_v2` | `{"id":N,"patch":{...}}` 部分更新 |
| UPDATE | `update_test_v2` | `{"id":N,...}` 全量替换 |
| PUT | `put_test_v2` | 与 UPDATE 同语义 |
| DELETE | `delete_test_v2` | `{"id":N}` 删除 |

构建与放置：

```bash
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
# Windows
cp examples/libary_demo/target/release/libary_demo.dll .dytes/libary/
# Linux
cp examples/libary_demo/target/release/libibary_demo.so .dytes/libary/libary_demo.so
```

从 `.dytes/` 启动服务后，`./libary` 指向测试插件目录：

```bash
cd .dytes && ./cyctron.exe start -port 44944
```

接口示例：

```bash
# 列表
curl http://127.0.0.1:44944/libary_demo

# 按 id 查询（query）
curl "http://127.0.0.1:44944/libary_demo?id=2"

# 创建
curl -X POST http://127.0.0.1:44944/libary_demo \
  -H "Content-Type: application/libary" \
  -d '{"name":"delta","type":"js"}'

# 用 FFI-Symbol 覆盖方法后缀
curl -X POST http://127.0.0.1:44944/libary_demo \
  -H "FFI-Symbol: health" \
  -d '{}'
```

## 7. 编写自己的插件

1. 创建 Rust cdylib 工程（或 C/C++ 动态库）。
2. 导出 `process_init` 或业务符号，签名为上述 C ABI。
3. 编译为 `.so` / `.dll` / `.dylib`。
4. 放到 `./libary/<name>.<ext>`。
5. 通过 `/<name>` 调用。

Rust 模板：

```rust
use std::ffi::c_char;

#[no_mangle]
pub extern "C" fn post_hello_v2(
    json_in: *const c_char,
    json_in_len: usize,
    json_out: *mut c_char,
    json_out_cap: usize,
    json_out_len: *mut usize,
) -> i32 {
    let _ = (json_in, json_in_len);
    let out = b"{\"ok\":true,\"message\":\"hello\"}";
    if out.len() + 1 > json_out_cap {
        return -1001; // ENOSPC
    }
    unsafe {
        *json_out_len = out.len();
        std::ptr::copy_nonoverlapping(out.as_ptr(), json_out as *mut u8, out.len());
        *json_out.add(out.len()) = 0;
    }
    0
}
```

## 8. 安全与限制

- 插件名来自 URI，当前只做路径遍历校验，没有白名单；不要把不可信 URI 直接映射到任意目录。
- 插件调用会阻塞主线程，避免在插件内做长时间计算或等待。
- 输出缓冲按需扩容，上限 16 MB；更大响应需要插件自行分页。
- Windows 下插件必须与 cyctron 的架构一致（x64），并确保依赖 DLL 可被找到。
- `FFI-Symbol` 未做字符校验，建议只使用 `[A-Za-z0-9_]`。

## 9. 测试

- `tests/test_libary.c`：全符号功能与边界。
- `tests/test_libary_stress.c`：串行/并发压力。
- `ts/libary_request.ts`：HTTP 层接口断言。

运行前需把 `libary_demo` 产物放入 `.dytes/libary/`（本地测试）或运行目录的 `libary/`。
