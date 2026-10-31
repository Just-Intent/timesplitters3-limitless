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
 * undo shadow). A duplicate or linked copy starts with Off; the setting is
 * per AI, not shared by a group.
 *
 * Save format. Story AI chunk (tag 0x00C7766C) data byte +0x29: 0 = no
 * boss (every stock map), 0xB0 | (level - 1) = boss with health x1, x2, x4,
 * x8. The compiler zeroes the container (rep stosd 0x0030070D); the chunk
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
#define BOS_EDITOR_SIZE 0x58390u
#define BOS_SAI 0x11950u                /* editor.json: 255 x 0xE4 */
#define BOS_STRIDE 0xE4u
#define BOS_COUNT 0x280u
#define BOS_CAP 255u
#define BOS_SELECTED 0x00762F94u        /* byte: entry being edited */
#define BOS_SCENE 0x0079094Cu
#define BOS_MODE 0x007DE455u            /* byte: 10 = story */
#define BOS_CONTAINER 0x00784014u
#define BOS_SAI_BASE 0x00783A50u        /* R: Story AI ids are R + index + 1 */
#define BOS_CLOSE 0x0031CAC0u           /* edit screen close, also the row callback */
#define BOS_ROWS_AT 0x0031D083u         /* return from sub_000784A0 before "Done" */
#define BOS_ROW 0x424F5301u
#define BOS_TYPE 0x34u
#define BOS_COLOUR 0x60u
#define BOS_ENTRY_INDEX 0x24u           /* loader: chunk +0x22 -> entry +0x24 */
#define BOS_TAG_SAI 0x00C7766Cu
#define BOS_TAG_END 0x00C7766Au
#define BOS_MAGIC 0x00C77667u
#define BOS_SCAN 0x00080000u            /* container scan bound: 512 KB */
#define BOS_CHUNK_NAME 0x00u            /* 32 bytes, NUL terminated */
#define BOS_CHUNK_INDEX 0x22u
#define BOS_CHUNK_FLAG 0x29u
#define BOS_CHUNK_MIN 0x44u             /* fixed part of a Story AI chunk */
#define BOS_FLAG_MARK 0xB0u             /* +0x29 = 0xB0 | (level - 1) */
#define BOS_FLAG_MASK 0xFCu
#define BOS_LEVELS 4u                   /* 1..4: health x1, x2, x4, x8 */
#define BOS_LEVEL_SLOTS 1024u           /* play-time index bound (= aitemplates) */
#define BOS_NAME 32u

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
#define BOS_RANGE 30.0f                 /* game units; TSFP_MAPMAKER_BOSS_RANGE */

#define BOS_CHARS 0x0053A3CCu           /* per character id, 0x30 bytes */
#define BOS_RES_SLOT 0x0078B860u        /* int16 per resource id < 0x9D0 */
#define BOS_RES_IDS 0x9D0u
#define BOS_RES_TABLE 0x00791D4Cu       /* -> 0x30-byte entries */
#define BOS_PROBE_LEVEL 160u
#define BOS_PROBE_LINES 4096u

#define BOS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00079D60(void),sub_00139740(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

static struct {
    uint32_t editor;                    /* editor object the table belongs to */
    uint8_t level[BOS_CAP];             /* 0 off, 1..4 */
    uint16_t colour[BOS_CAP];
    uint32_t text;                      /* guest: row label (32 B) + HUD label (32 B) */
    unsigned logged, dropped, bad_flags;
} bos;

