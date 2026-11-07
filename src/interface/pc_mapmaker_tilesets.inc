/* US 45410066 MapMaker: the hidden sixth tileset, Abstract (table index 0).
 * Evidence and the asset audit: docs/mapmaker/mapmaker-tilesets.md.
 *
 * Stock facts (CONFIRMED in the XBE and the game paks):
 *  - Tileset table 0x0052CF38, 6 x 0x58. Index 0 (text 0xAAF "Abstract",
 *    tile models 1980..2008 "PRETILE1..29", default sky 60, music
 *    tileset_lab_high, image resource 0) is the editor's preview set: the
 *    editor always preloads its 29 tile models (sub_00338110 from
 *    sub_0032CAB0) and builds its palette props for index 0 (sub_00311E90,
 *    jump table 0x00311F98 case 0 -> 0x7BC). Its tile models and the pad sets
 *    pad/data/tile1_1..35.raw are only in the editor pak pak/arcade/l_102.pak
 *    (model flags 0x200, resident); tileset1.pak holds its textures and the 22
 *    per-tileset item models (doors, keys, guns, crates...) of item column 0.
 *  - The Map Settings builder sub_003133E0 adds a row only for records
 *    0x0052CF90..0x0052D0F0 (0x00313510..0x00313536), so Abstract is never
 *    offered. Its rows are sub_00071800(text, index) (return 0x0031354A); the
 *    list binds editor +0x204, which sub_002FC0A0 copies to [0x00784020] and
 *    the editor then streams tileset1.pak (stream set 0x0052EC88 entry 0).
 *    No other tileset picker exists (new map: sub_002FEBF0 sets 3).
 *  - Play (scene 100): sub_00062790 mounts tileset<idx+1>.pak with
 *    sub_000622D0(sub_00338170(header +0xA), 1, 1) (call 0x000628ED) and then
 *    l_100.pak, which has no tileset 1 tile models or pads, so an Abstract map
 *    would build with missing room geometry and no pads (the builder
 *    sub_00085920's tile1 pad fallback, 0x00085AF4, finds nothing either).
 *
 * Hooks:
 *  - menu_row (WRAP sub_00071800): with TSFP_MAPMAKER_LIMITS=1, in the editor
 *    (scene 102), after the stock Virtual row (value 5, return 0x0031354A)
 *    append "Disco" (value 6, the Virtual variant of pc_mapmaker_disco.inc;
 *    label in guest heap) and "Abstract" (text 0xAAF, value 0) the stock way.
 *  - play_pak (WRAP sub_00338170, single caller 0x000628ED): for a map whose
 *    tileset is 0, mount l_102.pak first exactly as the stock tileset paks
 *    are (sub_000622D0(path, 1, 1): read whole, level heap, unmounted by the
 *    stock teardown sub_00062720), then its tile textures' pak
 *    arcademem/l_102m.pak index-only. Not gated: stock maps never have tileset 0,
 *    and an Abstract map must not be played without it. If the mount is not
 *    possible (mount table near its 30 entries, file missing) the map plays
 *    as Lab, whose props and music Abstract shares: header +0xA,
 *    [0x00784020] and the getter's argument become 1, and it says so.
 *  - Abstract has no Map Settings/info image (+0x54 is 0); both hooks give it
 *    Lab's image resource 0x17D.
 * Save: a map with tileset 0 saves in the fork's 64 KB tier
 * (mapmaker_container_over_stock), so a stock game refuses it instead of
 * loading a map it cannot play; System Link loads only the stock tier.
 * Requires the includer's WORD_AT, guest_call, g_eax, g_esp,
 * g_xbox_mem_offset, xbox_HeapAlloc and mapmaker_limits/mapmaker_limits_read and
 * MAPMAKER_PTR_OK (pc_mapmaker_limits.inc). */
#include <stdio.h>
#include <string.h>

