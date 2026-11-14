# System Link activation

The original US row callback 1A2700 checks event 8 and tail-calls browser 1A0A60
directly, bypassing the browser's manual lookup. Shared `pc_display_menu_lookup`
now intercepts the row callback before its original body and enables LAN only
on selection. Other events preserve Online; Online's direct browser call retains
its bridge. Test-SystemLink pre-enables networking and cannot prove this path.

`tests/graphics/test_fov_settings.py` runs without TSFP_NETWORK at startup,
selects the actual row callback through the production resolver, and checks LAN
activation before browser entry. Supported local US data additionally supplies
the original six-instruction callback and verifies that removing the row
interception fails. These cases use the existing Windows FOV CI route; Mac
execution and physical two-machine LAN remain distinct checks.

The shared menu emits `[PC-NET] System Link selected; enabling LAN transport`
before entering the browser. Transport startup then reports the chosen IPv4
and compatibility identity, followed by UDP bindings and bounded packet traces.
No network activity in the same-machine script establishes physical LAN discovery.
See [Online UI and host adapters](multiplayer-ui.md) for the separate bridge path.

## Receive bounds (patch 2475)

`network/2475-system-link-packet-bounds.patch` wraps three original System Link
handlers in `recomp_manual.c` through `src/network/pc_net_packet_guard_guest.inc`.
Each wrapper reads the same packet fields as the original and drops the message
the original's own early-exit way (eax 0, plain `ret`) when a field is out of range:

- fragment 352DF0 (eax = packet): length > 0xF0 overruns the 0x100-byte slot
  (data at +0x10); `index*0xEF + length > 0x8000` leaves the reassembly buffer
  [76ADF4];
- turret sync 3596F0: any slot >= the gun free-list total [732D38] (stock 32) or
  player byte (not 0xFF) >= the allocated players [790950] drops the message;
- death effect 357D70: a player >= [790950], or the add path with the stock
  30-entry effect list already full (count [75F6E8], the local adder 2B24E0
  stops at 30). The 31st entry used to overwrite the count.

Well-formed traffic reaches the original unchanged; drops log `[PC-NET]` three
times per kind. `tests/network/test_net_packet_guard.py` compiles the guard with
mutation cases and, with the supported private US executable, checks every cited
instruction through `tools/compiler/translation_decode.py`. No platform adapter
is involved; physical two-machine LAN play remains a separate check.
