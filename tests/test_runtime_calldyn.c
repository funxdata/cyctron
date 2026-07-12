/**
 * socket_dyncall.c
 * 
 * 测试 runtime_dyncall 功能
 * 测试进程管理、方法调用、错误处理等
 * 
 * 编译命令:
 *   gcc -o ./build/test/runtime_dyncall ./tests/test_runtime_calldyn.c ./src/runtime_dyncall.c -lffi -ldl
 * 
 * 用法: ./build/test/socket_dyncall [socket_demo_path]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

// ============ 外部类型和函数声明 ============

// 类型声明
typedef enum {
    RUNTIME_STATE_STOPPED = 0,
    RUNTIME_STATE_RUNNING,
    RUNTIME_STATE_ERROR,
} runtime_state_t;

// runtime_response_t 结构体 - 需要与 runtime_dyncall.c 中的定义一致
typedef struct runtime_response {
    int code;
    char *data;
    char *error;
} runtime_response_t;

typedef struct runtime_manager runtime_manager_t;

// 函数声明
runtime_manager_t* runtime_manager_create(const char *socket_demo_path);
int runtime_manager_start(runtime_manager_t *mgr);
int runtime_manager_stop(runtime_manager_t *mgr);
void runtime_manager_destroy(runtime_manager_t *mgr);
int runtime_manager_restart(runtime_manager_t *mgr);
int runtime_manager_call(runtime_manager_t *mgr, const char *method,
                         const char *params_json, runtime_response_t *response);
runtime_state_t runtime_manager_get_state(runtime_manager_t *mgr);
const char* runtime_manager_get_error(runtime_manager_t *mgr);
void runtime_response_free(runtime_response_t *resp);
int runtime_dyncall(const char *socket_path, const char *method, 
                    const char *params_json, char **result);

// ============ 测试配置 ============
#define DEFAULT_SOCKET_PATH "./core/soket_demo"

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

void print_response(const char *name, runtime_response_t *resp) {
    printf("  %s: code=%d", name, resp->code);
    if (resp->data) {
        if (strlen(resp->data) > 100) {
            printf(", data=%.100s...", resp->data);
        } else {
            printf(", data=%s", resp->data);
        }
    }
    if (resp->error) {
        printf(", error=%s", resp->error);
    }
    printf("\n");
}

// ============ 测试用例 ============

/**
 * 测试1: 创建和销毁管理器
 */
void test_manager_create_destroy(const char *socket_path) {
    printf("\n========== Test 1: Create & Destroy Manager ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (mgr) {
        test_assert(runtime_manager_get_state(mgr) == RUNTIME_STATE_STOPPED,
                    "Initial state should be STOPPED");
        runtime_manager_destroy(mgr);
        test_assert(1, "Destroy manager");
    }
}

/**
 * 测试2: 启动和停止
 */
void test_manager_start_stop(const char *socket_path) {
    printf("\n========== Test 2: Start & Stop ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    // 启动
    int ret = runtime_manager_start(mgr);
    test_assert(ret == 0 && runtime_manager_get_state(mgr) == RUNTIME_STATE_RUNNING,
                "Start runtime");
    
    // 重复启动 (应该成功)
    ret = runtime_manager_start(mgr);
    test_assert(ret == 0, "Start again (should be ok)");
    
    // 停止
    ret = runtime_manager_stop(mgr);
    test_assert(ret == 0 && runtime_manager_get_state(mgr) == RUNTIME_STATE_STOPPED,
                "Stop runtime");
    
    // 重复停止 (应该成功)
    ret = runtime_manager_stop(mgr);
    test_assert(ret == 0, "Stop again (should be ok)");
    
    runtime_manager_destroy(mgr);
}

/**
 * 测试3: 基本方法调用
 */
void test_basic_calls(const char *socket_path) {
    printf("\n========== Test 3: Basic Method Calls ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    runtime_manager_start(mgr);
    
    runtime_response_t resp;
    int ret;
    
    // 1. init
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "init", "{\"config\":{\"app\":\"test\"}}", &resp);
    print_response("init", &resp);
    test_assert(ret == 0 && resp.code == 200, "Init");
    runtime_response_free(&resp);
    
    // 2. status
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "status", "{}", &resp);
    print_response("status", &resp);
    test_assert(ret == 0 && resp.code == 200, "Status");
    runtime_response_free(&resp);
    
    // 3. update
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "update", "{\"data\":{\"key\":\"value\"}}", &resp);
    print_response("update", &resp);
    test_assert(ret == 0 && resp.code == 200, "Update");
    runtime_response_free(&resp);
    
    // 4. merge
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "merge", "{\"data\":{\"key2\":\"value2\"}}", &resp);
    print_response("merge", &resp);
    test_assert(ret == 0 && resp.code == 200, "Merge");
    runtime_response_free(&resp);
    
    // 5. msg
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "msg", "{\"type\":\"ping\"}", &resp);
    print_response("msg", &resp);
    test_assert(ret == 0 && resp.code == 200, "Process message");
    runtime_response_free(&resp);
    
    runtime_manager_stop(mgr);
    runtime_manager_destroy(mgr);
}

