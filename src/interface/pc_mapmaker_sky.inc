/* US 45410066 MapMaker "open sky": per-map cloud layer (docs/mapmaker/mapmaker-sky.md).
 *
 * The sky itself needs no change: the stock Map Settings menu (sub_003133E0)
 * already has a list row bound to editor +0x220 (0x00313833..0x0031383E) with
 * the 11 entries of 0x0052D150 ({-1 = tileset default, skies 58..67}); the map
 * stores it at info chunk +0x10 (0x00300795) and map start uses it
 * (sub_0023F390, 0x0023F482..0x0023F4BA). MapMaker skies have no cloud layer:
 * records 58..67 of the sky table 0x005149A8 (0x34 bytes) have zero cloud
 * colours at +0x10/+0x14, and sub_0011C9F0 enables the layer ([0x007AC7DC])
 * only when one is non-zero (0x0011CA31..0x0011CA45).
 *
 * With TSFP_MAPMAKER_LIMITS=1:
 * - editor (scene 102): a "Clouds" checkbox is appended to the sky row, after
 *   the stock checkbox bound to editor +0x21C (sub_00078560 at 0x00313883);
 *   its value is kept in map info flag 0x40000 (data +0x244), which no stock
 *   reader tests (bits used: 0x1, 0x2..0x4000, 0x8000, 0x10000, 0x20000;
 *   readers sub_00301A90, sub_002FDA20, sub_002BE750, sub_0023F390) and the
 *   stock rebuild sub_003006C0 rewrites from editor fields, so the hook sets it
 *   again after every rebuild;
 * - play (scene 100): before the cloud setup sub_0011C9F0 (load phase,
 *   sub_001CDB80, before sub_0023F390 sets the level's sky), a map with the
 *   flag gets the cloud fields +0x10..+0x23 of story sky record 5 (colours
 *   0x5A5A6420/0, 200.0, 50.0, 0) copied into its MapMaker sky record, and the
 *   scene-100 level entry 0x00518778 +0x10 holds the map's sky only while the
 *   original runs. Records 58..67 are used by no level table entry (87 entries
 *   at 0x00515FF8 checked), so story levels never read the copy; it is undone
 *   at the next scene-100 setup.
 * Extra skies (table mapmaker_sky_extras, pc_mapmaker_skymusic.inc): a map
 * with an extra sky row whose pak is mounted this level uses that story
 * record; the cloud hook then sets the level's sky to it while the original
 * runs and patches nothing (the record keeps its own cloud fields).
 * The stock game ignores the flag: such a map loads there with its sky and no
 * clouds. Requires the includer's WORD_AT, guest_call, g_esp,
 * mapmaker_limits/mapmaker_limits_read and MAPMAKER_PTR_OK
 * (pc_mapmaker_limits.inc). */
#include <stdio.h>
#include <string.h>

#define MAPMAKER_SKY_FLAG_CLOUDS 0x00040000u
#define MAPMAKER_SKY_TABLE 0x005149A8u
#define MAPMAKER_SKY_STRIDE 0x34u
#define MAPMAKER_SKY_FIRST 58
#define MAPMAKER_SKY_LAST 67
#define MAPMAKER_SKY_TEMPLATE 5
#define MAPMAKER_SKY_CLOUD_FIELD 0x10u
#define MAPMAKER_SKY_CLOUD_WORDS 5u
#define MAPMAKER_SKY_LEVEL 0x00518778u          /* level table entry of scene 100 */
#define MAPMAKER_SKY_TILESETS 0x0052CF40u       /* 6 x 0x58, +2 default sky index */
#define MAPMAKER_SKY_CONTAINER_MAX 0x00080000u  /* scan bound: the raised 512 KB container */
#define MAPMAKER_SKY_RET_LOADER_RESET 0x00301B4Fu /* sub_00301A90 -> sub_002FEBF0 */
#define MAPMAKER_SKY_RET_SETTINGS_BIND 0x00313888u /* sub_003133E0 -> sub_00078560(&editor+0x21C) */

/* Extra skies (docs/mapmaker/mapmaker-sky-music.md): story and arcade sky
 * records whose model is in a level pak MapMaker play does not mount. The
 * map keeps a MapMaker sky (fallback) in info +0x10 for stock builds and the
 * row number 1..17 in info flag bits 19..23; pc_mapmaker_skymusic.inc mounts
 * the pak and makes the map start use the record. */
