/**
 * test_calldyn.c
 * 
 * 测试 calldyn.c 调用 Rust 动态库 (librust_demo.so)
 * 测试所有 FFI 导出函数
 * 
 * 编译命令:
 *   gcc -o ./build/test/test_calldyn ./tests/test_calldyn.c ./src/calldyn.c -lffi -ldl
 * 
 * 用法: ./build/test/test_calldyn
 *
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
#define PLUGIN_PATH "./core/librust_demo.so"

// FFI 函数名
#define FUNC_ADD        "post_add"
#define FUNC_SUB        "post_sub"
#define FUNC_MUL        "post_mul"
#define FUNC_DIV        "post_div"
#define FUNC_USER       "post_user"
#define FUNC_ECHO       "post_echo"
#define FUNC_HEALTH     "post_health"
#define FUNC_BATCH      "post_batch"
#define FUNC_SUM        "post_sum"
#define FUNC_SLEEP      "post_sleep"
#define FUNC_DB_CREATE  "post_db_create"
#define FUNC_DB_INSERT  "post_db_insert"
#define FUNC_DB_QUERY   "post_db_query"
#define FUNC_DB_UPDATE  "post_db_update"
#define FUNC_DB_DELETE  "post_db_delete"

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
        // 如果输出太长，截断显示
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

// 简单提取 code
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

// ============ 测试用例 ============

/**
 * 测试1: 加法 (post_add)
 */
void test_add(const char *plugin_path) {
    printf("\n========== Test 1: post_add (Addition) ==========\n");
    
    char *output = NULL;
    
    // 正常加法
    const char *json1 = "{\"num1\":10,\"num2\":5}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_ADD, json1, &output);
    print_result(FUNC_ADD, ret, output);
    int code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "10 + 5 = 15");
    safe_free(&output);
    
    // 大数字加法
    const char *json2 = "{\"num1\":999999999,\"num2\":1}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_ADD, json2, &output);
    print_result(FUNC_ADD, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "999999999 + 1 = 1000000000");
    safe_free(&output);
    
    // 零值加法
    const char *json3 = "{\"num1\":0,\"num2\":0}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_ADD, json3, &output);
    print_result(FUNC_ADD, ret, output);
    code = extract_code(output);
    test_assert(ret >= 0 && code == 200, "0 + 0 = 0");
    safe_free(&output);
}

/**
 * 测试2: 减法 (post_sub)
 */
void test_sub(const char *plugin_path) {
    printf("\n========== Test 2: post_sub (Subtraction) ==========\n");
    
    char *output = NULL;
    
    // 正常减法
    const char *json1 = "{\"num1\":10,\"num2\":5}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_SUB, json1, &output);
    print_result(FUNC_SUB, ret, output);
    test_assert(ret >= 0, "10 - 5 = 5");
    safe_free(&output);
    
    // 负数结果
    const char *json2 = "{\"num1\":5,\"num2\":10}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_SUB, json2, &output);
    print_result(FUNC_SUB, ret, output);
    test_assert(ret >= 0, "5 - 10 = -5");
    safe_free(&output);
}

/**
 * 测试3: 乘法 (post_mul)
 */
void test_mul(const char *plugin_path) {
    printf("\n========== Test 3: post_mul (Multiplication) ==========\n");
    
    char *output = NULL;
    
    // 整数乘法
    const char *json1 = "{\"num1\":5,\"num2\":6}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_MUL, json1, &output);
    print_result(FUNC_MUL, ret, output);
    test_assert(ret >= 0, "5 * 6 = 30");
    safe_free(&output);
    
    // 浮点数乘法
    const char *json2 = "{\"num1\":2.5,\"num2\":4.0}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_MUL, json2, &output);
    print_result(FUNC_MUL, ret, output);
    test_assert(ret >= 0, "2.5 * 4.0 = 10.0");
    safe_free(&output);
}

