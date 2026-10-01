//! Stable, bounded translation from internal failures to the generated Git ABI.

const contract = @import("git_zig");

pub const Code = struct {
    pub const invalid: u16 = @intCast(contract.ERROR_CODE_INVALID);
    pub const missing: u16 = @intCast(contract.ERROR_CODE_MISSING);
    pub const exists: u16 = @intCast(contract.ERROR_CODE_EXISTS);
    pub const not_directory: u16 = @intCast(contract.ERROR_CODE_NOT_DIRECTORY);
    pub const is_directory: u16 = @intCast(contract.ERROR_CODE_IS_DIRECTORY);
    pub const not_empty: u16 = @intCast(contract.ERROR_CODE_NOT_EMPTY);
    pub const denied: u16 = @intCast(contract.ERROR_CODE_DENIED);
    pub const stale: u16 = @intCast(contract.ERROR_CODE_STALE);
    pub const conflict: u16 = @intCast(contract.ERROR_CODE_CONFLICT);
};

pub const Classification = struct {
    domain: u16,
    code: u16,
    retry: u16 = @intCast(contract.RETRY_NEVER),
};

pub fn classify(err: anyerror, opcode: u16) Classification {
    return switch (err) {
        error.InvalidPath, error.ReservedPath, error.CrossedBoundary => path(Code.invalid),
        error.NotExist, error.FileNotFound, error.PathNotFound => path(Code.missing),
        error.Exist, error.PathAlreadyExists => path(Code.exists),
        error.NotDir => path(Code.not_directory),
        error.IsDir => path(Code.is_directory),
        error.DirNotEmpty => path(Code.not_empty),
        error.ReadOnly, error.AccessDenied, error.PermissionDenied => path(Code.denied),
        error.Closed, error.InvalidHandle => usage(Code.stale),
        error.ReferenceHasChanged => domainCode(contract.ERROR_REFERENCE, Code.conflict),
        error.ObjectNotFound => domainCode(contract.ERROR_OBJECT, Code.missing),
        error.ReferenceNotFound => domainCode(contract.ERROR_REFERENCE, Code.missing),
        error.EntryNotFound, error.IndexNotFound => domainCode(contract.ERROR_INDEX, Code.missing),
        error.InvalidTransactionJournal => domainCode(contract.ERROR_PERSISTENCE, Code.invalid),
        error.NoSpace => domainCode(contract.ERROR_PERSISTENCE, Code.denied),
        error.TransactionTooLarge, error.DataTooLarge, error.TooManyStreams, error.OutOfMemory, error.SystemResources => domainCode(contract.ERROR_LIMIT, Code.invalid),
        error.Canceled, error.Cancelled => domainCode(contract.ERROR_CANCELLED, Code.invalid),
        error.InvalidHash, error.InvalidType => domainCode(contract.ERROR_OBJECT, Code.invalid),
        error.InvalidReferenceName, error.ReferenceNameEscape, error.ReferenceNameMismatch, error.DuplicateReference => domainCode(contract.ERROR_REFERENCE, Code.invalid),
        error.MalformedRefFile, error.EmptyRefFile => domainCode(contract.ERROR_REFERENCE, Code.invalid),
        error.BadConfig, error.InvalidMode, error.InvalidAction, error.MissingData, error.InvalidOffset, error.UnexpectedPayload => usage(Code.invalid),
        error.NoMergeHeads, error.StrategyOptionNotSupported, error.SubtreeShiftNotFound, error.UnbornHead => usage(Code.invalid),
        error.MissingAuthor, error.MissingCommitter => usage(Code.missing),
        error.DirtyIndex, error.UnmergedPaths => retried(contract.ERROR_INDEX, Code.conflict, contract.RETRY_AFTER_INPUT),
        error.DirtyWorktree, error.MergeInProgress => retried(contract.ERROR_WORKTREE, Code.conflict, contract.RETRY_AFTER_INPUT),
        error.MergeNotInProgress, error.CorruptMergeState => domainCode(contract.ERROR_WORKTREE, Code.stale),
        error.NotFastForward => domainCode(contract.ERROR_REFERENCE, Code.conflict),
        error.UnrelatedHistories => retried(contract.ERROR_REPOSITORY, Code.denied, contract.RETRY_AFTER_INPUT),
        error.ShallowHistory => domainCode(contract.ERROR_REPOSITORY, Code.invalid),
        error.OctopusConflict => domainCode(contract.ERROR_WORKTREE, Code.conflict),
        error.VirtualMergeDepth => domainCode(contract.ERROR_LIMIT, Code.invalid),
        error.InvalidGitFileName => domainCode(contract.ERROR_INTERNAL, Code.invalid),
        error.MutationInProgress => domainCode(contract.ERROR_PERSISTENCE, Code.conflict),
        else => domainCode(defaultDomain(opcode), Code.invalid),
    };
}

