# MapMaker editor object: Story AI, logic and free-standing objects

Area "editor" of the 2026-10-23 MapMaker limit raise. Research basis:
`logs/limits/ai-logic.md` and `logs/limits/items.md` §1.3 (ignored, main
checkout). CONFIRMED = seen in the disassembly at the given address;
INFERRED = deduced. Nothing here has been built or played yet.

Files: `config/guest-code-patches/editor.json` (lift-time operand
rewrites), `src/pc_mapmaker_editor.inc` (C policy, wrappers),
`tests/mapmaker_editor.c` + `tests/test_mapmaker_editor.py` (fixtures).

## Wrappers for the integrator

    WRAP sub_0031BE70 -> pc_mapmaker_editor_sai_add
    WRAP sub_00301A90 -> pc_mapmaker_editor_load
    WRAP sub_00240C50 -> pc_mapmaker_editor_level_load
    WRAP sub_00321E60 -> pc_mapmaker_editor_logic_can_add
    WRAP sub_00321E80 -> pc_mapmaker_editor_logic_full
    WRAP sub_00325F60 -> pc_mapmaker_editor_objective
    WRAP sub_0031DB00 -> pc_mapmaker_editor_node_room
    WRAP sub_00300670 -> pc_mapmaker_editor_logic_room
    WRAP sub_0033F480 -> pc_mapmaker_editor_logic_slot
    WRAP sub_00304330 -> pc_mapmaker_editor_free_place
    WRAP sub_002F7F30 -> pc_mapmaker_editor_free_move

Hooks that replace their original (`sai_add`, `logic_can_add`,
`logic_full`, `node_room`, and the refusal paths of `objective` and
`logic_slot`) finish with the original's `ret` (`g_esp += 4`; all six
originals end in a plain `ret`, CONFIRMED at `0x0031BEFE`, `0x00321E7F`,
`0x00321EE7`, `0x0031DB67`, `0x0032609D`, `0x0033F721`). The fixture runs every
hook from a fixed esp with originals that pop their return address and
checks esp + 4 afterwards. `logic_room` reads its caller's return address
before calling the original, which pops it.

Include `pc_mapmaker_editor.inc` after `pc_mapmaker_limits.inc` (it uses
`mapmaker_limits` / `mapmaker_limits_read`).

## Layout

The editor object (`sub_002FE360`, Level heap, pointer `[0x007844A8]`) was
0x11950 bytes with every array packed against the next field. editor.json
makes it 0x58390 bytes (`push 0x11950` at `0x002FE363`, and the clear at
`0x002FE37A` now zeroes all of it) and moves each array behind +0x11950.
Count fields stay where they were.

| Array | Stock | Enlarged | Delta |
|---|---|---|---|
| Story AI | +0x288, 50 x 0xE4 | +0x11950, 255 | +0x116C8 |
| Logic ops | +0x41E4, 30 x 0x220 | +0x1FC70, 120 | +0x1BA8C |
| Logic nodes | +0x81A8, 130 x 0xB4 | +0x2FB70, 520 | +0x279C8 |
| Logic records | +0xDD14, 100 x 0x8C | +0x46910, 400 | +0x38BFC |

| Free objects | +0x3070, 170 x 12 | +0x543D0, 680 | +0x51360 |
| Clipboard objects | +0x3870, 170 x 12 | +0x563B0, 680 | +0x52B40 |
| Clipboard counts | +0x3868..+0x386E | +0x5048..+0x504E | +0x17E0 |

The chunk writers read 0xC0 per node and 0x90
per record (12 / 4 bytes past the element, `0x00301624`, `0x003017C5`); with
these offsets the last node's over-read reads the first record and the last
record's reads the free-object array: reads only, inside the object (both
loaders copy 0xB4 / 0x8C, `0x00302697`, `0x00302736`).

## How the sites were found

