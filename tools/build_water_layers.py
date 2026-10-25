from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
from pathlib import Path
from PIL import Image
import re,json,math
import numpy as np

ROOT=REPO_ROOT
APP=ASSET_DIR
PACK=PACK_DIR
OUT=REPORT_DIR
text=(PACK/'hires.txt').read_text(encoding='utf-8-sig')
groups=[('waterfall','WaterfallFrame',8),('River','riverwaterframe',5),('OceanShore','OceanShore',5)]
specs=[[None]*3 for _ in range(128)]
report=[]
dest=APP/'hd/anim';dest.mkdir(exist_ok=True)
for layer,(prefix,condition,n) in enumerate(groups):
    # Still-room rules supply the same atlas offset used during NES scrolling.
    coordinates={}
    for line in text.splitlines():
        if 'ScrollNone]' not in line or f'<background>{prefix}1.png,'.lower() not in line.lower():continue
        room=re.search(r'Overworld([0-9A-F]{2})&ScrollNone',line)
        bg=re.search(r'<background>[^,]+,1,0,0,\d+,(\d+),(\d+)',line)
        if room and bg:coordinates[int(room[1],16)]=(int(bg[1]),int(bg[2]))
    period=None;begins=[]
    for f in range(1,n+1):
        m=re.search(rf'<condition>{condition}{f},frameRange,(\d+),(\d+)',text)
        assert m,(condition,f)
        period=int(m[1]);begins.append(int(m[2]))
    cropped={r:[] for r in coordinates}
    for frame in range(1,n+1):
        im=Image.open(PACK/f'{prefix}{frame}.png').convert('RGBA')
        for room,(x,y) in coordinates.items():
            crop=im.crop((x*2,(y+64)*2,x*2+512,(y+64)*2+352))
            a=np.array(crop);a[np.all(a[:,:,:3]==[255,0,255],axis=2),3]=0
            crop=Image.fromarray(a)
            cropped[room].append(crop)
        del im
    count=0
    for room,frames in cropped.items():
        boxes=[f.getchannel('A').getbbox() for f in frames]
        boxes=[b for b in boxes if b]
        if not boxes:continue
        x=min(b[0] for b in boxes)//2*2;y=min(b[1] for b in boxes)//2*2
        right=math.ceil(max(b[2] for b in boxes)/2)*2;bottom=math.ceil(max(b[3] for b in boxes)/2)*2
        w=right-x;h=bottom-y;cols=min(n,1024//w);per=cols*(1024//h)
        assert per>0
        pages=math.ceil(n/per);assert pages<=2
        for page in range(pages):
            size=min(per,n-page*per);rows=math.ceil(size/cols)
            sheet=Image.new('RGBA',(cols*w,rows*h))
            for local in range(size):sheet.paste(frames[page*per+local].crop((x,y,right,bottom)),((local%cols)*w,(local//cols)*h))
            sheet.save(dest/f'{room:02X}_{layer}_{page}.png')
        spec=[x,y,w,h,cols,per,n,period,begins+[0]*(8-n)]
        specs[room][layer]=spec;report.append({'room':f'{room:02X}','layer':prefix,'spec':spec,'pages':pages});count+=1
    print(prefix,'animated rooms',count,'period',period,'begins',begins,flush=True)

header=['// Generated from original hires.txt background rules.','#pragma once', 'struct HDWaterSpec { int x,y,w,h,columns,perPage,frames,period; int begins[8]; };','static const HDWaterSpec hdWaterSpecs[128][3] = {']
for layers in specs:
    line=[]
    for a in layers:
        line.append('{0,0,0,0,0,0,0,0,{0}}' if a is None else '{'+','.join(map(str,a[:8]))+',{'+','.join(map(str,a[8]))+'}}')
    header.append('    {'+','.join(line)+'},')
header.append('};')
(ROOT/'Loz/Game/Loz/HDGeneratedWater.h').write_text('\n'.join(header)+'\n')
(OUT/'water-rules.json').write_text(json.dumps(report,indent=2))
print('layers',len(report),'assets',len(list(dest.glob('*.png'))))
