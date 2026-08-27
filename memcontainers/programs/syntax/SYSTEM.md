# Syntax platform

`syntax` is AgentOS's owned structural parsing stack. Grammar compilation is a twigz host
action; parsing and edits happen inside the guest through one lazy resident service.

## Boundaries

The source of truth is split deliberately:

- `contracts/syntax.kdl` owns protocol messages and the versioned semantic vocabulary. The contract
  projector generates Rust, Zig, and Luau codecs/constants; consumers do not copy wire IDs.
- `@twigz` owns `.grammar` authoring, scanners as `scan` productions, pack C, and the Tree-sitter
  runtime filegroups. AgentOS packs lua+luau only via `twigz_pack`.
- `bazel/tools/syntax-registry-zig` projects pack `registry.json` into `registry.zig`
  (`extern fn tree_sitter_*`, interned `syntax_zig` traits). twigz does not emit Zig.
- `glue/` links the generic C runtime, generated parsers, and generated scanners behind Zig service
  lifecycle and resource policy. `/lib/luau/syntax.luau` is the typed guest client.

The lossless concrete syntax tree remains language-specific. Host-side semantic IR projects concrete
nodes and fields onto the shared vocabulary; the packer compiles that projection into immutable tables
indexed by Tree-sitter symbol and field IDs. Semantic identity is never inferred from coincidental
node spelling, and the guest neither ships nor parses semantic JSON.

## Build and runtime flow

```text
syntax.kdl -> contract projector -> generated Zig/Luau/Rust wire APIs
@twigz lua+luau grammars -> twigz_pack -> parser C + scanner C + registry.json
registry.json -> syntax-registry-zig -> registry.zig
parser pack + Tree-sitter C runtime + generated scanners -> /bin/syntax
/bin/syntax + syntax.luau -> loom image
```

Each language remains an independent Tree-sitter automaton. The packer deterministically renumbers
implementation IDs, then interns only byte-identical action lists and small parse-table rows across
the finished automata. It does not merge grammar states or broaden either language. The guest
consumes packed C parsers/scanners and `registry.json`; it does not ship semantic JSON.

The service owns parser instances, source buffers, trees, queries, and document revisions in guest
linear memory. Handles are session-owned. Node handles are monotonic and never recycled within a
document, then invalidated by edits; document and query handles fail closed after close/session teardown.
Edits validate ranges and overlap before mutation, apply to a copied tree, incrementally reparse, and
commit atomically. Guarded rewrites additionally verify SHA-256 digests and a syntax-error policy.

Hard limits bound source/query sizes, open documents, traversal/query pages, guest memory, fuel, and
table entries. The service uses the `isolated` tier (read-only VFS access, no ambient authority) and is
lazily activated inside `loom`, so every programmable image has structural parsing while resident
memory and startup are paid only after first use.

## Verification

- Twigz `//grammars:format_test` keeps first-party grammars canonical.
- `//memcontainers/contracts:syntax_{rust,zig,luau}_sync_test` prevent checked-in projection drift.
- `//memcontainers/programs/syntax/glue:size_limit` holds the optimized service at 400000 bytes.
- `//memcontainers/tests/e2e:core --test_arg=syntax` crosses the real kernel, lazy service,
  generated Luau codec, C runtime, Zig glue, queries, incremental edits, guarded rewrites, stale
  handles, Lua long brackets, and quoted-string kind 22.

Generated parser sources are implementation artifacts, never the public API. Changing the protocol or
semantic IDs starts in `syntax.kdl`; changing a grammar starts in twigz.
