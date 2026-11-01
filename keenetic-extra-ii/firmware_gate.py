#!/usr/bin/env python3
"""Read-only fail-closed structural gate: Keenetic MIPS uImage + SquashFS/XZ.
Never certifies that a router can actually boot. Stdlib only.
"""
import argparse,binascii,hashlib,json,lzma,struct
from pathlib import Path
FIELDS=('magic','inodes','mkfs','block','fragments','compression','block_log','flags','ids','major','minor','root_inode','bytes_used','id_table','xattr_table','inode_table','directory_table','fragment_table','lookup_table')
def metadata(fs,start,stop):
    out,offsets,p=bytearray(),{},start
    if start>=stop or stop>len(fs):raise ValueError('invalid metadata extent')
    while p<stop:
        if p+2>stop:raise ValueError('short metadata header')
        h,=struct.unpack_from('<H',fs,p); size=h&32767
        if not size or p+2+size>stop:raise ValueError('invalid metadata block')
        raw=fs[p+2:p+2+size]
        data=raw if h&32768 else lzma.decompress(raw)
        if len(data)>8192:raise ValueError('oversize metadata block')
        offsets[p-start]=len(out);out.extend(data);p+=size+2
    return out,offsets
def paths(blob,off):
    fs=blob[off:]
    if len(fs)<96:raise ValueError('short SquashFS')
    sb=dict(zip(FIELDS,struct.unpack_from('<5I6H8Q',fs)))
    if (sb['magic'],sb['major'],sb['minor'],sb['compression'])!=(0x73717368,4,0,4):raise ValueError('expected SquashFS 4/XZ')
    if sb['bytes_used']>len(fs):raise ValueError('truncated SquashFS')
    imem,imap=metadata(fs,sb['inode_table'],sb['directory_table'])
    dmem,dmap=metadata(fs,sb['directory_table'],sb['fragment_table'])
    found={'/'};visited=set()
    def walk(ref,parent,depth=0):
        if depth>40:raise ValueError('excessive directory depth')
        block,rel=ref>>16,ref&65535
        if block not in imap:raise ValueError('unknown inode block')
        p=imap[block]+rel
        if p+32>len(imem):raise ValueError('short inode')
        kind,_,_,_,_,ino=struct.unpack_from('<4H2I',imem,p)
        if kind not in (1,8) or ino in visited:return
        visited.add(ino)
        if kind==1:
            start,_,length,delta,_=struct.unpack_from('<IIHHI',imem,p+16)
        else:
            _,length,start,_,_,delta,_=struct.unpack_from('<4I2HI',imem,p+16)
        if start not in dmap:raise ValueError('unknown directory block')
        q=dmap[start]+delta;end=q+length-3
        if end>len(dmem):raise ValueError('directory outside table')
        while q<end:
            count,iblock,_=struct.unpack_from('<3I',dmem,q);q+=12
            if count>2000:raise ValueError('invalid directory count')
            for _ in range(count+1):
                rel,_,typ,nlen=struct.unpack_from('<HhHH',dmem,q);q+=8
                if q+nlen+1>len(dmem):raise ValueError('short name')
                name=bytes(dmem[q:q+nlen+1]).decode('utf8','replace');q+=nlen+1
                if not name or '/' in name or name in ('.','..'):raise ValueError('invalid name')
                path=parent+'/'+name;found.add(path)
                if typ in (1,8):walk((iblock<<16)|rel,path,depth+1)
        if q!=end:raise ValueError('invalid directory length')
    walk(sb['root_inode'],'')
    if len(found)!=sb['inodes']:raise ValueError(f'inode/path mismatch: {len(found)} vs {sb["inodes"]}')
    return found
def inspect(name,expected_profile='ki_rb',sdk_unpack=False):
    blob=Path(name).read_bytes();issues=[];warnings=[]
    report=dict(filename=Path(name).name,size=len(blob),sha256=hashlib.sha256(blob).hexdigest())
    if len(blob)<64 or blob[:4]!=bytes.fromhex('27051956'):
        return dict(report,status='BLOCKED',issues=['not a uImage'],warnings=[])
    _,hcrc,_,ksize,_,_,dcrc,_,arch,typ,comp,profile=struct.unpack_from('>7I4B32s',blob)
    profile=profile.split(b'\0')[0].decode('ascii','replace');report.update(profile=profile,kernel_size=ksize)
    h=bytearray(blob[:64]);h[4:8]=b'\0'*4
    if binascii.crc32(h)&0xffffffff!=hcrc:issues.append('uImage header CRC mismatch')
    if ksize>len(blob)-64 or binascii.crc32(blob[64:64+ksize])&0xffffffff!=dcrc:issues.append('uImage payload CRC/size mismatch')
    if (arch,typ,comp)!=(5,2,3):issues.append('expected MIPS Linux LZMA kernel')
    if expected_profile and profile!=expected_profile:issues.append(f'profile {profile!r} != {expected_profile!r}')
    off=blob.find(b'hsqs',64+ksize);report['squashfs_offset']=off
    if off<0:issues.append('SquashFS not found')
    else:
        try:
            p=paths(blob,off);report['filesystem_paths']=len(p)
            report['init_candidates_present']=[s for s in ('/init','/sbin/init','/bin/init','/sbin/ndm','/usr/sbin/ndm') if s in p]
            if not report['init_candidates_present']:issues.append('no userspace init/ndm entrypoint')
            if '/bin' not in p or '/sbin' not in p:issues.append('missing /bin or /sbin')
            if '/etc/components.xml' not in p:warnings.append('components.xml missing')
        except Exception as e:issues.append('invalid SquashFS: '+str(e))
    report['tlv_ndm']=b'TLV.NDM' in blob
    if not report['tlv_ndm']:(issues if sdk_unpack else warnings).append('TLV.NDM missing (SDK 5.01 unpack.sh incompatible)')
    if b'ki_rb 0.00.A.0.0-0' in blob[-512:]:warnings.append('placeholder firmware version')
    return dict(report,status='BLOCKED' if issues else 'STRUCTURE_OK_NOT_BOOT_TESTED',issues=issues,warnings=warnings)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('firmware');ap.add_argument('--profile',default='ki_rb');ap.add_argument('--sdk-unpack',action='store_true')
    a=ap.parse_args()
    try:out=inspect(a.firmware,a.profile,a.sdk_unpack)
    except Exception as e:out=dict(status='BLOCKED',issues=[str(e)],warnings=[])
    print(json.dumps(out,ensure_ascii=False,indent=2))
    return int(out['status']!='STRUCTURE_OK_NOT_BOOT_TESTED')
if __name__=='__main__':raise SystemExit(main())
