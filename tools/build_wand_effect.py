from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
from pathlib import Path
from PIL import Image,ImageOps
import re,json,struct
ROOT=REPO_ROOT
APP=ASSET_DIR
PACK=PACK_DIR
text=(PACK/'hires.txt').read_text(encoding='utf-8-sig');names=re.findall(r'^<img>(.*)',text,re.M)
src=Image.open(APP/'playerItem.png').convert('RGB');atlas=Image.open(PACK/'WandEffect.png').convert('RGBA')
atlas.putdata([(r,g,b,0 if (r,g,b)==(255,0,255) else a) for r,g,b,a in atlas.getdata()])
palraw=(APP/'pal.dat').read_bytes()
colors=[(0,0,0,0)]
for c in [2,0x22,0x30]:
    v,=struct.unpack_from('<I',palraw,c*4);colors.append(((v>>16)&255,(v>>8)&255,v&255,255))
base=Image.new('RGBA',src.size);base.putdata([colors[min(3,r>>4)] for r,g,b in src.getdata()])
base=base.resize((256,256),Image.Resampling.NEAREST)
sheet=Image.new('RGBA',(768,256));report=[]
def hash_tile(tile):
    return bytes(sum(((tile[y][x]>>b)&1)<<(7-x) for x in range(8)) for b in range(2) for y in range(8)).hex().upper()
for frame in range(3):
    image=base.copy();rules={}
    for m in re.finditer(rf'\[WandFrame{frame+1}\]<tile>13,([0-9A-Fa-f]{{32}}),FF022230,(\d+),(\d+),1,Y',text):
        rules[m[1].upper()]=(int(m[2]),int(m[3]),m[0])
    matched=[]
    for y in range(0,128,8):
        for x in range(0,128,8):
            t=[[min(3,src.getpixel((x+xx,y+yy))[0]>>4) for xx in range(8)] for yy in range(8)]
            for fx,fy,tt in [(False,False,t),(True,False,[r[::-1] for r in t]),(False,True,t[::-1]),(True,True,[r[::-1] for r in t[::-1]])]:
                m=rules.get(hash_tile(tt))
                if m:
                    sx,sy,line=m;patch=atlas.crop((sx,sy,sx+16,sy+16))
                    if fx:patch=ImageOps.mirror(patch)
                    if fy:patch=ImageOps.flip(patch)
                    image.paste(patch,(x*2,y*2));matched.append([x,y]);break
    sheet.paste(image,(frame*256,0));report.append(matched)
    assert len(matched)>=6,(frame,matched)
dest=APP/'hd/effects';dest.mkdir(exist_ok=True)
sheet.save(dest/'wand.png')
(REPORT_DIR/'wand-coverage.json').write_text(json.dumps(report))
print('Wand tiles per frame:',list(map(len,report)))
