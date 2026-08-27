//! /bin mcbox — Zig multicall over `@utilz` with mc `sys.Impl` attach.

const std = @import("std");
const agent_sys = @import("sys");
const utilz = @import("utilz");
const mc_impl = @import("mc_impl.zig");

pub const panic = std.debug.FullPanic(struct {
    pub fn panic(msg: []const u8, first_trace_addr: ?usize) noreturn {
        _ = first_trace_addr;
        utilz.sys.writeAll(utilz.sys.STDERR, "mcbox: panic: ") catch {};
        utilz.sys.writeAll(utilz.sys.STDERR, msg) catch {};
        utilz.sys.writeAll(utilz.sys.STDERR, "\n") catch {};
        utilz.sys.exit(127);
    }
}.panic);

fn basenameOf(path: []const u8) []const u8 {
    var idx: usize = 0;
    var i: usize = path.len;
    while (i > 0) {
        i -= 1;
        if (path[i] == '/' or path[i] == '\\') {
            idx = i + 1;
            break;
        }
    }
    return path[idx..];
}

const applet_names_joined: []const u8 = blk: {
    var buf: []const u8 = "";
    for (utilz.registry.box, 0..) |a, i| {
        if (i != 0) buf = buf ++ ", ";
        buf = buf ++ a.name;
    }
    break :blk buf;
};

var arena_state: std.heap.ArenaAllocator = undefined;

fn dispatch() noreturn {
    utilz.sys.init();
    arena_state = std.heap.ArenaAllocator.init(agent_sys.wasm_allocator);
    const gpa = arena_state.allocator();
    const argv = utilz.sys.argsAlloc(gpa) catch &.{};

    var ctx = utilz.Ctx{
        .args = argv,
        .gpa = gpa,
        .stdin = utilz.sys.STDIN,
        .stdout = utilz.sys.STDOUT,
        .stderr = utilz.sys.STDERR,
    };

    if (argv.len == 0) utilz.sys.exit(2);

    const bundle_name = basenameOf(argv[0]);
    if (utilz.registry.find(bundle_name)) |applet| {
        utilz.sys.exit(applet.run(&ctx));
    }

    if (argv.len >= 2) {
        const name = basenameOf(argv[1]);
        if (utilz.registry.find(name)) |applet| {
            ctx.args = argv[1..];
            utilz.sys.exit(applet.run(&ctx));
        }
        ctx.errPrint("{s}: applet not in this box\n", .{name});
        utilz.sys.exit(127);
    }

    ctx.errPrint("mcbox: usage: <applet> [args...]  (applets: {s})\n", .{applet_names_joined});
    utilz.sys.exit(2);
}

pub export fn _start() void {
    mc_impl.attach();
    dispatch();
}
