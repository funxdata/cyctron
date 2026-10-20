# 测试说明：
#   - test_libary / test_libary_stress: libary_demo 插件单元与压力测试，
#     插件产物位于 ./libary/（构建 examples/libary_demo 后拷贝），
#     插件缺失时只构建、不注册 CTest
#   - test_calldyn / test_dyn_socket: FFI 插件联调测试，运行前需构建 Rust 插件
#     并拷贝到 ./core/（见 examples/），插件缺失时自动跳过 CTest 注册
#   - test_nng_ipc: NNG IPC 演示（无限循环），仅构建，不注册 CTest
#   - test_basic / test_data / test_multiple / test_pause_resume /
#     test_performance / test_realtime: 基于旧版 realtime API，待迁移后接入

if(WIN32)
    set(LIBARY_PLUGIN "${CMAKE_SOURCE_DIR}/libary/libary_demo.dll")
else()
    set(LIBARY_PLUGIN "${CMAKE_SOURCE_DIR}/libary/libary_demo.so")
endif()

# ---------- libary：两个平台都构建 ----------
add_executable(test_libary test_libary.c
    ../src/calldyn.c ../crates/log/src/log.c)
target_include_directories(test_libary PRIVATE
    ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_libary PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

add_executable(test_libary_stress test_libary_stress.c
    ../src/calldyn.c ../crates/log/src/log.c)
target_include_directories(test_libary_stress PRIVATE
    ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_libary_stress PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

if(EXISTS "${LIBARY_PLUGIN}")
    add_test(NAME libary COMMAND test_libary WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME libary_stress COMMAND test_libary_stress
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(libary_stress PROPERTIES TIMEOUT 120)
endif()

# ---------- SSE：插件存在时注册 ----------
if(WIN32)
    set(SSE_PLUGIN "${CMAKE_SOURCE_DIR}/libary/sse_demo.dll")
else()
    set(SSE_PLUGIN "${CMAKE_SOURCE_DIR}/libary/sse_demo.so")
endif()

add_executable(test_sse test_sse.c
    ../src/handler_sse.c ../src/calldyn.c
    ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
target_include_directories(test_sse PRIVATE
    ../include ../crates/mongoose ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_sse PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

if(EXISTS "${SSE_PLUGIN}")
    add_test(NAME sse COMMAND test_sse WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(sse PROPERTIES TIMEOUT 60)
endif()

if(NOT WIN32)
    add_executable(test_calldyn test_calldyn.c ../src/calldyn.c ../crates/log/src/log.c)
    target_include_directories(test_calldyn PRIVATE
        ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
    target_link_libraries(test_calldyn PRIVATE
        ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

    add_executable(test_dyn_socket test_dyn_socket.c ../src/calldyn.c ../crates/log/src/log.c)
    target_include_directories(test_dyn_socket PRIVATE
        ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
    target_link_libraries(test_dyn_socket PRIVATE
        ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

    add_executable(test_nng_ipc test_nng_ipc.c)
    target_link_libraries(test_nng_ipc PRIVATE nng ${PLATFORM_LIBS})

    add_executable(test_static test_static.c
        ../src/handler_static.c ../src/handler_utils.c ../src/core/uri.c
        ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
    target_include_directories(test_static PRIVATE
        ../include ../crates/mongoose ../crates/log/src)
    target_link_libraries(test_static PRIVATE ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})
    add_test(NAME static COMMAND test_static)

    # test_dyn_socket 与 libsoket_demo 插件 API 不匹配（插件未导出
    # process_pause/resume/status/close，且 process_init 不退出），暂不注册，
    # 待插件补齐符号和退出逻辑后再 add_test
    if(EXISTS "${CMAKE_SOURCE_DIR}/core/librust_demo.so")
        add_test(NAME calldyn COMMAND test_calldyn WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(calldyn PROPERTIES TIMEOUT 60)
    endif()
endif()
