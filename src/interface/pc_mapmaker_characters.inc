/* US 45410066 MapMaker custom character set: offer every Arcade character.
 * Docs: docs/mapmaker/mapmaker-characters.md. Research basis:
 * logs/limits/features/story-enemies.md (main checkout, ignored).
 *
 * Stock (CONFIRMED in text.asm and the decode, tests/mapmaker/test_mapmaker_characters.py):
 * the editor's Map Settings character set (sub_003128F0 opens descriptor
 * 0x0052375C, whose slot screen 0x0052372C opens the picker 0x0052374C) edits
 * the 10-slot work copy 0x0075FD90 (stride 0x1C; loaded by sub_002C00F0 from
 * the editor's set editor+0x11860, stored back by sub_002BFFD0). The picker's
 * builder sub_002C1EA0 walks the 150 rows of the Arcade character table
 * 0x00521A18 (stride 0x30: +1 sbyte variant, +4 character id, +0xC portrait,
 * +0x14 unlock rule) and adds a row only when sub_001CD980(id, variant)
 * (call 0x002C1F30, returning to 0x002C1F35) says the profile has unlocked
 * it: row found by sub_002BE590, rule type +0x14 != 0, then the "all
 * characters" cheat byte [0x0075AEFC] or the rule check sub_001CD3E0
 * (type 1 always, 2..8 story/challenge/league conditions, 0 never).
 * There is no size, category or memory filter: big characters such as the
 * giant robot (row 90, id 0xA8, rule 2: story level 9) or the four Jacob
 * Crow rows (11..14, rule 3) are missing only because they are locked. The
 * pick callback sub_002C1C90 stores the row's id/variant with no further check.
 *
 * Change (TSFP_MAPMAKER_LIMITS=1, editor scene 102 only, only the call that
 * returns to 0x002C1F35): after the original, a row whose rule type is a real
 * rule (1..8) is reported as available. The stock answer is unchanged for
 * every other caller (Arcade/character select, the profile's own bot sets in
 * scene 101). Only Arcade-table rows are ever offered, so every pick is a
 * character the stock game already places in Arcade levels and MapMaker
 * presets (preset 7 holds the giant robot, preset 8 the trophy monster).
 *
 * Load diagnostic: at a MapMaker level start (scene 100) the set preload
 * sub_0023F320 / sub_0023F350 calls sub_000D7780(id, id) for every member
 * (returns 0x0023F33F / 0x0023F37F). The hook logs, without changing
 * anything, any member that is not an Arcade-table character or whose model
 * (character table 0x0053A3CC + id*0x30 +4) has no resource slot
 * (0x0078B860[model], [0x00791D4C]); the stock preload and spawn read the
 * set themselves, so nothing is skipped (see the doc).
 *
 * Save format: unchanged. The map-info chunk (0x00C77668) stores each member
 * as word id (+0x18+4i) and sbyte variant (+0x1B+4i); the loader (0x00301D5F)
 * and play builder sub_002C0210 copy them without an unlock check.
 *
 * Requires the includer's WORD_AT/g_esp/g_eax/g_xbox_mem_offset and
 * pc_mapmaker_limits.inc (mapmaker_limits, mapmaker_limits_read). */
#include <stdio.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define CHR_SCENE 0x0079094Cu
#define CHR_TABLE 0x00521A18u           /* Arcade character table, 150 x 0x30 */
#define CHR_ROWS 150u
#define CHR_ROW 0x30u
#define CHR_RULE_MAX 8u                 /* sub_001CD3E0 jump table 0x001CD530: types 0..8 */
#define CHR_PICKER_RET 0x002C1F35u      /* sub_002C1EA0: call sub_001CD980 at 0x002C1F30 */
#define CHR_PRELOAD_CUSTOM 0x0023F33Fu  /* sub_0023F320: call sub_000D7780 at 0x0023F33A */
#define CHR_PRELOAD_PRESET 0x0023F37Fu  /* sub_0023F350: call sub_000D7780 at 0x0023F37A */
#define CHR_CHARS 0x0053A3CCu           /* character table, 0x30 per id, +4 model */
#define CHR_IDS 0xB0u                   /* ids in the Arcade table are 0x00..0xAF */
#define CHR_MODEL_MAX 0x9D0u            /* sub_000A5E40 ignores ids >= 0x9D0 */
#define CHR_SLOTS 0x0078B860u           /* int16 resource slot per id */
#define CHR_RESOURCES 0x00791D4Cu       /* resource entries, 0x30 each */
#define CHR_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define CHR_H(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static unsigned chr_widened, chr_logged;

static void chr_log(const char *what,uint32_t a,uint32_t b)
{
    if(chr_logged>=16u) return;
    chr_logged++;
    fprintf(stderr,"[mapmaker] characters: %s (0x%X, %d)\n",what,(unsigned)a,(int)b);
    fflush(stderr);
}

/* sub_002BE590: the first row with this id, and with this variant unless < 0. */
static uint32_t chr_row(uint32_t id,int32_t variant)
{
    uint32_t i,r;
    for(i=0;i<CHR_ROWS;i++) {
        r=CHR_TABLE+i*CHR_ROW;
        if(WORD_AT(r+4u)!=id) continue;
        if(variant<0 || (int32_t)(int8_t)CHR_B(r+1u)==variant) return r;
    }
    return 0;
}

static int chr_offerable(uint32_t id,int32_t variant)
{
    uint32_t r=chr_row(id,variant),rule;
    if(!r) return 0;
    rule=WORD_AT(r+0x14u);
    return rule>=1u && rule<=CHR_RULE_MAX;
}

/* WRAP sub_001CD980 (cdecl, args id, variant; returns 0/1). */
void pc_mapmaker_characters_eligible(void (*original)(void))
{
    uint32_t ret,id;
    int32_t variant;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits || WORD_AT(CHR_SCENE)!=102u || !MAPMAKER_STACK_OK(12) ||
       (ret=WORD_AT(g_esp))!=CHR_PICKER_RET) {
        original();
        return;
    }
    id=WORD_AT(g_esp+4u);variant=(int32_t)WORD_AT(g_esp+8u);
    original();                                   /* pops the return address */
    if(g_eax==0u && chr_offerable(id,variant)) {
        g_eax=1u;
        if(chr_widened++<4u) chr_log("offering a character the profile has not unlocked",id,variant);
    }
    (void)ret;
}

/* WRAP sub_000D7780 (cdecl, args id, id): log a set member that cannot load. */
void pc_mapmaker_characters_preload(void (*original)(void))
{
    uint32_t ret,id,model,table;
    int16_t slot;
    mapmaker_limits_read();
    if(mapmaker_limits.limits && WORD_AT(CHR_SCENE)==100u && MAPMAKER_STACK_OK(12) &&
       ((ret=WORD_AT(g_esp))==CHR_PRELOAD_CUSTOM || ret==CHR_PRELOAD_PRESET)) {
        id=WORD_AT(g_esp+4u);
        if(id>=CHR_IDS || !chr_row(id,-1)) chr_log("set member is not an Arcade character; stock preload runs",id,-1);
        else {
            model=WORD_AT(CHR_CHARS+id*0x30u+4u);
            table=WORD_AT(CHR_RESOURCES);
            if(model>=CHR_MODEL_MAX) chr_log("set member has no model id",id,(int32_t)model);
            else if((slot=CHR_H(CHR_SLOTS+model*2u))<0 || !MAPMAKER_PTR_OK(table+(uint32_t)slot*0x30u,0x30u))
                chr_log("set member model has no resource slot",id,(int32_t)model);
        }
    }
    original();
}
