//! Bounded, pull-based host-call relay. Bridge imports only enqueue; the caller
//! answers later without running application code under the Wasmtime store lock.

use std::collections::{BTreeSet, HashMap, VecDeque};
use std::sync::{mpsc, Arc, Mutex};

use super::notifier::Notifier;
use constants_rust::{
    EAGAIN, EMSGSIZE, PERSIST_OP_DELETE, PERSIST_OP_GET, PERSIST_OP_LIST, PERSIST_OP_PUT,
};
use embedder_rust::{
    self as contract, EffectCancel, HostCallEvent, HttpEffectRequest, PersistEffectRequest,
    ToolApprovalRequest, WsCloseEvent, WsConnectRequest, WsSendRequest,
};
use host::{
    HostCallCapability, NetCapability, PersistCapability, ToolApprovalDecision, ToolApprovalFacts,
    ToolApprover,
};

const WS_WINDOW_BYTES: usize = 1024 * 1024;

#[derive(Clone, Copy)]
pub struct RelayLimits {
    pub max_events: usize,
    pub max_event_bytes: usize,
    pub max_single_event_bytes: usize,
    pub max_total_response_bytes: usize,
    pub max_single_response_bytes: usize,
    pub max_ws_sockets: usize,
    pub max_ws_incoming_bytes: usize,
}

#[derive(Clone, Copy, Eq, PartialEq)]
enum EventPhase {
    Queued,
    Leased,
    Responding,
    Cancelled,
}

struct ResponseStream {
    response: EffectBody,
    declared_body_len: usize,
    reserved_bytes: usize,
}

struct Event {
    kind: u32,
    payload: Vec<u8>,
    phase: EventPhase,
    native_handle: Option<u64>,
    original_handle: Option<u64>,
    stream: Option<ResponseStream>,
    socket: Option<i32>,
    ws_bytes: usize,
}

struct WsSocket {
    native_handle: Option<u64>,
    open: bool,
    failed: bool,
    guest_closed: bool,
    incoming: VecDeque<Vec<u8>>,
    incoming_bytes: usize,
    front_offset: usize,
    outgoing_bytes: usize,
}

struct Slot {
    event_id: u64,
    kind: u32,
    result: Option<EffectBody>,
    offset: usize,
}

pub struct EffectBody {
    pub failed: bool,
    pub head: Vec<u8>,
    pub body: Vec<u8>,
}

struct Inner {
    closed: bool,
    limits: RelayLimits,
    notifier: Option<Arc<Notifier>>,
    routes: BTreeSet<String>,
    persist_enabled: bool,
    http_enabled: bool,
    next_bridge_handle: i32,
    next_event_id: u64,
    events: HashMap<u64, Event>,
    queue: VecDeque<u64>,
    slots: HashMap<i32, Slot>,
    sockets: HashMap<i32, WsSocket>,
    queued_bytes: usize,
    response_bytes: usize,
    approvals: HashMap<i32, mpsc::Sender<ToolApprovalDecision>>,
    refused: [u64; 5],
}

pub struct RelayStats {
    pub queued_events: u64,
    pub leased_events: u64,
    pub queued_bytes: u64,
    pub response_bytes: u64,
    pub refused: [u64; 5],
}

fn family_index(kind: u32) -> usize {
    match kind {
        contract::MC_HOST_EVENT_HOST_CALL => 0,
        contract::MC_HOST_EVENT_PERSIST_GET
        | contract::MC_HOST_EVENT_PERSIST_PUT
        | contract::MC_HOST_EVENT_PERSIST_DELETE
        | contract::MC_HOST_EVENT_PERSIST_LIST => 1,
        contract::MC_HOST_EVENT_HTTP_REQUEST => 2,
        contract::MC_HOST_EVENT_WS_CONNECT | contract::MC_HOST_EVENT_WS_SEND => 3,
        contract::MC_HOST_EVENT_TOOL_APPROVAL => 4,
        _ => unreachable!("only bridge starts are admitted"),
    }
}

impl Inner {
    fn requeue(&mut self, id: u64) {
        let index = self
            .queue
            .iter()
            .position(|queued| *queued > id)
            .unwrap_or(self.queue.len());
        self.queue.insert(index, id);
    }

    fn close_slot(&mut self, handle: i32) {
        let Some(slot) = self.slots.remove(&handle) else {
            return;
        };
        if let Some(result) = slot.result {
            self.response_bytes -= result.head.len() + result.body.len();
        }
        match self.events.get(&slot.event_id).map(|event| event.phase) {
            Some(EventPhase::Queued) => {
                self.queue.retain(|id| *id != slot.event_id);
                if let Some(event) = self.events.remove(&slot.event_id) {
                    self.queued_bytes -= event.payload.len();
                }
            }
            Some(EventPhase::Leased | EventPhase::Responding) => self.cancel_leased(slot.event_id),
            _ => {}
        }
    }

