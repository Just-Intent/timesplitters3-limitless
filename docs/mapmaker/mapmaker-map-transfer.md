# MapMaker map export and import

Status 2026-11-06: implemented in the shared launcher (Saves page), with
Windows tests passing on synthetic save folders. Not checked in the game
(the game was never run for this work). The native Mac launcher compiles the
same code but has not been built or tested on a Mac.

## Where maps live

CONFIRMED from the US XBE disassembly and a disposable test save
(`private/test-saves/mapmaker-probe`):

- Each saved map is its own XAPI save directory under the save folder:
  `<saves>/UserData/45410066/<12 hex digits>/`, holding
  - `SaveMeta.xbx`: UTF-16LE with BOM, `Name=MapMaker: <name>\r\n`
    (`Carte: ` in French; `sub_001906B0` accepts both prefixes);
  - `anicemap.mkr`: the map body (one size tier: 0x7FE0, 0xFFE0, 0x1FFE0,
    0x3FFE0 or 0x7FFE0 bytes) followed by a 20-byte HMAC-SHA1 signature.
- The directory name is a hash of the full save name (`sub_0037DCE0`, lifted
  XAPI): `h = (h * 0x10000 + c) mod 0xFFFFFFFFFFC5` over the UTF-16 code units,
  printed as 12 uppercase hex digits. It matches every probe save, for
  example `MapMaker: s0` -> `7DC83E79F248`, and the profile `Player 1` ->
  `16D15BD8D404`.
- Profiles are save directories in the same folder (`SaveMeta.xbx` +
  `tsftrpft`). **Maps are not stored per profile**: every profile in a save
  folder sees the same maps. "Another profile" therefore means another save
  folder (another installation, a custom save folder chosen in the launcher,
  or another computer).
- The PC runtime signs with a zero `XboxSignatureKey`, so the signature is
  `HMAC-SHA1(16 zero bytes, body)`; it does not depend on the machine or the
  profile. The probe maps verify this way.

Body layout (CONFIRMED on the probe maps, see also
[mapmaker-container.md](mapmaker-container.md)): magic 0x00C77667, version
0x12D, then chunks `{tag 0x00C776xx, size, data}`. The first chunk 0x68
(map information) holds the piece count at data +4, the tileset word at +0xA,
the map name (ASCII, NUL-terminated) at +0x62 and the used size at +0x248.
The end chunk 0x6A carries its own offset as size; `used` = its offset + 8.
The zlib CRC-32 of the first `used` bytes is stored at `used`. The bytes after
the CRC are zero. The name field width is INFERRED as 32 bytes (the save list
entry keeps 0x20 bytes for the name); renames never write into nonzero bytes.

## The .tsmap file

```
"TSFPMAP1"                  8 bytes
u32 header length H          little endian, 1..65536
H bytes UTF-8 JSON           metadata (below)
u32 map length M             body + 20, at most 0x7FFE0 + 20
M bytes                      the exact anicemap.mkr
32 bytes                     SHA-256 of everything before
```

Metadata: `format` (1), `title_id` ("45410066"), `name`, `body_size`, `tier`,
`used`, `pieces`, `tileset`, `container_version`, `crc32`, `launcher`,
`game` (installed game version, may be empty) and `exported_utc`. Import
re-derives every value from the map bytes and rejects a file whose
`body_size`, `pieces` or `used` disagree.

## Import checks

A file is refused, with nothing written and no backup made, when: the
SHA-256 or length is wrong; the format/title is unknown; the body is not a
size tier; magic/version differ; the CRC fails; the chunk list or end marker
is damaged; the tileset is >= 7; it has more than 511 pieces
(`MAPMAKER_LIMIT_PIECES`); or the name is not 1-31 printable ASCII characters.
A different signature is accepted (for example a file signed by another
key) because the import always re-signs.

Then, holding the launcher save lock with the game closed, the launcher
recovers any interrupted restore, makes a full save backup (reason
"Before map import", listed on the Saves page), picks a free name (the same
name, else `name (2)`, `name (3)`... shortened to 31 characters; names are
compared ignoring case, and the hashed directory must not exist), writes the
new name into the body, recomputes the CRC and signature, writes both files
into a `.tsmap-import-<guid>` staging folder and renames it to the hashed
directory. Existing maps and other save files are never opened for writing.
A leftover staging folder holding only those two files is removed on the next
import; anything else in it stops the import and is kept.

Export is read-only for saves. It writes to the chosen file (through
`<file>.new`, verified, then replaced) or, without a picker, to
`Backups/Maps/<name>.tsmap` (never overwriting an earlier export). Export
refuses targets inside the save folder and damaged maps.

Maps over 32 KB, or with more than the stock counts, need this mod's
container patch; older builds and the original game refuse them (error 7).
The Saves page says so for maps over 32 KB.

## Using it

1. Close the game. Open the launcher, **Saves**.
2. Under **MapMaker maps**, pick a map and choose **Export map...**. Save the
   `.tsmap` file anywhere (USB stick, chat, cloud folder).
3. On the other computer or installation (or after **Choose save folder...**
   for another save folder), choose **Import map...** and pick the file. The
   map appears in the game's MapMaker load list for every profile.

## Code and tests

- `tools/launcher/MapTransfer.cs`: list, pack/unpack, validation, rename,
  export, import. Shared by the Windows (WPF) and Mac (Avalonia) launchers.
- `tools/launcher/SavesPage.cs`: the MapMaker maps section; the hosts supply
  the file pickers (`Launcher.cs`, `macos/NativeWindow.cs`).
- `tests/launcher/launcher_maps.cs` (run from the shared UI checks): hash
  vectors, round trip between two synthetic save folders (other files
  unchanged, backup made), the 64 KB tier, collisions and long names,
  damaged/oversized/over-cap files, staging cleanup. `launcher_shared_ui.cs`
  checks the page lists maps and disables export for a damaged one.

## Not done / other option

- In-game export/import in the MapMaker load/save screen was not built. It
  would need a new menu entry in the guest menu code
  (`pc_menu_mapmaker_browser.inc`), a native file picker (as the save-folder
  picker in `pc_storage_menu.inc`) and the same validation in C; the game is
  also the process holding the map buffer, so a write would have to refresh
  the slot list (`[0x00778D38]`). The launcher route needs no game change and
  works while the game is closed, which is why it came first.
- Not verified in the game: that an imported map loads, plays and re-saves
  into the same directory (expected, since the directory is the game's own
  hash of the name). Mac launcher build/tests and the Avalonia pickers are
  unverified.
- Raw `anicemap.mkr` import (for example from an Xbox) is not offered.
