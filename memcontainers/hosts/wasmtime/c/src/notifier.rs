//! Coalesced wake hints. The callback runs only on this notifier's thread.

use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc, Mutex};
use std::thread::{self, JoinHandle};

use embedder_rust as contract;

pub type WakeCallback = unsafe extern "C" fn(*mut c_void);

enum Signal {
    Wake,
    Stop,
}

pub struct Notifier {
    sender: mpsc::SyncSender<Signal>,
    outstanding: Arc<AtomicBool>,
    closed: AtomicBool,
    thread: Mutex<Option<JoinHandle<()>>>,
}

impl Notifier {
    pub fn new(wake: WakeCallback, user_data: usize) -> Result<Arc<Self>, u32> {
        let (sender, receiver) = mpsc::sync_channel(1);
        let outstanding = Arc::new(AtomicBool::new(false));
        let worker_outstanding = Arc::clone(&outstanding);
        let thread = thread::Builder::new()
            .name("mc-host-wake".to_owned())
            .spawn(move || {
                while let Ok(signal) = receiver.recv() {
                    match signal {
                        Signal::Wake => {
                            unsafe { wake(user_data as *mut c_void) };
                            worker_outstanding.store(false, Ordering::Release);
                        }
                        Signal::Stop => break,
                    }
                }
            })
            .map_err(|_| contract::MC_HOST_RESOURCE_LIMIT)?;
        Ok(Arc::new(Self {
            sender,
            outstanding,
            closed: AtomicBool::new(false),
            thread: Mutex::new(Some(thread)),
        }))
    }

    pub fn notify(&self) {
        if self.closed.load(Ordering::Acquire)
            || self
                .outstanding
                .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
                .is_err()
        {
            return;
        }
        if self.sender.try_send(Signal::Wake).is_err() {
            self.outstanding.store(false, Ordering::Release);
        }
    }

    pub fn idle(&self) -> bool {
        !self.outstanding.load(Ordering::Acquire)
    }

    pub fn shutdown(&self) {
        self.closed.store(true, Ordering::Release);
        let _ = self.sender.try_send(Signal::Stop);
        if let Ok(mut thread) = self.thread.lock() {
            if let Some(thread) = thread.take() {
                let _ = thread.join();
            }
        }
    }
}

impl Drop for Notifier {
    fn drop(&mut self) {
        self.shutdown();
    }
}
