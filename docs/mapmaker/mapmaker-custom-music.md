# Your own music in MapMaker and Arcade

User request (2026-11-03): "Ability to import your own music and make
available for mapmaker and arcade."

**Status:** implemented with fixture tests only. It has not been built or
played on either platform. Agent notes:
[custom-music.md](../agent-context/custom-music.md).

## Using it

1. Import WAV or MP3 files with the launcher (Saves page > **Imported
   music** > **Import music...**), or copy them into the music folder:
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

Up to 64 tracks are listed (file names up to 63 bytes, ending in `.wav` or
`.mp3`). Names show without the extension, with `_` as spaces; non-ASCII
letters show as `?`.

## Formats

- **WAV:** PCM 8, 16, 24 or 32-bit, or 32/64-bit float (WAVE_FORMAT_EXTENSIBLE
  too), 8,000 to 192,000 Hz. Read by the shared code itself on every host.
- **MP3:** MPEG audio files (ID3 tags are skipped), decoded with the FFmpeg
  libraries the Windows game already ships for movies (`avcodec`, `avutil`,
  `swresample` in `Bin`). MP3 rows appear only when those libraries load; the
  native Mac build has no FFmpeg yet, so it offers WAV only (MAC-138). A sample
  rate change inside one file ends the track.

Mono and stereo play as they are; surround WAVs play their front left/right
pair. Files are streamed from disk and loop. OGG, FLAC and other formats are
not read: convert them to WAV or MP3 first.

## What to expect

- A jingle during the level (an objective, a pickup) pauses your track; it
  carries on when the jingle ends. Restarting the level starts it again.
- The track replaces the level music only. The end-of-match jingles (Mission
  Complete, Game Over) are the game's own; when one starts, your track stops.
- It follows the Music volume slider (full slider = the file's own level),
  pauses with the game's music and stops when you leave the level.
- **Volume:** at the same slider position an imported track is already
  played 8 to 12 dB higher than the game's level music (the game's music
  plays at 50-80% of the slider, minus 6 dB of console headroom). If it still
  sounds quiet, add `gain_db=6` (any number from -12 to 12, in dB) on its own
  line in `pc-music.ini` (beside the display settings, after the
  `TSFP_MUSIC_V1` line). Positive values above what the voice can give are
  applied to the sound itself with a soft limiter, so a loud track bends
  rather than crackles. It takes effect when the next track starts; the
  Arcade choice keeps the line. The log's `[music] player: level ...` line
  shows the level used and the stock range at that slider.
- **Louder than full (2026-11-07):** the Music bar in Audio Options (the
  Settings menu and the pause menu share it) now has 24 steps instead of 16.
  Steps 0-16 are the game's own; the bar just fills in smaller steps (the
  old full volume is two thirds of the way along). Each step past 16 adds
  1.5 dB to imported music, up to +12 dB at the end, on top of `gain_db=`
  (the two together stop at +12 dB). Arrows and mouse drag both work, and
  the change is heard at once. The game's own music does not get louder past
  step 16. The extra steps are saved as `slider_extra=` (0-8) in
  `pc-music.ini`; the profile itself still stores 16, so the stock game and
  older builds read it normally. They count only while the profile's slider
  is full: a profile saved lower shows its own value, and reaching 16 again
  starts from 16. The log shows `[music] player: Music slider 16 + N ticks`.
- If the music is switched off in the options, nothing plays.
- **Fallbacks:** a map whose file is missing plays the tileset's music (the
  log says `[music] map's imported track is missing`). An Arcade choice whose
  file is missing plays the level's music. A stock or older build loads such
  a map with the tileset's music; the map is not moved to the larger save
  tier.
- **Which music wins:** an imported Arcade choice first, then the map's own
  imported track, which also replaces a stock Arcade music pick made in the
  play setup. A map whose Music row is a stock track plays that track.
- **Log:** every decision is logged as `[music] play: ...` or
  `[music] map: imported track`; a track that does not start says why. The
  player then logs `[music] player: first submission ...`, and when it ends,
  `[music] player: stopped: <why>`. Builds .44-.46 started the track but kept
  it silent (they read the wrong pause word); .47+ follow the game's music
  pause.
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

The Saves page has an **Imported music** section on Windows and macOS:

- **Import music...** picks one or more `.wav`/`.mp3` files and copies them
  into the music folder. Names are made safe for the game, and a name that is
  already used gets ` (2)`, ` (3)`... Nothing is replaced; your originals stay
  where they are. Each copy is staged and moved into place, so a cancelled or
  failed import leaves no partial track. Files must look like WAV/MP3, be at
  most 512 MiB each, and the folder holds at most 64 tracks.
- **Remove track...** asks first, then moves the file to the Recycle Bin
  (Trash on Mac). Close the game first.
- **Open music folder** shows the folder.

## Playtest

1. Import two WAV files (44.1 kHz stereo, 22 kHz mono) and one MP3 with the launcher.
2. MapMaker with `TSFP_MAPMAKER_LIMITS=1`: open Map Settings > Music, confirm
   both `Imported:` rows, pick one, save, reload the map: the row is kept.
3. Play the map: your track plays and loops; pause (silent), resume, change
   the Music slider; finish the match: the jingle replaces it. In the pause
   menu's Audio Options move Music past the old full point (arrows, then a
   mouse drag): the track gets louder each step, the bar stays in its box at
   4:3, 16:9 and 21:9, and `pc-music.ini` gains `slider_extra=`.
4. Rename the file and replay: tileset music and the log line above.
5. Arcade: choose an imported row, start a level, check the music; reopen
   setup: the row is still selected; pick a stock row: stock music again.
6. Remove the MP3 from the launcher: it goes to the Recycle Bin; a map using it
   plays the tileset music.
7. Repeat on macOS (native build): WAV rows only until MAC-138's FFmpeg adapter.
