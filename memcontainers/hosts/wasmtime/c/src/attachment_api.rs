//! Immutable native attachment plans. Each VM gets its own relay state.

use std::mem::{align_of, size_of};

use embedder_rust::{
    self as contract, AttachmentDefinition, HostCallAttachment, HostToolAttachment,
    HttpApplicationAttachment, PersistAttachment,
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
                )
            }
            contract::MC_HOST_ATTACHMENT_PERSIST => {
                let provider = PersistAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?
                    .state
                    .add_persist_provider(plan, provider.identity)
            }
            contract::MC_HOST_ATTACHMENT_HTTP_APPLICATION => {
                let transport = HttpApplicationAttachment::decode(&definition.payload)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
                ActiveRuntime::acquire(runtime)?
                    .state
                    .add_http_application(plan, transport.identity)
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
                )
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
};
