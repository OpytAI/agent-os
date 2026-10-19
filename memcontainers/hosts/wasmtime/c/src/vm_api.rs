//! VM lifecycle, terminal control, and bounded kernel drive.

use std::mem::{align_of, size_of};
use std::ptr;

use embedder_rust as contract;

use super::resources::{VmConfig, VmReport};
use super::*;

const MAX_KERNEL_BYTES: u64 = 256 * 1024 * 1024;
const MAX_IMAGE_BYTES: u64 = 256 * 1024 * 1024;
const MAX_INPUT_BYTES: u64 = 16 * 1024 * 1024;

pub(super) unsafe fn bytes<'a>(value: Bytes, maximum: u64) -> Result<&'a [u8], ResultCode> {
    if value.len > maximum {
        return Err(contract::MC_HOST_RESOURCE_LIMIT);
    }
    if value.len == 0 {
        return Ok(&[]);
    }
    if value.data.is_null() {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let length = usize::try_from(value.len).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
    Ok(unsafe { std::slice::from_raw_parts(value.data, length) })
}

pub(super) unsafe fn input<T: Copy>(ptr: *const T) -> Result<T, ResultCode> {
    if ptr.is_null() || (ptr as usize) % align_of::<Header>() != 0 {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let header = unsafe { ptr::read(ptr.cast::<Header>()) };
    if !valid_header(header, size_of::<T>()) || header.size as usize > MAX_INPUT_STRUCT_SIZE {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let extra = header.size as usize - size_of::<T>();
    if extra != 0 {
        let tail =
            unsafe { std::slice::from_raw_parts(ptr.cast::<u8>().add(size_of::<T>()), extra) };
        if tail.iter().any(|byte| *byte != 0) {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
    }
    Ok(unsafe { ptr::read_unaligned(ptr) })
}

pub(super) unsafe fn output_size<T>(ptr: *mut T) -> Result<usize, ResultCode> {
    if ptr.is_null() || (ptr as usize) % align_of::<Header>() != 0 {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let header = unsafe { ptr::read(ptr.cast::<Header>()) };
    if !valid_header(header, size_of::<Header>()) {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    Ok((header.size as usize).min(size_of::<T>()))
}

pub(super) unsafe fn copy_output<T>(out: *mut T, value: &T, count: usize) {
    unsafe { ptr::copy_nonoverlapping((value as *const T).cast::<u8>(), out.cast::<u8>(), count) };
}

pub unsafe extern "C" fn kernel_load(
    runtime: u64,
    wasm: Bytes,
    out_kernel: *mut u64,
    out_info: *mut KernelInfo,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_kernel.is_null() || (out_kernel as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let info_size = unsafe { output_size(out_info)? };
        let wasm = unsafe { bytes(wasm, MAX_KERNEL_BYTES)? };
        if ranges_overlap(
            out_kernel as usize,
            size_of::<u64>(),
            out_info as usize,
            info_size,
        ) || ranges_overlap(
            wasm.as_ptr() as usize,
            wasm.len(),
            out_kernel as usize,
            size_of::<u64>(),
        ) || ranges_overlap(
            wasm.as_ptr() as usize,
            wasm.len(),
            out_info as usize,
            info_size,
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out_kernel = 0 };
        let active = ActiveRuntime::acquire(runtime)?;
        let (kernel, digest) = active.state.load_kernel(wasm)?;
        let info = KernelInfo {
            header: Header {
                size: info_size as u32,
                version: 1,
                flags: 0,
            },
            digest,
        };
        unsafe {
            copy_output(out_info, &info, info_size);
            *out_kernel = kernel;
        }
        Ok(())
    })
}

pub unsafe extern "C" fn kernel_close(runtime: u64, kernel: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.close_kernel(kernel)
    })
}

pub unsafe extern "C" fn vm_create(
    runtime: u64,
    kernel: u64,
    plan: u64,
    options: *const VmCreateOptions,
    out_vm: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_vm.is_null() || (out_vm as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let options_ptr = options;
        let options = unsafe { input(options_ptr)? };
        if ranges_overlap(
            options_ptr as usize,
            options.header.size as usize,
            out_vm as usize,
            size_of::<u64>(),
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if options.flags & !contract::MC_HOST_VM_CREATE_DETERMINISTIC != 0 {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        if (options.terminal_cols == 0) != (options.terminal_rows == 0) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let terminal_size = if options.terminal_cols != 0 {
            Some((
                i32::try_from(options.terminal_cols)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?,
                i32::try_from(options.terminal_rows)
                    .map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?,
            ))
        } else {
            None
        };
        if options.layer_count > 64 || (options.layer_count != 0 && options.layers.is_null()) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if options.layer_count != 0 && (options.layers as usize) % align_of::<Bytes>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if ranges_overlap(
            options.layers as usize,
            options.layer_count as usize * size_of::<Bytes>(),
            out_vm as usize,
            size_of::<u64>(),
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut layers = Vec::with_capacity(options.layer_count as usize);
        let mut total = 0u64;
        for index in 0..options.layer_count as usize {
            let descriptor = unsafe { ptr::read(options.layers.add(index)) };
            total = total
                .checked_add(descriptor.len)
                .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
            if total > MAX_IMAGE_BYTES {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            let layer = unsafe { bytes(descriptor, MAX_IMAGE_BYTES)? };
            if ranges_overlap(
                layer.as_ptr() as usize,
                layer.len(),
                out_vm as usize,
                size_of::<u64>(),
            ) {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            layers.push(layer.to_vec());
        }
        let config = VmConfig {
            layers,
            deterministic: options.flags & contract::MC_HOST_VM_CREATE_DETERMINISTIC != 0,
            requested_workers: if options.requested_workers == -1 {
                None
            } else if options.requested_workers >= 0 {
                Some(options.requested_workers)
            } else {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            },
            contract_tier: options.contract_tier,
            contract_budget_mib: options.contract_budget_mib,
            contract_fuel: options.contract_fuel,
            terminal_size,
        };
        unsafe { *out_vm = 0 };
        let active = ActiveRuntime::acquire(runtime)?;
        let handle = active.state.create_vm(kernel, plan, config)?;
        unsafe { *out_vm = handle };
        Ok(())
    })
}

pub unsafe extern "C" fn vm_restore(
    runtime: u64,
    kernel: u64,
    plan: u64,
    options: *const VmRestoreOptions,
    out_vm: *mut u64,
    error: *mut Error,
) -> ResultCode {
    vm_restore_common(
        runtime,
        kernel,
        plan,
        options,
        Bytes {
            data: std::ptr::null(),
            len: 0,
        },
        out_vm,
        error,
    )
}

pub unsafe extern "C" fn vm_restore_attached(
    runtime: u64,
    kernel: u64,
    plan: u64,
    options: *const VmRestoreOptions,
    manifest: Bytes,
    out_vm: *mut u64,
    error: *mut Error,
) -> ResultCode {
    vm_restore_common(runtime, kernel, plan, options, manifest, out_vm, error)
}

fn vm_restore_common(
    runtime: u64,
    kernel: u64,
    plan: u64,
    options: *const VmRestoreOptions,
    manifest: Bytes,
    out_vm: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_vm.is_null() || (out_vm as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let options_ptr = options;
        let options = unsafe { input(options_ptr)? };
        if ranges_overlap(
            options_ptr as usize,
            options.header.size as usize,
            out_vm as usize,
            size_of::<u64>(),
        ) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if options.reserved != 0
            || (options.mode != contract::MC_HOST_RESTORE_DETACHED
                && options.mode != contract::MC_HOST_RESTORE_STRICT)
            || (options.mode == contract::MC_HOST_RESTORE_DETACHED && plan != 0)
        {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        if options.mode == contract::MC_HOST_RESTORE_DETACHED && manifest.len != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let snapshot = unsafe { bytes(options.snapshot, 512 * 1024 * 1024)? };
        super::snapshot_api::check_snapshot_memory(snapshot)?;
        let baseline = if options.baseline.len == 0 {
            None
        } else {
            Some(unsafe { bytes(options.baseline, 512 * 1024 * 1024)? })
        };
        if let Some(base) = baseline {
            super::snapshot_api::check_snapshot_memory(base)?;
        }
        let manifest = if manifest.len == 0 {
            None
        } else {
            Some(unsafe { bytes(manifest, 1024 * 1024)? })
        };
        if ranges_overlap(
            snapshot.as_ptr() as usize,
            snapshot.len(),
            out_vm as usize,
            size_of::<u64>(),
        ) || baseline.is_some_and(|base| {
            ranges_overlap(
                base.as_ptr() as usize,
                base.len(),
                out_vm as usize,
                size_of::<u64>(),
            )
        }) || manifest.is_some_and(|bytes| {
            ranges_overlap(
                bytes.as_ptr() as usize,
                bytes.len(),
                out_vm as usize,
                size_of::<u64>(),
            )
        }) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out_vm = 0 };
        let active = ActiveRuntime::acquire(runtime)?;
        let vm =
            active
                .state
                .restore_vm(kernel, plan, options.mode, snapshot, baseline, manifest)?;
        unsafe { *out_vm = vm };
        Ok(())
    })
}

pub unsafe extern "C" fn vm_fork(
    runtime: u64,
    source: u64,
    out_vm: *mut u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        if out_vm.is_null() || (out_vm as usize) % align_of::<u64>() != 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out_vm = 0 };
        let child = ActiveRuntime::acquire(runtime)?.state.fork_vm(source)?;
        unsafe { *out_vm = child };
        Ok(())
    })
}

fn status_value(report: VmReport, size: usize) -> VmStatus {
    let observed = report.observed;
    let output = report.output;
    let state = if observed.closing {
        contract::MC_HOST_VM_CLOSING
    } else if observed.poisoned {
        contract::MC_HOST_VM_POISONED
    } else if observed.exit_code.is_some() {
        contract::MC_HOST_VM_EXITED
    } else if observed.last_tick == contract::MC_HOST_TICK_WAITING {
        contract::MC_HOST_VM_WAITING
    } else {
        contract::MC_HOST_VM_RUNNABLE
    };
    VmStatus {
        header: Header {
            size: size as u32,
            version: 1,
            flags: 0,
        },
        state,
        last_tick: observed.last_tick,
        flags: u32::from(observed.exit_code.is_some()) * contract::MC_HOST_VM_FLAG_HAS_EXIT_CODE
            | u32::from(report.at_prompt) * contract::MC_HOST_VM_FLAG_AT_PROMPT
            | u32::from(output.record_count != 0) * contract::MC_HOST_VM_FLAG_OUTPUT_READY
            | u32::from(report.operation_ready) * contract::MC_HOST_VM_FLAG_OPERATION_READY
            | u32::from(report.operation_pending) * contract::MC_HOST_VM_FLAG_OPERATION_PENDING
            | u32::from(report.effect_ready) * contract::MC_HOST_VM_FLAG_EFFECT_READY,
        exit_code: observed.exit_code.unwrap_or(0),
        output_records: output.record_count as u64,
        output_bytes: output.retained_bytes as u64,
        output_dropped_bytes: output.dropped_bytes,
        inflight_egress: report.inflight_egress,
        pending_commits: report.pending_commits,
        workers: report.workers,
        reserved: 0,
    }
}

pub unsafe extern "C" fn vm_status(
    runtime: u64,
    vm: u64,
    out: *mut VmStatus,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let size = unsafe { output_size(out)? };
        let active = ActiveRuntime::acquire(runtime)?;
        let report = active.state.vm(vm)?.status()?;
        let value = status_value(report, size);
        unsafe { copy_output(out, &value, size) };
        Ok(())
    })
}

pub unsafe extern "C" fn vm_drive(
    runtime: u64,
    vm: u64,
    options: *const DriveOptions,
    out: *mut DriveResult,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let options = unsafe { input(options)? };
        let size = unsafe { output_size(out)? };
        if options.reserved != 0 {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        let active = ActiveRuntime::acquire(runtime)?;
        let driven = active
            .state
            .vm(vm)?
            .drive(options.max_ticks, options.stop_mask)?;
        let value = DriveResult {
            header: Header {
                size: size as u32,
                version: 1,
                flags: 0,
            },
            ticks: driven.ticks,
            stop_reason: driven.stop_reason,
            status: status_value(driven.report, size_of::<VmStatus>()),
        };
        unsafe { copy_output(out, &value, size) };
        Ok(())
    })
}

