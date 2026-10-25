from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
from pathlib import Path
from PIL import Image
import re
ROOT=REPO_ROOT;PACK=PACK_DIR;APP=ASSET_DIR
text=(PACK/'hires.txt').read_text(encoding='utf-8-sig');names=re.findall(r'^<img>(.*)',text,re.M)
groups={}
for m in re.finditer(r'\[([Dd]ungeon\dwaterframe)([0-3])(?:&Dungeon\dCheck)?\]<tile>(\d+),FFFFFFFFFFFFFFFF0000000000000000,0F([A-Fa-f0-9]{6}),(\d+),(\d+),1,N',text):
    level=int(re.search(r'\d',m[1])[0]);frame=int(m[2]);groups.setdefault((level,m[4].upper(),m[1]),{})[frame]=(names[int(m[3])],int(m[5]),int(m[6]))
out=['#pragma once','struct HDDungeonWater { unsigned palette; int level; int period; int begins[4]; const char* path; const char* edges[7]; };','static const HDDungeonWater hdDungeonWaterSpecs[] = {']
for (level,key,condition),frames in groups.items():
    assert len(frames)==4
    sheet=Image.new('RGBA',(128,64));begins=[];period=0
    for f in range(4):
        filename,x,y=frames[f];tile=Image.open(PACK/filename).convert('RGBA').crop((x,y,x+16,y+16))
        for xx in range(2):
            for yy in range(2):sheet.paste(tile,(f*32+xx*16,yy*16))
        edge=Image.open(PACK/filename).convert('RGBA').crop((x+64,y,x+80,y+16))
        for xx in range(2):sheet.paste(edge,(f*32+xx*16,32))
        m=re.search(rf'<condition>{condition}{f},frameRange,(\d+),(\d+)',text);assert m,condition
        period=max(period,int(m[1]));begins.append(int(m[2]))
    path=f'hd/anim/dungeon-water-{level}-{key}.png';sheet.save(APP/path)
    old=APP/f'hd/anim/dungeon-water-{key}.png'
    if old.exists():old.unlink()
    edges=[]
    for i in range(1,8):
        m=re.search(rf'<condition>{condition.replace("frame","edge")}{i},tileNearby,0,-8,([A-Fa-f0-9]{{32}}),',text)
        edges.append('"'+(m[1] if m else '')+'"')
    out.append('{0x'+key+','+str(level)+','+str(period)+',{'+','.join(map(str,begins))+'},"'+path+'",{'+','.join(edges)+'}},')
out.append('};');(ROOT/'Loz/Game/Loz/HDGeneratedDungeonWater.h').write_text('\n'.join(out)+'\n')
print('Dungeon water palettes:',len(groups))
