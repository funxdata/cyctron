// libary_request.ts - libary 动态库接口验证（可重复运行）
// 前置：主目录执行 ./cyctron start（并把 examples/libary_demo 产物放到 ./libary/）
// 运行：deno run --allow-net libary_request.ts [port]
//
// 覆盖：GET 无参/query 带参/不存在、POST 空/正常、PATCH、UPDATE、PUT、
//       DELETE（body 与 query）、FFI-Symbol 负例、未知插件。
// 脚本自行创建测试数据并以返回的 id 驱动后续断言，不依赖服务初始状态。

const port = Deno.args[0] ?? "44944";
const base = `http://127.0.0.1:${port}`;
const LIB = "/libary_demo";

let passed = 0;
let failed = 0;

interface Reply {
  status: number;
  text: string;
  data: any;
}

async function request(
  method: string,
  path: string,
  body?: string,
  headers?: Record<string, string>,
): Promise<Reply> {
  const res = await fetch(base + path, { method, headers, body });
  const text = await res.text();
  let data: any = null;
  try {
    data = JSON.parse(text);
  } catch {
    // 非 JSON 响应（静态 404 文本等）由调用方按 text 断言
  }
  return { status: res.status, text, data };
}

function check(
  name: string,
  ok: boolean,
  detail: string,
) {
  if (ok) {
    passed++;
    console.log(`  [PASS] ${name}`);
  } else {
    failed++;
    console.log(`  [FAIL] ${name}  ${detail}`);
  }
}

async function expectJson(name: string, r: Reply, pred: (d: any) => boolean) {
  check(
    name,
    r.status === 200 && r.data !== null && pred(r.data),
    `status=${r.status} body=${r.text.slice(0, 160)}`,
  );
}

async function expectStatus(name: string, r: Reply, status: number, text?: string) {
  check(
    name,
    r.status === status && (text === undefined || r.text.toLowerCase().includes(text.toLowerCase())),
    `status=${r.status} (expect ${status}) body=${r.text.slice(0, 160)}`,
  );
}

console.log(`== libary 接口验证 ${base} ==`);

// ---------- GET 无参数 ----------
await expectJson("GET 无参数 → 列表", await request("GET", LIB), (d) =>
  d.ok === true && d.method === "GET" && Array.isArray(d.data?.items) &&
  d.data.count === d.data.items.length);

// ---------- 创建两条测试数据 ----------
const a = await request("POST", LIB, JSON.stringify({ name: "alpha-test", type: "img" }));
const b = await request("POST", LIB, JSON.stringify({ name: "beta-test", type: "js" }));
await expectJson("POST 带 body → 创建 A", a, (d) =>
  d.ok === true && d.method === "POST" && typeof d.id === "number" &&
  d.created?.name === "alpha-test" && d.created?.id === d.id);
await expectJson("POST 带 body → 创建 B", b, (d) =>
  d.ok === true && typeof d.id === "number" && d.created?.name === "beta-test");

const idA: number = a.data?.id;
const idB: number = b.data?.id;

// ---------- GET 带参 ----------
await expectJson(`GET ?id=${idA} → alpha-test`, await request("GET", `${LIB}?id=${idA}`), (d) =>
  d.ok === true && d.data?.id === idA && d.data?.name === "alpha-test");
await expectJson(`GET ?id=${idB} → beta-test`, await request("GET", `${LIB}?id=${idB}`), (d) =>
  d.ok === true && d.data?.id === idB && d.data?.name === "beta-test");
await expectJson("GET ?id=999999 → not_found", await request("GET", `${LIB}?id=999999`), (d) =>
  d.ok === false && d.error === "not_found");

// ---------- POST 边界 ----------
await expectJson("POST 无 body → empty_body", await request("POST", LIB), (d) =>
  d.ok === false && d.method === "POST" && d.error === "empty_body");

// ---------- PATCH ----------
await expectJson("PATCH 无 body → missing_id", await request("PATCH", LIB), (d) =>
  d.ok === false && d.method === "PATCH" && d.error === "missing_id");
await expectJson(`PATCH ?id=${idA} 部分更新`, await request(
  "PATCH",
  LIB,
  JSON.stringify({ id: idA, patch: { name: "renamed" } }),
), (d) =>
  d.ok === true && d.method === "PATCH" && d.patched?.name === "renamed" &&
  d.patched?.type === "img");
await expectJson("PATCH 不存在 id → not_found", await request(
  "PATCH",
  LIB,
  JSON.stringify({ id: 999999, patch: { x: 1 } }),
), (d) => d.ok === false && d.error === "not_found");

// ---------- UPDATE / PUT ----------
await expectJson("UPDATE 无 body → empty_body", await request("UPDATE", LIB), (d) =>
  d.ok === false && d.method === "UPDATE" && d.error === "empty_body");
await expectJson(`UPDATE ?id=${idB} 全量替换`, await request(
  "UPDATE",
  LIB,
  JSON.stringify({ id: idB, name: "replaced" }),
), (d) =>
  d.ok === true && d.method === "UPDATE" && d.updated?.id === idB &&
  d.updated?.name === "replaced");
await expectJson(`PUT ?id=${idA} 与 UPDATE 同语义`, await request(
  "PUT",
  LIB,
  JSON.stringify({ id: idA, name: "putted" }),
), (d) =>
  d.ok === true && d.method === "PUT" && d.updated?.id === idA &&
  d.updated?.name === "putted");

// ---------- DELETE ----------
await expectJson(`DELETE query ?id=${idA}`, await request("DELETE", `${LIB}?id=${idA}`), (d) =>
  d.ok === true && d.method === "DELETE" && d.deleted?.id === idA);
await expectJson(`DELETE body {id:${idB}}`, await request(
  "DELETE",
  LIB,
  JSON.stringify({ id: idB }),
), (d) => d.ok === true && d.deleted?.id === idB);
await expectJson("DELETE 不存在 → not_found", await request("DELETE", `${LIB}?id=999999`), (d) =>
  d.ok === false && d.error === "not_found");

// ---------- 负例 ----------
await expectStatus("FFI-Symbol 负例 → 404", await request(
  "POST",
  LIB,
  JSON.stringify({ name: "x" }),
  { "FFI-Symbol": "no_such_symbol" },
), 404, "Plugin Not Found");
await expectStatus("未知插件 → 404", await request("GET", "/no_such_lib"), 404, "Not found");

console.log("=================================");
console.log(`Passed: ${passed}  Failed: ${failed}`);
if (failed > 0) Deno.exit(1);