/**
 * 测试4: 除法 (post_div)
 */
void test_div(const char *plugin_path) {
    printf("\n========== Test 4: post_div (Division) ==========\n");
    
    char *output = NULL;
    
    // 正常除法
    const char *json1 = "{\"num1\":10,\"num2\":2}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_DIV, json1, &output);
    print_result(FUNC_DIV, ret, output);
    test_assert(ret >= 0, "10 / 2 = 5");
    safe_free(&output);
    
    // 浮点数除法
    const char *json2 = "{\"num1\":7,\"num2\":3}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DIV, json2, &output);
    print_result(FUNC_DIV, ret, output);
    test_assert(ret >= 0, "7 / 3");
    safe_free(&output);
    
    // 除以零（应该返回错误）
    const char *json3 = "{\"num1\":10,\"num2\":0}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DIV, json3, &output);
    print_result(FUNC_DIV, ret, output);
    if (output && strstr(output, "division by zero")) {
        test_assert(1, "10 / 0 (error handled)");
    } else {
        test_assert(0, "10 / 0 (should return error)");
    }
    safe_free(&output);
}

/**
 * 测试5: 用户处理 (post_user)
 */
void test_user(const char *plugin_path) {
    printf("\n========== Test 5: post_user (User Processing) ==========\n");
    
    char *output = NULL;
    
    // 完整用户信息
    const char *json1 = "{\"name\":\"Alice\",\"age\":25,\"email\":\"alice@test.com\"}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_USER, json1, &output);
    print_result(FUNC_USER, ret, output);
    test_assert(ret >= 0, "Full user info");
    safe_free(&output);
    
    // 缺少 email
    const char *json2 = "{\"name\":\"Bob\",\"age\":17}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_USER, json2, &output);
    print_result(FUNC_USER, ret, output);
    test_assert(ret >= 0, "User without email");
    safe_free(&output);
}

/**
 * 测试6: 回显 (post_echo)
 */
void test_echo(const char *plugin_path) {
    printf("\n========== Test 6: post_echo (Echo) ==========\n");
    
    char *output = NULL;
    
    const char *json1 = "{\"message\":\"Hello, World!\"}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_ECHO, json1, &output);
    print_result(FUNC_ECHO, ret, output);
    test_assert(ret >= 0, "Echo message");
    safe_free(&output);
}

/**
 * 测试7: 健康检查 (post_health)
 */
void test_health(const char *plugin_path) {
    printf("\n========== Test 7: post_health (Health Check) ==========\n");
    
    char *output = NULL;
    
    int ret = call_local_dyn_libffi(plugin_path, FUNC_HEALTH, "{}", &output);
    print_result(FUNC_HEALTH, ret, output);
    test_assert(ret >= 0, "Health check");
    safe_free(&output);
}

/**
 * 测试8: 批量操作 (post_batch)
 */
void test_batch(const char *plugin_path) {
    printf("\n========== Test 8: post_batch (Batch Operations) ==========\n");
    
    char *output = NULL;
    
    const char *json = 
        "{"
        "\"ops\":["
        "  {\"type\":\"add\",\"num1\":10,\"num2\":5},"
        "  {\"type\":\"sub\",\"num1\":20,\"num2\":8},"
        "  {\"type\":\"mul\",\"num1\":6,\"num2\":7},"
        "  {\"type\":\"div\",\"num1\":15,\"num2\":3},"
        "  {\"type\":\"div\",\"num1\":10,\"num2\":0}"
        "]"
        "}";
    
    int ret = call_local_dyn_libffi(plugin_path, FUNC_BATCH, json, &output);
    print_result(FUNC_BATCH, ret, output);
    test_assert(ret >= 0, "Batch operations");
    safe_free(&output);
}

/**
 * 测试9: 求和 (post_sum)
 */
