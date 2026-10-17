//! Runtime-owned kernel identities. Child resources use the same namespace,
//! kind, generation, and slot encoding as runtime handles.

use std::sync::{Arc, Mutex, Weak};

use embedder_rust as contract;
use host::output_queue::{OutputQueue, OutputRecord, OutputStats, LOG, STDERR, STDOUT};
use host::{
    inspect_snapshot_bytes, ExecOptions, KernelCacheLimit, KernelHost, KernelHostBuilder,
    KernelModuleCache, SnapshotDescription, TickState,
};

use super::operations::OperationRecord;
use super::{pack_kind, unpack, MAX_GENERATION, MAX_INDEX};

pub const KERNEL_KIND: u8 = 2;
pub const VM_KIND: u8 = 3;
pub const OUTPUT_KIND: u8 = 4;
pub const BASELINE_KIND: u8 = 5;
pub const SNAPSHOT_KIND: u8 = 6;
pub const OPERATION_KIND: u8 = 7;
const MAX_KERNELS: usize = 64;
const MAX_KERNEL_BYTES: usize = 256 * 1024 * 1024;
const MAX_VMS: usize = 16;
const MAX_IMAGE_BYTES: usize = 256 * 1024 * 1024;
const MAX_DRIVE_TICKS: u32 = 1_000;
const OUTPUT_MAX_BYTES: usize = 4 * 1024 * 1024;
const OUTPUT_HIGH_WATER: usize = 3 * 1024 * 1024;
const OUTPUT_MAX_RECORDS: usize = 4_096;
const SNAPSHOT_MAX_BYTES: usize = 512 * 1024 * 1024;
const SNAPSHOT_TOTAL_BYTES: usize = 1024 * 1024 * 1024;
const MAX_OPERATIONS_PER_VM: usize = 64;

struct Slot<T> {
    generation: u32,
    value: Option<T>,
    retired: bool,
}

struct HandleTable<T> {
    kind: u8,
    slots: Vec<Slot<T>>,
    free: Vec<usize>,
    live: usize,
}

impl<T> HandleTable<T> {
    fn new(kind: u8) -> Self {
        Self {
            kind,
            slots: Vec::new(),
            free: Vec::new(),
            live: 0,
        }
    }

