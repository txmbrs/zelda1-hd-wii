from asset_config import REPO_ROOT, ASSET_DIR, PACK_DIR, REPORT_DIR
"""Compile supported original sprite conditions to tile patches and runtime predicates.
Unknown predicates are rejected, never guessed true. The report lists them.
"""
from pathlib import Path
from PIL import Image,ImageOps
import re,json,collections
ROOT=REPO_ROOT;APP=ASSET_DIR;OUT=REPORT_DIR
PACK=PACK_DIR
text=(PACK/'hires.txt').read_text(encoding='utf-8-sig');names=re.findall(r'^<img>(.*)',text,re.M)
conditions={m[1]:m[2].split(',') for m in re.finditer(r'^<condition>([^,]+),(.+)',text,re.M)}
def decode(h):
    b=bytes.fromhex(h);return [[((b[y]>>(7-x))&1)|(((b[y+8]>>(7-x))&1)<<1) for x in range(8)] for y in range(8)]
def encode(t):return bytes(sum(((t[y][x]>>b)&1)<<(7-x) for x in range(8)) for b in range(2) for y in range(8))
def transform(t,fx,fy):return [r[::-1] if fx else r[:] for r in (t[::-1] if fy else t)]
rejected=collections.Counter();rawrules=collections.defaultdict(list)
for line in text.splitlines():
    m=re.match(r'\[([^\]]+)\]<tile>(\d+),([0-9A-Fa-f]{32}),FF([0-9A-Fa-f]{6}),(\d+),(\d+),1,[NY]',line)
    if not m:continue
    terms=m[1].split('&');compiled=[];valid=True
    for term in terms:
        negative=term.startswith('!');name=term.lstrip('!');c=conditions.get(name)
        if name in ['hmirror','vmirror']:spec=(2 if name=='hmirror' else 3,0,0,bytes(16))
        elif c and c[0]=='frameRange':spec=(0,int(c[1]),int(c[2]),bytes(16))
        elif c and c[0]=='spriteNearby':spec=(1,int(c[1]),int(c[2]),bytes.fromhex(c[3]))
        elif c and c[0]=='memoryCheckConstant' and c[1].upper()=='10' and c[2]=='==':spec=(4,int(c[3],16),0,bytes(16))
        else:rejected[name]+=1;valid=False;break
        compiled.append((negative,spec,int(c[4][2:],16) if c and c[0]=='spriteNearby' else 0))
    if valid:rawrules[(m[3].upper(),m[4].upper())].append((names[int(m[2])],int(m[5]),int(m[6]),compiled))
sources=['owNpcs','uwNpcs','uwBoss1257','uwBoss3468','uwBoss9','playerItem']
entries=[];predicates=[];patches=[];patchids={};cache={};groups=[]
for sheetid,name in enumerate(sources):
    src=Image.open(APP/(name+'.png')).convert('RGB');pixels=src.load()
    for y in range(0,src.height,8):
        for x in range(0,src.width,8):
            if name=='playerItem' and y<48:continue
            t=[[min(3,pixels[x+xx,y+yy][0]>>4) for xx in range(8)] for yy in range(8)]
            if not any(map(any,t)):continue
            start=len(entries)
            for key in sorted({k[1] for k in rawrules}):
              seen=set()
              for fx,fy in [(False,False),(True,False),(False,True),(True,True)]:
                h=encode(transform(t,fx,fy)).hex().upper()
                for filename,sx,sy,conds in rawrules.get((h,key),[]):
                    identity=(filename,sx,sy,str(conds),fx,fy)
                    if identity in seen:continue
                    seen.add(identity)
                    if filename not in cache:
                        im=Image.open(PACK/filename).convert('RGBA');im.putdata([(r,g,b,0 if (r,g,b)==(255,0,255) else a) for r,g,b,a in im.getdata()]);cache[filename]=im
                    patch=cache[filename].crop((sx,sy,sx+16,sy+16))
                    if fx:patch=ImageOps.mirror(patch)
                    if fy:patch=ImageOps.flip(patch)
                    blob=patch.tobytes()
                    if blob not in patchids:patchids[blob]=len(patches);patches.append(patch)
                    pstart=len(predicates)
                    for neg,(kind,a,b,data),neighbor_palette in conds:
                        if kind==1:
                            a*= -1 if fx else 1;b*= -1 if fy else 1;data=encode(transform(decode(data.hex()),fx,fy))
                        predicates.append((kind,int(neg),a,b,data,neighbor_palette))
                    entries.append((int(key,16),patchids[blob],pstart,len(conds),int(fx)|(int(fy)<<1)))
            if len(entries)>start:groups.append((sheetid,x,y,start,len(entries)-start))
dest=APP/'hd/conditional';dest.mkdir(exist_ok=True)
for page in range((len(patches)+1023)//1024):
    atlas=Image.new('RGBA',(512,512))
    for local,patch in enumerate(patches[page*1024:(page+1)*1024]):atlas.paste(patch,((local%32)*16,(local//32)*16))
    atlas.save(dest/f'{page}.png')
def nums(values):return ','.join(map(str,values))
out=['#pragma once','struct HDPredicate { int kind,negative,a,b; unsigned palette; unsigned char hash[16]; };','static const HDPredicate hdPredicates[] = {']
out += ['{'+nums(p[:4])+','+str(p[5])+',{'+nums(p[4])+'}},' for p in predicates];out+=['};','struct HDConditionalRule { unsigned palette; int patch,first,count,mirror; };','static const HDConditionalRule hdConditionalRules[] = {']
out += ['{'+nums(e)+'},' for e in entries];out+=['};','struct HDConditionalGroup { int sheet,x,y,first,count; };','static const HDConditionalGroup hdConditionalGroups[] = {']
out += ['{'+nums(g)+'},' for g in groups];out+=['};',f'static const int HDConditionalPages = {(len(patches)+1023)//1024};']
(ROOT/'Loz/Game/Loz/HDGeneratedConditional.h').write_text('\n'.join(out)+'\n')
report={'rules':len(entries),'groups':len(groups),'patches':len(patches),'pages':(len(patches)+1023)//1024,'unsupported_predicates':dict(rejected)}
(OUT/'conditional-report.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='unsupported_predicates'})
