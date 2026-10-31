/* US 45410066 MapMaker Story AI bosses and the story creature probe
 * (docs/mapmaker/mapmaker-bosses.md; research logs/limits/features/story-enemies.md).
 * Boss option: opt-in with TSFP_MAPMAKER_LIMITS=1 (mapmaker_limits.limits);
 * with it off every boss hook runs its original only. The probe has its own
 * switch, TSFP_MAPMAKER_CREATURE_PROBE=1, and never writes guest memory.
 *
 * Editor (scene 102). The Story AI edit screen (sub_0031CB90) calls the menu
 * spacer sub_000784A0 at 0x0031D07E just before its "Done" row; this area's
 * row hook adds one row there (after the original, like the aitemplates
 * rows): "Boss: Off" / "Boss: On" / "Boss: health x2" / "x4" / "x8". Its
 * callback is the edit screen's close, sub_0031CAC0, told apart by the
 * highlighted row id at menu+0x18C. A click cycles the setting, rewrites
 * the row's label buffer in place and leaves the edit screen open (the hook
 * then does the original's ret itself). Type 5 AIs get no row.
 *
 * The setting lives in a C table indexed like the editor's Story AI entries
 * (no free editor entry field: +0x50 belongs to aitemplates), with each
 * entry's colour (+0x60) as a consistency check. It follows the entries
 * through delete (sub_0031C8B0, memmove), map reset (sub_002FEBF0), editor
 * load (sub_00301A90, also undo) and compile (sub_003006C0, save and the
 * undo shadow). A duplicate or linked copy starts with its source's Boss
 * level and Health display (close hook); later edits stay per AI, not shared
 * by a group.
 *
 * Save format. Story AI chunk (tag 0x00C7766C) data byte +0x29: 0 = no
 * boss (every stock map), 0xB0 | (level - 1) = boss with health x1, x2, x4,
 * x8 and the Boss bar; 0xB4 | (level - 1) = the same with the Sub-boss bar. The compiler zeroes the container (rep stosd 0x0030070D); the chunk
 * writer (0x003013E1..0x003015DF) stores byte +0x28 and word +0x2A but
 * never +0x29; the editor loader (0x00302428..0x0030267A) and every play
 * reader of the chunk (sub_0023F390, sub_00240590, sub_00240C50,
 * sub_00337D80, sub_0033EC30) never read it (decode check in
 * tests/mapmaker/test_mapmaker_bosses.py). A stock game ignores it; other
 * values are logged and ignored.
 *
 * Play (scene 100, story mode byte [0x007DE455] == 10).
 * - sub_00240C50 (level start): index -> boss level and the AI's saved
 *   name (chunk +0x00) from the map container [0x00784014].
 * - Health: a character entity's health is the float pair +0x1CC (current)
 *   / +0x1D8 (maximum): set at creation (sub_000DAC30, 0x000DADEE) and at AI
 *   start (sub_0015F8C0, 0x0015FAB4 = sub_000B9A90(entity)). Each HUD frame
 *   every live Story AI (list 0x00900100, actors.json relocation of
 *   0x007A3580; pool entry a: a+0 entity, entity+0x7C = a, a+0xBF0 spawn
 *   record, record+8 = id R + index + 1) whose boss level is > 1 gets its
 *   maximum and current health multiplied once; a maximum that differs
 *   from the value this hook wrote means the game set it again (respawn of
 *   a pool slot) and is scaled again.
 * - Bar: the story boss bar is drawn by sub_0014CC80 (called from the HUD
 *   frame sub_0014F0B0 at 0x0014F256 with ebx = HUD block 0x007A6160 +
 *   player*0x177C): for the story focus entity [[0x007B0C7C]+0xFF4] whose
 *   model word +0x94 is a known boss it calls sub_00139740 (esi = HUD block,
 *   edi = entity, args text, fraction, int *drawn) and sets HUD block +8
 *   bit 0 (0x0014D0D5); otherwise it clears that bit (0x0014D0DD). After
 *   the original, if no stock bar was drawn, this hook finds the nearest
 *   living Boss AI within TSFP_MAPMAKER_BOSS_RANGE game units (default 30;
 *   3D distance of entity +0x64/+0x68/+0x6C to the current player's entity
 *   [[0x007B0C7C]+0x14]; no line of sight) and calls sub_00139740 itself
 *   with the AI's name as the label (sub_00139740 itself reads
 *   +0x1CC/+0x1D8 and smooths in HUD block +0x171C..+0x172C). The focus
 *   field and the actor's AI state are never written: the other focus
 *   meter in sub_0014F0B0 (0x0014FAE3, player +0x9E0 against the focus's
 *   a+0x28) is a different bar. Out of range, dead (health <= 0) or gone
 *   from the live list: no bar.
 * - Several bosses: up to BOS_BARS_MAX bars (2 in split screen), nearest
 *   first, stacked upward from the stock bar position. sub_00139740 places
 *   its box at the bottom of the viewport: y1 = [[0x007E71C8]+0x18] - the int
 *   HUD margin [0x007A614C] (0x0013986E..0x001398AB), height 45 x [0x004E68F8]
 *   x 1, 0.933 or 0.8 (0x00139840..0x0013988C), frame 6 x [0x004E68F8] above
 *   it (sub_00130400). The hook raises the margin by slot x BOS_BAR_STEP x
 *   [0x004E68F8] around each call and puts it back (the HUD frame rewrites
 *   it, 0x0014B084). A stock bar (HUD +8 bit 0 after the original) keeps
 *   slot 0. The smoothing state +0x171C/+0x1724/+0x172C is swapped per boss
 *   (BOS_SMOOTH table) and the stock values restored afterwards; each copy's
 *   fade +0x172C is lowered as sub_0014B090 lowers the live one (dt x
 *   [0x00475D10] per HUD block clock +0x2C), else the bar never drains. Coordinates
 *   stay guest 640x480, so widescreen keeps them in the 4:3 HUD box.
 * - Health display (row "Health display: Boss bar / Sub-boss bar", built
 *   only while Boss is not Off; +0x29 bit 2 = Sub-boss): a Sub-boss AI gets
 *   no bottom bar; instead the meters hook (sub_0014D1F0) draws the stock
 *   MapMaker overhead meter sub_0013A400 for it (see that hook). A real
 *   trophy monster's own meter/bar follow the same choice (realbosses).
 *
 * Stronger health (same switch). The stock Health row of the edit screen
 * (text 0xC0D, options 0xC0E..0xC12 = Weakling, Low, Normal, High, Tough Guy,
 * values 0..4, bound to entry +0x4C by sub_00078590 at 0x0031CFD2) gets four
 * more options (values 5..8): Brute, Juggernaut, Titan, Colossus = 2, 4, 8,
 * 16 x Tough Guy. The writer copies +0x4C to chunk +0x30 (0x0030149F); play
 * maps it through 0x0052E060[v] (sub_0031BA70, 0x00242356; 5 entries) to the
 * spawn record +0x4E and the scene-100 health table 0x004DACF4 (sub_000B9A30;
 * Tough Guy 20.0 = index 0). So the compile hook stores 4 (Tough Guy) at +0x30
 * for 5..8 and the extra level in chunk byte +0x21 (0xA0 | 1..4; never
 * written or read by stock code, decode check); a stock game reads Tough
 * Guy. The editor load puts 4 + extra back into +0x4C; in play the HUD hook
 * multiplies such an AI's health by 2^extra (times the Boss factor).
 *
 * Probe (TSFP_MAPMAKER_CREATURE_PROBE=1, any scene where the HUD runs; log
 * TSFP_MAPMAKER_CREATURE_PROBE_LOG (appended) or stderr). Once per level
 * (scene or actor pool [0x007356D8] change): the scene, mode and, for each
 * story-only character id (logs/limits/features/story-enemies.md section 2),
 * its models from the character table 0x0053A3CC + id*0x30 (+4, +8) and the
 * resource slot 0x0078B860[model] (int16) -> entry [0x00791D4C] + slot*0x30
 * (+0 type, +0x10/+0x14 resources, +0x28 flags). Then each new live
 * character (pool entry, record, id) once: id a+0x48, variant a+0x4A, AI
 * class a+0x10/+0x14, spawn record class +0/+4 and id +8, model word
 * entity+0x94, skeleton [[entity+4]+0x88]+4 and health. At most 160 new
 * characters per level and 4096 lines per run.
 *
 * Requires the includer's WORD_AT, guest_call, save_registers/
 * restore_registers, g_esp/g_eax/g_ebx/g_esi/g_edi, g_xbox_mem_offset,
 * xbox_HeapAlloc, mapmaker_limits/mapmaker_limits_read and
 * MAPMAKER_GUEST_RET/MAPMAKER_PTR_OK (pc_mapmaker_limits.inc). The HUD hook
 * runs every frame: fixed image addresses and MAPMAKER_PTR_OK only. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define BOS_EDITOR_PTR 0x007844A8u
#define BOS_EDITOR_SIZE 0x74C00u
#define BOS_SAI 0x58390u                /* editor.json: 511 x 0xE4 */
#define BOS_STRIDE 0xE4u
#define BOS_COUNT 0x280u
#define BOS_CAP 511u
#define BOS_SELECTED 0x00762F94u        /* entry being edited (word: sai-index.json) */
#define BOS_SCENE 0x0079094Cu
#define BOS_MODE 0x007DE455u            /* byte: 10 = story */
#define BOS_CONTAINER 0x00784014u
#define BOS_SAI_BASE 0x00783A50u        /* R: Story AI ids are R + index + 1 */
#define BOS_CLOSE 0x0031CAC0u           /* edit screen close, also the row callback */
#define BOS_ROWS_AT 0x0031D083u         /* return from sub_000784A0 before "Done" */
#define BOS_ROW 0x424F5301u
#define BOS_ROW_DISPLAY 0x424F5302u     /* "Health display:" row, only while Boss is not Off */
#define BOS_TYPE 0x34u
#define BOS_COLOUR 0x60u
#define BOS_COPY_FIRST 0x28u            /* settings copied to a duplicate/linked copy */
#define BOS_COPY_END 0x50u
#define BOS_ENTRY_INDEX 0x24u           /* loader: chunk +0x22 -> entry +0x24 */
#define BOS_TAG_SAI 0x00C7766Cu
#define BOS_TAG_END 0x00C7766Au
#define BOS_MAGIC 0x00C77667u
#define BOS_SCAN 0x00080000u            /* container scan bound: 512 KB */
#define BOS_CHUNK_NAME 0x00u            /* 32 bytes, NUL terminated */
#define BOS_CHUNK_INDEX 0x22u
#define BOS_CHUNK_FLAG 0x29u
#define BOS_CHUNK_MIN 0x44u             /* fixed part of a Story AI chunk */
#define BOS_FLAG_MARK 0xB0u             /* +0x29 = 0xB0 | sub-boss bit | (level - 1) */
#define BOS_FLAG_MASK 0xF8u
#define BOS_FLAG_SUB 0x04u              /* bit 2: Sub-boss bar (overhead meter), else the Boss bar */
#define BOS_LEVELS 4u                   /* 1..4: health x1, x2, x4, x8 */
#define BOS_LEVEL_SLOTS 1024u           /* play-time index bound (= aitemplates) */
#define BOS_NAME 32u
#define BOS_HEALTH 0x4Cu                /* entry: Health list value 0..4 (+ ours 5..8) */
#define BOS_HEALTH_ROW_AT 0x0031CFD7u   /* return from sub_00078590(&entry+0x4C) */
#define BOS_HEALTH_TOUGH 4u             /* stock maximum: Tough Guy */
#define BOS_HEALTH_EXTRA 4u             /* 5..8: x2, x4, x8, x16 Tough Guy */
#define BOS_CHUNK_HEALTH 0x30u          /* writer 0x003014A2 (byte) */
#define BOS_CHUNK_EXTRA 0x21u           /* 0xA0 | extra level */
#define BOS_EXTRA_MARK 0xA0u
#define BOS_EXTRA_MASK 0xF8u
#define BOS_TEXT_DISPLAY (1u+BOS_BARS_MAX+BOS_HEALTH_EXTRA)
#define BOS_TEXTS (2u+BOS_BARS_MAX+BOS_HEALTH_EXTRA)   /* row label, HUD labels, health options, display row */