#define TS_TABLE 0x0052CF38u
#define TS_STRIDE 0x58u
#define TS_ABSTRACT 0u
#define TS_FALLBACK 1u                     /* Lab */
#define TS_LAST_STOCK_ROW 5u               /* Virtual, the last stock row */
#define TS_IMAGE_FIELD 0x54u               /* image resource (sub_00311810, sub_002C8450) */
#define TS_LAB_IMAGE 0x17Du
#define TS_RET_MENU_ROW 0x0031354Au        /* sub_003133E0 -> sub_00071800(text, index) */
#define TS_RET_PAK_PATH 0x000628F2u        /* sub_00062790 -> sub_00338170(header +0xA) */
#define TS_SCENE 0x0079094Cu
#define TS_CONTAINER 0x00784014u
#define TS_CURRENT 0x00784020u             /* current tileset (editor and play) */
#define TS_MOUNTS 0x006D7880u              /* mounted paks; table of 30 (0x000620F9) */
#define TS_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define TS_MOUNT_KEEP 8u                   /* l_100, gun, chr, overlay and spare after us */
#define TS_CONTAINER_MAX 0x00080000u
#define TS_PAK_LEN 21u
#define TS_TEX_PAK_LEN 25u
#define TS_DISCO_ROW 6u                    /* MAPMAKER_VARIANT_ROW (pc_mapmaker_sky.inc) */
static const char ts_editor_pak[TS_PAK_LEN]="pak/arcade/l_102.pak";
static const char ts_editor_textures[TS_TEX_PAK_LEN]="pak/arcademem/l_102m.pak";

extern void sub_0003D7B0(void),sub_00071800(void),sub_000622D0(void),sub_00062260(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

static struct {
    unsigned long long rows, mounts, fallbacks, logged, disco_rows, textures;
    uint32_t disco_text;                   /* guest: "Disco" */
} ts_state;

static int ts_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static void ts_log(const char *what,uint32_t a,uint32_t b)
{
    if(ts_state.logged++>=8u) return;
    fprintf(stderr,"[mapmaker] tilesets: %s (%u, %u)\n",what,a,b);
    fflush(stderr);
}
/* Give Abstract Lab's image (stock 0) for the map info panel. */
static void ts_image(void)
{
    uint32_t a=TS_TABLE+TS_ABSTRACT*TS_STRIDE+TS_IMAGE_FIELD;
    if(WORD_AT(a)==0u) WORD_AT(a)=TS_LAB_IMAGE;
}
/* Data of the map info chunk 0x00C77668 (tileset word +0xA), or 0. */
static uint32_t ts_info(uint32_t c)
{
    uint32_t off,tag,size;
    if(!MAPMAKER_PTR_OK(c,16u) || WORD_AT(c)!=0x00C77667u) return 0;
    for(off=8u;off+8u<=TS_CONTAINER_MAX;off+=8u+size) {
        if(!MAPMAKER_PTR_OK(c+off,8u)) return 0;
        tag=WORD_AT(c+off);size=WORD_AT(c+off+4u);
        if(tag==0x00C7766Au || size>TS_CONTAINER_MAX) return 0;
        if(tag==0x00C77668u) return size>=0x0Cu && MAPMAKER_PTR_OK(c+off+8u,size)?c+off+8u:0u;
    }
    return 0;
}

/* The Disco row's label, copied once into guest memory; 0 without memory. */
static uint32_t ts_disco_text(void)
{
    static const char label[]="Disco";
    uint32_t p;
    if(ts_state.disco_text) return ts_state.disco_text;
    if(!(p=xbox_HeapAlloc((uint32_t)sizeof label,4u))) { ts_log("no memory for the Disco row label",0,0);return 0; }
    memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),label,sizeof label);
    return ts_state.disco_text=p;
}

/* WRAP sub_00071800 (menu: add a list row; cdecl text, value). */
void pc_mapmaker_tilesets_menu_row(void (*original)(void))
{
    uint32_t value,eax,text;
    if(!MAPMAKER_PTR_OK(g_esp,12u) || WORD_AT(g_esp)!=TS_RET_MENU_ROW) { original();return; }
    value=WORD_AT(g_esp+8u);
    original();
    if(value!=TS_LAST_STOCK_ROW || !ts_on() || WORD_AT(TS_SCENE)!=102u) return;
    eax=g_eax;
    if(ts_disco_text()) {                                /* the Disco variant of Virtual */
        guest_call(sub_00071800,2,ts_state.disco_text,TS_DISCO_ROW);
        if(!ts_state.disco_rows++) ts_log("Disco (Virtual variant) offered in Map Settings",TS_DISCO_ROW,WORD_AT(TS_CURRENT));
    }
    text=guest_call(sub_0003D7B0,1,WORD_AT(TS_TABLE+TS_ABSTRACT*TS_STRIDE));
    if(text) {
        guest_call(sub_00071800,2,text,TS_ABSTRACT);
        if(!ts_state.rows++) ts_log("Abstract offered in Map Settings",TS_ABSTRACT,WORD_AT(TS_CURRENT));
    } else ts_log("no text for Abstract; row not added",WORD_AT(TS_TABLE),0);
    ts_image();
    g_eax=eax;
}

