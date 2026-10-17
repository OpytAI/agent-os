//! Bounded, pull-based host-call relay. Bridge imports only enqueue; the caller
//! answers later without running application code under the Wasmtime store lock.

use std::collections::{BTreeSet, HashMap, VecDeque};
use std::sync::{Arc, Mutex};

use embedder_rust::{self as contract, EffectCancel, HostCallEvent, HostCallResponse};
use host::HostCallCapability;

const MAX_EVENTS: usize = 256;
const MAX_EVENT_BYTES: usize = 4 * 1024 * 1024;
const EVENT_HIGH_WATER: usize = 3 * 1024 * 1024;
const MAX_RESPONSE_BYTES: usize = 16 * 1024 * 1024;

#[derive(Clone, Copy, Eq, PartialEq)]
enum EventPhase {
    Queued,
    Leased,
    Cancelled,
}

struct Event {
    kind: u32,
    payload: Vec<u8>,
    phase: EventPhase,
    native_handle: Option<u64>,
    original_handle: Option<u64>,
}

struct Slot {
    event_id: u64,
    result: Option<HostCallResponse>,
    offset: usize,
}

struct Inner {
    routes: BTreeSet<String>,
    next_bridge_handle: i32,
    next_event_id: u64,
    events: HashMap<u64, Event>,
    queue: VecDeque<u64>,
    slots: HashMap<i32, Slot>,
    queued_bytes: usize,
}

impl Inner {
    fn cancel_leased(&mut self, id: u64) {
        let Some(event) = self.events.get_mut(&id) else {
            return;
        };
        if event.phase != EventPhase::Leased {
            return;
        }
        let Some(original_handle) = event.native_handle else {
            if let Some(event) = self.events.remove(&id) {
                self.queued_bytes -= event.payload.len();
            }
            return;
        };
        event.phase = EventPhase::Cancelled;
        if self.next_event_id == u64::MAX {
            return;
        }
        let notice_id = self.next_event_id;
        self.next_event_id += 1;
        let payload = EffectCancel {
            original_event_id: original_handle as i64,
        }
        .encode();
        self.queued_bytes += payload.len();
        self.events.insert(
            notice_id,
            Event {
                kind: contract::MC_HOST_EVENT_HOST_CALL_CLOSE,
                payload,
                phase: EventPhase::Queued,
                native_handle: None,
                original_handle: Some(original_handle),
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
    pub fn new(routes: BTreeSet<String>) -> Arc<Self> {
        Arc::new(Self {
            inner: Mutex::new(Inner {
                routes,
                next_bridge_handle: 1,
                next_event_id: 1,
                events: HashMap::new(),
                queue: VecDeque::new(),
                slots: HashMap::new(),
                queued_bytes: 0,
            }),
        })
    }

    pub fn adapter(self: &Arc<Self>) -> Box<dyn HostCallCapability> {
        Box::new(RelayHostCall(Arc::clone(self)))
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
        if event.kind != contract::MC_HOST_EVENT_HOST_CALL_CLOSE
            || event.phase != EventPhase::Leased
            || event.native_handle != Some(handle)
        {
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

    pub fn respond(&self, id: u64, handle: u64, response: HostCallResponse) -> Result<(), u32> {
        if response.body.len() > MAX_RESPONSE_BYTES {
            return Err(contract::MC_HOST_RESOURCE_LIMIT);
        }
        let mut inner = self.inner.lock().map_err(|_| contract::MC_HOST_INTERNAL)?;
        let event = inner
            .events
            .get(&id)
            .ok_or(contract::MC_HOST_STALE_HANDLE)?;
        if event.native_handle != Some(handle)
            || event.phase != EventPhase::Leased
            || event.kind != contract::MC_HOST_EVENT_HOST_CALL
        {
            return Err(contract::MC_HOST_INVALID_STATE);
        }
        let Some(slot) = inner.slots.values_mut().find(|slot| slot.event_id == id) else {
            return Err(contract::MC_HOST_CANCELLED);
        };
        slot.result = Some(response);
        let event = inner.events.remove(&id).ok_or(contract::MC_HOST_INTERNAL)?;
        inner.queued_bytes -= event.payload.len();
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
        Ok((event.kind, event.payload.clone()))
    }

    pub fn cancel_all(&self) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.slots.clear();
            let leased = inner
                .events
                .iter()
                .filter_map(|(id, event)| {
                    (event.kind == contract::MC_HOST_EVENT_HOST_CALL
                        && event.phase == EventPhase::Leased)
                        .then_some(*id)
                })
                .collect::<Vec<_>>();
            inner.events.retain(|_, event| {
                (event.phase == EventPhase::Leased && event.native_handle.is_some())
                    || event.phase == EventPhase::Cancelled
                    || (event.phase == EventPhase::Queued
                        && event.kind == contract::MC_HOST_EVENT_HOST_CALL_CLOSE)
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
        if !inner.routes.contains(name)
            || inner.events.len() >= MAX_EVENTS
            || inner
                .queued_bytes
                .checked_add(payload.len())
                .is_none_or(|total| total > MAX_EVENT_BYTES)
            || inner.next_bridge_handle == i32::MAX
            || inner.next_event_id == u64::MAX
        {
            return -1;
        }
        let handle = inner.next_bridge_handle;
        inner.next_bridge_handle += 1;
        let id = inner.next_event_id;
        inner.next_event_id += 1;
        inner.queued_bytes += payload.len();
        inner.events.insert(
            id,
            Event {
                kind: contract::MC_HOST_EVENT_HOST_CALL,
                payload,
                phase: EventPhase::Queued,
                native_handle: None,
                original_handle: None,
            },
        );
        inner.queue.push_back(id);
        inner.slots.insert(
            handle,
            Slot {
                event_id: id,
                result: None,
                offset: 0,
            },
        );
        handle
    }

    fn poll(&mut self, handle: i32) -> i32 {
        let Ok(inner) = self.0.inner.lock() else {
            return -1;
        };
        match inner.slots.get(&handle) {
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
        let Some(slot) = inner.slots.get_mut(&handle) else {
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
        let Some(slot) = inner.slots.remove(&handle) else {
            return;
        };
        match inner.events.get(&slot.event_id).map(|event| event.phase) {
            Some(EventPhase::Queued) => {
                inner.queue.retain(|id| *id != slot.event_id);
                if let Some(event) = inner.events.remove(&slot.event_id) {
                    inner.queued_bytes -= event.payload.len();
                }
            }
            Some(EventPhase::Leased) => inner.cancel_leased(slot.event_id),
            _ => {}
        }
    }
}