#define BOS_PLAYER 0x007B0C7Cu          /* current player: +4 index, +0x14 entity */
#define BOS_HUD 0x007A6160u             /* HUD block per player, 0x0014F0CC */
#define BOS_HUD_STRIDE 0x177Cu
#define BOS_HUD_FLAGS 8u                /* bit 0: a boss bar was drawn (0x0014D0D5) */
#define BOS_LIST 0x00900100u            /* live characters (actors-list.json) */
#define BOS_LIST_COUNT 0x0074C37Cu
#define BOS_HUMANS 0x00790950u
#define BOS_LIST_MAX 64u
#define BOS_POOLPTR 0x007356D8u
#define BOS_ACTOR_SIZE 0xEA0u
#define BOS_ACTOR_RECORD 0xBF0u
#define BOS_ENT_SIZE 0x200u
#define BOS_ENT_AI 0x7Cu
#define BOS_ENT_POS 0x64u
#define BOS_ENT_MODEL 0x94u
#define BOS_ENT_HEALTH 0x1CCu
#define BOS_ENT_MAX 0x1D8u
#define BOS_ENT_RECORD 0x268u           /* damage-stage record: the stock loop draws the meter (0x0014D249) */
#define BOS_ENT_INST 4u                 /* model instance: +0x8C skeleton header, +0x10 node matrices */
#define BOS_RANGE 30.0f                 /* game units; TSFP_MAPMAKER_BOSS_RANGE */
#define BOS_BARS_MAX 4u                 /* boss bars per viewport, stock bar included */
#define BOS_BARS_SPLIT 2u               /* with more than one human player */
#define BOS_BAR_STEP 57.0f              /* 45 box + 6 frame + 6 gap, x [0x004E68F8] */
#define BOS_MARGIN 0x007A614Cu          /* int bottom HUD margin read at 0x00139876 */
#define BOS_HUD_SCALE 0x004E68F8u       /* float HUD y scale */
#define BOS_HUD_SMOOTH 0x171Cu          /* +0x171C, +0x1724, +0x172C (sub_00139740) */
#define BOS_SMOOTH 16u
#define BOS_HUD_CLOCK 0x2Cu             /* float HUD block clock: += dt per sub_0014B090 (0x0014B23E) */
#define BOS_FADE_RATE 0x00475D10u       /* float 2.0: +0x172C -= dt x this, floor 0 (0x0014B24F..0x0014B276) */

