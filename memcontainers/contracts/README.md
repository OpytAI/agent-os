# contracts — projected boundaries

This directory is the **single source of truth** for every boundary in AgentOS
(SYSTEMS.md). Neither the kernel, hosts, shims, nor clients _is_ the truth — the
contract is — so they cannot drift.

| File            | Boundary       | Direction            | Module                           |
| --------------- | -------------- | -------------------- | -------------------------------- |
| `syscalls.kdl`  | syscall        | guest → kernel       | `mc`                             |
| `bridge.kdl`    | bridge         | kernel → host        | `env`                            |
| `control.kdl`   | control        | host → kernel        | `mc_ctl_*`                       |
| `wire.kdl`      | wire           | server ↔ client      | —                                |
| `constants.kdl` | shared         | —                    | errno, tiers, flags, ABI version (merged with `@shcore//:shell_abi.kdl`) |
| `snapshot.kdl`  | snapshot value | host ↔ host/store    | MCSN v2                          |
| `llb.kdl`       | build graph    | client ↔ solver      | LLB                              |
| `syntax.kdl`    | syntax service | Luau ↔ `/svc/syntax` | framed messages                  |
| `git.kdl`       | host git remotes | dual-host orch decisions | stderr / depth / pack defaults |

The syscall values originate in the frozen `mc` ABI. **Do not renumber them.** New AgentOS-native
boundaries such as MCSN, LLB, and syntax are owned here directly and change through their declared
version policy.

## How a contract becomes code

```
contracts/*.kdl ──(//contracts/codegen:projector)──> gen/*.rs
                                                ├─> gen/*.zig
                                                ├─> gen/*.ts / *.ex / *.luau
                                                └─> OpenAPI / AsyncAPI / Markdown
```

`abi_library` (`codegen/defs.bzl`) runs the projector once per language and wires a
`write_source_files` drift gate. A stale checked-in projection is a failed
`diff_test` (B2) — in every language at once.

## Adding or changing a syscall

1. Edit **one line** in `syscalls.kdl` (or bump `abi-version` minor in
   `constants.kdl` for an additive change).
2. `bazel test //...` regenerates every projection. The Rust kernel's exhaustive
   `match` fails to compile until it has a handler — drift is a compile error.
3. Conformance fails until a guest exercises the new syscall (or it carries a
   documented exclusion).

## Status

The `.kdl` files are authoritative. The projector (`codegen/src/projector.rs`)
emits Rust, Zig, TypeScript, Elixir, Luau, Markdown, AsyncAPI, and OpenAPI.
`abi_library()` compile-validates the Rust/Zig projections and drift-gates every
language with `diff_test`. Consume them as `//memcontainers/contracts:mc_rust`,
`:env_zig`, `:wire_ts`, …

The generated Rust is a `macro_rules!` callback table; the kernel, host, and
sysroot supply `$emit`, so no ABI is hand-written (B2).
