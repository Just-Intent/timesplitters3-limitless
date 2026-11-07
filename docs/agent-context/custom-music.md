# Imported music (agent guide)

User request (2026-11-03): "Ability to import your own music and make
available for mapmaker and arcade." Player documentation:
[mapmaker-custom-music.md](../mapmaker/mapmaker-custom-music.md). Nothing here
has been built or played; fixtures only.

## Entry points

| Piece | File | Notes |
|---|---|---|
| Host folder, WAV reader, player thread, `pc-music.ini` | `src/interface/pc_custom_music.h` | header-only; `PC_CUSTOM_MUSIC_IMPLEMENTATION` in `pc_mapmaker_custommusic.inc` (one TU: `pc_display_menu.c`) |
| MapMaker/Arcade glue (`cmu_*`, 4 wrappers) | `src/interface/pc_mapmaker_custommusic.inc` | included by `pc_mapmaker_skymusic.inc`; rows/compile/load/music hooks there call it |
| Output slot 273 `RECOMP_AUDIO_SLOT_MUSIC` | `patches/media/0059-custom-music-voice.patch` (XAudio2 voice count 273 -> 274), `src/native/audio_sdl.c` (`SLOT_COUNT`) | runtime header gains the define |
| Folder routing | `TSFP_MUSIC_DIR`: `tools/launcher/GameSettings.cs` (`LauncherPlatform.Music`: `Music`, Mac checkout `private/music`), `tools/play-native-mac.py`; game default `private/music` | `Music` is a protected install-root name (`Core.cs`) |

New wrappers (area `music`; the integrator adds them to the generated
wrappers patch, not edited here):

| WRAP | handler | why |
|---|---|---|
| `sub_000289C0` | `pc_custom_music_channel` | a jingle channel (1/2) ends the imported track |
| `sub_00071BE0` | `pc_custom_music_arcade_rows` | Arcade rows after the 58 stock rows (return `0x002C8047`, `ebx` 0) |
| `sub_002C71C0` | `pc_custom_music_arcade_show` | item 0xE shows the imported row |
| `sub_002C7430` | `pc_custom_music_arcade_store` | imported row -> `pc-music.ini`, stock fields row 0 |

Existing wrappers reused: `sub_00078590` (rows), `sub_003006C0` (compile),
`sub_00301A90` (load), `sub_00045DE0` (music) via `pc_mapmaker_skymusic_*`.

## Contracts

- Formats: RIFF/WAVE PCM 8/16/24/32 int, 32/64 float, extensible, 1..8 ch
  (front pair), 8..192 kHz -> s16 mono/stereo. No codec dependency (FFmpeg
  is Windows-only and video-only here). List entries keep their index for
  the process; editor values `0x100 + index`, Arcade rows `58 + offer`.
- Map: chunk `0x00C776F7` size 0x44 {word 1, byte length, 0, name[64]} before
  the terminator; stock fallback -1 stays; **not** over-stock (stock builds
  skip it). Missing file: tileset music.
- Play: `sub_00045DE0` returns `0x00045FBC` (map, `eax == -1`, limits) or
  `0x00045F4B` (Arcade, guest row 0); eax -> 0 (channel 0 silent), jingles
  stock. Watch: scene word, slider `[0x005236BC]/16 x 0.65`, pause `[0x0064BDC0]`.
- Lead 250 ms in 4096-frame chunks; a refused submission is offered again.

## Tests

`tests/media/test_custom_music.py` (host, 26 mutations, WAVs written by the
test) and `tests/mapmaker/test_mapmaker_custommusic.py` (glue through the
skymusic hooks, 34 mutations, XBE facts when the private XBE exists);
`tests/mapmaker/mapmaker_skymusic.c` gained output stubs. Both are in
`runtime-patches.yml`. WinError 4551 on a fixture is the host's application
control: rerun.

## Unverified

Any game run; the Arcade menu's own use of item 0xE values past 57 (only the
two known readers were checked); whether the stock player keeps channel 0
state consistent when id 0 plays; Mac build of the thread/`dirent` paths;
launcher C# build. Launcher import button: pending.
