cmake_minimum_required(VERSION 3.28)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 关闭 NNG 自带测试，避免污染 ctest
set(NNG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(crates/nng)
set(CORE_SOURCES
    src/main.c
    src/realtime.c
    src/handler.c
    src/event_pusher.c
    src/handler_static.c
    src/handler_utils.c
    src/handler_libary.c
    src/handler_ws.c
    src/calldyn.c
    src/core/uri.c
    crates/mongoose/mongoose.c
    crates/log/src/log.c
)

set(LIBEV_SOURCES
    crates/libev/ev.c
)

find_package(Threads REQUIRED)
set(PLATFORM_LIBS Threads::Threads)

if(WIN32)
    list(APPEND PLATFORM_LIBS ws2_32)

    add_compile_definitions(
        _WIN32_WINNT=0x0600
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS

        EV_STANDALONE=1
        EV_USE_WIN32=1
        EV_USE_SELECT=1
    )

    # Windows 使用仓库内置的静态 libffi（MinGW）
    set(LIBFFI_DIR ${CMAKE_SOURCE_DIR}/crates/libffi-packages)
    if(EXISTS ${LIBFFI_DIR}/libffi.a)
        set(FFI_INCLUDE_DIRS ${LIBFFI_DIR})
        set(FFI_LIBRARIES ${LIBFFI_DIR}/libffi.a)
    else()
        message(FATAL_ERROR "libffi.a not found in ${LIBFFI_DIR}")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)
    set(FFI_INCLUDE_DIRS ${FFI_INCLUDE_DIRS})
    set(FFI_LIBRARIES ${FFI_LIBRARIES})

    list(APPEND PLATFORM_LIBS ${FFI_LIBRARIES} dl)
endif()

add_executable(cyctron
    ${CORE_SOURCES}
    ${LIBEV_SOURCES}
)

target_include_directories(cyctron PRIVATE
    include
    crates/mongoose
    crates/libev
    crates/log/src
    ${FFI_INCLUDE_DIRS}
)

# 链接 NNG（CMake 会自动处理路径）与 libffi
target_link_libraries(cyctron PRIVATE
    ${PLATFORM_LIBS}
    ${FFI_LIBRARIES}
    nng
)

# 主程序生成到项目主目录（./cyctron）
set_target_properties(cyctron PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

enable_testing()
add_subdirectory(tests)