fn defaultDomain(opcode: u16) u16 {
    return switch (opcode) {
        contract.OP_FILE_WRITE,
        contract.OP_FILE_STAT,
        contract.OP_FILE_READ,
        contract.OP_FILE_REMOVE,
        contract.OP_FILE_RENAME,
        contract.OP_FILE_READDIR,
        contract.OP_MOUNT,
        => @intCast(contract.ERROR_PATH),
        contract.OP_OBJECT => @intCast(contract.ERROR_OBJECT),
        contract.OP_REF, contract.OP_REF_TRANSACTION => @intCast(contract.ERROR_REFERENCE),
        contract.OP_ADD => @intCast(contract.ERROR_INDEX),
        contract.OP_CHECKOUT, contract.OP_STATUS, contract.OP_DIFF, contract.OP_COMMIT, contract.OP_MERGE => @intCast(contract.ERROR_WORKTREE),
        contract.OP_PACK_IMPORT, contract.OP_PACK_BUILD => @intCast(contract.ERROR_PACK),
        contract.OP_CLONE, contract.OP_FETCH, contract.OP_PULL, contract.OP_PUSH => @intCast(contract.ERROR_REMOTE),
        contract.OP_CHECKPOINT, contract.OP_RESTORE => @intCast(contract.ERROR_PERSISTENCE),
        else => @intCast(contract.ERROR_REPOSITORY),
    };
}

fn path(code: u16) Classification {
    return domainCode(contract.ERROR_PATH, code);
}

fn usage(code: u16) Classification {
    return domainCode(contract.ERROR_USAGE, code);
}

fn domainCode(domain: anytype, code: u16) Classification {
    return .{ .domain = @intCast(domain), .code = code };
}

fn retried(domain: anytype, code: u16, retry: anytype) Classification {
    return .{ .domain = @intCast(domain), .code = code, .retry = @intCast(retry) };
}

test "typed error classification preserves path reference limit and persistence domains" {
    const std = @import("std");
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_PATH)), classify(error.NotExist, contract.OP_FILE_READ).domain);
    try std.testing.expectEqual(Code.missing, classify(error.NotExist, contract.OP_FILE_READ).code);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_REFERENCE)), classify(error.ReferenceHasChanged, contract.OP_REF).domain);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_LIMIT)), classify(error.OutOfMemory, contract.OP_OBJECT).domain);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_PERSISTENCE)), classify(error.InvalidTransactionJournal, contract.OP_REPOSITORY_OPEN).domain);
    const not_ff = classify(error.NotFastForward, contract.OP_MERGE);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_REFERENCE)), not_ff.domain);
    try std.testing.expectEqual(Code.conflict, not_ff.code);
    try std.testing.expectEqual(@as(u16, @intCast(contract.RETRY_NEVER)), not_ff.retry);
    const dirty = classify(error.DirtyWorktree, contract.OP_MERGE);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_WORKTREE)), dirty.domain);
    try std.testing.expectEqual(@as(u16, @intCast(contract.RETRY_AFTER_INPUT)), dirty.retry);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_WORKTREE)), classify(error.MissingMessage, contract.OP_COMMIT).domain);
    try std.testing.expectEqual(@as(u16, @intCast(contract.ERROR_USAGE)), classify(error.NoMergeHeads, contract.OP_MERGE).domain);
}
