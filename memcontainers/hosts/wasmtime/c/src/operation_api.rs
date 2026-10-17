//! Generated operation envelopes over the kernel-owned control channel.

use std::mem::{align_of, size_of};

use constants_rust::{EXEC_MODE_DIRECT, EXEC_MODE_SHELL};
use ctl_rust::ExecRequest;
use embedder_rust::{self as contract, OperationRequest};

use super::vm_api::{bytes, copy_output, output_size};
use super::*;

const MAX_REQUEST_BYTES: u64 = 1024 * 1024;

fn valid_exec_request(kind: u32, request: &ExecRequest) -> bool {
    let valid_shape = if kind == contract::MC_HOST_OP_EXEC_SHELL {
        request.mode == EXEC_MODE_SHELL
            && request.command.as_ref().is_some_and(|cmd| !cmd.is_empty())
            && request.argv.is_empty()
    } else {
        request.mode == EXEC_MODE_DIRECT
            && request.command.is_none()
            && request
                .argv
                .first()
                .is_some_and(|arg| !arg.value.is_empty())
    };
    valid_shape
        && request
            .command
            .as_ref()
            .is_none_or(|value| !value.contains('\0'))
        && request
            .cwd
            .as_ref()
            .is_none_or(|value| !value.contains('\0'))
        && request.argv.iter().all(|arg| !arg.value.contains('\0'))
        && request
            .env
            .iter()
            .all(|(key, value)| !key.contains('\0') && !value.contains('\0'))
}

pub unsafe extern "C" fn start(
    runtime: u64,
    vm: u64,
    request: Bytes,
    out_operation: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_operation.is_null() || (out_operation as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let frame = unsafe { bytes(request, MAX_REQUEST_BYTES)? };
        if ranges_overlap(
            frame.as_ptr() as usize,
            frame.len(),
            out_operation as usize,
            size_of::<u64>(),
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let envelope =
            OperationRequest::decode(frame).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if envelope.kind != contract::MC_HOST_OP_EXEC_SHELL
            && envelope.kind != contract::MC_HOST_OP_EXEC_DIRECT
        {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        let request = ExecRequest::decode(&envelope.payload)
            .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if !valid_exec_request(envelope.kind, &request) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let operation =
            ActiveRuntime::acquire(runtime)?
                .state
                .start_exec(vm, envelope.kind, request)?;
        unsafe { *out_operation = operation };
        Ok(())
    })
}

pub unsafe extern "C" fn poll(
    runtime: u64,
    operation: u64,
    out: *mut OperationStatus,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out)? };
        let record = ActiveRuntime::acquire(runtime)?
            .state
            .operation(operation)?;
        let observed = record.observed()?;
        let value = OperationStatus {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            kind: observed.kind,
            state: observed.state,
            error: observed.error,
            reserved: 0,
            result_length: observed.result_length,
        };
        unsafe { copy_output(out, &value, size) };
        Ok(())
    })
}

pub unsafe extern "C" fn result(
    runtime: u64,
    operation: u64,
    buffer: *mut Buffer,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if buffer.is_null() || (buffer as usize) % align_of::<Buffer>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let caller = unsafe { ptr::read(buffer) };
        if caller.capacity != 0 && caller.data.is_null() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let record = ActiveRuntime::acquire(runtime)?
            .state
            .operation(operation)?;
        let bytes = record.result()?;
        if !caller.data.is_null()
            && ranges_overlap(
                caller.data as usize,
                caller.capacity.min(bytes.len() as u64) as usize,
                buffer as usize,
                size_of::<Buffer>(),
            )
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { (*buffer).required = bytes.len() as u64 };
        if caller.capacity < bytes.len() as u64 {
            return Err(contract::MC_HOST_BUFFER_TOO_SMALL);
        }
        if !bytes.is_empty() {
            unsafe { ptr::copy_nonoverlapping(bytes.as_ptr(), caller.data, bytes.len()) };
        }
        Ok(())
    })
}

pub unsafe extern "C" fn peek(
    runtime: u64,
    operation: u64,
    channel: u32,
    cursor: u64,
    buffer: *mut Buffer,
    out_copied: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if channel != host::output_queue::STDOUT {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
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
        let bytes = ActiveRuntime::acquire(runtime)?
            .state
            .operation_peek(operation)?;
        let cursor = usize::try_from(cursor).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if cursor > bytes.len() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let capacity =
            usize::try_from(caller.capacity).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        let copied = capacity.min(bytes.len() - cursor);
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
            unsafe { ptr::copy_nonoverlapping(bytes[cursor..].as_ptr(), caller.data, copied) };
        }
        unsafe {
            (*buffer).required = bytes.len() as u64;
            *out_copied = copied as u64;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn cancel(runtime: u64, operation: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .operation_cancel(operation)
    })
}

pub unsafe extern "C" fn close(runtime: u64, operation: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .operation_close(operation)
    })
}

pub static OPERATION_API: OperationApi = OperationApi {
    header: Header {
        size: size_of::<OperationApi>() as u32,
        version: 1,
        flags: 0,
    },
    start,
    poll,
    result,
    peek,
    cancel,
    close,
};
