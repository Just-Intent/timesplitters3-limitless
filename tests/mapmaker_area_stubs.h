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
#endif
