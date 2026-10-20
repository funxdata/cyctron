// sse_demo - cyctron SSE 事件源示例
//
// C ABI: int <symbol>(const char *json_in, char *json_out)
// 输出缓冲区 64 KB，输出为紧凑 JSON，字段顺序固定为 id,event,data。
//
// 导出符号：
//   process_init  {"config":{"interval_ms":200,"event":"tick"}} -> {"ok":true,...}
//   sse_next      {"after_id":N} -> {"id":N+1,"event":"tick","data":{...}}
//   sse_status    {} -> {"ok":true,"seq":N,"interval_ms":200,"event":"tick"}
//   sse_reset     {} -> {"ok":true,"seq":0}

use serde_json::Value;
use std::ffi::c_char;
use std::os::raw::c_int;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

const OUT_BUF_SIZE: usize = 65536;

struct State {
    seq: u64,
    interval_ms: u32,
    event: String,
}

static STATE: OnceLock<Mutex<State>> = OnceLock::new();

fn state() -> &'static Mutex<State> {
    STATE.get_or_init(|| {
        Mutex::new(State {
            seq: 0,
            interval_ms: 200,
            event: "tick".to_string(),
        })
    })
}

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

fn write_out_str(json_out: *mut c_char, s: &str) -> c_int {
    if json_out.is_null() {
        return -1;
    }
    let bytes = s.as_bytes();
    let truncated = bytes.len() >= OUT_BUF_SIZE;
    let n = bytes.len().min(OUT_BUF_SIZE - 1);
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), json_out as *mut u8, n);
        *json_out.add(n) = 0;
    }
    if truncated { -1 } else { 0 }
}

// 事件名只保留安全字符，避免破坏 SSE 行格式
fn safe_event(name: &str) -> String {
    let filtered: String = name
        .chars()
        .filter(|c| c.is_ascii_alphanumeric() || *c == '_' || *c == '-')
        .collect();
    if filtered.is_empty() {
        "tick".to_string()
    } else {
        filtered
    }
}

fn now_secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn process_init(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);
    let cfg = input.get("config").cloned().unwrap_or(Value::Null);

    let interval = cfg
        .get("interval_ms")
        .and_then(|v| v.as_u64())
        .unwrap_or(200)
        .clamp(50, 5000) as u32;
    let event = safe_event(
        cfg.get("event")
            .and_then(|v| v.as_str())
            .unwrap_or("tick"),
    );

    let mut st = state().lock().unwrap();
    st.seq = 0;
    st.interval_ms = interval;
    st.event = event.clone();
    drop(st);

    let out = format!(
        "{{\"ok\":true,\"interval_ms\":{},\"event\":\"{}\"}}",
        interval, event
    );
    write_out_str(json_out, &out)
}

#[no_mangle]
pub extern "C" fn sse_next(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let input = parse_in(json_in);
    let after_id = input
        .get("after_id")
        .and_then(|v| v.as_u64())
        .unwrap_or(0);

    let mut st = state().lock().unwrap();
    let id = std::cmp::max(after_id.saturating_add(1), st.seq.saturating_add(1));
    st.seq = id;
    let event = st.event.clone();
    drop(st);

    let ts = now_secs();
    let data = format!(
        "{{\"seq\":{},\"ts\":{},\"message\":\"sse tick #{}\"}}",
        id, ts, id
    );
    let out = format!(
        "{{\"id\":{},\"event\":\"{}\",\"data\":{}}}",
        id, event, data
    );
    write_out_str(json_out, &out)
}

#[no_mangle]
pub extern "C" fn sse_status(_json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let st = state().lock().unwrap();
    let out = format!(
        "{{\"ok\":true,\"seq\":{},\"interval_ms\":{},\"event\":\"{}\"}}",
        st.seq, st.interval_ms, st.event
    );
    drop(st);
    write_out_str(json_out, &out)
}

#[no_mangle]
pub extern "C" fn sse_reset(_json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let mut st = state().lock().unwrap();
    st.seq = 0;
    drop(st);
    write_out_str(json_out, "{\"ok\":true,\"seq\":0}")
}