#define MAPMAKER_SKY_EXTRA_SHIFT 19u
#define MAPMAKER_SKY_EXTRA_MASK 0x00F80000u
static const struct mapmaker_sky_extra {
    int sky, fallback;        /* sky table record; MapMaker sky stock builds get */
    const char *pak, *label;
} mapmaker_sky_extras[]={
    { 5,64,"pak/arcade/l_20_ar.pak","Story: Cold Night"},
    {35,58,"pak/arcade/l_59_ar.pak","Story: Castle"},
    {39,63,"pak/cutscene/l_33_cs.pak","Story: Train"},
    {41,60,"pak/arcade/l_38_ar.pak","Arcade: Virtua"},
    {42,67,"pak/arcade/l_29_ar.pak","Arcade: Prison"},
    {43,61,"pak/challenge/l_64_ch.pak","Story: Time to Split"},
    {44,61,"pak/story/l_81_st.pak","Story: Nemo"},
    {45,59,"pak/arcade/l_46_ar.pak","Arcade: Venice"},
    {46,61,"pak/cutscene/l_40_cs.pak","Story: Street War"},
    {47,62,"pak/arcade/l_48_ar.pak","Arcade: Vietnam"},
    {48,62,"pak/arcade/l_49_ar.pak","Arcade: Temple"},
    {49,60,"pak/arcade/l_15_ar.pak","Arcade: Spaceport"},
    {50,63,"pak/challenge/l_76_ch.pak","Story: Depot"},
    {51,64,"pak/arcade/l_53_ar.pak","Arcade: Sniper"},
    {52,61,"pak/challenge/l_66_ch.pak","Story: NF1"},
    {56,66,"pak/arcade/l_25_ar.pak","Arcade: Chinese"},
    {57,59,"pak/arcade/l_39_ar.pak","Arcade: Horror"},
};
#define MAPMAKER_SKY_EXTRA_COUNT ((uint32_t)(sizeof mapmaker_sky_extras/sizeof mapmaker_sky_extras[0]))

extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
extern void sub_00079D60(void),sub_0007D4F0(void),sub_00078560(void);

static struct {
    uint32_t storage;   /* guest: +0 checkbox value, +4 "Clouds" */
    uint32_t editor;    /* editor object the state below belongs to */
    int live, bound, value;
    int extra_ready;    /* this level's extra-sky pak is mounted (pc_mapmaker_skymusic.inc) */
    uint32_t (*extra_prepare)(uint32_t info); /* skymusic: mount the row's pak now; row or 0 */
    int have_pristine, patched;
    uint32_t pristine[MAPMAKER_SKY_LAST-MAPMAKER_SKY_FIRST+1][MAPMAKER_SKY_CLOUD_WORDS];
    unsigned long long no_storage, no_info, applied;
} mapmaker_sky;

static int mapmaker_sky_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t mapmaker_sky_scene(void) { return WORD_AT(0x0079094Cu); }

/* Data of the map info chunk 0x00C77668 in a container, or 0. */
static uint32_t mapmaker_sky_info(uint32_t c)
{
    uint32_t off,tag,size;
    if(!MAPMAKER_PTR_OK(c,16u) || WORD_AT(c)!=0x00C77667u) return 0;
    for(off=8u;off+8u<=MAPMAKER_SKY_CONTAINER_MAX;off+=8u+size) {
        if(!MAPMAKER_PTR_OK(c+off,8u)) return 0;
        tag=WORD_AT(c+off);size=WORD_AT(c+off+4u);
        if(tag==0x00C7766Au || size>MAPMAKER_SKY_CONTAINER_MAX) return 0;
        if(tag==0x00C77668u) return size>=0x248u && MAPMAKER_PTR_OK(c+off+8u,size)?c+off+8u:0u;
    }
    return 0;
}
static int mapmaker_sky_container_flag(uint32_t c)
{
    uint32_t info=mapmaker_sky_info(c);
    return info && (WORD_AT(info+0x244u)&MAPMAKER_SKY_FLAG_CLOUDS)?1:0;
}

/* The editor's current cloud choice: the checkbox once the menu has bound
 * it, an explicit value after a reset, else what the container holds (a map
 * just loaded, or the last rebuild). */
