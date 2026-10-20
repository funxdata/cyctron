// sse_request.ts - SSE 端点接口验证
// 前置：主目录执行 ./cyctron start（并将 examples/sse_demo 产物放入 ./libary/）
// 运行：deno run --allow-net ts/sse_request.ts [port]
//
// 覆盖：连接/响应头、事件格式、id 递增、data JSON、Last-Event-ID 续传、方法限制。

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
  events: SseEvent[];
}

function parseBlock(block: string): SseEvent | null {
  const ev: SseEvent = {};
  let hasField = false;

  for (const line of block.split("\n")) {
    if (!line || line.startsWith(":")) continue; // 注释/心跳
    const idx = line.indexOf(":");
    if (idx < 0) continue;
    const field = line.slice(0, idx);
    let value = line.slice(idx + 1);
    if (value.startsWith(" ")) value = value.slice(1);

    if (field === "id") {
      ev.id = Number(value);
      hasField = true;
    } else if (field === "event") {
      ev.event = value;
      hasField = true;
    } else if (field === "data") {
      ev.data = ev.data === undefined ? value : `${ev.data}\n${value}`;
      hasField = true;
    }
  }

  return hasField ? ev : null;
}

async function readSse(
  path: string,
  count: number,
  headers: Record<string, string> = {},
  timeoutMs = 8000,
): Promise<SseResult> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const res = await fetch(base + path, {
      headers: { Accept: "text/event-stream", ...headers },
      signal: controller.signal,
    });

    const result: SseResult = {
      status: res.status,
      contentType: res.headers.get("content-type") ?? "",
      events: [],
    };

    if (!res.body || res.status !== 200) return result;

    const reader = res.body.getReader();
    const decoder = new TextDecoder();
    let buf = "";

    while (result.events.length < count) {
      const { done, value } = await reader.read();
      if (done) break;
      buf += decoder.decode(value, { stream: true });

      let sep = buf.indexOf("\n\n");
      while (sep >= 0) {
        const block = buf.slice(0, sep);
        buf = buf.slice(sep + 2);
        const ev = parseBlock(block);
        if (ev) result.events.push(ev);
        if (result.events.length >= count) break;
        sep = buf.indexOf("\n\n");
      }
    }

    await reader.cancel().catch(() => {});
    return result;
  } finally {
    clearTimeout(timer);
  }
}

console.log(`== SSE 接口验证 ${base} ==`);

// 1. 基础连接与事件
const first = await readSse("/sse?interval_ms=50", 2);
check("GET /sse status=200", first.status === 200, `status=${first.status}`);
check(
  "Content-Type text/event-stream",
  first.contentType.startsWith("text/event-stream"),
  first.contentType,
);
check("收到至少 2 条事件", first.events.length >= 2, `got ${first.events.length}`);

if (first.events.length >= 2) {
  const [a, b] = first.events;
  check("事件包含 id", typeof a.id === "number" && typeof b.id === "number", JSON.stringify(a));
  check("id 单调递增", (b.id ?? 0) > (a.id ?? 0), `${a.id} -> ${b.id}`);
  check("event=tick", a.event === "tick", String(a.event));

  const parsed = a.data ? JSON.parse(a.data) : null;
  check(
    "data 是合法 JSON 且含 seq",
    parsed !== null && typeof parsed.seq === "number",
    String(a.data),
  );

  // 2. Last-Event-ID 续传
  const lastId = b.id ?? 0;
  const resumed = await readSse(
    "/sse?interval_ms=50",
    1,
    { "Last-Event-ID": String(lastId) },
  );
  check(
    "Last-Event-ID 续传 id 更大",
    resumed.events.length >= 1 && (resumed.events[0].id ?? 0) > lastId,
    `last=${lastId} first=${resumed.events[0]?.id}`,
  );
}

// 3. 方法限制
const post = await fetch(base + "/sse", { method: "POST" });
check("POST /sse -> 405", post.status === 405, `status=${post.status}`);
await post.text().catch(() => {});

console.log("================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
