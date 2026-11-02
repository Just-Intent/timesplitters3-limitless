# Mounted-gun ownership (#385)

`src/input/pc_mounted.h` follows the player entity's `+0xC14` gun link in
mounted mode `0x80`. The US `B76B0` mount and `B7830` dismount, plus CCTV entries
`373E10`/`3756E0`, write/clear the operator at gun data `+0xD8`. The helper checks
that reciprocal link and the gun entity backpointer before changing aim.

Gun data `+0x68` is an AI target (`242F60`/`245CE0`), not the operator. Data `+4`
is an authored instance name copied by constructor `245990`; `AGUN` is one name,
not a type discriminator. The previous helper incorrectly required both that
name and the AI target to match the player. Do not restore those restrictions.

The guarded readable extent is `0xDC`, covering the operator before any write;
only the original angle prefix needs write access. Mode, reciprocal links,
locked states, pause, finite values and authored limits remain guarded. Windows
and SDL share the helper. Ordinary vehicle occupants use mode `0x400`
(`17CE00`/`17CF30` -> `D2CE0`), not `0x8000`; `0x8000` dispatches
separately through `37AA70`. Mode `0x400` gunner seats are now handled by the same shared helper;
`0x8000` has the Battle Mech route described below.

`tests/gameplay/test_mounted_mouse.py` runs asset-free ownership/cadence fixtures,
both adapter bodies with controlled memory maps, and an optional supported-XBE
test executing the original `B76B0`/`B7830` ownership writes and cleanup. No
generated guest code is checked in. The input self-tests use the corrected
operator field too. These checks do not establish live mission or physical Mac
input acceptance; see STATUS and MAC-001.

`mounted_guest_binding.c` passes the original routines' `C14`/`D8` links to the
actual mouse helper. Lookup, player-mode and weapon boundary services are
controlled; ownership writes/cleanup execute unchanged. The optional test
covers four names, independent AI targets, repeated mounting and Original60/
native cadence; it skips without local game data. The adapter fixtures reject
target-only authorization, a different operator, broken backpointers and an
unreadable operator tail. Runtime CI already selects the mounted suite.

`python -m pytest tests/gameplay/test_mounted_mouse.py tests/input/test_terminal_mouse.py -q`
compiles guarded host-memory C fixtures on Windows/macOS. Mounted tests cover
operator/local port, single-consumption relative motion, gun-specific
pitch sign, FOV/inversion/speed, authored clamps/fixed axes/full-circle wrap,
paused/stale/focus rejection and returning to ordinary look. Runtime CI includes
both tests. Physical turret aiming/firing and native host delivery remain
separate gameplay checks; see #182 in STATUS and MAC-001.
`native_mounted_mouse.c` checks 60/120/144 with 125/1000/8000 Hz packets,
jitter, quarter speed, fresh/integrated displacement, pause/invalid clocks,
handoff and untouched player angles. Old-clock/batch mutations must fail.
These controlled-field tests do not certify live turret rendering/input.
Consult STATUS for current build hashes, not historical candidate paths.

Live acceptance distinguishes control modes. The Scotland road gun above the
coastal ruins was observed using mode `0x40`: ordinary player angles drove its
mounted view and gun angles, and mouse replay, firing and dismount passed on
merged source `3b4e679b`. That is not evidence for the repaired `0x80` route or
the second Machine Wars tank. See STATUS for the bounded run and remaining gaps.
`mouse_region.c` checks actual Windows/SDL mouse bodies with controlled maps:
one query for either address order, forward page spans, split/read-only/inaccessible
FOV maps, expiry, terminal routing and bounds. Persistent/uncached/unordered
mutations must fail. `camera_region_host.c` validates real OS protection changes
on Windows/macOS. Mocked locks/events are not physical input. `auto_look_region.c`
checks the pre-guest auto-look scope: one same-map query, split/read-only maps,
expiry and bit restoration. Its mutations run in registered `test_auto_look.py`.
`tests/input/pc_controls.c` and the SDL `--input-self-test` now exercise mounted
mode through the actual adapters, changing the native rate without recreating
input capture. The focused Windows `tsfp_pc_controls_test` target passes with
the isolated fixture config and scratch directory. SDL physical-host execution
is still pending. The portable packet fixture explicitly includes `<stddef.h>`
for `ptrdiff_t`; Mac Clang does not supply it through `<stdlib.h>` as MSVC did.

