# TODO: 静态文件处理

## 1. 在 cyctron 中，创建对静态文件的处理
- [*] 1.1 实现 `GET/HEAD` 路由到静态文件的分发（`handler.c` → `handler_static.c`）
- [*] 1.2 无后缀 URI 回退到 `index.html`；带后缀 URI 按 `assets/` 目录查找文件
- [*] 1.3 按 MIME 类型正确返回 `Content-Type`（html/js/json/img 等）
- [*] 1.4 路径安全：拒绝 `..` 遍历（已实现 400，验证覆盖各分支）

## 2. 在 tests 中添加单元测试
- [*] 2.1 为静态文件处理添加单元测试（`tests/test_static.c`：命中、回退、404、遍历拒绝）
- [*] 2.2 将新测试接入 `tests/CMakeLists.txt` 并注册 ctest

## 3. 编译完成后，将 cyctron 执行文件放到主目录中
- [*] 3.1 CMake 设置输出路径，`cyctron` 可执行文件生成到项目主目录
- [*] 3.2 全量编译通过，主目录出现 `./cyctron`

## 4. 在主目录中创建 assets 目录及相关文件
- [*] 4.1 创建 `assets/img/`、`assets/js/`、`assets/json/` 目录（位于 `web/` 根下）
- [*] 4.2 放入示例文件（`logo.svg`、`app.js`、`data.json`，index.html 引用它们）

## 5. 执行 `./cyctron start` 启动应用
- [*] 5.1 启动成功，日志输出监听地址
- [*] 5.2 通过浏览器/curl 验证静态资源可访问（`/assets/js/app.js` 等）

## 6. 在 ts 目录中创建请求文件，保证请求准确
- [*] 6.1 编写 `ts/static_request.ts`：对 `/`、`/index.html`、各 assets 文件的请求与断言
- [*] 6.2 用 deno 实际运行验证全部请求通过（9/9 PASS）
