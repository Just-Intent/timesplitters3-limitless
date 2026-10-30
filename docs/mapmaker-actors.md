# MapMaker actors area: how many characters can be alive at once

US Xbox build 45410066. This area decides how many characters (players and
AI) a MapMaker map can have alive at the same time. Everything here comes
from static analysis of the disassembly and the lift checker. Nothing has
been checked in play yet.

- Background: `logs/limits/ai-logic.md` §"Spawn runtime" and
  `logs/limits/turrets.md` (both ignored).
- Code: `config/guest-code-patches/actors.json` (byte rewrites, one
  relocation), `config/guest-code-patches/actors-list.json` (189 moved
  references, generated) and `src/pc_mapmaker_actors.inc` (C hooks).
- Tests: `tests/mapmaker_actors.c` and `tests/test_mapmaker_actors.py`.

**CONFIRMED** means the instruction was seen at the given address;
**INFERRED** means it was reasoned, not seen.

## Result

| Limit | Stock | Now | Scenes |
| --- | --- | --- | --- |
| Characters alive (players + AI) | 32 (spawner holds AI back at 20) | **64** (`TSFP_MAPMAKER_ALIVE=1..64` lowers it; 20 = stock) | 100, story mode, local play |
| AI alive | 30 | **63 - (humans - 1)**: 63 with one player, 60 with four | same |
| Everything else (story levels, editor, arcade modes, System Link) | 32 / 30 | unchanged: 32 characters, 30 AI, stock throttle | all other |

"Wide" below means: scene 100 (MapMaker map in play), story mode (byte
`[0x007DE455] == 10`), no System Link session (`[0x0076B108] == 0`, the flag
`sub_003533F0` returns). Only then can a character id reach 32..63.
MapMaker arcade maps keep 32 characters (their pair table holds only
humans + 10 anyway; their bots are a separate 16-character system).

## Design

A character's id is its actor pool slot (`[entry+8]`; `sub_0015DF20`
returns it; CONFIRMED). Everything indexed by id or by live-list index must
hold 64, and everything that cannot be widened must skip ids >= 32 without
writing anywhere. Three kinds of change:

1. **Capacity, every scene** (memory only; the slot guard still keeps 32
   characters outside wide play): the live list, the AI table, the pair table,
   the 3-per-id table 0x00739AA0, the perception layout.
2. **Scene-100 pool**: 64 actor slots, allocated by a reimplementation of
   `sub_000D6960` (other scenes run the original, 32 or 4 slots), so story
   levels do not pay 120 KB of Level heap.
3. **Guards** where the engine has fixed 32-entry storage that cannot be
   relocated (stack frames, per-actor fields): reimplementation with a
   64-entry array, or a documented, logged limit. Every guard that drops work
   counts it and logs the first three per kind
   (`[mapmaker] <what> (<a> of <b>)` on stderr).

### Capacity sites (patch tables)

