cmake_minimum_required(VERSION 3.30)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(crates/nng)

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
        NO_LIBFFI

        EV_STANDALONE=1
        EV_USE_WIN32=1
        EV_USE_SELECT=1
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)

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

# 链接 NNG（CMake 会自动处理路径）
target_link_libraries(cyctron PRIVATE
    ${PLATFORM_LIBS}
    nng
)

