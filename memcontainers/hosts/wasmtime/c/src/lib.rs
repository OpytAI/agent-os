//! Native ABI admission, runtime identity, and staged product interfaces.
//!
//! Runtime IDs live in a process table, not at caller-held addresses. Closing a
//! runtime cannot free memory that a concurrent call has already borrowed.

use std::ffi::c_void;
use std::mem::{align_of, offset_of, size_of};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::{Arc, Mutex, OnceLock};

use embedder_rust as contract;

mod operation_api;
mod operations;
mod output_api;
mod resources;
mod snapshot_api;
mod vm_api;
use resources::RuntimeState;

type ResultCode = u32;
type RuntimeHandle = u64;

const RUNTIME_KIND: u8 = 1;
const MAX_NAMESPACE: u32 = u16::MAX as u32;
const MAX_INDEX: usize = (1 << 20) - 1;
const MAX_GENERATION: u32 = (1 << 20) - 1;
const INDEX_MASK: u64 = (1 << 20) - 1;
const GENERATION_MASK: u64 = (1 << 20) - 1;
const MAX_INPUT_STRUCT_SIZE: usize = 4096;
const FEATURE_BITS: u64 = contract::MC_HOST_FEATURE_VM_BASIC
    | contract::MC_HOST_FEATURE_OUTPUT
    | contract::MC_HOST_FEATURE_SNAPSHOT
    | contract::MC_HOST_FEATURE_EXEC_OPERATION
    | contract::MC_HOST_FEATURE_FS_OPERATION
    | contract::MC_HOST_FEATURE_AUTOCOMPLETE_OPERATION
    | contract::MC_HOST_FEATURE_SERVICE_OPERATION
    | contract::MC_HOST_FEATURE_COMMIT_OPERATION;

