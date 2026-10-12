import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { mc } from "../src/index.js";
import { CompilerEngine } from "../src/compiler/engine.js";
import { buildSourcePackage } from "../src/compiler/package.js";
import {
  DEFAULT_FUEL,
  DEFAULT_MEM,
  DEFAULT_TABLE,
  encodeGuestRequest,
  handleGuestRequest,
  STATUS,
} from "../src/compiler/request.js";
import { customSectionPayloads } from "../src/compiler/wasm.js";

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();
const zeroDigest = new Uint8Array(32);

async function checkAllocationCleanup(failureAt: number): Promise<void> {
  const allocations = new Map<number, number>();
  const memory = new WebAssembly.Memory({ initial: 1 });
  let allocationCount = 0;
  let nextPointer = 64;
  let contexts = 0;
  const originalInstance = WebAssembly.Instance;
  WebAssembly.Instance = new Proxy(originalInstance, {
    construct() {
      return {
        exports: {
          memory,
          luauc_v1_alloc(size: number) {
            allocationCount += 1;
            if (allocationCount === failureAt) return 0;
            const pointer = nextPointer;
            nextPointer += size;
            allocations.set(pointer, size);
            return pointer;
          },
          luauc_v1_dealloc(pointer: number, size: number) {
            assert.equal(allocations.get(pointer), size);
            allocations.delete(pointer);
          },
          luauc_v1_context_create(_profile: number, _profileSize: number, _pack: number, _packSize: number, result: number) {
            contexts += 1;
            new DataView(memory.buffer).setUint32(result, 1, true);
            return 0;
          },
          luauc_v1_context_destroy() { contexts -= 1; },
          luauc_v1_describe(pointer: number) {
            new DataView(memory.buffer).setUint32(pointer + 12, 320, true);
            return 0;
          },
        },
      };
    },
  });
  try {
    const emptyWasm = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
    const loading = CompilerEngine.load({ engine: emptyWasm, pack: emptyWasm, profile: new Uint8Array() });
    if (failureAt <= 4) {
      await assert.rejects(loading, /allocation failed/);
    } else {
      const engine = await loading;
      try {
        await assert.rejects(engine.compile(new Uint8Array(240)), /allocation failed/);
      } finally {
        await engine.close();
      }
    }
    assert.equal(allocations.size, 0, `allocation leak after failure ${failureAt}`);
    assert.equal(contexts, 0, `context leak after failure ${failureAt}`);
  } finally {
    WebAssembly.Instance = originalInstance;
  }
}

for (const failureAt of [1, 2, 3, 4, 5, 6]) await checkAllocationCleanup(failureAt);

function runfile(rel: string | undefined, envVar: string): string {
  if (!rel) throw new Error(`${envVar} is not set`);
  const root = process.env.RUNFILES_DIR;
  if (!root) throw new Error("RUNFILES_DIR is not set");
  return join(root, rel);
}

function bytes(envVar: string): Uint8Array {
  return new Uint8Array(readFileSync(runfile(process.env[envVar], envVar)));
}

function frame(body: Uint8Array): { status: number; diagnostic: string; artifact: Uint8Array } {
  const view = new DataView(body.buffer, body.byteOffset, body.byteLength);
  const status = view.getUint32(0, true);
  const diagnosticLength = view.getUint32(4, true);
  const diagnostic = textDecoder.decode(body.subarray(8, 8 + diagnosticLength));
  const artifactLength = view.getUint32(8 + diagnosticLength, true);
  const artifact = body.subarray(12 + diagnosticLength, 12 + diagnosticLength + artifactLength);
  return { status, diagnostic, artifact };
}

async function reject(body: Uint8Array, callerMask = 0xff): Promise<{ status: number; diagnostic: string; calls: number }> {
  let calls = 0;
  const response = await handleGuestRequest({
    body,
    profileDigest: zeroDigest,
    packDigest: zeroDigest,
    callerMask,
    compile: async () => {
      calls += 1;
      return { status: 0, diagnostic: new Uint8Array(), guest: new Uint8Array() };
    },
  });
  const parsed = frame(response);
  return { status: parsed.status, diagnostic: parsed.diagnostic, calls };
}

