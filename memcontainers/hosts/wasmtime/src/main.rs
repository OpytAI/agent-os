//! `mc` — the AgentOS runtime CLI. Loads a `kernel.wasm` (and an optional base image),
//! then drives it either interactively over a raw terminal or from a scripted input
//! transcript (the mode the e2e suite uses). All the real work lives in the `host`
//! library; this is the thin front door, named `mc` for the running system's identity,
//! not `agent-os-*`.

use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use clap::Parser;
use host::{CaptureSink, DiskPersist, KernelHostBuilder, RealNet, StdioSink, TickState};

mod terminal;
use terminal::Terminal;

#[derive(Parser, Debug)]
#[command(name = "mc", about = "AgentOS runtime — load and drive a kernel.wasm")]
struct Cli {
    /// Path to the kernel.wasm artifact.
    #[arg(long, value_name = "PATH")]
    kernel: PathBuf,

    /// Optional base image (tar) to feed via mc_load_base_image.
    #[arg(long, value_name = "PATH")]
    image: Option<PathBuf>,

    /// Replay scripted input bytes from PATH instead of opening a TTY. The host exits once
    /// output has been quiet at a settled prompt or `--timeout-ticks` is reached.
    #[arg(long, value_name = "PATH")]
    script: Option<PathBuf>,

    /// Maximum ticks to run in `--script` mode before bailing.
    #[arg(long, default_value_t = 5000)]
    timeout_ticks: usize,

    /// Grant the network capability (real HTTP/HTTPS/WebSocket egress). Without it the
    /// kernel's `fetch`/`wscat` report "network unavailable" (the default-deny gate, A9).
    #[arg(long)]
    allow_net: bool,

    /// Grant the persistence capability, backed by the directory at PATH (created if
    /// needed). The kernel surfaces it as `/var/persist`; data survives restarts. Without
    /// it, `/var/persist` is read/write-denied (the default-deny gate, A9).
    #[arg(long, value_name = "PATH")]
    persist_dir: Option<PathBuf>,

    /// Write the cross-host test snapshot to PATH.
    #[arg(long, hide = true, value_name = "PATH")]
    snapshot_fixture_output: Option<PathBuf>,

    /// Write the incremental cross-host test snapshot bundle.
    #[arg(long, hide = true, requires = "snapshot_fixture_output")]
    snapshot_fixture_incremental: bool,
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    let wasm_bytes = std::fs::read(&cli.kernel)
        .with_context(|| format!("reading kernel from {}", cli.kernel.display()))?;
    let base_image = match &cli.image {
        Some(p) => Some(
            std::fs::read(p).with_context(|| format!("reading base image from {}", p.display()))?,
        ),
        None => None,
    };

    if let Some(out) = &cli.snapshot_fixture_output {
        return write_snapshot_fixture(
            wasm_bytes,
            base_image.context("snapshot fixture requires --image")?,
            out,
            cli.snapshot_fixture_incremental,
        );
    }

    let mut builder = KernelHostBuilder::new(wasm_bytes)
        .with_base_image(base_image)
        .with_stdout(Box::new(StdioSink::stdout()))
        .with_stderr(Box::new(StdioSink::stderr()))
        .with_log(Box::new(StdioSink::stdout()));
    if cli.allow_net {
        builder = builder.with_net(Box::new(RealNet::new()));
    }
    if let Some(dir) = &cli.persist_dir {
        builder = builder.with_persist(Box::new(DiskPersist::new(dir)));
    }
    let mut host = builder.build().context("building KernelHost")?;

    if let Some(path) = &cli.script {
        let bytes = std::fs::read(path)
            .with_context(|| format!("reading script from {}", path.display()))?;
        host.send_input(&bytes).context("sending scripted input")?;
        host.run_script(cli.timeout_ticks)
            .context("running scripted session")?;
        return Ok(());
    }

    // Interactive: raw terminal mode. Install a panic hook so a wasmtime trap or any panic
    // restores cooked mode before the unwind prints.
    install_raw_mode_panic_hook();
    let terminal = Terminal::new()?;
    loop {
        if let Ok(Some(key)) = terminal.read_key() {
            use crossterm::event::{KeyCode, KeyModifiers};
            if let KeyCode::Char('c') = key.code {
                if key.modifiers.contains(KeyModifiers::CONTROL) {
                    break;
                }
            }
            let byte: u8 = match key.code {
                KeyCode::Char(c) => c as u8,
                KeyCode::Enter => b'\n',
                KeyCode::Backspace => 0x08,
                _ => 0,
            };
            if byte != 0 {
                host.send_input(&[byte])?;
            }
        }

        if host.tick()? == TickState::Exited {
            break;
        }

        std::thread::sleep(std::time::Duration::from_millis(5));
    }

    Ok(())
}

fn write_snapshot_fixture(
    kernel: Vec<u8>,
    base: Vec<u8>,
    out: &Path,
    incremental: bool,
) -> Result<()> {
    const MARKER: &[u8] = b"rust-host snapshot -> js-host restore";

    let (sink, _stdout) = CaptureSink::new();
    let mut host = KernelHostBuilder::new(kernel)
        .with_base_image(Some(base))
        .with_stdout(Box::new(sink))
        .deterministic()
        .build()
        .context("booting kernel.wasm under the wasmtime host")?;

    if incremental {
        let base_snap = host.snapshot().context("snapshotting the baseline VM")?;
        host.write_file("/tmp/xhost", MARKER)
            .context("writing the cross-host marker")?;
        let delta = host
            .snapshot_incremental(&base_snap)
            .context("snapshotting changed pages")?;
        let mut bundle = Vec::with_capacity(4 + base_snap.len() + delta.len());
        bundle.extend_from_slice(&(base_snap.len() as u32).to_le_bytes());
        bundle.extend_from_slice(&base_snap);
        bundle.extend_from_slice(&delta);
        std::fs::write(out, &bundle)
            .with_context(|| format!("writing incremental bundle to {}", out.display()))?;
        eprintln!(
            "cross-host incremental snapshot: {} bytes over {} byte baseline",
            delta.len(),
            base_snap.len()
        );
        return Ok(());
    }

    host.write_file("/tmp/xhost", MARKER)
        .context("writing the cross-host marker")?;
    let snap = host.snapshot().context("snapshotting the booted VM")?;
    std::fs::write(out, &snap).with_context(|| format!("writing snapshot to {}", out.display()))?;
    eprintln!(
        "cross-host snapshot: {} bytes produced by the wasmtime host",
        snap.len()
    );
    Ok(())
}

fn install_raw_mode_panic_hook() {
    let default = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let _ = crossterm::terminal::disable_raw_mode();
        default(info);
    }));
}