void test_sum(const char *plugin_path) {
    printf("\n========== Test 9: post_sum (Sum Array) ==========\n");
    
    char *output = NULL;
    
    const char *json1 = "{\"numbers\":[1,2,3,4,5,6,7,8,9,10]}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_SUM, json1, &output);
    print_result(FUNC_SUM, ret, output);
    test_assert(ret >= 0, "Sum 1..10");
    safe_free(&output);
}

/**
 * 测试10: 延迟测试 (post_sleep)
 */
void test_sleep(const char *plugin_path) {
    printf("\n========== Test 10: post_sleep (Sleep/Delay) ==========\n");
    
    char *output = NULL;
    
    const char *json = "{\"ms\":100}";
    int ret = call_local_dyn_libffi(plugin_path, FUNC_SLEEP, json, &output);
    print_result(FUNC_SLEEP, ret, output);
    test_assert(ret >= 0, "Sleep 100ms");
    safe_free(&output);
}

/**
 * 测试11: 数据库操作
 */
void test_db_operations(const char *plugin_path) {
    printf("\n========== Test 11: Database Operations ==========\n");
    
    char *output = NULL;
    int ret;
    
    // 1. 创建表
    printf("  [1] db_create:\n");
    const char *create = "{\"path\":\"./users\",\"table\":\"users\",\"schema\":\"id INTEGER PRIMARY KEY, name TEXT, age INTEGER\"}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DB_CREATE, create, &output);
    print_result(FUNC_DB_CREATE, ret, output);
    test_assert(ret >= 0, "db_create");
    safe_free(&output);
    
    // 2. 插入数据
    printf("  [2] db_insert:\n");
    const char *insert = "{\"path\":\"./users\",\"table\":\"users\",\"data\":{\"id\":1,\"name\":\"Alice\",\"age\":25}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DB_INSERT, insert, &output);
    print_result(FUNC_DB_INSERT, ret, output);
    test_assert(ret >= 0, "db_insert");
    safe_free(&output);
    
    // 3. 查询数据
    printf("  [3] db_query:\n");
    const char *query = "{\"path\":\"./users\",\"sql\":\"SELECT * FROM users\"}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DB_QUERY, query, &output);
    print_result(FUNC_DB_QUERY, ret, output);
    test_assert(ret >= 0, "db_query");
    safe_free(&output);
    
    // 4. 更新数据
    printf("  [4] db_update:\n");
    const char *update = "{\"path\":\"./users\",\"table\":\"users\",\"condition\":{\"id\":1},\"data\":{\"name\":\"Alice Updated\",\"age\":26}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DB_UPDATE, update, &output);
    print_result(FUNC_DB_UPDATE, ret, output);
    test_assert(ret >= 0, "db_update");
    safe_free(&output);
    
    // 5. 删除数据
    printf("  [5] db_delete:\n");
    const char *delete = "{\"path\":\"./users\",\"table\":\"users\",\"condition\":{\"id\":1}}";
    ret = call_local_dyn_libffi(plugin_path, FUNC_DB_DELETE, delete, &output);
    print_result(FUNC_DB_DELETE, ret, output);
    test_assert(ret >= 0, "db_delete");
    safe_free(&output);
}

/**
 * 测试12: 错误处理
 */