#define BOS_CHARS 0x0053A3CCu           /* per character id, 0x30 bytes */
#define BOS_RES_SLOT 0x0078B860u        /* int16 per resource id < 0x9D0 */
#define BOS_RES_IDS 0x9D0u
#define BOS_RES_TABLE 0x00791D4Cu       /* -> 0x30-byte entries */
#define BOS_PROBE_LEVEL 160u
#define BOS_PROBE_LINES 4096u

#define BOS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00079D60(void),sub_00139740(void),sub_00071800(void),sub_0013A400(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

static struct {
    uint32_t editor;                    /* editor object the table belongs to */
    uint8_t level[BOS_CAP];             /* 0 off, 1..4 */
    uint8_t sub[BOS_CAP];               /* 1: Sub-boss bar (only with a level) */
    uint16_t colour[BOS_CAP];
    uint32_t text;                      /* guest: BOS_TEXTS labels of 32 B */
    unsigned logged, dropped, bad_flags, bad_extra;
} bos;

static struct {
    int active;
    uint8_t level[BOS_LEVEL_SLOTS];     /* boss level 0..4 */
    uint8_t extra[BOS_LEVEL_SLOTS];     /* stronger health 0..4 */
    uint8_t sub[BOS_LEVEL_SLOTS];       /* 1: Sub-boss bar: overhead meter, no bottom bar */
    unsigned subs, meters;
    char name[BOS_LEVEL_SLOTS][BOS_NAME];
    struct { uint32_t actor, record, max_bits; } scaled[BOS_LIST_MAX];
    struct { uint32_t hud, entity, frame, state[3], timed; float clock; } smooth[BOS_SMOOTH];
    uint32_t frame;
    float range2;
} bos_play;

