#!/usr/bin/env python3
"""Rebuild PSVSD/AM's original vector assets and bitmap text atlases (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import re

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets'
FONT=ASSETS/'fonts/DejaVuSans.ttf'
BOLD=ASSETS/'fonts/DejaVuSans-Bold.ttf'

def fonts():
    pixels=[];records=[];descriptors=[]
    for size,bold in [(18,False),(22,False),(30,True),(42,True)]:
        font=ImageFont.truetype(str(BOLD if bold else FONT),size)
        first=len(records)
        for code in range(32,127):
            char=chr(code);box=font.getbbox(char);w=max(0,box[2]-box[0]);h=max(0,box[3]-box[1])
            im=Image.new('L',(max(1,w),max(1,h)))
            ImageDraw.Draw(im).text((-box[0],-box[1]),char,font=font,fill=255)
            records.append((len(pixels),w,h,box[0],box[1],round(font.getlength(char))))
            pixels.extend(im.crop((0,0,w,h)).tobytes())
        descriptors.append((first,size))
    lines=['/* Generated from bundled DejaVu Sans. See assets/fonts/LICENSE.txt. */',
           'typedef struct { unsigned offset; unsigned char w,h; signed char x,y; unsigned char advance; } UiGlyph;',
           'static const unsigned char ui_font_pixels[]={']
    for i in range(0,len(pixels),48):lines.append(','.join(map(str,pixels[i:i+48]))+',')
    lines+=['};','static const UiGlyph ui_glyphs[]={']
    lines.extend('{'+','.join(map(str,r))+'},' for r in records)
    lines+=['};','static const unsigned short ui_font_first[]={'+','.join(str(f) for f,s in descriptors)+'};']
    (ROOT/'src/app/font_data.h').write_text('\n'.join(lines)+'\n')

def png_indexed(im,path):
    im.convert('RGB').quantize(colors=128,method=Image.Quantize.MEDIANCUT).save(path,optimize=True)

def icons():
    # Rear microSD profile follows the
    # Kingston microSD mechanical drawing (page 24), with contacts at the top.
    white='#edf5ff';cyan='#46dcc7';navy='#102137';size=512
    version=re.search(r'#define ACCESSORY_VERSION "([^"]+)"',(ROOT/'src/common/paths.h').read_text()).group(1)
    # Unit geometry is 11 x 15. SVG curves and raster samples share the path.
    commands=[];outline=[];current=(0,0)
    def move(x,y):
        nonlocal current
        commands.append(f'M{x} {y}');current=(x,y);outline.append(current)
    def line(x,y):
        nonlocal current
        commands.append(f'L{x} {y}');current=(x,y);outline.append(current)
    def quad(cx,cy,x,y):
        nonlocal current
        commands.append(f'Q{cx} {cy} {x} {y}');sx,sy=current
        for i in range(1,13):
            t=i/12;outline.append(((1-t)**2*sx+2*(1-t)*t*cx+t*t*x,(1-t)**2*sy+2*(1-t)*t*cy+t*t*y))
        current=(x,y)
    move(2.1,0);line(10.2,0);quad(11,0,11,.8);line(11,14.2);quad(11,15,10.2,15)
    line(.8,15);quad(0,15,0,14.2);line(0,9.7);line(.7,9.0);line(.7,8.15)
    quad(.7,7.9,.45,7.9);line(.25,7.9);quad(0,7.9,0,7.65);line(0,6.5)
    quad(0,6.4,.12,6.25);line(1.18,5.0);quad(1.3,4.85,1.3,4.65)
    line(1.3,.8);quad(1.3,0,2.1,0)
    path=' '.join(commands)+' Z';scale=16;left=168;top=60
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">',
        '<title>PSVSD/AM - Accessory Storage Mount</title>',
        f'<rect width="512" height="512" fill="{navy}"/>',
        f'<g transform="translate({left} {top}) scale({scale})">',f'<path d="{path}" fill="{cyan}"/>']
    im=Image.new('RGB',(size,size),navy);draw=ImageDraw.Draw(im)
    draw.polygon([(round(left+x*scale),round(top+y*scale)) for x,y in outline],fill=cyan)
    for i in range(8):
        x=1.9+i*1.1;y=.8 if i in (3,5) else 1.1
        svg.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="0.8" height="{4-y:.2f}" fill="{navy}"/>')
        draw.rectangle((round(left+x*scale),round(top+y*scale),round(left+(x+.8)*scale),round(top+4*scale)),fill=navy)
    svg.append('</g>')
    draw.text((256,269),version,font=ImageFont.truetype(str(BOLD),50),anchor='ms',fill=navy)
    draw.text((256,378),'PSVSD/AM',font=ImageFont.truetype(str(BOLD),64),anchor='ms',fill=white)
    draw.text((256,413),'Accessory',font=ImageFont.truetype(str(FONT),28),anchor='ms',fill=cyan)
    draw.text((256,446),'Storage Mount',font=ImageFont.truetype(str(FONT),28),anchor='ms',fill=white)
    svg+=['<g text-anchor="middle" font-family="DejaVu Sans,sans-serif">',
        f'<text x="256" y="269" font-size="50" font-weight="bold" fill="{navy}">{version}</text>',
        '<text x="256" y="378" font-size="64" font-weight="bold" fill="#edf5ff">PSVSD/AM</text>',
        '<text x="256" y="413" font-size="28" fill="#46dcc7">Accessory</text>',
        '<text x="256" y="446" font-size="28" fill="#edf5ff">Storage Mount</text></g></svg>']
    (ASSETS/'icon.svg').write_text('\n'.join(svg)+'\n')
    icon=im.resize((128,128),Image.Resampling.LANCZOS);png_indexed(icon,ASSETS/'icon0.png')
    bg=Image.new('RGB',(840,500),'#09111f');draw=ImageDraw.Draw(bg)
    draw.rounded_rectangle((28,28,812,472),radius=28,fill='#102137',outline='#233e57',width=2)
    draw.text((58,76),'PSVSD/AM',font=ImageFont.truetype(str(BOLD),64),fill=white)
    draw.text((62,160),'Accessory Storage Mount',font=ImageFont.truetype(str(FONT),28),fill=cyan)
    draw.text((62,214),'Internal storage. Open gamecard slot.',font=ImageFont.truetype(str(FONT),23),fill='#9bacc3')
    draw.text((62,272),'v'+version,font=ImageFont.truetype(str(BOLD),24),fill=cyan)
    round_mask=Image.new('L',(224,224));ImageDraw.Draw(round_mask).ellipse((0,0,223,223),fill=255)
    bg.paste(im.resize((224,224),Image.Resampling.LANCZOS),(560,126),round_mask)
    draw.text((62,400),'RefurbedTech',font=ImageFont.truetype(str(FONT),23),fill='#9bacc3')
    png_indexed(bg,ASSETS/'bg.png')
    startup=Image.new('RGB',(280,158),'#102137');startup.paste(im.resize((136,136),Image.Resampling.LANCZOS),(12,10))
    ImageDraw.Draw(startup).text((150,62),'OPEN',font=ImageFont.truetype(str(BOLD),24),fill=white)
    png_indexed(startup,ASSETS/'startup.png')
    template='''<?xml version="1.0" encoding="UTF-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background><image>bg.png</image></livearea-background>
  <gate><startup-image>startup.png</startup-image></gate>
</livearea>
'''
    (ASSETS/'template.xml').write_bytes(template.replace('\n','\r\n').encode())

if __name__=='__main__':fonts();icons()
