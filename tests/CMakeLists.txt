# 测试说明：
#   - test_libary / test_libary_stress：依赖 .dytes/libary/libary_demo，
#     插件缺失时只构建、不注册 CTest
#   - test_sse：依赖 .dytes/libary/sse_demo，插件缺失时只构建、不注册 CTest
#   - test_nng_stress：NNG IPC PUB/SUB 吞吐压力测试，结果写入
#     docs/results/stress.jsonl（可设置 STRESS_REPORT_DIR 覆盖）
#   - 其余为纯函数/单元测试，全平台构建并注册 CTest

if(WIN32)
    set(LIBARY_PLUGIN "${CMAKE_SOURCE_DIR}/.dytes/libary/libary_demo.dll")
else()
    set(LIBARY_PLUGIN "${CMAKE_SOURCE_DIR}/.dytes/libary/libary_demo.so")
endif()

# ---------- libary：两个平台都构建 ----------
add_executable(test_libary test_libary.c
    ../src/calldyn.c ../src/plugin_path.c ../crates/log/src/log.c)
target_include_directories(test_libary PRIVATE
    ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_libary PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

add_executable(test_libary_stress test_libary_stress.c
    ../src/calldyn.c ../src/plugin_path.c ../crates/log/src/log.c)
target_include_directories(test_libary_stress PRIVATE
    ../include ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_libary_stress PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

if(EXISTS "${LIBARY_PLUGIN}")
    add_test(NAME libary COMMAND test_libary WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME libary_stress COMMAND test_libary_stress
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(libary_stress PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "STRESS_REPORT_DIR=${CMAKE_SOURCE_DIR}/docs/results")
endif()

# ---------- SSE：插件存在时注册 ----------
if(WIN32)
    set(SSE_PLUGIN "${CMAKE_SOURCE_DIR}/.dytes/libary/sse_demo.dll")
else()
    set(SSE_PLUGIN "${CMAKE_SOURCE_DIR}/.dytes/libary/sse_demo.so")
endif()

add_executable(test_sse test_sse.c
    ../src/handler_sse.c ../src/calldyn.c ../src/plugin_path.c
    ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
target_include_directories(test_sse PRIVATE
    ../include ../crates/mongoose ../crates/log/src ${FFI_INCLUDE_DIRS})
target_link_libraries(test_sse PRIVATE
    ${FFI_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})

if(EXISTS "${SSE_PLUGIN}")
    add_test(NAME sse COMMAND test_sse WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(sse PROPERTIES TIMEOUT 60)
endif()

# ---------- NNG IPC 压力测试：两个平台都构建 ----------
add_executable(test_nng_stress test_nng_stress.c)
target_include_directories(test_nng_stress PRIVATE
    ${CMAKE_SOURCE_DIR}/crates/nng/include)
target_link_libraries(test_nng_stress PRIVATE nng ${PLATFORM_LIBS})
add_test(NAME nng_stress COMMAND test_nng_stress
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(nng_stress PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "STRESS_REPORT_DIR=${CMAKE_SOURCE_DIR}/docs/results")

if(NOT WIN32)
    add_executable(test_static test_static.c
        ../src/handler_static.c ../src/handler_utils.c ../src/core/uri.c
        ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
    target_include_directories(test_static PRIVATE
        ../include ../crates/mongoose ../crates/log/src)
    target_link_libraries(test_static PRIVATE ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})
    add_test(NAME static COMMAND test_static)
endif()

# ---------- WebSocket call helpers: pure functions, both platforms ----------
foreach(call_test ws_call ws_call_parse ws_call_errors ws_call_response)
    add_executable(test_${call_test} test_${call_test}.c
        ../src/ws_call.c ../src/core/json_validate.c ../crates/mongoose/mongoose.c)
    target_include_directories(test_${call_test} PRIVATE
        ../include ../crates/mongoose)
    target_link_libraries(test_${call_test} PRIVATE ${PLATFORM_LIBS})
    add_test(NAME ${call_test} COMMAND test_${call_test})
endforeach()

# ---------- event formatting: pure functions ----------
add_executable(test_event_push_dynamic test_event_push_dynamic.c ../src/event_format.c)
target_include_directories(test_event_push_dynamic PRIVATE ../include)
target_link_libraries(test_event_push_dynamic PRIVATE ${PLATFORM_LIBS})
add_test(NAME event_push_dynamic COMMAND test_event_push_dynamic)

# ---------- JSON validation: pure functions ----------
add_executable(test_json_validate test_json_validate.c ../src/core/json_validate.c)
target_include_directories(test_json_validate PRIVATE ../include)
target_link_libraries(test_json_validate PRIVATE ${PLATFORM_LIBS})
add_test(NAME json_validate COMMAND test_json_validate)

# ---------- URI utilities: pure functions ----------
add_executable(test_uri_utils test_uri_utils.c
    ../src/core/uri.c ../src/handler_utils.c)
target_include_directories(test_uri_utils PRIVATE ../include)
target_link_libraries(test_uri_utils PRIVATE ${PLATFORM_LIBS})
add_test(NAME uri_utils COMMAND test_uri_utils)

# ---------- WebSocket message size ----------
add_executable(test_ws_message_limit test_ws_message_limit.c ../src/ws_message.c)
target_include_directories(test_ws_message_limit PRIVATE ../include)
target_link_libraries(test_ws_message_limit PRIVATE ${PLATFORM_LIBS})
add_test(NAME ws_message_limit COMMAND test_ws_message_limit)

# ---------- static SPA fallback ----------
add_executable(test_static_spa test_static_spa.c
    ../src/handler_static.c ../src/handler_utils.c ../src/core/uri.c
    ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
target_include_directories(test_static_spa PRIVATE
    ../include ../crates/mongoose ../crates/log/src)
target_link_libraries(test_static_spa PRIVATE ${CMAKE_DL_LIBS} ${PLATFORM_LIBS})
add_test(NAME static_spa COMMAND test_static_spa)

# ---------- legacy command JSON parsing ----------
add_executable(test_ws_parse_legacy test_ws_parse_legacy.c
    ../src/ws_command.c ../crates/mongoose/mongoose.c)
target_include_directories(test_ws_parse_legacy PRIVATE
    ../include ../crates/mongoose)
target_link_libraries(test_ws_parse_legacy PRIVATE ${PLATFORM_LIBS})
add_test(NAME ws_parse_legacy COMMAND test_ws_parse_legacy)

# ---------- realtime task ownership ----------
add_executable(test_realtime_ownership test_realtime_ownership.c
    ../src/realtime.c ../src/plugin_path.c ../crates/log/src/log.c)
target_include_directories(test_realtime_ownership PRIVATE
    ../include ../crates/log/src ${CMAKE_SOURCE_DIR}/crates/nng/include)
target_link_libraries(test_realtime_ownership PRIVATE nng ${PLATFORM_LIBS})
add_test(NAME realtime_ownership COMMAND test_realtime_ownership)

# ---------- plugin path resolution: only libary ----------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugin_path_fixture)
add_executable(test_plugin_path test_plugin_path.c ../src/plugin_path.c)
target_include_directories(test_plugin_path PRIVATE ../include)
target_link_libraries(test_plugin_path PRIVATE ${PLATFORM_LIBS})
add_test(NAME plugin_path COMMAND test_plugin_path
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugin_path_fixture)

# ---------- WebSocket worker ----------
add_executable(test_ws_worker test_ws_worker.c
    ../src/ws_worker.c ../src/ws_call.c ../src/core/json_validate.c ../src/plugin_path.c
    ../crates/mongoose/mongoose.c ../crates/log/src/log.c)
target_include_directories(test_ws_worker PRIVATE
    ../include ../crates/mongoose ../crates/log/src)
target_link_libraries(test_ws_worker PRIVATE ${PLATFORM_LIBS})
add_test(NAME ws_worker COMMAND test_ws_worker)
set_tests_properties(ws_worker PROPERTIES TIMEOUT 30)
