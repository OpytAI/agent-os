//! Runtime-owned kernel identities. Child resources use the same namespace,
//! kind, generation, and slot encoding as runtime handles.

use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, Weak};

use embedder_rust::{self as contract, EffectEvent};
use host::output_queue::{OutputQueue, OutputRecord, OutputStats, LOG, STDERR, STDOUT};
use host::{
    inspect_snapshot_bytes, ControlRetryAction, ExecOptions, HostToolDef, KernelCacheLimit,
    KernelHost, KernelHostBuilder, KernelModuleCache, NativeCatalogUpdate, SnapshotDescription,
    TickState,
};

use super::operations::OperationRecord;
use super::relay::{EffectBody, Relay};
use super::{pack_kind, unpack, MAX_GENERATION, MAX_INDEX};

pub const KERNEL_KIND: u8 = 2;
pub const VM_KIND: u8 = 3;
pub const OUTPUT_KIND: u8 = 4;
pub const BASELINE_KIND: u8 = 5;
pub const SNAPSHOT_KIND: u8 = 6;
pub const OPERATION_KIND: u8 = 7;
pub const PLAN_KIND: u8 = 8;
pub const EFFECT_KIND: u8 = 9;
pub const SOCKET_KIND: u8 = 10;
const MAX_KERNELS: usize = 64;
const MAX_KERNEL_BYTES: usize = 256 * 1024 * 1024;
const MAX_VMS: usize = contract::MC_HOST_HARD_MAX_VMS as usize;
const MAX_IMAGE_BYTES: usize = 256 * 1024 * 1024;
const MAX_DRIVE_TICKS: u32 = contract::MC_HOST_HARD_MAX_DRIVE_TICKS;
const MAX_PLANS: usize = 64;

#[derive(Clone, Copy)]
pub struct RuntimeLimits {
    pub max_vms: usize,
    pub max_drive_ticks: u32,
    pub max_output_bytes: usize,
    pub max_output_records: usize,
    pub max_snapshot_bytes: usize,
    pub max_snapshot_total_bytes: usize,
    pub max_operations_per_vm: usize,
}

impl RuntimeLimits {
    pub fn new(
        max_vms: u32,
        max_drive_ticks: u32,
        max_output_bytes: u32,
        max_output_records: u32,
        max_snapshot_bytes: u32,
        max_snapshot_total_bytes: u32,
        max_operations_per_vm: u32,
    ) -> Result<Self, u32> {
        if max_vms > MAX_VMS as u32
            || max_drive_ticks > MAX_DRIVE_TICKS
            || max_output_bytes > contract::MC_HOST_HARD_MAX_OUTPUT_BYTES
            || max_output_records > contract::MC_HOST_HARD_MAX_OUTPUT_RECORDS
            || max_snapshot_bytes > contract::MC_HOST_HARD_MAX_SNAPSHOT_BYTES
            || max_snapshot_total_bytes > contract::MC_HOST_HARD_MAX_SNAPSHOT_TOTAL_BYTES
            || max_operations_per_vm > contract::MC_HOST_HARD_MAX_OPERATIONS_PER_VM
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        if (max_output_bytes != 0 && max_output_bytes < 4)
            || (max_output_records != 0 && max_output_records < 4)
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        Ok(Self {
            max_vms: if max_vms == 0 {
                contract::MC_HOST_DEFAULT_MAX_VMS as usize
            } else {
                max_vms as usize
            },
            max_drive_ticks: if max_drive_ticks == 0 {
                contract::MC_HOST_DEFAULT_MAX_DRIVE_TICKS
            } else {
                max_drive_ticks
            },
            max_output_bytes: if max_output_bytes == 0 {
                contract::MC_HOST_DEFAULT_MAX_OUTPUT_BYTES as usize
            } else {
                max_output_bytes as usize
            },
            max_output_records: if max_output_records == 0 {
                contract::MC_HOST_DEFAULT_MAX_OUTPUT_RECORDS as usize
            } else {
                max_output_records as usize
            },
            max_snapshot_bytes: if max_snapshot_bytes == 0 {
                contract::MC_HOST_DEFAULT_MAX_SNAPSHOT_BYTES as usize
            } else {
                max_snapshot_bytes as usize
            },
            max_snapshot_total_bytes: if max_snapshot_total_bytes == 0 {
                contract::MC_HOST_DEFAULT_MAX_SNAPSHOT_TOTAL_BYTES as usize
            } else {
                max_snapshot_total_bytes as usize
            },
            max_operations_per_vm: if max_operations_per_vm == 0 {
                contract::MC_HOST_DEFAULT_MAX_OPERATIONS_PER_VM as usize
            } else {
                max_operations_per_vm as usize
            },
        })
    }
}

