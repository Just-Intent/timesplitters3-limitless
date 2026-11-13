# MapMaker map export and import

The launcher's **Saves** page can copy a MapMaker map out of your save folder
into a single `.tsmap` file, and add a `.tsmap` file back as a new map. Use it
to share maps with friends, keep a copy of a favourite map, or move maps to
another computer or another save folder.

## Export a map

1. Close the game and open the launcher.
2. Select **Saves**. Under **MapMaker maps**, pick a map from the list. The
   line below it shows its piece count.
3. Choose **Export map...** and pick where to save the `.tsmap` file (for
   example a USB stick or a shared folder).

Exporting only reads your saves; nothing in the save folder changes.
**Open map exports** opens `Backups/Maps` in the game folder.

## Import a map

1. Close the game and open the launcher on the computer or installation that
   should receive the map. To add it to a different save folder, choose that
   folder first with **Choose save folder...**.
2. Select **Saves**, then **Import map...**, and pick the `.tsmap` file.

The launcher first checks the file and then backs up your current saves. The
backup appears in the restore-point list as **Before map import**. The map is
added as a new map. If a map with the same name already exists, the copy is
named `Name (2)`, `Name (3)` and so on. Existing maps are never replaced or
changed.

Maps belong to a save folder, not to a game profile: every profile that uses
the save folder sees the imported map in MapMaker's load list.

## When a file is refused

Nothing is written and no backup is made when the launcher refuses a file.
Common reasons:

- The file is damaged, incomplete or not a `.tsmap` file.
- The map has more than 200 pieces, or is larger than the 32 KB map size the
  game loads.
- The map name is not 1-31 plain ASCII characters.
- The game is running. Close it and try again.

A map listed as **cannot be exported** on the Saves page is damaged in the save
folder; the reason is shown below the list.

## The .tsmap file

A `.tsmap` file holds the exact saved map plus a short description and a
SHA-256 checksum:

```
"TSFPMAP1"                 8 bytes
u32 header length H        little endian, 1..65536
H bytes UTF-8 JSON         metadata
u32 map length M           0x7FE0 + 20
M bytes                    the saved anicemap.mkr (map body + 20-byte signature)
32 bytes                   SHA-256 of everything before
```

The metadata records the map's name, size, piece count, tileset, CRC, the
launcher and game versions, and the export time. Import re-reads these values
from the map itself and refuses a file whose description does not match. On
import the launcher writes the map's new name into it and recomputes its CRC
and save signature for the receiving save folder.

## Limits

- Close the game for both export and import.
- Importing a map into the game while it is running, or exporting from inside
  MapMaker, is not available.
- Maps from an Xbox (raw `anicemap.mkr` files) cannot be imported directly.
