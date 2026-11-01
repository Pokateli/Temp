// SPDX-License-Identifier: GPL-2.0-only
//! AUPE 0.5.69: allocator-free, `no_std` post-AEAD peer state machine.
//!
//! This is NOT a Linux module. A future Rust-for-Linux adapter must provide:
//! * an authenticated, pinned Noise keypair + confirmed public key;
//! * ownership of four stable, zero-initialized AfState allocations;
//! * mutual exclusion for the whole call, INCLUDING the emit callback;
//! * lifetime/RCU/refcount and netns scoping; no raw pointer is wire-supplied.
//! All data accepted by `verified_rx` must already have passed AEAD.
//! Public C functions are intentionally suffixed `_rs`; never silently override
//! existing C symbols until a matching kernel build and ABI test is available.
use core::{ffi::c_void, mem, ptr};
use super::{CryptoBinding, CryptoRef, KeypairBank, RxGate,
    aupe_binding_init_rs, aupe_binding_install_rs, aupe_binding_resolve_rs,
    aupe_binding_retire_previous_rs, aupe_binding_close_rs,
    aupe_gate_bind_rs, aupe_gate_close_rs,
    aupe_keypairs_install_rs, aupe_keypairs_clear_rs,
    aupe_keypairs_retire_previous_rs};
use super::fragment::{AfState, PairMux, aupe_pair_mux_init_rs,
    aupe_pair_mux_rotate_rs, aupe_pair_mux_retire_previous_rs,
    aupe_pair_mux_clear_rs};
use super::dispatch::{Emit, aupe_dispatch_keypair_rs};

const AF_WAIT: i32 = 0;
const AF_ALREADY_BOUND: i32 = 3;
const AF_BAD: i32 = -1;
const AF_STALE_EPOCH: i32 = -4;
const AF_CLOSED: i32 = -10;

#[repr(C)]
pub struct KernelPeerCore {
    gate: RxGate,
    keypairs: KeypairBank,
    binding: CryptoBinding,
    pair: PairMux,
    initialized: u8,
    closed: u8,
}

/// Use these queries instead of duplicating Rust layout in C. The allocation
/// must be aligned, zeroed, stable and protected by an external per-peer lock.
#[unsafe(no_mangle)]
pub extern "C" fn aupe_rs_peer_size() -> usize {mem::size_of::<KernelPeerCore>()}
#[unsafe(no_mangle)]
pub extern "C" fn aupe_rs_peer_align() -> usize {mem::align_of::<KernelPeerCore>()}
#[unsafe(no_mangle)]
pub extern "C" fn aupe_rs_workspace_size() -> usize {mem::size_of::<AfState>()}
#[unsafe(no_mangle)]
pub extern "C" fn aupe_rs_workspace_align() -> usize {mem::align_of::<AfState>()}

/// # Safety
/// `p` points to an exclusive, correctly aligned allocation of `size()` bytes.
/// All four AfState pointers are distinct, stable, valid, zeroed and remain
/// alive until the owner has closed this object and quiesced every caller.
/// `key` points to 32 bytes from a *completed trusted Noise handshake*.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_rs_peer_init(
    p: *mut KernelPeerCore, key: *const u8,
    a: *mut AfState, b: *mut AfState, c: *mut AfState, d: *mut AfState,
) -> i32 {
    if p.is_null() || key.is_null() {return AF_BAD}
    let ws = [a,b,c,d];
    for i in 0..4 {
        if ws[i].is_null() {return AF_BAD}
        for j in 0..i {if ws[j] == ws[i] {return AF_BAD}}
    }
    unsafe {ptr::write_bytes(p,0,1)};
    let s=unsafe{&mut *p};
    let r=unsafe{aupe_pair_mux_init_rs(&mut s.pair,a,b,c,d)};
    if r!=AF_WAIT{return r}
    let r=unsafe{aupe_binding_init_rs(&mut s.binding,key)};
    if r!=AF_WAIT {
        unsafe{aupe_pair_mux_clear_rs(&mut s.pair)};
        return r
    }
    s.initialized=1;
    AF_WAIT
}

