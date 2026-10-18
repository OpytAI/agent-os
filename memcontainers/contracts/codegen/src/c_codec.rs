//! Borrowed, allocation-free C views of embedder contract messages.

use super::{Message, MessageField};
use std::fmt::Write;

fn snake(name: &str) -> String {
    let mut out = String::new();
    for (index, ch) in name.chars().enumerate() {
        if ch.is_ascii_uppercase() && index != 0 {
            out.push('_');
        }
        out.push(ch.to_ascii_lowercase());
    }
    out
}

fn scalar(ty: &str) -> Option<&'static str> {
    match ty {
        "u16" => Some("uint16_t"),
        "u32" => Some("uint32_t"),
        "i32" => Some("int32_t"),
        "i64" => Some("int64_t"),
        "bool" => Some("uint8_t"),
        _ => None,
    }
}

fn list_inner(ty: &str) -> Option<&str> {
    ty.strip_prefix("list<")?.strip_suffix('>')
}

fn field_type(field: &MessageField) -> String {
    if let Some(ty) = scalar(&field.ty) {
        ty.to_string()
    } else if field.ty == "strmap" {
        "mc_host_wire_map".to_string()
    } else if list_inner(&field.ty).is_some() {
        "mc_host_wire_list".to_string()
    } else {
        "mc_host_wire_bytes".to_string()
    }
}