static void bos_log(const char *what,unsigned a,unsigned b)
{
    if(bos.logged>=16u) return;
    bos.logged++;
    fprintf(stderr,"[mapmaker] story AI bosses: %s (%u, %u)\n",what,a,b);
    fflush(stderr);
}
static uint32_t bos_get16(uint32_t a) { return BOS_B(a)|(uint32_t)BOS_B(a+1u)<<8; }
static float bos_getf(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static void bos_setf(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static unsigned bos_multiplier(unsigned level) { return level?1u<<(level-1u):1u; }
static unsigned bos_decode(uint32_t flag)
{
    if(!flag) return 0;
    if((flag&BOS_FLAG_MASK)!=BOS_FLAG_MARK) { bos.bad_flags++;bos_log("unknown chunk +0x29 value ignored",flag,0);return 0; }
    return (flag&3u)+1u;
}
/* 1 when a valid boss flag asks for the Sub-boss bar (bit 2); no second log. */
static unsigned bos_decode_sub(uint32_t flag) { return (flag&BOS_FLAG_MASK)==BOS_FLAG_MARK && (flag&BOS_FLAG_SUB)?1u:0u; }
/* Chunk +0x21: 0 or 0xA0 | 1..4 with the stock health byte at Tough Guy. */
static unsigned bos_decode_extra(uint32_t flag,uint32_t health)
{
    if(!flag) return 0;
    if((flag&BOS_EXTRA_MASK)!=BOS_EXTRA_MARK || !(flag&7u) || (flag&7u)>BOS_HEALTH_EXTRA || health!=BOS_HEALTH_TOUGH) {
        bos.bad_extra++;bos_log("unknown chunk +0x21 health value ignored",flag,health);
        return 0;
    }
    return flag&7u;
}

/* ---- editor ------------------------------------------------------------- */
static int bos_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }
static uint32_t bos_editor(void)
{
    uint32_t e=WORD_AT(BOS_EDITOR_PTR);
    return e && MAPMAKER_PTR_OK(e,BOS_EDITOR_SIZE)?e:0u;
}
static uint32_t bos_count(uint32_t e) { uint32_t c=WORD_AT(e+BOS_COUNT); return c<=BOS_CAP?c:0u; }
static uint32_t bos_entry(uint32_t e,uint32_t i) { return e+BOS_SAI+i*BOS_STRIDE; }
static uint32_t bos_selected(void) { return WORD_AT(BOS_SELECTED)&0xFFFFu; }
static uint16_t bos_colour_of(uint32_t e,uint32_t i) { return (uint16_t)WORD_AT(bos_entry(e,i)+BOS_COLOUR); }
/* The entry's boss level; a stale slot (other editor, other colour) is 0. */
static unsigned bos_get(uint32_t e,uint32_t i)
{
    if(e!=bos.editor || i>=bos_count(e) || !bos.level[i]) return 0;
    if(bos.colour[i]!=bos_colour_of(e,i)) {
        bos.dropped++;bos_log("boss setting dropped: entry changed",i,bos.colour[i]);
        bos.level[i]=0;bos.sub[i]=0;
        return 0;
    }
    return bos.level[i];
}
static unsigned bos_get_sub(uint32_t e,uint32_t i) { return bos_get(e,i)?bos.sub[i]:0u; }
/* A Sub-boss choice is kept only with a level (Off clears it, as the save does). */
static void bos_set(uint32_t e,uint32_t i,unsigned level,unsigned sub)
{
    if(bos.editor!=e) { memset(bos.level,0,sizeof bos.level);memset(bos.sub,0,sizeof bos.sub);bos.editor=e; }
    if(i>=BOS_CAP) return;
    bos.level[i]=(uint8_t)(level<=BOS_LEVELS?level:0u);
    bos.sub[i]=(uint8_t)(bos.level[i] && sub?1u:0u);
    bos.colour[i]=bos_colour_of(e,i);
}
static uint32_t bos_text(unsigned which)
{
    if(!bos.text) {
        bos.text=xbox_HeapAlloc(BOS_TEXTS*BOS_NAME,4u);
        if(!bos.text) return 0;
        memset((void *)((uintptr_t)bos.text+g_xbox_mem_offset),0,BOS_TEXTS*BOS_NAME);
    }
    return which<BOS_TEXTS?bos.text+which*BOS_NAME:0u;
}
static void bos_put_text(uint32_t text,const char *s)
{
    char *p=(char *)((uintptr_t)text+g_xbox_mem_offset);
    size_t n=strlen(s);
    if(n>BOS_NAME-1u) n=BOS_NAME-1u;
    memset(p,0,BOS_NAME);
    memcpy(p,s,n);
}
/* The row label, written in place so an open edit screen shows the change. */
static uint32_t bos_label(unsigned level)
{
    static const char *const label[BOS_LEVELS+1u]={"Boss: Off","Boss: On","Boss: health x2",
        "Boss: health x4","Boss: health x8"};
    uint32_t text=bos_text(0);
    if(text) bos_put_text(text,label[level<=BOS_LEVELS?level:0u]);
    return text;
}
/* The "Health display" row label (its own buffer, also rewritten in place). */
static uint32_t bos_display_label(unsigned level,unsigned sub)
{
    uint32_t text=bos_text(BOS_TEXT_DISPLAY);
    if(text) bos_put_text(text,!level?"Health display: (Boss off)":sub?"Health display: Sub-boss bar":"Health display: Boss bar");
    return text;
}

/* sub_000784A0(spacing): the edit screen calls it once at 0x0031D07E, just
 * before its "Done" row whose callback is the builder's second argument. */
void pc_mapmaker_bosses_rows(void (*original)(void))
{
    uint32_t r=0,cb=0,e,s,text;
    unsigned level;
    if(MAPMAKER_STACK_OK(0x28)) { r=WORD_AT(g_esp);cb=WORD_AT(g_esp+0x24u); } /* [esp+0x20] after the return */
    original();
    if(r!=BOS_ROWS_AT || cb!=BOS_CLOSE || !bos_on() || WORD_AT(BOS_SCENE)!=102u || !(e=bos_editor())) return;
    s=bos_selected();
    if(s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) return;
    if((text=bos_label(level=bos_get(e,s)))!=0) guest_call(sub_00079D60,6,text,BOS_ROW,0u,0u,BOS_CLOSE,0u);
    if(text && level && (text=bos_display_label(level,bos_get_sub(e,s)))!=0)
        guest_call(sub_00079D60,6,text,BOS_ROW_DISPLAY,0u,0u,BOS_CLOSE,0u);
}

/* sub_00078590(&field): binds the current list row to a field and selects its
 * value. At 0x0031CFD2 the row is the stock Health list (values 0..4); the
 * stronger options are appended first so the binding finds values 5..8. */
void pc_mapmaker_bosses_health_rows(void (*original)(void))
{
    static const char *const label[BOS_HEALTH_EXTRA]={"Brute","Juggernaut","Titan","Colossus"};
    uint32_t r=0,field=0,e,s,k;
    if(MAPMAKER_STACK_OK(8)) { r=WORD_AT(g_esp);field=WORD_AT(g_esp+4u); }
    if(r==BOS_HEALTH_ROW_AT && bos_on() && WORD_AT(BOS_SCENE)==102u && (e=bos_editor())!=0
       && (s=bos_selected())<bos_count(e) && field==bos_entry(e,s)+BOS_HEALTH)
        for(k=0;k<BOS_HEALTH_EXTRA;k++) {
            uint32_t text=bos_text(1u+BOS_BARS_MAX+k);
            if(!text) break;
            bos_put_text(text,label[k]);
            guest_call(sub_00071800,2,text,BOS_HEALTH_TOUGH+1u+k);
        }
    original();
}

/* Entry c is a copy of s ("Duplicate Story AI" / "Add linked copy",
 * aitemplates' ait_copy_all): every setting word +0x28..+0x4C equal (the
 * link +0x50 and later fields may differ). */
static int bos_is_copy(uint32_t e,uint32_t s,uint32_t c)
{
    uint32_t o;
    for(o=BOS_COPY_FIRST;o<BOS_COPY_END;o+=4u) if(WORD_AT(bos_entry(e,s)+o)!=WORD_AT(bos_entry(e,c)+o)) return 0;
    return 1;
}
/* sub_0031CAC0(menu): edit screen close, also the boss row's callback. A
 * close that adds one entry copied from the edited AI (the aitemplates rows,
 * chained inside this hook) gives the copy the source's Boss level and
 * Health display; the stronger Health value is entry +0x4C, copied there. */
void pc_mapmaker_bosses_close(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,row,s;
    unsigned level,sub;
    if(!bos_on() || !(e=bos_editor())) { original();return; }
    row=menu && MAPMAKER_PTR_OK(menu,0x190u)?WORD_AT(menu+0x18Cu):0u;
    s=bos_selected();
    if(row!=BOS_ROW && row!=BOS_ROW_DISPLAY) {    /* aitemplates (inside) may add a copy of s */
        uint32_t before=bos_count(e);
        level=s<before?bos_get(e,s):0u;sub=level?bos_get_sub(e,s):0u;
        original();
        if(level && bos_count(e)==before+1u && bos_selected()==s && bos_is_copy(e,s,before)) bos_set(e,before,level,sub);
        return;
    }
    if(s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) { original();return; }
    level=bos_get(e,s);sub=bos_get_sub(e,s);
    if(row==BOS_ROW) level=(level+1u)%(BOS_LEVELS+1u);
    else if(level) sub=!sub;                      /* Boss bar <-> Sub-boss bar; nothing while Off */
    bos_set(e,s,level,sub);
    bos_label(level);
    bos_display_label(level,bos_get_sub(e,s));
    MAPMAKER_GUEST_RET();                         /* sub_0031CAC0: ret; the screen stays open */
}

/* sub_0031C8B0(index): delete one Story AI (memmove of the later entries). */
void pc_mapmaker_bosses_delete(void (*original)(void))
{
    uint32_t i=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0xFFFFFFFFu,e,before,after;
    if(!bos_on() || !(e=bos_editor())) { original();return; }
    before=bos_count(e);
    original();
    after=bos_count(e);
    if(e!=bos.editor || i>=before || after+1u!=before) return;
    memmove(bos.level+i,bos.level+i+1u,before-i-1u);
    memmove(bos.sub+i,bos.sub+i+1u,before-i-1u);
    memmove(bos.colour+i,bos.colour+i+1u,(before-i-1u)*sizeof bos.colour[0]);
    bos.level[before-1u]=0;bos.sub[before-1u]=0;
}

/* sub_002FEBF0: editor map reset (new map; the loader calls it first). */
void pc_mapmaker_bosses_reset(void (*original)(void))
{
    original();
    memset(bos.level,0,sizeof bos.level);
    memset(bos.sub,0,sizeof bos.sub);
}

/* Visit each Story AI chunk's data in order; stops at the end tag, a malformed
 * size, the scan bound or after max chunks. Returns the number visited. */
typedef void (*BosChunkFn)(uint32_t data,unsigned ordinal,void *ctx);
static unsigned bos_walk(uint32_t container,unsigned max,BosChunkFn fn,void *ctx)
{
    uint32_t c,end;
    unsigned n=0,steps;
    if(!container || !MAPMAKER_PTR_OK(container,BOS_SCAN) || WORD_AT(container)!=BOS_MAGIC) return 0;
    c=container+8u;end=container+BOS_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==BOS_TAG_END || sz>end-c-8u) break;
        if(tag==BOS_TAG_SAI && sz>=BOS_CHUNK_MIN) {
            if(n>=max) break;
            fn(c+8u,n++,ctx);
        }
        c+=8u+sz;
    }
    return n;
}

