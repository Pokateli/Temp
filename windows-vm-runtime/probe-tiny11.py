#!/usr/bin/env python3
"""Probe local Tiny11/TCG QEMU runtime. A screenshot is NOT proof Windows booted.

Usage:
  QEMU_HOME=/mnt/data/debian13-chatgpt-vm python3 probe-tiny11.py /mnt/data/tiny11-lab --duration 90
Requires only Python 3 standard library.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

p = argparse.ArgumentParser()
p.add_argument("base", type=Path, help="Directory with tiny11-zstd.qcow2 and OVMF files")
p.add_argument("--duration", type=int, default=90, help="Seconds to run QEMU before terminating")
args = p.parse_args()
base = args.base.absolute()
base.mkdir(parents=True, exist_ok=True)
shell = Path(__file__).with_name("start-tiny11.sh")
if not shell.exists():
    raise SystemExit("Missing start-tiny11.sh")
stdout_path = base / "probe-console.log"
socket_path = base / "monitor.sock"
shot = base / "probe-screen.ppm"
for old in (shot,):
    old.unlink(missing_ok=True)

def hmp(command: str):
    if not socket_path.is_socket():
        return False, "QEMU monitor not ready"
    try:
        with socket.socket(socket.AF_UNIX) as s:
            s.settimeout(4)
            s.connect(str(socket_path))
            _ = s.recv(1024)
            s.sendall((command + "\n").encode("ascii"))
            time.sleep(.4)
            try:
                return True, s.recv(4096).decode("ascii", errors="replace")
            except socket.timeout:
                return True, "(no HMP response)"
    except (OSError, TimeoutError) as ex:
        return False, repr(ex)

started = time.monotonic()
with open(stdout_path, "w", encoding="utf-8") as log:
    proc = subprocess.Popen(["bash", str(shell), str(base)], stdout=log,
                            stderr=subprocess.STDOUT, start_new_session=True)
    last_hmp = ""
    try:
        until = started + args.duration
        while time.monotonic() < until and proc.poll() is None:
            ok, response = hmp("info status")
            if ok:
                last_hmp = response
            if ok and time.monotonic() - started > 10 and not shot.exists():
                hmp("screendump " + str(shot))
            time.sleep(2)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=4)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
report = {
    "qemu_process_exit": proc.returncode,
    "elapsed_seconds": round(time.monotonic() - started, 1),
    "hmp_reachable": bool(last_hmp),
    "qemu_hmp_status": last_hmp[-600:],
    "screen_exists": shot.exists(),
    "screen_bytes": shot.stat().st_size if shot.exists() else 0,
    "log": str(stdout_path),
    "screenshot": str(shot) if shot.exists() else None,
    "windows_guest_boot_confirmed": False,
    "note": "Running QEMU and displaying UEFI/VGA alone does not prove Windows booted.",
}
report_path = base / "probe-result.json"
report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(report, ensure_ascii=False, indent=2))
if proc.returncode not in (0, -15, 143) and not last_hmp:
    sys.exit(1)
