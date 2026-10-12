//! `/bin/luau-compile`. The guest reads Luau sources, asks the host to compile
//! and stamp them, and writes the returned program.

#![no_std]
#![no_main]

extern crate alloc;

use alloc::vec;
use alloc::vec::Vec;

use sysroot as rt;

#[global_allocator]
static ALLOCATOR: talc::wasm::WasmDynamicTalc = talc::wasm::new_wasm_dynamic_allocator();

mod wire;
use wire::{encode, parse_frame, sort_modules, Module, Request};

rt::entry!(main);

const MAX_ARGV: usize = 64 * 1024;
const MAX_ARGS: usize = 300;
const USAGE: &str = "\
usage: luau-compile -o <out> [--tier <name>] [--mem <n>] [--fuel <n>] [--table <n>] [--entry <name>] <file>...\n";

fn main() {
    rt::exit(run());
}

fn run() -> i32 {
    let mut abuf = vec![0u8; MAX_ARGV];
    let an = rt::args_into(&mut abuf);
    if an >= MAX_ARGV {
        rt::eprint("luau-compile: argument bytes exceed 65535-byte limit\n");
        return 2;
    }
    let mut args: [&[u8]; MAX_ARGS] = [&[]; MAX_ARGS];
    let mut argc = 0usize;
    let mut too_many = false;
    for part in abuf[..an].split(|&byte| byte == 0) {
        if part.is_empty() {
            continue;
        }
        if argc >= MAX_ARGS {
            too_many = true;
            break;
        }
        args[argc] = part;
        argc += 1;
    }
    if too_many || argc == 0 {
        rt::eprint(USAGE);
        return 2;
    }
    if argc == 2 && is_help(args[1]) {
        rt::print(USAGE);
        return 0;
    }
    for arg in args.iter().take(argc).skip(1) {
        if is_help(arg) {
            rt::print(USAGE);
            return 0;
        }
    }

    let mut out: Option<&[u8]> = None;
    let mut tier: u16 = 0;
    let mut mem: u64 = 0;
    let mut fuel: u64 = 0;
    let mut table: u64 = 0;
    let mut mask: u16 = 0;
    let mut entry: Option<&[u8]> = None;
    let mut files: Vec<&[u8]> = Vec::new();
    let mut index = 1usize;
    while index < argc {
        let arg = args[index];
        if arg == b"-o" || arg == b"--tier" || arg == b"--mem" || arg == b"--fuel" || arg == b"--table" || arg == b"--entry"
        {
            if index + 1 >= argc {
                rt::eprint(USAGE);
                return 2;
            }
            let value = args[index + 1];
            index += 2;
            if arg == b"-o" {
                if out.is_some() {
                    rt::eprint(USAGE);
                    return 2;
                }
                out = Some(value);
            } else if arg == b"--tier" {
                if tier != 0 {
                    rt::eprint(USAGE);
                    return 2;
                }
                match tier_code(value) {
                    Some(code) => tier = code,
                    None => {
                        rt::eprint(USAGE);
                        return 2;
                    }
                }
            } else if arg == b"--entry" {
                if entry.is_some() {
                    rt::eprint(USAGE);
                    return 2;
                }
                entry = Some(value);
            } else {
                let bit = if arg == b"--mem" {
                    1u16
                } else if arg == b"--fuel" {
                    2
                } else {
                    4
                };
                if mask & bit != 0 {
                    rt::eprint(USAGE);
                    return 2;
                }
                match parse_u64(value) {
                    Some(number) => {
                        mask |= bit;
                        if bit == 1 {
                            mem = number;
                        } else if bit == 2 {
                            fuel = number;
                        } else {
                            table = number;
                        }
                    }
                    None => {
                        rt::eprint(USAGE);
                        return 2;
                    }
                }
            }
        } else if arg.first() == Some(&b'-') {
            rt::eprint(USAGE);
            return 2;
        } else {
            files.push(arg);
            index += 1;
        }
    }

    let Some(out) = out else {
        rt::eprint(USAGE);
        return 2;
    };
    if files.is_empty() {
        rt::eprint(USAGE);
        return 2;
    }

    let mut owned = Vec::new();
    for path in files {
        let name = match module_name(path) {
            Some(name) => name.to_vec(),
            None => {
                rt::eprint("luau-compile: invalid module name ");
                rt::eprint_bytes(path);
                rt::eprint("\n");
                return 2;
            }
        };
        let source = match read_file(path) {
            Ok(source) => source,
            Err(()) => return cannot_read(path),
        };
        owned.push(OwnedModule { name, source });
    }
    for index in 1..owned.len() {
        if owned[..index].iter().any(|module| module.name == owned[index].name) {
            rt::eprint("luau-compile: duplicate module ");
            rt::eprint_bytes(&owned[index].name);
            rt::eprint("\n");
            return 2;
        }
    }
    let entry_owned: Vec<u8> = match entry {
        Some(name) => name.to_vec(),
        None => owned[0].name.clone(),
    };
    if !owned.iter().any(|module| module.name == entry_owned) {
        rt::eprint("luau-compile: invalid entry\n");
        return 2;
    }

    let mut modules: Vec<Module<'_>> = owned
        .iter()
        .map(|module| Module {
            name: module.name.as_slice(),
            source: module.source.as_slice(),
        })
        .collect();
    sort_modules(&mut modules);
    let request = Request {
        tier,
        budget_mask: mask,
        mem,
        fuel,
        table,
        entry: entry_owned.as_slice(),
        modules: modules.as_slice(),
    };
    let body = encode(&request);
    let frame = match compiler_call(&body) {
        Ok(frame) => frame,
        Err(CallError::Detached) => {
            rt::eprint("luau-compile: compiler is not attached\n");
            return 1;
        }
        Err(CallError::Bad) => {
            rt::eprint("luau-compile: bad compiler response\n");
            return 1;
        }
    };
    if frame.status != 0 {
        rt::eprint_bytes(&frame.diagnostic);
        if frame.diagnostic.last() != Some(&b'\n') {
            rt::eprint("\n");
        }
        return 1;
    }
    if write_file(out, &frame.artifact).is_err() {
        rt::eprint("luau-compile: cannot write ");
        rt::eprint_bytes(out);
        rt::eprint("\n");
        return 1;
    }
    0
}

