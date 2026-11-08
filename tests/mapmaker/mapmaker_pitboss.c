/* Asset-free contract for src/interface/pc_mapmaker_pitboss.inc (story
 * assets wave 3: the mission 6 pit skeleton and its ragbugs). Fake guest
 * memory holds a map container (the map's own set, Story AI chunks), the AI
 * pool and records, the arena constants at their US XBE values, the
 * resource table, the pad table and the anim slot tables. The fake originals
 * pop their return address like the lifted callees; every hook call checks
 * esp and what the fake fight saw. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) { g_ecx=1; }

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define POOL 0x00400000u
#define RECS 0x00480000u
#define ENTS 0x00490000u
#define RESTAB 0x00500000u
#define REQS 0x00510000u
#define BLOCKS 0x00700000u
#define PADTAB 0x00680004u                /* not 0x20 aligned: the pads must still land on records */
#define HEAP0 0x00800000u

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static int near(float a,float b) { return a-b<0.002f && b-a<0.002f; }

/* ---- the guest functions the area calls -------------------------------- */
void sub_0028A0A0(void),sub_000A1B80(void),sub_000942B0(void);
void sub_0029E0A0(void),sub_0028C560(void),sub_00287E00(void),sub_00287860(void),sub_0015DDA0(void),
    sub_00059DD0(void),sub_000A9070(void),sub_000622D0(void),sub_00062260(void),sub_000A5E40(void),
    sub_000631C0(void),sub_00096220(void);
void sub_00287E00(void) { g_ecx=10; }
void sub_00287860(void) { g_ecx=11; }
void sub_0015DDA0(void) { g_ecx=12; }
void sub_00059DD0(void) { g_ecx=13; }
void sub_000A9070(void) { g_ecx=14; }
void sub_000622D0(void) { g_ecx=15; }
void sub_00062260(void) { g_ecx=16; }
void sub_000A5E40(void) { g_ecx=17; }
void sub_000631C0(void) { g_ecx=18; }
void sub_00096220(void) { g_ecx=19; }
void sub_0028A0A0(void) { g_ecx=20; }
void sub_000A1B80(void) { g_ecx=21; }
void sub_000942B0(void) { g_ecx=22; }

static uint32_t heap=HEAP0,mounted[8],removed[16],deleted[16],requested[8],textured[8];
static unsigned nmounted,nremoved,ndeleted,nrequested,ntextured,nallocs,ninits,mount_calls;
static int anim_done,alloc_fails,mount_fail;
static uint32_t next_block=BLOCKS;
static unsigned launches;
static int hold_intro;
static unsigned poses,hides;
static uint32_t pose_ent,pose_bits;
static uint32_t launch_state;
static float launch_cx;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p=(heap+alignment-1u)&~(alignment-1u);
    heap=p+bytes;
    return p;
}
static int area_on=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)area;(void)hook;return area_on; }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[4]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<4;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00287E00) {                              /* the stock block init: a block, a particle block, statics */
        CHECK(n==0);
        nallocs++;
        if(alloc_fails) return 0;
        WORD_AT(0x0078B1ECu)=next_block;next_block+=0x5000u;
        WORD_AT(0x0078B0D0u)=next_block;next_block+=0x8000u;
        WORD_AT(0x0078B0CCu)+=0x10u;                   /* the statics change with every init */
    } else if(f==sub_00287860) { CHECK(n==0);ninits++;WORD_AT(WORD_AT(0x0078B1ECu)+0x4000u)=0xA11u; }
    else if(f==sub_0015DDA0) { CHECK(n==1);if(nremoved<16u) removed[nremoved]=x[0];nremoved++; }
    else if(f==sub_00059DD0) { CHECK(n==1);r=(uint32_t)anim_done; }
    else if(f==sub_000A9070) { CHECK(n==1);if(ndeleted<16u) deleted[ndeleted]=x[0];ndeleted++; }
    else if(f==sub_000622D0) {
        CHECK(n==3 && x[1]==0 && x[2]==1);
        mount_calls++;
        if(mount_fail) return 0xFFFFFFFFu;
        if(nmounted<8u) mounted[nmounted++]=x[0];
        WORD_AT(0x006D7880u)++;r=1;
    } else if(f==sub_00062260) {
        unsigned k;
        r=0xFFFFFFFFu;
        for(k=0;k<nmounted;k++) if(mounted[k]==x[0]) r=k;
        if(x[0]==0x0049D59Cu) r=7;                     /* lv17_4.pak: the creatures area mounted it */
    } else if(f==sub_000A5E40) { CHECK(n==1);if(nrequested<8u) requested[nrequested]=x[0];nrequested++; }
    else if(f==sub_000631C0) r=x[0]>=0x00520000u && x[0]<0x00520100u?REQS+(x[0]-0x00520000u):0u;
    else if(f==sub_00096220) { CHECK(n==1);if(ntextured<8u) textured[ntextured]=x[0];ntextured++; }
    else if(f==sub_0028A0A0) {                         /* the stock launch: up to 3 parked bugs a call */
        uint32_t st=WORD_AT(0x0078B1ECu);
        unsigned k,m=0;
        CHECK(n==1);
        launches++;launch_state=st;launch_cx=FLOAT_AT(0x0049F380u);
        for(k=0;k<6u && m<3u;k++) if(WORD_AT(st+0x4A30u+k*0x44u) && !WORD_AT(st+0x4A54u+k*0x44u)) { WORD_AT(st+0x4A54u+k*0x44u)=1u;m++; }
    }
    else if(f==sub_000A1B80) {                         /* the pose: the slot bit must be clear for it to compute */
        CHECK(n==1);
        poses++;pose_ent=x[0];pose_bits=HALF_AT(x[0]+0x14u);
        HALF_AT(x[0]+0x14u)|=(uint16_t)(1u<<(WORD_AT(WORD_AT(0x007B0CC0u)+0x214u)&15u));
    }
    else if(f==sub_000942B0) {                         /* the slot-4 hide: instance +0x1F6 |= 8 */
        CHECK(n==2 && x[1]==8u);
        hides++;BYTE_AT(x[0]+0x1F6u)|=(uint8_t)x[1];
    }
    else CHECK(!"unexpected guest call");
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_pitboss.inc"

