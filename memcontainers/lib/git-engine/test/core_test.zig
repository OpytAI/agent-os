const std = @import("std");
const contract = @import("git_zig");
const core = @import("core");
const Browser = @import("browser_backend").Browser;

test "generated envelope decoders are fuzzable hostile-input boundaries" {
    try std.testing.fuzz({}, fuzzEnvelopes, .{});
}

fn fuzzEnvelopes(_: void, smith: *std.testing.Smith) !void {
    var buffer: [4096]u8 = undefined;
    const length = smith.slice(&buffer);
    const bytes = buffer[0..length];
    if (contract.decodeRequestEnvelope(bytes)) |_| {} else |_| {}
    if (contract.decodeResponseEnvelope(bytes)) |_| {} else |_| {}
}

test "session and result handles are generational" {
    var engine = core.Engine(Browser).init(std.testing.allocator, .{});
    defer engine.deinit();
    const config = try (contract.SessionConfig{
        .backend = @intCast(contract.BACKEND_BROWSER),
        .read_only = false,
        .root = "",
    }).encode(std.testing.allocator);
    defer std.testing.allocator.free(config);

    const opened = engine.sessionOpen(config, 7);
    try std.testing.expect(opened != 0);
    const length = engine.resultLen(opened);
    try std.testing.expect(length > contract.ENVELOPE_HEADER_BYTES);
    try std.testing.expectEqual(@as(u32, 0), engine.resultFree(opened));
    try std.testing.expectEqual(@as(u32, 0), engine.resultLen(opened));
    try std.testing.expectEqual(@as(u32, 1), engine.resultFree(opened));
}

test "commit object id is the resolved HEAD hash" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);

    const init_request = try requestEnvelope(allocator, @intCast(contract.OP_REPOSITORY_INIT), 10, &.{});
    defer allocator.free(init_request);
    const initialized = try takeResult(&engine, allocator, engine.execute(session, init_request));
    defer allocator.free(initialized);
    try expectEnvelope(initialized, @intCast(contract.OP_REPOSITORY_INIT), @intCast(contract.STATUS_OK), 10);

    const write_payload = try (contract.FileRequest{ .path = "tracked.txt", .data = "stable object id" }).encode(allocator);
    defer allocator.free(write_payload);
    const write_request = try requestEnvelope(allocator, @intCast(contract.OP_FILE_WRITE), 11, write_payload);
    defer allocator.free(write_request);
    const written = try takeResult(&engine, allocator, engine.execute(session, write_request));
    defer allocator.free(written);
    try expectEnvelope(written, @intCast(contract.OP_FILE_WRITE), @intCast(contract.STATUS_OK), 11);

    const paths = [_]contract.StringPair{.{ .key = "tracked.txt", .value = "" }};
    const add_payload = try (contract.PorcelainRequest{ .action = contract.ACTION_UPDATE, .flags = 0, .paths = &paths }).encode(allocator);
    defer allocator.free(add_payload);
    const add_request = try requestEnvelope(allocator, @intCast(contract.OP_ADD), 12, add_payload);
    defer allocator.free(add_request);
    const added = try takeResult(&engine, allocator, engine.execute(session, add_request));
    defer allocator.free(added);
    try expectEnvelope(added, @intCast(contract.OP_ADD), @intCast(contract.STATUS_OK), 12);

    const identity = contract.Signature{ .name = "AgentOS", .email = "agentos@example.test", .unix_seconds = 1_700_000_000, .timezone_minutes = 0 };
    const commit_payload = try (contract.PorcelainRequest{ .action = contract.ACTION_CREATE, .flags = 0, .message = "initial", .paths = &.{}, .author = identity, .committer = identity }).encode(allocator);
    defer allocator.free(commit_payload);
    const commit_request = try requestEnvelope(allocator, @intCast(contract.OP_COMMIT), 13, commit_payload);
    defer allocator.free(commit_request);
    const committed_bytes = try takeResult(&engine, allocator, engine.execute(session, commit_request));
    defer allocator.free(committed_bytes);
    try expectEnvelope(committed_bytes, @intCast(contract.OP_COMMIT), @intCast(contract.STATUS_OK), 13);
    const committed = try contract.CommitResult.decode(allocator, committed_bytes[contract.ENVELOPE_HEADER_BYTES..]);

    const resolve_payload = try (contract.PorcelainRequest{ .action = contract.ACTION_GET, .flags = 0, .revision = "HEAD", .paths = &.{} }).encode(allocator);
    defer allocator.free(resolve_payload);
    const resolve_request = try requestEnvelope(allocator, @intCast(contract.OP_RESOLVE_REVISION), 14, resolve_payload);
    defer allocator.free(resolve_request);
    const resolved_bytes = try takeResult(&engine, allocator, engine.execute(session, resolve_request));
    defer allocator.free(resolved_bytes);
    try expectEnvelope(resolved_bytes, @intCast(contract.OP_RESOLVE_REVISION), @intCast(contract.STATUS_OK), 14);
    const resolved = try contract.ResolveResult.decode(allocator, resolved_bytes[contract.ENVELOPE_HEADER_BYTES..]);

    try std.testing.expectEqual(@as(u16, 1), committed.object_id.algorithm);
    try std.testing.expectEqual(@as(usize, 20), committed.object_id.bytes.len);
    try std.testing.expectEqualSlices(u8, committed.object_id.bytes, resolved.object_id.bytes);
    const zero_hash = [_]u8{0} ** 20;
    try std.testing.expect(!std.mem.eql(u8, committed.object_id.bytes, &zero_hash));
}