#[repr(C)]
#[derive(Clone, Copy)]
struct Header {
    size: u32,
    version: u16,
    flags: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct Buffer {
    data: *mut u8,
    capacity: u64,
    required: u64,
}

#[repr(C)]
struct Error {
    header: Header,
    domain: u32,
    code: i32,
    retryable: u8,
    reserved: [u8; 7],
    message: Buffer,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct RuntimeOptions {
    header: Header,
}

#[repr(C)]
struct LibraryInfo {
    header: Header,
    abi_major: u16,
    abi_minor: u16,
    implementation_table_size: u32,
    feature_bits: u64,
}

#[repr(C)]
struct Api {
    header: Header,
    abi_major: u16,
    abi_minor: u16,
    implementation_table_size: u32,
    feature_bits: u64,
    runtime_create:
        unsafe extern "C" fn(*const RuntimeOptions, *mut RuntimeHandle, *mut Error) -> ResultCode,
    runtime_close: unsafe extern "C" fn(RuntimeHandle, *mut Error) -> ResultCode,
    library_info: unsafe extern "C" fn(*mut LibraryInfo, *mut Error) -> ResultCode,
    get_interface:
        unsafe extern "C" fn(RuntimeHandle, u32, u16, u32, *mut c_void, *mut Error) -> ResultCode,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Bytes {
    data: *const u8,
    len: u64,
}

#[repr(C)]
struct KernelInfo {
    header: Header,
    digest: [u8; 32],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct VmCreateOptions {
    header: Header,
    layers: *const Bytes,
    layer_count: u64,
    contract_tier: i32,
    contract_budget_mib: i32,
    contract_fuel: i64,
    requested_workers: i32,
    flags: u32,
    terminal_cols: u32,
    terminal_rows: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct VmRestoreOptions {
    header: Header,
    snapshot: Bytes,
    baseline: Bytes,
    mode: u32,
    reserved: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct DriveOptions {
    header: Header,
    max_ticks: u32,
    reserved: u32,
    stop_mask: u64,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct VmStatus {
    header: Header,
    state: u32,
    last_tick: u32,
    flags: u32,
    exit_code: i32,
    output_records: u64,
    output_bytes: u64,
    output_dropped_bytes: u64,
    inflight_egress: i32,
    pending_commits: i32,
    workers: u32,
    reserved: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct DriveResult {
    header: Header,
    ticks: u32,
    stop_reason: u32,
    status: VmStatus,
}

#[repr(C)]
struct VmApi {
    header: Header,
    kernel_load: unsafe extern "C" fn(
        RuntimeHandle,
        Bytes,
        *mut u64,
        *mut KernelInfo,
        *mut Error,
    ) -> ResultCode,
    kernel_close: unsafe extern "C" fn(RuntimeHandle, u64, *mut Error) -> ResultCode,
    vm_create: unsafe extern "C" fn(
        RuntimeHandle,
        u64,
        u64,
        *const VmCreateOptions,
        *mut u64,
        *mut Error,
    ) -> ResultCode,
    vm_restore: unsafe extern "C" fn(
        RuntimeHandle,
        u64,
        u64,
        *const VmRestoreOptions,
        *mut u64,
        *mut Error,
    ) -> ResultCode,
    vm_status: unsafe extern "C" fn(RuntimeHandle, u64, *mut VmStatus, *mut Error) -> ResultCode,
    vm_drive: unsafe extern "C" fn(
        RuntimeHandle,
        u64,
        *const DriveOptions,
        *mut DriveResult,
        *mut Error,
    ) -> ResultCode,
    vm_close: unsafe extern "C" fn(RuntimeHandle, u64, *mut Error) -> ResultCode,
    vm_cancel_and_close: unsafe extern "C" fn(RuntimeHandle, u64, *mut Error) -> ResultCode,
    terminal_input: unsafe extern "C" fn(RuntimeHandle, u64, Bytes, *mut Error) -> ResultCode,
    terminal_resize: unsafe extern "C" fn(RuntimeHandle, u64, u32, u32, *mut Error) -> ResultCode,
}

#[repr(C)]
struct OutputRecordInfo {
    header: Header,
    kind: u32,
    channel: u32,
    sequence: u64,
    byte_offset: u64,
    length: u64,
    first_lost_sequence: u64,
    last_lost_sequence: u64,
    byte_end: u64,
}

#[repr(C)]
struct OutputStatsInfo {
    header: Header,
    records: u64,
    retained_bytes: u64,
    dropped_bytes: u64,
    leased_records: u64,
    flags: u32,
    reserved: u32,
}

#[repr(C)]
struct OutputApi {
    header: Header,
    next: unsafe extern "C" fn(u64, u64, *mut u64, *mut OutputRecordInfo, *mut Error) -> ResultCode,
    read: unsafe extern "C" fn(u64, u64, u64, *mut Buffer, *mut u64, *mut Error) -> ResultCode,
    ack: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    release: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    stats: unsafe extern "C" fn(u64, u64, *mut OutputStatsInfo, *mut Error) -> ResultCode,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SnapshotBeginOptions {
    header: Header,
    mode: u32,
    reserved: u32,
    baseline: u64,
}

#[repr(C)]
struct SnapshotInfo {
    header: Header,
    kind: u32,
    mcsn_version: u32,
    kernel_digest: [u8; 32],
    baseline_id: [u8; 32],
    content_digest: [u8; 32],
    length: u64,
    memory_len: u64,
}

#[repr(C)]
struct SnapshotApi {
    header: Header,
    baseline_import: unsafe extern "C" fn(
        u64,
        u64,
        Bytes,
        *mut u64,
        *mut SnapshotInfo,
        *mut Error,
    ) -> ResultCode,
    baseline_close: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    begin: unsafe extern "C" fn(
        u64,
        u64,
        *const SnapshotBeginOptions,
        *mut u64,
        *mut SnapshotInfo,
        *mut Error,
    ) -> ResultCode,
    read: unsafe extern "C" fn(u64, u64, u64, *mut Buffer, *mut u64, *mut Error) -> ResultCode,
    finish: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    cancel: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    inspect: unsafe extern "C" fn(u64, Bytes, Bytes, *mut SnapshotInfo, *mut Error) -> ResultCode,
}

#[repr(C)]
struct OperationStatus {
    header: Header,
    kind: u32,
    state: u32,
    error: u32,
    reserved: u32,
    result_length: u64,
}

#[repr(C)]
struct OperationApi {
    header: Header,
    start: unsafe extern "C" fn(u64, u64, Bytes, *mut u64, *mut Error) -> ResultCode,
    poll: unsafe extern "C" fn(u64, u64, *mut OperationStatus, *mut Error) -> ResultCode,
    result: unsafe extern "C" fn(u64, u64, *mut Buffer, *mut Error) -> ResultCode,
    peek: unsafe extern "C" fn(u64, u64, u32, u64, *mut Buffer, *mut u64, *mut Error) -> ResultCode,
    cancel: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
    close: unsafe extern "C" fn(u64, u64, *mut Error) -> ResultCode,
}

// The manifest in embedder.kdl is checked independently by Rust and C. A
// field reorder or platform-specific padding change fails compilation.
const _: () = {
    assert!(size_of::<Header>() == contract::MC_HOST_LAYOUT_HEADER_SIZE);
    assert!(align_of::<Header>() == contract::MC_HOST_LAYOUT_HEADER_ALIGN);
    assert!(offset_of!(Header, size) == contract::MC_HOST_LAYOUT_HEADER_SIZE_OFFSET);
    assert!(offset_of!(Header, version) == contract::MC_HOST_LAYOUT_HEADER_VERSION_OFFSET);
    assert!(offset_of!(Header, flags) == contract::MC_HOST_LAYOUT_HEADER_FLAGS_OFFSET);

    assert!(size_of::<Buffer>() == contract::MC_HOST_LAYOUT_BUFFER_SIZE);
    assert!(align_of::<Buffer>() == contract::MC_HOST_LAYOUT_BUFFER_ALIGN);
    assert!(offset_of!(Buffer, data) == contract::MC_HOST_LAYOUT_BUFFER_DATA_OFFSET);
    assert!(offset_of!(Buffer, capacity) == contract::MC_HOST_LAYOUT_BUFFER_CAPACITY_OFFSET);
    assert!(offset_of!(Buffer, required) == contract::MC_HOST_LAYOUT_BUFFER_REQUIRED_OFFSET);

    assert!(size_of::<Error>() == contract::MC_HOST_LAYOUT_ERROR_SIZE);
    assert!(align_of::<Error>() == contract::MC_HOST_LAYOUT_ERROR_ALIGN);
    assert!(offset_of!(Error, header) == contract::MC_HOST_LAYOUT_ERROR_HEADER_OFFSET);
    assert!(offset_of!(Error, domain) == contract::MC_HOST_LAYOUT_ERROR_DOMAIN_OFFSET);
    assert!(offset_of!(Error, code) == contract::MC_HOST_LAYOUT_ERROR_CODE_OFFSET);
    assert!(offset_of!(Error, retryable) == contract::MC_HOST_LAYOUT_ERROR_RETRYABLE_OFFSET);
    assert!(offset_of!(Error, reserved) == contract::MC_HOST_LAYOUT_ERROR_RESERVED_OFFSET);
    assert!(offset_of!(Error, message) == contract::MC_HOST_LAYOUT_ERROR_MESSAGE_OFFSET);

    assert!(size_of::<RuntimeOptions>() == contract::MC_HOST_LAYOUT_RUNTIME_OPTIONS_SIZE);
    assert!(align_of::<RuntimeOptions>() == contract::MC_HOST_LAYOUT_RUNTIME_OPTIONS_ALIGN);
    assert!(
        offset_of!(RuntimeOptions, header)
            == contract::MC_HOST_LAYOUT_RUNTIME_OPTIONS_HEADER_OFFSET
    );

    assert!(size_of::<LibraryInfo>() == contract::MC_HOST_LAYOUT_LIBRARY_INFO_SIZE);
    assert!(align_of::<LibraryInfo>() == contract::MC_HOST_LAYOUT_LIBRARY_INFO_ALIGN);
    assert!(offset_of!(LibraryInfo, header) == contract::MC_HOST_LAYOUT_LIBRARY_INFO_HEADER_OFFSET);
    assert!(
        offset_of!(LibraryInfo, abi_major)
            == contract::MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MAJOR_OFFSET
    );
    assert!(
        offset_of!(LibraryInfo, abi_minor)
            == contract::MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MINOR_OFFSET
    );
    assert!(
        offset_of!(LibraryInfo, implementation_table_size)
            == contract::MC_HOST_LAYOUT_LIBRARY_INFO_IMPLEMENTATION_TABLE_SIZE_OFFSET
    );
    assert!(
        offset_of!(LibraryInfo, feature_bits)
            == contract::MC_HOST_LAYOUT_LIBRARY_INFO_FEATURE_BITS_OFFSET
    );

    assert!(size_of::<Api>() == contract::MC_HOST_LAYOUT_API_SIZE);
    assert!(align_of::<Api>() == contract::MC_HOST_LAYOUT_API_ALIGN);
    assert!(offset_of!(Api, header) == contract::MC_HOST_LAYOUT_API_HEADER_OFFSET);
    assert!(offset_of!(Api, abi_major) == contract::MC_HOST_LAYOUT_API_ABI_MAJOR_OFFSET);
    assert!(offset_of!(Api, abi_minor) == contract::MC_HOST_LAYOUT_API_ABI_MINOR_OFFSET);
    assert!(
        offset_of!(Api, implementation_table_size)
            == contract::MC_HOST_LAYOUT_API_IMPLEMENTATION_TABLE_SIZE_OFFSET
    );
    assert!(offset_of!(Api, feature_bits) == contract::MC_HOST_LAYOUT_API_FEATURE_BITS_OFFSET);
    assert!(offset_of!(Api, runtime_create) == contract::MC_HOST_LAYOUT_API_RUNTIME_CREATE_OFFSET);
    assert!(offset_of!(Api, runtime_close) == contract::MC_HOST_LAYOUT_API_RUNTIME_CLOSE_OFFSET);
    assert!(offset_of!(Api, library_info) == contract::MC_HOST_LAYOUT_API_LIBRARY_INFO_OFFSET);
    assert!(offset_of!(Api, get_interface) == contract::MC_HOST_LAYOUT_API_GET_INTERFACE_OFFSET);
    assert!(size_of::<Bytes>() == contract::MC_HOST_LAYOUT_BYTES_SIZE);
    assert!(align_of::<Bytes>() == contract::MC_HOST_LAYOUT_BYTES_ALIGN);
    assert!(offset_of!(Bytes, data) == contract::MC_HOST_LAYOUT_BYTES_DATA_OFFSET);
    assert!(offset_of!(Bytes, len) == contract::MC_HOST_LAYOUT_BYTES_LEN_OFFSET);

    assert!(size_of::<KernelInfo>() == contract::MC_HOST_LAYOUT_KERNEL_INFO_SIZE);
    assert!(align_of::<KernelInfo>() == contract::MC_HOST_LAYOUT_KERNEL_INFO_ALIGN);
    assert!(offset_of!(KernelInfo, header) == contract::MC_HOST_LAYOUT_KERNEL_INFO_HEADER_OFFSET);
    assert!(offset_of!(KernelInfo, digest) == contract::MC_HOST_LAYOUT_KERNEL_INFO_DIGEST_OFFSET);

    assert!(size_of::<VmCreateOptions>() == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_SIZE);
    assert!(align_of::<VmCreateOptions>() == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_ALIGN);
    assert!(
        offset_of!(VmCreateOptions, header)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_HEADER_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, layers)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYERS_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, layer_count)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYER_COUNT_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, contract_tier)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_TIER_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, contract_budget_mib)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_BUDGET_MIB_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, contract_fuel)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_FUEL_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, requested_workers)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_REQUESTED_WORKERS_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, flags)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_FLAGS_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, terminal_cols)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_COLS_OFFSET
    );
    assert!(
        offset_of!(VmCreateOptions, terminal_rows)
            == contract::MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_ROWS_OFFSET
    );

