#!/usr/bin/env python3
"""Read-only fan/USB controller evidence extraction from an offline Linux root."""
import os,re,sys,stat,subprocess
from pathlib import Path

root=Path(sys.argv[1]).resolve()
if not (root/"etc").exists():
    sys.exit("not a Linux root filesystem")
WORD=re.compile(r'fan.?control|fan.?speed|fan[0-9]|cooler|pwm[0-9]?|duty.?cycle|tachom|hidraw|hidapi|libusb|ttyUSB|ttyACM|usbserial|cp210x|ch34[01]|ftdi_sio|/sys/class/hwmon|/dev/bus/usb|idVendor|idProduct|usb.*serial|nvidia-smi.*fan|fan.*nvidia|hid_device|i2c-dev|fancontrol',re.I)
PATH_RE=re.compile(r'fan|cool|pwm|usb|hidraw|hidapi|serial|hwmon|tty',re.I)
SUFFIXES={".service",".timer",".rules",".sh",".py",".conf",".ini",".toml",".yaml",".yml",".json",".cfg",".lua",".pl",".rc",".desktop",".xml",".txt",".log",".c",".cpp",".h",".rs",".go",".js",".bash",".profile",".config"}
SKIP_DIRS={"proc","sys","dev","run","tmp","node_modules",".cache",".git","__pycache__","vendor","firmware"}
SENSITIVE=re.compile(r'private.?key|password|passwd|api[_-]?key|bearer |authentication|secret|ssh-rsa|BEGIN .*PRIVATE|wallet.?address|mnemonic',re.I)
def redact(s):
    if SENSITIVE.search(s): return "[omitted: potentially sensitive]"
    s=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[IPv4]',s)
    s=re.sub(r'[\w.+-]+@[\w.-]+\.\w+','[email]',s)
    return s.replace("\x00","")[:240]
def walk(base):
    if not base.exists():return
    for d,ds,fs in os.walk(base,followlinks=False):
        ds[:]=[x for x in ds if x not in SKIP_DIRS and not x.startswith(".cache")]
        for f in fs:
            p=Path(d)/f
            try:
                if p.is_symlink() or not p.is_file(): continue
                sz=p.stat().st_size
                if sz > 100*1024*1024: continue
                yield p,sz
            except (OSError,PermissionError):continue
print("=== ROOT IDENTIFICATION ===",flush=True)
for f in ["etc/os-release","etc/issue","etc/hostname"]:
    try: print(f,redact((root/f).read_text(errors="replace").strip().replace("\n","; ")))
    except OSError: pass
print("\n=== CANDIDATE FILE PATHS ===",flush=True)
paths=[]
for d in ["etc","opt","usr/local","home","root","var/spool/cron","var/log","usr/lib/systemd/system","lib/systemd/system"]:
    for p,sz in walk(root/d):
        rel=str(p.relative_to(root))
        paths.append((p,sz,rel))
names=[(p,s,rel) for p,s,rel in paths if PATH_RE.search(p.name) and not rel.startswith("usr/lib/firmware")]
for p,s,rel in names[:180]:
    print(f"PATH {rel} ({s} B)")
print(f"total_candidate_paths={len(names)} total_scanned_files={len(paths)}",flush=True)
print("\n=== CONFIG/SCRIPT MATCHES WITH CONTEXT ===",flush=True)
matches=[]
for p,sz,rel in paths:
    if sz>3*1024*1024 or not sz:continue
    if not (p.suffix.lower() in SUFFIXES or p.name in {"crontab","rc.local","inittab","environment","modules","xinitrc","bashrc"}):
        continue
    try:
        b=p.read_bytes()
        if b"\x00" in b[:4096]:continue
        lines=b.decode("utf8",errors="replace").splitlines()
    except (OSError,PermissionError):continue
    hit=[i for i,line in enumerate(lines) if WORD.search(line)]
    if hit:
        score=sum(6 if re.search(r'^(opt|usr/local|etc|home|root)/',rel) else 1 for _ in hit)
        score+=len(hit)
        matches.append((score,rel,lines,hit))
matches.sort(reverse=True)
for score,rel,lines,hit in matches[:80]:
    print(f"--- {rel} matches={len(hit)} score={score}")
    printed=set()
    for i in hit[:8]:
        for j in range(max(0,i-1),min(len(lines),i+2)):
            if j in printed:continue
            printed.add(j)
            print(f"{j+1:05d}: {redact(lines[j])}")
print(f"matched_text_files={len(matches)}",flush=True)
print("\n=== EXECUTABLE STRINGS: HARDWARE/CONTROLLER MARKERS ===",flush=True)
candidates=[]
for p,sz,rel in paths:
    if sz<4096 or sz>70*1024*1024:continue
    if not rel.startswith(("opt/","usr/local/","home/","root/")):continue
    if p.suffix.lower() in SUFFIXES or p.suffix.lower() in {".ko",".a",".so",".gz",".zip",".xz",".zst",".png",".db",".sqlite"}:continue
    try:
        with p.open("rb") as f: magic=f.read(4)
    except OSError:continue
    if magic!=b"\x7fELF":continue
    candidates.append((p,sz,rel))
print(f"elf_candidates={len(candidates)}")
found=0
for p,sz,rel in candidates[:160]:
    try:
        r=subprocess.run(["strings","-a","-n","6",str(p)],capture_output=True,text=True,errors="replace",timeout=18)
        ls=[redact(x) for x in r.stdout.splitlines() if WORD.search(x) and len(x)<800]
        if ls:
            print(f"--- BIN {rel} hits={len(ls)} size={sz}")
            for line in ls[:18]: print(line)
            found+=1
    except (OSError,subprocess.TimeoutExpired):pass
print(f"matched_elf_files={found}",flush=True)
print("\n=== INVESTIGATOR NOTES ===")
print("Find the USB VID/PID in dmesg/udev records to distinguish real device from compiled driver strings.")
print("Match service ExecStart to script/binary, inspect nearby byte writes and temperature logic.")
