/* US 45410066 MapMaker play: security cameras taken over through a switch.
 * Area `camera`. Docs: docs/mapmaker/mapmaker-camera.md. Tests:
 * tests/mapmaker/test_mapmaker_camera.py.
 *
 * A MapMaker camera (item types 0x35..0x37, switch link type 0x18 in
 * sub_0033F0E0) is a mounted-gun actor. The switch calls sub_003391D0, which
 * mounts it with sub_000B76B0: player mode 0x80, character extra +0xC14 = the
 * camera entity, camera data +0xD8 = the character. The mouse then aims it
 * through src/input/pc_mounted.h, the stick through sub_00258A20; both write
 * data +0x1C (pitch) / +0x24 (yaw).
 *
 * Cause (CONFIRMED in text.asm): the actor update sub_00246000 runs its state
 * machine (data +0x10) every tick: state 2 sweeps pitch/yaw by +0x58/+0x5C
 * per tick between the limits +0x34..+0x38 / +0x3C..+0x40, negating the
 * speed at a limit; state 4 eases back to the saved sweep pose +0x20/+0x28;
 * state 3 turns towards an AI target. None of this checks the operator +0xD8.
 * The only stock skip is data flag +8 bit 0 (`test cl,1` at 0x002460E5),
 * which sub_003391D0 sets on mount ONLY when flag 0x20 is already set (it
 * clears 0x20; dismount clears bit 0 and sets 0x20). A camera whose flags
 * lack 0x20 keeps sweeping while the player holds it, so every mouse or
 * stick movement is fought by the sweep.
 *
 * Fix: WRAP sub_00246000 (cdecl, entity at [esp+4], plain ret). In scene 100,
 * while the actor has a live operator (data +0 = entity, +0xD8 = character,
 * character extra +0xC14 = entity, its player record +0x14 = character and
 * mode 0x80), run the original with flag bit 0 set, exactly as the stock
 * switch path does, and clear it again afterwards if it was not set before.
 * The sweep pose, speeds and limits are untouched, so the sweep resumes from
 * where the player left the camera when control ends (or the stock state 4
 * easing returns it). Other scenes (story CCTV/turret scripts) are stock.
 *
 * Diagnostics (all scenes 100, capped per map build, prefix
 * "[mapmaker] camera:"): each change of a camera's control/sweep state, and
 * from the portal walk wrapper each change of the current view's slot,
 * camera room, visible room count, floor range of the visible rooms,
 * deepest walk depth, far plane and FOV (mapmaker_camera_view_diag).
 * Requires the includer's WORD_AT, FLOAT_AT, g_esp and g_xbox_mem_offset. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif

#define MAPMAKER_CAMERA_SCENE 0x0079094Cu
#define MAPMAKER_CAMERA_PLAY 100u
#define MAPMAKER_CAMERA_FROZEN 0x1u      /* data +8 bit 0: sub_00246000 skips its state machine */
#define MAPMAKER_CAMERA_LOG_CAP 96u
#define MAPMAKER_CAMERA_GUNS 16u
#define MAPMAKER_CAMERA_VIEW_RECTS 0x1000u /* view shadow: room r at +0x1000+12*r (rooms.json; stock +0x3BC) */

static struct {
    unsigned logs;
    unsigned rooms;                       /* room count of the build the log cap belongs to */
    struct { uint32_t entity, key; } guns[MAPMAKER_CAMERA_GUNS];
    uint32_t view_key[8];
    unsigned long long frozen_ticks;
} mapmaker_camera;

static int mapmaker_camera_log_ok(void)
{
    unsigned rooms=WORD_AT(0x007B975Cu);
    if(rooms!=mapmaker_camera.rooms) {      /* new map build: fresh log budget */
        mapmaker_camera.rooms=rooms;mapmaker_camera.logs=0;
        memset(mapmaker_camera.guns,0,sizeof mapmaker_camera.guns);
        memset(mapmaker_camera.view_key,0,sizeof mapmaker_camera.view_key);
    }
    return mapmaker_camera.logs<MAPMAKER_CAMERA_LOG_CAP;
}

/* The character controlling this camera actor, or 0 (all links reciprocal). */
static uint32_t mapmaker_camera_operator(uint32_t entity,uint32_t data)
{
    uint32_t op=WORD_AT(data+0xD8u),extra,player;
    if(!op || !MAPMAKER_PTR_OK(op,0x154u)) return 0;
    if(!MAPMAKER_PTR_OK(extra=WORD_AT(op+0x7Cu),0xC18u) || WORD_AT(extra+0xC14u)!=entity) return 0;
    if(!MAPMAKER_PTR_OK(player=WORD_AT(op+0x150u),0x118u) || WORD_AT(player+0x14u)!=op) return 0;
    return WORD_AT(player+0x114u)==0x80u ? op : 0;
}

