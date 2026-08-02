use serde_json::json;
use std::ffi::{c_char, c_int};

use crate::utils::cjson::{parse_json, write_error, write_success};

// ============ 输入数据结构 ============

#[derive(Debug, serde::Deserialize)]
pub struct AddInput {
    pub num1: u64,
    pub num2: u64,
}

#[derive(Debug, serde::Deserialize)]
pub struct SubInput {
    pub num1: i64,
    pub num2: i64,
}

#[derive(Debug, serde::Deserialize)]
pub struct MulInput {
    pub num1: f64,
    pub num2: f64,
}

#[derive(Debug, serde::Deserialize)]
pub struct DivInput {
    pub num1: f64,
    pub num2: f64,
}

#[derive(Debug, serde::Deserialize)]
pub struct UserInput {
    pub name: String,
    pub age: u8,
    pub email: Option<String>,
}

#[derive(Debug, serde::Deserialize)]
pub struct EchoInput {
    pub message: String,
}

#[derive(Debug, serde::Deserialize)]
pub struct SleepInput {
    pub ms: u64,
}

#[derive(Debug, serde::Deserialize)]
pub struct SumInput {
    pub numbers: Vec<u64>,
}

#[derive(Debug, serde::Deserialize)]
pub struct BatchInput {
    pub ops: Vec<BatchOp>,
}

#[derive(Debug, serde::Deserialize)]
#[serde(tag = "type")]
pub enum BatchOp {
    #[serde(rename = "add")]
    Add { num1: u64, num2: u64 },
    #[serde(rename = "sub")]
    Sub { num1: i64, num2: i64 },
    #[serde(rename = "mul")]
    Mul { num1: f64, num2: f64 },
    #[serde(rename = "div")]
    Div { num1: f64, num2: f64 },
}

// ============ FFI 导出函数 ============

/// 加法: num1 + num2
#[unsafe(no_mangle)]
pub extern "C" fn post_add(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: AddInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let result = req.num1 + req.num2;
    write_success(json_out, "success", json!(result))
}

/// 减法: num1 - num2
#[unsafe(no_mangle)]
pub extern "C" fn post_sub(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: SubInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let result = req.num1 - req.num2;
    write_success(json_out, "success", json!(result))
}

/// 乘法: num1 * num2
#[unsafe(no_mangle)]
pub extern "C" fn post_mul(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: MulInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let result = req.num1 * req.num2;
    write_success(json_out, "success", json!(result))
}

/// 除法: num1 / num2
#[unsafe(no_mangle)]
pub extern "C" fn post_div(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: DivInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    if req.num2 == 0.0 {
        return write_error(json_out, 400103, "division by zero");
    }

    let result = req.num1 / req.num2;
    write_success(json_out, "success", json!(result))
}

/// 处理用户信息
#[unsafe(no_mangle)]
pub extern "C" fn post_user(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: UserInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let response = json!({
        "greeting": format!("Hello, {}!", req.name),
        "age": req.age,
        "email": req.email.unwrap_or_else(|| "no email provided".to_string()),
        "status": if req.age >= 18 { "adult" } else { "minor" }
    });

    write_success(json_out, "user processed", response)
}

/// 回显消息
#[unsafe(no_mangle)]
pub extern "C" fn post_echo(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: EchoInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let response = json!({
        "echo": req.message,
        "length": req.message.len(),
        "timestamp": std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs()
    });

    write_success(json_out, "echo", response)
}

/// 健康检查
#[unsafe(no_mangle)]
pub extern "C" fn post_health(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let _ = json_in; // 忽略输入

    let response = json!({
        "status": "ok",
        "service": "rust_demo",
        "version": env!("CARGO_PKG_VERSION")
    });

    write_success(json_out, "healthy", response)
}

/// 批量操作
#[unsafe(no_mangle)]
pub extern "C" fn post_batch(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: BatchInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let mut results = Vec::new();

    for (i, op) in req.ops.iter().enumerate() {
        let result = match op {
            BatchOp::Add { num1, num2 } => {
                json!({
                    "index": i,
                    "type": "add",
                    "result": num1 + num2
                })
            }
            BatchOp::Sub { num1, num2 } => {
                json!({
                    "index": i,
                    "type": "sub",
                    "result": num1 - num2
                })
            }
            BatchOp::Mul { num1, num2 } => {
                json!({
                    "index": i,
                    "type": "mul",
                    "result": num1 * num2
                })
            }
            BatchOp::Div { num1, num2 } => {
                if *num2 == 0.0 {
                    json!({
                        "index": i,
                        "type": "div",
                        "error": "division by zero"
                    })
                } else {
                    json!({
                        "index": i,
                        "type": "div",
                        "result": num1 / num2
                    })
                }
            }
        };
        results.push(result);
    }

    let response = json!({
        "total": req.ops.len(),
        "results": results
    });

    write_success(json_out, "batch processed", response)
}

