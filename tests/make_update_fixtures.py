"""Small real ZIP/SFO fixtures, including invalid archives; no VitaSDK required."""
import struct, sys, zipfile
from pathlib import Path
root=Path(sys.argv[1]);root.mkdir(exist_ok=True)
(root/'ux0:data/PSVSD_Accessory/update').mkdir(parents=True,exist_ok=True)
(root/'app0:installer').mkdir(exist_ok=True)
for p in ['app0:main-head.bin','app0:installer/eboot.bin','app0:installer/param.sfo','app0:installer/head.bin']:(root/p).write_bytes(b'fixture')
(root/'app0:sce_sys').mkdir(exist_ok=True);(root/'app0:sce_sys/icon0.png').write_bytes(b'fixture')
def sfo(title='RTPSV0001',version='01.10'):
    keys=b'TITLE_ID\0APP_VER\0CONTENT_ID\0';values=b'';entries=b''
    for key,value in [('TITLE_ID',title),('APP_VER',version),('CONTENT_ID','HB0001-'+title+'_00-0000000000000000')]:
        value=value.encode()+b'\0';entries+=struct.pack('<HHIII',keys.index(key.encode()),0x204,len(value),len(value),len(values));values+=value
    return struct.pack('<5I',0x46535000,0x101,68,68+len(keys),3)+entries+keys+values
for name in ['good','traversal','duplicate','wrong_title','wrong_version','oversize','truncated','crc','symlink','missing_helper','wrong_helper_title','wrong_helper_version']:
    with zipfile.ZipFile(root/(name+'.vpk'),'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('sce_sys/param.sfo',sfo('OTHER0001' if name=='wrong_title' else 'RTPSV0001','01.11' if name=='wrong_version' else '01.10'))
        z.writestr('eboot.bin',b'executable fixture')
        z.writestr('main-head.bin',b'new main head')
        z.writestr('sce_sys/icon0.png',b'new icon')
        if name!='missing_helper':
            z.writestr('installer/eboot.bin',b'new helper')
            z.writestr('installer/head.bin',b'new helper head')
            z.writestr('installer/param.sfo',sfo('OTHER0001' if name=='wrong_helper_title' else 'RTPSVUPD1','01.11' if name=='wrong_helper_version' else '01.10'))
        if name=='traversal':z.writestr('../escape',b'bad')
        if name=='duplicate':z.writestr('eboot.bin',b'duplicate')
        if name=='oversize':z.writestr('large',b'A'*(32*1024*1024+1))
        if name=='symlink':
            i=zipfile.ZipInfo('link');i.create_system=3;i.external_attr=0o120777<<16;z.writestr(i,b'../escape')
    p=root/(name+'.vpk');b=bytearray(p.read_bytes())
    if name=='truncated':p.write_bytes(b[:-30])
    if name=='crc':
        start=b.index(b'PK\x01\x02');b[start+16]^=1;p.write_bytes(b)
