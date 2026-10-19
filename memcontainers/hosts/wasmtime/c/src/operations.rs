//! Operation-owned terminal results. Polling reads kernel jobs but never ticks.

use std::sync::{Arc, Mutex};

use ctl_rust::{ExecOutcome, SvcResponse};
use embedder_rust::{self as contract, OperationResult};
use host::{ControlAttempt, ControlRetryAction, KernelHost, NativeCatalogUpdate};

fn result_overhead() -> usize {
    OperationResult::default().encode().len()
}

pub struct OperationRecord {
    pub vm: u64,
    pub kind: u32,
    max_result_bytes: usize,
    state: Mutex<OperationState>,
}

enum OperationState {
    New,
    PendingExec(i32),
    PendingService(i32),
    NewRetry(ControlRetryAction),
    PendingRetry(ControlRetryAction),
    CatalogStage(CatalogStage),
    Ready(Arc<[u8]>),
    Failed(u32),
    Cancelled,
}

struct CatalogStage {
    actions: Vec<ControlRetryAction>,
    cursor: usize,
    service_request: Vec<u8>,
}

#[derive(Clone, Copy)]
pub struct OperationObserved {
    pub kind: u32,
    pub state: u32,
    pub error: u32,
    pub result_length: u64,
}

impl OperationRecord {
    pub fn new(vm: u64, kind: u32, max_result_bytes: usize) -> Self {
        Self {
            vm,
            kind,
            max_result_bytes,
            state: Mutex::new(OperationState::New),
        }
    }

    pub fn new_retry(
        vm: u64,
        kind: u32,
        action: ControlRetryAction,
        max_result_bytes: usize,
    ) -> Self {
        Self {
            vm,
            kind,
            max_result_bytes,
            state: Mutex::new(OperationState::NewRetry(action)),
        }
    }

    pub fn new_catalog(vm: u64, update: NativeCatalogUpdate, max_result_bytes: usize) -> Self {
        let mut actions = [
            "/etc",
            "/etc/tools",
            "/etc/tools/catalog",
            "/etc/tools/catalog/records",
        ]
        .into_iter()
        .map(|path| ControlRetryAction::Mkdir(path.to_owned()))
        .collect::<Vec<_>>();
        actions.extend(
            update
                .records
                .into_iter()
                .map(|(sha, data)| ControlRetryAction::Write {
                    path: format!("/etc/tools/catalog/records/{sha}"),
                    data,
                }),
        );
        Self {
            vm,
            kind: contract::MC_HOST_OP_CATALOG_INJECT,
            max_result_bytes,
            state: Mutex::new(OperationState::CatalogStage(CatalogStage {
                actions,
                cursor: 0,
                service_request: update.service_request,
            })),
        }
    }

