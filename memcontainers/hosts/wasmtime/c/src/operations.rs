//! Operation-owned terminal results. Polling reads kernel jobs but never ticks.

use std::sync::{Arc, Mutex};

use ctl_rust::ExecOutcome;
use embedder_rust::{self as contract, OperationResult};
use host::{ControlAttempt, ControlRetryAction, KernelHost};

const MAX_RESULT_BYTES: usize = 16 * 1024 * 1024;

pub struct OperationRecord {
    pub vm: u64,
    pub kind: u32,
    state: Mutex<OperationState>,
}

enum OperationState {
    New,
    PendingExec(i32),
    NewRetry(ControlRetryAction),
    PendingRetry(ControlRetryAction),
    Ready(Arc<[u8]>),
    Failed(u32),
    Cancelled,
}

#[derive(Clone, Copy)]
pub struct OperationObserved {
    pub kind: u32,
    pub state: u32,
    pub error: u32,
    pub result_length: u64,
}

impl OperationRecord {
    pub fn new(vm: u64, kind: u32) -> Self {
        Self {
            vm,
            kind,
            state: Mutex::new(OperationState::New),
        }
    }

    pub fn new_retry(vm: u64, kind: u32, action: ControlRetryAction) -> Self {
        Self {
            vm,
            kind,
            state: Mutex::new(OperationState::NewRetry(action)),
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

    pub fn observed(&self) -> Result<OperationObserved, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let (state_code, error, result_length) = match &*state {
            OperationState::New => (contract::MC_HOST_OP_NEW, 0, 0),
            OperationState::NewRetry(_) => (contract::MC_HOST_OP_NEW, 0, 0),
            OperationState::PendingExec(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
            OperationState::PendingRetry(_) => (contract::MC_HOST_OP_PENDING, 0, 0),
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
                | OperationState::NewRetry(_)
                | OperationState::PendingRetry(_)
        ))
    }

    pub fn retry_waiting(&self) -> Result<bool, u32> {
        let state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(matches!(
            *state,
            OperationState::NewRetry(_) | OperationState::PendingRetry(_)
        ))
    }

    pub fn advance_retry(&self, host: &mut KernelHost) -> Result<(), u32> {
        let mut state = self.state.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
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
                if payload.len() > MAX_RESULT_BYTES.saturating_sub(64) {
                    *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
                } else {
                    let frame = OperationResult {
                        kind: self.kind,
                        status,
                        payload,
                    }
                    .encode();
                    if frame.len() > MAX_RESULT_BYTES {
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
        let raw_size = result
            .stdout
            .len()
            .checked_add(result.stderr.len())
            .and_then(|n| n.checked_add(64));
        let Some(raw_size) = raw_size else {
            *state = OperationState::Failed(contract::MC_HOST_RESOURCE_LIMIT);
            return Ok(());
        };
        if raw_size > MAX_RESULT_BYTES {
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
        if frame.len() > MAX_RESULT_BYTES {
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
        if bytes.len() > MAX_RESULT_BYTES {
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
            OperationState::New | OperationState::NewRetry(_) | OperationState::PendingRetry(_) => {
                *state = OperationState::Cancelled
            }
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