Vehicle aim investigation (#385, 2026-10-29): `D5C70` dispatches mode `0x400`
to `D2470`. The character's `+0xC08` vehicle handle resolves through the entity
table at `72F5E8` (stride `0x2E8`, handle checked at `+0x2C`). `17A0A0` finds
the occupied seat by matching the character handle against vehicle-data seat
records (`+0x70`, stride `0x34`, count `+0x140`) and returns its type at `+0x30`.
`17A120` returns that record; `17A190` uses its gun index at `+0x2C` to select
the gun-data entry (`vehicle data +0x144`, stride `0x10`, count `+0x164`).
Do not substitute a character name or an assumed `C14` link for this lookup.

Seat types 3/4 read controller axes in `D2470`, build a vehicle input packet,
replace player yaw, and zero player pitch. `17DA00` routes that packet to
`17A440`, which updates gun yaw/pitch from packet fields, then applies gun
limits. The PC hook runs earlier at `D2B80` (`D5EB0` calls it before `D5C70`).
Before the vehicle repair, mode `0x400` fell through to ordinary mouse look,
so those player-angle writes did not reach the gunner's turret. This was reproduced by executing the
original `D2470`, `17A0A0` and `17A440` with controlled service boundaries;
see STATUS. Type 0 has different camera/driver behavior and must not blindly
receive the gunner fix. Exact Scotland truck/Past/Future mission seat types
and Past Cortez's reported inverted vertical direction remain unverified.

The shared helper now resolves gunner seat types 3/4 on every update, validates
the vehicle/seat handles and gun backpointer, and writes the seat's gun angles.
A conflicting D8 operator rejects input; zero D8 is allowed with reciprocal
seat ownership because the original `17CE00` binding does not write D8.
Types 0/1/2 preserve the existing driver/passenger route. Unknown/broken vehicle
bindings consume motion without writing player or gun angles. Seat/gun counts
are bounded to their original inline arrays (4/2). Vehicle states 13/14 are
locked; equal limits are unrestricted and unequal limits clamp (`17A440`),
unlike mode-0x80's equal-limit locks/full-circle wrap. Relative motion remains
single-consumption displacement, including native intervals below one tick.

`mounted_mouse.c` covers seat replacement, malformed bindings, inversion and
60/120/180/240 intervals; `mouse_region.c` exercises both actual host adapter
bodies and inaccessible/read-only gun memory. Optional `vehicle_mouse_guest.c`
executes original D2470/17A0A0/17A440 and gun-camera matrix 151650/152090 with
controlled surrounding services. The old-route mutation must fail. Positive
gun pitch produces a downward direction in that original matrix; this does
not identify which control path the reported Past Cortez scene was using.

Tracked tank driver follow-up: a live first Machine Wars snapshot showed local
seat type 0, vehicle flags 0x1F and gun index 0; the companion occupied type 3.
Do not infer seat type from character name or narrative role. The shared mouse
helper recognizes type 0 only with tracked flag bit 1, uses the driver's negative
-28..-4 camera elevation (mouse-up decreases it), and preserves the original
17A440 driver camera-ray aim and authored gun limits. Gunner types still use
their gun-angle route. Other drivers/passengers retain their existing behavior.

`pc_tank.h` and `pc_tank_guest.inc` wrap original 1730E0 only for the local active
keyboard/mouse driver. Signed left/right track input replaces camera-relative
heading selection: W/S feed both tracks and A/D oppose them, normalized for
diagonals. The synchronous scope temporarily selects the original non-player
track-input branch through actor+150, updates the camera pitch target normally
written by the skipped branch, and restores the link using fresh permissions,
actor handle and player backpointer checks. It does not retain mappings across
the guest call. The original force, wheel and wake/callback code still runs.
Controller input uses the original player branch. The 2080 tank patch registers
the wrapper; generated code is not edited. Vehicle mouse fixtures now execute
original 1730E0, with controlled wake/model services; camera tests separately
exercise original nonzero negative elevation, whose custom ABI only saves ESI.

First-tank user acceptance reported improved driving/aim but reversed A/D.
The signed-track mix now uses `(forward + steer, forward - steer)` to reverse
steering only; forward/reverse and mouse elevation stay as tested. The original
driver fixture checks both steering signs at four camera headings.

## Battle Mech mouse (#425)

Mode 0x8000 dispatches to original US 37AA70, which updates mech target
pitch/yaw at +2C/+30 rather than ordinary player angles. It resolves scene
733108 -> +2AC actor -> +7C extra -> +AA0 mech data. 37A730 assigns the
local input slot at +9C and enables player control at +50; 37B720 copies the
targets into live angles +20/+24 while that control flag is set. The shared
pc_mounted.h route validates those spans, actor type 8, local-slot ownership,
control enablement and writable targets on each call. No AI target or cached
vehicle-seat link authorizes it. Paused/unfocused/stale input is consumed without
movement; exiting restores ordinary mouse routing. Sensitivity, inversion and
FOV scaling match shared input. Signed -80..10 pitch clamps and 0..360 yaw
preserve the original wrapped limits without letting a large mouse packet jump
across the forbidden pitch arc. Original walking, firing and controller code
is unchanged.

The registered test_mounted_mouse.py suite includes mech_mouse.c (60/120/180/
240, limits, focus, pause, stale/malformed ownership and handoff), both production
host adapters, and optional original-XBE 37AA70/3797B0 execution. The old-route
mutation must fail. The original-code fixture proves target survival, direction
vector updates and controller inputs; it is not a live mission or Mac-input test.

Follow-up (user report: mech view still fixed). Static US-XBE review confirms
the route: 37A730 is the only 0x8000 setter; D5EB0 always calls the D2B80 hook
before D5C70 -> 37AA70; 37B720 state 5 keeps +2C/+30 and copies them to
+20/+24 while +50 is set; camera type 0x1F (CCAB0 case CD589) requires a
type-8 target and aims through 3797B0 from +20/+24. [0x733108] is the script
camera (C6150 clears it, CC170 aims it). No failing link was proven offline.
The route no longer rejects an unusable camera FOV (37AA70 has no FOV input;
it now only scales). PcInput.mech_status names the outcome and both adapters
log each change once ("[PC-MOUSE] Battle Mech: ..."), so the next playtest
log identifies a failed link. Live mission behavior remains unverified.

## MapMaker switch cameras (v0.4.0-mapmaker.31 report)

A MapMaker camera (item types 0x35..0x37, switch link 0x18 in `33F0E0`) is
this mode-0x80 actor; the switch mounts it through `3391D0` -> `B76B0`. Its
update `246000` sweeps in state 2 (+58/+5C per tick between +34..+38 and
+3C..+40, speed negated at a limit) without checking +D8. Stock skips the
state machine only on data+8 bit 0, which `3391D0` sets when flag 0x20 is set.
`pc_mapmaker_camera.inc` sets that bit around `246000` in scene 100 while the
reciprocal operator links hold, then restores it. With flag 0x1000000 the
helper scales by |+5C|: the sweep's negated speed no longer drops mouse aim.
`mounted_mouse.c` covers the negative speed; `test_mapmaker_camera.py` the hold.

## Mounted-gun / camera view on a wide display

The mode-0x80 3D view is already Hor+ (`[PC-DISPLAY] View:` aspect 1.768 at
16:9). HUD mode 10 (`0x004E6900` + 10 x 0x1C: update `12E100`, draw `14BFF0`)
frames it from `14C281` in a single view: 3% x 15% inset bars, green tint,
camera-gun tint, scanlines, 0x11B brackets, labels, meter. AUTO placement
boxed these at 4:3. `pc_display_viewfinder` (WRAP `14BFF0`) makes the shared
`1EFB0`/`1F090` wrappers draw them STRETCH with `pc_ui_viewfinder_x` (edges
on their sides, pieces fitted about their third) and labels (`41DC0`) at the
matching edge placement. `pc_display_view_static` (WRAP `F8DC0`) scales the
static tile width (record +0x26) by the HUD scale under STRETCH and restores
the guest random state (`4E7950`, `7497C8`). Only wide, single, full views;
`TSFP_CAMERA_WIDESCREEN=0` restores stock. Logged as `[PC-DISPLAY] Camera
view:`. Test `tests/graphics/test_camera_widescreen.py`; see mapmaker-camera.md.