    fn insert(&mut self, namespace: u16, value: T) -> Result<u64, u32> {
        let index = if let Some(index) = self.free.pop() {
            index
        } else {
            if self.slots.len() >= MAX_INDEX {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            self.slots.push(Slot {
                generation: 1,
                value: None,
                retired: false,
            });
            self.slots.len() - 1
        };
        let slot = &mut self.slots[index];
        if slot.retired || slot.value.is_some() {
            return Err(contract::MC_HOST_INTERNAL);
        }
        slot.value = Some(value);
        self.live += 1;
        Ok(pack_kind(namespace, self.kind, slot.generation, index))
    }

    fn slot_mut(&mut self, namespace: u16, handle: u64) -> Result<(usize, &mut Slot<T>), u32> {
        let (actual_namespace, kind, generation, index) =
            unpack(handle).ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if kind != self.kind {
            return Err(contract::MC_HOST_WRONG_KIND);
        }
        if actual_namespace != namespace {
            return Err(contract::MC_HOST_STALE_HANDLE);
        }
        let slot = self
            .slots
            .get_mut(index)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if slot.generation != generation || slot.value.is_none() {
            return Err(contract::MC_HOST_STALE_HANDLE);
        }
        Ok((index, slot))
    }

    fn get(&mut self, namespace: u16, handle: u64) -> Result<&T, u32> {
        Ok(self
            .slot_mut(namespace, handle)?
            .1
            .value
            .as_ref()
            .expect("validated slot"))
    }

    fn get_mut(&mut self, namespace: u16, handle: u64) -> Result<&mut T, u32> {
        Ok(self
            .slot_mut(namespace, handle)?
            .1
            .value
            .as_mut()
            .expect("validated slot"))
    }

    fn remove(&mut self, namespace: u16, handle: u64) -> Result<T, u32> {
        let (index, slot) = self.slot_mut(namespace, handle)?;
        let value = slot.value.take().expect("validated slot");
        if slot.generation == MAX_GENERATION {
            slot.retired = true;
        } else {
            slot.generation += 1;
            self.free.push(index);
        }
        self.live -= 1;
        Ok(value)
    }
}

struct KernelRecord {
    bytes: Arc<[u8]>,
    digest: [u8; 32],
    pins: usize,
}

pub struct VmRecord {
    kernel_handle: u64,
    host: Mutex<Option<KernelHost>>,
    pub output: Arc<OutputQueue>,
    observed: Mutex<VmObserved>,
    operations: Mutex<Vec<Weak<OperationRecord>>>,
    // The Rust host memoizes baseline integrity by buffer address. Keep the
    // immutable full MCSN bytes at that address until this VM is closed.
    _snapshot_anchor: Option<Arc<[u8]>>,
}

pub struct SnapshotRecord {
    pub bytes: Vec<u8>,
    kernel_handle: u64,
}

#[derive(Clone, Copy)]
pub struct VmObserved {
    pub last_tick: u32,
    pub exit_code: Option<i32>,
    pub poisoned: bool,
    pub closing: bool,
}

pub struct VmDriveResult {
    pub ticks: u32,
    pub stop_reason: u32,
    pub report: VmReport,
}

pub struct VmReport {
    pub observed: VmObserved,
    pub output: OutputStats,
    pub inflight_egress: i32,
    pub pending_commits: i32,
    pub workers: u32,
    pub at_prompt: bool,
    pub operation_ready: bool,
    pub operation_pending: bool,
}

pub struct VmConfig {
    pub layers: Vec<Vec<u8>>,
    pub deterministic: bool,
    pub requested_workers: Option<i32>,
    pub contract_tier: i32,
    pub contract_budget_mib: i32,
    pub contract_fuel: i64,
    pub terminal_size: Option<(i32, i32)>,
}

impl VmRecord {
    fn operation_readiness(&self) -> Result<(bool, bool), u32> {
        let mut ready = false;
        let mut pending = false;
        for operation in self.live_operations()? {
            let state = operation.observed()?.state;
            ready |= state == contract::MC_HOST_OP_READY
                || state == contract::MC_HOST_OP_FAILED
                || state == contract::MC_HOST_OP_CANCELLED;
            pending |= state == contract::MC_HOST_OP_NEW || state == contract::MC_HOST_OP_PENDING;
        }
        Ok((ready, pending))
    }

    fn live_operations(&self) -> Result<Vec<Arc<OperationRecord>>, u32> {
        let mut records = self
            .operations
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let mut live = Vec::with_capacity(records.len());
        records.retain(|record| {
            if let Some(record) = record.upgrade() {
                live.push(record);
                true
            } else {
                false
            }
        });
        Ok(live)
    }

