# Changelog

## 0.1.0-alpha

First public source snapshot of this Wii port, based on the current private development build.

- Native Wii entry point, SDL2 rendering/audio, controller input and SD saves.
- Local importer for user-supplied PRG0 data and Zelda Remastered V1.3 assets.
- HD room paintings, selected conditional sprite rules, water animations and a corner HUD/inventory layout.
- DOL section alignment during the build.
- Cave rendering optimization: skip the legacy tile layer covered by the HD painting; cache missing-resource lookups for the app session.

Known limitations and verification scope are listed in README.md. This release does not claim complete original-pack compatibility or confirmed resolution of physical Wii cave stutter.