/* Mount a pak from a path below the guest stack: whole on the level heap as
 * the stock tileset paks (whole 1) or index-only as chr.pak (whole 0); 1
 * when it is mounted. */
static int ts_mount_pak(const char *pak,uint32_t len,uint32_t whole)
{
    uint32_t s,mounted=0,esp=g_esp;
    int ok;
    if(!MAPMAKER_PTR_OK(esp-0x400u,0x400u)) return 0;
    s=(esp-64u)&~3u;                                     /* the path, below the guest stack */
    memcpy((void *)((uintptr_t)s+g_xbox_mem_offset),pak,len);
    g_esp=s;                                             /* calls run below the string */
    if((int32_t)guest_call(sub_00062260,1,s)!=-1) ok=1;  /* already mounted */
    else if((mounted=WORD_AT(TS_MOUNTS))+TS_MOUNT_KEEP>TS_MOUNT_MAX) {
        ts_log(whole?"pak mount table too full for l_102.pak":"pak mount table too full for l_102m.pak",mounted,TS_MOUNT_MAX);
        ok=0;
    } else if((int32_t)guest_call(sub_000622D0,3,s,whole,1u)>=0) ok=1;
    else {
        ts_log(whole?"pak/arcade/l_102.pak did not mount":"pak/arcademem/l_102m.pak did not mount",mounted,0);
        ok=0;
    }
    g_esp=esp;
    return ok;
}
/* Mount l_102.pak like the stock tileset paks; 1 when it is mounted. Its tile
 * models' 87 textures are only in the editor's memory pak l_102m.pak, which
 * play (scene 100 mounts arcademem/l_100m.pak, sub_00062610(scene, 1) at
 * 0x0006289E) never mounts: it is mounted index-only too (its 24 names shared
 * with l_100m and 31 with gun.pak are byte-identical). Without it the map
 * still plays, its tiles untextured. */
static int ts_mount_editor_pak(void)
{
    if(!ts_mount_pak(ts_editor_pak,TS_PAK_LEN,1u)) return 0;
    if(ts_mount_pak(ts_editor_textures,TS_TEX_PAK_LEN,0u)) {
        if(!ts_state.textures++) ts_log("Abstract map: mounted pak/arcademem/l_102m.pak for its tile textures",WORD_AT(TS_MOUNTS),0);
    } else ts_log("Abstract map without l_102m.pak; its tiles have no textures",WORD_AT(TS_MOUNTS),0);
    return 1;
}

/* WRAP sub_00338170 (tileset index -> stream pak path; cdecl, one argument). */
void pc_mapmaker_tilesets_play_pak(void (*original)(void))
{
    uint32_t info;
    if(!MAPMAKER_PTR_OK(g_esp,8u) || WORD_AT(g_esp)!=TS_RET_PAK_PATH || WORD_AT(g_esp+4u)!=TS_ABSTRACT) {
        original();
        return;
    }
    ts_image();
    if(ts_mount_editor_pak()) {
        if(!ts_state.mounts++) ts_log("Abstract map: mounted pak/arcade/l_102.pak for its tiles and pads",WORD_AT(TS_MOUNTS),0);
    } else {
        ts_state.fallbacks++;
        ts_log("Abstract map without l_102.pak; playing it as Lab",WORD_AT(TS_MOUNTS),TS_FALLBACK);
        if((info=ts_info(WORD_AT(TS_CONTAINER)))!=0)
            WORD_AT(info+0x08u)=(WORD_AT(info+0x08u)&0xFFFFu)|(TS_FALLBACK<<16);
        if(WORD_AT(TS_CURRENT)==TS_ABSTRACT) WORD_AT(TS_CURRENT)=TS_FALLBACK;
        WORD_AT(g_esp+4u)=TS_FALLBACK;
    }
    original();
}
