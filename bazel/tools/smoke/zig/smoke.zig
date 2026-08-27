//! Toolchain smoke guest — proves the wasm32-freestanding Zig toolchain links a
//! `.wasm` through the same transition real guests use. Not a product binary.

export fn add(a: i32, b: i32) i32 {
    return a +% b;
}
