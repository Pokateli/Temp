#!/usr/bin/env bash
# Prove compressed qcow2 clusters and byte-perfect round trip with Zstandard.
set -Eeuo pipefail
sudo apt-get update -qq
sudo apt-get install -y -qq --no-install-recommends qemu-utils python3
mkdir -p tiny11-qcow2-logs
python3 - <<'PY'
from pathlib import Path
from os import urandom
p=Path('/tmp/qcow2-zstd-test.raw')
with p.open('wb') as f:
    for i in range(128):
        if i%16 == 0:
            f.write(urandom(1024*1024))
        elif i%3==0:
            f.write(b'Tiny11 ZSTD qcow2 verification '*(1024*1024//31)+b' '*(1024*1024%31))
        else:
            f.write(b'\0'*(1024*1024))
print('Created raw volume',p.stat().st_size)
PY
qemu-img convert -c -f raw -O qcow2 -o compat=1.1,compression_type=zstd /tmp/qcow2-zstd-test.raw /tmp/qcow2-zstd-test.qcow2
qemu-img info --output=json /tmp/qcow2-zstd-test.qcow2 | tee tiny11-qcow2-logs/qcow2-info.json
qemu-img map --output=json /tmp/qcow2-zstd-test.qcow2 | tee tiny11-qcow2-logs/qcow2-map.json
qemu-img compare -f raw -F qcow2 /tmp/qcow2-zstd-test.raw /tmp/qcow2-zstd-test.qcow2 | tee tiny11-qcow2-logs/compare.txt
python3 - <<'PY'
import json,os
info=json.load(open('tiny11-qcow2-logs/qcow2-info.json'))
maps=json.load(open('tiny11-qcow2-logs/qcow2-map.json'))
s=str(info).lower()
if 'zstd' not in s:
    raise SystemExit('QCOW2 metadata has no zstd compression type: '+repr(info))
n=sum(1 for e in maps if e.get('compressed',False))
print('Compressed extents reported:',n)
if n==0: raise SystemExit('No compressed extents in qemu-img map: compression not verified')
raw=os.stat('/tmp/qcow2-zstd-test.raw').st_size
qc=os.stat('/tmp/qcow2-zstd-test.qcow2').st_size
print(f'Raw={raw}, ZSTD_QCOW2={qc}, ratio={qc/raw:.2%}')
if qc>=raw: raise SystemExit('Compressed qcow2 not smaller than input')
print('PASS: ZSTD compressed clusters, verified byte-identical data')
PY