    fn enqueue(&mut self, kind: u32, payload: Vec<u8>) -> Option<i32> {
        if self.closed {
            return None;
        }
        if self.slots.len() >= self.limits.max_events
            || payload.len() > self.limits.max_single_event_bytes
            || self
                .queued_bytes
                .checked_add(payload.len())
                .is_none_or(|total| total > self.limits.max_event_bytes)
            || self.next_bridge_handle == i32::MAX
            || self.next_event_id > u64::MAX - self.limits.max_events as u64
        {
            let counter = &mut self.refused[family_index(kind)];
            *counter = counter.saturating_add(1);
            return None;
        }
        let handle = self.next_bridge_handle;
        self.next_bridge_handle += 1;
        let id = self.next_event_id;
        self.next_event_id += 1;
        self.queued_bytes += payload.len();
        self.events.insert(
            id,
            Event {
                kind,
                payload,
                phase: EventPhase::Queued,
                native_handle: None,
                original_handle: None,
                stream: None,
                socket: None,
                ws_bytes: 0,
            },
        );
        self.queue.push_back(id);
        self.slots.insert(
            handle,
            Slot {
                event_id: id,
                kind,
                result: None,
                offset: 0,
            },
        );
        if let Some(notifier) = &self.notifier {
            notifier.notify();
        }
        Some(handle)
    }

    fn attach_socket(&mut self, event_handle: i32, socket: i32, ws_bytes: usize) {
        let id = self.slots[&event_handle].event_id;
        let event = self.events.get_mut(&id).expect("newly queued event");
        event.socket = Some(socket);
        event.ws_bytes = ws_bytes;
    }

    // Close notifications must be deliverable even when ordinary effect admission
    // is full. At most one is generated for each application-owned socket.
    fn enqueue_socket_close(&mut self, socket: i32, reason: u32) {
        let payload = WsCloseEvent { reason }.encode();
        let id = self.next_event_id;
        self.next_event_id += 1;
        self.queued_bytes += payload.len();
        self.events.insert(
            id,
            Event {
                kind: contract::MC_HOST_EVENT_WS_CLOSE,
                payload,
                phase: EventPhase::Queued,
                native_handle: None,
                original_handle: None,
                stream: None,
                socket: Some(socket),
                ws_bytes: 0,
            },
        );
        self.queue.push_back(id);
        if let Some(notifier) = &self.notifier {
            notifier.notify();
        }
    }

    fn cancel_leased(&mut self, id: u64) {
        let Some(event) = self.events.get_mut(&id) else {
            return;
        };
        if event.phase != EventPhase::Leased && event.phase != EventPhase::Responding {
            return;
        }
        if let Some(stream) = event.stream.take() {
            self.response_bytes -= stream.reserved_bytes;
        }
        let Some(original_handle) = event.native_handle else {
            if let Some(event) = self.events.remove(&id) {
                self.queued_bytes -= event.payload.len();
            }
            return;
        };
        event.phase = EventPhase::Cancelled;
        let notice_id = self.next_event_id;
        self.next_event_id += 1;
        let notice_kind = match event.kind {
            contract::MC_HOST_EVENT_HOST_CALL => contract::MC_HOST_EVENT_HOST_CALL_CLOSE,
            contract::MC_HOST_EVENT_HTTP_REQUEST => contract::MC_HOST_EVENT_HTTP_CANCEL,
            _ => contract::MC_HOST_EVENT_OPERATION_CANCEL,
        };
        let payload = EffectCancel {
            original_event_id: original_handle as i64,
        }
        .encode();
        self.queued_bytes += payload.len();
        self.events.insert(
            notice_id,
            Event {
                kind: notice_kind,
                payload,
                phase: EventPhase::Queued,
                native_handle: None,
                original_handle: Some(original_handle),
                stream: None,
                socket: None,
                ws_bytes: 0,
            },
        );
        self.queue.push_back(notice_id);
        if let Some(notifier) = &self.notifier {
            notifier.notify();
        }
    }
}

pub struct Relay {
    inner: Mutex<Inner>,
}

pub struct LeasedEvent {
    pub id: u64,
    pub kind: u32,
    pub payload: Vec<u8>,
    pub socket: Option<i32>,
}

impl Relay {
    pub fn new(
        routes: BTreeSet<String>,
        persist_enabled: bool,
        http_enabled: bool,
        limits: RelayLimits,
        notifier: Option<Arc<Notifier>>,
    ) -> Arc<Self> {
        Arc::new(Self {
            inner: Mutex::new(Inner {
                closed: false,
                limits,
                notifier,
                routes,
                persist_enabled,
                http_enabled,
                next_bridge_handle: 1,
                next_event_id: 1,
                events: HashMap::new(),
                queue: VecDeque::new(),
                slots: HashMap::new(),
                sockets: HashMap::new(),
                queued_bytes: 0,
                response_bytes: 0,
                approvals: HashMap::new(),
                refused: [0; 5],
            }),
        })
    }

