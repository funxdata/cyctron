#include <string.h>
#include "server.h"

// 拒绝含 ".." 的 URI，防止 ../ 路径遍历
int uri_has_traversal(const char *uri) {
    return uri && strstr(uri, "..") != NULL;
}
