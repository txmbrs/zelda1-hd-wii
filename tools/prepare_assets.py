"""Prepare private SD assets from a user-supplied PRG0 ROM and HD pack.

No game data is downloaded. Run on Windows with .NET 9 SDK, Python and FFmpeg.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    inputs = p.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--rom', type=Path)
    inputs.add_argument('--extracted', type=Path, help='Existing data extracted by this port (top-level PNG/DAT/TAB files)')
    p.add_argument('--pack', type=Path, required=True, help='Folder containing hires.txt (V1.3)')
    p.add_argument('--out', type=Path, default=ROOT/'package/apps/zelda1hd')
    p.add_argument('--dol', type=Path, help='Optional downloaded or locally built boot.dol')
    a = p.parse_args()
    pack, app = a.pack.resolve(), a.out.resolve()
    if a.rom:
        raw = a.rom.read_bytes()
        if raw[:4] != b'NES\x1a' or hashlib.md5(raw[16:]).hexdigest() != 'd9a1631d5c32d35594b9484862ea26cba':
            p.error('Expected an unmodified North American PRG0 iNES ROM; see README.')
    if not (pack/'hires.txt').is_file():
        p.error('--pack must contain hires.txt from Zelda Remastered V1.3.')
    for executable in (('dotnet', 'ffmpeg') if a.rom else ('ffmpeg',)):
        if not shutil.which(executable):
            p.error(f'{executable} is required on PATH.')
    if app.exists() and any(app.iterdir()):
        p.error('Output folder must be empty. Choose a fresh --out folder to protect existing saves.')
    app.mkdir(parents=True, exist_ok=True)
    project = ROOT/'Loz/Tools/ExtractLoz/ExtractLoz_net9.csproj'
    if a.rom:
        run('dotnet', 'run', '--project', project, '-c', 'Release', '--', a.rom.resolve(), 'all', '-out', app)
    else:
        for source in a.extracted.iterdir():
            if source.is_file() and source.suffix.lower() in ('.png','.dat','.tab'):
                shutil.copyfile(source,app/source.name)
    for required in ('overworldRoomAttr.dat', 'playerItem.png', 'font.png', 'pal.dat', 'Effects.dat'):
        if not (app/required).is_file():
            raise RuntimeError(f'Extractor did not produce {required}')
    text = (pack/'hires.txt').read_text(encoding='utf-8-sig')

    def save(im, relative, transparent=True):
        im = im.convert('RGBA')
        if transparent:
            im.putdata([(r,g,b,0 if (r,g,b)==(255,0,255) else alpha) for r,g,b,alpha in im.getdata()])
        target = app/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        im.save(target)

    # hires.txt offsets are NES coordinates including the 64px HUD above the
    # playfield. The paintings are 2x; each room's playfield is 512x352px.
    for atlas_name, dest in [('overworld.png', 'ow'), ('overworld2.png', 'ow2')]:
        atlas = Image.open(pack/atlas_name).convert('RGBA')
        rooms = {}
        for line in text.splitlines():
            if '&ScrollNone' not in line or '<background>'+atlas_name+',' not in line:
                continue
            room = re.search(r'Overworld([0-9A-F]{2})(?:&|\])', line)
            background = re.search(r'<background>[^,]+,1,0,0,\d+,(\d+),(\d+)', line)
            if room and background:
                xy = tuple(map(int, background.groups()))
                if room[1] in rooms and rooms[room[1]] != xy:
                    raise RuntimeError(f'Ambiguous room rule: {room[1]}')
                rooms[room[1]] = xy
        if len(rooms) != (128 if dest=='ow' else 4):
            raise RuntimeError(f'Unexpected {atlas_name} rule count: {len(rooms)}')
        for room,(x,y) in rooms.items():
            save(atlas.crop((x*2,(y+64)*2,x*2+512,(y+64)*2+352)), f'hd/{dest}/{room}.png', False)

    copies = {}
    for prefix, names in {
        'link': [f'LinkCustomRunning{c}{s}.png' for c in ('Green','Blue','Red') for s in ('SmallShield','CrossShield')]+['LinkGreyDeath.png'],
        'title': [f'title{i}.png' for i in range(1,9)],
        'ui': [f'selectscreen{i}.png' for i in range(1,7)]+['GameOver.png','hud.png'],
        'cave': ['CaveFloor.png','CaveFloor2.png'],
        'dungeon': [f'StairwayBackground{i}{fade}.png' for i in (1,2) for fade in ('','Fade1','Fade2')],
        'boss': ['AquamentusBlue.png','AquamentusRed.png','Ganon.png'],
    }.items():
        copies.update({name:f'hd/{prefix}/{name}' for name in names})
    for name, relative in copies.items():
        save(Image.open(pack/name), relative)
    hud = Image.open(pack/'hud.png').convert('RGBA')
    hud.putdata([(r,g,b,0 if (r,g,b) in ((0,0,0),(255,0,255)) else alpha) for r,g,b,alpha in hud.getdata()])
    save(hud.crop((0,0,512,128)), 'hd/ui/hud-overlay.png')
    grey = Image.open(pack/'LinkGreyDeath.png').convert('RGBA')
    for index,right_x in enumerate((80,96)):
        sprite = Image.new('RGBA',(32,32))
        sprite.paste(grey.crop((64,0,80,32)),(0,0))
        right = grey.crop((right_x,0,right_x+16,32))
        sprite.paste(ImageOps.mirror(right) if index else right,(16,0))
        save(sprite,f'hd/link/death-{index}.png')

    audio = app/'audio'
    audio.mkdir(exist_ok=True)
    source = (ROOT/'Loz/Game/Loz/Sound_wii.cpp').read_text()
    for name in sorted(set(re.findall(r'"([^"/]+\.(?:ogg|wav))"',source))):
        if name.endswith('.ogg'):
            shutil.copyfile(pack/name, audio/name)
        else:
            run('ffmpeg','-hide_banner','-loglevel','error','-y','-i',pack/(Path(name).stem+'.ogg'),'-acodec','pcm_s16le','-ar','22050',audio/name)

    env = dict(os.environ, ZELDA_HD_PACK=str(pack), ZELDA_ASSET_DIR=str(app))
    for script in ('build_hd_resources.py','build_water_layers.py','build_dungeon_water.py',
                   'build_wand_effect.py','build_styled_hud.py','build_conditional_sprites.py'):
        run(sys.executable,ROOT/'tools'/script,env=env)
    for location in (pack/'ZeldaRemasteredReadme.txt',pack.parent/'ZeldaRemasteredReadme.txt'):
        if location.is_file():
            shutil.copyfile(location,app/'HD-PACK-LICENSE.txt')
            break
    else:
        raise RuntimeError('Keep ZeldaRemasteredReadme.txt beside the HD pack; it contains its credits and license.')
    shutil.copyfile(ROOT/'hbc/meta.xml',app/'meta.xml')
    if a.dol:
        shutil.copyfile(a.dol,app/'boot.dol')
    manifest={str(f.relative_to(app)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(app.rglob('*')) if f.is_file()}
    (ROOT/'build/assets-manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'Ready: {app}\nCopy this folder to SD:/apps/zelda1hd; add boot.dol if not supplied.')


if __name__ == '__main__':
    main()
