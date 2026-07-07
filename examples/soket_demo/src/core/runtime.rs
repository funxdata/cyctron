use serde_json::{Value, json};
use std::sync::Mutex;

#[derive(Debug, Clone)]
pub struct RuntimeState {
    pub data: Value,
    pub status: String,
    pub count: u64,
    pub last_update: u64,
}

impl Default for RuntimeState {
    fn default() -> Self {
        Self {
            data: json!({}),
            status: "idle".to_string(),
            count: 0,
            last_update: 0,
        }
    }
}

lazy_static::lazy_static! {
    static ref STATE: Mutex<RuntimeState> = Mutex::new(RuntimeState::default());
}

fn now() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs()
}

/// 空函数，用于占位
pub fn do_runtime_app() -> &'static str {
    "runtime is running"
}

/// 初始化
pub fn init_runtime(config: Option<Value>) -> Result<String, String> {
    let mut state = STATE.lock().unwrap();
    state.status = "running".to_string();
    state.count = 0;
    state.last_update = now();
    if let Some(cfg) = config {
        state.data = cfg;
    }
    Ok("Runtime initialized".to_string())
}

/// 获取当前状态
pub fn get_state() -> RuntimeState {
    STATE.lock().unwrap().clone()
}

/// 获取实时数据
pub fn get_realtime_data() -> Value {
    let state = STATE.lock().unwrap();
    json!({
        "status": state.status,
        "count": state.count,
        "last_update": state.last_update,
        "data": state.data,
    })
}

/// 更新数据
pub fn update_data(data: Value) -> Result<(), String> {
    let mut state = STATE.lock().unwrap();
    state.data = data;
    state.last_update = now();
    state.count += 1;
    Ok(())
}

/// 合并数据
pub fn merge_data(data: Value) -> Result<(), String> {
    let mut state = STATE.lock().unwrap();
    if let Some(obj) = state.data.as_object_mut() {
        if let Some(new_obj) = data.as_object() {
            for (k, v) in new_obj {
                obj.insert(k.clone(), v.clone());
            }
        }
    }
    state.last_update = now();
    state.count += 1;
    Ok(())
}

/// 暂停
pub fn pause_runtime() -> Result<(), String> {
    let mut state = STATE.lock().unwrap();
    state.status = "paused".to_string();
    Ok(())
}

/// 恢复
pub fn resume_runtime() -> Result<(), String> {
    let mut state = STATE.lock().unwrap();
    state.status = "running".to_string();
    Ok(())
}

/// 关闭
pub fn close_runtime() -> Result<String, String> {
    let mut state = STATE.lock().unwrap();
    state.status = "stopped".to_string();
    state.data = json!({});
    Ok("Runtime closed".to_string())
}

/// 处理数据
pub fn process_data(input: Value) -> Result<Value, String> {
    let mut state = STATE.lock().unwrap();
    state.count += 1;
    state.last_update = now();
    
    if let Some(obj) = state.data.as_object_mut() {
        if let Some(input_obj) = input.as_object() {
            for (k, v) in input_obj {
                obj.insert(k.clone(), v.clone());
            }
        }
    }
    
    Ok(json!({
        "status": state.status,
        "count": state.count,
        "last_update": state.last_update,
        "data": state.data,
    }))
}