struct OwnedModule {
    name: Vec<u8>,
    source: Vec<u8>,
}

enum CallError {
    Detached,
    Bad,
}

fn compiler_call(body: &[u8]) -> Result<wire::Frame, CallError> {
    let mut request = Vec::with_capacity(9 + body.len());
    request.extend_from_slice(b"compiler\0");
    request.extend_from_slice(body);
    let fd = match rt::host_call(&request) {
        Ok(fd) => fd,
        Err(_) => return Err(CallError::Detached),
    };
    let mut response = Vec::new();
    let mut buf = [0u8; 8192];
    let limit = wire::MAX_ARTIFACT + wire::MAX_DIAGNOSTIC + 12;
    loop {
        match rt::read(fd, &mut buf) {
            Ok(0) => break,
            Ok(n) => {
                if response.len() + n > limit {
                    rt::close(fd);
                    return Err(CallError::Bad);
                }
                response.extend_from_slice(&buf[..n]);
            }
            Err(_) => {
                rt::close(fd);
                return Err(CallError::Bad);
            }
        }
    }
    rt::close(fd);
    parse_frame(&response).map_err(|_| CallError::Bad)
}

fn is_help(arg: &[u8]) -> bool {
    arg == b"help" || arg == b"--help" || arg == b"-h"
}

fn module_name(path: &[u8]) -> Option<&[u8]> {
    let file = match path.iter().rposition(|&byte| byte == b'/') {
        Some(index) => &path[index + 1..],
        None => path,
    };
    let stem = file.strip_suffix(b".luau").unwrap_or(file);
    if valid_module_name(stem) { Some(stem) } else { None }
}

fn valid_module_name(name: &[u8]) -> bool {
    if name.is_empty() || name[0] == b'/' || name[name.len() - 1] == b'/' {
        return false;
    }
    let mut previous = 0u8;
    for &byte in name {
        let ok = byte.is_ascii_lowercase() || byte.is_ascii_digit() || byte == b'_' || byte == b'.' || byte == b'/' || byte == b'-';
        if !ok {
            return false;
        }
        if previous == b'.' && byte == b'.' {
            return false;
        }
        previous = byte;
    }
    true
}

fn read_file(path: &[u8]) -> Result<Vec<u8>, ()> {
    let path = core::str::from_utf8(path).map_err(|_| ())?;
    let fd = rt::open(path, rt::O_READ).map_err(|_| ())?;
    let mut out = Vec::new();
    let mut buf = [0u8; 8192];
    loop {
        match rt::read(fd, &mut buf) {
            Ok(0) => break,
            Ok(n) => out.extend_from_slice(&buf[..n]),
            Err(_) => {
                rt::close(fd);
                return Err(());
            }
        }
    }
    rt::close(fd);
    Ok(out)
}

fn write_file(path: &[u8], bytes: &[u8]) -> Result<(), ()> {
    let path = core::str::from_utf8(path).map_err(|_| ())?;
    let fd = rt::open(path, rt::O_WRITE | rt::O_CREATE | rt::O_TRUNC).map_err(|_| ())?;
    let result = rt::write_all(fd, bytes);
    rt::close(fd);
    result.map_err(|_| ())?;
    // Program lookup runs only an owner-executable file.
    rt::chmod(path, 0o755).map_err(|_| ())
}

fn cannot_read(path: &[u8]) -> i32 {
    rt::eprint("luau-compile: cannot read ");
    rt::eprint_bytes(path);
    rt::eprint("\n");
    1
}

fn tier_code(name: &[u8]) -> Option<u16> {
    match name {
        b"full" => Some(1),
        b"read-write" => Some(2),
        b"read-only" => Some(3),
        b"isolated" => Some(4),
        _ => None,
    }
}

fn parse_u64(bytes: &[u8]) -> Option<u64> {
    if bytes.is_empty() {
        return None;
    }
    let mut value = 0u64;
    for &byte in bytes {
        if !byte.is_ascii_digit() {
            return None;
        }
        value = value.checked_mul(10)?.checked_add((byte - b'0') as u64)?;
    }
    if value == 0 { None } else { Some(value) }
}
