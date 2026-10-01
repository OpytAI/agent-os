//! Guest `/bin/git` JSON request encoding. Host-testable without the sysroot.

#![allow(dead_code)]

pub fn copy_bytes(src: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    if src.len() > out.len() {
        return Err(1);
    }
    out[..src.len()].copy_from_slice(src);
    Ok(src.len())
}

pub fn push(out: &mut [u8], i: usize, s: &[u8]) -> Result<usize, i32> {
    if i + s.len() > out.len() {
        return Err(1);
    }
    out[i..i + s.len()].copy_from_slice(s);
    Ok(i + s.len())
}

pub fn push_escaped(out: &mut [u8], mut i: usize, s: &[u8]) -> Result<usize, i32> {
    for &c in s {
        match c {
            b'"' => {
                i = push(out, i, b"\\\"")?;
                continue;
            }
            b'\\' => {
                i = push(out, i, b"\\\\")?;
                continue;
            }
            b'\n' => {
                i = push(out, i, b"\\n")?;
                continue;
            }
            b'\r' => {
                i = push(out, i, b"\\r")?;
                continue;
            }
            b'\t' => {
                i = push(out, i, b"\\t")?;
                continue;
            }
            0x08 => {
                i = push(out, i, b"\\b")?;
                continue;
            }
            0x0c => {
                i = push(out, i, b"\\f")?;
                continue;
            }
            0x00..=0x1f => {
                const HEX: &[u8; 16] = b"0123456789abcdef";
                let esc = [
                    b'\\',
                    b'u',
                    b'0',
                    b'0',
                    HEX[(c >> 4) as usize],
                    HEX[(c & 15) as usize],
                ];
                i = push(out, i, &esc)?;
                continue;
            }
            _ => {}
        }
        if i >= out.len() {
            return Err(1);
        }
        out[i] = c;
        i += 1;
    }
    Ok(i)
}

pub fn valid_depth(value: &[u8]) -> bool {
    !value.is_empty() && value != b"0" && value.iter().all(|byte| byte.is_ascii_digit())
}

pub fn valid_refspec(value: &[u8]) -> bool {
    if !value.starts_with(b"refs/") {
        return false;
    }
    let mut colon = None;
    for (index, byte) in value.iter().enumerate() {
        if *byte == b':' {
            if colon.is_some() {
                return false;
            }
            colon = Some(index);
        }
    }
    match colon {
        Some(index) => {
            index > 0 && index + 1 < value.len() && value[index + 1..].starts_with(b"refs/")
        }
        None => false,
    }
}

pub fn fmt_remote(
    op: &[u8],
    url: Option<&[u8]>,
    depth: Option<&[u8]>,
    out: &mut [u8],
) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"")?;
    i = push(out, i, op)?;
    i = push(out, i, b"\",\"args\":{")?;
    if let Some(url) = url {
        i = push(out, i, b"\"url\":\"")?;
        i = push_escaped(out, i, url)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(depth) = depth {
        if url.is_some() {
            i = push(out, i, b",")?;
        }
        i = push(out, i, b"\"depth\":")?;
        i = push(out, i, depth)?;
    }
    i = push(out, i, b"}}")?;
    Ok(i)
}

pub fn fmt_push(url: &[u8], refspec: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"push\",\"args\":{\"url\":\"")?;
    i = push_escaped(out, i, url)?;
    i = push(out, i, b"\",\"refspecs\":[\"")?;
    i = push_escaped(out, i, refspec)?;
    i = push(out, i, b"\"]}}")?;
    Ok(i)
}

pub fn fmt_op_path(op: &[u8], path: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"")?;
    i = push(out, i, op)?;
    i = push(out, i, b"\",\"args\":{\"path\":\"")?;
    i = push_escaped(out, i, path)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_op_name(op: &[u8], name: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"")?;
    i = push(out, i, op)?;
    i = push(out, i, b"\",\"args\":{\"name\":\"")?;
    i = push_escaped(out, i, name)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_op_rev(op: &[u8], rev: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"")?;
    i = push(out, i, op)?;
    i = push(out, i, b"\",\"args\":{\"rev\":\"")?;
    i = push_escaped(out, i, rev)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_reset(mode: &[u8], rev: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"reset\",\"args\":{\"mode\":\"")?;
    i = push_escaped(out, i, mode)?;
    i = push(out, i, b"\",\"rev\":\"")?;
    i = push_escaped(out, i, rev)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_tag_delete(name: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"tag\",\"args\":{\"name\":\"")?;
    i = push_escaped(out, i, name)?;
    i = push(out, i, b"\",\"delete\":true}}")?;
    Ok(i)
}

