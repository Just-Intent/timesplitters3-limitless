# MapMaker map file validation

MapMaker maps can be shared (their signature key is zero), and the original
editor loader `sub_00301A90` and the play-time passes `sub_00240590` /
`sub_00240C50` trust every count, index and size in the 32 KB map container. A damaged or crafted file could overflow heap, stack and globals.

`storage/2765-mapmaker-map-validation.patch` wraps two original functions in
`recomp_manual.c` under the shared display/controls guards. Both call
`src/interface/pc_mapmaker_validate.inc`, included by `pc_display_menu.c` after
`guest_writable` / `guest_call`:

- `pc_mapmaker_validate_editor` (301A90, editor load, container at [esp+4]):
  a bad map is refused the stock way. The container magic is cleared and the
  original takes its own mismatch path (301BAC -> 3029C7, returns 0), as for a
  corrupt file. A valid map runs the original unchanged.
- `pc_mapmaker_validate_level` (6D7A0, level setup): only in scene 100 before
  the one-time map pass ([6F32EC] == 0). There is no stock refusal there, so for
  the call a bad chunk gets tag 0 (every walker skips it), a bad header hides
  itself and every piece chunk, and a malformed walk is cut with an end tag.
  All tags are restored afterwards.

Only bounds that would make a loader leave its memory are checked; positions,
floors, overlap and items per tile are never judged, so editor glitches still
load. Bounds (each confirmed in the original code): walk inside 32 KB with an
end tag; tileset < 7; header word count <= 17; piece count <= 1024 in the
editor (piece buffer 301BCA) and <= 500 at level setup (240C50's 0x1030 frame
array at frame+0x90; entry 500 is past it); pieces and items fit their chunk;
tile id < 45; item id 0..79; patrol count <= 8; story AI name NUL < 0x24;
logic links <= 13 (type 3) / <= 10 (type 0xA) with indices inside the 1024-slot
buffer; chunk 0x73 <= 0x4B00, 0x74 <= 2000, 0x75 <= 3000. Link indices equal to
the piece count stay valid: the saver writes them for a missing piece (30177E).

Refusals log `[PC-MAPMAKER]` (first four). `tests/storage/test_mapmaker_validate.py`
compiles the validator with fake guest memory (accepted glitch maps, refused
crafted maps, level hiding/restoring, mutation cases), checks the hook wiring,
and with the supported private US executable decodes every cited instruction
and the 45-row tile table. `tests/graphics/fov_settings.c` stubs the two tile
pad-count routines the shared menu unit now references. In-game editor and
Story/Arcade map loads remain a manual check.
