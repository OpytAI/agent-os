//! Project twigz pack registry.json into AgentOS registry.zig.

const std = @import("std");

const Role = struct { field_id: u16, semantic_id: u32 };
const Slice = struct { start: u32, count: u16 };
const NONE: u32 = std.math.maxInt(u32);

fn asInt(v: std.json.Value) !u64 {
    return switch (v) {
        .integer => |x| if (x >= 0) @intCast(x) else error.Invalid,
        .number_string => |s| std.fmt.parseInt(u64, s, 10),
        else => error.Invalid,
    };
}

fn asStr(v: std.json.Value) ![]const u8 {
    return switch (v) {
        .string => |s| s,
        else => error.Invalid,
    };
}

fn internU32(gpa: std.mem.Allocator, pool: *std.ArrayList(u32), interned: *std.ArrayList(Slice), items: []const u32) !Slice {
    for (interned.items) |s| {
        if (std.mem.eql(u32, pool.items[s.start..][0..s.count], items)) return s;
    }
    const start: u32 = @intCast(pool.items.len);
    try pool.appendSlice(gpa, items);
    const s = Slice{ .start = start, .count = @intCast(items.len) };
    try interned.append(gpa, s);
    return s;
}

fn rolesEql(a: []const Role, b: []const Role) bool {
    if (a.len != b.len) return false;
    for (a, b) |l, r| {
        if (l.field_id != r.field_id or l.semantic_id != r.semantic_id) return false;
    }
    return true;
}

fn internRoles(gpa: std.mem.Allocator, pool: *std.ArrayList(Role), interned: *std.ArrayList(Slice), items: []const Role) !Slice {
    for (interned.items) |s| {
        if (rolesEql(pool.items[s.start..][0..s.count], items)) return s;
    }
    const start: u32 = @intCast(pool.items.len);
    try pool.appendSlice(gpa, items);
    const s = Slice{ .start = start, .count = @intCast(items.len) };
    try interned.append(gpa, s);
    return s;
}

