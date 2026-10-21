// choose_dir.ts - 通过 SSE 调用 sys_info.dll 的目录选择对话框
//
// 运行：
//   ./cyctron start -port 44944
//   deno run --allow-net ts/choose_dir.ts 44944 [超时秒数]
//
// 说明：
//   - 请求会阻塞在系统目录选择对话框，直到选择或取消。
//   - 使用 once=1，避免 SSE 周期调用反复弹窗。
//   - 选择完成/取消后检查 /api/status，确认 cyctron 仍可响应。

const port = Deno.args[0] ?? "44944";
const timeoutSec = Number(Deno.args[1] ?? "120");
const base = `http://127.0.0.1:${port}`;
const timeoutMs = Number.isFinite(timeoutSec) && timeoutSec > 0 ? timeoutSec * 1000 : 120000;

let passed = 0;
let failed = 0;

function check(name: string, ok: boolean, detail = "") {
  if (ok) {
    passed++;
    console.log(`  [PASS] ${name}`);
  } else {
    failed++;
    console.log(`  [FAIL] ${name}  ${detail}`);
  }
}

interface SseEvent {
  id?: number;
  event?: string;
  data?: string;
}

interface SseResult {
  status: number;
  contentType: string;
  event?: SseEvent;
}

function parseBlock(block: string): SseEvent {
  const ev: SseEvent = {};
  for (const line of block.split("\n")) {
    if (!line || line.startsWith(":")) continue;
    const idx = line.indexOf(":");
    if (idx < 0) continue;
    const field = line.slice(0, idx);
    let value = line.slice(idx + 1);
    if (value.startsWith(" ")) value = value.slice(1);
    if (field === "id") ev.id = Number(value);
    else if (field === "event") ev.event = value;
    else if (field === "data") ev.data = ev.data === undefined ? value : `${ev.data}\n${value}`;
  }
  return ev;
}

async function readFirst(path: string, ms: number): Promise<SseResult> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), ms);

  try {
    const res = await fetch(base + path, {
      headers: { Accept: "text/event-stream" },
      signal: controller.signal,
    });

    const result: SseResult = {
      status: res.status,
      contentType: res.headers.get("content-type") ?? "",
    };

    if (!res.body || res.status !== 200) return result;

    const reader = res.body.getReader();
    const decoder = new TextDecoder();
    let buf = "";

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buf += decoder.decode(value, { stream: true });

      const sep = buf.indexOf("\n\n");
      if (sep >= 0) {
        result.event = parseBlock(buf.slice(0, sep));
        await reader.cancel().catch(() => {});
        break;
      }
    }

    return result;
  } finally {
    clearTimeout(timer);
  }
}

function payloadOf(ev?: SseEvent): any {
  if (!ev?.data) return null;
  try {
    return JSON.parse(ev.data);
  } catch {
    return null;
  }
}

function dataOf(payload: any): any {
  if (payload && typeof payload === "object" && "data" in payload) return payload.data;
  return payload;
}

async function checkService(): Promise<boolean> {
  // 避免复用 SSE 长连接：用独立 TCP 连接检查服务健康
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  for (let i = 0; i < 5; i++) {
    try {
      const conn = await Deno.connect({ hostname: "127.0.0.1", port: Number(port) });
      await conn.write(encoder.encode(
        "GET /api/status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n",
      ));
      const buf = new Uint8Array(2048);
      const n = await conn.read(buf);
      conn.close();
      if (n && decoder.decode(buf.subarray(0, n)).includes("\"status\":\"running\"")) {
        return true;
      }
    } catch {
      // retry
    }
    await new Promise((r) => setTimeout(r, 300));
  }
  return false;
}

console.log(`== SSE -> sys_info.dll 目录选择 ${base} ==`);
console.log("请在弹出的系统目录选择框中选择目录，或点击取消...");

const url = `/sse?${new URLSearchParams({
  lib: "sys_info",
  symbol: "post_choose_dir",
  once: "1",
  interval_ms: "200",
}).toString()}`;

let result: SseResult;
try {
  result = await readFirst(url, timeoutMs);
} catch (err) {
  failed++;
  console.log(
    `  [FAIL] 等待选择超时（${timeoutMs / 1000}s）: ${err instanceof Error ? err.message : err}`,
  );
  const healthy = await checkService();
  check("超时后服务仍可响应", healthy);
  console.log("================================");
  console.log(`Passed: ${passed}  Failed: ${failed}`);
  Deno.exit(1);
}

check("SSE 响应 200", result.status === 200, `status=${result.status}`);
check(
  "Content-Type text/event-stream",
  result.contentType.startsWith("text/event-stream"),
  result.contentType,
);
check(
  "event=post_choose_dir",
  result.event?.event === "post_choose_dir",
  String(result.event?.event),
);

const payload = payloadOf(result.event);
const data = dataOf(payload);
const cancelled = payload?.msg === "cancelled" || data?.path === null;

if (cancelled) {
  console.log("  已取消选择（流程正常结束）");
  check("取消时 path=null", data?.path === null, JSON.stringify(data));
} else {
  check(
    "选择返回非空 path",
    data !== null && typeof data?.path === "string" && data.path.length > 0,
    JSON.stringify(data),
  );
  if (data?.path) console.log(`  选择目录: ${data.path}`);
}

const healthy = await checkService();
check("调用后服务仍可响应", healthy);

console.log("================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
