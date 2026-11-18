# 已知问题索引

本目录记录对 `main` 分支（2026-11-18）的一次静态代码扫描结果。每条问题独立成文，
包含位置、现象、影响与建议修复方向；修复后请更新本表的状态列。

严重级别：

| 级别 | 含义 |
| --- | --- |
| P0 | 崩溃 / 内存破坏 / 可被远程触发的安全问题 |
| P1 | 功能错误（路由、协议、平台行为不符合预期） |
| P2 | 资源泄漏、并发/健壮性问题、可恢复的异常行为 |
| P3 | 可维护性、死代码、文档与实现漂移 |

状态：`open` / `partial` / `fixed` / `wontfix`。

| 编号 | 级别 | 标题 | 状态 |
| --- | --- | --- | --- |
| [005](./005-client-uuid-path-injection.md) | P0 | 客户端 `uuid` 未校验即拼入 socket 路径与 JSON | open |

## 本轮仓库整理（已处理）

| 事项 | 结果 |
| --- | --- |
| 删除 `.vscode/` | 已 `git rm`（编辑器本地配置不入库） |
| 增加 skills 目录 | 新增 `.reasonix/skills/cyctron-dev/SKILL.md`，供 Reasonix 项目技能发现 |
| build 相关内容 | `.gitignore` 合并为单条 `build*/`；`build/`、`build-*/` 全部忽略，任何构建产物都不提交 |
| 移除 `/api/*` | 按 cyctron 定位（HTTP 同步调用、SSE 等待、WebSocket 双向）删除遗留 API 路由，删除 issue 001；健康检查改用 `/` |
| 处理 002 | 插件失败路径补 `free`；`calldyn` 失败时释放缓冲区并置空；缓冲区提升到 1 MB 且去掉整块 `memset`，删除 issue 002 |
| 处理 003（ABI v2） | 插件入口改为 `int fn(in, in_len, out, out_cap, out_len)`；符号带 `_v2`；输出不足返回 `-1001`，宿主扩容至 16 MB 重试；libary/SSE 默认/WS `call` 已切换，删除 issue 003 |
| 处理 004 | dlopen 缓存全平台使用同一把 `pthread_mutex`；缓存路径改为动态分配，不再截断；缓存满时 `log_warn`，删除 issue 004 |
| 处理 006 | `conn_id` 全链路改为 `uint64_t`；`uint64_t` 的日志/格式化统一用 `%llu` + cast；`event_pusher_get_push_count` 加锁读取，删除 issue 006 |
| 处理 007 | 升级前切换 `c->fn`，由 `MG_EV_WS_OPEN` 统一注册/欢迎；删除 `ws_on_upgrade`；升级失败检查 `c->is_websocket` 并回 426，删除 issue 007 |
| 处理 008 | done 队列上限 256，溢出丢弃并告警；耗时统计毫秒转微秒；执行超过 `timeout_ms` 映射为 `-7`；worker 返回 `-2` busy，删除 issue 008 |
| 处理 009 | SSE `once` 发送后关闭连接并释放槽位；`data` 按行加 `data:` 前缀、`event` 过滤换行；截断输出 `log_warn`，删除 issue 009 |
| 处理 010 | 链表访问统一在 `g_scheduler.mutex` 内，事件发送移到锁外；删除心跳/重启死代码；`rt_start_task` 校验 lib 名，删除 issue 010 |
| 处理 011 | `call_local_dyn_socklibffi` 用 `FFI_EXT` 拼平台扩展名，先走 `plugin_resolve_path`，回退 `./core/`；默认库名改为 `soket_demo`，删除 issue 011 |
| 处理 012 | cyctron 不再读取 `./plugins/`（设计插件调用移至 chatllm）；`plugin_resolve_path` 只查 `libary` 并校验名字，删除 issue 012 |
| 处理 013 | `BUILD_TESTS` 已生效；`CMAKE_SYSTEM_VERSION` 移到 `project()` 之前；CI 文档统一到 `dytes-release.yml`，删除 issue 013 |
| 处理 014 | 删除 `build_ffi_path`、`include/config.h`；`uri.h` 加 include guard；1 MiB 限制统一到 `WS_MAX_MESSAGE_BYTES`，删除 issue 014 |
| 处理 015 | 增加 SIGINT/SIGTERM 与退出清理；`stop` 通过 `cyctron.pid` 终止；`reload` 明确不支持；`-port` 校验 1–65535，删除 issue 015 |
| 处理 016 | 新增 `json_validate`（无分配、深度上限 64）；`call` payload 与 args 做完整 JSON 校验；`event_push_with_type` 非法 data 丢弃并 `log_warn`，删除 issue 016 |
| 处理 017 | 按当前实现更新 README/docs：`.dytes` 产物与启动目录、内置首页、libary 路径、ctest/CI、分支与测试说明，删除 issue 017 |
| 处理 018 | 删除 9 个旧/不匹配测试文件；新增 `json_validate`/`uri_utils` 测试；清理 ctest 注释与测试文档，删除 issue 018 |
| 处理 019 | 静态资源优先、未知路径 404；`web/index.html` 优先；SPA fallback 仅限 `Accept: text/html`，删除 issue 019 |