    pub fn snapshot(&self, baseline: Option<&[u8]>) -> Result<Vec<u8>, u32> {
        let mut host = self.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        let observed = self
            .observed
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if observed.closing {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if observed.poisoned {
            return Err(contract::MC_HOST_POISONED);
        }
        drop(observed);
        for record in self.live_operations()? {
            if record.pending()? {
                return Err(contract::MC_HOST_NOT_QUIESCENT);
            }
        }
        if host
            .inflight_egress()
            .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?
            != 0
            || host
                .pending_commits()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?
                != 0
        {
            return Err(contract::MC_HOST_NOT_QUIESCENT);
        }
        if host.snapshot_len() > SNAPSHOT_MAX_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let bytes = match baseline {
            Some(base) => host
                .snapshot_incremental(base)
                .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?,
            None => {
                let mut bytes = vec![0; host.snapshot_len()];
                host.snapshot_into(&mut bytes)
                    .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
                bytes
            }
        };
        if bytes.len() > SNAPSHOT_MAX_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        Ok(bytes)
    }
    fn report(&self, host: &mut KernelHost) -> Result<VmReport, u32> {
        let (operation_ready, operation_pending) = self.operation_readiness()?;
        let observed = *self
            .observed
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(VmReport {
            observed,
            output: self.output.stats(),
            inflight_egress: host
                .inflight_egress()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?,
            pending_commits: host
                .pending_commits()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?,
            workers: host
                .workers()
                .try_into()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?,
            at_prompt: host.at_prompt(),
            operation_ready,
            operation_pending,
        })
    }

    pub fn status(&self) -> Result<VmReport, u32> {
        let mut host = self.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        match host.as_mut() {
            Some(host) => self.report(host),
            None => {
                let (operation_ready, operation_pending) = self.operation_readiness()?;
                Ok(VmReport {
                    operation_ready,
                    operation_pending,
                    observed: *self
                        .observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?,
                    output: self.output.stats(),
                    inflight_egress: 0,
                    pending_commits: 0,
                    workers: 0,
                    at_prompt: false,
                })
            }
        }
    }

    pub fn input(&self, bytes: &[u8]) -> Result<(), u32> {
        let mut host = self.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        if self
            .observed
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .closing
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if host.exit_code().is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        host.send_input(bytes)
            .map_err(|_| contract::MC_HOST_KERNEL_ERROR)
    }

    pub fn resize(&self, cols: i32, rows: i32) -> Result<(), u32> {
        let mut host = self.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        if self
            .observed
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .closing
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if host.exit_code().is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        host.resize_terminal(cols, rows)
            .map_err(|_| contract::MC_HOST_KERNEL_ERROR)
    }

    pub fn drive(&self, max_ticks: u32, stop_mask: u64) -> Result<VmDriveResult, u32> {
        if max_ticks == 0 || max_ticks > MAX_DRIVE_TICKS {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if stop_mask & !(contract::MC_HOST_DRIVE_ON_OUTPUT | contract::MC_HOST_DRIVE_ON_OPERATION)
            != 0
        {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        let mut host = self.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        {
            let observed = self
                .observed
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if observed.closing {
                return Err(contract::MC_HOST_INVALID_STATE);
            }
            if observed.poisoned {
                return Err(contract::MC_HOST_POISONED);
            }
        }
        let mut ticks = 0;
        let mut stop_reason = contract::MC_HOST_DRIVE_STOP_BUDGET;
        for _ in 0..max_ticks {
            if host.exit_code().is_some() {
                stop_reason = contract::MC_HOST_DRIVE_STOP_EXITED;
                break;
            }
            let output = self.output.stats();
            if output.poisoned {
                return Err(contract::MC_HOST_OUTPUT_LOST);
            }
            if output.above_high_water {
                stop_reason = contract::MC_HOST_DRIVE_STOP_OUTPUT;
                break;
            }
            if stop_mask & contract::MC_HOST_DRIVE_ON_OUTPUT != 0 && output.record_count != 0 {
                stop_reason = contract::MC_HOST_DRIVE_STOP_OUTPUT;
                break;
            }
            if stop_mask & contract::MC_HOST_DRIVE_ON_OPERATION != 0
                && self.operation_readiness()?.0
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_OPERATION;
                break;
            }
            let tick = match host.tick() {
                Ok(tick) => tick,
                Err(_) => {
                    self.observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?
                        .poisoned = true;
                    return Err(contract::MC_HOST_GUEST_TRAP);
                }
            };
            ticks += 1;
            for operation in self.live_operations()? {
                if operation.poll_exec(host).is_err() {
                    self.observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?
                        .poisoned = true;
                    return Err(contract::MC_HOST_KERNEL_ERROR);
                }
            }
            let mut observed = self
                .observed
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            observed.last_tick = match tick {
                TickState::Runnable => contract::MC_HOST_TICK_RUNNABLE,
                TickState::Waiting => contract::MC_HOST_TICK_WAITING,
                TickState::Exited => contract::MC_HOST_TICK_EXITED,
            };
            observed.exit_code = host.exit_code();
            drop(observed);
            if tick == TickState::Exited {
                stop_reason = contract::MC_HOST_DRIVE_STOP_EXITED;
                break;
            }
            if stop_mask & contract::MC_HOST_DRIVE_ON_OUTPUT != 0
                && self.output.stats().record_count != 0
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_OUTPUT;
                break;
            }
            if stop_mask & contract::MC_HOST_DRIVE_ON_OPERATION != 0
                && self.operation_readiness()?.0
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_OPERATION;
                break;
            }
            if tick == TickState::Waiting {
                stop_reason = contract::MC_HOST_DRIVE_STOP_WAITING;
                break;
            }
        }
        Ok(VmDriveResult {
            ticks,
            stop_reason,
            report: self.report(host)?,
        })
    }
}

struct Resources {
    kernels: HandleTable<KernelRecord>,
    kernel_bytes: usize,
    vms: HandleTable<Arc<VmRecord>>,
    outputs: HandleTable<OutputLease>,
    baselines: HandleTable<Arc<SnapshotRecord>>,
    snapshots: HandleTable<Arc<SnapshotRecord>>,
    operations: HandleTable<Arc<OperationRecord>>,
    snapshot_bytes: usize,
}

struct OutputLease {
    vm: u64,
    queue: Arc<OutputQueue>,
    record: OutputRecord,
}

pub struct RuntimeState {
    namespace: u16,
    pub cache: Arc<KernelModuleCache>,
    resources: Mutex<Resources>,
}

impl RuntimeState {
    pub fn new(namespace: u16) -> Self {
        Self {
            namespace,
            cache: Arc::new(KernelModuleCache::bounded(MAX_KERNELS, MAX_KERNEL_BYTES)),
            resources: Mutex::new(Resources {
                kernels: HandleTable::new(KERNEL_KIND),
                kernel_bytes: 0,
                vms: HandleTable::new(VM_KIND),
                outputs: HandleTable::new(OUTPUT_KIND),
                baselines: HandleTable::new(BASELINE_KIND),
                snapshots: HandleTable::new(SNAPSHOT_KIND),
                operations: HandleTable::new(OPERATION_KIND),
                snapshot_bytes: 0,
            }),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.resources.lock().is_ok_and(|resources| {
            resources.kernels.live == 0
                && resources.vms.live == 0
                && resources.outputs.live == 0
                && resources.baselines.live == 0
                && resources.snapshots.live == 0
                && resources.operations.live == 0
        })
    }

    pub fn load_kernel(&self, wasm: &[u8]) -> Result<(u64, [u8; 32]), u32> {
        if wasm.is_empty() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if wasm.len() > MAX_KERNEL_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        {
            let resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.kernels.live >= MAX_KERNELS
                || resources
                    .kernel_bytes
                    .checked_add(wasm.len())
                    .is_none_or(|total| total > MAX_KERNEL_BYTES)
            {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
        }
        // A runtime call reference protects this runtime while compilation runs.
        // Hold no resource table lock across Wasmtime compilation.
        let digest = self.cache.load(wasm).map_err(|error| {
            if error.is::<KernelCacheLimit>() {
                contract::MC_HOST_RESOURCE_LIMIT
            } else {
                contract::MC_HOST_KERNEL_ERROR
            }
        })?;
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if resources.kernels.live >= MAX_KERNELS
            || resources
                .kernel_bytes
                .checked_add(wasm.len())
                .is_none_or(|total| total > MAX_KERNEL_BYTES)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let record = KernelRecord {
            bytes: Arc::from(wasm),
            digest,
            pins: 0,
        };
        let handle = resources.kernels.insert(self.namespace, record)?;
        resources.kernel_bytes += wasm.len();
        Ok((handle, digest))
    }

    pub fn close_kernel(&self, handle: u64) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if resources.kernels.get(self.namespace, handle)?.pins != 0 {
            return Err(contract::MC_HOST_BUSY);
        }
        let record = resources.kernels.remove(self.namespace, handle)?;
        resources.kernel_bytes -= record.bytes.len();
        Ok(())
    }

    fn unpin_kernel(&self, handle: u64) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let record = resources.kernels.get_mut(self.namespace, handle)?;
        record.pins = record
            .pins
            .checked_sub(1)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        Ok(())
    }

