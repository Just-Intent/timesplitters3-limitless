/* US 45410066 MapMaker debug panel: the game-thread reading.
 *
 *   called from pc_developer_guest_frame (pc_developer_guest.inc), every
 *   guest frame; reads at most every 250 ms (pc_debug_mapmaker_due)
 *
 * Read-only: fixed image counters the game already maintains and the host
 * counters of the MapMaker areas, nothing per object. The only loop is the
 * visible-room count over the view's room rectangles (at most 1023 entries,
 * four times a second, scene 100), the same test as the camera diagnostics
 * (mapmaker_camera_view_diag). The reading is handed to pc_debug.c as values;
 * the presentation thread never sees a guest pointer. Panel, hotkey and
 * format: src/diagnostics/pc_debug_panel.h, docs/mapmaker/mapmaker-limitless.md "Debug panel".
 *
 * Requires the includer's WORD_AT, FLOAT_AT, g_xbox_mem_offset and, from the
 * areas included before it, mapmaker_limits (pc_mapmaker_limits.inc),
 * mapmaker_memory and mapmaker_nav (pc_mapmaker_memory.inc) and
 * mapmaker_rooms_walk_calls (pc_mapmaker_roomquery.inc). */
#include <stdint.h>
#include <string.h>
#include "pc_debug.h"

#define MAPMAKER_PANEL_SCENE 0x0079094Cu
#define MAPMAKER_PANEL_SESSION 0x0076B108u     /* network session */
#define MAPMAKER_PANEL_MODE 0x007DE455u        /* byte: 10 = Story */
#define MAPMAKER_PANEL_CHARACTERS 0x0074C37Cu  /* live character list (players first) */
#define MAPMAKER_PANEL_AI 0x007A2958u          /* AI table slots used */
#define MAPMAKER_PANEL_OBJ_CAP 0x0072F5E4u     /* engine object pool slots */
#define MAPMAKER_PANEL_OBJ_COUNT 0x007B4FACu   /* slots handed out */
#define MAPMAKER_PANEL_OBJ_FREE 0x007B65B8u    /* freed slots below the count */
#define MAPMAKER_PANEL_PADS 0x007B6664u        /* regular pads loaded */
#define MAPMAKER_PANEL_COVER 0x007B65C0u       /* cover pads loaded */
#define MAPMAKER_PANEL_LINKS 0x007B6660u       /* links loaded */
#define MAPMAKER_PANEL_GROUPS 0x007B666Cu      /* pad groups in play */
#define MAPMAKER_PANEL_ROOMS 0x007B975Cu       /* rooms of the map build */
#define MAPMAKER_PANEL_RECORDS 0x00708788u     /* room records, 0xB4 each */
#define MAPMAKER_PANEL_VIEW 0x007B0CC0u        /* current view: +0x2A0 camera room */
#define MAPMAKER_PANEL_VIEW_RECTS 0x1000u      /* room r rectangle at view +0x1000 + 12 r (rooms.json) */
#define MAPMAKER_PANEL_PLAYER 0x007B0C7Cu      /* current player: +0x14 character, +0x64 position */
#define MAPMAKER_PANEL_LEVEL_FREE 0x006B8384u  /* Level heap / heap 0 shared free count */
#define MAPMAKER_PANEL_ROOM_CAP 1023u
#define MAPMAKER_PANEL_CHARACTER_CAP 64u       /* pc_mapmaker_actors_list_full */
#define MAPMAKER_PANEL_AI_WIDE 63u             /* MAPMAKER_ACTORS_AI_CAP: local MapMaker Story */
#define MAPMAKER_PANEL_AI_STOCK 30u            /* MAPMAKER_ACTORS_STOCK_AI */
#define MAPMAKER_PANEL_GUEST_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                                      (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))

