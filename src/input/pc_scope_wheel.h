#ifndef TSFP_PC_SCOPE_WHEEL_H
#define TSFP_PC_SCOPE_WHEEL_H
/* Mouse-wheel scope zoom (US Xbox 45410066), shared by both input adapters.
 *
 * Normally the wheel is bound to Next/Previous weapon (Wheel up/down). While
 * player 1 looks through a zoomable scope, a wheel notch instead holds the
 * bound "Weapon mode / zoom in" (wheel up) or "Grenade type / zoom out"
 * (wheel down) key briefly and never reaches weapon switching. Iron sights
 * and ordinary aim keep wheel weapon switching.
 *
 * Guest evidence (sub_0025E650, the per-player aim/zoom update called from
 * sub_000D5EB0, with [0x007B0C7C] = player, [0x007B0CC0] = its camera):
 * - player+0x0C bit 0x20 is aim mode (toggled/held from the aim mask +0x28).
 * - bit 0x40 is set when aim starts only if sub_001B7DF0(player+0x55C) says
 *   the current weapon's sight is a scope (weapon flag 0x20, or 0x40 in
 *   weapon mode 1); sub_00259DF0 or the exit path clear it, and while it is
 *   set the camera gets +0x21C bit 0x80 (scope view).
 * - while aiming, held masks +0x30/+0x34 (D-pad up/down in every control
 *   scheme, sub_002586C0) move the zoom target camera+0x388 by 60 degrees a
 *   second, clamped to [camera+0x394, camera+0x390]. sub_001B7D00 fills that
 *   range from the weapon table; sub_001B7D70 collapses it when the scope is
 *   unavailable, so min < max means the scope can actually zoom.
 * Zoom acts while the button is held, so a notch is a short hold. */
#include "pc_guest_memory.h"
#include "pc_settings.h"
#include <math.h>
#include <stdint.h>

#define PC_SCOPE_WHEEL_UP_KEY 0x86   /* VK_F23, "Wheel up" */
#define PC_SCOPE_WHEEL_DOWN_KEY 0x87 /* VK_F24, "Wheel down" */
#define PC_SCOPE_ACTION_ZOOM_IN 10   /* "Weapon mode / zoom in" (D-pad up) */
#define PC_SCOPE_ACTION_ZOOM_OUT 11  /* "Grenade type / zoom out" (D-pad down) */
#define PC_SCOPE_ACTION_PREVIOUS 12  /* "Previous weapon" (D-pad left) */
#define PC_SCOPE_ACTION_NEXT 13      /* "Next weapon" (D-pad right) */
/* About five 60 Hz frames: roughly 5 degrees of the 26-degree sniper range. */
#define PC_SCOPE_WHEEL_MS 80u
#define PC_SCOPE_WHEEL_MAX_MS 640u
/* The player update publishes the state every frame; older state is stale
 * (menus, cutscenes, loading) and never redirects the wheel. */
#define PC_SCOPE_WHEEL_FRESH_MS 250u

typedef struct PcScopeWheel {
    int scoped;
    unsigned long long seen;
    int route_up, route_down;      /* wheel key bound only to weapon switching */
    int zoom_in_key, zoom_out_key; /* first non-wheel key of each zoom action */
    int dir;
    unsigned long long until;
} PcScopeWheel;

/* -1: not local player 1 (leave the state alone); 0: not in a zoomable
 * scope; 1: player 1 is looking through a scope that can zoom. */
static inline int pc_scope_view(uint32_t player, uint32_t camera, ptrdiff_t offset)
{
    uintptr_t p = (uintptr_t)player + offset, c = (uintptr_t)camera + offset;
    float minimum, maximum;
    if (!player || !pc_guest_memory(p, 0x10, 0)) return -1;
    /* +4 is the local controller index (-1 for AI, 1..3 for other local
     * players); the PC mouse and keyboard drive player 1 only. */
    if (*(const int32_t *)(p + 4) != 0) return -1;
    if ((*(const uint32_t *)(p + 0xc) & 0x60) != 0x60) return 0;
    if (!camera || *(const uint32_t *)(p + 8) != camera || !pc_guest_memory(c + 0x390, 8, 0)) return 0;
    maximum = *(const float *)(c + 0x390);
    minimum = *(const float *)(c + 0x394);
    return isfinite(minimum) && isfinite(maximum) && minimum < maximum;
}

