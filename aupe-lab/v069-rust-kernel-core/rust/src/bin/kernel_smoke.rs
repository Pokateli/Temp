// SPDX-License-Identifier: GPL-2.0-only
//! Exercise allocator-free AUPE kernel-facing core inside LOCAL Debian VM.
//! A trusted caller has already performed Noise/AEAD (not supplied here).
use aupe_rust_core::{CryptoRef,kernel_core::*};
use core::ffi::c_void;
use std::{alloc::{alloc_zeroed,dealloc,Layout},vec::Vec};

#[repr(C)] struct TestState { _opaque: [u8; 1] }
#[derive(Debug)] struct AlignedBuffer {ptr:*mut u8,layout:Layout}
impl AlignedBuffer {
  fn new(size:usize,align:usize)->Self {
    let layout=Layout::from_size_align(size,align).unwrap();
    let p=unsafe{alloc_zeroed(layout)};
    assert!(!p.is_null());Self{ptr:p,layout}
  }
}
impl Drop for AlignedBuffer{fn drop(&mut self){unsafe{dealloc(self.ptr,self.layout)}}}
unsafe extern "C" fn sink(ctx:*mut c_void,packet:*const u8,n:u32)->i32{
  let got=unsafe{&mut *(ctx as *mut Vec<u8>)};
  got.extend_from_slice(unsafe{core::slice::from_raw_parts(packet,n as usize)});0
}
fn packet()->[u8;28] {
    let mut p=[0u8;28];p[0]=0x45;p[3]=28;p[8]=64;p[9]=17;
    p[12..16].copy_from_slice(&[172,16,2,2]);p[16..20].copy_from_slice(&[172,16,2,2]);
    p[20..].copy_from_slice(b"AUPE069!");
    let mut sum:u32=p[..20].chunks_exact(2).map(|v|u16::from_be_bytes([v[0],v[1]]) as u32).sum();
    while sum>>16!=0{sum=(sum&65535)+(sum>>16)};
    p[10..12].copy_from_slice(&(!(sum as u16)).to_be_bytes());p
}
fn main(){
  let p=AlignedBuffer::new(aupe_rs_peer_size(),aupe_rs_peer_align());
  let wsize=aupe_rust_core::aupe_rust_sizeof_frag();
  let work:[AlignedBuffer;4]=std::array::from_fn(|_|AlignedBuffer::new(wsize,8));
  let ws=[work[0].ptr,work[1].ptr,work[2].ptr,work[3].ptr];
  let peer=p.ptr as *mut KernelPeerCore;
  let key=[7u8;32];
  assert_eq!(unsafe{aupe_rs_peer_init(peer,key.as_ptr(),ws[0].cast(),ws[1].cast(),ws[2].cast(),ws[3].cast())},0);
  let k1=1u8;let k2=2u8;
  let r1=CryptoRef{keypair:&k1 as *const u8 as *const c_void,generation:7};
  let r2=CryptoRef{keypair:&k2 as *const u8 as *const c_void,generation:8};
  let mut epoch=0;
  assert_eq!(unsafe{aupe_rs_peer_install(peer,key.as_ptr(),r1,100,&mut epoch)},0);assert_eq!(epoch,1);
  let pkt=packet();let mut out=Vec::new();let mut scratch=vec![0;65535];
  let rx=|reference:CryptoRef,c:u64,t:u64,buf:&mut Vec<u8>,scratch:&mut Vec<u8>| unsafe{
    aupe_rs_peer_verified_rx(peer,reference,c,t,pkt.as_ptr(),pkt.len() as u32,scratch.as_mut_ptr(),65535,
      Some(sink),buf as *mut _ as *mut c_void)};
  assert_eq!(rx(r1,1,101,&mut out,&mut scratch),2);assert_eq!(out,pkt);
  assert_eq!(rx(r1,1,102,&mut out,&mut scratch),-13);
  assert_eq!(unsafe{aupe_rs_peer_install(peer,key.as_ptr(),r2,200,&mut epoch)},0);assert_eq!(epoch,2);
  out.clear();assert_eq!(rx(r1,2,201,&mut out,&mut scratch),2);assert_eq!(out,pkt);
  assert_eq!(unsafe{aupe_rs_peer_retire_previous(peer,r1)},0);
  assert_eq!(rx(r1,3,202,&mut out,&mut scratch),-4);
  unsafe{aupe_rs_peer_close(peer)};
  assert_eq!(rx(r2,4,203,&mut out,&mut scratch),-10);
  println!("AUPE069_LOCAL_VM_SMOKE_PASS: allocator-free Rust core, pinned AEAD owner, replay, rekey, retirement, revocation");
}
