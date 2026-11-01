# MapMaker Khallos in the flying chair (story mission 4 boss)

Status: implemented in `src/interface/pc_mapmaker_khallos.inc` (area
`khallos`), fixture-tested (`tests/mapmaker/test_mapmaker_khallos.py`: 178
mutations, plus decode, US XBE data and game-pak checks of every site below).
Wired in v0.4.0-mapmaker.25 (picker row 156); **up to four chairs per map**
since then ([Several chairs](#several-chairs)), with three new wrappers and
more logging ([Not appearing in .25](#not-appearing-in-v040-mapmaker25)). No guest-code table,
no new save bytes. Switch: `TSFP_MAPMAKER_LIMITS=1`; `TSFP_MAPMAKER_KHALLOS=0`
turns only this boss off (its placeholder then plays as an ordinary bot).

Research: `logs/limits/features/real-bosses.md` section 5 (main checkout,
ignored) rated the port "near impractical" because the flight is relative to
fixed train coordinates. The decode below shows that is only partly true: the
chair's "anchor" is its **live position**, integrated every frame; only a few
constants are train-absolute, and each can be moved to the spawn. The full
stock flight runs, around the spawn. CONFIRMED = seen in the decode, the US XBE
data or the game paks and checked by the test; INFERRED = reasoning.

## What the boss is

| | value |
|---|---|
| chair | a **prop**: model 0x376 `TRAIN_KHALLOSJETPACK`, resource type 0x4000000, path `train/khallosjetpack`, init index 30 -> `sub_001F4E00` case 0x376 (jump table 0x001F596C/0x001F58B4 -> 0x001F5281) (CONFIRMED) |
| model file | only `pak/stream/lv11/prop3/4/6/7.pak`; not `chr.pak`, `gun.pak`, `l_100.pak`. Only prop1-4 have XBE path strings; this area mounts prop3.pak (string 0x0049E1D0, 694 KB) (CONFIRMED). Textures: not checked separately (INFERRED: inside the .xbr, as for the tank) |
| scene-100 row | 0x376 is a row of `prop_all_xbas` (the table MapMaker play loads) (CONFIRMED) |
| init | `sub_001EEA30` (esi = chair): zeroes `0x0078F000..0x0078F06F`, `[0x0078F000]` = chair, live position `[0x0078F004..0C]` = (-576.2, 7.5, 0), active flag `[0x0078F04C]` = 0, jet emitter `[0x0078F054]`, health 60 (+0x1CC/+0x1D8); update `sub_001EEBD0` (slot 3), damage `sub_001EF840` (CONFIRMED) -> kept per chair by this area ([Several chairs](#several-chairs)) |
| other variant | when `[[0x007B0C7C]+0x1568]` is 0xE or 0xF the init takes the train's "rising away" variant (`sub_001EFBA0`), which never sets `[0x0078F000]` at creation (CONFIRMED). Meaning of +0x1568 unknown |
| seated Khallos | a **separate story AI of class 8** (AI +0x14 = 8, +0x10 = 0: `sub_0027D590` 0x0027D5B4 -> `sub_00263260`): plays `khallosidle` (0xBFB) and copies the chair's live position + its rotated seat offset (chair +0x274) and angles every frame (CONFIRMED). The research's open question "is the rider part of the mesh" is answered: no rider in the mesh is needed, the train seats an AI (INFERRED from the handler) |
| rider anim | `khallosidle` 0xBFB is named only by `pak/stream/lv11/anim/lv11.pak` (string 0x0049D9C8, table 0x00484410, 33 anims 0xB99..0xBFB, 300,416 B), registered by `sub_001CA2F0` at 0x001CA8D8 when mounted; no id collides with a global anim pak (CONFIRMED) |
| scene tests | none in the chair's code (CONFIRMED: only mode/player-count reads) |

The fight (`sub_001EEBD0`, CONFIRMED; meanings INFERRED):

- **Start**: picks the steered player (the second when story mode, two or more
  players and the first one's +0xA8 is larger), waits until its +0xA8 (x)
  passes -535 (`[0x00500088]`), then sets the active flag, the story focus
  (`+0xFF4`) and fires level event 0x29.
- **Flight**: thrust 9.8 +- 3 (random) along the chair's tilt against gravity
  9.8; below y 7 the thrust triples, above y 10 the lift halves. A tilt
  controller steers at (player +0xB4 - 11 (`[0x00500094]`), 10, 0); yaw is
  clamped to 60..120 degrees (facing +x +- 30). The live position is
  integrated and copied to the chair (+0x64).
- **Fire**: every 1.0-2.5 s from two gun nodes at the player (+0xB4..+0xBC),
  muzzle flash `sub_000DFFA0`, projectile type 0x35 `sub_000EA9C0`, sound 0x393.
  Engine loop sound 0x394 every frame.
- **Damage** (`sub_001EF840`, only while active, hit record with a hit point
  +0x28 and direction +0x30): spin from the hit, health -= +0x20, level event
  0x12 at health <= 35, 0x13 at <= 15. At health < 0 the train's death: tag
  'FTWL', three emitters `[0x0078DF50/54/58]` stopped, focus cleared, level
  event 4, particle effects at fixed train points (-593, ..) through
  definitions `0x0078E900`/`0x0078EF60` and emitter `[0x0078E468]`, which only
  the train setup (`sub_001F1260`, `sub_001F5A10`) fills.
- **After death** the update rockets the chair away (thrust x4) for ever; the
  chair is never removed (the train's script takes over, INFERRED).

## Not appearing in v0.4.0-mapmaker.25

Playtest log (`logs/mapmaker25-bossmap.err`, one map per boss): in the chair's
map each level logs `mounted prop3.pak` and `requested the chair model`, then
nothing. That level shows **no AI start of any character**: the realbosses
area logs every AI start while its walker state exists (allocated in every
level, cap 12 per level never reached), and the tank's and the trophy
monster's maps show the same gap, while the walker, Crowe, robot, skeleton,
worm and mothership maps start theirs. So the placeholder AI never reached
`sub_0015B830`: the cause is upstream of this area (shared with the tank and
the trophy monster; reported to the main session). Ruled out: the
mothership's `l_64_ch.pak` prop table (identical to `l_100.pak`'s, checked),
and the character maker's model map (`0x000DAC6F` is relocated by the rooms
area). New log lines to pin it down next time: every placeholder or Khallos
character made by `sub_000DAC30` (with its pool slot and object, or `NOT
made`), every AI start of id 0x33, and each step of the chair (created, anchor
rebased, rider, fight, shot down, removed).

## Several chairs

User (2026-11-01): "All of these one-per-map limits need fixed." The chair's
code keeps its state in globals (CONFIRMED, `test_multi_chair_globals`):

| global | users | here |
|---|---|---|
| `0x0078F000..0x0078F06F` (object, live position, velocity, thrust, active flag, jet emitter, gun toggle, fire timer, the rider's sway +0x60..+0x6C) | the init `sub_001EEA30`, the update, the damage callback, the rising variant, the init switch `sub_001F4E00`, the seated Khallos' handler `sub_00263260` and his pose callback `sub_00263150` | **per chair** |
| `[0x0078DF50..58]` the death's emitters (made by the init through `sub_00378530`) | the init, the damage callback | **per chair** |
| `[0x00500088]`, `[0x00500094]` | the update only (set and restored around each call already) | per call |

The globals of chair k are loaded around every entry into that code and
saved back after, the previous values restored (nests): the creation (the
init writes the new chair's values; a failed creation puts the old ones back),
the update, the damage callback (it reads `[0x0078F000]`, not the object hit;
the dispatcher `sub_000BB490` passes the object hit at record +0, which picks
the chair), and the seated Khallos' handler and pose callback (the chair he
rides). Each chair has its own placeholder (removed on its first frame), its
own rider (the nearest unbound Khallos within 5 m), its own frame (moved to
its own spawn) and its own removal. Cap: four (`KHL_MAX`); a fifth
placeholder stays a soldier (logged).

## Train assumptions and what this area does about them

| train assumption (CONFIRMED site) | in a MapMaker map | this area |
|---|---|---|
| start line: player x passes -535 (`[0x00500088]`, read only by the update) | never passed (or always) | **update**: the constant is set to -FLT_MAX ("not yet") for the call until a living player is within 30 m (x/z) of the chair, then +FLT_MAX ("now"); restored after |
| height band 7..10 and target height 10 (shared constants 10.0/7.0, inline) | the map's floor height | **update**: frame moved by dy = spawn y + 1.0 - 7.5: start 1 m above the spawn floor, band +0.5..+3.5, hover +3.5 |
| target z 0 (inline `0 - z`) | a line through the map's origin | **update**: frame moved by dz = the steered player's z clamped to spawn z +- 12 |
| target x = player x - 11 (`[0x00500094]`, read only by the update) | follows the player anywhere | **update**: the constant is set so the target is "player x - 11" clamped to spawn x +- 12; restored after |
| yaw 60..120 (shared constants) | | kept: the chair stays on the -x side of the player, as on the train, so it faces the player unless clamped by the box |
| start position (-576.2, 7.5, 0) | elsewhere | **ai_start**: live position and chair = spawn + (0, 1.0, 0) |
| events 0x29, 0x12, 0x13, 4 | the map's own logic markers | **event**: dropped |
| the death's particles/emitter need the train setup; fixed train points | null / garbage, crash risk (INFERRED) | **damage**: the killing hit is cut to the remaining health, so stock leaves health at exactly 0 and skips the train death; then health = the real result, the three emitters stopped (stock `sub_0010EB90`, null safe), focus cleared |
| the dead chair is never removed | flies for ever | **update**: removed 25 m above its spawn or after 240 frames, as the rising variant removes its chair (`sub_000A9070`, `sub_0010E2A0` on the jet emitter, `[0x0078F000]` = 0); the rider too |
| bar label text 0xFF1 (`sub_0014CC80` case 0x376) | a MapMaker level text id | **text**: "Khallos" |

**The moved frame.** For each active update call, d = (0, dy, dz): the live
position `[0x0078F004..0C]`, the chair's +0x64..+0x6C and every player's
+0xB8/+0xBC (y/z of the vector the update steers and aims at) are moved by -d
and put back after (players bit for bit). The update's positions are all
"live position + rotated offset" (`sub_00152090` is a 3x3 rotation;
`sub_000A14B0` caches only the rotation at +0x274, CONFIRMED), so the whole
flight is consistent in the moved frame. Its calls out that place things in
the world (the full call list is checked by the test) run unshifted: the
frame is put back for the call, the position argument moved by +d, then all
restored bit for bit:

| call (return address) | position |
|---|---|
| `sub_0010E730` jet flame (0x001EED7A) | arg 2 |
| `sub_000DFFA0` muzzle flash (0x001EF689, 0x001EF6EA) | arg 3 |
| `sub_000EA9C0` projectile (0x001EF75B) | arg 2 |
| `sub_00045820` gunshot sound (0x001EF7B4) | record (arg 1) +0x1C |
| `sub_000468A0` engine loop (0x001EF824) | arg 2 (`sub_000A5A60`'s static chair position 0x00715F98) |

What the player sees (INFERRED from the decode): the chair hovers 1 m above
its spawn until someone comes within 30 m, then rises to about 3.5 m above the
spawn floor and keeps 11 m on the -x side of the player, at the player's z,
inside a 24 x 24 m box around the spawn, bobbing on random thrust, turning to
face the player and firing; hits spin it. At 0 health it shoots upward and
vanishes.

**Not rebased (risk).** The facing limit (60..120 degrees, +x +- 30): a player
on the chair's +x side outside that cone (possible only near the box edge) is
shot at "forward" instead (stock fallback when the aim is more than ~25
degrees off). The chair writes its position directly: it flies through walls
(as on the train). Leave the box (spawn +- 12 m in x and z, up to 4 m above
the floor) open.

## Placement and save format

A map places the chair with a **Story AI whose character is id 0x33** (the
fifth copy of the story-only cutscene soldier `chrs/soldiermodel_talk`, model
0x619 in `chr.pak`; ids 0x2F..0x33 and 0x4B share it; no Arcade row; no other
port uses it; CONFIRMED). Change `KHL_ID` (one define) if the integrator
prefers another id. Optional rider: a **Khallos Story AI (id 2, the Arcade
Khallos, model 0x582 `chrs/trainkhallos`, the train's model)** placed within
5 m (x/z) of the chair's Story AI. The nearest one becomes the rider (class 8)
when lv11.pak is mounted (the map's set holds both 0x33 and 2); every other
Khallos is untouched.

- Save format: **unchanged** (stock custom-set members, stock Story AI chunks).
- Stock or older builds: a soldier bot and a Khallos bot.
- With `TSFP_MAPMAKER_KHALLOS=0`, when the pak or model cannot be loaded, the
  train's other variant runs, or for a fifth placeholder: a soldier bot (logged).
- The Story AI's own settings (health, Boss flag, logic) do not apply to the
  chair: it is a prop with its own health (60) and damage rules, and its defeat
  is not seen by MapMaker logic. The rider is removed with the chair (whether
  that counts as an AI death for MapMaker logic is unknown).

Editor: this area adds **no picker row** (the picker hooks are
`pc_mapmaker_realbosses.inc`'s). Suggested `rbs_extra` row:
`{156u,0x33u,0x52420033u,0x02u,"Khallos (Chair)"}` (row 156, id 0x33, text id
0x52420033, preview stand-in the Arcade Khallos, id 2). Khallos himself is
already an Arcade row.

## Play, step by step

1. **mount** (`sub_00062790`, once per level from `sub_001CDB80`): after the
   original, the last level is forgotten (and the log budget refilled); when
   the map's own set holds 0x33 (or cannot be read) and prop3.pak is not
   mounted, mount it with `sub_000622D0(0x0049E1D0, 0, 1)` (kept open, read on
   demand, like `chr.pak`) when the 64-slot table keeps 8 free.
2. **anims** (`sub_001C8540`, before `sub_001CA2F0` registers the mounted anim
   paks): when the set holds 0x33 and 2, mount lv11.pak with `sub_0005A130`
   (read whole, like the stock level anim paks).
3. **request** (`sub_0015E7C0`, story level init, after the resource table and
   before the load pass): `sub_000A5E40(0x376)`; nothing else requests it.
4. **ai_start** (`sub_0015B830`, after the original): the first 0x33 entry, when
   prop3.pak is mounted, the relocated resource map (`0x008CA000`) has 0x376
   with init `sub_001F4E00`, and the model is loaded or its request holds the
   read file: `sub_000A6F90(0x376, &pos, &rot, 0, 0)` at the placeholder (as the
   tank). If `[0x0078F000]` is not the new chair the other variant ran:
   `sub_000A9070(chair)`, placeholder stays a bot. Else live position and chair
   = spawn + (0, 1, 0). Khallos entries are noted; the nearest within 5 m of the
   spawn becomes the rider: AI +0x10 = 0, +0x14 = 8, next state +0x9D0 = 0
   (the class-8 handler's state 0 sits him down).
5. **update** (`sub_001EEBD0`): first frame removes the placeholder AI
   (`sub_0015DDA0`, after checking the pool entry still holds it; focus
   cleared). Then wake, moved frame, box, removal of the dead chair (above).
6. **damage**, **event**, **text**, and the five world calls: as above.

## Wrappers for the integrator

All in `src/interface/pc_mapmaker_khallos.inc`; every hook runs the original
unchanged when the switch is off, outside scene 100, or for anything not this
level's chair.

| wrapper | function | chaining |
|---|---|---|
| `WRAP sub_00062790 -> pc_mapmaker_khallos_mount` | level pak mounts (no args) | **shared** with `pc_mapmaker_tank_mount`; order free (each mounts its own pak after the original) |
| `WRAP sub_001C8540 -> pc_mapmaker_khallos_anims` | level anim paks (no args) | **shared** with `pc_mapmaker_realbosses_anims`, `pc_mapmaker_robot_anims`, `pc_mapmaker_crowe_anims`; order free (each mounts its own pak after the original) |
| `WRAP sub_0015E7C0 -> pc_mapmaker_khallos_request` | story level init (no args) | **shared** with `pc_mapmaker_tank_request`; order free (works after the original) |
| `WRAP sub_0015B830 -> pc_mapmaker_khallos_ai_start` | AI start (cdecl pool entry) | **shared** with `pc_mapmaker_realbosses_ai_start`, `pc_mapmaker_tank_ai_start`, ...; order free (acts after the original on ids 0x33 and 2 only; realbosses acts on its own ids) |
| `WRAP sub_001EEBD0 -> pc_mapmaker_khallos_update` | chair frame (cdecl object) | new |
| `WRAP sub_001EF840 -> pc_mapmaker_khallos_damage` | chair damage (cdecl hit record) | new |
| `WRAP sub_002274C0 -> pc_mapmaker_khallos_event` | level event (cdecl) | **shared** with the other boss ports; order free (own return addresses only) |
| `WRAP sub_0003D7B0 -> pc_mapmaker_khallos_text` | text lookup (cdecl), hot | **shared**; order free (id 0xFF1 at 0x0014CDA6 only) |
| `WRAP sub_0010E730 -> pc_mapmaker_khallos_flame` | emitter position (cdecl), hot | new; acts only inside the chair's update at 0x001EED7A |
| `WRAP sub_000DFFA0 -> pc_mapmaker_khallos_flash` | muzzle flash (cdecl), hot | new; inside the update at 0x001EF689/0x001EF6EA |
| `WRAP sub_000EA9C0 -> pc_mapmaker_khallos_shot` | projectile (cdecl), hot | new; inside the update at 0x001EF75B |
| `WRAP sub_00045820 -> pc_mapmaker_khallos_sound` | sound (cdecl record), hot | new; inside the update at 0x001EF7B4 |
| `WRAP sub_000468A0 -> pc_mapmaker_khallos_engine` | looping sound (cdecl), hot | new; inside the update at 0x001EF824 |
| `WRAP sub_00263260 -> pc_mapmaker_khallos_rider` | seated Khallos' handler (cdecl entity) | **new (several chairs)**; only for this level's riders |
| `WRAP sub_00263150 -> pc_mapmaker_khallos_rider_pose` | his pose callback (cdecl entity) | **new (several chairs)**; reached through a stored pointer, so the lifted function is wrapped |
| `WRAP sub_000DAC30 -> pc_mapmaker_khallos_create` | character maker (cdecl id, .., pool slot) | **new (log only)**; **shared** with `pc_mapmaker_realbosses_create`, order free |

The five world-call wrappers test one flag first (set only while the chair's
update runs in the moved frame), so they cost one load elsewhere. Host code
also calls two of them through `guest_call`: the realbosses Crow walker's fire
(`sub_000EA9C0`) and the menu sliders' click sound (`sub_00045820`); the flag
is clear then, so they run the original.

Guest functions called: `sub_00062260` (is a pak mounted), `sub_000622D0`
(mount kept open), `sub_0005A130` (mount an anim pak), `sub_000A5E40` (request
a model), `sub_000631C0` (find a model's request), `sub_000A6F90` (create the
chair), `sub_000A9070` (remove an object), `sub_0010E2A0` (free an emitter),
`sub_0010EB90` (stop an emitter), `sub_0015DDA0` (remove an AI). Host:
`xbox_HeapAlloc` (label, 32-byte argument buffer, once per session). The area
stubs (`tests/mapmaker/mapmaker_area_stubs.h`) already have `sub_0005A130`
and `sub_0010EB90`; they need `sub_000A9070` and `sub_0010E2A0` (and the
tank's `sub_000622D0`, `sub_000A6F90`, `sub_000A5E40`, `sub_000631C0` if not
there yet).

Also for the integrator: the picker row above, and the `docs/agent-context`
guide entry for this area.

## Risks (not verified in game)

- Never built or played. The moved frame is fixture-tested against a fake
  update; the real update's other reads (all checked in the decode) are
  inside the moved frame.
- The five world-call hooks put the frame back for their call. If any of
  those callees reads other positions this area does not move back (the
  decode shows none in the update itself), an effect could appear offset.
- Players' +0xB8/+0xBC are moved during the chair's update. Anything else
  reading them during that call (none found) would see moved values.
- prop3.pak (694 KB, kept open) and lv11.pak (300 KB, read whole when a
  rider is possible) add level memory and two mount slots.
- The other variant (player +0x1568 = 0xE/0xF; meaning unknown) would leave a
  soldier bot (logged).
- The rider: class 8 stops all MapMaker bot behaviour (no shooting, no
  movement of its own). Shooting the rider kills him as a normal AI while
  the chair flies on (behaviour of a dead class-8 AI in the chair unknown).
  Removing him with the chair might count as an AI death for MapMaker logic.
- The chair passes through walls; MapMaker rooms smaller than the 24 x 24 m
  box let it fly outside them.
- Collision of shots with the chair on MapMaker floors: stock prop damage
  path (INFERRED unchanged).

## User test steps

1. A map whose custom set holds id 0x33 with one Story AI of it on an open
   floor: in story play a flying chair hovers there (log: `mounted prop3.pak`,
   `requested the chair model`, `Khallos' chair created`, `spawn (...)`,
   `chair placeholder AI removed`).
2. Also add Khallos (id 2) to the set and place him within 5 m of it: he sits
   in the chair (log `mounted lv11.pak`, `Khallos rides the chair`).
3. Walk within 30 m: the fight starts (log `fight started`, `chair level
   event dropped`); the chair rises, stays about 11 m from you, faces you and
   fires; the bottom bar reads "Khallos".
4. Shoot it: hits spin it; map logic markers are unaffected.
5. Bring it to 0: it shoots upward and vanishes, with its rider (log `chair
   shot down`, `flew off and was removed`, `rider removed with the chair`).
6. Two to four placeholders: each becomes a chair flying around its own
   spawn (log `chair N spawn (...)`); shooting one down removes only that one
   (and its rider). A fifth stays a soldier; `TSFP_MAPMAKER_KHALLOS=0` gives
   a soldier; story mission 4 still plays stock.
7. If no chair appears: the log should now say whether the placeholder
   character was made (`chair placeholder character made` / `NOT made`) and
   whether its AI started (`chair placeholder AI starts`). Neither line means
   the map never spawned that Story AI.
