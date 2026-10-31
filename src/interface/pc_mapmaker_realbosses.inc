/* US 45410066 MapMaker real story bosses: the giant trophy monster of story
 * mission 5 (the "deerbeast") as a MapMaker Story AI.
 * Docs: docs/mapmaker/mapmaker-realbosses.md. Research basis:
 * logs/limits/features/real-bosses.md (main checkout, ignored). Area
 * `realbosses`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1.
 *
 * The creature (CONFIRMED, US XBE data and tests/mapmaker/test_mapmaker_realbosses.py):
 * character id 0x15 (character table 0x0053A3CC + 0x15*0x30: voice set
 * 0x00538B08, model 0x5BE CHR_MANSION_DEERBEAST, LOD 0x5BF), no Arcade-table
 * row. Its story behaviour is AI class 0x80 (AI +0x14 = 0x80, +0x10 = 0:
 * dispatcher sub_0027D590 0x0027DB94 -> sub_00280FA0 at 0x0027DBE2); its
 * eight animations are named only by pak/stream/lv1/anim/lv1_3.pak (string
 * 0x0049D77C, registered by sub_001CA2F0 at 0x001CADA3 when that pak is
 * mounted). The model is in pak/chr.pak, which a MapMaker map in play
 * (scene 100) mounts; sub_000A5E40 skips the story-only flag 0x2000000 in
 * scene 100 (0x000A5E83) and honours it in the editor (scene 0x66, 0x000A5E93).
 *
 * Stage 1, editor (scene 102): the custom character set picker
 * (descriptor 0x0052374C) lists Arcade rows only. This area adds one more
 * row, "Giant Trophy Monster", with the synthetic row id RBS_ROW (150, one
 * past the 150-row Arcade table):
 *  - picker_end (sub_000781A0, only its call at 0x002C1FA0 that closes the
 *    picker list, return 0x002C1FA5): before the original, add the row the
 *    way the stock loop does (sub_00079D60(text, 150, 0x20900, 0, 0x002C1C90,
 *    0) then sub_00071C70(sub_00071630(), 0): enabled).
 *  - pick (sub_002C1C90, the row callback): the original stores row 150's
 *    "id" and "variant" from past the table; afterwards every work-copy slot
 *    (0x0075FD90, 10 x 0x1C) holding row 150 gets id 0x15 and variant -1
 *    (sub_002BE270(0x15) == 0: the stock reload drops variants of this id).
 *  - row_of (sub_002BE600(id, variant) -> row or -1): for id 0x15, the slot
 *    reload in sub_002C0490 (return 0x002C054C) and the Story AI edit
 *    screen's name lookup (sub_002BE650 at 0x002BE65F, called from
 *    sub_00303350 0x003033BB -> 0x003033C0) get row 150; the slot screen's
 *    preview selection in sub_002C1570 (returns 0x002C1805/0x002C1814) gets
 *    the Arcade trophy monster row (id 0xA7), so the preview panel
 *    (sub_002C2B40 reads [0x0078AEA8] as an Arcade row) never sees 150.
 *  - highlight (sub_002C1E10, picker highlight: [0x0078AEA8] = row id):
 *    150 becomes the trophy monster row as well.
 *  - row_text (sub_002BE300(row) -> text id; stock returns 0 for row >= 150):
 *    row 150 -> RBS_TEXT; text (sub_0003D7B0(id) -> string) returns the
 *    label for RBS_TEXT. Every other id/row runs the original first thing.
 * Nothing in the editor requests model 0x5BE: model_request
 * (sub_000A5E40) logs such a request in scene 102 (the stock call skips the
 * flagged model there) and changes nothing.
 *
 * Save format: unchanged. The slot is stored as a stock custom-set member
 * (map-info chunk 0x00C77668 +0x18: word id 0x15, sbyte variant -1). No Story
 * AI chunk byte is used (+0x21 belongs to the health levels, +0x29 to the
 * Boss flag). A stock game or an older fork build loading the map: the
 * picker/slot screen shows the member with the empty text 0 (sub_002BE600 ->
 * -1, sub_002BE300(-1) -> 0 -> 0x004766D8), and play spawns id 0x15 with the
 * stock MapMaker class (0x11000000/0: sub_000D6D00 covers ids 0x16..0xA3
 * only) on the human rig - the stage 1 behaviour. Story mission 5 holds the
 * same id in its own set, so the id itself is stock data.
 *
 * Stage 2, play (scene 100, story mode byte [0x007DE455] == 10; opt out with
 * TSFP_MAPMAKER_REAL_BOSS=0 for the model-only creature):
 *  - anims (sub_001C8540, per-level anim pak mount; sub_0005C420 calls it at
 *    0x0005C501 and registers the names of every mounted anim pak with
 *    sub_001CA2F0 at 0x0005C558): after the original, when the map's custom
 *    set has id 0x15 (or the container cannot be read), lv1_3.pak is not yet
 *    mounted (sub_00062260) and the 30-slot mount table [0x006D7880] has at
 *    least two free slots, mount it with sub_0005A130 (= sub_000622D0(path,
 *    1, 1): read whole, level heap) exactly as the stock per-level anim paks
 *    are; the stock level teardown (sub_00062720 from sub_0003D440) unmounts
 *    it with them.
 *  - ai_start (sub_0015B830(a), AI start; story mode copies spawn record
 *    +0/+4 to AI +0x10/+0x14 at 0x0015B9A3): before the original, for a pool
 *    entry whose character (a+0x48, the index sub_00280FA0 itself uses) is
 *    0x15 or whose entity model ([a]+0x94) is 0x5BE, and only when lv1_3.pak
 *    is mounted: record +0 = 0, +4 = 0x80 (MapMaker wrote 0x11000000/0 at
 *    0x002421A7). The AI also gets its Story AI chunk's Boss flag (+0x29,
 *    0xB0 | level) noted for the HUD hook. Without the pak it keeps the
 *    MapMaker class (stage 1) and says so once.
 *  - Level events: sub_00280FA0 and its damage callback sub_000BC4D0 cannot
 *    reach sub_002274C0 (static call graph, checked by the test), so no
 *    MapMaker logic marker can be fired by this creature; no guard is
 *    installed.
 *  - Boss bar: sub_00280FA0 sets the story focus [[0x007B0C7C]+0xFF4] itself
 *    (0x002811BE); sub_0014CC80 draws it for model 0x5BE with label text
 *    0xDCC (call 0x0014CED0 -> 0x0014CED5). In scene 100 that id is the
 *    MapMaker level's own text, so text returns the label there instead.
 *    hud (sub_0014CC80, chained INSIDE pc_mapmaker_bosses_hud): when the
 *    focus is a real boss whose Story AI also has the Boss flag, the focus
 *    is cleared for the original call and restored after, so the stock bar
 *    is not drawn and the bosses area draws that AI's single Boss bar (its
 *    name, health multiplier, stacking), or none for the Sub-boss bar choice.
 *    Without the Boss flag the stock bar stays (one bar). The Boss/health multipliers act on entity +0x1CC/+0x1D8
 *    as for any Story AI.
 *  - meter (sub_0013A400, the overhead health meter): stock MapMaker story
 *    play draws it in sub_0014D1F0 (HUD, called at 0x001CEF19) only when the
 *    scene is 100 (0x0014D21C) and the mode byte is story (0x0014D225), for
 *    every AI-list actor (0x007A3580, filled with entity +0x7C by
 *    sub_0015BCA0) from index [0x00790950] to [0x0074C37C] whose entity has a
 *    damage-stage record at +0x268 and is not the player's own entity
 *    ([[0x007B0C7C]+0x14]): call sub_0013A400(actor, 0, 0x7F, 0x6E) at
 *    0x0014D25F, its only caller. MapMaker's own Story AIs never get such a
 *    record; the deerbeast handler makes one in its intro state
 *    (sub_0012C800 at 0x002811A3, stored at 0x002811AA), so in MapMaker the
 *    real boss gets a floating meter (placed for a human-sized actor, i.e.
 *    on its back). Mission 5 is scene 1: the scene test skips the loop. The
 *    hook skips the draw (sub_0013A400 only draws: billboard quads through
 *    sub_0001AC50, plain ret) for actors noted by ai_start whose character
 *    is 0x15 or entity model 0x5BE, unless their Story AI chose the
 *    Sub-boss bar; every other actor, scene and caller runs the original. The record itself stays (sub_0014CC80 and the damage
 *    flash in sub_0014D3A0 read it).
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax,
 * g_xbox_mem_offset, xbox_HeapAlloc, pc_mapmaker_area_enabled and
 * pc_mapmaker_limits.inc (mapmaker_limits, MAPMAKER_GUEST_RET,
 * MAPMAKER_PTR_OK). Hot hooks (text: every text lookup; hud: every HUD
 * frame; meter: per frame per recorded actor) test fixed values first and use MAPMAKER_PTR_OK only.
 *
 * Health display (bosses area row, Story AI chunk +0x29 bit 2), per real boss:
 * not Boss-flagged = stock bottom bar, meter hidden; Boss bar = the bosses
 * area's bottom bar, meter hidden; Sub-boss bar = no bottom bar (focus
 * cleared for the stock call, bosses draws none), the stock meter kept. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define RBS_SCENE 0x0079094Cu
#define RBS_MODE 0x007DE455u            /* byte: 10 = story */
#define RBS_ID 0x15u                    /* character id of the deerbeast */
#define RBS_MODEL 0x5BEu                /* CHR_MANSION_DEERBEAST */
#define RBS_LOD 0x5BFu
#define RBS_ROW 150u                    /* synthetic picker row: one past the Arcade table */
#define RBS_TEXT 0x52420015u            /* synthetic text id, only from row_text */
#define RBS_LABEL "Giant Trophy Monster"
#define RBS_TABLE 0x00521A18u           /* Arcade character table, 150 x 0x30, +4 id */
#define RBS_ROWS 150u
#define RBS_TROPHY 0xA7u                /* Arcade trophy monster: preview stand-in */
#define RBS_PICK_FLAGS 0x20900u         /* sub_002C1EA0 item flags in scene 0x66 (0x002C1EC6) */
#define RBS_PICK_CB 0x002C1C90u
#define RBS_PICKER_END 0x002C1FA5u      /* sub_002C1EA0: call sub_000781A0 at 0x002C1FA0 */
#define RBS_SLOTS 0x0075FD90u           /* work copy: 10 x 0x1C, +0 id, +0x14 row, +0x18 variant */
#define RBS_SLOT_STRIDE 0x1Cu
#define RBS_SLOT_COUNT 10u
#define RBS_PREVIEW 0x0078AEA8u         /* Arcade row shown by the preview panel */
#define RBS_RET_RELOAD 0x002C054Cu      /* sub_002C0490: slot reload */
#define RBS_RET_NAME 0x002BE65Fu        /* sub_002BE650 */
#define RBS_RET_NAME_OUTER 0x003033C0u  /* sub_00303350 case 3 (Story AI edit screen) */
#define RBS_RET_PREVIEW1 0x002C1805u    /* sub_002C1570 */
#define RBS_RET_PREVIEW2 0x002C1814u
#define RBS_BAR_TEXT 0xDCCu             /* sub_0014CC80 model 0x5BE label */
#define RBS_RET_BAR_TEXT 0x0014CED5u
#define RBS_EMPTY_TEXT 0x004766D8u      /* sub_0003D7B0(0) */
#define RBS_PAK 0x0049D77Cu             /* "pak/stream/lv1/anim/lv1_3.pak" */
#define RBS_MOUNTS 0x006D7880u          /* mounted paks, table of 30 (0x000620F9) */
#define RBS_MOUNT_MAX 30u
#define RBS_MOUNT_KEEP 2u               /* leave this many slots for stock streaming */
#define RBS_CONTAINER 0x00784014u
#define RBS_MAGIC 0x00C77667u
#define RBS_TAG_INFO 0x00C77668u
#define RBS_TAG_SAI 0x00C7766Cu
#define RBS_TAG_END 0x00C7766Au
#define RBS_SCAN 0x00080000u            /* container: 512 KB */
#define RBS_INFO_SET 0x14u              /* word: set index, >= 0xC custom */
#define RBS_INFO_COUNT 0x16u            /* byte: members */
#define RBS_INFO_MEMBERS 0x18u          /* 4 bytes each, word id */
#define RBS_SET_MAX 20u
#define RBS_SAI_INDEX 0x22u
#define RBS_SAI_BOSS 0x29u              /* bosses area: 0xB0 | sub-boss 4 | (level - 1) */
#define RBS_BOSS_BAR 1u                 /* real[].boss: Boss-flagged, the bottom Boss bar */
#define RBS_BOSS_SUB 2u                 /* Boss-flagged with the Sub-boss bar: overhead meter */
#define RBS_SAI_MIN 0x44u
#define RBS_SAI_BASE 0x00783A50u        /* R: Story AI ids are R + index + 1 */
#define RBS_ACTOR 0xEA0u
#define RBS_ACTOR_CHAR 0x48u
#define RBS_ACTOR_RECORD 0xBF0u
#define RBS_ENT_AI 0x7Cu
#define RBS_ENT_MODEL 0x94u
#define RBS_CLASS_LO 0u                 /* record +0 -> AI +0x10 */
#define RBS_CLASS_HI 0x80u              /* record +4 -> AI +0x14: sub_00280FA0 */
#define RBS_PLAYER 0x007B0C7Cu
#define RBS_FOCUS 0xFF4u
#define RBS_REAL_MAX 16u
#define RBS_RET_METER 0x0014D264u       /* sub_0014D1F0: call sub_0013A400 at 0x0014D25F */
#define RBS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define RBS_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00079D60(void),sub_00071630(void),sub_00071C70(void),sub_0005A130(void),sub_00062260(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
int pc_mapmaker_area_enabled(const char *area,const char *hook);