static void bos_compile_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    uint32_t e=*(const uint32_t *)ctx,i=bos_get16(data+BOS_CHUNK_INDEX);
    unsigned level=bos_get(e,i),extra=0;
    (void)ordinal;
    BOS_B(data+BOS_CHUNK_FLAG)=(uint8_t)(level?BOS_FLAG_MARK|(bos_get_sub(e,i)?BOS_FLAG_SUB:0u)|(level-1u):0u);
    if(i<bos_count(e)) {
        uint32_t health=WORD_AT(bos_entry(e,i)+BOS_HEALTH);
        if(health>BOS_HEALTH_TOUGH) {
            if(health<=BOS_HEALTH_TOUGH+BOS_HEALTH_EXTRA) extra=health-BOS_HEALTH_TOUGH;
            else bos_log("health value out of range saved as Tough Guy",i,health);
            BOS_B(data+BOS_CHUNK_HEALTH)=(uint8_t)BOS_HEALTH_TOUGH;   /* the stock table has 5 entries */
        }
    }
    BOS_B(data+BOS_CHUNK_EXTRA)=(uint8_t)(extra?BOS_EXTRA_MARK|extra:0u);
}
/* sub_003006C0(container, flags): compile the editor into a container. */
void pc_mapmaker_bosses_compile(void (*original)(void))
{
    uint32_t container=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e;
    original();
    if(!bos_on() || !(e=bos_editor())) return;
    bos_walk(container,bos_count(e),bos_compile_chunk,&e);
}

typedef struct { uint32_t index[BOS_CAP], flag[BOS_CAP], extra[BOS_CAP], health[BOS_CAP]; } BosLoad;
static void bos_load_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    BosLoad *x=(BosLoad *)ctx;
    x->index[ordinal]=(uint32_t)(int32_t)(int16_t)bos_get16(data+BOS_CHUNK_INDEX);
    x->flag[ordinal]=BOS_B(data+BOS_CHUNK_FLAG);
    x->extra[ordinal]=BOS_B(data+BOS_CHUNK_EXTRA);
    x->health[ordinal]=BOS_B(data+BOS_CHUNK_HEALTH);
}
/* sub_00301A90(container, flags): editor load (appends the chunks' entries). */
void pc_mapmaker_bosses_load(void (*original)(void))
{
    static BosLoad x;
    uint32_t container=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,count,base,k;
    unsigned n;
    original();
    if(!bos_on() || !(e=bos_editor())) return;
    n=bos_walk(container,BOS_CAP,bos_load_chunk,&x);
    count=bos_count(e);
    if(n>count) n=count;
    base=count-n;
    for(k=0;k<n;k++) {
        unsigned level=bos_decode(x.flag[k]),extra=bos_decode_extra(x.extra[k],x.health[k]);
        uint32_t entry=bos_entry(e,base+k);
        int same=WORD_AT(entry+BOS_ENTRY_INDEX)==x.index[k];
        if(level && !same) {
            bos.dropped++;bos_log("boss setting dropped at load",k,x.flag[k]);
            level=0;
        }
        bos_set(e,base+k,level,bos_decode_sub(x.flag[k]));
        if(extra) {
            if(same && WORD_AT(entry+BOS_HEALTH)==BOS_HEALTH_TOUGH) WORD_AT(entry+BOS_HEALTH)=BOS_HEALTH_TOUGH+extra;
            else { bos.dropped++;bos_log("health setting dropped at load",k,x.extra[k]); }
        }
    }
}

/* ---- play (scene 100) ---------------------------------------------------- */
static void bos_level_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    uint32_t i=bos_get16(data+BOS_CHUNK_INDEX);
    unsigned level=bos_decode(BOS_B(data+BOS_CHUNK_FLAG)),sub=bos_decode_sub(BOS_B(data+BOS_CHUNK_FLAG)),*n=(unsigned *)ctx;
    unsigned extra=bos_decode_extra(BOS_B(data+BOS_CHUNK_EXTRA),BOS_B(data+BOS_CHUNK_HEALTH));
    (void)ordinal;
    if(!level && !extra) return;
    if(i>=BOS_LEVEL_SLOTS) { bos_log("boss index out of range in play",i,0);return; }
    bos_play.level[i]=(uint8_t)level;
    bos_play.extra[i]=(uint8_t)extra;
    bos_play.sub[i]=(uint8_t)(level && sub);
    if(level && sub) bos_play.subs++;
    memcpy(bos_play.name[i],(const char *)((uintptr_t)(data+BOS_CHUNK_NAME)+g_xbox_mem_offset),BOS_NAME-1u);
    bos_play.name[i][BOS_NAME-1u]=0;
    ++*n;
}
static float bos_range2(void)
{
    const char *v=getenv("TSFP_MAPMAKER_BOSS_RANGE");
    float r=BOS_RANGE;
    if(v && *v) {
        float f=(float)atof(v);
        if(f>=1.0f && f<=100000.0f) r=f;
    }
    return r*r;
}
/* sub_00240C50: a MapMaker map starts. */
void pc_mapmaker_bosses_level(void (*original)(void))
{
    unsigned n=0;
    original();
    memset(&bos_play,0,sizeof bos_play);
    if(!bos_on()) return;
    bos_walk(WORD_AT(BOS_CONTAINER),BOS_LEVEL_SLOTS,bos_level_chunk,&n);
    bos_play.range2=bos_range2();
    bos_play.active=n!=0;
}

