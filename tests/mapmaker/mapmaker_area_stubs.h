/* Link stubs for fixtures that compile the whole guest include chain
 * (pc_display_menu.c / pc_menu_mouse_guest.inc), e.g. menu_mouse.c and
 * fov_settings.c: game functions and runtime helpers that the MapMaker
 * capacity areas (src/pc_mapmaker_<area>.inc) call. Each area's own fixture
 * tests their behaviour. */
#ifndef TSFP_MAPMAKER_AREA_STUBS_H
#define TSFP_MAPMAKER_AREA_STUBS_H
#include <stdint.h>
void sub_000827B0(void) {} void sub_000827F0(void) {} void sub_0035E960(void) {}
void sub_00025280(void) {} void sub_00336600(void) {}
uint32_t xbox_ContiguousAlloc(uint32_t size, uint32_t alignment) { (void)size; (void)alignment; return 0; }
/* memory, probe (host allocation counters) */
int xbox_ContiguousFree(uint32_t address) { (void)address; return 0; }
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0; }
void pc_capture_allocation_usage(uint32_t out[6]) { for (unsigned i = 0; i < 6; i++) out[i] = 0; }
/* objects */
void sub_0009A2C0(void) {}
void sub_0009A5B0(void) {}
void sub_0009CC40(void) {}
void sub_0009CCA0(void) {}
void sub_000A5AF0(void) {}
void sub_000AEAC0(void) {}
void sub_000B05B0(void) {}
void sub_000B3BE0(void) {}
void sub_000C3540(void) {}
void sub_001806B0(void) {}
void sub_00242BA0(void) {}
void sub_00248970(void) {}
void sub_002FDF50(void) {}
void sub_00333700(void) {}
/* actors */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_001CD040(void) {}
#endif
void sub_001638F0(void) {}
void sub_003533F0(void) {}
/* editor, rooms */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_0003D7B0(void) {}
#endif
void sub_0007A270(void) {}
void sub_00157440(void) {}
void sub_00300670(void) {}
void sub_0031BB60(void) {}
void sub_003C85D4(void) {}
/* aitemplates */
void sub_0031BE70(void) {}
void sub_00079D60(void) {}
/* bosses */
void sub_00139740(void) {}
void sub_00071800(void) {}
void sub_0013A400(void) {}
/* realbosses */
void sub_0005A130(void) {}
void sub_00062260(void) {}
void sub_00071630(void) {}
void sub_00071C70(void) {}
/* sky (sub_00078560 becomes a wrapper dispatcher) */
void sub_00078560(void) {}
void sub_0007D4F0(void) {}
/* sets (pc_mapmaker_sets.inc: slot screen, Story AI choice, preload) */
void sub_00071970(void) {}
void sub_00074670(void) {}
void sub_002BE650(void) {}
void sub_002BFFD0(void) {}
void sub_000D7780(void) {}
void sub_002C0310(void) {}
#endif
