// Guest compile request (AOLC1). The host builds SourcePackageV1, stamps the
// linked wasm, and returns those bytes. Statuses 0..7 come from the compiler.
// Statuses 8..19 are rejected before compile.

import { tierCaps } from "@mc/contracts/constants";
import { buildSourcePackage, encodedPackageLength, validModuleName, MAX_REQUEST_BYTES } from "./package.js";
import { hexBytes, logCompile } from "./metrics.js";
import { stampGuest } from "./wasm.js";

export const STATUS = {
  ok: 0,
  invalidArgument: 1,
  invalidRequest: 2,
  frontendFailure: 3,
  backendFailure: 4,
  linkFailure: 5,
  resourceLimit: 6,
  invalidContext: 7,
  invalidMagic: 8,
  invalidVersion: 9,
  invalidHeader: 10,
  invalidFlags: 11,
  invalidModuleCount: 12,
  invalidEntry: 13,
  duplicateModule: 14,
  invalidModuleName: 15,
  requestTooLarge: 16,
  invalidTier: 17,
  installRejected: 18,
  truncated: 19,
} as const;

/** Kernel budget for a program with no mc_budget section. */
export const DEFAULT_MEM = 16 * 1024 * 1024;
export const DEFAULT_FUEL = 50_000_000_000n;
export const DEFAULT_TABLE = 10_000;
/** Hard maxima from the kernel. A VM ceiling can be lower. */
export const HARD_MEM = 1024 * 1024 * 1024;
export const HARD_FUEL = 4_000_000_000_000n;
export const HARD_TABLE = 1_000_000;

const MAGIC = [0x41, 0x4f, 0x4c, 0x43, 0x31, 0x00, 0x00, 0x00];
const HEADER_SIZE = 56;
const TIER_NAME: Record<number, string> = {
  1: "full",
  2: "read-write",
  3: "read-only",
  4: "isolated",
};

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

export interface GuestModule {
  name: string;
  source: Uint8Array;
}

export interface CompileResult {
  status: number;
  diagnostic: Uint8Array;
  guest: Uint8Array;
}

export interface ParsedGuestRequest {
  tier: number;
  budgetMask: number;
  mem: bigint;
  fuel: bigint;
  table: bigint;
  entry: string;
  modules: GuestModule[];
}

export interface Stamp {
  tier: string;
  mem: bigint;
  fuel: bigint;
  table: number;
}

type Failure = { status: number; diagnostic: string };

function fail(status: number, diagnostic: string): Failure {
  return { status, diagnostic };
}

function u16(bytes: Uint8Array, offset: number): number {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint16(offset, true);
}

function u32(bytes: Uint8Array, offset: number): number {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(offset, true);
}

function u64(bytes: Uint8Array, offset: number): bigint {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getBigUint64(offset, true);
}

function putU16(target: Uint8Array, offset: number, value: number): void {
  new DataView(target.buffer, target.byteOffset, target.byteLength).setUint16(offset, value, true);
}

function putU32(target: Uint8Array, offset: number, value: number): void {
  new DataView(target.buffer, target.byteOffset, target.byteLength).setUint32(offset, value, true);
}

function putU64(target: Uint8Array, offset: number, value: bigint): void {
  new DataView(target.buffer, target.byteOffset, target.byteLength).setBigUint64(offset, value, true);
}

export function frameResponse(status: number, diagnostic: Uint8Array, artifact: Uint8Array): Uint8Array {
  const guest = status === 0 ? artifact : new Uint8Array();
  const out = new Uint8Array(12 + diagnostic.length + guest.length);
  putU32(out, 0, status >>> 0);
  putU32(out, 4, diagnostic.length);
  out.set(diagnostic, 8);
  putU32(out, 8 + diagnostic.length, guest.length);
  out.set(guest, 12 + diagnostic.length);
  return out;
}

function frameNamed(status: number, diagnostic: string): Uint8Array {
  return frameResponse(status, textEncoder.encode(diagnostic), new Uint8Array());
}

function readCounted(bytes: Uint8Array, cursor: number): { value: Uint8Array; next: number } | null {
  if (cursor + 4 > bytes.length) return null;
  const length = u32(bytes, cursor);
  const start = cursor + 4;
  const end = start + length;
  if (end > bytes.length) return null;
  return { value: bytes.subarray(start, end), next: end };
}

function isFailure(value: ParsedGuestRequest | Stamp | Failure): value is Failure {
  return "diagnostic" in value;
}

