// @generated from contracts/embedder.kdl by //contracts/codegen:projector — do not edit.
#![no_std]
#![allow(dead_code)]

pub const MC_HOST_OK: u32 = 0;
pub const MC_HOST_BUFFER_TOO_SMALL: u32 = 1;
pub const MC_HOST_WOULD_BLOCK: u32 = 2;
pub const MC_HOST_INVALID_ARGUMENT: u32 = 3;
pub const MC_HOST_INVALID_STATE: u32 = 4;
pub const MC_HOST_STALE_HANDLE: u32 = 5;
pub const MC_HOST_WRONG_KIND: u32 = 6;
pub const MC_HOST_BUSY: u32 = 7;
pub const MC_HOST_NOT_QUIESCENT: u32 = 8;
pub const MC_HOST_UNSUPPORTED: u32 = 9;
pub const MC_HOST_UNSUPPORTED_ABI: u32 = 10;
pub const MC_HOST_CANCELLED: u32 = 11;
pub const MC_HOST_RESOURCE_LIMIT: u32 = 12;
pub const MC_HOST_KERNEL_ERROR: u32 = 13;
pub const MC_HOST_GUEST_TRAP: u32 = 14;
pub const MC_HOST_SNAPSHOT_ERROR: u32 = 15;
pub const MC_HOST_HOST_IO: u32 = 16;
pub const MC_HOST_POISONED: u32 = 17;
pub const MC_HOST_INTERNAL: u32 = 18;
pub const MC_HOST_OUTPUT_LOST: u32 = 19;
pub const MC_HOST_IFACE_VM_V1: u32 = 1;
pub const MC_HOST_IFACE_OPERATION_V1: u32 = 2;
pub const MC_HOST_IFACE_EFFECT_V1: u32 = 3;
pub const MC_HOST_IFACE_SNAPSHOT_V1: u32 = 4;
pub const MC_HOST_IFACE_NATIVE_CAPS_V1: u32 = 5;
pub const MC_HOST_IFACE_CATALOG_V1: u32 = 6;
pub const MC_HOST_IFACE_PERF_V1: u32 = 7;
pub const MC_HOST_IFACE_OUTPUT_V1: u32 = 8;
pub const MC_HOST_IFACE_ATTACHMENT_V1: u32 = 9;
pub const MC_HOST_FEATURE_VM_BASIC: u64 = 1;
pub const MC_HOST_FEATURE_OUTPUT: u64 = 2;
pub const MC_HOST_FEATURE_SNAPSHOT: u64 = 4;
pub const MC_HOST_FEATURE_EXEC_OPERATION: u64 = 8;
pub const MC_HOST_FEATURE_FS_OPERATION: u64 = 16;
pub const MC_HOST_FEATURE_AUTOCOMPLETE_OPERATION: u64 = 32;
pub const MC_HOST_FEATURE_SERVICE_OPERATION: u64 = 64;
pub const MC_HOST_FEATURE_COMMIT_OPERATION: u64 = 128;
pub const MC_HOST_FEATURE_ATTACHMENT_PLAN: u64 = 256;
pub const MC_HOST_FEATURE_HOST_CALL_EFFECT: u64 = 512;
pub const MC_HOST_FEATURE_MOUNT_OPERATION: u64 = 1024;
pub const MC_HOST_FEATURE_PERSIST_EFFECT: u64 = 2048;
pub const MC_HOST_FEATURE_HTTP_EFFECT: u64 = 4096;
pub const MC_HOST_FEATURE_WS_EFFECT: u64 = 8192;
pub const MC_HOST_FEATURE_NATIVE_CONNECTION: u64 = 16384;
pub const MC_HOST_DEFAULT_MAX_VMS: u32 = 16;
pub const MC_HOST_HARD_MAX_VMS: u32 = 16;
pub const MC_HOST_DEFAULT_MAX_DRIVE_TICKS: u32 = 1000;
pub const MC_HOST_HARD_MAX_DRIVE_TICKS: u32 = 1000;
pub const MC_HOST_DEFAULT_MAX_OUTPUT_BYTES: u32 = 4194304;
pub const MC_HOST_HARD_MAX_OUTPUT_BYTES: u32 = 4194304;
pub const MC_HOST_DEFAULT_MAX_OUTPUT_RECORDS: u32 = 4096;
pub const MC_HOST_HARD_MAX_OUTPUT_RECORDS: u32 = 4096;
pub const MC_HOST_DEFAULT_MAX_SNAPSHOT_BYTES: u32 = 536870912;
pub const MC_HOST_HARD_MAX_SNAPSHOT_BYTES: u32 = 536870912;
pub const MC_HOST_DEFAULT_MAX_SNAPSHOT_TOTAL_BYTES: u32 = 1073741824;
pub const MC_HOST_HARD_MAX_SNAPSHOT_TOTAL_BYTES: u32 = 1073741824;
pub const MC_HOST_DEFAULT_MAX_OPERATIONS_PER_VM: u32 = 64;
pub const MC_HOST_HARD_MAX_OPERATIONS_PER_VM: u32 = 64;
pub const MC_HOST_TICK_WAITING: u32 = 0;
pub const MC_HOST_TICK_RUNNABLE: u32 = 1;
pub const MC_HOST_TICK_EXITED: u32 = 2;
pub const MC_HOST_VM_RUNNABLE: u32 = 1;
pub const MC_HOST_VM_WAITING: u32 = 2;
pub const MC_HOST_VM_EXITED: u32 = 3;
pub const MC_HOST_VM_POISONED: u32 = 4;
pub const MC_HOST_VM_CLOSING: u32 = 5;
pub const MC_HOST_VM_FLAG_HAS_EXIT_CODE: u32 = 1;
pub const MC_HOST_VM_FLAG_AT_PROMPT: u32 = 2;
pub const MC_HOST_VM_CREATE_DETERMINISTIC: u32 = 1;
pub const MC_HOST_RESTORE_STRICT: u32 = 1;
pub const MC_HOST_RESTORE_DETACHED: u32 = 2;
pub const MC_HOST_DRIVE_STOP_BUDGET: u32 = 1;
pub const MC_HOST_DRIVE_STOP_WAITING: u32 = 2;
pub const MC_HOST_DRIVE_STOP_EXITED: u32 = 3;
pub const MC_HOST_DRIVE_STOP_OUTPUT: u32 = 4;
pub const MC_HOST_DRIVE_STOP_EFFECT: u32 = 5;
pub const MC_HOST_DRIVE_STOP_POISONED: u32 = 6;
pub const MC_HOST_DRIVE_STOP_OPERATION: u32 = 7;
pub const MC_HOST_DRIVE_ON_OUTPUT: u64 = 1;
pub const MC_HOST_DRIVE_ON_OPERATION: u64 = 2;
pub const MC_HOST_DRIVE_ON_EFFECT: u64 = 4;
pub const MC_HOST_VM_FLAG_OUTPUT_READY: u32 = 4;
pub const MC_HOST_VM_FLAG_OPERATION_READY: u32 = 8;
pub const MC_HOST_VM_FLAG_OPERATION_PENDING: u32 = 16;
pub const MC_HOST_VM_FLAG_EFFECT_READY: u32 = 32;
pub const MC_HOST_OUTPUT_DATA: u32 = 1;
pub const MC_HOST_OUTPUT_GAP: u32 = 2;
pub const MC_HOST_OUTPUT_ABOVE_HIGH_WATER: u32 = 1;
pub const MC_HOST_OUTPUT_POISONED: u32 = 2;
pub const MC_HOST_SNAPSHOT_FULL: u32 = 1;
pub const MC_HOST_SNAPSHOT_INCREMENTAL: u32 = 2;
pub const MC_HOST_OP_NEW: u32 = 0;
pub const MC_HOST_OP_PENDING: u32 = 1;
pub const MC_HOST_OP_READY: u32 = 2;
pub const MC_HOST_OP_FAILED: u32 = 3;
pub const MC_HOST_OP_CANCELLED: u32 = 4;
pub const MC_HOST_OP_EXEC_SHELL: u32 = 1;
pub const MC_HOST_OP_EXEC_DIRECT: u32 = 2;
pub const MC_HOST_OP_FS_READ: u32 = 3;
pub const MC_HOST_OP_FS_WRITE: u32 = 4;
pub const MC_HOST_OP_FS_READDIR: u32 = 5;
pub const MC_HOST_OP_FS_STAT: u32 = 6;
pub const MC_HOST_OP_FS_READLINK: u32 = 7;
pub const MC_HOST_OP_FS_MKDIR: u32 = 8;
pub const MC_HOST_OP_FS_UNLINK: u32 = 9;
pub const MC_HOST_OP_FS_CHMOD: u32 = 10;
pub const MC_HOST_OP_FS_SYMLINK: u32 = 11;
pub const MC_HOST_OP_MOUNT: u32 = 12;
pub const MC_HOST_OP_UNMOUNT: u32 = 13;
pub const MC_HOST_OP_AUTOCOMPLETE: u32 = 14;
pub const MC_HOST_OP_SERVICE_CALL: u32 = 15;
pub const MC_HOST_OP_COMMIT_LAYER: u32 = 16;
pub const MC_HOST_OP_CATALOG_INJECT: u32 = 17;
pub const MC_HOST_ATTACHMENT_HOST_CALL: u32 = 1;
pub const MC_HOST_ATTACHMENT_PERSIST: u32 = 2;
pub const MC_HOST_ATTACHMENT_HTTP_APPLICATION: u32 = 3;
pub const MC_HOST_ATTACHMENT_HOST_TOOL: u32 = 4;
pub const MC_HOST_ATTACHMENT_CONNECTION: u32 = 5;
pub const MC_HOST_ATTACHMENT_CONNECTION_POLICY: u32 = 6;
pub const MC_HOST_EVENT_HTTP_REQUEST: u32 = 1;
pub const MC_HOST_EVENT_HTTP_CANCEL: u32 = 2;
pub const MC_HOST_EVENT_WS_CONNECT: u32 = 3;
pub const MC_HOST_EVENT_WS_SEND: u32 = 4;
pub const MC_HOST_EVENT_WS_CLOSE: u32 = 5;
pub const MC_HOST_EVENT_HOST_CALL: u32 = 6;
pub const MC_HOST_EVENT_HOST_CALL_CLOSE: u32 = 7;
pub const MC_HOST_EVENT_PERSIST_GET: u32 = 8;
pub const MC_HOST_EVENT_PERSIST_PUT: u32 = 9;
pub const MC_HOST_EVENT_PERSIST_DELETE: u32 = 10;
pub const MC_HOST_EVENT_PERSIST_LIST: u32 = 11;
pub const MC_HOST_EVENT_TOOL_APPROVAL: u32 = 12;
pub const MC_HOST_EVENT_OPERATION_CANCEL: u32 = 13;
pub const MC_HOST_WS_CLOSE_GUEST: u32 = 1;
pub const MC_HOST_WS_CLOSE_VM: u32 = 2;

