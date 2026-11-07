/* US 45410066 MapMaker "Disco" tileset: a variant of Virtual (table index 5)
 * whose tiles and props show the Disco arcade level's own textures, read at
 * runtime from the player's game data. Survey, pairing and user guide:
 * docs/mapmaker/mapmaker-disco-variant.md. Included at the end of
 * pc_mapmaker_skymusic.inc (smx_mount, smx_path, smx_on, smx_scene).
 *
 * Map format: header word +0xA stays 5 (Virtual) and info flag bit 31
 * (MAPMAKER_VARIANT_FLAG, pc_mapmaker_sky.inc, proven free there) marks the
 * variant; a stock game or an older build plays such a map as plain Virtual.
 * The editor keeps the choice in editor +0x204 (the Map Settings tileset
 * list, read only by sub_002FC0A0): 6 = Disco (MAPMAKER_VARIANT_ROW).
 *
 * Stock facts (CONFIRMED in the XBE and the game paks):
 *  - A model file begins with the offset of its texture list: 16-byte rows
 *    {id, id, 0, 0} (24-bit id; 0xFFFFFFFF ends the list). sub_0008E3E0
 *    (cdecl list, file) is the first call on a model's list on every MapMaker
 *    route: the load pass sub_00096220 (0x0009628F, before sub_0008DD10 and
 *    the queue sub_000637E0 "textures/%06x.xbt"), the creation pass
 *    sub_00096330 (0x00096354), sub_00089DE0 (0x00089E3F) and sub_00097FC0
 *    (0x00098005). The texture pass later replaces word 0 by the loaded
 *    texture's row, so a list whose words 0 and 1 differ was registered.
 *  - Virtual's 29 tile models list only 9 textures; the 5 opaque ones are
 *    aliased (table below), the 4 glow masks are kept. No model of l_100,
 *    l_102, gun, chr, l_100m or l_102m lists a Virtual id, and none of those
 *    paks holds a Virtual texture, so rewriting by id touches only tileset6
 *    models.
 *  - The Disco textures are in pak/arcademem/l_56_arm.pak (283 entries); its
 *    names shared with l_100m (162), gun (27) and l_102m (19) are
 *    byte-identical, none with l_100, l_102, chr or tileset6.
 *  - Mount table 0x00907000 (paktable.json, 64 x 0x1C): +8 entry count,
 *    +0x10 the index (16-byte rows, CRC32 of the name at +0), also for an
 *    index-only mount (sub_000620E0, 0x000621D0..0x00062226).
 *  - Editor tileset switch: sub_002FC0A0 (only caller 0x00313311, the Map
 *    Settings close) copies editor +0x204 to [0x00784020] and sets editor
 *    +0x238 |= 0x12; the editor tick sub_002FC200 frees the palette props
 *    and pieces' previews and calls sub_00338180(tileset, 0) (0x002FC265; also
 *    0x00311FD5) -> sub_00061E90(stream set [0x0052EC88 + t*0x18]), which does
 *    nothing when that set is already current ([0x007DE314]); otherwise it
 *    unloads the current set with sub_00061D80 (-5 = busy) and starts the new.
 *
 * Hooks (TSFP_MAPMAKER_LIMITS=1):
 *  - textures (WRAP sub_0008E3E0, before the original): while the variant is
 *    live (play: a Disco map whose pak mounted this level; editor: the
 *    Virtual stream loaded as Disco) a fresh list's rows with an aliased
 *    Virtual id get the Disco id in both words. Ids missing from the mounted
 *    pak keep their stock id.
 *  - menu_close (WRAP sub_002FC0A0): editor +0x204 = 6 is handed to the
 *    original as 5 (base Virtual) and put back after; switching between
 *    Virtual and Disco sets the stock reload flags 0x12.
 *  - stream (WRAP sub_00338180, scene 102): for tileset 5 with the Disco
 *    choice the pak is mounted first; when the Virtual set is already
 *    current but was loaded for the other look, it is unloaded the stock way
 *    (sub_00061D80) and marked not current, so the original reloads it and
 *    its models' lists are read again.
 *  - play (from pc_mapmaker_skymusic.inc): the map start mounts the pak
 *    index-only and checks the ids (dsc_level_start), the new level resets
 *    (dsc_level_reset), the first weather frame logs the rewrites
 *    (dsc_frame). Defaults (sky 59, music 0xBA) are in skymusic/sky. */