test "restored browser session requires explicit repository open" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const original_session = try openSession(&engine, allocator);

    const init_request = try requestEnvelope(allocator, @intCast(contract.OP_REPOSITORY_INIT), 20, &.{});
    defer allocator.free(init_request);
    const initialized = try takeResult(&engine, allocator, engine.execute(original_session, init_request));
    defer allocator.free(initialized);
    try expectEnvelope(initialized, @intCast(contract.OP_REPOSITORY_INIT), @intCast(contract.STATUS_OK), 20);

    const checkpoint_request = try requestEnvelope(allocator, @intCast(contract.OP_CHECKPOINT), 21, &.{});
    defer allocator.free(checkpoint_request);
    const checkpointed = try takeResult(&engine, allocator, engine.execute(original_session, checkpoint_request));
    defer allocator.free(checkpointed);
    try expectEnvelope(checkpointed, @intCast(contract.OP_CHECKPOINT), @intCast(contract.STATUS_OK), 21);
    const snapshot = try contract.SnapshotResult.decode(allocator, checkpointed[contract.ENVELOPE_HEADER_BYTES..]);

    const restored_config = try (contract.SessionConfig{ .backend = @intCast(contract.BACKEND_BROWSER), .read_only = false, .root = "", .restore = snapshot.image }).encode(allocator);
    defer allocator.free(restored_config);
    const opened_session_bytes = try takeResult(&engine, allocator, engine.sessionOpen(restored_config, 22));
    defer allocator.free(opened_session_bytes);
    try expectEnvelope(opened_session_bytes, @intCast(contract.OP_SESSION_OPEN), @intCast(contract.STATUS_OK), 22);
    const opened_session = try contract.Result.decode(allocator, opened_session_bytes[contract.ENVELOPE_HEADER_BYTES..]);
    const restored_session = opened_session.handle.?;

    const open_request = try requestEnvelope(allocator, @intCast(contract.OP_REPOSITORY_OPEN), 23, &.{});
    defer allocator.free(open_request);
    const opened_repository = try takeResult(&engine, allocator, engine.execute(restored_session, open_request));
    defer allocator.free(opened_repository);
    try expectEnvelope(opened_repository, @intCast(contract.OP_REPOSITORY_OPEN), @intCast(contract.STATUS_OK), 23);
}

test "remote clone effect is resumable and intermediate acknowledgements use HTTP opcode" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);

    const remote_payload = try (contract.RemoteRequest{ .action = 1, .url = "https://git.example/repository.git", .refspecs = &.{}, .flags = 0 }).encode(allocator);
    defer allocator.free(remote_payload);
    const request = try requestEnvelope(allocator, @intCast(contract.OP_CLONE), 41, remote_payload);
    defer allocator.free(request);
    const first = try takeResult(&engine, allocator, engine.execute(session, request));
    defer allocator.free(first);
    try expectEnvelope(first, @intCast(contract.OP_CLONE), @intCast(contract.STATUS_EFFECT), 41);
    const effect = try contract.HttpEffect.decode(allocator, first[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(effect.headers);
    try std.testing.expectEqualStrings("GET", effect.method);
    try std.testing.expect(std.mem.endsWith(u8, effect.path, "/info/refs?service=git-upload-pack"));

    const begin_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_BEGIN), .status = 200, .headers = &.{}, .data = null }).encode(allocator);
    defer allocator.free(begin_payload);
    const begin_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 41, begin_payload);
    defer allocator.free(begin_request);
    const ack = try takeResult(&engine, allocator, engine.execute(session, begin_request));
    defer allocator.free(ack);
    try expectEnvelope(ack, @intCast(contract.OP_HTTP_EFFECT), @intCast(contract.STATUS_OK), 41);

    const advertisement = try advertisedHead(allocator, "1111111111111111111111111111111111111111", "refs/heads/main");
    defer allocator.free(advertisement);
    const chunk_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_CHUNK), .headers = &.{}, .data = advertisement }).encode(allocator);
    defer allocator.free(chunk_payload);
    const chunk_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 41, chunk_payload);
    defer allocator.free(chunk_request);
    const chunk_ack = try takeResult(&engine, allocator, engine.execute(session, chunk_request));
    defer allocator.free(chunk_ack);
    try expectEnvelope(chunk_ack, @intCast(contract.OP_HTTP_EFFECT), @intCast(contract.STATUS_OK), 41);

    const end_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_END), .headers = &.{} }).encode(allocator);
    defer allocator.free(end_payload);
    const end_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 41, end_payload);
    defer allocator.free(end_request);
    const next = try takeResult(&engine, allocator, engine.execute(session, end_request));
    defer allocator.free(next);
    try expectEnvelope(next, @intCast(contract.OP_CLONE), @intCast(contract.STATUS_EFFECT), 41);
    const upload = try contract.HttpEffect.decode(allocator, next[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(upload.headers);
    try std.testing.expectEqual(effect.exchange, upload.exchange);
    try std.testing.expectEqualStrings("POST", upload.method);
    try std.testing.expect(upload.body != null);
    try std.testing.expect(std.mem.endsWith(u8, upload.path, "/git-upload-pack"));
}

test "remote cancellation invalidates exchange and session close owns its streams" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    const payload = try (contract.RemoteRequest{ .action = 1, .url = "https://git.example/r.git", .refspecs = &.{}, .flags = 0 }).encode(allocator);
    defer allocator.free(payload);
    const request = try requestEnvelope(allocator, @intCast(contract.OP_CLONE), 77, payload);
    defer allocator.free(request);
    const response = try takeResult(&engine, allocator, engine.execute(session, request));
    defer allocator.free(response);
    const effect = try contract.HttpEffect.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(effect.headers);

    const cancel_payload = try (contract.StreamRequest{ .action = @intCast(contract.STREAM_ABORT), .handle = effect.exchange }).encode(allocator);
    defer allocator.free(cancel_payload);
    const cancel_request = try requestEnvelope(allocator, @intCast(contract.OP_REMOTE_CANCEL), 78, cancel_payload);
    defer allocator.free(cancel_request);
    const cancelled = try takeResult(&engine, allocator, engine.execute(session, cancel_request));
    defer allocator.free(cancelled);
    try expectEnvelope(cancelled, @intCast(contract.OP_REMOTE_CANCEL), @intCast(contract.STATUS_OK), 78);

    const end_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_END), .headers = &.{} }).encode(allocator);
    defer allocator.free(end_payload);
    const end_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 77, end_payload);
    defer allocator.free(end_request);
    const stale = try takeResult(&engine, allocator, engine.execute(session, end_request));
    defer allocator.free(stale);
    try expectEnvelope(stale, @intCast(contract.OP_HTTP_EFFECT), @intCast(contract.STATUS_ERROR), 77);
    try std.testing.expectEqual(@as(u32, 0), engine.sessionClose(session));
}

test "remote rejects non HTTP origins before reserving state" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    const payload = try (contract.RemoteRequest{ .action = 1, .url = "file:///outside", .refspecs = &.{}, .flags = 0 }).encode(allocator);
    defer allocator.free(payload);
    const request = try requestEnvelope(allocator, @intCast(contract.OP_CLONE), 91, payload);
    defer allocator.free(request);
    const response = try takeResult(&engine, allocator, engine.execute(session, request));
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_CLONE), @intCast(contract.STATUS_ERROR), 91);
}