static struct {
    int active;
    uint8_t level[BOS_LEVEL_SLOTS];
    char name[BOS_LEVEL_SLOTS][BOS_NAME];
    struct { uint32_t actor, record, max_bits; } scaled[BOS_LIST_MAX];
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

/* ---- editor ------------------------------------------------------------- */
static int bos_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }
static uint32_t bos_editor(void)
{
    uint32_t e=WORD_AT(BOS_EDITOR_PTR);
    return e && MAPMAKER_PTR_OK(e,BOS_EDITOR_SIZE)?e:0u;
}
static uint32_t bos_count(uint32_t e) { uint32_t c=WORD_AT(e+BOS_COUNT); return c<=BOS_CAP?c:0u; }
static uint32_t bos_entry(uint32_t e,uint32_t i) { return e+BOS_SAI+i*BOS_STRIDE; }
static uint32_t bos_selected(void) { return WORD_AT(BOS_SELECTED)&0xFFu; }
static uint16_t bos_colour_of(uint32_t e,uint32_t i) { return (uint16_t)WORD_AT(bos_entry(e,i)+BOS_COLOUR); }
/* The entry's boss level; a stale slot (other editor, other colour) is 0. */
static unsigned bos_get(uint32_t e,uint32_t i)
{
    if(e!=bos.editor || i>=bos_count(e) || !bos.level[i]) return 0;
    if(bos.colour[i]!=bos_colour_of(e,i)) {
        bos.dropped++;bos_log("boss setting dropped: entry changed",i,bos.colour[i]);
        bos.level[i]=0;
        return 0;
    }
    return bos.level[i];
}
static void bos_set(uint32_t e,uint32_t i,unsigned level)
{
    if(bos.editor!=e) { memset(bos.level,0,sizeof bos.level);bos.editor=e; }
    if(i>=BOS_CAP) return;
    bos.level[i]=(uint8_t)(level<=BOS_LEVELS?level:0u);
    bos.colour[i]=bos_colour_of(e,i);
}
static uint32_t bos_text(unsigned which)
{
    if(!bos.text) {
        bos.text=xbox_HeapAlloc(2u*BOS_NAME,4u);
        if(!bos.text) return 0;
        memset((void *)((uintptr_t)bos.text+g_xbox_mem_offset),0,2u*BOS_NAME);
    }
    return bos.text+which*BOS_NAME;
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

/* sub_000784A0(spacing): the edit screen calls it once at 0x0031D07E, just
 * before its "Done" row whose callback is the builder's second argument. */
void pc_mapmaker_bosses_rows(void (*original)(void))
{
    uint32_t r=0,cb=0,e,s,text;
    if(MAPMAKER_STACK_OK(0x28)) { r=WORD_AT(g_esp);cb=WORD_AT(g_esp+0x24u); } /* [esp+0x20] after the return */
    original();
    if(r!=BOS_ROWS_AT || cb!=BOS_CLOSE || !bos_on() || WORD_AT(BOS_SCENE)!=102u || !(e=bos_editor())) return;
    s=bos_selected();
    if(s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) return;
    if((text=bos_label(bos_get(e,s)))!=0) guest_call(sub_00079D60,6,text,BOS_ROW,0u,0u,BOS_CLOSE,0u);
}

/* sub_0031CAC0(menu): edit screen close, also the boss row's callback. */
void pc_mapmaker_bosses_close(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,row,s;
    unsigned level;
    if(!bos_on() || !(e=bos_editor())) { original();return; }
    row=menu && MAPMAKER_PTR_OK(menu,0x190u)?WORD_AT(menu+0x18Cu):0u;
    s=bos_selected();
    if(row!=BOS_ROW || s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) { original();return; }
    level=(bos_get(e,s)+1u)%(BOS_LEVELS+1u);
    bos_set(e,s,level);
    bos_label(level);
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
    memmove(bos.colour+i,bos.colour+i+1u,(before-i-1u)*sizeof bos.colour[0]);
    bos.level[before-1u]=0;
}

/* sub_002FEBF0: editor map reset (new map; the loader calls it first). */
void pc_mapmaker_bosses_reset(void (*original)(void))
{
    original();
    memset(bos.level,0,sizeof bos.level);
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
    unsigned level=bos_get(e,i);
    (void)ordinal;
    BOS_B(data+BOS_CHUNK_FLAG)=(uint8_t)(level?BOS_FLAG_MARK|(level-1u):0u);
}
/* sub_003006C0(container, flags): compile the editor into a container. */
void pc_mapmaker_bosses_compile(void (*original)(void))
{
    uint32_t container=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e;
    original();
    if(!bos_on() || !(e=bos_editor())) return;
    bos_walk(container,bos_count(e),bos_compile_chunk,&e);
}

typedef struct { uint32_t index[BOS_CAP], flag[BOS_CAP]; } BosLoad;
static void bos_load_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    BosLoad *x=(BosLoad *)ctx;
    x->index[ordinal]=(uint32_t)(int32_t)(int16_t)bos_get16(data+BOS_CHUNK_INDEX);
    x->flag[ordinal]=BOS_B(data+BOS_CHUNK_FLAG);
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
        unsigned level=bos_decode(x.flag[k]);
        if(level && WORD_AT(bos_entry(e,base+k)+BOS_ENTRY_INDEX)!=x.index[k]) {
            bos.dropped++;bos_log("boss setting dropped at load",k,x.flag[k]);
            level=0;
        }
        bos_set(e,base+k,level);
    }
}

/* ---- play (scene 100) ---------------------------------------------------- */
static void bos_level_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    uint32_t i=bos_get16(data+BOS_CHUNK_INDEX);
    unsigned level=bos_decode(BOS_B(data+BOS_CHUNK_FLAG)),*n=(unsigned *)ctx;
    (void)ordinal;
    if(!level) return;
    if(i>=BOS_LEVEL_SLOTS) { bos_log("boss index out of range in play",i,0);return; }
    bos_play.level[i]=(uint8_t)level;
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