    pub fn create_vm(&self, kernel_handle: u64, config: VmConfig) -> Result<u64, u32> {
        if config.layers.len() > 64
            || config
                .layers
                .iter()
                .try_fold(0usize, |total, layer| total.checked_add(layer.len()))
                .is_none_or(|total| total > MAX_IMAGE_BYTES)
            || config.requested_workers.is_some_and(|workers| workers < 0)
            || !(0..=4).contains(&config.contract_tier)
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let kernel_bytes = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= MAX_VMS {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            let record = resources.kernels.get_mut(self.namespace, kernel_handle)?;
            record.pins = record
                .pins
                .checked_add(1)
                .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
            Arc::clone(&record.bytes)
        };

        let output = OutputQueue::new(OUTPUT_MAX_BYTES, OUTPUT_HIGH_WATER, OUTPUT_MAX_RECORDS);
        let mut builder = KernelHostBuilder::new(kernel_bytes.to_vec())
            .with_module_cache(Arc::clone(&self.cache))
            .with_layers(config.layers)
            .with_contract(
                config.contract_tier,
                config.contract_budget_mib,
                config.contract_fuel,
            )
            .with_stdout(Box::new(output.sink(STDOUT)))
            .with_stderr(Box::new(output.sink(STDERR)))
            .with_log(Box::new(output.sink(LOG)));
        if config.deterministic {
            builder = builder.deterministic();
        }
        if let Some(workers) = config.requested_workers {
            builder = builder.with_workers(workers);
        }
        let mut host = match builder.build_pre_prompt() {
            Ok(host) => host,
            Err(_) => {
                self.unpin_kernel(kernel_handle)?;
                return Err(contract::MC_HOST_KERNEL_ERROR);
            }
        };
        if let Some((cols, rows)) = config.terminal_size {
            if host.resize_terminal(cols, rows).is_err() {
                self.unpin_kernel(kernel_handle)?;
                return Err(contract::MC_HOST_KERNEL_ERROR);
            }
        }
        let exit_code = host.exit_code();
        let vm = Arc::new(VmRecord {
            kernel_handle,
            host: Mutex::new(Some(host)),
            output,
            observed: Mutex::new(VmObserved {
                last_tick: if exit_code.is_some() {
                    contract::MC_HOST_TICK_EXITED
                } else {
                    contract::MC_HOST_TICK_RUNNABLE
                },
                exit_code,
                poisoned: false,
                closing: false,
            }),
            operations: Mutex::new(Vec::new()),
            _snapshot_anchor: None,
        });
        let inserted = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= MAX_VMS {
                Err(contract::MC_HOST_RESOURCE_LIMIT)
            } else {
                resources.vms.insert(self.namespace, vm)
            }
        };
        if inserted.is_err() {
            self.unpin_kernel(kernel_handle)?;
        }
        inserted
    }