extern crate alloc;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum WireError { WrongMessage, UnsupportedVersion, Truncated, InvalidUtf8, NonCanonicalMap, InvalidPresence, TrailingBytes }

fn ctl_put_u16(out: &mut Vec<u8>, v: u16) { out.extend_from_slice(&v.to_le_bytes()); }
fn ctl_put_u32(out: &mut Vec<u8>, v: u32) { out.extend_from_slice(&v.to_le_bytes()); }
fn ctl_put_i32(out: &mut Vec<u8>, v: i32) { out.extend_from_slice(&v.to_le_bytes()); }
fn ctl_put_i64(out: &mut Vec<u8>, v: i64) { out.extend_from_slice(&v.to_le_bytes()); }
fn ctl_put_bool(out: &mut Vec<u8>, v: bool) { out.push(if v { 1 } else { 0 }); }
fn ctl_put_bytes(out: &mut Vec<u8>, v: &[u8]) { ctl_put_u32(out, v.len() as u32); out.extend_from_slice(v); }
fn ctl_put_str(out: &mut Vec<u8>, v: &str) { ctl_put_bytes(out, v.as_bytes()); }
fn ctl_put_strmap(out: &mut Vec<u8>, v: &BTreeMap<String, String>) { ctl_put_u32(out, v.len() as u32); for (k, val) in v { ctl_put_str(out, k); ctl_put_str(out, val); } }
fn ctl_put_message_list<T, F>(out: &mut Vec<u8>, values: &[T], mut encode: F) where F: FnMut(&T) -> Vec<u8> { ctl_put_u32(out, values.len() as u32); for value in values { let frame = encode(value); ctl_put_bytes(out, &frame); } }
fn ctl_need<'a>(bytes: &'a [u8], off: &mut usize, len: usize) -> Result<&'a [u8], WireError> { let end = off.checked_add(len).ok_or(WireError::Truncated)?; if end > bytes.len() { return Err(WireError::Truncated); } let out = &bytes[*off..end]; *off = end; Ok(out) }
fn ctl_read_u8(bytes: &[u8], off: &mut usize) -> Result<u8, WireError> { Ok(ctl_need(bytes, off, 1)?[0]) }
fn ctl_read_u16(bytes: &[u8], off: &mut usize) -> Result<u16, WireError> { let b = ctl_need(bytes, off, 2)?; Ok(u16::from_le_bytes([b[0], b[1]])) }
fn ctl_read_u32(bytes: &[u8], off: &mut usize) -> Result<u32, WireError> { let b = ctl_need(bytes, off, 4)?; Ok(u32::from_le_bytes([b[0], b[1], b[2], b[3]])) }
fn ctl_read_i32(bytes: &[u8], off: &mut usize) -> Result<i32, WireError> { let b = ctl_need(bytes, off, 4)?; Ok(i32::from_le_bytes([b[0], b[1], b[2], b[3]])) }
fn ctl_read_i64(bytes: &[u8], off: &mut usize) -> Result<i64, WireError> { let b = ctl_need(bytes, off, 8)?; Ok(i64::from_le_bytes([b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]])) }
fn ctl_read_bool(bytes: &[u8], off: &mut usize) -> Result<bool, WireError> { match ctl_read_u8(bytes, off)? { 0 => Ok(false), 1 => Ok(true), _ => Err(WireError::InvalidPresence) } }
fn ctl_read_bytes(bytes: &[u8], off: &mut usize) -> Result<Vec<u8>, WireError> { let len = ctl_read_u32(bytes, off)? as usize; Ok(ctl_need(bytes, off, len)?.to_vec()) }
fn ctl_read_str(bytes: &[u8], off: &mut usize) -> Result<String, WireError> { String::from_utf8(ctl_read_bytes(bytes, off)?).map_err(|_| WireError::InvalidUtf8) }
fn ctl_read_strmap(bytes: &[u8], off: &mut usize) -> Result<BTreeMap<String, String>, WireError> { let n = ctl_read_u32(bytes, off)? as usize; if n > bytes.len().saturating_sub(*off) / 8 { return Err(WireError::Truncated); } let mut out = BTreeMap::new(); let mut prev: Option<String> = None; for _ in 0..n { let k = ctl_read_str(bytes, off)?; if prev.as_ref().map_or(false, |p| p >= &k) { return Err(WireError::NonCanonicalMap); } let v = ctl_read_str(bytes, off)?; prev = Some(k.clone()); out.insert(k, v); } Ok(out) }

