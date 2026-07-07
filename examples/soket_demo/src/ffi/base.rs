use std::ffi::{c_char};
use std::os::raw::c_int;
use serde_json::json;

use crate::utils::cjson::{parse_json_value, write_success, write_error};
use crate::core::runtime::{init_runtime, close_runtime, get_realtime_data};

// =====================================================
// 基础 FFI 函数
// =====================================================

/// 初始化
#[no_mangle]
pub extern "C" fn process_init(
    json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_init called");
    
    let input = parse_json_value(json_in);
    let config = input.get("config").cloned();
    
    match init_runtime(config) {
        Ok(_msg) => {
            let data = get_realtime_data();
            write_success(json_out, "init success", json!({
                "state": data,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 关闭
#[no_mangle]
pub extern "C" fn process_close(
    _json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_close called");
    
    match close_runtime() {
        Ok(msg) => {
            write_success(json_out, &msg, json!({}))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}