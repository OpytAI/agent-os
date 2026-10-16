//! Native ABI admission and runtime identity. No VM interface is advertised yet.
//!
//! Runtime IDs live in a process table, not at caller-held addresses. Closing a
//! runtime cannot free memory that a concurrent call has already borrowed.

use std::ffi::c_void;
use std::mem::size_of;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::{Mutex, OnceLock};

use embedder_rust as contract;

type ResultCode = u32;
type RuntimeHandle = u64;

const RUNTIME_KIND: u8 = 1;
const MAX_NAMESPACE: u32 = u16::MAX as u32;
const MAX_INDEX: usize = (1 << 20) - 1;
const MAX_GENERATION: u32 = (1 << 20) - 1;
const INDEX_MASK: u64 = (1 << 20) - 1;
const GENERATION_MASK: u64 = (1 << 20) - 1;
const MAX_INPUT_STRUCT_SIZE: usize = 4096;

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
    runtime_create: unsafe extern "C" fn(*const RuntimeOptions, *mut RuntimeHandle, *mut Error) -> ResultCode,
    runtime_close: unsafe extern "C" fn(RuntimeHandle, *mut Error) -> ResultCode,
    library_info: unsafe extern "C" fn(*mut LibraryInfo, *mut Error) -> ResultCode,
    get_interface: unsafe extern "C" fn(RuntimeHandle, u32, u16, u32, *mut c_void, *mut Error) -> ResultCode,
}

// Both languages check the same public layout numbers. A later projector
// phase will derive these assertions from embedder.kdl instead of hand-coding them.
const _: [(); 8] = [(); size_of::<Header>()];
const _: [(); 24] = [(); size_of::<Buffer>()];
const _: [(); 48] = [(); size_of::<Error>()];
const _: [(); 24] = [(); size_of::<LibraryInfo>()];
const _: [(); 56] = [(); size_of::<Api>()];

struct RuntimeSlot {
    namespace: u16,
    generation: u32,
    open: bool,
    active_calls: usize,
    retired: bool,
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
            });
            self.slots.len() - 1
        };
        let slot = &mut self.slots[index];
        if slot.open || slot.retired || slot.active_calls != 0 {
            return Err(contract::MC_HOST_INTERNAL);
        }
        slot.namespace = namespace as u16;
        slot.open = true;
        self.next_namespace = namespace;
        Ok(pack(slot.namespace, slot.generation, index))
    }

    fn slot_mut(&mut self, handle: RuntimeHandle) -> Result<&mut RuntimeSlot, ResultCode> {
        let (namespace, kind, generation, index) = unpack(handle).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if kind != RUNTIME_KIND {
            return Err(contract::MC_HOST_WRONG_KIND);
        }
        let slot = self.slots.get_mut(index).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if !slot.open || slot.namespace != namespace || slot.generation != generation {
            return Err(contract::MC_HOST_STALE_HANDLE);
        }
        Ok(slot)
    }

    fn close(&mut self, handle: RuntimeHandle) -> Result<(), ResultCode> {
        let (_, _, _, index) = unpack(handle).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        let slot = self.slot_mut(handle)?;
        if slot.active_calls != 0 {
            return Err(contract::MC_HOST_BUSY);
        }
        slot.open = false;
        if slot.generation == MAX_GENERATION {
            slot.retired = true;
        } else {
            slot.generation += 1;
            self.free.push(index);
        }
        Ok(())
    }
}

fn pack(namespace: u16, generation: u32, index: usize) -> RuntimeHandle {
    ((namespace as u64) << 48)
        | ((RUNTIME_KIND as u64) << 40)
        | ((generation as u64) << 20)
        | (index as u64 + 1)
}

fn unpack(handle: RuntimeHandle) -> Option<(u16, u8, u32, usize)> {
    let index = (handle & INDEX_MASK) as usize;
    let generation = ((handle >> 20) & GENERATION_MASK) as u32;
    if index == 0 || generation == 0 {
        return None;
    }
    Some(((handle >> 48) as u16, ((handle >> 40) & 0xff) as u8, generation, index - 1))
}

fn runtimes() -> &'static Mutex<RuntimeTable> {
    static TABLE: OnceLock<Mutex<RuntimeTable>> = OnceLock::new();
    TABLE.get_or_init(|| Mutex::new(RuntimeTable::default()))
}

struct ActiveRuntime(RuntimeHandle);

impl ActiveRuntime {
    fn acquire(handle: RuntimeHandle) -> Result<Self, ResultCode> {
        let mut table = runtimes().lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let slot = table.slot_mut(handle)?;
        slot.active_calls = slot.active_calls.checked_add(1).ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        Ok(Self(handle))
    }
}

impl Drop for ActiveRuntime {
    fn drop(&mut self) {
        if let Ok(mut table) = runtimes().lock() {
            if let Ok(slot) = table.slot_mut(self.0) {
                slot.active_calls -= 1;
            }
        }
    }
}

