// socket_init.ts
declare global {
  var funxsocket: WebSocket | undefined;
  var funxmessage: {
    onmessage?: (data: any) => void;
    onerror?: (error: any) => void;
  };
}

// 创建 WebSocket 连接并设置为全局变量
export const init_socket = (onMessage?: (data: any) => void) => {
  const socket = new WebSocket("ws://localhost:44944/soket");

  socket.onopen = () => {
    console.log("✅ WebSocket 连接成功！");
    globalThis.funxsocket = socket;
    
    // 初始化消息处理器
    globalThis.funxmessage = {
      onmessage: onMessage
    };
  };

  socket.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data);
      console.log("📥 收到响应:", JSON.stringify(data, null, 2));
      
      // 调用自定义消息处理器
      if (globalThis.funxmessage?.onmessage) {
        globalThis.funxmessage.onmessage(data);
      }
    } catch (error) {
      console.log("📥 收到原始数据:", e.data);
    }
  };

  socket.onclose = (e) => {
    console.log("🔌 连接关闭:", e.code, e.reason);
    delete globalThis.funxsocket;
  };

  socket.onerror = (e) => {
    console.error("❌ WebSocket 错误:", e);
  };
};

// 发送消息
export const send_message = (data: any) => {
  if (globalThis.funxsocket && globalThis.funxsocket.readyState === WebSocket.OPEN) {
    const msg = typeof data === 'string' ? data : JSON.stringify(data);
    globalThis.funxsocket.send(msg);
    console.log("📤 发送:", msg);
    return true;
  } else {
    console.warn("⚠️ WebSocket 未连接");
    return false;
  }
};

// 发送 JSON 命令
export const send_cmd = (cmd: string, params?: Record<string, any>) => {
  const msg = { cmd, ...params };
  return send_message(msg);
};

