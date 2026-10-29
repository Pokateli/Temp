#!/usr/bin/env python3
"""First-boot preinstalled Tiny11 on GitHub Ubuntu KVM, then freeze standalone ZSTD QCOW2.

Consumes an authenticated, already-DISM-installed Windows QCOW2 and local credentials
from a separately encrypted GitHub Actions stage artifact. Does not upload OS bytes.
Guest network is disabled. Only a guest Windows ready proof plus clean shutdown
authorizes a reusable disk.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time

def sha256(path):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda:f.read(4*1024*1024),b""):h.update(b)
    return h.hexdigest()

def cmd(*args):
    subprocess.run(list(map(str,args)),check=True)

def info(path):
    return json.loads(subprocess.check_output(["qemu-img","info","--output=json",str(path)],text=True))

def screen(monitor,dest):
    if not monitor.is_socket():return
    try:
        with socket.socket(socket.AF_UNIX) as s:
            s.settimeout(3)
            s.connect(str(monitor))
            s.recv(1024)
            s.sendall(("screendump "+str(dest)+"\n").encode())
    except (OSError,TimeoutError) as exc:
        print("screendump unavailable:",str(exc),flush=True)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--vm-dir",type=Path,required=True)
    p.add_argument("--seconds",type=int,default=2700)
    p.add_argument("--ram",type=int,default=2560)
    a=p.parse_args()
    root=a.vm_dir.resolve()
    root.mkdir(parents=True,exist_ok=True)
    base=root/"tiny11-zstd.qcow2"
    credentials=root/"ready-credentials.txt"
    ovmf_code=Path("/usr/share/OVMF/OVMF_CODE_4M.fd")
    ovmf_vars=Path("/usr/share/OVMF/OVMF_VARS_4M.fd")
    for source in (base,credentials,ovmf_code,ovmf_vars):
        if not source.is_file():raise SystemExit("Missing required stage component: "+str(source))
    if not Path("/dev/kvm").exists():raise SystemExit("No /dev/kvm: hardware-accelerated first boot unavailable")
    base_info=info(base)
    if base_info.get("format")!="qcow2" or base_info.get("backing-filename"):
        raise SystemExit("Expected standalone pre-first-login QCOW2")
    if base_info.get("format-specific",{}).get("data",{}).get("compression-type")!="zstd":
        raise SystemExit("Expected pre-first-login ZSTD QCOW2")
    print("Pre-first-login stage:",base.stat().st_size,"bytes; free disk",shutil.disk_usage(root).free,flush=True)
    overlay=root/"tiny11-firstboot.qcow2"
    if overlay.exists():raise SystemExit("Refusing to reuse existing firstboot overlay")
    cmd("qemu-img","create","-f","qcow2","-F","qcow2","-b",base,overlay)
    code=root/"OVMF_CODE.fd"
    vars_boot=root/"OVMF_VARS.fd"
    shutil.copyfile(ovmf_code,code)
    shutil.copyfile(ovmf_vars,vars_boot)
    share=root/"sharefiles"
    share.mkdir(exist_ok=True)
    (share/"TINY11_PROBE.TXT").write_text("READY_FIRST_BOOT\n",encoding="ascii")
    marker=share/"TINY11_FIRST_LOGON_READY.TXT"
    marker.unlink(missing_ok=True)
    serial=root/"firstboot-serial.log"
    monitor=root/"firstboot-monitor.sock"
    monitor.unlink(missing_ok=True)
    qemu=[
        "qemu-system-x86_64","-machine","pc,accel=kvm","-cpu","host",
        "-smp","2","-m",str(a.ram),"-display","none","-vga","std",
        "-monitor","unix:"+str(monitor)+",server=on,wait=off",
        "-serial","file:"+str(serial),"-nic","none",
        "-drive","if=pflash,format=raw,readonly=on,file="+str(code),
        "-drive","if=pflash,format=raw,file="+str(vars_boot),
        "-drive","if=ide,format=qcow2,file="+str(overlay),
        "-drive","file=fat:rw:"+str(share)+",format=raw,if=ide"
    ]
    deadline=time.monotonic()+a.seconds
    snaps=(30,120,300,600,1200,2400)
    captured=set()
    stderr=root/"firstboot-qemu.log"
    print("Starting actual installed Windows on KVM (first OOBE only). Guest network: off.",flush=True)
    timeout=False
    with stderr.open("w") as log:
        proc=subprocess.Popen(qemu,stdout=log,stderr=subprocess.STDOUT)
        started=time.monotonic()
        try:
            while proc.poll() is None:
                elapsed=int(time.monotonic()-started)
                if time.monotonic()>=deadline:
                    timeout=True
                    break
                for point in snaps:
                    if elapsed>=point and point not in captured:
                        captured.add(point)
                        screen(monitor,root/("firstboot-"+str(point)+".ppm"))
                if elapsed % 90 < 5:
                    print("KVM firstboot elapsed:",elapsed,"sec",flush=True)
                time.sleep(5)
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=10)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
    if timeout:raise SystemExit("First boot did not complete in timeout; refusing unready disk")
    if proc.returncode!=0:raise SystemExit("QEMU exit was not clean: "+str(proc.returncode))
    fat=marker.read_text(errors="replace") if marker.exists() else ""
    serial_text=serial.read_text(errors="replace") if serial.exists() else ""
    fat_pass="TINY11_FIRST_LOGON_READY" in fat and "INSTALLED_AND_OOBE_FINISHED" in fat
    serial_pass="TINY11_FIRST_LOGON_READY INSTALLED_AND_OOBE_FINISHED" in serial_text
    if not (fat_pass or serial_pass):
        raise SystemExit("OOBE/login proof missing on both FAT and COM1; no ready disk")
    print("PASS: real guest first login and clean poweroff. FAT:",fat_pass,"COM1:",serial_pass,flush=True)
    ready=root/"tiny11-ready-zstd.qcow2"
    cmd("qemu-img","convert","-p","-c","-f","qcow2","-O","qcow2",
        "-o","compat=1.1,compression_type=zstd",overlay,ready)
    cmd("qemu-img","compare","-f","qcow2","-F","qcow2",overlay,ready)
    meta=info(ready)
    if meta.get("backing-filename") or meta.get("format")!="qcow2":
        raise SystemExit("Compressed result must be standalone QCOW2")
    if meta.get("format-specific",{}).get("data",{}).get("compression-type")!="zstd":
        raise SystemExit("Ready disk does not report ZSTD")
    d_hash=sha256(ready)
    c_hash=sha256(code)
    v_hash=sha256(vars_boot)
    manifest={
        "schema":1,"name":"Tiny11 Windows 11 Enterprise Evaluation; installed, OOBE-complete",
        "disk":ready.name,"virtual_size_bytes":meta["virtual-size"],
        "disk_size_bytes":ready.stat().st_size,"compression":"zstd",
        "disk_sha256":d_hash,"ovmf_code_sha256":c_hash,"ovmf_vars_sha256":v_hash,
        "prepared_first_boot":True,"install_required_on_next_start":False,
        "default_user":"vmuser","credential_filename":credentials.name,
        "network":"off","vm_ram_mib":a.ram,"vm_cpus":2,
        "firstboot_accelerator":"kvm","firstboot_fat_proof":fat_pass,
        "firstboot_serial_proof":serial_pass
    }
    (root/"tiny11-ready-manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    (root/"SHA256SUMS").write_text(
        f"{d_hash} *{ready.name}\n{c_hash} *{code.name}\n{v_hash} *{vars_boot.name}\n")
    print("READY ZSTD QCOW2:",ready.stat().st_size,"bytes; SHA256",d_hash,flush=True)

if __name__=="__main__":
    main()
