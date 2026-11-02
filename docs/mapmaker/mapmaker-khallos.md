# MapMaker Khallos in the flying chair (story mission 4 boss)

Status: implemented in `src/interface/pc_mapmaker_khallos.inc` (area
`khallos`), fixture-tested (`tests/mapmaker/test_mapmaker_khallos.py`: 271
mutations, plus decode, US XBE data and game-pak checks of every site below).
Since the .27 playtest the placeholder itself is the seated Khallos and each
chair gets its own named Boss bar ([Playtest .27](#playtest-v040-mapmaker27-seated-khallos-and-bars)).
Since .28 the chair turns to the player at any bearing, the rider is put on
his seat every flying frame and the chair's gun and engine sounds are loaded
([Playtest .28](#playtest-v040-mapmaker28-orientation-seat-and-sound)).
Since .31 a rider asks for his class 8 too (the engine put chair 0's rider
back to a bot class every frame), a seated rider gets khallosidle again when
another anim plays, and a level restart makes new chairs
([Playtest .31](#playtest-v040-mapmaker31-unseated-riders-and-the-restart)).
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

## Crash in v0.4.0-mapmaker.26: emitter owner slots

Playtest (.26, the first build with several chairs): loading a map with a
single Khallos crashed (crash dump pending). A use-after-free in the
per-chair copies, confirmed in the decode (`test_emitter_owner_slots`):

- `sub_00111AE0` (the emitter maker, through `sub_00378530`) stores the
  address of its owner slot in the new emitter (+0) and the emitter in the
  slot. The chair's jet (`[0x0078F054]`) and first death emitter
  (`[0x0078DF50]`) are such owner slots (init `sub_001EEA30`, 0x001EEB26 /
  0x001EEB7A).
- When an emitter is freed or recycled, at any time and from the particle
  system (`sub_0010E070`, from `sub_0010E110`, `sub_0010E510`, `sub_00111E40`,
  and `sub_0010E2A0`), the engine writes 0 through that address.
- With the chair statics swapped in and out, the stock slot holds the outside
  value (or another chair's) most of the time: the engine clears the wrong
  slot and the chair's own copy keeps a dangling emitter pointer that its next
  update hands to `sub_0010E730`.

Fix: each chair owns two guest slots (one 32-byte allocation per session,
cleared at every level's pak mount). After the chair is made its emitters'
owner fields point at its slots; while the chair's code runs (each swap in)
the value and the owner move to the stock slot and back after. A failed
creation (no object, or the rising variant) detaches the emitters it made.
An emitter the engine frees at any time now clears its own chair's slot.

Not from this pattern: the robot swaps no emitter slots, and the realbosses
skeleton/walker and split Crowe pass emitter owners inside their own heap
state blocks (the blocks are per instance; only their pointers are swapped),
so their .26 crashes need their own look.

## Several chairs

User (2026-11-01): "All of these one-per-map limits need fixed." The chair's
code keeps its state in globals (CONFIRMED, `test_multi_chair_globals`):

| global | users | here |
|---|---|---|
| `0x0078F000..0x0078F06F` (object, live position, velocity, thrust, active flag, jet emitter, gun toggle, fire timer, the rider's sway +0x60..+0x6C) | the init `sub_001EEA30`, the update, the damage callback, the rising variant, the init switch `sub_001F4E00`, the seated Khallos' handler `sub_00263260` and his pose callback `sub_00263150` | **per chair** |
| `[0x0078DF50..58]` the death's emitters (made by the init through `sub_00378530`) | the init, the damage callback | **per chair** |
| owner slots `[0x0078F054]`, `[0x0078DF50]` (emitter +0 points at them) | the particle system clears them on free | **per chair guest slots**, the emitters re-pointed (see the .26 crash) |
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
| yaw 60..120: 90 +- `[0x005000B8]` (30, read by the update only) | the player is anywhere around the chair | **update** (since .28): the half-width is set to 360 for the call and restored, so the yaw follows the player's bearing |
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
prefers another id. Since .27 that Story AI is itself made with Khallos'
models (model 0x582 `chrs/trainkhallos`, the train's) and rides the chair; its
name and Boss setting give the chair's bar. Fallback (it could not be made
as Khallos): a **Khallos Story AI (id 2)** placed within 5 m (x/z) becomes
the rider (class 8) when lv11.pak is mounted; every other Khallos is
untouched.

- Save format: **unchanged** (stock custom-set members, stock Story AI chunks).
- Stock or older builds: a soldier bot and a Khallos bot.
- With `TSFP_MAPMAKER_KHALLOS=0`, when the pak or model cannot be loaded, the
  train's other variant runs, or for a fifth placeholder: a soldier bot (logged).
- The Story AI's Boss flag, name and Boss/health multipliers apply to the
  chair's bar and health (60 x factor) since .27; its own health setting and
  logic do not: the chair keeps its damage rules and its defeat is not seen
  by MapMaker logic. The rider is removed with the chair (whether that counts
  as an AI death for MapMaker logic is unknown).

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
| `WRAP sub_000DAC30 -> pc_mapmaker_khallos_create` | character maker (cdecl id, .., pool slot) | **shared** with `pc_mapmaker_realbosses_create`, order free; since .27 also makes the placeholder with Khallos' models (character table swap around the original) |
| `WRAP sub_00045670 -> pc_mapmaker_khallos_sound_load` | level sound needed? (cdecl sound id -> nonzero) | **new (.28)**; called only by the level's sound load `sub_00027700`, about 5,700 times per level load; acts after the original on 0x393/0x394 only |
| `WRAP sub_0014CC80 -> pc_mapmaker_khallos_hud` | story boss bar (ebx = HUD block) | **new (.27)**; chain **inside** `pc_mapmaker_bosses_hud`, like `pc_mapmaker_robot_hud` / `pc_mapmaker_realbosses_hud` (order among those free) |
| `WRAP sub_001CDD60 -> pc_mapmaker_khallos_restart` | level restart (no args), called only at 0x0003D4E1 | **new (.31)**; works before the original; nothing else wraps it |

`pc_mapmaker_bosses.inc` gains `bos_bar_proxy` (no wrapper; this area sets
it at its mount hook when both are built in).

The five world-call wrappers test one flag first (set only while the chair's
update runs in the moved frame), so they cost one load elsewhere. Host code
also calls two of them through `guest_call`: the realbosses Crow walker's fire
(`sub_000EA9C0`) and the menu sliders' click sound (`sub_00045820`); the flag
is clear then, so they run the original.

Guest functions called: `sub_00062260` (is a pak mounted), `sub_000622D0`
(mount kept open), `sub_0005A130` (mount an anim pak), `sub_000A5E40` (request
a model), `sub_000631C0` (find a model's request), `sub_000A6F90` (create the
chair), `sub_000A9070` (remove an object), `sub_0010E2A0` (free an emitter),
`sub_0010EB90` (stop an emitter), `sub_0015DDA0` (remove an AI), since .31
`sub_0005F460` (play an anim; already in the area stubs). Host:
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
2. Khallos sits in the chair (log `mounted lv11.pak`, `chair placeholder made
   with Khallos' models`, `chair placeholder model (0x582, 0x582)`, `the
   placeholder is Khallos and rides his own chair`); no other Story AI needed.
3. Walk within 30 m: the fight starts (log `fight started`, `chair level
   event dropped`); the chair rises, stays about 11 m from you, faces you and
   fires; with Boss on, the bottom bar reads the Story AI's name (log `stock
   chair bar hidden`). Shooting Khallos himself hurts the chair (log `its
   rider was hit, the chair takes it`).
4. Shoot it: hits spin it; map logic markers are unaffected.
5. Bring it to 0: it shoots upward and vanishes, with its rider (log `chair
   shot down`, `flew off and was removed`, `rider removed with the chair`).
6. Two to four placeholders: each becomes a chair flying around its own
   spawn (log `chair N spawn (...)`), each with Khallos and its own bar (up to
   four in range, nearest first); hits log `chair N hit`; shooting one down
   removes only that one (and its rider). A fifth stays a soldier; `TSFP_MAPMAKER_KHALLOS=0` gives
   a soldier; story mission 4 still plays stock.
7. (.28) Walk around a flying chair: it turns to face you on every side and
   Khallos stays in his seat (log `chair N frame F: yaw .., player at dx ..
   dz ..; seat offset (..)`, seat offset near 0 after the first line). You
   hear its gun and the jet loop (log `the chair's level-11 sound is loaded`
   twice per level; status `gun n (0 failed), engine n (0 not playing)`).
8. (.31) Every Khallos sits in his chair: status lines show rider calls
   rising, `state 1 class 0/8 asked 0/8`, `anim 0xBFB`. Restart the map from
   the pause menu: log `level restart: chair state reset`, then the chairs
   are made again and Khallos rides each (not on foot).
9. If no chair appears: the log should now say whether the placeholder
   character was made (`chair placeholder character made` / `NOT made`) and
   whether its AI started (`chair placeholder AI starts`). Neither line means
   the map never spawned that Story AI.

## Playtest v0.4.0-mapmaker.27: seated Khallos and bars

User: "Single khallos works, but khallos is not actually visible. It's just
his chair floating with the appropriate animations. Still takes damage." and
"3 khallos' didn't seem to take damage, and only 1 boss health bar appeared.
Also, health read khallos instead of the story AI name." Log `logs/m27.err`
(main checkout) lines 13330-16510.

| report | cause | fix |
|---|---|---|
| no Khallos on the chair | **proven** (log): no `mounted lv11.pak`, no Khallos character or AI start, no `rides the chair` line. The rider had to be a separate Khallos Story AI (id 2) in the set and within 5 m; the map had only the chair's Story AI, which was removed on the first frame | the placeholder is made **with Khallos' models** and rides its own chair (below); lv11.pak is mounted whenever the set holds 0x33 |
| one bar, labelled "Khallos" | **proven** (decode): the only bar was the stock one (`sub_0014CC80` draws the focus `[[0x007B0C7C]+0xFF4]`, set by the update to the chair that woke last, label text 0xFF1 = this area's "Khallos"); the bosses area draws Story AI bars, and the placeholder AI was gone | **hud** hides the stock bar of a ridden chair; the bosses area draws the rider's Boss bar with his Story AI name and **the chair's health** (`bos_bar_proxy`) |
| several chairs took no damage | **suspected**, not reproduced: the damage route is per chair (record +0 picks the chair, fixture-tested since .26) and the decode shows no other chair test; with one bar on the last-woken chair (chair 0 in the log, woken after 1 and 2), hits on the others never showed | per-chair bars (above); new log lines `chair N hit (active, damage): health a -> b` (first 4 per chair) and for hits through the rider |

The seated Khallos, step by step:

- **request**: when the set lacks Khallos (id 2), his models (character
  table `0x0053A3CC` + 2x0x30: +4 = 0x582 `chrs/trainkhallos`, in `chr.pak`,
  +8 when not -1) are requested with `sub_000A5E40`, as the set preload
  `sub_000D7780` does for members.
- **create** (`sub_000DAC30`, already wrapped): for id 0x33, when lv11.pak is
  mounted, a chair can be made (prop3.pak, row, init, model read), fewer than
  four chairs exist and both his models are read, id 0x33's +4/+8 are set to
  Khallos' for that call and put back after (`sub_000DAC30` reads the model at
  0x000DAC59 and +8 at 0x000DB084, nothing else; decode-checked). Otherwise
  the soldier model, as before.
- **ai_start**: a placeholder whose entity model word (+0x94) is Khallos' gets
  class 8 (+0x10 = 0, +0x14 = 8, +0x9D0 = 0) and rides its own chair; it is
  not removed. A Khallos Story AI placed nearby then stays a bot. A soldier-
  model placeholder still takes a nearby Khallos as before, or is removed.
- **update**: each frame, after the original, the rider's health is put back
  to its maximum; what he lost goes to the chair while it fights (a kill this
  way is the same "shot down" as a hit on the chair). A one-hit kill of the
  rider (health <= 0) is forwarded and not revived.
- **hud** (`sub_0014CC80`, chained inside `pc_mapmaker_bosses_hud`): a focus
  on a chair whose rider is still its AI is cleared for the stock call and put
  back. The bosses area then draws one bar per Boss-flagged rider in range:
  `bos_bar_proxy` (set by this area at each level's mount) makes the chair the
  bar's entity for health, Boss/health multipliers, range and death; the
  label is the Story AI's saved name. Boss Off: no health shown, as for the
  other ported bosses. Sub-boss bar: the overhead meter shows the rider's own
  (always full) health (not handled).
- **text**: "Khallos" stays for a chair without a rider (fallback).

## Playtest v0.4.0-mapmaker.28: orientation, seat and sound

User (three chairs): "The khallos' didn't really have much audio, they didn't
orient toward me even though they fired accurately at me, and two of them kind
of floated in front of their chairs." Log `logs/m28.err` (main checkout): all
three seated, hits through the riders, bars; no line covered yaw, seat or
sound, so the status lines below were added.

| report | cause | fix |
|---|---|---|
| no turning to the player | **proven** (decode): the yaw target is the bearing to the player, `atan2(dx, dz)` (0x001EEEED, x 57.3) clamped to 90 +- `[0x005000B8]` (30): the train always has the player on the chair's +x side. Shots aim at the player directly (gun nodes, +0xB4..+0xBC), so they hit. The yaw error is wrapped generically (`-180`, `1/360`, `360`), the tilt controller works in the chair's own axes | the active update runs with `[0x005000B8]` = 360 (read by the update alone), restored after: no clamp |
| two riders "in front" of their seat | **not proven**. The handler `sub_00263260` (state 1) puts him at live position + `[0x0049E4BC]` (-1.2) x the chair's up row (+0x284, `sub_00152090` row 1), all per chair (swap list checked, entity arg for both the handler and the pose callback `sub_00012E60`/`sub_000A0CA3`). States other than 0 and 1 and an AI class other than 0/8 skip that placement, leaving him where he was while the chair flies on (suspected: an AI state request or class reset by other code; the handler running late or not every frame) | after each flying frame the area puts a living rider on the seat itself (same formula, the chair's angles copied); a class other than 0/8 is put back, a state request of 2+ (or state 2+) becomes 0 ("sit down"); the measured offset is logged |
| little audio | **proven** (decode + `sound/sounddata` in `sounds_e.pak`): the gun `0x393` SFX_11_KHALLOS_GUN (flags 0x0B00000C) and the engine loop `0x394` SFX_11_JETPACK_NEW (0x0B00041C) carry flag 4 with level byte 11; `sub_00045670` loads such a sound only when the scene is that level, so in scene 100 neither is loaded and `sub_00045820` (via `sub_00027F80`, table `[0x004CA9B4]` +0xC) returns -1. Per chair already: the shot record is a stack local, the loop handle is chair +0x22C | **sound_load** (`sub_00045670`) answers 4 ("load") for 0x393/0x394 in a scene-100 map whose set holds 0x33 (flag set by the mount hook, which runs before `sub_000454F0` in the level setup). Voice: the chair code plays no voice; the train's Cortez/Anya lines come from its level script (events dropped here), Khallos' only own line 0x43C is unused, so none is added |

New log lines, per chair (6 at most, the first flying frame then every 300):
`chair N frame F: yaw Y, player at dx .. dz ..; seat offset (x, y, z), rider
calls C state S class H/L; sounds: gun n (f failed), engine n (m not
playing)`. A large seat offset with few rider calls means the handler did not
run; class or state fixes print `rider's AI class .. put back` / `rider state
.. sits down again`.

Risks: the rider's room id (`sub_0009FBE0`, set by the handler) is not
refreshed by the area's own placement; two more sounds per Khallos map.

## Playtest v0.4.0-mapmaker.31: unseated riders and the restart

User (three chairs): "all khallos AI plus the floating chairs spawning. AI
turned toward me but didn't put in their chairs; they were kind of floating
off of them. When restarting the map, the chairs didn't even spawn, and three
khallos' on the ground just ran toward me." Log `logs/m31.err` (main
checkout): the map's load is lines 3909-8930; it holds **three** runs (AI
starts of 0x33 at 6195, 6975, 7136), because a restart logs no new
`skymusic: new MapMaker level` line. The level at 8931 is another (Crowe) map.

| report | cause | fix |
|---|---|---|
| no chairs after a restart, Khallos on foot | **proven** (decode + log). `sub_0003D440` runs "START restart" (`[0x006B7AA8]` set, string 0x0047817C) as the teardown `sub_001CDAF0` plus `sub_001CDD60`, which never calls `sub_001CDB80`, the "from scratch" setup holding the pak mount (`sub_00062790`, the only place this area forgot its chairs). The restart makes and starts the placeholders again in the same pool entries with the same records (6975: `AI start: character 0x33, model 0x582`), so `ai_start` found "this chair placeholder already has its chair" for each and left it a bot; the create hook still made it with Khallos' models. No khallos line after the first run: the 64-line budget was spent by `the rider was killed by one hit`, logged every frame | **restart** (`sub_001CDD60`, new wrapper): before the original, the chairs, riders, counters, emitter owner slots and the log budget are reset as at a new level; the level's sound flag and the chair model request are kept (the restart mounts and loads nothing). The killed rider is logged once per chair. Each level start and restart logs `new level` / `level restart: chair state reset (N chairs and M riders forgotten; ..)` |
| chair 0's rider: "rider calls 0 state 0" every status line, class `2/0`, later `4000000/0` put back each frame | **proven** (decode): the AI frame calls `sub_0015C700` (0x0015F078) right before the think `sub_0027FE30` (0x0015F07E); it makes the class **asked for** (AI +0xAA8/+0xAAC, written by `sub_00017570` from 221 sites) the class (0x0015C788). The area wrote class 8 at +0x10/+0x14 only, so whatever asked for another class (2, then 0x4000000; which site is not identified, INFERRED a reaction of the bot logic, chair 0 woke first and was hit first) won again on the next think, and the class-8 handler never ran for him; the area's put-back after each flying frame hid it from the status line | **class**: the rider gets class 8 **and** asks for 8 (+0xAA8 = 0, +0xAAC = 8) with state 0; every frame (waiting or flying) a living rider asking for another class is asked back to 8 with state 0, and the engine's own change (`sub_0015C700`, with `sub_00158D40`'s exit flags) seats him. Logged: `rider's AI class .., asked ..: asked back to 0/8` (4 per chair) and `asked .. (asked back n)` in the status line |
| riders 1 and 2 "floating off" although handler state 1 and seat offset < 0.1 | **suspected**, not proven (no anim was logged): state 0 plays khallosidle with `sub_0005F460(ent, 0xBFB, 0, 1.0)` and moves to state 1 whatever it returns; it returns 0 when the anim's data is not loaded (`[[0x007E3954] + slot*4]`) or `sub_001C8C20` refuses it over the anim playing (current anim at entity +4 -> +0xD4). A placeholder starts as a bot (a running or reaction anim may play) where the train's Khallos starts seated; a standing Khallos at the seat position (feet 1.2 m under the live position) would look like "floating off" the chair. The seat point itself is the train's formula for the same prop | **idle**: after each handler call in state 1, a current anim other than 0xBFB gets khallosidle again the handler's way, at once and then every 30 calls while it differs. Logged: `rider anim X is not khallosidle 0xBFB (slot S, data D): played again (played/refused)` (3 per chair) and `anim .. (khallosidle played again n, f failed)` in the status line |

Kept from earlier builds: the free yaw (facing), the level-11 sounds, the
named bars. Not changed: the seat formula (the measured offset was below
0.1 m on every line).

What the next log should show: per chair `chair N frame ..: rider calls`
rising for **every** chair, `state 1 class 0/8 asked 0/8`, and `anim 0xBFB`.
If the anim is still another, the `played again` line says whether it was
refused (`data 0` = khallosidle not loaded; `slot -1` = lv11.pak's names not
registered) or played and then replaced (repeated tries in the count). After
a restart: `level restart: chair state reset (3 chairs ..)` followed by three
`Khallos' chair created` lines.

Risk: asking for class 8 every frame overrides any other class the game asks
for a living rider (none is wanted while he is seated); a dead rider is left
to the engine. The restart wrapper assumes the teardown removed the old
chairs (the user saw none after the restart).

## Not appearing in .25/.26: set slot 13 (fixed in .27)

Khallos' chair sat on set slot 13, whose spawn mode mask never allowed story
mode; see [Spawn mode mask](mapmaker-sets.md).
