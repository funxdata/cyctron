const ws = new WebSocket('ws://localhost:44944/ws');

// 解析所有消息
ws.onmessage = (e) => {
    console.log('Response:', JSON.parse(e.data));
}

ws.send(JSON.stringify({ action: 'init' }));
ws.send(JSON.stringify({ action: 'create', name: 'task1' }));
ws.send(JSON.stringify({ action: 'status' }));

// 启动一个动态库的进程 
let data = {}
ws.send(JSON.stringify({ action: 'init' }));
ws.send(JSON.stringify({ action: 'create', name: 'task1' }));
ws.send(JSON.stringify({ action: 'status' }));

// 消息关闭
ws.onclose = (e) => {
    console.log(e);
}