    pub fn adapter(self: &Arc<Self>) -> Box<dyn HostCallCapability> {
        Box::new(RelayHostCall(Arc::clone(self)))
    }

    pub fn persist_enabled(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| inner.persist_enabled)
    }

    pub fn persist_adapter(self: &Arc<Self>) -> Box<dyn PersistCapability> {
        Box::new(RelayPersist(Arc::clone(self)))
    }

    pub fn http_enabled(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| inner.http_enabled)
    }

    pub fn net_adapter(self: &Arc<Self>) -> Box<dyn NetCapability> {
        Box::new(RelayNet(Arc::clone(self)))
    }

    pub fn approver(self: &Arc<Self>) -> Arc<dyn ToolApprover> {
        Arc::new(RelayApprover(Arc::clone(self)))
    }

    pub fn has_route(&self, name: &str) -> bool {
        self.inner
            .lock()
            .is_ok_and(|inner| inner.routes.contains(name))
    }

    pub fn replace_routes(&self, routes: BTreeSet<String>) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if inner.closed {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if !inner.events.is_empty() || !inner.slots.is_empty() {
            return Err(contract::MC_HOST_NOT_QUIESCENT);
        }
        inner.routes = routes;
        Ok(())
    }

    pub fn above_high_water(&self) -> bool {
        self.inner
            .lock()
            .is_ok_and(|inner| inner.queued_bytes >= inner.limits.max_event_bytes * 3 / 4)
    }

    pub fn ready(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| !inner.queue.is_empty())
    }

    pub fn stats(&self) -> Result<RelayStats, u32> {
        let inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(RelayStats {
            queued_events: inner.queue.len() as u64,
            leased_events: inner
                .events
                .values()
                .filter(|event| {
                    event.phase == EventPhase::Leased || event.phase == EventPhase::Responding
                })
                .count() as u64,
            queued_bytes: inner.queued_bytes as u64,
            response_bytes: inner.response_bytes as u64,
            refused: inner.refused,
        })
    }

    pub fn has_live_events(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| {
            !inner.events.is_empty() || inner.sockets.values().any(|s| !s.guest_closed)
        })
    }

    pub fn has_snapshot_blockers(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| {
            inner.sockets.values().any(|state| !state.guest_closed)
                || inner.events.values().any(|event| {
                    event.phase != EventPhase::Cancelled
                        && !matches!(
                            event.kind,
                            contract::MC_HOST_EVENT_HTTP_CANCEL
                                | contract::MC_HOST_EVENT_HOST_CALL_CLOSE
                                | contract::MC_HOST_EVENT_OPERATION_CANCEL
                                | contract::MC_HOST_EVENT_WS_CLOSE
                        )
                })
        })
    }

    pub fn socket_native_handle(&self, socket: i32) -> Result<Option<u64>, u32> {
        let inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        Ok(inner
            .sockets
            .get(&socket)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?
            .native_handle)
    }

    pub fn socket_push(&self, socket: i32, data: &[u8]) -> Result<(), u32> {
        if data.is_empty() {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        if data.len() > WS_WINDOW_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let state = inner
            .sockets
            .get(&socket)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if state.guest_closed || state.failed {
            return Err(contract::MC_HOST_CANCELLED);
        }
        if !state.open {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let total_incoming: usize = inner
            .sockets
            .values()
            .map(|state| state.incoming_bytes)
            .sum();
        if total_incoming + data.len() > inner.limits.max_ws_incoming_bytes {
            return Err(contract::MC_HOST_WOULD_BLOCK);
        }
        let state = inner.sockets.get_mut(&socket).expect("validated socket");
        if state.incoming_bytes + data.len() > WS_WINDOW_BYTES {
            return Err(contract::MC_HOST_WOULD_BLOCK);
        }
        state.incoming_bytes += data.len();
        state.incoming.push_back(data.to_vec());
        if let Some(notifier) = &inner.notifier {
            notifier.notify();
        }
        Ok(())
    }

    pub fn socket_close(&self, socket: i32) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let state = inner
            .sockets
            .get_mut(&socket)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if state.guest_closed {
            return Err(contract::MC_HOST_CANCELLED);
        }
        state.failed = true;
        if let Some(notifier) = &inner.notifier {
            notifier.notify();
        }
        Ok(())
    }

    pub fn next(&self, mask: u64) -> Result<Option<LeasedEvent>, u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let Some(index) = inner.queue.iter().position(|id| {
            inner.events.get(id).is_some_and(|event| {
                event.phase == EventPhase::Queued && (mask == 0 || mask & (1u64 << event.kind) != 0)
            })
        }) else {
            return Ok(None);
        };
        let id = inner
            .queue
            .remove(index)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        event.phase = EventPhase::Leased;
        Ok(Some(LeasedEvent {
            id,
            kind: event.kind,
            payload: event.payload.clone(),
            socket: event.socket,
        }))
    }

    pub fn bind_handle(&self, id: u64, handle: u64, socket_handle: Option<u64>) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.phase != EventPhase::Leased || event.native_handle.is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if let Some(socket) = event.socket {
            let state = inner
                .sockets
                .get(&socket)
                .ok_or(contract::MC_HOST_STALE_HANDLE)?;
            if (state.guest_closed && event.kind != contract::MC_HOST_EVENT_WS_CLOSE)
                || (state.native_handle.is_some() && state.native_handle != socket_handle)
            {
                return Err(contract::MC_HOST_INVALID_STATE);
            }
            if event.kind == contract::MC_HOST_EVENT_WS_CONNECT {
                inner
                    .sockets
                    .get_mut(&socket)
                    .expect("validated socket")
                    .native_handle = socket_handle;
            }
        }
        inner
            .events
            .get_mut(&id)
            .expect("validated event")
            .native_handle = Some(handle);
        Ok(())
    }

    pub fn release(&self, id: u64, handle: u64) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle) || event.phase != EventPhase::Leased {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        event.native_handle = None;
        event.phase = EventPhase::Queued;
        inner.requeue(id);
        Ok(())
    }

    pub fn unlease(&self, id: u64) {
        if let Ok(mut inner) = self.inner.lock() {
            if let Some(event) = inner.events.get_mut(&id) {
                if event.phase == EventPhase::Leased && event.native_handle.is_none() {
                    event.phase = EventPhase::Queued;
                    inner.requeue(id);
                }
            }
        }
    }

    pub fn ack_cancelled(&self, id: u64, handle: u64) -> Result<u64, u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.kind != contract::MC_HOST_EVENT_HTTP_CANCEL
            && event.kind != contract::MC_HOST_EVENT_HOST_CALL_CLOSE
            && event.kind != contract::MC_HOST_EVENT_OPERATION_CANCEL
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if event.phase != EventPhase::Leased || event.native_handle != Some(handle) {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let original_handle = event.original_handle.ok_or(contract::MC_HOST_INTERNAL)?;
        let notice = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= notice.payload.len();
        let original_id = inner
            .events
            .iter()
            .find_map(|(id, event)| (event.native_handle == Some(original_handle)).then_some(*id))
            .ok_or(contract::MC_HOST_INTERNAL)?;
        let original = inner
            .events
            .remove(&original_id)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= original.payload.len();
        Ok(original_handle)
    }

    pub fn ack_socket_close(&self, id: u64, handle: u64) -> Result<i32, u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.kind != contract::MC_HOST_EVENT_WS_CLOSE
            || event.phase != EventPhase::Leased
            || event.native_handle != Some(handle)
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let socket = event.socket.ok_or(contract::MC_HOST_INTERNAL)?;
        let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= event.payload.len();
        inner.sockets.remove(&socket);
        Ok(socket)
    }

    pub fn respond(
        &self,
        id: u64,
        handle: u64,
        kind: u32,
        response: EffectBody,
    ) -> Result<(), u32> {
        if kind == contract::MC_HOST_EVENT_HTTP_REQUEST && response.head.len() > 16 * 1024 {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let response_len = response
            .head
            .len()
            .checked_add(response.body.len())
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if response_len > inner.limits.max_single_response_bytes {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle)
            || event.phase != EventPhase::Leased
            || event.kind != kind
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if kind == contract::MC_HOST_EVENT_TOOL_APPROVAL {
            if !response.head.is_empty() || response.body.len() != 1 || response.body[0] > 1 {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            let slot_handle = inner
                .slots
                .iter()
                .find_map(|(slot_handle, slot)| (slot.event_id == id).then_some(*slot_handle))
                .ok_or(contract::MC_HOST_CANCELLED)?;
            let sender = inner
                .approvals
                .remove(&slot_handle)
                .ok_or(contract::MC_HOST_CANCELLED)?;
            inner.slots.remove(&slot_handle);
            let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
            inner.queued_bytes -= event.payload.len();
            let _ = sender.send(ToolApprovalDecision {
                allow: !response.failed,
                remember_session: !response.failed && response.body[0] == 1,
            });
            if let Some(notifier) = &inner.notifier {
                notifier.notify();
            }
            return Ok(());
        }
        if kind == contract::MC_HOST_EVENT_WS_CONNECT || kind == contract::MC_HOST_EVENT_WS_SEND {
            if !response.head.is_empty() || !response.body.is_empty() {
                return Err(contract::MC_HOST_INVALID_ARGUMENT);
            }
            let socket = event.socket.ok_or(contract::MC_HOST_INTERNAL)?;
            let ws_bytes = event.ws_bytes;
            let slot_handle = inner
                .slots
                .iter()
                .find_map(|(slot_handle, slot)| (slot.event_id == id).then_some(*slot_handle))
                .ok_or(contract::MC_HOST_CANCELLED)?;
            let state = inner
                .sockets
                .get_mut(&socket)
                .ok_or(contract::MC_HOST_CANCELLED)?;
            if state.guest_closed {
                return Err(contract::MC_HOST_CANCELLED);
            }
            if kind == contract::MC_HOST_EVENT_WS_CONNECT {
                state.open = !response.failed && !state.failed;
                state.failed |= response.failed;
            } else {
                state.outgoing_bytes -= ws_bytes;
                if response.failed {
                    state.failed = true;
                }
            }
            inner.slots.remove(&slot_handle);
            let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
            inner.queued_bytes -= event.payload.len();
            if let Some(notifier) = &inner.notifier {
                notifier.notify();
            }
            return Ok(());
        }
        if inner
            .response_bytes
            .checked_add(response_len)
            .is_none_or(|total| total > inner.limits.max_total_response_bytes)
        {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let Some(slot) = inner
            .slots
            .values_mut()
            .find(|slot| slot.event_id == id && slot.kind == kind)
        else {
            return Err(contract::MC_HOST_CANCELLED);
        };
        slot.result = Some(response);
        inner.response_bytes += response_len;
        let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= event.payload.len();
        if let Some(notifier) = &inner.notifier {
            notifier.notify();
        }
        Ok(())
    }

    pub fn response_begin(
        &self,
        id: u64,
        handle: u64,
        kind: u32,
        response: EffectBody,
        declared_body_len: usize,
    ) -> Result<(), u32> {
        if kind == contract::MC_HOST_EVENT_WS_CONNECT
            || kind == contract::MC_HOST_EVENT_WS_SEND
            || kind == contract::MC_HOST_EVENT_TOOL_APPROVAL
        {
            return Err(contract::MC_HOST_UNSUPPORTED);
        }
        if !response.body.is_empty()
            || (response.failed && declared_body_len != 0)
            || (kind == contract::MC_HOST_EVENT_HTTP_REQUEST && response.head.len() > 16 * 1024)
        {
            return Err(contract::MC_HOST_INVALID_ARGUMENT);
        }
        let reserved_bytes = response
            .head
            .len()
            .checked_add(declared_body_len)
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        if reserved_bytes > inner.limits.max_single_response_bytes {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle)
            || event.phase != EventPhase::Leased
            || event.kind != kind
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        if !inner
            .slots
            .values()
            .any(|slot| slot.event_id == id && slot.kind == kind)
        {
            return Err(contract::MC_HOST_CANCELLED);
        }
        let total = inner
            .response_bytes
            .checked_add(reserved_bytes)
            .ok_or(contract::MC_HOST_RESOURCE_LIMIT)?;
        if total > inner.limits.max_total_response_bytes {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_INTERNAL)?;
        event.phase = EventPhase::Responding;
        event.stream = Some(ResponseStream {
            response,
            declared_body_len,
            reserved_bytes,
        });
        inner.response_bytes = total;
        Ok(())
    }

    pub fn response_write(&self, id: u64, handle: u64, chunk: &[u8]) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle) || event.phase != EventPhase::Responding {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let stream = event.stream.as_mut().ok_or(contract::MC_HOST_INTERNAL)?;
        let remaining = stream.declared_body_len - stream.response.body.len();
        if chunk.len() > remaining {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        stream.response.body.extend_from_slice(chunk);
        Ok(())
    }

    pub fn response_finish(&self, id: u64, handle: u64) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle) || event.phase != EventPhase::Responding {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let stream = event.stream.as_ref().ok_or(contract::MC_HOST_INTERNAL)?;
        if stream.response.body.len() != stream.declared_body_len {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let kind = event.kind;
        if !inner
            .slots
            .values()
            .any(|slot| slot.event_id == id && slot.kind == kind)
        {
            return Err(contract::MC_HOST_CANCELLED);
        }
        let stream = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_INTERNAL)?
            .stream
            .take()
            .ok_or(contract::MC_HOST_INTERNAL)?;
        let slot = inner
            .slots
            .values_mut()
            .find(|slot| slot.event_id == id && slot.kind == kind)
            .ok_or(contract::MC_HOST_CANCELLED)?;
        slot.result = Some(stream.response);
        let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= event.payload.len();
        if let Some(notifier) = &inner.notifier {
            notifier.notify();
        }
        Ok(())
    }

    pub fn response_abort(&self, id: u64, handle: u64) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle) || event.phase != EventPhase::Responding {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let stream = event.stream.take().ok_or(contract::MC_HOST_INTERNAL)?;
        event.phase = EventPhase::Leased;
        inner.response_bytes -= stream.reserved_bytes;
        Ok(())
    }

    pub fn event(&self, id: u64, handle: u64) -> Result<(u32, Vec<u8>), u32> {
        let inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle) {
            return Err(contract::MC_HOST_STALE_HANDLE);
        }
        if event.phase == EventPhase::Cancelled {
            return Err(contract::MC_HOST_CANCELLED);
        }
        if event.phase == EventPhase::Responding {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        Ok((event.kind, event.payload.clone()))
    }

    pub fn cancel_all(&self) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.closed = true;
            inner.approvals.clear();
            let closing = inner
                .sockets
                .iter_mut()
                .filter_map(|(handle, state)| {
                    if state.guest_closed {
                        return None;
                    }
                    state.guest_closed = true;
                    state.native_handle.map(|_| *handle)
                })
                .collect::<Vec<_>>();
            inner
                .sockets
                .retain(|_, state| state.native_handle.is_some());
            inner.slots.clear();
            let leased = inner
                .events
                .iter()
                .filter_map(|(id, event)| {
                    ((event.phase == EventPhase::Leased || event.phase == EventPhase::Responding)
                        && event.original_handle.is_none()
                        && event.kind != contract::MC_HOST_EVENT_WS_CLOSE)
                        .then_some(*id)
                })
                .collect::<Vec<_>>();
            inner.events.retain(|_, event| {
                ((event.phase == EventPhase::Leased || event.phase == EventPhase::Responding)
                    && event.native_handle.is_some())
                    || event.phase == EventPhase::Cancelled
                    || (event.phase == EventPhase::Queued
                        && (event.kind == contract::MC_HOST_EVENT_HOST_CALL_CLOSE
                            || event.kind == contract::MC_HOST_EVENT_OPERATION_CANCEL
                            || event.kind == contract::MC_HOST_EVENT_HTTP_CANCEL
                            || event.kind == contract::MC_HOST_EVENT_WS_CLOSE))
            });
            let live_ids = inner
                .events
                .keys()
                .copied()
                .collect::<std::collections::HashSet<_>>();
            inner.queue.retain(|id| live_ids.contains(id));
            for id in leased {
                inner.cancel_leased(id);
            }
            for socket in closing {
                inner.enqueue_socket_close(socket, contract::MC_HOST_WS_CLOSE_VM);
            }
            inner.response_bytes = 0;
            inner.queued_bytes = inner.events.values().map(|event| event.payload.len()).sum();
        }
    }
}

