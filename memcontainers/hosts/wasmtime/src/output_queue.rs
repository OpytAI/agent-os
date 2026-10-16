//! Bounded terminal output for an externally driven VM. Sinks never block a
//! Wasmtime bridge call. Loss is represented by records in the same sequence.

use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

use crate::StreamSink;

pub const STDOUT: u32 = 1;
pub const STDERR: u32 = 2;
pub const LOG: u32 = 3;

#[derive(Clone, Debug)]
pub enum OutputRecord {
    Data {
        sequence: u64,
        channel: u32,
        byte_offset: u64,
        bytes: Arc<[u8]>,
    },
    Gap {
        sequence: u64,
        first_lost_sequence: u64,
        last_lost_sequence: u64,
        byte_start: u64,
        byte_end: u64,
    },
}

impl OutputRecord {
    pub fn sequence(&self) -> u64 {
        match self {
            Self::Data { sequence, .. } | Self::Gap { sequence, .. } => *sequence,
        }
    }

    fn byte_len(&self) -> usize {
        match self {
            Self::Data { bytes, .. } => bytes.len(),
            Self::Gap { .. } => 0,
        }
    }
}

struct Entry {
    record: OutputRecord,
    leased: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct OutputStats {
    pub retained_bytes: usize,
    pub dropped_bytes: u64,
    pub record_count: usize,
    pub leased_records: usize,
    pub above_high_water: bool,
    pub poisoned: bool,
}

struct State {
    records: VecDeque<Entry>,
    retained_bytes: usize,
    dropped_bytes: u64,
    next_sequence: u64,
    next_offset: u64,
    poisoned: bool,
}

impl State {
    fn record_loss(&mut self, bytes: usize) {
        match self.dropped_bytes.checked_add(bytes as u64) {
            Some(total) => self.dropped_bytes = total,
            None => {
                self.dropped_bytes = u64::MAX;
                self.poisoned = true;
            }
        }
    }
}

pub struct OutputQueue {
    state: Mutex<State>,
    max_bytes: usize,
    high_water_bytes: usize,
    max_records: usize,
    high_water_records: usize,
}

impl OutputQueue {
    pub fn new(max_bytes: usize, high_water_bytes: usize, max_records: usize) -> Arc<Self> {
        assert!(max_bytes > 0 && high_water_bytes < max_bytes && max_records > 1);
        Arc::new(Self {
            state: Mutex::new(State {
                records: VecDeque::new(),
                retained_bytes: 0,
                dropped_bytes: 0,
                next_sequence: 1,
                next_offset: 0,
                poisoned: false,
            }),
            max_bytes,
            high_water_bytes,
            max_records,
            high_water_records: max_records * 3 / 4,
        })
    }

    pub fn sink(self: &Arc<Self>, channel: u32) -> QueueSink {
        QueueSink {
            queue: Arc::clone(self),
            channel,
        }
    }

    pub fn stats(&self) -> OutputStats {
        let state = self.state.lock().unwrap_or_else(|error| error.into_inner());
        OutputStats {
            retained_bytes: state.retained_bytes,
            dropped_bytes: state.dropped_bytes,
            record_count: state.records.len(),
            leased_records: state.records.iter().filter(|entry| entry.leased).count(),
            above_high_water: state.retained_bytes >= self.high_water_bytes
                || state.records.len() >= self.high_water_records,
            poisoned: state.poisoned,
        }
    }

    /// Lease the oldest available record. A lease remains charged to the quota.
    pub fn next(&self) -> Option<OutputRecord> {
        let mut state = self.state.lock().unwrap_or_else(|error| error.into_inner());
        let entry = state.records.iter_mut().find(|entry| !entry.leased)?;
        entry.leased = true;
        Some(entry.record.clone())
    }

    pub fn release(&self, sequence: u64) -> bool {
        let mut state = self.state.lock().unwrap_or_else(|error| error.into_inner());
        if let Some(entry) = state
            .records
            .iter_mut()
            .find(|entry| entry.record.sequence() == sequence && entry.leased)
        {
            entry.leased = false;
            true
        } else {
            false
        }
    }

    pub fn ack(&self, sequence: u64) -> bool {
        let mut state = self.state.lock().unwrap_or_else(|error| error.into_inner());
        let Some(index) = state
            .records
            .iter()
            .position(|entry| entry.record.sequence() == sequence && entry.leased)
        else {
            return false;
        };
        let entry = state.records.remove(index).expect("validated queue index");
        state.retained_bytes -= entry.record.byte_len();
        true
    }

    fn write(&self, channel: u32, bytes: &[u8]) {
        if bytes.is_empty() {
            return;
        }
        let mut state = self.state.lock().unwrap_or_else(|error| error.into_inner());
        let Some(end_offset) = state.next_offset.checked_add(bytes.len() as u64) else {
            state.record_loss(bytes.len());
            state.poisoned = true;
            return;
        };
        let start_offset = state.next_offset;
        state.next_offset = end_offset;
        if state.poisoned {
            state.record_loss(bytes.len());
            return;
        }

        let desired = bytes.len().min(self.max_bytes);
        while state.retained_bytes.saturating_add(desired) > self.max_bytes {
            let Some(index) = state
                .records
                .iter()
                .position(|entry| !entry.leased && entry.record.byte_len() != 0)
            else {
                break;
            };
            let (sequence, byte_offset, lost) = match &state.records[index].record {
                OutputRecord::Data {
                    sequence,
                    byte_offset,
                    bytes,
                    ..
                } => (*sequence, *byte_offset, bytes.len()),
                OutputRecord::Gap { .. } => unreachable!("selected data record"),
            };
            state.records[index].record = OutputRecord::Gap {
                sequence,
                first_lost_sequence: sequence,
                last_lost_sequence: sequence,
                byte_start: byte_offset,
                byte_end: byte_offset + lost as u64,
            };
            state.retained_bytes -= lost;
            state.record_loss(lost);
        }

        let available = self.max_bytes - state.retained_bytes;
        let retained = bytes.len().min(available);
        let dropped_prefix = bytes.len() - retained;
        let needed_records = usize::from(dropped_prefix != 0) + usize::from(retained != 0);
        let Some(next_sequence) = state.next_sequence.checked_add(needed_records as u64) else {
            state.record_loss(bytes.len());
            state.poisoned = true;
            return;
        };
        if state.records.len().saturating_add(needed_records) > self.max_records {
            state.poisoned = true;
            state.record_loss(bytes.len());
            return;
        }
        if dropped_prefix != 0 {
            let sequence = state.next_sequence;
            state.next_sequence += 1;
            state.records.push_back(Entry {
                record: OutputRecord::Gap {
                    sequence,
                    first_lost_sequence: sequence,
                    last_lost_sequence: sequence,
                    byte_start: start_offset,
                    byte_end: start_offset + dropped_prefix as u64,
                },
                leased: false,
            });
            state.record_loss(dropped_prefix);
        }
        if retained != 0 {
            let sequence = state.next_sequence;
            state.records.push_back(Entry {
                record: OutputRecord::Data {
                    sequence,
                    channel,
                    byte_offset: start_offset + dropped_prefix as u64,
                    bytes: Arc::from(&bytes[dropped_prefix..]),
                },
                leased: false,
            });
            state.retained_bytes += retained;
        }
        state.next_sequence = next_sequence;
    }
}

pub struct QueueSink {
    queue: Arc<OutputQueue>,
    channel: u32,
}

impl StreamSink for QueueSink {
    fn write(&mut self, bytes: &[u8]) {
        self.queue.write(self.channel, bytes);
    }
}