#[derive(Clone, Default)]
struct PlanRecord {
    routes: BTreeMap<String, String>,
    host_tools: Vec<HostToolDef>,
    persist_identity: Option<String>,
    http_identity: Option<String>,
    sealed: bool,
}

struct EffectRecord {
    vm: u64,
    event_id: u64,
    kind: u32,
    socket: Option<u64>,
}

struct SocketRecord {
    vm: u64,
    bridge_handle: i32,
}

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
    relay: Option<Arc<Relay>>,
    max_drive_ticks: u32,
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

pub struct VmSnapshotReadiness {
    pub inflight_egress: bool,
    pub pending_commits: bool,
    pub pending_operations: bool,
    pub pending_effects: bool,
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
    pub effect_ready: bool,
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
    pub fn snapshot_readiness(&self) -> Result<VmSnapshotReadiness, u32> {
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
        Ok(VmSnapshotReadiness {
            inflight_egress: host
                .inflight_egress()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?
                != 0,
            pending_commits: host
                .pending_commits()
                .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?
                != 0,
            pending_operations: self.operation_readiness()?.1,
            pending_effects: self
                .relay
                .as_ref()
                .is_some_and(|relay| relay.has_snapshot_blockers()),
        })
    }

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

    pub fn snapshot(&self, baseline: Option<&[u8]>, max_bytes: usize) -> Result<Vec<u8>, u32> {
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
        if self
            .relay
            .as_ref()
            .is_some_and(|relay| relay.has_snapshot_blockers())
        {
            return Err(contract::MC_HOST_NOT_QUIESCENT);
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
        if baseline.is_none() && host.snapshot_len() > max_bytes {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let bytes = match baseline {
            Some(base) => host
                .snapshot_incremental_bounded(base, max_bytes)
                .map_err(|error| {
                    if error
                        .downcast_ref::<host::SnapshotLimitExceeded>()
                        .is_some()
                    {
                        contract::MC_HOST_RESOURCE_LIMIT
                    } else {
                        contract::MC_HOST_SNAPSHOT_ERROR
                    }
                })?,
            None => {
                let mut bytes = vec![0; host.snapshot_len()];
                host.snapshot_into(&mut bytes)
                    .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
                bytes
            }
        };
        if bytes.len() > max_bytes {
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
            effect_ready: self.relay.as_ref().is_some_and(|relay| relay.ready()),
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
                    effect_ready: self.relay.as_ref().is_some_and(|relay| relay.ready()),
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
        if max_ticks == 0 || max_ticks > self.max_drive_ticks {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if stop_mask
            & !(contract::MC_HOST_DRIVE_ON_OUTPUT
                | contract::MC_HOST_DRIVE_ON_OPERATION
                | contract::MC_HOST_DRIVE_ON_EFFECT)
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
            if self
                .relay
                .as_ref()
                .is_some_and(|relay| relay.above_high_water())
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_EFFECT;
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
            if stop_mask & contract::MC_HOST_DRIVE_ON_EFFECT != 0
                && self.relay.as_ref().is_some_and(|relay| relay.ready())
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_EFFECT;
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
            let operations = self.live_operations()?;
            for operation in &operations {
                if operation.poll_exec(host).is_err() {
                    self.observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?
                        .poisoned = true;
                    return Err(contract::MC_HOST_KERNEL_ERROR);
                }
                if operation.poll_service(host).is_err() {
                    self.observed
                        .lock()
                        .map_err(|_| contract::MC_HOST_INTERNAL)?
                        .poisoned = true;
                    return Err(contract::MC_HOST_KERNEL_ERROR);
                }
            }
            for operation in operations {
                if operation.retry_waiting()? {
                    if operation.advance_retry(host).is_err() {
                        self.observed
                            .lock()
                            .map_err(|_| contract::MC_HOST_INTERNAL)?
                            .poisoned = true;
                        return Err(contract::MC_HOST_KERNEL_ERROR);
                    }
                    break;
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
            if stop_mask & contract::MC_HOST_DRIVE_ON_EFFECT != 0
                && self.relay.as_ref().is_some_and(|relay| relay.ready())
            {
                stop_reason = contract::MC_HOST_DRIVE_STOP_EFFECT;
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
    plans: HandleTable<PlanRecord>,
    effects: HandleTable<EffectRecord>,
    sockets: HandleTable<SocketRecord>,
    snapshot_bytes: usize,
}

struct OutputLease {
    vm: u64,
    queue: Arc<OutputQueue>,
    record: OutputRecord,
}

pub struct RuntimeState {
    namespace: u16,
    limits: RuntimeLimits,
    pub cache: Arc<KernelModuleCache>,
    resources: Mutex<Resources>,
}

struct SnapshotCaptureReservation<'a> {
    runtime: &'a RuntimeState,
    reserved: usize,
}

impl Drop for SnapshotCaptureReservation<'_> {
    fn drop(&mut self) {
        if self.reserved != 0 {
            let mut resources = self
                .runtime
                .resources
                .lock()
                .unwrap_or_else(|error| error.into_inner());
            resources.snapshot_bytes -= self.reserved;
        }
    }
}

impl RuntimeState {
    pub fn new(namespace: u16, limits: RuntimeLimits) -> Self {
        Self {
            namespace,
            limits,
            cache: Arc::new(KernelModuleCache::bounded(MAX_KERNELS, MAX_KERNEL_BYTES)),
            resources: Mutex::new(Resources {
                kernels: HandleTable::new(KERNEL_KIND),
                kernel_bytes: 0,
                vms: HandleTable::new(VM_KIND),
                outputs: HandleTable::new(OUTPUT_KIND),
                baselines: HandleTable::new(BASELINE_KIND),
                snapshots: HandleTable::new(SNAPSHOT_KIND),
                operations: HandleTable::new(OPERATION_KIND),
                plans: HandleTable::new(PLAN_KIND),
                effects: HandleTable::new(EFFECT_KIND),
                sockets: HandleTable::new(SOCKET_KIND),
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
                && resources.plans.live == 0
                && resources.effects.live == 0
                && resources.sockets.live == 0
        })
    }

    pub fn create_plan(&self) -> Result<u64, u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        if resources.plans.live >= MAX_PLANS {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        resources
            .plans
            .insert(self.namespace, PlanRecord::default())
    }

    pub fn add_host_call_route(
        &self,
        plan: u64,
        name: String,
        identity: String,
    ) -> Result<(), u32> {
        if name.is_empty() || name.contains('\0') || identity.is_empty() || identity.contains('\0')
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let plan = resources.plans.get_mut(self.namespace, plan)?;
        if plan.sealed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if plan.routes.len() >= 128 || plan.routes.contains_key(&name) {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        plan.routes.insert(name, identity);
        Ok(())
    }

    pub fn add_host_tool(&self, plan: u64, tool: HostToolDef) -> Result<(), u32> {
        if tool.address.is_empty()
            || tool.binding_name.is_empty()
            || (tool.args_mode != "json" && tool.args_mode != "raw")
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let plan = resources.plans.get_mut(self.namespace, plan)?;
        if plan.sealed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if plan.host_tools.len() >= 256
            || plan
                .host_tools
                .iter()
                .any(|existing| existing.address == tool.address)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        plan.host_tools.push(tool);
        Ok(())
    }

    pub fn add_persist_provider(&self, plan: u64, identity: String) -> Result<(), u32> {
        if identity.is_empty() || identity.contains('\0') {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let plan = resources.plans.get_mut(self.namespace, plan)?;
        if plan.sealed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if plan.persist_identity.is_some() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        plan.persist_identity = Some(identity);
        Ok(())
    }

    pub fn add_http_application(&self, plan: u64, identity: String) -> Result<(), u32> {
        if identity.is_empty() || identity.contains('\0') {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let plan = resources.plans.get_mut(self.namespace, plan)?;
        if plan.sealed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if plan.http_identity.is_some() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        plan.http_identity = Some(identity);
        Ok(())
    }

    pub fn seal_plan(&self, plan: u64) -> Result<(), u32> {
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        resources.plans.get_mut(self.namespace, plan)?.sealed = true;
        Ok(())
    }

    pub fn close_plan(&self, plan: u64) -> Result<(), u32> {
        self.resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .plans
            .remove(self.namespace, plan)?;
        Ok(())
    }

    fn plan_capabilities(&self, plan: u64) -> Result<Option<PlanRecord>, u32> {
        if plan == 0 {
            return Ok(None);
        }
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let plan = resources.plans.get(self.namespace, plan)?;
        if !plan.sealed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        Ok(Some(plan.clone()))
    }

    pub fn effect_next(&self, vm_handle: u64, mask: u64) -> Result<(u64, u32, u64), u32> {
        let vm = self.vm(vm_handle)?;
        let relay = vm.relay.as_ref().ok_or(contract::MC_HOST_UNSUPPORTED)?;
        let leased = relay.next(mask)?.ok_or(contract::MC_HOST_WOULD_BLOCK)?;
        let prepared = (|| -> Result<(u64, Option<u64>, bool), u32> {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            let mut new_socket = false;
            let socket_handle = if let Some(bridge_handle) = leased.socket {
                match relay.socket_native_handle(bridge_handle)? {
                    Some(handle) => {
                        if resources.sockets.get(self.namespace, handle)?.vm != vm_handle {
                            return Err(contract::MC_HOST_STALE_HANDLE);
                        }
                        Some(handle)
                    }
                    None if leased.kind == contract::MC_HOST_EVENT_WS_CONNECT => {
                        new_socket = true;
                        Some(resources.sockets.insert(
                            self.namespace,
                            SocketRecord {
                                vm: vm_handle,
                                bridge_handle,
                            },
                        )?)
                    }
                    None => return Err(contract::MC_HOST_INTERNAL),
                }
            } else {
                None
            };
            let inserted = resources.effects.insert(
                self.namespace,
                EffectRecord {
                    vm: vm_handle,
                    event_id: leased.id,
                    kind: leased.kind,
                    socket: socket_handle,
                },
            );
            let handle = match inserted {
                Ok(handle) => handle,
                Err(error) => {
                    if new_socket {
                        resources
                            .sockets
                            .remove(self.namespace, socket_handle.expect("new socket"))?;
                    }
                    return Err(error);
                }
            };
            Ok((handle, socket_handle, new_socket))
        })();
        let (handle, socket_handle, new_socket) = match prepared {
            Ok(value) => value,
            Err(error) => {
                relay.unlease(leased.id);
                return Err(error);
            }
        };
        if let Err(error) = relay.bind_handle(leased.id, handle, socket_handle) {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            resources.effects.remove(self.namespace, handle)?;
            if new_socket {
                resources
                    .sockets
                    .remove(self.namespace, socket_handle.expect("new socket"))?;
            }
            relay.unlease(leased.id);
            return Err(error);
        }
        let frame = EffectEvent {
            kind: leased.kind,
            event_id: handle as i64,
            vm_id: vm_handle as i64,
            operation_id: None,
            socket_id: socket_handle.map(|handle| handle as i64),
            flags: 0,
            payload: leased.payload,
        }
        .encode();
        Ok((handle, leased.kind, frame.len() as u64))
    }

    fn effect_record(&self, handle: u64) -> Result<(u64, u64, Arc<Relay>), u32> {
        let (vm_handle, event_id) = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            let record = resources.effects.get(self.namespace, handle)?;
            (record.vm, record.event_id)
        };
        let vm = self.vm(vm_handle)?;
        let relay = Arc::clone(vm.relay.as_ref().ok_or(contract::MC_HOST_INTERNAL)?);
        Ok((vm_handle, event_id, relay))
    }

    pub fn effect_frame(&self, handle: u64) -> Result<Vec<u8>, u32> {
        let (vm, event_id, relay) = self.effect_record(handle)?;
        let (kind, payload) = relay.event(event_id, handle)?;
        let socket = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .effects
            .get(self.namespace, handle)?
            .socket;
        Ok(EffectEvent {
            kind,
            event_id: handle as i64,
            vm_id: vm as i64,
            operation_id: None,
            socket_id: socket.map(|handle| handle as i64),
            flags: 0,
            payload,
        }
        .encode())
    }

    pub fn effect_release(&self, handle: u64) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.release(event_id, handle)?;
        self.resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .effects
            .remove(self.namespace, handle)?;
        Ok(())
    }

    pub fn effect_respond(&self, handle: u64, kind: u32, response: EffectBody) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.respond(event_id, handle, kind, response)?;
        self.resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .effects
            .remove(self.namespace, handle)?;
        Ok(())
    }

    pub fn effect_response_begin(
        &self,
        handle: u64,
        kind: u32,
        response: EffectBody,
        declared_body_len: usize,
    ) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.response_begin(event_id, handle, kind, response, declared_body_len)
    }

    pub fn effect_response_write(&self, handle: u64, chunk: &[u8]) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.response_write(event_id, handle, chunk)
    }

    pub fn effect_response_finish(&self, handle: u64) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.response_finish(event_id, handle)?;
        self.resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .effects
            .remove(self.namespace, handle)?;
        Ok(())
    }

    pub fn effect_response_abort(&self, handle: u64) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        relay.response_abort(event_id, handle)
    }

    pub fn effect_ack(&self, handle: u64) -> Result<(), u32> {
        let (_, event_id, relay) = self.effect_record(handle)?;
        let (kind, socket) = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            let record = resources.effects.get(self.namespace, handle)?;
            (record.kind, record.socket)
        };
        if kind == contract::MC_HOST_EVENT_WS_CLOSE {
            relay.ack_socket_close(event_id, handle)?;
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            resources.effects.remove(self.namespace, handle)?;
            resources
                .sockets
                .remove(self.namespace, socket.ok_or(contract::MC_HOST_INTERNAL)?)?;
            return Ok(());
        }
        let original_handle = relay.ack_cancelled(event_id, handle)?;
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        resources.effects.remove(self.namespace, handle)?;
        resources.effects.remove(self.namespace, original_handle)?;
        Ok(())
    }

    fn socket_record(&self, handle: u64) -> Result<(Arc<Relay>, i32), u32> {
        let (vm_handle, bridge_handle) = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            let record = resources.sockets.get(self.namespace, handle)?;
            (record.vm, record.bridge_handle)
        };
        let vm = self.vm(vm_handle)?;
        Ok((
            Arc::clone(vm.relay.as_ref().ok_or(contract::MC_HOST_INTERNAL)?),
            bridge_handle,
        ))
    }

    pub fn socket_push(&self, handle: u64, data: &[u8]) -> Result<(), u32> {
        let (relay, bridge_handle) = self.socket_record(handle)?;
        relay.socket_push(bridge_handle, data)
    }

    pub fn socket_close(&self, handle: u64) -> Result<(), u32> {
        let (relay, bridge_handle) = self.socket_record(handle)?;
        relay.socket_close(bridge_handle)
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

    pub fn create_vm(
        &self,
        kernel_handle: u64,
        plan_handle: u64,
        config: VmConfig,
    ) -> Result<u64, u32> {
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
        let capabilities = self.plan_capabilities(plan_handle)?;
        let kernel_bytes = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= self.limits.max_vms {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            let record = resources.kernels.get_mut(self.namespace, kernel_handle)?;
            record.pins = record
                .pins
                .checked_add(1)
                .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
            Arc::clone(&record.bytes)
        };

        let output = OutputQueue::new(
            self.limits.max_output_bytes,
            self.limits.max_output_bytes * 3 / 4,
            self.limits.max_output_records,
        );
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
        let relay = capabilities.map(|plan| {
            Relay::new(
                plan.routes.into_keys().collect(),
                plan.persist_identity.is_some(),
                plan.http_identity.is_some(),
            )
        });
        if let Some(relay) = &relay {
            builder = builder.with_host_call(relay.adapter());
            if relay.persist_enabled() {
                builder = builder.with_persist(relay.persist_adapter());
            }
            if relay.http_enabled() {
                builder = builder.with_net(relay.net_adapter());
            }
        }
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
            relay,
            max_drive_ticks: self.limits.max_drive_ticks,
            _snapshot_anchor: None,
        });
        let inserted = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= self.limits.max_vms {
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
        plan_handle: u64,
        mode: u32,
        snapshot: &[u8],
        baseline: Option<&[u8]>,
    ) -> Result<u64, u32> {
        if snapshot.is_empty()
            || snapshot.len() > self.limits.max_snapshot_bytes
            || baseline
                .is_some_and(|base| base.is_empty() || base.len() > self.limits.max_snapshot_bytes)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let capabilities = if mode == contract::MC_HOST_RESTORE_STRICT {
            self.plan_capabilities(plan_handle)?
        } else {
            None
        };
        if capabilities.as_ref().is_some_and(|plan| {
            plan.host_tools
                .iter()
                .any(|tool| !plan.routes.contains_key(&tool.binding_name))
        }) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
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
            if resources.vms.live >= self.limits.max_vms {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            if resources
                .snapshot_bytes
                .checked_add(transient_bytes)
                .is_none_or(|total| total > self.limits.max_snapshot_total_bytes)
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
        let output = OutputQueue::new(
            self.limits.max_output_bytes,
            self.limits.max_output_bytes * 3 / 4,
            self.limits.max_output_records,
        );
        let mut builder = KernelHostBuilder::new(kernel_bytes.to_vec())
            .with_module_cache(Arc::clone(&self.cache))
            .with_stdout(Box::new(output.sink(STDOUT)))
            .with_stderr(Box::new(output.sink(STDERR)))
            .with_log(Box::new(output.sink(LOG)));
        let relay = capabilities.as_ref().map(|plan| {
            Relay::new(
                plan.routes.keys().cloned().collect(),
                plan.persist_identity.is_some(),
                plan.http_identity.is_some(),
            )
        });
        if let Some(relay) = &relay {
            builder = builder.with_host_call(relay.adapter());
            if relay.persist_enabled() {
                builder = builder.with_persist(relay.persist_adapter());
            }
            if relay.http_enabled() {
                builder = builder.with_net(relay.net_adapter());
            }
        }
        let restored = match baseline.as_deref() {
            Some(base) => builder.restore_incremental(&snapshot, base),
            None => builder.restore(&snapshot),
        };
        let mut host = match restored {
            Ok(host) => host,
            Err(_) => {
                self.unpin_kernel(kernel_handle)?;
                self.release_snapshot_charge(transient_bytes)?;
                return Err(contract::MC_HOST_SNAPSHOT_ERROR);
            }
        };
        let strict_valid = mode != contract::MC_HOST_RESTORE_STRICT
            || (host::validate_native_restored_catalog(
                &mut host,
                capabilities
                    .as_ref()
                    .map_or(&[], |plan| plan.host_tools.as_slice()),
            )
            .is_ok()
                && host::validate_native_restored_mounts(
                    &mut host,
                    &capabilities.as_ref().map_or_else(Default::default, |plan| {
                        plan.routes.keys().cloned().collect()
                    }),
                )
                .is_ok());
        if !strict_valid {
            self.unpin_kernel(kernel_handle)?;
            self.release_snapshot_charge(transient_bytes)?;
            return Err(contract::MC_HOST_INVALID_STATE);
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
            relay,
            max_drive_ticks: self.limits.max_drive_ticks,
            _snapshot_anchor: Some(baseline.unwrap_or(snapshot)),
        });
        let inserted = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            if resources.vms.live >= self.limits.max_vms {
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
        if bytes.is_empty() || bytes.len() > self.limits.max_snapshot_bytes {
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
            .is_none_or(|total| total > self.limits.max_snapshot_total_bytes)
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
        let mut reservation = {
            let mut resources = self
                .resources
                .lock()
                .map_err(|_| contract::MC_HOST_INTERNAL)?;
            let available = self
                .limits
                .max_snapshot_total_bytes
                .checked_sub(resources.snapshot_bytes)
                .ok_or(contract::MC_HOST_INTERNAL)?;
            if available == 0 {
                return Err(contract::MC_HOST_RESOURCE_LIMIT);
            }
            resources.snapshot_bytes += available;
            SnapshotCaptureReservation {
                runtime: self,
                reserved: available,
            }
        };
        let bytes = vm.snapshot(
            baseline.as_ref().map(|record| record.bytes.as_slice()),
            self.limits.max_snapshot_bytes.min(reservation.reserved),
        )?;
        let description = inspect_snapshot_bytes(
            &bytes,
            baseline.as_ref().map(|record| record.bytes.as_slice()),
        )
        .map_err(|_| contract::MC_HOST_SNAPSHOT_ERROR)?;
        let mut resources = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?;
        let length = bytes.len();
        let record = Arc::new(SnapshotRecord {
            bytes,
            kernel_handle: vm.kernel_handle,
        });
        let handle = resources.snapshots.insert(self.namespace, record)?;
        resources.snapshot_bytes -= reservation.reserved - length;
        reservation.reserved = 0;
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
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
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

    pub fn start_service(
        &self,
        vm_handle: u64,
        kind: u32,
        request: ctl_rust::SvcRequest,
    ) -> Result<u64, u32> {
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
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
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
        match host.service_start(&request.service, &request.request) {
            Ok(job) => {
                if let Err(error) = record.set_service_job(job) {
                    let _ = host.service_cancel(job);
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

    pub fn start_commit(&self, vm_handle: u64) -> Result<u64, u32> {
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
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
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
        let record = Arc::new(OperationRecord::new(
            vm_handle,
            contract::MC_HOST_OP_COMMIT_LAYER,
        ));
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
        match host.commit_layer() {
            Ok((layer, digest)) => {
                let payload = contract::LayerCommitResult { digest, layer }.encode();
                record.complete(0, payload)?;
                Ok(handle)
            }
            Err(_) => {
                self.remove_operation(handle)?;
                Err(contract::MC_HOST_KERNEL_ERROR)
            }
        }
    }

    pub fn start_mount(
        &self,
        vm_handle: u64,
        path: &str,
        read_only: Option<bool>,
    ) -> Result<u64, u32> {
        if path.is_empty() || !path.starts_with('/') || path.contains('\0') {
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
        if read_only.is_some() && !vm.relay.as_ref().is_some_and(|relay| relay.has_route(path)) {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let kind = if read_only.is_some() {
            contract::MC_HOST_OP_MOUNT
        } else {
            contract::MC_HOST_OP_UNMOUNT
        };
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
        let result = match read_only {
            Some(read_only) => host.mount(path, read_only),
            None => host.unmount(path),
        };
        match result {
            Ok(()) => {
                record.complete(0, Vec::new())?;
                Ok(handle)
            }
            Err(_) => {
                self.remove_operation(handle)?;
                Err(contract::MC_HOST_KERNEL_ERROR)
            }
        }
    }

    pub fn start_retry(
        &self,
        vm_handle: u64,
        kind: u32,
        action: ControlRetryAction,
    ) -> Result<u64, u32> {
        let vm = self.vm(vm_handle)?;
        let host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        if host.is_none() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let observed = vm.observed.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if observed.closing || observed.exit_code.is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if observed.poisoned {
            return Err(contract::MC_HOST_POISONED);
        }
        drop(observed);
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let record = Arc::new(OperationRecord::new_retry(vm_handle, kind, action));
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
        Ok(handle)
    }

    pub fn start_catalog(&self, vm_handle: u64, update: NativeCatalogUpdate) -> Result<u64, u32> {
        let vm = self.vm(vm_handle)?;
        let host = vm.host.try_lock().map_err(|_| contract::MC_HOST_BUSY)?;
        if host.is_none() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let observed = vm.observed.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if observed.closing || observed.exit_code.is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if observed.poisoned {
            return Err(contract::MC_HOST_POISONED);
        }
        drop(observed);
        if update
            .binding_names
            .iter()
            .any(|name| !vm.relay.as_ref().is_some_and(|relay| relay.has_route(name)))
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if vm.live_operations()?.len() >= self.limits.max_operations_per_vm {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let record = Arc::new(OperationRecord::new_catalog(vm_handle, update));
        let handle = self
            .resources
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .operations
            .insert(self.namespace, Arc::clone(&record))?;
        vm.operations
            .lock()
            .map_err(|_| contract::MC_HOST_INTERNAL)?
            .push(Arc::downgrade(&record));
        Ok(handle)
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
        if vm
            .relay
            .as_ref()
            .is_some_and(|relay| relay.has_live_events())
        {
            if !was_closing {
                vm.observed
                    .lock()
                    .map_err(|_| contract::MC_HOST_INTERNAL)?
                    .closing = false;
            }
            return Err(contract::MC_HOST_BUSY);
        }
        if resources.effects.slots.iter().any(|slot| {
            slot.value
                .as_ref()
                .is_some_and(|effect| effect.vm == handle)
        }) {
            if !was_closing {
                vm.observed
                    .lock()
                    .map_err(|_| contract::MC_HOST_INTERNAL)?
                    .closing = false;
            }
            return Err(contract::MC_HOST_BUSY);
        }
        if resources.sockets.slots.iter().any(|slot| {
            slot.value
                .as_ref()
                .is_some_and(|socket| socket.vm == handle)
        }) {
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
        if let Some(relay) = &vm.relay {
            relay.cancel_all();
        }
        for operation in vm.live_operations()? {
            operation.cancel_after_store_drop()?;
        }
        drop(host);
        self.close_vm(handle)
    }
}