pub fn project(gpa: std.mem.Allocator, json_bytes: []const u8) ![]u8 {
    const parsed = try std.json.parseFromSlice(std.json.Value, gpa, json_bytes, .{});
    const root = parsed.value.object;
    const lang_arr = (root.get("languages") orelse return error.Invalid).array;

    var trait_pool: std.ArrayList(u32) = .empty;
    var trait_interned: std.ArrayList(Slice) = .empty;
    var role_pool: std.ArrayList(Role) = .empty;
    var role_interned: std.ArrayList(Slice) = .empty;

    const Entry = struct {
        semantic_id: u32,
        trait_start: u32,
        trait_count: u16,
        role_start: u32,
        role_count: u16,
    };
    const Lang = struct {
        name: []const u8,
        language_version: []const u8,
        grammar_version: []const u8,
        grammar_ir_version: u64,
        vocabulary_version: u64,
        tree_sitter_abi: u64,
        entries: []const Entry,
    };
    var langs: std.ArrayList(Lang) = .empty;

    for (lang_arr.items) |lang_v| {
        const obj = lang_v.object;
        const name = try asStr(obj.get("name") orelse return error.Invalid);
        const symbols = (obj.get("symbols") orelse return error.Invalid).array;
        var max_id: usize = 0;
        for (symbols.items) |sym_v| {
            const id: usize = @intCast(try asInt(sym_v.object.get("id") orelse return error.Invalid));
            if (id > max_id) max_id = id;
        }
        const n = if (symbols.items.len == 0) 0 else max_id + 1;
        var semantic = try gpa.alloc(u32, n);
        var trait_ids = try gpa.alloc([]u32, n);
        var role_ids = try gpa.alloc([]Role, n);
        for (0..n) |i| {
            semantic[i] = NONE;
            trait_ids[i] = &.{};
            role_ids[i] = &.{};
        }
        for (symbols.items) |sym_v| {
            const s = sym_v.object;
            const id: usize = @intCast(try asInt(s.get("id") orelse return error.Invalid));
            semantic[id] = switch (s.get("semantic_id") orelse return error.Invalid) {
                .null => NONE,
                else => |v| @intCast(try asInt(v)),
            };
            const traits_v = (s.get("traits") orelse return error.Invalid).array;
            const tbuf = try gpa.alloc(u32, traits_v.items.len);
            for (traits_v.items, 0..) |t, i| tbuf[i] = @intCast(try asInt(t));
            trait_ids[id] = tbuf;
            const roles_v = (s.get("roles") orelse return error.Invalid).array;
            const rbuf = try gpa.alloc(Role, roles_v.items.len);
            for (roles_v.items, 0..) |r, i| {
                const ro = r.object;
                rbuf[i] = .{
                    .field_id = @intCast(try asInt(ro.get("field_id") orelse return error.Invalid)),
                    .semantic_id = @intCast(try asInt(ro.get("semantic_id") orelse return error.Invalid)),
                };
            }
            role_ids[id] = rbuf;
        }

        const entries = try gpa.alloc(Entry, n);
        for (0..n) |i| {
            const t = try internU32(gpa, &trait_pool, &trait_interned, trait_ids[i]);
            const r = try internRoles(gpa, &role_pool, &role_interned, role_ids[i]);
            entries[i] = .{
                .semantic_id = semantic[i],
                .trait_start = t.start,
                .trait_count = t.count,
                .role_start = r.start,
                .role_count = r.count,
            };
        }
        try langs.append(gpa, .{
            .name = name,
            .language_version = try asStr(obj.get("language_version") orelse return error.Invalid),
            .grammar_version = try asStr(obj.get("grammar_version") orelse return error.Invalid),
            .grammar_ir_version = try asInt(obj.get("grammar_ir_version") orelse return error.Invalid),
            .vocabulary_version = try asInt(obj.get("vocabulary_version") orelse return error.Invalid),
            .tree_sitter_abi = try asInt(obj.get("tree_sitter_abi") orelse return error.Invalid),
            .entries = entries,
        });
    }

    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(gpa, "// @generated by syntax-registry-zig; do not edit.\n");
    try out.appendSlice(gpa, "const std = @import(\"std\");\n");
    try out.appendSlice(gpa, "const wire = @import(\"syntax_zig\");\n");
    try out.appendSlice(gpa, "pub const NONE_SEMANTIC: u32 = std.math.maxInt(u32);\n");
    try out.appendSlice(gpa, "pub const Role = struct { field_id: u16, semantic_id: u32 };\n");
    try out.appendSlice(gpa, "pub const Entry = struct { semantic_id: u32, trait_start: u32, trait_count: u16, role_start: u32, role_count: u16 };\n");
    try out.appendSlice(gpa, "pub const Map = struct { language_version: []const u8, grammar_version: []const u8, grammar_ir_version: u32, vocabulary_version: u32, tree_sitter_abi: u32, entries: []const Entry };\n");
    try out.appendSlice(gpa, "pub const Descriptor = struct { name: []const u8, semantic: *const Map };\n");
    try out.appendSlice(gpa, "pub const traits = [_]wire.SemanticTrait{\n");
    for (trait_pool.items) |id| try out.print(gpa, "    .{{ .id = {d} }},\n", .{id});
    try out.appendSlice(gpa, "};\npub const roles = [_]Role{\n");
    for (role_pool.items) |role| try out.print(gpa, "    .{{ .field_id = {d}, .semantic_id = {d} }},\n", .{ role.field_id, role.semantic_id });
    try out.appendSlice(gpa, "};\n");
    for (langs.items) |lang| {
        try out.print(gpa, "const {s}_entries = [_]Entry{{\n", .{lang.name});
        for (lang.entries) |e| {
            try out.print(gpa, "    .{{ .semantic_id = {d}, .trait_start = {d}, .trait_count = {d}, .role_start = {d}, .role_count = {d} }},\n", .{
                e.semantic_id, e.trait_start, e.trait_count, e.role_start, e.role_count,
            });
        }
        try out.print(gpa, "}};\nconst {s}_map = Map{{ .language_version = \"{s}\", .grammar_version = \"{s}\", .grammar_ir_version = {d}, .vocabulary_version = {d}, .tree_sitter_abi = {d}, .entries = &{s}_entries }};\npub extern fn tree_sitter_{s}() ?*const anyopaque;\n", .{
            lang.name,
            lang.language_version,
            lang.grammar_version,
            lang.grammar_ir_version,
            lang.vocabulary_version,
            lang.tree_sitter_abi,
            lang.name,
            lang.name,
        });
    }
    try out.appendSlice(gpa, "pub const descriptors = [_]Descriptor{\n");
    for (langs.items) |lang| {
        try out.print(gpa, "    .{{ .name = \"{s}\", .semantic = &{s}_map }},\n", .{ lang.name, lang.name });
    }
    try out.appendSlice(gpa, "};\npub fn language(comptime c: type, name: []const u8) ?*const c.TSLanguage {\n");
    for (langs.items) |lang| {
        try out.print(gpa, "    if (std.mem.eql(u8, name, \"{s}\")) return @ptrCast(tree_sitter_{s}());\n", .{ lang.name, lang.name });
    }
    try out.appendSlice(gpa, "    return null;\n}\n");
    try out.appendSlice(gpa, "pub fn descriptor(name: []const u8) ?*const Descriptor {\n    for (&descriptors) |*item| if (std.mem.eql(u8, item.name, name)) return item;\n    return null;\n}\n");
    try out.appendSlice(gpa, "pub fn entry(map: *const Map, symbol: u16) ?*const Entry {\n    if (symbol >= map.entries.len) return null;\n    const value = &map.entries[symbol];\n    return if (value.semantic_id == NONE_SEMANTIC) null else value;\n}\n");
    try out.appendSlice(gpa, "pub fn entryTraits(value: *const Entry) []const wire.SemanticTrait { return traits[value.trait_start..][0..value.trait_count]; }\n");
    try out.appendSlice(gpa, "pub fn entryRole(value: *const Entry, field_id: u16) ?u32 {\n    const values = roles[value.role_start..][0..value.role_count];\n    var low: usize = 0; var high: usize = values.len;\n    while (low < high) { const mid = low + (high - low) / 2; if (values[mid].field_id < field_id) low = mid + 1 else high = mid; }\n    return if (low < values.len and values[low].field_id == field_id) values[low].semantic_id else null;\n}\n");
    return out.items;
}

