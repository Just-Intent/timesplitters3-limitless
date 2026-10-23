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
#endif