/* Story AI index of live pool entry a with a boss or health setting, else
 * BOS_LEVEL_SLOTS; its entity and spawn record in *entity / *record. */
static uint32_t bos_actor(uint32_t a,uint32_t *entity,uint32_t *record)
{
    uint32_t ent,rec,i;
    if(!MAPMAKER_PTR_OK(a,BOS_ACTOR_SIZE)) return BOS_LEVEL_SLOTS;
    ent=WORD_AT(a);rec=WORD_AT(a+BOS_ACTOR_RECORD);
    if(!MAPMAKER_PTR_OK(ent,BOS_ENT_SIZE) || WORD_AT(ent+BOS_ENT_AI)!=a || !MAPMAKER_PTR_OK(rec,12u)) return BOS_LEVEL_SLOTS;
    i=WORD_AT(rec+8u)-WORD_AT(BOS_SAI_BASE)-1u;
    if(i>=BOS_LEVEL_SLOTS || (!bos_play.level[i] && !bos_play.extra[i])) return BOS_LEVEL_SLOTS;
    *entity=ent;*record=rec;
    return i;
}
/* Health factor: Boss x1..x8 times stronger health x2..x16. */
static unsigned bos_factor(uint32_t i) { return bos_multiplier(bos_play.level[i])<<bos_play.extra[i]; }
static void bos_scale(uint32_t a,uint32_t ent,uint32_t rec,unsigned factor)
{
    unsigned k,free_slot=BOS_LIST_MAX;
    float m=(float)factor,max,cur,scaled;
    uint32_t bits;
    if(factor<2u) return;
    for(k=0;k<BOS_LIST_MAX;k++) {
        if(bos_play.scaled[k].actor==a) break;
        if(!bos_play.scaled[k].actor && free_slot==BOS_LIST_MAX) free_slot=k;
    }
    if(k==BOS_LIST_MAX) {
        if(free_slot==BOS_LIST_MAX) { bos_log("boss health table full",BOS_LIST_MAX,0);return; }
        k=free_slot;bos_play.scaled[k].actor=a;bos_play.scaled[k].max_bits=0;
    }
    bits=WORD_AT(ent+BOS_ENT_MAX);
    if(bos_play.scaled[k].record==rec && bos_play.scaled[k].max_bits==bits) return;   /* already ours */
    max=bos_getf(ent+BOS_ENT_MAX);cur=bos_getf(ent+BOS_ENT_HEALTH);
    if(!(max>0.0f)) return;                       /* not set up yet */
    scaled=max*m;
    if(cur<=max*1.0001f) cur*=m;                 /* a fresh or partly hurt stock value */
    if(cur>scaled) cur=scaled;
    bos_setf(ent+BOS_ENT_MAX,scaled);
    bos_setf(ent+BOS_ENT_HEALTH,cur);
    bos_play.scaled[k].record=rec;
    bos_play.scaled[k].max_bits=WORD_AT(ent+BOS_ENT_MAX);
}
/* Scales every live Story AI with a factor and returns the living Boss AIs
 * within range of the current player, nearest first (at most max). */
static unsigned bos_nearest(uint32_t player,uint32_t *ents,uint32_t *index,unsigned max)
{
    uint32_t pe=WORD_AT(player+0x14u),count=WORD_AT(BOS_LIST_COUNT),humans=WORD_AT(BOS_HUMANS),i;
    float px,py,pz,dist[BOS_BARS_MAX];
    unsigned n=0,k;
    if(!MAPMAKER_PTR_OK(pe,BOS_ENT_SIZE)) return 0;
    px=bos_getf(pe+BOS_ENT_POS);py=bos_getf(pe+BOS_ENT_POS+4u);pz=bos_getf(pe+BOS_ENT_POS+8u);
    if(count>BOS_LIST_MAX) count=BOS_LIST_MAX;
    if(max>BOS_BARS_MAX) max=BOS_BARS_MAX;
    for(i=humans<count?humans:count;i<count;i++) {
        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec);
        float dx,dy,dz,d;
        if(s>=BOS_LEVEL_SLOTS) continue;
        bos_scale(a,ent,rec,bos_factor(s));
        if(!bos_play.level[s] || bos_play.sub[s] || !(bos_getf(ent+BOS_ENT_HEALTH)>0.0f)) continue;   /* Sub-boss: meter only */
        dx=bos_getf(ent+BOS_ENT_POS)-px;dy=bos_getf(ent+BOS_ENT_POS+4u)-py;dz=bos_getf(ent+BOS_ENT_POS+8u)-pz;
        d=dx*dx+dy*dy+dz*dz;
        if(!(d<=bos_play.range2)) continue;
        for(k=n;k>0 && dist[k-1u]>d;k--) if(k<max) { dist[k]=dist[k-1u];ents[k]=ents[k-1u];index[k]=index[k-1u]; }
        if(k<max) { dist[k]=d;ents[k]=ent;index[k]=s;if(n<max) n++; }
    }
    return n;
}
/* Per-boss copy of the bar's smoothing state (HUD block +0x171C/+0x1724/+0x172C). */
static unsigned bos_smooth_slot(uint32_t hud,uint32_t ent)
{
    unsigned k,old=0;
    for(k=0;k<BOS_SMOOTH;k++) {
        if(bos_play.smooth[k].hud==hud && bos_play.smooth[k].entity==ent) return k;
        if(bos_play.smooth[k].frame<bos_play.smooth[old].frame) old=k;
    }
    memset(&bos_play.smooth[old],0,sizeof bos_play.smooth[old]);   /* oldest: a fresh bar */
    bos_play.smooth[old].hud=hud;bos_play.smooth[old].entity=ent;
    return old;
}
static void bos_smooth_io(uint32_t hud,uint32_t *state,int to_hud)
{
    unsigned k;
    for(k=0;k<3u;k++) {
        uint32_t a=hud+BOS_HUD_SMOOTH+k*8u;
        if(to_hud) WORD_AT(a)=state[k]; else state[k]=WORD_AT(a);
    }
}
/* sub_00139740 shows cur/max + drain x fade (+0x1724 x +0x172C) and only
 * raises the fade to 1 on a hit; sub_0014B090 (game update, per player) is
 * what lowers the fade of the LIVE HUD block each tick: +0x172C -= dt x
 * [0x00475D10] (floor 0) while +0x2C += dt. A boss's copy is swapped out
 * during that tick, so without this its fade stayed 1 and the bar stayed
 * at its pre-damage length. Same rule, applied by the HUD clock time since
 * the copy's last draw (a clock that went back counts as no time). */
