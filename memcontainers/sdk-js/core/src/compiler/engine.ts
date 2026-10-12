// One luauc.wasm context for the life of the engine. compile returns compiler
// statuses 0..7; it does not throw for those statuses.

import { resolveLuaucEngine, resolveLuaucPack } from "../artifacts.js";
import { addDuration, addGuestBytes, hexBytes, incCompile, logCompile } from "./metrics.js";
import { bytesEqual, customSectionPayloads, profileFromPack } from "./wasm.js";

const PROFILE_SECTION = "luauc.runtime.v1";

interface LuaucExports {
  memory: WebAssembly.Memory;
  luauc_v1_alloc(size: number): number;
  luauc_v1_dealloc(pointer: number, size: number): void;
  luauc_v1_context_create(
    profilePointer: number,
    profileSize: number,
    packPointer: number,
    packSize: number,
    resultPointer: number,
  ): number;
  luauc_v1_context_destroy(handle: number): void;
  luauc_v1_compile(
    handle: number,
    requestPointer: number,
    requestSize: number,
    resultPointer: number,
  ): number;
  luauc_v1_result_free(resultPointer: number): void;
  luauc_v1_describe?(pointer: number): number;
}

export interface CompilerLoadOptions {
  engine?: Uint8Array;
  pack?: Uint8Array;
  profile?: Uint8Array;
}

export interface CompilerCompileResult {
  status: number;
  diagnostic: Uint8Array;
  guest: Uint8Array;
}

function copyBytes(bytes: Uint8Array): Uint8Array {
  return bytes.slice();
}

function alloc(api: LuaucExports, size: number): number {
  const pointer = api.luauc_v1_alloc(size);
  if (!pointer) throw new Error(`luauc allocation failed for ${size} bytes`);
  return pointer;
}

function readBytes(api: LuaucExports, pointer: number, size: number): Uint8Array {
  return copyBytes(new Uint8Array(api.memory.buffer, pointer, size));
}

export class CompilerEngine {
  readonly profileDigest: Uint8Array;
  readonly packDigest: Uint8Array;
  private readonly api: LuaucExports;
  private handle: number;
  private compileResultSize: number;
  private closed = false;

  private constructor(api: LuaucExports, handle: number, profileDigest: Uint8Array, packDigest: Uint8Array) {
    this.api = api;
    this.handle = handle;
    this.profileDigest = profileDigest;
    this.packDigest = packDigest;
    this.compileResultSize = 320;
  }

  static async load(opts: CompilerLoadOptions = {}): Promise<CompilerEngine> {
    const engineBytes = opts.engine ?? (await resolveLuaucEngine());
    const pack = opts.pack ?? (await resolveLuaucPack());
    const sections = customSectionPayloads(pack, PROFILE_SECTION);
    if (sections.length > 1) {
      throw new Error("luauc pack has more than one luauc.runtime.v1 profile section");
    }
    let profile = opts.profile;
    if (profile && sections.length === 1 && !bytesEqual(profile, sections[0]!)) {
      throw new Error("luauc profile does not match the pack's luauc.runtime.v1 section");
    }
    if (!profile) profile = sections.length === 1 ? profileFromPack(pack) : new Uint8Array();

    const wasmModule = new WebAssembly.Module(engineBytes);
    if (WebAssembly.Module.imports(wasmModule).length !== 0) {
      throw new Error("luauc.wasm has imports");
    }
    const instance = new WebAssembly.Instance(wasmModule, {});
    const api = instance.exports as unknown as LuaucExports;
    let profilePointer = 0;
    let packPointer = 0;
    let contextPointer = 0;
    let handle = 0;
    try {
      profilePointer = alloc(api, Math.max(profile.length, 1));
      packPointer = alloc(api, Math.max(pack.length, 1));
      contextPointer = alloc(api, 72);
      if (profile.length > 0) new Uint8Array(api.memory.buffer, profilePointer, profile.length).set(profile);
      if (pack.length > 0) new Uint8Array(api.memory.buffer, packPointer, pack.length).set(pack);
      new Uint8Array(api.memory.buffer, contextPointer, 72).fill(0);
      const fnStatus = api.luauc_v1_context_create(
        profilePointer,
        profile.length,
        packPointer,
        pack.length,
        contextPointer,
      );
      const view = new DataView(api.memory.buffer, contextPointer, 72);
      const fieldStatus = view.getUint32(4, true);
      const status = fnStatus || fieldStatus;
      if (status !== 0) {
        throw new Error(`luauc context creation failed with ${status}`);
      }
      handle = view.getUint32(0, true);
      const profileDigest = readBytes(api, contextPointer + 8, 32);
      const packDigest = readBytes(api, contextPointer + 40, 32);
      const engine = new CompilerEngine(api, handle, profileDigest, packDigest);
      engine.compileResultSize = describeCompileResultSize(api);
      return engine;
    } catch (error) {
      if (handle !== 0) api.luauc_v1_context_destroy(handle);
      throw error;
    } finally {
      if (contextPointer !== 0) api.luauc_v1_dealloc(contextPointer, 72);
      if (packPointer !== 0) api.luauc_v1_dealloc(packPointer, Math.max(pack.length, 1));
      if (profilePointer !== 0) api.luauc_v1_dealloc(profilePointer, Math.max(profile.length, 1));
    }
  }