test "remote accepts depth and rejects unsafe ref mappings before effects" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);

    const depth_payload = try (contract.RemoteRequest{ .action = 1, .url = "https://git.example/r.git", .refspecs = &.{}, .depth = 1, .flags = 0 }).encode(allocator);
    defer allocator.free(depth_payload);
    const depth_request = try requestEnvelope(allocator, @intCast(contract.OP_CLONE), 96, depth_payload);
    defer allocator.free(depth_request);
    const depth_response = try takeResult(&engine, allocator, engine.execute(session, depth_request));
    defer allocator.free(depth_response);
    try expectEnvelope(depth_response, @intCast(contract.OP_CLONE), @intCast(contract.STATUS_EFFECT), 96);

    const invalid_mapping = [_]contract.StringPair{.{ .key = "refs/heads/main", .value = "refs/heads/local" }};
    const fetch_payload = try (contract.RemoteRequest{ .action = 1, .url = "https://git.example/r.git", .remote = "origin", .refspecs = &invalid_mapping, .flags = 0 }).encode(allocator);
    defer allocator.free(fetch_payload);
    const fetch_request = try requestEnvelope(allocator, @intCast(contract.OP_FETCH), 97, fetch_payload);
    defer allocator.free(fetch_request);
    const fetch_response = try takeResult(&engine, allocator, engine.execute(session, fetch_request));
    defer allocator.free(fetch_response);
    try expectEnvelope(fetch_response, @intCast(contract.OP_FETCH), @intCast(contract.STATUS_ERROR), 97);
}

test "remote rejects out of order response actions and invalidates the exchange" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    const payload = try (contract.RemoteRequest{ .action = 1, .url = "https://git.example/r.git", .refspecs = &.{}, .flags = 0 }).encode(allocator);
    defer allocator.free(payload);
    const request = try requestEnvelope(allocator, @intCast(contract.OP_CLONE), 95, payload);
    defer allocator.free(request);
    const response = try takeResult(&engine, allocator, engine.execute(session, request));
    defer allocator.free(response);
    const effect = try contract.HttpEffect.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(effect.headers);

    const chunk_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_CHUNK), .headers = &.{}, .data = "out-of-order" }).encode(allocator);
    defer allocator.free(chunk_payload);
    const chunk_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 95, chunk_payload);
    defer allocator.free(chunk_request);
    const failed = try takeResult(&engine, allocator, engine.execute(session, chunk_request));
    defer allocator.free(failed);
    try expectEnvelope(failed, @intCast(contract.OP_CLONE), @intCast(contract.STATUS_ERROR), 95);

    const begin_payload = try (contract.HttpResponse{ .exchange = effect.exchange, .action = @intCast(contract.HTTP_RESPONSE_BEGIN), .status = 200, .headers = &.{} }).encode(allocator);
    defer allocator.free(begin_payload);
    const begin_request = try requestEnvelope(allocator, @intCast(contract.OP_HTTP_EFFECT), 95, begin_payload);
    defer allocator.free(begin_request);
    const stale = try takeResult(&engine, allocator, engine.execute(session, begin_request));
    defer allocator.free(stale);
    try expectEnvelope(stale, @intCast(contract.OP_HTTP_EFFECT), @intCast(contract.STATUS_ERROR), 95);
}

test "mount create distinguishes directories from files" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);

    const mkdir_payload = try (contract.MountRequest{ .action = @intCast(contract.MOUNT_CREATE), .path = "work/tree", .flags = 1, .mode = 0o040000 }).encode(allocator);
    defer allocator.free(mkdir_payload);
    const mkdir_request = try requestEnvelope(allocator, @intCast(contract.OP_MOUNT), 101, mkdir_payload);
    defer allocator.free(mkdir_request);
    const created_directory = try takeResult(&engine, allocator, engine.execute(session, mkdir_request));
    defer allocator.free(created_directory);
    try expectEnvelope(created_directory, @intCast(contract.OP_MOUNT), @intCast(contract.STATUS_OK), 101);
    const directory = try contract.FileResult.decode(allocator, created_directory[contract.ENVELOPE_HEADER_BYTES..]);
    try std.testing.expect(directory.mode & 0o040000 != 0);

    const file_payload = try (contract.MountRequest{ .action = @intCast(contract.MOUNT_CREATE), .path = "work/tree/file", .flags = 0, .data = "contents" }).encode(allocator);
    defer allocator.free(file_payload);
    const file_request = try requestEnvelope(allocator, @intCast(contract.OP_MOUNT), 102, file_payload);
    defer allocator.free(file_request);
    const created_file = try takeResult(&engine, allocator, engine.execute(session, file_request));
    defer allocator.free(created_file);
    try expectEnvelope(created_file, @intCast(contract.OP_MOUNT), @intCast(contract.STATUS_OK), 102);
    const file = try contract.FileResult.decode(allocator, created_file[contract.ENVELOPE_HEADER_BYTES..]);
    try std.testing.expectEqual(@as(u32, 8), file.size_low);
}

test "ignore query reports only paths matched by repository ignore rules" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    try initializeRepository(&engine, allocator, session, 110);
    try writeRepositoryFile(&engine, allocator, session, 111, ".gitignore", "*.log\nbuild/\n");
    try writeRepositoryFile(&engine, allocator, session, 112, "trace.log", "ignored");
    try writeRepositoryFile(&engine, allocator, session, 113, "kept.txt", "visible");

    const paths = [_]contract.StringPair{
        .{ .key = "trace.log", .value = "" },
        .{ .key = "kept.txt", .value = "" },
    };
    const payload = try (contract.PathQuery{ .paths = &paths }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(&engine, allocator, session, @intCast(contract.OP_IGNORE_QUERY), 114, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_IGNORE_QUERY), @intCast(contract.STATUS_OK), 114);
    const result = try contract.IgnoreResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(result.paths);
    try std.testing.expectEqual(@as(usize, 1), result.paths.len);
    try std.testing.expectEqualStrings("trace.log", result.paths[0].key);
}