export function parseGuestRequest(body: Uint8Array, callerMask = 0xff): ParsedGuestRequest | Failure {
  if (body.length < 8) return fail(STATUS.truncated, "Truncated");
  for (let index = 0; index < MAGIC.length; index++) {
    if (body[index] !== MAGIC[index]) return fail(STATUS.invalidMagic, "InvalidMagic");
  }
  if (body.length < HEADER_SIZE) return fail(STATUS.truncated, "Truncated");
  const version = u16(body, 8);
  if (version !== 1) return fail(STATUS.invalidVersion, "InvalidVersion");
  const headerSize = u16(body, 10);
  const totalSize = u32(body, 12);
  const count = u32(body, 16);
  const tier = u16(body, 20);
  const budgetMask = u16(body, 22);
  const reserved = u32(body, 24);
  const pad = u32(body, 28);
  const mem = u64(body, 32);
  const fuel = u64(body, 40);
  const table = u64(body, 48);
  if (headerSize !== HEADER_SIZE || reserved !== 0 || pad !== 0 || budgetMask > 7) {
    return fail(STATUS.invalidHeader, "InvalidHeader");
  }
  if ((budgetMask & 1) === 0 ? mem !== 0n : mem === 0n) return fail(STATUS.invalidHeader, "InvalidHeader");
  if ((budgetMask & 2) === 0 ? fuel !== 0n : fuel === 0n) return fail(STATUS.invalidHeader, "InvalidHeader");
  if ((budgetMask & 4) === 0 ? table !== 0n : table === 0n) return fail(STATUS.invalidHeader, "InvalidHeader");
  if (table > 0xffffffffn) return fail(STATUS.invalidHeader, "InvalidHeader");
  if (count < 1 || count > 128) return fail(STATUS.invalidModuleCount, "InvalidModuleCount");
  if (totalSize > MAX_REQUEST_BYTES || body.length > MAX_REQUEST_BYTES) {
    return fail(STATUS.requestTooLarge, "RequestTooLarge");
  }
  if (body.length < totalSize || totalSize < headerSize) return fail(STATUS.truncated, "Truncated");
  if (body.length > totalSize) return fail(STATUS.invalidHeader, "InvalidHeader");

  const declared = body.subarray(0, totalSize);
  let cursor = headerSize;
  const entryBytes = readCounted(declared, cursor);
  if (!entryBytes) return fail(STATUS.truncated, "Truncated");
  cursor = entryBytes.next;
  const entry = textDecoder.decode(entryBytes.value);
  if (textEncoder.encode(entry).length !== entryBytes.value.length) {
    return fail(STATUS.invalidModuleName, "InvalidModuleName");
  }
  const modules: GuestModule[] = [];
  for (let index = 0; index < count; index++) {
    const nameBytes = readCounted(declared, cursor);
    if (!nameBytes) return fail(STATUS.truncated, "Truncated");
    cursor = nameBytes.next;
    const sourceBytes = readCounted(declared, cursor);
    if (!sourceBytes) return fail(STATUS.truncated, "Truncated");
    cursor = sourceBytes.next;
    const name = textDecoder.decode(nameBytes.value);
    if (textEncoder.encode(name).length !== nameBytes.value.length || !validModuleName(name)) {
      return fail(STATUS.invalidModuleName, "InvalidModuleName");
    }
    modules.push({ name, source: sourceBytes.value });
  }
  if (cursor < totalSize) return fail(STATUS.truncated, "Truncated");
  if (cursor > totalSize) return fail(STATUS.invalidHeader, "InvalidHeader");

  const sorted = [...modules].sort((left, right) => compareNames(left.name, right.name));
  for (let index = 1; index < sorted.length; index++) {
    if (sorted[index - 1]!.name === sorted[index]!.name) {
      return fail(STATUS.duplicateModule, "DuplicateModule");
    }
  }
  if (!modules.some((module) => module.name === entry)) {
    return fail(STATUS.invalidEntry, "InvalidEntry");
  }
  if (tier !== 0) {
    const caps = tierCaps(tier);
    if (!(tier in TIER_NAME) || (caps & callerMask) !== caps) {
      return fail(STATUS.invalidTier, "InvalidTier");
    }
  }
  if (modules.some((module) => module.source.length === 0)) {
    return fail(STATUS.invalidRequest, "NonCanonicalLayout");
  }
  if (
    encodedPackageLength(modules.map((module) => ({ name: module.name, sourceLength: module.source.length }))) >
    MAX_REQUEST_BYTES
  ) {
    return fail(STATUS.requestTooLarge, "RequestTooLarge");
  }
  return { tier, budgetMask, mem, fuel, table, entry, modules };
}

export function resolveStamp(
  request: ParsedGuestRequest,
  callerMask: number,
  defaults: { tier?: number; memCeiling?: number; fuelCeiling?: bigint; tableCeiling?: number } = {},
): Stamp | Failure {
  const tier = request.tier === 0 ? defaults.tier ?? 1 : request.tier;
  const caps = tierCaps(tier);
  if (!(tier in TIER_NAME) || (caps & callerMask) !== caps) {
    return fail(STATUS.invalidTier, "InvalidTier");
  }
  const memCeiling = BigInt(defaults.memCeiling ?? HARD_MEM);
  const fuelCeiling = defaults.fuelCeiling ?? HARD_FUEL;
  const tableCeiling = defaults.tableCeiling ?? HARD_TABLE;
  const mem = clampBig((request.budgetMask & 1) !== 0 ? request.mem : BigInt(DEFAULT_MEM), memCeiling, BigInt(HARD_MEM));
  const fuel = clampBig((request.budgetMask & 2) !== 0 ? request.fuel : DEFAULT_FUEL, fuelCeiling, HARD_FUEL);
  const table = Math.min((request.budgetMask & 4) !== 0 ? Number(request.table) : DEFAULT_TABLE, tableCeiling, HARD_TABLE);
  return { tier: TIER_NAME[tier] ?? "full", mem, fuel, table };
}