static struct {
    int read, optout;
    uint32_t label;
    unsigned logged, meters;
    struct { uint32_t record; uint8_t boss; } real[RBS_REAL_MAX];
} rbs;

static void rbs_log(const char *what,uint32_t a,uint32_t b)
{
    if(rbs.logged>=24u) return;
    rbs.logged++;
    fprintf(stderr,"[mapmaker] realbosses: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int rbs_on(void)
{
    mapmaker_limits_read();
    if(!rbs.read) {
        const char *v=getenv("TSFP_MAPMAKER_REAL_BOSS");
        rbs.read=1;
        rbs.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int rbs_play(void)
{
    return rbs_on() && WORD_AT(RBS_SCENE)==100u && RBS_B(RBS_MODE)==10u;
}
/* The label string (Latin-1, like the game's text); allocated off the hot path. */
static uint32_t rbs_label(void)
{
    if(!rbs.label) {
        uint32_t p=xbox_HeapAlloc(sizeof RBS_LABEL,4u);
        if(p && MAPMAKER_PTR_OK(p,sizeof RBS_LABEL)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),RBS_LABEL,sizeof RBS_LABEL);
            rbs.label=p;
        } else rbs_log("no memory for the label",(uint32_t)sizeof RBS_LABEL,0);
    }
    return rbs.label;
}
static uint32_t rbs_trophy_row(void)
{
    uint32_t i;
    for(i=0;i<RBS_ROWS;i++) if(WORD_AT(RBS_TABLE+i*0x30u+4u)==RBS_TROPHY) return i;
    return 0;
}

/* ---- editor (scene 102) -------------------------------------------------- */
static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)==102u; }

