from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
from pathlib import Path
from PIL import Image,ImageDraw,ImageOps
import json,re,struct,hashlib
ROOT=REPO_ROOT;APP=ASSET_DIR;OUT=REPORT_DIR
PACK=PACK_DIR
atlas=Image.new('RGBA',(512,512));d=ImageDraw.Draw(atlas)
paper=Image.open(PACK/'PriorityGraphics2.png').convert('RGBA').crop((0,0,160,96))
paper.putdata([(r,g,b,0 if (r,g,b) in [(255,0,255),(0,0,0)] else a) for r,g,b,a in paper.getdata()])
atlas.paste(paper,(0,0))
for state in range(3):
    x=state*28;y=112
    points=[(x+2,y+2),(x+8,y+2),(x+12,y+6),(x+16,y+2),(x+22,y+2),(x+24,y+6),(x+24,y+12),(x+12,y+24),(x,y+12),(x,y+6),(x+2,y+2)]
    d.polygon(points,fill='#C84D44' if state==2 else '#29241E');d.line(points,fill='#8C342E' if state==2 else '#75664D',width=2)
    if state==1:
        filled=Image.new('RGBA',(26,26));fd=ImageDraw.Draw(filled)
        local=[(px-x,py-y) for px,py in points];fd.polygon(local,fill='#C84D44');fd.line(local,fill='#8C342E',width=2)
        atlas.alpha_composite(filled.crop((0,0,12,26)),(x,y))
    if state:
        d.line((x+3,y+6,x+7,y+6),fill='#F49A80',width=2)
        if state==2:d.line((x+12,y+19,x+21,y+10),fill='#9A3733',width=2)
font=Image.open(APP/'font.png').convert('RGBA')
for ch in range(36):
    source=font.crop(((ch&15)*8,(ch>>4)*8,(ch&15)*8+8,(ch>>4)*8+8));a=source.getchannel('R').point(lambda v:255 if v else 0)
    glyph=Image.new('RGBA',(8,8),'#F5E5C0');glyph.putalpha(a);atlas.alpha_composite(glyph,(ch*8,160))
raw=(APP/'playerItemsSheet.tab').read_bytes();n=struct.unpack_from('<H',raw)[0];off=struct.unpack_from('<'+'H'*n,raw,2);heap=raw[2+2*n:]
s=(ROOT/'Loz/Game/Loz/PlayerItemsAnim.h').read_text();names=re.findall(r'Anim_PI_\w+',s[s.index('{'):s.index('}')])
palette=(APP/'overworldInfo.dat').read_bytes()[:32];palraw=(APP/'pal.dat').read_bytes();base=Image.open(APP/'playerItem.png').convert('RGBA')
def sprite(anim,pal,w=28,h=34,override=None):
    _,sw,sh,x,y,flags=heap[off[names.index(anim)]:][:6]
    key=override or palette[pal*4+1:pal*4+4].hex().upper();path=APP/f'hd/tiles/playerItem/{key}.png'
    if path.exists():tile=Image.open(path).convert('RGBA').crop((x*2,y*2,(x+sw)*2,(y+sh)*2))
    else:
        tile=base.crop((x,y,x+sw,y+sh));pixels=[]
        for r,g,b,a in tile.getdata():
            index=min(3,r>>4);v=struct.unpack_from('<I',palraw,(palette[pal*4+index]&63)*4)[0]
            pixels.append(((v>>16)&255,(v>>8)&255,v&255,255 if index else 0))
        tile.putdata(pixels)
    if flags&1:tile=ImageOps.mirror(tile)
    if flags&2:tile=ImageOps.flip(tile)
    bounds=tile.getchannel('A').getbbox()
    if bounds:tile=tile.crop(bounds)
    return ImageOps.contain(tile,(w,h),Image.Resampling.NEAREST)
source=(ROOT/'Loz/Game/Loz/ItemObj.cpp').read_text();body=source.split('static const ItemGraphics sItemGraphics[]')[1].split('};')[0]
items=re.findall(r'\{\s*(Anim_PI_\w+),\s*(PlayerPal|BluePal|RedPal|OtherPal)',body)
for i,(anim,pal) in enumerate(items):
    tile=sprite(anim,{'PlayerPal':4,'BluePal':5,'RedPal':6,'OtherPal':7}[pal]);x=(i%8)*48;y=256+(i//8)*48
    atlas.alpha_composite(tile,(x+(48-tile.width)//2,y+(48-tile.height)//2))
for i,(anim,pal) in enumerate([('Anim_PI_RuppeeItem',4),('Anim_PI_KeyItem',6),('Anim_PI_BombItem',5)]):
    tile=sprite(anim,pal,20,30,'022230' if i==2 else '162730');atlas.alpha_composite(tile,(192+i*32+(24-tile.width)//2,48+(32-tile.height)//2))
path=APP/'hd/ui/hud-style.png';atlas.save(path)
print('HUD atlas:',path,'items:',len(items),'size:',atlas.size)
