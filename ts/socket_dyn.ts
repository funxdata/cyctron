// socket_dyn.ts
declare global {
  var funxsocket: WebSocket | undefined;
  var funxmessage: {
    onmessage?: (data: any) => void;
  };
}

// 动态库测试函数
export const dyn_init = () => {
  console.log("📦 [dyn_init] 发送 bind 命令...");
  
  // 发送 bind 命令
  globalThis.funxsocket?.send(JSON.stringify({
    cmd: "bind",
    uuid: "task-001",
    lib: "libsoket_demo"
  }));
};

// 注册绑定信息
const dyn_message_bind = (data: any) => {
  console.log("🔔 [dyn_message_bind] 收到 bind 响应:", data);
  
  if (data.data?.status === "success" || data.status === "success") {
    console.log("✅ 动态库绑定成功！");
  } else {
    console.log("❌ 动态库绑定失败");
  }
};

// 测试消息发送
export const dyn_send_msg = (uuid: string, msg: string) => {
  console.log(`📤 [dyn_send_msg] 发送消息到 ${uuid}...`);
  globalThis.funxsocket?.send(JSON.stringify({
    cmd: "msg",
    uuid: uuid,
    data: msg
  }));
};

// 测试状态查询
export const dyn_status = () => {
  console.log("📊 [dyn_status] 查询任务状态...");
  globalThis.funxsocket?.send(JSON.stringify({
    cmd: "status"
  }));
};

// 测试移除任务
export const dyn_remove = (uuid: string) => {
  console.log(`🗑️ [dyn_remove] 移除任务 ${uuid}...`);
  globalThis.funxsocket?.send(JSON.stringify({
    cmd: "remove",
    uuid: uuid
  }));
};

// 注册消息处理器
export const dyn_register = () => {
  globalThis.funxmessage = {
    onmessage: (data: any) => {
      console.log("🔔 [dyn_register] 收到消息:", data);
      
      // 根据消息类型处理
      if (data.type === "realtime" || data.type === "realtime") {
        const action = data.data?.action || data.action;
        if (action === "bind") {
          dyn_message_bind(data);
        } else if (action === "remove") {
          console.log(`🗑️ 任务移除: ${data.data?.uuid || data.uuid}`);
        } else if (action === "status") {
          console.log(`📊 任务状态:`, data.data || data);
        }
      } else if (data.type === "msg") {
        console.log(`💬 收到消息: ${data.data || data}`);
      } else if (data.type === "system") {
        console.log(`🖥️ 系统: ${data.data?.status || data.status}`);
      }
    }
  };
};
