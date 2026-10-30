# MapMaker actors area: how many characters can be alive at once

US Xbox build 45410066. This area decides how many characters (players and
AI) a MapMaker map can have alive at the same time, and makes MapMaker play
(scene 100) use all of it. Everything here comes from static analysis of the
disassembly. Nothing has been checked in play yet.

- Background: `logs/limits/ai-logic.md` §"Spawn runtime" and
  `logs/limits/turrets.md` (both ignored).
- Code: `config/guest-code-patches/actors.json` and
  `src/pc_mapmaker_actors.inc`.
- Tests: `tests/mapmaker_actors.c` and `tests/test_mapmaker_actors.py`.

**CONFIRMED** means the instruction was seen at the given address;
**INFERRED** means it was reasoned, not seen.

## Result

| Limit | Stock | Now | Scenes |
| --- | --- | --- | --- |
| Characters alive before the spawner holds AI back | 20 (about 19 AI with one player) | 32 (default; `TSFP_MAPMAKER_ALIVE=1..32`) | 100, local play only |
| AI slots | 30 | 30 (unchanged) | all |
| Actor pool | 32 slots | 32 (unchanged) | all |
| Simultaneous AI in a MapMaker map | about 19-20 | **30** (the AI slot cap; 30 + 2 players = 32) | 100 |

**Target reached: 30 AI alive at once (1.5x), not 64.** 30 AI plus up to two
players fill the engine's 32-character design exactly. Going past 32 needs
the work listed under "What blocks more than 32".

Placed story AI past 30 are not lost. Their spawn records wait in the
record list. They spawn when slots free up (when AI die), as stock records
did past 20.

## How it works

The per-frame spawner `sub_00164010` walks the spawn records. Two
comparisons hold AI back (CONFIRMED):

- `0x001640EC cmp dword [0x0074C37C], 0x14 / jl`: in every scene, once 20
  characters are alive, only records with flag `0x20000` continue.
- `0x001641A2 cmp dword [0x0074C37C], 0x14 / jle`, after
  `cmp [0x0079094C], 0x64 / jne`: in scene 100, nothing spawns past 20.

`[0x0074C37C]` is the count of the live character list `0x007A3580`
(humans included). Its only writers are `sub_00158D90`, `sub_0015BCA0`,
`sub_0015BCC0`, `sub_0015C3D0`, `sub_0015C560`, `sub_0015C700` and
`sub_0015F8C0` (CONFIRMED).

Both comparisons now read a gate dword at `0x00900000` instead
(`actors.json`, displacement only; the `0x14` stays). The hook on
`sub_00163F10` sets the gate. That function is the per-record check that
`sub_00164010` calls right before `0x001640EC` (its only caller,
`0x001640DF`). The hook runs the original, keeps its result, then writes:

- `count - (limit - 20)` in scene 100 when no network session is active
  (`[0x0076B108] == 0`, the flag `sub_003533F0` returns);
- `count` everywhere else. This includes story levels, the editor and a
  System Link session in scene 100, so behaviour there stays exactly stock.

So in a local MapMaker map, "20" means "the limit" at both sites. The gate
is exact at `0x001640EC`: nothing runs between the write and the compare.
For `0x001641A2`, the direct-call closure of the calls in between
(`sub_0006AF90`, `sub_0009CCA0`, `sub_0009A510`, `sub_0025F2A0`,
`sub_0022DCC0`: 31 functions) reaches no count writer (CONFIRMED). That
closure has one indirect call, a BSP-query callback in `sub_001571C0`;
that it does not change the count is INFERRED.

The default limit, 32, is the pool size. The 30 AI slots
(`cmp ebx,0x1E` @`0x0015DF7D`) are therefore what stops spawning: at most
30 AI plus the players. `TSFP_MAPMAKER_ALIVE` lowers the limit for slow
PCs (20 = stock). Out-of-range values are ignored.

### The slot guard

The stock code stays inside its tables only because of the 20 throttle.
AI take actor slots from `humans` upwards (`sub_0015DF20`: the first free
slot in `[humans, humans+used)`, else `humans+used`, with
`used < 30`; CONFIRMED). Two tables are smaller than `humans + 30`:

- **Actor pool** `[0x007356D8]`: 32 x 0xEA0 B (`sub_000D6960`; 4 in
  scene 101). With 3 or 4 humans, 30 AI end at slot 32-33, past the pool.
  The network receiver `sub_003580D0` also indexes the pool by a byte with
  no bound.
