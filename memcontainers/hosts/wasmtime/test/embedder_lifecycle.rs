use host::output_queue::{OutputQueue, OutputRecord, STDOUT};
use host::{CaptureSink, KernelHostBuilder, KernelModuleCache, TickState};
use runfiles::Runfiles;
use std::sync::Arc;

fn runfile(path: &str) -> Vec<u8> {
    let runfiles = Runfiles::create().expect("runfiles");
    std::fs::read(runfiles.rlocation(path).expect("runfile path")).expect("read runfile")
}

#[test]
fn pre_prompt_boot_exposes_kernel_ticks_to_the_embedder() {
    let kernel = runfile("_main/memcontainers/kernel/rust/kernel.wasm");
    let image = runfile("_main/memcontainers/images/base.tar");
    let cache = Arc::new(KernelModuleCache::bounded(1, kernel.len()));
    let digest = cache.load(&kernel).expect("load kernel");
    assert_eq!(digest, cache.load(&kernel).expect("reuse kernel"));

    let output = OutputQueue::new(1024 * 1024, 768 * 1024, 4096);
    let mut host = KernelHostBuilder::new(kernel.clone())
        .with_base_image(Some(image.clone()))
        .with_module_cache(cache.clone())
        .with_stdout(Box::new(output.sink(STDOUT)))
        .deterministic()
        .build_pre_prompt()
        .expect("initialize kernel");
    assert!(!host.at_prompt(), "boot must not hide scheduler ticks");

    for _ in 0..8192 {
        if host.at_prompt() {
            break;
        }
        assert_ne!(host.tick().expect("tick kernel"), TickState::Exited);
    }
    assert!(
        host.at_prompt(),
        "explicit ticks must reach the shell prompt"
    );
    assert_eq!(output.stats().dropped_bytes, 0);
    let mut terminal = Vec::new();
    while let Some(record) = output.next() {
        let sequence = record.sequence();
        match record {
            OutputRecord::Data { bytes, .. } => terminal.extend_from_slice(&bytes),
            OutputRecord::Gap { .. } => panic!("boot output must fit the queue"),
        }
        assert!(output.ack(sequence));
    }
    assert!(terminal.ends_with(b"$ "));
    assert_eq!(output.stats().retained_bytes, 0);

    let (second_sink, _) = CaptureSink::new();
    let second = KernelHostBuilder::new(kernel)
        .with_base_image(Some(image))
        .with_module_cache(cache)
        .with_stdout(Box::new(second_sink))
        .deterministic()
        .build()
        .expect("convenience boot");
    assert!(
        second.at_prompt(),
        "existing constructor remains ready at return"
    );
}
