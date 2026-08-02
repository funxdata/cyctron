/**
 * test_dyn_socket.c
 * 
 * 测试 calldyn.c 调用 socket_demo 动态库
 * 测试所有 FFI 导出函数
 * 
 * 编译命令:
 *   gcc -o ./build/test/test_dyn_socket ./tests/test_dyn_socket.c ./src/calldyn.c -lffi -ldl
 * 
 * 用法: ./build/test/test_dyn_socket [plugin_path]
 *   默认: ./build/test/test_dyn_socket ./core/libsocket_demo.so
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

// 外部函数声明
int call_local_dyn_libffi(const char *ffi_path, const char *func_name, 
                          const char *json_in, char **json_out);

// ============ 测试配置 ============
#define PLUGIN_PATH "./core/libsoket_demo.so"

// FFI 函数名
#define FUNC_INIT   "process_init"
#define FUNC_CLOSE  "process_close"
#define FUNC_UPDATE "process_update"
#define FUNC_MERGE  "process_merge"
#define FUNC_MSG    "process_msg"
#define FUNC_PAUSE  "process_pause"
#define FUNC_RESUME "process_resume"
#define FUNC_STATUS "process_status"

static int test_passed = 0;
static int test_failed = 0;

// ============ 测试辅助函数 ============

void test_assert(int condition, const char *test_name) {
    if (condition) {
        test_passed++;
        printf("[✓ PASS] %s\n", test_name);
    } else {
        test_failed++;
        printf("[✗ FAIL] %s\n", test_name);
    }
}

void safe_free(char **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

void print_result(const char *func_name, int ret, const char *output) {
    printf("  %s: ret=%d", func_name, ret);
    if (output && strlen(output) > 0) {
        if (strlen(output) > 200) {
            printf(", output=%.200s...\n", output);
        } else {
            printf(", output=%s\n", output);
        }
    } else if (output) {
        printf(", output=(empty)\n");
    } else {
        printf(", output=NULL\n");
    }
}

int extract_code(const char *json_str) {
    if (!json_str) return -1;
    char *code_str = strstr(json_str, "\"code\":");
    if (code_str) {
        int code;
        if (sscanf(code_str, "\"code\":%d", &code) == 1) {
            return code;
        }
    }
    return -1;
}

int extract_status(const char *json_str, char *status, size_t size) {
    if (!json_str || !status) return -1;
    char *status_str = strstr(json_str, "\"status\":");
    if (status_str) {
        if (sscanf(status_str, "\"status\":\"%[^\"]\"", status) == 1) {
            return 0;
        }
    }
    return -1;
}

// ============ 测试用例 ============

/**
 * 测试1: 初始化 (process_init)
 */
