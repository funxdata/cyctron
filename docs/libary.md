# libary 动态库插件

libary 是 cyctron 的核心扩展机制：HTTP 请求按 URI 映射到 `./libary/` 下的动态库，
再通过 libffi 调用库导出的 C ABI 函数。

## 1. 目录与扩展名

| 平台 | 目录 | 扩展名 |
| --- | --- | --- |
| Windows | `.\libary` | `.dll` |
| Linux | `./libary` | `.so` |
| macOS | `./libary` | `.dylib` |

目录与扩展名由 `include/global.h` 定义。`./libary/` 已被 `.gitignore` 忽略，
插件产物不入库。

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
| 插件路径 | `./libary/` + URI 去掉前导 `/` + 平台扩展名 |
| 方法 | 转小写，作为前缀 |
| 符号 | `<method>_<FFI-Symbol>`；`FFI-Symbol` 缺省为 `handle` |
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
int <method>_<symbol>(const char *json_in, char *json_out);
```

- `json_in`：UTF-8 JSON 字符串，可能为 `NULL`。
- `json_out`：64 KB 输出缓冲区，插件必须写入以 `\0` 结尾的 JSON。
- 返回值：`0` 成功；非 0 视为失败，HTTP 返回 404 `Plugin Not Found`。

服务端处理：

- `dlopen` 句柄缓存（最多 32 个），库常驻，库内 `static` 状态在多次调用间保留。
- 调用是同步的，运行在 mongoose 主线程。
- 输出超过 64 KB 会被截断。

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
| GET | `get_handle` | 无参返回列表；`{"id":N}` 返回单条；不存在返回 `not_found` |
| POST | `post_handle` | 空 body → `empty_body`；否则创建并自增 id |
| PATCH | `patch_handle` | `{"id":N,"patch":{...}}` 部分更新 |
| UPDATE | `update_handle` | `{"id":N,...}` 全量替换 |
| PUT | `put_handle` | 与 UPDATE 同语义 |
| DELETE | `delete_handle` | `{"id":N}` 删除 |

构建与放置：

```bash
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
# Windows
cp examples/libary_demo/target/release/libary_demo.dll libary/
# Linux
cp examples/libary_demo/target/release/libibary_demo.so libary/libary_demo.so
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
pub extern "C" fn post_hello(json_in: *const c_char, json_out: *mut c_char) -> i32 {
    let out = b"{\"ok\":true,\"message\":\"hello\"}\0";
    unsafe {
        std::ptr::copy_nonoverlapping(out.as_ptr(), json_out as *mut u8, out.len());
    }
    0
}
```

## 8. 安全与限制

- 插件名来自 URI，当前只做路径遍历校验，没有白名单；不要把不可信 URI 直接映射到任意目录。
- 插件调用会阻塞主线程，避免在插件内做长时间计算或等待。
- 输出缓冲固定 64 KB，大响应需自行分页。
- Windows 下插件必须与 cyctron 的架构一致（x64），并确保依赖 DLL 可被找到。
- `FFI-Symbol` 未做字符校验，建议只使用 `[A-Za-z0-9_]`。

## 9. 测试

- `tests/test_libary.c`：全符号功能与边界。
- `tests/test_libary_stress.c`：串行/并发压力（`socket` 分支提供统一结果记录）。
- `ts/libary_request.ts`：HTTP 层接口断言。

运行前需把 `libary_demo` 产物放入 `./libary/`。
