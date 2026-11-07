/* Asset-free contract for src/interface/pc_mapmaker_realbosses.inc: the
 * "Giant Trophy Monster" picker row (row 150 -> id 0x15) and its names in the
 * editor, the lv1_3.pak mount at a MapMaker level start, the class 0x80
 * override at AI start, the boss bar label and the single-bar rule with the
 * Boss flag. Fake guest memory; fake originals pop their return address like
 * the lifted callees, and every hook call checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00C00000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }

#define HEAP 0x00300000u
#define ITEM 0x00340000u
#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define ESP0 0x00600000u
#define R 10u
#define SKST 0x00700100u                /* fake skeleton state (0x4C7C) */
#define SKPT 0x00710000u                /* fake particle block (0x7D00) */
#define WKST 0x00720000u                /* fake Crow walker state (0x64DC) */
/* further instances of one level (the first keeps the addresses above) */
#define SKST_K(k) ((k)?0x00800000u+(uint32_t)(k)*0x10000u:SKST)
#define SKPT_K(k) ((k)?0x00880000u+(uint32_t)(k)*0x8000u:SKPT)
#define WKST_K(k) ((k)?0x00900000u+(uint32_t)(k)*0x8000u:WKST)
#define WKPT_K(k) (0x00940000u+(uint32_t)(k)*0x8000u)
#define WKEF_K(k) (0x00A00000u+(uint32_t)(k)*0x10000u)          /* effect blocks [0x0078B090] / [0x0078B098], 0x55F0 each */
#define WKEF0_K(k) (WKEF_K(k)+0x8000u)
static unsigned skel_seq(void), walk_seq(void);   /* instances made so far this level */
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)

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
void sub_00139740(void) { g_ecx=11; }
void sub_002E4B80(void) { g_ecx=12; }
void sub_002976F0(void) { g_ecx=13; }
void sub_0015DDA0(void) { g_ecx=14; }
void sub_0029CCB0(void) { g_ecx=15; }
void sub_00019D70(void) { g_ecx=16; }
void sub_002A7F30(void) { g_ecx=17; }
void sub_0005F460(void) { g_ecx=18; }
void sub_00287860(void) { g_ecx=19; }
void sub_00297690(void) { g_ecx=20; }
void sub_002BD480(void) { g_ecx=21; }
void sub_000C3C50(void) { g_ecx=22; }
/* The floor finder sub_000C3C50(point, up, radius): the highest floor at
 * most up over the point (room > 0, height in [0x004DCB00]); the stock also
 * writes [0x004DCAFC] and [0x00733088]. No floor map: none. */
static float skel_floors[2];            /* two floors at every x, z */
static int skel_floor_map;
static unsigned nfinder;
/* the stock skeleton / walker resets: the boss pointer and particle block (walker: effect block) each saw */
static unsigned nskel_reset, nwalk_reset, ngrid;
static uint32_t grid_args[8][4];
static uint32_t skel_reset_seen[8][2], walk_reset_seen[8][2];
static unsigned nanim;
static uint32_t fire_parts;
static uint32_t anim_args[4], anim_ok=1;
static unsigned nprincess;
static uint32_t princess_ent;
static uint32_t princess_state, princess_parts;
void sub_0029CEF0(void)                       /* pops like the lifted callee; emits a particle */
{
    nprincess++;princess_ent=WORD_AT(g_esp+4u);g_esp+=4u;
    princess_state=WORD_AT(0x0078B1ECu);princess_parts=WORD_AT(0x0078B0D0u);WORD_AT(0x0078B0CCu)++;
}
static unsigned nmansion_msg;
static uint32_t mansion_msg[2];
void sub_00286D30(void)                       /* mansion message handler: death = state 6, -1 */
{
    nmansion_msg++;mansion_msg[0]=WORD_AT(g_esp+4u);mansion_msg[1]=WORD_AT(g_esp+8u);
    g_eax=mansion_msg[1]==2u?0xFFFFFFFFu:0u;g_esp+=4u;
}
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }
static int bosses_area=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)hook;return strcmp(area,"bosses") || bosses_area; }

static unsigned nrows, nenable, nmount;
static struct { uint32_t text, id, flags, cb; char label[32]; } rows[16];
static uint32_t enable_item, enable_flag=0xEEu, mount_arg;
static int pak_mounted, skel_mounted, walk_mounted, mount_ok=1, alloc_ok=1, player_dead;
static unsigned nwalk_alloc, nremove, nfx;
static uint32_t remove_arg, fx_scene, fx_walk[8], fx_eff[8];
static unsigned nfire, nrender;
static uint32_t fire_args[2];
static unsigned nalloc, nstart, nshot, neffect, nbar;
static uint32_t start_args[2], shot_args[12], effect_args[2], bar_args[3], bar_esi, bar_edi, alloc_scene;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_000C3C50) {
        float p[3],y=-1.0e9f;
        int j;
        nfinder++;
        if(x[1]!=0x3FA00000u || x[2]!=0x3DCCCCCDu || x[0]<g_esp || x[0]+12u>ESP0) { fprintf(stderr,"floor finder arguments\n");exit(4); }
        memcpy(p,(const void *)((uintptr_t)x[0]+g_xbox_mem_offset),12);
        WORD_AT(0x004DCAFCu)=0xDEADu;WORD_AT(0x00733088u)=0xBEEFu;
        if(!skel_floor_map) return 0;
        for(j=0;j<2;j++) if(skel_floors[j]<=p[1]+1.25f && skel_floors[j]>y) y=skel_floors[j];
        if(y<-1.0e8f) return 0;
        memcpy((void *)((uintptr_t)0x004DCB00u+g_xbox_mem_offset),&y,4);
        return 3u;
    }
    if(f==sub_00079D60) {
        if(nrows<16) {
            rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].flags=x[2];rows[nrows].cb=x[4];
            memcpy(rows[nrows].label,(const char *)((uintptr_t)x[0]+g_xbox_mem_offset),31);rows[nrows].label[31]=0;
        }
        nrows++;return ITEM;
    }
    if(f==sub_00071630) return ITEM;
    if(f==sub_00071C70) { enable_flag=(nenable++?enable_flag:0u)|x[1];enable_item=x[0];return 0; }   /* any row disabled shows */
    if(f==sub_00062260) return ((x[0]==0x0049D77Cu && pak_mounted) || (x[0]==0x0049D6FCu && skel_mounted) ||
                                (x[0]==0x0049D9A8u && walk_mounted))?3u:0xFFFFFFFFu;
    if(f==sub_0005A130) {
        nmount++;mount_arg=x[0];
        if(mount_ok) { if(x[0]==0x0049D6FCu) skel_mounted=1; else if(x[0]==0x0049D9A8u) walk_mounted=1; else pak_mounted=1; }
        return mount_ok?1u:0u;
    }
    if(f==sub_00287E00) {   /* sub_0003DFA0 twice, then sub_00287860 */
        nalloc++;alloc_scene=WORD_AT(0x0079094Cu);
        if(alloc_ok) { unsigned q=skel_seq();WORD_AT(0x0078B1ECu)=SKST_K(q);WORD_AT(0x0078B0D0u)=SKPT_K(q);WORD_AT(SKST_K(q)+0x4160u)=0; }
        return 0;
    }
    if(f==sub_00059DB0) { nstart++;start_args[0]=x[0];start_args[1]=x[1];return x[1]==0x19Cu?0x77u:0x66u; }
    if(f==sub_000EA9C0) { nshot++;memcpy(shot_args,x,sizeof shot_args);return 0x1234u; }
    if(f==sub_0010EB90) { neffect++;effect_args[0]=x[0];effect_args[1]=x[1];return 0; }
    if(f==sub_00139740) {
        nbar++;memcpy(bar_args,x,sizeof bar_args);bar_esi=g_esi;bar_edi=g_edi;
        WORD_AT(x[2])=1u;return 0;
    }
    if(f==sub_002E4B80) return (uint32_t)player_dead;
    if(f==sub_002976F0) {   /* effect blocks, particles, state */
        unsigned q=walk_seq();
        nwalk_alloc++;if(alloc_ok) WORD_AT(0x0078B1E8u)=WKST_K(q);
        WORD_AT(0x0078B0D0u)=q?WKPT_K(q):SKPT;WORD_AT(0x0078B090u)=WKEF_K(q);
        WORD_AT(0x0078B098u)=WKEF0_K(q);WORD_AT(0x0078B08Cu)=0x1F4u;    /* two-player story count: 0x55F0 each */
        return 0;
    }
    if(f==sub_00287860) {
        if(nskel_reset<8u) { skel_reset_seen[nskel_reset][0]=WORD_AT(0x0078B1ECu);skel_reset_seen[nskel_reset][1]=WORD_AT(0x0078B0D0u); }
        nskel_reset++;WORD_AT(WORD_AT(0x0078B1ECu)+0x3E88u)=0;return 0;
    }
    if(f==sub_002BD480) {   /* the walker init's grid (8, 8, 1.0, state + 0x5C4) */
        if(ngrid<8u) memcpy(grid_args[ngrid],x,sizeof grid_args[0]);
        ngrid++;return 0;
    }
    if(f==sub_00297690) {   /* also emits into the particle block: the count moves */
        if(nwalk_reset<8u) { walk_reset_seen[nwalk_reset][0]=WORD_AT(0x0078B1E8u);walk_reset_seen[nwalk_reset][1]=WORD_AT(0x0078B090u); }
        nwalk_reset++;WORD_AT(0x0078B0CCu)+=0x100u;return 0;
    }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_0029CCB0) { nfx++;fx_scene=WORD_AT(0x0079094Cu);fx_walk[nfx&7u]=WORD_AT(0x0078B1E8u);fx_eff[nfx&7u]=WORD_AT(0x0078B090u);return 0; }
    if(f==sub_00019D70) { nrender++;return 0; }
    if(f==sub_002A7F30) { nfire++;fire_args[0]=x[0];fire_args[1]=x[1];fire_parts=WORD_AT(0x0078B0D0u);return 0; }
    if(f==sub_0005F460) {   /* play: the controller (entity +4) gets the animation */
        nanim++;memcpy(anim_args,x,sizeof anim_args);
        if(anim_ok) WORD_AT(WORD_AT(x[0]+4u)+0xD4u)=x[1];
        return anim_ok;
    }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_realbosses.inc"
/* the first giant skeleton / Crow walker (the single-boss cases below) */
#define rbs_skel (rbs_skels[0])
#define rbs_walk (rbs_walks[0])
static unsigned skel_seq(void) { return rbs_skelf.n; }
static unsigned walk_seq(void) { return rbs_walkf.n; }

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t orig_eax, seen_class[2], seen_focus;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
static void fake_ai(void) { originals++;seen_class[0]=WORD_AT(REC(0));seen_class[1]=WORD_AT(REC(0)+4u);g_esp+=4u; }
static void fake_ai_any(void)
{
    uint32_t rec=WORD_AT(WORD_AT(g_esp+4u)+0xBF0u);
    originals++;seen_class[0]=WORD_AT(rec);seen_class[1]=WORD_AT(rec+4u);g_esp+=4u;
}
static void fake_hud(void) { originals++;seen_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }
/* a boss's own code: records the boss pointers and particle/effect blocks it sees */
static uint32_t seen_skel, seen_walk, seen_parts, seen_eff;
static void fake_seen(void)
{
    originals++;seen_skel=WORD_AT(0x0078B1ECu);seen_walk=WORD_AT(0x0078B1E8u);
    seen_parts=WORD_AT(0x0078B0D0u);seen_eff=WORD_AT(0x0078B090u);g_esp+=4u;
}
/* a walker's first frame: looks up 'WELL' (sub_0023FF20 at 0x0029B32C) */
static uint32_t well_seen;
static void fake_fight_well(void)
{
    uint32_t sp=g_esp;
    originals++;
    g_esp-=8u;WORD_AT(g_esp)=0x0029B331u;WORD_AT(g_esp+4u)=0x57454C4Cu;orig_eax=0;
    pc_mapmaker_realbosses_tag(fake);
    well_seen=g_eax;g_esp=sp+4u;
}
/* a walker's death animation has ended: level event 2 (0x0029CB7B) */
static void fake_fight_done(void)
{
    uint32_t sp=g_esp;
    originals++;
    g_esp-=12u;WORD_AT(g_esp)=0x0029CB80u;WORD_AT(g_esp+4u)=2u;WORD_AT(g_esp+8u)=0;
    pc_mapmaker_realbosses_event(fake);
    g_esp=sp+4u;
}
static void fake_pick(void)
{   /* the stock callback stores data read past the Arcade table for row 150 */
    originals++;WORD_AT(0x0075FD90u+2u*0x1Cu)=0x12345u;WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)=150u;
    WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)=7u;g_esp+=4u;
}
static void fake_pick_skel(void)
{
    originals++;WORD_AT(0x0075FD90u+4u*0x1Cu)=0x54321u;WORD_AT(0x0075FD90u+4u*0x1Cu+0x14u)=151u;
    WORD_AT(0x0075FD90u+4u*0x1Cu+0x18u)=9u;g_esp+=4u;
}
static uint32_t seen_scene;
static void fake_scene(void) { originals++;seen_scene=WORD_AT(0x0079094Cu);g_esp+=4u; }
/* The anim slot table (sub_00057F30) and the restart's anim drop
 * (sub_001CDD60 -> sub_0005FC60 -> sub_0005C2F0): every loaded anim whose
 * slot word +4 has 0x4000 unmapped (pointer 0, id map 0xFFFF), the next slot
 * back to the level's mark. restart_take: a slot another registration fills
 * after the drop. */