    pub fn set_exec_job(&self, job: i32) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if !matches!(*state, OperationState::New) {
            return Err(contract::MC_HOST_INTERNAL);
        }
        *state = OperationState::PendingExec(job);
        Ok(())
    }

    pub fn set_service_job(&self, job: i32) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if !matches!(*state, OperationState::New) {
            return Err(contract::MC_HOST_INTERNAL);
        }
        *state = OperationState::PendingService(job);
        Ok(())
    }

    pub fn complete(&self, status: i32, payload: Vec<u8>) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if !matches!(*state, OperationState::New) {
            return Err(contract::MC_HOST_INTERNAL);
        }
        if payload.len() > self.max_result_bytes.saturating_sub(result_overhead()) {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
            return Ok(());
        }
        let frame = OperationResult {
            kind: self.kind,
            status,
            payload,
        }
        .encode();
        *state = if frame.len() > self.max_result_bytes {
            OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT)
        } else {
            OperationState::Ready(Arc::from(frame))
        };
        Ok(())
    }

    pub fn observed(&self) -> Result<OperationObserved, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let (state_code, error, result_length) = match &*state {
            OperationState::New => (contract::MC_HOST_OP_NEW, 0, 0),
            OperationState::NewRetry(_) => (contract::MC_HOST_OP_NEW, 0, 0),
            OperationState::PendingExec(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
            OperationState::PendingService(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
            OperationState::PendingRetry(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
            OperationState::CatalogStage(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
            OperationState::Ready(bytes) => (contract::MC_HOST_OP_READY, 0, bytes.len() as u64),
            OperationState::Failed(error) => (contract::MC_HOST_OP_FAILED, *error, 0),
            OperationState::Cancelled => (
                contract::MC_HOST_OP_CANCELLED,
                contract::MC_HOST_CANCELLED,
                0,
            ),
        };
        Ok(OperationObserved {
            kind: self.kind,
            state: state_code,
            error,
            result_length,
        })
    }

    pub fn pending(&self) -> Result<bool, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(matches!(
            *state,
            OperationState::New
                | OperationState::PendingExec(_)
                | OperationState::PendingService(_)
                | OperationState::NewRetry(_)
                | OperationState::PendingRetry(_)
                | OperationState::CatalogStage(_)
        ))
    }

    pub fn retry_waiting(&self) -> Result<bool, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(matches!(
            *state,
            OperationState::NewRetry(_)
                | OperationState::PendingRetry(_)
                | OperationState::CatalogStage(_)
        ))
    }

    pub fn advance_retry(&self, host: &mut KernelHost) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if let OperationState::CatalogStage(stage) = &mut *state {
            if stage.cursor < stage.actions.len() {
                let action = &stage.actions[stage.cursor];
                match host
                    .control_retry_attempt(action)
                    .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?
                {
                    ControlAttempt::Pending => return Ok(()),
                    ControlAttempt::Complete { status, .. }
                        if status == 0
                            || (stage.cursor < 4 && status == -constants_rust::EEXIST) =>
                    {
                        stage.cursor += 1;
                    }
                    ControlAttempt::Complete { .. } => {
                        *state = OperationState::Failed(contract::MC_HOST_KERNEL_ERROR);
                        return Ok(());
                    }
                }
            }
            if stage.cursor == stage.actions.len() {
                *state = match host.service_start("tools", &stage.service_request) {
                    Ok(job) => OperationState::PendingService(job),
                    Err(_) => OperationState::Failed(contract::MC_HOST_KERNEL_ERROR),
                };
            }
            return Ok(());
        }
        let action = match &*state {
            OperationState::NewRetry(action) | OperationState::PendingRetry(action) => action,
            _ => return Ok(()),
        };
        let result = host
            .control_retry_attempt(action)
            .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?;
        match result {
            ControlAttempt::Pending => {
                if let OperationState::NewRetry(action) = &*state {
                    *state = OperationState::PendingRetry(action.clone());
                }
            }
            ControlAttempt::Complete { status, payload } => {
                if payload.len() > self.max_result_bytes.saturating_sub(result_overhead()) {
                    *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
                } else {
                    let frame = OperationResult {
                        kind: self.kind,
                        status,
                        payload,
                    }
                    .encode();
                    if frame.len() > self.max_result_bytes {
                        *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
                    } else {
                        *state = OperationState::Ready(Arc::from(frame));
                    }
                }
            }
        }
        Ok(())
    }

    pub fn result(&self) -> Result<Arc<[u8]>, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        match &*state {
            OperationState::Ready(bytes) => Ok(Arc::clone(bytes)),
            OperationState::Failed(error) => Err(*error),
            OperationState::Cancelled => Err(contract::MC_HOST_CANCELLED),
            _ => Err(contract::MC_HOST_WOULD_BLOCK),
        }
    }

    pub fn poll_exec(&self, host: &mut KernelHost) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let OperationState::PendingExec(job) = *state else {
            return Ok(());
        };
        let result = match host.exec_poll(job) {
            Ok(Some(result)) => result,
            Ok(None) => return Ok(()),
            Err(_) => {
                if host.exec_cancel(job).is_err() {
                    return Err(contract::MC_HOST_KERNEL_ERROR);
                }
                *state = OperationState::Failed(contract::MC_HOST_KERNEL_ERROR);
                return Ok(());
            }
        };
        let overhead = ExecOutcome::default()
            .encode()
            .len()
            .checked_add(result_overhead());
        let raw_size = result
            .stdout
            .len()
            .checked_add(result.stderr.len())
            .and_then(|n| overhead.and_then(|overhead| n.checked_add(overhead)));
        let Some(raw_size) = raw_size else {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
            return Ok(());
        };
        if raw_size > self.max_result_bytes {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
            return Ok(());
        }
        let outcome = ExecOutcome {
            exit_code: result.exit_code,
            stdout: result.stdout,
            stderr: result.stderr,
        };
        let frame = OperationResult {
            kind: self.kind,
            status: outcome.exit_code,
            payload: outcome.encode(),
        }
        .encode();
        if frame.len() > self.max_result_bytes {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
        } else {
            *state = OperationState::Ready(Arc::from(frame));
        }
        Ok(())
    }

    pub fn poll_service(&self, host: &mut KernelHost) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let OperationState::PendingService(job) = *state else {
            return Ok(());
        };
        let result = match host.service_poll(job) {
            Ok(Some(result)) => result,
            Ok(None) => return Ok(()),
            Err(_) => {
                if host.service_cancel(job).is_err() {
                    return Err(contract::MC_HOST_KERNEL_ERROR);
                }
                *state = OperationState::Failed(contract::MC_HOST_KERNEL_ERROR);
                return Ok(());
            }
        };
        let payload = SvcResponse {
            status: result.status,
            body: result.body,
        }
        .encode();
        if payload.len() > self.max_result_bytes.saturating_sub(result_overhead()) {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
            return Ok(());
        }
        let frame = OperationResult {
            kind: self.kind,
            status: result.status,
            payload,
        }
        .encode();
        if frame.len() > self.max_result_bytes {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
        } else {
            *state = OperationState::Ready(Arc::from(frame));
        }
        Ok(())
    }

    pub fn peek_stdout(&self, host: &mut KernelHost) -> Result<Vec<u8>, u32> {
        if self.kind != contract::MC_HOST_OP_EXEC_SHELL
            && self.kind != contract::MC_HOST_OP_EXEC_DIRECT
        {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        if !host.supports_exec_stdout_peek() {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let OperationState::PendingExec(job) = *state else {
            return Err(contract::MC_HOST_INVALID_STATE);
        };
        let bytes = host
            .exec_stdout_peek(job)
            .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?;
        if bytes.len() > self.max_result_bytes {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        Ok(bytes)
    }

    pub fn cancel(&self, host: &mut KernelHost) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        match *state {
            OperationState::PendingExec(job) => {
                host.exec_cancel(job)
                    .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?;
                *state = OperationState::Cancelled;
            }
            OperationState::PendingService(job) => {
                host.service_cancel(job)
                    .map_err(|_| contract::MC_HOST_KERNEL_ERROR)?;
                *state = OperationState::Cancelled;
            }
            OperationState::New
            | OperationState::NewRetry(_)
            | OperationState::PendingRetry(_)
            | OperationState::CatalogStage(_) => *state = OperationState::Cancelled,
            OperationState::Ready(_) | OperationState::Failed(_) | OperationState::Cancelled => {}
        }
        Ok(())
    }

    pub fn cancel_after_store_drop(&self) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        *state = OperationState::Cancelled;
        Ok(())
    }
}
