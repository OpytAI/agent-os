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
pub const MC_HOST_TICK_WAITING: u32 = 0;
pub const MC_HOST_TICK_RUNNABLE: u32 = 1;
pub const MC_HOST_TICK_EXITED: u32 = 2;
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

// Native embedder ABI constants.
pub const MC_HOST_ABI_MAJOR: u16 = 1;
pub const MC_HOST_ABI_MINOR: u16 = 0;