#define AMAP 0x007E03C0u
#define APTRS 0x007E2AA4u
#define AWORDS 0x007DE660u
#define ANEXT 0x006D77F8u
#define AMARK 0x140u
#define ADATA(id) (0x00B00000u+(uint32_t)(id)*0x10u)
static uint32_t restart_take;
static void fake_restart(void)
{
    uint32_t id,slot;
    originals++;
    for(id=0;id<0xC03u;id++) {
        slot=HALF_AT(AMAP+id*2u);
        if(slot>=940u || !WORD_AT(APTRS+slot*4u) || !(WORD_AT(AWORDS+slot*8u+4u)&0x4000u)) continue;
        WORD_AT(APTRS+slot*4u)=0;HALF_AT(AMAP+id*2u)=0xFFFFu;
    }
    WORD_AT(ANEXT)=AMARK;
    if(restart_take) WORD_AT(APTRS+restart_take*4u)=0xCAFE0000u;
    g_esp+=4u;
}
static void anim_reg(uint32_t id,uint32_t slot,uint32_t flags)
{
    HALF_AT(AMAP+id*2u)=(uint16_t)slot;WORD_AT(APTRS+slot*4u)=ADATA(id);WORD_AT(AWORDS+slot*8u+4u)=flags;
}
static int anim_is(uint32_t id,uint32_t slot) { return HALF_AT(AMAP+id*2u)==slot && WORD_AT(APTRS+slot*4u)==ADATA(id); }
static int anim_gone(uint32_t id) { return HALF_AT(AMAP+id*2u)==0xFFFFu; }
static void anims_mansion(void)                 /* as the level load registered them (lv1_6, lv1_3, others) */
{
    uint32_t q;
    for(q=0;q<11u;q++) anim_reg(0x19Cu+q,0x100u+q,q==9u?0x5800u:0x1005800u);   /* princess_start .. raise_react_end */
    anim_reg(0x1A9u,0x10Bu,0x5800u);                                         /* princess_eyeoverwrite */
    anim_reg(0x16Au,0x10Cu,0x4000u);                                         /* jbeverybodyout: no boss's */
    for(q=0;q<8u;q++) anim_reg(0x127u+q,0x110u+q,0x5800u);                  /* the trophy monster's */
    anim_reg(0x80Eu,0x118u,0x1081800u);                                      /* splitcrowewalk: no 0x4000 */
}
static int anims_skel(void)
{
    uint32_t q;
    for(q=0;q<11u;q++) if(!anim_is(0x19Cu+q,0x100u+q)) return 0;
    return anim_is(0x1A9u,0x10Bu);
}
static int anims_trophy(void)
{
    uint32_t q;
    for(q=0;q<8u;q++) if(!anim_is(0x127u+q,0x110u+q)) return 0;
    return 1;
}
static int anims_trophy_gone(void)
{
    uint32_t q;
    for(q=0;q<8u;q++) if(!anim_gone(0x127u+q)) return 0;
    return 1;
}
/* the stock aim (0x00287F02..0x00287FC5): origin = the hand point, direction =
 * the unit vector to target point [state +0x4158] at state +0x4164, phase 1 */
static uint32_t aim_hand;
static void fake_aim(void)
{
    uint32_t st=WORD_AT(0x0078B1ECu),t=st+0x4164u+WORD_AT(st+0x4158u)*12u,s=st+0x413Cu;
    float d[3],l;
    int j;
    originals++;seen_scene=WORD_AT(0x0079094Cu);
    for(j=0;j<3;j++) { FLOAT_AT(s+j*4u)=FLOAT_AT(aim_hand+j*4u);d[j]=FLOAT_AT(t+j*4u)-FLOAT_AT(aim_hand+j*4u); }
    l=rbs_sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
    for(j=0;j<3;j++) FLOAT_AT(s+0xCu+j*4u)=d[j]/l;
    WORD_AT(st+0x4160u)=1u;g_esp+=4u;
}
static int near(float a,float b) { return a-b<0.01f && b-a<0.01f; }
static void fake_highlight(void) { originals++;WORD_AT(0x0078AEA8u)=WORD_AT(0x00700000u);g_esp+=4u; }
/* Calls hook fn with return address ret and up to three args; checks esp and
 * how often the original ran. */