/* ---- the fake fight and handlers ---------------------------------------- */
static uint32_t seen_state,seen_fx,fights;
static float seen_x,seen_y,seen_z,seen_cx,seen_park;
void sub_0029E0A0(void)
{
    uint32_t st=WORD_AT(0x0078B1ECu),ent=WORD_AT(g_esp+4u),a=WORD_AT(ent+0x7Cu);
    fights++;
    seen_state=st;seen_fx=WORD_AT(0x0078B0D0u);
    seen_x=FLOAT_AT(0x0049F498u);seen_y=FLOAT_AT(0x0049F494u);seen_z=FLOAT_AT(0x0049F37Cu);seen_cx=FLOAT_AT(0x0049F380u);
    FLOAT_AT(st+0x423Cu)+=1.0f;                         /* the rack timer counts */
    if(WORD_AT(a+0x20u)==0u) {                          /* state 0: pads looked up (MapMaker has none: -1) */
        unsigned i;
        for(i=0;i<13u;i++) WORD_AT(st+0x4BF0u+i*4u)=0xFFFFFFFFu;
        WORD_AT(a+0x20u)=1u;
    } else if(WORD_AT(a+0x20u)==1u && !hold_intro) {                   /* the rack is up: the fight proper, the bottle made */
        uint32_t o=st==BLOCKS?0x00452000u:0x00453000u;
        WORD_AT(a+0x20u)=6u;WORD_AT(st+0x4270u)=o;
        HALF_AT(o+0x94u)=0x24Au;BYTE_AT(o+0x30u)=0;FLOAT_AT(o+0x64u)=FLOAT_AT(0x0049F488u);
        BYTE_AT(ent+0x30u)=2;WORD_AT(ent+0x38u)=12u;WORD_AT(ent+0x3Cu)=13u;   /* the boss lists rooms 12, 13 */
    }
    g_esp+=4u;                                          /* the lifted tail jump's ret */
}
static uint32_t seen_msg_state;
void sub_0028C560(void) { seen_msg_state=WORD_AT(0x0078B1ECu);g_eax=0xFFFFFFFFu;g_esp+=4u; }
static unsigned originals;
static uint32_t orig_state;
static void fake_plain(void) { originals++;orig_state=WORD_AT(0x0078B1ECu);g_esp+=4u; }
static void fake_damage(void) { uint32_t st=WORD_AT(0x0078B1ECu);originals++;orig_state=st;FLOAT_AT(st+0x4274u)-=2.6667f;g_esp+=4u; }
static float bug_park;
static void fake_ragbug(void) { originals++;bug_park=FLOAT_AT(0x0049F390u);g_esp+=4u; }
static uint32_t hud_focus;
static void fake_hud(void) { originals++;hud_focus=WORD_AT(WORD_AT(0x007B0C7Cu)+0xFF4u);g_esp+=4u; }
static void fake_restart(void)                          /* the restart unmaps every 0x4000 anim */
{
    unsigned id;
    originals++;
    for(id=0x23Bu;id<0x247u;id++) {
        uint16_t s=HALF_AT(0x007E03C0u+id*2u);
        if(s!=0xFFFFu) { WORD_AT(WORD_AT(0x007E3954u)+s*4u)=0;HALF_AT(0x007E03C0u+id*2u)=0xFFFFu; }
    }
    g_esp+=4u;
}

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value) setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static uint32_t call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,int line)
{
    uint32_t sp=g_esp;
    g_esp-=12u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a0;WORD_AT(g_esp+8u)=a1;
    g_eax=0xDEADu;
    hook(orig);
    if(g_esp!=sp-8u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-(sp-8u)));failures++; }
    g_esp=sp;
    return g_eax;
}
#define CALL(h,o,ret,a0,a1) call_hook(h,o,ret,a0,a1,__LINE__)