/// 求和 (数组)
#[unsafe(no_mangle)]
pub extern "C" fn post_sum(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: SumInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let sum: u64 = req.numbers.iter().sum();
    let count = req.numbers.len();
    let avg = if count > 0 {
        sum as f64 / count as f64
    } else {
        0.0
    };

    let response = json!({
        "sum": sum,
        "count": count,
        "average": avg,
        "numbers": req.numbers
    });

    write_success(json_out, "sum computed", response)
}

/// 延迟测试 (模拟耗时操作)
#[unsafe(no_mangle)]
pub extern "C" fn post_sleep(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let req: SleepInput = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    // 模拟耗时操作
    std::thread::sleep(std::time::Duration::from_millis(req.ms));

    let response = json!({
        "slept": req.ms,
        "message": format!("slept for {} ms", req.ms)
    });

    write_success(json_out, "sleep completed", response)
}

/// 数据库操作示例
#[unsafe(no_mangle)]
pub extern "C" fn post_db_create(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let body: serde_json::Value = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let path = body.get("path").and_then(|v| v.as_str()).unwrap_or("");
    let table = body.get("table").and_then(|v| v.as_str()).unwrap_or("");
    let schema = body.get("schema").and_then(|v| v.as_str()).unwrap_or("");

    let response = json!({
        "action": "create_table",
        "path": path,
        "table": table,
        "schema": schema,
        "status": "created"
    });

    write_success(json_out, "table created", response)
}

#[unsafe(no_mangle)]
pub extern "C" fn post_db_insert(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let body: serde_json::Value = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let path = body.get("path").and_then(|v| v.as_str()).unwrap_or("");
    let table = body.get("table").and_then(|v| v.as_str()).unwrap_or("");
    let data = body.get("data").unwrap_or(&serde_json::Value::Null);

    let response = json!({
        "action": "insert",
        "path": path,
        "table": table,
        "data": data,
        "status": "inserted"
    });

    write_success(json_out, "data inserted", response)
}

#[unsafe(no_mangle)]
pub extern "C" fn post_db_query(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let body: serde_json::Value = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let path = body.get("path").and_then(|v| v.as_str()).unwrap_or("");
    let sql = body.get("sql").and_then(|v| v.as_str()).unwrap_or("");

    // 模拟查询结果
    let mock_data = json!([
        {"id": 1, "name": "Alice", "age": 25},
        {"id": 2, "name": "Bob", "age": 30},
        {"id": 3, "name": "Charlie", "age": 35}
    ]);

    let response = json!({
        "action": "query",
        "path": path,
        "sql": sql,
        "result": mock_data,
        "count": 3
    });

    write_success(json_out, "query completed", response)
}

#[unsafe(no_mangle)]
pub extern "C" fn post_db_update(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let body: serde_json::Value = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let path = body.get("path").and_then(|v| v.as_str()).unwrap_or("");
    let table = body.get("table").and_then(|v| v.as_str()).unwrap_or("");
    let condition = body.get("condition").unwrap_or(&serde_json::Value::Null);
    let data = body.get("data").unwrap_or(&serde_json::Value::Null);

    let response = json!({
        "action": "update",
        "path": path,
        "table": table,
        "condition": condition,
        "data": data,
        "status": "updated",
        "affected_rows": 1
    });

    write_success(json_out, "data updated", response)
}

#[unsafe(no_mangle)]
pub extern "C" fn post_db_delete(json_in: *const c_char, json_out: *mut c_char) -> c_int {
    let body: serde_json::Value = match parse_json(json_in) {
        Ok(v) => v,
        Err(e) => return write_error(json_out, 400100, &e),
    };

    let path = body.get("path").and_then(|v| v.as_str()).unwrap_or("");
    let table = body.get("table").and_then(|v| v.as_str()).unwrap_or("");
    let condition = body.get("condition").unwrap_or(&serde_json::Value::Null);

    let response = json!({
        "action": "delete",
        "path": path,
        "table": table,
        "condition": condition,
        "status": "deleted",
        "affected_rows": 1
    });

    write_success(json_out, "data deleted", response)
}