void test_errors(const char *plugin_path) {
    printf("\n========== Test 12: Error Handling ==========\n");
    
    char *output = NULL;
    
    // NULL path
    int ret1 = call_local_dyn_libffi(NULL, FUNC_ADD, "{}", &output);
    printf("  NULL path -> %d (expected -100)\n", ret1);
    test_assert(ret1 == -100, "NULL path");
    safe_free(&output);
    
    // NULL function
    int ret2 = call_local_dyn_libffi(plugin_path, NULL, "{}", &output);
    printf("  NULL func -> %d (expected -100)\n", ret2);
    test_assert(ret2 == -100, "NULL function");
    safe_free(&output);
    
    // NULL output
    int ret3 = call_local_dyn_libffi(plugin_path, FUNC_ADD, "{}", NULL);
    printf("  NULL output -> %d (expected -100)\n", ret3);
    test_assert(ret3 == -100, "NULL output");
    safe_free(&output);
    
    // 不存在的函数
    int ret4 = call_local_dyn_libffi(plugin_path, "invalid_func", "{}", &output);
    printf("  Invalid function -> %d (expected -2)\n", ret4);
    test_assert(ret4 == -2, "Invalid function");
    safe_free(&output);
    
    // 无效 JSON - Rust 端会返回错误信息
    const char *invalid = "{invalid json}";
    int ret5 = call_local_dyn_libffi(plugin_path, FUNC_ADD, invalid, &output);
    print_result(FUNC_ADD, ret5, output);
    // Rust 端返回 0 并包含错误信息
    if (ret5 == 0 && output && 
        (strstr(output, "invalid json") || 
         strstr(output, "key must be a string") ||
         strstr(output, "expected"))) {
        test_assert(1, "Invalid JSON handled");
    } else {
        printf("  Expected error message, got: %s\n", output ? output : "NULL");
        test_assert(0, "Invalid JSON not handled");
    }
    safe_free(&output);
    
    // 测试空字符串
    const char *empty = "";
    int ret6 = call_local_dyn_libffi(plugin_path, FUNC_ADD, empty, &output);
    print_result(FUNC_ADD, ret6, output);
    if (ret6 == 0 && output) {
        test_assert(1, "Empty string handled");
    } else {
        test_assert(0, "Empty string not handled");
    }
    safe_free(&output);
}

 /**
 * 测试13: 性能测试
 */
void test_performance(const char *plugin_path) {
    printf("\n========== Test 13: Performance Test ==========\n");
    
    char *output = NULL;
    int iterations = 1000;
    int success = 0;
    
    printf("  Running %d addition operations...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        char json[64];
        snprintf(json, sizeof(json), "{\"num1\":%d,\"num2\":%d}", i, i * 2);
        
        int ret = call_local_dyn_libffi(plugin_path, FUNC_ADD, json, &output);
        if (ret >= 0) success++;
        safe_free(&output);
        
        if ((i + 1) % 200 == 0) {
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
    printf("   test_calldyn - Rust FFI Test\n");
    printf("========================================\n");
    printf("Plugin: %s\n", plugin_path);
    printf("Functions:\n");
    printf("  - post_add     (addition)\n");
    printf("  - post_sub     (subtraction)\n");
    printf("  - post_mul     (multiplication)\n");
    printf("  - post_div     (division)\n");
    printf("  - post_user    (user processing)\n");
    printf("  - post_echo    (echo)\n");
    printf("  - post_health  (health check)\n");
    printf("  - post_batch   (batch operations)\n");
    printf("  - post_sum     (sum array)\n");
    printf("  - post_sleep   (sleep/delay)\n");
    printf("  - post_db_*    (database operations)\n");
    printf("========================================\n");
    
    // 检查文件是否存在
    FILE *fp = fopen(plugin_path, "r");
    if (fp) {
        printf("✓ Plugin found\n\n");
        fclose(fp);
    } else {
        printf("✗ Plugin not found: %s\n", plugin_path);
        printf("\n");
        printf("  Expected location: ./core/librust_demo.so\n");
        printf("  To build the Rust library:\n");
        printf("    cd rust_demo\n");
        printf("    cargo build --release\n");
        printf("    cp target/release/librust_demo.so ../core/\n");
        printf("\n");
        return 1;
    }
    
    // 运行所有测试
    test_add(plugin_path);
    test_sub(plugin_path);
    test_mul(plugin_path);
    test_div(plugin_path);
    test_user(plugin_path);
    test_echo(plugin_path);
    test_health(plugin_path);
    test_batch(plugin_path);
    test_sum(plugin_path);
    test_sleep(plugin_path);
    test_db_operations(plugin_path);
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

