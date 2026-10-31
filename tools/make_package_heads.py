#!/usr/bin/env python3
# VitaShell fake-package header algorithm, TheFloW, GPL-3.0-or-later.
from pathlib import Path
import hashlib, struct
root=Path(__file__).resolve().parents[1]
def hmac(data):
    h=hashlib.sha1(data).digest()
    b=h[4:12]*2+h[12:16]+bytes([h[16],h[1],h[2],h[3]])+h[12:16]+bytes([h[16],h[1],h[2],h[3]])+bytes(32)
    return hashlib.sha1(b).digest()[:16]
for title,name in [('RTPSV0001','main-head.bin'),('RTPSVUPD1','updater/head.bin')]:
    b=bytearray((root/'tools/head-template.bin').read_bytes())
    content=('HB0001-'+title+'_00-0000000000000000').encode()
    b[0x30:0x60]=content.ljust(48,b'\0')
    u=lambda off:struct.unpack_from('>I',b,off)[0]
    n=u(0xd0);b[n:n+16]=hmac(b[:n])
    off,n,out=u(8),u(0x10),u(0xd4);b[out:out+16]=hmac(b[off:off+n-64])
    n=u(0xe8);b[n:n+16]=hmac(b[:n])
    (root/'assets'/name).write_bytes(b)
