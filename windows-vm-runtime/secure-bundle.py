#!/usr/bin/env python3
"""Encrypt/decrypt a prepared Windows QCOW2 bundle in 128 MiB authenticated chunks.

Use only with authorized Windows evaluation/licensing. Do not place plaintext OS
images or the passphrase in public GitHub Actions artifacts/logs.
Needs cryptography: python -m pip install cryptography
Set TINY11_ARCHIVE_PASSPHRASE (>=16 chars) via GitHub Actions encrypted secret.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import sys

CHUNK_SIZE=128*1024*1024
FILES=("tiny11-ready-zstd.qcow2","OVMF_CODE.fd","OVMF_VARS.fd",
       "tiny11-ready-manifest.json","ready-credentials.txt","SHA256SUMS")
def sha(buf):return hashlib.sha256(buf).hexdigest()
def main():
    a=argparse.ArgumentParser()
    a.add_argument("mode",choices=("encrypt","decrypt"))
    a.add_argument("source",type=Path)
    a.add_argument("destination",type=Path)
    x=a.parse_args()
    secret=os.environ.get("TINY11_ARCHIVE_PASSPHRASE","")
    if len(secret)<16:raise SystemExit("TINY11_ARCHIVE_PASSPHRASE missing or under 16 characters")
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    salt=None
    if x.mode=="encrypt":
        salt=secrets.token_bytes(16)
    else:
        manifest=json.loads((x.source/"envelope.json").read_text("utf-8"))
        if manifest.get("version")!=1 or manifest.get("cipher")!="AES-256-GCM":
            raise SystemExit("Unsupported encrypted bundle format")
        salt=bytes.fromhex(manifest["kdf_salt"])
    key=hashlib.scrypt(secret.encode("utf-8"),salt=salt,n=32768,r=8,p=1,dklen=32,maxmem=128*1024*1024)
    aes=AESGCM(key)
    dest=x.destination.resolve()
    dest.mkdir(parents=True,exist_ok=True)
    if x.mode=="encrypt":
        manifest={"version":1,"cipher":"AES-256-GCM","kdf":"scrypt",
                  "kdf_n":32768,"kdf_r":8,"kdf_p":1,"kdf_salt":salt.hex(),
                  "chunk_size":CHUNK_SIZE,"files":[]}
        seq=0
        for filename in FILES:
            source=x.source/filename
            if not source.is_file():raise SystemExit("Missing ready image component: "+str(source))
            state=hashlib.sha256()
            entry={"name":filename,"size":source.stat().st_size,"chunks":[]}
            with source.open("rb") as f:
                index=0
                while True:
                    data=f.read(CHUNK_SIZE)
                    if not data:break
                    state.update(data)
                    nonce=secrets.token_bytes(12)
                    aad=(filename+":"+str(index)).encode("utf-8")
                    encrypted=nonce+aes.encrypt(nonce,data,aad)
                    filename_out=f"{seq:05d}.aes"
                    (dest/filename_out).write_bytes(encrypted)
                    entry["chunks"].append({"name":filename_out,"sha256":sha(encrypted)})
                    seq+=1;index+=1
            entry["sha256"]=state.hexdigest()
            manifest["files"].append(entry)
            print(f"Encrypted {filename} to {len(entry['chunks'])} AES-256-GCM chunks",flush=True)
        (dest/"envelope.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
        print(f"Encrypted package ready with {seq} parts. Never publish passphrase.")
    else:
        for ent in manifest["files"]:
            name=ent["name"]
            if name not in FILES or Path(name).name!=name:
                raise SystemExit("Unsafe encrypted bundle member")
            output=dest/name
            if output.exists():raise SystemExit("Refusing to overwrite "+str(output))
            tmp=dest/(name+".partial")
            digest=hashlib.sha256()
            with tmp.open("wb") as out:
                for index,item in enumerate(ent["chunks"]):
                    part=item["name"]
                    if Path(part).name!=part or not part.endswith(".aes"):
                        raise SystemExit("Unsafe chunk filename")
                    encrypted=(x.source/part).read_bytes()
                    if sha(encrypted)!=item["sha256"]:
                        raise SystemExit("Encrypted chunk corrupted: "+part)
                    nonce,data=encrypted[:12],encrypted[12:]
                    aad=(name+":"+str(index)).encode("utf-8")
                    clear=aes.decrypt(nonce,data,aad)
                    digest.update(clear)
                    out.write(clear)
            if tmp.stat().st_size!=ent["size"] or digest.hexdigest()!=ent["sha256"]:
                tmp.unlink(missing_ok=True)
                raise SystemExit("Decrypted file size or SHA256 mismatch: "+name)
            tmp.rename(output)
            print("Decrypted and verified",name,flush=True)
        print("PASS: all prepared Windows VM files restored with AES-GCM authentication")

if __name__=="__main__":main()