struct RelayHostCall(Arc<Relay>);

impl HostCallCapability for RelayHostCall {
    fn start(&mut self, req: &[u8]) -> i32 {
        let Some(split) = req.iter().position(|byte| *byte == 0) else {
            return -1;
        };
        let Ok(name) = std::str::from_utf8(&req[..split]) else {
            return -1;
        };
        let payload = HostCallEvent {
            name: name.to_string(),
            request: req[split + 1..].to_vec(),
        }
        .encode();
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        if !inner.routes.contains(name) {
            return -1;
        }
        inner
            .enqueue(contract::MC_HOST_EVENT_HOST_CALL, payload)
            .unwrap_or(-1)
    }

    fn poll(&mut self, handle: i32) -> i32 {
        let Ok(inner) = self.0.inner.lock() else {
            return -1;
        };
        match inner.slots.get(&handle) {
            Some(slot) if slot.kind != contract::MC_HOST_EVENT_HOST_CALL => -1,
            Some(slot) if slot.result.as_ref().is_some_and(|result| result.failed) => -1,
            Some(slot) if slot.result.is_some() => 1,
            Some(_) => 0,
            None => -1,
        }
    }

    fn body(&mut self, handle: i32, buf: &mut [u8]) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(slot) = inner
            .slots
            .get_mut(&handle)
            .filter(|slot| slot.kind == contract::MC_HOST_EVENT_HOST_CALL)
        else {
            return -1;
        };
        let Some(result) = slot.result.as_ref() else {
            return 0;
        };
        if result.failed {
            return -1;
        }
        let count = (result.body.len() - slot.offset).min(buf.len());
        buf[..count].copy_from_slice(&result.body[slot.offset..slot.offset + count]);
        slot.offset += count;
        count as i32
    }

    fn close(&mut self, handle: i32) {
        let Ok(mut inner) = self.0.inner.lock() else {
            return;
        };
        if !inner
            .slots
            .get(&handle)
            .is_some_and(|slot| slot.kind == contract::MC_HOST_EVENT_HOST_CALL)
        {
            return;
        }
        inner.close_slot(handle);
    }
}