static inline void pc_scope_wheel_reset(PcScopeWheel *w)
{
    w->scoped = 0; w->dir = 0; w->until = 0;
}

/* keys: player 1's key bindings by action (pc_settings_bound_keys). */
static inline void pc_scope_wheel_bindings(PcScopeWheel *w, const int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS])
{
    int route[2] = {0, 0}, other[2] = {0, 0}, zoom[2] = {0, 0};
    for (int a = 0; a < PC_ACTION_COUNT; a++) for (int s = 0; s < PC_BINDING_SLOTS; s++) {
        int key = keys ? keys[a][s] : 0, wheel = key == PC_SCOPE_WHEEL_UP_KEY ? 0 :
                                                 key == PC_SCOPE_WHEEL_DOWN_KEY ? 1 : -1;
        if (!key) continue;
        if (wheel >= 0) {
            if (a == PC_SCOPE_ACTION_PREVIOUS || a == PC_SCOPE_ACTION_NEXT) route[wheel] = 1;
            else other[wheel] = 1;
        } else if (a == PC_SCOPE_ACTION_ZOOM_IN && !zoom[0]) zoom[0] = key;
        else if (a == PC_SCOPE_ACTION_ZOOM_OUT && !zoom[1]) zoom[1] = key;
    }
    /* Respect rebinding: a wheel direction bound to anything else is never
     * hijacked, even if it is also bound to weapon switching. */
    w->route_up = route[0] && !other[0];
    w->route_down = route[1] && !other[1];
    w->zoom_in_key = zoom[0];
    w->zoom_out_key = zoom[1];
}

/* view from pc_scope_view(); a negative view (another player) is ignored. */
static inline void pc_scope_wheel_state(PcScopeWheel *w, int view, unsigned long long now)
{
    if (view < 0) return;
    if (!view) { w->dir = 0; w->until = 0; } /* never press zoom keys outside the scope */
    w->scoped = view > 0;
    w->seen = now;
}

static inline int pc_scope_wheel_live(const PcScopeWheel *w, unsigned long long now)
{
    return w->scoped && now >= w->seen && now - w->seen < PC_SCOPE_WHEEL_FRESH_MS;
}

/* A wheel event of steps notches (positive = up/forward). Returns the steps
 * left for the ordinary wheel bindings; 0 when the notches became zoom. */
static inline int pc_scope_wheel_input(PcScopeWheel *w, int steps, unsigned long long now)
{
    int dir;
    unsigned n;
    if (!steps || !pc_scope_wheel_live(w, now)) return steps;
    dir = steps > 0 ? 1 : -1;
    if (!(dir > 0 ? w->route_up : w->route_down)) return steps;
    /* Swallowed even if the zoom action has no key: no weapon switch. */
    if (!(dir > 0 ? w->zoom_in_key : w->zoom_out_key)) return 0;
    n = (unsigned)(steps > 0 ? steps : -steps);
    if (dir != w->dir || now >= w->until) { w->dir = dir; w->until = now; }
    w->until += (unsigned long long)n * PC_SCOPE_WHEEL_MS;
    if (w->until > now + PC_SCOPE_WHEEL_MAX_MS) w->until = now + PC_SCOPE_WHEEL_MAX_MS;
    return 0;
}

/* Filter one polled key. active: gameplay owns the keyboard (no menu, the
 * game window focused). A routed wheel key never switches weapons in a
 * scope; the matching zoom key reads as held for the notch's duration. */
static inline int pc_scope_wheel_key(const PcScopeWheel *w, int key, int down, int active,
                                     unsigned long long now)
{
    if (!active || !pc_scope_wheel_live(w, now) || key <= 0) return down;
    if ((key == PC_SCOPE_WHEEL_UP_KEY && w->route_up) ||
        (key == PC_SCOPE_WHEEL_DOWN_KEY && w->route_down)) return 0;
    if (now < w->until) {
        if (w->dir > 0 && key == w->zoom_in_key) return 1;
        if (w->dir < 0 && key == w->zoom_out_key) return 1;
    }
    return down;
}
#endif
