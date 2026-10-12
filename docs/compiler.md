# Compiler

AgentOS can compile a Luau package ahead of time. The guest command is `luau --compile`. It runs
`/bin/luau-compile`. The compiler stays in the host. Omit the `compiler` create option and
`luau-compile` reports that the compiler is not attached. `/bin/luau` stays the interpreter.

```js
const vm = await mc.create({
  compiler: true,
});
```

`compiler: true` resolves `luauc.wasm`, the runtime pack, and the runtime profile. The object form
passes those bytes directly:

```js
const vm = await mc.create({
  compiler: { engine, pack, profile },
});
```

`engine` is `luauc.wasm`. `pack` is `luauc-runtime.pack.wasm`. `profile` is the matching
`luauc-runtime.profile`. JavaScript instantiates the zero-import compiler. The served runtime
supervises the native compiler port. One engine serves one VM. `CompilerEngine.compile` is the
same compile for a caller with no VM.

## Command

`/bin/luau-compile` lives on the loom image, next to `/bin/luau` and `/bin/luau-analyze`.

```text
luau --compile -o <out> [--tier <name>] [--mem <n>] [--fuel <n>] [--table <n>] [--entry <name>] <file>...
luau-compile -o <out> [--tier <name>] [--mem <n>] [--fuel <n>] [--table <n>] [--entry <name>] <file>...
```

`luau --compile` spawns `luau-compile` and forwards its exit. One source file is the module named
by its stem, and that module is the entry. A trailing `.luau` is removed from the stem. Further
files are more modules. `--entry` selects a different entry. The output path is a filename. The
guest writes the stamped program there.

The host always stamps `mc_tier` and `mc_budget`. An omitted tier uses the VM boot tier, which is
`full` when the contract leaves the tier unset. An omitted budget field uses 16 MiB, 50 billion
fuel, or a table of 10,000. A provided tier must fit the VM. A provided budget is clamped to the
VM ceiling. Tier names are `full`, `read-write`, `read-only`, and `isolated`.

`help`, `--help`, and `-h` print usage and exit 0. A usage error exits 2.

## Artifacts

`CompilerEngine` loads one compiler and compiles a source package. `resolveLuaucEngine`,
`resolveLuaucPack`, and `resolveLuaucProfile` find the three artifacts. Resolution checks the
`MC_LUAUC_ENGINE`, `MC_LUAUC_PACK`, and `MC_LUAUC_PROFILE` paths, then the install directory, then
the local cache. The installer does not download these files.

The served runtime reads `AGENTOS_LUAUC_ENGINE` for the native port and the same pack and profile
variables. `AgentOS.ControlPlane.attach_compiler/2` attaches that port after create.