/* WRAP sub_000781A0: the picker's list ends; add the Giant Trophy Monster row. */
void pc_mapmaker_realbosses_picker_end(void (*original)(void))
{
    uint32_t text;
    if(MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBS_PICKER_END && rbs_editor() && (text=rbs_label())!=0) {
        guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);
        guest_call(sub_00071C70,2,guest_call(sub_00071630,0),0u);   /* enabled, as the stock rows */
    }
    original();
}

/* WRAP sub_002C1C90 (pick callback): row 150 stores id 0x15, variant -1. */
void pc_mapmaker_realbosses_pick(void (*original)(void))
{
    uint32_t k,s;
    original();
    if(!rbs_editor()) return;
    for(k=0;k<RBS_SLOT_COUNT;k++) {
        s=RBS_SLOTS+k*RBS_SLOT_STRIDE;
        if(WORD_AT(s+0x14u)!=RBS_ROW) continue;
        if(WORD_AT(s)!=RBS_ID) rbs_log("slot set to the giant trophy monster",k,RBS_ID);
        WORD_AT(s)=RBS_ID;
        WORD_AT(s+0x18u)=0xFFFFFFFFu;
    }
}

/* WRAP sub_002C1E10 (picker highlight): the preview never gets row 150. */
void pc_mapmaker_realbosses_highlight(void (*original)(void))
{
    original();
    if(rbs_editor() && WORD_AT(RBS_PREVIEW)==RBS_ROW) WORD_AT(RBS_PREVIEW)=rbs_trophy_row();
}

