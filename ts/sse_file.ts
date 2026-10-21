// sse_file.ts - 通过 SSE 调用 sys_info.dll 的文件相关 FFI
//
// 运行：
//   ./cyctron start -port 44944
//   deno run --allow-net ts/sse_file.ts 44944
//
// 覆盖：
//   - post_file_exists：存在/不存在的文件
//   - post_file_info：文件类型、大小、时间字段
//
// SSE endpoint:
//   /sse?lib=sys_info&symbol=post_file_exists&args={"path":"README.md"}

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

console.log(`== SSE -> sys_info.dll 文件接口 ${base} ==`);

// 1. 存在的文件
const exists = await readFirst(sseUrl("sys_info", "post_file_exists", { path: "README.md" }));
check("SSE 响应 200", exists.status === 200, `status=${exists.status}`);
check(
  "Content-Type text/event-stream",
  exists.contentType.startsWith("text/event-stream"),
  exists.contentType,
);
check("event=post_file_exists", exists.event?.event === "post_file_exists", String(exists.event?.event));

const existsData = dataOf(payloadOf(exists.event));
check(
  "README.md exists=true",
  existsData !== null && existsData.exists === true,
  JSON.stringify(existsData),
);

// 2. 文件信息
const info = await readFirst(sseUrl("sys_info", "post_file_info", { path: "README.md" }));
check("event=post_file_info", info.event?.event === "post_file_info", String(info.event?.event));

const infoData = dataOf(payloadOf(info.event));
check(
  "README.md is_file=true",
  infoData !== null && infoData.is_file === true,
  JSON.stringify(infoData),
);
check(
  "README.md size > 0",
  infoData !== null && typeof infoData.size === "number" && infoData.size > 0,
  JSON.stringify(infoData),
);
if (infoData) {
  console.log(`    size=${infoData.size} modified=${infoData.modified} created=${infoData.created}`);
}

// 3. 不存在的文件
const missing = await readFirst(
  sseUrl("sys_info", "post_file_exists", { path: "no_such_file_xyz_123.txt" }),
);
const missingData = dataOf(payloadOf(missing.event));
check(
  "missing file exists=false",
  missingData !== null && missingData.exists === false,
  JSON.stringify(missingData),
);

console.log("================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
