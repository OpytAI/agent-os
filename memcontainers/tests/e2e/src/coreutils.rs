//! Coreutils — the guest `/bin` running through the REAL interactive shell on the REAL kernel. Each
//! boots the `posix` image, writes inputs via the control channel, runs a command through the
//! console, and asserts the terminal response. The output is CRLF: a tool emits LF to fd 1, and the
//! terminal's ONLCR (kernel io.rs) adds the CR — exactly what the agent's xterm.js sees. Behavioral
//! tests (mv/cp) run the command on the console, then verify the effect over the control channel.
//!
//! Each line proves: console → `/bin/sh -c` → `/bin/<tool>` (a Zig `@utilz` applet in the
//! per-tier mcbox, dispatched on argv[0]) → mc `sys.Impl` → the kernel.

use crate::boot_posix;

/// WHY: `cat` is a `@utilz` applet in the read-only mcbox.
/// GUARANTEES: it streams a file back byte-for-byte, ONLCR'd to CRLF on the terminal.
#[test]
fn cat_streams_a_file() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/note", b"agent-os e2e\n")
        .expect("write");
    assert_eq!(s.run_for_output("cat /tmp/note"), "agent-os e2e\r\n");
}

/// WHY: `base64` is a `@utilz` applet.
/// GUARANTEES: RFC 4648 encoding of the file.
#[test]
fn base64_encodes_a_file() {
    let mut s = boot_posix();
    s.host.write_file("/tmp/in", b"hello").expect("write");
    assert_eq!(s.run_for_output("base64 /tmp/in"), "aGVsbG8=\r\n");
}

/// WHY: `grep` is a `@utilz` applet.
/// GUARANTEES: it prints only the matching lines.
#[test]
fn grep_selects_matching_lines() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/lines", b"foo\nbar\nbaz\nqux\n")
        .expect("write");
    assert_eq!(s.run_for_output("grep ba /tmp/lines"), "bar\r\nbaz\r\n");
}

/// WHY: `sed` is a `@utilz` applet.
/// GUARANTEES: `s///` stream-edits the file.
#[test]
fn sed_substitutes() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/sed-in", b"hello world\n")
        .expect("write");
    assert_eq!(
        s.run_for_output("sed s/world/agent-os/ /tmp/sed-in"),
        "hello agent-os\r\n"
    );
}

/// WHY: `jq` is a `@utilz` applet in the read-only box.
/// GUARANTEES: the filter prints the selected value.
#[test]
fn jq_filters_json() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/j.json", b"{\"name\":\"agent-os\",\"n\":42}")
        .expect("write");
    assert_eq!(s.run_for_output("jq .n /tmp/j.json"), "42\r\n");
}

/// WHY: `head` is a `@utilz` applet.
/// GUARANTEES: `-N` prints the first N lines.
#[test]
fn head_selects_first_lines() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/multi", b"alpha\nbeta\ngamma\n")
        .expect("write");
    assert_eq!(s.run_for_output("head -2 /tmp/multi"), "alpha\r\nbeta\r\n");
}

/// WHY: `gzip` is a read-write `@utilz` applet.
/// GUARANTEES: compress → decompress recovers the original file.
#[test]
fn gzip_round_trips() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/gz", b"hello gzip round-trip\n")
        .expect("write");
    s.run_for_output("gzip /tmp/gz"); // → /tmp/gz.gz, removes /tmp/gz (silent)
    s.run_for_output("gzip -d /tmp/gz.gz"); // → /tmp/gz (silent)
    assert_eq!(
        s.run_for_output("cat /tmp/gz"),
        "hello gzip round-trip\r\n"
    );
}

/// WHY: `mv` mutates the filesystem (the read-write tier). GUARANTEES: the destination gets the
/// source's bytes and the source is gone — verified over the control channel (the fs effect is
/// real, not just terminal output).
#[test]
fn mv_renames_a_file() {
    let mut s = boot_posix();
    s.host.write_file("/tmp/x", b"aaa\n").expect("write /tmp/x");
    s.run_for_output("mv /tmp/x /tmp/y");
    assert_eq!(s.host.read_file("/tmp/y").expect("read /tmp/y"), b"aaa\n");
    assert!(
        s.host.read_file("/tmp/x").is_err(),
        "source must be gone after mv"
    );
}

/// WHY: `cp` copies (read-write) while leaving the source. GUARANTEES: destination and source both
/// hold the bytes after the copy — verified over the control channel.
#[test]
fn cp_copies_a_file() {
    let mut s = boot_posix();
    s.host
        .write_file("/tmp/src", b"copy me\n")
        .expect("write /tmp/src");
    s.run_for_output("cp /tmp/src /tmp/dst");
    assert_eq!(
        s.host.read_file("/tmp/dst").expect("read /tmp/dst"),
        b"copy me\n"
    );
    assert_eq!(
        s.host.read_file("/tmp/src").expect("read /tmp/src"),
        b"copy me\n"
    );
}

/// WHY: `/bin/env` mutates the guest `/env` overlay in place. GUARANTEES: `FOO=bar` is
/// visible to the spawned command (`printenv FOO` prints `bar`).
#[test]
fn env_sets_var_for_command() {
    let mut s = boot_posix();
    assert_eq!(s.run_for_output("env FOO=bar printenv FOO"), "bar\r\n");
}

/// WHY: wiping `/env` after spawn would destroy the guest environment. GUARANTEES: after
/// `env FOO=bar printenv FOO`, `/env` still exists and a planted live name still reads.
#[test]
fn env_leaves_guest_env_dir() {
    let mut s = boot_posix();
    assert_eq!(
        s.run_for_output("echo keep >/env/KEEP; printenv KEEP"),
        "keep\r\n"
    );
    assert_eq!(s.run_for_output("env FOO=bar printenv FOO"), "bar\r\n");
    s.host.stat("/env").expect("/env must still exist");
    assert_eq!(s.run_for_output("printenv KEEP"), "keep\r\n");
}

/// WHY: `-i` must not leak names that were in the live shell `/env` before the command.
/// GUARANTEES: `env -i PATH=/bin printenv` is exactly `PATH=/bin`.
#[test]
fn env_ignore_environment_keeps_only_assignments() {
    let mut s = boot_posix();
    let path = s.run_for_output("printenv PATH");
    assert!(
        !path.is_empty() && !path.contains("No such file"),
        "live PATH must exist before env -i, got:\n{path}"
    );
    assert_eq!(
        s.run_for_output("printf hidden >/env/SECRET; printenv SECRET"),
        "hidden\r\n"
    );
    assert_eq!(
        s.run_for_output("env -i PATH=/bin printenv"),
        "PATH=/bin\r\n"
    );
}
