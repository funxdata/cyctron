# 005 客户端 `uuid` 未校验即拼入 socket 路径与 JSON

- 级别：P0
- 位置：`src/handler_ws.c:88-101`、`src/handler_ws.c:73-80`、`src/realtime.c:111-117`
- 状态：open

## 现象

WebSocket `bind` 命令的 `uuid` 来自客户端，未经任何白名单校验：

```c
snprintf(socket_path, sizeof(socket_path), "/tmp/cyctron/%s.sock", req->uuid);
...
strncpy(node->task.uuid, uuid, sizeof(node->task.uuid) - 1);
snprintf(node->task.socket_id, sizeof(node->task.socket_id),
         "/tmp/cyctron_%s.sock", uuid);
```

- `uuid = "../../etc/x"` 会把 NNG 监听的 IPC 路径指到预期目录之外。
- 超长 `uuid` 被 `snprintf` 截断，不同的 `uuid` 可能映射到同一个 socket 路径。
- `send_event_to()` 直接用 `snprintf` 拼 `{"uuid":"%s"}`，`uuid` 含 `"` 即可注入
  JSON/截断事件结构（对比 `realtime.c:send_rt_event` 做了转义）。

## 影响

本地路径穿越、任务 ID 冲突、事件 JSON 注入。服务默认绑定 `0.0.0.0`，局域网内可触发。

## 建议

- 复用 `ws_call.c` 的 `ws_safe_name()` 或 `plugin_name_is_safe()` 校验 `uuid`
  （建议限制为 `[A-Za-z0-9_-]{1,63}`），不合法直接回 `error`。
- socket 文件名使用服务端生成的 `task_id`，不要使用客户端字符串。
- 所有拼接 JSON 的地方统一用 `event_escape_json`。
- `rt_create_task` 的 `socket_path` 参数当前被忽略，建议删除该参数以免误导。
