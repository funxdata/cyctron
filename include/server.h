#include <stddef.h>

struct mg_connection;
struct mg_http_message;

// -------------------------------
// 检查 URI 是否含路径遍历（"..")
// -------------------------------
int uri_has_traversal(const char *uri);

// -------------------------------
// 解析静态文件 URI 到磁盘路径（相对项目根）
// 返回 0 成功，-1 非法（含 ..）
// -------------------------------
int static_resolve_path(const char *uri, char *out, size_t out_size);

// -------------------------------
// HTTP Server 事件处理函数声明
// -------------------------------
void ev_handler(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_static(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_libary(struct mg_connection *c, int ev, void *ev_data);

// -------------------------------
// WebSocket 事件处理
// -------------------------------
void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data);