  async compile(sourcePackage: Uint8Array): Promise<CompilerCompileResult> {
    if (this.closed || this.handle === 0) throw new Error("luauc engine is closed");
    const started = performance.now();
    const api = this.api;
    const requestPointer = alloc(api, sourcePackage.length);
    let resultPointer = 0;
    try {
      resultPointer = alloc(api, this.compileResultSize);
      new Uint8Array(api.memory.buffer, requestPointer, sourcePackage.length).set(sourcePackage);
      new Uint8Array(api.memory.buffer, resultPointer, this.compileResultSize).fill(0);
      const fnStatus = api.luauc_v1_compile(this.handle, requestPointer, sourcePackage.length, resultPointer);
      const view = new DataView(api.memory.buffer, resultPointer, this.compileResultSize);
      const fieldStatus = view.getUint32(16, true);
      const status = (fnStatus || fieldStatus) >>> 0;
      const diagnosticPointer = view.getUint32(8, true);
      const diagnosticSize = view.getUint32(12, true);
      const diagnostic =
        diagnosticPointer !== 0 && diagnosticSize !== 0
          ? readBytes(api, diagnosticPointer, diagnosticSize)
          : new Uint8Array();
      const dataPointer = view.getUint32(0, true);
      const dataSize = view.getUint32(4, true);
      const guest = status === 0 && dataPointer !== 0 && dataSize !== 0 ? readBytes(api, dataPointer, dataSize) : new Uint8Array();
      const durationMs = Math.max(0, Math.round(performance.now() - started));
      incCompile(status);
      addDuration("wasm", durationMs / 1000);
      addGuestBytes(guest.length);
      logCompile({
        host: "wasm",
        requestId: sourcePackage.length >= 48 ? hexBytes(sourcePackage.subarray(32, 48)) : "-",
        status,
        durationMs,
        requestBytes: sourcePackage.length,
        guestBytes: guest.length,
        diagnosticBytes: diagnostic.length,
        moduleCount: sourcePackage.length >= 20 ? new DataView(sourcePackage.buffer, sourcePackage.byteOffset, sourcePackage.byteLength).getUint32(16, true) : 0,
      });
      return { status, diagnostic, guest };
    } finally {
      if (resultPointer !== 0) {
        api.luauc_v1_result_free(resultPointer);
        api.luauc_v1_dealloc(resultPointer, this.compileResultSize);
      }
      api.luauc_v1_dealloc(requestPointer, sourcePackage.length);
    }
  }

  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;
    if (this.handle !== 0) {
      this.api.luauc_v1_context_destroy(this.handle);
      this.handle = 0;
    }
  }
}

function describeCompileResultSize(api: LuaucExports): number {
  if (!api.luauc_v1_describe) return 320;
  const pointer = alloc(api, 32);
  try {
    if (api.luauc_v1_describe(pointer) !== 0) return 320;
    const size = new DataView(api.memory.buffer, pointer, 32).getUint32(12, true);
    return size > 0 ? size : 320;
  } finally {
    api.luauc_v1_dealloc(pointer, 32);
  }
}