    assert!(size_of::<VmRestoreOptions>() == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SIZE);
    assert!(align_of::<VmRestoreOptions>() == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_ALIGN);
    assert!(
        offset_of!(VmRestoreOptions, header)
            == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_HEADER_OFFSET
    );
    assert!(
        offset_of!(VmRestoreOptions, snapshot)
            == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SNAPSHOT_OFFSET
    );
    assert!(
        offset_of!(VmRestoreOptions, baseline)
            == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_BASELINE_OFFSET
    );
    assert!(
        offset_of!(VmRestoreOptions, mode)
            == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_MODE_OFFSET
    );
    assert!(
        offset_of!(VmRestoreOptions, reserved)
            == contract::MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_RESERVED_OFFSET
    );

    assert!(size_of::<DriveOptions>() == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_SIZE);
    assert!(align_of::<DriveOptions>() == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_ALIGN);
    assert!(
        offset_of!(DriveOptions, header) == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_HEADER_OFFSET
    );
    assert!(
        offset_of!(DriveOptions, max_ticks)
            == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_MAX_TICKS_OFFSET
    );
    assert!(
        offset_of!(DriveOptions, reserved)
            == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_RESERVED_OFFSET
    );
    assert!(
        offset_of!(DriveOptions, stop_mask)
            == contract::MC_HOST_LAYOUT_DRIVE_OPTIONS_STOP_MASK_OFFSET
    );

