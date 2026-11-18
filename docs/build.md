# 构建

## 1. 依赖

| 依赖 | 用途 |
| --- | --- |
| CMake ≥ 3.28 | 构建系统 |
| C11 编译器 | GCC / MinGW / MSVC / Clang |
| `crates/mongoose` | HTTP/WS/JSON |
| `crates/nng` | IPC |
| `crates/libev` | 事件循环（构建依赖） |
| `crates/log` | 日志 |
| libffi | 动态库调用 |
| Rust（可选） | 插件示例 |
| Deno（可选） | TS 接口测试 |

子模块初始化：

```bash
git submodule update --init --recursive
```

## 2. 快速构建

### Linux / macOS

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

需要：

```bash
# Debian/Ubuntu
sudo apt install cmake ninja-build gcc pkg-config libffi-dev
# macOS
brew install cmake libffi pkg-config
```

Linux/macOS 通过 `pkg-config` 查找 libffi。

### Windows（MinGW）

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

依赖 MSYS2：

```bash
pacman -S mingw-w64-x86_64-toolchain
```

Windows 使用仓库内置的静态 libffi：

```
crates/libffi-packages/libffi.a
crates/libffi-packages/ffi.h
crates/libffi-packages/ffitarget.h
```

CMake 会自动把该目录加入 include 并链接 `libffi.a`。

## 3. 产物

- Windows：`.dytes/cyctron.exe`
- Linux/macOS：`.dytes/cyctron`

由 `RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.dytes"` 指定；从 `.dytes/`
启动时 `./libary`、`./web` 指向本地测试资源。

## 4. 构建插件

### libary_demo

```bash
cargo build --release --manifest-path examples/libary_demo/Cargo.toml
# Windows
cp examples/libary_demo/target/release/libary_demo.dll .dytes/libary/
# Linux
cp examples/libary_demo/target/release/libibary_demo.so .dytes/libary/libary_demo.so
```

### sse_demo

```bash
cargo build --release --manifest-path examples/sse_demo/Cargo.toml
# Windows
cp examples/sse_demo/target/release/sse_demo.dll .dytes/libary/
# Linux
cp examples/sse_demo/target/release/libsse_demo.so .dytes/libary/sse_demo.so
```

### soket_demo / rust_demo

```bash
cargo build --release --manifest-path examples/soket_demo/Cargo.toml
cargo build --release --manifest-path examples/rust_demo/Cargo.toml
```

soket 插件的产物通常放到 `./core/soket_demo.<ext>`（由 `calldyn.c` 的 `call_local_dyn_socklibffi` 查找）。

## 5. 测试构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

测试目标与插件依赖见 [testing.md](./testing.md)。

## 6. CI

| 工作流 | 平台 | 说明 |
| --- | --- | --- |
| `.github/workflows/dytes-release.yml` | Windows / Linux / macOS | tag 触发；按平台构建后调用 `dytes publish` |

发布构建使用 `-DBUILD_TESTS=OFF`，不生成测试目标；测试在本地或独立测试任务中运行。
CI 读取 `dytes.json` 的 `name`/`version`，由 dytes 上传到：
`core/cyctron/<version>/<platform>`。

## 7. 常见构建问题

| 问题 | 处理 |
| --- | --- |
| `libffi.a not found` | 确认 `crates/libffi-packages/libffi.a` 存在；Windows 需要该静态包 |
| 找不到 `nng/nng.h` | 初始化子模块 `crates/nng` |
| `pkg-config` 找不到 libffi | 安装 `libffi-dev` 或设置 `PKG_CONFIG_PATH` |
| MinGW 链接失败 | 使用 `-G "MinGW Makefiles"`，并确保在 MSYS2 MinGW shell 中 |
| Release 无控制台 | Windows Release 使用 `-mwindows`；调试用 Debug |
| `mongoose.c` 编译错误 | 确认 `crates/mongoose` 子模块版本与 CMakeLists 期望的版本匹配 |

## 8. 清理

```bash
rm -rf build build-*   # Windows: rmdir /s build
cargo clean --manifest-path examples/sse_demo/Cargo.toml
```
