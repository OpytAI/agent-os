//! Immutable MCSN snapshot and baseline handles.

use std::mem::{align_of, size_of};

use embedder_rust as contract;
use host::{inspect_snapshot_bytes, SnapshotDescription};

use super::vm_api::{bytes, copy_output, input, output_size};
use super::*;

const MAX_SNAPSHOT_BYTES: u64 = 512 * 1024 * 1024;

pub(super) fn check_snapshot_memory(snapshot: &[u8]) -> Result<(), ResultCode> {
    let view =
        snapshot_rust::parse_snapshot(snapshot).map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
    if view.memory_len > MAX_SNAPSHOT_BYTES as usize {
        return Err(contract::MC_HOST_RESOURCE_LIMIT);
    }
    Ok(())
}

fn info(description: SnapshotDescription, size: usize) -> SnapshotInfo {
    SnapshotInfo {
        header: Header {
            size: size as u32,
            version: 1,
            flags: 0,
        },
        kind: if description.incremental {
            contract::MC_HOST_SNAPSHOT_INCREMENTAL
        } else {
            contract::MC_HOST_SNAPSHOT_FULL
        },
        mcsn_version: snapshot_rust::SNAPSHOT_VERSION,
        kernel_digest: description.kernel_digest,
        baseline_id: description.baseline_id,
        content_digest: description.content_digest,
        length: description.length as u64,
        memory_len: description.memory_len as u64,
    }
}

pub unsafe extern "C" fn baseline_import(
    runtime: u64,
    kernel: u64,
    bytes_in: Bytes,
    out_baseline: *mut u64,
    out_info: *mut SnapshotInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_baseline.is_null() || (out_baseline as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let size = unsafe { output_size(out_info)? };
        if ranges_overlap(
            out_baseline as usize,
            size_of::<u64>(),
            out_info as usize,
            size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let bytes = unsafe { bytes(bytes_in, MAX_SNAPSHOT_BYTES)? };
        if ranges_overlap(
            bytes.as_ptr() as usize,
            bytes.len(),
            out_baseline as usize,
            size_of::<u64>(),
        ) || ranges_overlap(
            bytes.as_ptr() as usize,
            bytes.len(),
            out_info as usize,
            size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let active = ActiveRuntime::acquire(runtime)?;
        let (baseline, description) = active.state.import_baseline(kernel, bytes)?;
        let value = info(description, size);
        unsafe {
            copy_output(out_info, &value, size);
            *out_baseline = baseline;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn baseline_close(
    runtime: u64,
    baseline: u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .close_baseline(baseline)
    })
}

pub unsafe extern "C" fn begin(
    runtime: u64,
    vm: u64,
    options: *const SnapshotBeginOptions,
    out_snapshot: *mut u64,
    out_info: *mut SnapshotInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_snapshot.is_null() || (out_snapshot as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let options = unsafe { input(options)? };
        let size = unsafe { output_size(out_info)? };
        if ranges_overlap(
            out_snapshot as usize,
            size_of::<u64>(),
            out_info as usize,
            size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if options.reserved != 0 {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        let baseline = match options.mode {
            contract::MC_HOST_SNAPSHOT_FULL if options.baseline == 0 => None,
            contract::MC_HOST_SNAPSHOT_INCREMENTAL if options.baseline != 0 => {
                Some(options.baseline)
            }
            _ => return Err(contract::MC_HOST_INVALID_ARGUMENT),
        };
        let active = ActiveRuntime::acquire(runtime)?;
        let (snapshot, description) = active.state.begin_snapshot(vm, baseline)?;
        let value = info(description, size);
        unsafe {
            copy_output(out_info, &value, size);
            *out_snapshot = snapshot;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn read(
    runtime: u64,
    snapshot: u64,
    offset: u64,
    buffer: *mut Buffer,
    out_copied: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if buffer.is_null()
            || out_copied.is_null()
            || (buffer as usize) % align_of::<Buffer>() != 0
            || (out_copied as usize) % align_of::<u64>() != 0
            || ranges_overlap(
                buffer as usize,
                size_of::<Buffer>(),
                out_copied as usize,
                size_of::<u64>(),
            )
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let caller = unsafe { ptr::read(buffer) };
        if caller.capacity != 0 && caller.data.is_null() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let capacity =
            usize::try_from(caller.capacity).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        let record = ActiveRuntime::acquire(runtime)?
            .state
            .snapshot_record(snapshot)?;
        let offset = usize::try_from(offset).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if offset > record.bytes.len() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let copied = capacity.min(record.bytes.len() - offset);
        if copied != 0 {
            if ranges_overlap(
                caller.data as usize,
                copied,
                buffer as usize,
                size_of::<Buffer>(),
            ) || ranges_overlap(
                caller.data as usize,
                copied,
                out_copied as usize,
                size_of::<u64>(),
            ) {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            unsafe {
                ptr::copy_nonoverlapping(record.bytes[offset..].as_ptr(), caller.data, copied)
            };
        }
        unsafe {
            (*buffer).required = record.bytes.len() as u64;
            *out_copied = copied as u64;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn finish(runtime: u64, snapshot: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .close_snapshot(snapshot)
    })
}

pub unsafe extern "C" fn cancel(runtime: u64, snapshot: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .close_snapshot(snapshot)
    })
}

pub unsafe extern "C" fn inspect(
    runtime: u64,
    snapshot: Bytes,
    baseline: Bytes,
    out_info: *mut SnapshotInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out_info)? };
        let snapshot = unsafe { bytes(snapshot, MAX_SNAPSHOT_BYTES)? };
        check_snapshot_memory(snapshot)?;
        let baseline = if baseline.len == 0 {
            None
        } else {
            Some(unsafe { bytes(baseline, MAX_SNAPSHOT_BYTES)? })
        };
        if let Some(base) = baseline {
            check_snapshot_memory(base)?;
        }
        if ranges_overlap(
            snapshot.as_ptr() as usize,
            snapshot.len(),
            out_info as usize,
            size,
        ) || baseline.is_some_and(|base| {
            ranges_overlap(base.as_ptr() as usize, base.len(), out_info as usize, size)
        }) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let _active = ActiveRuntime::acquire(runtime)?;
        let description = inspect_snapshot_bytes(snapshot, baseline)
            .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
        let value = info(description, size);
        unsafe { copy_output(out_info, &value, size) };
        Ok(())
    })
}

pub static SNAPSHOT_API: SnapshotApi = SnapshotApi {
    header: Header {
        size: size_of::<SnapshotApi>() as u32,
        version: 1,
        flags: 0,
    },
    baseline_import,
    baseline_close,
    begin,
    read,
    finish,
    cancel,
    inspect,
};
