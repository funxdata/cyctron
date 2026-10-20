cmake_minimum_required(VERSION 3.28)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =====================================================
# Windows 平台准备（必须在任何子目录之前）
# 合并说明：保留 developer 分支的 Windows 7 目标与 UTF-8 选项，
# 同时保留 libary 分支“NNG 自带测试关闭”的设置，避免 NNG 的
# 测试目标污染本项目的 ctest。
# =====================================================
if(WIN32)
    # 目标 Windows 7 及以上，保证产物可在旧系统运行
    set(CMAKE_SYSTEM_VERSION "6.1" CACHE STRING "" FORCE)
    set(CMAKE_SYSTEM_NAME "Windows" CACHE STRING "" FORCE)

    add_compile_definitions(
        _WIN32_WINNT=0x0601
        WINVER=0x0601
        _CRT_SECURE_NO_WARNINGS
    )

    # 源码与执行字符集统一为 UTF-8（MinGW/MSVC 语法不同）
    if(MSVC)
        add_compile_options("/utf-8")
    else()
        add_compile_options("-finput-charset=UTF-8" "-fexec-charset=UTF-8")
    endif()

    # NNG 子构建通过环境变量读取目标系统版本
    set(ENV{CMAKE_SYSTEM_VERSION} "6.1")
endif()

# =====================================================
# NNG
# =====================================================
# 关闭 NNG 自带测试，避免污染 ctest
set(NNG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(crates/nng)

# =====================================================
# Core sources
# 合并说明：采用 libary 分支的精简列表——os/chat/database handler 已移除，
# 所有插件调用统一走 handler_libary.c。
# =====================================================
set(CORE_SOURCES
    src/main.c
    src/realtime.c
    src/handler.c
    src/event_pusher.c
    src/handler_static.c
    src/handler_utils.c
    src/handler_libary.c
    src/handler_ws.c
    src/handler_sse.c
    src/calldyn.c
    src/core/uri.c
    crates/mongoose/mongoose.c
    crates/log/src/log.c
)

set(LIBEV_SOURCES
    crates/libev/ev.c
)

# =====================================================
# Platform libs
# =====================================================
find_package(Threads REQUIRED)

set(PLATFORM_LIBS
    Threads::Threads
)

if(WIN32)
    # user32 供 GetConsoleWindow/ShowWindow 等 Win32 API 使用
    list(APPEND PLATFORM_LIBS
        ws2_32
        user32
    )

    # 合并说明：保留 libary 分支的 _CRT_SECURE_NO_WARNINGS 与 libev 配置，
    # 删除 developer 分支的 NO_LIBFFI —— libary 的 calldyn.c 无条件依赖 libffi。
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS

        EV_STANDALONE=1
        EV_USE_WIN32=1
        EV_USE_SELECT=1
    )

    # Windows 使用仓库内置的静态 libffi（MinGW）。
    # 合并说明：developer 分支把该逻辑放在 link 阶段，这里提前到平台段，
    # 让主程序与 tests 共用同一组 FFI_INCLUDE_DIRS / FFI_LIBRARIES。
    set(LIBFFI_DIR ${CMAKE_SOURCE_DIR}/crates/libffi-packages)
    if(EXISTS ${LIBFFI_DIR}/libffi.a)
        set(FFI_INCLUDE_DIRS ${LIBFFI_DIR})
        set(FFI_LIBRARIES ${LIBFFI_DIR}/libffi.a)
        message(STATUS "Using static libffi.a from ${LIBFFI_DIR}")
    else()
        message(FATAL_ERROR "libffi.a not found in ${LIBFFI_DIR}")
    endif()
else()
    # Linux/macOS 使用系统 libffi
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)

    set(FFI_INCLUDE_DIRS ${FFI_INCLUDE_DIRS})
    set(FFI_LIBRARIES ${FFI_LIBRARIES})

    list(APPEND PLATFORM_LIBS ${FFI_LIBRARIES} dl)
endif()

# =====================================================
# Main executable
# =====================================================
add_executable(cyctron
    ${CORE_SOURCES}
    ${LIBEV_SOURCES}
)

# =====================================================
# Windows 子系统与优化（合并自 developer）
# 合并说明：Debug 保留控制台便于排查；Release 使用 GUI 子系统，
# 避免双击运行时弹出黑窗。libary 分支原配置没有这一段，予以保留。
# =====================================================
if(WIN32)
    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_options(cyctron PRIVATE "/SUBSYSTEM:CONSOLE")
        else()
            target_link_options(cyctron PRIVATE
                "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
        endif()
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_options(cyctron PRIVATE "-mconsole")
            target_compile_options(cyctron PRIVATE -g -O0)
        else()
            target_link_options(cyctron PRIVATE "-mwindows")
            target_compile_options(cyctron PRIVATE -O2)
        endif()
    endif()
endif()

# =====================================================
# MinGW 完全静态链接
# 合并说明：使用 target_link_options，避免污染全局链接参数；
# 同时解决 developer 分支重复追加 -static 的问题。
# =====================================================
if(WIN32 AND NOT MSVC)
    target_link_options(cyctron PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

# =====================================================
# Include directories
# =====================================================
target_include_directories(cyctron PRIVATE
    include
    crates/mongoose
    crates/libev
    crates/log/src
    ${FFI_INCLUDE_DIRS}
)

# =====================================================
# Link libraries
# 合并说明：libary 分支通过 ${FFI_LIBRARIES} 显式链接 libffi，
# NNG 由 CMake 目标 nng 自动处理路径。
# =====================================================
target_link_libraries(cyctron PRIVATE
    ${PLATFORM_LIBS}
    ${FFI_LIBRARIES}
    nng
)

# 主程序生成到项目主目录（./cyctron 或 ./cyctron.exe），便于直接运行
set_target_properties(cyctron PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# =====================================================
# Static runtime (MSVC)
# =====================================================
if(MSVC)
    set_property(
        TARGET cyctron
        PROPERTY
        MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# =====================================================
# Install
# =====================================================
install(
    TARGETS cyctron
    RUNTIME DESTINATION .
)

# =====================================================
# Tests
# =====================================================
enable_testing()
add_subdirectory(tests)

# =====================================================
# Build summary
# =====================================================
message(STATUS "")
message(STATUS "=================================")
message(STATUS "cyctron build configuration")
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform   : ${CMAKE_SYSTEM_NAME}")
if(WIN32 AND NOT MSVC)
    message(STATUS "Compiler   : MinGW/GCC")
elseif(MSVC)
    message(STATUS "Compiler   : MSVC")
endif()
message(STATUS "=================================")
message(STATUS "")