struct RelayPersist(Arc<Relay>);

impl PersistCapability for RelayPersist {
    fn start(&mut self, req: &[u8]) -> i32 {
        if req.len() < 8 {
            return -1;
        }
        let op = u32::from_le_bytes(req[..4].try_into().unwrap());
        let key_len = u32::from_le_bytes(req[4..8].try_into().unwrap()) as usize;
        if key_len > req.len() - 8 {
            return -1;
        }
        let kind = match op {
            PERSIST_OP_GET => contract::MC_HOST_EVENT_PERSIST_GET,
            PERSIST_OP_PUT => contract::MC_HOST_EVENT_PERSIST_PUT,
            PERSIST_OP_DELETE => contract::MC_HOST_EVENT_PERSIST_DELETE,
            PERSIST_OP_LIST => contract::MC_HOST_EVENT_PERSIST_LIST,
            _ => return -1,
        };
        let value = &req[8 + key_len..];
        if op != PERSIST_OP_PUT && !value.is_empty() {
            return -1;
        }
        let payload = PersistEffectRequest {
            key: req[8..8 + key_len].to_vec(),
            value: (op == PERSIST_OP_PUT).then(|| value.to_vec()),
        }
        .encode();
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        if !inner.persist_enabled {
            return -1;
        }
        inner.enqueue(kind, payload).unwrap_or(-1)
    }