void test_init(const char *plugin_path) {
    printf("\n========== Test 1: process_init (Initialize) ==========\n");
    
    char *output = NULL;
    
    // 不带配置初始化
    const char *json1 = "{}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_INIT, json1, &output);
    print_result(FUNC_INIT, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Init without config");
    safe_free(&output);
    
    // 带配置初始化
    const char *json2 = "{\"config\":{\"app_name\":\"socket_demo\",\"version\":\"1.0.0\",\"mode\":\"production\"}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_INIT, json2, &output);
    print_result(FUNC_INIT, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Init with config");
    safe_free(&output);
}

/**
 * 测试2: 更新数据 (process_update)
 */
void test_update(const char *plugin_path) {
    printf("\n========== Test 2: process_update (Update Data) ==========\n");
    
    char *output = NULL;
    
    // 更新数据
    const char *json1 = "{\"data\":{\"user\":\"Alice\",\"age\":25,\"city\":\"Beijing\"}}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_UPDATE, json1, &output);
    print_result(FUNC_UPDATE, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Update data");
    safe_free(&output);
    
    // 再次更新（覆盖）
    const char *json2 = "{\"data\":{\"user\":\"Bob\",\"age\":30,\"country\":\"China\"}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_UPDATE, json2, &output);
    print_result(FUNC_UPDATE, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Update data again (overwrite)");
    safe_free(&output);
}

/**
 * 测试3: 合并数据 (process_merge)
 */
void test_merge(const char *plugin_path) {
    printf("\n========== Test 3: process_merge (Merge Data) ==========\n");
    
    char *output = NULL;
    
    // 先重置数据
    const char *reset = "{\"data\":{\"base\":\"initial\"}}";
    call_local_dyn_libffi(plugin_path, FUNC_UPDATE, reset, &output);
    safe_free(&output);
    
    // 合并数据
    const char *json1 = "{\"data\":{\"field1\":\"value1\",\"field2\":\"value2\"}}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_MERGE, json1, &output);
    print_result(FUNC_MERGE, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Merge first data");
    safe_free(&output);
    
    // 再次合并
    const char *json2 = "{\"data\":{\"field3\":\"value3\",\"field4\":\"value4\"}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_MERGE, json2, &output);
    print_result(FUNC_MERGE, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Merge second data");
    safe_free(&output);
}

/**
 * 测试4: 处理消息 (process_msg)
 */
void test_msg(const char *plugin_path) {
    printf("\n========== Test 4: process_msg (Process Message) ==========\n");
    
    char *output = NULL;
    
    // 简单消息
    const char *json1 = "{\"type\":\"ping\",\"data\":{\"timestamp\":1234567890}}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_MSG, json1, &output);
    print_result(FUNC_MSG, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Process ping message");
    safe_free(&output);
    
    // 复杂消息
    const char *json2 = "{\"type\":\"command\",\"action\":\"process\",\"params\":{\"id\":100,\"name\":\"test\"}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_MSG, json2, &output);
    print_result(FUNC_MSG, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Process command message");
    safe_free(&output);
}

/**
 * 测试5: 暂停 (process_pause)
 */
void test_pause(const char *plugin_path) {
    printf("\n========== Test 5: process_pause (Pause) ==========\n");
    
    char *output = NULL;
    
    int ret = call_local_dyn_libffi(plugin_path, FUNC_PAUSE, "{}", &output);
    print_result(FUNC_PAUSE, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Pause runtime");
    safe_free(&output);
    
    // 验证状态
    ret = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", &output);
    print_result(FUNC_STATUS, ret, output);
    char status[32];
    if (extract_status(output, status, sizeof(status)) == 0) {
        printf("  Status after pause: %s\n", status);
        test_assert(strcmp(status, "paused") == 0, "Status should be paused");
    }
    safe_free(&output);
}

/**
 * 测试6: 恢复 (process_resume)
 */
void test_resume(const char *plugin_path) {
    printf("\n========== Test 6: process_resume (Resume) ==========\n");
    
    char *output = NULL;
    
    // 先暂停
    call_local_dyn_libffi(plugin_path, FUNC_PAUSE, "{}", &output);
    safe_free(&output);
    
    // 恢复
    int ret = call_local_dyn_libffi(plugin_path, FUNC_RESUME, "{}", &output);
    print_result(FUNC_RESUME, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Resume runtime");
    safe_free(&output);
    
    // 验证状态
    ret = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", &output);
    char status[32];
    if (extract_status(output, status, sizeof(status)) == 0) {
        printf("  Status after resume: %s\n", status);
        test_assert(strcmp(status, "running") == 0, "Status should be running");
    }
    safe_free(&output);
}

/**
 * 测试7: 状态查询 (process_status)
 */
void test_status(const char *plugin_path) {
    printf("\n========== Test 7: process_status (Query Status) ==========\n");
    
    char *output = NULL;
    
    int ret = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", &output);
    print_result(FUNC_STATUS, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Query status");
    safe_free(&output);
}

/**
 * 测试8: 关闭 (process_close)
 */
void test_close(const char *plugin_path) {
    printf("\n========== Test 8: process_close (Close) ==========\n");
    
    char *output = NULL;
    
    int ret = call_local_dyn_libffi(plugin_path, FUNC_CLOSE, "{}", &output);
    print_result(FUNC_CLOSE, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "Close runtime");
    safe_free(&output);
}

/**
 * 测试9: 完整工作流
 */
void test_workflow(const char *plugin_path) {
    printf("\n========== Test 9: Complete Workflow ==========\n");
    
    char *output = NULL;
    int ret;
    
    // 1. 初始化
    printf("  [1] Init:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_INIT, "{\"config\":{\"app\":\"test\"}}", &output);
    print_result(FUNC_INIT, ret, output);
    test_assert(ret >= 0, "Init");
    safe_free(&output);
    
    // 2. 更新数据
    printf("  [2] Update:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_UPDATE, "{\"data\":{\"step\":1,\"action\":\"start\"}}", &output);
    print_result(FUNC_UPDATE, ret, output);
    test_assert(ret >= 0, "Update");
    safe_free(&output);
    
    // 3. 合并数据
    printf("  [3] Merge:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_MERGE, "{\"data\":{\"extra\":\"info\"}}", &output);
    print_result(FUNC_MERGE, ret, output);
    test_assert(ret >= 0, "Merge");
    safe_free(&output);
    
    // 4. 处理消息
    printf("  [4] Process Message:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_MSG, "{\"type\":\"process\",\"data\":{\"id\":123}}", &output);
    print_result(FUNC_MSG, ret, output);
    test_assert(ret >= 0, "Process message");
    safe_free(&output);
    
    // 5. 查询状态
    printf("  [5] Query Status:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", &output);
    print_result(FUNC_STATUS, ret, output);
    test_assert(ret >= 0, "Query status");
    safe_free(&output);
    
    // 6. 暂停
    printf("  [6] Pause:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_PAUSE, "{}", &output);
    print_result(FUNC_PAUSE, ret, output);
    test_assert(ret >= 0, "Pause");
    safe_free(&output);
    
    // 7. 恢复
    printf("  [7] Resume:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_RESUME, "{}", &output);
    print_result(FUNC_RESUME, ret, output);
    test_assert(ret >= 0, "Resume");
    safe_free(&output);
    
    // 8. 关闭
    printf("  [8] Close:\n");
    ret = call_local_dyn_libffi(plugin_path, FUNC_CLOSE, "{}", &output);
    print_result(FUNC_CLOSE, ret, output);
    test_assert(ret >= 0, "Close");
    safe_free(&output);
}

/**
 * 测试10: 错误处理
 */
void test_errors(const char *plugin_path) {
    printf("\n========== Test 10: Error Handling ==========\n");
    
    char *output = NULL;
    
    // NULL path
    int ret1 = call_local_dyn_libffi(NULL, FUNC_STATUS, "{}", &output);
    printf("  NULL path -> %d (expected -100)\n", ret1);
    test_assert(ret1 == -100, "NULL path");
    safe_free(&output);
    
    // NULL function
    int ret2 = call_local_dyn_libffi(plugin_path, NULL, "{}", &output);
    printf("  NULL func -> %d (expected -100)\n", ret2);
    test_assert(ret2 == -100, "NULL function");
    safe_free(&output);
    
    // NULL output
    int ret3 = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", NULL);
    printf("  NULL output -> %d (expected -100)\n", ret3);
    test_assert(ret3 == -100, "NULL output");
    safe_free(&output);
    
    // 不存在的函数
    int ret4 = call_local_dyn_libffi(plugin_path, "invalid_func", "{}", &output);
    printf("  Invalid function -> %d (expected -2)\n", ret4);
    test_assert(ret4 == -2, "Invalid function");
    safe_free(&output);
    
    // 无效 JSON
    const char *invalid = "{invalid json}";
    int ret5 = call_local_dyn_libffi(plugin_path, FUNC_STATUS, invalid, &output);
    print_result(FUNC_STATUS, ret5, output);
    if (ret5 == 0 && output) {
        test_assert(1, "Invalid JSON handled");
    } else {
        test_assert(0, "Invalid JSON not handled");
    }
    safe_free(&output);
}

/**
 * 测试11: 性能测试
 */
void test_performance(const char *plugin_path) {
    printf("\n========== Test 11: Performance Test ==========\n");
    
    char *output = NULL;
    int iterations = 500;
    int success = 0;
    
    printf("  Running %d status queries...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        int ret = call_local_dyn_libffi(plugin_path, FUNC_STATUS, "{}", &output);
        if (ret >= 0) success++;
        safe_free(&output);
        
        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d\n", i + 1, iterations);
        }
    }
    
    printf("  Success: %d/%d (%.1f%%)\n", success, iterations,
           (float)success / iterations * 100);
    
    test_assert(success >= iterations * 0.95, "Performance test");
}

// ============ 主函数 ============

int main(int argc, char **argv) {
    const char *plugin_path = (argc > 1) ? argv[1] : PLUGIN_PATH;
    
    printf("========================================\n");
    printf("   test_dyn_socket - Socket Demo FFI Test\n");
    printf("========================================\n");
    printf("Plugin: %s\n", plugin_path);
    printf("Functions:\n");
    printf("  - process_init    (initialize runtime)\n");
    printf("  - process_close   (close runtime)\n");
    printf("  - process_update  (update data - overwrite)\n");
    printf("  - process_merge   (merge data - append)\n");
    printf("  - process_msg     (process message)\n");
    printf("  - process_pause   (pause runtime)\n");
    printf("  - process_resume  (resume runtime)\n");
    printf("  - process_status  (query status)\n");
    printf("========================================\n");
    
    // 检查文件是否存在
    FILE *fp = fopen(plugin_path, "r");
    if (fp) {
        printf("✓ Plugin found\n\n");
        fclose(fp);
    } else {
        printf("✗ Plugin not found: %s\n", plugin_path);
        printf("\n");
        printf("  To build the socket_demo library:\n");
        printf("    cd examples/soket_demo\n");
        printf("    cargo build --release\n");
        printf("\n");
        printf("  Expected location: ./examples/soket_demo/target/release/libsocket_demo.so\n");
        return 1;
    }
    
    // 运行所有测试
    test_init(plugin_path);
    test_update(plugin_path);
    test_merge(plugin_path);
    test_msg(plugin_path);
    test_pause(plugin_path);
    test_resume(plugin_path);
    test_status(plugin_path);
    test_close(plugin_path);
    test_workflow(plugin_path);
    test_errors(plugin_path);
    test_performance(plugin_path);
    
    // 输出结果
    printf("\n========================================\n");
    printf("   Test Results\n");
    printf("========================================\n");
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("Total:  %d\n", test_passed + test_failed);
    printf("========================================\n");
    
    return (test_failed == 0) ? 0 : 1;
}