fn ctl_read_message_list<T, F>(bytes: &[u8], off: &mut usize, mut decode: F) -> Result<Vec<T>, WireError> where F: FnMut(&[u8]) -> Result<T, WireError> { let n = ctl_read_u32(bytes, off)? as usize; if n > bytes.len().saturating_sub(*off) / 4 { return Err(WireError::Truncated); } let mut out = Vec::with_capacity(n); for _ in 0..n { let frame = ctl_read_bytes(bytes, off)?; out.push(decode(&frame)?); } Ok(out) }

/// Versioned closed attachment variant. The generated kind table defines the payload message.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct AttachmentDefinition {
    pub kind: u32,
    pub payload: Vec<u8>,
}

pub const ATTACHMENT_DEFINITION_MSG_ID: u16 = 20;
pub const ATTACHMENT_DEFINITION_VERSION: u8 = 1;
impl AttachmentDefinition {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, ATTACHMENT_DEFINITION_MSG_ID);
        out.push(ATTACHMENT_DEFINITION_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_bytes(&mut out, &self.payload);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != ATTACHMENT_DEFINITION_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != ATTACHMENT_DEFINITION_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let payload = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            payload,
        })
    }
}

/// One asynchronous host-control request. Payload is the generated kind-specific message.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct OperationRequest {
    pub kind: u32,
    pub payload: Vec<u8>,
}

pub const OPERATION_REQUEST_MSG_ID: u16 = 21;
pub const OPERATION_REQUEST_VERSION: u8 = 1;
impl OperationRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, OPERATION_REQUEST_MSG_ID);
        out.push(OPERATION_REQUEST_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_bytes(&mut out, &self.payload);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != OPERATION_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != OPERATION_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let payload = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            payload,
        })
    }
}

/// One asynchronous host-control terminal value. Guest status remains data.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct OperationResult {
    pub kind: u32,
    pub status: i32,
    pub payload: Vec<u8>,
}

pub const OPERATION_RESULT_MSG_ID: u16 = 22;
pub const OPERATION_RESULT_VERSION: u8 = 1;
impl OperationResult {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, OPERATION_RESULT_MSG_ID);
        out.push(OPERATION_RESULT_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_i32(&mut out, self.status);
        ctl_put_bytes(&mut out, &self.payload);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != OPERATION_RESULT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != OPERATION_RESULT_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let status = ctl_read_i32(bytes, &mut off)?;
        let payload = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            status,
            payload,
        })
    }
}

/// Generation-bound, typed native-host effect envelope.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct EffectEvent {
    pub kind: u32,
    pub event_id: i64,
    pub vm_id: i64,
    pub operation_id: Option<i64>,
    pub socket_id: Option<i64>,
    pub flags: u32,
    pub payload: Vec<u8>,
}

pub const EFFECT_EVENT_MSG_ID: u16 = 23;
pub const EFFECT_EVENT_VERSION: u8 = 1;
impl EffectEvent {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, EFFECT_EVENT_MSG_ID);
        out.push(EFFECT_EVENT_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_i64(&mut out, self.event_id);
        ctl_put_i64(&mut out, self.vm_id);
        match &self.operation_id {
            Some(v) => {
                out.push(1);
        ctl_put_i64(&mut out, *v);
            }
            None => out.push(0),
        }
        match &self.socket_id {
            Some(v) => {
                out.push(1);
        ctl_put_i64(&mut out, *v);
            }
            None => out.push(0),
        }
        ctl_put_u32(&mut out, self.flags);
        ctl_put_bytes(&mut out, &self.payload);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != EFFECT_EVENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != EFFECT_EVENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let event_id = ctl_read_i64(bytes, &mut off)?;
        let vm_id = ctl_read_i64(bytes, &mut off)?;
        let operation_id = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_i64(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let socket_id = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_i64(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let flags = ctl_read_u32(bytes, &mut off)?;
        let payload = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            event_id,
            vm_id,
            operation_id,
            socket_id,
            flags,
            payload,
        })
    }
}

/// Exactly one kind-matched terminal response for an effect event.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct EffectResponse {
    pub kind: u32,
    pub event_id: i64,
    pub payload: Vec<u8>,
}

pub const EFFECT_RESPONSE_MSG_ID: u16 = 24;
pub const EFFECT_RESPONSE_VERSION: u8 = 1;
impl EffectResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, EFFECT_RESPONSE_MSG_ID);
        out.push(EFFECT_RESPONSE_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_i64(&mut out, self.event_id);
        ctl_put_bytes(&mut out, &self.payload);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != EFFECT_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != EFFECT_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let event_id = ctl_read_i64(bytes, &mut off)?;
        let payload = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            event_id,
            payload,
        })
    }
}

/// Non-secret attachment identity included in strict-restore companion metadata.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct AttachmentManifestEntry {
    pub kind: u32,
    pub identity: String,
    pub digest: Vec<u8>,
}

pub const ATTACHMENT_MANIFEST_ENTRY_MSG_ID: u16 = 25;
pub const ATTACHMENT_MANIFEST_ENTRY_VERSION: u8 = 1;
impl AttachmentManifestEntry {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, ATTACHMENT_MANIFEST_ENTRY_MSG_ID);
        out.push(ATTACHMENT_MANIFEST_ENTRY_VERSION);
        ctl_put_u32(&mut out, self.kind);
        ctl_put_str(&mut out, &self.identity);
        ctl_put_bytes(&mut out, &self.digest);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != ATTACHMENT_MANIFEST_ENTRY_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != ATTACHMENT_MANIFEST_ENTRY_VERSION { return Err(WireError::UnsupportedVersion); }
        let kind = ctl_read_u32(bytes, &mut off)?;
        let identity = ctl_read_str(bytes, &mut off)?;
        let digest = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            kind,
            identity,
            digest,
        })
    }
}

/// Canonical non-secret attachment manifest. Credentials and callback addresses are absent.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct AttachmentManifest {
    pub entries: Vec<AttachmentManifestEntry>,
}

