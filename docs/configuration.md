# 配置

## 1. 命令行

```bash
./cyctron help
./cyctron start [-port <port>]
./cyctron stop
./cyctron reload
```

| 命令 | 说明 |
| --- | --- |
| `start` | 启动 HTTP + WebSocket 服务，默认 `0.0.0.0:44944` |
| `start -port 8080` | 指定端口 |
| `stop` | 预留（当前仅打印 TODO） |
| `reload` | 预留（当前仅打印 TODO） |
| `help` | 显示帮助 |

## 2. 端口与监听地址

- 默认 host：`0.0.0.0`
- 默认 port：`44944`
- 修改方式：`start -port <port>`

## 3. 目录约定

| 目录 | 用途 | 是否入库 |
| --- | --- | --- |
| `web/` | 静态文件根目录 | 否（`.gitignore`） |
| `libary/` | 插件动态库 | 否 |
| `core/` | soket 插件（`libsoket_demo`） | 否 |
| `test-results/` | 压力测试结果 | 否 |
| `build*/` | 构建目录 | 否 |

静态文件规则：

- `/` 或无后缀 → `web/index.html`
- 带后缀 → `web/<uri>`

## 4. 平台路径（include/global.h）

| 宏 | Windows | Linux | macOS |
| --- | --- | --- | --- |
| `PATH_SEP` | `\` | `/` | `/` |
| `WEB_ROOT` | `.\web` | `./web` | `./web` |
| `LIBARY_DIR` | `.\libary` | `./libary` | `./libary` |
| `FFI_EXT` | `.dll` | `.so` | `.dylib` |

## 5. 编译宏（include/config.h）

| 宏 | 默认 | 说明 |
| --- | --- | --- |
| `CT_ENABLE_LOG` | 1 | 日志开关 |
| `CT_IO_SIZE` | 512 | I/O 缓冲 |
| `CT_MAX_RECV_SIZE` | 3 MB | 最大接收/请求体上限 |
| `CT_MAX_HTTP_HEADERS` | 30 | 最大头部数 |
| `CT_HTTP_INDEX` | `index.html` | 目录默认页 |
| `CT_PATH_MAX` | PATH_MAX/128 | 路径长度 |
| `CT_SOCK_LISTEN_BACKLOG_SIZE` | 128 | listen backlog |
| `CT_ENABLE_MD5` | 1 | MD5 |
| `CT_ENABLE_CHACHA20` | 1 | ChaCha20 |
| `CT_ENABLE_TCPIP` | 0 | 内建网络栈（未启用） |

> 说明：`config.h` 中保留了 mongoose/嵌入式网络栈的历史宏，
> 当前主程序实际使用的是 `crates/mongoose` 与系统 socket。

## 6. CORS

所有普通响应带：

```
Access-Control-Allow-Origin: *
```

预检允许的方法与头部见 [http-api.md](./http-api.md)。

## 7. 环境变量

| 变量 | 作用 | 使用方 |
| --- | --- | --- |
| `STRESS_REPORT_DIR` | 压力测试结果目录 | `tests/stress_report.h`（`socket` 分支） |

## 8. 日志

- 使用 `crates/log`，默认输出到控制台/stderr。
- 组件前缀：`[WS]`、`[SSE]`、`[Scheduler]`、`[NNG]`、`[EventPusher]`、`[CALDYN]`。
- Release 构建在 Windows 下使用 `-mwindows`（无控制台），调试时建议使用 Debug 构建。

## 9. 发布配置

`dytes.json`：

```json
{
  "name": "cyctron",
  "version": "1.0.2",
  "description": "local http server",
  "type": "core",
  "exports": {
    "./build/release/cyctron.exe": "cyctron.exe"
  }
}
```

CI（`.github/workflows/`）读取该文件中的版本号，构建后上传到又拍云 S3。