#include <stdio.h>
#include <string.h>

#define DSC_PAK "pak/arcademem/l_56_arm.pak"
#define DSC_CURRENT 0x00784020u            /* current tileset (editor and play) */
#define DSC_ED_TILESET MAPMAKER_VARIANT_ED_TILESET
#define DSC_ED_FLAGS 0x238u
#define DSC_ED_RELOAD 0x12u                /* sub_002FC0A0 0x002FC0B9 */
#define DSC_ED_SIZE 0x240u
#define DSC_STREAM_SETS 0x0052EC88u        /* 6 x 0x18, +0 stream set name (sub_00338180) */
#define DSC_STREAM_STRIDE 0x18u
#define DSC_TILESETS 6u
#define DSC_STREAM_NOW 0x007DE314u         /* current set (sub_00066560) */
#define DSC_STREAM_PREV 0x007DE310u        /* previous set (sub_00066580) */
#define DSC_STREAM_BUSY 0xFFFFFFFBu        /* sub_00061D80: -5, still loading */
#define DSC_PAKTABLE 0x00907000u           /* paktable.json: 64 slots of 0x1C */
#define DSC_SLOT 0x1Cu
#define DSC_SLOT_COUNT 0x08u
#define DSC_SLOT_INDEX 0x10u
#define DSC_SLOTS 64u
#define DSC_INDEX_MAX 0x4000u
#define DSC_LIST_MAX 256u                  /* rows scanned per texture list */
#define DSC_LOGS 48u

extern void sub_00061D80(void);

/* Virtual tile texture -> Disco texture (survey pairing table). */
static const struct { uint32_t from, to; const char *what; } dsc_alias[]={
    {0xE871CCu,0x7DDE70u,"main dark panel -> starry dark surface"},
    {0x25AE6Eu,0x0D9139u,"glow band -> gold retro wallpaper"},
    {0x294554u,0x7B0662u,"framed square -> red plush"},
    {0x79B187u,0x3C2652u,"light framed panel -> dark leather"},
    {0x666443u,0xF05B7Cu,"grey trim -> tiger print"},
};
#define DSC_ALIASES ((uint32_t)(sizeof dsc_alias/sizeof dsc_alias[0]))

static struct {
    uint32_t live[DSC_ALIASES];      /* Disco id per alias, 0 = keep the stock id */
    uint32_t live_n;
    int level_tried, play_ready;     /* this level (scene 100) */
    int stream_variant;              /* the Virtual editor stream was loaded as Disco */
    int editor_ready;                /* scene 102: alias the lists read now */
    uint32_t lists, entries;         /* this level / stream: lists and rows rewritten */
    int frame_logged, editor_logged, missing_logged, mount_logged;
    unsigned logged;
} dsc;

static void dsc_log(const char *fmt,uint32_t a,uint32_t b,uint32_t c)
{
    if(dsc.logged++>=DSC_LOGS) return;
    fputs("[mapmaker] disco: ",stderr);
    fprintf(stderr,fmt,a,b,c);
    fputc('\n',stderr);
    fflush(stderr);
}

/* CRC-32 (zlib) of a pak entry name, as the game hashes it (sub_00150530). */
static uint32_t dsc_crc(const char *s)
{
    uint32_t c=0xFFFFFFFFu;
    int k;
    for(;*s;s++) {
        c^=(uint8_t)*s;
        for(k=0;k<8;k++) c=c&1u?(c>>1)^0xEDB88320u:c>>1;
    }
    return ~c;
}
/* 1 when the mounted slot's index lists textures/<id>.xbt. */
static int dsc_slot_has(uint32_t slot,uint32_t id)
{
    char name[24];
    uint32_t base=DSC_PAKTABLE+slot*DSC_SLOT,n,index,k,crc;
    if(slot>=DSC_SLOTS) return 0;
    n=WORD_AT(base+DSC_SLOT_COUNT);index=WORD_AT(base+DSC_SLOT_INDEX);
    if(!n || n>DSC_INDEX_MAX || !MAPMAKER_PTR_OK(index,n*16u)) return 0;
    snprintf(name,sizeof name,"textures/%06x.xbt",(unsigned)id);
    crc=dsc_crc(name);
    for(k=0;k<n;k++) if(WORD_AT(index+k*16u)==crc) return 1;
    return 0;
}
/* Mount the Disco pak index-only (smx_mount: kept 8 slots free) and keep the
 * aliases whose texture it holds. Returns how many; 0 = plain Virtual. */