static uint32_t call(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,int expect_orig,int line)
{
    int before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=a1;WORD_AT(ESP0+8u)=a2;WORD_AT(ESP0+12u)=a3;
    g_eax=0xDDDDu;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));failures++; }
    if(expect_orig>=0 && originals-before!=expect_orig) { fprintf(stderr,"%s:%d: original ran %d times\n",__FILE__,line,originals-before);failures++; }
    g_esp=ESP0;
    return g_eax;
}
#define CALL(fn,orig,ret,a1,a2,a3,n) call(fn,orig,ret,a1,a2,a3,n,__LINE__)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    rbs.read=0;
}
static void arcade_table(void)
{
    unsigned i;
    for(i=0;i<150u;i++) WORD_AT(0x00521A18u+i*0x30u+4u)=0x30u+i%64u;
    WORD_AT(0x00521A18u+43u*0x30u+4u)=0xA7u;                 /* trophy monster row */
    WORD_AT(0x00521A18u+144u*0x30u+4u)=0xA6u;                /* skeleton cowboy row */
    WORD_AT(0x00521A18u+90u*0x30u+4u)=0xA8u;                 /* giant robot row */
    WORD_AT(0x00521A18u+88u*0x30u+4u)=0xA9u;WORD_AT(0x00521A18u+89u*0x30u+4u)=0xAAu;   /* Prometheus SD/7, SK/8 */
    WORD_AT(0x00521A18u+14u*0x30u+4u)=0x1Eu;                 /* Old Crow row */
    for(i=0;i<43u;i++) if(WORD_AT(0x00521A18u+i*0x30u+4u)==0xA7u) WORD_AT(0x00521A18u+i*0x30u+4u)=0x20u;
}
/* map container: map-info chunk (custom set), then Story AI chunks */
static void container(uint32_t set_index,const uint16_t *ids,unsigned n,const uint8_t *boss,unsigned nsai)
{
    uint32_t c=CONT+8u,k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x00080000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    WORD_AT(c)=0x00C77668u;WORD_AT(c+4u)=0x18u+20u*4u;
    HALF_AT(c+8u+0x14u)=(uint16_t)set_index;BYTE_AT(c+8u+0x16u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(c+8u+0x18u+k*4u)=ids[k];
    c+=8u+0x18u+20u*4u;
    for(k=0;k<nsai;k++) {
        WORD_AT(c)=0x00C7766Cu;WORD_AT(c+4u)=0x44u;
        HALF_AT(c+8u+0x22u)=(uint16_t)k;BYTE_AT(c+8u+0x29u)=boss[k];
        BYTE_AT(c+8u+0x28u)=0xFFu;                           /* character slot: none (sai_slot sets one) */
        c+=8u+0x44u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
/* the k-th Story AI chunk's character slot (+0x28) */
static void sai_slot(unsigned k,uint32_t slot)
{
    uint32_t c=CONT+8u;
    while(WORD_AT(c)!=0x00C7766Au) {
        if(WORD_AT(c)==0x00C7766Cu && !k--) { BYTE_AT(c+8u+0x28u)=(uint8_t)slot;return; }
        c+=8u+WORD_AT(c+4u);
    }
}
#define FXW(fx,a) (*(uint32_t *)((uint8_t *)&(fx)+((a)-0x0078B080u)))   /* a static in an instance's saved copy */
static int fx_is(const uint8_t *base) { return !memcmp((void *)((uintptr_t)0x0078B080u+g_xbox_mem_offset),base,0x15Cu); }
/* the sets area's chunk for slots 11-20 (0x00C776F5), inserted before the terminator */
static void container_extra(const uint16_t *ids,unsigned n)
{
    uint32_t c=CONT+8u,k;
    while(WORD_AT(c)!=0x00C7766Au) c+=8u+WORD_AT(c+4u);
    WORD_AT(c)=0x00C776F5u;WORD_AT(c+4u)=0x30u;
    HALF_AT(c+8u)=1u;BYTE_AT(c+10u)=10u;BYTE_AT(c+11u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(c+12u+k*4u)=ids[k];
    c+=8u+0x30u;
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static void actor(unsigned i,uint32_t id,uint32_t model,uint32_t index)
{
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);HALF_AT(ENT(i)+0x94u)=(uint16_t)model;
    WORD_AT(REC(i))=0x11000000u;WORD_AT(REC(i)+4u)=0;WORD_AT(REC(i)+8u)=R+index+1u;
}

/* One level with a set: anims, level init, then a skeleton (actor 6) and a
 * trophy monster (actor 7) start. expect: 3 both real bosses, 2 skeleton
 * only, 1 trophy only, 0 neither. */
static void level_with(uint32_t index,const uint16_t *ids,unsigned n,const uint16_t *extra,unsigned ne,int expect,int line)
{
    static const uint8_t none[]={0,0};
    unsigned m=nmount,al=nalloc;
    int fails=failures;
    container(index,ids,n,none,2);
    if(extra) container_extra(extra,ne);
    pak_mounted=skel_mounted=0;WORD_AT(0x006D7880u)=10u;
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    call(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1,line);
    actor(6,0x28,0x5E9,0);actor(7,0x15,0x5BE,1);
    call(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(6),0,0,1,line);
    CHECK(seen_class[1]==((expect&2)?0x20000u:0u));
    call(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(7),0,0,1,line);
    CHECK(seen_class[1]==((expect&1)?0x80u:0u));
    CHECK(skel_mounted==!!(expect&2) && pak_mounted==!!(expect&1));
    CHECK(nmount-m==(unsigned)(!!(expect&2)+!!(expect&1)));
    CHECK(nalloc-al==(unsigned)!!(expect&2) && (rbs_skel.state==SKST)==!!(expect&2));
    CHECK(((expect&2)?rbs_skel.record==REC(6):!rbs_skel.record));
    m=nmansion_msg;                                       /* its death reaches the mansion handler */
    call(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(6),2u,0,(expect&2)?0:1,line);
    CHECK(nmansion_msg-m==(unsigned)!!(expect&2));
    if(failures!=fails) fprintf(stderr,"  (level_with from line %d)\n",line);
}
#define LEVEL_WITH(i,ids,n,ex,ne,e) level_with(i,ids,n,ex,ne,e,__LINE__)

int main(void)
{
    static const uint16_t with[]={0x0C,0x15,0xA8}, without[]={0x0C,0xA8};
    static const uint8_t boss_on[]={0,0xB1}, boss_off[]={0,0};
    unsigned k;
    g_xbox_mem_offset=(uintptr_t)memory;
    arcade_table();
    WORD_AT(0x00783A50u)=R;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    orig_eax=0xFFFFFFFFu;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x15u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,150u,0,0,1)==0);
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,1)==0x1234u);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    container(0x11,with,3,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    actor(0,0x15,0x5BE,1);pak_mounted=1;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* off: overhead meter drawn */
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- editor, on ----------------------------------------------------- */
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1F9Fu,0,0,0,1);   /* another list */
    CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=101u;                                          /* front-end bot sets */
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==16 && rows[0].id==150u && rows[0].flags==0x20900u && rows[0].cb==0x002C1C90u);
    CHECK(!strcmp(rows[0].label,"Giant Trophy Monster"));
    CHECK(rows[1].id==151u && rows[1].flags==0x20900u && rows[1].cb==0x002C1C90u && !strcmp(rows[1].label,"Giant Skeleton"));
    CHECK(rows[2].id==152u && rows[2].flags==0x20900u && rows[2].cb==0x002C1C90u && !strcmp(rows[2].label,"Crow Walker"));
    CHECK(rows[3].id==153u && !strcmp(rows[3].label,"Giant Robot") && rows[4].id==154u && !strcmp(rows[4].label,"Split Crowe"));
    CHECK(rows[5].id==155u && !strcmp(rows[5].label,"Castle Tank") && rows[5].flags==0x20900u && rows[5].cb==0x002C1C90u);
    CHECK(rows[6].id==156u && !strcmp(rows[6].label,"Khallos (Chair)") && rows[7].id==157u && !strcmp(rows[7].label,"Mansion Worm"));
    CHECK(rows[8].id==158u && !strcmp(rows[8].label,"Mothership"));
    CHECK(rows[9].id==159u && !strcmp(rows[9].label,"Prometheus SK/8") && rows[10].id==160u && !strcmp(rows[10].label,"Prometheus SD/7"));
    /* the story creatures (pc_mapmaker_creatures.def): rows 161..163 */
    CHECK(rows[11].id==161u && !strcmp(rows[11].label,"Catacomb Ghost") && rows[11].flags==0x20900u && rows[11].cb==0x002C1C90u);
    CHECK(rows[12].id==162u && !strcmp(rows[12].label,"SecuriDroid XP") && rows[13].id==163u && !strcmp(rows[13].label,"Mutant (Story)"));
    CHECK(rows[14].id==164u && !strcmp(rows[14].label,"Pit Skeleton") && rows[15].id==165u && !strcmp(rows[15].label,"Ragbug"));
    CHECK(nenable==16 && enable_item==ITEM && enable_flag==0);
    /* pick: row 150 -> id 0x15, variant -1; other slots untouched */
    for(k=0;k<10u;k++) { WORD_AT(0x0075FD90u+k*0x1Cu)=0x40u+k;WORD_AT(0x0075FD90u+k*0x1Cu+0x14u)=k;WORD_AT(0x0075FD90u+k*0x1Cu+0x18u)=0; }
    CALL(pc_mapmaker_realbosses_pick,fake_pick,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu)==0x15u && WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)==0xFFFFFFFFu);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)==150u);
    CHECK(WORD_AT(0x0075FD90u+3u*0x1Cu)==0x43u && WORD_AT(0x0075FD90u+3u*0x1Cu+0x18u)==0);
    CHECK(WORD_AT(0x0075FD90u+9u*0x1Cu)==0x49u);
    CALL(pc_mapmaker_realbosses_pick,fake_pick_skel,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0075FD90u+4u*0x1Cu)==0x28u && WORD_AT(0x0075FD90u+4u*0x1Cu+0x18u)==0xFFFFFFFFu);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu)==0x15u);
    /* highlight: the preview row never becomes 150 */
    WORD_AT(0x00700000u)=150u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==43u);
    WORD_AT(0x00700000u)=151u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==144u);
    WORD_AT(0x00700000u)=12u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==12u);
    /* row lookups */
    orig_eax=0xFFFFFFFFu;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x15u,0xFFFFFFFFu,0,0)==150u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x15u,0xFFFFFFFFu,0x003033C0u,0)==150u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x15u,0xFFFFFFFFu,0x000DAEB6u,1)==0xFFFFFFFFu);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x15u,0xFFFFFFFFu,0,0)==43u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1814u,0x15u,0xFFFFFFFFu,0,0)==43u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x15u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x28u,0xFFFFFFFFu,0,0)==151u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x28u,0xFFFFFFFFu,0x003033C0u,0)==151u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x28u,0xFFFFFFFFu,0,0)==144u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x28u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    orig_eax=7u;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0xA8u,0xFFFFFFFFu,0,1)==7u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x29u,0xFFFFFFFFu,0,1)==7u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x16u,0xFFFFFFFFu,0,1)==7u);
    /* names */
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,150u,0,0,0)==0x52420015u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,151u,0,0,0)==0x52420028u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,152u,0,0,0)==0x52420064u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,153u,0,0,0)==0x52420042u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,154u,0,0,0)==0x52420046u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,155u,0,0,0)==0x52420012u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,156u,0,0,0)==0x52420033u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,158u,0,0,0)==0x52420075u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,159u,0,0,0)==0x52420062u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,160u,0,0,0)==0x52420036u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,161u,0,0,0)==0x5242002Fu);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,162u,0,0,0)==0x5242004Du);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,163u,0,0,0)==0x52420030u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,164u,0,0,1)==0);
    {   /* the ported bosses' rows: names, pick, row lookups and previews */
        uint32_t s=CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x52420012u,0,0,0);
        CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Castle Tank"));
        orig_eax=0x1234u;
        CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x52420043u,0,0,1)==0x1234u);   /* not a row */
        WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=153u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=155u;
        CALL(pc_mapmaker_realbosses_pick,fake,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0075FD90u+7u*0x1Cu)==0x42u && WORD_AT(0x0075FD90u+8u*0x1Cu)==0x12u && WORD_AT(0x0075FD90u+8u*0x1Cu+0x18u)==0xFFFFFFFFu);
        WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=7u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=8u;
        orig_eax=0xFFFFFFFFu;
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x46u,0xFFFFFFFFu,0,0)==154u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x46u,0xFFFFFFFFu,0,0)==14u);   /* Old Crow */
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x12u,0xFFFFFFFFu,0,0)==90u);   /* giant robot */
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x42u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
        WORD_AT(0x00700000u)=153u;
        CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0078AEA8u)==90u);
        /* the Prometheus robots: rows 159/160 store 0x62/0x36, previews their Arcade twins 0xAA/0xA9 */
        WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=159u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=160u;
        CALL(pc_mapmaker_realbosses_pick,fake,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0075FD90u+7u*0x1Cu)==0x62u && WORD_AT(0x0075FD90u+8u*0x1Cu)==0x36u && WORD_AT(0x0075FD90u+7u*0x1Cu+0x18u)==0xFFFFFFFFu);
        WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=7u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=8u;
        orig_eax=0xFFFFFFFFu;
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x62u,0xFFFFFFFFu,0,0)==159u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x62u,0xFFFFFFFFu,0,0)==89u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x36u,0xFFFFFFFFu,0,0)==88u);
        s=CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x52420062u,0,0,0);
        CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Prometheus SK/8"));
        /* the creatures: rows 161..163 store 0x2F/0x4D/0x30 (variant -1), names, row lookups, previews */
        WORD_AT(0x0075FD90u+6u*0x1Cu+0x14u)=161u;WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=162u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=163u;
        CALL(pc_mapmaker_realbosses_pick,fake,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0075FD90u+6u*0x1Cu)==0x2Fu && WORD_AT(0x0075FD90u+7u*0x1Cu)==0x4Du && WORD_AT(0x0075FD90u+8u*0x1Cu)==0x30u);
        CHECK(WORD_AT(0x0075FD90u+6u*0x1Cu+0x18u)==0xFFFFFFFFu && WORD_AT(0x0075FD90u+8u*0x1Cu+0x18u)==0xFFFFFFFFu);
        WORD_AT(0x0075FD90u+6u*0x1Cu+0x14u)=6u;WORD_AT(0x0075FD90u+7u*0x1Cu+0x14u)=7u;WORD_AT(0x0075FD90u+8u*0x1Cu+0x14u)=8u;
        orig_eax=0xFFFFFFFFu;
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x2Fu,0xFFFFFFFFu,0,0)==161u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x4Du,0xFFFFFFFFu,0x003033C0u,0)==162u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x30u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);   /* play: stock */
        s=CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x5242004Du,0,0,0);
        CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"SecuriDroid XP"));
        /* the eight older extra rows' labels no longer spill over the log budget (extra_label[6] held 6) */
        CHECK(rbs.logged<24u);
        orig_eax=0;
        orig_eax=0;
    }
    orig_eax=0xE20u;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,149u,0,0,1)==0xE20u);
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,0xFFFFFFFFu,0,0,1)==0);
    orig_eax=0x1234u;
    k=CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0x52420015u,0,0,0);
    CHECK(k==rows[0].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0x52420028u,0,0,0)==rows[1].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0xE20u,0,0,1)==0x1234u);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,1)==0x1234u);   /* not in play */
    CALL(pc_mapmaker_realbosses_model_request,fake,0x000D77AFu,0x5BEu,0,0,1);           /* log only */
    /* editor scene: nothing of play runs */
    container(0x11,with,3,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);

    /* ---- play: anim pak mount -------------------------------------------- */
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=9u;                   /* not story */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    BYTE_AT(0x007DE455u)=10u;
    container(0x11,without,2,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(3,with,3,boss_off,2);                                      /* preset set: stale members */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(0x11,with,3,boss_off,2);
    WORD_AT(0x006D7880u)=57u;                                            /* table nearly full */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    WORD_AT(0x006D7880u)=56u;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1 && mount_arg==0x0049D77Cu && pak_mounted);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);         /* already mounted */
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x00784014u)=0;                                /* container unreadable */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==4 && skel_mounted && pak_mounted && walk_mounted);     /* all, skeleton first */
    skel_mounted=walk_mounted=0;
    container(0x11,with,3,boss_on,2);

    /* ---- play: class at AI start ----------------------------------------- */
    pak_mounted=0;
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* no real boss noted yet */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* no anims: bot */
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    pak_mounted=1;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    actor(0,0x0C,0x572,1);                                               /* another character */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CHECK(rbs.real[0].record==0);
    BYTE_AT(0x007DE455u)=9u;
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    BYTE_AT(0x007DE455u)=10u;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80u);                     /* before the original */
    CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==1);
    actor(0,0x00,0x5BE,0);                                               /* by model; Story AI 0: no Boss */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80u);
    CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==0);

    /* ---- play: bar label and a single bar ------------------------------- */
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,0)==rows[0].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CEDAu,0xDCCu,0,0,1)==0x1234u);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCDu,0,0,1)==0x1234u);
    WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* Boss Off: no bar at all */
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* Boss flag: bosses draws */
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    bosses_area=0;                                                        /* no bosses area: as Boss Off */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(rbs.real[0].boss==0);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0);
    bosses_area=1;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    HALF_AT(ENT(0)+0x94u)=0x572u;                                        /* focus is another model */
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(0));
    HALF_AT(ENT(0)+0x94u)=0x5BEu;
    actor(1,0x15,0x5BE,0);WORD_AT(PLAYER+0xFF4u)=ENT(1);                  /* another, unnoted record */
    WORD_AT(A(1)+0xBF0u)=REC(7);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(1));
    WORD_AT(PLAYER+0xFF4u)=ENT(0);

    /* ---- play: no overhead meter on the real boss ------------------------ */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* hidden */
    HALF_AT(ENT(0)+0x94u)=0x572u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* by character id */
    HALF_AT(A(0)+0x48u)=0x0C;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* reused actor, other character */
    HALF_AT(ENT(0)+0x94u)=0x5BEu;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* by model */
    HALF_AT(A(0)+0x48u)=0x15;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D269u,A(0),0,0x7Fu,1);  /* another caller */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(1),0,0x7Fu,1);  /* deerbeast without the class */
    actor(2,0x0C,0x572,0);WORD_AT(A(2)+0xBF0u)=REC(0);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(2),0,0x7Fu,1);  /* other MapMaker AI keeps it */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,0,0,0x7Fu,1);     /* bad pointer */
    BYTE_AT(0x007DE455u)=9u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* not story */
    BYTE_AT(0x007DE455u)=10u;
    WORD_AT(0x0079094Cu)=1u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* story level scene */
    WORD_AT(0x0079094Cu)=100u;

    /* ---- play: health display choice (chunk +0x29 bit 2 = Sub-boss bar) ---- */
    {
        static const uint8_t boss_sub[]={0,0xB5}, boss_bad[]={0,0xB9};
        container(0x11,with,3,boss_sub,2);
        actor(0,0x15,0x5BE,1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==2);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* Sub-boss: meter kept */
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* and no stock bottom bar */
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
        container(0x11,with,3,boss_on,2);                                     /* Boss bar: meter hidden */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==1);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
        container(0x11,with,3,boss_off,2);                                    /* Boss Off: no meter, no bar */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
        container(0x11,with,3,boss_bad,2);                                    /* unknown value: no Boss flag */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
    }
    /* ---- play: the giant skeleton (id 0x28, model 0x5E9) ------------------ */
    {
        static const uint16_t skel[]={0x0C,0x28}, both[]={0x15,0x28};
        static const uint8_t none3[]={0,0,0}, bar3[]={0,0,0xB1}, sub3[]={0,0,0xB5};
        static const uint32_t actions[8]={8,0,6,3,7,4,2,5};
        unsigned m;
        WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
        for(k=0;k<8u;k++) WORD_AT(0x005208B8u+k*4u)=actions[k];
        WORD_AT(0x005208B8u+8u*4u)=6u;                                       /* past the table */
        /* anims: lv1_6.pak for a custom set holding 0x28 */
        container(0x11,skel,2,none3,3);
        m=nmount;skel_mounted=0;pak_mounted=0;
        WORD_AT(0x006D7880u)=57u;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m && !skel_mounted);                                   /* table nearly full */
        WORD_AT(0x006D7880u)=10u;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u && mount_arg==0x0049D6FCu && skel_mounted && !pak_mounted);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u);                                                 /* already mounted */
        container(0x11,both,2,none3,3);skel_mounted=0;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+3u && skel_mounted && pak_mounted);                  /* both bosses: both paks */
        container(0x11,without,2,none3,3);skel_mounted=0;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(!skel_mounted);
        /* level: the stock state allocation */
        container(0x11,skel,2,none3,3);
        WORD_AT(0x0078B1ECu)=0x00BAD000u;WORD_AT(0x0078B0D0u)=0x00BAD100u;   /* an earlier level's */
        BYTE_AT(0x007DE455u)=9u;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0 && !rbs_skel.state);
        BYTE_AT(0x007DE455u)=10u;
        container(0x11,without,2,none3,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0 && !rbs_skel.state && WORD_AT(0x0078B1ECu)==0x00BAD000u);
        container(0x11,skel,2,none3,3);
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==1 && !rbs_skel.state && WORD_AT(0x0078B1ECu)==0);       /* failure seen */
        alloc_ok=1;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==2 && alloc_scene==100u && rbs_skel.state==SKST && rbs_skel.parts==SKPT);
        /* ai_start: the class, one per map */
        actor(3,0x28,0x5E9,2);actor(4,0x28,0x5E9,1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);    /* no lv1_6 */
        CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && !rbs_skel.record);
        skel_mounted=1;
        WORD_AT(0x0078B1ECu)=SKST+0x10u;                                     /* someone else's state */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_skel.record);
        WORD_AT(0x0078B1ECu)=SKST;
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_skel.record);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[0]==0 && seen_class[1]==0x20000u && rbs_skel.record==REC(3));   /* before the original */
        CHECK(rbs_skel.pad_set==1 && rbs_skel.pad==FLOAT_AT(ENT(3)+0x68u));             /* its placed height */
        alloc_ok=0;                                    /* a second one, no memory for its own state: a bot */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(4),0,0,1);         /* a second one */
        CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && rbs_skel.record==REC(3) && rbs_skelf.extras==1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);         /* same one again */
        CHECK(seen_class[1]==0x20000u && rbs_skel.record==REC(3));
        actor(5,0x0C,0x5E9,0);                                               /* by model */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(5),0,0,1);
        CHECK(seen_class[1]==0 && rbs_skelf.extras==2);
        alloc_ok=1;
        {   /* the pit skeleton carrier 0x31 is made with model 0x5E9: pc_mapmaker_pitboss.inc's, never claimed here */
            unsigned n0=rbs_skelf.n;
            actor(6,0x31,0x5E9,0);
            CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(6),0,0,1);
            CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && rbs_skelf.n==n0 && rbs_skelf.extras==2);
        }
        /* princess: the mansion variant */
        k=nprincess;
        FLOAT_AT(ENT(3)+0x68u)=-12.5f;                                       /* spawn height: a lower floor */
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(3),0,0,1);     /* no floor read yet */
        CHECK(FLOAT_AT(ENT(3)+0x68u)==-12.5f);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,0);
        CHECK(nprincess==k+1u && princess_ent==ENT(3));
        CHECK(rbs_skel.floor_set && rbs_skel.floor==-12.5f);
        /* the fight sets the height to 0 every frame; the facing step after it restores the floor */
        FLOAT_AT(ENT(3)+0x68u)=0.0f;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,0);
        CHECK(rbs_skel.floor==-12.5f);                                      /* read once */
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(3),0,0,1);
        CHECK(FLOAT_AT(ENT(3)+0x68u)==-12.5f);
        FLOAT_AT(ENT(3)+0x68u)=0.0f;
        CALL(pc_mapmaker_realbosses_face,fake,0x0029E528u,ENT(3),0,0,1);     /* the catacombs call */
        CHECK(FLOAT_AT(ENT(3)+0x68u)==0.0f);
        FLOAT_AT(ENT(4)+0x68u)=3.0f;
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(4),0,0,1);     /* not the boss */
        CHECK(FLOAT_AT(ENT(4)+0x68u)==3.0f);
        FLOAT_AT(ENT(3)+0x68u)=-12.5f;
        /* fire: the mansion particle draw, once per frame step, while +0x3E88 == 1 */
        WORD_AT(0x007B0CC0u)=0x00650000u;WORD_AT(0x00650000u+0x208u)=0x00654321u;
        WORD_AT(SKST+0x3E88u)=0;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfire==0);
        WORD_AT(SKST+0x3E88u)=1u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);         /* another caller */
        CHECK(nfire==0);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfire==1 && nrender==1 && fire_args[0]==0x00654321u && fire_args[1]==0x40200000u);
        WORD_AT(0x0079094Cu)=1u;                                             /* mission 5 draws it itself */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        WORD_AT(0x0079094Cu)=100u;WORD_AT(0x0078B1ECu)=SKST+0x10u;          /* not this level's state */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        WORD_AT(0x0078B1ECu)=SKST;
        CHECK(nfire==1);
        WORD_AT(SKST+0x3E88u)=0;
        k=nprincess;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(4),0,0,1);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,0,0,0,1);
        WORD_AT(0x0079094Cu)=1u;                                             /* mission 5 itself */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        WORD_AT(0x0079094Cu)=100u;
        WORD_AT(0x0078B1ECu)=SKST+0x10u;                                     /* state replaced: not ours */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        WORD_AT(0x0078B1ECu)=SKST;
        CHECK(nprincess==k);
        /* risen: the entrance animation 0x19C */
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,0)==0x77u);
        CHECK(nstart==1 && start_args[0]==ENT(3) && start_args[1]==0x19Cu);
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(4),0,0,1)==0);
        WORD_AT(0x0079094Cu)=17u;
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,1)==0);
        WORD_AT(0x0079094Cu)=100u;
        /* throw: aim with scene 1, fire in the real scene, then done */
        WORD_AT(A(3)+0x9DCu)=1u;                                             /* action type 0 */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(seen_scene==100u);
        WORD_AT(A(3)+0x9DCu)=2u;WORD_AT(SKST+0x4160u)=0;                     /* type 6: aim */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(seen_scene==1u && WORD_AT(0x0079094Cu)==100u);
        WORD_AT(SKST+0x4160u)=1u;WORD_AT(SKST+0x413Cu+0x18u)=0x00ABC000u;WORD_AT(ENT(3)+0x34u)=0x5151u;
        WORD_AT(SKST+0x413Cu+0x10u)=0x7FC00000u;                            /* a NaN aim: no throw */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);
        CHECK(nshot==0 && WORD_AT(SKST+0x4160u)==2u && rbs_skel.skipped==1);
        FLOAT_AT(SKST+0x413Cu+0x10u)=0.5f;FLOAT_AT(SKST+0x413Cu)=2.0e6f;   /* far off the map: no throw */
        WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);
        CHECK(nshot==0 && WORD_AT(SKST+0x4160u)==2u);
        FLOAT_AT(SKST+0x413Cu)=4.0f;WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);   /* fire */
        CHECK(nshot==1 && WORD_AT(SKST+0x4160u)==2u);
        CHECK(shot_args[0]==0x34u && shot_args[1]==SKST+0x413Cu && shot_args[2]==SKST+0x4148u && shot_args[3]==0x3E99999Au);
        CHECK(shot_args[4]==ENT(3) && shot_args[5]==A(3) && shot_args[6]==2u && shot_args[7]==1u && shot_args[8]==0x5151u);
        CHECK(shot_args[9]==0xEu && shot_args[10]==0xCu && shot_args[11]==0x3DCCCCCDu);
        CHECK(neffect==1 && effect_args[0]==0x00ABC000u && effect_args[1]==0);
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);   /* done */
        CHECK(nshot==1 && WORD_AT(SKST+0x4160u)==2u);
        WORD_AT(A(3)+0x9DCu)=8u;                                             /* past the table */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        WORD_AT(A(4)+0x9DCu)=2u;WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(4),0x00600F00u,0,1);   /* not the boss */
        CHECK(nshot==1 && seen_scene==100u);
        /* aim at a target a floor up (v0.4.0-mapmaker.40): the boss at -12.5,
         * its hand at -8.5 (below the floor above), the player's feet ~5 up at
         * -7.5, 10 ahead: aimed at its body, not into the floor's edge */
        {
            uint32_t st=SKST,t=SKST+0x4164u+12u;
            float hx=1.0f,hy=-8.5f,hz=2.0f,dy;
            rbs_skel.pad_set=0;                                              /* the first-frame fallback */
            aim_hand=0x00601000u;FLOAT_AT(aim_hand)=hx;FLOAT_AT(aim_hand+4u)=hy;FLOAT_AT(aim_hand+8u)=hz;
            WORD_AT(st+0x4158u)=1u;FLOAT_AT(t)=hx;FLOAT_AT(t+4u)=-7.5f;FLOAT_AT(t+8u)=hz+10.0f;
            WORD_AT(A(3)+0x9DCu)=2u;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            dy=FLOAT_AT(st+0x413Cu+0x10u);
            CHECK(seen_scene==1u && WORD_AT(st+0x4160u)==1u && rbs_skel.aimed==1u);
            CHECK(near(dy,2.5f/rbs_sqrt(2.5f*2.5f+100.0f)) && near(FLOAT_AT(st+0x413Cu+0x14u),10.0f/rbs_sqrt(2.5f*2.5f+100.0f)));
            CHECK(near(FLOAT_AT(st+0x413Cu+0xCu),0.0f) && FLOAT_AT(st+0x413Cu+4u)==hy);   /* the hand stays the origin */
            /* steeply overhead: the climb held at about 60 degrees, a unit vector */
            FLOAT_AT(t)=hx+1.0f;FLOAT_AT(t+4u)=-2.0f;FLOAT_AT(t+8u)=hz;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),0.866f) && near(FLOAT_AT(st+0x413Cu+0xCu),0.5f) && near(FLOAT_AT(st+0x413Cu+0x14u),0.0f));
            /* the boss's own floor: the stock aim, untouched */
            FLOAT_AT(t)=hx;FLOAT_AT(t+4u)=-12.0f;FLOAT_AT(t+8u)=hz+10.0f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),-3.5f/rbs_sqrt(3.5f*3.5f+100.0f)) && rbs_skel.aimed==2u);
            /* still aiming (phase 0 kept): nothing changed */
            WORD_AT(st+0x4160u)=0;FLOAT_AT(t+4u)=-7.5f;
            CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.aimed==2u && near(FLOAT_AT(st+0x413Cu+0x10u),-3.5f/rbs_sqrt(3.5f*3.5f+100.0f)));
            CHECK(rbs_skel.ground_set==-1 && rbs_skel.ground==-12.5f);   /* no floor map: the spawn height */
            WORD_AT(st+0x4158u)=0;WORD_AT(st+0x4160u)=2u;WORD_AT(A(3)+0x9DCu)=8u;
        }
        /* v0.4.0-mapmaker.42 ("they don't hit me if I stand in place on the
         * same floor"): its first-frame height -9.49 under the floor it fights
         * on (0.5; another floor at -20): the reference is the floor above,
         * same-floor targets keep the stock aim */
        {
            uint32_t st=SKST,t=SKST+0x4164u+12u;
            float hx=1.0f,hy=1.2f,hz=2.0f,dx,dy,dz,y;
            unsigned n;
            aim_hand=0x00601000u;FLOAT_AT(aim_hand)=hx;FLOAT_AT(aim_hand+4u)=hy;FLOAT_AT(aim_hand+8u)=hz;
            FLOAT_AT(ENT(3)+0x64u)=hx;FLOAT_AT(ENT(3)+0x6Cu)=hz;
            rbs_skel.floor=-9.49f;rbs_skel.ground_set=0;rbs_skel.aimed=0;rbs_skel.pad_set=0;
            skel_floor_map=1;skel_floors[0]=0.5f;skel_floors[1]=-20.0f;nfinder=0;
            WORD_AT(0x004DCB00u)=0x1111u;WORD_AT(0x004DCAFCu)=0x2222u;WORD_AT(0x00733088u)=0x3333u;
            WORD_AT(st+0x4158u)=1u;FLOAT_AT(t)=hx;FLOAT_AT(t+4u)=0.5f;FLOAT_AT(t+8u)=hz+10.0f;
            WORD_AT(A(3)+0x9DCu)=2u;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.ground_set==2 && near(rbs_skel.ground,0.5f) && nfinder>=2u);
            CHECK(WORD_AT(0x004DCB00u)==0x1111u && WORD_AT(0x004DCAFCu)==0x2222u && WORD_AT(0x00733088u)==0x3333u);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),-0.7f/rbs_sqrt(0.49f+100.0f)) && rbs_skel.aimed==0);   /* stock */
            /* a little over that floor (a step, a jump): still stock; the floor is found once */
            FLOAT_AT(t+4u)=2.7f;WORD_AT(st+0x4160u)=0;n=nfinder;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),1.5f/rbs_sqrt(2.25f+100.0f)) && rbs_skel.aimed==0 && nfinder==n);
            /* the floor above (5.5), close (3 ahead) from a low hand: raised; the
             * line is over its feet and under its head where it stands */
            FLOAT_AT(t+4u)=5.5f;FLOAT_AT(t+8u)=hz+3.0f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            dx=FLOAT_AT(st+0x413Cu+0xCu);dy=FLOAT_AT(st+0x413Cu+0x10u);dz=FLOAT_AT(st+0x413Cu+0x14u);
            y=hy+dy/rbs_sqrt(dx*dx+dz*dz)*3.0f;
            CHECK(rbs_skel.aimed==1 && y>=5.5f && y<=7.5f);
            /* ... and from a hand high over its floor (11.2): from under the
             * target's ceiling (5.5 + 4), straight at its body */
            hy=11.2f;FLOAT_AT(aim_hand+4u)=hy;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            dx=FLOAT_AT(st+0x413Cu+0xCu);dy=FLOAT_AT(st+0x413Cu+0x10u);dz=FLOAT_AT(st+0x413Cu+0x14u);
            y=FLOAT_AT(st+0x413Cu+4u)+dy/rbs_sqrt(dx*dx+dz*dz)*3.0f;
            CHECK(rbs_skel.aimed==2 && near(y,7.0f) && near(FLOAT_AT(st+0x413Cu+4u),9.5f));
            /* its spawn on its own floor (-9.49 over -9.5) under an upper floor
             * (0.5): its own floor counts; the floor above is a floor up */
            rbs_skel.ground_set=0;skel_floors[1]=-9.5f;hy=-8.0f;FLOAT_AT(aim_hand+4u)=hy;
            FLOAT_AT(t+4u)=-9.5f;FLOAT_AT(t+8u)=hz+10.0f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.ground_set==1 && near(rbs_skel.ground,-9.5f) && rbs_skel.aimed==2);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),-1.5f/rbs_sqrt(2.25f+100.0f)));
            FLOAT_AT(t+4u)=0.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.aimed==3);
            /* m42.err: placed on the 0.5 floor (AI start 0.51), its first
             * fight frame 5 lower (-4.49) over a floor at -4.5: the placed
             * height counts, the player on 0.5 gets the stock aim */
            rbs_skel.floor=-4.49f;rbs_skel.pad=0.51f;rbs_skel.pad_set=1;rbs_skel.ground_set=0;
            skel_floors[0]=0.5f;skel_floors[1]=-4.5f;hy=1.2f;FLOAT_AT(aim_hand+4u)=hy;
            FLOAT_AT(t+4u)=0.5f;FLOAT_AT(t+8u)=hz+10.0f;WORD_AT(st+0x4160u)=0;n=rbs_skel.aimed;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.ground_set==1 && near(rbs_skel.ground,0.5f) && rbs_skel.aimed==n);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),-0.7f/rbs_sqrt(0.49f+100.0f)));
            /* v0.4.0-mapmaker.44 (m43b/m44.err: placed and fighting at -4.49
             * over a -4.5 floor under a 0.5 one, the hand 6.2, 10.7 over its
             * feet; every aim at a player on 0.5, "floor one"): the shot starts
             * under that player's ceiling (0.5 + 4), the raised aim from there */
            rbs_skel.floor=-4.49f;rbs_skel.pad=-4.49f;rbs_skel.pad_set=1;rbs_skel.ground_set=0;rbs_skel.lowered=0;
            hy=6.2f;FLOAT_AT(aim_hand+4u)=hy;
            FLOAT_AT(t+4u)=0.5f;FLOAT_AT(t+8u)=hz+10.0f;WORD_AT(st+0x4160u)=0;n=rbs_skel.aimed;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.ground_set==1 && near(rbs_skel.ground,-4.5f) && rbs_skel.aimed==n+1u && rbs_skel.lowered==1u);
            CHECK(near(FLOAT_AT(st+0x413Cu+4u),4.5f) && FLOAT_AT(st+0x413Cu)==hx && FLOAT_AT(st+0x413Cu+8u)==hz);
            CHECK(near(FLOAT_AT(st+0x413Cu+0xCu),0.0f) && near(FLOAT_AT(st+0x413Cu+0x10u),-2.5f/rbs_sqrt(106.25f)) &&
                  near(FLOAT_AT(st+0x413Cu+0x14u),10.0f/rbs_sqrt(106.25f)));
            /* ... the player on its own floor (-4.5): the stock aim at the
             * feet from under the floor above (-0.5) */
            FLOAT_AT(t+4u)=-4.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.lowered==2u && rbs_skel.aimed==n+1u && near(FLOAT_AT(st+0x413Cu+4u),-0.5f));
            CHECK(near(FLOAT_AT(st+0x413Cu+0xCu),0.0f) && near(FLOAT_AT(st+0x413Cu+0x10u),-4.0f/rbs_sqrt(116.0f)) &&
                  near(FLOAT_AT(st+0x413Cu+0x14u),10.0f/rbs_sqrt(116.0f)));
            /* ... a jump on it (-3.0): 4 over the target */
            FLOAT_AT(t+4u)=-3.0f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.lowered==3u && near(FLOAT_AT(st+0x413Cu+4u),1.0f) && near(FLOAT_AT(st+0x413Cu+0x10u),-4.0f/rbs_sqrt(116.0f)));
            /* ... the player two floors up (5.5): the hand is in that room, raised aim from it */
            FLOAT_AT(t+4u)=5.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.aimed==n+2u && rbs_skel.lowered==3u && FLOAT_AT(st+0x413Cu+4u)==hy);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),0.8f/rbs_sqrt(0.64f+100.0f)));
            /* ... a hand low on its floor (-2.4): the stock aim, untouched */
            hy=-2.4f;FLOAT_AT(aim_hand+4u)=hy;FLOAT_AT(t+4u)=-4.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.lowered==3u && rbs_skel.aimed==n+2u && FLOAT_AT(st+0x413Cu+4u)==hy);
            CHECK(near(FLOAT_AT(st+0x413Cu+0x10u),-2.1f/rbs_sqrt(2.1f*2.1f+100.0f)));
            /* ... a target a floor below it (-9.5): the stock aim from the hand */
            hy=6.2f;FLOAT_AT(aim_hand+4u)=hy;FLOAT_AT(t+4u)=-9.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.lowered==3u && FLOAT_AT(st+0x413Cu+4u)==hy && near(FLOAT_AT(st+0x413Cu+0x10u),-15.7f/rbs_sqrt(15.7f*15.7f+100.0f)));
            /* ... the fire phase throws from the lowered origin */
            FLOAT_AT(t+4u)=0.5f;WORD_AT(st+0x4160u)=0;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,1);
            CHECK(rbs_skel.lowered==4u && WORD_AT(st+0x4160u)==1u && near(FLOAT_AT(st+0x413Cu+4u),4.5f));
            n=nshot;
            CALL(pc_mapmaker_realbosses_throw,fake_aim,0x0029D055u,ENT(3),0x00600F00u,0,0);
            CHECK(nshot==n+1u && shot_args[1]==st+0x413Cu && near(FLOAT_AT(shot_args[1]+4u),4.5f) && WORD_AT(st+0x4160u)==2u);
            nshot=n;
            rbs_skel.floor=-12.5f;rbs_skel.ground_set=0;rbs_skel.pad_set=0;skel_floor_map=0;
            WORD_AT(st+0x4158u)=0;WORD_AT(st+0x4160u)=2u;WORD_AT(A(3)+0x9DCu)=8u;
        }
        /* event: its death marker is dropped */
        CALL(pc_mapmaker_realbosses_event,fake,0x0029E068u,3u,0,0,0);
        CHECK(rbs_skelf.events==1);
        CALL(pc_mapmaker_realbosses_event,fake,0x00227000u,3u,0,0,1);        /* map logic */
        WORD_AT(0x0079094Cu)=1u;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029E068u,3u,0,0,1);        /* mission 5 */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(rbs_skelf.events==1);
        /* message: the death (2) reaches the mansion handler, not the catacombs one */
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0x00600F00u,0)==0xFFFFFFFFu);
        CHECK(nmansion_msg==1 && mansion_msg[0]==ENT(3) && mansion_msg[1]==2u && rbs_skelf.deaths==1);
        CHECK(CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),5u,0,0)==0);   /* removal: as scene 1 */
        CHECK(nmansion_msg==2 && mansion_msg[1]==5u);
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(4),2u,0,1);             /* not the boss */
        WORD_AT(0x0079094Cu)=1u;
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* mission 5: stock */
        WORD_AT(0x0079094Cu)=17u;
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* catacombs: stock */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(nmansion_msg==2);
        /* bar: Boss Off = no health shown; the stock call never sees the focus */
        WORD_AT(PLAYER+0xFF4u)=ENT(3);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3) && nbar==0);
        container(0x11,skel,2,bar3,3);                                        /* Boss flag: bosses draws */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(rbs.real[1].record==REC(3) && rbs.real[1].boss==1);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && nbar==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3));
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,0);  /* meter hidden */
        container(0x11,skel,2,sub3,3);                                        /* Sub-boss: the meter */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && nbar==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,1);
        container(0x11,skel,2,none3,3);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,0);  /* no flag: hidden */
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(4),0,0x7Fu,1);  /* the extra keeps it */
        /* the next level forgets this one */
        container(0x11,without,2,none3,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(!rbs_skel.record && !rbs_skel.state);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,1);
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(nshot==1);
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* forgotten boss */
        CHECK(nmansion_msg==2);
        WORD_AT(PLAYER+0xFF4u)=ENT(0);
    }
    /* ---- play: both real bosses in one set, any slot 1-20, any order ------ */
    {   /* stock map-info form: count 10, empty slots -1 */
#define E 0xFFFFu
        static const uint16_t skel_trophy[10]={0x28,0x15,E,E,E,E,E,E,E,E}, trophy_skel[10]={0x15,0x28,E,E,E,E,E,E,E,E};
        static const uint16_t plain[10]={0x0C,0xA8,E,E,E,E,E,E,E,E}, last2[10]={0x0C,E,E,E,E,E,E,E,0x15,0x28};
        static const uint16_t trophy0[10]={0x15,E,E,E,E,E,E,E,E,E}, skel0[10]={0x28,E,E,E,E,E,E,E,E,E};
        static const uint16_t x_skel_trophy[10]={0x28,0x15,E,E,E,E,E,E,E,E}, x_trophy_skel[10]={E,E,E,0x15,E,E,E,E,E,0x28};
        static const uint16_t x_skel[10]={0x28,E,E,E,E,E,E,E,E,E}, x_trophy[2]={E,0x15}, x_plain[10]={0xA8,E,E,E,E,E,E,E,E,E};
        LEVEL_WITH(0x11,skel_trophy,10,NULL,0,3);          /* the user's set: skeleton slot 1, trophy slot 2 */
        LEVEL_WITH(0x11,trophy_skel,10,NULL,0,3);          /* reversed */
        LEVEL_WITH(0x11,last2,10,NULL,0,3);                /* slots 9 and 10 */
        LEVEL_WITH(0x11,plain,10,x_skel_trophy,10,3);      /* slots 11 and 12 */
        LEVEL_WITH(0x11,plain,10,x_trophy_skel,10,3);      /* slots 14 and 20, reversed */
        LEVEL_WITH(0x11,trophy0,10,x_skel,10,3);           /* trophy slot 1, skeleton slot 11 */
        LEVEL_WITH(0x11,skel0,10,x_trophy,2,3);            /* skeleton slot 1, trophy slot 12 (short chunk) */
        LEVEL_WITH(0x11,plain,10,x_skel,10,2);             /* skeleton only, slot 11 */
        LEVEL_WITH(0x11,plain,10,x_trophy,2,1);            /* trophy only, slot 12 */
        LEVEL_WITH(0x11,plain,10,x_plain,10,0);            /* neither */
        LEVEL_WITH(0x0C,skel_trophy,10,x_skel_trophy,10,0); /* a profile set index: the members are not played */
        LEVEL_WITH(0x11,plain,10,x_skel_trophy,0,0);       /* chunk with count 0: not the sets area's */
        container(0x11,plain,10,boss_off,2);container_extra(x_skel,10);
        {   /* a wrong chunk header is ignored as by sets_take */
            uint32_t c=CONT+8u;
            while(WORD_AT(c)!=0x00C776F5u) c+=8u+WORD_AT(c+4u);
            BYTE_AT(c+10u)=9u;
            CHECK(rbs_map_has(0x28)==0);
            BYTE_AT(c+10u)=10u;HALF_AT(c+8u)=2u;
            CHECK(rbs_map_has(0x28)==0);
            HALF_AT(c+8u)=1u;
            CHECK(rbs_map_has(0x28)==1 && rbs_map_has(0x15)==0);
        }
#undef E
    }
    /* ---- the Crow walker (id 0x64) ---------------------------------------- */
    {
        static const uint16_t walk_set[]={0x0C,0x64}, no_walk[]={0x0C,0xA8};
        static const uint8_t walk_bar[]={0,0xB1,0}, walk_none[]={0,0,0};
        uint32_t m,well,w2;
        unsigned f0;
        /* editor: pick, preview, row lookups, names */
        WORD_AT(0x0079094Cu)=102u;
        WORD_AT(0x0075FD90u+6u*0x1Cu)=0x99u;WORD_AT(0x0075FD90u+6u*0x1Cu+0x14u)=152u;WORD_AT(0x0075FD90u+6u*0x1Cu+0x18u)=3u;
        CALL(pc_mapmaker_realbosses_pick,fake,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0075FD90u+6u*0x1Cu)==0x64u && WORD_AT(0x0075FD90u+6u*0x1Cu+0x18u)==0xFFFFFFFFu);
        CHECK(WORD_AT(0x0075FD90u+4u*0x1Cu)==0x28u);
        WORD_AT(0x00700000u)=152u;
        CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0078AEA8u)==90u);
        orig_eax=0xFFFFFFFFu;
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x64u,0xFFFFFFFFu,0,0)==152u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x64u,0xFFFFFFFFu,0x003033C0u,0)==152u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x64u,0xFFFFFFFFu,0,0)==90u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x64u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
        {
            uint32_t s=CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x52420064u,0,0,0);
            CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Crow Walker"));
        }
        /* play: anims, level state, class */
        WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
        container(0x11,no_walk,2,walk_none,3);
        pak_mounted=skel_mounted=walk_mounted=0;WORD_AT(0x006D7880u)=10u;
        m=nmount;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nmount==m && !walk_mounted && nwalk_alloc==0 && !rbs_walk.state);    /* not in the set */
        container(0x11,walk_set,2,walk_bar,3);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u && walk_mounted && mount_arg==0x0049D9A8u && !pak_mounted && !skel_mounted);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u);                                                      /* already mounted */
        WORD_AT(0x0078B1E8u)=0x00123450u;                                        /* stale pointer */
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nwalk_alloc==1 && !rbs_walk.state);
        alloc_ok=1;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nwalk_alloc==2 && rbs_walk.state==WKST && !rbs_skel.state);
        actor(8,0x64,0x637,1);actor(9,0x64,0x637,2);
        walk_mounted=0;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);                             /* no anims: bot */
        walk_mounted=1;
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        WORD_AT(0x0078B1E8u)=0;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);                             /* state gone */
        WORD_AT(0x0078B1E8u)=WKST;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[0]==0 && seen_class[1]==0x100000u && rbs_walk.record==REC(8) && rbs_walk.actor==A(8));
        for(k=0;k<16u && rbs.real[k].record!=REC(8);k++) {}
        CHECK(k<16u && rbs.real[k].boss==1u);                                    /* Boss flag noted */
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(9),0,0,1);
        alloc_ok=1;
        CHECK(seen_class[1]==0 && rbs_walk.record==REC(8));                      /* no memory for a second */
        /* WELL: a stand-in only for the walker's lookup, only when none is found */
        orig_eax=0;
        well=CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1);
        CHECK(well && well!=0xDDDDu);
        WORD_AT(well+0x28u)=0x40u;
        w2=CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1);
        CHECK(w2==well && WORD_AT(well+0x28u)==0);                               /* reused, zeroed */
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x00290000u,0x57454C4Cu,0,0,1)==0);   /* other caller */
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x50524150u,0,0,1)==0);   /* other tag */
        orig_eax=0x00654320u;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0x00654320u);   /* a real one */
        orig_eax=0;WORD_AT(0x0079094Cu)=81u;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0);   /* mission 13 */
        WORD_AT(0x0079094Cu)=100u;
        /* effects: after the frame step at its one call site, in scene 100 */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1 && fx_scene==100u);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);
        CHECK(nfx==1);
        WORD_AT(0x0078B1E8u)=0x00123450u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);                                                            /* not this level's */
        WORD_AT(0x0078B1E8u)=WKST;WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);                                                            /* stock draws it there */
        WORD_AT(0x0079094Cu)=100u;
        /* wake-up: the intro state 1 waits for a level trigger no MapMaker map has */
        f0=nfx;
        WORD_AT(A(8)+0x20u)=1u;WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;BYTE_AT(WKST+0x98u)=0;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* first frame not done */
        BYTE_AT(WKST+0x98u)=1u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* other caller */
        WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* mission 13 has SPC1/2 */
        WORD_AT(0x0079094Cu)=100u;
        WORD_AT(A(8)+0x9D0u)=3u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==3u);                                          /* a request pending */
        WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;WORD_AT(A(8)+0xBF0u)=REC(9);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* slot reused */
        WORD_AT(A(8)+0xBF0u)=REC(8);WORD_AT(ENT(8)+0x7Cu)=A(9);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* entity moved on */
        WORD_AT(ENT(8)+0x7Cu)=A(8);
        WORD_AT(A(8)+0x20u)=4u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* already fighting */
        WORD_AT(A(8)+0x20u)=1u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==2u && WORD_AT(A(8)+0x20u)==1u && rbs_walk.woken==1u);   /* roar, then fight */
        CHECK(nfx==f0+6u);                                                        /* beams still drawn */
        nfx=f0;
        /* the roar: state 2 waits for an animation to finish; a creature starts in its bind pose */