    pub fn restore_vm(
        &self,
        kernel_handle: u64,
        snapshot: &[u8],
        baseline: Option<&[u8]>,
    ) -> Result<u64, u32> {
        if snapshot.is_empty()
            || snapshot.len() > 512 * 1024 * 1024
            || baseline.is_some_and(|base| base.is_empty() || base.len() > 512 * 1024 * 1024)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let transient_bytes = snapshot
            .len()
            .checked_add(baseline.map_or(0, |bytes| bytes.len()))
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        let retained_bytes = baseline.map_or(snapshot.len(), |bytes| bytes.len());
        let kernel_bytes = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= MAX_VMS {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            if resources
                .snapshot_bytes
                .checked_add(transient_bytes)
                .is_none_or(|total| total > SNAPSHOT_TOTAL_BYTES)
            {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            let record = resources.kernels.get_mut(self.namespace, kernel_handle)?;
            record.pins = record
                .pins
                .checked_add(1)
                .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
            let bytes = Arc::clone(&record.bytes);
            resources.snapshot_bytes += transient_bytes;
            bytes
        };
        let snapshot: Arc<[u8]> = Arc::from(snapshot);
        let baseline: Option<Arc<[u8]>> = baseline.map(Arc::from);
        let output = OutputQueue::new(OUTPUT_MAX_BYTES, OUTPUT_HIGH_WATER, OUTPUT_MAX_RECORDS);
        let builder = KernelHostBuilder::new(kernel_bytes.to_vec())
            .with_module_cache(Arc::clone(&self.cache))
            .with_stdout(Box::new(output.sink(STDOUT)))
            .with_stderr(Box::new(output.sink(STDERR)))
            .with_log(Box::new(output.sink(LOG)));
        let restored = match baseline.as_deref() {
            Some(base) => builder.restore_incremental(&snapshot, base),
            None => builder.restore(&snapshot),
        };
        let host = match restored {
            Ok(host) => host,
            Err(_) => {
                self.unpin_kernel(kernel_handle)?;
                self.release_snapshot_charge(transient_bytes)?;
                return Err(contract::MC_HOST_SNAPSHOT_ERROR);
            }
        };
        let exit_code = host.exit_code();
        let vm = Arc::new(VmRecord {
            kernel_handle,
            host: Mutex::new(Some(host)),
            output,
            observed: Mutex::new(VmObserved {
                last_tick: if exit_code.is_some() {
                    contract::MC_HOST_TICK_EXITED
                } else {
                    contract::MC_HOST_TICK_RUNNABLE
                },
                exit_code,
                poisoned: false,
                closing: false,
            }),
            operations: Mutex::new(Vec::new()),
            _snapshot_anchor: Some(baseline.unwrap_or(snapshot)),
        });
        let inserted = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= MAX_VMS {
                Err(contract::MC_HOST_RESOURCE_LIMIT)
            } else {
                resources.vms.insert(self.namespace, vm)
            }
        };
        if inserted.is_err() {
            self.unpin_kernel(kernel_handle)?;
            self.release_snapshot_charge(transient_bytes)?;
        } else {
            self.release_snapshot_charge(transient_bytes - retained_bytes)?;
        }
        inserted
    }

