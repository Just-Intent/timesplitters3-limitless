/* US 45410066 MapMaker real story bosses: the particle/effect statics
 * 0x0078B080..0x0078B1DB (sub_002A62C0..sub_002A8760: the particle block
 * [0x0078B0D0], its count [0x0078B0CC], emitter shapes, the effect blocks
 * [0x0078B090]/[0x0078B098]) that the giant skeleton, the Crow walker
 * (pc_mapmaker_realbosses.inc) and the split Crowe (pc_mapmaker_crowe.inc)
 * share. Each boss instance keeps its own copy (MapmakerFx); one holder's
 * copy is live at a time.
 *
 * mapmaker_fx_enter(c) parks the live window in its current holder's copy
 * (or in the outside copy when no boss holds it) and loads c's (0: the
 * outside values); it returns the previous holder, which mapmaker_fx_leave
 * enters again. Entering the holder again changes nothing. So the nesting
 * order does not matter: a Crowe's hit callback inside a walker's damage
 * inside that Crowe's own fight still sees that Crowe's latest values, and
 * every caller gets its own back (v0.4.0-mapmaker.28: each area swapped its
 * part of the window on its own and skipped the swap for "the same instance
 * again", so that re-entry ran on the other area's values).
 *
 * Requires the includer's g_xbox_mem_offset. Docs: "Several split Crowes" in
 * docs/mapmaker/mapmaker-crowe.md, "Several per map" in
 * docs/mapmaker/mapmaker-realbosses.md. */
#ifndef PC_MAPMAKER_BOSSFX_INC
#define PC_MAPMAKER_BOSSFX_INC
#include <stdint.h>
#include <string.h>

#define MAPMAKER_FX 0x0078B080u
#define MAPMAKER_FX_SIZE 0x15Cu
typedef struct { uint8_t b[MAPMAKER_FX_SIZE]; } MapmakerFx;
static MapmakerFx mapmaker_fx_outside, *mapmaker_fx_holder;   /* holder 0: the outside values are live */

static MapmakerFx *mapmaker_fx_enter(MapmakerFx *c)
{
    MapmakerFx *prev=mapmaker_fx_holder;
    uint8_t *live=(uint8_t *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset);
    if(prev==c) return prev;
    memcpy(prev?prev->b:mapmaker_fx_outside.b,live,MAPMAKER_FX_SIZE);
    memcpy(live,c?c->b:mapmaker_fx_outside.b,MAPMAKER_FX_SIZE);
    mapmaker_fx_holder=c;
    return prev;
}
static void mapmaker_fx_leave(MapmakerFx *prev) { (void)mapmaker_fx_enter(prev); }
/* Copies about to be dropped (a new level): when one of them is live, the
 * outside values come back first. */
static void mapmaker_fx_forget(const void *copies,size_t bytes)
{
    const uint8_t *h=(const uint8_t *)mapmaker_fx_holder,*lo=(const uint8_t *)copies;
    if(h && h>=lo && h<lo+bytes) mapmaker_fx_enter(0);
}
#endif