/* WRAP sub_002BE600 (cdecl id, variant -> Arcade row or -1). */
void pc_mapmaker_realbosses_row_of(void (*original)(void))
{
    uint32_t ret,row;
    if(!MAPMAKER_STACK_OK(16) || WORD_AT(g_esp+4u)!=RBS_ID || !rbs_editor()) { original();return; }
    ret=WORD_AT(g_esp);
    if(ret==RBS_RET_RELOAD || (ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)) row=RBS_ROW;
    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=rbs_trophy_row();
    else { original();return; }
    g_eax=row;
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_002BE300 (cdecl row -> text id; 0 for rows >= 150). */
void pc_mapmaker_realbosses_row_text(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp+4u)!=RBS_ROW || !rbs_editor() || !rbs_label()) { original();return; }
    g_eax=RBS_TEXT;
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0003D7B0 (cdecl text id -> string). Runs for every text lookup:
 * fixed compares only, the original first thing for every other id. */
void pc_mapmaker_realbosses_text(void (*original)(void))
{
    uint32_t id;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    id=WORD_AT(g_esp+4u);
    if(id==RBS_TEXT) {
        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;
        MAPMAKER_GUEST_RET();
        return;
    }
    if(id==RBS_BAR_TEXT && WORD_AT(g_esp)==RBS_RET_BAR_TEXT && rbs.label && rbs_play()) {
        g_eax=rbs.label;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* WRAP sub_000A5E40 (model request): log a deerbeast model request in the editor. */
void pc_mapmaker_realbosses_model_request(void (*original)(void))
{
    uint32_t model;
    if(MAPMAKER_STACK_OK(8) && ((model=WORD_AT(g_esp+4u))==RBS_MODEL || model==RBS_LOD) && rbs_editor())
        rbs_log("editor requested the deerbeast model; the stock call skips flagged models here",model,WORD_AT(g_esp));
    original();
}

/* ---- play (scene 100) ---------------------------------------------------- */
/* 1: the map's custom set has id 0x15; 0: it has not; -1: unreadable. */
static int rbs_map_has_deerbeast(void)
{
    uint32_t container=WORD_AT(RBS_CONTAINER),c,end,steps;
    if(!container || !MAPMAKER_PTR_OK(container,RBS_SCAN) || WORD_AT(container)!=RBS_MAGIC) return -1;
    c=container+8u;end=container+RBS_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u,n,k;
        if(tag==RBS_TAG_END || sz>end-c-8u) break;
        if(tag==RBS_TAG_INFO && sz>=RBS_INFO_MEMBERS) {
            if(RBS_H(d+RBS_INFO_SET)<0xCu) return 0;          /* a preset set */
            n=RBS_B(d+RBS_INFO_COUNT);
            if(n>RBS_SET_MAX) n=RBS_SET_MAX;
            if(RBS_INFO_MEMBERS+n*4u>sz) n=(sz-RBS_INFO_MEMBERS)/4u;
            for(k=0;k<n;k++) if(RBS_H(d+RBS_INFO_MEMBERS+k*4u)==RBS_ID) return 1;
            return 0;
        }
        c+=8u+sz;
    }
    return -1;
}
static int rbs_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,RBS_PAK)!=-1; }