function clampBig(value: bigint, ceiling: bigint, hard: bigint): bigint {
  let out = value;
  if (out > ceiling) out = ceiling;
  if (out > hard) out = hard;
  return out;
}

function compareNames(left: string, right: string): number {
  const a = textEncoder.encode(left);
  const b = textEncoder.encode(right);
  const count = Math.min(a.length, b.length);
  for (let index = 0; index < count; index++) {
    if (a[index] !== b[index]) return a[index]! - b[index]!;
  }
  return a.length - b.length;
}

/** Parse AOLC1, compile when the request is well formed, stamp, and frame the response. */
export async function handleGuestRequest(input: {
  body: Uint8Array;
  profileDigest: Uint8Array;
  packDigest: Uint8Array;
  callerMask: number;
  defaultTier?: number;
  memCeiling?: number;
  fuelCeiling?: bigint;
  tableCeiling?: number;
  compile: (sourcePackage: Uint8Array) => Promise<CompileResult>;
}): Promise<Uint8Array> {
  const started = performance.now();
  const parsed = parseGuestRequest(input.body, input.callerMask);
  if (isFailure(parsed)) {
    logCompile({
      host: "wasm",
      requestId: "-",
      status: parsed.status,
      durationMs: Math.max(0, Math.round(performance.now() - started)),
      requestBytes: input.body.length,
      guestBytes: 0,
      diagnosticBytes: textEncoder.encode(parsed.diagnostic).length,
      moduleCount: 0,
    });
    return frameNamed(parsed.status, parsed.diagnostic);
  }
  const stamp = resolveStamp(parsed, input.callerMask, {
    tier: input.defaultTier,
    memCeiling: input.memCeiling,
    fuelCeiling: input.fuelCeiling,
    tableCeiling: input.tableCeiling,
  });
  if (isFailure(stamp)) {
    logCompile({
      host: "wasm",
      requestId: "-",
      status: stamp.status,
      durationMs: Math.max(0, Math.round(performance.now() - started)),
      requestBytes: input.body.length,
      guestBytes: 0,
      diagnosticBytes: textEncoder.encode(stamp.diagnostic).length,
      moduleCount: parsed.modules.length,
    });
    return frameNamed(stamp.status, stamp.diagnostic);
  }
  const sourcePackage = await buildSourcePackage(parsed.modules, parsed.entry, input.profileDigest, input.packDigest);
  const compiled = await input.compile(sourcePackage);
  const status = compiled.status >>> 0;
  if (status !== 0) return frameResponse(status, compiled.diagnostic, new Uint8Array());
  const stamped = stampGuest(compiled.guest, stamp.tier, stamp.mem, stamp.fuel, stamp.table);
  return frameResponse(0, new Uint8Array(), stamped);
}

export interface EncodeGuestOptions {
  tier?: number;
  budgetMask?: number;
  mem?: bigint;
  fuel?: bigint;
  table?: bigint;
  entry: string;
  modules: GuestModule[];
  totalSize?: number;
  headerSize?: number;
}

/** Test helper. Preserves module order; the host sorts when it compiles. */
export function encodeGuestRequest(options: EncodeGuestOptions): Uint8Array {
  const headerSize = options.headerSize ?? HEADER_SIZE;
  const budgetMask = options.budgetMask ?? 0;
  const chunks: Uint8Array[] = [];
  const pushU32String = (value: string | Uint8Array) => {
    const bytes = typeof value === "string" ? textEncoder.encode(value) : value;
    const header = new Uint8Array(4);
    putU32(header, 0, bytes.length);
    chunks.push(header, bytes);
  };
  pushU32String(options.entry);
  for (const module of options.modules) {
    pushU32String(module.name);
    const source = module.source;
    const header = new Uint8Array(4);
    putU32(header, 0, source.length);
    chunks.push(header, source);
  }
  let payloadLength = 0;
  for (const chunk of chunks) payloadLength += chunk.length;
  const total = options.totalSize ?? headerSize + payloadLength;
  const out = new Uint8Array(Math.max(headerSize, HEADER_SIZE) + payloadLength);
  out.set(Uint8Array.from(MAGIC), 0);
  putU16(out, 8, 1);
  putU16(out, 10, headerSize);
  putU32(out, 12, total);
  putU32(out, 16, options.modules.length);
  putU16(out, 20, options.tier ?? 0);
  putU16(out, 22, budgetMask);
  putU32(out, 28, 0);
  putU64(out, 32, (budgetMask & 1) !== 0 ? (options.mem ?? 0n) : 0n);
  putU64(out, 40, (budgetMask & 2) !== 0 ? (options.fuel ?? 0n) : 0n);
  putU64(out, 48, (budgetMask & 4) !== 0 ? (options.table ?? 0n) : 0n);
  let cursor = headerSize;
  for (const chunk of chunks) {
    out.set(chunk, cursor);
    cursor += chunk.length;
  }
  return out.subarray(0, headerSize + payloadLength);
}
