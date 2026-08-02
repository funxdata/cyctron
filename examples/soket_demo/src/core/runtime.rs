// src/core/runtime.rs
//! 运行时核心模块

use crate::core::app::process_data;
use crate::utils::sock::NngReceiver;
use serde_json::{json, Value};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
use std::thread;
use std::time::Duration;

// ============================================
//  全局状态
// ============================================

struct RuntimeState {
    running: AtomicBool,
    paused: AtomicBool,
    receiver: Mutex<NngReceiver>,
    task_uuid: Mutex<String>,
}

impl RuntimeState {
    fn new() -> Self {
        Self {
            running: AtomicBool::new(false),
            paused: AtomicBool::new(false),
            receiver: Mutex::new(NngReceiver::new()),
            task_uuid: Mutex::new(String::new()),
        }
    }

    fn is_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }

    fn is_paused(&self) -> bool {
        self.paused.load(Ordering::SeqCst)
    }

    fn set_running(&self, val: bool) {
        self.running.store(val, Ordering::SeqCst);
    }

    fn set_paused(&self, val: bool) {
        self.paused.store(val, Ordering::SeqCst);
    }

    fn receiver(&self) -> &Mutex<NngReceiver> {
        &self.receiver
    }

    fn set_uuid(&self, uuid: String) {
        let mut guard = self.task_uuid.lock().unwrap();
        *guard = uuid;
    }

    fn get_uuid(&self) -> String {
        self.task_uuid.lock().unwrap().clone()
    }
}

// 全局单例
fn state() -> &'static RuntimeState {
    static STATE: OnceLock<RuntimeState> = OnceLock::new();
    STATE.get_or_init(RuntimeState::new)
}

// ============================================
//  公开 API
// ============================================

pub fn init_runtime(config: Option<Value>) -> Result<String, String> {
    let st = state();
    let cfg = config.unwrap_or_default();

    let uuid = cfg
        .get("uuid")
        .and_then(|v| v.as_str())
        .unwrap_or("default")
        .to_string();
    st.set_uuid(uuid.clone());

    // 默认使用 /tmp/cyctron_<uuid>.sock
    let default_path = format!("ipc:///tmp/cyctron_{}.sock", uuid);
    let url = cfg
        .get("nng_url")
        .and_then(|v| v.as_str())
        .unwrap_or(&default_path);

    let mut receiver = st.receiver().lock().unwrap();
    match receiver.start(url) {
        Ok(_) => {
            st.set_running(true);
            st.set_paused(false);

            let uuid_clone = uuid.clone();
            thread::spawn(move || {
                message_loop(uuid_clone);
            });

            Ok(format!("Runtime initialized (uuid: {})", uuid))
        }
        Err(e) => Err(format!("Failed to start NNG: {}", e)),
    }
}

pub fn close_runtime() -> Result<String, String> {
    let st = state();
    st.set_running(false);
    st.set_paused(false);

    let mut receiver = st.receiver().lock().unwrap();
    receiver.stop();
    Ok("Runtime closed".to_string())
}

pub fn pause_runtime() -> Result<(), String> {
    let st = state();
    if !st.is_running() {
        return Err("Runtime not running".to_string());
    }
    st.set_paused(true);
    Ok(())
}

pub fn resume_runtime() -> Result<(), String> {
    let st = state();
    if !st.is_running() {
        return Err("Runtime not running".to_string());
    }
    st.set_paused(false);
    Ok(())
}

pub fn get_status() -> Value {
    let st = state();
    json!({
        "running": st.is_running(),
        "paused": st.is_paused(),
        "task_uuid": st.get_uuid(),
    })
}

// ============================================
//  消息处理循环
// ============================================

fn message_loop(uuid: String) {
    println!("[RUNTIME] Message loop started (uuid: {})", uuid);
    let st = state();

    while st.is_running() {
        if st.is_paused() {
            thread::sleep(Duration::from_millis(100));
            continue;
        }

        let receiver = st.receiver().lock().unwrap();
        if let Some(msg) = receiver.try_recv(100) {
            handle_message(msg, &uuid);
        }
    }

    println!("[RUNTIME] Message loop stopped (uuid: {})", uuid);
}

fn handle_message(msg: String, task_uuid: &str) {
    println!("[RUNTIME] Received: {}", msg);

    let json = match serde_json::from_str::<Value>(&msg) {
        Ok(v) => v,
        Err(_) => {
            let response = process_data(msg.as_bytes());
            if let Ok(text) = String::from_utf8(response) {
                println!("[RUNTIME] Response: {}", text);
            }
            return;
        }
    };

    // 控制命令
    if let Some(cmd) = json.get("cmd").and_then(|v| v.as_str()) {
        match cmd {
            "pause" => {
                let _ = pause_runtime();
                return;
            }
            "resume" => {
                let _ = resume_runtime();
                return;
            }
            "stop" | "close" => {
                let _ = close_runtime();
                return;
            }
            _ => {}
        }
    }

    let response = process_data(json.to_string().as_bytes());
    if let Ok(text) = String::from_utf8(response) {
        if let Ok(mut resp_json) = serde_json::from_str::<Value>(&text) {
            if let Some(obj) = resp_json.as_object_mut() {
                obj.insert("task_uuid".to_string(), json!(task_uuid));
            }
            if let Ok(with_uuid) = serde_json::to_string(&resp_json) {
                println!("[RUNTIME] Response: {}", with_uuid);
                return;
            }
        }
        println!("[RUNTIME] Response: {}", text);
    }
}

