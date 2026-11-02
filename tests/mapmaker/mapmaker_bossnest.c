/* Asset-free contract for the split Crowe (pc_mapmaker_crowe.inc) and the
 * Crow walker (pc_mapmaker_realbosses.inc) in one map, as the build has
 * them: both areas in one translation unit, sharing the boss particle/effect
 * statics 0x0078B080..0x0078B1DB through pc_mapmaker_bossfx.inc.
 *
 * v0.4.0-mapmaker.28 crash (four of each, crash-20261102-024139391): the
 * per-character update ran with esp 0x20 too high. The Crowe's egg drop made
 * four null props per egg and their release on landing skipped four wild
 * calls, each leaving esp 8 bytes high. Checked here: the egg drop is skipped
 * inside a Crowe's fight with esp balanced, and every nesting of the two
 * areas' entries (a walker damage callback during a Crowe think, that Crowe's
 * hit callback inside it, another Crowe and walker inside that, and the
 * reverse) sees its own state and statics, leaves esp balanced and puts
 * every global back. Fake guest memory; fake originals pop their return
 * address like the lifted callees. */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }

#define CONT 0x00200000u
#define HEAP 0x00300000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define STACK 0x00600000u               /* nested calls: one 0x1000 frame per depth */
#define R 10u
#define CST(k) (0x00700000u+(uint32_t)(k)*0x3000u)   /* Crowe states (0x2E04) */
#define CPT(k) (0x00710000u+(uint32_t)(k)*0x8000u)   /* Crowe particle blocks */
#define CEF(k) (0x007E0000u+(uint32_t)(k)*0x1000u)   /* Crowe effect blocks */
#define WST(k) (0x00740000u+(uint32_t)(k)*0x8000u)   /* walker states (0x64DC) */
#define WPT(k) (0x00780000u+(uint32_t)(k)*0x8000u)
#define WEF(k) (0x007C0000u+(uint32_t)(k)*0x1000u)
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define FXW(fx,a) (*(uint32_t *)((uint8_t *)&(fx)+((a)-0x0078B080u)))

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_00079D60(void) { g_ecx=2; }
void sub_00071630(void) { g_ecx=3; }
void sub_00071C70(void) { g_ecx=4; }
void sub_0005A130(void) { g_ecx=5; }
void sub_00062260(void) { g_ecx=6; }
void sub_00287E00(void) { g_ecx=7; }
void sub_00059DB0(void) { g_ecx=8; }
void sub_000EA9C0(void) { g_ecx=9; }
void sub_0010EB90(void) { g_ecx=10; }
void sub_002976F0(void) { g_ecx=11; }
void sub_0015DDA0(void) { g_ecx=12; }
void sub_0029CCB0(void) { g_ecx=13; }
void sub_00019D70(void) { g_ecx=14; }
void sub_002A7F30(void) { g_ecx=15; }
void sub_0005F460(void) { g_ecx=16; }
void sub_0028D6D0(void) { g_ecx=17; }
void sub_002A62C0(void) { g_ecx=18; }
void sub_0028D710(void) { g_ecx=19; }
void sub_000EFBD0(void) { g_ecx=20; }
void sub_0029CEF0(void) { g_ecx=21;g_esp+=4u; }
void sub_00286D30(void) { g_ecx=22;g_esp+=4u; }
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p=heap_next;(void)alignment;heap_next+=(bytes+15u)&~15u;return p; }
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)area;(void)hook;return 1; }

static unsigned ncrowe, nwalk;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) return 3u;                         /* every pak mounted */
    if(f==sub_0028D6D0) {                                  /* a Crowe's state and effect blocks */
        WORD_AT(0x0075F5E8u)=CST(ncrowe);WORD_AT(0x0078B090u)=CEF(ncrowe);WORD_AT(0x0078B098u)=CEF(ncrowe)+0x800u;
        return 0;
    }
    if(f==sub_002A62C0) { WORD_AT(0x0078B0D0u)=CPT(ncrowe);ncrowe++;return 0; }
    if(f==sub_002976F0) {                                  /* a walker's state, particles, effect blocks */
        WORD_AT(0x0078B1E8u)=WST(nwalk);WORD_AT(0x0078B0D0u)=WPT(nwalk);WORD_AT(0x0078B090u)=WEF(nwalk);nwalk++;
        return 0;
    }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_realbosses.inc"
#include "pc_mapmaker_crowe.inc"

static int failures, originals, depth;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void fake(void) { originals++;g_esp+=4u; }
/* hook fn with return address ret and argument a1 on a fresh frame below the
 * current one; esp and the argument slot must come back exactly */