test "projects two languages and interns shared traits" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const json =
        \\{"languages":[
        \\ {"name":"lua","language_version":"5.4.0","grammar_version":"1","grammar_ir_version":2,"vocabulary_version":2,"tree_sitter_abi":14,
        \\  "symbols":[
        \\   {"id":0,"semantic_id":1,"traits":[1],"roles":[]},
        \\   {"id":1,"semantic_id":22,"traits":[1],"roles":[{"field_id":1,"semantic_id":7}]}
        \\  ]},
        \\ {"name":"luau","language_version":"0.725.0","grammar_version":"1","grammar_ir_version":2,"vocabulary_version":2,"tree_sitter_abi":14,
        \\  "symbols":[
        \\   {"id":0,"semantic_id":1,"traits":[1],"roles":[]}
        \\  ]}
        \\]}
    ;
    const out = try project(arena.allocator(), json);
    try std.testing.expect(std.mem.indexOf(u8, out, "pub extern fn tree_sitter_lua()") != null);
    try std.testing.expect(std.mem.indexOf(u8, out, "pub extern fn tree_sitter_luau()") != null);
    try std.testing.expect(std.mem.indexOf(u8, out, ".{ .id = 1 }") != null);
    var trait_count: usize = 0;
    var i: usize = 0;
    while (std.mem.indexOfPos(u8, out, i, ".{ .id = 1 }")) |at| {
        trait_count += 1;
        i = at + 1;
    }
    try std.testing.expectEqual(@as(usize, 1), trait_count);
    try std.testing.expect(std.mem.indexOf(u8, out, ".{ .name = \"lua\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, out, ".{ .name = \"luau\"") != null);
}

test "invalid json fails" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    try std.testing.expectError(error.SyntaxError, project(arena.allocator(), "not-json"));
}
