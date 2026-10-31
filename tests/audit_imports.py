#!/usr/bin/env python3
"""Check required VELF library/function imports against VitaSDK database models.

Usage: python tests/audit_imports.py VELF SDK_DB_ROOT
Requires PyYAML. Models 3.60 and the 3.63 database overlay; this is not a
running-console library enumeration or a hardware/ABI validation.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import yaml


def imports(path):
    b=Path(path).read_bytes()
    h=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
    assert h[0][:4]==b'\x7fELF' and h[2]==40
    segments=[]
    for i in range(h[10]):
        ph=struct.unpack_from('<8I',b,h[5]+h[9]*i)
        if ph[0]==1: segments.append(ph)
    seg=segments[h[4]>>30]
    data=b[seg[1]:seg[1]+seg[4]]
    entry=h[4]&0x3fffffff
    start,end=struct.unpack_from('<II',data,entry+44)
    def off(addr): return addr-seg[2] if addr>=seg[2] else addr
    def string(addr): return data[off(addr):].split(b'\0',1)[0].decode()
    out=[]
    while start<end:
        size,version,flags,nf=struct.unpack_from('<4H',data,start)
        if size==0x34:
            lib,name=struct.unpack_from('<II',data,start+16)
            functions=struct.unpack_from('<I',data,start+28)[0]
        elif size==0x24:
            lib,name,functions=struct.unpack_from('<III',data,start+12)
        else: raise ValueError(f'Unsupported import table size {size:x}')
        nids=list(struct.unpack_from('<'+'I'*nf,data,off(functions)))
        out.append(dict(name=string(name),nid=lib,functions=nids,version=version,flags=flags))
        start+=size
    return out


def model(root,overlay=False):
    libraries={}
    for folder in (['360','363'] if overlay else ['360']):
        for p in sorted((Path(root)/folder).glob('*.yml')):
            d=yaml.safe_load(p.read_text())
            for m in d.get('modules',{}).values():
                for name,lib in m.get('libraries',{}).items():
                    libraries[name]=dict(nid=lib['nid'],functions=set(lib.get('functions',{}).values()))
    def nid(name): return int.from_bytes(hashlib.sha256(name.encode()).digest()[:4],'big')
    for lib,functions in {
        'taihenForKernel':['taiGetModuleInfoForKernel','taiInjectDataForKernel','taiInjectReleaseForKernel'],
        'taihenModuleUtils':['module_get_offset','module_get_export_func'],
        'taihenUnsafe':['taiLoadKernelModule','taiStartKernelModuleForUser','taiUnloadKernelModule'],
    }.items(): libraries[lib]=dict(nid=nid(lib),functions={nid(fn) for fn in functions})
    return libraries


def audit(path,root):
    required=imports(path)
    result=dict(binary=Path(path).name,required_libraries=[],models=[])
    for lib in required:
        result['required_libraries'].append(dict(name=lib['name'],nid=f"{lib['nid']:08X}",
            functions=[f'{n:08X}' for n in lib['functions']]))
    for label,overlay in [('3.60 database',False),('3.63 overlay on 3.60',True)]:
        known=model(root,overlay);errors=[]
        for lib in required:
            target=known.get(lib['name'])
            if not target: errors.append(f"Unknown library: {lib['name']}")
            elif target['nid']!=lib['nid']:
                errors.append(f"{lib['name']}: required {lib['nid']:08X}, model exports {target['nid']:08X}")
            else:
                for n in lib['functions']:
                    if n not in target['functions']: errors.append(f"{lib['name']}: unknown function {n:08X}")
        result['models'].append(dict(model=label,passed=not errors,errors=errors))
    return result


if __name__=='__main__':
    r=audit(sys.argv[1],sys.argv[2]);print(json.dumps(r,indent=2))
    sys.exit(0 if all(m['passed'] for m in r['models']) else 1)
