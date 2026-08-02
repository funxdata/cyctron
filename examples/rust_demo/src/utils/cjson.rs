use serde_json::{json, Value};
use serde::de::DeserializeOwned;
use std::ffi::{CStr, c_char};
use std::os::raw::c_int;

/// 写 JSON 到 C buffer
pub fn write_json_out(json_out: *mut c_char, value: Value) -> c_int {
    let out_str = value.to_string();
    let bytes = out_str.as_bytes();

    unsafe {
        if json_out.is_null() {
            return -1;
        }

        if bytes.len() >= 65536 {
            return -2;
        }

        std::ptr::copy_nonoverlapping(bytes.as_ptr(), json_out as *mut u8, bytes.len());

        *json_out.add(bytes.len()) = 0;
    }

    0
}

/// 返回错误 JSON
pub fn write_error(json_out: *mut c_char, code: i32, msg: &str) -> c_int {
    write_json_out(
        json_out,
        json!({
            "code": code,
            "data": {},
            "msg": msg
        }),
    )
}

/// 返回成功 JSON
pub fn write_success(json_out: *mut c_char, msg: &str, data: Value) -> c_int {
    write_json_out(
        json_out,
        json!({
            "code": 200,
            "data": data,
            "msg": msg
        }),
    )
}

/// 解析 JSON
pub fn parse_json<T>(
    json_in: *const c_char,
) -> Result<T, String>
where
    T: DeserializeOwned,
{
    if json_in.is_null() {
        return Err("json_in is null".into());
    }

    let body = unsafe {
        CStr::from_ptr(json_in)
            .to_str()
            .map_err(|e| e.to_string())?
    };

    serde_json::from_str(body)
        .map_err(|e| e.to_string())
}
