//! Bounded, pull-based host-call relay. Bridge imports only enqueue; the caller
//! answers later without running application code under the Wasmtime store lock.

use std::collections::{BTreeSet, HashMap, VecDeque};
use std::sync::{Arc, Mutex};

use constants_rust::{PERSIST_OP_DELETE, PERSIST_OP_GET, PERSIST_OP_LIST, PERSIST_OP_PUT};
use embedder_rust::{
    self as contract, EffectCancel, HostCallEvent, HttpEffectRequest, PersistEffectRequest,
};
use host::{HostCallCapability, NetCapability, PersistCapability};

const MAX_EVENTS: usize = 256;
const MAX_EVENT_BYTES: usize = 4 * 1024 * 1024;
const EVENT_HIGH_WATER: usize = 3 * 1024 * 1024;
const MAX_RESPONSE_BYTES: usize = 16 * 1024 * 1024;
const MAX_TOTAL_RESPONSE_BYTES: usize = 64 * 1024 * 1024;

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
    routes: BTreeSet<String>,
    persist_enabled: bool,
    http_enabled: bool,
    next_bridge_handle: i32,
    next_event_id: u64,
    events: HashMap<u64, Event>,
    queue: VecDeque<u64>,
    slots: HashMap<i32, Slot>,
    queued_bytes: usize,
    response_bytes: usize,
}

impl Inner {
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
        if self.slots.len() >= MAX_EVENTS
            || self
                .queued_bytes
                .checked_add(payload.len())
                .is_none_or(|total| total > MAX_EVENT_BYTES)
            || self.next_bridge_handle == i32::MAX
            || self.next_event_id > u64::MAX - MAX_EVENTS as u64
        {
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
        Some(handle)
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
            },
        );
        self.queue.push_back(notice_id);
    }
}

pub struct Relay {
    inner: Mutex<Inner>,
}

pub struct LeasedEvent {
    pub id: u64,
    pub kind: u32,
    pub payload: Vec<u8>,
}

impl Relay {
    pub fn new(routes: BTreeSet<String>, persist_enabled: bool, http_enabled: bool) -> Arc<Self> {
        Arc::new(Self {
            inner: Mutex::new(Inner {
                routes,
                persist_enabled,
                http_enabled,
                next_bridge_handle: 1,
                next_event_id: 1,
                events: HashMap::new(),
                queue: VecDeque::new(),
                slots: HashMap::new(),
                queued_bytes: 0,
                response_bytes: 0,
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

    pub fn has_route(&self, name: &str) -> bool {
        self.inner
            .lock()
            .is_ok_and(|inner| inner.routes.contains(name))
    }

    pub fn above_high_water(&self) -> bool {
        self.inner
            .lock()
            .is_ok_and(|inner| inner.queued_bytes >= EVENT_HIGH_WATER)
    }

    pub fn ready(&self) -> bool {
        self.inner.lock().is_ok_and(|inner| !inner.queue.is_empty())
    }

    pub fn has_live_events(&self) -> bool {
        self.inner
            .lock()
            .is_ok_and(|inner| !inner.events.is_empty())
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
        }))
    }

    pub fn bind_handle(&self, id: u64, handle: u64) -> Result<(), u32> {
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get_mut(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.phase != EventPhase::Leased || event.native_handle.is_some() {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        event.native_handle = Some(handle);
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
        inner.queue.push_front(id);
        Ok(())
    }

    pub fn unlease(&self, id: u64) {
        if let Ok(mut inner) = self.inner.lock() {
            if let Some(event) = inner.events.get_mut(&id) {
                if event.phase == EventPhase::Leased && event.native_handle.is_none() {
                    event.phase = EventPhase::Queued;
                    inner.queue.push_front(id);
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
        if response_len > MAX_RESPONSE_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
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
        if inner
            .response_bytes
            .checked_add(response_len)
            .is_none_or(|total| total > MAX_TOTAL_RESPONSE_BYTES)
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
        if reserved_bytes > MAX_RESPONSE_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
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
        if total > MAX_TOTAL_RESPONSE_BYTES {
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
            inner.slots.clear();
            let leased = inner
                .events
                .iter()
                .filter_map(|(id, event)| {
                    ((event.phase == EventPhase::Leased || event.phase == EventPhase::Responding)
                        && event.original_handle.is_none())
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
                            || event.kind == contract::MC_HOST_EVENT_HTTP_CANCEL))
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

    fn ws_connect(&mut self, _url: &str) -> i32 {
        -1
    }
    fn ws_send(&mut self, _handle: i32, _data: &[u8]) -> i32 {
        -1
    }
    fn ws_ready(&mut self, _handle: i32) -> i32 {
        1
    }
    fn ws_recv(&mut self, _handle: i32, _buf: &mut [u8]) -> i32 {
        -1
    }
    fn ws_close(&mut self, _handle: i32) {}
}