static int mapmaker_sky_clouds_now(uint32_t container)
{
    if(!mapmaker_sky.live || mapmaker_sky.editor!=WORD_AT(0x007844A8u))
        return mapmaker_sky_container_flag(container);
    if(mapmaker_sky.bound && mapmaker_sky.storage) return WORD_AT(mapmaker_sky.storage)?1:0;
    return mapmaker_sky.value;
}

static uint32_t mapmaker_sky_storage(void)
{
    if(!mapmaker_sky.storage) {
        static const char label[]="Clouds";
        uint32_t p=xbox_HeapAlloc(16u,4u);
        if(!p) {
            if(mapmaker_sky.no_storage++<4u) fprintf(stderr,"[mapmaker] sky: no memory for the Clouds checkbox\n");
            return 0;
        }
        WORD_AT(p)=0;memcpy((void *)((uintptr_t)p+4u+g_xbox_mem_offset),label,sizeof label);
        mapmaker_sky.storage=p;
    }
    return mapmaker_sky.storage;
}

/* WRAP sub_002FEBF0 (editor map reset, one cdecl argument): called by the
 * loader at the start of a load (then the container is the truth) and by
 * New map / editor start (then the map has no clouds until changed). */
void pc_mapmaker_sky_reset(void (*original)(void))
{
    uint32_t ret=MAPMAKER_PTR_OK(g_esp,4u)?WORD_AT(g_esp):0u;
    original();
    if(!mapmaker_sky_on() || mapmaker_sky_scene()!=102u) return;
    mapmaker_sky.editor=WORD_AT(0x007844A8u);
    mapmaker_sky.bound=0;mapmaker_sky.value=0;
    mapmaker_sky.live=ret!=MAPMAKER_SKY_RET_LOADER_RESET;
}

/* WRAP sub_00078560 (menu: bind the last item to a dword). After the stock
 * binding of the sky row's checkbox, append the Clouds checkbox the same way
 * the stock code adds a game-mode checkbox (0x00313940..0x00313974):
 * sub_00079D60(text, -1, 0x8800, 0, 0, 0), sub_0007D4F0(0, 0), bind. */
void pc_mapmaker_sky_settings(void (*original)(void))
{
    uint32_t ret=MAPMAKER_PTR_OK(g_esp,4u)?WORD_AT(g_esp):0u,cell,editor;
    original();
    if(ret!=MAPMAKER_SKY_RET_SETTINGS_BIND || !mapmaker_sky_on() || mapmaker_sky_scene()!=102u) return;
    if(!(cell=mapmaker_sky_storage())) return;
    editor=WORD_AT(0x007844A8u);
    WORD_AT(cell)=(uint32_t)mapmaker_sky_clouds_now(WORD_AT(0x00784014u));
    mapmaker_sky.editor=editor;mapmaker_sky.live=1;mapmaker_sky.bound=1;
    guest_call(sub_00079D60,6,cell+4u,0xFFFFFFFFu,0x8800u,0u,0u,0u);
    guest_call(sub_0007D4F0,2,0u,0u);
    guest_call(sub_00078560,1,cell);
}

/* WRAP sub_003006C0 (rebuild the container from the editor: container, mode).
 * The choice is read before the rebuild (it may come from that container) and
 * written into the rebuilt info chunk. Chains with the container area's hook. */
void pc_mapmaker_sky_compile(void (*original)(void))
{
    uint32_t container,info;
    int clouds;
    if(!mapmaker_sky_on() || mapmaker_sky_scene()!=102u || !MAPMAKER_PTR_OK(g_esp,8u)) { original();return; }
    container=WORD_AT(g_esp+4u);
    clouds=mapmaker_sky_clouds_now(container);
    original();
    if(!(info=mapmaker_sky_info(container))) {
        if(clouds && mapmaker_sky.no_info++<4u) fprintf(stderr,"[mapmaker] sky: rebuilt map has no info chunk; Clouds not stored\n");
        return;
    }
    if(clouds) WORD_AT(info+0x244u)|=MAPMAKER_SKY_FLAG_CLOUDS;
    else WORD_AT(info+0x244u)&=~MAPMAKER_SKY_FLAG_CLOUDS;
}

/* The map's extra sky row (1..MAPMAKER_SKY_EXTRA_COUNT), or 0: none, an
 * unknown row, or this level's pak is not mounted. */