| Structure | Stock | Now | Sites |
| --- | --- | --- | --- |
| Live list `0x007A3580` | 32 dwords | 64 at `0x00900100` | every reference moved by `operand` entries in `actors-list.json`: 181 instructions in 131+ functions (`tools/guest-refs.py 0x007A357C 0x84`), incl. `list[-1]` at `0x0015BCE7` (`0x007A357C` -> `0x009000FC`) and the immediate forms in `sub_003576B0` (`0x003576D2`, `0x003576F8`, `0x0035774B`, `0x00357817`). `0x007A3600` is a separate counter (`sub_0015E830`) and stays. Clears `mov ecx,0x20` @`0x00158D91`, @`0x0015C577` -> 0x40 |
| AI table `0x007A2D00` (index slot - humans) | 30 dwords | 64 at `0x00900200` | 8 `operand` entries (`0x0015DFE9`, `0x0015EA70`, `0x0025F94E/5E/6E/7F`, `0x002B522C/3C`). `0x007A2D24` @`0x002354E8` is the group table `0x007A2D80` at index n-1 and stays |
| AI slot cap | `cmp ebx,0x1E` @`0x0015DF7D` | operand 0x3F | the slot hook enforces 30 except when wide |
| Pair table `[0x0074C368]`, story mode | 32 characters | 64 (2016 pairs x 12 B) | `mov ecx,0x20` @`0x0015E7D2` (alloc), @`0x0015E75F` (clear), `mov edx,0x20` @`0x0015E0E8` (column clear). Story-mode levels pay +18,240 B of Level heap. Its only loop reader `sub_0015D870` is bounded by the live count (CONFIRMED) |
| Table `0x00739AA0` (3 dwords per id: `id*3 + 0..2`, `sub_000F7630`, `F76F0`, `F7B10`, `F8510`) | 32 ids (0x180 B) | 64 ids at `0x00900300` (0x300 B) | `relocate` (7 offsets; `0x00739C20` @`0x000F7B4C`/`0x000F7BBC` is another array: keep) + clear `mov ecx,0x60` @`0x000F897C` -> 0xC0 |
| Per-player perception `[player+0xF74]` | 32 characters (+0..+0x1FF), objects from +0x200 | 64 characters (+0..+0x3FF), objects from +0x400 | `mov edi,0x200` @`0x000CFA31`, `mov [esp+0x10],0x200` @`0x000D07E7`, `lea esi,[ebp+0x20]` @`0x00259B42` -> 0x400/0x40. **Shared change:** the slice is 0xC00 B (64 x 16 + 128 objects x 16) in `objects.json` (`obj-perception-clear/-stride/-reset/-alloc/-reset2`, was 0xA00); `tests/test_mapmaker_objects.py` follows. Character writers index `id << 4` (`sub_00259930` @`0x00259ADC`); readers `sub_000CF8D0`, `sub_000D01C0`, `sub_000D0730` (CONFIRMED) |
| Actor pool `[0x007356D8]` | 32 x 0xEA0 (4 in scene 101) | 64 in scene 100 | hook on `sub_000D6960` (alloc) / `sub_000D6920` (clear); formula sites `and 0xE4 / add 0x20` @`0x000D6974/77`, `0x000D6A25/28`, `0x000D6937/3A` are not patched |
| Spawner throttle | 20 alive | 64 (wide), 32 (other local MapMaker modes) | gate dword `0x00900000` read by `0x001640EC` and `0x001641A2` (first round) |

`relocate` cannot be used for the live list: alias functions decode the same
instructions twice (`sub_001DFF20`/`sub_001DFF90`/`sub_001E0009` at
`0x001DFF90`, `0x001E0044`, `0x001E009E`, `0x001E0140`; `sub_0026EFC0`/
`sub_0026F4CF`/`sub_0026F4DE` at `0x0026F59A`) and the checker then reports
"two relocated operands" (the objects area hit the same limit). Operand
overrides are keyed by address, so one entry covers every alias decode.
`actors-list.json` was generated with the lift's own decoder
(`tools.recomp.disasm`), and `test_moved_references` checks each entry
keeps its offset; the count (181 + 8) matches `guest-refs.py`. No data word
points into either table (`--data`: none).

### Hooks the integrator must add (`recomp_manual.c`)