An interprocedural tracker over the decoded functions (the same decoder the
lift uses) follows the editor pointer from every load of `[0x007844A8]`:
registers, stack slots, call arguments (per calling context, up to eight
contexts per function), return values, pointer differences, and constants
folded into an index (`(count+3)*0xE4`). Each value carries the instructions
that contributed its constant parts and the multipliers of its variable part.
A memory access or address whose constant offset lands in an old array, or
whose index stride is an array's element stride (catching biased indices
such as `node[sel-2]` at `0x00324914`), names the instruction to rewrite.
Its results were then checked against:

- the research report's register-tracker appendix: Story AI identical
  (135 references + the folded store);
- every constant in the generated C of every function that loads the editor
  pointer (no unexplained array-range constant left; the rest are text ids,
  container offsets and flags);
- the lift's own verifier (`check-tables.py`: every entry applies, no
  instruction boundary changes).

Corrections to the report found this way: `0x003260C6`
(`[esi+ecx+0x404C]`) and `0x003260D4` (`[edi+0x3FC4]`) address the LAST op,
`ops[count-1]`, through a constant below the array (0x3FC4 = 0x41E4 - 0x220):
not in the report, they would have read the vacated old array.
`0x0032490D`/`0x00324914` (`+0x805C`/`+0x8040`, filed under ops) are
`nodes[sel-2]` (stride 0xB4) and move with the nodes. `0x0031FE32`
(`[ecx+eax*8+0x11398]`) and `0x0031FCEE`/`0x00327A8D` (`+0x11394`) address the
[kind][4] x 0x18 table at +0x113C4 with a biased index, not the records
array, and stay; `0x00316569` and four others read `[x*4+0x4064]`, the dword
array at +0x4068 with index-1, not the clipboard.

Folded sites: `0x003024F9` `add edx, 3` then `imul edx, 0xE4` then
`mov [edx+eax], ecx` (SAI[count]+0x24, loader). The delta is not a multiple
of 0xE4, so the store at `0x00302502` gets displacement 0x116C8 (an operand
override may set a displacement the encoding does not have).

## Story AI (stage 1)

- 136 operand rewrites (135 displacements/immediates + the folded store) and
  the two clears (`0x002FEC2D`, `0x00321C6A`: 0xB22 -> 0x38C7 dwords).
- Editor limit: `sub_0031BE70` is reimplemented (`pc_mapmaker_editor_sai_add`):
  stock 50, 255 with `TSFP_MAPMAKER_LIMITS=1`. Same container-space test
  (`sub_00300670(0x58)`), same messages; the limit message gets the limit in
  force. The editor colour id (+0x60, index into the 50-entry palette
  `0x0052CE00`, assigned by `sub_0031BB60`) is 50 for every story AI past
  the palette (stock behaviour, reads past the table); the wrapper gives
  those index mod 50.
