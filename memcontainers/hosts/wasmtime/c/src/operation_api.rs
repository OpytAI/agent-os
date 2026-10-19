//! Generated operation envelopes over the kernel-owned control channel.

use std::mem::{align_of, size_of};

use constants_rust::{
    AUTOCOMPLETE_MAX_FRAME_BYTES, AUTOCOMPLETE_MAX_ITEMS, EXEC_MODE_DIRECT, EXEC_MODE_SHELL,
};
use ctl_rust::{AutocompleteRequest, ExecRequest, SvcRequest};
use embedder_rust::{
    self as contract, CatalogApplyRequest, FilesystemRequest, MountRequest, OperationRequest,
};
use host::ControlRetryAction;

use super::vm_api::{bytes, copy_output, output_size};
use super::*;

const MAX_REQUEST_BYTES: u64 = 18 * 1024 * 1024;
const MAX_ORDINARY_REQUEST_BYTES: usize = 1024 * 1024;

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

fn filesystem_action(kind: u32, payload: &[u8]) -> Result<ControlRetryAction, ResultCode> {
    let request =
        FilesystemRequest::decode(payload).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
    let FilesystemRequest {
        path,
        data,
        target,
        mode,
    } = request;
    if path.is_empty() || path.contains('\0') || target.as_ref().is_some_and(|s| s.contains('\0')) {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let no_extra = data.is_none() && target.is_none() && mode.is_none();
    let action = match kind {
        contract::MC_HOST_OP_FS_READ if no_extra => ControlRetryAction::Read(path),
        contract::MC_HOST_OP_FS_READLINK if no_extra => ControlRetryAction::Readlink(path),
        contract::MC_HOST_OP_FS_READDIR if no_extra => ControlRetryAction::Readdir(path),
        contract::MC_HOST_OP_FS_STAT if no_extra => ControlRetryAction::Stat(path),
        contract::MC_HOST_OP_FS_MKDIR if no_extra => ControlRetryAction::Mkdir(path),
        contract::MC_HOST_OP_FS_UNLINK if no_extra => ControlRetryAction::Unlink(path),
        contract::MC_HOST_OP_FS_WRITE if target.is_none() && mode.is_none() => {
            ControlRetryAction::Write {
                path,
                data: data.ok_or(contract::MC_HOST_INVALID_ARGUMENT)?,
            }
        }
        contract::MC_HOST_OP_FS_CHMOD if data.is_none() && target.is_none() => {
            ControlRetryAction::Chmod {
                path,
                mode: mode.ok_or(contract::MC_HOST_INVALID_ARGUMENT)?,
            }
        }
        contract::MC_HOST_OP_FS_SYMLINK if data.is_none() && mode.is_none() => {
            let target = target
                .filter(|value| !value.is_empty())
                .ok_or(contract::MC_HOST_INVALID_ARGUMENT)?;
            ControlRetryAction::Symlink { target, link: path }
        }
        _ => return Err(contract::MC_HOST_INVALID_ARGUMENT),
    };
    Ok(action)
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
        if envelope.kind != contract::MC_HOST_OP_CATALOG_INJECT
            && frame.len() > MAX_ORDINARY_REQUEST_BYTES
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let operation = match envelope.kind {
            contract::MC_HOST_OP_CATALOG_INJECT => {
                let request = CatalogApplyRequest::decode(&envelope.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                let records = request
                    .records
                    .into_iter()
                    .map(|record| (record.sha, record.bytes))
                    .collect();
                let update = host::prepare_native_catalog_update(
                    &request.index_json,
                    &request.base_digest,
                    records,
                )
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?.state.start_catalog(vm, update)?
            }
            contract::MC_HOST_OP_MOUNT | contract::MC_HOST_OP_UNMOUNT => {
                let request = MountRequest::decode(&envelope.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                if envelope.kind == contract::MC_HOST_OP_UNMOUNT && request.read_only {
                    return Err(contract::MC_HOST_INVALID_ARGUMENT);
                }
                ActiveRuntime::acquire(runtime)?.state.start_mount(
                    vm,
                    &request.path,
                    if envelope.kind == contract::MC_HOST_OP_MOUNT {
                        Some(request.read_only)
                    } else {
                        None
                    },
                )?
            }
            contract::MC_HOST_OP_COMMIT_LAYER => {
                if !envelope.payload.is_empty() {
                    return Err(contract::MC_HOST_INVALID_ARGUMENT);
                }
                ActiveRuntime::acquire(runtime)?.state.start_commit(vm)?
            }
            contract::MC_HOST_OP_SERVICE_CALL => {
                let request = SvcRequest::decode(&envelope.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                if request.service.is_empty() || request.service.contains('\0') {
                    return Err(contract::MC_HOST_INVALID_ARGUMENT);
                }
                ActiveRuntime::acquire(runtime)?
                    .state
                    .start_service(vm, envelope.kind, request)?
            }
            contract::MC_HOST_OP_AUTOCOMPLETE => {
                let request = AutocompleteRequest::decode(&envelope.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                if envelope.payload.len() > AUTOCOMPLETE_MAX_FRAME_BYTES as usize
                    || request.cursor as usize > request.source.len()
                    || request.limit > AUTOCOMPLETE_MAX_ITEMS as u32
                    || request
                        .cwd
                        .as_ref()
                        .is_some_and(|value| value.contains('\0'))
                    || request
                        .env
                        .iter()
                        .any(|(key, value)| key.contains('\0') || value.contains('\0'))
                {
                    return Err(contract::MC_HOST_INVALID_ARGUMENT);
                }
                ActiveRuntime::acquire(runtime)?.state.start_retry(
                    vm,
                    envelope.kind,
                    ControlRetryAction::Autocomplete(envelope.payload),
                )?
            }
            contract::MC_HOST_OP_EXEC_SHELL | contract::MC_HOST_OP_EXEC_DIRECT => {
                let request = ExecRequest::decode(&envelope.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                if !valid_exec_request(envelope.kind, &request) {
                    return Err(contract::MC_HOST_INVALID_ARGUMENT);
                }
                ActiveRuntime::acquire(runtime)?
                    .state
                    .start_exec(vm, envelope.kind, request)?
            }
            contract::MC_HOST_OP_FS_READ
            | contract::MC_HOST_OP_FS_WRITE
            | contract::MC_HOST_OP_FS_READDIR
            | contract::MC_HOST_OP_FS_STAT
            | contract::MC_HOST_OP_FS_READLINK
            | contract::MC_HOST_OP_FS_MKDIR
            | contract::MC_HOST_OP_FS_UNLINK
            | contract::MC_HOST_OP_FS_CHMOD
            | contract::MC_HOST_OP_FS_SYMLINK => {
                let action = filesystem_action(envelope.kind, &envelope.payload)?;
                ActiveRuntime::acquire(runtime)?
                    .state
                    .start_retry(vm, envelope.kind, action)?
            }
            _ => return Err(contract::MC_HOST_UNSUPPORTED),
        };
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