    fn poll(&mut self, handle: i32) -> i32 {
        let Ok(inner) = self.0.inner.lock() else {
            return -1;
        };
        match inner.slots.get(&handle) {
            Some(slot) if !is_persist_kind(slot.kind) => -1,
            Some(slot) if slot.result.as_ref().is_some_and(|result| result.failed) => -1,
            Some(slot) if slot.result.is_some() => 1,
            Some(_) => 0,
            None => -1,
        }
    }

    fn body(&mut self, handle: i32, buf: &mut [u8]) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(slot) = inner
            .slots
            .get_mut(&handle)
            .filter(|slot| is_persist_kind(slot.kind))
        else {
            return -1;
        };
        let Some(result) = slot.result.as_ref() else {
            return 0;
        };
        if result.failed {
            return -1;
        }
        let count = (result.body.len() - slot.offset).min(buf.len());
        buf[..count].copy_from_slice(&result.body[slot.offset..slot.offset + count]);
        slot.offset += count;
        count as i32
    }

    fn close(&mut self, handle: i32) {
        let Ok(mut inner) = self.0.inner.lock() else {
            return;
        };
        if !inner
            .slots
            .get(&handle)
            .is_some_and(|slot| is_persist_kind(slot.kind))
        {
            return;
        }
        inner.close_slot(handle);
    }
}