- The byte index `[0x00762F94]` is not widened: 255 is its ceiling and every
  reader zero-extends it (`0x0031BAC0`, `0x0031BB30`, `0x0031CAC0`,
  `0x0031CB91` movzx; `sub_0031BA80`'s `al` goes to `sub_0031BAA0`, which
  movzx's its argument). CONFIRMED.
- `sub_00323220` (editor) fills an unread 50-slot stack array per story AI
  (`0x00323300`): frame 0xCC -> 0x4CC (306 slots).
- `sub_00240590` (map start, count pass) keeps one stack pointer per story AI
  chunk at frame+0x1C with no bound (`0x00240776`): frame 0xE4 -> 0x101C,
  1024 slots; its argument read `[esp+0xF0]` moves with the frame
  (`0x00240598`) and the three epilogues follow. CONFIRMED that no other
  esp-relative access in it lies above the array.
- Capacity guards: `pc_mapmaker_editor_load` (editor loader `sub_00301A90`,
  which appends every chunk with no bound) and
  `pc_mapmaker_editor_level_load` (`sub_00240C50`, both play-time passes)
  hide chunks past capacity by setting their tag to 0 for the duration of the
  call (both loaders skip unknown tags: `0x00301C08` switch default,
  `sub_00240590`/`sub_00240C50` compare chains) and restore them afterwards.
  The loader caps are the array capacities, not the opt-in limit, so a map
  made with the limits on still loads with them off.
- Play time is otherwise heap by count (`desc+0x74` = n x 0x98 etc.).
  Simultaneous AI are not raised (32 actors, 30 AI slots, ~20 live in scene
  100); see "Spawning" below.

### Story AI list after "New Story AI"

CONFIRMED: the list is the menu built by `sub_0031D580` (row 0 "New Story
AI", callback `sub_0031BE70`; one row per story AI with its points). After an
add, `sub_0031BB60` sets editor+0x238 bit 0x40 (`0x0031BD26`) and flags the
menu for rebuild (`sub_000746D0`: menu+0x44 |= 0x20). The list's update
callback `sub_0031C9E0` tests the bit (`0x0031C9E5`), calls
`sub_000736F0(menu, 1.0)` (scroll target menu+0x88 = the bottom) and clears
it. Nothing else reads or sets that bit, and neither function changes the
menu's selected row: the cursor stays on "New Story AI", the view jumps to
the bottom. With `TSFP_MAPMAKER_LIMITS=1`, `pc_mapmaker_editor_sai_add`
clears the bit after a successful add, so the scroll position (and the
selection) stay where they were and several AI can be created in a row.
Limits off: stock jump. The new row is appended at the bottom and is only
visible once the player scrolls there.

## Logic (stage 2)

- 387 operand rewrites (ops 96 incl. the folded `0x00326024`
  `OPS[node.op]+0x21C = 1` store, nodes 154, records 137) and the three
  clears in `sub_00321C00` (0xFF0 -> 0x3FC0, 0x16DA -> 0x5B48,
  0xDAC -> 0x36B0 dwords).
- Editor limits (opt-in): ops 30 -> 120 and objectives 10 -> 40
  (`sub_00321E60`/`sub_00321E80` reimplemented; `sub_00325F60`, which turns
  a mover into an objective without asking `sub_00321E60`, has its own test
  at `0x00325F65` raised to 40 in editor.json and the stock 10 enforced by the
  wrapper when the limits are off). Nodes per type (`sub_0031DB00`: type 0
  -> 30, types 1/2 -> 50) x4; kind-0 ops (data `0x0052E8E8`, read by
  `sub_00322AC0` and printed by `sub_0031DF80`) 30 -> 120. Kind-1 ops stay at
  8: `sub_003242B0` sizes a 0x110-byte heap block for 8 entries.
- Records have no editor limit at all in the stock game (CONFIRMED: no
  compare against +0xDD10), and the node/record allocators (`sub_00320B20`,
  `sub_00320CC0`, `sub_00325820`) cannot fail. Backstop: every logic editing
  action first asks the container-space test `sub_00300670`; for the nine
  logic call sites (return addresses in the .inc) the wrapper also answers
  "no room" (stock message) when ops are at capacity or nodes/records are
  within 8 of it. The objective wrapper applies the same test.
- `sub_002FBDE0` collects matching op indices into an unchecked 8-slot stack
  array (`0x002FBE1E`): frame 0x20 -> 0x200 (128 slots, more than the 120
  ops).
- Loader: ops 120 (kind-1 ops 8, kind byte at data+4), nodes 520, records
  400.
- Play time: heap by count, except the 45-slot table at
  `[0x00765E44]+0xB4` (`sub_0033F480`, unbounded). Its slot index is the HUD
  marker id: `sub_002287E0` allocates the marker array `[0x0078CECC]` as
  (objects + 45) x 0x4C with the first 45 reserved for logic, and
  `sub_00227750` / `sub_00227620` index it with the slot (`cmp edi, 0x2D` at
  `0x002277D6`). Enlarging the table would make logic ids alias object
  markers, so it is guarded instead (`pc_mapmaker_editor_logic_slot`): a 46th
  distinct record's action is skipped (logged once). INFERRED: actions that
  reach it are objective/marker actions; more than 45 distinct ones at once
  is the remaining limit.

## Free-standing objects (stage 3)

The objects live in two set structs, {u16 count[type 0..2], u16 total;
entry[170] of 12 B}: the map at +0x3068 and the clipboard at +0x3868.
`sub_002F7DE0` (remove), `sub_002F7F30` (copy between sets) and
`sub_002F9440` take a set pointer and reach entries as set + 8 + 12*i for
either set (CONFIRMED, register arguments). Moving the entries but not the
counts therefore needs the same delta for both sets: the brief's offsets
are kept for both arrays, and the clipboard COUNTS move instead, to
+0x5048 (inside the vacated old ops array) so that +0x5048 + 8 + 0x51360 =
+0x563B0. The map's counts (+0x3068..+0x306E, read by
`pc_mapmaker_limits.inc` and the items area) stay. Conflict with the brief:
only those four clipboard count words move.

- 119 rewrites: 95 map-entry references, 5 clipboard-entry references, 19
  clipboard-count references (including the three clipboard set-base
  formations `0x002F9842`, `0x002F9E00`, `0x002FB7F7`), the four
  set-relative entry formations (`0x002F7DEC`, `0x002F7F37`, `0x002F7F83`,
  `0x002F9462`: +0x51360), and five folded index constants
  (`lea [i+i*2+0xC1E]` then `[ed+x*4]` at `0x002F80B6`/`0x0030473B`;
  `add eax,0x40A` then `[ed+(x*3)*4]` at `0x003020E5`/`0x00304B97`;
  `lea [i+i*2+0xE1C]` at `0x002F98A6` for the clipboard), whose constants
  move by delta/4 or delta/12. Kept: the map set base and counts, the
  dword array at +0x4068 (and `[x*4+0x4064]`, its index-1 form).
- Limits (opt-in): per type 40/30/100 -> 160/120/400 (sum 680 = capacity).
  `sub_00304330` (place, `mov ebx,0x28` / `0x1E`) and `sub_002F7F30`
  (copy, `mov edx,0x28/0x1E/0x64`) keep their stock compares; the wrappers
  show the per-type count as stock-1 for the call and put back the real
  count plus what the original added (CONFIRMED that both only compare and
  increment that word, and call nothing that reads it). Cosmetic: at the
  raised limit the refusal text still names the stock number.
- Loader: 680 free-object chunks.
- Note: with `TSFP_MAPMAKER_LIMITS=1` the item total (200, `pc_mapmaker_limits.inc`)
  counts free objects too (+0x306E), so that total binds first.
- Play time: heap by count (`desc+0x1C`, 0x6C each); no fixed table.

## Spawning (story AI past 50)

CONFIRMED structure: `sub_00164010` runs every frame over all
`desc+0x78` spawn records (one per story AI chunk, built by `sub_00240C50`;
loop bound is the count, no fixed table) with per-record state in
`[0x007A2820]` (n x 0x18, heap). A record spawns while its spawned total
(`state+0`) is below record+0x60, its live count (`state+4`) below
record+0x5C, its trigger (`sub_0006C890`) and timer (`state+0xC`) allow it,
and (scene 100) fewer than 20 AI are alive (`0x001640EC`,
`0x00164199`). So story AI past 50 are spawned exactly like the first 50,
as live slots free up; whether one respawns is its own +0x60 setting.
INFERRED (not played): with many placed AI the map fills to ~20 live and
refills as they die. Simultaneous AI are not raised.

The extension sub-range 0x00900000..0x0091FFFF is not used: every table
here is per map (editor object, Level heap) and the play-time 45-slot
table is guarded rather than relocated.

## Validation

- `tests/test_mapmaker_editor.py`: the C policy against fake guest memory
  (with mutations), and the table against this layout (each rewrite moves
  its value by exactly its array's delta; old value present in the bytes).
- `check-tables.py` on the folder: "table verified".
- Not built, not played.
