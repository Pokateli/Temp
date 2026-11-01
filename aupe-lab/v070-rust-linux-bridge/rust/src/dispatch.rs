// SPDX-License-Identifier: GPL-2.0-only
//! Post-AEAD dispatch. Only a cryptographic owner may call this interface.
//! Holding the authenticated peer's lock for the whole callback is REQUIRED.
use core::{ffi::c_void, ptr, slice};
use super::{RxGate, ReplayWindow, KeypairBank, aupe_gate_check_rs,
    aupe_replay_accept_rs, aupe_keypairs_accept_rs};
use super::fragment::{AfMux, PairMux, af_mux_feed_rs, af_mux_take_rs, aupe_pair_mux_select_rs};
const AF_WAIT:i32=0;const AF_DONE:i32=1;const AF_BYPASS:i32=2;const AF_BAD:i32=-1;
const AF_STALE:i32=-4;const AF_CLOSED:i32=-10;const MAX_PACKET:u32=65535;
pub type Emit = Option<unsafe extern "C" fn(*mut c_void,*const u8,u32)->i32>;
fn overlap(a:*const u8,an:usize,b:*mut u8,bn:usize)->bool{
    let aa=a as usize;let bb=b as usize;
    if aa<=bb {bb-aa<an}else{aa-bb<bn}
}
unsafe fn dispatch(gate:*const RxGate,mux:*mut AfMux,replay:*mut ReplayWindow,
    bank:*mut KeypairBank,key_epoch:u64,gate_epoch:u64,mux_epoch:u64,
    counter:u64,now:u64,packet:*const u8,n:u32,scratch:*mut u8,
    capacity:u32,emit:Emit,ctx:*mut c_void)->i32 {
    if gate.is_null()||mux.is_null()||packet.is_null()||scratch.is_null()||emit.is_none()
       ||n==0||n>MAX_PACKET||capacity<MAX_PACKET||overlap(packet,n as usize,scratch,capacity as usize){return AF_BAD}
    let r=unsafe{aupe_gate_check_rs(gate,gate_epoch)};
    if r!=AF_WAIT{return r}
    if !bank.is_null(){let r=unsafe{aupe_keypairs_accept_rs(bank,key_epoch,counter,now)};if r!=AF_WAIT{return r}}
    else if !replay.is_null(){let r=unsafe{aupe_replay_accept_rs(replay,gate_epoch,counter)};if r!=AF_WAIT{return r}}
    let mut slot=2u32;
    let r=unsafe{af_mux_feed_rs(mux,mux_epoch,now,packet,n,&mut slot)};
    let callback=emit.unwrap();
    if r==AF_BYPASS{return if unsafe{callback(ctx,packet,n)}==0 {AF_BYPASS}else{AF_BAD}}
    if r!=AF_DONE{return r}
    let size=unsafe{af_mux_take_rs(mux,mux_epoch,slot,scratch,capacity)};
    if size<=20{return if size<0{size}else{AF_BAD}}
    let rv=if unsafe{callback(ctx,scratch,size as u32)}==0{AF_DONE}else{AF_BAD};
    // Preserve C contract: caller-supplied scratch is wiped on both callback outcomes.
    for b in unsafe{slice::from_raw_parts_mut(scratch,size as usize)}{unsafe{ptr::write_volatile(b,0)}}
    rv
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_dispatch_verified_rs(gate:*const RxGate,mux:*mut AfMux,
  epoch:u64,now:u64,packet:*const u8,n:u32,scratch:*mut u8,cap:u32,emit:Emit,ctx:*mut c_void)->i32{
    unsafe{dispatch(gate,mux,ptr::null_mut(),ptr::null_mut(),0,epoch,epoch,0,now,packet,n,scratch,cap,emit,ctx)}
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_dispatch_counter_rs(gate:*const RxGate,mux:*mut AfMux,
  replay:*mut ReplayWindow,epoch:u64,counter:u64,now:u64,packet:*const u8,n:u32,
  scratch:*mut u8,cap:u32,emit:Emit,ctx:*mut c_void)->i32{
    if replay.is_null(){return AF_BAD}
    unsafe{dispatch(gate,mux,replay,ptr::null_mut(),0,epoch,epoch,counter,now,packet,n,scratch,cap,emit,ctx)}
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aupe_dispatch_keypair_rs(gate:*const RxGate,pair:*mut PairMux,
  bank:*mut KeypairBank,key_epoch:u64,counter:u64,now:u64,packet:*const u8,
  n:u32,scratch:*mut u8,cap:u32,emit:Emit,ctx:*mut c_void)->i32{
    if gate.is_null()||bank.is_null()||pair.is_null(){return AF_BAD}
    let g=unsafe{&*gate};
    if g.state==2{return AF_CLOSED}
    if g.state!=1{return AF_STALE}
    let mux=unsafe{aupe_pair_mux_select_rs(pair,key_epoch)};
    if mux.is_null(){return AF_STALE}
    unsafe{dispatch(gate,mux,ptr::null_mut(),bank,key_epoch,g.epoch,key_epoch,counter,now,packet,n,scratch,cap,emit,ctx)}
}