/* Boss level of live pool entry a (0 = none); its entity in *entity. */
static unsigned bos_actor(uint32_t a,uint32_t *entity,uint32_t *record)
{
    uint32_t ent,rec,i;
    if(!MAPMAKER_PTR_OK(a,BOS_ACTOR_SIZE)) return 0;
    ent=WORD_AT(a);rec=WORD_AT(a+BOS_ACTOR_RECORD);
    if(!MAPMAKER_PTR_OK(ent,BOS_ENT_SIZE) || WORD_AT(ent+BOS_ENT_AI)!=a || !MAPMAKER_PTR_OK(rec,12u)) return 0;
    i=WORD_AT(rec+8u)-WORD_AT(BOS_SAI_BASE)-1u;
    if(i>=BOS_LEVEL_SLOTS || !bos_play.level[i]) return 0;
    *entity=ent;*record=rec;
    return bos_play.level[i];
}
static void bos_scale(uint32_t a,uint32_t ent,uint32_t rec,unsigned level)
{
    unsigned k,free_slot=BOS_LIST_MAX;
    float m=(float)bos_multiplier(level),max,cur,scaled;
    uint32_t bits;
    if(level<2u) return;
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
/* Nearest living boss entity within range of the current player, or 0;
 * scales boss health on the way. */
static uint32_t bos_nearest(uint32_t player,unsigned *index)
{
    uint32_t pe=WORD_AT(player+0x14u),count=WORD_AT(BOS_LIST_COUNT),humans=WORD_AT(BOS_HUMANS),i,best=0;
    float px,py,pz,best_d=0.0f;
    if(!MAPMAKER_PTR_OK(pe,BOS_ENT_SIZE)) return 0;
    px=bos_getf(pe+BOS_ENT_POS);py=bos_getf(pe+BOS_ENT_POS+4u);pz=bos_getf(pe+BOS_ENT_POS+8u);
    if(count>BOS_LIST_MAX) count=BOS_LIST_MAX;
    for(i=humans<count?humans:count;i<count;i++) {
        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0;
        unsigned level=bos_actor(a,&ent,&rec);
        float dx,dy,dz,d;
        if(!level) continue;
        bos_scale(a,ent,rec,level);
        if(!(bos_getf(ent+BOS_ENT_HEALTH)>0.0f)) continue;
        dx=bos_getf(ent+BOS_ENT_POS)-px;dy=bos_getf(ent+BOS_ENT_POS+4u)-py;dz=bos_getf(ent+BOS_ENT_POS+8u)-pz;
        d=dx*dx+dy*dy+dz*dz;
        if(!(d<=bos_play.range2) || (best && d>=best_d)) continue;
        best=ent;best_d=d;*index=WORD_AT(rec+8u)-WORD_AT(BOS_SAI_BASE)-1u;
    }
    return best;
}

static void bos_probe_frame(void);
/* sub_0014CC80 (ebx = HUD block): the story boss bar, every HUD frame. */
void pc_mapmaker_bosses_hud(void (*original)(void))
{
    uint32_t hud=g_ebx,player,p,ent,text,buf,drawn;
    unsigned index=0;
    GuestRegisters r;
    original();
    bos_probe_frame();
    if(!bos_play.active || !mapmaker_limits.limits || WORD_AT(BOS_SCENE)!=100u || BOS_B(BOS_MODE)!=10u) return;
    player=WORD_AT(BOS_PLAYER);
    if(!MAPMAKER_PTR_OK(player,0x1000u) || (p=WORD_AT(player+4u))>3u || hud!=BOS_HUD+p*BOS_HUD_STRIDE) return;
    ent=bos_nearest(player,&index);
    if(!ent || (WORD_AT(hud+BOS_HUD_FLAGS)&1u) || !MAPMAKER_STACK_OK(0x100) || !(text=bos_text(1))) return;
    bos_put_text(text,index<BOS_LEVEL_SLOTS && bos_play.name[index][0]?bos_play.name[index]:"Boss");
    save_registers(&r);
    buf=g_esp-0x80u;g_esp=buf;
    WORD_AT(buf+0x40u)=0;
    g_esi=hud;g_edi=ent;
    guest_call(sub_00139740,3,text,0u,buf+0x40u);
    drawn=WORD_AT(buf+0x40u);
    restore_registers(&r);
    if(drawn) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;    /* as the stock bar (0x0014D0D5) */
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
