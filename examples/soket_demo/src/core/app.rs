// src/core/app.rs
//! 应用核心逻辑模块

use serde_json::{json, Value};

/// 处理接收到的数据
pub fn process_data(data: &[u8]) -> Vec<u8> {
    if let Ok(json) = serde_json::from_slice::<Value>(data) {
        return process_json(json);
    }

    if let Ok(text) = String::from_utf8(data.to_vec()) {
        return process_text(&text);
    }

    process_binary(data)
}

fn process_json(json: Value) -> Vec<u8> {
    let msg_type = json
        .get("type")
        .and_then(|v| v.as_str())
        .unwrap_or("unknown");

    match msg_type {
        "sensor" => handle_sensor(json),
        "command" => handle_command(json),
        "echo" => handle_echo(json),
        "test" => handle_test(json),
        _ => handle_default(json),
    }
}

fn process_text(text: &str) -> Vec<u8> {
    match text.trim() {
        "ping" => b"pong".to_vec(),
        "hello" => b"Hello from IPC!".to_vec(),
        "status" => b"Status: running".to_vec(),
        "test" => serde_json::to_vec(&json!({
            "type": "test_response",
            "status": "ok"
        }))
        .unwrap_or_default(),
        _ => format!("Echo: {}", text).into_bytes(),
    }
}

fn process_binary(data: &[u8]) -> Vec<u8> {
    json!({
        "type": "binary_response",
        "size": data.len(),
        "hex": hex::encode(data),
    })
    .to_string()
    .into_bytes()
}

// ============================================
//  处理器
// ============================================

fn handle_sensor(json: Value) -> Vec<u8> {
    let sensor = json
        .get("sensor")
        .and_then(|v| v.as_str())
        .unwrap_or("unknown");
    let value = json.get("value").and_then(|v| v.as_f64()).unwrap_or(0.0);

    json!({
        "type": "sensor_ack",
        "sensor": sensor,
        "value": value,
        "status": "ok"
    })
    .to_string()
    .into_bytes()
}

fn handle_command(json: Value) -> Vec<u8> {
    let cmd = json
        .get("cmd")
        .and_then(|v| v.as_str())
        .unwrap_or("unknown");

    json!({
        "type": "command_ack",
        "cmd": cmd,
        "status": "executed"
    })
    .to_string()
    .into_bytes()
}

fn handle_echo(json: Value) -> Vec<u8> {
    let msg = json.get("message").and_then(|v| v.as_str()).unwrap_or("");

    json!({
        "type": "echo_response",
        "original": msg
    })
    .to_string()
    .into_bytes()
}

fn handle_test(json: Value) -> Vec<u8> {
    let id = json.get("id").and_then(|v| v.as_u64()).unwrap_or(0);

    json!({
        "type": "test_response",
        "id": id,
        "status": "ok"
    })
    .to_string()
    .into_bytes()
}

fn handle_default(json: Value) -> Vec<u8> {
    json!({
        "type": "default_ack",
        "received": json,
        "status": "ok"
    })
    .to_string()
    .into_bytes()
}