static void bos_smooth_fade(unsigned s,uint32_t hud)
{
    float now=bos_getf(hud+BOS_HUD_CLOCK),dt=now-bos_play.smooth[s].clock,fade;
    if(bos_play.smooth[s].timed && dt>0.0f) {
        memcpy(&fade,&bos_play.smooth[s].state[2],4);
        fade-=dt*bos_getf(BOS_FADE_RATE);
        if(fade<0.0f) fade=0.0f;
        memcpy(&bos_play.smooth[s].state[2],&fade,4);
    }
    bos_play.smooth[s].clock=now;bos_play.smooth[s].timed=1u;
}
static int bos_step(void)
{
    float scale=bos_getf(BOS_HUD_SCALE);
    if(!(scale>=0.25f && scale<=4.0f)) scale=1.0f;
    return (int)(BOS_BAR_STEP*scale+0.5f);
}

static void bos_probe_frame(void);
/* sub_0014CC80 (ebx = HUD block): the story boss bar, every HUD frame. */
void pc_mapmaker_bosses_hud(void (*original)(void))
{
    uint32_t hud=g_ebx,player,p,ents[BOS_BARS_MAX],index[BOS_BARS_MAX],stock[3],margin;
    unsigned n,k,slot,cap,any=0;
    int step;
    GuestRegisters r;
    original();
    bos_probe_frame();
    if(!bos_play.active || !mapmaker_limits.limits || WORD_AT(BOS_SCENE)!=100u || BOS_B(BOS_MODE)!=10u) return;
    player=WORD_AT(BOS_PLAYER);
    if(!MAPMAKER_PTR_OK(player,0x1000u) || (p=WORD_AT(player+4u))>3u || hud!=BOS_HUD+p*BOS_HUD_STRIDE) return;
    cap=WORD_AT(BOS_HUMANS)>1u?BOS_BARS_SPLIT:BOS_BARS_MAX;
    slot=(WORD_AT(hud+BOS_HUD_FLAGS)&1u)?1u:0u;  /* a stock bar keeps the bottom slot */
    n=bos_nearest(player,ents,index,cap-slot);
    if(!n || !MAPMAKER_STACK_OK(0x100)) return;
    bos_play.frame++;
    step=bos_step();
    margin=WORD_AT(BOS_MARGIN);
    bos_smooth_io(hud,stock,0);
    for(k=0;k<n && slot<cap;k++) {
        uint32_t text=bos_text(1u+k),buf,drawn;
        unsigned s=bos_smooth_slot(hud,ents[k]);
        if(!text) break;
        bos_put_text(text,index[k]<BOS_LEVEL_SLOTS && bos_play.name[index[k]][0]?bos_play.name[index[k]]:"Boss");
        bos_smooth_fade(s,hud);
        bos_smooth_io(hud,bos_play.smooth[s].state,1);
        WORD_AT(BOS_MARGIN)=margin+(uint32_t)(step*(int)slot);
        save_registers(&r);
        buf=g_esp-0x80u;g_esp=buf;
        WORD_AT(buf+0x40u)=0;
        g_esi=hud;g_edi=ents[k];
        guest_call(sub_00139740,3,text,0u,buf+0x40u);
        drawn=WORD_AT(buf+0x40u);
        restore_registers(&r);
        bos_smooth_io(hud,bos_play.smooth[s].state,0);
        bos_play.smooth[s].frame=bos_play.frame;
        if(drawn) { any=1;slot++; }
    }
    WORD_AT(BOS_MARGIN)=margin;
    bos_smooth_io(hud,stock,1);                   /* the stock bar's own state */
    if(any) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;      /* as the stock bar (0x0014D0D5) */
}

/* sub_0014D1F0 (HUD, per viewport): its scene-100 story loop draws the
 * overhead meter sub_0013A400(actor, 0, 0x7F, 0x6E) for actors whose entity
 * has a damage-stage record (+0x268, 0x0014D249). Ordinary Story AIs never
 * get one, and making one (sub_0012C800: 17-slot bump pool 0x00745938, never
 * freed until 0x0014853D; +0x268 has ~100 readers incl. the damage flash
 * sub_0014D3A0) is not safe, so before the original (the loop runs first
 * thing, no state set before it) this draws the same meter for each living
 * Sub-boss AI without a record; one with a record is left to the stock loop.
 * sub_0013A400 only draws (cdecl, plain ret; reads actor +0xC68 fade, entity
 * +0x10 visibility, +0x64..+0x6C, [entity+4] +0x8C/+0x10 node matrices). */
void pc_mapmaker_bosses_meters(void (*original)(void))
{
    uint32_t player,pe,count,humans,i;
    GuestRegisters r;
    if(!bos_play.subs || !mapmaker_limits.limits || WORD_AT(BOS_SCENE)!=100u || BOS_B(BOS_MODE)!=10u
       || !MAPMAKER_STACK_OK(0x400)) { original();return; }
    player=WORD_AT(BOS_PLAYER);
    if(!MAPMAKER_PTR_OK(player,0x18u) || (int32_t)WORD_AT(player+4u)<0) { original();return; }   /* 0x0014D1FB/0x0014D206 */
    pe=WORD_AT(player+0x14u);
    count=WORD_AT(BOS_LIST_COUNT);humans=WORD_AT(BOS_HUMANS);
    if(count>BOS_LIST_MAX) count=BOS_LIST_MAX;
    save_registers(&r);
    for(i=humans<count?humans:count;i<count;i++) {
        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),inst;
        if(s>=BOS_LEVEL_SLOTS || !bos_play.level[s] || !bos_play.sub[s]) continue;
        if(ent==pe || !MAPMAKER_PTR_OK(ent,BOS_ENT_RECORD+4u) || WORD_AT(ent+BOS_ENT_RECORD)) continue;
        if(!(bos_getf(ent+BOS_ENT_HEALTH)>0.0f) || !(bos_getf(ent+BOS_ENT_MAX)>0.0f)) continue;
        inst=WORD_AT(ent+BOS_ENT_INST);
        if(!MAPMAKER_PTR_OK(inst,0x90u) || !MAPMAKER_PTR_OK(WORD_AT(inst+0x8Cu),0x1Cu) || !MAPMAKER_PTR_OK(WORD_AT(inst+0x10u),0x40u)) continue;
        guest_call(sub_0013A400,4,a,0u,0x7Fu,0x6Eu);
        restore_registers(&r);
        bos_play.meters++;
    }
    restore_registers(&r);
    original();
}

