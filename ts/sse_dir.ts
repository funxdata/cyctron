// sse_dir.ts - 通过 SSE 调用 sys_info.dll 的目录相关 FFI
//
// 运行：
//   ./cyctron start -port 44944
//   deno run --allow-net --allow-read --allow-write ts/sse_dir.ts 44944
//
// 覆盖：
//   - post_cur_dir：读取当前工作目录
//   - post_create_dir：创建目录，并通过 Deno.stat 验证后清理
//
// SSE endpoint:
//   /sse?lib=sys_info&symbol=post_cur_dir&interval_ms=200
//   /sse?lib=sys_info&symbol=post_create_dir&args={"dir_name":"..."}

const port = Deno.args[0] ?? "44944";
const base = `http://127.0.0.1:${port}`;

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
  raw: string;
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

async function readFirst(path: string, timeoutMs = 8000): Promise<SseResult> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const res = await fetch(base + path, {
      headers: { Accept: "text/event-stream" },
      signal: controller.signal,
    });

    const result: SseResult = {
      status: res.status,
      contentType: res.headers.get("content-type") ?? "",
      raw: "",
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
        result.raw = buf.slice(0, sep);
        result.event = parseBlock(result.raw);
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

function sseUrl(lib: string, symbol: string, args?: unknown): string {
  const params = new URLSearchParams({ lib, symbol, interval_ms: "200" });
  if (args !== undefined) params.set("args", JSON.stringify(args));
  return `/sse?${params.toString()}`;
}

console.log(`== SSE -> sys_info.dll 目录接口 ${base} ==`);

// 1. 当前目录
const cur = await readFirst(sseUrl("sys_info", "post_cur_dir"));
check("SSE 响应 200", cur.status === 200, `status=${cur.status}`);
check(
  "Content-Type text/event-stream",
  cur.contentType.startsWith("text/event-stream"),
  cur.contentType,
);
check("event=post_cur_dir", cur.event?.event === "post_cur_dir", String(cur.event?.event));

const curData = dataOf(payloadOf(cur.event));
check(
  "post_cur_dir 返回 path",
  curData !== null && typeof curData.path === "string" && curData.path.length > 0,
  JSON.stringify(curData),
);
if (curData?.path) console.log(`    cwd: ${curData.path}`);

// 2. 创建目录
const dirName = `sse_test_dir_${Date.now()}`;
const made = await readFirst(sseUrl("sys_info", "post_create_dir", { dir_name: dirName }));
check("event=post_create_dir", made.event?.event === "post_create_dir", String(made.event?.event));

const madeData = dataOf(payloadOf(made.event));
check(
  "post_create_dir 返回 path",
  madeData !== null && typeof madeData.path === "string",
  JSON.stringify(madeData),
);

let isDir = false;
try {
  const st = await Deno.stat(dirName);
  isDir = st.isDirectory;
} catch (err) {
  isDir = false;
  console.log(`    stat 失败: ${err instanceof Error ? err.message : err}`);
}
check(`目录已创建: ${dirName}`, isDir);

// 清理
try {
  await Deno.remove(dirName, { recursive: true });
  console.log(`    已清理 ${dirName}`);
} catch (err) {
  console.log(`    清理失败: ${err instanceof Error ? err.message : err}`);
}

console.log("================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
