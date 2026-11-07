/* US 45410066 MapMaker story creatures: one generic deployer for story-level
 * creatures as MapMaker Story AIs, driven by the rows of
 * pc_mapmaker_creatures.def (picker row, character id, look, AI class, anim
 * pak and keep-list, optional level hook). Area `creatures`; every hook is
 * inert unless TSFP_MAPMAKER_LIMITS=1, and only acts in MapMaker play (scene
 * 100, story mode byte [0x007DE455] == 10) for set members that are a row's
 * id. TSFP_MAPMAKER_CREATURES=0 keeps every creature on the MapMaker bot AI.
 * Docs: docs/mapmaker/mapmaker-creatures.md. Tests:
 * tests/mapmaker/test_mapmaker_creatures.py.
 *
 * Editor (scene 102): the rows are added to the custom-set picker by
 * pc_mapmaker_realbosses.inc (rbs_extra, from the same .def): label, pick
 * (id, variant -1), slot reload, Story AI name and preview. Save format
 * unchanged: a stock set member (word id, sbyte variant -1); a stock or older
 * build spawns the id with the MapMaker bot class on its own model.
 *
 * Play, per level (CONFIRMED in the decode, the test names the sites):
 *  - anims (sub_001C8540, the level's anim pak mount; sub_0005C420 calls it at
 *    0x0005C501 and registers every mounted anim pak's names with
 *    sub_001CA2F0 at 0x0005C558): after the original, for each row whose id
 *    the map's own set holds (slots 1-10 in the map-info chunk, 11-20 in the
 *    sets chunk), mount its pak whole with sub_0005A130 (= sub_000622D0(path,
 *    1, 1), the stock per-level anim pak call) unless mounted, while 8 of the
 *    64 mount slots stay free. The stock teardown unmounts it. Whole, not
 *    index-only: sub_00057F30 keeps a pointer into the anim file and fixes it
 *    up in place, so the file must stay in memory (as every boss pak). The
 *    anim slot budget (pc_mapmaker_animslots.inc) registers only the row's
 *    entries. Then the row's level hook, if any.
 *  - request (sub_0015E7C0, story level init): after the original, for a row
 *    with a look whose look id the set does not hold, request the look's
 *    models (character table +4/+8, below 0x9D0) with sub_000A5E40 for the
 *    level's load pass, as the set preload sub_000D7780 does for members.
 *  - create (sub_000DAC30, cdecl character id, ..., pool slot): for a row id
 *    with a look whose models are loaded or read (resource row and request
 *    data, as the khallos area checks), the character table's +4/+8 for the
 *    id hold the look's models for this call only, so the creature is made
 *    with them (skeleton from the model); else its own models.
 *  - ai_start (sub_0015B830(a); story mode copies spawn record +0/+4 to AI
 *    +0x10/+0x14 at 0x0015B9A3): before the original, a pool entry whose
 *    character a+0x48 is a row id, whose pak is mounted and every kept anim
 *    id resolves (id map 0x007E03C0, not 0xFFFF): record +0 = class_lo,
 *    +4 = class_hi (MapMaker wrote 0x11000000/0 at 0x002421A7). Otherwise the
 *    bot class stays and the reason is logged. Several per map: no per-AI
 *    state anywhere (the handlers keep theirs in the AI entry).
 * Restart (sub_001CDD60): nothing to do. The kept entries' slot words have
 * no 0x4000 flag, so the restart's sub_0005C2F0 keeps them mapped; the
 * record keeps its class for the restarted AI start.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp, g_eax,
 * g_xbox_mem_offset and pc_mapmaker_limits.inc. Statics are prefixed crt/CRT_. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define CRT_SCENE 0x0079094Cu
#define CRT_MODE 0x007DE455u            /* byte: 10 = story */
#define CRT_CONTAINER 0x00784014u
#define CRT_MAGIC 0x00C77667u
#define CRT_TAG_INFO 0x00C77668u        /* map info: +0x14 word set, +0x16 count, +0x18 members (4 B, word id) */
#define CRT_TAG_SETS 0x00C776F5u        /* sets area: +0 word 1, +2 10, +3 count, +4 members (slots 11-20) */
#define CRT_TAG_END 0x00C7766Au
#define CRT_SCAN 0x00080000u
#define CRT_SET_MAP 0x11u               /* the map's own set */
#define CRT_SLOTS 20u
#define CRT_INFO_SLOTS 10u
#define CRT_CHARS 0x0053A3CCu           /* character table, 0x30 per id: +4 model, +8 second model */
#define CRT_CHAR_STRIDE 0x30u
#define CRT_CHAR_MODEL 4u
#define CRT_NO_LOOK 0xFFu
#define CRT_MODEL_MAX 0x9D0u            /* sub_000A5E40 ignores ids >= this */
#define CRT_RESMAP 0x008CA000u          /* model -> resource row (moved there by the rooms area) */
#define CRT_RESMAP_MODELS 0xA00u
#define CRT_RES 0x00791D4Cu             /* resource table, 0x30 per entry */
#define CRT_RES_LOADED 0x4u
#define CRT_RES_PATH 0x10u
#define CRT_RES_LOD 0x14u
#define CRT_REQ_DATA 0x10u
#define CRT_MOUNTS 0x006D7880u          /* mounted paks */
#define CRT_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define CRT_MOUNT_KEEP 8u
#define CRT_ANIM_MAP 0x007E03C0u        /* int16 anim slot per id, 0xFFFF free */
#define CRT_ANIM_IDS 0xC03u
#define CRT_ENTRY 0x18u                 /* registration entry: +0x10 anim id */
#define CRT_ENTRY_ID 0x10u
#define CRT_ACTOR 0xEA0u
#define CRT_ACTOR_CHAR 0x48u
#define CRT_ACTOR_RECORD 0xBF0u
#define CRT_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define CRT_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_0005A130(void),sub_00062260(void),sub_000A5E40(void),sub_000631C0(void);

