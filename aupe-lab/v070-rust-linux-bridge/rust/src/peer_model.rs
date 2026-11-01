// SPDX-License-Identifier: GPL-2.0-only
//! Userspace reference for the AUPE kernel peer/registry/crypto-owner control
//! path. This intentionally does NOT claim to be a Rust-for-Linux module.
//! The AEAD owner must authenticate packets before calling `verified_rx`.
#![cfg(feature="std")]
use std::{collections::BTreeMap, sync::{Arc,Mutex}};
use core::{ffi::c_void, ptr};
use super::{RxGate, KeypairBank, CryptoBinding, CryptoRef,
            aupe_binding_init_rs, aupe_binding_install_rs,
            aupe_binding_resolve_rs,aupe_binding_close_rs,
            aupe_keypairs_install_rs,aupe_keypairs_clear_rs,
            aupe_gate_bind_rs,aupe_gate_close_rs};
use super::fragment::{AfState,PairMux,aupe_pair_mux_init_rs,
            aupe_pair_mux_rotate_rs,aupe_pair_mux_clear_rs};
use super::dispatch::aupe_dispatch_keypair_rs;

fn empty<T>() -> T {unsafe{std::mem::zeroed()}}

pub struct Peer {
    public_key: [u8;32],
    gate: RxGate,
    keypairs: KeypairBank,
    binding: CryptoBinding,
    pair: PairMux,
    // Each workspace is fixed and cannot move after initialization.
    workspace: [Box<AfState>;4],
    scratch: Vec<u8>,
    closed: bool,
}
impl Peer {
    pub fn new(key:[u8;32])->Result<Self,i32> {
        let mut work:[Box<AfState>;4]=std::array::from_fn(|_| Box::new(empty()));
        let mut pair:PairMux=empty();
        let [a,b,c,d]=&mut work;
        let r=unsafe{aupe_pair_mux_init_rs(&mut pair,&mut**a,&mut**b,&mut**c,&mut**d)};
        if r!=0{return Err(r)}
        let mut binding:CryptoBinding=empty();
        let r=unsafe{aupe_binding_init_rs(&mut binding,key.as_ptr())};
        if r!=0{return Err(r)}
        Ok(Self{public_key:key,gate:empty(),keypairs:empty(),binding,pair,
           workspace:work,scratch:vec![0;65535],closed:false})
    }
    pub fn install(&mut self,authenticated_pubkey:[u8;32],reference:CryptoRef,now:u64)->Result<u64,i32>{
        if self.closed{return Err(-10)}
        let mut assigned=0;
        let r=unsafe{aupe_binding_install_rs(&mut self.binding,authenticated_pubkey.as_ptr(),reference,now,&mut assigned)};
        if r==3{return Ok(assigned)}
        if r!=0{return Err(r)}
        let r=unsafe{aupe_keypairs_install_rs(&mut self.keypairs,assigned,now)};
        if r!=0 {self.close();return Err(r)}
        let r=unsafe{aupe_pair_mux_rotate_rs(&mut self.pair,assigned)};
        if r!=0 {self.close();return Err(r)}
        let r=unsafe{aupe_gate_bind_rs(&mut self.gate,assigned)};
        if r!=0 {self.close();return Err(r)}
        Ok(assigned)
    }
    /// Only a cryptographic caller that completed AEAD verification with the
    /// *same* pinned keypair reference may invoke this function.
    pub unsafe fn verified_rx(&mut self, reference:CryptoRef,counter:u64,now:u64,
                              payload:&[u8])->Result<Vec<u8>,i32> {
        if self.closed{return Err(-10)}
        let mut epoch=0;
        let r=unsafe{aupe_binding_resolve_rs(&mut self.binding,reference,now,&mut epoch)};
        if r!=0{return Err(r)}
        let mut delivered=Vec::<u8>::new();
        unsafe extern "C" fn sink(context:*mut c_void,packet:*const u8,n:u32)->i32 {
            let out=unsafe{&mut*(context as *mut Vec<u8>)};
            let packet=unsafe{std::slice::from_raw_parts(packet,n as usize)};
            out.extend_from_slice(packet);0
        }
        let r=unsafe{aupe_dispatch_keypair_rs(&self.gate,&mut self.pair,&mut self.keypairs,
                       epoch,counter,now,payload.as_ptr(),payload.len() as u32,
                       self.scratch.as_mut_ptr(),self.scratch.len() as u32,
                       Some(sink),&mut delivered as *mut _ as *mut c_void)};
        if r==1||r==2{return Ok(delivered)}
        Err(r)
    }
    pub fn close(&mut self) {
        if !self.closed {
            unsafe{aupe_binding_close_rs(&mut self.binding);
                aupe_pair_mux_clear_rs(&mut self.pair);
                aupe_keypairs_clear_rs(&mut self.keypairs);
                aupe_gate_close_rs(&mut self.gate)};
            self.scratch.fill(0); self.closed=true;
        }
    }
    pub fn authenticated_key(&self)->[u8;32]{self.public_key}
    pub fn workspace_count(&self)->usize{self.workspace.len()}
}
impl Drop for Peer {fn drop(&mut self){self.close();}}

/// Concurrent reference holder. Revocation unpublishes before closing; any
/// previously acquired Arc remains valid and sees closed on the next call.
pub struct Registry {
    peers: Mutex<BTreeMap<u64,Arc<Mutex<Peer>>>>,
    highest: std::sync::atomic::AtomicU64,
}
impl Registry {
    pub fn new()->Self{Self{peers:Mutex::new(BTreeMap::new()),
        highest:std::sync::atomic::AtomicU64::new(0)}}
    pub fn add(&self,key:[u8;32])->Result<u64,i32>{
        let p=Arc::new(Mutex::new(Peer::new(key)?));
        let mut map=self.peers.lock().map_err(|_|-1)?;
        if map.len()>=64{return Err(-11)}
        let id=self.highest.fetch_add(1,std::sync::atomic::Ordering::Relaxed)+1;
        map.insert(id,p);Ok(id)
    }
    pub fn acquire(&self,handle:u64)->Option<Arc<Mutex<Peer>>>{
        self.peers.lock().ok()?.get(&handle).cloned()
    }
    pub fn revoke(&self,handle:u64)->Result<(),i32>{
        let peer=self.peers.lock().map_err(|_|-1)?.remove(&handle).ok_or(-4)?;
        peer.lock().map_err(|_|-1)?.close();Ok(())
    }
    pub fn count(&self)->usize{self.peers.lock().map(|p|p.len()).unwrap_or(0)}
}
impl Default for Registry{fn default()->Self{Self::new()}}

#[cfg(test)]mod tests{
    use super::*;
    #[test]fn peer_identity_and_lifecycle(){
        let r=Registry::new();let key=[11u8;32];let id=r.add(key).unwrap();
        let p=r.acquire(id).unwrap();
        assert_eq!(p.lock().unwrap().authenticated_key(),key);
        assert_eq!(p.lock().unwrap().workspace_count(),4);
        r.revoke(id).unwrap();
        assert!(r.acquire(id).is_none());
        assert!(p.lock().unwrap().closed);
    }
    #[test]fn multiple_same_inner_ip_owners(){
        let r=Registry::new();let a=r.add([1;32]).unwrap();let b=r.add([2;32]).unwrap();
        assert_ne!(a,b);r.revoke(a).unwrap();assert!(r.acquire(b).is_some());
    }
}
