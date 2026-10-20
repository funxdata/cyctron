# TODO: libary 库开发

## 1. 需求记录
- [*] 1.1 将需求转换为 todo 清单（本文件）

## 2. 创建 libary 目录，Rust 生成对应库文件
- [ ] 2.1 创建 `examples/libary_demo` Rust cdylib 工程（产物 `liblibary_demo.so`）
- [ ] 2.2 导出 FFI 符号：`get/post/patch/update/put/delete_handle`（C ABI：`int fn(const char *in, char *out)`）
- [ ] 2.3 功能实现：GET 列表/按 id 查询、POST 创建、PATCH 部分更新、UPDATE 全量更新、PUT/DELETE

## 3. C 端只调用 libary 库，删除 os/chat/database 调用
- [ ] 3.1 删除 `handler_os.c`、`handler_chat.c`、`handler_database.c` 及路由分发
- [ ] 3.2 清理 `server.h`（CT_* 枚举/声明）、`global.h`（OS/CHAT/DATABASE_DIR）、`CMakeLists.txt` 源列表
- [ ] 3.3 `handler_libary.c` 支持 query string 剥离（GET 带参必需）、路径遍历防护
- [ ] 3.4 非 API/静态/WS 的请求统一路由到 libary handler

## 4. Rust 库文件拷贝到 ./libary/
- [ ] 4.1 `cargo build --release` 编译通过
- [ ] 4.2 产物拷贝为 `./libary/libary_demo.so`（.gitignore 忽略，不入库）

## 5. tests 单元测试 + 压力测试
- [ ] 5.1 `tests/test_libary.c`：全符号功能断言 + 边界（NULL、非法 JSON、未知符号）
- [ ] 5.2 `tests/test_libary_stress.c`：串行与多线程并发 FFI 压测，断言零失败并输出吞吐
- [ ] 5.3 接入 `tests/CMakeLists.txt` 并在 ctest 通过

## 6. ts 接口调用测试
- [ ] 6.1 GET 无参数请求断言
- [ ] 6.2 GET 带参数（query）请求断言
- [ ] 6.3 POST / PATCH / UPDATE 无参与带参请求断言（含 PUT、DELETE、FFI-Symbol 负例）
- [ ] 6.4 deno 实跑全部通过

## 7. 提交到 libary 分支
- [ ] 7.1 创建 `libary` 分支，全部测试通过后提交模块代码（.so 产物不入库）