static void mapmaker_panel_read(PcDebugMapmaker *m)
{
    uint32_t cap,count,freed,view,player,ent,n,records,r;
    memset(m,0,sizeof *m);
    m->scene=(int32_t)WORD_AT(MAPMAKER_PANEL_SCENE);
    m->mapmaker=m->scene==100;
    m->visible_rooms=-1;m->camera_room=-1;
    mapmaker_limits_read();
    m->characters=WORD_AT(MAPMAKER_PANEL_CHARACTERS);
    m->characters_cap=MAPMAKER_PANEL_CHARACTER_CAP;
    m->ai=WORD_AT(MAPMAKER_PANEL_AI);
    m->ai_cap=m->mapmaker && !WORD_AT(MAPMAKER_PANEL_SESSION) &&
              *(uint8_t *)((uintptr_t)MAPMAKER_PANEL_MODE+g_xbox_mem_offset)==10u ?
              MAPMAKER_PANEL_AI_WIDE : MAPMAKER_PANEL_AI_STOCK;
    cap=WORD_AT(MAPMAKER_PANEL_OBJ_CAP);count=WORD_AT(MAPMAKER_PANEL_OBJ_COUNT);freed=WORD_AT(MAPMAKER_PANEL_OBJ_FREE);
    if(cap && freed<=count) { m->objects_known=1;m->objects=count-freed;m->objects_cap=cap; }
    m->pads=WORD_AT(MAPMAKER_PANEL_PADS);m->cover=WORD_AT(MAPMAKER_PANEL_COVER);
    m->links=WORD_AT(MAPMAKER_PANEL_LINKS);m->groups=WORD_AT(MAPMAKER_PANEL_GROUPS);
    if(m->mapmaker && mapmaker_limits.pools) {   /* the pools pc_display_mapmaker_pools sized */
        m->pads_cap=MAPMAKER_POOL_PADS;m->cover_cap=MAPMAKER_POOL_COVER;
        m->links_cap=MAPMAKER_POOL_LINKS;m->groups_cap=MAPMAKER_LIMIT_GROUPS;
    }
    n=WORD_AT(MAPMAKER_PANEL_ROOMS);
    m->rooms=n;
    if(m->mapmaker) m->rooms_cap=MAPMAKER_PANEL_ROOM_CAP;
    view=WORD_AT(MAPMAKER_PANEL_VIEW);
    if(MAPMAKER_PANEL_GUEST_OK(view,0x2A4u) && WORD_AT(view+0x2A0u)<=0xFFFFu) m->camera_room=(int)WORD_AT(view+0x2A0u);
    records=WORD_AT(MAPMAKER_PANEL_RECORDS);
    if(m->mapmaker && n>=1u && n<=MAPMAKER_PANEL_ROOM_CAP &&
       MAPMAKER_PANEL_GUEST_OK(view,MAPMAKER_PANEL_VIEW_RECTS+(n+1u)*12u) && MAPMAKER_PANEL_GUEST_OK(records,(n+1u)*0xB4u)) {
        int visible=0;
        for(r=1;r<=n;r++) {
            uint32_t e=view+MAPMAKER_PANEL_VIEW_RECTS+r*12u;
            int16_t x0=*(int16_t *)((uintptr_t)(e+4u)+g_xbox_mem_offset),x1=*(int16_t *)((uintptr_t)(e+8u)+g_xbox_mem_offset);
            if((int32_t)WORD_AT(e)>=0 && x0<x1) visible++;
        }
        m->visible_rooms=visible;
    }
    player=WORD_AT(MAPMAKER_PANEL_PLAYER);
    if(MAPMAKER_PANEL_GUEST_OK(player,0x18u) && MAPMAKER_PANEL_GUEST_OK(ent=WORD_AT(player+0x14u),0x70u)) {
        m->player=1;m->x=FLOAT_AT(ent+0x64u);m->y=FLOAT_AT(ent+0x68u);m->z=FLOAT_AT(ent+0x6Cu);
    }
    m->heap_known=1;
    m->level_free=(int32_t)WORD_AT(MAPMAKER_PANEL_LEVEL_FREE);
    m->nav_used=mapmaker_nav.used;m->nav_size=mapmaker_nav.size;m->nav_fallbacks=mapmaker_nav.fallbacks;
    m->heap_overruns=mapmaker_memory.overrun_allocs;
    m->room_queries=mapmaker_rooms_walk_calls;
}

void pc_mapmaker_panel_guest_tick(void)
{
    PcDebugMapmaker m;
    if(!pc_debug_mapmaker_due()) return;
    mapmaker_panel_read(&m);
    pc_debug_mapmaker_publish(&m);
}