/* WRAP sub_001C8540 (mount the level's anim paks; names registered next). */
void pc_mapmaker_realbosses_anims(void (*original)(void))
{
    int has;
    uint32_t mounted;
    original();
    if(!rbs_play() || rbs.optout) return;
    if((has=rbs_map_has_deerbeast())==0) return;
    rbs_label();                                       /* the bar label, off the HUD path */
    if(rbs_pak_mounted()) return;
    if((mounted=WORD_AT(RBS_MOUNTS))+RBS_MOUNT_KEEP>RBS_MOUNT_MAX) {
        rbs_log("pak mount table full; the giant trophy monster keeps the bot behaviour",mounted,RBS_MOUNT_MAX);
        return;
    }
    if(!guest_call(sub_0005A130,1,RBS_PAK)) rbs_log("lv1_3.pak did not mount; bot behaviour",mounted,(uint32_t)has);
    else rbs_log(has<0?"mounted lv1_3.pak (map container unreadable)":"mounted lv1_3.pak for the giant trophy monster",mounted+1u,0);
}

static uint8_t rbs_boss_flag(uint32_t index)
{
    uint32_t container=WORD_AT(RBS_CONTAINER),c,end,steps;
    if(!container || !MAPMAKER_PTR_OK(container,RBS_SCAN) || WORD_AT(container)!=RBS_MAGIC) return 0;
    c=container+8u;end=container+RBS_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==RBS_TAG_END || sz>end-c-8u) break;
        if(tag==RBS_TAG_SAI && sz>=RBS_SAI_MIN && RBS_H(c+8u+RBS_SAI_INDEX)==index) {
            uint8_t f=RBS_B(c+8u+RBS_SAI_BOSS);
            if((f&0xF8u)!=0xB0u) return 0;
            return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);
        }
        c+=8u+sz;
    }
    return 0;
}
static void rbs_note(uint32_t record,uint8_t boss)
{
    unsigned k,free_k=RBS_REAL_MAX;
    for(k=0;k<RBS_REAL_MAX;k++) {
        if(rbs.real[k].record==record) break;
        if(!rbs.real[k].record && free_k==RBS_REAL_MAX) free_k=k;
    }
    if(k==RBS_REAL_MAX) {
        if(free_k==RBS_REAL_MAX) { rbs_log("real boss table full; bar may be doubled",RBS_REAL_MAX,record);return; }
        k=free_k;
    }
    rbs.real[k].record=record;rbs.real[k].boss=boss;
}

