// SourcePackageV1 layout matches luauc hosts/js/host.mjs buildRequest.
// Coverage is 0 and every inline plan is empty. The frontend contract is the
// pinned compiler identity; a mismatch is CompilerContractMismatch.

import { FRONTEND_CONTRACT } from "./contract.js";

export const MAX_REQUEST_BYTES = 16 * 1024 * 1024;
// The office package object is 39 MiB. This ceiling covers that object and the linked module.
export const COMPILE_BUDGET_BYTES = 64 * 1024 * 1024;
const HEADER_SIZE = 240;
const RECORD_SIZE = 64;

const textEncoder = new TextEncoder();

export interface SourceModule {
  name: string;
  source: Uint8Array;
}

export function validModuleName(name: string): boolean {
  if (name.length === 0 || name.startsWith("/") || name.endsWith("/") || name.includes("..")) {
    return false;
  }
  for (let index = 0; index < name.length; index++) {
    const code = name.charCodeAt(index);
    const ok =
      (code >= 0x61 && code <= 0x7a) ||
      (code >= 0x30 && code <= 0x39) ||
      code === 0x5f ||
      code === 0x2d ||
      code === 0x2e ||
      code === 0x2f;
    if (!ok) return false;
  }
  return true;
}

export function encodedPackageLength(
  modules: readonly { name: string; sourceLength: number }[],
): number {
  let total = HEADER_SIZE + modules.length * RECORD_SIZE;
  for (const module of modules) {
    const nameLength = textEncoder.encode(module.name).length;
    total += nameLength + nameLength + 6 + module.sourceLength;
  }
  return total;
}

function compareBytes(left: Uint8Array, right: Uint8Array): number {
  const count = Math.min(left.length, right.length);
  for (let index = 0; index < count; index++) {
    if (left[index] !== right[index]) return left[index]! - right[index]!;
  }
  return left.length - right.length;
}

function concat(parts: Uint8Array[]): Uint8Array {
  let total = 0;
  for (const part of parts) total += part.length;
  const out = new Uint8Array(total);
  let cursor = 0;
  for (const part of parts) {
    out.set(part, cursor);
    cursor += part.length;
  }
  return out;
}

function putU16(target: Uint8Array, offset: number, value: number): void {
  new DataView(target.buffer, target.byteOffset, target.byteLength).setUint16(offset, value, true);
}

function putU32(target: Uint8Array, offset: number, value: number): void {
  new DataView(target.buffer, target.byteOffset, target.byteLength).setUint32(offset, value, true);
}

async function sha256(value: Uint8Array): Promise<Uint8Array> {
  const subtle = globalThis.crypto?.subtle;
  if (!subtle) throw new Error("the JavaScript host requires the standard Web Crypto digest API");
  return new Uint8Array(await subtle.digest("SHA-256", value));
}

function sized(value: Uint8Array): Uint8Array {
  const header = new Uint8Array(4);
  putU32(header, 0, value.length);
  return concat([header, value]);
}

/** Encode one SourcePackageV1. Modules are sorted by raw name bytes. */
export async function buildSourcePackage(
  modules: readonly SourceModule[],
  entryName: string,
  profileDigest: Uint8Array,
  packDigest: Uint8Array,
): Promise<Uint8Array> {
  if (profileDigest.length !== 32 || packDigest.length !== 32) {
    throw new Error("profile and pack digests must be 32 bytes");
  }
  const prepared = await Promise.all(
    modules.map(async (module) => {
      const name = textEncoder.encode(module.name);
      const source = module.source;
      return {
        name,
        sourceName: textEncoder.encode(`@${module.name}.luau`),
        source,
        contentDigest: await sha256(source),
      };
    }),
  );
  prepared.sort((left, right) => compareBytes(left.name, right.name));
  if (
    prepared.length === 0 ||
    prepared.some((module, index) => index > 0 && compareBytes(prepared[index - 1]!.name, module.name) === 0)
  ) {
    throw new Error("invalid source package modules");
  }
  const entryBytes = textEncoder.encode(entryName);
  const entryModuleId = prepared.findIndex((module) => compareBytes(module.name, entryBytes) === 0);
  if (entryModuleId < 0) throw new Error(`missing entry module ${entryName}`);

  const manifestHeader = new Uint8Array(8);
  putU32(manifestHeader, 0, prepared.length);
  putU32(manifestHeader, 4, entryModuleId);
  const manifest: Uint8Array[] = [manifestHeader];
  for (const module of prepared) {
    const planCount = new Uint8Array(4);
    manifest.push(sized(module.name), sized(module.sourceName), module.contentDigest, planCount);
  }
  const manifestDigest = await sha256(concat(manifest));

  const total = prepared.reduce(
    (size, module) => size + module.name.length + module.sourceName.length + module.source.length,
    HEADER_SIZE + RECORD_SIZE * prepared.length,
  );
  const request = new Uint8Array(total);
  request.set(textEncoder.encode("LUAUCS1\0"), 0);
  putU16(request, 8, 1);
  putU16(request, 10, HEADER_SIZE);
  putU32(request, 12, total);
  putU32(request, 16, prepared.length);
  putU32(request, 20, entryModuleId);
  putU32(request, 24, RECORD_SIZE);
  putU32(request, 28, 0);
  putU32(request, 176, 0);
  putU32(request, 180, 64);
  putU32(request, 184, 0);
  putU32(request, 188, 1_048_576);
  putU32(request, 192, 4096);
  putU32(request, 196, COMPILE_BUDGET_BYTES);

  const coverage = new Uint8Array(4);
  const optionBytes = new Uint8Array(32);
  putU32(optionBytes, 0, 0);
  putU32(optionBytes, 4, 64);
  putU32(optionBytes, 8, 0);
  putU32(optionBytes, 12, 1_048_576);
  putU32(optionBytes, 16, 4096);
  putU32(optionBytes, 20, COMPILE_BUDGET_BYTES);
  const planPreimage: Uint8Array[] = prepared.map(() => new Uint8Array(4));
  const requestId = await sha256(
    concat([
      textEncoder.encode("luauc-source-request-v1\0"),
      coverage,
      FRONTEND_CONTRACT,
      profileDigest,
      packDigest,
      manifestDigest,
      optionBytes,
      ...planPreimage,
    ]),
  );
  request.set(requestId.subarray(0, 16), 32);
  request.set(FRONTEND_CONTRACT, 48);
  request.set(profileDigest, 80);
  request.set(packDigest, 112);
  request.set(manifestDigest, 144);

  let cursor = HEADER_SIZE + RECORD_SIZE * prepared.length;
  for (let index = 0; index < prepared.length; index++) {
    const module = prepared[index]!;
    const record = HEADER_SIZE + RECORD_SIZE * index;
    putU32(request, record, cursor);
    putU32(request, record + 4, module.name.length);
    request.set(module.name, cursor);
    cursor += module.name.length;
    putU32(request, record + 8, cursor);
    putU32(request, record + 12, module.sourceName.length);
    request.set(module.sourceName, cursor);
    cursor += module.sourceName.length;
    putU32(request, record + 16, cursor);
    putU32(request, record + 20, module.source.length);
    request.set(module.source, cursor);
    cursor += module.source.length;
    request.set(module.contentDigest, record + 24);
  }
  if (cursor !== request.length) throw new Error("source package request layout drifted");
  return request;
}