static uint32_t mapmaker_sky_extra_of(uint32_t info)
{
    uint32_t k=(WORD_AT(info+0x244u)&MAPMAKER_SKY_EXTRA_MASK)>>MAPMAKER_SKY_EXTRA_SHIFT;
    if(!k || k>MAPMAKER_SKY_EXTRA_COUNT) return 0;
    if(mapmaker_sky.extra_prepare) return mapmaker_sky.extra_prepare(info);
    return mapmaker_sky.extra_ready?k:0u;
}

/* The map's effective sky: an extra sky, else info +0x10 if not negative,
 * else the tileset's. */
static int mapmaker_sky_index(uint32_t info)
{
    int sky=(int16_t)(WORD_AT(info+0x10u)&0xFFFFu);
    uint32_t tileset=WORD_AT(info+0x08u)>>16;      /* word +0x0A */
    uint32_t extra=mapmaker_sky_extra_of(info);
    if(extra) return mapmaker_sky_extras[extra-1u].sky;
    if(sky>=0) return sky;
    if(tileset>=6u) return -1;
    return (int)(WORD_AT(MAPMAKER_SKY_TILESETS+tileset*0x58u)>>16&0xFFu);
}
static uint32_t mapmaker_sky_record(int sky) { return MAPMAKER_SKY_TABLE+(uint32_t)sky*MAPMAKER_SKY_STRIDE+MAPMAKER_SKY_CLOUD_FIELD; }
static void mapmaker_sky_restore(void)
{
    int s;unsigned i;
    if(!mapmaker_sky.patched) return;
    for(s=MAPMAKER_SKY_FIRST;s<=MAPMAKER_SKY_LAST;s++)
        for(i=0;i<MAPMAKER_SKY_CLOUD_WORDS;i++)
            WORD_AT(mapmaker_sky_record(s)+i*4u)=mapmaker_sky.pristine[s-MAPMAKER_SKY_FIRST][i];
    mapmaker_sky.patched=0;
}

/* WRAP sub_0011C9F0 (cloud layer setup, every level load). */
void pc_mapmaker_sky_clouds(void (*original)(void))
{
    uint32_t info,old,i;
    int sky,s;
    if(mapmaker_sky_scene()!=100u) { original();return; }
    mapmaker_sky_restore();
    info=mapmaker_sky_on()?mapmaker_sky_info(WORD_AT(0x00784014u)):0u;
    if(info && mapmaker_sky_extra_of(info)) {
        /* An extra (story) sky keeps its own cloud fields: never patched,
         * the level's sky is that record while the original reads it. */
        sky=mapmaker_sky_index(info);
        old=WORD_AT(MAPMAKER_SKY_LEVEL+0x10u);
        WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=(uint32_t)sky;
        original();
        WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;
        return;
    }
    if(!info || !(WORD_AT(info+0x244u)&MAPMAKER_SKY_FLAG_CLOUDS)) { original();return; }
    sky=mapmaker_sky_index(info);
    if(sky<MAPMAKER_SKY_FIRST || sky>MAPMAKER_SKY_LAST) {
        fprintf(stderr,"[mapmaker] sky: Clouds ignored for sky %d (not a MapMaker sky)\n",sky);
        original();
        return;
    }
    if(!mapmaker_sky.have_pristine) {
        for(s=MAPMAKER_SKY_FIRST;s<=MAPMAKER_SKY_LAST;s++)
            for(i=0;i<MAPMAKER_SKY_CLOUD_WORDS;i++)
                mapmaker_sky.pristine[s-MAPMAKER_SKY_FIRST][i]=WORD_AT(mapmaker_sky_record(s)+i*4u);
        mapmaker_sky.have_pristine=1;
    }
    for(i=0;i<MAPMAKER_SKY_CLOUD_WORDS;i++)
        WORD_AT(mapmaker_sky_record(sky)+i*4u)=WORD_AT(mapmaker_sky_record(MAPMAKER_SKY_TEMPLATE)+i*4u);
    mapmaker_sky.patched=1;
    if(mapmaker_sky.applied++<4u) fprintf(stderr,"[mapmaker] sky: Clouds on for sky %d\n",sky);
    old=WORD_AT(MAPMAKER_SKY_LEVEL+0x10u);
    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=(uint32_t)sky;
    original();
    WORD_AT(MAPMAKER_SKY_LEVEL+0x10u)=old;
}