static uint32_t dsc_prepare(void)
{
    uint32_t path,k;
    int32_t slot;
    memset(dsc.live,0,sizeof dsc.live);dsc.live_n=0;
    if(!smx_mount(DSC_PAK)) {
        if(!dsc.mount_logged++) dsc_log("pak/arcademem/l_56_arm.pak not mounted; plain Virtual textures kept",0,0,0);
        return 0;
    }
    if(!(path=smx_path(DSC_PAK)) || (slot=(int32_t)guest_call(sub_00062260,1,path))<0) {
        dsc_log("l_56_arm.pak not found in the mount table; plain Virtual textures kept",0,0,0);
        return 0;
    }
    for(k=0;k<DSC_ALIASES;k++) {
        if(dsc_slot_has((uint32_t)slot,dsc_alias[k].to)) { dsc.live[k]=dsc_alias[k].to;dsc.live_n++; }
        else if(dsc.missing_logged++<(int)DSC_ALIASES)
            dsc_log("textures/%06x.xbt missing from l_56_arm.pak; Virtual %06x kept (slot %u)",dsc_alias[k].to,dsc_alias[k].from,(uint32_t)slot);
    }
    return dsc.live_n;
}

/* Rewrite one fresh texture list; returns the rows changed. */
static uint32_t dsc_rewrite(uint32_t list)
{
    uint32_t k,j,id,n=0;
    for(k=0;k<DSC_LIST_MAX && MAPMAKER_PTR_OK(list+k*16u,16u);k++) {
        uint32_t e=list+k*16u;
        if((id=WORD_AT(e))==0xFFFFFFFFu) break;
        if(WORD_AT(e+4u)!=id) continue;          /* registered (row in word 0) or embedded */
        for(j=0;j<DSC_ALIASES;j++)
            if(dsc.live[j] && id==dsc_alias[j].from) { WORD_AT(e)=WORD_AT(e+4u)=dsc.live[j];n++;break; }
    }
    return n;
}
static int dsc_active(void)
{
    uint32_t s=smx_scene();
    if(s==100u) return dsc.play_ready;
    if(s==102u) return dsc.editor_ready && WORD_AT(DSC_CURRENT)==MAPMAKER_VARIANT_BASE;
    return 0;
}

/* WRAP sub_0008E3E0 (cdecl list, file: first pass over a model's texture list). */
void pc_mapmaker_disco_textures(void (*original)(void))
{
    uint32_t list,n;
    if((dsc.play_ready || dsc.editor_ready) && MAPMAKER_PTR_OK(g_esp,12u) && dsc_active() &&
       MAPMAKER_PTR_OK(list=WORD_AT(g_esp+4u),16u) && (n=dsc_rewrite(list))!=0) {
        dsc.lists++;dsc.entries+=n;
    }
    original();
}

/* A new level (pc_mapmaker_skymusic_mount): nothing tried yet. */
static void dsc_level_reset(void)
{
    dsc.level_tried=0;dsc.play_ready=0;dsc.frame_logged=0;
    if(smx_scene()!=102u) { dsc.lists=0;dsc.entries=0; }
}
/* The map start of a Disco map (scene 100, before the load pass reads the
 * tileset's models): mount and check once per level; one line says so. */
