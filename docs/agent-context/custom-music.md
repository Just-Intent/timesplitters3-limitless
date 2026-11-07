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
| MP3 decoder adapter `pc_music_codec_*` | `src/interface/pc_music_codec.c` | own TU (title CMake, `patches/media/2160-custom-music-codec.patch`, pinned last in `apply-patches.py`); FFmpeg headers only where `XBOXRECOMP_FFMPEG_DIR` has them, else a stub |
| Launcher import/list/remove | `tools/launcher/MusicLibrary.cs`, Saves page section in `SavesPage.cs` | pickers + recycle adapters: `Launcher.cs` (WPF dialog, `SHFileOperationW` Recycle Bin), `macos/NativeWindow.cs` (Avalonia picker, `~/.Trash`) |
| Output slot 273 `RECOMP_AUDIO_SLOT_MUSIC` | `patches/media/0059-custom-music-voice.patch` (XAudio2 voice count 273 -> 274), `src/native/audio_sdl.c` (`SLOT_COUNT`) | runtime header gains the define |
| Folder routing | `TSFP_MUSIC_DIR`: `tools/launcher/GameSettings.cs` (`LauncherPlatform.Music`: `Music`, Mac checkout `private/music`), `tools/play-native-mac.py`; game default `private/music` | `Music` is a protected install-root name (`Core.cs`) |

New wrappers (area `music`; the integrator adds them to the generated
wrappers patch, not edited here):

| WRAP | handler | why |
|---|---|---|
| `sub_000289C0` | `pc_custom_music_channel` | jingle channel 1/2: end-of-match (sub_00045A00 returns `0x0006709B`/`0x000670B4`, at `[esp+0x10]`) ends the track; any other holds it until the stock player (`[0x0064BDC0]` != 2, after >= 60 steps, at most 2000) |
| `sub_001CDD60` | `pc_custom_music_restart` | **new (2026-11-07)**: a level restart calls no `sub_00045DE0`; the level's track (`cmu.level`) starts again |
| `sub_00071BE0` | `pc_custom_music_arcade_rows` | Arcade rows after the 58 stock rows (return `0x002C8047`, `ebx` 0) |
| `sub_002C71C0` | `pc_custom_music_arcade_show` | item 0xE shows the imported row |
| `sub_002C7430` | `pc_custom_music_arcade_store` | imported row -> `pc-music.ini`, stock fields row 0 |

Existing wrappers reused: `sub_00078590` (rows), `sub_003006C0` (compile),
`sub_00301A90` (load), `sub_00045DE0` (music) via `pc_mapmaker_skymusic_*`.

## Contracts

- Formats: RIFF/WAVE PCM 8/16/24/32 int, 32/64 float, extensible, 1..8 ch
  (front pair), 8..192 kHz -> s16 mono/stereo, read in the header. `.mp3`
  (`PC_MUSIC_KIND_CODEC`) goes to `pc_music_codec_open(FILE*)` (owns the
  stream), listed only while `pc_music_codec_available()`. The adapter
  dlopens avutil/swresample/avcodec like `xmv_decode.c` (beside the exe,
  `RECOMP_FFMPEG_DIR` = launcher `Bin`, `PC_MUSIC_FFMPEG_BIN`); no
  libavformat: skips ID3v2 (+footer), `av_parser_parse2` (MP3) through a
  16 KiB buffer, first frame within 1 MiB, rate change = error, any sample
  format -> s16 front pair. Loop = `pc_music_codec_rewind`; a track that
  yields nothing after a rewind ends. No new packaged DLL. List entries keep their index for
  the process; editor values `0x100 + index`, Arcade rows `58 + offer`.
- Map: chunk `0x00C776F7` size 0x44 {word 1, byte length, 0, name[64]} before
  the terminator; stock fallback -1 stays; **not** over-stock (stock builds
  skip it). Missing file: tileset music.
- Play: `sub_00045DE0` returns `0x00045FBC` (map) or `0x00045F4B` (Arcade,
  guest row 0); eax -> 0 (channel 0 silent), jingles stock. Map eax is
  `sub_00045E60`: info word +0x0E, or a setup's stock Arcade pick
  (`[0x00761404]`, mode byte != 10, jukebox[profile +0x114] != -1). Since
  2026-11-07 the map's chunk plays when limits, scene 100 and the map's own
  word -1 hold, even over a stock setup pick (logged); an imported Arcade
  choice comes first and a missing one falls through to the map's track.
- Watch: a scene word off the play scene holds the track; 100 steps in a
  row (~1.5 s) end it (m48.err 18020 ended mid-fight on a blip). Logs
  `watch: ... track kept|ended`, `jingle over`, `new level music`. Level
  `PC_MUSIC_LEVEL` 1.0: full slider = 0 dB (0.65 was quiet in .48).
- Logs: budget per message (8 each, 32 kinds); the Map Settings offer line
  only when its count changes. Before this one 48-line budget was spent by
  list builds (m45b.err: 48 offer lines, then nothing), hiding every play
  line. Each silent play return logs `[music] play: <why> (ret, scene, eax,
  map chunk, map music, arcade)`; a failed start logs `pc_music_error()`. Watch: scene word, slider `[0x005236BC]/16`, pause `[0x006D5EB8]` (set by sub_00045AB0, cleared by sub_00045AF0). Not the
  player's +0x28 `[0x0064BDC0]`: a state (0 idle, 1 pause-before-start, 2
  playing, 3 stopped), 3 after the menu music, which held every track silent
  through .46 (m46.err: `map: imported track`, never `slot=273`). The player
  logs its first submission, `held ... paused` and `stopped: <why>`.