test "shallow boundary update is returned by subsequent get" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    try initializeRepository(&engine, allocator, session, 120);
    const commit_hash = try createCommit(&engine, allocator, session, 121, "one.txt", "one", "shallow boundary");
    const oid = contract.ObjectId{ .algorithm = 1, .bytes = &commit_hash };

    const update_payload = try (contract.ShallowRequest{ .action = @intCast(contract.ACTION_UPDATE), .commits = &.{oid} }).encode(allocator);
    defer allocator.free(update_payload);
    const updated = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SHALLOW), 124, update_payload);
    defer allocator.free(updated);
    try expectEnvelope(updated, @intCast(contract.OP_SHALLOW), @intCast(contract.STATUS_OK), 124);

    const get_payload = try (contract.ShallowRequest{ .action = @intCast(contract.ACTION_GET), .commits = &.{} }).encode(allocator);
    defer allocator.free(get_payload);
    const fetched = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SHALLOW), 125, get_payload);
    defer allocator.free(fetched);
    try expectEnvelope(fetched, @intCast(contract.OP_SHALLOW), @intCast(contract.STATUS_OK), 125);
    const result = try contract.ShallowResult.decode(allocator, fetched[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(result.commits);
    try std.testing.expectEqual(@as(usize, 1), result.commits.len);
    try std.testing.expectEqualSlices(u8, &commit_hash, result.commits[0].bytes);
}

test "submodule list get and create preserve the exact gitlink" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const session = try openSession(&engine, allocator);
    try initializeRepository(&engine, allocator, session, 130);
    const commit_hash = try createCommit(&engine, allocator, session, 131, "module.txt", "module", "module target");
    try writeRepositoryFile(&engine, allocator, session, 134, ".gitmodules", "[submodule \"deps/lib\"]\n\tpath = deps/lib\n\turl = https://git.example/lib.git\n");

    const list_payload = try (contract.SubmoduleRequest{ .action = @intCast(contract.ACTION_LIST) }).encode(allocator);
    defer allocator.free(list_payload);
    const listed = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SUBMODULE), 135, list_payload);
    defer allocator.free(listed);
    const before = try contract.SubmoduleResult.decode(allocator, listed[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(before.entries);
    try std.testing.expectEqual(@as(usize, 1), before.entries.len);
    try std.testing.expectEqualStrings("deps/lib", before.entries[0].path);
    try std.testing.expect(before.entries[0].gitlink == null);

    const oid = contract.ObjectId{ .algorithm = 1, .bytes = &commit_hash };
    const create_payload = try (contract.SubmoduleRequest{ .action = @intCast(contract.ACTION_CREATE), .path = "deps/lib", .object_id = oid }).encode(allocator);
    defer allocator.free(create_payload);
    const created = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SUBMODULE), 136, create_payload);
    defer allocator.free(created);
    try expectEnvelope(created, @intCast(contract.OP_SUBMODULE), @intCast(contract.STATUS_OK), 136);

    const get_payload = try (contract.SubmoduleRequest{ .action = @intCast(contract.ACTION_GET), .path = "deps/lib" }).encode(allocator);
    defer allocator.free(get_payload);
    const fetched = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SUBMODULE), 137, get_payload);
    defer allocator.free(fetched);
    const after = try contract.SubmoduleResult.decode(allocator, fetched[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(after.entries);
    try std.testing.expectEqual(@as(usize, 1), after.entries.len);
    try std.testing.expectEqualStrings("deps/lib", after.entries[0].name);
    try std.testing.expectEqualStrings("https://git.example/lib.git", after.entries[0].url);
    try std.testing.expectEqualSlices(u8, &commit_hash, after.entries[0].gitlink.?.bytes);
}

test "reference transactions are atomic generational and session owned" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    const first_session = try openSession(&engine, allocator);
    try initializeRepository(&engine, allocator, first_session, 140);
    const commit_hash = try createCommit(&engine, allocator, first_session, 141, "ref.txt", "ref", "ref target");
    const oid = contract.ObjectId{ .algorithm = 1, .bytes = &commit_hash };
    const replacement_hash = try createCommit(&engine, allocator, first_session, 144, "ref.txt", "replacement", "replacement target");
    const replacement_oid = contract.ObjectId{ .algorithm = 1, .bytes = &replacement_hash };

    const initial_updates = [_]contract.RefUpdate{
        .{ .name = "refs/heads/atomic-a", .new_value = oid, .require_absent = true },
        .{ .name = "refs/heads/atomic-b", .new_value = oid, .require_absent = true },
    };
    const first_handle = try beginRefTransaction(&engine, allocator, first_session, 148, &initial_updates);
    const finished = try finishRefTransaction(&engine, allocator, first_session, 149, first_handle, contract.ACTION_FINISH);
    defer allocator.free(finished);
    try expectEnvelope(finished, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_OK), 149);

    const expected_old_updates = [_]contract.RefUpdate{
        .{ .name = "refs/heads/atomic-a", .new_value = replacement_oid, .expected_value = oid, .require_absent = false },
        .{ .name = "refs/heads/atomic-b", .new_value = replacement_oid, .expected_value = oid, .require_absent = false },
    };
    const expected_handle = try beginRefTransaction(&engine, allocator, first_session, 150, &expected_old_updates);
    const expected_finished = try finishRefTransaction(&engine, allocator, first_session, 151, expected_handle, contract.ACTION_FINISH);
    defer allocator.free(expected_finished);
    try expectEnvelope(expected_finished, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_OK), 151);
    try expectReferenceHash(&engine, allocator, first_session, 152, "refs/heads/atomic-a", &replacement_hash);
    try expectReferenceHash(&engine, allocator, first_session, 153, "refs/heads/atomic-b", &replacement_hash);

    const wrong_hash = [_]u8{0xaa} ** 20;
    const wrong_oid = contract.ObjectId{ .algorithm = 1, .bytes = &wrong_hash };
    const rejected_updates = [_]contract.RefUpdate{
        .{ .name = "refs/heads/atomic-a", .new_value = oid, .expected_value = wrong_oid, .require_absent = false },
        .{ .name = "refs/heads/must-not-appear", .new_value = oid, .require_absent = true },
    };
    const rejected_handle = try beginRefTransaction(&engine, allocator, first_session, 154, &rejected_updates);
    const rejected = try finishRefTransaction(&engine, allocator, first_session, 155, rejected_handle, contract.ACTION_FINISH);
    defer allocator.free(rejected);
    try expectEnvelope(rejected, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_ERROR), 155);
    try expectMissingReference(&engine, allocator, first_session, 156, "refs/heads/must-not-appear");

    const abort_updates = [_]contract.RefUpdate{.{ .name = "refs/heads/aborted", .new_value = oid, .require_absent = true }};
    const abort_handle = try beginRefTransaction(&engine, allocator, first_session, 157, &abort_updates);
    const second_session = try openSession(&engine, allocator);
    const foreign = try finishRefTransaction(&engine, allocator, second_session, 158, abort_handle, contract.ACTION_FINISH);
    defer allocator.free(foreign);
    try expectEnvelope(foreign, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_ERROR), 158);
    const aborted = try finishRefTransaction(&engine, allocator, first_session, 159, abort_handle, contract.ACTION_ABORT);
    defer allocator.free(aborted);
    try expectEnvelope(aborted, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_OK), 159);
    const stale = try finishRefTransaction(&engine, allocator, first_session, 160, abort_handle, contract.ACTION_ABORT);
    defer allocator.free(stale);
    try expectEnvelope(stale, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_ERROR), 160);
    try expectMissingReference(&engine, allocator, first_session, 161, "refs/heads/aborted");
}

