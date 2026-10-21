//! Immutable native attachment plans. Each VM gets its own relay state.

use std::mem::{align_of, size_of};
use std::ptr;

use embedder_rust::{
    self as contract, AttachmentDefinition, ConnectionAttachment, ConnectionPolicyAttachment,
    HostCallAttachment, HostToolAttachment, HttpApplicationAttachment, PersistAttachment,
};

use super::vm_api::bytes;
use super::*;

unsafe extern "C" fn plan_create(runtime: u64, out: *mut u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        if out.is_null() || (out as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out = 0 };
        let handle = ActiveRuntime::acquire(runtime)?.state.create_plan()?;
        unsafe { *out = handle };
        Ok(())
    })
}

unsafe extern "C" fn plan_add(
    runtime: u64,
    plan: u64,
    definition: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let frame = unsafe { bytes(definition, 1024 * 1024)? };
        if frame.len() > ActiveRuntime::acquire(runtime)?.state.max_message_bytes() {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let definition =
            AttachmentDefinition::decode(frame).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        match definition.kind {
            contract::MC_HOST_ATTACHMENT_HOST_CALL => {
                let route = HostCallAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?.state.add_host_call_route(
                    plan,
                    route.name,
                    route.identity,
                    frame.len(),
                )
            }
            contract::MC_HOST_ATTACHMENT_PERSIST => {
                let provider = PersistAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?.state.add_persist_provider(
                    plan,
                    provider.identity,
                    frame.len(),
                )
            }
            contract::MC_HOST_ATTACHMENT_HTTP_APPLICATION => {
                let transport = HttpApplicationAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?.state.add_http_application(
                    plan,
                    transport.identity,
                    frame.len(),
                )
            }
            contract::MC_HOST_ATTACHMENT_HOST_TOOL => {
                let tool = HostToolAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?.state.add_host_tool(
                    plan,
                    host::HostToolDef {
                        address: tool.address,
                        description: tool.description,
                        binding_name: tool.binding_name,
                        args_mode: tool.args_mode,
                        input_schema: tool.input_schema,
                        output_schema: tool.output_schema,
                        annotations: tool.annotations,
                    },
                    frame.len(),
                )
            }
            contract::MC_HOST_ATTACHMENT_CONNECTION => {
                let connection = ConnectionAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?
                    .state
                    .add_connection(plan, connection, frame.len())
            }
            contract::MC_HOST_ATTACHMENT_CONNECTION_POLICY => {
                let policy = ConnectionPolicyAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?
                    .state
                    .add_connection_policy(plan, policy, frame.len())
            }
            _ => Err(contract::MC_HOST_UNSUPPORTED),
        }
    })
}

unsafe extern "C" fn plan_seal(runtime: u64, plan: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.seal_plan(plan)
    })
}

unsafe extern "C" fn plan_close(runtime: u64, plan: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.close_plan(plan)
    })
}

unsafe extern "C" fn vm_attach_update(
    runtime: u64,
    vm: u64,
    plan: u64,
    error: *mut Error,
) -> ResultCode {
    guard_vm(error, runtime, vm, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .vm_attach_update(vm, plan)
    })
}

unsafe extern "C" fn vm_attachment_manifest(
    runtime: u64,
    vm: u64,
    buffer: *mut Buffer,
    error: *mut Error,
) -> ResultCode {
    guard_vm(error, runtime, vm, || {
        if buffer.is_null() || (buffer as usize) % align_of::<Buffer>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let caller = unsafe { ptr::read(buffer) };
        if caller.capacity != 0 && caller.data.is_null() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let frame = ActiveRuntime::acquire(runtime)?
            .state
            .vm_attachment_manifest(vm)?;
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

pub(super) static ATTACHMENT_API: AttachmentApi = AttachmentApi {
    header: Header {
        size: size_of::<AttachmentApi>() as u32,
        version: 1,
        flags: 0,
    },
    plan_create,
    plan_add,
    plan_seal,
    plan_close,
    vm_attach_update,
    vm_attachment_manifest,
};
