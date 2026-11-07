# Your own music in MapMaker and Arcade

User request (2026-11-03): "Ability to import your own music and make
available for mapmaker and arcade."

**Status:** implemented with fixture tests only. It has not been built or
played on either platform. Agent notes:
[custom-music.md](../agent-context/custom-music.md).

## Using it

1. Put WAV files in the music folder:
   - installed game: `Music` beside `Saves` and `Settings` (the launcher sets
     `TSFP_MUSIC_DIR`);
   - source checkout: `private/music` (ignored by Git; also the game's
     default when `TSFP_MUSIC_DIR` is not set).
2. **MapMaker** (needs `TSFP_MAPMAKER_LIMITS=1`, like the other extra rows):
   Map Settings > Music lists `Imported: <name>` after the extra stock
   tracks. The map saves the file name; reloading it selects the same row.
3. **Arcade**: the setup's Music choice lists `Imported: <name>` after the
   58 stock rows. The choice is remembered in `pc-music.ini` beside the
   display settings.

Up to 64 tracks are listed (file names up to 63 bytes, ending in `.wav`).
Names show without `.wav`, with `_` as spaces; non-ASCII letters show as `?`.

## Formats

WAV only: PCM 8, 16, 24 or 32-bit, or 32/64-bit float (WAVE_FORMAT_EXTENSIBLE
too), 8,000 to 192,000 Hz. Mono and stereo play as they are; surround files
play their front left/right pair. The file is streamed from disk and loops.

Why WAV: it needs no decoder, so the same shared code reads it on Windows
and macOS. The runtime's FFmpeg is loaded only for movies on Windows and has
no container reader; MP3/OGG/FLAC would need a new decoder on both hosts.
Convert other files first, for example with the FFmpeg that ships in the
game's `Bin` folder on Windows:

```
Bin\ffmpeg.exe -i song.mp3 -ar 44100 -ac 2 Music\song.wav
```

(If your build has no `ffmpeg.exe`, any audio editor that exports WAV works.)

## What to expect

- The track replaces the level music only. The end-of-match jingles (Mission
  Complete, Game Over) are the game's own; when one starts, your track stops.
- It follows the Music volume slider (at about the stock tracks' level),
  pauses with the game's music and stops when you leave the level.
- If the music is switched off in the options, nothing plays.
- **Fallbacks:** a map whose file is missing plays the tileset's music (the
  log says `[music] map's imported track is missing`). An Arcade choice whose
  file is missing plays the level's music. A stock or older build loads such
  a map with the tileset's music; the map is not moved to the larger save
  tier.
- **System Link:** your track plays only on your machine. Other players hear
  the level's music (the game sends "level music" for an imported choice).
- The Arcade choice applies only while the game's own choice is "level
  music" (row 0), which is what an imported choice stores.

## Save format

A map with an imported track carries one extra chunk before the terminator:
tag `0x00C776F7`, size `0x44`: `+0` word version 1, `+2` byte name length,
`+3` 0, `+4` the file name (64 bytes, zero-padded). The map-info music word
stays -1 (tileset). Every stock chunk walker skips an unknown tag by its size
([mapmaker-sets.md](mapmaker-sets.md#save-format)).

## Importing from the launcher

Pending. Planned: a Music page action that copies chosen `.wav` files into
the music folder (and, on Windows, converts other formats with the bundled
FFmpeg), never touching saves or settings. Until then copy files by hand.

## Playtest

1. Copy two WAV files (one 44.1 kHz stereo, one 22 kHz mono) into the folder.
2. MapMaker with `TSFP_MAPMAKER_LIMITS=1`: open Map Settings > Music, confirm
   both `Imported:` rows, pick one, save, reload the map: the row is kept.
3. Play the map: your track plays and loops; pause (silent), resume, change
   the Music slider; finish the match: the jingle replaces it.
4. Rename the file and replay: tileset music and the log line above.
5. Arcade: choose an imported row, start a level, check the music; reopen
   setup: the row is still selected; pick a stock row: stock music again.
6. Repeat on macOS (native build).
