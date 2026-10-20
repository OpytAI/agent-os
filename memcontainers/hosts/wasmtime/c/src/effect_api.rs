//! Pull-based application effects. A response never calls application code from a bridge import.

use std::mem::{align_of, size_of};
use std::ptr;

use embedder_rust::{
    self as contract, EffectResponse, HostCallResponse, HttpEffectResponse, PersistEffectResponse,
    ToolApprovalResponse, WsConnectResponse, WsSendResponse,
};

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

fn decode_response(event: u64, frame: &[u8]) -> Result<(u32, EffectBody), u32> {
    let response = EffectResponse::decode(frame).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
    if response.event_id as u64 != event {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let payload = match response.kind {
        contract::MC_HOST_EVENT_HOST_CALL => {
            let body = HostCallResponse::decode(&response.payload)
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
            EffectBody {
                failed: body.failed,
                head: Vec::new(),
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
                head: Vec::new(),
                body: body.body,
            }
        }
        contract::MC_HOST_EVENT_HTTP_REQUEST => {
            let body = HttpEffectResponse::decode(&response.payload)
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
            if !body.failed && body.head.is_empty() {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            EffectBody {
                failed: body.failed,
                head: body.head,
                body: body.body,
            }
        }
        contract::MC_HOST_EVENT_WS_CONNECT => {
            let body = WsConnectResponse::decode(&response.payload)
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
            EffectBody {
                failed: body.failed,
                head: Vec::new(),
                body: Vec::new(),
            }
        }
        contract::MC_HOST_EVENT_WS_SEND => {
            let body = WsSendResponse::decode(&response.payload)
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
            EffectBody {
                failed: body.failed,
                head: Vec::new(),
                body: Vec::new(),
            }
        }
        contract::MC_HOST_EVENT_TOOL_APPROVAL => {
            let body = ToolApprovalResponse::decode(&response.payload)
                .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
            if !body.allow && body.remember_session {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            EffectBody {
                failed: !body.allow,
                head: Vec::new(),
                body: vec![u8::from(body.remember_session)],
            }
        }
        _ => return Err(contract::MC_HOST_UNSUPPORTED),
    };
    Ok((response.kind, payload))
}

unsafe extern "C" fn respond(
    runtime: u64,
    event: u64,
    frame: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let frame = unsafe { bytes(frame, 16 * 1024 * 1024 + 128)? };
        if frame.len() > ActiveRuntime::acquire(runtime)?.state.max_message_bytes() {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let (kind, payload) = decode_response(event, frame)?;
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_respond(event, kind, payload)
    })
}

unsafe extern "C" fn response_begin(
    runtime: u64,
    event: u64,
    metadata: Bytes,
    declared_body_len: u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let frame = unsafe { bytes(metadata, 16 * 1024 + 256)? };
        if frame.len() > ActiveRuntime::acquire(runtime)?.state.max_message_bytes() {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let (kind, payload) = decode_response(event, frame)?;
        let declared =
            usize::try_from(declared_body_len).map_err(|_| contract::MC_HOST_RESOURCE_LIMIT)?;
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_response_begin(event, kind, payload, declared)
    })
}

unsafe extern "C" fn response_write(
    runtime: u64,
    event: u64,
    chunk: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let chunk = unsafe { bytes(chunk, 16 * 1024 * 1024)? };
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_response_write(event, chunk)
    })
}

unsafe extern "C" fn response_finish(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_response_finish(event)
    })
}

unsafe extern "C" fn response_abort(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .effect_response_abort(event)
    })
}

unsafe extern "C" fn ack(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.effect_ack(event)
    })
}

unsafe extern "C" fn socket_push(
    runtime: u64,
    socket: u64,
    data: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let data = unsafe { bytes(data, 1024 * 1024)? };
        ActiveRuntime::acquire(runtime)?
            .state
            .socket_push(socket, data)
    })
}

unsafe extern "C" fn socket_close(runtime: u64, socket: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.socket_close(socket)
    })
}

unsafe extern "C" fn info(
    runtime: u64,
    event: u64,
    out_info: *mut EffectInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out_info)? };
        let (kind, length) = ActiveRuntime::acquire(runtime)?.state.effect_info(event)?;
        let value = EffectInfo {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            kind,
            reserved: 0,
            length,
        };
        unsafe { copy_output(out_info, &value, size) };
        Ok(())
    })
}

unsafe extern "C" fn cancel(runtime: u64, event: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.effect_cancel(event)
    })
}

unsafe extern "C" fn stats(
    runtime: u64,
    vm: u64,
    out_stats: *mut EffectStats,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out_stats)? };
        let state = ActiveRuntime::acquire(runtime)?.state.effect_stats(vm)?;
        let value = EffectStats {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            queued_events: state.queued_events,
            leased_events: state.leased_events,
            queued_bytes: state.queued_bytes,
            response_bytes: state.response_bytes,
            refused_host_call: state.refused[0],
            refused_persist: state.refused[1],
            refused_http: state.refused[2],
            refused_ws: state.refused[3],
            refused_approval: state.refused[4],
        };
        unsafe { copy_output(out_stats, &value, size) };
        Ok(())
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
    response_begin,
    response_write,
    response_finish,
    response_abort,
    socket_push,
    socket_close,
    info,
    cancel,
    stats,
};