- **Pair table** `[0x0074C368]`: one 12-byte entry per character pair,
  index `max*(max-1)/2 + min` (`sub_0015BD00`/`sub_0015BD30`). It is sized
  in `sub_0015E7C0` and cleared in `sub_0015E4F0` and `sub_0015E090`: 32
  characters in story mode (byte `[0x007DE455] == 10`), otherwise
  `humans + 10`. `sub_0015E090` clears the new id's row without a bound.

The hook on `sub_0015DF20` (scene 100 only) predicts the slot the original
will take. When that slot is at or past `min(32, pair capacity)`, the hook
returns -1 without running the original. This is the original's own refusal
when its 30 slots are used, and the only caller `sub_00164010` skips it
(`cmp ebx,-1; je` @`0x00164231`). The hook counts the drop and logs the
first three (`[mapmaker] actor pool full: AI not spawned (slot S of N)`).
A refused record retries on later frames, as stock does at 30.

### Hooks the integrator must add (`recomp_manual.c`)

Include `pc_mapmaker_actors.inc` after `pc_mapmaker_limits.inc` in
`src/pc_menu_mouse_guest.inc`. For non-display builds add `{ original(); }`
stubs.

    WRAP sub_00163F10 -> pc_mapmaker_actors_spawn_gate
    WRAP sub_0015DF20 -> pc_mapmaker_actors_slot

Both are cdecl with a plain `ret` (`0x00163FF7`/`0x00164004`,
`0x0015E004`/`0x0015E085`; CONFIRMED). `sub_00163F10` takes the record in
`edx`; the hook runs the original before touching anything.
**`sub_00163F10` must be wrapped wherever `actors.json` is applied.**
Without the hook the gate stays 0, so the throttle would be off in every
scene. The fixture checks the hook and table together.

## Inventory: what is sized by the character count

Character ids are actor slots, 0..31 (`[char+8]`, returned by
`sub_0015DF20`; CONFIRMED).

| Structure | Size | Indexed by | Status |
| --- | --- | --- | --- |
| Actor pool `[0x007356D8]` | 32 x 0xEA0 (heap) | slot | CONFIRMED `sub_000D6960` (`add edx,0x20` @`0x000D6977`, `add ecx,0x20` @`0x000D6A28`). 141 functions use the 0xEA0 stride |
| Live list `0x007A3580` | 32 dwords (.bss, zeroed by `rep stosd 0x20` @`0x0015C577`) | list index; count `[0x0074C37C]` | CONFIRMED. 174 references in 131 functions, plus fixed entries `0x007A3584/88/8C/C0/C4`. Appended with no bound (`sub_0015BCA0`, `sub_0015C3D0`) |
| AI table `0x007A2D00` | 30 dwords | slot - humans | CONFIRMED (`0x0015DFE9`). `0x007A2D80` onwards is other data |
| AI slots in use `[0x007A2958]` | cap 30 | - | CONFIRMED `0x0015DF7D` |
| Pair table `[0x0074C368]` | N(N-1)/2 x 12 B, N = 32 or humans+10 | pair of slots | CONFIRMED `sub_0015E7C0` (`mov ecx,0x20` @`0x0015E7D2`), `sub_0015E4F0` @`0x0015E75F`, `sub_0015E090` @`0x0015E0E8` |
| Perception array, player+0xF74 | 32 x 16 B characters, then objects from +0x200 | id << 4 | CONFIRMED `sub_000CF8D0` (@`0x000CF9E6`, objects at `mov edi,0x200` @`0x000CFA31`), `sub_000D0730` @`0x000D07E7`. The objects area grew the objects part to 128 (0xA00 B); the 32 characters stay at 0..0x1FF |
| Per-character 32-bit masks | 1 bit per id (`shl reg,cl`) | id | CONFIRMED: `[0x007A2BC8]` (`sub_000B3630`, `sub_000BA310`, `sub_000D50E0`, `sub_002B4C70`, `sub_002B55D0`, `sub_002B7750`, `sub_002284D0`; modes 5 and 12), `sub_00064810` and `sub_00067F00` (scores). An id >= 32 aliases bit id-32 (x86 masks the shift count) |
| Stack arrays of characters | `sub_002284D0`: 2 x 32 dwords (frame 0x138); `sub_00163A80`..`sub_00163D90`: 30 dwords (frame 0x80); `sub_002332F0`, `sub_00272450`, `sub_0027BA40`, `sub_0027E6C0`, `sub_002B0770`, `sub_0037AE20` | loop over count/pool | CONFIRMED for `sub_002284D0` and `sub_00163B40` (30 entries before the return address; same group, self excluded, so at most 29 with 30 AI). Others INFERRED, not sized |
| AI bookkeeping lists | `0x007A2960` 55 x 8 B (cap 0x37 @`0x00160825`), `0x007A2C60`/`0x007A3620` 20 x 8 B, `0x007A36E0` 25 x 8 B, `0x007A2880` 30 dwords | count | CONFIRMED sizes. Per-event, not per-character. `sub_001609F0` stores before its `cmp 0x1E` bound (one dword past the end, `0x007A28F8`, never referenced): a stock quirk |
| Arcade score tables `0x007DE4C0`, `0x007DE660`, `0x007E03C0` | 16 entries | player | INFERRED: arcade (humans + bots <= 16) |
| Turret gun/owner pools | 32 -> 80 | turret | objects area; not characters (turrets take no actor slot; CONFIRMED in turrets.md) |
| System Link character sync | `sub_00357320` (`cmp ebp,0x20`), `sub_003580D0` (byte slot, no bound) | slot | CONFIRMED loops/indexing. Network bots: humans + bots <= 16 (`src/pc_network_bots_guest.inc`) |
| Radar table `0x007A38A0` | sine/cosine floats | angle | CONFIRMED not per-character |
| Save/replay | - | - | No per-character table found in the map container (AI are spawn records, `ai-logic.md`); INFERRED |
| Other spawner `0x00228DF0 cmp [0x0074C37C],0xC` | - | - | Not MapMaker story (INFERRED); unchanged |