test "merge commits both parents and round-trips conflicts" {
    const allocator = std.testing.allocator;
    var engine = core.Engine(Browser).init(allocator, .{});
    defer engine.deinit();
    var ids: u32 = 400;
    const session = try openSession(&engine, allocator);
    try initializeRepository(&engine, allocator, session, bump(&ids));

    const readonly = try openReadOnly(&engine, allocator);
    try expectFailure(&engine, allocator, readonly, @intCast(contract.OP_MERGE), bump(&ids), &.{}, contract.ERROR_PATH, contract.ERROR_CODE_DENIED, contract.RETRY_NEVER);

    const base = try createCommit(&engine, allocator, session, reserve(&ids, 3), "note.txt", "a\nb\nc\n", "base");
    const ours = try createCommit(&engine, allocator, session, reserve(&ids, 3), "note.txt", "A\nb\nc\n", "ours");
    const base_hex = formatHex(base);
    try resetHard(&engine, allocator, session, bump(&ids), &base_hex);
    const theirs = try createCommit(&engine, allocator, session, reserve(&ids, 3), "note.txt", "a\nb\nC\n", "theirs");
    const ours_hex = formatHex(ours);
    const theirs_hex = formatHex(theirs);
    try resetHard(&engine, allocator, session, bump(&ids), &ours_hex);

    const merged = try runMerge(&engine, allocator, session, bump(&ids), &.{&theirs_hex}, .{});
    defer freeMerge(allocator, merged);
    try std.testing.expectEqual(@as(u16, @intCast(contract.MERGE_OUTCOME_COMMITTED)), merged.outcome);
    const merge_hash = merged.hash orelse return error.UnexpectedObjectId;
    const object = try commitObject(&engine, allocator, session, bump(&ids), &merge_hash);
    defer allocator.free(object);
    try std.testing.expect(std.mem.indexOf(u8, object, "1700000000") != null);
    try std.testing.expect(std.mem.indexOf(u8, object, "AgentOS") != null);
    try std.testing.expectEqual(@as(usize, 2), std.mem.count(u8, object, "parent "));
    const merged_text = try readText(&engine, allocator, session, bump(&ids), "note.txt");
    defer allocator.free(merged_text);
    try std.testing.expectEqualStrings("A\nb\nC\n", merged_text);

    const ahead = try createCommit(&engine, allocator, session, reserve(&ids, 3), "extra.txt", "more\n", "ahead");
    const merge_hex = formatHex(merge_hash);
    try resetHard(&engine, allocator, session, bump(&ids), &merge_hex);
    const ahead_hex = formatHex(ahead);
    const forwarded = try runMerge(&engine, allocator, session, bump(&ids), &.{&ahead_hex}, .{});
    defer freeMerge(allocator, forwarded);
    try std.testing.expectEqual(@as(u16, @intCast(contract.MERGE_OUTCOME_FAST_FORWARD)), forwarded.outcome);
    try std.testing.expectEqualSlices(u8, &ahead, &(forwarded.hash orelse return error.UnexpectedObjectId));
    const continue_after_ff = try continuePayload(allocator);
    defer allocator.free(continue_after_ff);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), continue_after_ff, contract.ERROR_WORKTREE, contract.ERROR_CODE_STALE, contract.RETRY_NEVER);

    try resetHard(&engine, allocator, session, bump(&ids), &merge_hex);
    const side = try createCommit(&engine, allocator, session, reserve(&ids, 3), "other.txt", "side\n", "side");
    try resetHard(&engine, allocator, session, bump(&ids), &ahead_hex);
    const side_hex = formatHex(side);
    const ff_only = try mergePayload(allocator, &.{&side_hex}, .{ .fast_forward = @intCast(contract.MERGE_FF_ONLY) });
    defer allocator.free(ff_only);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), ff_only, contract.ERROR_REFERENCE, contract.ERROR_CODE_CONFLICT, contract.RETRY_NEVER);
    const stayed = try headHash(&engine, allocator, session, bump(&ids));
    try std.testing.expectEqualSlices(u8, &ahead, &stayed);
    const extra_text = try readText(&engine, allocator, session, bump(&ids), "extra.txt");
    defer allocator.free(extra_text);
    try std.testing.expectEqualStrings("more\n", extra_text);
    const continue_after_ff_only = try continuePayload(allocator);
    defer allocator.free(continue_after_ff_only);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), continue_after_ff_only, contract.ERROR_WORKTREE, contract.ERROR_CODE_STALE, contract.RETRY_NEVER);

    const conflict_base = try createCommit(&engine, allocator, session, reserve(&ids, 3), "conflict.txt", "a\nb\nc\n", "conflict base");
    const conflict_ours = try createCommit(&engine, allocator, session, reserve(&ids, 3), "conflict.txt", "a\nX\nc\n", "conflict ours");
    const conflict_base_hex = formatHex(conflict_base);
    try resetHard(&engine, allocator, session, bump(&ids), &conflict_base_hex);
    const conflict_theirs = try createCommit(&engine, allocator, session, reserve(&ids, 3), "conflict.txt", "a\nY\nc\n", "conflict theirs");
    const conflict_ours_hex = formatHex(conflict_ours);
    const conflict_theirs_hex = formatHex(conflict_theirs);
    try resetHard(&engine, allocator, session, bump(&ids), &conflict_ours_hex);
    const conflicted = try runMerge(&engine, allocator, session, bump(&ids), &.{&conflict_theirs_hex}, .{});
    defer freeMerge(allocator, conflicted);
    try std.testing.expectEqual(@as(u16, @intCast(contract.MERGE_OUTCOME_CONFLICTS)), conflicted.outcome);
    try std.testing.expect(conflicted.hash == null);
    try std.testing.expect(conflicted.conflicts.len >= 1);
    try std.testing.expectEqual(@as(u16, 'U'), conflicted.conflicts[0].ours);
    try std.testing.expectEqual(@as(u16, 'U'), conflicted.conflicts[0].theirs);
    try std.testing.expect(try statusHas(&engine, allocator, session, bump(&ids), "conflict.txt", 'U', 'U'));
    const again = try mergePayload(allocator, &.{&conflict_theirs_hex}, .{});
    defer allocator.free(again);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), again, contract.ERROR_WORKTREE, contract.ERROR_CODE_CONFLICT, contract.RETRY_AFTER_INPUT);
    try addPath(&engine, allocator, session, bump(&ids), "conflict.txt");
    _ = try commitMessage(&engine, allocator, session, bump(&ids), "resolve conflict");
    const continue_after_commit = try continuePayload(allocator);
    defer allocator.free(continue_after_commit);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), continue_after_commit, contract.ERROR_WORKTREE, contract.ERROR_CODE_STALE, contract.RETRY_NEVER);

    const abort_base = try createCommit(&engine, allocator, session, reserve(&ids, 3), "abort.txt", "a\nb\nc\n", "abort base");
    const abort_ours = try createCommit(&engine, allocator, session, reserve(&ids, 3), "abort.txt", "a\nX\nc\n", "abort ours");
    const abort_base_hex = formatHex(abort_base);
    try resetHard(&engine, allocator, session, bump(&ids), &abort_base_hex);
    const abort_theirs = try createCommit(&engine, allocator, session, reserve(&ids, 3), "abort.txt", "a\nY\nc\n", "abort theirs");
    const abort_ours_hex = formatHex(abort_ours);
    const abort_theirs_hex = formatHex(abort_theirs);
    try resetHard(&engine, allocator, session, bump(&ids), &abort_ours_hex);
    const abort_merge = try runMerge(&engine, allocator, session, bump(&ids), &.{&abort_theirs_hex}, .{});
    defer freeMerge(allocator, abort_merge);
    try std.testing.expectEqual(@as(u16, @intCast(contract.MERGE_OUTCOME_CONFLICTS)), abort_merge.outcome);
    const aborted = try runMerge(&engine, allocator, session, bump(&ids), &.{}, .{ .action = @intCast(contract.ACTION_ABORT) });
    defer freeMerge(allocator, aborted);
    try std.testing.expectEqual(@as(u16, @intCast(contract.MERGE_OUTCOME_ABORTED)), aborted.outcome);
    const restored = try readText(&engine, allocator, session, bump(&ids), "abort.txt");
    defer allocator.free(restored);
    try std.testing.expectEqualStrings("a\nX\nc\n", restored);
    try std.testing.expect(std.mem.indexOf(u8, restored, "<<<<<<<") == null);
    const continue_after_abort = try continuePayload(allocator);
    defer allocator.free(continue_after_abort);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), continue_after_abort, contract.ERROR_WORKTREE, contract.ERROR_CODE_STALE, contract.RETRY_NEVER);

    const octopus_base = try createCommit(&engine, allocator, session, reserve(&ids, 3), "octo.txt", "base\n", "octopus base");
    const octopus_left = try createCommit(&engine, allocator, session, reserve(&ids, 3), "octo.txt", "left\n", "octopus left");
    const octopus_base_hex = formatHex(octopus_base);
    try resetHard(&engine, allocator, session, bump(&ids), &octopus_base_hex);
    const octopus_right = try createCommit(&engine, allocator, session, reserve(&ids, 3), "octo.txt", "right\n", "octopus right");
    const left_hex = formatHex(octopus_left);
    const right_hex = formatHex(octopus_right);
    try resetHard(&engine, allocator, session, bump(&ids), &octopus_base_hex);
    const octopus = try mergePayload(allocator, &.{ &left_hex, &right_hex }, .{ .strategy = @intCast(contract.MERGE_STRATEGY_OCTOPUS) });
    defer allocator.free(octopus);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), octopus, contract.ERROR_WORKTREE, contract.ERROR_CODE_CONFLICT, contract.RETRY_NEVER);
    const octopus_text = try readText(&engine, allocator, session, bump(&ids), "octo.txt");
    defer allocator.free(octopus_text);
    try std.testing.expectEqualStrings("base\n", octopus_text);

    try writeRepositoryFile(&engine, allocator, session, bump(&ids), "octo.txt", "dirty\n");
    const dirty = try mergePayload(allocator, &.{&left_hex}, .{});
    defer allocator.free(dirty);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), dirty, contract.ERROR_WORKTREE, contract.ERROR_CODE_CONFLICT, contract.RETRY_AFTER_INPUT);
    const dirty_text = try readText(&engine, allocator, session, bump(&ids), "octo.txt");
    defer allocator.free(dirty_text);
    try std.testing.expectEqualStrings("dirty\n", dirty_text);
    try resetHard(&engine, allocator, session, bump(&ids), "HEAD");

    _ = try createCommit(&engine, allocator, session, reserve(&ids, 3), "dir/keep.txt", "keep\n", "sparse");
    const sparse_paths = [_]contract.StringPair{.{ .key = "dir", .value = "" }};
    const sparse_payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_LIST), .flags = 0, .paths = &sparse_paths }).encode(allocator);
    defer allocator.free(sparse_payload);
    const sparse_id = bump(&ids);
    const sparsened = try executeRequest(&engine, allocator, session, @intCast(contract.OP_SPARSE), sparse_id, sparse_payload);
    defer allocator.free(sparsened);
    try expectEnvelope(sparsened, @intCast(contract.OP_SPARSE), @intCast(contract.STATUS_OK), sparse_id);
    const sparse_merge = try mergePayload(allocator, &.{"HEAD"}, .{});
    defer allocator.free(sparse_merge);
    try expectFailure(&engine, allocator, session, @intCast(contract.OP_MERGE), bump(&ids), sparse_merge, contract.ERROR_USAGE, contract.ERROR_CODE_INVALID, contract.RETRY_NEVER);
}

