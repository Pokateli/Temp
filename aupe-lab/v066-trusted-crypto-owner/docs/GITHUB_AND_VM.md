# Publication and VM constraints

Version: AUPE 0.5.66. Publish source in a dedicated branch in Pokateli/Temp (avoid Actions Storage) and optionally attach the ZIP as ordinary release asset, not a GitHub Actions artifact. Do NOT trigger workflows. Module built only for Linux `6.12.107+deb13-amd64` headers available in this container. A `6.12.113+deb13-amd64` rebuild and Debian 13 VM load/handshake are required before deployment. Preserve the previous 0.5.65 branch and archive.
