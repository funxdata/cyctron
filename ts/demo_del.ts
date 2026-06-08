export const local_del = async (name_prompt: string): Promise<any> => {
  try {
    const response = await fetch("http://127.0.0.1:44944/librust_demo", {
      method: "POST",
      headers: {
        "Content-Type": "application/libary",  // Corrected the content-type
        "FFI-Symbol": "del", 
      },
      body: JSON.stringify({
        "num1": 10,
        "num2": 5,
      }),
    });
    if (!response.ok) {
      throw new Error(`HTTP 错误: ${response.status} - ${response.statusText}`);
    }
    const data = await response.json();
    if (data.code === 200) {
      console.log("info", data.msg);
      return data.data;
    } else {
      throw new Error(`err info: ${data.msg}`);
    }
  } catch (err) {
    console.error("发送失败:", err);
    return err;
  }
};

const res = await local_del("");
console.log(res);