pub const ATTACHMENT_MANIFEST_MSG_ID: u16 = 26;
pub const ATTACHMENT_MANIFEST_VERSION: u8 = 1;
impl AttachmentManifest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, ATTACHMENT_MANIFEST_MSG_ID);
        out.push(ATTACHMENT_MANIFEST_VERSION);
        ctl_put_message_list(&mut out, &self.entries, |v| v.encode());
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != ATTACHMENT_MANIFEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != ATTACHMENT_MANIFEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let entries = ctl_read_message_list(bytes, &mut off, AttachmentManifestEntry::decode)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            entries,
        })
    }
}

/// One native filesystem control attempt. The operation kind selects and validates the required fields.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct FilesystemRequest {
    pub path: String,
    pub data: Option<Vec<u8>>,
    pub target: Option<String>,
    pub mode: Option<u32>,
}

pub const FILESYSTEM_REQUEST_MSG_ID: u16 = 27;
pub const FILESYSTEM_REQUEST_VERSION: u8 = 1;
impl FilesystemRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, FILESYSTEM_REQUEST_MSG_ID);
        out.push(FILESYSTEM_REQUEST_VERSION);
        ctl_put_str(&mut out, &self.path);
        match &self.data {
            Some(v) => {
                out.push(1);
        ctl_put_bytes(&mut out, v);
            }
            None => out.push(0),
        }
        match &self.target {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        match &self.mode {
            Some(v) => {
                out.push(1);
        ctl_put_u32(&mut out, *v);
            }
            None => out.push(0),
        }
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != FILESYSTEM_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != FILESYSTEM_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let path = ctl_read_str(bytes, &mut off)?;
        let data = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_bytes(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let target = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let mode = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_u32(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            path,
            data,
            target,
            mode,
        })
    }
}

/// Content-addressed layer produced by mc_commit_layer.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct LayerCommitResult {
    pub digest: String,
    pub layer: Vec<u8>,
}

pub const LAYER_COMMIT_RESULT_MSG_ID: u16 = 28;
pub const LAYER_COMMIT_RESULT_VERSION: u8 = 1;
impl LayerCommitResult {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, LAYER_COMMIT_RESULT_MSG_ID);
        out.push(LAYER_COMMIT_RESULT_VERSION);
        ctl_put_str(&mut out, &self.digest);
        ctl_put_bytes(&mut out, &self.layer);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != LAYER_COMMIT_RESULT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != LAYER_COMMIT_RESULT_VERSION { return Err(WireError::UnsupportedVersion); }
        let digest = ctl_read_str(bytes, &mut off)?;
        let layer = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            digest,
            layer,
        })
    }
}

/// One application-provided host-call route and its non-secret stable identity.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HostCallAttachment {
    pub name: String,
    pub identity: String,
}

pub const HOST_CALL_ATTACHMENT_MSG_ID: u16 = 29;
pub const HOST_CALL_ATTACHMENT_VERSION: u8 = 1;
impl HostCallAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HOST_CALL_ATTACHMENT_MSG_ID);
        out.push(HOST_CALL_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.name);
        ctl_put_str(&mut out, &self.identity);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HOST_CALL_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HOST_CALL_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let name = ctl_read_str(bytes, &mut off)?;
        let identity = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            name,
            identity,
        })
    }
}

/// Mount a plan-registered host-call driver at its own path.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct MountRequest {
    pub path: String,
    pub read_only: bool,
}

pub const MOUNT_REQUEST_MSG_ID: u16 = 30;
pub const MOUNT_REQUEST_VERSION: u8 = 1;
impl MountRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, MOUNT_REQUEST_MSG_ID);
        out.push(MOUNT_REQUEST_VERSION);
        ctl_put_str(&mut out, &self.path);
        ctl_put_bool(&mut out, self.read_only);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != MOUNT_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != MOUNT_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let path = ctl_read_str(bytes, &mut off)?;
        let read_only = ctl_read_bool(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            path,
            read_only,
        })
    }
}

/// Binary-safe request delivered to an application host-call route.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HostCallEvent {
    pub name: String,
    pub request: Vec<u8>,
}

pub const HOST_CALL_EVENT_MSG_ID: u16 = 31;
pub const HOST_CALL_EVENT_VERSION: u8 = 1;
impl HostCallEvent {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HOST_CALL_EVENT_MSG_ID);
        out.push(HOST_CALL_EVENT_VERSION);
        ctl_put_str(&mut out, &self.name);
        ctl_put_bytes(&mut out, &self.request);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HOST_CALL_EVENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HOST_CALL_EVENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let name = ctl_read_str(bytes, &mut off)?;
        let request = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            name,
            request,
        })
    }
}

/// One whole host-call response; the kernel has no body-pending sentinel.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HostCallResponse {
    pub failed: bool,
    pub body: Vec<u8>,
}

pub const HOST_CALL_RESPONSE_MSG_ID: u16 = 32;
pub const HOST_CALL_RESPONSE_VERSION: u8 = 1;
impl HostCallResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HOST_CALL_RESPONSE_MSG_ID);
        out.push(HOST_CALL_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.failed);
        ctl_put_bytes(&mut out, &self.body);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HOST_CALL_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HOST_CALL_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let failed = ctl_read_bool(bytes, &mut off)?;
        let body = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            failed,
            body,
        })
    }
}

/// One cancellation notice for a leased effect. Acknowledgment retires the original lease.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct EffectCancel {
    pub original_event_id: i64,
}

pub const EFFECT_CANCEL_MSG_ID: u16 = 33;
pub const EFFECT_CANCEL_VERSION: u8 = 1;
impl EffectCancel {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, EFFECT_CANCEL_MSG_ID);
        out.push(EFFECT_CANCEL_VERSION);
        ctl_put_i64(&mut out, self.original_event_id);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != EFFECT_CANCEL_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != EFFECT_CANCEL_VERSION { return Err(WireError::UnsupportedVersion); }
        let original_event_id = ctl_read_i64(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            original_event_id,
        })
    }
}

/// Non-secret identity of the application persistence provider.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct PersistAttachment {
    pub identity: String,
}

pub const PERSIST_ATTACHMENT_MSG_ID: u16 = 34;
pub const PERSIST_ATTACHMENT_VERSION: u8 = 1;
impl PersistAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, PERSIST_ATTACHMENT_MSG_ID);
        out.push(PERSIST_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.identity);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != PERSIST_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != PERSIST_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let identity = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            identity,
        })
    }
}

/// Typed persistence request. The event kind selects get, put, delete, or list.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct PersistEffectRequest {
    pub key: Vec<u8>,
    pub value: Option<Vec<u8>>,
}

pub const PERSIST_EFFECT_REQUEST_MSG_ID: u16 = 35;
pub const PERSIST_EFFECT_REQUEST_VERSION: u8 = 1;
impl PersistEffectRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, PERSIST_EFFECT_REQUEST_MSG_ID);
        out.push(PERSIST_EFFECT_REQUEST_VERSION);
        ctl_put_bytes(&mut out, &self.key);
        match &self.value {
            Some(v) => {
                out.push(1);
        ctl_put_bytes(&mut out, v);
            }
            None => out.push(0),
        }
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != PERSIST_EFFECT_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != PERSIST_EFFECT_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let key = ctl_read_bytes(bytes, &mut off)?;
        let value = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_bytes(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            key,
            value,
        })
    }
}

