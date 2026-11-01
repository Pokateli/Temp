// SPDX-License-Identifier: GPL-2.0-only
//! Debian VM single-process post-AEAD lifecycle smoke (no WG Noise engine).
use aupe_rust_core::{peer_model::Registry,CryptoRef};
fn checksum(p:&[u8])->u16{
 let mut sum:u32=p.chunks_exact(2).map(|c|u16::from_be_bytes([c[0],c[1]]) as u32).sum();
 while sum>65535{sum=(sum&65535)+(sum>>16)};!(sum as u16)
}
fn plain_ipv4()->Vec<u8>{
 let mut p=vec![0u8;28];p[0]=0x45;p[2..4].copy_from_slice(&28u16.to_be_bytes());
 p[8]=64;p[9]=17;p[12..16].copy_from_slice(&[172,16,2,2]);
 p[16..20].copy_from_slice(&[172,16,2,1]);
 p[20..].copy_from_slice(b"AUPErust");
 let cs=checksum(&p[..20]);p[10..12].copy_from_slice(&cs.to_be_bytes());p
}
fn main(){
 let registry=Registry::new();
 let a=registry.add([7u8;32]).unwrap();let b=registry.add([8u8;32]).unwrap();
 let t1=Box::new(1u8);let t2=Box::new(2u8);
 let ref1=CryptoRef{keypair:&*t1 as *const u8 as *const _,generation:1};
 let ref2=CryptoRef{keypair:&*t2 as *const u8 as *const _,generation:2};
 let packet=plain_ipv4();
 let p=registry.acquire(a).unwrap();let mut g=p.lock().unwrap();
 assert_eq!(g.install([7;32],ref1,100).unwrap(),1);
 let delivered=unsafe{g.verified_rx(ref1,123,101,&packet)}.unwrap();
 assert_eq!(delivered,packet);
 assert_eq!(unsafe{g.verified_rx(ref1,123,102,&packet)}.unwrap_err(),-13);
 assert_eq!(g.install([7;32],ref2,200).unwrap(),2);
 let delivered=unsafe{g.verified_rx(ref1,124,201,&packet)}.unwrap();
 assert_eq!(delivered,packet);
 let delivered=unsafe{g.verified_rx(ref2,125,202,&packet)}.unwrap();
 assert_eq!(delivered,packet);
 drop(g);
 assert!(registry.acquire(b).is_some());registry.revoke(a).unwrap();
 assert!(registry.acquire(a).is_none());assert_eq!(registry.count(),1);
 println!("VM_SMOKE_PASS: Rust peer registry, identical inner IPv4, auth owner, rekey, anti-replay, revoke");
}
