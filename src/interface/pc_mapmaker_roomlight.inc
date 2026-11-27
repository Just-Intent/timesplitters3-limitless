/* US 45410066 MapMaker play: a room light's colour spread to the room's
 * objects, in host code.
 *
 *   WRAP sub_00115F50 -> pc_mapmaker_rooms_light_spread   (area rooms)
 *
 * In scene 100 every draw of a type-2 object (sub_000A1DC0 0x000A1E7B ->
 * sub_00116130: the room's animated light record [0x0073D96C] + room*40)
 * ends in sub_00115F50(room, red, green) with blue in ebx, which walks the
 * WHOLE object pool (count [0x007B4FAC], stride 0x2E8, pool [0x0072F5E8]):
 *  - an object in that room (+0x34) that is not type 2 (+0x20) gets the
 *    colour on its model instance (+4): sub_00094250(inst, 1.0, r, g, b),
 *    then sub_00094290(inst, 0x1D) when [[inst+4]+8] is 0;
 *  - an object in a player-held room id 0x401..0x404 (rooms.json
 *    rooms-00115fb3/-fbe/-fc9; stock 0xCA..0xCD): its player
 *    (sub_00356800(id - 0x401), [0x007B0C48] + i*0x1584) gets the colour on
 *    the three held instances +0x1D8, +0x5B0, +0xE84 when the player's object
 *    (+0x14) is in the room;
 *  - a type-2 object: its room record (+0xB2 of [0x00708788] + id*0xB4)
 *    gets flag 0x80 (only for the room itself when [0x004D1984] is set).
 * With every visible room drawn each frame that is (visible rooms) x
 * (objects) lifted iterations per frame: on a 395-piece map with many Story
 * AI about 380 x 1,500, most of the frame. This is the same walk in host code:
 * the same order and tests, the count and pool re-read every step, the same
 * guest calls with the same arguments; only the iterations themselves are
 * native. TSFP_MAPMAKER_ROOM_LIGHT=0 runs the original. Stats in the probe
 * line of pc_mapmaker_rooms_light_spread_log (first call, then every 65536th).
 *
 * Requires the includer's WORD_AT, MAPMAKER_PTR_OK (or guest_writable),
 * guest_call, g_esp, g_ebx, g_eax. */
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) guest_writable((uint32_t)(p),(size_t)(n))
#endif
#include <stdio.h>
#include <stdlib.h>

#define MAPMAKER_ROOMLIGHT_COUNT 0x007B4FACu       /* objects in use (slots 0..count-1) */
#define MAPMAKER_ROOMLIGHT_POOL 0x0072F5E8u        /* -> object pool */
#define MAPMAKER_ROOMLIGHT_STRIDE 0x2E8u
#define MAPMAKER_ROOMLIGHT_PLAYERS 0x007B0C48u     /* -> players, 0x1584 each */
#define MAPMAKER_ROOMLIGHT_PLAYER_STRIDE 0x1584u
#define MAPMAKER_ROOMLIGHT_RECORDS 0x00708788u     /* -> room records, 0xB4 each, flags word +0xB2 */
#define MAPMAKER_ROOMLIGHT_SELF_ONLY 0x004D1984u
#define MAPMAKER_ROOMLIGHT_HELD_FIRST 0x401u       /* rooms.json rooms-00115fb3 (stock 0xCA) */
#define MAPMAKER_ROOMLIGHT_HELD_END 0x405u         /* rooms.json rooms-00115fbe (stock 0xCE) */
#define MAPMAKER_ROOMLIGHT_ONE 0x3F800000u         /* push 0x3f800000: 1.0 */
#define MAPMAKER_ROOMLIGHT_SKIP 0x1Du              /* push 0x1d */

extern void sub_00094250(void),sub_00094290(void),sub_00356800(void);

static struct { unsigned long long calls,objects,coloured; } mapmaker_roomlight;

static int mapmaker_roomlight_mode(void)
{
    static int mode=-1;
    if(mode<0) { const char *e=getenv("TSFP_MAPMAKER_ROOM_LIGHT");mode=!(e && e[0]=='0'); }
    return mode;
}

/* 0x00115F9A..0x001160B3: colour one instance, then the 0x1D call when its
 * model's +8 is 0 (the instance is re-read after the first call). */
