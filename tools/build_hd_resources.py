from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
from pathlib import Path
from PIL import Image,ImageOps
import struct,re,json,hashlib

ROOT=REPO_ROOT
APP=ASSET_DIR
PACK=PACK_DIR
OUT=REPORT_DIR
OUT.mkdir(exist_ok=True)
text=(PACK/'hires.txt').read_text(encoding='utf-8-sig')
names=re.findall(r'^<img>(.*)',text,re.M)
rules={};levelrules={}
for line in text.splitlines():
    m=re.search(r'<tile>(\d+),([0-9A-Fa-f]{32}),([0-9A-Fa-f]{8}),(\d+),(\d+),1,[NY]',line)
    if not m:continue
    condition=line.split('<tile>')[0]
    context=re.fullmatch(r'\[Dungeon([1-9])Check\]',condition)
    if condition and condition!='[!OverworldCheckForSword&!StairCheckForSword]' and not context:continue
    key=(m[2].upper(),m[3][2:].upper(),m[3][:2].upper()=='FF')
    target=levelrules.setdefault(int(context[1]),{}) if context else rules
    target.setdefault(key,(names[int(m[1])],int(m[4]),int(m[5]),line))

keys=sorted({b[1:4].hex().upper() for f in [APP/'overworldInfo.dat',*APP.glob('levelInfo_*.dat')] for a in [f.read_bytes()[:32]] for b in [a[i:i+4] for i in range(0,32,4)]})
palraw=(APP/'pal.dat').read_bytes()
system=[]
for i in range(64):
    c=struct.unpack_from('<I',palraw,i*4)[0];system.append(((c>>16)&255,(c>>8)&255,c&255,255))
images={}
def image(name):
    if name not in images:
        im=Image.open(PACK/name).convert('RGBA')
        im.putdata([(r,g,b,0 if (r,g,b)==(255,0,255) else a) for r,g,b,a in im.getdata()])
        images[name]=im
    return images[name]
def pattern(tile):
    return bytes(sum(((tile[y][x]>>b)&1)<<(7-x) for x in range(8)) for b in range(2) for y in range(8)).hex().upper()
def rotate(t):return [list(row) for row in zip(*t[::-1])]

sources=['owNpcs','uwNpcs','uwBoss1257','uwBoss3468','uwBoss9','playerItem','overworldTiles','underworldTiles','underworldCellarTiles','underworldWalls','underworldDoors']
report={};available=[]
for name in sources:
    sprite=name in sources[:6]
    src=Image.open(APP/(name+'.png')).convert('RGB')
    tiles=[]
    for y in range(0,src.height,8):
        for x in range(0,src.width,8):
            if name=='playerItem' and y<48:continue
            t=[[min(3,src.getpixel((x+xx,y+yy))[0]>>4) for xx in range(8)] for yy in range(8)]
            if not any(map(any,t)):continue
            options=[];rotated=t
            for angle in range(4):
                for fx,fy,tt in [(False,False,rotated),(True,False,[row[::-1] for row in rotated]),(False,True,rotated[::-1]),(True,True,[row[::-1] for row in rotated[::-1]])]:
                    options.append((pattern(tt),angle,fx,fy))
                rotated=rotate(rotated)
            tiles.append((x,y,options))
    contexts=[(-1,rules)]+[(level,{**rules,**r}) for level,r in levelrules.items() if name.startswith('underworld')]
    for level,context_rules in contexts:
      for key in keys:
        opaque=name.endswith('Tiles')
        colors=[(0,0,0,255 if opaque else 0)]+[system[int(key[i:i+2],16)] for i in range(0,6,2)]
        base=Image.new('RGBA',src.size)
        base.putdata([colors[min(3,r>>4)] for r,g,b in src.getdata()])
        sheet=base.resize((src.width*2,src.height*2),Image.Resampling.NEAREST)
        matched=[]
        for x,y,options in tiles:
            for pat,angle,fx,fy in options:
                rule=context_rules.get((pat,key,sprite))
                if rule is None:continue
                file,sx,sy,line=rule
                # A solid NES water tile also appears in the generic CHR sheets.
                # Its contextual dungeon animation is drawn by the renderer.
                if name=='underworldTiles' and file.startswith('Chr_') and pat=='FFFFFFFFFFFFFFFF0000000000000000':continue
                patch=image(file).crop((sx,sy,sx+16,sy+16))
                if fy:patch=ImageOps.flip(patch)
                if fx:patch=ImageOps.mirror(patch)
                if angle:patch=patch.rotate(90*angle)
                if opaque:
                    patch=Image.alpha_composite(sheet.crop((x*2,y*2,x*2+16,y*2+16)),patch)
                sheet.paste(patch,(x*2,y*2))
                matched.append({'xy':[x,y],'rule':line,'rotation':angle*90,'flip':[fx,fy]})
                break
        if not matched:continue
        folder=name if level<0 else name+f'_d{level}'
        dest=APP/'hd/tiles'/folder/(key+'.png');dest.parent.mkdir(parents=True,exist_ok=True)
        if level>=0 and not any(t['rule'].startswith(f'[Dungeon{level}Check]') for t in matched):
            if dest.exists():dest.unlink()
            continue
        sheet.save(dest)
        available.append((name,int(key,16),level,folder))
        report[folder+'/'+key]={'mapped':len(matched),'total_nonblank':len(tiles),'tiles':matched}
    print(name,{k.split('/')[1]:v['mapped'] for k,v in report.items() if k.startswith(name+'/')},flush=True)

(OUT/'hd-tile-rules.json').write_text(json.dumps(report,indent=2))
header=['// Generated from hires.txt by changes-20261024/build_hd_resources.py.', '#pragma once','struct HDSheetKey { const char* name; unsigned palette; int level; const char* folder; };','static const HDSheetKey hdSheetKeys[] = {']
header += [f'    {{ "{n}", 0x{k:06X}, {level}, "{folder}" }},' for n,k,level,folder in available]
header+=['};']
(ROOT/'Loz/Game/Loz/HDGeneratedSheets.h').write_text('\n'.join(header)+'\n')
print('generated sheets',len(available))