/// One complete persistence result delivered before kernel poll reports ready.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct PersistEffectResponse {
    pub failed: bool,
    pub body: Vec<u8>,
}

pub const PERSIST_EFFECT_RESPONSE_MSG_ID: u16 = 36;
pub const PERSIST_EFFECT_RESPONSE_VERSION: u8 = 1;
impl PersistEffectResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, PERSIST_EFFECT_RESPONSE_MSG_ID);
        out.push(PERSIST_EFFECT_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.failed);
        ctl_put_bytes(&mut out, &self.body);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != PERSIST_EFFECT_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != PERSIST_EFFECT_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let failed = ctl_read_bool(bytes, &mut off)?;
        let body = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            failed,
            body,
        })
    }
}

/// Non-secret identity of the application HTTP transport. Connection markers are refused.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HttpApplicationAttachment {
    pub identity: String,
}

pub const HTTP_APPLICATION_ATTACHMENT_MSG_ID: u16 = 37;
pub const HTTP_APPLICATION_ATTACHMENT_VERSION: u8 = 1;
impl HttpApplicationAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HTTP_APPLICATION_ATTACHMENT_MSG_ID);
        out.push(HTTP_APPLICATION_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.identity);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HTTP_APPLICATION_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HTTP_APPLICATION_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let identity = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            identity,
        })
    }
}

/// Unmarked kernel HTTP request blob. No credential is injected by this transport.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HttpEffectRequest {
    pub request: Vec<u8>,
}

pub const HTTP_EFFECT_REQUEST_MSG_ID: u16 = 38;
pub const HTTP_EFFECT_REQUEST_VERSION: u8 = 1;
impl HttpEffectRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HTTP_EFFECT_REQUEST_MSG_ID);
        out.push(HTTP_EFFECT_REQUEST_VERSION);
        ctl_put_bytes(&mut out, &self.request);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HTTP_EFFECT_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HTTP_EFFECT_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let request = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            request,
        })
    }
}

/// Complete HTTP response. The head is at most the kernel HEAD_BUF size.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HttpEffectResponse {
    pub failed: bool,
    pub head: Vec<u8>,
    pub body: Vec<u8>,
}

pub const HTTP_EFFECT_RESPONSE_MSG_ID: u16 = 39;
pub const HTTP_EFFECT_RESPONSE_VERSION: u8 = 1;
impl HttpEffectResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HTTP_EFFECT_RESPONSE_MSG_ID);
        out.push(HTTP_EFFECT_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.failed);
        ctl_put_bytes(&mut out, &self.head);
        ctl_put_bytes(&mut out, &self.body);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HTTP_EFFECT_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HTTP_EFFECT_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let failed = ctl_read_bool(bytes, &mut off)?;
        let head = ctl_read_bytes(bytes, &mut off)?;
        let body = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            failed,
            head,
            body,
        })
    }
}

/// Guest WebSocket URL presented to the application transport.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct WsConnectRequest {
    pub url: String,
}

pub const WS_CONNECT_REQUEST_MSG_ID: u16 = 40;
pub const WS_CONNECT_REQUEST_VERSION: u8 = 1;
impl WsConnectRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, WS_CONNECT_REQUEST_MSG_ID);
        out.push(WS_CONNECT_REQUEST_VERSION);
        ctl_put_str(&mut out, &self.url);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != WS_CONNECT_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != WS_CONNECT_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let url = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            url,
        })
    }
}

/// Handshake completion. A failed handshake wakes the guest with an error.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct WsConnectResponse {
    pub failed: bool,
}

pub const WS_CONNECT_RESPONSE_MSG_ID: u16 = 41;
pub const WS_CONNECT_RESPONSE_VERSION: u8 = 1;
impl WsConnectResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, WS_CONNECT_RESPONSE_MSG_ID);
        out.push(WS_CONNECT_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.failed);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != WS_CONNECT_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != WS_CONNECT_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let failed = ctl_read_bool(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            failed,
        })
    }
}

/// One guest WebSocket message held under the per-socket send credit.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct WsSendRequest {
    pub data: Vec<u8>,
}

pub const WS_SEND_REQUEST_MSG_ID: u16 = 42;
pub const WS_SEND_REQUEST_VERSION: u8 = 1;
impl WsSendRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, WS_SEND_REQUEST_MSG_ID);
        out.push(WS_SEND_REQUEST_VERSION);
        ctl_put_bytes(&mut out, &self.data);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != WS_SEND_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != WS_SEND_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let data = ctl_read_bytes(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            data,
        })
    }
}

/// Send completion releases credit; failure closes the guest socket.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct WsSendResponse {
    pub failed: bool,
}

pub const WS_SEND_RESPONSE_MSG_ID: u16 = 43;
pub const WS_SEND_RESPONSE_VERSION: u8 = 1;
impl WsSendResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, WS_SEND_RESPONSE_MSG_ID);
        out.push(WS_SEND_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.failed);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != WS_SEND_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != WS_SEND_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let failed = ctl_read_bool(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            failed,
        })
    }
}

/// Close notification for an application-owned WebSocket connection.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct WsCloseEvent {
    pub reason: u32,
}

pub const WS_CLOSE_EVENT_MSG_ID: u16 = 44;
pub const WS_CLOSE_EVENT_VERSION: u8 = 1;
impl WsCloseEvent {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, WS_CLOSE_EVENT_MSG_ID);
        out.push(WS_CLOSE_EVENT_VERSION);
        ctl_put_u32(&mut out, self.reason);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != WS_CLOSE_EVENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != WS_CLOSE_EVENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let reason = ctl_read_u32(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            reason,
        })
    }
}

/// One complete content-addressed catalog shard to stage before apply.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct CatalogRecord {
    pub sha: String,
    pub bytes: Vec<u8>,
}

pub const CATALOG_RECORD_MSG_ID: u16 = 45;
pub const CATALOG_RECORD_VERSION: u8 = 1;
impl CatalogRecord {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CATALOG_RECORD_MSG_ID);
        out.push(CATALOG_RECORD_VERSION);
        ctl_put_str(&mut out, &self.sha);
        ctl_put_bytes(&mut out, &self.bytes);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let frame_len = bytes.len();
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CATALOG_RECORD_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CATALOG_RECORD_VERSION { return Err(WireError::UnsupportedVersion); }
        let sha = ctl_read_str(bytes, &mut off)?;
        let bytes = ctl_read_bytes(bytes, &mut off)?;
        if off != frame_len { return Err(WireError::TrailingBytes); }
        Ok(Self {
            sha,
            bytes,
        })
    }
}

/// Transactional live catalog update. Base digest is required; every indexed shard is supplied.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct CatalogApplyRequest {
    pub index_json: Vec<u8>,
    pub base_digest: String,
    pub records: Vec<CatalogRecord>,
}

