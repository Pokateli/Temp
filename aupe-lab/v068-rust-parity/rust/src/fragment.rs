// SPDX-License-Identifier: GPL-2.0-only
//! Wire-compatible AUPE 0.5.66 IPv4 fragment engine, dual flow mux and
//! current/previous keypair separation. FFI pointers are **caller-owned**;
//! caller must serialize access and allocate all workspaces outside RX.
use core::{ptr, slice};
const MAX_PAYLOAD: usize = 65515;
const BIT_BYTES: usize = (MAX_PAYLOAD + 7) / 8;
const MAX_PACKET: usize = MAX_PAYLOAD + 20;
const TIMEOUT: u64 = 5000;
const AF_WAIT: i32 = 0;
const AF_DONE: i32 = 1;
const AF_BYPASS: i32 = 2;
const AF_BAD: i32 = -1;
const AF_OVERLAP: i32 = -2;
const AF_OTHER: i32 = -3;
const AF_STALE: i32 = -4;
const AF_EXPIRED: i32 = -5;
const AF_NOT_READY: i32 = -6;
const AF_ECN: i32 = -7;
const AF_LIMIT: i32 = -8;
const AF_BUSY: i32 = -9;

#[repr(C)]
pub struct AfState {
    pub payload: [u8; MAX_PAYLOAD],
    pub seen: [u8; BIT_BYTES],
    pub first_header: [u8; 20],
    pub epoch: u64, pub expiry_ms: u64,
    pub src: u32, pub dst: u32,
    pub received_bytes: u32, pub furthest_end: u32,
    pub ident: u16, pub last_end: u16, pub accepted_frags: u16,
    pub protocol: u8, pub ecn_mask: u8,
    pub active: u8, pub has_last: u8, pub has_first: u8,
    pub complete: u8, pub bound: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AfMux {
    pub slot: [*mut AfState; 2], pub epoch: u64, pub bound: u8,
}
#[repr(C)]
pub struct PairMux {
    pub active: AfMux, pub previous: AfMux,
    pub workspace: [*mut AfState; 4],
}
#[inline] fn be16(p: &[u8], off: usize) -> u16 { u16::from_be_bytes([p[off],p[off+1]]) }
#[inline] fn be32(p: &[u8], off: usize) -> u32 { u32::from_be_bytes([p[off],p[off+1],p[off+2],p[off+3]]) }
fn checksum(p: &[u8]) -> u16 {
    let mut sum: u32 = p.chunks_exact(2).map(|v| u16::from_be_bytes([v[0],v[1]]) as u32).sum();
    if (p.len() & 1)!=0 {sum += (p[p.len()-1] as u32)<<8;}
    while sum>>16 != 0 {sum=(sum&65535)+(sum>>16)}
    !(sum as u16)
}
#[inline] fn has(s: &AfState, i: usize) -> bool {s.seen[i>>3] & (1 << (i&7)) != 0}
#[inline] fn mark(s: &mut AfState, i: usize) {s.seen[i>>3] |= 1 << (i&7)}
fn reset(s: &mut AfState) {
    // Erase plaintext with volatile stores to prevent optimizer elision.
    for b in s.payload.iter_mut() {unsafe {ptr::write_volatile(b,0)}}
    s.seen.fill(0); s.first_header.fill(0);
    s.expiry_ms=0;s.src=0;s.dst=0;s.received_bytes=0;s.furthest_end=0;
    s.ident=0;s.last_end=0;s.accepted_frags=0;s.ecn_mask=0;s.protocol=0;
    s.active=0;s.has_last=0;s.has_first=0;s.complete=0;
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_reset_rs(p:*mut AfState) {if let Some(s)=unsafe{p.as_mut()}{reset(s)}}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_bind_epoch_rs(p:*mut AfState, epoch:u64) {
    if let Some(s)=unsafe{p.as_mut()}{reset(s);s.epoch=epoch;s.bound=u8::from(epoch!=0)}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_unbind_rs(p:*mut AfState) {
    if let Some(s)=unsafe{p.as_mut()}{reset(s);s.epoch=0;s.bound=0}
}
#[inline] fn epoch_ok(s:&AfState,epoch:u64)->bool{s.bound!=0&&epoch!=0&&s.epoch==epoch}
fn expire(s:&mut AfState,epoch:u64,now:u64)->i32 {
    if !epoch_ok(s,epoch){return AF_STALE}
    if s.active==0{return AF_NOT_READY}
    if now.wrapping_sub(s.expiry_ms) < (1_u64<<63){reset(s);return AF_EXPIRED}
    AF_WAIT
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_expire_rs(p:*mut AfState,epoch:u64,now:u64)->i32 {
    match unsafe{p.as_mut()}{Some(s)=>expire(s,epoch,now),None=>AF_STALE}
}
fn feed(s:&mut AfState,epoch:u64,now:u64,p:&[u8])->i32 {
    if p.len()<20 {return AF_BAD}
    if !epoch_ok(s,epoch){return AF_STALE}
    if s.active!=0 && expire(s,epoch,now)==AF_EXPIRED{return AF_EXPIRED}
    if s.complete!=0{return AF_NOT_READY}
    if p[0]!=0x45{return AF_BAD}
    let total=be16(p,2) as usize;
    if total<20 || total!=p.len() || checksum(&p[..20])!=0{return AF_BAD}
    let flags=be16(p,6);
    if (flags&0x8000)!=0{return AF_BAD}
    if flags&0x4000!=0 {
        if flags&0x3fff!=0{return AF_BAD}return AF_BYPASS
    }
    let off=(flags&0x1fff) as usize;
    let more=(flags&0x2000)!=0;
    if !more && off==0{return AF_BYPASS}
    let len=total-20;
    if len==0 || (more && len&7!=0){return AF_BAD}
    let start=off*8;
    if start>MAX_PAYLOAD || len>MAX_PAYLOAD-start{return AF_BAD}
    let end=start+len;
    let id=be16(p,4);let src=be32(p,12);let dst=be32(p,16);let protocol=p[9];
    let ecn_bit=1_u8 << (p[1]&3);
    if s.active!=0 {
        if s.src!=src || s.dst!=dst || s.ident!=id || s.protocol!=protocol{return AF_OTHER}
    }else{
        s.src=src;s.dst=dst;s.ident=id;s.protocol=protocol;
        s.expiry_ms=now.wrapping_add(TIMEOUT);s.active=1;
    }
    if s.accepted_frags>=256 {reset(s);return AF_LIMIT}
    let ecn=ecn_bit|s.ecn_mask;
    if ((ecn&1)!=0&&(ecn&14)!=0)||ecn&6==6 {reset(s);return AF_ECN}
    if s.has_last!=0 && (end>s.last_end as usize || (more&&end==s.last_end as usize)) {reset(s);return AF_BAD}
    if !more {
        if s.has_last!=0 && s.last_end as usize!=end {reset(s);return AF_BAD}
        if s.furthest_end as usize>end {reset(s);return AF_BAD}
        s.last_end=end as u16;s.has_last=1;
    }
    for i in start..end {if has(s,i){reset(s);return AF_OVERLAP}}
    for i in start..end {mark(s,i)}
    s.payload[start..end].copy_from_slice(&p[20..]);
    s.received_bytes+=len as u32;s.accepted_frags+=1;s.ecn_mask=ecn;
    s.furthest_end=s.furthest_end.max(end as u32);
    if start==0{s.first_header.copy_from_slice(&p[..20]);s.has_first=1}
    if s.has_last!=0 && s.has_first!=0 && s.received_bytes==s.last_end as u32 {
        s.complete=1;AF_DONE
    }else{AF_WAIT}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_feed_ipv4_rs(p:*mut AfState,epoch:u64,now:u64,packet:*const u8,n:u32)->i32 {
    if p.is_null()||packet.is_null()||n<20{return AF_BAD}
    // Data may not overlap mutable state. This is a precondition of the C API.
    if n>65535{return AF_BAD}
    let bytes=unsafe{slice::from_raw_parts(packet,n as usize)};
    feed(unsafe{&mut*p},epoch,now,bytes)
}
fn take(s:&mut AfState,out:&mut[u8])->i32 {
    if s.complete==0||s.has_first==0||s.has_last==0{return AF_NOT_READY}
    let n=20+s.last_end as usize;
    if out.len()<n||n>65535{return AF_BAD}
    out[..20].copy_from_slice(&s.first_header);
    out[1]=(out[1]&0xfc)|if s.ecn_mask&8!=0{3}else if s.ecn_mask&4!=0{2}else if s.ecn_mask&2!=0{1}else{0};
    out[2..4].copy_from_slice(&(n as u16).to_be_bytes());
    out[6..8].fill(0);out[10..12].fill(0);
    let sum=checksum(&out[..20]);out[10..12].copy_from_slice(&sum.to_be_bytes());
    out[20..n].copy_from_slice(&s.payload[..s.last_end as usize]);
    reset(s);n as i32
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_take_ipv4_rs(p:*mut AfState,out:*mut u8,capacity:u32)->i32 {
    if p.is_null()||out.is_null(){return AF_NOT_READY}
    if capacity<20 || capacity>1_048_576{return AF_BAD}
    take(unsafe{&mut*p},unsafe{slice::from_raw_parts_mut(out,capacity as usize)})
}
fn valid(m:&AfMux,epoch:u64)->bool{
    m.bound!=0&&epoch!=0&&m.epoch==epoch&&
    !m.slot[0].is_null()&&!m.slot[1].is_null()&&m.slot[0]!=m.slot[1]
}
fn valid_ipv4(p:&[u8])->i32 {
    if p.len()<20||p.len()>65535||p[0]!=0x45||be16(p,2) as usize!=p.len()||checksum(&p[..20])!=0 {return AF_BAD}
    let f=be16(p,6);
    if f&0x8000!=0 || (f&0x4000!=0 && f&0x3fff!=0){return AF_BAD}
    if f&0x3fff!=0{AF_WAIT}else{AF_BYPASS}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_mux_bind_rs(m:*mut AfMux,a:*mut AfState,b:*mut AfState,epoch:u64)->i32 {
    if m.is_null()||a.is_null()||b.is_null()||a==b||epoch==0{return AF_BAD}
    let mux=unsafe{&mut*m};
    if mux.bound!=0 && (mux.slot[0]!=a||mux.slot[1]!=b){return AF_BAD}
    unsafe{af_bind_epoch_rs(a,epoch);af_bind_epoch_rs(b,epoch)};
    mux.slot=[a,b];mux.epoch=epoch;mux.bound=1;AF_WAIT
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_mux_unbind_rs(m:*mut AfMux){
    if let Some(mux)=unsafe{m.as_mut()} {
        let a=mux.slot[0];let b=mux.slot[1];
        if !a.is_null(){unsafe{af_unbind_rs(a)}}
        if !b.is_null()&&a!=b{unsafe{af_unbind_rs(b)}}
        mux.epoch=0;mux.bound=0;mux.slot=[ptr::null_mut();2];
    }
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_mux_expire_rs(m:*mut AfMux,epoch:u64,now:u64)->i32 {
    if m.is_null()||!valid(unsafe{&*m},epoch){return AF_STALE}
    let mut expired=0;
    for slot in unsafe{(*m).slot}{if unsafe{af_expire_rs(slot,epoch,now)}==AF_EXPIRED{expired+=1}}
    expired
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_mux_feed_rs(m:*mut AfMux,epoch:u64,now:u64,packet:*const u8,n:u32,ready:*mut u32)->i32 {
    if m.is_null()||!valid(unsafe{&*m},epoch){return AF_STALE}
    if ready.is_null(){return AF_BAD}
    unsafe{*ready=2};
    if packet.is_null()||n>65535{return AF_BAD}
    let bytes=unsafe{slice::from_raw_parts(packet,n as usize)};
    let kind=valid_ipv4(bytes);
    if kind!=AF_WAIT{return kind}
    if unsafe{af_mux_expire_rs(m,epoch,now)}==AF_STALE{return AF_STALE}
    let ident=be16(bytes,4);let src=be32(bytes,12);let dst=be32(bytes,16);let proto=bytes[9];
    let mut target:Option<usize>=None;let mut free:Option<usize>=None;
    for (i,p) in unsafe{(*m).slot}.iter().enumerate(){
        let s=unsafe{&**p};
        if s.active!=0&&s.src==src&&s.dst==dst&&s.ident==ident&&s.protocol==proto{target=Some(i);break}
        if s.active==0&&free.is_none(){free=Some(i)}
    }
    let slot=match target.or(free){Some(i)=>i,None=>return AF_BUSY};
    let rv=unsafe{af_feed_ipv4_rs((*m).slot[slot],epoch,now,packet,n)};
    if rv==AF_DONE{unsafe{*ready=slot as u32}};
    rv
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn af_mux_take_rs(m:*mut AfMux,epoch:u64,slot:u32,out:*mut u8,capacity:u32)->i32 {
    if m.is_null()||!valid(unsafe{&*m},epoch){return AF_STALE}
    if slot>=2{return AF_BAD}
    unsafe{af_take_ipv4_rs((*m).slot[slot as usize],out,capacity)}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn aupe_pair_mux_init_rs(p:*mut PairMux,a:*mut AfState,b:*mut AfState,c:*mut AfState,d:*mut AfState)->i32 {
    if p.is_null(){return AF_BAD}
    let slots=[a,b,c,d];
    for i in 0..4 {if slots[i].is_null(){return AF_BAD}for j in 0..i{if slots[i]==slots[j]{return AF_BAD}}}
    unsafe{ptr::write_bytes(p,0,1);(*p).workspace=slots};AF_WAIT
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn aupe_pair_mux_rotate_rs(p:*mut PairMux,epoch:u64)->i32 {
    if p.is_null()||epoch==0{return AF_BAD}
    let bank=unsafe{&mut*p};
    if bank.workspace[0].is_null(){return AF_BAD}
    if bank.active.bound!=0&&epoch<=bank.active.epoch{return AF_STALE}
    if bank.previous.bound!=0{unsafe{af_mux_unbind_rs(&mut bank.previous)}}
    let slots=if bank.active.bound!=0 {
        let old=bank.active;
        let slots=if old.slot[0]==bank.workspace[0]{[bank.workspace[2],bank.workspace[3]]}
                  else if old.slot[0]==bank.workspace[2]{[bank.workspace[0],bank.workspace[1]]}
                  else{return AF_BAD};
        bank.previous=old;slots
    } else {[bank.workspace[0],bank.workspace[1]]};
    bank.active=AfMux{slot:[ptr::null_mut();2],epoch:0,bound:0};
    unsafe{af_mux_bind_rs(&mut bank.active,slots[0],slots[1],epoch)}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn aupe_pair_mux_retire_previous_rs(p:*mut PairMux){
    if let Some(bank)=unsafe{p.as_mut()}{unsafe{af_mux_unbind_rs(&mut bank.previous)}}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn aupe_pair_mux_clear_rs(p:*mut PairMux){
    if let Some(bank)=unsafe{p.as_mut()}{unsafe{af_mux_unbind_rs(&mut bank.active);af_mux_unbind_rs(&mut bank.previous)}}
}
#[unsafe(no_mangle)] pub unsafe extern "C" fn aupe_pair_mux_select_rs(p:*mut PairMux,epoch:u64)->*mut AfMux {
    if p.is_null()||epoch==0{return ptr::null_mut()}
    let bank=unsafe{&mut*p};
    if bank.active.bound!=0&&bank.active.epoch==epoch{return &mut bank.active}
    if bank.previous.bound!=0&&bank.previous.epoch==epoch{return &mut bank.previous}
    ptr::null_mut()
}
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_frag()->usize {core::mem::size_of::<AfState>()}
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_mux()->usize {core::mem::size_of::<AfMux>()}
#[unsafe(no_mangle)] pub extern "C" fn aupe_rust_sizeof_pair_mux()->usize {core::mem::size_of::<PairMux>()}
