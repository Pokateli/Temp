#!/usr/bin/env python3
"""Extra II firmware preflight: read-only, stdlib only. Exit 1 for broken images."""
import sys,struct,binascii,hashlib,json
from pathlib import Path
def check(path):
 b=Path(path).read_bytes(); issues=[]
 if len(b)<64 or b[:4]!=bytes.fromhex("27051956"):
  raise ValueError("Not a legacy uImage")
 h=bytearray(b[:64]); fields=struct.unpack(">7I4B32s",h)
 hcrc=fields[1];h[4:8]=bytes(4)
 if binascii.crc32(h)&0xffffffff!=hcrc:issues.append("Invalid uImage header CRC")
 sz=fields[3]
 if sz>len(b)-64 or binascii.crc32(b[64:64+sz])&0xffffffff!=fields[6]:issues.append("Invalid kernel payload CRC")
 off=b.find(b"hsqs",64+sz)
 if off<0:issues.append("Missing SquashFS signature")
 else:
  sb=struct.unpack_from("<5I6H8Q",b,off)
  if sb[9:11]!=(4,0):issues.append("Unexpected SquashFS version")
  if sb[12]>len(b)-off:issues.append("SquashFS truncated")
 if b"TLV.NDM" not in b:issues.append("No TLV.NDM (official SDK unpack.sh incompatible)")
 if b"ki_rb 0.00.A.0.0-0" in b[-512:]:issues.append("Placeholder firmware version")
 return {"file":path,"sha256":hashlib.sha256(b).hexdigest(),"uimage_name":fields[-1].split(bytes(1))[0].decode("ascii","replace"),"squashfs_offset":off,"issues":issues,"note":"Checks only container format; inspect actual SquashFS contents with full audit kit. NEVER flash on preflight alone."}
if __name__=="__main__":
 try:print(json.dumps(check(sys.argv[1]),indent=2));sys.exit(0)
 except Exception as e:print("UNSAFE:",e,file=sys.stderr);sys.exit(1)
