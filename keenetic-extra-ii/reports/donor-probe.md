# ZyXEL Keenetic Extra II donor reconnaissance

Downloads are ephemeral. No firmware binary, artifact, cache or release asset is uploaded.
Static inspection is not a router boot test.

## ki_rb_4.03.C.6.3-9
Source: `https://osvault.keenetic.net/ki_rb/ki_rb_draft_4.03.C.6.3-9.bin`

Download failed; source unavailable.

## official_ki_rb_3.05.C.10.0-0.zip
Source: `https://archive.keenetic.ru/files/Keenetic_Extra_II_3.05.C.10.0-0.zip`

Downloaded bytes: 11554091
SHA256:
34f6cdf179dd4cd5145b8f93a57a307f9c4c55f1bc2acd16512190d5b2651443  donor-work/official_ki_rb_3.05.C.10.0-0.zip.bin

Structure (NOT boot verification):
~~~json
{
  "filename": "official_ki_rb_3.05.C.10.0-0.zip.bin",
  "size": 11665468,
  "sha256": "34f6cdf179dd4cd5145b8f93a57a307f9c4c55f1bc2acd16512190d5b2651443",
  "profile": "ki_rb",
  "kernel_size": 1514823,
  "squashfs_offset": 1572864,
  "filesystem_paths": 1037,
  "init_candidates_present": [
    "/sbin/init"
  ],
  "tlv_ndm": false,
  "status": "STRUCTURE_OK_NOT_BOOT_TESTED",
  "issues": [],
  "warnings": [
    "TLV.NDM missing (SDK 5.01 unpack.sh incompatible)"
  ]
}
~~~

## SDK facts

- SDK tag: https://github.com/keenetic/keenetic-sdk/tree/5.01.C.8.0-2
- Verified: no native ki_rb target in SDK 5.01; KN-1713 has another hardware ID.
- Do not flash a hybrid 5.x image without board-specific validation.