fn is_persist_kind(kind: u32) -> bool {
    matches!(
        kind,
        contract::MC_HOST_EVENT_PERSIST_GET
            | contract::MC_HOST_EVENT_PERSIST_PUT
            | contract::MC_HOST_EVENT_PERSIST_DELETE
            | contract::MC_HOST_EVENT_PERSIST_LIST
    )
}

struct RelayApprover(Arc<Relay>);

impl ToolApprover for RelayApprover {
    fn approve(&self, facts: ToolApprovalFacts) -> ToolApprovalDecision {
        let denied = ToolApprovalDecision {
            allow: false,
            remember_session: false,
        };
        let payload = ToolApprovalRequest {
            connection: facts.connection,
            method: facts.method,
            url: facts.url,
            origin: facts.origin,
            args_digest: facts.args_digest,
        }
        .encode();
        let receiver = {
            let Ok(mut inner) = self.0.inner.lock() else {
                return denied;
            };
            if inner.closed {
                return denied;
            }
            let Some(handle) = inner.enqueue(contract::MC_HOST_EVENT_TOOL_APPROVAL, payload) else {
                return denied;
            };
            let (sender, receiver) = mpsc::channel();
            inner.approvals.insert(handle, sender);
            receiver
        };
        receiver.recv().unwrap_or(denied)
    }
}

struct RelayNet(Arc<Relay>);

impl NetCapability for RelayNet {
    fn http_request(&mut self, req: &[u8]) -> i32 {
        let Some(end) = req.windows(2).position(|window| window == b"\n\n") else {
            return -1;
        };
        let Ok(head) = std::str::from_utf8(&req[..end]) else {
            return -1;
        };
        let mut lines = head.lines();
        if !lines.next().is_some_and(|first| {
            first
                .split_once(' ')
                .is_some_and(|(method, url)| !method.is_empty() && !url.trim().is_empty())
        }) {
            return -1;
        }
        for line in lines {
            let Some((name, _)) = line.split_once(':') else {
                return -1;
            };
            if name.trim().eq_ignore_ascii_case("X-MC-Connection") {
                return -1;
            }
        }
        let payload = HttpEffectRequest {
            request: req.to_vec(),
        }
        .encode();
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        if !inner.http_enabled {
            return -1;
        }
        inner
            .enqueue(contract::MC_HOST_EVENT_HTTP_REQUEST, payload)
            .unwrap_or(-1)
    }

    fn http_poll(&mut self, handle: i32, buf: &mut [u8]) -> i32 {
        let Ok(inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(slot) = inner
            .slots
            .get(&handle)
            .filter(|slot| slot.kind == contract::MC_HOST_EVENT_HTTP_REQUEST)
        else {
            return -1;
        };
        let Some(result) = &slot.result else {
            return 0;
        };
        if result.failed {
            return -1;
        }
        let count = result.head.len().min(buf.len());
        buf[..count].copy_from_slice(&result.head[..count]);
        count as i32
    }

    fn http_body(&mut self, handle: i32, buf: &mut [u8]) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(slot) = inner
            .slots
            .get_mut(&handle)
            .filter(|slot| slot.kind == contract::MC_HOST_EVENT_HTTP_REQUEST)
        else {
            return -1;
        };
        let Some(result) = &slot.result else {
            return 0;
        };
        if result.failed {
            return -1;
        }
        let count = (result.body.len() - slot.offset).min(buf.len());
        buf[..count].copy_from_slice(&result.body[slot.offset..slot.offset + count]);
        slot.offset += count;
        count as i32
    }

