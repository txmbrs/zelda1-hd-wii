# Zelda Remastered for Wii — unofficial alpha

An unofficial Wii adaptation of **[Zelda Remastered V1.3](https://www.romhacking.net/forum/index.php?topic=31559.0)**, the HD pack originally made for Mesen. Zelda Remastered supplies the replacement graphics, animation rules, music and sound used by this project; users prepare those assets locally from the original pack.

For native Wii execution, the current implementation uses the C++ engine and extraction tools from [Loz by Aldo J. Nunez](https://github.com/aldonunez/Loz), with a Wii SDL2/GX backend. Loz is the engine foundation, not the HD remaster. This is not a port of Mesen or a complete implementation of the remaster's IPS patches and rules.

**This is an experimental alpha, not a completed or fully faithful replacement for the original Mesen HD pack.** It is not affiliated with or endorsed by Nintendo. This repository does not include a ROM, extracted game data, HD artwork or audio.

## Installation

You need:

- A Wii with an existing homebrew launcher, an SD card and a Wii Remote (or supported controller).
- Your own unmodified North American PRG0 iNES ROM. The extractor validates the ROM payload MD5: `D9A1631D5C32D35594B9484862EA26CBA` (excluding the 16-byte header). The original pack documents full-file SHA-1 `DAB79C84934F9AA5DB4E7DAD390E5D0C12443FA2`.
- The original **Zelda Remastered V1.3** pack, obtained separately. Keep its `ZeldaRemasteredReadme.txt` beside the unpacked `ZeldaRemastered` folder. [Original project discussion](https://www.romhacking.net/forum/index.php?topic=31559.0).
- A Windows PC with Python 3.10+, .NET 9 SDK and FFmpeg on PATH. Asset extraction uses Windows System.Drawing; WSL is used for building the Wii executable, not for the extractor.

1. Download this repository's source and `boot.dol` from [Releases](https://github.com/txmbrs/zelda1-hd-wii/releases).
2. Open PowerShell in the source folder:

```powershell
py -m pip install -r requirements.txt
py tools/prepare_assets.py --rom "C:\Games\zelda-prg0.nes" --pack "C:\Games\ZeldaRemastered" --dol "C:\Downloads\boot.dol"
```

3. Copy the generated `package/apps/zelda1hd` folder to `SD:/apps/zelda1hd`.
4. Start it with your homebrew launcher.

The preparer requires an empty output folder so it does not overwrite saves. To choose another destination, add `--out "C:\Games\zelda-sd\apps\zelda1hd"`. Existing port users can reuse their locally extracted top-level data with `--extracted "C:\Games\old-zelda1hd"` instead of `--rom`. Back up saves before updating an installation.

Expected SD layout:

```text
apps/zelda1hd/
  boot.dol
  meta.xml
  *.dat, *.tab, *.png     # locally extracted data
  hd/                    # locally prepared HD assets
  audio/                 # locally prepared music/effects
  HD-PACK-LICENSE.txt
  saves/                 # created by the game
```

`boot.dol` alone is not a complete game. Do not share your generated `package/` directory.

## Controls

| Action | Wii Remote held sideways |
| --- | --- |
| Move / menu selection | D-pad |
| Sword / confirm | 2 or A |
| Selected item | 1 or B |
| Start / inventory | + |
| Select | − |
| Exit | HOME |

Nunchuk movement, Classic Controller and GameCube controller input are also implemented. GameCube Z exits the app. Menus support an IR pointer where implemented.

## Build

First run the asset preparer: it also generates the private HD lookup headers needed by the compiler. Then build with devkitPPC, libogc and Wii SDL2, SDL2_image, SDL2_mixer portlibs.

The known build environment is Docker under WSL Ubuntu. From the repository root in WSL:

```bash
docker run --rm -v "$PWD:/work" -w /work/Loz/Game/Loz \
  devkitpro/devkitppc@sha256:44cb1a920e1ec3ec7c06767493c3b85f8d643d6137cc4661f0201895ac6e4967 \
  bash -lc 'make -j4'
```

The result is `Loz/Game/Loz/boot.dol`. The Makefile runs `elf2dol` and the DOL section alignment helper automatically. Copy the result into your prepared app folder. Do not source a Windows host's shell profile inside WSL; run `bash --noprofile --norc` if needed.

## Status and limitations

- Native game logic is based on Loz. The HD pack was made for Mesen; this port does not implement the complete Mesen condition language or emulate NES memory/PPU/OAM.
- The pack's `ZeldaHD.ips` is **not applied** to the ROM. Its code patches and Redux features are not automatically present in this native engine. Supply the unpatched PRG0 ROM to the extractor.
- Supported assets include overworld room paintings, animated water, caves/cellars, selected dungeon graphics, sprites, title screens and audio. Unsupported conditions are listed in local `build/asset-reports/conditional-report.json`; they are rejected rather than treated as true.
- Current Wii cave stutter is under investigation. This alpha includes removal of the hidden legacy cave tile pass and repeated SD lookups for missing HD textures. The effect on physical Wii performance still needs confirmation.
- Full-game behavior, every dungeon and the second quest have not been comprehensively verified on physical hardware. Dolphin SD configuration can behave differently from a real Wii.
- Display output uses a 640×480 framebuffer and expanded VI scanout. It is not native 720p HD; “HD” refers to replacement assets.

Please report the room/location, quest, build version, controller and Wii/Dolphin details when filing an issue. A short video and relevant lines from `zelda-wii-debug.log` help diagnose rendering/performance problems. Do not upload ROMs or saves containing personal information.

## Credits and licensing

See [CREDITS.md](CREDITS.md) and [THIRD_PARTY.md](THIRD_PARTY.md). Code under the upstream Loz license is Apache 2.0; the original notices are preserved. This does not license Nintendo content or the HD pack's artwork/music. The pack has separate terms and credits, retained with local generated assets. Distribution without those assets does not imply Nintendo approval or a guarantee against rights-holder claims.

## Po polsku

Pobierz kod i `boot.dol` z Releases. Uruchom powyższe polecenia na Windowsie, podając własny ROM PRG0 i oryginalny pack V1.3. Skopiuj wygenerowany folder na `SD:/apps/zelda1hd`. Repozytorium nie zawiera ROM-u, grafik ani muzyki. To wersja alfa; problem płynności w jaskiniach wymaga jeszcze potwierdzenia na Wii.
