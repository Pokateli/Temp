#!/usr/bin/env python3
"""Stream a verified Tiny11-ready VM kit into connector-sized public GitHub artifacts.

This workflow is explicitly for the authorized public Temp development sandbox.
All output parts are UNENCRYPTED. The kit may contain local Windows credentials.
No intermediary tar archive is created; each chunk is <= 384 MiB.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import sys

CHUNK=384*1024*1024
FILES=("tiny11-ready-zstd.qcow2","OVMF_CODE.fd","OVMF_VARS.fd",
       "tiny11-ready-manifest.json","ready-credentials.txt","SHA256SUMS")

def digest(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(4*1024*1024),b""):h.update(b)
    return h.hexdigest()

class ChunkWriter:
    def __init__(self,root):
        self.root=root;self.index=0;self.output=None;self.offset=0
        self.file_list=[];self.total=0;self.hasher=None
    def writable(self):return True
    def write(self,data):
        view=memoryview(data)
        while view:
            if self.output is None:
                name=f"part-{self.index:03d}.bin"
                self.output=(self.root/name).open("wb")
                self.hasher=hashlib.sha256()
                self.file_list.append({"name":name,"size":0,"sha256":None})
                self.offset=0
            b=view[:CHUNK-self.offset]
            self.output.write(b);self.hasher.update(b);self.total+=len(b)
            self.offset+=len(b)
            self.file_list[-1]["size"]+=len(b)
            view=view[len(b):]
            if self.offset>=CHUNK:self.close_part()
        return len(data)
    def close_part(self):
        if self.output:
            self.file_list[-1]["sha256"]=self.hasher.hexdigest()
            self.output.close();self.output=None;self.index+=1
    def finish(self):self.close_part()

class ChunkReader:
    def __init__(self,root,meta):
        self.root=root;self.meta=meta;self.index=0
        self.input=None;self.remaining=0;self.hash=None
    def readable(self):return True
    def read(self,count):
        if count is None or count<0:count=1024*1024
        if self.input is None:
            if self.index>=len(self.meta["parts"]):return b""
            item=self.meta["parts"][self.index]
            name=item["name"]
            if name!=f"part-{self.index:03d}.bin":
                raise ValueError("Unexpected part name")
            self.input=(self.root/name).open("rb")
            self.remaining=item["size"]
            self.hash=hashlib.sha256()
        data=self.input.read(min(count,self.remaining))
        if not data and self.remaining:raise ValueError("Truncated archive part")
        self.remaining-=len(data);self.hash.update(data)
        if self.remaining==0:
            if self.hash.hexdigest()!=self.meta["parts"][self.index]["sha256"]:
                raise ValueError("Part hash mismatch")
            self.input.close();self.input=None;self.index+=1
        return data

def pack(src,dest):
    dest.mkdir(parents=True,exist_ok=True)
    if any(dest.iterdir()):raise SystemExit("Refuse non-empty destination")
    manifest={}
    for name in FILES:
        p=src/name
        if not p.is_file():raise SystemExit("Missing ready file: "+name)
        manifest[name]={"size":p.stat().st_size,"sha256":digest(p)}
    writer=ChunkWriter(dest)
    with tarfile.open(fileobj=writer,mode="w|",format=tarfile.PAX_FORMAT) as tar:
        for name in FILES:
            tar.add(src/name,arcname=name,recursive=False)
    writer.finish()
    index={"format":"tiny11-ready-public-parts-v1","unencrypted":True,
           "parts":writer.file_list,"tar_bytes":writer.total,
           "members":manifest}
    (dest/"ready-parts.json").write_text(json.dumps(index,indent=2)+"\n")
    print("Prepared",len(writer.file_list),"public parts totaling",writer.total,"bytes")
    print("Largest size",max((x["size"] for x in writer.file_list),default=0),"bytes")

def unpack(src,dest):
    idx=json.loads((src/"ready-parts.json").read_text())
    if idx.get("format")!="tiny11-ready-public-parts-v1":raise SystemExit("Invalid index")
    if sorted(idx["members"])!=sorted(FILES):raise SystemExit("Unexpected kit member listing")
    dest.mkdir(parents=True,exist_ok=True)
    if any(dest.iterdir()):raise SystemExit("Refuse nonempty destination")
    reader=ChunkReader(src,idx)
    with tarfile.open(fileobj=reader,mode="r|") as tar:
        seen=set()
        for item in tar:
            if item.name not in FILES or item.name in seen or not item.isfile():
                raise ValueError("Unknown, repeated or unsafe tar member")
            seen.add(item.name)
            if item.size!=idx["members"][item.name]["size"]:
                raise ValueError("Member size mismatch")
            with (dest/item.name).open("wb") as out:
                f=tar.extractfile(item)
                for block in iter(lambda:f.read(4*1024*1024),b""):out.write(block)
    if seen!=set(FILES):raise ValueError("Archive has missing files")
    for name in FILES:
        p=dest/name
        if digest(p)!=idx["members"][name]["sha256"]:
            raise ValueError("Restored member SHA256 mismatch: "+name)
    print("RESTORED verified ready Tiny11 VM kit")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("action",choices=("pack","unpack"))
    p.add_argument("source",type=Path)
    p.add_argument("destination",type=Path)
    a=p.parse_args()
    (pack if a.action=="pack" else unpack)(a.source,a.destination)
if __name__=="__main__":main()
