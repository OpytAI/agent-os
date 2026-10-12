//! Guest compile request (`AOLC1`) and the compiler response frame.
//!
//! One request. The host stamps `mc_tier` and `mc_budget` and returns the bytes.
//! Tier 0 and a clear budget bit ask the host for its default.

extern crate alloc;

use alloc::vec::Vec;

pub const MAX_DIAGNOSTIC: usize = 4096;
pub const MAX_ARTIFACT: usize = 16 * 1024 * 1024;
pub const HEADER_SIZE: usize = 56;

#[derive(Clone, Copy)]
pub struct Module<'a> {
    pub name: &'a [u8],
    pub source: &'a [u8],
}

pub struct Request<'a> {
    pub tier: u16,
    pub budget_mask: u16,
    pub mem: u64,
    pub fuel: u64,
    pub table: u64,
    pub entry: &'a [u8],
    pub modules: &'a [Module<'a>],
}

pub struct Frame {
    pub status: u32,
    pub diagnostic: Vec<u8>,
    pub artifact: Vec<u8>,
}

pub fn sort_modules(modules: &mut [Module<'_>]) {
    modules.sort_by(|left, right| left.name.cmp(right.name));
}

pub fn encode(request: &Request<'_>) -> Vec<u8> {
    let mut payload = Vec::new();
    push_counted(&mut payload, request.entry);
    for module in request.modules {
        push_counted(&mut payload, module.name);
        push_counted(&mut payload, module.source);
    }
    let total = HEADER_SIZE as u32 + payload.len() as u32;
    let mut out = Vec::with_capacity(total as usize);
    out.extend_from_slice(b"AOLC1\0\0\0");
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&(HEADER_SIZE as u16).to_le_bytes());
    out.extend_from_slice(&total.to_le_bytes());
    out.extend_from_slice(&(request.modules.len() as u32).to_le_bytes());
    out.extend_from_slice(&request.tier.to_le_bytes());
    out.extend_from_slice(&request.budget_mask.to_le_bytes());
    out.extend_from_slice(&0u32.to_le_bytes());
    out.extend_from_slice(&0u32.to_le_bytes());
    out.extend_from_slice(&request.mem.to_le_bytes());
    out.extend_from_slice(&request.fuel.to_le_bytes());
    out.extend_from_slice(&request.table.to_le_bytes());
    out.extend_from_slice(&payload);
    out
}

pub fn parse_frame(bytes: &[u8]) -> Result<Frame, ()> {
    if bytes.len() < 12 {
        return Err(());
    }
    let status = u32_at(bytes, 0)?;
    let diagnostic_len = u32_at(bytes, 4)? as usize;
    if diagnostic_len > MAX_DIAGNOSTIC {
        return Err(());
    }
    let artifact_len_at = 8 + diagnostic_len;
    if bytes.len() < artifact_len_at + 4 {
        return Err(());
    }
    let diagnostic = bytes[8..artifact_len_at].to_vec();
    let artifact_len = u32_at(bytes, artifact_len_at)? as usize;
    if artifact_len > MAX_ARTIFACT {
        return Err(());
    }
    let artifact_at = artifact_len_at + 4;
    let artifact_end = artifact_at + artifact_len;
    if bytes.len() != artifact_end {
        return Err(());
    }
    Ok(Frame {
        status,
        diagnostic,
        artifact: bytes[artifact_at..artifact_end].to_vec(),
    })
}

fn push_counted(out: &mut Vec<u8>, bytes: &[u8]) {
    out.extend_from_slice(&(bytes.len() as u32).to_le_bytes());
    out.extend_from_slice(bytes);
}

fn u32_at(bytes: &[u8], offset: usize) -> Result<u32, ()> {
    let end = offset + 4;
    if end > bytes.len() {
        return Err(());
    }
    let mut raw = [0u8; 4];
    raw.copy_from_slice(&bytes[offset..end]);
    Ok(u32::from_le_bytes(raw))
}
