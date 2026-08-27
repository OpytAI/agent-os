//! Project twigz pack registry.json into AgentOS registry.zig.

const std = @import("std");
const project = @import("project.zig");

pub fn main(init: std.process.Init) !void {
    const gpa = init.arena.allocator();
    var it = std.process.Args.Iterator.init(init.minimal.args);
    const argv0 = it.next() orelse "syntax-registry-zig";
    _ = argv0;
    const in_path = it.next() orelse {
        std.debug.print("usage: syntax-registry-zig <registry.json> <out.zig>\n", .{});
        std.process.exit(2);
    };
    const out_path = it.next() orelse {
        std.debug.print("usage: syntax-registry-zig <registry.json> <out.zig>\n", .{});
        std.process.exit(2);
    };

    const json_bytes = try std.Io.Dir.cwd().readFileAlloc(init.io, in_path, gpa, .unlimited);
    const zig_src = try project.project(gpa, json_bytes);
    const file = try std.Io.Dir.cwd().createFile(init.io, out_path, .{});
    defer file.close(init.io);
    try file.writeStreamingAll(init.io, zig_src);
}
