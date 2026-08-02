// demo_socket.ts
import { init_socket, send_message, send_cmd } from "./socket_init.ts";
import { 
  dyn_init, 
  dyn_register, 
  dyn_send_msg, 
  dyn_status, 
  dyn_remove 
} from "./socket_dyn.ts";

declare global {
  var funxsocket: WebSocket | undefined;
  var funxmessage: {
    onmessage?: (data: any) => void;
  };
}

// 测试用例列表
const test_cases = [
  { name: "ping", fn: () => send_message('ping') },
  { name: "status", fn: () => send_message('status') },
  { name: "bind", fn: () => send_cmd("bind", { uuid: "task-001", lib: "libsoket_demo" }) },
  { name: "status_json", fn: () => send_cmd("status") },
  { name: "msg", fn: () => send_cmd("msg", { uuid: "task-001", data: "hello world" }) },
  { name: "remove", fn: () => send_cmd("remove", { uuid: "task-001" }) },
];

// 交互式测试
async function interactive_test() {
  console.log("\n📋 可用命令:");
  console.log("  1. ping           - 测试连接");
  console.log("  2. status         - 简单状态");
  console.log("  3. bind           - 绑定动态库");
  console.log("  4. status_json    - 任务状态");
  console.log("  5. msg <text>     - 发送消息 (例如: msg hello)");
  console.log("  6. remove         - 移除任务");
  console.log("  7. all            - 运行所有测试");
  console.log("  8. help           - 显示帮助");
  console.log("  9. exit           - 退出\n");

  const lines: string[] = [];
  console.log("请输入命令 (输入 help 查看帮助):");

  // 使用 Deno 的 stdin 读取
  for await (const line of Deno.stdin.readable.pipeThrough(new TextDecoderStream())) {
    const input = line.trim();
    if (!input) continue;

    const parts = input.split(/\s+/);
    const cmd = parts[0].toLowerCase();
    const arg = parts.slice(1).join(" ");

    switch (cmd) {
      case "1":
      case "ping":
        send_message('ping');
        break;
      
      case "2":
      case "status":
        send_message('status');
        break;
      
      case "3":
      case "bind":
        dyn_init();
        break;
      
      case "4":
      case "status_json":
        dyn_status();
        break;
      
      case "5":
      case "msg":
        if (arg) {
          dyn_send_msg("task-001", arg);
        } else {
          console.log("⚠️ 请指定消息内容: msg <text>");
        }
        break;
      
      case "6":
      case "remove":
        dyn_remove("task-001");
        break;
      
      case "7":
      case "all":
        console.log("\n🔄 运行所有测试...\n");
        for (const test of test_cases) {
          console.log(`\n▶️ 测试: ${test.name}`);
          test.fn();
          await new Promise(r => setTimeout(r, 500));
        }
        break;
      
      case "8":
      case "help":
        console.log("\n📋 可用命令:");
        console.log("  1/ping          - 测试连接");
        console.log("  2/status        - 简单状态");
        console.log("  3/bind          - 绑定动态库");
        console.log("  4/status_json   - 任务状态");
        console.log("  5/msg <text>    - 发送消息");
        console.log("  6/remove        - 移除任务");
        console.log("  7/all           - 运行所有测试");
        console.log("  8/help          - 显示帮助");
        console.log("  9/exit          - 退出\n");
        break;
      
      case "9":
      case "exit":
      case "quit":
        console.log("👋 退出...");
        Deno.exit(0);
        break;
      
      default:
        console.log(`❌ 未知命令: ${cmd} (输入 help 查看帮助)`);
    }
  }
}

// 主函数
async function main() {
  console.log("========================================");
  console.log("  WebSocket 测试工具");
  console.log("========================================\n");

  // 注册动态库消息处理器
  dyn_register();

  // 初始化 WebSocket
  init_socket((data: any) => {
    // 可以在这里添加额外的消息处理
    // console.log("🔔 自定义处理器:", data);
  });

  // 等待连接建立
  await new Promise(r => setTimeout(r, 1000));

  // 检查连接状态
  if (globalThis.funxsocket) {
    console.log(`✅ 连接状态: ${globalThis.funxsocket.readyState}`);
  }

  // 启动交互式测试
  await interactive_test();
}

// 运行主程序
if (import.meta.main) {
  main().catch(console.error);
}

// 导出供其他模块使用
export { init_socket, send_message, send_cmd };