pub const CATALOG_APPLY_REQUEST_MSG_ID: u16 = 46;
pub const CATALOG_APPLY_REQUEST_VERSION: u8 = 1;
impl CatalogApplyRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CATALOG_APPLY_REQUEST_MSG_ID);
        out.push(CATALOG_APPLY_REQUEST_VERSION);
        ctl_put_bytes(&mut out, &self.index_json);
        ctl_put_str(&mut out, &self.base_digest);
        ctl_put_message_list(&mut out, &self.records, |v| v.encode());
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CATALOG_APPLY_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CATALOG_APPLY_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let index_json = ctl_read_bytes(bytes, &mut off)?;
        let base_digest = ctl_read_str(bytes, &mut off)?;
        let records = ctl_read_message_list(bytes, &mut off, CatalogRecord::decode)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            index_json,
            base_digest,
            records,
        })
    }
}

/// Host tool definition supplied with a strict restore plan. Its shard must match the restored catalog exactly.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct HostToolAttachment {
    pub address: String,
    pub description: String,
    pub binding_name: String,
    pub args_mode: String,
    pub input_schema: Option<String>,
    pub output_schema: Option<String>,
    pub annotations: Option<String>,
}

pub const HOST_TOOL_ATTACHMENT_MSG_ID: u16 = 47;
pub const HOST_TOOL_ATTACHMENT_VERSION: u8 = 1;
impl HostToolAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, HOST_TOOL_ATTACHMENT_MSG_ID);
        out.push(HOST_TOOL_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.address);
        ctl_put_str(&mut out, &self.description);
        ctl_put_str(&mut out, &self.binding_name);
        ctl_put_str(&mut out, &self.args_mode);
        match &self.input_schema {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        match &self.output_schema {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        match &self.annotations {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != HOST_TOOL_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != HOST_TOOL_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let address = ctl_read_str(bytes, &mut off)?;
        let description = ctl_read_str(bytes, &mut off)?;
        let binding_name = ctl_read_str(bytes, &mut off)?;
        let args_mode = ctl_read_str(bytes, &mut off)?;
        let input_schema = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let output_schema = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let annotations = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            address,
            description,
            binding_name,
            args_mode,
            input_schema,
            output_schema,
            annotations,
        })
    }
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ConnectionOrigin {
    pub value: String,
}

pub const CONNECTION_ORIGIN_MSG_ID: u16 = 54;
pub const CONNECTION_ORIGIN_VERSION: u8 = 1;
impl ConnectionOrigin {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CONNECTION_ORIGIN_MSG_ID);
        out.push(CONNECTION_ORIGIN_VERSION);
        ctl_put_str(&mut out, &self.value);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CONNECTION_ORIGIN_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CONNECTION_ORIGIN_VERSION { return Err(WireError::UnsupportedVersion); }
        let value = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            value,
        })
    }
}

/// The non-secret connection authority that strict restore must reattach.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ConnectionIdentity {
    pub reference: String,
    pub origins: Vec<ConnectionOrigin>,
}

pub const CONNECTION_IDENTITY_MSG_ID: u16 = 53;
pub const CONNECTION_IDENTITY_VERSION: u8 = 1;
impl ConnectionIdentity {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CONNECTION_IDENTITY_MSG_ID);
        out.push(CONNECTION_IDENTITY_VERSION);
        ctl_put_str(&mut out, &self.reference);
        ctl_put_message_list(&mut out, &self.origins, |v| v.encode());
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CONNECTION_IDENTITY_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CONNECTION_IDENTITY_VERSION { return Err(WireError::UnsupportedVersion); }
        let reference = ctl_read_str(bytes, &mut off)?;
        let origins = ctl_read_message_list(bytes, &mut off, ConnectionOrigin::decode)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            reference,
            origins,
        })
    }
}

/// Non-secret companion metadata for an exact MCSN and its native attachment identities.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct SnapshotAttachmentManifest {
    pub content_digest: Vec<u8>,
    pub routes: Vec<HostCallAttachment>,
    pub persist_identity: Option<String>,
    pub http_identity: Option<String>,
    pub connections: Vec<ConnectionIdentity>,
    pub policy_digest: Option<Vec<u8>>,
    pub native_net: bool,
}

pub const SNAPSHOT_ATTACHMENT_MANIFEST_MSG_ID: u16 = 48;
pub const SNAPSHOT_ATTACHMENT_MANIFEST_VERSION: u8 = 2;
impl SnapshotAttachmentManifest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, SNAPSHOT_ATTACHMENT_MANIFEST_MSG_ID);
        out.push(SNAPSHOT_ATTACHMENT_MANIFEST_VERSION);
        ctl_put_bytes(&mut out, &self.content_digest);
        ctl_put_message_list(&mut out, &self.routes, |v| v.encode());
        match &self.persist_identity {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        match &self.http_identity {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        ctl_put_message_list(&mut out, &self.connections, |v| v.encode());
        match &self.policy_digest {
            Some(v) => {
                out.push(1);
        ctl_put_bytes(&mut out, v);
            }
            None => out.push(0),
        }
        ctl_put_bool(&mut out, self.native_net);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != SNAPSHOT_ATTACHMENT_MANIFEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != SNAPSHOT_ATTACHMENT_MANIFEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let content_digest = ctl_read_bytes(bytes, &mut off)?;
        let routes = ctl_read_message_list(bytes, &mut off, HostCallAttachment::decode)?;
        let persist_identity = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let http_identity = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let connections = ctl_read_message_list(bytes, &mut off, ConnectionIdentity::decode)?;
        let policy_digest = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_bytes(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        let native_net = ctl_read_bool(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            content_digest,
            routes,
            persist_identity,
            http_identity,
            connections,
            policy_digest,
            native_net,
        })
    }
}

/// A private credential and its explicit allowed origins. Secret fields never enter snapshot metadata.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ConnectionAttachment {
    pub reference: String,
    pub credential_kind: String,
    pub credential_a: String,
    pub credential_b: String,
    pub origins: Vec<ConnectionOrigin>,
}

pub const CONNECTION_ATTACHMENT_MSG_ID: u16 = 49;
pub const CONNECTION_ATTACHMENT_VERSION: u8 = 1;
impl ConnectionAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CONNECTION_ATTACHMENT_MSG_ID);
        out.push(CONNECTION_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.reference);
        ctl_put_str(&mut out, &self.credential_kind);
        ctl_put_str(&mut out, &self.credential_a);
        ctl_put_str(&mut out, &self.credential_b);
        ctl_put_message_list(&mut out, &self.origins, |v| v.encode());
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CONNECTION_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CONNECTION_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let reference = ctl_read_str(bytes, &mut off)?;
        let credential_kind = ctl_read_str(bytes, &mut off)?;
        let credential_a = ctl_read_str(bytes, &mut off)?;
        let credential_b = ctl_read_str(bytes, &mut off)?;
        let origins = ctl_read_message_list(bytes, &mut off, ConnectionOrigin::decode)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            reference,
            credential_kind,
            credential_a,
            credential_b,
            origins,
        })
    }
}

