#!/usr/bin/env python3
"""Non-Windows KVM+OVMF+ZSTD-QCOW2 smoke probe on GitHub Ubuntu."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time

p=argparse.ArgumentParser()
p.add_argument("--dir",type=Path,required=True)
p.add_argument("--seconds",type=int,default=12)
a=p.parse_args()
root=a.dir.resolve()
for name in ("OVMF_CODE.fd","OVMF_VARS.fd","synthetic-zstd.qcow2"):
    if not (root/name).is_file():
        raise SystemExit("Missing synthetic input: "+name)
sock=root/"hmp.sock"
sock.unlink(missing_ok=True)
share=root/"share"
share.mkdir(exist_ok=True)
(share/"TINY11_PROBE.TXT").write_text("SYNTHETIC_NOT_WINDOWS\n")
command=[
    "qemu-system-x86_64","-machine","pc,accel=kvm","-cpu","host",
    "-smp","2","-m","1024","-display","none","-vga","std",
    "-monitor","unix:"+str(sock)+",server=on,wait=off",
    "-serial","file:"+str(root/"boot-serial.log"),"-nic","none",
    "-drive","if=pflash,format=raw,readonly=on,file="+str(root/"OVMF_CODE.fd"),
    "-drive","if=pflash,format=raw,file="+str(root/"OVMF_VARS.fd"),
    "-drive","if=ide,format=qcow2,file="+str(root/"synthetic-zstd.qcow2"),
    "-drive","file=fat:rw:"+str(share)+",format=raw,if=ide"
]
with (root/"qemu.log").open("w") as log:
    vm=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
    try:
        deadline=time.monotonic()+a.seconds
        while not sock.is_socket() and time.monotonic()<deadline and vm.poll() is None:
            time.sleep(.25)
        if not sock.is_socket():
            raise SystemExit("QEMU KVM monitor not available")
        with socket.socket(socket.AF_UNIX) as conn:
            conn.settimeout(3)
            conn.connect(str(sock))
            conn.recv(2048)
            conn.sendall(b"info kvm\n")
            time.sleep(.4)
            response=conn.recv(4096).decode(errors="replace")
            print("HMP KVM:",repr(response),flush=True)
            if "enabled" not in response.lower():
                raise SystemExit("KVM accelerator was not enabled")
            screenshot=root/"ovmf-kvm.ppm"
            conn.sendall(("screendump "+str(screenshot)+"\n").encode())
            while time.monotonic()<deadline and not screenshot.exists():
                time.sleep(.25)
            if not screenshot.exists() or screenshot.stat().st_size<1024:
                raise SystemExit("KVM OVMF screendump unavailable")
            print(json.dumps({"kvm_enabled":True,"screenshot_bytes":screenshot.stat().st_size,
                              "qemu_running":vm.poll() is None},indent=2),flush=True)
    finally:
        if vm.poll() is None:
            vm.terminate()
            try:vm.wait(timeout=8)
            except subprocess.TimeoutExpired:vm.kill();vm.wait()