## What blocks more than 32 (precisely)

All of these would have to change together. Any one left would corrupt
memory or misbehave:

1. **The actor pool**: two `add 0x20` sites, plus the scene-101 mask
   `and 0xE4`.
2. **The live list `0x007A3580`**: relocation of about 180 references,
   plus a bound on its unbounded appenders.
3. **The pair table formula**: three `0x20` sites.
4. **The perception layout**: the object part starts at a hard-coded
   +0x200 (`0x000CFA31`, `0x000D07E7`, and the objects area's five 0xA00
   sites). That would need moving to +0x400 for 64 characters, in step
   with `objects.json`.
5. **The 32-bit masks**: they would need widening. In story/MapMaker they
   are mostly arcade-mode gated (INFERRED).
6. **Every 30/32-entry stack array** of characters: at least 8 functions
   to audit and widen. The 30-entry ones overflow into the return address.
7. **The AI table 0x007A2D00[30] and the 30-slot cap**.
8. **System Link** byte/32-slot paths (keep local-only).

About 250 functions touch the pool, list or count. Items 2, 5 and 6
cannot be verified without that audit, so no part of a 64-character raise
ships here.

## Cost

- **Memory:** 4 bytes at `0x00900000`. No heap.
- **Per frame:** about 10 more characters' AI, animation and drawing when
  a map places them. Linear in characters: about +50% character cost over
  stock MapMaker.
- **O(N^2) work:**
  - Character-pair awareness (`sub_0015D870`, `sub_001638F0`; pair table)
    grows from about 21^2/2 to 32^2/2 = 512 pairs a frame. Each pair is a
    few dozen instructions, well under 0.1 ms.
  - Player perception (`sub_00259930`) is time-sliced over
    players x (characters + objects).
  - `sub_0015C1E0`, the post-spawn pass when more than 8 characters are
    alive, is a no-op on this build: its predicate `sub_0035DE70` returns
    0 (CONFIRMED).
- On a slow PC, `TSFP_MAPMAKER_ALIVE=24` (or 20) caps the extra load.

## Open risks

- Story AI past 20 in MapMaker take more Level heap for their bodies,
  weapons and objects (each AI is engine objects from the 1000-slot pool
  of the objects area). Not measured (INFERRED small next to 11-14 MB
  free).
- Perception/targeting with 30 AI and many objects has not been profiled.
- The gate at `0x001641A2` relies on the INFERRED behaviour of one indirect
  callback (see above).

## Validation

- `tests/test_mapmaker_actors.py`: the fixture plus 20 mutations, each
  detected, and a check that `actors.json` matches the C constants.
- `logs/limits/check-tables.py` on the whole folder prints "table verified".
- No build and no game run.

**Test in play:** a MapMaker story map with 40+ placed AI (needs the
editor area's SAI raise), one player:

- 30 AI should be alive together: watch `[0x0074C37C]` reach 31 and
  `[0x007A2958]` reach 30;
- then two players;
- a story level with many AI (unchanged behaviour);
- `TSFP_MAPMAKER_ALIVE=20` (stock);
- frame time on the low-spec PC with 30 AI in view;
- stderr: no `actor pool full` line with one or two players.