pub unsafe extern "C" fn vm_close(runtime: u64, vm: u64, error: *mut Error) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?.state.close_vm(vm)
    })
}

pub unsafe extern "C" fn vm_cancel_and_close(
    runtime: u64,
    vm: u64,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        ActiveRuntime::acquire(runtime)?
            .state
            .cancel_and_close_vm(vm)
    })
}

pub unsafe extern "C" fn terminal_input(
    runtime: u64,
    vm: u64,
    input: Bytes,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let input = unsafe { bytes(input, MAX_INPUT_BYTES)? };
        ActiveRuntime::acquire(runtime)?.state.vm(vm)?.input(input)
    })
}

pub unsafe extern "C" fn terminal_resize(
    runtime: u64,
    vm: u64,
    cols: u32,
    rows: u32,
    error: *mut Error,
) -> ResultCode {
    guard(error, || {
        let cols = i32::try_from(cols).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        let rows = i32::try_from(rows).map_err(|_| contract::MC_HOST_INVALID_ARGUMENT)?;
        if cols == 0 || rows == 0 {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        ActiveRuntime::acquire(runtime)?
            .state
            .vm(vm)?
            .resize(cols, rows)
    })
}

pub static VM_API: VmApi = VmApi {
    header: Header {
        size: size_of::<VmApi>() as u32,
        version: 1,
        flags: 0,
    },
    kernel_load,
    kernel_close,
    vm_create,
    vm_restore,
    vm_status,
    vm_drive,
    vm_close,
    vm_cancel_and_close,
    terminal_input,
    terminal_resize,
    vm_restore_attached,
    vm_fork,
};
