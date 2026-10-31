#!/usr/bin/env python3
import os,re,sys,subprocess,stat
from pathlib import Path
R=Path(sys.argv[1]).resolve()
T=re.compile(r'(?i)(pwm|fan.?control|fan.?speed|cooler|fan.?hub|tach|hidraw|libusb|usb-serial|ttyUSB|ttyACM|ch34[01]|cp210|ftdi|idVendor|idProduct|usb_device|usbdev|hidapi|usb.*fan|fan.*usb|nvidia-settings.*fan|hive.*fan|fan.*hive|lm-sensors|/dev/serial|usbmon|usbfs|fancontrol|manual.?fan|fancurve)')
NAME=re.compile(r'(?i)(pwm|fan|cool|usb|hid|serial|oc|hwmon|sensors)')
S=re.compile(r'(?i)(password|private.?key|seed phrase|api[_-]?key|access_token|auth_token|bearer |/etc/shadow|secret|wallet|ssh-rsa|BEGIN.*PRIVATE|client_secret)')
ALLOWED_EXT={".service",".rules",".timer",".sh",".bash",".py",".conf",".ini",".yaml",".yml",".json",".cfg",".txt",".log",".csv",".toml",".rs",".c",".cpp",".h",".go",".pl",".lua",".rc",".xml",".desktop",".env",".options"}
SPECIAL={"crontab","rc.local","inittab","udev","autostart","modules","environment","xinitrc","bashrc"}
BLOCK_DIR={"proc","sys","dev","run","tmp","lost+found","node_modules",".cache","__pycache__",".git",".local","__snapshots__","pip","go","rustlib","locale","man","doc"}
def safe(x):
 x=x.replace("\0","").strip()
 if S.search(x): return "[omitted sensitive line]"
 x=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[IPv4]',x)
 x=re.sub(r'[\w.+-]+@[\w.-]+\.\w+','[email]',x)
 return x[:280]
def command(cmd):
 try:
  r=subprocess.run(cmd,text=True,errors="replace",stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=20)
  return r.stdout[:10000].strip()
 except Exception as e:return str(e)
print("=== TOP-LEVEL STRUCTURE ===")
for p in sorted(R.iterdir()):
 try:
  typ="symlink:"+os.readlink(p) if p.is_symlink() else ("dir" if p.is_dir() else "file")
  print(str(p.relative_to(R)),typ)
 except OSError:pass
print("=== IMPORTANT DIRECTORY LISTINGS ===")
for d in ["hive","hive/bin","hive/etc","hive/opt","opt","usr/local/bin","usr/local/sbin","etc/systemd/system","etc/udev/rules.d","etc/init.d","var/log","root","home","home/user"]:
 p=R/d
 if p.exists() and p.is_dir():
  try:
   children=sorted([x.name for x in p.iterdir()])
   print(d,len(children),", ".join(children[:130]))
  except OSError:pass
print("=== FSTAB AND PACKAGE MARKERS ===")
for d in ["etc/fstab","etc/lsb-release","etc/hive-release","etc/hiveos-release"]:
 p=R/d
 if p.is_file():
  for line in p.read_text(errors="replace").splitlines()[:20]:print(d,safe(line))
print("=== FILE INVENTORY / PATH MATCH ===")
dirs=["hive","opt","root","home","etc","usr/local","usr/bin","usr/sbin","usr/lib/udev","lib/udev","var/log","var/lib","usr/share/hive","boot"]
total=0; paths=[]; possible=[]; binary=[]
for d in dirs:
 base=R/d
 if not base.exists():continue
 if base.is_symlink():
  print("SYMLINK_ROOT",d,os.readlink(base));continue
 for at,sub,fs in os.walk(base,followlinks=False):
  sub[:]=[x for x in sub if x not in BLOCK_DIR]
  for name in fs:
   p=Path(at)/name
   try:
    if p.is_symlink():continue
    st=p.stat()
    if not stat.S_ISREG(st.st_mode):continue
   except OSError:continue
   sz=st.st_size
   rel=p.relative_to(R).as_posix()
   total+=1
   if NAME.search(name):
    paths.append((rel,sz))
   if sz < 4000000 and (p.suffix.lower() in ALLOWED_EXT or name in SPECIAL or rel.startswith(("var/log","etc/udev"))):
    possible.append((p,rel))
   if 5000<sz<40000000 and (rel.startswith(("hive/","opt/","usr/local/","home/","root/")) or NAME.search(name)):
    binary.append((p,rel,sz))
print("totalfiles",total,"text_candidates",len(possible),"bin_candidates",len(binary),"name_hits",len(paths))
for rel,sz in sorted(paths,key=lambda a:(not a[0].startswith(("hive/","opt/","usr/local","etc/","var/log/")),a[0]))[:300]:print("NAME",rel,sz)
print("=== MATCHED TEXT WITH LINES ===")
matches=[]
for p,rel in possible:
 try:
  b=p.read_bytes()
  if b"\0" in b[:1000]:continue
  lines=b.decode("utf-8",errors="replace").splitlines()
 except OSError:continue
 hit=[i for i,line in enumerate(lines) if T.search(line)]
 if hit:
  priority=40 if rel.startswith(("hive/","opt/","usr/local/")) else 15 if rel.startswith(("etc/","var/log/")) else 3
  matches.append((priority+min(len(hit),40),rel,lines,hit))
matches.sort(reverse=True)
print("text_files_with_matches",len(matches))
for score,rel,lines,hits in matches[:130]:
 print(">>>",rel,"matches",len(hits),"score",score)
 for k in hits[:12]:
  for i in range(max(0,k-1),min(len(lines),k+2)):
   print(f" {i+1} {safe(lines[i])}")
print("=== EXECUTABLE/ELF STRINGS (limited) ===")
detected=0
for p,rel,sz in binary[:450]:
 try:
  with open(p,"rb") as f: mg=f.read(4)
  if mg != b'\x7fELF':continue
  r=subprocess.run(["strings","-a","-n","6",str(p)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,errors="replace",timeout=8)
  lines=[safe(x) for x in r.stdout.splitlines() if T.search(x) and len(x)<700]
  if lines:
   print("ELF",rel,"bytes",sz,"hits",len(lines))
   for s in lines[:15]:print(" ",s)
   detected+=1
 except Exception:pass
print("elf_with_matches",detected)
print("=== END ===")