const MergeCall = struct {
    strategy: u16 = @intCast(contract.MERGE_STRATEGY_DEFAULT),
    fast_forward: u16 = @intCast(contract.MERGE_FF),
    action: u16 = @intCast(contract.ACTION_CREATE),
};

const MergeView = struct {
    outcome: u16,
    hash: ?[20]u8 = null,
    conflicts: []const contract.MergeConflict,
    response: []u8,
};

fn bump(ids: *u32) u32 {
    const id = ids.*;
    ids.* += 1;
    return id;
}

fn reserve(ids: *u32, count: u32) u32 {
    const id = ids.*;
    ids.* += count;
    return id;
}

fn formatHex(hash: [20]u8) [40]u8 {
    var out: [40]u8 = undefined;
    const digits = "0123456789abcdef";
    for (hash, 0..) |byte, index| {
        out[index * 2] = digits[byte >> 4];
        out[index * 2 + 1] = digits[byte & 0xf];
    }
    return out;
}

fn mergeIdentity() contract.Signature {
    return .{ .name = "AgentOS", .email = "agentos@example.test", .unix_seconds = 1_700_000_000, .timezone_minutes = 0 };
}

fn mergePayload(allocator: std.mem.Allocator, revisions: []const []const u8, call: MergeCall) ![]u8 {
    const heads = try allocator.alloc(contract.MergeHead, revisions.len);
    defer allocator.free(heads);
    for (revisions, 0..) |revision, index| heads[index] = .{ .revision = revision };
    const identity = mergeIdentity();
    return (contract.MergeRequest{
        .action = call.action,
        .heads = heads,
        .strategy = call.strategy,
        .fast_forward = call.fast_forward,
        .favor = @intCast(contract.MERGE_FAVOR_NONE),
        .diff_algorithm = @intCast(contract.MERGE_DIFF_HISTOGRAM),
        .conflict_style = @intCast(contract.MERGE_STYLE_MERGE),
        .find_renames = true,
        .no_commit = false,
        .allow_unrelated_histories = false,
        .author = identity,
        .committer = identity,
    }).encode(allocator);
}

