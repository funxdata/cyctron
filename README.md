# cyctron（localserver）

small HTTP server running locally

## 功能概览

- HTTP + WebSocket 服务，默认监听 `0.0.0.0:44944`
- **libary 动态库插件**：按 URI 加载 `./libary/<name>.<ext>`，通过 `FFI-Symbol` 头选择导出函数
- 静态文件服务：根路径与无后缀 URI 回退到 `web/index.html`，带后缀 URI 映射到 `web/`
- NNG IPC 事件推送与实时任务调度（`ipc:///tmp/cyctron.sock`）

## 目录结构

```
include/       对外头文件（handler、libary、ws、realtime 等）
src/           服务实现（handler、calldyn、event_pusher、realtime）
src/core/      少量通用工具（URI 扩展名判断）
crates/        第三方子模块（mongoose、nng、libev、log、libffi）
examples/      Rust 动态库示例（rust_demo、soket_demo、libary_demo）
tests/         C 单元测试与压力测试（ctest）
ts/            Deno 接口验证脚本与 WebSocket 示例
todo/          模块待办与完成记录
```

## 开发

* 编译并运行测试

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

* 创建发布包

```
mkdir build
cd build
cmake -G Ninja ..

```

* 同时编译、测试和打包

```
ninja package test
```

## libary 插件

1. 编译示例动态库：

```
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
```

2. 将产物拷贝到 `./libary/`（`.gitignore` 已忽略该目录）：

| 平台 | 产物 |
| --- | --- |
| Linux | `libary/libary_demo.so` |
| macOS | `libary/libary_demo.dylib` |
| Windows | `libary/libary_demo.dll` |

3. 插件导出的 C ABI：

```c
int <method>_handle(const char *json_in, char *json_out); /* 输出缓冲 64KB */
```

`libary_demo` 已实现 `get/post/patch/update/put/delete_handle`。
请求 URI 为 `/libary_demo`，`GET` 参数可用 query string（如 `?id=2`），
其他方法使用 JSON body；`FFI-Symbol` 头可覆盖默认的 `handle` 后缀。

4. 接口验证（需先启动服务）：

```
./cyctron start
deno run --allow-net ts/libary_request.ts
deno run --allow-net ts/static_request.ts
```

## 合并说明

`main` 分支已合并 `libary` 与 `developer`：

- `libary`：统一插件路由、query 解析、路径遍历防护、动态库句柄缓存、libary 测试与 todo
- `developer`：Windows 7 目标、UTF-8 编译选项、Release 子系统、静态链接、NNG v1.12.0 子模块
- 冲突仅在 `CMakeLists.txt` 与 `dytes.json`：前者按 libary 结构整合 developer 的 Windows 选项，
  后者采用 developer 的 1.0.2 版本与发布导出配置

## windows 环境下的一些改变

* 相关依赖

https://www.msys2.org/

```
pacman -S mingw-w64-x86_64-toolchain
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
```

Windows 构建使用仓库内置的静态 libffi：`crates/libffi-packages/libffi.a`。
