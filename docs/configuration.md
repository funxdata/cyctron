# 配置

## 1. 命令行

```bash
cd .dytes
./cyctron.exe help
./cyctron.exe start [-port <port>]
./cyctron.exe stop
./cyctron.exe reload
```

| 命令 | 说明 |
| --- | --- |
| `start` | 启动 HTTP + WebSocket 服务，默认 `0.0.0.0:44944` |
| `start -port 8080` | 指定端口（1–65535） |
| `stop` | 读取启动目录的 `cyctron.pid` 并停止服务；Linux/macOS 发 SIGTERM，Windows 调用 TerminateProcess |
| `reload` | 不支持（当前没有配置文件），返回非 0 |
| `help` | 显示帮助 |

> `cyctron.pid` 在启动目录生成，已加入 `.gitignore`；`stop` 需要在同一目录执行。
> `SIGINT`/`SIGTERM` 会触发主循环退出，并按逆序清理 worker、realtime、NNG 与 mongoose。

## 2. 端口与监听地址

- 默认 host：`0.0.0.0`
- 默认 port：`44944`
- 修改方式：`start -port <port>`

## 3. 目录约定

| 目录 | 用途 | 是否入库 |
| --- | --- | --- |
| `.dytes/` | 本地测试环境（构建产物、`libary`、`web` 夹具、dytes test 目录） | 否 |
| `web/` | 可选静态资源目录 | 否（`.gitignore`） |
| `libary/` | 运行时能力库动态库 | 否 |
| `core/` | soket 插件（`soket_demo`） | 否 |
| `docs/results/` | 压力测试结果归档 | 是 |
| `build*/` | 构建目录 | 否 |

静态文件规则：

- `/`、`/index.html` → 优先 `web/index.html`，不存在时返回内置首页
- 带后缀或 `/assets/*` → `web/<uri>`，缺失返回 404
- 无后缀页面路由 → 先尝试 libary 插件；非插件且 `Accept: text/html` 时回退首页

## 4. 平台路径（include/global.h）

| 宏 | Windows | Linux | macOS |
| --- | --- | --- | --- |
| `PATH_SEP` | `\` | `/` | `/` |
| `WEB_ROOT` | `.\web` | `./web` | `./web` |
| `LIBARY_DIR` | `.\libary` | `./libary` | `./libary` |
| `FFI_EXT` | `.dll` | `.so` | `.dylib` |

## 5. 编译宏

主程序没有集中的 `config.h`；平台路径/扩展名由 `include/global.h` 控制，
请求体上限由 mongoose 默认配置决定。

## 6. CORS

所有普通响应带：

```
Access-Control-Allow-Origin: *
```

预检允许的方法与头部见 [http-api.md](./http-api.md)。

## 7. 环境变量

| 变量 | 作用 | 使用方 |
| --- | --- | --- |
| `STRESS_REPORT_DIR` | 压力测试结果目录 | `tests/stress_report.h` |

## 8. 日志

- 使用 `crates/log`，默认输出到控制台/stderr。
- 组件前缀：`[WS]`、`[SSE]`、`[Scheduler]`、`[NNG]`、`[EventPusher]`、`[CALDYN]`。
- Release 构建在 Windows 下使用 `-mwindows`（无控制台），调试时建议使用 Debug 构建。

## 9. 发布配置

`dytes.json`：

```json
{
  "name": "cyctron",
  "version": "1.0.19",
  "description": "local http server",
  "type": "core",
  "platform-exports": {
    "windows": { "./.dytes/cyctron.exe": "cyctron.exe" },
    "linux": { "./.dytes/cyctron": "cyctron.bin" },
    "macos": { "./.dytes/cyctron": "cyctron.app" }
  }
}
```

发布 workflow `.github/workflows/dytes-release.yml` 在 tag 触发时读取该文件，
按平台构建后调用 `dytes publish`。