    fn release_snapshot_charge(&self, amount: usize) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        resources.snapshot_bytes = resources
            .snapshot_bytes
            .checked_sub(amount)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        Ok(())
    }

    pub fn import_baseline(
        &self,
        kernel_handle: u64,
        bytes: &[u8],
    ) -> Result<(u64, SnapshotDescription), u32> {
        if bytes.is_empty() || bytes.len() > SNAPSHOT_MAX_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let description =
            inspect_snapshot_bytes(bytes, None).map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
        if description.incremental {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if resources
            .snapshot_bytes
            .checked_add(bytes.len())
            .is_none_or(|total| total > SNAPSHOT_TOTAL_BYTES)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let kernel = resources.kernels.get_mut(self.namespace, kernel_handle)?;
        if kernel.digest != description.kernel_digest {
            return Err(contract::MC_HOST_SNAPSHOT_ERROR);
        }
        kernel.pins = kernel
            .pins
            .checked_add(1)
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        let record = Arc::new(SnapshotRecord {
            bytes: bytes.to_vec(),
            kernel_handle,
        });
        match resources.baselines.insert(self.namespace, record) {
            Ok(handle) => {
                resources.snapshot_bytes += bytes.len();
                Ok((handle, description))
            }
            Err(error) => {
                resources
                    .kernels
                    .get_mut(self.namespace, kernel_handle)?
                    .pins -= 1;
                Err(error)
            }
        }
    }

    pub fn baseline(&self, handle: u64) -> Result<Arc<SnapshotRecord>, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(Arc::clone(resources.baselines.get(self.namespace, handle)?))
    }

    pub fn close_baseline(&self, handle: u64) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let record = resources.baselines.remove(self.namespace, handle)?;
        resources.snapshot_bytes -= record.bytes.len();
        resources
            .kernels
            .get_mut(self.namespace, record.kernel_handle)?
            .pins -= 1;
        Ok(())
    }

    pub fn begin_snapshot(
        &self,
        vm_handle: u64,
        baseline_handle: Option<u64>,
    ) -> Result<(u64, SnapshotDescription), u32> {
        let vm = self.vm(vm_handle)?;
        let baseline = match baseline_handle {
            Some(handle) => {
                let record = self.baseline(handle)?;
                if record.kernel_handle != vm.kernel_handle {
                    return Err(contract::MC_HOST_SNAPSHOT_ERROR);
                }
                Some(record)
            }
            None => None,
        };
        let bytes = vm.snapshot(baseline.as_ref().map(|record| record.bytes.as_slice()))?;
        let description = inspect_snapshot_bytes(
            &bytes,
            baseline.as_ref().map(|record| record.bytes.as_slice()),
        )
        .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if resources
            .snapshot_bytes
            .checked_add(bytes.len())
            .is_none_or(|total| total > SNAPSHOT_TOTAL_BYTES)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let length = bytes.len();
        let record = Arc::new(SnapshotRecord {
            bytes,
            kernel_handle: vm.kernel_handle,
        });
        let handle = resources.snapshots.insert(self.namespace, record)?;
        resources.snapshot_bytes += length;
        Ok((handle, description))
    }

    pub fn snapshot_record(&self, handle: u64) -> Result<Arc<SnapshotRecord>, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(Arc::clone(resources.snapshots.get(self.namespace, handle)?))
    }

    pub fn close_snapshot(&self, handle: u64) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let record = resources.snapshots.remove(self.namespace, handle)?;
        resources.snapshot_bytes -= record.bytes.len();
        Ok(())
    }

    pub fn vm(&self, handle: u64) -> Result<Arc<VmRecord>, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(Arc::clone(resources.vms.get(self.namespace, handle)?))
    }

    pub fn start_exec(
        &self,
        vm_handle: u64,
        kind: u32,
        request: ctl_rust::ExecRequest,
    ) -> Result<u64, u32> {
        if (kind == contract::MC_HOST_OP_EXEC_SHELL && request.command.is_none())
            || (kind == contract::MC_HOST_OP_EXEC_DIRECT && request.argv.is_empty())
            || (kind != contract::MC_HOST_OP_EXEC_SHELL && kind != contract::MC_HOST_OP_EXEC_DIRECT)
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let vm = self.vm(vm_handle)?;
        let mut host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        let observed = vm.observed.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if observed.closing || observed.exit_code.is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if observed.poisoned {
            return Err(contract::MC_HOST_POISONED);
        }
        drop(observed);
        if vm.live_operations()?.len() >= MAX_OPERATIONS_PER_VM {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }

        let record = Arc::new(OperationRecord::new(vm_handle, kind));
        let handle = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            resources
                .operations
                .insert(self.namespace, Arc::clone(&record))?
        };
        vm.operations
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .push(Arc::downgrade(&record));
        let options = ExecOptions {
            cwd: request.cwd,
            env: request.env,
            stdin: request.stdin,
        };
        let started = if kind == contract::MC_HOST_OP_EXEC_SHELL {
            host.exec_start(
                request
                    .command
                    .as_deref()
                    .ok_or(contract::MC_HOST_INVALID_ARGUMENT)?,
                options,
            )
        } else {
            let mut args = request.argv.into_iter().map(|arg| arg.value);
            let program = args.next().ok_or(contract::MC_HOST_INVALID_ARGUMENT)?;
            host.run_start(&program, &args.collect::<Vec<_>>(), options)
        };
        match started {
            Ok(job) => {
                if let Err(error) = record.set_exec_job(job) {
                    let _ = host.exec_cancel(job);
                    self.remove_operation(handle)?;
                    return Err(error);
                }
                Ok(handle)
            }
            Err(_) => {
                self.remove_operation(handle)?;
                Err(contract::MC_HOST_KERNEL_ERROR)
            }
        }
    }

    fn remove_operation(&self, handle: u64) -> Result<(), u32> {
        let record = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            resources.operations.remove(self.namespace, handle)?
        };
        let vm = self.vm(record.vm)?;
        vm.operations
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .retain(|weak| {
                weak.upgrade()
                    .is_some_and(|active| !Arc::ptr_eq(&active, &record))
            });
        Ok(())
    }

    pub fn operation(&self, handle: u64) -> Result<Arc<OperationRecord>, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(Arc::clone(
            resources.operations.get(self.namespace, handle)?,
        ))
    }

    pub fn operation_peek(&self, handle: u64) -> Result<Vec<u8>, u32> {
        let record = self.operation(handle)?;
        let vm = self.vm(record.vm)?;
        let mut host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        let host = host.as_mut().ok_or(contract::MC_HOST_INVALID_STATE)?;
        record.peek_stdout(host)
    }

    pub fn operation_cancel(&self, handle: u64) -> Result<(), u32> {
        let record = self.operation(handle)?;
        let vm = self.vm(record.vm)?;
        let mut host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        match host.as_mut() {
            Some(host) => record.cancel(host),
            None => record.cancel_after_store_drop(),
        }
    }

    pub fn operation_close(&self, handle: u64) -> Result<(), u32> {
        let record = self.operation(handle)?;
        if record.pending()? {
            return Err(contract::MC_HOST_BUSY);
        }
        self.remove_operation(handle)
    }

    pub fn output_next(&self, vm_handle: u64) -> Result<Option<(u64, OutputRecord)>, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let vm = Arc::clone(resources.vms.get(self.namespace, vm_handle)?);
        if vm
            .observed
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .closing
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let Some(record) = vm.output.next() else {
            return Ok(None);
        };
        let lease = OutputLease {
            vm: vm_handle,
            queue: Arc::clone(&vm.output),
            record: record.clone(),
        };
        match resources.outputs.insert(self.namespace, lease) {
            Ok(handle) => Ok(Some((handle, record))),
            Err(error) => {
                vm.output.release(record.sequence());
                Err(error)
            }
        }
    }

    pub fn output_record(&self, handle: u64) -> Result<OutputRecord, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(resources
            .outputs
            .get(self.namespace, handle)?
            .record
            .clone())
    }

    pub fn output_complete(&self, handle: u64, ack: bool) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let (vm_handle, queue, sequence) = {
            let lease = resources.outputs.get(self.namespace, handle)?;
            (lease.vm, Arc::clone(&lease.queue), lease.record.sequence())
        };
        if !ack
            && resources
                .vms
                .get(self.namespace, vm_handle)?
                .observed
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?
                .closing
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let done = if ack {
            queue.ack(sequence)
        } else {
            queue.release(sequence)
        };
        if !done {
            return Err(contract::MC_HOST_INTERNAL);
        }
        resources.outputs.remove(self.namespace, handle)?;
        Ok(())
    }

    pub fn close_vm(&self, handle: u64) -> Result<(), u32> {
        let vm = self.vm(handle)?;
        let was_closing = {
            let mut observed = vm.observed.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
            let was_closing = observed.closing;
            observed.closing = true;
            was_closing
        };
        let host_guard = match vm.host.try_lock() {
            Ok(guard) => guard,
            Err(_) => {
                if !was_closing {
                    vm.observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?
                        .closing = false;
                }
                return Err(contract::MC_HOST_BUSY);
            }
        };
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if vm.output.stats().leased_records != 0 {
            if !was_closing {
                vm.observed
                    .lock()
                    .map_err(|_| contract::MC_HOST_INTERNAL)?
                    .closing = false;
            }
            return Err(contract::MC_HOST_BUSY);
        }
        if !vm.live_operations()?.is_empty() {
            if !was_closing {
                vm.observed
                    .lock()
                    .map_err(|_| contract::MC_HOST_INTERNAL)?
                    .closing = false;
            }
            return Err(contract::MC_HOST_BUSY);
        }
        resources.vms.remove(self.namespace, handle)?;
        let kernel = resources
            .kernels
            .get_mut(self.namespace, vm.kernel_handle)?;
        kernel.pins = kernel
            .pins
            .checked_sub(1)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        if let Some(anchor) = &vm._snapshot_anchor {
            resources.snapshot_bytes = resources
                .snapshot_bytes
                .checked_sub(anchor.len())
                .ok_or(contract::MC_HOST_INTERNAL)?;
        }
        drop(host_guard);
        Ok(())
    }

    pub fn cancel_and_close_vm(&self, handle: u64) -> Result<(), u32> {
        let vm = self.vm(handle)?;
        let mut host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        {
            let mut observed = vm.observed.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
            observed.closing = true;
        }
        host.take();
        for operation in vm.live_operations()? {
            operation.cancel_after_store_drop()?;
        }
        drop(host);
        self.close_vm(handle)
    }
}
