#!/usr/bin/env python3
"""AES-256-GCM chunked encrypted transport, RSA3072 OAEP wrapping. Public GitHub artifact, private ChatGPT key."""
import argparse,json,hashlib,secrets
from pathlib import Path
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
P=16*1024*1024
ap=argparse.ArgumentParser();ap.add_argument('mode',choices=['encrypt','decrypt']);ap.add_argument('source',type=Path);ap.add_argument('destination',type=Path);ap.add_argument('--key',type=Path,required=True)
a=ap.parse_args();a.destination.mkdir(parents=True,exist_ok=True)
if a.mode=='encrypt':
 pub=serialization.load_pem_public_key(a.key.read_bytes())
 key=secrets.token_bytes(32);wrapped=pub.encrypt(key,padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()),algorithm=hashes.SHA256(),label=None))
 m={'format':'rsa-oaep-sha256+aes-256-gcm','wrapped_key':wrapped.hex(),'file_name':a.source.name,'parts':[]};h=hashlib.sha256();aes=AESGCM(key)
 with a.source.open('rb') as f:
  for i,data in enumerate(iter(lambda:f.read(P),b'')):
   h.update(data);nonce=secrets.token_bytes(12);n=f'chunk-{i:05d}.bin';blob=nonce+aes.encrypt(nonce,data,str(i).encode());(a.destination/n).write_bytes(blob)
   m['parts'].append({'name':n,'sha256':hashlib.sha256(blob).hexdigest()});print('encrypted part',i,len(data),flush=True)
 m['file_sha256']=h.hexdigest();(a.destination/'manifest.json').write_text(json.dumps(m,indent=2));print('SEALED',m['file_sha256'])
else:
 m=json.loads((a.source/'manifest.json').read_text());assert m['format']=='rsa-oaep-sha256+aes-256-gcm';priv=serialization.load_pem_private_key(a.key.read_bytes(),password=None)
 key=priv.decrypt(bytes.fromhex(m['wrapped_key']),padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()),algorithm=hashes.SHA256(),label=None));aes=AESGCM(key);out=a.destination/m['file_name'];h=hashlib.sha256()
 with out.open('wb') as f:
  for i,part in enumerate(m['parts']):
   assert part['name']==f'chunk-{i:05d}.bin';blob=(a.source/part['name']).read_bytes();assert hashlib.sha256(blob).hexdigest()==part['sha256']
   data=aes.decrypt(blob[:12],blob[12:],str(i).encode());h.update(data);f.write(data)
 if h.hexdigest()!=m['file_sha256']:out.unlink();raise ValueError('SHA256 mismatch')
 print('RESTORED',out,h.hexdigest())