fn valid_header(header: Header, minimum: usize) -> bool {
    header.version == 1 && header.flags == 0 && header.size as usize >= minimum
}

unsafe fn validate_options(options: *const RuntimeOptions) -> Result<(), ResultCode> {
    if options.is_null() {
        return Ok(());
    }
    let header = unsafe { (*options).header };
    if !valid_header(header, size_of::<RuntimeOptions>()) || header.size as usize > MAX_INPUT_STRUCT_SIZE {
        return Err(contract::MC_HOST_INVALID_ARGUMENT);
    }
    let tail_len = header.size as usize - size_of::<RuntimeOptions>();
    if tail_len != 0 {
        let tail = unsafe { std::slice::from_raw_parts((options as *const u8).add(size_of::<RuntimeOptions>()), tail_len) };
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
    let error = unsafe { &mut *error };
    if !valid_header(error.header, size_of::<Error>()) {
        return;
    }
    error.domain = if code == contract::MC_HOST_OK {
        0
    } else if code == contract::MC_HOST_STALE_HANDLE || code == contract::MC_HOST_WRONG_KIND {
        3
    } else {
        1
    };
    error.code = code as i32;
    error.retryable = u8::from(code == contract::MC_HOST_BUSY);
    error.reserved = [0; 7];
    error.message.required = message.len() as u64;
    if !error.message.data.is_null() && error.message.capacity != 0 {
        if let Ok(capacity) = usize::try_from(error.message.capacity) {
            let count = capacity.saturating_sub(1).min(message.len());
            unsafe {
                ptr::copy_nonoverlapping(message.as_ptr(), error.message.data, count);
                *error.message.data.add(count) = 0;
            }
        }
    }
}

fn guard(error: *mut Error, action: impl FnOnce() -> Result<(), ResultCode>) -> ResultCode {
    let outcome = catch_unwind(AssertUnwindSafe(action));
    let (code, message) = match outcome {
        Ok(Ok(())) => (contract::MC_HOST_OK, ""),
        Ok(Err(code)) => (code, "native host call rejected"),
        Err(_) => (contract::MC_HOST_INTERNAL, "panic contained at native host boundary"),
    };
    unsafe { write_error(error, code, message) };
    code
}

unsafe extern "C" fn runtime_create(options: *const RuntimeOptions, out: *mut RuntimeHandle, error: *mut Error) -> ResultCode {
    guard(error, || {
        if out.is_null() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe { *out = 0 };
        unsafe { validate_options(options)? };
        let handle = runtimes().lock().map_err(|_| contract::MC_HOST_INTERNAL)?.insert()?;
        unsafe { *out = handle };
        Ok(())
    })
}

unsafe extern "C" fn runtime_close(handle: RuntimeHandle, error: *mut Error) -> ResultCode {
    guard(error, || runtimes().lock().map_err(|_| contract::MC_HOST_INTERNAL)?.close(handle))
}

unsafe extern "C" fn library_info(out: *mut LibraryInfo, error: *mut Error) -> ResultCode {
    guard(error, || {
        if out.is_null() || !valid_header(unsafe { (*out).header }, size_of::<LibraryInfo>()) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        unsafe {
            (*out).header.size = size_of::<LibraryInfo>() as u32;
            (*out).abi_major = contract::MC_HOST_ABI_MAJOR;
            (*out).abi_minor = contract::MC_HOST_ABI_MINOR;
            (*out).implementation_table_size = size_of::<Api>() as u32;
            (*out).feature_bits = 0;
        }
        Ok(())
    })
}

unsafe extern "C" fn get_interface(handle: RuntimeHandle, _id: u32, _version: u16, _size: u32, _out: *mut c_void, error: *mut Error) -> ResultCode {
    guard(error, || {
        let _active = ActiveRuntime::acquire(handle)?;
        Err(contract::MC_HOST_UNSUPPORTED)
    })
}

static API: Api = Api {
    header: Header { size: size_of::<Api>() as u32, version: 1, flags: 0 },
    abi_major: contract::MC_HOST_ABI_MAJOR,
    abi_minor: contract::MC_HOST_ABI_MINOR,
    implementation_table_size: size_of::<Api>() as u32,
    feature_bits: 0,
    runtime_create,
    runtime_close,
    library_info,
    get_interface,
};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mc_host_get_api(major: u16, minimum_minor: u16, caller_size: u32, out: *mut c_void) -> ResultCode {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generation_and_namespace_reject_stale_runtime_handles() {
        let mut table = RuntimeTable::default();
        let first = table.insert().unwrap();
        table.close(first).unwrap();
        assert!(matches!(table.slot_mut(first), Err(contract::MC_HOST_STALE_HANDLE)));
        let second = table.insert().unwrap();
        assert_ne!(first, second);
        assert!(matches!(table.slot_mut(first), Err(contract::MC_HOST_STALE_HANDLE)));
        assert!(table.slot_mut(second).is_ok());
    }
}