    assert!(size_of::<VmStatus>() == contract::MC_HOST_LAYOUT_VM_STATUS_SIZE);
    assert!(align_of::<VmStatus>() == contract::MC_HOST_LAYOUT_VM_STATUS_ALIGN);
    assert!(offset_of!(VmStatus, header) == contract::MC_HOST_LAYOUT_VM_STATUS_HEADER_OFFSET);
    assert!(offset_of!(VmStatus, state) == contract::MC_HOST_LAYOUT_VM_STATUS_STATE_OFFSET);
    assert!(offset_of!(VmStatus, last_tick) == contract::MC_HOST_LAYOUT_VM_STATUS_LAST_TICK_OFFSET);
    assert!(offset_of!(VmStatus, flags) == contract::MC_HOST_LAYOUT_VM_STATUS_FLAGS_OFFSET);
    assert!(offset_of!(VmStatus, exit_code) == contract::MC_HOST_LAYOUT_VM_STATUS_EXIT_CODE_OFFSET);
    assert!(
        offset_of!(VmStatus, output_records)
            == contract::MC_HOST_LAYOUT_VM_STATUS_OUTPUT_RECORDS_OFFSET
    );
    assert!(
        offset_of!(VmStatus, output_bytes)
            == contract::MC_HOST_LAYOUT_VM_STATUS_OUTPUT_BYTES_OFFSET
    );
    assert!(
        offset_of!(VmStatus, output_dropped_bytes)
            == contract::MC_HOST_LAYOUT_VM_STATUS_OUTPUT_DROPPED_BYTES_OFFSET
    );
    assert!(
        offset_of!(VmStatus, inflight_egress)
            == contract::MC_HOST_LAYOUT_VM_STATUS_INFLIGHT_EGRESS_OFFSET
    );
    assert!(
        offset_of!(VmStatus, pending_commits)
            == contract::MC_HOST_LAYOUT_VM_STATUS_PENDING_COMMITS_OFFSET
    );
    assert!(offset_of!(VmStatus, workers) == contract::MC_HOST_LAYOUT_VM_STATUS_WORKERS_OFFSET);
    assert!(offset_of!(VmStatus, reserved) == contract::MC_HOST_LAYOUT_VM_STATUS_RESERVED_OFFSET);

    assert!(size_of::<DriveResult>() == contract::MC_HOST_LAYOUT_DRIVE_RESULT_SIZE);
    assert!(align_of::<DriveResult>() == contract::MC_HOST_LAYOUT_DRIVE_RESULT_ALIGN);
    assert!(offset_of!(DriveResult, header) == contract::MC_HOST_LAYOUT_DRIVE_RESULT_HEADER_OFFSET);
    assert!(offset_of!(DriveResult, ticks) == contract::MC_HOST_LAYOUT_DRIVE_RESULT_TICKS_OFFSET);
    assert!(
        offset_of!(DriveResult, stop_reason)
            == contract::MC_HOST_LAYOUT_DRIVE_RESULT_STOP_REASON_OFFSET
    );
    assert!(offset_of!(DriveResult, status) == contract::MC_HOST_LAYOUT_DRIVE_RESULT_STATUS_OFFSET);

