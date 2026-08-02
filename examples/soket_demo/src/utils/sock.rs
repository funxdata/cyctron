// src/utils/sock.rs
use std::ffi::CString;
use std::ptr::null_mut;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::sync::mpsc::{channel, Receiver, Sender};

use nng_sys::*;

pub struct NngReceiver {
    running: Arc<AtomicBool>,
    handle: Option<thread::JoinHandle<()>>,
    tx: Sender<String>,
    rx: Receiver<String>,
}

impl NngReceiver {
    pub fn new() -> Self {
        let (tx, rx) = channel();
        Self {
            running: Arc::new(AtomicBool::new(false)),
            handle: None,
            tx,
            rx,
        }
    }

    pub fn start(&mut self, url: &str) -> Result<(), String> {
        if self.running.load(Ordering::SeqCst) {
            return Err("Already running".to_string());
        }

        self.running.store(true, Ordering::SeqCst);
        let running = self.running.clone();
        let tx = self.tx.clone();
        let url = url.to_string();

        let handle = thread::spawn(move || {
            unsafe {
                let mut sock = nng_socket::default();
                
                if nng_sub0_open(&mut sock) != 0 {
                    eprintln!("[NNG] nng_sub0_open failed");
                    return;
                }

                nng_sub0_socket_subscribe(sock, std::ptr::null(), 0);

                let url_c = CString::new(url.as_str()).unwrap();
                if nng_dial(sock, url_c.as_ptr(), null_mut(), 0) != 0 {
                    eprintln!("[NNG] nng_dial {} failed", url);
                    nng_close(sock);
                    return;
                }

                println!("[NNG] ✅ Connected to {}", url);

                while running.load(Ordering::SeqCst) {
                    let mut msg: *mut nng_msg = null_mut();
                    let rv = nng_recvmsg(sock, &mut msg, 0);
                    
                    if rv == 0 && !msg.is_null() {
                        let body = nng_msg_body(msg);
                        let len = nng_msg_len(msg);
                        if !body.is_null() && len > 0 {
                            let data = std::slice::from_raw_parts(body as *const u8, len);
                            let _ = tx.send(String::from_utf8_lossy(data).to_string());
                        }
                        nng_msg_free(msg);
                    }
                }

                nng_close(sock);
                println!("[NNG] Receiver stopped");
            }
        });

        self.handle = Some(handle);
        Ok(())
    }

    pub fn stop(&mut self) {
        self.running.store(false, Ordering::SeqCst);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }

    pub fn try_recv(&self, timeout_ms: u64) -> Option<String> {
        match self.rx.recv_timeout(std::time::Duration::from_millis(timeout_ms)) {
            Ok(msg) => Some(msg),
            Err(_) => None,
        }
    }
}

impl Default for NngReceiver {
    fn default() -> Self {
        Self::new()
    }
}

