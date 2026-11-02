/* US 45410066 MapMaker sky tile, dev switch: clear node-mask bits of every
 * MapMaker room in play (docs/mapmaker/mapmaker-sky.md, "Room mask test switch").
 *
 * Each tile room is one model whose nodes are drawn only when the node's mask
 * (+0x5C) is 0 or shares a bit with the instance mask inst+0x30
 * (sub_0008F5E0, 0x0008F637..0x0008F64D). The instance mask comes from one
 * per-room dword table, stock 0x00703460, relocated to 0x008C0800 (rooms 0..255 of 512
 * entries) by config/guest-code-patches/rooms.json. Decoder scan of the stock
 * range (tools/analysis/guest-refs.py 0x703460 0x31C), CONFIRMED:
 *   0x000857A3 sub_00085710  the only store: table[build+0x28 + j] = build+0x44
 *                            for j < build+0x14 (rooms of one piece)
 *   0x0008A79B sub_0008A780  `mov edi, table` (clear at map build start)
 *   0x00085DCA sub_00085CC0  read -> inst+0x30 (0x00085DD0), then bit 25 test
 *   0x00083974 sub_00083970  getter (camera 0x000CAB14, weather 0x003381DC)
 * sub_0008A780 calls sub_00085710 once per piece (0x0008AC71, cdecl, one
 * argument, plain ret) before it creates the room props (sub_000895F0 at
 * 0x0008BAAD -> sub_00085CC0). Clearing the table right after the store keeps
 * the draw mask, the camera and the weather test consistent, touches no
 * editor data and nothing that is saved.
 *
 * TSFP_MAPMAKER_ROOM_MASK_CLEAR=<hex mask> (default off, scene 100 only,
 * independent of TSFP_MAPMAKER_LIMITS). One summary per map build goes to
 * stderr from pc_mapmaker_sky_room_mask_flush (called once per frame).
 * Requires the includer's WORD_AT and g_esp. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) MAPMAKER_PTR_OK(g_esp,(n))
#endif

#define MAPMAKER_SKYMASK_TABLE 0x008C0800u   /* relocated 0x00703460 */
#define MAPMAKER_SKYMASK_ROOMS 256u          /* dev switch: rooms 0..255 of the 0x800-byte table */
#define MAPMAKER_SKYMASK_SCENE_PLAY 100u
#define MAPMAKER_SKYMASK_DETAILS 64u
#define MAPMAKER_SKYMASK_BUILD_SIZE 0x48u    /* reads +0x14, +0x28, +0x44 */

static struct {
    int initialized;
    uint32_t clear;
    int pending;
    unsigned pieces, rooms, changed, beyond;
    unsigned char hit[MAPMAKER_SKYMASK_ROOMS];  /* 1 seen in this build, 2 changed */
    uint32_t old[MAPMAKER_SKYMASK_ROOMS], now[MAPMAKER_SKYMASK_ROOMS];
    unsigned long long builds, ignored;
} mapmaker_skymask;

static uint32_t mapmaker_skymask_value(void)
{
    if(!mapmaker_skymask.initialized) {
        const char *text=getenv("TSFP_MAPMAKER_ROOM_MASK_CLEAR");
        char *end=NULL;unsigned long value=text?strtoul(text,&end,16):0ul;
        mapmaker_skymask.initialized=1;
        if(text && *text && (end==text || *end || value>0xFFFFFFFFul)) {
            fprintf(stderr,"[mapmaker] room mask: TSFP_MAPMAKER_ROOM_MASK_CLEAR=%s is not a 32-bit hex mask; ignored\n",text);
            value=0;
        }
        mapmaker_skymask.clear=(uint32_t)value;
        if(mapmaker_skymask.clear)
            fprintf(stderr,"[mapmaker] room mask: dev switch TSFP_MAPMAKER_ROOM_MASK_CLEAR=0x%08X (MapMaker play only)\n",
                mapmaker_skymask.clear);
    }
    return mapmaker_skymask.clear;
}

