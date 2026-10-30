#!/usr/bin/env python3
"""Control one running Tiny11 QEMU VM via its local Unix HMP socket.

Examples:
    python3 vm-console.py status --vm-dir /mnt/data/tiny11-graphics-session
    python3 vm-console.py screenshot --vm-dir /mnt/data/tiny11-graphics-session
    python3 vm-console.py wake --vm-dir /mnt/data/tiny11-graphics-session
    python3 vm-console.py powerdown --vm-dir /mnt/data/tiny11-graphics-session

The VM remains local and offline: no network/VNC exposure.
Screenshot outputs a PPM; if Pillow is installed a PNG is also generated.
The immutable Tiny11 base QCOW2 is never touched by this utility.
"""
import argparse
from pathlib import Path
import re
import socket
import sys
import time

ANSI=re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
NAME=re.compile(r"^[a-z0-9_+.-]+$",re.I)

def hmp(socket_path,command,timeout):
    if not socket_path.is_socket():
        raise RuntimeError("QEMU monitor socket not found; VM may be stopped: "+str(socket_path))
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
        s.settimeout(timeout)
        s.connect(str(socket_path))
        try:s.recv(8192)
        except socket.timeout:pass
        s.sendall((command+"\n").encode("ascii"))
        chunks=[]
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            try:
                data=s.recv(8192)
                if not data:break
                chunks.append(data)
                if b"(qemu) " in data:break
            except socket.timeout:break
        return ANSI.sub("",b"".join(chunks).decode("utf-8",errors="replace"))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("action",choices=["status","screenshot","wake","powerdown","sendkey"])
    p.add_argument("--vm-dir",type=Path,default=Path("/mnt/data/tiny11-tcg-session"))
    p.add_argument("--keys",help="QEMU HMP key combination for sendkey, e.g. ctrl-alt-delete")
    p.add_argument("--timeout",type=float,default=6.0)
    args=p.parse_args()
    root=args.vm_dir.resolve()
    monitor=root/"qemu-monitor.sock"
    if args.timeout<1 or args.timeout>30:
        p.error("timeout must be between 1 and 30 seconds")
    if args.action=="status":
        cmd="info status"
    elif args.action=="wake":
        cmd="system_wakeup"
    elif args.action=="powerdown":
        cmd="system_powerdown"
    elif args.action=="sendkey":
        if not args.keys or len(args.keys)>128 or not NAME.fullmatch(args.keys):
            p.error("supply a valid QEMU key combination using --keys")
        cmd="sendkey "+args.keys
    else:
        # output path is fixed to the selected VM session directory,
        # preventing accidental overwrite of the base image or firmware.
        cmd="screendump "+str(root/"screen-captured.ppm")
    output=hmp(monitor,cmd,args.timeout)
    if "Error:" in output or "unknown command" in output.lower():
        raise SystemExit(output)
    print(output.strip())
    if args.action=="screenshot":
        ppm=root/"screen-captured.ppm"
        if not ppm.is_file() or ppm.stat().st_size<128:
            raise SystemExit("QEMU did not produce a framebuffer screendump")
        print("PPM:",ppm,"bytes:",ppm.stat().st_size)
        try:
            from PIL import Image
        except ImportError:
            print("Install Pillow for optional PNG conversion, or open PPM directly")
        else:
            png=root/"screen-captured.png"
            Image.open(ppm).save(png)
            print("PNG:",png,"bytes:",png.stat().st_size)
if __name__=="__main__":
    main()
