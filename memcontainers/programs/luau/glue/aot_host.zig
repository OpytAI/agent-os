//! AgentOS capabilities for strict native Luau packages.
//!
//! Luauc supplies the compiler, generated-code runtime, and pinned Luau VM. This archive publishes
//! the real AgentOS native libraries the compiled guest can read through package.loaded.

const lua = @import("lua.zig");
const c = lua.c;
const State = lua.State;

extern fn mc_open_sys(state: ?*State) void;
extern fn mc_open_json(state: ?*State) c_int;
extern fn mc_open_hash(state: ?*State) c_int;
extern fn mc_open_encoding(state: ?*State) c_int;
extern fn mc_open_deflate(state: ?*State) c_int;
extern fn mc_open_re(state: ?*State) c_int;

comptime {
    _ = @import("trap.zig");
    _ = @import("sys.zig");
    _ = @import("json.zig");
    _ = @import("hash.zig");
    _ = @import("encoding.zig");
    _ = @import("deflate.zig");
    _ = @import("re.zig");
    _ = @import("wasi_shim.zig");
}

fn storeNative(state: ?*State, name: [*:0]const u8, open: *const fn (?*State) callconv(.c) c_int) void {
    _ = open(state);
    c.lua_setfield(state, -2, name);
}

pub export fn mc_open_aot_host(state: ?*State) void {
    mc_open_sys(state);

    lua.newtable(state);
    lua.newtable(state);
    lua.getglobal(state, "sys");
    c.lua_setfield(state, -2, "sys");
    storeNative(state, "json", &mc_open_json);
    storeNative(state, "hash", &mc_open_hash);
    storeNative(state, "encoding", &mc_open_encoding);
    storeNative(state, "deflate", &mc_open_deflate);
    storeNative(state, "re", &mc_open_re);
    c.lua_setfield(state, -2, "loaded");
    lua.setglobal(state, "package");

    lua.getglobal(state, "package");
    _ = c.lua_getfield(state, -1, "loaded");
    _ = c.lua_getfield(state, -1, "json");
    lua.setglobal(state, "json");
    lua.pop(state, 2);
}