#define WCTL 0x00730000u
        WORD_AT(ENT(8)+4u)=WCTL;WORD_AT(WCTL+0xD4u)=0x6EEu;                    /* its own (the first): the stock path */
        WORD_AT(A(8)+0x20u)=2u;WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(WCTL+0xD4u)=0;WORD_AT(WCTL+0x12Cu)=0x6F1u;                       /* blending into its own */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(WCTL+0x12Cu)=0;WORD_AT(WCTL+0xD4u)=0x6FCu;                       /* the last of its own */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(WCTL+0xD4u)=0xFEu;                                               /* the bind pose (v0.4.0-mapmaker.25) */
        WORD_AT(WCTL+0x12Cu)=0;WORD_AT(A(8)+0x20u)=4u;                           /* already fighting */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(A(8)+0x20u)=2u;BYTE_AT(WKST+0x98u)=0;                            /* first frame not done */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        BYTE_AT(WKST+0x98u)=1u;WORD_AT(0x0079094Cu)=81u;                         /* mission 13 */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(0x0079094Cu)=100u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);               /* other caller */
        CHECK(nanim==0);
        WORD_AT(A(8)+0xBF0u)=REC(9);                                             /* slot reused */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==0);
        WORD_AT(A(8)+0xBF0u)=REC(8);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==1 && anim_args[0]==ENT(8) && anim_args[1]==0x6F7u && anim_args[2]==0 && anim_args[3]==0x3F800000u);
        CHECK(WORD_AT(WCTL+0xD4u)==0x6F7u && WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu && rbs_walk.roars==1u);
        WORD_AT(WCTL+0xD4u)=0;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==1);                                                         /* once a level */
        rbs_walk.roars=0;WORD_AT(WCTL+0xD4u)=0x6EDu;                             /* just below its own */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==2);
        rbs_walk.roars=0;WORD_AT(WCTL+0xD4u)=0x6FDu;                             /* just above */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==3);
        rbs_walk.roars=0;WORD_AT(WCTL+0xD4u)=0;WORD_AT(WCTL+0x12Cu)=0x6EDu;     /* blend: not its own */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==4);
        WORD_AT(WCTL+0x12Cu)=0;WORD_AT(WCTL+0xD4u)=0;
        rbs_walk.roars=0;WORD_AT(A(8)+0x20u)=1u;                                 /* state 1: wake and roar together */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==5 && WORD_AT(A(8)+0x9D0u)==2u && anim_args[1]==0x6F7u);
        rbs_walk.roars=0;WORD_AT(WCTL+0xD4u)=0;WORD_AT(A(8)+0x20u)=2u;WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;anim_ok=0;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);                /* refused: logged, not retried */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nanim==6 && rbs_walk.roars==1u);
        anim_ok=1;WORD_AT(ENT(8)+4u)=0;
        /* the guns: standing in state 0x13 in stage 1 with both guns off -> state 0x12 once */
        WORD_AT(A(8)+0x20u)=0x13u;WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;BYTE_AT(WKST+0x98u)=1u;
        WORD_AT(WKST+0x4CCu)=1u;WORD_AT(WKST+0x52Cu)=0;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu && rbs_walk.last==0x13u);        /* gun 1 active */
        WORD_AT(WKST+0x4CCu)=0;WORD_AT(WKST+0x52Cu)=1u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* gun 2 active */
        WORD_AT(WKST+0x52Cu)=0;BYTE_AT(WKST+0x98u)=2u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* stage 2: stock */
        BYTE_AT(WKST+0x98u)=1u;WORD_AT(A(8)+0x9D0u)=5u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==5u);                                          /* a request pending */
        WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;WORD_AT(A(8)+0x20u)=4u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu && rbs_walk.last==4u);           /* not standing */
        WORD_AT(A(8)+0x20u)=0x13u;WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* mission 13 */
        WORD_AT(0x0079094Cu)=100u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* other caller */
        WORD_AT(A(8)+0xBF0u)=REC(9);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu);                                 /* slot reused */
        WORD_AT(A(8)+0xBF0u)=REC(8);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0x12u && rbs_walk.armed==1u && rbs_walk.last==0x13u);
        WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(WORD_AT(A(8)+0x9D0u)==0xFFFFFFFFu && nanim==6u);                    /* once a level, no roar */
        CHECK(rbs_walk.moves>=3u);
        WORD_AT(WKST+0x4CCu)=0;WORD_AT(WKST+0x52Cu)=0;nfx=f0;
        WORD_AT(A(8)+0x20u)=0;WORD_AT(A(8)+0x9D0u)=0xFFFFFFFFu;
        /* HUD and meter as the other real bosses */
        WORD_AT(PLAYER+0xFF4u)=ENT(8);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(8));
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(8),0,0x7Fu,0);
        /* level events: dropped; at the end of the death the walker is removed */
        orig_eax=0;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029AEBEu,1u,0,0,0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029B120u,1u,0,0,0);
        CHECK(nremove==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x00290000u,1u,0,0,1);           /* any other caller */
        WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,1);           /* mission 13: stock */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(nremove==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1 && remove_arg==ENT(8) && WORD_AT(PLAYER+0xFF4u)==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1);                                                        /* once */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);   /* same record again */
        CHECK(rbs_walk.actor==A(8));
        WORD_AT(A(8)+0xBF0u)=REC(9);                                              /* the slot now holds another */
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1);
        WORD_AT(A(8)+0xBF0u)=REC(8);
        /* the next level forgets this one */
        container(0x11,no_walk,2,walk_none,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(!rbs_walk.record && !rbs_walk.state);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,1);
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);
        /* slots 11-20 */
        {
            static const uint16_t x_walk[2]={0xFFFFu,0x64};
            container(0x11,no_walk,2,walk_none,3);container_extra(x_walk,2);
            CHECK(rbs_map_has(0x64)==1 && rbs_map_has(0x28)==0 && rbs_map_has(0x15)==0);
            container(0x11,walk_set,2,walk_none,3);
            CHECK(rbs_map_has(0x64)==1 && rbs_map_has(0x28)==0);
        }
        WORD_AT(PLAYER+0xFF4u)=ENT(0);
    }
    /* ---- several giant skeletons and Crow walkers in one map ------------------ */
    {
        static const uint16_t both[]={0x0C,0x28,0x64};
        static const uint8_t none7[]={0,0,0,0,0,0,0};
        uint8_t base[0x15C];
        unsigned q,r;
        RbsSave s1,s2,s3;
        WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
        skel_mounted=walk_mounted=1;anim_ok=1;
        container(0x11,both,3,none7,7);
        sai_slot(0,1u);sai_slot(1,1u);sai_slot(2,1u);sai_slot(3,2u);sai_slot(4,2u);sai_slot(5,0u);sai_slot(6,25u);
        for(q=0;q<0x15Cu;q+=4u) WORD_AT(0x0078B080u+q)=0xF0000000u+q;   /* another boss's statics */
        memcpy(base,(void *)((uintptr_t)0x0078B080u+g_xbox_mem_offset),sizeof base);
        WORD_AT(0x0078B1ECu)=0x123u;WORD_AT(0x0078B1E8u)=0x456u;
        r=nalloc;q=nwalk_alloc;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(rbs_skelf.n==3u && rbs_walkf.n==2u && nalloc-r==3u && nwalk_alloc-q==2u);     /* one per placed AI */
        CHECK(rbs_skels[0].state==SKST && rbs_skels[1].state==SKST_K(1) && rbs_skels[2].state==SKST_K(2));
        CHECK(rbs_skels[1].parts==SKPT_K(1) && FXW(rbs_skels[1].fx,0x0078B0D0u)==SKPT_K(1));
        CHECK(rbs_walks[0].state==WKST && rbs_walks[1].state==WKST_K(1));
        CHECK(FXW(rbs_walks[1].fx,0x0078B090u)==WKEF_K(1) && FXW(rbs_walks[1].fx,0x0078B0D0u)==WKPT_K(1));
        CHECK(FXW(rbs_skels[2].fx,0x0078B0E0u)==0xF0000060u);                 /* each starts from the statics */
        CHECK(FXW(rbs_skels[0].fx,0x0078B090u)==0xF0000010u);                 /* not a walker's effect block */
        CHECK(fx_is(base));                                                   /* the statics put back */
        CHECK(WORD_AT(0x0078B1ECu)==SKST && WORD_AT(0x0078B1E8u)==WKST);       /* home: the first of each */
        /* AI start: one instance each, the fourth skeleton and third walker stay bots */
        for(q=10;q<17u;q++) actor(q,q<14u?0x28u:0x64u,q<14u?0x5E9u:0x637u,q);
        alloc_ok=0;                                    /* no level memory for a late instance */
        for(q=10;q<17u;q++) CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(q),0,0,1);
        alloc_ok=1;
        CHECK(rbs_skelf.n==3u && rbs_walkf.n==2u && fx_is(base));
        CHECK(rbs_skels[0].record==REC(10) && rbs_skels[1].record==REC(11) && rbs_skels[2].record==REC(12));
        CHECK(WORD_AT(REC(12)+4u)==0x20000u && WORD_AT(REC(13)+4u)==0 && rbs_skelf.extras>=1u);
        CHECK(rbs_walks[0].record==REC(14) && rbs_walks[1].record==REC(15) && rbs_walks[1].actor==A(15));
        CHECK(WORD_AT(REC(15)+4u)==0x100000u && WORD_AT(REC(16)+4u)==0);
        r=rbs_skelf.extras;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(11),0,0,1);   /* the same AI again */
        CHECK(rbs_skels[1].record==REC(11) && rbs_skelf.extras==r && WORD_AT(REC(11)+4u)==0x20000u);
        /* each skeleton's fight runs on its own state and particles; everything put back */
        q=nprincess;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(12),0,0,0);
        CHECK(nprincess==q+1u && princess_state==SKST_K(2) && princess_parts==SKPT_K(2));
        CHECK(fx_is(base) && WORD_AT(0x0078B1ECu)==SKST);
        CHECK(FXW(rbs_skels[2].fx,0x0078B0CCu)==0xF000004Du && FXW(rbs_skels[0].fx,0x0078B0CCu)==0xF000004Cu);   /* its own count */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(10),0,0,0);
        CHECK(princess_state==SKST && princess_parts==SKPT && FXW(rbs_skels[0].fx,0x0078B0CCu)==0xF000004Du);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(12),0,0,0);
        CHECK(FXW(rbs_skels[2].fx,0x0078B0CCu)==0xF000004Eu);                 /* kept between calls */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(13),0,0,1);   /* the bot one: stock */
        CALL(pc_mapmaker_realbosses_skel_slot,fake_seen,0x000A5D20u,ENT(11),0,0,1);
        CHECK(seen_skel==SKST_K(1) && seen_parts==SKPT_K(1) && WORD_AT(0x0078B1ECu)==SKST && fx_is(base));
        CALL(pc_mapmaker_realbosses_skel_slot,fake_seen,0x000A5D20u,ENT(13),0,0,1);
        CHECK(seen_skel==SKST && seen_parts==0xF0000050u);                    /* not one of ours: untouched */
        WORD_AT(0x0079094Cu)=1u;
        CALL(pc_mapmaker_realbosses_skel_slot,fake_seen,0x000A5D20u,ENT(11),0,0,1);
        CHECK(seen_skel==SKST);                                               /* mission 5: stock */
        WORD_AT(0x0079094Cu)=100u;
        /* nested swaps: the same instance swaps nothing, another one is put back */
        rbs_skel_in(&rbs_skels[1],&s1);
        CHECK(WORD_AT(0x0078B1ECu)==SKST_K(1) && rbs_skelf.cur==&rbs_skels[1] && s1.swapped);
        rbs_skel_in(&rbs_skels[1],&s2);
        CHECK(!s2.swapped);
        rbs_skel_in(&rbs_skels[2],&s3);
        CHECK(WORD_AT(0x0078B1ECu)==SKST_K(2) && WORD_AT(0x0078B0D0u)==SKPT_K(2));
        rbs_skel_out(&rbs_skels[2],&s3);
        CHECK(WORD_AT(0x0078B1ECu)==SKST_K(1) && WORD_AT(0x0078B0D0u)==SKPT_K(1) && rbs_skelf.cur==&rbs_skels[1]);
        rbs_skel_out(&rbs_skels[1],&s2);
        CHECK(WORD_AT(0x0078B1ECu)==SKST_K(1));
        rbs_skel_out(&rbs_skels[1],&s1);
        CHECK(WORD_AT(0x0078B1ECu)==SKST && fx_is(base) && !rbs_skelf.cur);
        /* the same instance again inside another area's boss (a split Crowe's
         * statics live) inside its own code: its own latest values, all back */
        {
            static MapmakerFx crowe;
            MapmakerFx *p;
            uint32_t count=FXW(rbs_skels[1].fx,0x0078B0CCu);
            memset(&crowe,0,sizeof crowe);FXW(crowe,0x0078B0D0u)=0x00799000u;
            rbs_skel_in(&rbs_skels[1],&s1);
            WORD_AT(0x0078B0CCu)=0x51u;
            p=mapmaker_fx_enter(&crowe);
            CHECK(WORD_AT(0x0078B0D0u)==0x00799000u && WORD_AT(0x0078B1ECu)==SKST_K(1) && p==&rbs_skels[1].fx);
            rbs_skel_in(&rbs_skels[1],&s2);
            CHECK(!s2.swapped && WORD_AT(0x0078B0D0u)==SKPT_K(1) && WORD_AT(0x0078B0CCu)==0x51u);
            WORD_AT(0x0078B0CCu)=0x52u;
            rbs_skel_out(&rbs_skels[1],&s2);
            CHECK(WORD_AT(0x0078B0D0u)==0x00799000u && rbs_skelf.cur==&rbs_skels[1] && WORD_AT(0x0078B1ECu)==SKST_K(1));
            WORD_AT(0x0078B0CCu)=0x77u;                                   /* the Crowe's own */
            mapmaker_fx_leave(p);
            CHECK(WORD_AT(0x0078B0CCu)==0x52u && WORD_AT(0x0078B0D0u)==SKPT_K(1) && FXW(crowe,0x0078B0CCu)==0x77u);
            rbs_skel_out(&rbs_skels[1],&s1);
            CHECK(fx_is(base) && !rbs_skelf.cur && !mapmaker_fx_holder && FXW(rbs_skels[1].fx,0x0078B0CCu)==0x52u);
            FXW(rbs_skels[1].fx,0x0078B0CCu)=count;
        }
        /* fire: each burning skeleton draws its own particles */
        WORD_AT(0x007B0CC0u)=PLAYER;WORD_AT(SKST_K(1)+0x3E88u)=1u;WORD_AT(SKST+0x3E88u)=0;WORD_AT(SKST_K(2)+0x3E88u)=0;
        q=nfire;r=nfx;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfire==q+1u && fire_parts==SKPT_K(1) && fx_is(base));
        /* walkers: beams with their own state and effect blocks */
        CHECK(nfx==r+2u && fx_walk[(r+1u)&7u]==WKST && fx_walk[(r+2u)&7u]==WKST_K(1) && fx_eff[(r+2u)&7u]==WKEF_K(1));
        CHECK(WORD_AT(0x0078B1E8u)==WKST && fx_is(base));
        WORD_AT(SKST_K(1)+0x3E88u)=0;
        CALL(pc_mapmaker_realbosses_walk_fight,fake_seen,0x0027DE5Fu,ENT(15),0,0,1);
        CHECK(seen_walk==WKST_K(1) && seen_eff==WKEF_K(1) && WORD_AT(0x0078B1E8u)==WKST && fx_is(base));
        CALL(pc_mapmaker_realbosses_walk_message,fake_seen,0x0027D42Bu,ENT(15),2u,0,1);
        CHECK(seen_walk==WKST_K(1));
        WORD_AT(0x00570000u)=ENT(15);
        CALL(pc_mapmaker_realbosses_walk_damage,fake_seen,0x0009E800u,0x00570000u,0,0,1);
        CHECK(seen_walk==WKST_K(1) && WORD_AT(0x0078B1E8u)==WKST);
        CALL(pc_mapmaker_realbosses_walk_slot,fake_seen,0x000A5D20u,ENT(14),0,0,1);
        CHECK(seen_walk==WKST && seen_eff==WKEF_K(0));
        CALL(pc_mapmaker_realbosses_walk_fight,fake_seen,0x0027DE5Fu,ENT(16),0,0,1);
        CHECK(seen_walk==WKST && seen_eff==0xF0000010u);                     /* the bot walker: untouched */
        WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_walk_fight,fake_seen,0x0027DE5Fu,ENT(15),0,0,1);
        CHECK(seen_walk==WKST);                                               /* mission 13: stock */
        WORD_AT(0x0079094Cu)=100u;
        /* one WELL stand-in per walker, looked up inside its own fight */
        CALL(pc_mapmaker_realbosses_walk_fight,fake_fight_well,0x0027DE5Fu,ENT(14),0,0,2);
        q=well_seen;
        CALL(pc_mapmaker_realbosses_walk_fight,fake_fight_well,0x0027DE5Fu,ENT(15),0,0,2);
        CHECK(q && well_seen && well_seen!=q && rbs_walks[1].wells==1u);
        CALL(pc_mapmaker_realbosses_walk_fight,fake_fight_well,0x0027DE5Fu,ENT(14),0,0,2);
        CHECK(well_seen==q);                                                  /* reused */
        /* the end of a walker's death removes that walker */
        q=nremove;
        CALL(pc_mapmaker_realbosses_walk_fight,fake_fight_done,0x0027DE5Fu,ENT(15),0,0,1);
        CHECK(nremove==q+1u && remove_arg==ENT(15) && !rbs_walks[1].actor && rbs_walks[0].actor==A(14));
        /* the boss pointer replaced by something else: no instance is ours any more */
        WORD_AT(0x0078B1ECu)=0x999u;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(10),0,0,1);
        WORD_AT(0x0078B1ECu)=SKST;
        /* at most four of each */
        container(0x11,both,3,none7,7);
        for(q=0;q<7u;q++) sai_slot(q,1u);
        r=nalloc;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(rbs_skelf.n==4u && nalloc-r==4u && rbs_walkf.n==1u && !rbs_skels[3].record);
        /* the level init counted fewer than were placed (.26: three each, one
         * counted): each extra gets its own state at its AI start, from the level's
         * statics, the boss pointers back on the first */
        container(0x11,both,3,none7,7);
        sai_slot(0,1u);sai_slot(1,2u);for(q=2;q<7u;q++) sai_slot(q,25u);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(rbs_skelf.n==1u && rbs_walkf.n==1u);
        for(q=0;q<0x15Cu;q+=4u) WORD_AT(0x0078B080u+q)=0xE0000000u+q;  /* other statics live now */
        r=nalloc;q=nwalk_alloc;
        {
            unsigned i;
            for(i=10;i<15u;i++) actor(i,i<13u?0x28u:0x64u,i<13u?0x5E9u:0x637u,i);
            for(i=10;i<15u;i++) CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(i),0,0,1);
        }
        CHECK(rbs_skelf.n==3u && nalloc-r==2u && rbs_walkf.n==2u && nwalk_alloc-q==1u);
        CHECK(rbs_skels[1].record==REC(11) && rbs_skels[2].record==REC(12) && rbs_walks[1].record==REC(14));
        CHECK(WORD_AT(REC(12)+4u)==0x20000u && WORD_AT(REC(14)+4u)==0x100000u);
        {   /* no fifth: the cap holds for late instances too */
            unsigned n0=rbs_skelf.n,w0=rbs_walkf.n;
            rbs_skelf.n=RBS_INST_MAX;rbs_walkf.n=RBS_INST_MAX;r=nalloc;
            CHECK(!rbs_skel_more() && !rbs_walk_more() && rbs_skelf.n==RBS_INST_MAX && rbs_walkf.n==RBS_INST_MAX && nalloc==r);
            rbs_skelf.n=n0;rbs_walkf.n=w0;
        }
        CHECK(FXW(rbs_skels[2].fx,0x0078B0E0u)==0xF0000060u);                 /* from the level's statics */
        CHECK(FXW(rbs_walks[1].fx,0x0078B0E0u)==0xF0000060u);                 /* the late walker too */
        CHECK(WORD_AT(0x0078B0E0u)==0xE0000060u);                             /* the live ones put back */
        CHECK(WORD_AT(0x0078B1ECu)==rbs_skels[0].state && WORD_AT(0x0078B1E8u)==rbs_walks[0].state);
        for(q=0;q<0x15Cu;q+=4u) WORD_AT(0x0078B080u+q)=0xF0000000u+q;
        {   /* the pause menu's restart (sub_001CDD60, no level init) frees only
             * heap 0 (sub_0003E740(3) -> sub_0003E240): four skeletons, the
             * fourth a late one, and two walkers keep their Level heap blocks
             * (heap 1) and set them up again as at level init: zeroed (the
             * last run poisoned them), from the level's statics, the stock
             * init's rest, no allocation; their records kept, the same
             * records find them again */
            uint8_t live[0x15C];
            unsigned a0,b0,s0,w0,g0;
            actor(15,0x28u,0x5E9u,15);
            CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(15),0,0,1);   /* the fourth, late */
            CHECK(rbs_skelf.n==4u && rbs_skels[3].record==REC(15) && rbs_skels[3].state==SKST_K(3));
            CHECK(FXW(rbs_walks[1].fx,0x0078B098u)==WKEF0_K(1) && FXW(rbs_walks[1].fx,0x0078B08Cu)==0x1F4u);
            a0=nalloc;b0=nwalk_alloc;s0=nskel_reset;w0=nwalk_reset;g0=ngrid;
            WORD_AT(0x0078B0E0u)=0xE0000060u;                            /* the outside statics differ from the level's */
            memcpy(live,(void *)((uintptr_t)0x0078B080u+g_xbox_mem_offset),sizeof live);
            rbs_walks[0].armed=1u;rbs_walks[1].roars=1u;rbs_walks[1].woken=1u;rbs_skels[2].floor_set=1;rbs_skels[1].throws=5u;
            rbs_skels[3].floor_set=1;rbs_skels[3].aimed=3u;rbs_skels[3].ground_set=2;rbs_walks[1].actor=A(14);
            for(q=0;q<4u;q++) {                                           /* the last run in every block */
                WORD_AT(SKST_K(q)+0x100u)=0xDEAD0000u+q;WORD_AT(SKST_K(q)+0x4C78u)=0xDEAD0010u+q;
                WORD_AT(SKPT_K(q)+0x40u)=0xBEEF0000u+q;WORD_AT(SKPT_K(q)+0x7CFCu)=0xBEEF0010u+q;
            }
            WORD_AT(WKST_K(1)+0x200u)=0xFEED0001u;WORD_AT(WKST_K(1)+0x64D8u)=0xFEED0002u;WORD_AT(WKPT_K(1)+0x7CFCu)=0xFEED0003u;
            WORD_AT(WKEF_K(1)+0x55ECu)=0xFEED0004u;WORD_AT(WKEF0_K(1)+0x10u)=0xFEED0005u;WORD_AT(WKEF_K(1)+0x55F0u)=0xC0FFEEu;   /* past its block: kept */
            FXW(rbs_skels[1].fx,0x0078B0CCu)=0x99u;                      /* last run's particle count */
            (void)mapmaker_fx_enter(&rbs_skels[1].fx);                    /* a call that never came back */
            rbs.logged=99u;
            /* the level load's anim slots: the restart unmaps every 0x4000 one (all
             * of lv1_6's and lv1_3's; user report .43, "glitching out after
             * restarting": no princess_start, the entrance never ended) */
            memset((void *)((uintptr_t)AMAP+g_xbox_mem_offset),0xFF,0xC03u*2u);
            WORD_AT(0x007E3954u)=APTRS;WORD_AT(ANEXT)=0x150u;
            anims_mansion();pak_mounted=1;                                 /* lv1_3 mounted, the trophy monster not in this set */
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);
            CHECK(anims_skel() && WORD_AT(ANEXT)==AMARK);                 /* the skeleton's again: same slots, same data */
            CHECK(anim_gone(0x16Au) && anims_trophy_gone() && anim_is(0x80Eu,0x118u));   /* the rest stock */
            CHECK(nalloc==a0 && nwalk_alloc==b0 && rbs_skelf.n==4u && rbs_walkf.n==2u);   /* no Level heap used */
            CHECK(nskel_reset==s0+4u && nwalk_reset==w0+2u && ngrid==g0+2u);
            for(q=0;q<4u;q++) {
                CHECK(rbs_skels[q].state==SKST_K(q) && rbs_skels[q].parts==SKPT_K(q) && FXW(rbs_skels[q].fx,0x0078B0D0u)==SKPT_K(q));
                CHECK(skel_reset_seen[(s0+q)&7u][0]==SKST_K(q) && skel_reset_seen[(s0+q)&7u][1]==SKPT_K(q));
                CHECK(!WORD_AT(SKST_K(q)+0x100u) && !WORD_AT(SKST_K(q)+0x4C78u) && !WORD_AT(SKPT_K(q)+0x40u) && !WORD_AT(SKPT_K(q)+0x7CFCu));
            }
            CHECK(rbs_walks[0].state==WKST_K(0) && rbs_walks[1].state==WKST_K(1) && FXW(rbs_walks[1].fx,0x0078B090u)==WKEF_K(1));
            CHECK(FXW(rbs_walks[1].fx,0x0078B098u)==WKEF0_K(1) && FXW(rbs_walks[1].fx,0x0078B08Cu)==0x1F4u && FXW(rbs_walks[1].fx,0x0078B0D0u)==WKPT_K(1));
            CHECK(walk_reset_seen[(w0+1u)&7u][0]==WKST_K(1) && walk_reset_seen[(w0+1u)&7u][1]==WKEF_K(1));
            CHECK(grid_args[(g0+1u)&7u][0]==8u && grid_args[(g0+1u)&7u][1]==8u && grid_args[(g0+1u)&7u][2]==0x3F800000u && grid_args[(g0+1u)&7u][3]==WKST_K(1)+0x5C4u);
            CHECK(!WORD_AT(WKST_K(1)+0x200u) && !WORD_AT(WKST_K(1)+0x64D8u) && !WORD_AT(WKPT_K(1)+0x7CFCu));
            CHECK(!WORD_AT(WKEF_K(1)+0x55ECu) && !WORD_AT(WKEF0_K(1)+0x10u) && WORD_AT(WKEF_K(1)+0x55F0u)==0xC0FFEEu);
            CHECK(rbs_skels[0].record==REC(10) && rbs_skels[2].record==REC(12) && rbs_skels[3].record==REC(15));
            CHECK(rbs_walks[0].record==REC(13) && rbs_walks[1].record==REC(14));
            CHECK(FXW(rbs_skels[1].fx,0x0078B0CCu)==0xF000004Cu && FXW(rbs_skels[3].fx,0x0078B0E0u)==0xF0000060u && FXW(rbs_walks[1].fx,0x0078B0E0u)==0xF0000060u);   /* the level's statics */
            CHECK(fx_is(live) && WORD_AT(0x0078B1ECu)==SKST_K(0) && WORD_AT(0x0078B1E8u)==WKST_K(0));
            CHECK(!rbs_skelf.cur && !rbs_walkf.cur && !mapmaker_fx_holder);
            CHECK(!rbs_walks[0].armed && !rbs_walks[1].roars && !rbs_walks[1].woken && !rbs_skels[2].floor_set && !rbs_skels[1].throws);
            CHECK(!rbs_skels[3].floor_set && !rbs_skels[3].aimed && !rbs_skels[3].ground_set && !rbs_walks[1].actor && rbs.logged<99u);
            for(q=10;q<16u;q++) CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(q),0,0,1);   /* started again */
            CHECK(rbs_skelf.n==4u && rbs_walkf.n==2u && nalloc==a0 && nwalk_alloc==b0);   /* no fifth */
            CHECK(rbs_skels[3].record==REC(15) && rbs_walks[1].record==REC(14) && rbs_walks[1].actor==A(14));
            CHECK(WORD_AT(REC(15)+4u)==0x20000u && WORD_AT(REC(14)+4u)==0x100000u);
            q=nprincess;
            CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(15),0,0,0);   /* the fourth fights on its own blocks */
            CHECK(nprincess==q+1u && princess_state==SKST_K(3) && princess_parts==SKPT_K(3));
            CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(11),0,0,0);
            CHECK(princess_state==SKST_K(1) && princess_parts==SKPT_K(1) && WORD_AT(0x0078B1ECu)==SKST_K(0));
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);   /* a second restart: the same blocks again */
            CHECK(nalloc==a0 && nwalk_alloc==b0 && rbs_skelf.n==4u && rbs_walkf.n==2u && rbs_skels[3].state==SKST_K(3) && rbs_walks[1].state==WKST_K(1));
            CHECK(anims_skel() && anims_trophy_gone() && anim_is(0x80Eu,0x118u));      /* and the anims again */
            for(q=0;q<4u;q++) CHECK(rbs_skels[q].record==REC(q<3u?10u+q:15u) && !rbs_skels[q].floor_set);
            CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(12),0,0,0);   /* each fights on its own blocks */
            CHECK(princess_state==SKST_K(2) && princess_parts==SKPT_K(2) && rbs_skels[2].floor_set);
            CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(15),0,0,0);
            CHECK(princess_state==SKST_K(3) && princess_parts==SKPT_K(3) && WORD_AT(0x0078B1ECu)==SKST_K(0));
            restart_take=0x101u;                                          /* princess_low_idle1's slot filled after the drop */
            anim_reg(0x1A0u,0x150u,0x1005800u);                           /* headbite at the mark or past it: a later one may take it */
            rbs_skels[1].parts=0x100u;                                    /* not a usable block: it and the rest dropped */
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);
            CHECK(anim_gone(0x19Du) && WORD_AT(APTRS+0x101u*4u)==0xCAFE0000u && anim_gone(0x1A0u));   /* left alone */
            CHECK(anim_is(0x19Cu,0x100u) && anim_is(0x1A6u,0x10Au) && anim_is(0x1A9u,0x10Bu));
            restart_take=0;
            CHECK(rbs_skelf.n==1u && rbs_skels[0].state==SKST_K(0) && rbs_skels[0].record==REC(10) && WORD_AT(0x0078B1ECu)==SKST_K(0));
            CHECK(!rbs_skels[1].record && !rbs_skels[1].state && !rbs_skels[3].record && !rbs_skels[3].state && rbs_walkf.n==2u);
            FXW(rbs_walks[0].fx,0x0078B08Cu)=0x1234u;                    /* an effect count of neither size: dropped */
            rbs_skels[0].state=0x100u;
            {   /* the trophy monster in the set too: its anims as well, with no boss state left */
                static const uint16_t all3[]={0x15,0x28,0x64};
                container(0x11,all3,3,none7,7);
                WORD_AT(APTRS+0x101u*4u)=0;anims_mansion();
                CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);
                CHECK(anims_skel() && anims_trophy() && anim_gone(0x16Au));
                pak_mounted=0;anims_mansion();                            /* lv1_3 not mounted: not this area's */
                CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);
                CHECK(anims_skel() && anims_trophy_gone());
                container(0x11,both,3,none7,7);
            }
            CHECK(!rbs_skelf.n && !rbs_walkf.n && !rbs_skels[0].record && !WORD_AT(0x0078B1ECu) && !WORD_AT(0x0078B1E8u));
            CHECK(nalloc==a0 && nwalk_alloc==b0);
            CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
            for(q=10;q<15u;q++) CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(q),0,0,1);
            a0=nalloc;s0=nskel_reset;
            WORD_AT(0x0079094Cu)=1u;                                       /* mission 5 resets its own */
            anims_mansion();
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);
            CHECK(nalloc==a0 && nskel_reset==s0 && rbs_skels[0].state==SKST);
            CHECK(anim_gone(0x19Cu) && anim_gone(0x1A9u));                 /* and its anims stay stock */
            WORD_AT(0x0079094Cu)=100u;
            set_env("TSFP_MAPMAKER_LIMITS",NULL);
            anims_mansion();
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);   /* off: the original only */
            CHECK(nalloc==a0 && nskel_reset==s0 && rbs_skels[0].state==SKST && anim_gone(0x19Cu));
            set_env("TSFP_MAPMAKER_LIMITS","1");
            set_env("TSFP_MAPMAKER_REAL_BOSS","0");rbs.read=0;
            anims_mansion();
            CALL(pc_mapmaker_realbosses_restart,fake_restart,0x0003D4E6u,0,0,0,1);   /* the bosses switched off: stock */
            CHECK(anim_gone(0x19Cu));
            set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);rbs.read=0;rbs.optout=0;
        }
        {   /* placed through a set slot 11-20 (sets chunk) */
            static const uint16_t x2[]={0x30,0x28};
            container(0x11,both,3,none7,7);container_extra(x2,2);
            sai_slot(0,11u);sai_slot(1,11u);sai_slot(2,10u);
            CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
            CHECK(rbs_skelf.n==2u);
        }
        {   /* a new level while a walker's copy is live (a call that never came back): the outside values first */
            uint8_t out[0x15C];
            memcpy(out,(void *)((uintptr_t)0x0078B080u+g_xbox_mem_offset),sizeof out);
            memset(&rbs_walks[0].fx,0xA5,sizeof rbs_walks[0].fx);
            (void)mapmaker_fx_enter(&rbs_walks[0].fx);
            CHECK(mapmaker_fx_holder==&rbs_walks[0].fx && !fx_is(out));
            CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
            CHECK(!mapmaker_fx_holder && fx_is(out) && FXW(rbs_skels[0].fx,0x0078B0E0u)==FXW(out,0x0078B0E0u));
        }
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(rbs_skelf.n==0 && rbs_walkf.n==0 && !WORD_AT(0x0078B1ECu) && !WORD_AT(0x0078B1E8u));
        alloc_ok=1;
        WORD_AT(PLAYER+0xFF4u)=ENT(0);
    }
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* not in play */
    CHECK(seen_focus==ENT(0));

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker realbosses ok");
    return 0;
}