/* ---- creature probe (read-only) ------------------------------------------ */
static const uint8_t bos_story_ids[]={0x04,0x12,0x15,0x28,0x2C,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,
    0x37,0x38,0x39,0x3A,0x3B,0x42,0x43,0x44,0x45,0x46,0x4A,0x4B,0x4D,0x4F,0x50,0x52,0x62,0x64,0x65,0x75};
static struct {
    int read, on;
    FILE *out;
    unsigned lines, level_new;
    uint32_t scene, pool;
    struct { uint32_t actor, record, id; } seen[BOS_PROBE_LEVEL];
} bos_probe;

static void bos_probe_line(const char *fmt,...)
{
    va_list ap;
    if(bos_probe.lines>=BOS_PROBE_LINES) return;
    if(++bos_probe.lines==BOS_PROBE_LINES) { fprintf(bos_probe.out,"[creature-probe] line cap reached\n");fflush(bos_probe.out);return; }
    va_start(ap,fmt);
    vfprintf(bos_probe.out,fmt,ap);
    va_end(ap);
    fputc('\n',bos_probe.out);
    fflush(bos_probe.out);
}
static int bos_probe_enabled(void)
{
    if(!bos_probe.read) {
        bos_probe.read=1;
        const char *v=getenv("TSFP_MAPMAKER_CREATURE_PROBE"),*path=getenv("TSFP_MAPMAKER_CREATURE_PROBE_LOG");
        bos_probe.on=v && v[0]=='1' && !v[1];
        bos_probe.out=stderr;
        if(bos_probe.on && path && *path) {
            FILE *f=fopen(path,"a");
            if(f) bos_probe.out=f;
        }
    }
    return bos_probe.on;
}
static void bos_probe_model(uint32_t id,const char *which,uint32_t model)
{
    int16_t slot;
    uint32_t table=WORD_AT(BOS_RES_TABLE),entry;
    if(model>=BOS_RES_IDS) { bos_probe_line("[creature-probe]   char 0x%02X %s 0x%X (not a resource id)",id,which,model);return; }
    slot=(int16_t)bos_get16(BOS_RES_SLOT+model*2u);
    entry=table+(uint32_t)(int32_t)slot*0x30u;
    if(slot<0 || !MAPMAKER_PTR_OK(table,0x30u) || !MAPMAKER_PTR_OK(entry,0x30u))
        bos_probe_line("[creature-probe]   char 0x%02X %s 0x%X slot %d (no entry)",id,which,model,slot);
    else
        bos_probe_line("[creature-probe]   char 0x%02X %s 0x%X slot %d type 0x%X res 0x%X/0x%X flags 0x%X",
                       id,which,model,slot,WORD_AT(entry),WORD_AT(entry+0x10u),WORD_AT(entry+0x14u),WORD_AT(entry+0x28u));
}
static void bos_probe_level(uint32_t scene,uint32_t pool)
{
    unsigned k;
    memset(bos_probe.seen,0,sizeof bos_probe.seen);
    bos_probe.level_new=0;bos_probe.scene=scene;bos_probe.pool=pool;
    bos_probe_line("[creature-probe] level: scene %u mode %u humans %u pool 0x%08X res table 0x%08X",
                   scene,BOS_B(BOS_MODE),WORD_AT(BOS_HUMANS),pool,WORD_AT(BOS_RES_TABLE));
    for(k=0;k<sizeof bos_story_ids;k++) {
        uint32_t row=BOS_CHARS+bos_story_ids[k]*0x30u;
        bos_probe_model(bos_story_ids[k],"model",WORD_AT(row+4u));
        bos_probe_model(bos_story_ids[k],"model2",WORD_AT(row+8u));
    }
}
static void bos_probe_frame(void)
{
    uint32_t scene,pool,count,i;
    if(!bos_probe_enabled() || bos_probe.lines>=BOS_PROBE_LINES) return;
    scene=WORD_AT(BOS_SCENE);pool=WORD_AT(BOS_POOLPTR);
    if(scene!=bos_probe.scene || pool!=bos_probe.pool) bos_probe_level(scene,pool);
    count=WORD_AT(BOS_LIST_COUNT);
    if(count>BOS_LIST_MAX) count=BOS_LIST_MAX;
    for(i=0;i<count && bos_probe.level_new<BOS_PROBE_LEVEL;i++) {
        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent,rec,id,k,model=0,skel=0xFFFFFFFFu,inst,hdr;
        if(!MAPMAKER_PTR_OK(a,BOS_ACTOR_SIZE)) continue;
        ent=WORD_AT(a);rec=WORD_AT(a+BOS_ACTOR_RECORD);id=WORD_AT(a+0x48u)&0xFFFFu;
        for(k=0;k<bos_probe.level_new;k++)
            if(bos_probe.seen[k].actor==a && bos_probe.seen[k].record==rec && bos_probe.seen[k].id==id) break;
        if(k<bos_probe.level_new) continue;
        bos_probe.seen[bos_probe.level_new].actor=a;bos_probe.seen[bos_probe.level_new].record=rec;
        bos_probe.seen[bos_probe.level_new].id=id;bos_probe.level_new++;
        if(MAPMAKER_PTR_OK(ent,BOS_ENT_SIZE)) {
            model=bos_get16(ent+BOS_ENT_MODEL);
            inst=WORD_AT(ent+4u);
            if(MAPMAKER_PTR_OK(inst,0x8Cu) && MAPMAKER_PTR_OK(hdr=WORD_AT(inst+0x88u),8u)) skel=WORD_AT(hdr+4u);
        }
        bos_probe_line("[creature-probe] char #%u id 0x%02X var %u ai+10 %08X ai+14 %08X rec%s +0 %08X +4 %08X id %d "
                       "model 0x%X skel 0x%X health %.3f/%.3f",
                       i,id,bos_get16(a+0x4Au),WORD_AT(a+0x10u),WORD_AT(a+0x14u),
                       MAPMAKER_PTR_OK(rec,12u)?"":" (none)",
                       MAPMAKER_PTR_OK(rec,12u)?WORD_AT(rec):0u,MAPMAKER_PTR_OK(rec,12u)?WORD_AT(rec+4u):0u,
                       MAPMAKER_PTR_OK(rec,12u)?(int)WORD_AT(rec+8u):-1,model,skel,
                       MAPMAKER_PTR_OK(ent,BOS_ENT_SIZE)?(double)bos_getf(ent+BOS_ENT_HEALTH):0.0,
                       MAPMAKER_PTR_OK(ent,BOS_ENT_SIZE)?(double)bos_getf(ent+BOS_ENT_MAX):0.0);
        if(id<0xB0u) {
            uint32_t row=BOS_CHARS+id*0x30u;
            bos_probe_model(id,"model",WORD_AT(row+4u));
        }
    }
}