static void dsc_level_start(uint32_t info)
{
    uint32_t n;
    if(dsc.level_tried || smx_scene()!=100u || !mapmaker_variant_disco(info)) return;
    dsc.level_tried=1;dsc.lists=0;dsc.entries=0;
    n=dsc_prepare();
    dsc.play_ready=n!=0;
    if(n) dsc_log("variant active (Virtual + Disco): l_56_arm.pak mounted, %u of %u texture ids aliased",n,DSC_ALIASES,0);
    else dsc_log("variant marked but its textures are missing: playing plain Virtual (0 of %u ids aliased)",DSC_ALIASES,0,0);
}
/* First weather frame of a Disco level: how many rows the alias changed. */
static void dsc_frame(void)
{
    if(!dsc.play_ready || dsc.frame_logged || smx_scene()!=100u) return;
    dsc.frame_logged=1;
    dsc_log("%u texture list rows rewritten in %u model lists this level (%u ids live)",dsc.entries,dsc.lists,dsc.live_n);
    if(!dsc.entries) dsc_log("no Virtual list was rewritten: tile textures were registered before the mount",0,0,0);
}

/* WRAP sub_002FC0A0 (Map Settings closed: editor +0x204 -> [0x00784020]). */
void pc_mapmaker_disco_menu_close(void (*original)(void))
{
    uint32_t ed=WORD_AT(SMX_EDITOR),before,after;
    int disco;
    if(smx_scene()!=102u || !MAPMAKER_PTR_OK(ed,DSC_ED_SIZE)) { original();return; }
    disco=WORD_AT(ed+DSC_ED_TILESET)==MAPMAKER_VARIANT_ROW;
    if(disco) WORD_AT(ed+DSC_ED_TILESET)=MAPMAKER_VARIANT_BASE;
    before=WORD_AT(DSC_CURRENT);
    original();
    after=WORD_AT(DSC_CURRENT);
    if(disco) WORD_AT(ed+DSC_ED_TILESET)=MAPMAKER_VARIANT_ROW;
    if(smx_on() && before==MAPMAKER_VARIANT_BASE && after==MAPMAKER_VARIANT_BASE && disco!=dsc.stream_variant) {
        WORD_AT(ed+DSC_ED_FLAGS)|=DSC_ED_RELOAD;
        dsc_log("editor: Virtual <-> Disco switch, tile models reload (Disco %u, loaded as Disco %u, tileset %u)",(uint32_t)disco,(uint32_t)dsc.stream_variant,after);
    }
}

/* WRAP sub_00338180 (cdecl tileset, flag: start the editor's tileset stream). */
void pc_mapmaker_disco_stream(void (*original)(void))
{
    uint32_t t,name,ed,r,n=0;
    int want,forced=0,current;
    if(!MAPMAKER_PTR_OK(g_esp,12u) || smx_scene()!=102u || (t=WORD_AT(g_esp+4u))>=DSC_TILESETS) { original();return; }
    name=WORD_AT(DSC_STREAM_SETS+t*DSC_STREAM_STRIDE);
    ed=WORD_AT(SMX_EDITOR);
    want=t==MAPMAKER_VARIANT_BASE && smx_on() && MAPMAKER_PTR_OK(ed,DSC_ED_SIZE) &&
         WORD_AT(ed+DSC_ED_TILESET)==MAPMAKER_VARIANT_ROW;
    if(want && !(n=dsc_prepare())) {
        want=0;
        dsc_log("editor: Disco textures missing; the editor shows plain Virtual (tileset %u)",t,0,0);
    }
    current=WORD_AT(DSC_STREAM_NOW)==name;
    if(current && t==MAPMAKER_VARIANT_BASE && want!=dsc.stream_variant) {
        r=guest_call(sub_00061D80,1,name);
        if(r==DSC_STREAM_BUSY) dsc_log("editor: tileset stream still loading; look kept until the next switch (Disco %u)",(uint32_t)want,0,0);
        else { WORD_AT(DSC_STREAM_NOW)=0xFFFFFFFFu;forced=1;current=0; }
    }
    if(current) want=dsc.stream_variant;   /* nothing is read again: the loaded look stays */
    dsc.editor_ready=want;
    if(!current) { dsc.lists=0;dsc.entries=0; }
    original();
    if(!current && g_eax) dsc.stream_variant=want;
    else if(!current) dsc.editor_ready=dsc.stream_variant=0;
    if(forced) WORD_AT(DSC_STREAM_PREV)=name;   /* as the stock switch leaves it */
    if(want && !current && g_eax && !dsc.editor_logged++)
        dsc_log("editor: Disco look on: l_56_arm.pak mounted, %u of %u texture ids aliased (reload %u)",n,DSC_ALIASES,(uint32_t)forced);
}
