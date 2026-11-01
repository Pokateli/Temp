#!/usr/bin/env python3
"""Minimal QEMU serial-root runner for the offline Debian 13 VM-Fabric image.

Original AUPE lab helper; intentionally no guest network and no VM state upload.
"""
import os, pty, re, select, signal, subprocess, sys, time, tty
from pathlib import Path

if len(sys.argv)!=4:
    sys.exit('usage: guest-exec.py BUNDLE SCRIPT LOG')
bundle, script, logfile=map(Path,sys.argv[1:])
if not (bundle/'start.sh').is_file() or not (bundle/'sharefiles').is_dir():
    sys.exit('ERROR: missing start.sh/sharefiles in bundle')
(bunshare:=bundle/'sharefiles').mkdir(exist_ok=True)
payload=bunshare/'aupe68_guest_test.sh'
payload.write_bytes(script.read_bytes())
master,slave=pty.openpty()
tty.setraw(slave)
proc=subprocess.Popen([str(bundle/'start.sh')],stdin=slave,stdout=slave,stderr=slave,start_new_session=True)
os.close(slave)
output=bytearray()
sent=False
success=False
begin=time.monotonic()
prompt=re.compile(rb'root@[A-Za-z0-9._-]+:[^\r\n]{0,120}#\s')
with logfile.open('wb') as log:
    try:
        while time.monotonic()-begin<720:
            if proc.poll() is not None: break
            if not select.select([master],[],[],0.7)[0]:continue
            try: block=os.read(master,65536)
            except OSError:break
            if not block:break
            log.write(block); log.flush()
            output.extend(block)
            if len(output)>8_000_000:del output[:-4_000_000]
            if not sent and prompt.search(output):
                script_cmd='mkdir -p /mnt/dual-vm-run; mount -o ro /dev/vdb /mnt/dual-vm-run || mount -o ro /dev/vdb1 /mnt/dual-vm-run; bash /mnt/dual-vm-run/aupe68_guest_test.sh; status=$?; echo AUPE68_SCRIPT_RC=$status; sync; systemctl poweroff\n'
                os.write(master,script_cmd.encode('ascii'));sent=True
            if b'AUPE68_SCRIPT_RC=0' in output and b'AUPE68_DEBIAN_VM_RESULT=PASS' in output:
                success=True
        if success and proc.poll() is None:
            try:proc.wait(timeout=25)
            except subprocess.TimeoutExpired:pass
    finally:
        if proc.poll() is None:
            try:os.killpg(proc.pid,signal.SIGTERM)
            except ProcessLookupError:pass
            try:proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid,signal.SIGKILL);proc.wait()
        os.close(master)
print(output[-20000:].decode('utf-8','replace'))
if not success:sys.exit('VM-SMOKE FAIL: check serial console log')
print('VM-SMOKE PASS')
