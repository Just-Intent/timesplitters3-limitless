/* US 45410066 MapMaker anim slot budget: the anim names a MapMaker level
 * (scene 100) registers, so a set holding every story boss fits the stock
 * slot table. Included by pc_mapmaker_worm.inc; runs inside its WRAP
 * sub_0005B980 -> pc_mapmaker_worm_register (no wrapper of its own). Docs:
 * docs/mapmaker/mapmaker-animslots.md. Tests:
 * tests/mapmaker/test_mapmaker_animslots.py.
 *
 * Stock (CONFIRMED, decode and US XBE data; the test names the sites):
 *  - sub_0005B980(prefix, table, count) registers entry by entry (0x18
 *    bytes: +0 name, +8/+0xC slot words, +0x10 id) through
 *    sub_00057F30(path, id, 0).
 *  - sub_00057F30 maps the id to a slot ([0x007E03C0 + id*2], 0xFFFF free:
 *    the next slot [0x006D77F8]), stores the anim pointer at
 *    [[0x007E3954] + slot*4] and the slot words at 0x007DE660 + slot*8, and
 *    fixes the file up once (flag 4 at header +8 guards a second fixup).
 *  - The slot arrays are static: 0x007E2AA4 (set by sub_0005C420) holds 940
 *    pointers up to 0x007E3954, 0x007DE660 holds 940 entries up to the id
 *    map 0x007E03C0. Slots 0..0x22 are reserved; nothing checks the count.
 *  - Slot 940 overwrites [0x007E3954] itself with the anim pointer; the next
 *    read [[0x007E3954] + 940*4] is junk and the level load crashes
 *    (v0.4.0-mapmaker.34: ecx 940, edi 0x2D1 millsgrenade_reload, the 11th
 *    entry of grenade.pak's table 0x0048F738 at 0x001CB1C3).
 * A MapMaker set holding the story bosses mounts their story level anim
 * paks, whose tables name the whole level cast (158 entries for lv1_3,
 * lv1_6, lv81, lv14, lv11, lv1_2), and the story creatures' paks
 * (pc_mapmaker_creatures.def: lv17, lv6, lv22); the scene's own paks fill
 * the rest.
 *
 * In scene 100 with TSFP_MAPMAKER_LIMITS=1 (story levels stay stock):
 *  - a boss pak's table registers only the boss's own anims (ans_paks: the
 *    ids its handlers play, test), entry by entry;
 *  - every entry registered entry by entry skips an id already registered
 *    this load (one copy: the first pak's, the same anim data) and, when the
 *    940 slots are used, a new id (logged; the anim is missing instead of a
 *    crash);
 *  - any other table runs stock when it fits, entry by entry otherwise.
 * Requires the includer's WORD_AT, guest_call, g_esp, g_xbox_mem_offset and
 * pc_mapmaker_limits.inc. Statics are prefixed ans/ANS_. */
#include <stdio.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define ANS_SCENE 0x0079094Cu
#define ANS_USED 0x006D77F8u           /* next free slot (sub_00057F30 0x00057FC1, 0x000581F4) */
#define ANS_SLOTS 940u                 /* (0x007E3954-0x007E2AA4)/4 = (0x007E03C0-0x007DE660)/8 */
#define ANS_MAP 0x007E03C0u            /* int16 slot per id, 0xFFFF free */
#define ANS_IDS 0xC03u                 /* sub_0005C420: 0x601 dwords + 1 word of 0xFFFF */
#define ANS_FREE 0xFFFFu
#define ANS_ENTRY 0x18u
#define ANS_ID 0x10u
#define ANS_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_0005B980(void);

/* A boss pak's registration table (sub_001CA2F0) and the entries its boss
 * needs: up to three runs (first, count). The ids are the ones its handlers
 * play (test_boss_anims_are_kept); the rest is the story level's cast. */
