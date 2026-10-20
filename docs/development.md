# 开发指南

## 1. 代码结构

```
src/
  main.c            启动、命令行、主循环
  handler.c         总路由（OPTIONS/WS/SSE/API/libary/static）
  handler_libary.c  插件调用
  handler_static.c  静态文件
  handler_ws.c      WebSocket 命令
  handler_sse.c     SSE
  calldyn.c         dlopen + libffi
  event_pusher.c    WS/NNG 事件
  realtime.c        任务表与任务 NNG
include/            对外头文件
examples/           Rust 插件示例
tests/              C 测试
ts/                 Deno 测试与客户端示例
docs/               系统文档（本目录）
todo/               模块设计与完成记录
```

## 2. 新增一个 HTTP 路由

1. 在 `include/server.h`（或新建头文件）声明处理函数。
2. 在 `src/handler.c` 的 `MG_EV_HTTP_MSG` 中加分支，注意顺序：
   - OPTIONS → WebSocket → SSE → `/api/*` → libary 探测 → 静态 → libary 兜底
3. 在 `CMakeLists.txt` 的 `CORE_SOURCES` 中加入新源文件。
4. 补充测试（C 或 TS）与文档。

示例：

```c
if (hm->uri.len == 5 && strncmp(hm->uri.buf, "/ping", 5) == 0) {
    mg_http_reply(c, 200,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "{\"pong\":true}");
    return;
}
```

## 3. 新增一个 libary 插件

1. 创建 Rust cdylib 工程：

```
examples/my_plugin/
  Cargo.toml
  src/lib.rs
```

```toml
[lib]
crate-type = ["cdylib"]
```

2. 导出 C ABI：

```rust
use std::ffi::c_char;

#[no_mangle]
pub extern "C" fn get_handle(json_in: *const c_char, json_out: *mut c_char) -> i32 {
    // 64 KB 输出缓冲
    let out = b"{\"ok\":true}\0";
    unsafe {
        std::ptr::copy_nonoverlapping(out.as_ptr(), json_out as *mut u8, out.len());
    }
    0
}
```

3. 编译并拷贝：

```bash
cargo build --release --manifest-path examples/my_plugin/Cargo.toml
cp examples/my_plugin/target/release/my_plugin.dll libary/   # Windows
```

4. 调用：`GET /my_plugin` → `get_handle`。

5. 测试：新增 `tests/test_my_plugin.c`，在 `tests/CMakeLists.txt` 中用
   `if(EXISTS ...)` 注册。

## 4. 新增一个测试

### C

```cmake
add_executable(test_xxx test_xxx.c ../src/calldyn.c ../crates/log/src/log.c)
target_include_directories(test_xxx PRIVATE
    ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_xxx PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})
add_test(NAME xxx COMMAND test_xxx WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

### TS

在 `ts/` 新建脚本，使用 `deno run --allow-net`，输出 PASS/FAIL 并以非 0 退出码表示失败。

## 5. todo 文档流程

模块开发前先写 `todo/<module>.md`：

```markdown
# TODO: <模块>

## 1. 需求记录
- [ ] 1.1 ...

## 2. 设计与文件
## 3. 测试计划
## 4. 实施阶段
## 5. 验收标准
```

- 未完成：`- [ ]`
- 已完成：`- [*]`

完成后同步更新 `docs/` 与根 `README.md`。

## 6. 分支约定

| 分支 | 用途 |
| --- | --- |
| `main` | 集成主线 |
| `developer` | 历史开发线 |
| `libary` | libary 插件与测试 |
| `socket` | soket 压力测试与实时通道优化 |
| `sse` | SSE 端点与 `sse_demo` |

提交前：

```bash
git diff --check
cmake --build build
ctest --test-dir build --output-on-failure
```

## 7. 代码约定

### 日志

```c
log_info("[SSE] client connected (slot=%d)", slot);
log_warn("[SSE] sse_next failed (rc=%d)", rc);
log_error("[SSE] plugin not found: %s", path);
```

### 错误响应

```c
mg_http_reply(c, 404,
    "Content-Type: application/json\r\n"
    "Access-Control-Allow-Origin: *\r\n",
    "{\"error\":404,\"message\":\"Not Found\"}");
```

### 内存

- `calldyn` 的 `json_out` 由 `malloc` 分配，调用方 `free`。
- `mg_json_get_str` 的返回值用 `mg_free`。
- `mg_json_get_tok` 返回原 JSON 的切片，不释放。

### 字符串安全

- URI/路径拼接前做 `..` 检查。
- 拼 JSON 时使用 `event_escape_json` 或 `mg_json_*`。
- 固定缓冲区注意截断与 `\0`。

## 8. 新增依赖

1. 优先使用 `crates/` 下的子模块。
2. 在 `CMakeLists.txt` 中加入 include 与 link。
3. Windows 需要静态库时放入 `crates/<name>` 并加入忽略规则。
4. 更新 `docs/build.md`。