    fn http_close(&mut self, handle: i32) {
        if let Ok(mut inner) = self.0.inner.lock() {
            if inner
                .slots
                .get(&handle)
                .is_some_and(|slot| slot.kind == contract::MC_HOST_EVENT_HTTP_REQUEST)
            {
                inner.close_slot(handle);
            }
        }
    }

    fn ws_connect(&mut self, url: &str) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        if !inner.http_enabled
            || inner.sockets.len() >= inner.limits.max_ws_sockets
            || url.len() > 16 * 1024
            || (!url.starts_with("ws://") && !url.starts_with("wss://"))
        {
            return -1;
        }
        let Some(handle) = inner.enqueue(
            contract::MC_HOST_EVENT_WS_CONNECT,
            WsConnectRequest {
                url: url.to_owned(),
            }
            .encode(),
        ) else {
            return -1;
        };
        inner.attach_socket(handle, handle, 0);
        inner.sockets.insert(
            handle,
            WsSocket {
                native_handle: None,
                open: false,
                failed: false,
                guest_closed: false,
                incoming: VecDeque::new(),
                incoming_bytes: 0,
                front_offset: 0,
                outgoing_bytes: 0,
            },
        );
        handle
    }
    fn ws_send(&mut self, handle: i32, data: &[u8]) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(state) = inner.sockets.get(&handle) else {
            return -1;
        };
        if state.failed || state.guest_closed {
            return -1;
        }
        if data.len() > WS_WINDOW_BYTES {
            return -EMSGSIZE;
        }
        if !state.open || state.outgoing_bytes + data.len() > WS_WINDOW_BYTES {
            return -EAGAIN;
        }
        let payload = WsSendRequest {
            data: data.to_vec(),
        }
        .encode();
        if payload.len() > inner.limits.max_event_bytes {
            return -EMSGSIZE;
        }
        let Some(event_handle) = inner.enqueue(contract::MC_HOST_EVENT_WS_SEND, payload) else {
            return -EAGAIN;
        };
        inner.attach_socket(event_handle, handle, data.len());
        inner
            .sockets
            .get_mut(&handle)
            .expect("validated socket")
            .outgoing_bytes += data.len();
        data.len() as i32
    }
    fn ws_ready(&mut self, handle: i32) -> i32 {
        let Ok(inner) = self.0.inner.lock() else {
            return 1;
        };
        let Some(state) = inner.sockets.get(&handle) else {
            return 1;
        };
        if state.failed || state.guest_closed {
            return 1;
        }
        (state.open
            && state.outgoing_bytes < WS_WINDOW_BYTES
            && inner.slots.len() < inner.limits.max_events
            && inner.queued_bytes < inner.limits.max_event_bytes) as i32
    }
    fn ws_recv(&mut self, handle: i32, buf: &mut [u8]) -> i32 {
        let Ok(mut inner) = self.0.inner.lock() else {
            return -1;
        };
        let Some(state) = inner.sockets.get_mut(&handle) else {
            return -1;
        };
        let Some(front) = state.incoming.front() else {
            return if state.failed || state.guest_closed {
                -1
            } else {
                0
            };
        };
        let count = (front.len() - state.front_offset).min(buf.len());
        buf[..count].copy_from_slice(&front[state.front_offset..state.front_offset + count]);
        state.front_offset += count;
        state.incoming_bytes -= count;
        if state.front_offset == front.len() {
            state.incoming.pop_front();
            state.front_offset = 0;
        }
        count as i32
    }
    fn ws_close(&mut self, handle: i32) {
        let Ok(mut inner) = self.0.inner.lock() else {
            return;
        };
        let Some(state) = inner.sockets.get_mut(&handle) else {
            return;
        };
        if state.guest_closed {
            return;
        }
        state.guest_closed = true;
        let adopted = state.native_handle.is_some();
        let handles = inner
            .slots
            .iter()
            .filter_map(|(slot_handle, slot)| {
                inner
                    .events
                    .get(&slot.event_id)
                    .filter(|event| event.socket == Some(handle))
                    .map(|_| *slot_handle)
            })
            .collect::<Vec<_>>();
        for event_handle in handles {
            inner.close_slot(event_handle);
        }
        if adopted {
            inner.enqueue_socket_close(handle, contract::MC_HOST_WS_CLOSE_GUEST);
        } else {
            inner.sockets.remove(&handle);
        }
    }
}