/// One rule for the shared host connection policy engine.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ConnectionPolicyAttachment {
    pub owner: String,
    pub pattern: String,
    pub action: String,
}

pub const CONNECTION_POLICY_ATTACHMENT_MSG_ID: u16 = 50;
pub const CONNECTION_POLICY_ATTACHMENT_VERSION: u8 = 1;
impl ConnectionPolicyAttachment {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, CONNECTION_POLICY_ATTACHMENT_MSG_ID);
        out.push(CONNECTION_POLICY_ATTACHMENT_VERSION);
        ctl_put_str(&mut out, &self.owner);
        ctl_put_str(&mut out, &self.pattern);
        ctl_put_str(&mut out, &self.action);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != CONNECTION_POLICY_ATTACHMENT_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != CONNECTION_POLICY_ATTACHMENT_VERSION { return Err(WireError::UnsupportedVersion); }
        let owner = ctl_read_str(bytes, &mut off)?;
        let pattern = ctl_read_str(bytes, &mut off)?;
        let action = ctl_read_str(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            owner,
            pattern,
            action,
        })
    }
}

/// Host-computed non-secret facts for one pending native HTTP request.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ToolApprovalRequest {
    pub connection: String,
    pub method: String,
    pub url: String,
    pub origin: String,
    pub args_digest: Option<String>,
}

pub const TOOL_APPROVAL_REQUEST_MSG_ID: u16 = 51;
pub const TOOL_APPROVAL_REQUEST_VERSION: u8 = 1;
impl ToolApprovalRequest {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, TOOL_APPROVAL_REQUEST_MSG_ID);
        out.push(TOOL_APPROVAL_REQUEST_VERSION);
        ctl_put_str(&mut out, &self.connection);
        ctl_put_str(&mut out, &self.method);
        ctl_put_str(&mut out, &self.url);
        ctl_put_str(&mut out, &self.origin);
        match &self.args_digest {
            Some(v) => {
                out.push(1);
        ctl_put_str(&mut out, v);
            }
            None => out.push(0),
        }
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != TOOL_APPROVAL_REQUEST_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != TOOL_APPROVAL_REQUEST_VERSION { return Err(WireError::UnsupportedVersion); }
        let connection = ctl_read_str(bytes, &mut off)?;
        let method = ctl_read_str(bytes, &mut off)?;
        let url = ctl_read_str(bytes, &mut off)?;
        let origin = ctl_read_str(bytes, &mut off)?;
        let args_digest = match ctl_read_u8(bytes, &mut off)? {
            0 => None,
            1 => Some(ctl_read_str(bytes, &mut off)?),
            _ => return Err(WireError::InvalidPresence),
        };
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            connection,
            method,
            url,
            origin,
            args_digest,
        })
    }
}

/// One approval decision for the waiting native HTTP request.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ToolApprovalResponse {
    pub allow: bool,
    pub remember_session: bool,
}

pub const TOOL_APPROVAL_RESPONSE_MSG_ID: u16 = 52;
pub const TOOL_APPROVAL_RESPONSE_VERSION: u8 = 1;
impl ToolApprovalResponse {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        ctl_put_u16(&mut out, TOOL_APPROVAL_RESPONSE_MSG_ID);
        out.push(TOOL_APPROVAL_RESPONSE_VERSION);
        ctl_put_bool(&mut out, self.allow);
        ctl_put_bool(&mut out, self.remember_session);
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self, WireError> {
        let mut off = 0usize;
        if ctl_read_u16(bytes, &mut off)? != TOOL_APPROVAL_RESPONSE_MSG_ID { return Err(WireError::WrongMessage); }
        if ctl_read_u8(bytes, &mut off)? != TOOL_APPROVAL_RESPONSE_VERSION { return Err(WireError::UnsupportedVersion); }
        let allow = ctl_read_bool(bytes, &mut off)?;
        let remember_session = ctl_read_bool(bytes, &mut off)?;
        if off != bytes.len() { return Err(WireError::TrailingBytes); }
        Ok(Self {
            allow,
            remember_session,
        })
    }
}

