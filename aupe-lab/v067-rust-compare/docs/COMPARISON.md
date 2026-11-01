# Fair C vs Rust comparison protocol

1. Build C and Rust for the **same x86_64 host**. Use GCC/Clang with `-O3` and Rust `opt-level=3`, LTO thin, `panic=abort`. Capture `gcc --version`, `clang --version`, `rustc --version`, CPU `lscpu`.
2. Validate `#[repr(C)]` layouts at the C FFI boundary: sizes and alignments for replay, keypairs, gate, handles and binding.
3. Run 300k replay, 30k gate, 50k handles, 120k keypair and 100k crypto-binding transitions and compare complete logical states and return codes against C. This test checks normal and some adverse state transitions but is NOT exhaustive.
4. Benchmark 2,000,000 monotonically increasing replay counters in an initialized window. This is a *microbenchmark*, not VPN throughput: the measured result includes C↔Rust FFI call overhead; single-run timing is sensitive to CPU frequency and noisy-neighbor effects.
5. For production, also measure `perf stat`, binary size, CPU/memory under real UDP traffic and test malformed packets, actual encryption/decryption, concurrent rekey, and kernel integration. None is provided here.

## Memory/state layout

C `sizeof` on this build: replay=1048 B; epoch gate=24 B; handle table=1024 B; two-keypair bank=2160 B; crypto binding=136 B. Rust sizes are measured and verified at runtime only after the Rust compiler is available.

## Report truthfulness

C-only tests are labeled C-only. No Rust performance or correctness claim is valid until dual-mode test reports a clean exit code. Results must be copied from an executed workflow run or local benchmark, never calculated from source inspection.
