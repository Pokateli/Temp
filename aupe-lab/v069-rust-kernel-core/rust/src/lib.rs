// SPDX-License-Identifier: GPL-2.0-only
// AUPE v0.5.67: independently translated Rust reference of five C components.
// ONLY userspace C/Rust FFI benchmark harness is supported here. Not a Rust kernel .ko.
#![cfg_attr(not(feature = "std"), no_std)]
use core::{ffi::c_void, ptr};

const AF_WAIT: i32 = 0;
const AF_BAD: i32 = -1;
const AF_STALE_EPOCH: i32 = -4;
const AF_CLOSED: i32 = -10;
const AF_ALREADY_BOUND: i32 = 3;
const REPLAY_DUPLICATE: i32 = -13;
const REPLAY_LIMIT: i32 = -14;
const REPLAY_WINDOW: u64 = 8192;
const REPLAY_MAX: u64 = u64::MAX - REPLAY_WINDOW;
const HANDLE_SLOTS: usize = 64;
const HANDLE_GEN_MAX: u64 = u64::MAX >> 8;
const GRACE_MS: u64 = 180_000;

#[inline(never)]
unsafe fn volatile_wipe<T>(p: *mut T) {
    // Mirrors C volatile byte loop, including padding, and avoids optimizer elision.
    let byte = p.cast::<u8>();
    for i in 0..core::mem::size_of::<T>() {
        unsafe { ptr::write_volatile(byte.add(i), 0) };
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ReplayWindow {
    seen: [u64; 128], epoch: u64, highest: u64, valid: u8, initialized: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct KeypairSlot { replay: ReplayWindow, epoch: u64, valid: u8 }
#[repr(C)]
pub struct KeypairBank {
    active: KeypairSlot, previous: KeypairSlot,
    high_watermark: u64, previous_deadline_ms: u64, last_now_ms: u64,
    clock_started: u8, enabled: u8,
}
#[repr(C)]
pub struct RxGate { epoch: u64, high_watermark: u64, state: u8 }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct HandleSlot { peer: *mut c_void, generation: u64 }
#[repr(C)]
pub struct HandleTable { slots: [HandleSlot; HANDLE_SLOTS] }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CryptoRef { pub keypair: *const c_void, pub generation: u64 }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BindingSlot { r: CryptoRef, epoch: u64, valid: u8 }
#[repr(C)]
pub struct CryptoBinding {
    authenticated_public_key: [u8; 32], active: BindingSlot, previous: BindingSlot,
    highest_owner_generation: u64, highest_aupe_epoch: u64,
    previous_deadline_ms: u64, last_clock_ms: u64,
    initialized: u8, closed: u8, clock_started: u8,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_replay_clear_rs(w: *mut ReplayWindow) {
    if !w.is_null() { unsafe { volatile_wipe(w) }; }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_replay_bind_rs(w: *mut ReplayWindow, epoch: u64) {
    if w.is_null() { return; }
    unsafe { aupe_replay_clear_rs(w) };
    if epoch != 0 {
        unsafe { (*w).epoch = epoch; (*w).valid = 1; }
    }
}
#[inline]
fn mark(w: &mut ReplayWindow, counter: u64) {
    let slot = (counter % REPLAY_WINDOW) as usize;
    w.seen[slot / 64] |= 1u64 << (slot % 64);
}
#[inline]
fn marked(w: &ReplayWindow, counter: u64) -> bool {
    let slot = (counter % REPLAY_WINDOW) as usize;
    (w.seen[slot / 64] & (1u64 << (slot % 64))) != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_replay_accept_rs(p: *mut ReplayWindow, epoch: u64, counter: u64) -> i32 {
    if p.is_null() || epoch == 0 { return AF_STALE_EPOCH; }
    let w = unsafe { &mut *p };
    if w.valid == 0 || w.epoch != epoch { return AF_STALE_EPOCH; }
    if counter >= REPLAY_MAX { return REPLAY_LIMIT; }
    if w.initialized == 0 {
        w.initialized = 1; w.highest = counter; mark(w, counter);
        return AF_WAIT;
    }
    if counter <= w.highest {
        if w.highest - counter >= REPLAY_WINDOW || marked(w, counter) { return REPLAY_DUPLICATE; }
        mark(w, counter);
        return AF_WAIT;
    }
    let delta = counter - w.highest;
    if delta >= REPLAY_WINDOW { w.seen.fill(0); }
    else {
        for k in 1..=delta {
            let slot = ((w.highest + k) % REPLAY_WINDOW) as usize;
            w.seen[slot / 64] &= !(1u64 << (slot % 64));
        }
    }
    w.highest = counter; mark(w, counter);
    AF_WAIT
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_gate_bind_rs(p: *mut RxGate, epoch: u64) -> i32 {
    if p.is_null() || epoch == 0 { return AF_BAD; }
    let g = unsafe { &mut *p };
    if g.state == 2 { return AF_CLOSED; }
    if g.state == 1 && g.epoch == epoch { return AF_ALREADY_BOUND; }
    if epoch <= g.high_watermark { return AF_STALE_EPOCH; }
    g.high_watermark = epoch; g.epoch = epoch; g.state = 1;
    AF_WAIT
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_gate_unbind_rs(p: *mut RxGate) {
    if !p.is_null() { let g = unsafe { &mut *p }; if g.state != 2 { g.epoch = 0; g.state = 0; } }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_gate_check_rs(p: *const RxGate, epoch: u64) -> i32 {
    if p.is_null() { return AF_BAD; }
    let g = unsafe { &*p };
    if g.state == 2 { return AF_CLOSED; }
    if g.state == 1 && epoch != 0 && epoch == g.epoch { AF_WAIT } else { AF_STALE_EPOCH }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_gate_close_rs(p: *mut RxGate) {
    if !p.is_null() { let g = unsafe { &mut *p }; g.epoch = 0; g.high_watermark = 0; g.state = 2; }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_init_rs(p: *mut HandleTable) {
    if !p.is_null() { unsafe { ptr::write_bytes(p, 0, 1) }; }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_init_seed_rs(p: *mut HandleTable, seed: u64) {
    if p.is_null() { return; }
    unsafe { aupe_handles_init_rs(p) };
    for s in unsafe { &mut (*p).slots } { s.generation = seed & 0x0000_ffff_ffff_ffff; }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_publish_rs(p: *mut HandleTable, peer: *mut c_void, out: *mut u64) -> i32 {
    if p.is_null() || peer.is_null() || out.is_null() { return AF_BAD; }
    unsafe { *out = 0; }
    for (i, s) in unsafe { &mut (*p).slots }.iter_mut().enumerate() {
        if !s.peer.is_null() || s.generation == HANDLE_GEN_MAX { continue; }
        s.generation += 1; s.peer = peer;
        unsafe { *out = (s.generation << 8) | ((i + 1) as u64); }
        return AF_WAIT;
    }
    -11
}
fn decode_handle(handle: u64) -> Option<usize> {
    let low = (handle & 255) as usize;
    if low == 0 || low > HANDLE_SLOTS || handle >> 8 == 0 { None } else { Some(low - 1) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_lookup_rs(p: *const HandleTable, handle: u64) -> *mut c_void {
    if p.is_null() { return ptr::null_mut(); }
    if let Some(index) = decode_handle(handle) {
        let s = &unsafe { &(*p).slots }[index];
        if s.generation == handle >> 8 { return s.peer; }
    }
    ptr::null_mut()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_remove_rs(p: *mut HandleTable, handle: u64) -> *mut c_void {
    if p.is_null() { return ptr::null_mut(); }
    if let Some(index) = decode_handle(handle) {
        let s = &mut unsafe { &mut (*p).slots }[index];
        if s.generation == handle >> 8 {
            let peer = s.peer; s.peer = ptr::null_mut(); return peer;
        }
    }
    ptr::null_mut()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_handles_drain_rs(p: *mut HandleTable, index: u32) -> *mut c_void {
    if p.is_null() || index as usize >= HANDLE_SLOTS { return ptr::null_mut(); }
    let s = &mut unsafe { &mut (*p).slots }[index as usize];
    let peer = s.peer; s.peer = ptr::null_mut(); peer
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_keypairs_clear_rs(p: *mut KeypairBank) {
    if !p.is_null() { unsafe { volatile_wipe(p) }; }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_keypairs_install_rs(p: *mut KeypairBank, epoch: u64, now: u64) -> i32 {
    if p.is_null() || epoch == 0 || now > u64::MAX - GRACE_MS { return AF_BAD; }
    let b = unsafe { &mut *p };
    if b.clock_started != 0 && now < b.last_now_ms { return AF_BAD; }
    if b.active.valid != 0 && b.active.epoch == epoch { return AF_ALREADY_BOUND; }
    if epoch <= b.high_watermark { return AF_STALE_EPOCH; }
    unsafe { volatile_wipe(&mut b.previous as *mut _) };
    if b.active.valid != 0 {
        b.previous = b.active; b.previous_deadline_ms = now + GRACE_MS;
    } else { b.previous_deadline_ms = 0; }
    unsafe { volatile_wipe(&mut b.active as *mut _) };
    unsafe { aupe_replay_bind_rs(&mut b.active.replay, epoch) };
    b.active.epoch = epoch; b.active.valid = 1;
    b.high_watermark = epoch; b.last_now_ms = now; b.clock_started = 1; b.enabled = 1;
    AF_WAIT
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_keypairs_accept_rs(p: *mut KeypairBank, epoch: u64, counter: u64, now: u64) -> i32 {
    if p.is_null() || epoch == 0 { return AF_STALE_EPOCH; }
    let b = unsafe { &mut *p };
    if b.enabled == 0 { return AF_STALE_EPOCH; }
    if b.clock_started != 0 && now < b.last_now_ms { return AF_BAD; }
    b.last_now_ms = now;
    if b.previous.valid != 0 && now >= b.previous_deadline_ms {
        unsafe { volatile_wipe(&mut b.previous as *mut _) };
        b.previous_deadline_ms = 0;
    }
    let slot = if b.active.valid != 0 && b.active.epoch == epoch { &mut b.active }
               else if b.previous.valid != 0 && b.previous.epoch == epoch { &mut b.previous }
               else { return AF_STALE_EPOCH; };
    unsafe { aupe_replay_accept_rs(&mut slot.replay, epoch, counter) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_keypairs_retire_previous_rs(p: *mut KeypairBank, epoch: u64) -> i32 {
    if p.is_null() || epoch == 0 { return AF_BAD; }
    let b = unsafe { &mut *p };
    if b.enabled == 0 { return AF_BAD; }
    if b.previous.valid == 0 || b.previous.epoch != epoch { return AF_STALE_EPOCH; }
    unsafe { volatile_wipe(&mut b.previous as *mut _) };
    b.previous_deadline_ms = 0;
    AF_WAIT
}

#[inline]
fn same_ref(a: CryptoRef, b: CryptoRef) -> bool { a.keypair == b.keypair && a.generation == b.generation }
#[inline]
fn valid_ref(r: CryptoRef) -> bool { !r.keypair.is_null() && r.generation != 0 }
fn pubkey_equal(a: &[u8; 32], b: &[u8; 32]) -> bool {
    let mut difference = 0u8;
    for i in 0..32 { difference |= a[i] ^ b[i]; }
    difference == 0
}
#[inline]
fn clock_ok(b: &CryptoBinding, now: u64) -> bool { b.clock_started == 0 || now >= b.last_clock_ms }
fn advance(b: &mut CryptoBinding, now: u64) {
    b.clock_started = 1; b.last_clock_ms = now;
    if b.previous.valid != 0 && now >= b.previous_deadline_ms {
        unsafe { volatile_wipe(&mut b.previous as *mut _) };
        b.previous_deadline_ms = 0;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_binding_init_rs(p: *mut CryptoBinding, key: *const u8) -> i32 {
    if p.is_null() || key.is_null() { return AF_BAD; }
    let b = unsafe { &mut *p };
    if b.initialized != 0 || b.closed != 0 { return AF_BAD; }
    let key_bytes = unsafe { core::slice::from_raw_parts(key, 32) };
    if !key_bytes.iter().any(|x| *x != 0) { return AF_BAD; }
    let mut saved = [0u8; 32]; saved.copy_from_slice(key_bytes);
    unsafe { ptr::write_bytes(p, 0, 1) };
    b.authenticated_public_key.copy_from_slice(&saved);
    b.initialized = 1;
    AF_WAIT
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_binding_install_rs(p: *mut CryptoBinding, key: *const u8, r: CryptoRef, now: u64, out: *mut u64) -> i32 {
    if !out.is_null() { unsafe { *out = 0 }; }
    if p.is_null() { return AF_CLOSED; }
    let b = unsafe { &mut *p };
    if b.initialized == 0 || b.closed != 0 { return AF_CLOSED; }
    if out.is_null() || key.is_null() || !valid_ref(r) || !clock_ok(b, now) { return AF_BAD; }
    let key_arr = unsafe { &*(key as *const [u8; 32]) };
    if !pubkey_equal(&b.authenticated_public_key, key_arr) { return AF_BAD; }
    if b.active.valid != 0 && same_ref(r, b.active.r) {
        advance(b, now); unsafe { *out = b.active.epoch; } return AF_ALREADY_BOUND;
    }
    if r.generation <= b.highest_owner_generation { return AF_STALE_EPOCH; }
    if (b.active.valid != 0 && b.active.r.keypair == r.keypair) ||
       (b.previous.valid != 0 && b.previous.r.keypair == r.keypair) ||
       b.highest_aupe_epoch == u64::MAX || now > u64::MAX - GRACE_MS { return AF_BAD; }
    advance(b, now);
    unsafe { volatile_wipe(&mut b.previous as *mut _) };
    if b.active.valid != 0 {
        b.previous = b.active; b.previous_deadline_ms = now + GRACE_MS;
    } else { b.previous_deadline_ms = 0; }
    b.highest_aupe_epoch += 1; b.highest_owner_generation = r.generation;
    b.active.r = r; b.active.epoch = b.highest_aupe_epoch; b.active.valid = 1;
    unsafe { *out = b.active.epoch; }
    AF_WAIT
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_binding_resolve_rs(p: *mut CryptoBinding, r: CryptoRef, now: u64, out: *mut u64) -> i32 {
    if !out.is_null() { unsafe { *out = 0; } }
    if p.is_null() { return AF_CLOSED; }
    let b = unsafe { &mut *p };
    if b.initialized == 0 || b.closed != 0 { return AF_CLOSED; }
    if out.is_null() || !valid_ref(r) || !clock_ok(b, now) { return AF_BAD; }
    advance(b, now);
    if b.active.valid != 0 && same_ref(r, b.active.r) {
        unsafe { *out = b.active.epoch; } return AF_WAIT;
    }
    if b.previous.valid != 0 && same_ref(r, b.previous.r) {
        unsafe { *out = b.previous.epoch; } return AF_WAIT;
    }
    AF_STALE_EPOCH
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_binding_retire_previous_rs(p: *mut CryptoBinding, r: CryptoRef) -> i32 {
    if p.is_null() { return AF_CLOSED; }
    let b = unsafe { &mut *p };
    if b.initialized == 0 || b.closed != 0 { return AF_CLOSED; }
    if !valid_ref(r) || b.previous.valid == 0 || !same_ref(r, b.previous.r) { return AF_STALE_EPOCH; }
    unsafe { volatile_wipe(&mut b.previous as *mut _) }; b.previous_deadline_ms = 0;
    AF_WAIT
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_binding_close_rs(p: *mut CryptoBinding) {
    if p.is_null() { return; }
    let b = unsafe { &mut *p };
    unsafe { volatile_wipe(&mut b.active as *mut _) };
    unsafe { volatile_wipe(&mut b.previous as *mut _) };
    for k in &mut b.authenticated_public_key { unsafe { ptr::write_volatile(k, 0) } }
    b.highest_owner_generation = 0; b.highest_aupe_epoch = 0;
    b.previous_deadline_ms = 0; b.last_clock_ms = 0;
    b.initialized = 0; b.closed = 1;
}

// Cross-check actual C/Rust layout before comparing or enabling a mixed build.
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_replay() -> usize { core::mem::size_of::<ReplayWindow>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_gate() -> usize { core::mem::size_of::<RxGate>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_handles() -> usize { core::mem::size_of::<HandleTable>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_keypairs() -> usize { core::mem::size_of::<KeypairBank>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_binding() -> usize { core::mem::size_of::<CryptoBinding>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_alignof_binding() -> usize { core::mem::align_of::<CryptoBinding>() }
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_alignof_keypairs() -> usize { core::mem::align_of::<KeypairBank>() }

mod fragment;
mod dispatch;
#[cfg(feature="std")]
pub mod peer_model;

// AUPE v0.5.69: allocator-free kernel-facing state/crypto-owner coordination.
// Synchronization, lifetimes and AEAD are STILL external responsibilities.
pub mod kernel_core;
