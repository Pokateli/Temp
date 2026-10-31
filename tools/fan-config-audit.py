#!/usr/bin/env python3
"""Read-only HiveOS configuration partition fan-controller identification. Never print credentials."""
import sys,re,os
from pathlib import Path
r=Path(sys.argv[1]); c=Path(sys.argv[2])
names=re.compile(r'(?i)octofan|coolbox|windtank|8mknet|ykeda|fan|pwm|usb|hive|rig')
keys=re.compile(r'(?i)octofan|coolbox|windtank|8mknet|ykeda|fan|pwm|usb|16c0|05dc|10c4|ea60')
sensitive=re.compile(r'(?i)password|passwd|secret|token|private|wallet|worker|id\s*=|url|address|ssh|username|apikey|api[_-]?key')
print("=== HIVE CONFIGURATION PARTITION ===")
print("mount",os.path.ismount(c))
if c.is_dir():
 for p in sorted(c.iterdir()):
  try: print("ROOT_ITEM",p.name,"DIR" if p.is_dir() else "FILE")
  except OSError:pass
 nfiles=0
 for d,ds,fs in os.walk(c,followlinks=False):
  depth=len(Path(d).relative_to(c).parts)
  if depth>4:ds[:]=[];continue
  for f in fs:
   p=Path(d)/f
   try:
    if p.is_symlink() or p.stat().st_size>1000000:continue
    bs=p.read_bytes()
    if b'\0' in bs[:500]:continue
    ls=bs.decode('utf8',errors='replace').splitlines()
   except OSError:continue
   matches=[(i+1,x.strip()) for i,x in enumerate(ls) if keys.search(x)]
   if matches or names.search(f):
    print("CONFIG_FILE",p.relative_to(c),"size",len(bs),"matched_lines",len(matches))
    for i,x in matches[:35]:
     print("  LINE",i,"[redacted]" if sensitive.search(x) else x[:180])
   nfiles+=1
 print("TEXT_FILES_CHECKED",nfiles)
print("=== ACTIVITY CLUES ===")
for rel in ["root/.bash_history","home/user/.bash_history","etc/rc.local","hive/etc/crontab.root","etc/crontab"]:
 p=r/rel
 if not p.is_file():continue
 try: lines=p.read_text(errors="replace").splitlines()
 except OSError:continue
 ms=[(i+1,x) for i,x in enumerate(lines) if keys.search(x)]
 print(rel,"matches",len(ms))
 for i,x in ms[:40]:print("  LINE",i,"[redacted]" if sensitive.search(x) else x[:180])
print("=== END ===")
