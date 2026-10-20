#include <string.h>
#include "server.h"
#include <stdio.h>

#ifdef _WIN32
    #define FFI_EXT ".dll"
    #define SEP "\\"
#elif __APPLE__
    #define FFI_EXT ".dylib"
    #define SEP "/"
#else
    #define FFI_EXT ".so"
    #define SEP "/"
#endif

//  build_ffi_path
void build_ffi_path(const char *uri, const char *dir, char *out, size_t out_size)
{
    const char *clean_uri = (uri[0] == '/') ? uri + 1 : uri;
    snprintf(out, out_size,
             "%s%s%s",
             dir,
             clean_uri,
             FFI_EXT);
}

// 拒绝含 ".." 的 URI，防止 ../ 路径遍历
int uri_has_traversal(const char *uri) {
    return uri && strstr(uri, "..") != NULL;
}