function shortInstallHeader(): Uint8Array {
  const body = new Uint8Array(32);
  body.set([0x41, 0x4f, 0x4c, 0x43, 0x31, 0, 0, 0]);
  const view = new DataView(body.buffer);
  view.setUint16(8, 1, true);
  view.setUint16(10, 56, true);
  view.setUint32(12, 16 * 1024 * 1024 + 1, true);
  view.setUint32(16, 2, true);
  view.setUint32(20, 1, true);
  return body;
}

function budget(payload: Uint8Array): { version: number; mem: bigint; fuel: bigint; table: number } {
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    version: view.getUint32(0, true),
    mem: view.getBigUint64(4, true),
    fuel: view.getBigUint64(12, true),
    table: view.getUint32(20, true),
  };
}

const badMask = await reject(
  encodeGuestRequest({
    entry: "main",
    modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
    budgetMask: 8,
  }),
);
assert.equal(badMask.status, STATUS.invalidHeader);
assert.equal(badMask.diagnostic, "InvalidHeader");
assert.equal(badMask.calls, 0);

const entry = await reject(
  encodeGuestRequest({
    entry: "nope",
    modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
  }),
);
assert.equal(entry.status, STATUS.invalidEntry);
assert.equal(entry.diagnostic, "InvalidEntry");
assert.equal(entry.calls, 0);

const empty = await reject(
  encodeGuestRequest({
    entry: "main",
    modules: [{ name: "main", source: new Uint8Array() }],
  }),
);
assert.equal(empty.status, STATUS.invalidRequest);
assert.equal(empty.diagnostic, "NonCanonicalLayout");
assert.equal(empty.calls, 0);

const tooLarge = await reject(
  encodeGuestRequest({
    entry: "main",
    modules: [{ name: "main", source: new Uint8Array(16 * 1024 * 1024 - 100) }],
  }),
);
assert.equal(tooLarge.status, STATUS.requestTooLarge);
assert.equal(tooLarge.diagnostic, "RequestTooLarge");
assert.equal(tooLarge.calls, 0);

const hugeField = await reject(
  encodeGuestRequest({
    entry: "main",
    modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
    totalSize: 16 * 1024 * 1024 + 1,
  }),
);
assert.equal(hugeField.status, STATUS.requestTooLarge);
assert.equal(hugeField.calls, 0);

const shortInstall = await reject(shortInstallHeader());
assert.equal(shortInstall.status, STATUS.truncated);
assert.equal(shortInstall.diagnostic, "Truncated");
assert.equal(shortInstall.calls, 0);

const aboveCaller = await reject(
  encodeGuestRequest({
    tier: 1,
    budgetMask: 7,
    mem: 1n,
    fuel: 1n,
    table: 1n,
    entry: "main",
    modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
  }),
  97,
);
assert.equal(aboveCaller.status, STATUS.invalidTier);
assert.equal(aboveCaller.diagnostic, "InvalidTier");
assert.equal(aboveCaller.calls, 0);

const engineBytes = bytes("MC_LUAUC_ENGINE");
const packBytes = bytes("MC_LUAUC_PACK");
const profileBytes = bytes("MC_LUAUC_PROFILE");
const engine = await CompilerEngine.load({
  engine: engineBytes,
  pack: packBytes,
  profile: profileBytes,
});
try {
  const sourcePackage = await buildSourcePackage(
    [{ name: "main", source: textEncoder.encode("return 1\n") }],
    "main",
    engine.profileDigest,
    engine.packDigest,
  );
  const compiled = await engine.compile(sourcePackage);
  assert.equal(compiled.status, 0, textDecoder.decode(compiled.diagnostic));
  assert.equal(compiled.guest[0], 0);
  assert.equal(compiled.guest[1], 0x61);
  assert.equal(compiled.guest[2], 0x73);
  assert.equal(compiled.guest[3], 0x6d);
  assert.equal(customSectionPayloads(compiled.guest, "mc_tier").length, 0);
  assert.equal(customSectionPayloads(compiled.guest, "mc_budget").length, 0);

  const explicit = await handleGuestRequest({
    body: encodeGuestRequest({
      tier: 1,
      budgetMask: 7,
      mem: 4096n,
      fuel: 1000n,
      table: 8n,
      entry: "main",
      modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
    }),
    profileDigest: engine.profileDigest,
    packDigest: engine.packDigest,
    callerMask: 0xff,
    compile: async () => compiled,
  });
  const explicitFrame = frame(explicit);
  assert.equal(explicitFrame.status, 0);
  assert.equal(textDecoder.decode(customSectionPayloads(explicitFrame.artifact, "mc_tier")[0]!), "full");
  assert.deepEqual(budget(customSectionPayloads(explicitFrame.artifact, "mc_budget")[0]!), {
    version: 1,
    mem: 4096n,
    fuel: 1000n,
    table: 8,
  });

  const defaults = await handleGuestRequest({
    body: encodeGuestRequest({
      entry: "main",
      modules: [{ name: "main", source: textEncoder.encode("return 1\n") }],
    }),
    profileDigest: engine.profileDigest,
    packDigest: engine.packDigest,
    callerMask: 0xff,
    compile: async () => compiled,
  });
  const defaultFrame = frame(defaults);
  assert.equal(defaultFrame.status, 0);
  assert.equal(textDecoder.decode(customSectionPayloads(defaultFrame.artifact, "mc_tier")[0]!), "full");
  assert.deepEqual(budget(customSectionPayloads(defaultFrame.artifact, "mc_budget")[0]!), {
    version: 1,
    mem: BigInt(DEFAULT_MEM),
    fuel: DEFAULT_FUEL,
    table: DEFAULT_TABLE,
  });
} finally {
  await engine.close();
}

