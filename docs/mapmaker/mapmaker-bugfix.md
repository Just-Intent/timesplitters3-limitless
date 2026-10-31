# MapMaker and System Link bug fixes (fixes 1-7)

Research: `logs/limits/features/bugs.md` (ignored). Sites were checked with the
lifter's decoder (`tests/mapmaker/test_mapmaker_bugfix.py`, `tests/mapmaker/test_mapmaker_validate.py`).
No extension memory is used.

| # | Fix | Where | Switch |
|---|---|---|---|
| 1 | System Link fragment receiver `sub_00352DF0`: drop a fragment with length > 0xF0 (slot overflow) or index*0xEF + length > 0x8000 (reassembly buffer `[0x0076ADF4]`) | `pc_mapmaker_bugfix_net_fragment` | always |
| 2 | `sub_0006D0B0`: the store `[esp+ebp*4+0x14]` came before `cmp ebp,0x50`, so the 81st match overwrote the return address. `0x0006D115 83fd50 -> 83fd4f` (at most 79 kept) | `config/guest-code-patches/bugfix.json` | always |
| 3 | Turret sync `sub_003596F0`: drop the message when a slot is past the gun pool (free-list total `[0x00732D38]`, max 80) or a player byte (not 0xFF) is past `[0x00790950]` player structs | `pc_mapmaker_bugfix_turret_sync` | always |
| 4 | Map validator for the editor loader and the scene-100 level setup | `src/interface/pc_mapmaker_validate.inc` | always |
| 5 | Network death effect `sub_00357D70`: drop for an unknown player, or when the effect list `[0x0075F6E8]` is at 64 (wide play) / 30 (otherwise), as `pc_mapmaker_actors_effects` does | `pc_mapmaker_bugfix_net_death` | always |
| 6 | Story HUD turret icons: the HUD scans slots 0..live-1 but live guns sit in the highest slots. In scene 100 the HUD gets the pool size as the count for the call, and blocks on the free list are skipped | `pc_mapmaker_bugfix_hud`, `_hud_turret` | `TSFP_MAPMAKER_LIMITS=1` |
| 7 | Arcade mode 1 bot goal `0x002B0363` read a word 0x413D8 bytes before the runtime pool. Now returns `runtime[h-0x4740].owner` (+8) | `pc_mapmaker_bugfix_bot_goal` | always |

## Validator (fix 4)

The validator checks only bounds that would make a loader read or write
outside its memory. It never checks positions, floors, overlap or items per
tile. The bounds are tabled in the header of `src/interface/pc_mapmaker_validate.inc`:
- walk inside 512 KB with an end tag
- header: tileset < 7, word count <= 17, pieces <= 1024 (editor) or <= 505 (level stack array)
- pieces fit their chunk: tile id < 45, item id 0..79, pool total 1024
- patrol count <= 8 and story AI name < 0x24
- logic links: <= 13 (type 3) or <= 10 (type 0xA), indices inside the 1024-slot buffer
- chunk sizes: 0x73 <= 0x4B00, 0x74 <= 2000, 0x75 <= 3000

Link indices equal to the piece count are accepted, because the saver writes
that for a piece it cannot find (`0x0030177E`). Piece chunks before any
header are not read by either loader, so they are accepted.

- Editor (`pc_mapmaker_editor_load` calls `pc_mapmaker_validate_editor_load`
  first): a bad map is refused the stock way. The magic is cleared and the
  original takes its own mismatch path (`0x003029C7`, returns 0).
- Level (`WRAP sub_0006D7A0`, scene 100 while `[0x006F32EC] == 0`): no stock
  refusal exists here, so for the call:
  - a bad chunk gets tag 0, so the walkers skip it
  - a bad header hides itself and every piece chunk
  - a malformed walk is cut with an end tag

  All tags are restored afterwards.

## WRAP lines

    WRAP sub_00352DF0 -> pc_mapmaker_bugfix_net_fragment
    WRAP sub_003596F0 -> pc_mapmaker_bugfix_turret_sync
    WRAP sub_00357D70 -> pc_mapmaker_bugfix_net_death
    WRAP sub_002B0320 -> pc_mapmaker_bugfix_bot_goal
    WRAP sub_00349E40 -> pc_mapmaker_bugfix_hud
    WRAP sub_00349970 -> pc_mapmaker_bugfix_hud_turret
    WRAP sub_0006D7A0 -> pc_mapmaker_validate_level

`pc_mapmaker_validate.inc` must be included before `pc_mapmaker_editor.inc`
(done in `src/interface/pc_menu_mouse_guest.inc`).