    assert!(size_of::<VmApi>() == contract::MC_HOST_LAYOUT_VM_API_SIZE);
    assert!(align_of::<VmApi>() == contract::MC_HOST_LAYOUT_VM_API_ALIGN);
    assert!(offset_of!(VmApi, header) == contract::MC_HOST_LAYOUT_VM_API_HEADER_OFFSET);
    assert!(offset_of!(VmApi, kernel_load) == contract::MC_HOST_LAYOUT_VM_API_KERNEL_LOAD_OFFSET);
    assert!(offset_of!(VmApi, kernel_close) == contract::MC_HOST_LAYOUT_VM_API_KERNEL_CLOSE_OFFSET);
    assert!(offset_of!(VmApi, vm_create) == contract::MC_HOST_LAYOUT_VM_API_VM_CREATE_OFFSET);
    assert!(offset_of!(VmApi, vm_restore) == contract::MC_HOST_LAYOUT_VM_API_VM_RESTORE_OFFSET);
    assert!(offset_of!(VmApi, vm_status) == contract::MC_HOST_LAYOUT_VM_API_VM_STATUS_OFFSET);
    assert!(offset_of!(VmApi, vm_drive) == contract::MC_HOST_LAYOUT_VM_API_VM_DRIVE_OFFSET);
    assert!(offset_of!(VmApi, vm_close) == contract::MC_HOST_LAYOUT_VM_API_VM_CLOSE_OFFSET);
    assert!(
        offset_of!(VmApi, vm_cancel_and_close)
            == contract::MC_HOST_LAYOUT_VM_API_VM_CANCEL_AND_CLOSE_OFFSET
    );
    assert!(
        offset_of!(VmApi, terminal_input) == contract::MC_HOST_LAYOUT_VM_API_TERMINAL_INPUT_OFFSET
    );
    assert!(
        offset_of!(VmApi, terminal_resize)
            == contract::MC_HOST_LAYOUT_VM_API_TERMINAL_RESIZE_OFFSET
    );
    assert!(size_of::<OutputRecordInfo>() == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_SIZE);
    assert!(align_of::<OutputRecordInfo>() == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_ALIGN);
    assert!(
        offset_of!(OutputRecordInfo, header)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_HEADER_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, kind) == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_KIND_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, channel)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_CHANNEL_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, sequence)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_SEQUENCE_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, byte_offset)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_OFFSET_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, length)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_LENGTH_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, first_lost_sequence)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_FIRST_LOST_SEQUENCE_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, last_lost_sequence)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_LAST_LOST_SEQUENCE_OFFSET
    );
    assert!(
        offset_of!(OutputRecordInfo, byte_end)
            == contract::MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_END_OFFSET
    );

    assert!(size_of::<OutputStatsInfo>() == contract::MC_HOST_LAYOUT_OUTPUT_STATS_SIZE);
    assert!(align_of::<OutputStatsInfo>() == contract::MC_HOST_LAYOUT_OUTPUT_STATS_ALIGN);
    assert!(
        offset_of!(OutputStatsInfo, header) == contract::MC_HOST_LAYOUT_OUTPUT_STATS_HEADER_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, records)
            == contract::MC_HOST_LAYOUT_OUTPUT_STATS_RECORDS_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, retained_bytes)
            == contract::MC_HOST_LAYOUT_OUTPUT_STATS_RETAINED_BYTES_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, dropped_bytes)
            == contract::MC_HOST_LAYOUT_OUTPUT_STATS_DROPPED_BYTES_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, leased_records)
            == contract::MC_HOST_LAYOUT_OUTPUT_STATS_LEASED_RECORDS_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, flags) == contract::MC_HOST_LAYOUT_OUTPUT_STATS_FLAGS_OFFSET
    );
    assert!(
        offset_of!(OutputStatsInfo, reserved)
            == contract::MC_HOST_LAYOUT_OUTPUT_STATS_RESERVED_OFFSET
    );

    assert!(size_of::<OutputApi>() == contract::MC_HOST_LAYOUT_OUTPUT_API_SIZE);
    assert!(align_of::<OutputApi>() == contract::MC_HOST_LAYOUT_OUTPUT_API_ALIGN);
    assert!(offset_of!(OutputApi, header) == contract::MC_HOST_LAYOUT_OUTPUT_API_HEADER_OFFSET);
    assert!(offset_of!(OutputApi, next) == contract::MC_HOST_LAYOUT_OUTPUT_API_NEXT_OFFSET);
    assert!(offset_of!(OutputApi, read) == contract::MC_HOST_LAYOUT_OUTPUT_API_READ_OFFSET);
    assert!(offset_of!(OutputApi, ack) == contract::MC_HOST_LAYOUT_OUTPUT_API_ACK_OFFSET);
    assert!(offset_of!(OutputApi, release) == contract::MC_HOST_LAYOUT_OUTPUT_API_RELEASE_OFFSET);
    assert!(offset_of!(OutputApi, stats) == contract::MC_HOST_LAYOUT_OUTPUT_API_STATS_OFFSET);
    assert!(
        size_of::<SnapshotBeginOptions>() == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_SIZE
    );
    assert!(
        align_of::<SnapshotBeginOptions>() == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_ALIGN
    );
    assert!(
        offset_of!(SnapshotBeginOptions, header)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_HEADER_OFFSET
    );
    assert!(
        offset_of!(SnapshotBeginOptions, mode)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_MODE_OFFSET
    );
    assert!(
        offset_of!(SnapshotBeginOptions, reserved)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_RESERVED_OFFSET
    );
    assert!(
        offset_of!(SnapshotBeginOptions, baseline)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_BASELINE_OFFSET
    );

    assert!(size_of::<SnapshotInfo>() == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_SIZE);
    assert!(align_of::<SnapshotInfo>() == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_ALIGN);
    assert!(
        offset_of!(SnapshotInfo, header) == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_HEADER_OFFSET
    );
    assert!(offset_of!(SnapshotInfo, kind) == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_KIND_OFFSET);
    assert!(
        offset_of!(SnapshotInfo, mcsn_version)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_MCSN_VERSION_OFFSET
    );
    assert!(
        offset_of!(SnapshotInfo, kernel_digest)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_KERNEL_DIGEST_OFFSET
    );
    assert!(
        offset_of!(SnapshotInfo, baseline_id)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_BASELINE_ID_OFFSET
    );
    assert!(
        offset_of!(SnapshotInfo, content_digest)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_CONTENT_DIGEST_OFFSET
    );
    assert!(
        offset_of!(SnapshotInfo, length) == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_LENGTH_OFFSET
    );
    assert!(
        offset_of!(SnapshotInfo, memory_len)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_INFO_MEMORY_LEN_OFFSET
    );

    assert!(size_of::<SnapshotApi>() == contract::MC_HOST_LAYOUT_SNAPSHOT_API_SIZE);
    assert!(align_of::<SnapshotApi>() == contract::MC_HOST_LAYOUT_SNAPSHOT_API_ALIGN);
    assert!(offset_of!(SnapshotApi, header) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_HEADER_OFFSET);
    assert!(
        offset_of!(SnapshotApi, baseline_import)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_IMPORT_OFFSET
    );
    assert!(
        offset_of!(SnapshotApi, baseline_close)
            == contract::MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_CLOSE_OFFSET
    );
    assert!(offset_of!(SnapshotApi, begin) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_BEGIN_OFFSET);
    assert!(offset_of!(SnapshotApi, read) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_READ_OFFSET);
    assert!(offset_of!(SnapshotApi, finish) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_FINISH_OFFSET);
    assert!(offset_of!(SnapshotApi, cancel) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_CANCEL_OFFSET);
    assert!(
        offset_of!(SnapshotApi, inspect) == contract::MC_HOST_LAYOUT_SNAPSHOT_API_INSPECT_OFFSET
    );
    assert!(size_of::<OperationStatus>() == contract::MC_HOST_LAYOUT_OPERATION_STATUS_SIZE);
    assert!(align_of::<OperationStatus>() == contract::MC_HOST_LAYOUT_OPERATION_STATUS_ALIGN);
    assert!(
        offset_of!(OperationStatus, header)
            == contract::MC_HOST_LAYOUT_OPERATION_STATUS_HEADER_OFFSET
    );
    assert!(
        offset_of!(OperationStatus, kind) == contract::MC_HOST_LAYOUT_OPERATION_STATUS_KIND_OFFSET
    );
    assert!(
        offset_of!(OperationStatus, state)
            == contract::MC_HOST_LAYOUT_OPERATION_STATUS_STATE_OFFSET
    );
    assert!(
        offset_of!(OperationStatus, error)
            == contract::MC_HOST_LAYOUT_OPERATION_STATUS_ERROR_OFFSET
    );
    assert!(
        offset_of!(OperationStatus, reserved)
            == contract::MC_HOST_LAYOUT_OPERATION_STATUS_RESERVED_OFFSET
    );
    assert!(
        offset_of!(OperationStatus, result_length)
            == contract::MC_HOST_LAYOUT_OPERATION_STATUS_RESULT_LENGTH_OFFSET
    );

    assert!(size_of::<OperationApi>() == contract::MC_HOST_LAYOUT_OPERATION_API_SIZE);
    assert!(align_of::<OperationApi>() == contract::MC_HOST_LAYOUT_OPERATION_API_ALIGN);
    assert!(
        offset_of!(OperationApi, header) == contract::MC_HOST_LAYOUT_OPERATION_API_HEADER_OFFSET
    );
    assert!(offset_of!(OperationApi, start) == contract::MC_HOST_LAYOUT_OPERATION_API_START_OFFSET);
    assert!(offset_of!(OperationApi, poll) == contract::MC_HOST_LAYOUT_OPERATION_API_POLL_OFFSET);
    assert!(
        offset_of!(OperationApi, result) == contract::MC_HOST_LAYOUT_OPERATION_API_RESULT_OFFSET
    );
    assert!(offset_of!(OperationApi, peek) == contract::MC_HOST_LAYOUT_OPERATION_API_PEEK_OFFSET);
    assert!(
        offset_of!(OperationApi, cancel) == contract::MC_HOST_LAYOUT_OPERATION_API_CANCEL_OFFSET
    );
    assert!(offset_of!(OperationApi, close) == contract::MC_HOST_LAYOUT_OPERATION_API_CLOSE_OFFSET);
};

