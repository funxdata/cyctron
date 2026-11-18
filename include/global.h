#pragma once

#ifdef _WIN32
    #define PATH_SEP "\\"
    #define WEB_ROOT ".\\web"
    #define LIBARY_DIR ".\\libary"
    #define FFI_EXT ".dll"
#elif __APPLE__
    #define PATH_SEP "/"
    #define WEB_ROOT "./web"
    #define LIBARY_DIR "./libary"
    #define FFI_EXT ".dylib"
#else
    #define PATH_SEP "/"
    #define WEB_ROOT "./web"
    #define LIBARY_DIR "./libary"
    #define FFI_EXT ".so"
#endif