/* ---- the map and the world ---------------------------------------------- */
static void chunk(uint32_t *c,uint32_t tag,uint32_t size) { WORD_AT(*c)=tag;WORD_AT(*c+4u)=size;*c+=8u+size; }
/* members ids[0..n-1]; Story AIs: one chunk per slot in sai[0..m-1] (index i, Boss flag boss[i]) */
static void map(uint32_t setno,const uint16_t *ids,unsigned n,const uint8_t *sai,unsigned m,const uint8_t *boss)
{
    uint32_t c=CONT+8u,info;
    unsigned k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x4000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    info=c+8u;chunk(&c,0x00C77668u,0x40u);
    HALF_AT(info+0x14u)=(uint16_t)setno;BYTE_AT(info+0x16u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(info+0x18u+k*4u)=ids[k];
    for(k=0;k<m;k++) {
        uint32_t d=c+8u;
        chunk(&c,0x00C7766Cu,0x50u);
        BYTE_AT(d+0x28u)=sai[k];HALF_AT(d+0x22u)=(uint16_t)k;BYTE_AT(d+0x29u)=boss?boss[k]:0u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static const float stock[][2]={
    {0x0049F498u,-11.8f},{0x0049F494u,-100.279f},{0x0049F37Cu,56.295f},{0x0049F380u,11.8f},{0x0049F378u,100.279f},
    {0x0049F390u,-12.73f},{0x0049F38Cu,-101.5f},{0x0049F388u,62.42f},{0x0049F3A4u,-102.5f},{0x0049F398u,-93.3f},
    {0x0049F394u,-91.3f},{0x0049F3A8u,-75.0f},{0x0049F3ACu,-83.8f},{0x0049F3B0u,-81.8f},{0x0049F3B4u,-79.8f},
    {0x0049F45Cu,-11.28f},{0x0049F48Cu,-77.8f},{0x0049F458u,55.6f},{0x0049F478u,-106.5f},{0x0049F488u,-3.68f},
    {0x0049F484u,-96.88f},{0x0049F480u,58.8f},{0x0049F374u,-11.9f},{0x0049F370u,-91.0f},{0x0049F36Cu,62.0f},
    {0x0049F368u,-18.5f},{0x0049F364u,-87.4f},{0x0049F360u,54.64f},{0x0049F35Cu,-10.5f},{0x0049E854u,-92.0f},
    {0x0049F358u,46.3f},{0x0049F354u,-4.26f},{0x0049F350u,-90.5f},{0x0049F34Cu,55.4f},
};
#define NSTOCK (sizeof stock/sizeof stock[0])
static void constants(void) { unsigned i;for(i=0;i<NSTOCK;i++) FLOAT_AT((uint32_t)stock[i][0])=stock[i][1]; }
static int constants_stock(void)
{
    unsigned i;
    for(i=0;i<NSTOCK;i++) if(FLOAT_AT((uint32_t)stock[i][0])!=stock[i][1]) return 0;
    return 1;
}
/* a pool entry, its record and its entity at (x, y, z) */
static uint32_t actor(unsigned i,uint32_t id,uint32_t hi,float x,float y,float z)
{
    uint32_t a=POOL+i*0xEA0u,rec=RECS+i*0x100u,e=ENTS+i*0x400u;
    memset((void *)((uintptr_t)a+g_xbox_mem_offset),0,0xEA0u);
    memset((void *)((uintptr_t)e+g_xbox_mem_offset),0,0x400u);
    HALF_AT(a+0x48u)=(uint16_t)id;WORD_AT(a+0xBF0u)=rec;WORD_AT(a)=e;
    WORD_AT(rec)=0x10000000u;WORD_AT(rec+4u)=hi;WORD_AT(rec+8u)=0x00001000u+1u+i;
    WORD_AT(e+0x7Cu)=a;WORD_AT(e+4u)=0x00470000u;
    FLOAT_AT(e+0x64u)=x;FLOAT_AT(e+0x68u)=y;FLOAT_AT(e+0x6Cu)=z;
    return a;
}
static void models(int loaded,int read)
{
    static const uint32_t m[4]={0x249u,0x24Bu,0x24Au,0x246u};
    unsigned k;
    WORD_AT(0x00791D4Cu)=RESTAB;
    memset((void *)((uintptr_t)0x008CA000u+g_xbox_mem_offset),0xFF,0xA00u*2u);
    for(k=0;k<4u;k++) {
        HALF_AT(0x008CA000u+m[k]*2u)=(uint16_t)k;
        WORD_AT(RESTAB+k*0x30u+4u)=(uint32_t)loaded;WORD_AT(RESTAB+k*0x30u+0x10u)=0x00520000u+k*0x20u;
        WORD_AT(REQS+k*0x20u+0x10u)=read?0x00530000u:0u;BYTE_AT(REQS+k*0x20u+2u)=(uint8_t)(k==1?0:0x10);
    }
}
static void anims(void)
{
    unsigned i;
    memset((void *)((uintptr_t)0x007E03C0u+g_xbox_mem_offset),0xFF,0xC03u*2u);
    WORD_AT(0x007E3954u)=0x00690000u;WORD_AT(0x006D77F8u)=600u;
    for(i=0;i<12u;i++) {
        HALF_AT(0x007E03C0u+(0x23Bu+i)*2u)=(uint16_t)(500u+i);
        WORD_AT(0x00690000u+(500u+i)*4u)=0x00691000u+i*0x100u;
        WORD_AT(0x007DE660u+(500u+i)*8u+4u)=i==3?0x1800u:0x5800u;   /* one without the flag: not noted */
    }
}

int main(void)
{
    uint32_t a1,a2,a3,b1,b2,b3,e1,e2,st1,st2,p1;
    unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    constants();anims();models(0,1);
    WORD_AT(0x00715F2Cu)=PADTAB;
    WORD_AT(0x007B0CC0u)=0x00470800u;WORD_AT(0x00470800u+0x214u)=3u;
    WORD_AT(0x00470000u+0x10u)=0x00471000u;   /* every fake entity's anim controller: posed bones */
    WORD_AT(0x007B0C7Cu)=0x00460000u;WORD_AT(0x00783A50u)=0x00001000u;   /* Story AI ids: R + index + 1 */
    {
        static const uint16_t set[]={0x0C,0x31,0xB0,0x28};
        static const uint8_t sai[]={1,1,2,2,0},boss[]={0xB1,0xB5,0,0,0};
        map(0x11u,set,4u,sai,5u,boss);
    }
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;

    /* ---- switch off: originals only -------------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
    CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
    CHECK(originals==2 && mount_calls==0 && nallocs==0);
    a1=actor(0,0x31u,0x20000u,4.0f,2.0f,-6.0f);e1=WORD_AT(a1);
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(originals==3 && fights==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- stock story mission 6 (scene 17): untouched ----------------------- */
    WORD_AT(0x0079094Cu)=17u;
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
    CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CALL(pc_mapmaker_pitboss_logic,fake_plain,0x0029EDA5u,0,0);
    CALL(pc_mapmaker_pitboss_failure,fake_plain,0x0029FD15u,0,0);
    CHECK(originals==8 && mount_calls==0 && nallocs==0 && fights==0);
    WORD_AT(0x0079094Cu)=100u;

    /* ---- a preset set: nothing ------------------------------------------- */
    {
        static const uint16_t set[]={0x31,0xB0};
        static const uint8_t sai[]={0};
        map(3u,set,2u,sai,1u,0);
        CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
        CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
        CHECK(mount_calls==0 && nallocs==0 && pit_lv.n==0);
    }
    {
        static const uint16_t set[]={0x0C,0x31,0xB0,0x28};
        static const uint8_t sai[]={1,1,2,2,0},boss[]={0xB1,0xB5,0,0,0};
        map(0x11u,set,4u,sai,5u,boss);
    }

    /* ---- mount: l_17_st and l_17_stm kept open; the skip flag ---------- */
    WORD_AT(0x006D7C4Cu)=1u;
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
    CHECK(mount_calls==0);
    WORD_AT(0x006D7C4Cu)=0;
    WORD_AT(0x006D7880u)=57u;                           /* 57 + 8 > 64: full */
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
    CHECK(mount_calls==0 && !pit_lv.pak_mounted[0]);
    WORD_AT(0x006D7880u)=20u;
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);
    CHECK(mount_calls==2 && nmounted==2 && pit_lv.pak_mounted[0] && pit_lv.pak_mounted[1]);
    CHECK(!strcmp((const char *)((uintptr_t)mounted[0]+g_xbox_mem_offset),"pak/story/l_17_st.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounted[1]+g_xbox_mem_offset),"pak/storymem/l_17_stm.pak"));
    CALL(pc_mapmaker_pitboss_mount,fake_plain,0x0003D000u,0,0);   /* mounted already: not again */
    CHECK(mount_calls==2);

    /* ---- request and textures ----------------------------------------- */
    CALL(pc_mapmaker_pitboss_request,fake_plain,0x0015E7C5u,0,0);
    CHECK(nrequested==4 && requested[0]==0x249u && requested[1]==0x24Bu && requested[2]==0x24Au && requested[3]==0x246u);
    CALL(pc_mapmaker_pitboss_textures,fake_plain,0x0003D5EAu,0,0);
    CHECK(ntextured==3 && textured[0]==0x00520000u && textured[1]==0x00520040u && textured[2]==0x00520060u);
    CHECK(!(BYTE_AT(REQS+2u)&0x10u) && !(BYTE_AT(REQS+0x42u)&0x10u));
    CALL(pc_mapmaker_pitboss_textures,fake_plain,0x0003D5EAu,0,0);   /* once a level */
    CHECK(ntextured==3);

    /* ---- level: two blocks, the statics put back ------------------------- */
    WORD_AT(0x0078B1ECu)=0;WORD_AT(0x0078B0CCu)=0x123u;WORD_AT(0x0078B0D0u)=0x456u;
    CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
    CHECK(nallocs==2 && pit_lv.n==2);
    CHECK(WORD_AT(0x0078B0CCu)==0x123u && WORD_AT(0x0078B0D0u)==0x456u);   /* the live statics as they were */
    st1=pit_lv.k[0].state;st2=pit_lv.k[1].state;
    CHECK(st1==BLOCKS && st2==BLOCKS+0xD000u && WORD_AT(0x0078B1ECu)==st1);   /* home: nobody else held it */
    CHECK(near(pit_lv.base[0],-11.8f) && near(pit_lv.base[3],11.8f));

    /* ---- ai start: two pit skeletons placed, three ragbugs noted --------- */
    a1=actor(0,0x31u,0x20000u,4.0f,2.0f,-6.0f);e1=WORD_AT(a1);
    a2=actor(1,0x31u,0x20000u,40.0f,-3.0f,10.0f);e2=WORD_AT(a2);
    b1=actor(2,0xB0u,0x200000u,39.0f,-3.0f,9.0f);
    b2=actor(3,0xB0u,0x200000u,3.0f,2.0f,-5.0f);
    b3=actor(4,0xB0u,0x200000u,5.0f,2.0f,-5.0f);
    a3=actor(5,0x31u,0x11000000u,0.f,0.f,0.f);         /* bot class (creatures refused): not ours */
    for(i=0;i<6u;i++) CALL(pc_mapmaker_pitboss_ai_start,fake_plain,0x0015BA00u,POOL+i*0xEA0u,0);
    CHECK(pit_lv.k[0].record==RECS && pit_lv.k[1].record==RECS+0x100u && pit_lv.nbugs==3);
    CHECK(pit_lv.k[0].placed && near(pit_lv.k[0].dx,15.8f) && near(pit_lv.k[0].dy,102.279f) && near(pit_lv.k[0].dz,-62.295f));
    CHECK(pit_lv.k[0].boss==1u && pit_lv.k[1].boss==2u);
    (void)a3;

    /* ---- the fight: its block, its statics, its arena; all back after --- */
    WORD_AT(0x0078B0D0u)=0x456u;
    anim_done=0;
    HALF_AT(e1+0x14u)=8u;                               /* drawn earlier this view slot: stale bones */
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(fights==1 && seen_state==st1 && seen_fx==pit_lv.k[0].parts);
    CHECK(poses==0u && HALF_AT(e1+0x14u)==8u);         /* .68: no pose call from the fight frame (it broke the camera) */
    CHECK(near(seen_x,4.0f) && near(seen_y,2.0f) && near(seen_z,-6.0f) && near(seen_cx,11.8f-15.8f));
    CHECK(constants_stock() && WORD_AT(0x0078B1ECu)==st1 && WORD_AT(0x0078B0D0u)==0x456u);
    CHECK(FLOAT_AT(st1+0x423Cu)==0.0f);                 /* rack timer held */
    for(i=0;i<13u;i++) {                                 /* pads: the instance's records, through the table */
        uint32_t r=PADTAB+(uint32_t)((int32_t)WORD_AT(st1+0x4BF0u+i*4u)*0x20);
        CHECK(WORD_AT(st1+0x4BF0u+i*4u)==MAPMAKER_POOL_PADS-13u+i);   /* the pool's tail, below the cover base 0x4000 (sub_0009A420) */
        CHECK(WORD_AT(r)==0x4B5u+i && WORD_AT(r+4u)==1u);
        CHECK(near(FLOAT_AT(r+0xCu),pit_pad_xyz[i][0]+15.8f) && near(FLOAT_AT(r+0x10u),pit_pad_xyz[i][1]+102.279f));
    }
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e2,0);
    CHECK(fights==2 && seen_state==st2 && near(seen_x,40.0f) && near(seen_z,10.0f) && constants_stock());
    CHECK(WORD_AT(0x0078B1ECu)==st1);
    /* a mansion skeleton (char 0x28) and anything else: the original */
    {
        uint32_t m=actor(6,0x28u,0x20000u,0.f,0.f,0.f);
        originals=0;
        CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,WORD_AT(m),0);
        CALL(pc_mapmaker_pitboss_message,fake_plain,0x0027D3ACu,WORD_AT(m),2u);
        CHECK(originals==2 && fights==2);
    }

    /* ---- message: the pit handler with its block ------------------------- */
    CHECK(CALL(pc_mapmaker_pitboss_message,fake_plain,0x0027D3ACu,e2,2u)==0xFFFFFFFFu && seen_msg_state==st2 && WORD_AT(0x0078B1ECu)==st1);

    /* ---- callbacks: the owner, the current fight, else home ------------- */
    WORD_AT(st2+0x4270u)=0x00444000u;                  /* pit 2's bottle */
    WORD_AT(0x00445000u)=0x00444000u;                  /* a damage record on it */
    WORD_AT(0x0078B1ECu)=0x00999000u;                  /* somebody else's block outside */
    CALL(pc_mapmaker_pitboss_bottle_damage,fake_plain,0x000BB0F0u,0x00445000u,0);
    CHECK(orig_state==st2 && WORD_AT(0x0078B1ECu)==0x00999000u);
    CALL(pc_mapmaker_pitboss_bottle_slot,fake_plain,0x000A5D00u,0x00444000u,0);
    CHECK(orig_state==st2);
    CALL(pc_mapmaker_pitboss_boss_slot,fake_plain,0x000A5D00u,e1,0);
    CHECK(orig_state==st1);
    CALL(pc_mapmaker_pitboss_fire_type,fake_plain,0x00378500u,0x00447000u,0);
    CHECK(orig_state==st1 && WORD_AT(0x0078B1ECu)==0x00999000u);   /* no owner: home */
    WORD_AT(0x0078B1ECu)=st1;

    /* ---- the NMME test: only at its two sites, only inside a fight ------- */
    originals=0;
    CALL(pc_mapmaker_pitboss_logic,fake_plain,0x0029EDA5u,0,0);   /* no fight running: stock */
    CHECK(originals==1);
    pit_lv.cur=&pit_lv.k[0];pit_lv.k[0].ent=e1;
    CHECK(CALL(pc_mapmaker_pitboss_logic,fake_plain,0x0029EDA5u,0,0)==0u);
    anim_done=0;
    CHECK(CALL(pc_mapmaker_pitboss_logic,fake_plain,0x0029EED1u,0,0)==0u);
    anim_done=1;
    CHECK(CALL(pc_mapmaker_pitboss_logic,fake_plain,0x0029EED1u,0,0)==1u);
    CALL(pc_mapmaker_pitboss_logic,fake_plain,0x00234000u,0,0);
    CHECK(originals==2);
    pit_lv.cur=0;

    /* ---- event 2 and the failure: dropped at their sites only -------------- */
    originals=0;
    CALL(pc_mapmaker_pitboss_event,fake_plain,0x0029FC38u,2u,0);
    CALL(pc_mapmaker_pitboss_failure,fake_plain,0x0029FD15u,0,0);
    CHECK(originals==0);
    CALL(pc_mapmaker_pitboss_event,fake_plain,0x0029E068u,3u,0);
    CALL(pc_mapmaker_pitboss_failure,fake_plain,0x00212000u,0,0);
    CHECK(originals==2);

    /* ---- ragbugs: nearest placed skeleton, its offset, its flock -------- */
    originals=0;
    CALL(pc_mapmaker_pitboss_ragbug,fake_ragbug,0x0027DEE3u,WORD_AT(b1),0);
    CHECK(originals==1 && pit_lv.bug[0].owner==1 && pit_lv.bug[0].slot==0 && near(bug_park,-12.73f+51.8f) && constants_stock());
    CALL(pc_mapmaker_pitboss_ragbug,fake_ragbug,0x0027DEE3u,WORD_AT(b2),0);
    WORD_AT(WORD_AT(b2)+0x28u)=0xFFu;
    hold_intro=1;CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);hold_intro=0;
    CHECK(!pit_lv.k[0].released);                      /* b3 has not joined yet: the flock is not complete */
    CALL(pc_mapmaker_pitboss_ragbug,fake_ragbug,0x0027DEE3u,WORD_AT(b3),0);
    CHECK(pit_lv.bug[1].owner==0 && pit_lv.bug[2].owner==0 && pit_lv.bug[2].slot==1 && near(bug_park,-12.73f+15.8f));
    WORD_AT(st1+0x4BCCu)=0x00454000u;HALF_AT(0x00454000u+0x94u)=0x249u;WORD_AT(0x00454000u+4u)=0x00455000u;   /* its collision hull */
    hold_intro=1;                                       /* still in its intro (state 1): the flock waits */
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(launches==0u && pit_lv.k[0].released);       /* every ragbug joined: noted, not launched */
    CHECK(hides==1u && (BYTE_AT(0x00455000u+0x1F6u)&8u));   /* the hull hidden as the story's slot 4 does, once */
    hold_intro=0;
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(WORD_AT(st1+0x4A30u)==WORD_AT(b2) && WORD_AT(st1+0x4A74u)==WORD_AT(b3) && WORD_AT(WORD_AT(b2)+0x28u)==0xFEu);
    /* the fight proper: no launch of ours (story timing: state 10 only), the hull stays hidden */
    CHECK(launches==0u && WORD_AT(st1+0x4A54u)==0u && WORD_AT(st1+0x4270u)==0x00452000u && hides==1u && constants_stock());
    CHECK(BYTE_AT(0x00452000u+0x30u)==2u && WORD_AT(0x00452000u+0x38u)==12u && WORD_AT(0x00452000u+0x3Cu)==13u && WORD_AT(0x00452000u+0x34u)==12u);
    /* the bottle at the mouth: 11 over the feet, 2.5 ahead (yaw 0: +z), whatever the fight wrote; the hull has its callback */
    CHECK(near(FLOAT_AT(0x00452000u+0x64u),4.0f) && near(FLOAT_AT(0x00452000u+0x68u),13.0f) && near(FLOAT_AT(0x00452000u+0x6Cu),-3.5f));
    CHECK(WORD_AT(0x00454000u+0x19Cu)==0x0028C2F0u);
    {   /* a NaN bone point and a turned boss: still a finite mouth, no NaN left */
        uint32_t o=0x00452000u;
        memset((void *)((uintptr_t)(o+0x64u)+g_xbox_mem_offset),0xFF,12u);FLOAT_AT(o+0xC0u)=FLOAT_AT(o+0x64u);
        FLOAT_AT(e1+0x70u)=1.5707963f;
        CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
        CHECK(near(FLOAT_AT(o+0x64u),6.5f) && near(FLOAT_AT(o+0x68u),13.0f) && near(FLOAT_AT(o+0x6Cu),-6.0f) && FLOAT_AT(o+0xC0u)==0.0f);
        CHECK(near(FLOAT_AT(o+0x70u),1.5707963f));
        FLOAT_AT(e1+0x70u)=0.0f;
    }
    /* a hit on it: logged with the health before and after */
    {
        static void (*const dmg)(void)=fake_damage;
        WORD_AT(0x00446000u)=0x00452000u;WORD_AT(0x00446000u+0x14u)=0x21u;FLOAT_AT(0x00446000u+0x20u)=1.0f;
        FLOAT_AT(st1+0x4274u)=8.0f;
        CALL(pc_mapmaker_pitboss_bottle_damage,dmg,0x000BB63Au,0x00446000u,0);
        CHECK(FLOAT_AT(st1+0x4274u)==8.0f-2.6667f && orig_state==st1);
        WORD_AT(0x00446000u)=0x00454000u;                /* a hit on the hull: a quarter */
        CALL(pc_mapmaker_pitboss_bottle_damage,dmg,0x000BB63Au,0x00446000u,0);
        CHECK(near(FLOAT_AT(st1+0x4274u),8.0f-2.6667f-2.6667f*0.25f) && orig_state==st1);
    }
    {   /* a map whose own pads reach the pool's tail: the rim pads are not written over them */
        WORD_AT(0x007B6664u)=MAPMAKER_POOL_PADS-10u;WORD_AT(st1+0x4BF0u)=0xFFFFFFFFu;
        CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
        CHECK(WORD_AT(st1+0x4BF0u)==0xFFFFFFFFu);
        WORD_AT(0x007B6664u)=0;
    }
    WORD_AT(st1+0x4A30u)=0;                             /* the death removed it */
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(WORD_AT(st1+0x4A30u)==0 && WORD_AT(st1+0x4A74u)==WORD_AT(b3) && launches==0u);

    /* ---- hud and meter --------------------------------------------------- */
    WORD_AT(0x00460000u+0xFF4u)=e1;
    CALL(pc_mapmaker_pitboss_hud,fake_hud,0x001CEF00u,0,0);
    CHECK(hud_focus==0 && WORD_AT(0x00460000u+0xFF4u)==e1);
    originals=0;
    CALL(pc_mapmaker_pitboss_meter,fake_plain,0x0014D264u,a1,0);   /* Boss bar: no meter */
    CHECK(originals==0);
    CALL(pc_mapmaker_pitboss_meter,fake_plain,0x0014D264u,a2,0);   /* Sub-boss bar: the meter */
    CHECK(originals==1);

    /* ---- restart: props deleted, anims mapped again, blocks again -------- */
    WORD_AT(st1+0x4270u)=0x00448000u;WORD_AT(st1+0x4BCCu)=0x00449000u;WORD_AT(st1+0x4BD8u)=0x0044A000u;WORD_AT(st1+0x4BDCu)=0x0044B000u;
    WORD_AT(st2+0x1234u)=0x77u;
    ninits=0;ndeleted=0;
    CALL(pc_mapmaker_pitboss_restart,fake_restart,0x0003D4E6u,0,0);
    CHECK(ndeleted==5);                                 /* pit 1's four, pit 2's bottle */
    CHECK(WORD_AT(st1+0x4270u)==0 && ninits==2 && WORD_AT(st2+0x1234u)==0 && WORD_AT(st2+0x4000u)==0xA11u);
    CHECK(pit_lv.n==2 && pit_lv.k[0].state==st1 && pit_lv.k[0].record==RECS && !pit_lv.k[0].placed);
    for(i=0;i<12u;i++) CHECK(HALF_AT(0x007E03C0u+(0x23Bu+i)*2u)==(i==3?0xFFFFu:500u+i));
    CHECK(WORD_AT(0x00690000u+500u*4u)==0x00691000u);
    CHECK(pit_lv.bug[0].owner==-1 && constants_stock());

    /* ---- not ready or no block: removed ---------------------------------- */
    CALL(pc_mapmaker_pitboss_ai_start,fake_plain,0x0015BA00u,a1,0);
    models(0,0);pit_lv.k[0].ready=0;
    nremoved=0;
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(nremoved==1 && removed[0]==e1);
    models(1,0);
    p1=fights;
    CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,e1,0);
    CHECK(fights==p1+1);
    {
        uint32_t x=actor(7,0x31u,0x20000u,0.f,0.f,0.f);    /* a third: no block */
        CALL(pc_mapmaker_pitboss_princess,fake_plain,0x0027DDD8u,WORD_AT(x),0);
        CHECK(nremoved==2 && removed[1]==WORD_AT(x) && fights==p1+1);
    }

    /* ---- a level without the pit skeleton: ragbugs removed --------------- */
    {
        static const uint16_t set[]={0x0C,0xB0};
        static const uint8_t sai[]={1};
        map(0x11u,set,2u,sai,1u,0);
        CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
        CHECK(pit_lv.n==0);
        nremoved=0;originals=0;
        CALL(pc_mapmaker_pitboss_ragbug,fake_ragbug,0x0027DEE3u,WORD_AT(b1),0);
        CHECK(nremoved==1 && originals==0);
    }
    /* ---- no level memory: no instance ------------------------------------- */
    {
        static const uint16_t set[]={0x31};
        static const uint8_t sai[]={0};
        map(0x11u,set,1u,sai,1u,0);
        alloc_fails=1;
        CALL(pc_mapmaker_pitboss_level,fake_plain,0x0015E81Du,0,0);
        CHECK(pit_lv.n==0);
        alloc_fails=0;
    }

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("ok\n");
    return 0;
}
