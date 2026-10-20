// libary_demo - cyctron libary 目录动态库示例
// C ABI: int <method>_handle(const char *json_in, char *json_out)
// 输出缓冲约定 64KB（与 calldyn.c PLUGIN_JSON_BUF_SIZE 一致），输出为紧凑 JSON。

use serde_json::{json, Value};
use std::collections::HashMap;
use std::ffi::c_char;
use std::os::raw::c_int;
use std::ptr;
use std::sync::OnceLock;
use std::sync::RwLock;

const OUT_BUF_SIZE: usize = 65536;

// 全局资源存储：id -> item 索引（避免大表线性扫描），
// 进程内常驻（依赖 C 端 dlopen 句柄缓存）
type ItemMap = HashMap<u64, Value>;
static ITEMS: OnceLock<RwLock<ItemMap>> = OnceLock::new();

fn items() -> &'static RwLock<ItemMap> {
    ITEMS.get_or_init(|| {
        RwLock::new(HashMap::from([
            (1, json!({"id": 1, "name": "alpha", "type": "img"})),
            (2, json!({"id": 2, "name": "beta", "type": "js"})),
            (3, json!({"id": 3, "name": "gamma", "type": "json"})),
        ]))
    })
}

static NEXT_ID: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(4);

// 解析输入：NULL/空串/非法 JSON 一律视为 Value::Null（无参数）
fn parse_in(json_in: *const c_char) -> Value {
    if json_in.is_null() {
        return Value::Null;
    }
    let s = unsafe { std::ffi::CStr::from_ptr(json_in) }.to_string_lossy();
    let s = s.trim();
    if s.is_empty() {
        return Value::Null;
    }
    serde_json::from_str(s).unwrap_or(Value::Null)
}

fn write_out(json_out: *mut c_char, v: &Value) -> c_int {
    if json_out.is_null() {
        return -1;
    }
    let s = v.to_string();
    write_out_str(json_out, &s)
}

fn write_out_str(json_out: *mut c_char, s: &str) -> c_int {
    if json_out.is_null() {
        return -1;
    }
    let bytes = s.as_bytes();
    let n = bytes.len().min(OUT_BUF_SIZE - 1);
    unsafe {
        ptr::copy_nonoverlapping(bytes.as_ptr(), json_out as *mut u8, n);
        *json_out.add(n) = 0;
    }
    0
}

// ---------------- GET：无参数 → 全部资源；带 id → 单个资源 ----------------
#[no_mangle]
pub extern "C" fn get_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);
    let guard = items().read().unwrap();

    if input.is_null() {
        // 手工拼装避免 json! 宏对全部 items 的深拷贝（大表时的主要开销）
        let mut list: Vec<&Value> = guard.values().collect();
        list.sort_by_key(|it| it["id"].as_u64().unwrap_or(0));
        let mut s = String::with_capacity(64 + list.len() * 64);
        s.push_str("{\"ok\":true,\"method\":\"GET\",\"params\":null,\"data\":{\"items\":[");
        for (i, it) in list.iter().enumerate() {
            if i > 0 {
                s.push(',');
            }
            s.push_str(&it.to_string());
        }
        s.push_str(&format!("],\"count\":{}}}}}", guard.len()));
        return write_out_str(json_out, &s);
    }

    let params = input.as_object().cloned().unwrap_or_default();
    if let Some(id) = params.get("id").and_then(|v| v.as_u64()) {
        return match guard.get(&id) {
            Some(item) => {
                let resp = json!({
                    "ok": true,
                    "method": "GET",
                    "params": {"id": id},
                    "data": item,
                });
                write_out(json_out, &resp)
            }
            None => {
                let resp = json!({
                    "ok": false,
                    "method": "GET",
                    "error": "not_found",
                    "params": {"id": id},
                });
                write_out(json_out, &resp)
            }
        };
    }

    // 有参数但无 id：原样回显参数
    let resp = json!({
        "ok": true,
        "method": "GET",
        "params": Value::Object(params),
        "data": {"count": guard.len()},
    });
    drop(guard);
    write_out(json_out, &resp)
}