pub(super) fn emit(messages: &[Message]) -> String {
    let mut out = String::new();
    out.push_str(r#"

/* Portable message codec. Decoded byte views borrow the input frame. */
#ifndef MC_HOST_WIRE_COMMON
#define MC_HOST_WIRE_COMMON
#include <stddef.h>
#include <string.h>

typedef enum mc_host_wire_result {
    MC_HOST_WIRE_OK = 0,
    MC_HOST_WIRE_INVALID_ARGUMENT = 1,
    MC_HOST_WIRE_BUFFER_TOO_SMALL = 2,
    MC_HOST_WIRE_WRONG_MESSAGE = 3,
    MC_HOST_WIRE_UNSUPPORTED_VERSION = 4,
    MC_HOST_WIRE_TRUNCATED = 5,
    MC_HOST_WIRE_INVALID_UTF8 = 6,
    MC_HOST_WIRE_INVALID_PRESENCE = 7,
    MC_HOST_WIRE_TRAILING_BYTES = 8,
    MC_HOST_WIRE_OVERFLOW = 9,
    MC_HOST_WIRE_NON_CANONICAL_MAP = 10
} mc_host_wire_result;

typedef struct mc_host_wire_bytes {
    const uint8_t *data;
    uint32_t len;
} mc_host_wire_bytes;

/* data contains count consecutive u32-length-prefixed nested frames. */
typedef struct mc_host_wire_list {
    const uint8_t *data;
    size_t len;
    uint32_t count;
} mc_host_wire_list;

typedef mc_host_wire_list mc_host_wire_map;

typedef struct mc_host_wire_reader {
    const uint8_t *data;
    size_t len;
    size_t off;
    mc_host_wire_result error;
} mc_host_wire_reader;

typedef struct mc_host_wire_writer {
    uint8_t *data;
    size_t cap;
    size_t off;
    mc_host_wire_result error;
} mc_host_wire_writer;

static inline int mc_host_wire_take(mc_host_wire_reader *r, size_t n, const uint8_t **p) {
    if (r->error != MC_HOST_WIRE_OK) return 0;
    if (r->off > r->len || n > r->len - r->off) {
        r->error = MC_HOST_WIRE_TRUNCATED;
        return 0;
    }
    *p = r->data + r->off;
    r->off += n;
    return 1;
}

static inline uint8_t mc_host_wire_get_u8(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    return mc_host_wire_take(r, 1, &p) ? p[0] : 0;
}

static inline uint16_t mc_host_wire_get_u16(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    return mc_host_wire_take(r, 2, &p) ? (uint16_t)(p[0] | (uint16_t)p[1] << 8) : 0;
}

static inline uint32_t mc_host_wire_get_u32(mc_host_wire_reader *r) {
    const uint8_t *p = NULL;
    if (!mc_host_wire_take(r, 4, &p)) return 0;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint64_t mc_host_wire_get_u64(mc_host_wire_reader *r) {
    uint64_t lo = mc_host_wire_get_u32(r);
    uint64_t hi = mc_host_wire_get_u32(r);
    return lo | hi << 32;
}

static inline uint8_t mc_host_wire_get_flag(mc_host_wire_reader *r) {
    uint8_t value = mc_host_wire_get_u8(r);
    if (r->error == MC_HOST_WIRE_OK && value > 1) r->error = MC_HOST_WIRE_INVALID_PRESENCE;
    return value;
}

static inline int mc_host_wire_utf8(const uint8_t *s, size_t n) {
    for (size_t i = 0; i < n;) {
        uint8_t a = s[i++];
        if (a < 0x80) continue;
        if (a < 0xC2 || a > 0xF4 || i == n) return 0;
        uint8_t b = s[i++];
        if (b < 0x80 || b > 0xBF) return 0;
        if (a < 0xE0) continue;
        if (i == n) return 0;
        uint8_t c = s[i++];
        if (c < 0x80 || c > 0xBF || (a == 0xE0 && b < 0xA0) ||
            (a == 0xED && b > 0x9F)) return 0;
        if (a < 0xF0) continue;
        if (i == n) return 0;
        uint8_t d = s[i++];
        if (d < 0x80 || d > 0xBF || (a == 0xF0 && b < 0x90) ||
            (a == 0xF4 && b > 0x8F)) return 0;
    }
    return 1;
}

static inline mc_host_wire_bytes mc_host_wire_get_bytes(mc_host_wire_reader *r, int utf8) {
    mc_host_wire_bytes value = {0};
    uint32_t len = mc_host_wire_get_u32(r);
    const uint8_t *p = NULL;
    if (!mc_host_wire_take(r, len, &p)) return value;
    if (utf8 && !mc_host_wire_utf8(p, len)) {
        r->error = MC_HOST_WIRE_INVALID_UTF8;
        return value;
    }
    value.data = p;
    value.len = len;
    return value;
}

static inline int mc_host_wire_less(mc_host_wire_bytes a, mc_host_wire_bytes b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int order = n ? memcmp(a.data, b.data, n) : 0;
    return order < 0 || (order == 0 && a.len < b.len);
}

static inline mc_host_wire_map mc_host_wire_get_map(mc_host_wire_reader *r) {
    mc_host_wire_map map = {0};
    map.count = mc_host_wire_get_u32(r);
    if (r->error != MC_HOST_WIRE_OK) return map;
    map.data = r->data + r->off;
    size_t start = r->off;
    mc_host_wire_bytes previous = {0};
    for (uint32_t i = 0; i < map.count && r->error == MC_HOST_WIRE_OK; ++i) {
        mc_host_wire_bytes key = mc_host_wire_get_bytes(r, 1);
        if (r->error == MC_HOST_WIRE_OK && i && !mc_host_wire_less(previous, key))
            r->error = MC_HOST_WIRE_NON_CANONICAL_MAP;
        mc_host_wire_get_bytes(r, 1);
        previous = key;
    }
    map.len = r->off - start;
    return map;
}

static inline void mc_host_wire_put(mc_host_wire_writer *w, const uint8_t *p, size_t n) {
    if (w->error != MC_HOST_WIRE_OK) return;
    if (n > SIZE_MAX - w->off) {
        w->error = MC_HOST_WIRE_OVERFLOW;
        return;
    }
    if (n && !p) {
        w->error = MC_HOST_WIRE_INVALID_ARGUMENT;
        return;
    }
    if (w->data && n) {
        if (w->off > w->cap || n > w->cap - w->off) {
            w->error = MC_HOST_WIRE_BUFFER_TOO_SMALL;
            return;
        }
        memcpy(w->data + w->off, p, n);
    }
    w->off += n;
}

static inline void mc_host_wire_put_u8(mc_host_wire_writer *w, uint8_t value) {
    mc_host_wire_put(w, &value, 1);
}

static inline void mc_host_wire_put_u16(mc_host_wire_writer *w, uint16_t value) {
    uint8_t p[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    mc_host_wire_put(w, p, 2);
}

static inline void mc_host_wire_put_u32(mc_host_wire_writer *w, uint32_t value) {
    uint8_t p[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                    (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    mc_host_wire_put(w, p, 4);
}

static inline void mc_host_wire_put_u64(mc_host_wire_writer *w, uint64_t value) {
    mc_host_wire_put_u32(w, (uint32_t)value);
    mc_host_wire_put_u32(w, (uint32_t)(value >> 32));
}

static inline void mc_host_wire_put_bytes(mc_host_wire_writer *w, mc_host_wire_bytes value, int utf8) {
    if (value.len && !value.data) { w->error = MC_HOST_WIRE_INVALID_ARGUMENT; return; }
    if (utf8 && !mc_host_wire_utf8(value.data, value.len)) {
        w->error = MC_HOST_WIRE_INVALID_UTF8;
        return;
    }
    mc_host_wire_put_u32(w, value.len);
    mc_host_wire_put(w, value.data, value.len);
}

/* Iterate a decoded list without allocating. Returns 1 for an item, 0 at end, -1 on corruption. */
static inline int mc_host_wire_list_next(mc_host_wire_list list, size_t *offset,
                                         mc_host_wire_bytes *item) {
    if (!offset || !item || *offset > list.len) return -1;
    if (*offset == list.len) return 0;
    if (list.len - *offset < 4) return -1;
    mc_host_wire_reader r = {list.data, list.len, *offset, MC_HOST_WIRE_OK};
    *item = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return -1;
    *offset = r.off;
    return 1;
}

static inline mc_host_wire_result mc_host_wire_list_append(mc_host_wire_list *list,
        uint8_t *buffer, size_t capacity, mc_host_wire_bytes item) {
    if (!list || !buffer || (item.len && !item.data) ||
        (list->data && list->data != buffer) || list->count == UINT32_MAX)
        return MC_HOST_WIRE_INVALID_ARGUMENT;
    if (item.len) {
        uintptr_t start = (uintptr_t)item.data;
        uintptr_t destination = (uintptr_t)buffer;
        if ((start >= destination && start - destination < capacity) ||
            (start < destination && item.len > destination - start))
            return MC_HOST_WIRE_INVALID_ARGUMENT;
    }
    if (list->len > capacity || capacity - list->len < 4 ||
        item.len > capacity - list->len - 4)
        return MC_HOST_WIRE_BUFFER_TOO_SMALL;
    mc_host_wire_writer writer = {buffer, capacity, list->len, MC_HOST_WIRE_OK};
    mc_host_wire_put_bytes(&writer, item, 0);
    if (writer.error != MC_HOST_WIRE_OK) return writer.error;
    list->data = buffer;
    list->len = writer.off;
    ++list->count;
    return MC_HOST_WIRE_OK;
}
#endif /* MC_HOST_WIRE_COMMON */
"#);

    for message in messages {
        emit_message(&mut out, message);
    }
    out
}

fn emit_message(out: &mut String, message: &Message) {
    let name = snake(&message.name);
    let ctype = format!("mc_host_wire_{name}");
    writeln!(out, "\ntypedef struct {ctype} {{").unwrap();
    for field in &message.fields {
        if field.optional {
            writeln!(out, "    uint8_t has_{};", field.name).unwrap();
        }
        writeln!(out, "    {} {};", field_type(field), field.name).unwrap();
    }
    writeln!(out, "}} {ctype};").unwrap();

    writeln!(out, "static inline mc_host_wire_result mc_host_wire_decode_{name}(const uint8_t *data, size_t len, {ctype} *out) {{").unwrap();
    out.push_str("    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;\n");
    out.push_str("    memset(out, 0, sizeof(*out));\n");
    out.push_str("    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};\n");
    writeln!(out, "    if (mc_host_wire_get_u16(&r) != {} && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;", message.id).unwrap();
    writeln!(out, "    if (mc_host_wire_get_u8(&r) != {} && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;", message.version).unwrap();
    for field in &message.fields {
        if field.optional {
            writeln!(
                out,
                "    out->has_{} = mc_host_wire_get_flag(&r);",
                field.name
            )
            .unwrap();
            writeln!(out, "    if (out->has_{}) {{", field.name).unwrap();
        }
        emit_decode_field(out, field, "out->");
        if field.optional {
            out.push_str("    }\n");
        }
    }
    out.push_str("    if (r.error != MC_HOST_WIRE_OK) return r.error;\n");
    out.push_str("    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;\n}\n");

    writeln!(out, "static inline mc_host_wire_result mc_host_wire_encode_{name}(const {ctype} *value, uint8_t *data, size_t capacity, size_t *required) {{").unwrap();
    out.push_str("    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;\n");
    writeln!(out, "    for (int pass = 0; pass < 2; ++pass) {{").unwrap();
    out.push_str(
        "        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};\n",
    );
    writeln!(out, "        mc_host_wire_put_u16(&w, {});", message.id).unwrap();
    writeln!(out, "        mc_host_wire_put_u8(&w, {});", message.version).unwrap();
    for field in &message.fields {
        if field.optional {
            writeln!(
                out,
                "        if (value->has_{} > 1) return MC_HOST_WIRE_INVALID_PRESENCE;",
                field.name
            )
            .unwrap();
            writeln!(
                out,
                "        mc_host_wire_put_u8(&w, value->has_{});",
                field.name
            )
            .unwrap();
            writeln!(out, "        if (value->has_{}) {{", field.name).unwrap();
        }
        emit_encode_field(out, field);
        if field.optional {
            out.push_str("        }\n");
        }
    }
    out.push_str("        if (w.error != MC_HOST_WIRE_OK) return w.error;\n");
    out.push_str("        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }\n");
    out.push_str("    }\n    return MC_HOST_WIRE_OK;\n}\n");
}

fn emit_decode_field(out: &mut String, field: &MessageField, prefix: &str) {
    let dest = format!("{prefix}{}", field.name);
    if let Some(inner) = list_inner(&field.ty) {
        let inner_name = snake(inner);
        writeln!(out, "        {dest}.count = mc_host_wire_get_u32(&r);").unwrap();
        writeln!(out, "        {dest}.data = r.data + r.off;").unwrap();
        out.push_str("        size_t list_start = r.off;\n");
        writeln!(
            out,
            "        for (uint32_t i = 0; i < {dest}.count && r.error == MC_HOST_WIRE_OK; ++i) {{"
        )
        .unwrap();
        out.push_str("            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);\n");
        writeln!(out, "            mc_host_wire_{inner_name} nested;").unwrap();
        writeln!(out, "            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_{inner_name}(item.data, item.len, &nested);").unwrap();
        out.push_str("        }\n");
        writeln!(out, "        {dest}.len = r.off - list_start;").unwrap();
        return;
    }
    let expr = match field.ty.as_str() {
        "u16" => "mc_host_wire_get_u16(&r)",
        "u32" | "i32" => "mc_host_wire_get_u32(&r)",
        "i64" => "mc_host_wire_get_u64(&r)",
        "bool" => "mc_host_wire_get_flag(&r)",
        "str" => "mc_host_wire_get_bytes(&r, 1)",
        "bytes" => "mc_host_wire_get_bytes(&r, 0)",
        "strmap" => "mc_host_wire_get_map(&r)",
        _ => panic!("unsupported embedder C codec field {}", field.ty),
    };
    writeln!(out, "        {dest} = {expr};").unwrap();
}

fn emit_encode_field(out: &mut String, field: &MessageField) {
    let value = format!("value->{}", field.name);
    if let Some(inner) = list_inner(&field.ty) {
        let inner_name = snake(inner);
        writeln!(out, "            mc_host_wire_reader list_reader = {{{value}.data, {value}.len, 0, MC_HOST_WIRE_OK}};").unwrap();
        writeln!(
            out,
            "            for (uint32_t i = 0; i < {value}.count; ++i) {{"
        )
        .unwrap();
        out.push_str(
            "                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);\n",
        );
        writeln!(out, "                mc_host_wire_{inner_name} nested;").unwrap();
        writeln!(out, "                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_{inner_name}(item.data, item.len, &nested);").unwrap();
        out.push_str("            }\n");
        writeln!(out, "            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != {value}.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;").unwrap();
        writeln!(out, "            mc_host_wire_put_u32(&w, {value}.count);").unwrap();
        writeln!(
            out,
            "            mc_host_wire_put(&w, {value}.data, {value}.len);"
        )
        .unwrap();
        return;
    }
    let call = match field.ty.as_str() {
        "u16" => format!("mc_host_wire_put_u16(&w, {value})"),
        "u32" | "i32" => format!("mc_host_wire_put_u32(&w, (uint32_t){value})"),
        "i64" => format!("mc_host_wire_put_u64(&w, (uint64_t){value})"),
        "bool" => {
            writeln!(
                out,
                "            if ({value} > 1) return MC_HOST_WIRE_INVALID_PRESENCE;"
            )
            .unwrap();
            format!("mc_host_wire_put_u8(&w, {value})")
        }
        "str" => format!("mc_host_wire_put_bytes(&w, {value}, 1)"),
        "bytes" => format!("mc_host_wire_put_bytes(&w, {value}, 0)"),
        "strmap" => {
            writeln!(out, "            mc_host_wire_reader map_reader = {{{value}.data, {value}.len, 0, MC_HOST_WIRE_OK}};").unwrap();
            out.push_str("            mc_host_wire_bytes previous_key = {0};\n");
            writeln!(out, "            for (uint32_t i = 0; i < {value}.count && map_reader.error == MC_HOST_WIRE_OK; ++i) {{").unwrap();
            out.push_str("                mc_host_wire_bytes key = mc_host_wire_get_bytes(&map_reader, 1);\n");
            out.push_str("                if (map_reader.error == MC_HOST_WIRE_OK && i && !mc_host_wire_less(previous_key, key)) map_reader.error = MC_HOST_WIRE_NON_CANONICAL_MAP;\n");
            out.push_str("                mc_host_wire_get_bytes(&map_reader, 1);\n                previous_key = key;\n            }\n");
            writeln!(out, "            if (map_reader.error != MC_HOST_WIRE_OK || map_reader.off != {value}.len) return map_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : map_reader.error;").unwrap();
            writeln!(out, "            mc_host_wire_put_u32(&w, {value}.count);").unwrap();
            format!("mc_host_wire_put(&w, {value}.data, {value}.len)")
        }
        _ => panic!("unsupported embedder C codec field {}", field.ty),
    };
    writeln!(out, "            {call};").unwrap();
}
