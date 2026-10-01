import assert from "node:assert/strict";
import { access, mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawn } from "node:child_process";

const executable = join(process.env.RUNFILES_DIR, process.env.AGENTOS_GIT_ENGINE);
const root = await mkdtemp(join(tmpdir(), "agentos-gitz-port-"));
const child = spawn(executable, [], { stdio: ["pipe", "pipe", "pipe"] });
let stderr = "";
child.stderr.setEncoding("utf8");
child.stderr.on("data", chunk => { stderr += chunk; });

const iterator = child.stdout[Symbol.asyncIterator]();
let buffered = Buffer.alloc(0);
async function readExact(length) {
  while (buffered.length < length) {
    const next = await iterator.next();
    assert.equal(next.done, false, `port EOF: ${stderr}`);
    buffered = Buffer.concat([buffered, next.value]);
  }
  const out = buffered.subarray(0, length);
  buffered = buffered.subarray(length);
  return out;
}

async function readFrame() {
  const prefix = await readExact(4);
  return readExact(prefix.readUInt32LE(0));
}

function request(opcode, requestId, payload = Buffer.alloc(0)) {
  const frame = Buffer.alloc(20 + payload.length);
  frame.write("AOGQ", 0, "ascii");
  frame.writeUInt16LE(1, 4);
  frame.writeUInt16LE(0, 6);
  frame.writeUInt16LE(opcode, 8);
  frame.writeUInt16LE(0, 10);
  frame.writeUInt32LE(requestId, 12);
  frame.writeUInt32LE(payload.length, 16);
  payload.copy(frame, 20);
  const carrier = Buffer.alloc(4 + frame.length);
  carrier.writeUInt32LE(frame.length, 0);
  frame.copy(carrier, 4);
  return carrier;
}

function sessionConfig(path) {
  const rootBytes = Buffer.from(path, "utf8");
  const payload = Buffer.alloc(3 + 2 + 1 + 4 + rootBytes.length + 1);
  payload.writeUInt16LE(1, 0);
  payload[2] = 1;
  payload.writeUInt16LE(2, 3);
  payload[5] = 0;
  payload.writeUInt32LE(rootBytes.length, 6);
  rootBytes.copy(payload, 10);
  payload[10 + rootBytes.length] = 0;
  return payload;
}

function bytesField(bytes) {
  const out = Buffer.alloc(4 + bytes.length);
  out.writeUInt32LE(bytes.length, 0);
  bytes.copy(out, 4);
  return out;
}

function u16(value) { const out = Buffer.alloc(2); out.writeUInt16LE(value); return out; }
function u32(value) { const out = Buffer.alloc(4); out.writeUInt32LE(value); return out; }
function i64(value) { const out = Buffer.alloc(8); out.writeBigInt64LE(BigInt(value)); return out; }
function i32(value) { const out = Buffer.alloc(4); out.writeInt32LE(value); return out; }

function fileWrite(path, data) {
  return Buffer.concat([
    u16(6), Buffer.of(1), bytesField(Buffer.from(path)),
    Buffer.of(0, 0, 0, 0, 1), bytesField(Buffer.from(data)), Buffer.of(0),
  ]);
}

function signature(name, email, seconds) {
  return Buffer.concat([u16(4), Buffer.of(1), bytesField(Buffer.from(name)), bytesField(Buffer.from(email)), i64(seconds), i32(0)]);
}

function porcelain({ action, revision, message, paths = [], author, committer }) {
  const optional = value => value === undefined ? Buffer.of(0) : Buffer.concat([Buffer.of(1), bytesField(value)]);
  const map = [u32(paths.length)];
  for (const path of [...paths].sort()) map.push(bytesField(Buffer.from(path)), bytesField(Buffer.alloc(0)));
  return Buffer.concat([
    u16(7), Buffer.of(1), u16(action), u32(0),
    optional(revision === undefined ? undefined : Buffer.from(revision)),
    Buffer.of(0),
    optional(message === undefined ? undefined : Buffer.from(message)),
    ...map,
    Buffer.of(0, 0),
    optional(author), optional(committer),
  ]);
}

function mergeHead(revision) {
  return Buffer.concat([u16(42), Buffer.of(1), bytesField(Buffer.from(revision))]);
}

function mergeRequest({ action = 3, revisions = [], author, strategy = 0, ff = 0 }) {
  const heads = [u32(revisions.length), ...revisions.map((revision) => bytesField(mergeHead(revision)))];
  const signatureField = author === undefined ? Buffer.of(0) : Buffer.concat([Buffer.of(1), bytesField(author)]);
  return Buffer.concat([
    u16(43), Buffer.of(1), u16(action), ...heads,
    u16(strategy), u16(ff), u16(0), u16(0), u16(0),
    Buffer.of(1, 0, 0, 0, 0, 0),
    signatureField, signatureField,
  ]);
}

function fileRead(path) {
  return Buffer.concat([u16(6), Buffer.of(1), bytesField(Buffer.from(path)), Buffer.of(0, 0, 0, 0, 0, 0)]);
}

function hashOf(frame) {
  return frame.subarray(frame.length - 20).toString("hex");
}

function assertResponse(frame, opcode, requestId) {
  assert.equal(frame.subarray(0, 4).toString("ascii"), "AOGR");
  assert.equal(frame.readUInt16LE(8), opcode);
  assert.equal(frame.readUInt16LE(10), 0);
  assert.equal(frame.readUInt32LE(12), requestId);
  assert.equal(frame.length, 20 + frame.readUInt32LE(16));
}

