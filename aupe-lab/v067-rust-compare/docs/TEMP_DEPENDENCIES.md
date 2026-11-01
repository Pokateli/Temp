# Dependency transport via Pokateli/Temp

Use a dedicated isolated branch `aupe/0.5.67-rust-compare`. The workflow `aupe-v067-rust-compare.yml` downloads a pinned Rust toolchain using rustup from the GitHub-hosted runner and builds the offline/dependency-free Rust crate. Output is recorded as workflow logs only. **No `actions/upload-artifact`, no `actions/cache`, no GH Actions artifacts, no billable storage dependencies.**

The source repository is unchanged on `main`. Workflow may consume GitHub Actions CPU minutes and logs; check remaining quota. For local offline compilation, a complete Rust compiler and standard libraries are still required; this workflow does not magically install rustc into the isolated ChatGPT container.
