/** Walk a Wasm module and return the payload of its one `luauc.runtime.v1` custom section. */

const PROFILE_SECTION = "luauc.runtime.v1";
const STAMP_NAMES = ["mc_tier", "mc_budget", "mc_service", "mc_applets"];

function readUleb(bytes: Uint8Array, at: number): { value: number; next: number } | null {
  let value = 0;
  let shift = 0;
  let cursor = at;
  while (cursor < bytes.length && shift <= 28) {
    const byte = bytes[cursor]!;
    cursor += 1;
    value |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) return { value, next: cursor };
    shift += 7;
  }
  return null;
}

function writeUleb(value: number, out: number[]): void {
  let n = value >>> 0;
  for (;;) {
    const byte = n & 0x7f;
    n >>>= 7;
    if (n !== 0) out.push(byte | 0x80);
    else {
      out.push(byte);
      return;
    }
  }
}

export function customSectionPayloads(wasm: Uint8Array, name: string): Uint8Array[] {
  if (wasm.length < 8 || wasm[0] !== 0 || wasm[1] !== 0x61 || wasm[2] !== 0x73 || wasm[3] !== 0x6d) {
    return [];
  }
  const found: Uint8Array[] = [];
  let cursor = 8;
  const wanted = new TextEncoder().encode(name);
  while (cursor < wasm.length) {
    const id = wasm[cursor]!;
    const size = readUleb(wasm, cursor + 1);
    if (!size) break;
    const body = size.next;
    const end = body + size.value;
    if (end > wasm.length) break;
    if (id === 0) {
      const nameLen = readUleb(wasm, body);
      if (nameLen) {
        const nameStart = nameLen.next;
        const nameEnd = nameStart + nameLen.value;
        if (nameEnd <= end && bytesEqual(wasm.subarray(nameStart, nameEnd), wanted)) {
          found.push(wasm.slice(nameEnd, end));
        }
      }
    }
    cursor = end;
  }
  return found;
}

export function profileFromPack(pack: Uint8Array): Uint8Array {
  const sections = customSectionPayloads(pack, PROFILE_SECTION);
  if (sections.length !== 1) {
    throw new Error("luauc pack is missing its luauc.runtime.v1 profile section");
  }
  return sections[0]!;
}

export function bytesEqual(left: Uint8Array, right: Uint8Array): boolean {
  if (left.length !== right.length) return false;
  for (let index = 0; index < left.length; index++) {
    if (left[index] !== right[index]) return false;
  }
  return true;
}

function appendCustom(name: string, payload: Uint8Array, out: number[]): void {
  const nameBytes = new TextEncoder().encode(name);
  const body: number[] = [];
  writeUleb(nameBytes.length, body);
  for (const byte of nameBytes) body.push(byte);
  for (const byte of payload) body.push(byte);
  out.push(0);
  writeUleb(body.length, out);
  for (const byte of body) out.push(byte);
}

function stripCustomSections(wasm: Uint8Array, names: string[]): Uint8Array {
  const drop = new Set(names);
  const out = Array.from(wasm.subarray(0, 8));
  let cursor = 8;
  while (cursor < wasm.length) {
    const id = wasm[cursor]!;
    const size = readUleb(wasm, cursor + 1);
    if (!size) {
      for (let index = cursor; index < wasm.length; index++) out.push(wasm[index]!);
      break;
    }
    const body = size.next;
    const end = body + size.value;
    if (end > wasm.length) {
      for (let index = cursor; index < wasm.length; index++) out.push(wasm[index]!);
      break;
    }
    let skip = false;
    if (id === 0) {
      const nameLen = readUleb(wasm, body);
      if (nameLen) {
        const nameStart = nameLen.next;
        const nameEnd = nameStart + nameLen.value;
        if (nameEnd <= end) {
          const sectionName = new TextDecoder().decode(wasm.subarray(nameStart, nameEnd));
          skip = drop.has(sectionName);
        }
      }
    }
    if (!skip) {
      for (let index = cursor; index < end; index++) out.push(wasm[index]!);
    }
    cursor = end;
  }
  return Uint8Array.from(out);
}

/** Append `mc_tier` and, when the budget is nonzero, `mc_budget`, matching `mc-stamp`. */
export function stampGuest(
  wasm: Uint8Array,
  tierName: string,
  mem: bigint,
  fuel: bigint,
  table: number,
): Uint8Array {
  const stripped = stripCustomSections(wasm, STAMP_NAMES);
  const out = Array.from(stripped);
  appendCustom("mc_tier", new TextEncoder().encode(tierName), out);
  if (mem !== 0n || fuel !== 0n || table !== 0) {
    const budget = new Uint8Array(24);
    const view = new DataView(budget.buffer);
    view.setUint32(0, 1, true);
    view.setBigUint64(4, mem, true);
    view.setBigUint64(12, fuel, true);
    view.setUint32(20, table, true);
    appendCustom("mc_budget", budget, out);
  }
  return Uint8Array.from(out);
}