static void call_at(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,int line)
{
    uint32_t sp=g_esp,frame=STACK-(uint32_t)depth*0x1000u;
    depth++;
    g_esp=frame;WORD_AT(frame)=ret;WORD_AT(frame+4u)=a1;WORD_AT(frame+8u)=0;WORD_AT(frame+12u)=0;
    fn(orig);
    if(g_esp!=frame+4u) { fprintf(stderr,"%s:%d: esp %+d (depth %d)\n",__FILE__,line,(int)(g_esp-frame),depth);failures++; }
    if(WORD_AT(frame+4u)!=a1) { fprintf(stderr,"%s:%d: argument slot changed\n",__FILE__,line);failures++; }
    depth--;
    g_esp=sp;
}
#define CALL(fn,orig,ret,a1) call_at(fn,orig,ret,a1,__LINE__)
static void actor(unsigned i,uint32_t id,uint32_t model,uint32_t index)
{
    memset((void *)((uintptr_t)A(i)+g_xbox_mem_offset),0,0xEA0u);
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);HALF_AT(ENT(i)+0x94u)=(uint16_t)model;
    WORD_AT(REC(i))=0x11000000u;WORD_AT(REC(i)+4u)=0;WORD_AT(REC(i)+8u)=R+index+1u;
    WORD_AT(A(i)+0x20u)=4u;WORD_AT(A(i)+0x9D0u)=0xFFFFFFFFu;     /* fighting */
}
/* map container: the map's set {0x0C, 0x46, 0x64}; Story AI chunks 0-3 with
 * character slots 1, 1, 2, 2 (two Crowes, two walkers) */