/* Print and reset the summary of the last map build. Cheap when idle. */
void pc_mapmaker_sky_room_mask_flush(void)
{
    unsigned r,shown=0;
    if(!mapmaker_skymask.pending) return;
    mapmaker_skymask.builds++;
    fprintf(stderr,"[mapmaker] room mask: cleared 0x%08X on %u rooms (build %llu: %u rooms in %u pieces)\n",
        mapmaker_skymask.clear,mapmaker_skymask.changed,mapmaker_skymask.builds,
        mapmaker_skymask.rooms,mapmaker_skymask.pieces);
    for(r=0;r<MAPMAKER_SKYMASK_ROOMS;r++) {
        if(mapmaker_skymask.hit[r]!=2u) { mapmaker_skymask.hit[r]=0;continue; }
        if(shown<MAPMAKER_SKYMASK_DETAILS)  /* room = table index + 1, as in the probe (model 0x908 + room - 1) */
            fprintf(stderr,"[mapmaker] room mask: room %u 0x%08X -> 0x%08X\n",r+1u,mapmaker_skymask.old[r],mapmaker_skymask.now[r]);
        shown++;
        mapmaker_skymask.hit[r]=0;
    }
    if(shown>MAPMAKER_SKYMASK_DETAILS)
        fprintf(stderr,"[mapmaker] room mask: %u more changed rooms not listed\n",shown-MAPMAKER_SKYMASK_DETAILS);
    if(mapmaker_skymask.beyond)
        fprintf(stderr,"[mapmaker] room mask: %u rooms past the %u-entry table left unchanged\n",
            mapmaker_skymask.beyond,MAPMAKER_SKYMASK_ROOMS);
    fflush(stderr);
    mapmaker_skymask.pending=0;
    mapmaker_skymask.pieces=mapmaker_skymask.rooms=mapmaker_skymask.changed=mapmaker_skymask.beyond=0;
}

/* WRAP sub_00085710 (store one piece's per-room data, cdecl, one argument:
 * the 0x68-byte build record). Calls the original exactly once. */
void pc_mapmaker_sky_room_mask(void (*original)(void))
{
    uint32_t clear=mapmaker_skymask_value(),build=0,base,count,j;
    if(clear && MAPMAKER_STACK_OK(8u)) {
        uint32_t scene=WORD_AT(0x0079094Cu);
        if(scene==MAPMAKER_SKYMASK_SCENE_PLAY) build=WORD_AT(g_esp+4u);
        else if(!mapmaker_skymask.ignored++)
            fprintf(stderr,"[mapmaker] room mask: room build in scene %u ignored (play is scene 100)\n",scene);
    }
    original();
    if(!build || !MAPMAKER_PTR_OK(build,MAPMAKER_SKYMASK_BUILD_SIZE)) return;
    base=WORD_AT(build+0x28u);count=WORD_AT(build+0x14u);
    if((int32_t)count<=0) return;
    for(j=0;j<count && base+j<MAPMAKER_SKYMASK_ROOMS;j++)  /* a room seen again: a new map build */
        if(mapmaker_skymask.hit[base+j]) { pc_mapmaker_sky_room_mask_flush();break; }
    mapmaker_skymask.pieces++;
    for(j=0;j<count;j++) {
        uint32_t room=base+j,address,old;
        if(room>=MAPMAKER_SKYMASK_ROOMS) { mapmaker_skymask.beyond+=count-j;break; }
        address=MAPMAKER_SKYMASK_TABLE+room*4u;old=WORD_AT(address);
        mapmaker_skymask.rooms++;mapmaker_skymask.pending=1;mapmaker_skymask.hit[room]=1;
        if(!(old&clear)) continue;
        WORD_AT(address)=old&~clear;
        mapmaker_skymask.hit[room]=2;mapmaker_skymask.old[room]=old;mapmaker_skymask.now[room]=old&~clear;
        mapmaker_skymask.changed++;
    }
}

#ifdef TSFP_MAPMAKER_SKYMASK_TEST
static void mapmaker_skymask_reset(void) { memset(&mapmaker_skymask,0,sizeof mapmaker_skymask); }
#endif
