// tests/test_realtime_ownership.c - task ownership by WebSocket connection
#include "../include/realtime.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

static void check(int cond, const char *name) {
    if (cond) { passed++; printf("  [PASS] %s\n", name); }
    else { failed++; printf("  [FAIL] %s\n", name); }
}

/* Stubs for realtime.c dependencies (no WS / no plugin in unit tests). */
void event_push_with_type(const char *type, const char *data) {
    (void)type;
    (void)data;
}

void event_escape_json(const char *in, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (!in) { out[0] = 0; return; }
    size_t i = 0;
    for (; in[i] && i + 1 < out_size; i++) out[i] = in[i];
    out[i] = 0;
}

int call_local_dyn_socklibffi(const char *lib_name, const char *json_in,
                              char **json_out) {
    (void)lib_name;
    (void)json_in;
    if (json_out) *json_out = NULL;
    return -1;
}

int main(void) {
    printf("=== test_realtime_ownership ===\n");

    rt_manager_t *mgr = rt_init();
    check(mgr != NULL, "rt_init");

    rt_task_t *a = rt_create_task(mgr, "u1", "/tmp/a.sock");
    rt_task_t *b = rt_create_task(mgr, "u2", "/tmp/b.sock");
    rt_task_t *c = rt_create_task(mgr, "u3", "/tmp/c.sock");
    check(a && b && c, "create three tasks");
    check(rt_get_task_count(mgr) == 3, "count == 3");

    a->conn_id = 1;
    b->conn_id = 2;
    c->conn_id = 1;

    printf("== remove by conn ==\n");
    check(rt_remove_tasks_by_conn(mgr, 1) == 2, "removed 2 tasks for conn 1");
    check(rt_get_task_count(mgr) == 1, "count == 1");
    check(rt_find_task_by_uuid(mgr, "u1") == NULL, "u1 removed");
    check(rt_find_task_by_uuid(mgr, "u2") != NULL, "u2 still exists");
    check(rt_find_task_by_uuid(mgr, "u3") == NULL, "u3 removed");

    printf("== unknown conn ==\n");
    check(rt_remove_tasks_by_conn(mgr, 99) == 0, "unknown conn removes 0");
    check(rt_get_task_count(mgr) == 1, "count unchanged");

    rt_cleanup(mgr);
    check(rt_get_task_count(mgr) == 0, "cleanup empties tasks");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