static void mapmaker_camera_gun_log(uint32_t entity,uint32_t data,uint32_t op,int frozen)
{
    uint32_t state=WORD_AT(data+0x10u),flags=WORD_AT(data+8u);
    uint32_t key=(state&0xFFu)|(op?0x100u:0)|(frozen?0x200u:0)|((flags&0x21u)<<12);
    unsigned i,slot=MAPMAKER_CAMERA_GUNS;
    if(!mapmaker_camera_log_ok()) return;
    for(i=0;i<MAPMAKER_CAMERA_GUNS;i++) {
        if(mapmaker_camera.guns[i].entity==entity) { slot=i;break; }
        if(slot==MAPMAKER_CAMERA_GUNS && !mapmaker_camera.guns[i].entity) slot=i;
    }
    if(slot==MAPMAKER_CAMERA_GUNS) return;
    if(mapmaker_camera.guns[slot].entity==entity && mapmaker_camera.guns[slot].key==key) return;
    if(mapmaker_camera.guns[slot].entity!=entity && !op) return;  /* only cameras a player has used */
    mapmaker_camera.guns[slot].entity=entity;mapmaker_camera.guns[slot].key=key;
    mapmaker_camera.logs++;
    fprintf(stderr,"[mapmaker] camera: actor 0x%08X type %u state %u flags 0x%08X operator 0x%08X: sweep %s"
            " (pitch %.1f in %.1f..%.1f, yaw %.1f in %.1f..%.1f, sweep speed %.3f/%.3f)\n",
            (unsigned)entity,(unsigned)WORD_AT(data+0xCu),(unsigned)state,(unsigned)flags,(unsigned)op,
            frozen?"suspended while controlled":op?"stock (already held by the stock flag)":"running",
            FLOAT_AT(data+0x1Cu),FLOAT_AT(data+0x34u),FLOAT_AT(data+0x38u),
            FLOAT_AT(data+0x24u),FLOAT_AT(data+0x3Cu),FLOAT_AT(data+0x40u),
            FLOAT_AT(data+0x58u),FLOAT_AT(data+0x5Cu));
    fflush(stderr);
}

/* WRAP sub_00246000: mounted-gun/camera actor update (cdecl, entity at
 * [esp+4], plain ret). */
void pc_mapmaker_camera_gun_update(void (*original)(void))
{
    uint32_t entity=WORD_AT(g_esp+4u),data=0,op=0;
    int set=0;
    if(WORD_AT(MAPMAKER_CAMERA_SCENE)==MAPMAKER_CAMERA_PLAY && MAPMAKER_PTR_OK(entity,0x80u) &&
       MAPMAKER_PTR_OK(data=WORD_AT(entity+0x7Cu),0x1C4u) && WORD_AT(data)==entity) {
        op=mapmaker_camera_operator(entity,data);
        set=op && !(WORD_AT(data+8u)&MAPMAKER_CAMERA_FROZEN);
        mapmaker_camera_gun_log(entity,data,op,set);   /* the flags as the game left them */
        if(set) {
            WORD_AT(data+8u)|=MAPMAKER_CAMERA_FROZEN;
            mapmaker_camera.frozen_ticks++;
        }
    } else data=0;
    original();
    if(set) WORD_AT(data+8u)&=~MAPMAKER_CAMERA_FROZEN;
}

/* Called from the portal walk wrapper (pc_mapmaker_openview_walk, after the
 * stock walk of the current view): one line whenever what the view reaches
 * changes. Floors from the room boxes (record +0x14 min y, +0x20 max y;
 * play y = floor x 5.0). Read-only. */
#define MAPMAKER_CAMERA_VIEW_DIAG 1
static void mapmaker_camera_view_diag(void)
{
    uint32_t view=WORD_AT(0x007B0CC0u),n=WORD_AT(0x007B975Cu),records=WORD_AT(0x00708788u),r;
    uint32_t slot,room,visible=0,key,actor,mode=0;
    int lo=1000,hi=-1000,top_room=0;
    float far_plane,fov;
    if(WORD_AT(MAPMAKER_CAMERA_SCENE)!=MAPMAKER_CAMERA_PLAY || n<1u || n>255u) return;
    if(!MAPMAKER_PTR_OK(view,MAPMAKER_CAMERA_VIEW_RECTS+(n+1u)*12u) || !MAPMAKER_PTR_OK(records,(n+1u)*0xB4u)) return;
    for(r=1;r<=n;r++) {
        uint32_t e=view+MAPMAKER_CAMERA_VIEW_RECTS+r*12u;
        int16_t x0=*(int16_t *)((uintptr_t)(e+4u)+g_xbox_mem_offset),x1=*(int16_t *)((uintptr_t)(e+8u)+g_xbox_mem_offset);
        float y0=FLOAT_AT(records+r*0xB4u+0x14u),y1=FLOAT_AT(records+r*0xB4u+0x20u);
        int f0,f1;
        if((int32_t)WORD_AT(e)<0 || !(x0<x1)) continue;
        visible++;
        if(!isfinite(y0) || !isfinite(y1)) continue;
        f0=(int)floorf(y0/5.f+.5f);f1=(int)floorf(y1/5.f-.5f);
        if(f1<f0) f1=f0;
        if(f0<lo) lo=f0;
        if(f1>hi) { hi=f1;top_room=(int)r; }
    }
    slot=WORD_AT(view+0x214u)&7u;room=WORD_AT(view+0x2A0u);
    actor=WORD_AT(0x007B0C7Cu);
    if(MAPMAKER_PTR_OK(actor,0x118u)) mode=WORD_AT(actor+0x114u);
    key=visible|((uint32_t)(hi+16)&0xFFu)<<8|(room&0xFFu)<<16|((mode==0x80u)<<24)|((uint32_t)(lo+16)&0x3Fu)<<25;
    if(key==mapmaker_camera.view_key[slot] || !mapmaker_camera_log_ok()) return;
    mapmaker_camera.view_key[slot]=key;mapmaker_camera.logs++;
    far_plane=FLOAT_AT(0x004DCB0Cu);fov=MAPMAKER_PTR_OK(view,0x388u)?FLOAT_AT(view+0x384u):0.f;
    fprintf(stderr,"[mapmaker] camera: view %u%s: camera room %u, %u of %u rooms visible, floors %d..%d (top room %d), deepest walk %d, far %.0f, fov %.1f\n",
            (unsigned)slot,mode==0x80u?" (player on a mounted gun/camera)":"",(unsigned)room,
            (unsigned)visible,(unsigned)n,visible?lo:0,visible?hi:0,top_room,
            (int)(int32_t)WORD_AT(view+0xD28u),far_plane,fov);
    fflush(stderr);
}