/// Fail closed on *any* partial rekey failure. This differs from the old
/// userspace model, which could leave a new binding visible after a failed
/// keypair-bank or mux update. No operation may recover a closed owner.
/// # Safety: caller holds the same lock as all other operations on this peer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_rs_peer_install(
    p:*mut KernelPeerCore, authenticated_pubkey:*const u8,
    reference:CryptoRef, now:u64, out_epoch:*mut u64,
)->i32{
    if !out_epoch.is_null(){unsafe{*out_epoch=0}}
    if p.is_null()||authenticated_pubkey.is_null()||out_epoch.is_null(){return AF_BAD}
    let s=unsafe{&mut *p};
    if s.initialized!=1||s.closed!=0{return AF_CLOSED}
    let mut epoch=0;
    let rv=unsafe{aupe_binding_install_rs(&mut s.binding,authenticated_pubkey,reference,now,&mut epoch)};
    if rv==AF_ALREADY_BOUND {
        unsafe{*out_epoch=epoch};return AF_WAIT
    }
    if rv!=AF_WAIT{return rv}
    let a=unsafe{aupe_keypairs_install_rs(&mut s.keypairs,epoch,now)};
    let b=if a==AF_WAIT {unsafe{aupe_pair_mux_rotate_rs(&mut s.pair,epoch)}} else {a};
    let c=if b==AF_WAIT {unsafe{aupe_gate_bind_rs(&mut s.gate,epoch)}} else {b};
    if c!=AF_WAIT {
        unsafe{aupe_rs_peer_close(p)};
        return c
    }
    unsafe{*out_epoch=epoch};
    AF_WAIT
}

/// # Safety: only a caller that has *already completed* AEAD verification
/// against this exact pinned `reference` may call. The caller serializes
/// installation, RX, retirement and close. In particular, no overlap is
/// allowed between plaintext, workspace, scratch and owner state, and `emit`
/// must not recursively call back into the same peer while its lock is held.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_rs_peer_verified_rx(
    p:*mut KernelPeerCore, reference:CryptoRef,counter:u64,now:u64,
    plaintext:*const u8,n:u32,scratch:*mut u8,capacity:u32,
    emit:Emit,ctx:*mut c_void,
)->i32{
    if p.is_null(){return AF_BAD}
    let s=unsafe{&mut*p};
    if s.initialized!=1||s.closed!=0{return AF_CLOSED}
    let mut epoch=0;
    let rv=unsafe{aupe_binding_resolve_rs(&mut s.binding,reference,now,&mut epoch)};
    if rv!=AF_WAIT{return rv}
    unsafe{aupe_dispatch_keypair_rs(&s.gate,&mut s.pair,&mut s.keypairs,
          epoch,counter,now,plaintext,n,scratch,capacity,emit,ctx)}
}

/// # Safety: externally serialized as for `install`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_rs_peer_retire_previous(
    p:*mut KernelPeerCore,reference:CryptoRef,
)->i32{
    if p.is_null(){return AF_BAD}
    let s=unsafe{&mut*p};
    if s.initialized!=1||s.closed!=0{return AF_CLOSED}
    let slot=&s.binding.previous;
    if slot.valid==0||slot.r.keypair!=reference.keypair||slot.r.generation!=reference.generation {
        return AF_STALE_EPOCH
    }
    let rv=unsafe{aupe_keypairs_retire_previous_rs(&mut s.keypairs,slot.epoch)};
    if rv!=AF_WAIT{unsafe{aupe_rs_peer_close(p)};return rv}
    unsafe{aupe_pair_mux_retire_previous_rs(&mut s.pair)};
    let rv=unsafe{aupe_binding_retire_previous_rs(&mut s.binding,reference)};
    if rv!=AF_WAIT{unsafe{aupe_rs_peer_close(p)}}
    rv
}