fn continuePayload(allocator: std.mem.Allocator) ![]u8 {
    return mergePayload(allocator, &.{}, .{ .action = @intCast(contract.ACTION_FINISH) });
}

fn runMerge(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, revisions: []const []const u8, call: MergeCall) !MergeView {
    const payload = try mergePayload(allocator, revisions, call);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_MERGE), request_id, payload);
    errdefer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_MERGE), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.MergeResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    var hash: ?[20]u8 = null;
    if (result.object_id) |oid| {
        if (oid.bytes.len != 20) return error.UnexpectedObjectId;
        var copied: [20]u8 = undefined;
        @memcpy(&copied, oid.bytes);
        hash = copied;
    }
    return .{ .outcome = result.outcome, .hash = hash, .conflicts = result.conflicts, .response = response };
}

fn freeMerge(allocator: std.mem.Allocator, view: MergeView) void {
    allocator.free(view.conflicts);
    allocator.free(view.response);
}

fn expectFailure(engine: anytype, allocator: std.mem.Allocator, session: u32, opcode: u16, request_id: u32, payload: []const u8, domain: u32, code: u32, retry: u32) !void {
    const response = try executeRequest(engine, allocator, session, opcode, request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, opcode, @intCast(contract.STATUS_ERROR), request_id);
    const failure = try contract.EngineError.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    try std.testing.expectEqual(@as(u16, @intCast(domain)), failure.domain);
    try std.testing.expectEqual(@as(u16, @intCast(code)), failure.code);
    try std.testing.expectEqual(@as(u16, @intCast(retry)), failure.retry);
}

fn headHash(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32) ![20]u8 {
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_GET), .flags = 0, .revision = "HEAD", .paths = &.{} }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_RESOLVE_REVISION), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_RESOLVE_REVISION), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.ResolveResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    if (result.object_id.bytes.len != 20) return error.UnexpectedObjectId;
    var hash: [20]u8 = undefined;
    @memcpy(&hash, result.object_id.bytes);
    return hash;
}

fn resetHard(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, revision: []const u8) !void {
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.RESET_HARD), .flags = 0, .revision = revision, .paths = &.{} }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_RESET), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_RESET), @intCast(contract.STATUS_OK), request_id);
}

fn addPath(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, path: []const u8) !void {
    const paths = [_]contract.StringPair{.{ .key = path, .value = "" }};
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_UPDATE), .flags = 0, .paths = &paths }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_ADD), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_ADD), @intCast(contract.STATUS_OK), request_id);
}

fn commitMessage(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, message: []const u8) ![20]u8 {
    const identity = mergeIdentity();
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_CREATE), .flags = 0, .message = message, .paths = &.{}, .author = identity, .committer = identity }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_COMMIT), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_COMMIT), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.CommitResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    if (result.object_id.bytes.len != 20) return error.UnexpectedObjectId;
    var hash: [20]u8 = undefined;
    @memcpy(&hash, result.object_id.bytes);
    return hash;
}

fn readText(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, path: []const u8) ![]u8 {
    const payload = try (contract.FileRequest{ .path = path }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_FILE_READ), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_FILE_READ), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.FileResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    return allocator.dupe(u8, result.data orelse return error.MissingData);
}

fn commitObject(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, hash: []const u8) ![]u8 {
    const payload = try (contract.ObjectRequest{ .action = @intCast(contract.ACTION_GET), .kind = 1, .object_id = .{ .algorithm = 1, .bytes = hash } }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_OBJECT), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_OBJECT), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.ObjectResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    return allocator.dupe(u8, result.data orelse return error.MissingData);
}

fn statusHas(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, path: []const u8, index: u16, worktree: u16) !bool {
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_STATUS), request_id, &.{});
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_STATUS), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.StatusResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    defer allocator.free(result.entries);
    for (result.entries) |entry| {
        if (entry.index == index and entry.worktree == worktree and std.mem.eql(u8, entry.path, path)) return true;
    }
    return false;
}

fn openReadOnly(engine: anytype, allocator: std.mem.Allocator) !u32 {
    const config = try (contract.SessionConfig{ .backend = @intCast(contract.BACKEND_BROWSER), .read_only = true, .root = "" }).encode(allocator);
    defer allocator.free(config);
    const response = try takeResult(engine, allocator, engine.sessionOpen(config, 2));
    defer allocator.free(response);
    return (try contract.Result.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..])).handle.?;
}

fn openSession(engine: anytype, allocator: std.mem.Allocator) !u32 {
    const config = try (contract.SessionConfig{ .backend = @intCast(contract.BACKEND_BROWSER), .read_only = false, .root = "" }).encode(allocator);
    defer allocator.free(config);
    const response = try takeResult(engine, allocator, engine.sessionOpen(config, 1));
    defer allocator.free(response);
    return (try contract.Result.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..])).handle.?;
}

fn initializeRepository(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32) !void {
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_REPOSITORY_INIT), request_id, &.{});
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_REPOSITORY_INIT), @intCast(contract.STATUS_OK), request_id);
}

fn writeRepositoryFile(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, path: []const u8, data: []const u8) !void {
    const payload = try (contract.FileRequest{ .path = path, .data = data }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_FILE_WRITE), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_FILE_WRITE), @intCast(contract.STATUS_OK), request_id);
}