- Lead 250 ms in 4096-frame chunks; a refused submission is offered again.
- Loudness (2026-11-07, .50 "still quiet"): stock level music reaches
  XAudio2 at `2000 log10(pct/100) - 600`, pct = `(int)(slider/16 x 100 x
  level/100)` (sub_00045B60; levels `0x00535858` 50..80, `0x005358B8` 80;
  halved in scene 0x65) -> player +0x54 -> sub_00028180 `2000 log10` ->
  `CDirectSoundStream_SetVolume`; the stream adds mix bins (0@0 1@0: 0) and
  minus headroom 600 (`hle_dsound_stream_adjust`). One stereo voice, identity
  matrix, same 0.5 master as slot 273; no summing. Logged: m48/m50 level
  stock -1135 (slider 11, pct 54), menu -1737; slot 273 -325. Same curve, so
  imported = stock + 8.0..12.0 dB at every slider. Content (ffmpeg ebur128):
  60 s of m48's stock packets decoded from `musicts.pak` -16.3 LUFS, the
  user's MP3 -9.9 LUFS: imported ~14.5 LU louder at the output. Not matched
  (that cuts ~8 dB). Trim `gain_db=` in `pc-music.ini` (-12..+12, any line
  after the header, kept by `pc_music_arcade_save`): voice = slider + trim
  up to 0 dB (`gainOf` clamps >0), the rest is `pc_music_boost` on the
  samples, soft-clipped above 0.75 (tanh knee). Read at each start; one
  `[music] player: level ... stock level music at this slider: a..b` line.
- Slider ticks past 16 (2026-11-07, "increase the number of ticks ... so I
  can turn it up louder"): `src/interface/pc_menu_sliders.inc`. The Music bar
  shows 24 positions; the word `[0x005236BC]` and profile +0x120 stay 0..16
  (load sub_002C3300 0x2C3341, accept/back 0x2C39B9/0x2C3AC6, new profile 11
  at 0x2D259D: only stock code and our clamped writes touch it), so stock
  builds read every profile unchanged. Ticks 17..24 = `slider_extra=` 0..8 in
  `pc-music.ini` (`pc_music_slider_extra[_set]`, live value
  `pc_music_extra_live`, read on the game thread; every rewrite keeps
  `arcade=`/`gain_db=`), counted only when the word is 16 (gain 1): +150 per
  tick added to `gain_db=`, the sum capped at +1200, then the same voice/boost
  split. Stock music is not raised: its pct (sub_00045B60, <= 80 at 16) is
  passed through the guest player and the XDK `SetVolume` original
  (HLE_CALL_ORIGINAL), whose handling of pct > 100 / positive mB is unproven;
  host `gainOf`/SDL clamp at 0. Paths: descriptor 0x0052377C {build
  0x2C3E20, tick 0x2C3720}, pushed by Settings 0x2CBE1A and pause 0x2EC215.
  Tick: `pc_display_menu_lookup` 0x2C3720 -> `audio_options_tick` ->
  `pc_display_menu_audio_tick` (not a WRAP). Before the original it reads
  menu+0xC4 (0x40000 Left first, 0x80000 Right) and, via sub_00074720 /
  sub_00071A60, whether the focused item is Music (id 1, +0x28 bit 0 clear);
  after it, at a word of 16: Right adds a tick + click; Left with ticks takes
  one and puts back the 16 the original lowered to 15 (sub_00045C60 again);
  15 -> 16 clears stale ticks. Bar: `pc_display_menu_audio_draw` notes the
  row id; `pc_display_menu_slider_bar` (sub_000814B0) from return
  0x2C35F9/0x2C3709 gets `[esp+4]` = position/24, same rectangle at every
  aspect. Mouse: `menu_music_set` over 24. Log on change: `[music] player:
  Music slider 16 + N ticks`.

## Tests

`tests/media/test_custom_music.py` (host, 51 mutations incl. trim/boost/soft clip/slider ticks, WAVs and fake
`FAKE` .mp3 decoder input written by the test), `tests/media/test_music_codec.py`
(stub build always; real FFmpeg build where the pinned package is unpacked,
MP3s made by its `ffmpeg.exe` with libmp3lame, 9 mutations; skipped in CI and
on non-Windows), launcher `tests/launcher/launcher_music.cs` + a Saves page
case in `launcher_shared_ui.cs` and `tests/mapmaker/test_mapmaker_custommusic.py` (glue through the
skymusic hooks, 44 mutations incl. log budget/decision lines checked on stderr, XBE facts when the private XBE exists);
`tests/mapmaker/mapmaker_skymusic.c` gained output stubs. Slider ticks:
`tests/input/menu_mouse.c` `music_slider_tests` (bar fraction at 25
positions/both layouts, keys, focus/disabled/inactive, `pc-music.ini` text;
`TSFP_DISPLAY_CONFIG` required) with 7 mutations in `test_menu_mouse.py`,
and `test_stock_music_slider_code` (XBE bytes). Both are in
`runtime-patches.yml`. WinError 4551 on a fixture is the host's application
control: rerun.

## Unverified

Any game run (MP3 decode was only exercised by the fixture); the Arcade menu's own use of item 0xE values past 57 (only the
two known readers were checked); whether the stock player keeps channel 0
state consistent when id 0 plays; Mac build of the thread/`dirent` paths;
Mac launcher build (Avalonia picker, Trash move); Mac MP3 needs an FFmpeg
adapter (MAC-133); whether the user hears the trim/boost as intended (the
"still quiet" report contradicts the measured levels: system/stock-volume
perception is unexplained); the slider ticks past 16 in a game (draw,
keys, mouse, pause menu, controller repeat), and whether the sub_00071A60
item matches the original's focus in every case. FFmpeg log lines (`Header missing`) may reach stderr on junk.