/// # Safety: caller prevents any new operation, quiesces existing callers and
/// keeps all four workspaces alive until this call finishes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_rs_peer_close(p:*mut KernelPeerCore){
    if p.is_null(){return}
    let s=unsafe{&mut*p};
    if s.initialized!=1||s.closed!=0{return}
    unsafe{
        aupe_binding_close_rs(&mut s.binding);
        aupe_pair_mux_clear_rs(&mut s.pair);
        aupe_keypairs_clear_rs(&mut s.keypairs);
        aupe_gate_close_rs(&mut s.gate);
    }
    s.closed=1;
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{boxed::Box,vec};
    fn zero<T>()->T {unsafe{mem::zeroed()}}
    fn setup() -> (Box<KernelPeerCore>,[Box<AfState>;4],Box<u8>,Box<u8>) {
        let mut ws:[Box<AfState>;4]=core::array::from_fn(|_|Box::new(zero()));
        let mut p:Box<KernelPeerCore>=Box::new(zero());
        let k1=Box::new(1u8);let k2=Box::new(2u8);
        let status=unsafe{aupe_rs_peer_init(&mut *p,[7u8;32].as_ptr(),
            &mut *ws[0],&mut *ws[1],&mut *ws[2],&mut *ws[3])};
        assert_eq!(status,0);
        (p,ws,k1,k2)
    }
    fn ref_of(p:&u8,generation:u64)->CryptoRef { CryptoRef{keypair:p as *const u8 as *const c_void,generation} }
    fn ipv4()->[u8;28]{
        let mut p=[0u8;28];p[0]=0x45;p[3]=28;p[8]=64;p[9]=17;
        p[12..16].copy_from_slice(&[172,16,2,2]);p[16..20].copy_from_slice(&[172,16,2,2]);
        p[20..28].copy_from_slice(b"AUPE069!");
        let mut sum:u32=p[..20].chunks_exact(2).map(|v|u16::from_be_bytes([v[0],v[1]]) as u32).sum();
        while sum>>16!=0{sum=(sum&65535)+(sum>>16)}
        p[10..12].copy_from_slice(&(!(sum as u16)).to_be_bytes());p
    }
    unsafe extern "C" fn sink(ctx:*mut c_void,p:*const u8,n:u32)->i32 {
        let out=unsafe{&mut*(ctx as *mut Vec<u8>)};
        out.extend_from_slice(unsafe{core::slice::from_raw_parts(p,n as usize)});0
    }
    #[test]
    fn verified_rx_rekey_replay_retire_revoke(){
        let (mut p,_ws,k1,k2)=setup();
        let r1=ref_of(&k1,7);let r2=ref_of(&k2,8);
        let mut epoch=0;
        assert_eq!(unsafe{aupe_rs_peer_install(&mut*p,[6u8;32].as_ptr(),r1,100,&mut epoch)},AF_BAD);
        assert_eq!(epoch,0);
        assert_eq!(unsafe{aupe_rs_peer_install(&mut*p,[7u8;32].as_ptr(),r1,100,&mut epoch)},0);
        assert_eq!(epoch,1);
        assert_eq!(unsafe{aupe_rs_peer_install(&mut*p,[7u8;32].as_ptr(),r1,101,&mut epoch)},0);
        assert_eq!(epoch,1);
        let packet=ipv4();let mut scratch=vec![0u8;65535];let mut got:Vec<u8>=vec![];
        let r=unsafe{aupe_rs_peer_verified_rx(&mut*p,r1,11,102,packet.as_ptr(),28,
                 scratch.as_mut_ptr(),65535,Some(sink),&mut got as *mut _ as *mut c_void)};
        assert_eq!(r,2);assert_eq!(got,packet);
        assert_eq!(unsafe{aupe_rs_peer_verified_rx(&mut*p,r1,11,103,packet.as_ptr(),28,
            scratch.as_mut_ptr(),65535,Some(sink),&mut got as *mut _ as *mut c_void)},-13);
        assert_eq!(unsafe{aupe_rs_peer_install(&mut*p,[7u8;32].as_ptr(),r2,200,&mut epoch)},0);
        assert_eq!(epoch,2);
        got.clear();
        assert_eq!(unsafe{aupe_rs_peer_verified_rx(&mut*p,r1,12,201,packet.as_ptr(),28,
            scratch.as_mut_ptr(),65535,Some(sink),&mut got as *mut _ as *mut c_void)},2);
        assert_eq!(got,packet);
        assert_eq!(unsafe{aupe_rs_peer_retire_previous(&mut*p,r1)},0);
        assert_eq!(unsafe{aupe_rs_peer_verified_rx(&mut*p,r1,13,202,packet.as_ptr(),28,
            scratch.as_mut_ptr(),65535,Some(sink),&mut got as *mut _ as *mut c_void)},-4);
        unsafe{aupe_rs_peer_close(&mut *p)};
        assert_eq!(unsafe{aupe_rs_peer_install(&mut*p,[7u8;32].as_ptr(),r2,203,&mut epoch)},-10);
        assert_eq!(unsafe{aupe_rs_peer_verified_rx(&mut*p,r2,13,204,packet.as_ptr(),28,
            scratch.as_mut_ptr(),65535,Some(sink),&mut got as *mut _ as *mut c_void)},-10);
        unsafe{aupe_rs_peer_close(&mut *p)};
    }
    #[test]
    fn reject_workspace_alias_and_untrusted_null(){
        let mut p:KernelPeerCore=zero();let mut w:Box<AfState>=Box::new(zero());
        assert_eq!(unsafe{aupe_rs_peer_init(&mut p,[1u8;32].as_ptr(),&mut*w,&mut*w,&mut*w,&mut*w)},AF_BAD);
        assert_eq!(unsafe{aupe_rs_peer_init(&mut p,ptr::null(),&mut*w,&mut*w,&mut*w,&mut*w)},AF_BAD);
        assert!(aupe_rs_peer_size()>2000);assert!(aupe_rs_peer_align()>=8);
    }
}