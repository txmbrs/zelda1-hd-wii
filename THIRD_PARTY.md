# Third-party components

The engine source originates from Loz (Apache 2.0), with LICENSE and NOTICE preserved at the repository root. The Wii build links third-party libraries under their own licenses. Bundled notice texts are in `licenses/` and are also attached to the alpha release as one text file.

| Component | Upstream / license |
| --- | --- |
| libogc (including console Wii input support) | https://github.com/devkitPro/libogc — zlib-style and BSD/OAR notices |
| libfat | https://github.com/devkitPro/libfat — included upstream notice |
| SDL2, SDL2_image, SDL2_mixer | https://libsdl.org — zlib-style |
| libmodplug | https://github.com/Konstanty/libmodplug — public domain |
| libogg, libvorbis, opus, opusfile | https://xiph.org — BSD-style |
| libpng | http://www.libpng.org/pub/png/libpng.html — libpng license |
| libjpeg-turbo | https://libjpeg-turbo.org — IJG/BSD/zlib terms |
| zlib | https://zlib.net — zlib license |

The devkitPPC compiler and development tools have separate licenses; using the toolchain does not make those licenses the license of this engine. The pinned Docker environment and build command are documented in README.md.

This software is based in part on the work of the Independent JPEG Group.

Zelda Remastered and Nintendo assets have independent terms; the Apache license does not cover them. Supply the original pack and ROM locally. Keep the pack's full readme with your private prepared assets.