/* WRAP sub_0015B830 (cdecl AI pool entry): give the deerbeast its class. */
void pc_mapmaker_realbosses_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec,id;
    if(!MAPMAKER_STACK_OK(8) || !rbs_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+RBS_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_MODEL)) {
        original();
        return;
    }
    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy monster with bot behaviour",a,0);
    else if(!rbs_pak_mounted()) rbs_log("lv1_3.pak not mounted: giant trophy monster with bot behaviour",a,0);
    else {
        id=WORD_AT(rec+8u);
        WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;
        rbs_note(rec,pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);
        rbs_log("giant trophy monster gets the story monster AI (class 0x80)",a,id);
    }
    original();
}

/* WRAP sub_0014CC80 (story boss bar; chain INSIDE pc_mapmaker_bosses_hud). */
void pc_mapmaker_realbosses_hud(void (*original)(void))
{
    uint32_t player,focus,a,rec;
    unsigned k;
    if(!rbs.real[0].record) { original();return; }   /* cheap: no real boss started yet (filled from 0) */
    player=WORD_AT(RBS_PLAYER);
    if(!MAPMAKER_PTR_OK(player,RBS_FOCUS+4u) || !MAPMAKER_PTR_OK(focus=WORD_AT(player+RBS_FOCUS),0x100u) ||
       RBS_H(focus+RBS_ENT_MODEL)!=RBS_MODEL || !rbs_play()) { original();return; }
    a=WORD_AT(focus+RBS_ENT_AI);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR)) { original();return; }
    rec=WORD_AT(a+RBS_ACTOR_RECORD);
    for(k=0;k<RBS_REAL_MAX && rbs.real[k].record!=rec;k++) {}
    if(k==RBS_REAL_MAX || !rbs.real[k].boss) { original();return; }
    WORD_AT(player+RBS_FOCUS)=0;                     /* Boss bar: ours replaces it; Sub-boss: the meter does */
    original();
    WORD_AT(player+RBS_FOCUS)=focus;
}

/* WRAP sub_0013A400 (cdecl actor, 0, 0x7F, 0x6E; MapMaker story overhead
 * meter, per frame per actor with a +0x268 record): no meter on a real boss. */
void pc_mapmaker_realbosses_meter(void (*original)(void))
{
    uint32_t a,ent,rec;
    unsigned k;
    if(!rbs.real[0].record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=RBS_RET_METER) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) { original();return; }
    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL) { original();return; }
    rec=WORD_AT(a+RBS_ACTOR_RECORD);
    for(k=0;k<RBS_REAL_MAX && rbs.real[k].record!=rec;k++) {}
    if(k==RBS_REAL_MAX || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play()) { original();return; }   /* Sub-boss: keep it */
    if(!rbs.meters++) rbs_log("overhead health meter hidden on the giant trophy monster",a,ent);
    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */
}
