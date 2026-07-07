use std::ffi::{c_char};
use std::os::raw::c_int;
use serde_json::json;

use crate::utils::cjson::{parse_json_value, write_success, write_error};
use crate::core::runtime::{
    update_data,
    merge_data,
    pause_runtime,
    resume_runtime,
    get_realtime_data,
    process_data,
};

/// 更新数据（覆盖）
#[no_mangle]
pub extern "C" fn process_update(
    json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_update called");
    
    let input = parse_json_value(json_in);
    let data = input.get("data").cloned().unwrap_or(json!({}));
    
    match update_data(data) {
        Ok(()) => {
            let state = get_realtime_data();
            write_success(json_out, "update success", json!({
                "state": state,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 合并数据（追加）
#[no_mangle]
pub extern "C" fn process_merge(
    json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_merge called");
    
    let input = parse_json_value(json_in);
    let data = input.get("data").cloned().unwrap_or(json!({}));
    
    match merge_data(data) {
        Ok(()) => {
            let state = get_realtime_data();
            write_success(json_out, "merge success", json!({
                "state": state,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 处理消息（调度器处理）
#[no_mangle]
pub extern "C" fn process_msg(
    json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_msg called");
    
    let input = parse_json_value(json_in);
    
    match process_data(input) {
        Ok(result) => {
            write_success(json_out, "process success", json!({
                "result": result,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 暂停
#[no_mangle]
pub extern "C" fn process_pause(
    _json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_pause called");
    
    match pause_runtime() {
        Ok(()) => {
            let state = get_realtime_data();
            write_success(json_out, "paused", json!({
                "state": state,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 恢复
#[no_mangle]
pub extern "C" fn process_resume(
    _json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_resume called");
    
    match resume_runtime() {
        Ok(()) => {
            let state = get_realtime_data();
            write_success(json_out, "resumed", json!({
                "state": state,
            }))
        }
        Err(e) => {
            write_error(json_out, -1, &e)
        }
    }
}

/// 状态查询（实时获取数据）
#[no_mangle]
pub extern "C" fn process_status(
    _json_in: *const c_char,
    json_out: *mut c_char,
) -> c_int {
    println!("[RUST] process_status called");
    
    let state = get_realtime_data();
    write_success(json_out, "status ok", json!({
        "state": state,
    }))
}