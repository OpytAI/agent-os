// In-process luauc counters. Not Prometheus. The Elixir port keeps the same
// names in its own ETS table.

export type CompileHost = "wasm" | "port";
export type HostCallResult = "ok" | "detached" | "rejected";

const compileTotal = new Map<number, number>();
const durationSeconds = new Map<CompileHost, number>();
let guestBytes = 0;
let engineRestarts = 0;
const hostCalls = new Map<HostCallResult, number>();

function add(map: Map<number, number>, key: number, amount: number): void {
  map.set(key, (map.get(key) ?? 0) + amount);
}

export function incCompile(status: number): void {
  add(compileTotal, status, 1);
}

export function addDuration(host: CompileHost, seconds: number): void {
  durationSeconds.set(host, (durationSeconds.get(host) ?? 0) + seconds);
}

export function addGuestBytes(bytes: number): void {
  if (bytes > 0) guestBytes += bytes;
}

export function incRestart(): void {
  engineRestarts += 1;
}

export function incHostCall(result: HostCallResult): void {
  hostCalls.set(result, (hostCalls.get(result) ?? 0) + 1);
}

export function snapshot(): {
  compileTotal: Record<string, number>;
  durationSeconds: Record<string, number>;
  guestBytes: number;
  engineRestarts: number;
  hostCalls: Record<string, number>;
} {
  return {
    compileTotal: Object.fromEntries(compileTotal),
    durationSeconds: Object.fromEntries(durationSeconds),
    guestBytes,
    engineRestarts,
    hostCalls: Object.fromEntries(hostCalls),
  };
}

export function reset(): void {
  compileTotal.clear();
  durationSeconds.clear();
  guestBytes = 0;
  engineRestarts = 0;
  hostCalls.clear();
}

export function logCompile(fields: {
  host: CompileHost;
  requestId: string;
  status: number;
  durationMs: number;
  requestBytes: number;
  guestBytes: number;
  diagnosticBytes: number;
  moduleCount: number;
}): void {
  console.info(
    `luauc compile host=${fields.host} request_id=${fields.requestId} status=${fields.status} duration_ms=${fields.durationMs} request_bytes=${fields.requestBytes} guest_bytes=${fields.guestBytes} diagnostic_bytes=${fields.diagnosticBytes} module_count=${fields.moduleCount}`,
  );
}

export function hexBytes(bytes: Uint8Array): string {
  let out = "";
  for (const byte of bytes) out += byte.toString(16).padStart(2, "0");
  return out;
}
