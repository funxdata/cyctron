#include <string.h>
#include "server.h"


ct_content_type parse_content_type(const char *content_type) {
    if (!content_type || *content_type == '\0')
        return CT_UNKNOWN;
    if (strstr(content_type, "text/html")) return CT_STATIC;
    if (strstr(content_type, "application/json")) return CT_STATIC;
    if (strstr(content_type, "text/css")) return CT_STATIC;
    if (strstr(content_type, "image/")) return CT_STATIC;
    if (strstr(content_type, "application/javascript")) return CT_STATIC;
    if (strstr(content_type, "application/x-www-form-urlencoded")) return CT_STATIC;
    if (strstr(content_type, "multipart/form-data")) return CT_STATIC;
    if (strstr(content_type, "text/plain")) return CT_STATIC;
    if (strstr(content_type, "application/xml")) return CT_STATIC;
    if (strstr(content_type, "application/pdf")) return CT_STATIC;
    if (strstr(content_type, "application/database")) return CT_DATABASE;
    if (strstr(content_type, "application/os")) return CT_OS;
    if (strstr(content_type, "application/chat")) return CT_CHAT;
    if (strstr(content_type, "application/libary")) return CT_LIBARY;
    return CT_UNKNOWN;
}