// static_request.ts - 静态文件服务请求验证（精确内容一致性）
// 前置：在项目主目录执行 ./cyctron start
// 运行：deno run --allow-net static_request.ts [port]
//
// 验证目标：每个静态资源的响应体与 web/ 下磁盘文件逐字节一致。

const port = Deno.args[0] ?? "44944";
const base = `http://127.0.0.1:${port}`;

let passed = 0;
let failed = 0;

function fileBody(lines: string[]): string {
  return lines.join("\n") + "\n";
}

// 与 web/ 下文件逐字节对应的期望内容
const INDEX_HTML = fileBody([
  "<!DOCTYPE html>",
  '<html lang="zh-CN">',
  "<head>",
  '  <meta charset="UTF-8">',
  "  <title>cyctron</title>",
  '  <link rel="icon" href="/assets/img/logo.svg" type="image/svg+xml">',
  "</head>",
  "<body>",
  "  <h1>cyctron</h1>",
  '  <img src="/assets/img/logo.svg" alt="logo" width="120">',
  '  <img src="/assets/img/banner.svg" alt="banner" width="240">',
  '  <p id="app-status">loading...</p>',
  '  <p id="app-config">config: loading...</p>',
  '  <script src="/assets/js/app.js"></script>',
  '  <script src="/assets/js/util.js"></script>',
  "</body>",
  "</html>",
]);

const APP_JS = fileBody([
  "// app.js - 固定内容，用于静态服务一致性验证",
  "(() => {",
  '  const el = document.getElementById("app-status");',
  "  if (!el) return;",
  '  fetch("/assets/json/data.json")',
  "    .then((r) => r.json())",
  "    .then((d) => {",
  "      el.textContent = `${d.name} ${d.version}: ${d.message}`;",
  "    });",
  "})();",
]);

const UTIL_JS = fileBody([
  "// util.js - 固定内容，用于静态服务一致性验证",
  "(() => {",
  '  const el = document.getElementById("app-config");',
  "  if (!el) return;",
  '  fetch("/assets/json/config.json")',
  "    .then((r) => r.json())",
  "    .then((c) => {",
  "      el.textContent = `config: ${c.env}/${c.region}`;",
  "    });",
  "})();",
]);

const DATA_JSON = fileBody([
  "{",
  '  "name": "cyctron",',
  '  "version": "1.0.1",',
  '  "message": "static assets served"',
  "}",
]);

const CONFIG_JSON = fileBody([
  "{",
  '  "env": "local",',
  '  "region": "cn-north",',
  '  "debug": false',
  "}",
]);

const LOGO_SVG = fileBody([
  '<svg xmlns="http://www.w3.org/2000/svg" width="120" height="120" viewBox="0 0 120 120">',
  '  <circle cx="60" cy="60" r="54" fill="#1e6f5c"/>',
  '  <text x="60" y="70" font-family="monospace" font-size="34" fill="#ffffff" text-anchor="middle">cy</text>',
  "</svg>",
]);

const BANNER_SVG = fileBody([
  '<svg xmlns="http://www.w3.org/2000/svg" width="240" height="60" viewBox="0 0 240 60">',
  '  <rect width="240" height="60" rx="8" fill="#1e6f5c"/>',
  '  <text x="120" y="38" font-family="monospace" font-size="20" fill="#ffffff" text-anchor="middle">cyctron static</text>',
  "</svg>",
]);

interface ExactCase {
  name: string;
  path: string;
  type: string;
  body: string;
}

const EXACT_CASES: ExactCase[] = [
  { name: "index: /", path: "/", type: "text/html", body: INDEX_HTML },
  { name: "index: /index.html", path: "/index.html", type: "text/html", body: INDEX_HTML },
  { name: "js: app.js", path: "/assets/js/app.js", type: "text/javascript", body: APP_JS },
  { name: "js: util.js", path: "/assets/js/util.js", type: "text/javascript", body: UTIL_JS },
  { name: "json: data.json", path: "/assets/json/data.json", type: "application/json", body: DATA_JSON },
  { name: "json: config.json", path: "/assets/json/config.json", type: "application/json", body: CONFIG_JSON },
  { name: "img: logo.svg", path: "/assets/img/logo.svg", type: "image/svg+xml", body: LOGO_SVG },
  { name: "img: banner.svg", path: "/assets/img/banner.svg", type: "image/svg+xml", body: BANNER_SVG },
];

async function checkExact(c: ExactCase) {
  try {
    const res = await fetch(base + c.path);
    const body = await res.text();
    const typeOk = res.headers.get("content-type")?.startsWith(c.type);
    const statusOk = res.status === 200;
    const bodyOk = body === c.body;

    if (statusOk && typeOk && bodyOk) {
      passed++;
      console.log(`  [PASS] ${c.name} (status/type/body exact)`);
    } else {
      failed++;
      console.log(
        `  [FAIL] ${c.name}  status=${res.status} type=${res.headers.get("content-type")} bodyExact=${bodyOk}`,
      );
      if (!bodyOk) {
        console.log(`    expected ${c.body.length} bytes, got ${body.length} bytes`);
      }
    }
  } catch (err) {
    failed++;
    console.log(`  [FAIL] ${c.name}  ${err.message}`);
  }
}

async function checkStatus(
  name: string,
  path: string,
  expect: { status: number; type?: string; bodyIncludes?: string },
) {
  try {
    const res = await fetch(base + path);
    const body = await res.text();
    const ok = res.status === expect.status &&
      (expect.type === undefined || res.headers.get("content-type")?.startsWith(expect.type)) &&
      (expect.bodyIncludes === undefined || body.includes(expect.bodyIncludes));

    if (ok) {
      passed++;
      console.log(`  [PASS] ${name}`);
    } else {
      failed++;
      console.log(`  [FAIL] ${name}  status=${res.status} (expect ${expect.status})`);
    }
  } catch (err) {
    failed++;
    console.log(`  [FAIL] ${name}  ${err.message}`);
  }
}

console.log(`== 静态文件一致性验证 ${base} ==`);
console.log("== 精确内容比对 ==");

for (const c of EXACT_CASES) {
  await checkExact(c);
}

console.log("== 状态码/边界 ==");

await checkStatus("404: missing json", "/assets/json/nope.json", { status: 404 });
await checkStatus("404: missing dir index", "/assets/nothere/", { status: 404 });
await checkStatus("api: /api/status", "/api/status", {
  status: 200,
  type: "application/json",
  bodyIncludes: "running",
});

// Deno fetch 会规范化 /../，用原始 TCP 发送未规范化路径验证服务端拒绝
{
  const conn = await Deno.connect({ hostname: "127.0.0.1", port: Number(port) });
  const req = "GET /../etc/passwd HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
  await conn.write(new TextEncoder().encode(req));
  const buf = new Uint8Array(256);
  const n = await conn.read(buf);
  const head = new TextDecoder().decode(buf.subarray(0, n ?? 0));
  conn.close();
  if (head.startsWith("HTTP/1.1 400")) {
    passed++;
    console.log("  [PASS] 400: traversal (raw socket)");
  } else {
    failed++;
    console.log(`  [FAIL] 400: traversal (raw socket)  got: ${head.split("\r\n")[0]}`);
  }
}

console.log("=================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