try {
  child.stdin.write(request(2, 1, sessionConfig(root)));
  assertResponse(await readFrame(), 2, 1);

  child.stdin.write(request(1, 2));
  const described = await readFrame();
  assertResponse(described, 1, 2);
  assert.ok(described.includes(Buffer.from("31b716bfedd2bfbbd5711c0178d1930f777547e5")));

  child.stdin.write(request(16, 3));
  assertResponse(await readFrame(), 16, 3);
  await access(join(root, ".git", "HEAD"));

  child.stdin.write(request(258, 4, fileWrite("nested/hello.txt", "hello from shared core\n")));
  assertResponse(await readFrame(), 258, 4);
  assert.equal(await readFile(join(root, "nested", "hello.txt"), "utf8"), "hello from shared core\n");

  child.stdin.write(request(272, 5));
  const untracked = await readFrame();
  assertResponse(untracked, 272, 5);
  assert.ok(untracked.includes(Buffer.from("nested/hello.txt")));

  child.stdin.write(request(273, 6, porcelain({ action: 4, paths: ["nested/hello.txt"] })));
  assertResponse(await readFrame(), 273, 6);

  const sig = signature("Artifact Test", "artifact@example.invalid", 1_700_000_000);
  child.stdin.write(request(275, 7, porcelain({ action: 3, message: "artifact commit", author: sig, committer: sig })));
  assertResponse(await readFrame(), 275, 7);
  child.stdin.write(request(277, 8, porcelain({ action: 2, revision: "HEAD" })));
  const resolved = await readFrame();
  assertResponse(resolved, 277, 8);
  assert.equal(resolved.readUInt16LE(20), 20);

  async function call(opcode, id, payload) {
    child.stdin.write(request(opcode, id, payload));
    const frame = await readFrame();
    assertResponse(frame, opcode, id);
    return frame;
  }
  child.stdin.write(request(258, 20, fileWrite("lines.txt", "a\nb\nc\n")));
  assertResponse(await readFrame(), 258, 20);
  await call(273, 21, porcelain({ action: 4, paths: ["lines.txt"] }));
  const baseCommit = await call(275, 22, porcelain({ action: 3, message: "merge base", author: sig, committer: sig }));
  const base = hashOf(baseCommit);
  child.stdin.write(request(258, 23, fileWrite("lines.txt", "A\nb\nc\n")));
  assertResponse(await readFrame(), 258, 23);
  await call(273, 24, porcelain({ action: 4, paths: ["lines.txt"] }));
  const oursCommit = await call(275, 25, porcelain({ action: 3, message: "merge ours", author: sig, committer: sig }));
  const ours = hashOf(oursCommit);
  await call(281, 26, porcelain({ action: 3, revision: base }));
  child.stdin.write(request(258, 27, fileWrite("lines.txt", "a\nb\nC\n")));
  assertResponse(await readFrame(), 258, 27);
  await call(273, 28, porcelain({ action: 4, paths: ["lines.txt"] }));
  const theirsCommit = await call(275, 29, porcelain({ action: 3, message: "merge theirs", author: sig, committer: sig }));
  await call(281, 30, porcelain({ action: 3, revision: ours }));
  const clean = await call(289, 31, mergeRequest({ revisions: [hashOf(theirsCommit)], author: sig }));
  assert.equal(clean.readUInt16LE(20), 45);
  assert.equal(clean.readUInt16LE(27), 3);
  child.stdin.write(request(258, 32, fileWrite("conflict.txt", "a\nb\nc\n")));
  assertResponse(await readFrame(), 258, 32);
  await call(273, 33, porcelain({ action: 4, paths: ["conflict.txt"] }));
  const conflictBase = hashOf(await call(275, 34, porcelain({ action: 3, message: "conflict base", author: sig, committer: sig })));
  child.stdin.write(request(258, 35, fileWrite("conflict.txt", "a\nX\nc\n")));
  assertResponse(await readFrame(), 258, 35);
  await call(273, 36, porcelain({ action: 4, paths: ["conflict.txt"] }));
  const conflictOurs = hashOf(await call(275, 37, porcelain({ action: 3, message: "conflict ours", author: sig, committer: sig })));
  await call(281, 38, porcelain({ action: 3, revision: conflictBase }));
  child.stdin.write(request(258, 39, fileWrite("conflict.txt", "a\nY\nc\n")));
  assertResponse(await readFrame(), 258, 39);
  await call(273, 40, porcelain({ action: 4, paths: ["conflict.txt"] }));
  const conflictTheirs = hashOf(await call(275, 41, porcelain({ action: 3, message: "conflict theirs", author: sig, committer: sig })));
  await call(281, 42, porcelain({ action: 3, revision: conflictOurs }));
  const conflict = await call(289, 43, mergeRequest({ revisions: [conflictTheirs], author: sig }));
  assert.equal(conflict.readUInt16LE(27), 4);
  await access(join(root, ".git", "MERGE_HEAD"));
  await access(join(root, ".git", "ORIG_HEAD"));
  const abort = await call(289, 44, mergeRequest({ action: 9, author: sig }));
  assert.equal(abort.readUInt16LE(27), 6);
  assert.equal(await readFile(join(root, "conflict.txt"), "utf8"), "a\nX\nc\n");
  await assert.rejects(access(join(root, ".git", "ORIG_HEAD")));
  await assert.rejects(access(join(root, ".git", "MERGE_HEAD")));

  child.stdin.write(request(3, 9));
  child.stdin.end();
  const exitCode = await new Promise((resolve, reject) => {
    child.once("error", reject);
    child.once("exit", resolve);
  });
  assert.equal(exitCode, 0, stderr);
  assert.equal(stderr, "", `native Git engine emitted diagnostics: ${stderr}`);
  console.log(JSON.stringify({ native_port: true, durable_root: true, local_commit: true, short_frame_carrier: true }));
} finally {
  if (child.exitCode === null) child.kill("SIGKILL");
  await rm(root, { recursive: true, force: true });
}