pub fn fmt_branch_delete(name: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"branch\",\"args\":{\"name\":\"")?;
    i = push_escaped(out, i, name)?;
    i = push(out, i, b"\",\"delete\":true}}")?;
    Ok(i)
}

pub fn fmt_config_get(key: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(
        out,
        i,
        b"{\"op\":\"config\",\"args\":{\"action\":\"get\",\"key\":\"",
    )?;
    i = push_escaped(out, i, key)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_config_set(key: &[u8], value: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(
        out,
        i,
        b"{\"op\":\"config\",\"args\":{\"action\":\"set\",\"key\":\"",
    )?;
    i = push_escaped(out, i, key)?;
    i = push(out, i, b"\",\"value\":\"")?;
    i = push_escaped(out, i, value)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_remote_add(name: &[u8], url: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(
        out,
        i,
        b"{\"op\":\"remote\",\"args\":{\"action\":\"add\",\"name\":\"",
    )?;
    i = push_escaped(out, i, name)?;
    i = push(out, i, b"\",\"url\":\"")?;
    i = push_escaped(out, i, url)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_remote_remove(name: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(
        out,
        i,
        b"{\"op\":\"remote\",\"args\":{\"action\":\"remove\",\"name\":\"",
    )?;
    i = push_escaped(out, i, name)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_commit(msg: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"commit\",\"args\":{\"message\":\"")?;
    i = push_escaped(out, i, msg)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

pub fn fmt_rev_parse(rev: &[u8], out: &mut [u8]) -> Result<usize, i32> {
    let mut i = 0usize;
    i = push(out, i, b"{\"op\":\"rev-parse\",\"args\":{\"rev\":\"")?;
    i = push_escaped(out, i, rev)?;
    i = push(out, i, b"\"}}")?;
    Ok(i)
}

/// Parse `git clone|fetch|pull [--depth N] [url]` into the host_call JSON body.
pub fn build_depth_remote(
    op: &[u8],
    args: &[&[u8]],
    require_url: bool,
    out: &mut [u8],
) -> Result<usize, i32> {
    let mut url: Option<&[u8]> = None;
    let mut depth: Option<&[u8]> = None;
    let mut i = 2usize;
    while i < args.len() {
        let a = args[i];
        if a == b"--depth" {
            if depth.is_some() || i + 1 >= args.len() || !valid_depth(args[i + 1]) {
                return Err(2);
            }
            depth = Some(args[i + 1]);
            i += 2;
            continue;
        }
        if a.first() == Some(&b'-') {
            return Err(2);
        }
        if url.is_some() || a.is_empty() {
            return Err(2);
        }
        url = Some(a);
        i += 1;
    }
    if require_url && url.is_none() {
        return Err(2);
    }
    fmt_remote(op, url, depth, out)
}

/// Parse remote argv (`clone`/`fetch`/`pull`/`push`) into the host_call JSON body.
pub fn build_remote_request(cmd: &[u8], args: &[&[u8]], out: &mut [u8]) -> Result<usize, i32> {
    let argc = args.len();
    if cmd == b"fetch" || cmd == b"pull" {
        return build_depth_remote(cmd, args, false, out);
    }
    if cmd == b"push" {
        if argc != 4 || args[2].is_empty() || !valid_refspec(args[3]) {
            return Err(2);
        }
        return fmt_push(args[2], args[3], out);
    }
    if cmd == b"clone" {
        return build_depth_remote(cmd, args, true, out);
    }
    Err(2)
}

/// Parse local argv that is a single JSON request (not add/rm/diff loops).
pub fn build_local_request(cmd: &[u8], args: &[&[u8]], out: &mut [u8]) -> Result<usize, i32> {
    let argc = args.len();
    if cmd == b"init" {
        if argc != 2 {
            return Err(2);
        }
        return copy_bytes(b"{\"op\":\"init\"}", out);
    }
    if cmd == b"status" {
        if argc != 2 {
            return Err(2);
        }
        return copy_bytes(b"{\"op\":\"status\",\"args\":{\"short\":false}}", out);
    }
    if cmd == b"log" {
        if argc != 2 {
            return Err(2);
        }
        return copy_bytes(b"{\"op\":\"log\",\"args\":{\"max_count\":32}}", out);
    }
    if cmd == b"commit" {
        if argc != 4 || args[2] != b"-m" {
            return Err(2);
        }
        return fmt_commit(args[3], out);
    }
    if cmd == b"checkout" || cmd == b"switch" {
        if argc != 3 || args[2].first() == Some(&b'-') {
            return Err(2);
        }
        return fmt_op_name(cmd, args[2], out);
    }
    if cmd == b"rev-parse" {
        if argc > 3 {
            return Err(2);
        }
        let rev = if argc >= 3 { args[2] } else { b"HEAD" };
        return fmt_rev_parse(rev, out);
    }
    if cmd == b"diff" {
        if argc != 3 || (args[2] != b"--cached" && args[2] != b"--staged") {
            return Err(2);
        }
        return copy_bytes(b"{\"op\":\"diff\",\"args\":{\"cached\":true}}", out);
    }
    if cmd == b"merge" {
        return build_merge_request(args, out);
    }
    Err(2)
}

/// Parse `git merge` into an ordered-head JSON request.
pub fn build_merge_request(args: &[&[u8]], out: &mut [u8]) -> Result<usize, i32> {
    if args.len() < 2 || args[1] != b"merge" {
        return Err(2);
    }
    if args.len() >= 3 && (args[2] == b"--abort" || args[2] == b"--continue") {
        if args.len() != 3 {
            return Err(2);
        }
        let action: &[u8] = if args[2] == b"--abort" {
            b"abort"
        } else {
            b"continue"
        };
        let mut i = push(out, 0, b"{\"op\":\"merge\",\"args\":{\"action\":\"")?;
        i = push(out, i, action)?;
        i = push(out, i, b"\"}}")?;
        return Ok(i);
    }

    let mut no_commit = false;
    let mut allow_unrelated = false;
    let mut find_renames = true;
    let mut saw_renames = false;
    let mut ff: Option<&[u8]> = None;
    let mut strategy: Option<&[u8]> = None;
    let mut favor: Option<&[u8]> = None;
    let mut diff_algorithm: Option<&[u8]> = None;
    let mut conflict_style: Option<&[u8]> = None;
    let mut subtree: Option<&[u8]> = None;
    let mut message: Option<&[u8]> = None;
    let mut threshold: Option<u16> = None;
    let mut heads: [&[u8]; 14] = [&[]; 14];
    let mut head_count = 0usize;
    let mut index = 2usize;
    while index < args.len() {
        let arg = args[index];
        if arg == b"--" {
            return Err(2);
        }
        if arg == b"--squash" || (arg == b"-s" && index + 1 < args.len() && args[index + 1] == b"theirs") {
            return Err(2);
        }
        if arg == b"--no-commit" {
            if no_commit {
                return Err(2);
            }
            no_commit = true;
        } else if arg == b"--allow-unrelated-histories" {
            if allow_unrelated {
                return Err(2);
            }
            allow_unrelated = true;
        } else if arg == b"--ff" || arg == b"--no-ff" || arg == b"--ff-only" {
            if ff.is_some() {
                return Err(2);
            }
            ff = Some(if arg == b"--ff" {
                b"ff"
            } else if arg == b"--no-ff" {
                b"no-ff"
            } else {
                b"ff-only"
            });
        } else if arg == b"-s" {
            index += 1;
            if index >= args.len() || strategy.is_some() || !merge_strategy(args[index]) {
                return Err(2);
            }
            strategy = Some(args[index]);
        } else if arg == b"-m" {
            index += 1;
            if index >= args.len() || message.is_some() {
                return Err(2);
            }
            message = Some(args[index]);
        } else if arg == b"-X" || arg.starts_with(b"-X") {
            let value = if arg == b"-X" {
                index += 1;
                if index >= args.len() {
                    return Err(2);
                }
                args[index]
            } else {
                &arg[2..]
            };
            if !apply_merge_option(
                value,
                &mut favor,
                &mut find_renames,
                &mut saw_renames,
                &mut threshold,
                &mut diff_algorithm,
                &mut conflict_style,
                &mut subtree,
            ) {
                return Err(2);
            }
        } else if arg.first() == Some(&b'-') {
            return Err(2);
        } else {
            if arg.is_empty() || head_count == heads.len() {
                return Err(2);
            }
            heads[head_count] = arg;
            head_count += 1;
        }
        index += 1;
    }
    if head_count == 0 {
        return Err(2);
    }

    let mut i = push(out, 0, b"{\"op\":\"merge\",\"args\":{\"heads\":[")?;
    for (slot, head) in heads[..head_count].iter().enumerate() {
        if slot != 0 {
            i = push(out, i, b",")?;
        }
        i = push(out, i, b"\"")?;
        i = push_escaped(out, i, head)?;
        i = push(out, i, b"\"")?;
    }
    i = push(out, i, b"],\"find_renames\":")?;
    i = push(out, i, if find_renames { b"true" } else { b"false" })?;
    if let Some(value) = ff {
        i = push(out, i, b",\"ff\":\"")?;
        i = push(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(value) = strategy {
        i = push(out, i, b",\"strategy\":\"")?;
        i = push(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(value) = favor {
        i = push(out, i, b",\"favor\":\"")?;
        i = push(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(value) = threshold {
        i = push(out, i, b",\"rename_threshold\":")?;
        i = push_u16(out, i, value)?;
    }
    if let Some(value) = diff_algorithm {
        i = push(out, i, b",\"diff_algorithm\":\"")?;
        i = push(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(value) = conflict_style {
        i = push(out, i, b",\"conflict_style\":\"")?;
        i = push(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if let Some(value) = subtree {
        i = push(out, i, b",\"subtree\":\"")?;
        i = push_escaped(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    if allow_unrelated {
        i = push(out, i, b",\"allow_unrelated_histories\":true")?;
    }
    if no_commit {
        i = push(out, i, b",\"no_commit\":true")?;
    }
    if let Some(value) = message {
        i = push(out, i, b",\"message\":\"")?;
        i = push_escaped(out, i, value)?;
        i = push(out, i, b"\"")?;
    }
    i = push(out, i, b"}}")?;
    Ok(i)
}

fn merge_strategy(value: &[u8]) -> bool {
    matches!(
        value,
        b"ort" | b"recursive" | b"resolve" | b"octopus" | b"ours" | b"subtree"
    )
}

fn apply_merge_option<'a>(
    value: &'a [u8],
    favor: &mut Option<&'a [u8]>,
    find_renames: &mut bool,
    saw_renames: &mut bool,
    threshold: &mut Option<u16>,
    diff_algorithm: &mut Option<&'a [u8]>,
    conflict_style: &mut Option<&'a [u8]>,
    subtree: &mut Option<&'a [u8]>,
) -> bool {
    if value == b"ours" || value == b"theirs" {
        if favor.is_some() {
            return false;
        }
        *favor = Some(value);
        return true;
    }
    if value == b"find-renames" || value == b"no-renames" {
        if *saw_renames {
            return false;
        }
        *saw_renames = true;
        *find_renames = value == b"find-renames";
        return true;
    }
    if let Some(raw) = value.strip_prefix(b"rename-threshold=") {
        if threshold.is_some() {
            return false;
        }
        let parsed = match parse_threshold(raw) {
            Some(parsed) => parsed,
            None => return false,
        };
        *threshold = Some(parsed);
        return true;
    }
    if let Some(raw) = value.strip_prefix(b"diff-algorithm=") {
        if diff_algorithm.is_some()
            || !matches!(raw, b"histogram" | b"myers" | b"minimal" | b"patience")
        {
            return false;
        }
        *diff_algorithm = Some(raw);
        return true;
    }
    if let Some(raw) = value.strip_prefix(b"conflict-style=") {
        if conflict_style.is_some() || !matches!(raw, b"merge" | b"diff3" | b"zdiff3") {
            return false;
        }
        *conflict_style = Some(raw);
        return true;
    }
    if let Some(raw) = value.strip_prefix(b"subtree=") {
        if subtree.is_some() || raw.is_empty() {
            return false;
        }
        *subtree = Some(raw);
        return true;
    }
    false
}

fn parse_threshold(value: &[u8]) -> Option<u16> {
    if value.is_empty() || value.len() > 3 || !value.iter().all(|byte| byte.is_ascii_digit()) {
        return None;
    }
    let mut number: u16 = 0;
    for &byte in value {
        number = number * 10 + (byte - b'0') as u16;
    }
    if number > 100 {
        None
    } else {
        Some(number)
    }
}

fn push_u16(out: &mut [u8], mut i: usize, value: u16) -> Result<usize, i32> {
    let mut buf = [0u8; 3];
    let mut length = 0usize;
    let mut number = value;
    loop {
        buf[length] = b'0' + (number % 10) as u8;
        length += 1;
        number /= 10;
        if number == 0 {
            break;
        }
    }
    while length > 0 {
        length -= 1;
        if i >= out.len() {
            return Err(1);
        }
        out[i] = buf[length];
        i += 1;
    }
    Ok(i)
}
