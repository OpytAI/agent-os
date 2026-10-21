// @generated from contracts/control.kdl by //contracts/codegen:projector — do not edit.
#ifndef MC_CONTROL_CONTRACT_H
#define MC_CONTROL_CONTRACT_H

#include <stdint.h>


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
    MC_HOST_WIRE_NON_CANONICAL_MAP = 10,
    MC_HOST_WIRE_TOO_LARGE = 11
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

typedef struct mc_host_wire_exec_arg {
    mc_host_wire_bytes value;
} mc_host_wire_exec_arg;
static inline mc_host_wire_result mc_host_wire_decode_exec_arg(const uint8_t *data, size_t len, mc_host_wire_exec_arg *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 12 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->value = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_exec_arg(const mc_host_wire_exec_arg *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 12);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->value, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_exec_request {
    int32_t mode;
    uint8_t has_command;
    mc_host_wire_bytes command;
    mc_host_wire_list argv;
    uint8_t has_cwd;
    mc_host_wire_bytes cwd;
    mc_host_wire_map env;
    uint8_t has_stdin;
    mc_host_wire_bytes stdin;
} mc_host_wire_exec_request;
static inline mc_host_wire_result mc_host_wire_decode_exec_request(const uint8_t *data, size_t len, mc_host_wire_exec_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 2 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->mode = mc_host_wire_get_u32(&r);
    out->has_command = mc_host_wire_get_flag(&r);
    if (out->has_command) {
        out->command = mc_host_wire_get_bytes(&r, 1);
    }
        {
        out->argv.count = mc_host_wire_get_u32(&r);
        out->argv.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->argv.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_exec_arg nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_exec_arg(item.data, item.len, &nested);
        }
        out->argv.len = r.off - list_start;
        }
    out->has_cwd = mc_host_wire_get_flag(&r);
    if (out->has_cwd) {
        out->cwd = mc_host_wire_get_bytes(&r, 1);
    }
        out->env = mc_host_wire_get_map(&r);
    out->has_stdin = mc_host_wire_get_flag(&r);
    if (out->has_stdin) {
        out->stdin = mc_host_wire_get_bytes(&r, 0);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_exec_request(const mc_host_wire_exec_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 1);
        mc_host_wire_put_u8(&w, 2);
            mc_host_wire_put_u32(&w, (uint32_t)value->mode);
        if (value->has_command > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_command);
        if (value->has_command) {
            mc_host_wire_put_bytes(&w, value->command, 1);
        }
            {
            mc_host_wire_reader list_reader = {value->argv.data, value->argv.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->argv.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_exec_arg nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_exec_arg(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->argv.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->argv.count);
            mc_host_wire_put(&w, value->argv.data, value->argv.len);
            }
        if (value->has_cwd > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_cwd);
        if (value->has_cwd) {
            mc_host_wire_put_bytes(&w, value->cwd, 1);
        }
            mc_host_wire_reader map_reader = {value->env.data, value->env.len, 0, MC_HOST_WIRE_OK};
            mc_host_wire_bytes previous_key = {0};
            for (uint32_t i = 0; i < value->env.count && map_reader.error == MC_HOST_WIRE_OK; ++i) {
                mc_host_wire_bytes key = mc_host_wire_get_bytes(&map_reader, 1);
                if (map_reader.error == MC_HOST_WIRE_OK && i && !mc_host_wire_less(previous_key, key)) map_reader.error = MC_HOST_WIRE_NON_CANONICAL_MAP;
                mc_host_wire_get_bytes(&map_reader, 1);
                previous_key = key;
            }
            if (map_reader.error != MC_HOST_WIRE_OK || map_reader.off != value->env.len) return map_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : map_reader.error;
            mc_host_wire_put_u32(&w, value->env.count);
            mc_host_wire_put(&w, value->env.data, value->env.len);
        if (value->has_stdin > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_stdin);
        if (value->has_stdin) {
            mc_host_wire_put_bytes(&w, value->stdin, 0);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_exec_outcome {
    int32_t exit_code;
    mc_host_wire_bytes stdout;
    mc_host_wire_bytes stderr;
} mc_host_wire_exec_outcome;
static inline mc_host_wire_result mc_host_wire_decode_exec_outcome(const uint8_t *data, size_t len, mc_host_wire_exec_outcome *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 2 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->exit_code = mc_host_wire_get_u32(&r);
        out->stdout = mc_host_wire_get_bytes(&r, 0);
        out->stderr = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_exec_outcome(const mc_host_wire_exec_outcome *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 2);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->exit_code);
            mc_host_wire_put_bytes(&w, value->stdout, 0);
            mc_host_wire_put_bytes(&w, value->stderr, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_file_stat {
    int64_t size;
    uint8_t is_dir;
    uint8_t is_symlink;
    uint32_t nlink;
    uint32_t mode;
} mc_host_wire_file_stat;
static inline mc_host_wire_result mc_host_wire_decode_file_stat(const uint8_t *data, size_t len, mc_host_wire_file_stat *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 3 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->size = mc_host_wire_get_u64(&r);
        out->is_dir = mc_host_wire_get_flag(&r);
        out->is_symlink = mc_host_wire_get_flag(&r);
        out->nlink = mc_host_wire_get_u32(&r);
        out->mode = mc_host_wire_get_u32(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_file_stat(const mc_host_wire_file_stat *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 3);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u64(&w, (uint64_t)value->size);
            if (value->is_dir > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->is_dir);
            if (value->is_symlink > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->is_symlink);
            mc_host_wire_put_u32(&w, (uint32_t)value->nlink);
            mc_host_wire_put_u32(&w, (uint32_t)value->mode);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_dir_entry {
    mc_host_wire_bytes name;
    uint8_t is_dir;
    uint8_t is_symlink;
} mc_host_wire_dir_entry;
static inline mc_host_wire_result mc_host_wire_decode_dir_entry(const uint8_t *data, size_t len, mc_host_wire_dir_entry *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 4 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->name = mc_host_wire_get_bytes(&r, 1);
        out->is_dir = mc_host_wire_get_flag(&r);
        out->is_symlink = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_dir_entry(const mc_host_wire_dir_entry *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 4);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->name, 1);
            if (value->is_dir > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->is_dir);
            if (value->is_symlink > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->is_symlink);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_dir_entries {
    mc_host_wire_list entries;
} mc_host_wire_dir_entries;
static inline mc_host_wire_result mc_host_wire_decode_dir_entries(const uint8_t *data, size_t len, mc_host_wire_dir_entries *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 5 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        {
        out->entries.count = mc_host_wire_get_u32(&r);
        out->entries.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->entries.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_dir_entry nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_dir_entry(item.data, item.len, &nested);
        }
        out->entries.len = r.off - list_start;
        }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_dir_entries(const mc_host_wire_dir_entries *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 5);
        mc_host_wire_put_u8(&w, 1);
            {
            mc_host_wire_reader list_reader = {value->entries.data, value->entries.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->entries.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_dir_entry nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_dir_entry(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->entries.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->entries.count);
            mc_host_wire_put(&w, value->entries.data, value->entries.len);
            }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_svc_request {
    mc_host_wire_bytes service;
    mc_host_wire_bytes request;
} mc_host_wire_svc_request;
static inline mc_host_wire_result mc_host_wire_decode_svc_request(const uint8_t *data, size_t len, mc_host_wire_svc_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 6 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->service = mc_host_wire_get_bytes(&r, 1);
        out->request = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_svc_request(const mc_host_wire_svc_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 6);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->service, 1);
            mc_host_wire_put_bytes(&w, value->request, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_svc_response {
    int32_t status;
    mc_host_wire_bytes body;
} mc_host_wire_svc_response;
static inline mc_host_wire_result mc_host_wire_decode_svc_response(const uint8_t *data, size_t len, mc_host_wire_svc_response *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 7 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->status = mc_host_wire_get_u32(&r);
        out->body = mc_host_wire_get_bytes(&r, 0);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_svc_response(const mc_host_wire_svc_response *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 7);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->status);
            mc_host_wire_put_bytes(&w, value->body, 0);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_relay_event {
    mc_host_wire_bytes kind;
    int32_t handle;
    uint8_t has_request;
    mc_host_wire_bytes request;
    uint8_t has_name;
    mc_host_wire_bytes name;
    uint8_t has_body;
    mc_host_wire_bytes body;
    uint8_t has_key;
    mc_host_wire_bytes key;
    uint8_t has_value;
    mc_host_wire_bytes value;
    uint8_t has_prefix;
    mc_host_wire_bytes prefix;
    uint8_t has_url;
    mc_host_wire_bytes url;
    uint8_t has_data;
    mc_host_wire_bytes data;
    uint8_t has_connection;
    mc_host_wire_bytes connection;
    uint8_t has_method;
    mc_host_wire_bytes method;
    uint8_t has_origin;
    mc_host_wire_bytes origin;
    uint8_t has_args_digest;
    mc_host_wire_bytes args_digest;
} mc_host_wire_relay_event;
static inline mc_host_wire_result mc_host_wire_decode_relay_event(const uint8_t *data, size_t len, mc_host_wire_relay_event *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 8 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->kind = mc_host_wire_get_bytes(&r, 1);
        out->handle = mc_host_wire_get_u32(&r);
    out->has_request = mc_host_wire_get_flag(&r);
    if (out->has_request) {
        out->request = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_name = mc_host_wire_get_flag(&r);
    if (out->has_name) {
        out->name = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_body = mc_host_wire_get_flag(&r);
    if (out->has_body) {
        out->body = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_key = mc_host_wire_get_flag(&r);
    if (out->has_key) {
        out->key = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_value = mc_host_wire_get_flag(&r);
    if (out->has_value) {
        out->value = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_prefix = mc_host_wire_get_flag(&r);
    if (out->has_prefix) {
        out->prefix = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_url = mc_host_wire_get_flag(&r);
    if (out->has_url) {
        out->url = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_data = mc_host_wire_get_flag(&r);
    if (out->has_data) {
        out->data = mc_host_wire_get_bytes(&r, 0);
    }
    out->has_connection = mc_host_wire_get_flag(&r);
    if (out->has_connection) {
        out->connection = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_method = mc_host_wire_get_flag(&r);
    if (out->has_method) {
        out->method = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_origin = mc_host_wire_get_flag(&r);
    if (out->has_origin) {
        out->origin = mc_host_wire_get_bytes(&r, 1);
    }
    out->has_args_digest = mc_host_wire_get_flag(&r);
    if (out->has_args_digest) {
        out->args_digest = mc_host_wire_get_bytes(&r, 1);
    }
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_relay_event(const mc_host_wire_relay_event *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 8);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->kind, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->handle);
        if (value->has_request > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_request);
        if (value->has_request) {
            mc_host_wire_put_bytes(&w, value->request, 0);
        }
        if (value->has_name > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_name);
        if (value->has_name) {
            mc_host_wire_put_bytes(&w, value->name, 1);
        }
        if (value->has_body > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_body);
        if (value->has_body) {
            mc_host_wire_put_bytes(&w, value->body, 0);
        }
        if (value->has_key > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_key);
        if (value->has_key) {
            mc_host_wire_put_bytes(&w, value->key, 0);
        }
        if (value->has_value > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_value);
        if (value->has_value) {
            mc_host_wire_put_bytes(&w, value->value, 0);
        }
        if (value->has_prefix > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_prefix);
        if (value->has_prefix) {
            mc_host_wire_put_bytes(&w, value->prefix, 0);
        }
        if (value->has_url > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_url);
        if (value->has_url) {
            mc_host_wire_put_bytes(&w, value->url, 1);
        }
        if (value->has_data > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_data);
        if (value->has_data) {
            mc_host_wire_put_bytes(&w, value->data, 0);
        }
        if (value->has_connection > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_connection);
        if (value->has_connection) {
            mc_host_wire_put_bytes(&w, value->connection, 1);
        }
        if (value->has_method > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_method);
        if (value->has_method) {
            mc_host_wire_put_bytes(&w, value->method, 1);
        }
        if (value->has_origin > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_origin);
        if (value->has_origin) {
            mc_host_wire_put_bytes(&w, value->origin, 1);
        }
        if (value->has_args_digest > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_args_digest);
        if (value->has_args_digest) {
            mc_host_wire_put_bytes(&w, value->args_digest, 1);
        }
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_autocomplete_request {
    mc_host_wire_bytes source;
    uint32_t cursor;
    uint8_t has_cwd;
    mc_host_wire_bytes cwd;
    mc_host_wire_map env;
    uint32_t limit;
} mc_host_wire_autocomplete_request;
static inline mc_host_wire_result mc_host_wire_decode_autocomplete_request(const uint8_t *data, size_t len, mc_host_wire_autocomplete_request *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 9 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->source = mc_host_wire_get_bytes(&r, 0);
        out->cursor = mc_host_wire_get_u32(&r);
    out->has_cwd = mc_host_wire_get_flag(&r);
    if (out->has_cwd) {
        out->cwd = mc_host_wire_get_bytes(&r, 1);
    }
        out->env = mc_host_wire_get_map(&r);
        out->limit = mc_host_wire_get_u32(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_autocomplete_request(const mc_host_wire_autocomplete_request *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 9);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->source, 0);
            mc_host_wire_put_u32(&w, (uint32_t)value->cursor);
        if (value->has_cwd > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
        mc_host_wire_put_u8(&w, value->has_cwd);
        if (value->has_cwd) {
            mc_host_wire_put_bytes(&w, value->cwd, 1);
        }
            mc_host_wire_reader map_reader = {value->env.data, value->env.len, 0, MC_HOST_WIRE_OK};
            mc_host_wire_bytes previous_key = {0};
            for (uint32_t i = 0; i < value->env.count && map_reader.error == MC_HOST_WIRE_OK; ++i) {
                mc_host_wire_bytes key = mc_host_wire_get_bytes(&map_reader, 1);
                if (map_reader.error == MC_HOST_WIRE_OK && i && !mc_host_wire_less(previous_key, key)) map_reader.error = MC_HOST_WIRE_NON_CANONICAL_MAP;
                mc_host_wire_get_bytes(&map_reader, 1);
                previous_key = key;
            }
            if (map_reader.error != MC_HOST_WIRE_OK || map_reader.off != value->env.len) return map_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : map_reader.error;
            mc_host_wire_put_u32(&w, value->env.count);
            mc_host_wire_put(&w, value->env.data, value->env.len);
            mc_host_wire_put_u32(&w, (uint32_t)value->limit);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_autocomplete_item {
    mc_host_wire_bytes label;
    mc_host_wire_bytes value;
    mc_host_wire_bytes kind;
} mc_host_wire_autocomplete_item;
static inline mc_host_wire_result mc_host_wire_decode_autocomplete_item(const uint8_t *data, size_t len, mc_host_wire_autocomplete_item *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 10 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->label = mc_host_wire_get_bytes(&r, 1);
        out->value = mc_host_wire_get_bytes(&r, 1);
        out->kind = mc_host_wire_get_bytes(&r, 1);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_autocomplete_item(const mc_host_wire_autocomplete_item *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 10);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_bytes(&w, value->label, 1);
            mc_host_wire_put_bytes(&w, value->value, 1);
            mc_host_wire_put_bytes(&w, value->kind, 1);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

typedef struct mc_host_wire_autocomplete_result {
    uint32_t replace_start;
    uint32_t replace_end;
    mc_host_wire_bytes common_prefix;
    mc_host_wire_list items;
    uint8_t truncated;
} mc_host_wire_autocomplete_result;
static inline mc_host_wire_result mc_host_wire_decode_autocomplete_result(const uint8_t *data, size_t len, mc_host_wire_autocomplete_result *out) {
    if (!out || (len && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    mc_host_wire_reader r = {data, len, 0, MC_HOST_WIRE_OK};
    if (mc_host_wire_get_u16(&r) != 11 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_WRONG_MESSAGE;
    if (mc_host_wire_get_u8(&r) != 1 && r.error == MC_HOST_WIRE_OK) return MC_HOST_WIRE_UNSUPPORTED_VERSION;
        out->replace_start = mc_host_wire_get_u32(&r);
        out->replace_end = mc_host_wire_get_u32(&r);
        out->common_prefix = mc_host_wire_get_bytes(&r, 1);
        {
        out->items.count = mc_host_wire_get_u32(&r);
        out->items.data = r.data + r.off;
        size_t list_start = r.off;
        for (uint32_t i = 0; i < out->items.count && r.error == MC_HOST_WIRE_OK; ++i) {
            mc_host_wire_bytes item = mc_host_wire_get_bytes(&r, 0);
            mc_host_wire_autocomplete_item nested;
            if (r.error == MC_HOST_WIRE_OK) r.error = mc_host_wire_decode_autocomplete_item(item.data, item.len, &nested);
        }
        out->items.len = r.off - list_start;
        }
        out->truncated = mc_host_wire_get_flag(&r);
    if (r.error != MC_HOST_WIRE_OK) return r.error;
    return r.off == r.len ? MC_HOST_WIRE_OK : MC_HOST_WIRE_TRAILING_BYTES;
}
static inline mc_host_wire_result mc_host_wire_encode_autocomplete_result(const mc_host_wire_autocomplete_result *value, uint8_t *data, size_t capacity, size_t *required) {
    if (!value || !required || (capacity && !data)) return MC_HOST_WIRE_INVALID_ARGUMENT;
    for (int pass = 0; pass < 2; ++pass) {
        mc_host_wire_writer w = {pass ? data : NULL, capacity, 0, MC_HOST_WIRE_OK};
        mc_host_wire_put_u16(&w, 11);
        mc_host_wire_put_u8(&w, 1);
            mc_host_wire_put_u32(&w, (uint32_t)value->replace_start);
            mc_host_wire_put_u32(&w, (uint32_t)value->replace_end);
            mc_host_wire_put_bytes(&w, value->common_prefix, 1);
            {
            mc_host_wire_reader list_reader = {value->items.data, value->items.len, 0, MC_HOST_WIRE_OK};
            for (uint32_t i = 0; i < value->items.count; ++i) {
                mc_host_wire_bytes item = mc_host_wire_get_bytes(&list_reader, 0);
                mc_host_wire_autocomplete_item nested;
                if (list_reader.error == MC_HOST_WIRE_OK) list_reader.error = mc_host_wire_decode_autocomplete_item(item.data, item.len, &nested);
            }
            if (list_reader.error != MC_HOST_WIRE_OK || list_reader.off != value->items.len) return list_reader.error == MC_HOST_WIRE_OK ? MC_HOST_WIRE_TRAILING_BYTES : list_reader.error;
            mc_host_wire_put_u32(&w, value->items.count);
            mc_host_wire_put(&w, value->items.data, value->items.len);
            }
            if (value->truncated > 1) return MC_HOST_WIRE_INVALID_PRESENCE;
            mc_host_wire_put_u8(&w, value->truncated);
        if (w.error != MC_HOST_WIRE_OK) return w.error;
        if (!pass) { *required = w.off; if (capacity < w.off) return MC_HOST_WIRE_BUFFER_TOO_SMALL; }
    }
    return MC_HOST_WIRE_OK;
}

#endif /* MC_CONTROL_CONTRACT_H */
