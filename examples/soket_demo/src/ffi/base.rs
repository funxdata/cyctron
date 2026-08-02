// src/ffi/base.rs
use serde_json::json;
use std::ffi::c_char;
use std::os::raw::c_int;

use crate::core::runtime::{close_runtime, get_status, init_runtime};
use crate::utils::cjson::{parse_json_value, write_error, write_success};

/// 初始化运行时
/// 
/// # 参数
/// - `json_in`: 输入 JSON，包含 `config.uuid` 字段
/// - `json_out`: 输出 JSON 缓冲区
/// 
/// # 返回
/// - `0`: 成功
/// - `-1`: 失败
#[no_mangle]
pub extern "C" fn process_init(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_json_value(json_in);
    
    // 提取 uuid
    let uuid = input
        .get("config")
        .and_then(|c| c.get("uuid"))
        .and_then(|v| v.as_str())
        .unwrap_or("default")
        .to_string();
    
    println!("[RUST] process_init uuid: {}", uuid);

    // 将 uuid 放入 config 传给 runtime
    let mut config = input.get("config").cloned().unwrap_or_default();
    if let Some(obj) = config.as_object_mut() {
        obj.insert("uuid".to_string(), json!(uuid));
    }

    match init_runtime(Some(config)) {
        Ok(msg) => {
            let data = get_status();
            write_success(json_out, &msg, json!({ "state": data, "uuid": uuid }))
        }
        Err(e) => write_error(json_out, -1, &e),
    }
}

