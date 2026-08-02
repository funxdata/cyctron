cmake_minimum_required(VERSION 3.30)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =====================================================
# 设置 Windows 版本（必须在任何子目录之前）
# =====================================================
if(WIN32)
    # 强制设置 Windows 版本为 Windows 7 或更高
    set(CMAKE_SYSTEM_VERSION "6.1" CACHE STRING "" FORCE)
    set(CMAKE_SYSTEM_NAME "Windows" CACHE STRING "" FORCE)
    
    # 定义 Windows 版本宏（必须在所有编译之前）
    add_compile_definitions(
        _WIN32_WINNT=0x0601
        WINVER=0x0601
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# =====================================================
# Windows 编译选项
# =====================================================
if(WIN32)
    if(MSVC)
        add_compile_options("/utf-8")
    else()
        add_compile_options("-finput-charset=UTF-8" "-fexec-charset=UTF-8")
    endif()
endif()

# =====================================================
# NNG subdirectory
# =====================================================
# 在添加子目录前，确保 NNG 知道我们在 Windows 上
if(WIN32)
    # 为 NNG 设置环境变量
    set(ENV{CMAKE_SYSTEM_VERSION} "6.1")
endif()

add_subdirectory(crates/nng)

# =====================================================
# Core sources
# =====================================================
set(CORE_SOURCES
    src/main.c
    src/realtime.c
    src/handler.c
    src/event_pusher.c
    src/handler_static.c
    src/handler_utils.c
    src/handler_os.c
    src/handler_database.c
    src/handler_chat.c
    src/handler_libary.c
    src/handler_ws.c
    src/calldyn.c
    src/core/uri.c
    crates/mongoose/mongoose.c
    crates/log/src/log.c
)

# =====================================================
# libev
# =====================================================
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

# =====================================================
# Windows
# =====================================================
if(WIN32)
    list(APPEND PLATFORM_LIBS
        ws2_32
        user32
        nng  # 确保 nng 被链接
    )

    # 额外的编译定义（不覆盖之前的）
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        EV_STANDALONE=1
        EV_USE_WIN32=1
        EV_USE_SELECT=1
        NO_LIBFFI
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG
            "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SUBSYSTEM:CONSOLE"
        )
    else()
        if(MSVC)
            set(CMAKE_EXE_LINKER_FLAGS_RELEASE
                "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            )
        else()
            set(CMAKE_EXE_LINKER_FLAGS_RELEASE
                "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -mwindows"
            )
        endif()
    endif()

else()
    # Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)
    list(APPEND PLATFORM_LIBS
        ${FFI_LIBRARIES}
        dl
    )
endif()

# =====================================================
# Main executable
# =====================================================
add_executable(cyctron
    ${CORE_SOURCES}
    ${LIBEV_SOURCES}
)

# =====================================================
# MinGW 完全静态链接
# =====================================================
if(WIN32 AND NOT MSVC)
    target_link_options(cyctron PRIVATE -static -static-libgcc -static-libstdc++)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# =====================================================
# Include directories
# =====================================================
target_include_directories(cyctron PRIVATE
    include
    crates/mongoose
    crates/libev
    crates/log/src
)

# =====================================================
# libffi (Windows static)
# =====================================================
if(WIN32)
    set(LIBFFI_DIR ${CMAKE_SOURCE_DIR}/crates/libffi-packages)
    if(EXISTS ${LIBFFI_DIR}/libffi.a)
        target_include_directories(cyctron PRIVATE ${LIBFFI_DIR})
        target_link_libraries(cyctron PRIVATE ${LIBFFI_DIR}/libffi.a)
        target_compile_definitions(cyctron PRIVATE HAS_LIBFFI=1)
        message(STATUS "Using static libffi.a from ${LIBFFI_DIR}")
    else()
        message(STATUS "libffi.a not found, using NO_LIBFFI")
        target_compile_definitions(cyctron PRIVATE NO_LIBFFI=1)
    endif()
endif()
# =====================================================
# Link NNG and platform libraries
# =====================================================
target_link_libraries(cyctron PRIVATE
    ${PLATFORM_LIBS}
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

message(STATUS "")
message(STATUS "=================================")
message(STATUS "cyctron build configuration")
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform   : ${CMAKE_SYSTEM_NAME}")
message(STATUS "=================================")
message(STATUS "")