static void mapmaker_roomlight_colour(uint32_t holder,uint32_t red,uint32_t green,uint32_t blue)
{
    uint32_t inst=WORD_AT(holder),model;
    guest_call(sub_00094250,5,inst,MAPMAKER_ROOMLIGHT_ONE,red,green,blue);
    inst=WORD_AT(holder);
    model=MAPMAKER_PTR_OK(inst,8u)?WORD_AT(inst+4u):0u;
    if(MAPMAKER_PTR_OK(model,12u) && !WORD_AT(model+8u)) guest_call(sub_00094290,2,inst,MAPMAKER_ROOMLIGHT_SKIP);
    mapmaker_roomlight.coloured++;
}

void pc_mapmaker_rooms_light_spread(void (*original)(void))
{
    uint32_t room,red,green,blue=g_ebx,i;
    if(!mapmaker_roomlight_mode() || !MAPMAKER_PTR_OK(g_esp,16u)) { original();return; }
    room=WORD_AT(g_esp+4u);red=WORD_AT(g_esp+8u);green=WORD_AT(g_esp+12u);
    mapmaker_roomlight.calls++;
    for(i=0;(int32_t)i<(int32_t)WORD_AT(MAPMAKER_ROOMLIGHT_COUNT);i++) {
        uint32_t o=WORD_AT(MAPMAKER_ROOMLIGHT_POOL)+i*MAPMAKER_ROOMLIGHT_STRIDE,at;
        int32_t type;
        mapmaker_roomlight.objects++;
        if(!MAPMAKER_PTR_OK(o,0x38u)) continue;
        at=WORD_AT(o+0x34u);type=(int32_t)WORD_AT(o+0x20u);
        if(at==room && type!=2) {
            if(WORD_AT(o+4u)) mapmaker_roomlight_colour(o+4u,red,green,blue);
            continue;
        }
        if((int32_t)at>=(int32_t)MAPMAKER_ROOMLIGHT_HELD_FIRST && (int32_t)at<(int32_t)MAPMAKER_ROOMLIGHT_HELD_END) {
            uint32_t player=guest_call(sub_00356800,1,at-MAPMAKER_ROOMLIGHT_HELD_FIRST),p,body,k;
            static const uint32_t held[3]={0x1D8u,0x5B0u,0xE84u};
            p=WORD_AT(MAPMAKER_ROOMLIGHT_PLAYERS)+player*MAPMAKER_ROOMLIGHT_PLAYER_STRIDE;
            if(!MAPMAKER_PTR_OK(p,0xE88u)) continue;
            body=WORD_AT(p+0x14u);
            if(!body || !MAPMAKER_PTR_OK(body,0x38u) || WORD_AT(body+0x34u)!=room) continue;
            for(k=0;k<3;k++) {
                uint32_t h=WORD_AT(p+held[k]);
                if(h && MAPMAKER_PTR_OK(h,8u) && WORD_AT(h+4u)) mapmaker_roomlight_colour(h+4u,red,green,blue);
            }
            continue;
        }
        if(type!=2) continue;
        if(WORD_AT(MAPMAKER_ROOMLIGHT_SELF_ONLY) && at!=room) continue;
        {
            uint32_t flags=WORD_AT(MAPMAKER_ROOMLIGHT_RECORDS)+at*0xB4u+0xB2u;
            uint16_t w;
            if(!MAPMAKER_PTR_OK(flags,2u)) continue;
            w=*(uint16_t *)((uintptr_t)flags+g_xbox_mem_offset);
            if(!(w&0x80u)) *(uint16_t *)((uintptr_t)flags+g_xbox_mem_offset)=(uint16_t)(w|0x80u);
        }
    }
    if(mapmaker_roomlight.calls==1u || !(mapmaker_roomlight.calls&0xFFFFu))
        fprintf(stderr,"[mapmaker] room light spread: %llu calls, %llu objects walked, %llu instances coloured (host code)\n",
                mapmaker_roomlight.calls,mapmaker_roomlight.objects,mapmaker_roomlight.coloured);
    MAPMAKER_GUEST_RET();                        /* sub_00115F50 ends in a plain ret */
}