fn createCommit(engine: anytype, allocator: std.mem.Allocator, session: u32, first_request_id: u32, path: []const u8, data: []const u8, message: []const u8) ![20]u8 {
    try writeRepositoryFile(engine, allocator, session, first_request_id, path, data);
    const paths = [_]contract.StringPair{.{ .key = path, .value = "" }};
    const add_payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_UPDATE), .flags = 0, .paths = &paths }).encode(allocator);
    defer allocator.free(add_payload);
    const added = try executeRequest(engine, allocator, session, @intCast(contract.OP_ADD), first_request_id + 1, add_payload);
    defer allocator.free(added);
    try expectEnvelope(added, @intCast(contract.OP_ADD), @intCast(contract.STATUS_OK), first_request_id + 1);

    const identity = contract.Signature{ .name = "AgentOS", .email = "agentos@example.test", .unix_seconds = 1_700_000_000, .timezone_minutes = 0 };
    const commit_payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_CREATE), .flags = 0, .message = message, .paths = &.{}, .author = identity, .committer = identity }).encode(allocator);
    defer allocator.free(commit_payload);
    const committed = try executeRequest(engine, allocator, session, @intCast(contract.OP_COMMIT), first_request_id + 2, commit_payload);
    defer allocator.free(committed);
    try expectEnvelope(committed, @intCast(contract.OP_COMMIT), @intCast(contract.STATUS_OK), first_request_id + 2);
    const result = try contract.CommitResult.decode(allocator, committed[contract.ENVELOPE_HEADER_BYTES..]);
    if (result.object_id.algorithm != 1 or result.object_id.bytes.len != 20) return error.UnexpectedObjectId;
    var hash: [20]u8 = undefined;
    @memcpy(&hash, result.object_id.bytes);
    return hash;
}

fn beginRefTransaction(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, updates: []const contract.RefUpdate) !u32 {
    const payload = try (contract.RefTransactionRequest{ .action = @intCast(contract.ACTION_BEGIN), .updates = updates }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_REF_TRANSACTION), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_REF_TRANSACTION), @intCast(contract.STATUS_OK), request_id);
    return (try contract.RefTransactionResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..])).handle.?;
}

fn finishRefTransaction(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, handle: u32, action: u32) ![]u8 {
    const payload = try (contract.RefTransactionRequest{ .action = @intCast(action), .handle = handle, .updates = &.{} }).encode(allocator);
    defer allocator.free(payload);
    return executeRequest(engine, allocator, session, @intCast(contract.OP_REF_TRANSACTION), request_id, payload);
}

fn expectReferenceHash(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, name: []const u8, expected: []const u8) !void {
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_GET), .flags = 0, .target = name, .paths = &.{} }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_REF), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_REF), @intCast(contract.STATUS_OK), request_id);
    const result = try contract.ReferenceResult.decode(allocator, response[contract.ENVELOPE_HEADER_BYTES..]);
    try std.testing.expectEqualSlices(u8, expected, result.object_id.?.bytes);
}

fn expectMissingReference(engine: anytype, allocator: std.mem.Allocator, session: u32, request_id: u32, name: []const u8) !void {
    const payload = try (contract.PorcelainRequest{ .action = @intCast(contract.ACTION_GET), .flags = 0, .target = name, .paths = &.{} }).encode(allocator);
    defer allocator.free(payload);
    const response = try executeRequest(engine, allocator, session, @intCast(contract.OP_REF), request_id, payload);
    defer allocator.free(response);
    try expectEnvelope(response, @intCast(contract.OP_REF), @intCast(contract.STATUS_ERROR), request_id);
}

fn executeRequest(engine: anytype, allocator: std.mem.Allocator, session: u32, opcode: u16, request_id: u32, payload: []const u8) ![]u8 {
    const request = try requestEnvelope(allocator, opcode, request_id, payload);
    defer allocator.free(request);
    return takeResult(engine, allocator, engine.execute(session, request));
}

fn takeResult(engine: anytype, allocator: std.mem.Allocator, handle: u32) ![]u8 {
    if (handle == 0) return error.NoResult;
    const length = engine.resultLen(handle);
    const bytes = try allocator.alloc(u8, length);
    errdefer allocator.free(bytes);
    if (engine.resultRead(handle, 0, bytes) != length) return error.ShortResult;
    if (engine.resultFree(handle) != 0) return error.ResultFreeFailed;
    return bytes;
}

fn requestEnvelope(allocator: std.mem.Allocator, opcode: u16, request_id: u32, payload: []const u8) ![]u8 {
    const bytes = try allocator.alloc(u8, contract.ENVELOPE_HEADER_BYTES + payload.len);
    @memcpy(bytes[0..4], contract.REQUEST_MAGIC);
    std.mem.writeInt(u16, bytes[4..6], @intCast(contract.PROTOCOL_VERSION), .little);
    std.mem.writeInt(u16, bytes[6..8], @intCast(contract.PROTOCOL_MINOR), .little);
    std.mem.writeInt(u16, bytes[8..10], opcode, .little);
    std.mem.writeInt(u16, bytes[10..12], 0, .little);
    std.mem.writeInt(u32, bytes[12..16], request_id, .little);
    std.mem.writeInt(u32, bytes[16..20], @intCast(payload.len), .little);
    @memcpy(bytes[20..], payload);
    return bytes;
}

fn expectEnvelope(bytes: []const u8, opcode: u16, status: u16, request_id: u32) !void {
    try std.testing.expectEqualSlices(u8, contract.RESPONSE_MAGIC, bytes[0..4]);
    try std.testing.expectEqual(opcode, std.mem.readInt(u16, bytes[8..10], .little));
    try std.testing.expectEqual(status, std.mem.readInt(u16, bytes[10..12], .little));
    try std.testing.expectEqual(request_id, std.mem.readInt(u32, bytes[12..16], .little));
}

fn advertisedHead(allocator: std.mem.Allocator, hash: []const u8, target: []const u8) ![]u8 {
    var output: std.ArrayList(u8) = .empty;
    errdefer output.deinit(allocator);
    try appendPktLine(&output, allocator, "# service=git-upload-pack\n");
    try output.appendSlice(allocator, "0000");
    const head = try std.fmt.allocPrint(allocator, "{s} HEAD\x00symref=HEAD:{s}\n", .{ hash, target });
    defer allocator.free(head);
    try appendPktLine(&output, allocator, head);
    const branch = try std.fmt.allocPrint(allocator, "{s} {s}\n", .{ hash, target });
    defer allocator.free(branch);
    try appendPktLine(&output, allocator, branch);
    try output.appendSlice(allocator, "0000");
    return output.toOwnedSlice(allocator);
}

fn appendPktLine(output: *std.ArrayList(u8), allocator: std.mem.Allocator, payload: []const u8) !void {
    var header: [4]u8 = undefined;
    const digits = "0123456789abcdef";
    const length: u16 = @intCast(payload.len + 4);
    header[0] = digits[(length >> 12) & 0xf];
    header[1] = digits[(length >> 8) & 0xf];
    header[2] = digits[(length >> 4) & 0xf];
    header[3] = digits[length & 0xf];
    try output.appendSlice(allocator, &header);
    try output.appendSlice(allocator, payload);
}