All are cdecl with a plain `ret` (CONFIRMED at each function's end). For
non-display builds add `{ original(); }` stubs.

    WRAP sub_00163F10 -> pc_mapmaker_actors_spawn_gate
    WRAP sub_0015DF20 -> pc_mapmaker_actors_slot
    WRAP sub_000D6960 -> pc_mapmaker_actors_pool
    WRAP sub_000D6920 -> pc_mapmaker_actors_pool_clear
    WRAP sub_0025F830 -> pc_mapmaker_actors_placed
    WRAP sub_0015BCA0 -> pc_mapmaker_actors_list_add
    WRAP sub_0015C3D0 -> pc_mapmaker_actors_list_readd
    WRAP sub_00163B40 -> pc_mapmaker_actors_pick_type
    WRAP sub_00163BD0 -> pc_mapmaker_actors_pick_team_ai
    WRAP sub_00163C50 -> pc_mapmaker_actors_pick_group
    WRAP sub_00163CF0 -> pc_mapmaker_actors_pick_team
    WRAP sub_00163D90 -> pc_mapmaker_actors_pick_group_ai
    WRAP sub_0027BA40 -> pc_mapmaker_actors_enemies
    WRAP sub_000644D0 -> pc_mapmaker_actors_kill
    WRAP sub_000640C0 -> pc_mapmaker_actors_score
    WRAP sub_00064650 -> pc_mapmaker_actors_deaths
    WRAP sub_000DAC30 -> pc_mapmaker_actors_create
    WRAP sub_001339E0 -> pc_mapmaker_actors_blip

(The last two are new in the second round; `sub_001339E0` takes the blip
position in `edx`, which the hook leaves untouched.)

**`sub_0015DF20` and `sub_00163F10` must be wrapped wherever `actors.json`
is applied** (`TSFP_MAPMAKER_AREA_OFF=actors` is not safe with the table):
the AI slot cap is 63 in the code and only the slot hook keeps 30 in story
levels; without the gate hook the throttle is off.

New `guest_call` targets: `sub_0003DFA0`, `sub_001CD040`, `sub_001638F0`,
`sub_003533F0` (stubs added to `tests/mapmaker_area_stubs.h`).

### What each hook does (evidence)

- **pool / pool_clear.** `sub_000D6960` clears `0x00735630` (0x28 dwords),
  `0x007331A8` (0x870), allocates `slots*0xEA0` rounded to 16 from the Level
  heap, clears `0x00735368` (0xB1), sets `[0x007356D0]` from
  `sub_001CD040() & [0x00790958] & [0x00790954]` bit 0x1000 (the scene-101
  test does not apply in scene 100), clears `[0x007356D4]`, the pool and
  `[0x007B0C44]`; nothing when `[0x007DE304] == 0` (CONFIRMED, 75
  instructions). The hook does exactly this with 64 slots in scene 100 and
  records (pointer, slots); other scenes run the original and record 32 (4
  in 101). The three cleared tables are 40 weapon records and similar, not
  per-character (CONFIRMED by their users `sub_000D73B0`, `sub_000D8A00`,
  `sub_000D8B10`).
- **slot (every scene).** Predicts the original's slot (first free in
  `[humans, humans+used)`, story mode tests `+0x10/+0x14`, else `+0x18/+0x1C`;
  else `humans+used`). A new slot past the AI cap (30, or 63 when wide) is
  refused silently, like the stock refusal at 30. A slot at or past
  `min(recorded pool slots, 32 or 64, pair capacity)` is refused, counted and
  logged. Refusal = eax -1 without running the original; the only caller
  `sub_00164010` skips it (`0x00164231`) and retries on later frames.
