cmake_minimum_required(VERSION 3.30)
project(cyctron C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =====================================================
# 1. Core sources
# =====================================================

set(CORE_SOURCES
    src/main.c
    src/realtime.c
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

# =====================================================
# 2. libev (IMPORTANT: ONLY ev.c)
# =====================================================

set(LIBEV_SOURCES
    crates/libev/ev.c
)

# =====================================================
# 3. Platform libs
# =====================================================

find_package(Threads REQUIRED)
set(PLATFORM_LIBS Threads::Threads)

if(WIN32)
    list(APPEND PLATFORM_LIBS ws2_32)

    add_compile_definitions(
        _WIN32_WINNT=0x0600
        WIN32_LEAN_AND_MEAN
        NO_LIBFFI

        # libev backend switch
        EV_STANDALONE=1
        EV_USE_WIN32=1
        EV_USE_SELECT=1
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)

    list(APPEND PLATFORM_LIBS ${FFI_LIBRARIES} dl ev)
endif()

# =====================================================
# 4. Main executable
# =====================================================

add_executable(cyctron
    ${CORE_SOURCES}
    ${LIBEV_SOURCES}
)

target_include_directories(cyctron PRIVATE
    include
    crates/mongoose
    crates/libev
    crates/log/src
)

target_link_libraries(cyctron PRIVATE ${PLATFORM_LIBS})

# =====================================================
# 5. Tests
# =====================================================

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Criterion (optional on Windows)
    if(WIN32)
        message(STATUS "Windows: Criterion disabled")
        set(CRITERION_LIBS "")
        set(CRITERION_INCLUDES "")
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(CRITERION REQUIRED criterion)

        set(CRITERION_LIBS ${CRITERION_LIBRARIES})
        set(CRITERION_INCLUDES ${CRITERION_INCLUDE_DIRS})
    endif()

    set(TEST_COMMON
        src/realtime.c
        ${LIBEV_SOURCES}
    )

    set(TEST_INCLUDE
        include
        crates/mongoose
        crates/libev
        crates/log/src
        ${CRITERION_INCLUDES}
    )

    function(add_rt_test name src)
        add_executable(${name}
            ${src}
            ${TEST_COMMON}
        )

        target_include_directories(${name} PRIVATE ${TEST_INCLUDE})

        target_link_libraries(${name} PRIVATE
            ${PLATFORM_LIBS}
            ${CRITERION_LIBS}
        )

        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_rt_test(test_basic tests/test_basic.c)
    add_rt_test(test_multiple tests/test_multiple.c)
    add_rt_test(test_pause_resume tests/test_pause_resume.c)
    add_rt_test(test_data tests/test_data.c)
    add_rt_test(test_performance tests/test_performance.c)
    add_rt_test(cyctron_tests tests/test_realtime.c)

endif()