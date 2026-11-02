# MapMaker switch cameras

Area `camera` (`src/interface/pc_mapmaker_camera.inc`). Test:
`tests/mapmaker/test_mapmaker_camera.py` (fixture `mapmaker_camera.c`, plus a
decoder check of the original bytes that skips without the private XBE).
Static analysis and fixtures only; not built or played by the agent that
wrote it. Reported in the v0.4.0-mapmaker.31 playtest.

## 1. The camera keeps sweeping while you control it (fixed)

A MapMaker camera is a mounted-gun actor (item types 0x35..0x37). A switch
linked to it (link type 0x18, `sub_0033F0E0`) mounts it with `sub_003391D0` ->
`sub_000B76B0`: player mode 0x80, character extra `+0xC14` = the camera,
camera data `+0xD8` = the character. Mouse aim goes through
`src/input/pc_mounted.h`, the stick through `sub_00258A20`; both write pitch
`+0x1C` and yaw `+0x24`.

Cause (confirmed in the US code): the actor update `sub_00246000` runs its
state machine every tick. State 2 sweeps pitch and yaw by `+0x58`/`+0x5C` per
tick between `+0x34..+0x38` and `+0x3C..+0x40`, negating the speed at a limit;
state 4 eases back to the saved pose; state 3 turns to an AI target. None of
it checks the operator. The only stock way out is data flag `+8` bit 0
(`test cl,1` at `0x002460E5`), and the switch sets that bit on mount only when
flag 0x20 is already set (`0x003391E6`). A camera without 0x20 keeps sweeping
under the player, for the mouse and the controller alike.

Fix: WRAP `sub_00246000` (cdecl, entity at `[esp+4]`, plain ret) ->
`pc_mapmaker_camera_gun_update`, area `camera`, scene 100 only. While the
actor's operator links are reciprocal (data `+0` = entity, `+0xD8` =
character, character extra `+0xC14` = entity, player `+0x14` = character,
mode 0x80) the original runs with bit 0 set, then the bit is cleared again
unless the game had set it. Pose, speeds and limits are untouched, so the
sweep resumes when control ends. Story levels keep the stock behaviour.

Related mouse fix (`pc_mounted.h`): guns with flag 0x1000000 scale aim by
`+0x5C`, which is also the sweep speed and is negative after a bounce; the
helper used to drop all mouse input then. It now uses the magnitude.

## 2. The camera view stays 4:3 on a widescreen display (open)

Not reproduced offline. Checked: the mounted view is the player camera in
camera mode 2 (`sub_000CBCF0` record `0x004DCC90`), whose projection is built
by `sub_000C77C0` from `sub_000CCAB0`; `pc_display_camera` widens that one
for every caller (the log shows factor 1.325967). The remote-view overlay
`sub_00137E30` draws, for camera-type guns (`sub_00242EE0`), a flat viewport
rectangle (`sub_0001EFB0`), which already spans the window. The
`[PC-DISPLAY] View:` line now records, per change, scene, player mode,
camera flags `+0x21C`, viewport, factors, the final projection x/y and FOV.
A 4:3 projection under player mode 0x80 points at the 3D path; a wide
projection with a boxed picture points at a 2D overlay or texture feed.

## 3. A tall stack seen from the bottom floor stops near floor 10 (open)

The log shows every piece built (rooms 255 of 255, nothing dropped, floors
-2..13, no open-air pieces, so the open-air widening is idle). The shared far
plane is 1800 (`0x004DCB0C`) and floor 13 is y 65, so the far clip is not the
limit. In scene 100 the portal walk `sub_00087F80` lets each portal through
twice per walk (the second time with the whole viewport, `0x000881FB`) and
skips a portal flagged for the view (`+0x16` bit `view+0x214`). No depth or
floor cap was found. The `[mapmaker] camera: view ...` line (from the
`sub_00089B90` wrapper, scene 100) prints, per change: view slot, camera
room, visible rooms of the total, the floor range of the visible rooms (room
box y / 5) and the top room, the deepest walk depth, far plane and FOV.
`[mapmaker] camera: actor ...` lines give each used camera's state, flags,
operator, angles, limits and sweep speeds.

## Playtest

Take a camera through its switch: it must stop sweeping, follow only the
mouse/stick, and resume sweeping after release; repeat a second time.
Story CCTV turrets (Breaking and Entering) unchanged. Send the log with the
`[mapmaker] camera:` and `[PC-DISPLAY] View:` lines for 2 and 3.
`TSFP_MAPMAKER_AREA_OFF=camera` restores the stock update.
