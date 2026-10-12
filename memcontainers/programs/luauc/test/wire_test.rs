#[path = "../src/wire.rs"]
mod wire;

use wire::{encode, parse_frame, sort_modules, Module, Request};

fn u16_at(bytes: &[u8], offset: usize) -> u16 {
    u16::from_le_bytes(bytes[offset..offset + 2].try_into().unwrap())
}

fn u32_at(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
}

fn u64_at(bytes: &[u8], offset: usize) -> u64 {
    u64::from_le_bytes(bytes[offset..offset + 8].try_into().unwrap())
}

#[test]
fn compile_request_uses_a_56_byte_header() {
    let modules = [Module {
        name: b"main",
        source: b"return 1\n",
    }];
    let bytes = encode(&Request {
        tier: 0,
        budget_mask: 0,
        mem: 0,
        fuel: 0,
        table: 0,
        entry: b"main",
        modules: &modules,
    });
    assert_eq!(&bytes[..8], b"AOLC1\0\0\0");
    assert_eq!(u16_at(&bytes, 8), 1);
    assert_eq!(u16_at(&bytes, 10), 56);
    assert_eq!(u32_at(&bytes, 12) as usize, bytes.len());
    assert_eq!(u32_at(&bytes, 16), 1);
    assert_eq!(u16_at(&bytes, 20), 0);
    assert_eq!(u16_at(&bytes, 22), 0);
    assert_eq!(u64_at(&bytes, 32), 0);
    assert_eq!(u64_at(&bytes, 40), 0);
    assert_eq!(u64_at(&bytes, 48), 0);
}

#[test]
fn explicit_budget_bits_are_stored() {
    let modules = [Module {
        name: b"main",
        source: b"return 1\n",
    }];
    let bytes = encode(&Request {
        tier: 3,
        budget_mask: 0b111,
        mem: 4096,
        fuel: 1000,
        table: 8,
        entry: b"main",
        modules: &modules,
    });
    assert_eq!(u16_at(&bytes, 20), 3);
    assert_eq!(u16_at(&bytes, 22), 0b111);
    assert_eq!(u64_at(&bytes, 32), 4096);
    assert_eq!(u64_at(&bytes, 40), 1000);
    assert_eq!(u64_at(&bytes, 48), 8);
}

#[test]
fn module_names_sort_by_raw_bytes() {
    let mut modules = [
        Module {
            name: b"b",
            source: b"return 2\n",
        },
        Module {
            name: b"a",
            source: b"return 1\n",
        },
    ];
    sort_modules(&mut modules);
    assert_eq!(modules[0].name, b"a");
    assert_eq!(modules[1].name, b"b");
}

#[test]
fn response_frame_rejects_trailing_bytes_and_long_diagnostics() {
    let mut ok = Vec::new();
    ok.extend_from_slice(&0u32.to_le_bytes());
    ok.extend_from_slice(&0u32.to_le_bytes());
    ok.extend_from_slice(&0u32.to_le_bytes());
    let frame = parse_frame(&ok).unwrap();
    assert_eq!(frame.status, 0);
    assert_eq!(frame.diagnostic, b"");
    assert_eq!(frame.artifact, b"");

    ok.push(1);
    assert!(parse_frame(&ok).is_err());

    let mut long = Vec::new();
    long.extend_from_slice(&0u32.to_le_bytes());
    long.extend_from_slice(&4097u32.to_le_bytes());
    long.extend(std::iter::repeat(b'x').take(4097));
    long.extend_from_slice(&0u32.to_le_bytes());
    assert!(parse_frame(&long).is_err());
}