// ---------------- POST：空 body → empty_body；带 body → 创建资源 ----------------
#[no_mangle]
pub extern "C" fn post_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);

    if !input.is_object() || input.as_object().map_or(true, |o| o.is_empty()) {
        let resp = json!({"ok": false, "method": "POST", "error": "empty_body"});
        return write_out(json_out, &resp);
    }

    let id = NEXT_ID.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
    let mut created = input.as_object().cloned().unwrap_or_default();
    created.insert("id".to_string(), json!(id));
    items().write().unwrap().insert(id, Value::Object(created.clone()));
    let resp = json!({
        "ok": true,
        "method": "POST",
        "id": id,
        "created": Value::Object(created),
    });
    write_out(json_out, &resp)
}

// ---------------- PATCH：{"id":N,"patch":{...}} → 部分更新 ----------------
#[no_mangle]
pub extern "C" fn patch_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);
    let missing = |err: &str| write_out(json_out, &json!({"ok": false, "method": "PATCH", "error": err}));

    let id = match input.get("id").and_then(|v| v.as_u64()) {
        Some(id) => id,
        None => return missing("missing_id"),
    };
    let patch = match input.get("patch").and_then(|v| v.as_object()) {
        Some(p) if !p.is_empty() => p.clone(),
        _ => return missing("empty_patch"),
    };

    let mut guard = items().write().unwrap();
    let Some(base) = guard.get(&id).cloned() else {
        return missing("not_found");
    };

    let mut patched = base.as_object().cloned().unwrap_or_default();
    for (k, v) in patch {
        patched.insert(k, v);
    }
    let patched = Value::Object(patched);
    guard.insert(id, patched.clone());
    let resp = json!({
        "ok": true,
        "method": "PATCH",
        "patched": patched,
    });
    write_out(json_out, &resp)
}

// ---------------- UPDATE：{"id":N,...} → 除 id 外整体替换 ----------------
#[no_mangle]
pub extern "C" fn update_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    update_impl(json_in, json_out, "UPDATE")
}

// ---------------- PUT：与 UPDATE 同语义 ----------------
#[no_mangle]
pub extern "C" fn put_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    update_impl(json_in, json_out, "PUT")
}

fn update_impl(json_in: *const c_char, json_out: *mut c_char, method: &str) -> c_int {
    let input = parse_in(json_in);
    let missing = |err: &str| {
        write_out(json_out, &json!({"ok": false, "method": method, "error": err}))
    };

    let id = match input.get("id").and_then(|v| v.as_u64()) {
        Some(id) => id,
        None => {
            let has_body = input.as_object().map_or(false, |o| !o.is_empty());
            return missing(if has_body { "missing_id" } else { "empty_body" });
        }
    };

    let mut guard = items().write().unwrap();
    if !guard.contains_key(&id) {
        return missing("not_found");
    }

    let mut updated = input.as_object().cloned().unwrap_or_default();
    updated.insert("id".to_string(), json!(id));
    let updated = Value::Object(updated);
    guard.insert(id, updated.clone());
    let resp = json!({
        "ok": true,
        "method": method,
        "updated": updated,
    });
    write_out(json_out, &resp)
}

// ---------------- DELETE：{"id":N} → 删除 ----------------
#[no_mangle]
pub extern "C" fn delete_handle(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);
    let missing = |err: &str| write_out(json_out, &json!({"ok": false, "method": "DELETE", "error": err}));

    let id = match input.get("id").and_then(|v| v.as_u64()) {
        Some(id) => id,
        None => return missing("missing_id"),
    };

    let mut guard = items().write().unwrap();
    if guard.remove(&id).is_none() {
        return missing("not_found");
    }

    let resp = json!({
        "ok": true,
        "method": "DELETE",
        "deleted": {"id": id},
    });
    write_out(json_out, &resp)
}