typedef struct {
    uint32_t row, id, look, preview;
    const char *label;
    uint32_t lo, hi, pak;
    const char *pak_name;
    uint32_t table, count, first, n;
    void (*level)(unsigned kind);
} CrtKind;
static const CrtKind crt_kinds[]={
#define MAPMAKER_CREATURE(row,id,look,preview,label,lo,hi,pak,pak_name,table,count,first,n,level) \
    {row,id,look,preview,label,lo,hi,pak,pak_name,table,count,first,n,level},
#include "pc_mapmaker_creatures.def"
#undef MAPMAKER_CREATURE
};
#define CRT_KINDS (sizeof crt_kinds/sizeof crt_kinds[0])

static struct {
    int read, optout;
    unsigned logged;
    unsigned started[CRT_KINDS], refused[CRT_KINDS], made[CRT_KINDS], own[CRT_KINDS];
} crt;

static void crt_log(const char *what,unsigned k,uint32_t a,uint32_t b)
{
    if(crt.logged>=32u) return;
    crt.logged++;
    fprintf(stderr,"[mapmaker] creatures: %s: %s (0x%X, 0x%X)\n",k<CRT_KINDS?crt_kinds[k].label:"",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int crt_on(void)
{
    mapmaker_limits_read();
    if(!crt.read) {
        const char *v=getenv("TSFP_MAPMAKER_CREATURES");
        crt.read=1;
        crt.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int crt_play(void) { return crt_on() && WORD_AT(CRT_SCENE)==100u && CRT_B(CRT_MODE)==10u; }
static unsigned crt_kind_of(uint32_t id)
{
    unsigned k;
    for(k=0;k<CRT_KINDS && crt_kinds[k].id!=id;k++) {}
    return k;
}

/* The map's own set, slots 1-20 (0xFFFF empty). 1: read; 0: a preset or
 * profile set (no creature); -1: the container cannot be read. */
static int crt_set(uint16_t ids[CRT_SLOTS])
{
    uint32_t container=WORD_AT(CRT_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    for(k=0;k<CRT_SLOTS;k++) ids[k]=0xFFFFu;
    if(!container || !MAPMAKER_PTR_OK(container,CRT_SCAN) || WORD_AT(container)!=CRT_MAGIC) return -1;
    c=container+8u;end=container+CRT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==CRT_TAG_END || sz>end-c-8u) break;
        if(tag==CRT_TAG_INFO && sz>=0x18u && !info) { info=d;isz=sz; }
        else if(tag==CRT_TAG_SETS && sz>=0x30u && !extra && CRT_H(d)==1u && CRT_B(d+2u)==CRT_INFO_SLOTS &&
                CRT_B(d+3u)>=1u && CRT_B(d+3u)<=CRT_SLOTS-CRT_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(CRT_H(info+0x14u)!=CRT_SET_MAP) return 0;
    n=CRT_B(info+0x16u);
    if(n>CRT_INFO_SLOTS) n=CRT_INFO_SLOTS;
    if(0x18u+n*4u>isz) n=(isz-0x18u)/4u;
    for(k=0;k<n;k++) ids[k]=CRT_H(info+0x18u+k*4u);
    for(k=0;extra && k<CRT_B(extra+3u);k++) ids[CRT_INFO_SLOTS+k]=CRT_H(extra+4u+k*4u);
    return 1;
}
static int crt_has(const uint16_t ids[CRT_SLOTS],uint32_t id)
{
    unsigned k;
    for(k=0;k<CRT_SLOTS;k++) if(ids[k]==id) return 1;
    return 0;
}
static int crt_mounted(uint32_t path) { return (int32_t)guest_call(sub_00062260,1,path)!=-1; }
/* Kept anim ids of a row that resolve now (id map), *of: how many it keeps. */
static unsigned crt_anims(unsigned k,unsigned *of)
{
    const CrtKind *r=&crt_kinds[k];
    uint32_t i,id;
    unsigned ok=0;
    *of=r->n;
    for(i=0;i<r->n;i++) {
        uint32_t e=r->table+(r->first+i)*CRT_ENTRY;
        if(!MAPMAKER_PTR_OK(e,CRT_ENTRY)) continue;
        id=CRT_H(e+CRT_ENTRY_ID);
        if(id<CRT_ANIM_IDS && CRT_H(CRT_ANIM_MAP+id*2u)!=0xFFFFu) ok++;
    }
    return ok;
}
static uint32_t crt_model(uint32_t id,unsigned i) { return WORD_AT(CRT_CHARS+id*CRT_CHAR_STRIDE+CRT_CHAR_MODEL+i*4u); }
static uint32_t crt_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(CRT_RES),e;
    if(model>=CRT_RESMAP_MODELS) return 0;
    i=(int16_t)CRT_H(CRT_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
static int crt_path_ready(uint32_t path)
{
    uint32_t r;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+CRT_REQ_DATA)!=0;
}
/* The look's models (+4, and +8 unless none) are loaded or read this level. */
static int crt_look_ready(uint32_t look)
{
    uint32_t m,e,path;
    unsigned i;
    for(i=0;i<2u;i++) {
        if((m=crt_model(look,i))==0xFFFFFFFFu && i) continue;
        if(m>=CRT_MODEL_MAX || !(e=crt_resource(m))) return 0;
        if(WORD_AT(e+CRT_RES_LOADED)) continue;
        if(!crt_path_ready(WORD_AT(e+CRT_RES_PATH)) || ((path=WORD_AT(e+CRT_RES_LOD))!=0 && !crt_path_ready(path))) return 0;
    }
    return 1;
}

/* WRAP sub_001C8540 (the level's anim paks; sub_001CA2F0 registers the names next). */
void pc_mapmaker_creatures_anims(void (*original)(void))
{
    uint16_t ids[CRT_SLOTS];
    uint32_t used;
    unsigned k;
    original();
    if(!crt_play()) return;
    crt.logged=0;
    memset(crt.started,0,sizeof crt.started);memset(crt.refused,0,sizeof crt.refused);memset(crt.made,0,sizeof crt.made);memset(crt.own,0,sizeof crt.own);
    if(crt.optout || crt_set(ids)!=1) return;
    for(k=0;k<CRT_KINDS;k++) {
        const CrtKind *r=&crt_kinds[k];
        if(!crt_has(ids,r->id)) continue;
        if(crt_mounted(r->pak)) crt_log("anim pak already mounted",k,r->pak,0);
        else if((used=WORD_AT(CRT_MOUNTS))+CRT_MOUNT_KEEP>CRT_MOUNT_MAX) { crt_log("pak mount table full; keeps the bot AI",k,used,CRT_MOUNT_MAX);continue; }
        else if(!guest_call(sub_0005A130,1,r->pak)) { crt_log("anim pak did not mount; keeps the bot AI",k,used,0);continue; }
        else {
            fprintf(stderr,"[mapmaker] creatures: %s: mounted %s.pak (anim entries %u..%u kept, mount slots used %u)\n",
                    r->label,r->pak_name,(unsigned)r->first,(unsigned)(r->first+r->n-1u),(unsigned)WORD_AT(CRT_MOUNTS));
            fflush(stderr);
        }
        if(r->level) r->level(k);
    }
}

/* WRAP sub_0015E7C0 (story level init, no args): request each look's models. */
void pc_mapmaker_creatures_request(void (*original)(void))
{
    uint16_t ids[CRT_SLOTS];
    uint32_t m;
    unsigned k,i;
    original();
    if(!crt_play() || crt.optout || crt_set(ids)!=1) return;
    for(k=0;k<CRT_KINDS;k++) {
        const CrtKind *r=&crt_kinds[k];
        if(r->look==CRT_NO_LOOK || !crt_has(ids,r->id) || crt_has(ids,r->look)) continue;   /* the set preload asks for a member's */
        for(i=0;i<2u;i++) if((m=crt_model(r->look,i))<CRT_MODEL_MAX) guest_call(sub_000A5E40,1,m);
        crt_log("requested the look's models",k,crt_model(r->look,0),crt_model(r->look,1));
    }
}

/* WRAP sub_000DAC30 (cdecl character id, ..., pool slot -> object or 0). */
void pc_mapmaker_creatures_create(void (*original)(void))
{
    uint32_t id,c,keep[2];
    unsigned k,i;
    if(!MAPMAKER_STACK_OK(8) || (k=crt_kind_of(id=WORD_AT(g_esp+4u)))>=CRT_KINDS || crt_kinds[k].look==CRT_NO_LOOK ||
       !crt_play() || crt.optout) { original();return; }
    if(!crt_look_ready(crt_kinds[k].look)) {
        if(!crt.own[k]++) crt_log("look's models not loaded: made with its own",k,id,crt_kinds[k].look);
        original();
        return;
    }
    c=CRT_CHARS+id*CRT_CHAR_STRIDE+CRT_CHAR_MODEL;
    for(i=0;i<2u;i++) { keep[i]=WORD_AT(c+i*4u);WORD_AT(c+i*4u)=crt_model(crt_kinds[k].look,i); }
    original();
    for(i=0;i<2u;i++) WORD_AT(c+i*4u)=keep[i];
    if(!crt.made[k]++) crt_log("made with the look's models (character, object)",k,crt_kinds[k].look,g_eax);
}

/* WRAP sub_0015B830 (cdecl AI pool entry): give a creature its story class. */
void pc_mapmaker_creatures_ai_start(void (*original)(void))
{
    uint32_t a,rec;
    unsigned k,ok,of;
    if(!MAPMAKER_STACK_OK(8) || !crt_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,CRT_ACTOR) || (k=crt_kind_of(CRT_H(a+CRT_ACTOR_CHAR)))>=CRT_KINDS ||
       !MAPMAKER_PTR_OK(rec=WORD_AT(a+CRT_ACTOR_RECORD),12u)) { original();return; }
    if(crt.optout) { if(!crt.refused[k]++) crt_log("TSFP_MAPMAKER_CREATURES=0: keeps the bot AI",k,a,0); }
    else if(!crt_mounted(crt_kinds[k].pak)) { if(!crt.refused[k]++) crt_log("anim pak not mounted: keeps the bot AI",k,a,0); }
    else if((ok=crt_anims(k,&of))<of) { if(!crt.refused[k]++) crt_log("anims not registered (resolved, kept): keeps the bot AI",k,ok,of); }
    else {
        WORD_AT(rec)=crt_kinds[k].lo;WORD_AT(rec+4u)=crt_kinds[k].hi;
        if(!crt.started[k]++) {
            fprintf(stderr,"[mapmaker] creatures: %s (character 0x%X) gets its story AI, class 0x%X:0x%08X, %u of %u anims from %s.pak\n",
                    crt_kinds[k].label,(unsigned)crt_kinds[k].id,(unsigned)crt_kinds[k].hi,(unsigned)crt_kinds[k].lo,ok,of,crt_kinds[k].pak_name);
            fflush(stderr);
        }
    }
    original();
}