// Native embedder ABI constants.
pub const MC_HOST_ABI_MAJOR: u16 = 1;
pub const MC_HOST_ABI_MINOR: u16 = 0;
pub const MC_HOST_LAYOUT_HEADER_SIZE: usize = 8;
pub const MC_HOST_LAYOUT_HEADER_ALIGN: usize = 4;
pub const MC_HOST_LAYOUT_HEADER_SIZE_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_HEADER_VERSION_OFFSET: usize = 4;
pub const MC_HOST_LAYOUT_HEADER_FLAGS_OFFSET: usize = 6;
pub const MC_HOST_LAYOUT_BUFFER_SIZE: usize = 24;
pub const MC_HOST_LAYOUT_BUFFER_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_BUFFER_DATA_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_BUFFER_CAPACITY_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_BUFFER_REQUIRED_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_ERROR_SIZE: usize = 48;
pub const MC_HOST_LAYOUT_ERROR_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_ERROR_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_ERROR_DOMAIN_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_ERROR_CODE_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_ERROR_RETRYABLE_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_ERROR_RESERVED_OFFSET: usize = 17;
pub const MC_HOST_LAYOUT_ERROR_MESSAGE_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_SIZE: usize = 36;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_ALIGN: usize = 4;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_VMS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_DRIVE_TICKS_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OUTPUT_BYTES_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OUTPUT_RECORDS_OFFSET: usize = 20;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_SNAPSHOT_BYTES_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_SNAPSHOT_TOTAL_BYTES_OFFSET: usize = 28;
pub const MC_HOST_LAYOUT_RUNTIME_OPTIONS_MAX_OPERATIONS_PER_VM_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_SIZE: usize = 24;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MAJOR_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_ABI_MINOR_OFFSET: usize = 10;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_IMPLEMENTATION_TABLE_SIZE_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_LIBRARY_INFO_FEATURE_BITS_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_API_SIZE: usize = 56;
pub const MC_HOST_LAYOUT_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_API_ABI_MAJOR_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_API_ABI_MINOR_OFFSET: usize = 10;
pub const MC_HOST_LAYOUT_API_IMPLEMENTATION_TABLE_SIZE_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_API_FEATURE_BITS_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_API_RUNTIME_CREATE_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_API_RUNTIME_CLOSE_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_API_LIBRARY_INFO_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_API_GET_INTERFACE_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_BYTES_SIZE: usize = 16;
pub const MC_HOST_LAYOUT_BYTES_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_BYTES_DATA_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_BYTES_LEN_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_KERNEL_INFO_SIZE: usize = 40;
pub const MC_HOST_LAYOUT_KERNEL_INFO_ALIGN: usize = 4;
pub const MC_HOST_LAYOUT_KERNEL_INFO_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_KERNEL_INFO_DIGEST_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_SIZE: usize = 56;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYERS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_LAYER_COUNT_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_TIER_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_BUDGET_MIB_OFFSET: usize = 28;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_CONTRACT_FUEL_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_REQUESTED_WORKERS_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_FLAGS_OFFSET: usize = 44;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_COLS_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_VM_CREATE_OPTIONS_TERMINAL_ROWS_OFFSET: usize = 52;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SIZE: usize = 48;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_SNAPSHOT_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_BASELINE_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_MODE_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_VM_RESTORE_OPTIONS_RESERVED_OFFSET: usize = 44;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_SIZE: usize = 24;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_MAX_TICKS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_RESERVED_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_DRIVE_OPTIONS_STOP_MASK_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_VM_STATUS_SIZE: usize = 64;
pub const MC_HOST_LAYOUT_VM_STATUS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_VM_STATUS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_VM_STATUS_STATE_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_VM_STATUS_LAST_TICK_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_VM_STATUS_FLAGS_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_VM_STATUS_EXIT_CODE_OFFSET: usize = 20;
pub const MC_HOST_LAYOUT_VM_STATUS_OUTPUT_RECORDS_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_VM_STATUS_OUTPUT_BYTES_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_VM_STATUS_OUTPUT_DROPPED_BYTES_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_VM_STATUS_INFLIGHT_EGRESS_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_VM_STATUS_PENDING_COMMITS_OFFSET: usize = 52;
pub const MC_HOST_LAYOUT_VM_STATUS_WORKERS_OFFSET: usize = 56;
pub const MC_HOST_LAYOUT_VM_STATUS_RESERVED_OFFSET: usize = 60;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_SIZE: usize = 80;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_TICKS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_STOP_REASON_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_DRIVE_RESULT_STATUS_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_VM_API_SIZE: usize = 104;
pub const MC_HOST_LAYOUT_VM_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_VM_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_VM_API_KERNEL_LOAD_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_VM_API_KERNEL_CLOSE_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_VM_API_VM_CREATE_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_VM_API_VM_RESTORE_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_VM_API_VM_STATUS_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_VM_API_VM_DRIVE_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_VM_API_VM_CLOSE_OFFSET: usize = 56;
pub const MC_HOST_LAYOUT_VM_API_VM_CANCEL_AND_CLOSE_OFFSET: usize = 64;
pub const MC_HOST_LAYOUT_VM_API_TERMINAL_INPUT_OFFSET: usize = 72;
pub const MC_HOST_LAYOUT_VM_API_TERMINAL_RESIZE_OFFSET: usize = 80;
pub const MC_HOST_LAYOUT_VM_API_VM_RESTORE_ATTACHED_OFFSET: usize = 88;
pub const MC_HOST_LAYOUT_VM_API_VM_FORK_OFFSET: usize = 96;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_SIZE: usize = 64;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_KIND_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_CHANNEL_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_SEQUENCE_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_OFFSET_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_LENGTH_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_FIRST_LOST_SEQUENCE_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_LAST_LOST_SEQUENCE_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_OUTPUT_RECORD_BYTE_END_OFFSET: usize = 56;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_SIZE: usize = 48;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_RECORDS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_RETAINED_BYTES_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_DROPPED_BYTES_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_LEASED_RECORDS_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_FLAGS_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_OUTPUT_STATS_RESERVED_OFFSET: usize = 44;
pub const MC_HOST_LAYOUT_OUTPUT_API_SIZE: usize = 48;
pub const MC_HOST_LAYOUT_OUTPUT_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_OUTPUT_API_NEXT_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_OUTPUT_API_READ_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_OUTPUT_API_ACK_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_OUTPUT_API_RELEASE_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_OUTPUT_API_STATS_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_SIZE: usize = 24;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_MODE_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_RESERVED_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_SNAPSHOT_BEGIN_OPTIONS_BASELINE_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_SIZE: usize = 128;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_KIND_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_MCSN_VERSION_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_KERNEL_DIGEST_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_BASELINE_ID_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_CONTENT_DIGEST_OFFSET: usize = 80;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_LENGTH_OFFSET: usize = 112;
pub const MC_HOST_LAYOUT_SNAPSHOT_INFO_MEMORY_LEN_OFFSET: usize = 120;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_SIZE: usize = 32;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_ALIGN: usize = 4;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_INFLIGHT_EGRESS_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_COMMITS_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_OPERATIONS_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_EFFECTS_OFFSET: usize = 20;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_PENDING_TRANSACTIONS_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_SNAPSHOT_READINESS_RESERVED_OFFSET: usize = 28;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_SIZE: usize = 80;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_IMPORT_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_BASELINE_CLOSE_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_BEGIN_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_READ_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_FINISH_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_CANCEL_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_INSPECT_OFFSET: usize = 56;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_READINESS_OFFSET: usize = 64;
pub const MC_HOST_LAYOUT_SNAPSHOT_API_ATTACHMENT_MANIFEST_OFFSET: usize = 72;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_SIZE: usize = 32;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_KIND_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_STATE_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_ERROR_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_RESERVED_OFFSET: usize = 20;
pub const MC_HOST_LAYOUT_OPERATION_STATUS_RESULT_LENGTH_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_OPERATION_API_SIZE: usize = 56;
pub const MC_HOST_LAYOUT_OPERATION_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_OPERATION_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_OPERATION_API_START_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_OPERATION_API_POLL_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_OPERATION_API_RESULT_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_OPERATION_API_PEEK_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_OPERATION_API_CANCEL_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_OPERATION_API_CLOSE_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_SIZE: usize = 40;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_CREATE_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_ADD_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_SEAL_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_ATTACHMENT_API_PLAN_CLOSE_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_EFFECT_INFO_SIZE: usize = 24;
pub const MC_HOST_LAYOUT_EFFECT_INFO_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_EFFECT_INFO_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_EFFECT_INFO_KIND_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_EFFECT_INFO_RESERVED_OFFSET: usize = 12;
pub const MC_HOST_LAYOUT_EFFECT_INFO_LENGTH_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_EFFECT_API_SIZE: usize = 96;
pub const MC_HOST_LAYOUT_EFFECT_API_ALIGN: usize = 8;
pub const MC_HOST_LAYOUT_EFFECT_API_HEADER_OFFSET: usize = 0;
pub const MC_HOST_LAYOUT_EFFECT_API_NEXT_OFFSET: usize = 8;
pub const MC_HOST_LAYOUT_EFFECT_API_READ_OFFSET: usize = 16;
pub const MC_HOST_LAYOUT_EFFECT_API_RELEASE_OFFSET: usize = 24;
pub const MC_HOST_LAYOUT_EFFECT_API_RESPOND_OFFSET: usize = 32;
pub const MC_HOST_LAYOUT_EFFECT_API_ACK_OFFSET: usize = 40;
pub const MC_HOST_LAYOUT_EFFECT_API_RESPONSE_BEGIN_OFFSET: usize = 48;
pub const MC_HOST_LAYOUT_EFFECT_API_RESPONSE_WRITE_OFFSET: usize = 56;
pub const MC_HOST_LAYOUT_EFFECT_API_RESPONSE_FINISH_OFFSET: usize = 64;
pub const MC_HOST_LAYOUT_EFFECT_API_RESPONSE_ABORT_OFFSET: usize = 72;
pub const MC_HOST_LAYOUT_EFFECT_API_SOCKET_PUSH_OFFSET: usize = 80;
pub const MC_HOST_LAYOUT_EFFECT_API_SOCKET_CLOSE_OFFSET: usize = 88;