/**
 * 测试4: 暂停和恢复
 */
void test_pause_resume(const char *socket_path) {
    printf("\n========== Test 4: Pause & Resume ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    runtime_manager_start(mgr);
    
    // init
    runtime_response_t resp;
    memset(&resp, 0, sizeof(resp));
    runtime_manager_call(mgr, "init", "{}", &resp);
    runtime_response_free(&resp);
    
    // pause
    memset(&resp, 0, sizeof(resp));
    int ret = runtime_manager_call(mgr, "pause", "{}", &resp);
    print_response("pause", &resp);
    test_assert(ret == 0 && resp.code == 200, "Pause");
    runtime_response_free(&resp);
    
    // status after pause
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "status", "{}", &resp);
    print_response("status after pause", &resp);
    if (resp.data && strstr(resp.data, "\"paused\"")) {
        test_assert(1, "Status is paused");
    } else {
        test_assert(0, "Status should be paused");
    }
    runtime_response_free(&resp);
    
    // resume
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "resume", "{}", &resp);
    print_response("resume", &resp);
    test_assert(ret == 0 && resp.code == 200, "Resume");
    runtime_response_free(&resp);
    
    // status after resume
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "status", "{}", &resp);
    print_response("status after resume", &resp);
    if (resp.data && strstr(resp.data, "\"running\"")) {
        test_assert(1, "Status is running");
    } else {
        test_assert(0, "Status should be running");
    }
    runtime_response_free(&resp);
    
    runtime_manager_stop(mgr);
    runtime_manager_destroy(mgr);
}

/**
 * 测试5: 错误处理
 */
void test_error_handling(const char *socket_path) {
    printf("\n========== Test 5: Error Handling ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    // 未启动时调用 (应该失败)
    runtime_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int ret = runtime_manager_call(mgr, "status", "{}", &resp);
    printf("  Call without start: ret=%d\n", ret);
    test_assert(ret != 0, "Call without start should fail");
    runtime_response_free(&resp);
    
    // 启动
    runtime_manager_start(mgr);
    
    // 未知方法
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "unknown_method", "{}", &resp);
    print_response("unknown_method", &resp);
    test_assert(ret != 0 || resp.code != 200, "Unknown method should fail");
    runtime_response_free(&resp);
    
    // 无效 JSON
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "update", "{invalid}", &resp);
    print_response("invalid json", &resp);
    // 可能返回错误
    
    runtime_manager_stop(mgr);
    runtime_manager_destroy(mgr);
}

/**
 * 测试6: 重启功能
 */
