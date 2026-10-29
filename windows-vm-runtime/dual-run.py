#!/usr/bin/env python3
"""Launch one prepared Tiny11 QCOW2 in either ChatGPT Linux sandbox or GitHub Windows runner.

Prerequisites: ready-zstd QCOW2, OVMF_CODE.fd, OVMF_VARS.fd, manifest, QEMU TCG.
Never modifies the compressed base disk: each session uses a writable QCOW2 overlay.
Network defaults to none. VNC is loopback-only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import sys
import time

def sha256(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for chunk in iter(lambda:f.read(4*1024*1024),b""): h.update(chunk)
    return h.hexdigest()

def qemu_command(binary, qemu_home):
    if qemu_home:
        home=Path(qemu_home).resolve()
        executable=home/"bin"/binary
        if platform.system()=="Windows": executable=executable.with_suffix(".exe")
        if executable.is_file():
            ld=home/"lib"/"ld-linux-x86-64.so.2"
            if ld.is_file() and platform.system()=="Linux":
                return [str(ld),"--library-path",str(home/"lib"),str(executable)]
            return [str(executable)]
    found=shutil.which(binary)
    if not found and platform.system()=="Windows":
        candidate=Path("C:/Program Files/qemu")/(binary+".exe")
        if candidate.is_file():found=str(candidate)
    if not found:raise SystemExit("Cannot locate "+binary+"; set QEMU_HOME or install QEMU")
    return [found]

def main():
    p=argparse.ArgumentParser()
    p.add_argument("action",choices=("verify","start","probe"))
    p.add_argument("--vm-dir",type=Path,default=Path("/mnt/data/tiny11-lab" if os.name!="nt" else "./tiny11-lab"))
    p.add_argument("--qemu-home",default=os.environ.get("QEMU_HOME"))
    p.add_argument("--ram",type=int,default=2560,help="guest RAM in MiB")
    p.add_argument("--cpus",type=int,default=2)
    p.add_argument("--accelerator",choices=("tcg","kvm"),default="tcg",
                   help="KVM is for GitHub Ubuntu runner with /dev/kvm; sandbox defaults to TCG")
    p.add_argument("--headless",action="store_true",help="QEMU no GUI; VGA screenshots through monitor")
    p.add_argument("--seconds",type=int,default=600,help="probe time budget")
    p.add_argument("--reset-session",action="store_true",help="delete saved overlay and session UEFI vars")
    p.add_argument("--skip-hash",action="store_true",help="skip 8-GB SHA-256 check on repeat starts")
    opt=p.parse_args()
    root=opt.vm_dir.resolve()
    manifest=root/"tiny11-ready-manifest.json"
    if not manifest.is_file():raise SystemExit("Missing ready manifest: "+str(manifest))
    meta=json.loads(manifest.read_text(encoding="utf-8-sig"))
    if meta.get("schema")!=1 or not meta.get("prepared_first_boot") or meta.get("install_required_on_next_start"):
        raise SystemExit("Not a fully prepared Tiny11 base disk; refuse install-stage image")
    disk=root/meta["disk"]
    code=root/"OVMF_CODE.fd"
    vars_base=root/"OVMF_VARS.fd"
    for f in (disk,code,vars_base):
        if not f.is_file():raise SystemExit("Missing required prepared VM file: "+str(f))
    if disk.name!="tiny11-ready-zstd.qcow2" or not disk.is_file():
        raise SystemExit("Unrecognized disk name")
    if not opt.skip_hash:
        for f,field in ((disk,"disk_sha256"),(code,"ovmf_code_sha256"),(vars_base,"ovmf_vars_sha256")):
            want=meta[field].lower()
            have=sha256(f).lower()
            if have!=want:raise SystemExit("SHA-256 mismatch: "+str(f))
            print("Verified",f.name,have[:16]+"...",flush=True)
    qimg=qemu_command("qemu-img",opt.qemu_home)
    info=json.loads(subprocess.check_output(qimg+["info","--output=json",str(disk)],text=True))
    if info.get("format")!="qcow2":
        raise SystemExit("Base disk is not QCOW2")
    if info.get("backing-filename"):
        raise SystemExit("Base disk unexpectedly depends on another image")
    fmtdata=info.get("format-specific",{}).get("data",{})
    if fmtdata.get("compression-type")!="zstd":
        raise SystemExit("Base QCOW2 clusters are not configured for Zstandard")
    if int(info.get("virtual-size",0))!=int(meta["virtual_size_bytes"]):
        raise SystemExit("Virtual size mismatch")
    if opt.action=="verify":
        print(json.dumps({"status":"ready","base":str(disk),"physical_bytes":disk.stat().st_size,
                          "virtual_bytes":info["virtual-size"],"compression":"zstd",
                          "installation_required":False},indent=2))
        return
    if opt.ram<1024 or opt.ram>8192:raise SystemExit("Guest RAM must be 1024..8192 MiB")
    root.mkdir(parents=True,exist_ok=True)
    # Automated probes are disposable: never let a failed or forcibly stopped
    # second boot damage the persistent interactive session or the clean base.
    is_probe=opt.action=="probe"
    overlay=root/("tiny11-probe.qcow2" if is_probe else "tiny11-session.qcow2")
    session_vars=root/("OVMF_VARS-probe.fd" if is_probe else "OVMF_VARS-session.fd")
    if is_probe or opt.reset_session:
        overlay.unlink(missing_ok=True)
        session_vars.unlink(missing_ok=True)
    if not overlay.is_file():
        subprocess.run(qimg+["create","-f","qcow2","-F","qcow2","-b",str(disk),str(overlay)],check=True)
    if not session_vars.is_file():shutil.copyfile(vars_base,session_vars)
    share=root/"sharefiles";share.mkdir(exist_ok=True)
    (share/"TINY11_INSTALLED_READY.TXT").unlink(missing_ok=True)
    # Only the automated probe requests a Windows-initiated clean shutdown.
    # Interactive 'start' never issues an automated shutdown request.
    probe_request=share/"TINY11_PROBE.TXT"
    shutdown_request=share/"TINY11_AUTOTEST_SHUTDOWN.TXT"
    if opt.action=="probe":
        probe_request.write_text("DUAL_RUNTIME_READY_PROBE\n",encoding="ascii")
        shutdown_request.write_text("AUTOTEST_SHUTDOWN_V1\n",encoding="ascii")
    else:
        probe_request.unlink(missing_ok=True)
        shutdown_request.unlink(missing_ok=True)
    monitor=root/"qemu-monitor.sock"
    if monitor.exists():monitor.unlink()
    serial_path=root/"qemu-serial.log"
    # Stale COM1 logs must NEVER be allowed to confirm a later boot.
    if opt.action=="probe":serial_path.unlink(missing_ok=True)
    cmd=qemu_command("qemu-system-x86_64",opt.qemu_home)
    if opt.qemu_home:
        candidate=Path(opt.qemu_home)/"share"/"qemu"
        if candidate.is_dir():cmd+=["-L",str(candidate)]
    cmd+=["-machine","pc,accel="+opt.accelerator,
          "-cpu","host" if opt.accelerator=="kvm" else "max",
          "-smp",str(opt.cpus),"-m",str(opt.ram),
          "-drive","if=pflash,format=raw,readonly=on,file="+str(code),
          "-drive","if=pflash,format=raw,file="+str(session_vars),
          "-drive","if=ide,format=qcow2,file="+str(overlay),
          "-drive","file=fat:rw:"+str(share)+",format=raw,if=ide",
          "-nic","none","-vga","std"]
    if opt.headless:cmd+=["-display","none"]
    else:cmd+=["-display","vnc=127.0.0.1:0"]
    if os.name=="nt":
        cmd+=["-monitor","tcp:127.0.0.1:4444,server=on,wait=off"]
    else:cmd+=["-monitor","unix:"+str(monitor)+",server=on,wait=off"]
    cmd+=["-serial","file:"+str(serial_path)]
    print("Starting installed Windows via QEMU "+opt.accelerator.upper()+". No reinstall. Network disabled.",flush=True)
    print("VNC is loopback-only on port 5900 (unless --headless).",flush=True)
    if opt.action=="start":
        rc=subprocess.call(cmd)
        raise SystemExit(rc)
    log=root/"qemu-probe.log"
    ready=share/"TINY11_INSTALLED_READY.TXT"
    with log.open("w",encoding="utf8") as out:
        proc=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
        deadline=time.monotonic()+opt.seconds
        timed_out=False
        host_terminated=False
        guest_marker_at=None
        acpi_requested=False
        acpi_grace_seconds=90
        def proof_status():
            fat=ready.read_text(errors="replace") if ready.is_file() else ""
            serial=serial_path.read_text(errors="replace") if serial_path.is_file() else ""
            return ("TINY11_INSTALLED_READY" in fat and "COMPUTER=" in fat,
                    "TINY11_INSTALLED_READY SECOND_BOOT_CONFIRMED" in serial)
        try:
            while proc.poll() is None:
                now=time.monotonic()
                fat_live,serial_live=proof_status()
                if (fat_live or serial_live) and guest_marker_at is None:
                    guest_marker_at=now
                    print("Second installed-Windows boot confirmed. Requesting graceful ACPI shutdown.",flush=True)
                if guest_marker_at is not None and not acpi_requested and os.name!="nt":
                    if monitor.is_socket():
                        try:
                            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as monitor_sock:
                                monitor_sock.settimeout(3)
                                monitor_sock.connect(str(monitor))
                                monitor_sock.recv(4096)
                                monitor_sock.sendall(b"system_powerdown\n")
                            acpi_requested=True
                        except OSError as e:
                            print("ACPI poweroff request unavailable:",e,flush=True)
                if guest_marker_at is not None and now-guest_marker_at>=acpi_grace_seconds:
                    print("Windows boot proof present but guest did not power off: discarding probe overlay.",flush=True)
                    break
                if now>=deadline:
                    timed_out=True
                    break
                time.sleep(5)
        finally:
            if proc.poll() is None:
                host_terminated=True
                proc.terminate()
                try:proc.wait(timeout=8)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
        # vvfat can publish guest writes only when QEMU exits; reread both
        # sources after all emulated block devices have been closed.
        fat_confirmed,serial_confirmed=proof_status()
        success=fat_confirmed or serial_confirmed
        clean_shutdown=(not host_terminated and not timed_out and proc.returncode==0)
        # The first OOBE disk was already captured after a verified CLEAN
        # shutdown. A second boot is read-only against that base; the probe
        # overlay is disposable and never distributed, even on forced exit.
        if is_probe:
            overlay.unlink(missing_ok=True)
            session_vars.unlink(missing_ok=True)
        if success:
            print("PASS: second boot confirmed by guest FAT/COM1. Guest clean shutdown:",clean_shutdown,flush=True)
        result={"boot_confirmed":success,"runtime":"qemu-"+opt.accelerator,"probe_seconds":opt.seconds,
                "guest_shutdown_clean":clean_shutdown,
                "qemu_exit_code":proc.returncode,"probe_timed_out":timed_out,
                "host_terminated":host_terminated,"probe_overlay_discarded":is_probe,
                "acpi_shutdown_requested":acpi_requested,
                "fat_marker_confirmed":fat_confirmed,"serial_marker_confirmed":serial_confirmed,
                "guest_ready_file":str(ready),"log":str(log),"image_sha256":meta["disk_sha256"]}
        (root/"probe-result.json").write_text(json.dumps(result,indent=2)+"\n")
        print(json.dumps(result,indent=2))
        if not success:raise SystemExit(1)

if __name__=="__main__":
    main()
