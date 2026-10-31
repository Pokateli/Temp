# AUPE 0.5.59 (2026-11-01)

Experimental dual-slot peer-scoped IPv4 reassembly, two concurrent fragmented
inner datagrams per trusted Noise session. Rejected third datagram never evicts
valid in-flight/completed flows. Nonfragmented authenticated packets bypass
independent of occupied slots. New kernel process-context peer adapter with
mutex, fixed 64-object cap, sleepable allocation and atomic feed+take.
No changes to WG/AWG cryptography or protocol negotiation. No complete AUPE
integration or 6.12.113/VM qualification. Regression on 0.5.58 passed.

# AUPE 0.5.58

See earlier devkit for single-slot IPv4 validation, ECN, scrubbing and expiry.