void test_restart(const char *socket_path) {
    printf("\n========== Test 6: Restart ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    // 启动
    runtime_manager_start(mgr);
    test_assert(runtime_manager_get_state(mgr) == RUNTIME_STATE_RUNNING,
                "Start runtime");
    
    // 重启
    int ret = runtime_manager_restart(mgr);
    test_assert(ret == 0 && runtime_manager_get_state(mgr) == RUNTIME_STATE_RUNNING,
                "Restart runtime");
    
    // 验证功能正常
    runtime_response_t resp;
    memset(&resp, 0, sizeof(resp));
    ret = runtime_manager_call(mgr, "status", "{}", &resp);
    test_assert(ret == 0 && resp.code == 200, "Call after restart");
    runtime_response_free(&resp);
    
    runtime_manager_stop(mgr);
    runtime_manager_destroy(mgr);
}

/**
 * 测试7: 简化 API (类似 calldyn.c 风格)
 */
void test_simple_api(const char *socket_path) {
    printf("\n========== Test 7: Simple API (dyncall style) ==========\n");
    
    char *result = NULL;
    int ret;
    
    // init
    ret = runtime_dyncall(socket_path, "init", "{\"config\":{\"app\":\"test\"}}", &result);
    printf("  init: ret=%d, result=%s\n", ret, result ? result : "NULL");
    test_assert(ret == 0 && result != NULL, "Init via dyncall");
    free(result);
    result = NULL;
    
    // status
    ret = runtime_dyncall(socket_path, "status", "{}", &result);
    printf("  status: ret=%d, result=%s\n", ret, result ? result : "NULL");
    test_assert(ret == 0 && result != NULL, "Status via dyncall");
    free(result);
    result = NULL;
    
    // update
    ret = runtime_dyncall(socket_path, "update", "{\"data\":{\"test\":\"ok\"}}", &result);
    printf("  update: ret=%d, result=%s\n", ret, result ? result : "NULL");
    test_assert(ret == 0 && result != NULL, "Update via dyncall");
    free(result);
    result = NULL;
    
    // 错误调用 (未知方法)
    ret = runtime_dyncall(socket_path, "unknown", "{}", &result);
    printf("  unknown: ret=%d, result=%s\n", ret, result ? result : "NULL");
    test_assert(ret != 0, "Unknown method should fail");
    free(result);
    result = NULL;
}

/**
 * 测试8: 性能测试
 */
void test_performance(const char *socket_path) {
    printf("\n========== Test 8: Performance ==========\n");
    
    runtime_manager_t *mgr = runtime_manager_create(socket_path);
    test_assert(mgr != NULL, "Create manager");
    
    if (!mgr) return;
    
    runtime_manager_start(mgr);
    runtime_manager_call(mgr, "init", "{}", NULL);
    
    int iterations = 100;
    int success = 0;
    runtime_response_t resp;
    
    printf("  Running %d status calls...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        memset(&resp, 0, sizeof(resp));
        int ret = runtime_manager_call(mgr, "status", "{}", &resp);
        if (ret == 0 && resp.code == 200) {
            success++;
        }
        runtime_response_free(&resp);
    }
    
    printf("  Success: %d/%d (%.1f%%)\n", success, iterations,
           (float)success / iterations * 100);
    
    test_assert(success >= iterations * 0.95, "Performance test");
    
    runtime_manager_stop(mgr);
    runtime_manager_destroy(mgr);
}

// ============ 主函数 ============

int main(int argc, char **argv) {
    const char *socket_path = (argc > 1) ? argv[1] : DEFAULT_SOCKET_PATH;
    
    printf("========================================\n");
    printf("   runtime_dyncall - Test Suite\n");
    printf("========================================\n");
    printf("Socket demo: %s\n", socket_path);
    printf("========================================\n");
    
    // 检查文件是否存在
    FILE *fp = fopen(socket_path, "r");
    if (fp) {
        printf("✓ Socket demo found\n\n");
        fclose(fp);
    } else {
        printf("✗ Socket demo not found: %s\n", socket_path);
        printf("\n");
        printf("  To build socket_demo:\n");
        printf("    cd examples/soket_demo\n");
        printf("    cargo build --release\n");
        printf("\n");
        return 1;
    }
    
    // 运行所有测试
    test_manager_create_destroy(socket_path);
    test_manager_start_stop(socket_path);
    test_basic_calls(socket_path);
    test_pause_resume(socket_path);
    test_error_handling(socket_path);
    test_restart(socket_path);
    test_simple_api(socket_path);
    test_performance(socket_path);
    
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

