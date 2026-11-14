//! Caller-supplied clock and entropy for deterministic VMs.
//!
//! A deterministic VM reads no ambient state from the machine it runs on. The
//! application supplies the wall clock, the monotonic base, and the entropy
//! seed on create and restore, and may replace them at the start of a drive.
//! A snapshot does not carry these values, so a restore that is given the
//! values of the original run reads the same clock and the same bytes.

use std::sync::{Arc, Mutex, MutexGuard};

use host::{ClockSource, RngSource, SeededRng};

/// One set of ambient inputs, as the application states them.
#[derive(Clone, Copy)]
pub struct AmbientInputs {
    pub wall_ms: i64,
    pub monotonic_ms: i64,
    pub entropy_seed: u64,
}

impl AmbientInputs {
    pub fn is_zero(&self) -> bool {
        self.wall_ms == 0 && self.monotonic_ms == 0 && self.entropy_seed == 0
    }

    pub fn is_valid(&self) -> bool {
        self.wall_ms >= 0 && self.monotonic_ms >= 0
    }
}

struct State {
    wall_ms: i64,
    monotonic_ms: i64,
    rng: SeededRng,
}

/// The VM record and the kernel host's clock and RNG share one state.
#[derive(Clone)]
pub struct Ambient(Arc<Mutex<State>>);

impl Ambient {
    pub fn new(inputs: AmbientInputs) -> Self {
        Self(Arc::new(Mutex::new(State {
            wall_ms: inputs.wall_ms,
            monotonic_ms: inputs.monotonic_ms,
            rng: SeededRng::new(inputs.entropy_seed),
        })))
    }

    // A reader that panicked cannot leave these plain values half-written.
    fn state(&self) -> MutexGuard<'_, State> {
        self.0.lock().unwrap_or_else(|poison| poison.into_inner())
    }

    pub fn set(&self, inputs: AmbientInputs) {
        let mut state = self.state();
        state.wall_ms = inputs.wall_ms;
        state.monotonic_ms = inputs.monotonic_ms;
        state.rng = SeededRng::new(inputs.entropy_seed);
    }

    /// Inputs for a fork of this VM. The child keeps the parent's clock and
    /// takes its seed from the parent's stream, so the two streams differ and
    /// both stay a function of the parent's inputs.
    pub fn fork_inputs(&self) -> AmbientInputs {
        let mut state = self.state();
        let mut seed = [0u8; 8];
        state.rng.fill(&mut seed);
        AmbientInputs {
            wall_ms: state.wall_ms,
            monotonic_ms: state.monotonic_ms,
            entropy_seed: u64::from_le_bytes(seed),
        }
    }

    pub fn clock(&self) -> Box<dyn ClockSource> {
        Box::new(AmbientClock(self.clone()))
    }

    pub fn rng(&self) -> Box<dyn RngSource> {
        Box::new(AmbientRng(self.clone()))
    }
}

struct AmbientClock(Ambient);

impl ClockSource for AmbientClock {
    fn now_millis(&mut self) -> i64 {
        self.0.state().wall_ms
    }

    // One millisecond per read: a loop that polls the clock makes progress
    // without reading how long the host took.
    fn monotonic_millis(&mut self) -> i64 {
        let mut state = self.0.state();
        let value = state.monotonic_ms;
        state.monotonic_ms = value.saturating_add(1);
        value
    }
}

struct AmbientRng(Ambient);

impl RngSource for AmbientRng {
    fn fill(&mut self, buf: &mut [u8]) {
        self.0.state().rng.fill(buf);
    }
}