struct RuntimeSlot {
    namespace: u16,
    generation: u32,
    open: bool,
    active_calls: usize,
    retired: bool,
    state: Option<std::sync::Arc<RuntimeState>>,
}

#[derive(Default)]
struct RuntimeTable {
    slots: Vec<RuntimeSlot>,
    free: Vec<usize>,
    next_namespace: u32,
}

impl RuntimeTable {
    fn insert(&mut self) -> Result<RuntimeHandle, ResultCode> {
        let namespace = self.next_namespace.saturating_add(1);
        if namespace > MAX_NAMESPACE {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let index = if let Some(index) = self.free.pop() {
            index
        } else {
            if self.slots.len() >= MAX_INDEX {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            self.slots.push(RuntimeSlot {
                namespace: 0,
                generation: 1,
                open: false,
                active_calls: 0,
                retired: false,
                state: None,
            });
            self.slots.len() - 1
        };
        let slot = &mut self.slots[index];
        if slot.open || slot.retired || slot.active_calls != 0 {
            return Err(contract::MC_HOST_INTERNAL);
        }
        slot.namespace = namespace as u16;
        slot.open = true;
        slot.state = Some(std::sync::Arc::new(RuntimeState::new(slot.namespace)));
        self.next_namespace = namespace;
        Ok(pack(slot.namespace, slot.generation, index))
    }

    fn slot_mut(&mut self, handle: RuntimeHandle) -> Result<&mut RuntimeSlot, ResultCode> {
        let (namespace, kind, generation, index) =
            unpack(handle).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if kind != RUNTIME_KIND {
            return Err(contract::MC_HOST_WRONG_KIND);
        }
        let slot = self
            .slots
            .get_mut(index)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if !slot.open || slot.namespace != namespace || slot.generation != generation {
            return Err(contract::MC_HOST_STALE_HANDLE);
        }
        Ok(slot)
    }

    fn close(&mut self, handle: RuntimeHandle) -> Result<(), ResultCode> {
        let (_, _, _, index) = unpack(handle).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        let slot = self.slot_mut(handle)?;
        if slot.active_calls != 0 || slot.state.as_ref().is_some_and(|state| !state.is_empty()) {
            return Err(contract::MC_HOST_BUSY);
        }
        slot.open = false;
        slot.state = None;
        if slot.generation == MAX_GENERATION {
            slot.retired = true;
        } else {
            slot.generation += 1;
            self.free.push(index);
        }
        Ok(())
    }
}

fn pack_kind(namespace: u16, kind: u8, generation: u32, index: usize) -> RuntimeHandle {
    ((namespace as u64) << 48)
        | ((kind as u64) << 40)
        | ((generation as u64) << 20)
        | (index as u64 + 1)
}

fn pack(namespace: u16, generation: u32, index: usize) -> RuntimeHandle {
    pack_kind(namespace, RUNTIME_KIND, generation, index)
}

fn unpack(handle: RuntimeHandle) -> Option<(u16, u8, u32, usize)> {
    let index = (handle & INDEX_MASK) as usize;
    let generation = ((handle >> 20) & GENERATION_MASK) as u32;
    if index == 0 || generation == 0 {
        return None;
    }
    Some((
        (handle >> 48) as u16,
        ((handle >> 40) & 0xff) as u8,
        generation,
        index - 1,
    ))
}

fn runtimes() -> &'static Mutex<RuntimeTable> {
    static TABLE: OnceLock<Mutex<RuntimeTable>> = OnceLock::new();
    TABLE.get_or_init(|| Mutex::new(RuntimeTable::default()))
}

struct ActiveRuntime {
    handle: RuntimeHandle,
    state: Arc<RuntimeState>,
}

impl ActiveRuntime {
    fn acquire(handle: RuntimeHandle) -> Result<Self, ResultCode> {
        let mut table = runtimes().lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let slot = table.slot_mut(handle)?;
        slot.active_calls = slot
            .active_calls
            .checked_add(1)
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        let state = Arc::clone(slot.state.as_ref().ok_or(contract::MC_HOST_INTERNAL)?);
        Ok(Self { handle, state })
    }
}

impl Drop for ActiveRuntime {
    fn drop(&mut self) {
        if let Ok(mut table) = runtimes().lock() {
            if let Ok(slot) = table.slot_mut(self.handle) {
                slot.active_calls -= 1;
            }
        }
    }
}

fn valid_header(header: Header, minimum: usize) -> bool {
    header.version == 1 && header.flags == 0 && header.size as usize >= minimum
}

fn ranges_overlap(a: usize, a_len: usize, b: usize, b_len: usize) -> bool {
    match (a.checked_add(a_len), b.checked_add(b_len)) {
        (Some(a_end), Some(b_end)) => a < b_end && b < a_end,
        _ => true,
    }
}

unsafe fn validate_options(options: *const RuntimeOptions) -> Result<(), ResultCode> {
    if options.is_null() {
        return Ok(());
    }
    if (options as usize) % align_of::<Header>() != 0 {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let header = unsafe { (*options).header };
    if !valid_header(header, size_of::<RuntimeOptions>())
        || header.size as usize > MAX_INPUT_STRUCT_SIZE
    {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let tail_len = header.size as usize - size_of::<RuntimeOptions>();
    if tail_len != 0 {
        let tail = unsafe {
            std::slice::from_raw_parts(
                (options as *const u8).add(size_of::<RuntimeOptions>()),
                tail_len,
            )
        };
        if tail.iter().any(|byte| *byte != 0) {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
    }
    Ok(())
}

unsafe fn write_error(error: *mut Error, code: ResultCode, message: &str) {
    if error.is_null() {
        return;
    }
    if (error as usize) % align_of::<Header>() != 0 {
        return;
    }
    let caller_header = unsafe { ptr::read(error.cast::<Header>()) };
    if !valid_header(caller_header, size_of::<Header>()) {
        return;
    }
    let count = (caller_header.size as usize).min(size_of::<Error>());
    let mut value = Error {
        header: Header {
            size: count as u32,
            version: 1,
            flags: 0,
        },
        domain: 0,
        code: code as i32,
        retryable: u8::from(code == contract::MC_HOST_BUSY),
        reserved: [0; 7],
        message: Buffer {
            data: ptr::null_mut(),
            capacity: 0,
            required: message.len() as u64,
        },
    };
    value.domain = if code == contract::MC_HOST_OK
        || code == contract::MC_HOST_WOULD_BLOCK
        || code == contract::MC_HOST_BUFFER_TOO_SMALL
    {
        0
    } else if code == contract::MC_HOST_STALE_HANDLE || code == contract::MC_HOST_WRONG_KIND {
        3
    } else {
        1
    };
    if count == size_of::<Error>() && (error as usize) % align_of::<Error>() == 0 {
        let caller_buffer = unsafe { ptr::read(ptr::addr_of!((*error).message)) };
        value.message.data = caller_buffer.data;
        value.message.capacity = caller_buffer.capacity;
        if !caller_buffer.data.is_null() && caller_buffer.capacity != 0 {
            if let Ok(capacity) = usize::try_from(caller_buffer.capacity) {
                let written = capacity.saturating_sub(1).min(message.len());
                let dst = caller_buffer.data as usize;
                let error_start = error as usize;
                let overlaps_error = dst < error_start.saturating_add(count)
                    && dst.saturating_add(written.saturating_add(1)) > error_start;
                if !overlaps_error {
                    unsafe {
                        ptr::copy_nonoverlapping(message.as_ptr(), caller_buffer.data, written);
                        *caller_buffer.data.add(written) = 0;
                    }
                }
            }
        }
    }
    unsafe {
        ptr::copy_nonoverlapping(
            (&value as *const Error).cast::<u8>(),
            error.cast::<u8>(),
            count,
        )
    };
}

fn guard(error: *mut Error, action: impl FnOnce() -> Result<(), ResultCode>) -> ResultCode {
    let outcome = catch_unwind(AssertUnwindSafe(action));
    let (code, message) = match outcome {
        Ok(Ok(())) => (contract::MC_HOST_OK, ""),
        Ok(Err(code)) => (code, "native host call rejected"),
        Err(_) => (
            contract::MC_HOST_INTERNAL,
            "panic contained at native host boundary",
        ),
    };
    unsafe { write_error(error, code, message) };
    code
}

unsafe extern "C" fn runtime_create(
    options: *const RuntimeOptions,
    out: *mut RuntimeHandle,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out.is_null() || (out as usize) % align_of::<RuntimeHandle>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if !options.is_null() {
            if (options as usize) % align_of::<Header>() != 0 {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            let option_size = (unsafe { (*options).header.size as usize }).max(size_of::<Header>());
            if ranges_overlap(
                options as usize,
                option_size,
                out as usize,
                size_of::<RuntimeHandle>(),
            ) {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
        }
        unsafe { *out = 0 };
        unsafe { validate_options(options)? };
        let handle = runtimes()
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .insert()?;
        unsafe { *out = handle };
        Ok(())
    })
}

unsafe extern "C" fn runtime_close(handle: RuntimeHandle, error: *mut Error) -> ResultCode {
    guard(error, || {
        runtimes()
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .close(handle)
    })
}

unsafe extern "C" fn library_info(out: *mut LibraryInfo, error: *mut Error) -> ResultCode {
    guard(error, || {
        if out.is_null() || (out as usize) % align_of::<Header>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let caller_header = unsafe { ptr::read(out.cast::<Header>()) };
        if !valid_header(caller_header, size_of::<Header>()) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let count = (caller_header.size as usize).min(size_of::<LibraryInfo>());
        let value = LibraryInfo {
            header: Header {
                size: count as u32,
                version: 1,
                flags: 0,
            },
            abi_major: contract::MC_HOST_ABI_MAJOR,
            abi_minor: contract::MC_HOST_ABI_MINOR,
            implementation_table_size: size_of::<Api>() as u32,
            feature_bits: FEATURE_BITS,
        };
        unsafe {
            ptr::copy_nonoverlapping(
                (&value as *const LibraryInfo).cast::<u8>(),
                out.cast::<u8>(),
                count,
            )
        };
        Ok(())
    })
}

unsafe extern "C" fn get_interface(
    handle: RuntimeHandle,
    id: u32,
    version: u16,
    size: u32,
    out: *mut c_void,
    error: *mut Error,
) -> ResultCode {
    if !error.is_null()
        && !out.is_null()
        && ranges_overlap(
            error as usize,
            size_of::<Error>(),
            out as usize,
            size as usize,
        )
    {
        return contract::MC_HOST_INVALID_ARGUMENT;
    }
    guard(error, || {
        let _active = ActiveRuntime::acquire(handle)?;
        if out.is_null()
            || (out as usize) % align_of::<Header>() != 0
            || size < size_of::<Header>() as u32
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if version != 1 {
            return Err(contract::MC_HOST_UNSUPPORTED_ABI);
        }
        let (table, implementation_size) = match id {
            contract::MC_HOST_IFACE_VM_V1 => (
                &vm_api::VM_API as *const VmApi as *const u8,
                size_of::<VmApi>(),
            ),
            contract::MC_HOST_IFACE_OUTPUT_V1 => (
                &output_api::OUTPUT_API as *const OutputApi as *const u8,
                size_of::<OutputApi>(),
            ),
            contract::MC_HOST_IFACE_SNAPSHOT_V1 => (
                &snapshot_api::SNAPSHOT_API as *const SnapshotApi as *const u8,
                size_of::<SnapshotApi>(),
            ),
            contract::MC_HOST_IFACE_OPERATION_V1 => (
                &operation_api::OPERATION_API as *const OperationApi as *const u8,
                size_of::<OperationApi>(),
            ),
            _ => return Err(contract::MC_HOST_UNSUPPORTED),
        };
        unsafe {
            ptr::copy_nonoverlapping(
                table,
                out.cast::<u8>(),
                (size as usize).min(implementation_size),
            )
        };
        Ok(())
    })
}

static API: Api = Api {
    header: Header {
        size: size_of::<Api>() as u32,
        version: 1,
        flags: 0,
    },
    abi_major: contract::MC_HOST_ABI_MAJOR,
    abi_minor: contract::MC_HOST_ABI_MINOR,
    implementation_table_size: size_of::<Api>() as u32,
    feature_bits: FEATURE_BITS,
    runtime_create,
    runtime_close,
    library_info,
    get_interface,
};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mc_host_get_api(
    major: u16,
    minimum_minor: u16,
    caller_size: u32,
    out: *mut c_void,
) -> ResultCode {
    match catch_unwind(AssertUnwindSafe(|| {
        if major != contract::MC_HOST_ABI_MAJOR || minimum_minor > contract::MC_HOST_ABI_MINOR {
            return contract::MC_HOST_UNSUPPORTED_ABI;
        }
        if out.is_null() || caller_size < size_of::<Header>() as u32 {
            return contract::MC_HOST_INVALID_ARGUMENT;
        }
        let count = (caller_size as usize).min(size_of::<Api>());
        unsafe { ptr::copy_nonoverlapping(&API as *const Api as *const u8, out as *mut u8, count) };
        contract::MC_HOST_OK
    })) {
        Ok(code) => code,
        Err(_) => contract::MC_HOST_INTERNAL,
    }
}
