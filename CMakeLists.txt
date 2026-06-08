cmake_minimum_required(VERSION 3.30)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ========================
# Sources
# ========================
set(SOURCES
    src/main.c
    src/handler.c
    src/handler_static.c
    src/handler_utils.c
    src/handler_os.c
    src/handler_database.c
    src/handler_chat.c
    src/handler_libary.c
    src/handler_ws.c
    src/calldyn_libffi.c     # ⭐ 永远编译（关键）
    src/core/uri.c
    crates/mongoose/mongoose.c
    crates/log/src/log.c
)

add_executable(cyctron ${SOURCES})

# ========================
# include dirs
# ========================
target_include_directories(cyctron PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/crates/mongoose
    ${CMAKE_CURRENT_SOURCE_DIR}/crates/log/src
)

# ========================
# Threads
# ========================
find_package(Threads REQUIRED)
target_link_libraries(cyctron PRIVATE Threads::Threads)

# ========================
# OS-specific libs
# ========================
if(UNIX AND NOT APPLE)
    target_link_libraries(cyctron PRIVATE dl)
endif()

if(WIN32)
    target_link_libraries(cyctron PRIVATE ws2_32)
endif()

# ========================
# libffi（跨平台正确姿势）
# ========================
if(WIN32)

    # Windows：不依赖 libffi（你的 calldyn_libffi.c 已经做 fallback）
    message(STATUS "Windows build: using fallback libffi implementation")

    add_compile_definitions(NO_LIBFFI)

else()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)

    target_include_directories(cyctron PRIVATE ${FFI_INCLUDE_DIRS})
    target_link_libraries(cyctron PRIVATE ${FFI_LIBRARIES})

endif()

# ========================
# Tests (Linux/mac only)
# ========================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS AND NOT WIN32)

    enable_testing()

    pkg_check_modules(CRITERION REQUIRED criterion)

    add_executable(cyctron_tests
        tests/test_basic.c
        src/handler_utils.c
    )

    target_include_directories(cyctron_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CRITERION_INCLUDE_DIRS}
    )

    target_link_libraries(cyctron_tests PRIVATE
        ${CRITERION_LIBRARIES}
        Threads::Threads
    )

    add_test(NAME cyctron_tests COMMAND cyctron_tests)
endif()