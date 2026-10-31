#!/usr/bin/env python3
"""Targeted read-only USB PWM controller files and activation evidence."""
import os,re,sys,subprocess,hashlib
from pathlib import Path
R=Path(sys.argv[1]); C=Path(sys.argv[2]) if len(sys.argv)>2 else Path("/nonexistent")
KEY=re.compile(r'(?i)\b(octofan|coolbox|windtank|8mknet|ykeda|fan_controller_cli|fan.?ctrl|watchdog-octofan|fan.?hub)\b|16c0|05dc|1a86|7523|ttyUSB|hidraw')
RISK=re.compile(r'(?i)(wallet|passwd|password|api[_-]?key|private_key|secret|access_token|seed|auth|ssh|address|worker|user=|url=|rig_id)')
def protect(x):
 x=x.strip()[:340]
 if RISK.search(x): return "[omitted sensitive line]"
 x=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[IPv4]',x)
 return x
def describe(p):
 try:
  q=subprocess.run(["file","-b",str(p)],text=True,capture_output=True,timeout=5)
  return q.stdout.strip()
 except Exception:return ""
def printfile(rel,n=210):
 p=R/rel
 if not p.exists():return
 if p.is_dir():
  print("DIRECTORY",rel)
  for f in sorted(p.iterdir()):
   try: print(" ",f.name,"symlink="+os.readlink(f) if f.is_symlink() else f.stat().st_size, describe(f) if f.is_file() else "")
   except Exception:pass
  return
 try:
  b=p.read_bytes()
 except OSError:return
 print(f"=== FILE {rel}, bytes={len(b)}, sha256={hashlib.sha256(b).hexdigest()}, type={describe(p)} ===")
 if b"\0" in b[:256]:return
 for k,x in enumerate(b.decode("utf-8",errors="replace").splitlines()[:n]):
  print(f"{k+1:04d} {protect(x)}")
for f in [
 "hive/opt/octofan","hive/opt/coolbox","hive/opt/8mknet","hive/opt/windtank","hive/opt/ykeda",
 "hive/opt/octofan/octofan","hive/opt/octofan/watchdog-octofan.sh",
 "hive/opt/coolbox/coolbox","hive/opt/8mknet/8mknet_autofan",
 "hive/opt/windtank/windtank_autofan","hive/opt/ykeda/ykeda_autofan",
 "hive/bin/gpu-fans-find","hive/etc/hive.service","hive/etc/snmpd.conf",
 "etc/udev/rules.d/99-chinadogs.rules","hive/opt/octofan/rig-intake-temp.sh"
]: printfile(f,260)
print("=== CONTROLLER EXECUTABLE STRINGS ===")
for rel in ["hive/opt/octofan/fan_controller_cli","hive/opt/octofan/watchdog-octofan","hive/opt/coolbox/avrdude","hive/opt/coolbox/coolbox","hive/opt/octofan/octofan"]:
 p=R/rel
 if not p.is_file():continue
 print(f"BIN {rel} bytes={p.stat().st_size} type={describe(p)}")
 if p.stat().st_size>20000000:continue
 try:
  b=subprocess.run(["strings","-a","-n","4",str(p)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=20,text=True,errors="replace").stdout.splitlines()
  q=[protect(x) for x in b if re.search(r'(?i)usb|hid|firmware|fan|pwm|speed|temp|serial|vendor|product|16c0|05dc|avrdude|octo|coolbox|comport|baud|channel|report|control.?transfer',x) and len(x)<350 and not RISK.search(x)]
  print("matches",len(q))
  for line in q[:160]:print(" ",line)
 except Exception as e:print("strings-error",str(e))
print("=== STARTUP REFERENCES (only relevant lines) ===")
sources=["hive","etc","root","home","hive-config"]
count=0
for top in sources:
 base=R/top
 if not base.exists():continue
 for d,dirs,files in os.walk(base,followlinks=False):
  dirs[:]=[x for x in dirs if x not in {"miners","lib","node_modules",".cache",".ssh",".git","sympy","usr","share","firmware"}]
  for file in files:
   p=Path(d)/file
   try:
    if p.is_symlink() or p.stat().st_size>700000 or p.stat().st_size<1:continue
    content=p.read_bytes()
    if b"\0" in content[:1024]:continue
    lines=content.decode("utf-8",errors="replace").splitlines()
   except Exception:continue
   idx=[i for i,x in enumerate(lines) if KEY.search(x)]
   if not idx:continue
   print("MATCH",p.relative_to(R),len(idx))
   for i in idx[:10]:print(f" {i+1} {protect(lines[i])}")
   count+=1
print("matching_files",count)
print("=== CONFIG PARTITION SELECTED NAMES ===")
if C.is_dir():
 for d,ds,fs in os.walk(C,followlinks=False):
  depth=len(Path(d).relative_to(C).parts)
  if depth>3:ds[:]=[];continue
  for f in fs:
   p=Path(d)/f
   try:
    if p.is_symlink() or p.stat().st_size>100000:continue
    b=p.read_bytes()
    if b"\0" in b[:500]:continue
    rows=b.decode("utf-8",errors="replace").splitlines()
    selected=[(i+1,protect(x)) for i,x in enumerate(rows) if KEY.search(x)]
    if selected:
     print("CFG",p.relative_to(C),len(selected))
     for i,x in selected[:10]:print(i,x)
   except Exception:pass
print("=== END TARGETED CONTROLLER AUDIT ===")
