//! Pull-based application effects. A response never calls application code from a bridge import.

use std::mem::{align_of, size_of};
use std::ptr;

use embedder_rust::{self as contract, EffectResponse, HostCallResponse, PersistEffectResponse};

use super::relay::EffectBody;
use super::vm_api::{bytes, copy_output, output_size};
use super::*;

unsafe extern "C" fn next(
    runtime: u64,
    vm: u64,
    mask: u64,
    out_event: *mut u64,
    out_info: *mut EffectInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_event.is_null() || (out_event as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let size = unsafe { output_size(out_info)? };
        if ranges_overlap(
            out_event as usize,
            size_of::<u64>(),
            out_info as usize,
            size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out_event = 0 };
        let (event, kind, length) = ActiveRuntime::acquire(runtime)?
            .state
            .effect_next(vm, mask)?;
        let info = EffectInfo {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            kind,
            reserved: 0,
            length,
        };
        unsafe {
            copy_output(out_info, &info, size);
            *out_event = event;
        }
        Ok(())
    })
}

unsafe extern "C" fn read(
    runtime: u64,
    event: u64,
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
        let frame = ActiveRuntime::acquire(runtime)?.state.effect_frame(event)?;
        if !caller.data.is_null()
            && ranges_overlap(
                caller.data as usize,
                caller.capacity.min(frame.len() as u64) as usize,
                buffer as usize,
                size_of::<Buffer>(),
            )
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { (*buffer).required = frame.len() as u64 };
        if caller.capacity < frame.len() as u64 {
            return Err(contract::MC_HOST_BUFFER_TOO_SMALL);
        }
        if !frame.is_empty() {
            unsafe { ptr::copy_nonoverlapping(frame.as_ptr(), caller.data, frame.len()) };
        }
        Ok(())
    })
}

unsafe extern "C" fn release(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.effect_release(event)
    })
}

unsafe extern "C" fn respond(
    runtime: u64,
    event: u64,
    frame: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let frame = unsafe { bytes(frame, 16 * 1024 * 1024 + 128)? };
        let response =
            EffectResponse::decode(frame).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if response.event_id as u64 != event {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let payload = match response.kind {
            contract::MC_HOST_EVENT_HOST_CALL => {
                let body = HostCallResponse::decode(&response.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                EffectBody {
                    failed: body.failed,
                    body: body.body,
                }
            }
            contract::MC_HOST_EVENT_PERSIST_GET
            | contract::MC_HOST_EVENT_PERSIST_PUT
            | contract::MC_HOST_EVENT_PERSIST_DELETE
            | contract::MC_HOST_EVENT_PERSIST_LIST => {
                let body = PersistEffectResponse::decode(&response.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                EffectBody {
                    failed: body.failed,
                    body: body.body,
                }
            }
            _ => return Err(contract::MC_HOST_UNSUPPORTED),
        };
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_respond(event, response.kind, payload)
    })
}

unsafe extern "C" fn ack(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.effect_ack(event)
    })
}

pub(super) static EFFECT_API: EffectApi = EffectApi {
    header: Header {
        size: size_of::<EffectApi>() as u32,
        version: 1,
        flags: 0,
    },
    next,
    read,
    release,
    respond,
    ack,
};