const kernel = bytes("MC_KERNEL_WASM");
const loom = bytes("MC_LOOM_IMAGE");

const detached = await mc.create({ kernel, image: loom, deterministic: true });
try {
  const interpreted = await detached.run("luau", ["-e", "print(1)"]);
  assert.equal(interpreted.exitCode, 0, interpreted.stderr);
  assert.equal(interpreted.stdout, "1\n");
  await detached.fs.write("/home/user/main.luau", "return 1\n");
  const missing = await detached.run("luau", ["--compile", "-o", "/home/user/out", "/home/user/main.luau"]);
  assert.equal(missing.exitCode, 1, missing.stderr);
  assert.equal(missing.stderr.includes("luau-compile: compiler is not attached"), true, missing.stderr);
} finally {
  await detached.close();
}

const vm = await mc.create({
  kernel,
  image: loom,
  deterministic: true,
  compiler: { engine: engineBytes, pack: packBytes, profile: profileBytes },
});
try {
  await vm.fs.write("/home/user/main.luau", "return 1\n");
  const compiledGuest = await vm.run("luau", ["--compile", "-o", "/home/user/app", "/home/user/main.luau"]);
  assert.equal(compiledGuest.exitCode, 0, compiledGuest.stderr);
  const guest = await vm.fs.read("/home/user/app");
  assert.equal(guest[0], 0);
  assert.equal(guest[1], 0x61);
  assert.equal(guest[2], 0x73);
  assert.equal(guest[3], 0x6d);
  assert.equal(textDecoder.decode(customSectionPayloads(guest, "mc_tier")[0]!), "full");
  assert.deepEqual(budget(customSectionPayloads(guest, "mc_budget")[0]!), {
    version: 1,
    mem: BigInt(DEFAULT_MEM),
    fuel: DEFAULT_FUEL,
    table: DEFAULT_TABLE,
  });

  const narrowed = await vm.run("luau-compile", [
    "-o",
    "/home/user/narrow",
    "--tier",
    "read-only",
    "--mem",
    "4096",
    "--fuel",
    "1000",
    "--table",
    "8",
    "/home/user/main.luau",
  ]);
  assert.equal(narrowed.exitCode, 0, narrowed.stderr);
  const stamped = await vm.fs.read("/home/user/narrow");
  assert.equal(textDecoder.decode(customSectionPayloads(stamped, "mc_tier")[0]!), "read-only");
  assert.deepEqual(budget(customSectionPayloads(stamped, "mc_budget")[0]!), {
    version: 1,
    mem: 4096n,
    fuel: 1000n,
    table: 8,
  });

  // Same sources /bin/luau embeds. `luau --compile` only sees the files it is given.
  const officeModules = [
    "calc",
    "chart",
    "color",
    "docx",
    "media",
    "opc",
    "path",
    "pptx",
    "xform",
    "xlsx",
    "xml",
    "zip",
  ];
  const officeListed = process.env.MC_OFFICE_BATTERIES;
  if (!officeListed) throw new Error("MC_OFFICE_BATTERIES is not set");
  const runfilesRoot = process.env.RUNFILES_DIR;
  if (!runfilesRoot) throw new Error("RUNFILES_DIR is not set");
  const officeSource = new Map<string, string>();
  for (const rel of officeListed.split(" ").filter((part) => part.length > 0)) {
    const base = rel.slice(rel.lastIndexOf("/") + 1);
    if (!base.endsWith(".luau")) continue;
    officeSource.set(base.slice(0, -".luau".length), readFileSync(join(runfilesRoot, rel), "utf8"));
  }
  for (const name of officeModules) {
    if (!officeSource.has(name)) throw new Error(`missing office battery ${name}`);
  }
  for (const name of ["http", "log", "time", "url"]) {
    if (!officeSource.has(name)) throw new Error(`missing workflow battery ${name}`);
  }
  await vm.fs.mkdir("/home/user/lib");

  async function compileRun(entry: string, source: string, modules: string[], budget: string[]): Promise<{ exitCode: number; stdout: string; stderr: string }> {
    await vm.fs.write(`/home/user/${entry}.luau`, source);
    const args = ["--compile", "-o", `/home/user/${entry}`, ...budget, "--entry", entry, `/home/user/${entry}.luau`];
    for (const name of modules) {
      const path = `/home/user/lib/${name}.luau`;
      await vm.fs.write(path, officeSource.get(name)!);
      args.push(path);
    }
    const compiled = await vm.run("luau", args);
    assert.equal(compiled.exitCode, 0, `${entry} compile failed: ${compiled.stderr}`);
    return vm.run(`/home/user/${entry}`);
  }

  const native = await compileRun(
    "native",
    [
      'local json = require("json")',
      'local hash = require("hash")',
      'local encoding = require("encoding")',
      'local deflate = require("deflate")',
      'local re = require("re")',
      "",
      'local decoded = json.decode(json.encode({ name = "Acme", arr = 120000 }))',
      'assert(decoded.name == "Acme" and decoded.arr == 120000, "json roundtrip")',
      'local digest = hash.sha256("Acme")',
      'assert(digest == "37036cd8f9746d335038eca92f8a73ae5f1bca4779a1e55e5812e37743b2f5bf", digest)',
      'assert(encoding.base64.decode(encoding.base64.encode("Acme")) == "Acme", "base64")',
      'assert(encoding.hex.encode("AB") == "4142", "hex")',
      "local packed = deflate.compress(decoded.name)",
      "local plain = deflate.decompress(packed)",
      'assert(plain == "Acme", "deflate")',
      'local pat = re.compile("^Acme")',
      "assert(pat, \"re compile\")",
      "assert(pat:test(decoded.name), \"re test\")",
      "assert(not pat:test(\"Globex\"), \"re miss\")",
      "print(digest .. \" \" .. decoded.name)",
    ].join("\n"),
    [],
    [],
  );
  assert.equal(native.exitCode, 0, `${native.stderr}${native.stdout}`);
  assert.equal(native.stdout, "37036cd8f9746d335038eca92f8a73ae5f1bca4779a1e55e5812e37743b2f5bf Acme\n");

  const session = await compileRun(
    "session",
    [
      'local log = require("log")',
      'log.info("revenue", { customer = "Acme", arr = 120000 })',
    ].join("\n"),
    ["log", "time"],
    [],
  );
  assert.equal(session.exitCode, 0, `${session.stderr}${session.stdout}`);
  const logged = JSON.parse(session.stdout) as { level: string; msg: string; customer: string; arr: number; time: number };
  assert.equal(logged.level, "info");
  assert.equal(logged.msg, "revenue");
  assert.equal(logged.customer, "Acme");
  assert.equal(logged.arr, 120000);
  assert.equal(typeof logged.time, "number");

  const request = await compileRun(
    "request",
    [
      'local url = require("url")',
      'local http = require("http")',
      'local query = url.encode_query({ b = "x y", a = "1" })',
      'assert(query == "a=1&b=x%20y", query)',
      'local res, err = http.get("https://example.test/v1", { query = { q = "a b" }, json = { n = 1 } })',
      "assert(res == nil, \"fetch must fail closed\")",
      "print(query)",
      "print(err)",
    ].join("\n"),
    ["http", "url"],
    [],
  );
  assert.equal(request.exitCode, 0, `${request.stderr}${request.stdout}`);
  assert.equal(request.stdout, "a=1&b=x%20y\nEPERM\n");

  const noteBody = "<note>210000</note>";
  const packaged = await compileRun(
    "package",
    [
      'local zip = require("zip")',
      'local xml = require("xml")',
      'local path = require("path")',
      'local name = path.join("notes", "revenue.xml")',
      'assert(name == "notes/revenue.xml", name)',
      'local body = xml.serialize(xml.element("note", {}, { xml.text("210000") }))',
      'assert(body == "<note>210000</note>", body)',
      "local archive = zip.new()",
      "archive:add(name, body)",
      "local bytes, werr = zip.write(archive)",
      'assert(type(bytes) == "string", tostring(werr))',
      'assert(sys.fs.write("/tmp/note.zip", bytes), "write")',
      'local raw, rerr = sys.fs.read("/tmp/note.zip")',
      'assert(type(raw) == "string", tostring(rerr))',
      "local back, zerr = zip.read(raw)",
      "assert(back, tostring(zerr))",
      "local entry = back:get(name)",
      'assert(entry and entry.data == body, "entry")',
      "print(body)",
    ].join("\n"),
    ["zip", "xml", "path"],
    ["--mem", String(64 * 1024 * 1024), "--fuel", "2000000000000", "--table", "20000"],
  );
  assert.equal(packaged.exitCode, 0, `${packaged.stderr}${packaged.stdout}`);
  assert.equal(packaged.stdout, `${noteBody}\n`);
  const noteZip = await vm.fs.read("/tmp/note.zip");
  assert.equal(noteZip[0], 0x50);
  assert.equal(noteZip[1], 0x4b);
  assert.equal(noteZip.length > 64, true);

  const officeApp = [
    'local xlsx = require("xlsx")',
    'local docx = require("docx")',
    'local pptx = require("pptx")',
    "",
    "local wb = xlsx.new()",
    'local ws = wb:addWorksheet("Revenue")',
    "ws:setColumns({",
    '  { header = "Customer", key = "customer", width = 20 },',
    '  { header = "ARR", key = "arr", width = 14 },',
    "})",
    'ws:addRow({ customer = "Acme", arr = 120000 })',
    'ws:addRow({ customer = "Globex", arr = 90000 })',
    'ws:setCell("B4", xlsx.formula("SUM(B2:B3)"))',
    "",
    "wb:recalculate()",
    'assert(#wb:scanErrors() == 0, "formula errors present")',
    'assert(wb:save("/tmp/revenue.xlsx"))',
    "",
    'local b4 = ws:cell("B4").value',
    'print(b4.formula .. " = " .. b4.result.value)',
    "",
    "local doc = docx.new({ children = {",
    '  docx.heading(1, "Revenue"),',
    '  docx.paragraph("ARR total is " .. tostring(b4.result.value)),',
    "} })",
    'assert(doc:save("/tmp/revenue.docx"))',
    "",
    'local pres = pptx.new({ slideSize = "16:9" })',
    'local title = pres:addSlide({ layout = "title" })',
    'title:setTitle("Revenue")',
    'title:setBody("ARR total " .. tostring(b4.result.value))',
    'assert(pres:save("/tmp/revenue.pptx"))',
    "",
  ].join("\n");
  await vm.fs.write("/home/user/office.luau", officeApp);
  const compileArgs = [
    "--compile",
    "-o",
    "/home/user/office",
    "--mem",
    String(256 * 1024 * 1024),
    "--fuel",
    "2000000000000",
    "--table",
    "40000",
    "--entry",
    "office",
    "/home/user/office.luau",
  ];
  for (const name of officeModules) {
    const path = `/home/user/lib/${name}.luau`;
    await vm.fs.write(path, officeSource.get(name)!);
    compileArgs.push(path);
  }
  const officeCompile = await vm.run("luau", compileArgs);
  assert.equal(officeCompile.exitCode, 0, officeCompile.stderr);
  const office = await vm.run("/home/user/office");
  assert.equal(office.exitCode, 0, `${office.stderr}${office.stdout}`);
  assert.equal(office.stdout, "SUM(B2:B3) = 210000\n");
  for (const path of ["/tmp/revenue.xlsx", "/tmp/revenue.docx", "/tmp/revenue.pptx"]) {
    const saved = await vm.fs.read(path);
    assert.equal(saved[0], 0x50, path);
    assert.equal(saved[1], 0x4b, path);
    assert.equal(saved.length > 64, true, path);
  }
} finally {
  await vm.close();
}