- **placed.** `sub_0025F830` (story mode level start, from `sub_0015C560`)
  sets `[0x007A2958] = [[0x0074C36C]+0x30]` and creates that many AI with ids
  `humans+i` and AI table entries, no bound (CONFIRMED). The hook clamps that
  level-data count to `min(64, pool slots - humans)` and keeps the clamped
  value. Stock story levels stay below it (32 - humans >= 28 is more than
  any stock level's count: INFERRED from the stock 30-slot design).
- **list_add / list_readd.** `sub_0015BCA0` and `sub_0015C3D0` append with
  no bound (CONFIRMED). They refuse at 64 (returns: the count / 0 =
  "not added"). The inline appenders in `sub_0015C560` (humans only),
  `sub_0015C700` and `sub_0015F8C0` are bounded by the characters
  themselves: one entry per pool slot (INFERRED).
- **pick_\*.** Each of `sub_00163B40/BD0/C50/CF0/D90` reserves 0x80 B (32
  dwords) at `[esp+0x10]` and stores one entry per matching live character
  bounded only by the count (CONFIRMED `0x00163B86`, `0x00163C19`,
  `0x00163C9F`, `0x00163D3F`, `0x00163DD9`), then calls
  `sub_001638F0(self, n, array, default)`. With up to 32 live characters the
  original runs; past that the hook builds the same list (same filters: self
  skipped; `w[e+0x48]` type, or the spawn record `[e+0xBF0]` field `+8` or
  `+0x28` equal to the own record's `+0x74`; C50/CF0 take every human) in a
  64-entry array below the guest stack and makes the same call. A selector
  < 0 returns the default at once, as the originals.
- **enemies.** `sub_0027BA40(entry, flag)` stores enemies (another team
  `[e+0x84]`, not `[e+0x24]&8`, more filters incl. an RNG test for
  `[e+0xC6C]&0x10`) at `[esp+0x34]`, 32 dwords up to the end of its 0xA4
  frame (CONFIRMED `0x0027BB83`), then returns the object with the lowest
  score. Each score depends only on the AI and that candidate (distance,
  pair table, `sub_000640C0` of the candidate, `[0x007DE474]` fields), ties
  keep the first in list order (`comiss`/`jbe` @`0x0027BCA4`,
  `0x0027BE96`), and the early returns (`flag && [0x0074C388] != id`
  @`0x0027BA7A`; `[0x007DE458]&0x40` @`0x0027BA98`) build no array
  (CONFIRMED). Past 33 live characters the hook runs the original twice over
  a list of the same length in which the other entries are the AI itself
  (rejected at once by the team test): pass A = the longest prefix holding at
  most 32 candidates (team/flag superset), pass B = pass A's winner first,
  then the rest (at most 31 candidates). Pass B returns the stock argmin over
  all candidates, and the count, hence its `(count - humans)` RNG divisor
  @`0x0027BB5C`, is unchanged. The list is restored afterwards. The direct
  call closure (16 functions) never writes the list or the count and has no
  indirect call (CONFIRMED). Only difference from an unbounded stock run: the
  pass-A winner's RNG test (flag 0x10 characters only) is rolled again in
  pass B. Cost: two calls when more than 32 candidates exist.
- **create / kill / score / deaths.** Each actor has 32 per-opponent kill
  words at `+0xA10` (cleared as 0x10 dwords + 1 word by `sub_000DAC30`
  @`0x000DAEC1`); `+0xA50`, `+0xA52`, `+0xA60`.. are other score fields.
  Victims 32..63 now have a side table at `0x00900600`: 64 killer slots x 32
  words, word `(k*32 + v-32)*2`.
  - `sub_000644D0(killer, victim, points)` adds the points at
    `killer+0xA10+victim*2` in round states 6/7 (@`0x000645CC`, CONFIRMED).
    For victims 32..63 the hook runs the original, takes the points back out
    of the stray field and adds them to the side table at the killer's slot
    (`[killer+8]`). Sub-modes 0x12/0x13 (arcade, other paths) are skipped and
    counted; they never have ids >= 32.
  - `sub_000DAC30` (character creation, slot = 3rd argument @`0x000DADAD`)
    clears the new actor's words; the hook clears its side row when the
    creation succeeded (the slot's `+0` holds the returned object). The pool
    allocation (every scene) and the pool clear reset the whole side table,
    as the pool's own words are reset.
  - `sub_000640C0` (kills on characters of other teams; callers include the
    AI picker `sub_0027BA40` and `sub_000674C0`) sums, past 32 slots, the
    words of victims 0..31 and the side table for 32..63, with the same team
    test, network term, `[0x007DE474]` and `[0x007DE458]` 0x20/0x10 paths.
  - `sub_00064650` (times killed: a column sum over every slot) sums the side
    table's column for ids 32..63.
  - Other `+0xA10` users are gated: `sub_00063EB0` (not mode 10, copies the
    arcade scoreboard), `sub_00359AA0`/`sub_0035BEF0` (network). The kill
    words are therefore shown nowhere in story mode except through these two
    sums (CONFIRMED by the reference list `0x00063F61`, `0x000640E7`,
    `0x0006413B`, `0x00064568`, `0x000645CC`, `0x00064689`, `0x000DAEC6`,
    `0x00359C5C`, `0x0035C1E2/F7/50`).
- **blip / radar.** `sub_00133AB0` (radar) walks the characters
  `0..humans+AI` with the network table `0x00777244` (0x124 per slot) in step
  and stopped at `0x0077AD94`, 52 slots (`0x00133BA0`); `[ebx]` is read only
  in a System Link session (@`0x00133C60`/`0x00133C69`). The bound is now 64
  slots (`0x0077BB44`). Each blip is written by `sub_001339E0` to
  `0x007A5DC0 + n*16` with the counter `[0x007490F8]` and no bound; the buffer
  ends at `0x007A6140` (56 blips) and also receives the radar objects of the
  list `[0x007A20C0]` and mode 1/2/6 markers, so even stock could overflow it
  with many objects. Its two references (`0x00133A2F` write, `0x00134722`
  draw loop) now point at a 256-blip buffer at `0x00902100`, and the hook
  skips a blip when 256 are already queued (counted, logged).
  `0x007A5E78` @`0x00147283` lies in the old range but is the end of
  another table's loop. Mode 1/2 extras compare the character counter with
  52 (`0x00133DB2`, `0x00133E0D`); those modes keep <= 32 characters.
- **patrol table.** `sub_002B2750` (AI behaviour, flag 0x1000000) reads
  `0x00520C38 + (id*10 + step)*4` (5 sites, CONFIRMED). Only ids 0..4 have
  routes: its switch on `id-1` (@`0x002B3AB6`) sets the end marker for ids
  1..4, and the data shows 5 rows of pad ids ending in -1 followed by other
  tables (rows 5..31 are unrelated data in stock too). The five references
  now read a copy at `0x00901600` that the pool hook refreshes at each level
  load: rows 0..31 byte-identical to `0x00520C38`, rows 32..64 all -1.
  `sub_0009CC40(-1)` returns -1 (binary search, CONFIRMED), so an AI with id
  >= 32 takes the code's own "no route pad" path: it heads for player 1's
  current pad (`[[[0x007B0C48]+0x14]+0x7C]+0x678` @`0x002B3B4C`, and the
  `jl` @`0x002B3CA6`). Story levels read exactly the stock rows for ids
  0..31 (only a step past row 31's end, which stock would read from the
  next table, now sees -1).

## Inventory audit (311 functions)

Every function that touches the live list, the count, the pool, the 0xEA0
stride, the AI table, the pair table or the mask `[0x007A2BC8]` (311
functions, 72k instructions; list built by script) was audited for fixed
storage indexed by id or list index, constant 32/30 loops, masks, stack
arrays and narrow id fields. Findings and how each is handled:

| Finding | Where | Handling |
| --- | --- | --- |
| Stack arrays of 32 | `sub_00163B40/BD0/C50/CF0/D90`, `sub_0027BA40` | hooks above |
| Stack arrays that are safe | `sub_00163A80/AF0` (humans + 1), `sub_0015E120` (2), `sub_0027E6C0` (10), `sub_002B0770` (20), `sub_002F1C80` (5 ranks), `sub_00272450`, `sub_0037AE20` (not characters) | none needed (CONFIRMED) |
| `sub_002284D0` 2 x 32 stack arrays + mask | only caller `sub_00228870` when `[0x007DE455]==5` | gated (arcade) |
| 32-bit character masks `[0x007A2BC8]` (1 << id) | `sub_000B3630`, `sub_000BA310`, `sub_000D50E0`, `sub_001252C0`, `sub_001255E0`, `sub_0013E090`, `sub_0013EA80`, `sub_0013F410`, `sub_00064810`, `sub_00067F00`, `sub_002B4C70`, `sub_002B51E0`, `sub_002B55D0`, `sub_002B7750`, `sub_002E7080`, `sub_0035A120`, `sub_0035A270` | all gated: mode 5, sub-modes 0xB/0xC, network or never in mode 10 (CONFIRMED per site) |
| 16-entry arcade score tables `0x007DE4C0`, `0x006F0B08` records | scoreboard screens `sub_0013DD40`, `sub_002E6F80`..`sub_002F70D0`, `sub_000657A0`, `sub_000646B0` | gated (arcade scoreboards; read-only; INFERRED for the screen root) |
| Pair-table users | `sub_0015BD00`, `sub_0015BD30`, `sub_0015D870`, `sub_00163E10`, `sub_000E91F0` | table sized for 64 |
| 0x00739AA0 | as above | relocated |
| Level-start AI count | `sub_0025F830` | placed hook |
| Network 16-slot loops, `0x00777244` net table, byte slots | `sub_003559A0`, `sub_003576B0`, `sub_00357320`, `sub_003580D0`, `sub_00359910`, `sub_0035BEF0`, `sub_00068830` team setup | gated (System Link never wide) |
| Player structs `[0x007B0C48] + id*0x1584` (16) | indexed by ids < humans (`sub_00259930` and others, CONFIRMED); `sub_002F1C80` @`0x002F1F35` behind `[obj+0x150] != 0` (INFERRED human controller) | none |
| Radar `sub_00133AB0` / blips `sub_001339E0` | loop stopped at 52 characters (`0x00133BA0`); blip buffer `0x007A5DC0` (56) with an unbounded counter | 64 characters; 256-blip buffer at `0x00902100`, bounded by the blip hook |
| Patrol table `0x00520C38` (`id*10 + step`, initialised .data, routes for ids 0..4) | `sub_002B2750` @`0x002B3B38`, `0x002B3B75`, `0x002B3C7D`, `0x002B3C94`, `0x002B3CBB` | read from a copy: rows 0..31 stock, ids 32..63 "no route" (the code's fallback: player 1's pad) |
| Per-opponent kill words actor `+0xA10` (32) | `sub_000644D0`, `sub_000640C0`, `sub_00064650`, `sub_000DAC30` | side table for victims 32..63 |
| Other constants | `sub_00228D90` wave cap 12, `sub_0025F0C0` 32 checkpoints, `sub_001E6F00` slots 0/1, `sub_00245CE0` pickup types | not character limits |

No character id is stored in a field narrower than a byte; the network
byte slots are System Link only. Per-character data otherwise lives inside
the 0xEA0 actor entry, which grows with the pool.

## Shared files changed

- `config/guest-code-patches/objects.json`: the five `obj-perception-*`
  sizes 0xA00 -> 0xC00 (64 characters + 128 objects), and the matching line
  in `tests/test_mapmaker_objects.py`.
- `src/pc_display_menu.c` (`pc_network_bots_revive`): host code wrote the AI
  table at `0x007A2D00`; it now writes `0x00900200`. No other host code
  (src/, the title's manual C) uses the live list, the AI table or
  0x00739AA0 by address (searched). `tests/pickup_collision.c` sets the old
  list address but translates the unpatched XBE, so it is unaffected.
- `tests/mapmaker_area_stubs.h`: stubs for the new guest calls.

## Memory

- Fixed (this area's sub-range): `0x00900000` gate, `0x009000FC..0x009001FF`
  live list (incl. the `list[-1]` word), `0x00900200..0x009002FF` AI table,
  `0x00900300..0x009005FF` table 0x00739AA0, `0x00900600..0x009015FF` kill
  side table (4 KB), `0x00901600..0x00902027` patrol copy (65 rows x 40 B),
  `0x00902100..0x009030FF` radar blips (256 x 16 B). About 12.3 KB of the
  128 KB; the old .bss tables are left unused.
- Level heap: actor pool +119,808 B (scene 100 only); pair table +18,240 B
  (story-mode levels, every scene); perception +0x200 B per player (every
  scene). Guest stack: 0x100 B transient in the pick hooks.

## Cost per frame at 64 characters (INFERRED, not measured)

- Linear: about twice the AI, animation and drawing of a 32-character map.
- Pairwise (each AI scans the live list): `sub_0015D870` (pair awareness),
  `sub_00163E10`, the pick functions and `sub_0027BA40` loop over up to 64
  entries per AI call: about 60 x 64 = 3,840 cheap iterations per such
  function per frame (tens of instructions each), on the order of 0.1-0.5 ms
  on a slow PC. No time-slicing was added: there is no evidence it is
  needed, and slicing target choice would change AI behaviour.
- Player perception `sub_00259930` keeps a cursor across frames and stops
  after the first visibility raycast, so its per-frame work is bounded
  (players x (characters + objects) cheap steps, at most a few raycasts).
  `sub_000CF8D0` is linear per player.
- Collision between characters was not profiled.
- `TSFP_MAPMAKER_ALIVE=32` (or 24, 20) caps the load on a slow PC.

## Open risks

- Played once (build 06aed2de, first round): MapMaker story with many AI and
  team deathmatch ran well; the kill-word guard fired for victims 53/55/57,
  which the side table now records. The second-round changes are not run.
- Per-character arrays reached only through other pointers (sound voices,
  animation caches): two searches found none. (1) Global tables of 29-34
  entries indexed by a register: only 0x00739AA0 is per-character (moved).
  (2) Every `mov r,[x+8]` followed by a scaled use of `r` against a global
  (34 patterns): all are other structures' fields or stack arguments, none a
  character id. Sound and animation state lives in the 0xEA0 actor entry and
  the character's engine object (objects area pool), both sized per
  character. Not proven absent for code reached only through function
  pointers.
- AI ids >= 32 have no scripted patrol route (none exists for ids >= 5 in
  stock either); with behaviour flag 0x1000000 they follow player 1.
- The enemy picker re-rolls the RNG test of pass A's winner (flag-0x10
  characters only) in pass B.
- Without the hooks (non-display builds, `TSFP_MAPMAKER_AREA_OFF=actors`)
  the patrol copy is all zero and the blip buffer unbounded, in addition to
  the first-round caveats (slot cap 63, gate 0).
- More AI means more engine objects and Level heap (the objects area's
  1000-object pool); not measured.
- `objects.json` and `tests/test_mapmaker_objects.py` were changed (perception
  slice 0xA00 -> 0xC00); the objects owner should confirm.

## Validation

- `tests/test_mapmaker_actors.py`: the fixture plus 71 mutations, each
  detected; checks that `actors.json`, `actors-list.json` and `objects.json`
  match the C constants and that every moved reference keeps its offset. The
  enemy picker is checked against a model of `sub_0027BA40` (32-entry array
  overflow flagged, argmin, first on ties) over 24 random lists of 42-64
  characters: the hook's choice equals the unbounded argmin every time.
- `logs/limits/check-tables.py` on the whole folder prints "table verified".
- No build and no game run.

**Test in play** (MapMaker story map with 70+ placed AI; needs the editor
area's raised AI limit), watching stderr for `[mapmaker]` lines:

- one player: the count `[0x0074C37C]` reaches 64, `[0x007A2958]` 63; the
  live list is at `0x00900100`;
- four players: 60 AI;
- AI fighting, targeting, kills and scores with 40+ AI on one team and on two;
  no `kill of character >= 32` line any more (only in arcade sub-modes);
- the radar with 60 AI near the player: blips for all of them;
- the player's auto-aim/targeting on AI with high ids (perception layout);
- a story level, an arcade MapMaker map and System Link (stock behaviour);
- frame time on the low-spec PC with 60 AI in view, then
  `TSFP_MAPMAKER_ALIVE=32`.

## Decoder classification (2026-10-30)

`tests/test_mapmaker_actors_classified.py` (lifter's decoder) re-checks the hand inventory: in the ~304 character functions every cmp/mov/push/add/sub/imul/lea immediate 0x1D..0x21 is patched, hooked (the pool formula) or reviewed (32 sites), and every reference into the live list or AI table is moved (189 = 181 + 8, plus the kept `0x002354E8`). No missed site. Two bounded 30-entry lists were noted: `0x007A2880` (`sub_001609F0`, add/remove only, nothing iterates it) and the effect list `0x0075F670` (`sub_002B24E0` refuses past 30): memory-safe stock limits, not per-character tables.