static void container(void)
{
    static const uint16_t ids[]={0x0C,0x46,0x64};
    static const uint8_t slot[]={1,1,2,2};
    uint32_t c=CONT+8u,k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x00080000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    WORD_AT(c)=0x00C77668u;WORD_AT(c+4u)=0x18u+20u*4u;
    HALF_AT(c+8u+0x14u)=0x11u;BYTE_AT(c+8u+0x16u)=3u;
    for(k=0;k<3u;k++) HALF_AT(c+8u+0x18u+k*4u)=ids[k];
    c+=8u+0x18u+20u*4u;
    for(k=0;k<4u;k++) {
        WORD_AT(c)=0x00C7766Cu;WORD_AT(c+4u)=0x44u;
        HALF_AT(c+8u+0x22u)=(uint16_t)k;BYTE_AT(c+8u+0x28u)=slot[k];
        c+=8u+0x44u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    rbs.read=0;crw.read=0;
}
static uint8_t outside[MAPMAKER_FX_SIZE];
static int window_is(const uint8_t *b) { return !memcmp((void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),b,MAPMAKER_FX_SIZE); }
/* everything back as it was outside the bosses' code */
static void all_back(int line)
{
    if(!window_is(outside) || mapmaker_fx_holder || crw_cur!=-1 || rbs_walkf.cur || WORD_AT(0x0075F5E8u)!=0x00ABC000u ||
       WORD_AT(0x0078B1E8u)!=WST(0)) { fprintf(stderr,"%s:%d: globals not put back\n",__FILE__,line);failures++; }
}

/* ---- what each boss's code sees -------------------------------------------- */
enum { CROWE_A, CROWE_B, WALKER_1, WALKER_2 };          /* actors 0-3 */
static struct { uint32_t state, walk, parts, count, eff; } seen[8];
static unsigned nseen;
static void look(void)
{
    if(nseen<8u) {
        seen[nseen].state=WORD_AT(0x0075F5E8u);seen[nseen].walk=WORD_AT(0x0078B1E8u);
        seen[nseen].parts=WORD_AT(0x0078B0D0u);seen[nseen].count=WORD_AT(0x0078B0CCu);seen[nseen].eff=WORD_AT(0x0078B090u);
    }
    nseen++;
}
static void fake_look(void) { originals++;look();g_esp+=4u; }
/* Crowe A hit again: counts one more particle */
static void fake_crowe_again(void) { originals++;look();WORD_AT(0x0078B0CCu)++;g_esp+=4u; }
/* the walker's damage callback: its own particles, then Crowe A's hit, Crowe
 * B's message, walker 2's slot and itself again */
static void fake_walk_damage(void)
{
    originals++;look();
    WORD_AT(0x0078B0CCu)=0x202u;WORD_AT(0x0078B1D8u)=0xD1D8u;        /* a walker-only static too */
    WORD_AT(PLAYER+0x300u)=ENT(CROWE_A);WORD_AT(PLAYER+0x304u)=ENT(WALKER_1);
    CALL(pc_mapmaker_crowe_hit,fake_crowe_again,0x000BA3E0u,PLAYER+0x300u);
    CHECK(WORD_AT(0x0078B0D0u)==WPT(0) && WORD_AT(0x0078B0CCu)==0x202u && WORD_AT(0x0075F5E8u)==CST(0));
    CALL(pc_mapmaker_crowe_message,fake_look,0x0027D3EEu,ENT(CROWE_B));
    CALL(pc_mapmaker_realbosses_walk_slot,fake_look,0x000A5D20u,ENT(WALKER_2));
    CALL(pc_mapmaker_realbosses_walk_slot,fake_look,0x000A5D20u,ENT(WALKER_1));
    CHECK(WORD_AT(0x0078B0D0u)==WPT(0) && WORD_AT(0x0078B0CCu)==0x202u && WORD_AT(0x0078B1E8u)==WST(0));
    g_esp+=4u;
}
/* Crowe A's think: a walker damage callback inside */
static void fake_crowe_think(void)
{
    originals++;look();
    WORD_AT(0x0078B0CCu)=0x101u;
    WORD_AT(PLAYER+0x400u)=ENT(WALKER_1);
    CALL(pc_mapmaker_realbosses_walk_damage,fake_walk_damage,0x000BB060u,PLAYER+0x400u);
    CHECK(WORD_AT(0x0075F5E8u)==CST(0) && WORD_AT(0x0078B0D0u)==CPT(0) && WORD_AT(0x0078B0CCu)==0x102u);   /* its hit inside */
    g_esp+=4u;
}
/* the reverse: walker 2's fight hits Crowe B, whose think hurts walker 2 again */
static void fake_walk2_hurt(void) { originals++;look();WORD_AT(0x0078B0CCu)=0x404u;g_esp+=4u; }
static void fake_crowe_b_think(void)
{
    originals++;look();
    WORD_AT(0x0078B0CCu)=0x303u;
    WORD_AT(PLAYER+0x500u)=ENT(WALKER_2);
    CALL(pc_mapmaker_realbosses_walk_damage,fake_walk2_hurt,0x000BB060u,PLAYER+0x500u);
    CHECK(WORD_AT(0x0078B0CCu)==0x303u && WORD_AT(0x0078B0D0u)==CPT(1) && WORD_AT(0x0078B1E8u)==WST(1));
    g_esp+=4u;
}
static void fake_walk2_fight(void)
{
    originals++;look();
    WORD_AT(0x0078B0CCu)=0x400u;
    CALL(pc_mapmaker_crowe_think,fake_crowe_b_think,0x000A7760u,ENT(CROWE_B));
    CHECK(WORD_AT(0x0078B0CCu)==0x404u && WORD_AT(0x0078B0D0u)==WPT(1) && WORD_AT(0x0078B1E8u)==WST(1));
    g_esp+=4u;
}
/* Crowe A's fight drops an egg (0x00291E7F): stock would spawn four null props */
static unsigned nspawn;
static void fake_egg_spawn(void) { originals++;nspawn++;g_esp+=4u; }
static void fake_crowe_fight(void)
{
    originals++;look();
    CALL(pc_mapmaker_crowe_eggs,fake_egg_spawn,0x00291E84u,CST(0)+0x11A0u);
    CALL(pc_mapmaker_crowe_eggs,fake_egg_spawn,0x00291E89u,CST(0)+0x11A0u);   /* not the drop: stock */
    WORD_AT(PLAYER+0x400u)=ENT(WALKER_1);
    CALL(pc_mapmaker_realbosses_walk_damage,fake_walk_damage,0x000BB060u,PLAYER+0x400u);
    CHECK(WORD_AT(0x0078B0D0u)==CPT(0) && WORD_AT(0x0075F5E8u)==CST(0));
    g_esp+=4u;
}

int main(void)
{
    unsigned k;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x00783A50u)=R;WORD_AT(0x007B0C7Cu)=PLAYER;
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);set_env("TSFP_MAPMAKER_LIMITS","1");
    container();
    for(k=0;k<MAPMAKER_FX_SIZE;k+=4u) WORD_AT(MAPMAKER_FX+k)=0xC0000000u+k;   /* the outside statics */
    WORD_AT(0x0075F5E8u)=0x00ABC000u;                                       /* a stale outside state pointer */

    /* level: two walkers counted and allocated, one Crowe now, one at its AI start */
    CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0);
    {
        uint32_t keep=WORD_AT(0x0075F5E8u);
        CALL(pc_mapmaker_crowe_level,fake,0x0015E81Du,0);
        CHECK(keep==0x00ABC000u && WORD_AT(0x0075F5E8u)==0);              /* the level clears a stale one */
        WORD_AT(0x0075F5E8u)=0x00ABC000u;
    }
    memcpy(outside,(void *)((uintptr_t)MAPMAKER_FX+g_xbox_mem_offset),sizeof outside);
    CHECK(rbs_walkf.n==2u && crw_n==1u && WORD_AT(0x0078B1E8u)==WST(0));
    actor(CROWE_A,0x46,0x5E8,0);actor(CROWE_B,0x46,0x5E8,1);actor(WALKER_1,0x64,0x637,2);actor(WALKER_2,0x64,0x637,3);
    for(k=0;k<4u;k++) {
        CALL(pc_mapmaker_realbosses_ai_start,fake,0x0015F981u,A(k));
        CALL(pc_mapmaker_crowe_ai_start,fake,0x0015F981u,A(k));
    }
    CHECK(crw_n==2u && crw_inst[1].state==CST(1) && FXW(crw_inst[1].fx,0x0078B0D0u)==CPT(1));
    CHECK(WORD_AT(REC(CROWE_A)+4u)==0x40000u && WORD_AT(REC(CROWE_B)+4u)==0x40000u);
    CHECK(WORD_AT(REC(WALKER_1)+4u)==0x100000u && WORD_AT(REC(WALKER_2)+4u)==0x100000u);
    CHECK(FXW(rbs_walks[1].fx,0x0078B0D0u)==WPT(1) && FXW(rbs_walks[0].fx,0x0078B090u)==WEF(0));
    all_back(__LINE__);

    /* a walker damage callback during a Crowe think, that Crowe's hit inside it,
     * another Crowe's message and both walkers' slots inside that */
    nseen=0;
    CALL(pc_mapmaker_crowe_think,fake_crowe_think,0x000A7760u,ENT(CROWE_A));
    CHECK(nseen==6u);
    CHECK(seen[0].state==CST(0) && seen[0].parts==CPT(0) && seen[0].eff==CEF(0) && seen[0].count==0xC000004Cu);   /* A */
    CHECK(seen[1].walk==WST(0) && seen[1].parts==WPT(0) && seen[1].eff==WEF(0) && seen[1].count==0xC000004Cu);   /* walker 1 */
    CHECK(seen[2].state==CST(0) && seen[2].parts==CPT(0) && seen[2].count==0x101u);                            /* A again: its latest */
    CHECK(seen[3].state==CST(1) && seen[3].parts==CPT(1) && seen[3].count==0xC000004Cu);                       /* Crowe B */
    CHECK(seen[4].walk==WST(1) && seen[4].parts==WPT(1) && seen[4].eff==WEF(1));                               /* walker 2 */
    CHECK(seen[5].walk==WST(0) && seen[5].parts==WPT(0) && seen[5].count==0x202u);                             /* walker 1 again */
    all_back(__LINE__);
    CHECK(FXW(crw_inst[0].fx,0x0078B0CCu)==0x102u && FXW(rbs_walks[0].fx,0x0078B0CCu)==0x202u);              /* each keeps its own */
    CHECK(FXW(rbs_walks[0].fx,0x0078B1D8u)==0xD1D8u && FXW(crw_inst[0].fx,0x0078B1D8u)==0xC0000158u);

    /* the reverse: a walker's fight, a Crowe's think inside, the same walker hurt inside that */
    nseen=0;
    CALL(pc_mapmaker_realbosses_walk_fight,fake_walk2_fight,0x0027DE5Fu,ENT(WALKER_2));
    CHECK(nseen==3u && seen[0].walk==WST(1) && seen[0].parts==WPT(1));
    CHECK(seen[1].state==CST(1) && seen[1].parts==CPT(1) && seen[1].walk==WST(1));
    CHECK(seen[2].walk==WST(1) && seen[2].parts==WPT(1) && seen[2].count==0x400u);                             /* walker 2's latest */
    all_back(__LINE__);
    CHECK(FXW(rbs_walks[1].fx,0x0078B0CCu)==0x404u && FXW(crw_inst[1].fx,0x0078B0CCu)==0x303u);

    /* the egg drop inside a Crowe's fight: skipped, esp balanced; the walker inside still its own */
    nspawn=0;nseen=0;
    CALL(pc_mapmaker_crowe_fight,fake_crowe_fight,0x0027DE3Eu,ENT(CROWE_A));
    CHECK(nspawn==1u && crw.eggs==1u);                                     /* only the other caller ran */
    CHECK(seen[0].state==CST(0) && seen[1].walk==WST(0) && seen[1].parts==WPT(0));
    all_back(__LINE__);
    CALL(pc_mapmaker_crowe_eggs,fake_egg_spawn,0x00291E84u,CST(0)+0x11A0u);   /* outside a Crowe's fight: stock */
    CHECK(nspawn==2u && crw.eggs==1u);

    /* the next level forgets both areas' bosses; the outside values stay live */
    container();HALF_AT(CONT+8u+8u+0x18u+4u)=0x0Cu;HALF_AT(CONT+8u+8u+0x18u+8u)=0x0Cu;   /* neither boss */
    CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0);
    CALL(pc_mapmaker_crowe_level,fake,0x0015E81Du,0);
    CHECK(!rbs_walkf.n && !crw_n && !mapmaker_fx_holder && window_is(outside));

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker bossnest ok");
    return 0;
}
