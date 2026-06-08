#[test]
fn test_del() {
    use rust_demo::del;
    use serde_json::{json, Value};
    use std::ffi::CString;

    let input = json!({
        "num1": 100,
        "num2": 42
    });

    let input_c = CString::new(input.to_string()).unwrap();
    let mut output_buf = vec![0i8; 1024];

    let ret = del(input_c.as_ptr(), output_buf.as_mut_ptr());

    assert_eq!(ret, 0);

    let output_str = unsafe {
        std::ffi::CStr::from_ptr(output_buf.as_ptr())
            .to_str()
            .unwrap()
    };

    let output: Value = serde_json::from_str(output_str).unwrap();
    println!("output: {:?}", output);
    assert_eq!(output["code"], 200);
    assert_eq!(output["data"], 58);
    assert_eq!(output["msg"], "success");
}
