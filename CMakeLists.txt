cmake_minimum_required(VERSION 3.30)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
    src/calldyn.c
    src/core/uri.c
    crates/mongoose/mongoose.c
    crates/log/src/log.c
)

add_executable(cyctron ${SOURCES})

target_include_directories(cyctron PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/crates/mongoose
    ${CMAKE_CURRENT_SOURCE_DIR}/crates/log/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)

target_link_libraries(cyctron PRIVATE
    Threads::Threads
    dl
)

if (WIN32)
    target_link_libraries(cyctron PRIVATE ws2_32)
endif()


# ========================
# Tests (Criterion)
# ========================
option(BUILD_TESTS "Build unit tests" ON)

if (BUILD_TESTS)
    enable_testing()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CRITERION REQUIRED criterion)

    add_executable(cyctron_tests
        tests/test_basic.c
        # 如果测试需要复用源码，可加：
        src/handler_utils.c
    )

    target_include_directories(cyctron_tests PRIVATE
        ${CRITERION_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(cyctron_tests PRIVATE
        ${CRITERION_LIBRARIES}
        Threads::Threads
    )

    add_test(
        NAME cyctron_tests
        COMMAND cyctron_tests
    )
endif()