static const struct ans_pak_s {
    uint32_t table, count;
    struct { uint8_t first, n; } run[3];
    const char *pak;
} ans_paks[]={
    {0x0048C1B0u,0x10u,{{0,8},{0,0},{0,0}},"lv1_3"},      /* trophy monster: mattackheadbutt..mcharge 0x127..0x12E */
    {0x0048C738u,0x12u,{{1,11},{14,1},{0,0}},"lv1_6"},    /* skeleton: princess_start..princess_raise_react_end 0x19C..0x1A6, princess_eyeoverwrite 0x1A9 */
    {0x00484F68u,0x14u,{{5,15},{0,0},{0,0}},"lv81"},      /* Crow walker: crowewalker* 0x6EE..0x6FC */
    {0x0047E938u,0x29u,{{23,3},{29,9},{0,0}},"lv14"},     /* giant robot giantrob_* 0x807..0x809; Crowe splitcrowe* 0x80E..0x815, eggnoise 0x816 */
    {0x00484410u,0x21u,{{32,1},{0,0},{0,0}},"lv11"},      /* seated Khallos: khallosidle 0xBFB */
    {0x0048BEC8u,0x1Eu,{{5,1},{21,8},{0,0}},"lv1_2"},     /* mansion worm: barney_worm 0x149, wrmburst..wrmdeath 0x194..0x19B */
    /* the story creatures (pc_mapmaker_creatures.def): one run per row; rows
     * sharing a pak keep the union of their runs (ans_keep) */
#define MAPMAKER_CREATURE(row,id,look,preview,label,lo,hi,pak,pak_name,table,count,first,n,level) {table,count,{{first,n},{0,0},{0,0}},pak_name},
#include "pc_mapmaker_creatures.def"
#undef MAPMAKER_CREATURE
};
#define ANS_NPAKS (sizeof ans_paks/sizeof ans_paks[0])

static struct {
    int busy;
    unsigned logged, tables, kept, dropped, shared, full, levels;
} ans;

static void ans_log(const char *what,const char *pak,uint32_t a,uint32_t b)
{
    if(ans.logged>=48u) return;
    ans.logged++;
    fprintf(stderr,"[mapmaker] animslots: %s %s (0x%X, 0x%X)\n",what,pak,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static const struct ans_pak_s *ans_find(uint32_t table,uint32_t count)
{
    unsigned i;
    for(i=0;i<ANS_NPAKS;i++) if(ans_paks[i].table==table && ans_paks[i].count==count) return &ans_paks[i];
    return NULL;
}
static int ans_keep(const struct ans_pak_s *p,uint32_t k)
{
    unsigned i,r;
    if(!p) return 1;
    for(i=0;i<ANS_NPAKS;i++) {
        const struct ans_pak_s *q=&ans_paks[i];
        if(q->table!=p->table || q->count!=p->count) continue;
        for(r=0;r<3u;r++) if(k>=q->run[r].first && k<(uint32_t)q->run[r].first+q->run[r].n) return 1;
    }
    return 0;
}

/* Called first by pc_mapmaker_worm_register (WRAP sub_0005B980, cdecl prefix,
 * table, count). 1: handled here (the original's ret done), 0: the caller
 * goes on (the worm's own path or the stock original). */
static int ans_register(void)
{
    uint32_t prefix,table,count,used,k,e,id;
    const struct ans_pak_s *p;
    unsigned kept=0,dropped=0,shared=0,full=0;
    if(ans.busy || !MAPMAKER_STACK_OK(16)) return 0;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits || WORD_AT(ANS_SCENE)!=100u) return 0;
    prefix=WORD_AT(g_esp+4u);table=WORD_AT(g_esp+8u);count=WORD_AT(g_esp+12u);
    if(!count || count>0x400u || !MAPMAKER_PTR_OK(table,count*ANS_ENTRY)) return 0;
    p=ans_find(table,count);
    used=WORD_AT(ANS_USED);
    if(!p && used<=ANS_SLOTS && count<=ANS_SLOTS-used) return 0;   /* fits: stock */
    ans.busy=1;
    for(k=0;k<count;k++) {
        e=table+k*ANS_ENTRY;
        id=ANS_H(e+ANS_ID);
        if(!ans_keep(p,k)) { dropped++;continue; }
        if(id<ANS_IDS && ANS_H(ANS_MAP+id*2u)!=ANS_FREE) { shared++;continue; }   /* another pak's copy */
        if(WORD_AT(ANS_USED)>=ANS_SLOTS) { full++;continue; }
        guest_call(sub_0005B980,3,prefix,e,1u);
        kept++;
    }
    ans.busy=0;
    ans.tables++;ans.kept+=kept;ans.dropped+=dropped;ans.shared+=shared;ans.full+=full;
    if(p) ans_log("boss anims registered (kept, left out) from",p->pak,kept,dropped);
    if(shared) ans_log("anim ids already registered, kept the first copy: table",p?p->pak:"",table,shared);
    if(full) ans_log("anim slot table full (940): left out of table",p?p->pak:"",table,full);
    MAPMAKER_GUEST_RET();
    return 1;
}
