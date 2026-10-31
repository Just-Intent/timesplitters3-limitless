/* US 45410066 MapMaker play: draw the real story bosses (the giant skeleton,
 * model 0x5E9, and the giant trophy monster, model 0x5BE) without the room
 * scissor, so their whole height shows. Area `bigdraw`.
 * Docs: docs/mapmaker/mapmaker-realbosses.md ("Whole height on screen").
 *
 * Cause (CONFIRMED, decode checks in tests/mapmaker/test_mapmaker_bigdraw.py):
 * a prop (0x2E8-byte object; +0x20 type, 8 = character; word +0x94 model) is
 * drawn with a screen scissor that is the union of the portal rectangles of
 * the rooms its bounding box touches.
 *  - sub_0009FBE0 lists up to 10 rooms (+0x30 count, +0x38 ids) overlapping
 *    the box +0x1A8 centre +-0x1C0 half size (sub_000873C0 at 0x0009FCF0);
 *    for a character the box is a cube of the model's radius
 *    (sub_0009EAB0 -> sub_0008E5F0, model +0x30).
 *  - The portal walk sub_00087F80 gives the camera's room the whole viewport
 *    [0x007E71C8] (call 0x00089BD9 from sub_00089B90) and every other room
 *    the screen rectangle of the portals it is seen through (view +0x3BC +
 *    12*room: int32 flag, int16 x0, y0, x1, y1).
 *  - sub_000862C0(prop), called only by sub_000A2430 (0x000A246B, return
 *    0x000A2470) once per frame per prop of a room about to be drawn
 *    (sub_000A79B0 from sub_00086480), stores the union of the rectangles of
 *    the visible listed rooms, clamped to the viewport, at +0x118 x0, +0x11C
 *    y0, +0x120 x1, +0x124 y1 (props outside a real room get the viewport).
 *    sub_000A2430 culls the prop when its projected box misses that
 *    rectangle, and sub_000A2740 sets it as the scissor (sub_0001A010 at
 *    0x000A285B) before the model draw (sub_0008F950 at 0x000A28D4).
 * A MapMaker tile is one room one floor high, and its portals are its
 * doorways and open sides, one floor high. A boss standing in another tile
 * than the camera is therefore cut at the projected top of the portal it is
 * seen through (about two floors at the boss's distance); nothing in a room
 * above it is visible either, because no portal leads there (open tops have
 * no room above them). Mission 5 (scene 1) fights the boss inside the
 * courtyard room the player stands in: that room's rectangle is the whole
 * viewport, so the stock game never clips it.
 *
 * Fix: WRAP sub_000862C0. After the original, for a character prop with
 * model 0x5E9 or 0x5BE in scene 100 with TSFP_MAPMAKER_LIMITS=1, when the
 * stock rectangle is not empty (one of its rooms is visible), replace it with
 * the whole viewport, the value the camera's own room gets. Depth testing
 * still hides it behind every wall that is drawn; the cull test then uses the
 * viewport. Every other prop, scene and caller keeps the stock rectangle; a
 * boss none of whose rooms is visible stays culled as stock (counted).
 * Hot (per frame per prop of a drawn room): fixed values first, no system
 * calls. Requires the includer's WORD_AT, FLOAT_AT, g_esp, g_xbox_mem_offset
 * and pc_mapmaker_limits.inc (mapmaker_limits, MAPMAKER_PTR_OK). */
#include <stdio.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) MAPMAKER_PTR_OK(g_esp,(n))
#endif

#define BIGDRAW_SCENE 0x0079094Cu
#define BIGDRAW_PLAY 100u
#define BIGDRAW_VIEWPORT 0x007E71C8u     /* pointer: int32 +0 x0, +4 x1, +0x14 y0, +0x18 y1 */
#define BIGDRAW_RET 0x000A2470u          /* sub_000A2430: call sub_000862C0 at 0x000A246B */
#define BIGDRAW_PROP_SIZE 0x2E8u
#define BIGDRAW_TYPE 0x20u
#define BIGDRAW_CHARACTER 8u
#define BIGDRAW_MODEL 0x94u
#define BIGDRAW_RECT 0x118u              /* float x0, y0, x1, y1 */
#define BIGDRAW_SKELETON 0x5E9u          /* CHR_MANSION_MANSIONBOSS */
#define BIGDRAW_TROPHY 0x5BEu            /* CHR_MANSION_DEERBEAST */
#define BIGDRAW_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static struct { unsigned long long widened, unseen; unsigned logged; } mapmaker_bigdraw;

static void mapmaker_bigdraw_log(const char *what,uint32_t prop,uint32_t model)
{
    if(mapmaker_bigdraw.logged>=4u) return;
    mapmaker_bigdraw.logged++;
    fprintf(stderr,"[mapmaker] bigdraw: %s (prop 0x%X, model 0x%X)\n",what,(unsigned)prop,(unsigned)model);
    fflush(stderr);
}

/* WRAP sub_000862C0 (cdecl prop, plain ret; the prop's draw rectangle). */
void pc_mapmaker_bigdraw_rect(void (*original)(void))
{
    uint32_t prop=0,model,vp;
    if(MAPMAKER_STACK_OK(8u) && WORD_AT(g_esp)==BIGDRAW_RET) prop=WORD_AT(g_esp+4u);
    original();
    if(!prop || !MAPMAKER_PTR_OK(prop,BIGDRAW_PROP_SIZE)) return;
    model=BIGDRAW_H(prop+BIGDRAW_MODEL);
    if((model!=BIGDRAW_SKELETON && model!=BIGDRAW_TROPHY) || WORD_AT(prop+BIGDRAW_TYPE)!=BIGDRAW_CHARACTER) return;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits || WORD_AT(BIGDRAW_SCENE)!=BIGDRAW_PLAY) return;
    vp=WORD_AT(BIGDRAW_VIEWPORT);
    if(!MAPMAKER_PTR_OK(vp,0x1Cu)) return;
    if(!(FLOAT_AT(prop+BIGDRAW_RECT)<FLOAT_AT(prop+BIGDRAW_RECT+8u) &&
         FLOAT_AT(prop+BIGDRAW_RECT+4u)<FLOAT_AT(prop+BIGDRAW_RECT+12u))) {
        if(!mapmaker_bigdraw.unseen++) mapmaker_bigdraw_log("real boss in no visible room: left to the stock cull",prop,model);
        return;
    }
    FLOAT_AT(prop+BIGDRAW_RECT)=(float)(int32_t)WORD_AT(vp);
    FLOAT_AT(prop+BIGDRAW_RECT+4u)=(float)(int32_t)WORD_AT(vp+0x14u);
    FLOAT_AT(prop+BIGDRAW_RECT+8u)=(float)(int32_t)WORD_AT(vp+4u);
    FLOAT_AT(prop+BIGDRAW_RECT+12u)=(float)(int32_t)WORD_AT(vp+0x18u);
    if(!mapmaker_bigdraw.widened++) mapmaker_bigdraw_log("real boss drawn over the whole view, not clipped to its room's portals",prop,model);
}
