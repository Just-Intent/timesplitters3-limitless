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

(Logic and free objects: later stages, below.)

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

Corrections to the report found this way: `0x0031FE32`
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

## Validation

- `tests/test_mapmaker_editor.py`: the C policy against fake guest memory
  (with mutations), and the table against this layout (each rewrite moves
  its value by exactly its array's delta; old value present in the bytes).
- `check-tables.py` on the folder: "table verified".
- Not built, not played.
