//! Bounded, immutable terminal output leases for native callers.

use std::mem::{align_of, size_of};

use embedder_rust as contract;
use host::output_queue::OutputRecord;

use super::vm_api::{copy_output, output_size};
use super::*;

fn record_info(record: &OutputRecord, size: usize) -> OutputRecordInfo {
    let mut info = OutputRecordInfo {
        header: Header {
            size: size as u32,
            version: 1,
            flags: 0,
        },
        kind: 0,
        channel: 0,
        sequence: record.sequence(),
        byte_offset: 0,
        length: 0,
        first_lost_sequence: 0,
        last_lost_sequence: 0,
        byte_end: 0,
    };
    match record {
        OutputRecord::Data {
            channel,
            byte_offset,
            bytes,
            ..
        } => {
            info.kind = contract::MC_HOST_OUTPUT_DATA;
            info.channel = *channel;
            info.byte_offset = *byte_offset;
            info.length = bytes.len() as u64;
            info.byte_end = byte_offset + bytes.len() as u64;
        }
        OutputRecord::Gap {
            first_lost_sequence,
            last_lost_sequence,
            byte_start,
            byte_end,
            ..
        } => {
            info.kind = contract::MC_HOST_OUTPUT_GAP;
            info.byte_offset = *byte_start;
            info.length = byte_end - byte_start;
            info.first_lost_sequence = *first_lost_sequence;
            info.last_lost_sequence = *last_lost_sequence;
            info.byte_end = *byte_end;
        }
    }
    info
}

pub unsafe extern "C" fn next(
    runtime: u64,
    vm: u64,
    out_handle: *mut u64,
    out_record: *mut OutputRecordInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_handle.is_null() || (out_handle as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let size = unsafe { output_size(out_record)? };
        if ranges_overlap(
            out_handle as usize,
            size_of::<u64>(),
            out_record as usize,
            size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out_handle = 0 };
        let active = ActiveRuntime::acquire(runtime)?;
        let Some((handle, record)) = active.state.output_next(vm)? else {
            return Err(contract::MC_HOST_WOULD_BLOCK);
        };
        let info = record_info(&record, size);
        unsafe {
            copy_output(out_record, &info, size);
            *out_handle = handle;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn read(
    runtime: u64,
    output: u64,
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
        let active = ActiveRuntime::acquire(runtime)?;
        let record = active.state.output_record(output)?;
        let bytes = match &record {
            OutputRecord::Data { bytes, .. } => bytes.as_ref(),
            OutputRecord::Gap { .. } => &[],
        };
        let offset = usize::try_from(offset).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if offset > bytes.len() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let copied = capacity.min(bytes.len() - offset);
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
            unsafe { ptr::copy_nonoverlapping(bytes[offset..].as_ptr(), caller.data, copied) };
        }
        unsafe {
            (*buffer).required = bytes.len() as u64;
            *out_copied = copied as u64;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn ack(runtime: u64, output: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .output_complete(output, true)
    })
}

pub unsafe extern "C" fn release(runtime: u64, output: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .output_complete(output, false)
    })
}

pub unsafe extern "C" fn stats(
    runtime: u64,
    vm: u64,
    out: *mut OutputStatsInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out)? };
        let active = ActiveRuntime::acquire(runtime)?;
        let stats = active.state.vm(vm)?.output.stats();
        let value = OutputStatsInfo {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            records: stats.record_count as u64,
            retained_bytes: stats.retained_bytes as u64,
            dropped_bytes: stats.dropped_bytes,
            leased_records: stats.leased_records as u64,
            flags: if stats.above_high_water {
                contract::MC_HOST_OUTPUT_ABOVE_HIGH_WATER
            } else {
                0
            } | if stats.poisoned {
                contract::MC_HOST_OUTPUT_POISONED
            } else {
                0
            },
            reserved: 0,
        };
        unsafe { copy_output(out, &value, size) };
        Ok(())
    })
}

pub static OUTPUT_API: OutputApi = OutputApi {
    header: Header {
        size: size_of::<OutputApi>() as u32,
        version: 1,
        flags: 0,
    },
    next,
    read,
    ack,
    release,
    stats,
};
