#define MAX_METHOD        16
#define MAX_URI          256
#define MAX_CONTENT_TYPE 128

struct mg_connection;
struct mg_http_message;

// -------------------------------
// Content-Type 枚举
// -------------------------------
typedef enum {
    CT_STATIC,      // 静态文件
    CT_DATABASE,    // 数据库接口
    CT_OS,          // 操作系统相关
    CT_LIBARY,     // 基础库
    CT_CHAT,        // 对话接口
    CT_UNKNOWN,     // 未知类型
} ct_content_type;

// -------------------------------
// 内容类型解析函数
// 根据 HTTP Header 的 Content-Type 返回对应枚举
// -------------------------------
ct_content_type parse_content_type(const char *content_type);

// -------------------------------
// HTTP Server 事件处理函数声明
// -------------------------------
void ev_handler(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_static(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_database(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_os(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_chat(struct mg_connection *c, int ev, void *ev_data);
void ev_handler_libary(struct mg_connection *c, int ev, void *ev_data);

// -------------------------------
// WebSocket 事件处理
// -------------------------------
void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data);

// 判断是否为 WebSocket Upgrade 请求
int is_ws_upgrade(struct mg_http_message *hm);

// 手动触发 WebSocket 握手
void ws_accept_manual(struct mg_connection *c, struct mg_http_message *hm);
