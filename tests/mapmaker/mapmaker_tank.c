/* Asset-free contract for src/interface/pc_mapmaker_tank.inc: the castle tank
 * in MapMaker story play. prop6.pak mount at the level's pak mounts, the tank
 * created at a placeholder Story AI (id 0x12), the placeholder removed on the
 * tank's first frame, the proximity wake, the courtyard test moved to the
 * spawn with its bounded drive-target search, fight-state logs, the castle
 * frame shift around the target and state-7 code, the parked wreck, the
 * dropped level event and the bar labels. Fake guest memory; fake originals
 * pop their return address like the lifted callees, and every hook call
 * checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
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
#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define PLAYERS 0x00510000u
#define RES 0x00520000u
#define GUNS (WORD_AT(0x0078B560u))   /* the active tank's gun records */
#define TANK 0x00560000u
#define ESP0 0x00600000u
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define PENT(i) (0x00580000u+(uint32_t)(i)*0x400u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=5; }   /* pc_mapmaker_limits.inc */
void sub_00062260(void) { g_ecx=1; }
void sub_000622D0(void) { g_ecx=2; }
void sub_000A6F90(void) { g_ecx=3; }
void sub_0015DDA0(void) { g_ecx=4; }
void sub_000A5E40(void) { g_ecx=6; }
void sub_000631C0(void) { g_ecx=7; }
void sub_00256040(void) { g_ecx=8; }
/* sub_00256040(tank, explosive), as stock: only in state 9 with the explosive
 * on parts 15..17, health -= 10.1, the death (state 10) at 0 */
static unsigned nmine;
static uint32_t mine_args[2], mine_state, mine_guns;
/* model requests: rows 3/4/5 (tank/turret/wreck) have path PATH(row) and,
 * once requested, a request REQ(row) whose +0x10 holds the file when read */
#define PATH(r) (0x00530000u+(uint32_t)(r)*0x40u)
#define REQ(r) (0x00534000u+(uint32_t)(r)*0x40u)
static int req_state[8], req_reads=1, req_after_original;
static unsigned nrequest;
#define READY() (req_state[3]=req_state[4]=req_state[5]=2)   /* the models were read */
static uint32_t requested[8];
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }

static int pak_mounted, mount_ok=1, spawn_ok=1, originals;
static unsigned nmount, nspawn, nremove;
static uint32_t mount_args[3], spawn_args[5], remove_arg;
static float spawn_pos[3], spawn_rot[3];
static int spawn_after_original;
static uint32_t spawn_obj=TANK;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) return (x[0]==0x0049DF7Cu && pak_mounted)?4u:0xFFFFFFFFu;
    if(f==sub_000622D0) {
        nmount++;memcpy(mount_args,x,sizeof mount_args);
        if(mount_ok && x[0]==0x0049DF7Cu) pak_mounted=1;
        return mount_ok?7u:0xFFFFFFFFu;
    }
    if(f==sub_000A6F90) {   /* sub_000A6EC0: object at pos/rot, then the init (sub_00251AA0) */
        nspawn++;memcpy(spawn_args,x,sizeof spawn_args);
        spawn_after_original=originals;
        for(i=0;i<3;i++) { spawn_pos[i]=FLOAT_AT(x[1]+(uint32_t)i*4u);spawn_rot[i]=FLOAT_AT(x[2]+(uint32_t)i*4u); }
        if(!spawn_ok) return 0;
        for(i=0;i<3;i++) FLOAT_AT(spawn_obj+0x64u+(uint32_t)i*4u)=spawn_pos[i];
        HALF_AT(spawn_obj+0x94u)=(uint16_t)x[0];
        WORD_AT(spawn_obj+0x28u)=(WORD_AT(spawn_obj+0x28u)&~1u)|0x10u;   /* sub_002560C0: hidden until its fight */
        /* sub_00251AA0: zero the block, point it at the XBE tables, reset sounds and some table words */
        memset((void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),0,0x64u);
        WORD_AT(0x0078B560u)=0x0051B9C8u;WORD_AT(0x0078B564u)=0x0051B998u;WORD_AT(0x0078B568u)=0x0051B920u;WORD_AT(0x0078B56Cu)=0x0051BB44u;
        FLOAT_AT(0x0078B580u)=-9.0f;FLOAT_AT(0x0078B584u)=spawn_pos[1];FLOAT_AT(0x0078B588u)=-3.2f;
        for(i=0;i<0x1C;i++) WORD_AT(0x0075F468u+(uint32_t)i*4u)=0x5000u+nspawn;
        FLOAT_AT(0x0051B9C8u+0x10u)=(float)nspawn;WORD_AT(0x0051B9C0u)=0;WORD_AT(0x0051B920u+0x68u)=0;
        return spawn_obj;
    }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_00256040) {
        int32_t part=(int32_t)WORD_AT(x[1]+0x15Cu);
        nmine++;mine_args[0]=x[0];mine_args[1]=x[1];mine_state=WORD_AT(0x0078B594u);mine_guns=WORD_AT(0x0078B560u);
        if(WORD_AT(0x0078B594u)!=9u || part<15 || part>17) return 0;
        FLOAT_AT(x[0]+0x1CCu)-=10.1f;
        if(!(FLOAT_AT(x[0]+0x1CCu)>0.0f)) WORD_AT(0x0078B594u)=10u;
        return 0;
    }
    if(f==sub_000A5E40) {   /* queued; the load pass reads it before any AI starts */
        int16_t r=(int16_t)HALF_AT(0x008CA000u+x[0]*2u);
        if(nrequest<8u) requested[nrequest]=x[0];
        nrequest++;req_after_original=originals;
        if(r>=0 && r<8) req_state[r]=req_reads?2:1;
        return 0;
    }
    if(f==sub_000631C0) {
        for(i=0;i<8;i++) if(x[0]==PATH(i) && req_state[i]) { WORD_AT(REQ(i)+0x10u)=req_state[i]==2?0x00538000u:0;return REQ(i); }
        return 0;
    }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
/* pc_mapmaker_bosses.inc's Boss bar hooks, as in the build (included first there) */
#define BOS_BAR_EXTRA 1
typedef int (*BosBarExtra)(unsigned k,uint32_t *entity,uint32_t *id);
static BosBarExtra bar_extra;
static unsigned bar_adds;
static void bos_bar_extra_add(BosBarExtra f) { bar_extra=f;bar_adds++; }
static const char *story_names[8];                 /* record ids 0x100..0x107 */
static const char *bos_story_name(uint32_t id) { return id>=0x100u && id<0x108u?story_names[id-0x100u]:0; }
#include "pc_mapmaker_tank.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define NEAR(a,b) ((a)-(b)<0.001f && (b)-(a)<0.001f)
static uint32_t orig_eax, pre_eax=0xDDDDu;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
/* sub_00254690 seen in the castle frame: records the tank and target it saw,
 * picks a castle point (10, 0.5, -2) unless told not to. */
static float seen_tank[3], seen_target[3], seen_goal[3];
static int target_writes=1;
static void fake_target(void)
{
    int i;
    uint32_t t=WORD_AT(0x0078B5B0u);
    originals++;
    for(i=0;i<3;i++) { seen_tank[i]=FLOAT_AT(g_edi+0x64u+(uint32_t)i*4u);seen_goal[i]=FLOAT_AT(0x0078B580u+(uint32_t)i*4u); }
    if(t) for(i=0;i<3;i++) seen_target[i]=FLOAT_AT(t+0x64u+(uint32_t)i*4u);
    if(target_writes) { FLOAT_AT(0x0078B580u)=10.0f;FLOAT_AT(0x0078B584u)=0.5f;FLOAT_AT(0x0078B588u)=-2.0f; }
    g_esp+=4u;
}
/* sub_00255390 state 7: castle aim points into the guns, then sub_00254690. */
static void fake_state(void)
{
    uint32_t k;
    originals++;
    for(k=0;k<5u;k++) { FLOAT_AT(GUNS+k*0x4Cu+0x10u)=-5.3f;FLOAT_AT(GUNS+k*0x4Cu+0x14u)=-1.4f;FLOAT_AT(GUNS+k*0x4Cu+0x18u)=19.9f; }
    g_edi=g_eax;
    g_esp-=4u;WORD_AT(g_esp)=0x002554CCu;
    pc_mapmaker_tank_target(fake_target);   /* the inner call: already shifted */
    WORD_AT(0x0078B598u)=WORD_AT(0x0078B594u);
    g_esp+=4u;
}
/* sub_00255C20: wreck, turret, the tank parked at the castle point (0, 20, 0). */
static void fake_wreck(void) { originals++;FLOAT_AT(g_esi+0x64u)=0;FLOAT_AT(g_esi+0x68u)=20.0f;FLOAT_AT(g_esi+0x6Cu)=0;g_esp+=4u; }
/* sub_002522B0: records the cannon aim point it turned toward */
#define CANNON (GUNS+4u*0x4Cu+0x10u)
static float gun_seen[3];
static void fake_gun(void) { int i;originals++;for(i=0;i<3;i++) gun_seen[i]=FLOAT_AT(CANNON+(uint32_t)i*4u);g_esp+=4u; }
static void cannon(float x,float y,float z) { FLOAT_AT(CANNON)=x;FLOAT_AT(CANNON+4u)=y;FLOAT_AT(CANNON+8u)=z; }
/* an entry's original: records the state it ran with, changes it, and can
 * run another tank's damage inside (a shell hitting it) */
static uint32_t entry_state, entry_guns, entry_set, nest_obj, inner_state;
static void fake_inner(void) { originals++;inner_state=WORD_AT(0x0078B594u);WORD_AT(0x0078B594u)=99u;g_esp+=4u; }
void pc_mapmaker_tank_damage(void (*original)(void));
static void fake_entry(void)
{
    originals++;
    entry_state=WORD_AT(0x0078B594u);entry_guns=WORD_AT(0x0078B560u);
    if(nest_obj) {
        WORD_AT(0x0078B598u)=0x77u+entry_set;                         /* changed before the inner call */
        WORD_AT(0x00570400u)=nest_obj;
        g_esp-=8u;WORD_AT(g_esp)=0x00256012u;WORD_AT(g_esp+4u)=0x00570400u;
        pc_mapmaker_tank_damage(fake_inner);
        g_esp+=4u;
        if(WORD_AT(0x0078B594u)!=entry_state) entry_state=0xBADu;     /* the outer tank's state is back */
    }
    WORD_AT(0x0078B594u)=entry_set;WORD_AT(0x0075F468u)=entry_set;
    g_esp+=4u;
}
static int ai_originals;
static void fake_ai(void) { originals++;ai_originals++;g_esp+=4u; }
/* sub_001CDD60: notes how many tanks the area still knew when it ran (1 + n) */
static unsigned restart_saw;
static void fake_restart(void) { originals++;restart_saw=tnk_lv.n+1u;g_esp+=4u; }

static uint32_t call(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,int expect_orig,int line)
{
    int before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=a1;WORD_AT(ESP0+8u)=a2;WORD_AT(ESP0+12u)=a3;
    g_eax=pre_eax;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));failures++; }
    if(expect_orig>=0 && originals-before!=expect_orig) { fprintf(stderr,"%s:%d: original ran %d times\n",__FILE__,line,originals-before);failures++; }
    g_esp=ESP0;pre_eax=0xDDDDu;
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
    tnk.read=0;
}
/* map container: map-info chunk (custom set), optional sets chunk (slots 11-20) */
static void container(uint32_t set_index,const uint16_t *ids,unsigned n,const uint16_t *extra,unsigned ne)
{
    uint32_t c=CONT+8u,k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x00080000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    WORD_AT(c)=0x00C77668u;WORD_AT(c+4u)=0x18u+20u*4u;
    HALF_AT(c+8u+0x14u)=(uint16_t)set_index;BYTE_AT(c+8u+0x16u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(c+8u+0x18u+k*4u)=ids[k];
    c+=8u+0x18u+20u*4u;
    if(extra) {
        WORD_AT(c)=0x00C776F5u;WORD_AT(c+4u)=0x30u;
        HALF_AT(c+8u)=1u;BYTE_AT(c+10u)=10u;BYTE_AT(c+11u)=(uint8_t)ne;
        for(k=0;k<ne;k++) HALF_AT(c+12u+k*4u)=extra[k];
        c+=8u+0x30u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static void actor(unsigned i,uint32_t id,float x,float y,float z,float yaw)
{
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);
    FLOAT_AT(ENT(i)+0x64u)=x;FLOAT_AT(ENT(i)+0x68u)=y;FLOAT_AT(ENT(i)+0x6Cu)=z;FLOAT_AT(ENT(i)+0x70u)=yaw;
}
/* scene-100 resource table: 0x281 (init 0x002560C0), 0x297, 0x298 */
static void resources(int tank,int turret,int wreck,uint32_t init)
{
    uint32_t m;
    for(m=0;m<0xA00u;m++) HALF_AT(0x008CA000u+m*2u)=0xFFFFu;
    WORD_AT(0x00791D4Cu)=RES;
    if(tank) { HALF_AT(0x008CA000u+0x281u*2u)=3;WORD_AT(RES+3u*0x30u+0x2Cu)=init; }
    if(turret) HALF_AT(0x008CA000u+0x297u*2u)=4;
    if(wreck) HALF_AT(0x008CA000u+0x298u*2u)=5;
    HALF_AT(0x008CA000u+0x12u*2u)=6;
    for(m=0;m<8u;m++) { WORD_AT(RES+m*0x30u+0x10u)=PATH(m);WORD_AT(RES+m*0x30u+0x14u)=0;WORD_AT(RES+m*0x30u+4u)=0;req_state[m]=0; }
    HALF_AT(0x0078B860u+0x281u*2u)=1;HALF_AT(0x0078B860u+0x297u*2u)=1;HALF_AT(0x0078B860u+0x298u*2u)=1;   /* the stock map: stale, never written */
    WORD_AT(RES+1u*0x30u+0x2Cu)=0;
}
static void players(float x0,float z0,float h0,float x1,float z1,float h1)
{
    WORD_AT(0x00790950u)=2u;WORD_AT(0x007B0C48u)=PLAYERS;
    WORD_AT(PLAYERS+0x14u)=PENT(0);WORD_AT(PLAYERS+0x1584u+0x14u)=PENT(1);
    FLOAT_AT(PENT(0)+0x64u)=x0;FLOAT_AT(PENT(0)+0x6Cu)=z0;FLOAT_AT(PENT(0)+0x1CCu)=h0;
    FLOAT_AT(PENT(1)+0x64u)=x1;FLOAT_AT(PENT(1)+0x6Cu)=z1;FLOAT_AT(PENT(1)+0x1CCu)=h1;
}
static void play_scene(void) { WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u; }
/* the stock courtyard rectangles 0x0051BB58 (x1, z1, x2, z2; x reversed) */
static const float yard[12]={-3.0f,-10.0f,-15.0f,-6.8f, -3.0f,-6.8f,-17.5f,6.7f, -3.0f,6.7f,-21.8f,13.0f};
static void stock_yard(void) { memcpy((void *)((uintptr_t)0x0051BB58u+g_xbox_mem_offset),yard,sizeof yard); }
#define PT 0x00570800u
/* sub_00251CD0 on (x, z), called from ret */
static uint32_t area_at(float x,float z,uint32_t ret,int line)
{
    FLOAT_AT(PT)=x;FLOAT_AT(PT+4u)=99.0f;FLOAT_AT(PT+8u)=z;g_ecx=PT;pre_eax=0xEEEEu;
    return call(pc_mapmaker_tank_area,fake,ret,0,0,0,0,line);
}
#define AREA(x,z) area_at(x,z,0x00255697u,__LINE__)
/* a new level with the tank placeholder at (40, 2, 30) facing 1.5 */
static uint32_t level_with_tank(int line)
{
    static const uint16_t ids[]={0x0C,0x12,0xA8};
    unsigned s=nspawn;
    play_scene();
    container(0x11,ids,3,NULL,0);
    resources(1,1,1,0x002560C0u);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;WORD_AT(0x006D7C4Cu)=0;
    call(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1,line);
    call(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1,line);
    actor(0,0x12,40.0f,2.0f,30.0f,1.5f);
    call(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1,line);
    if(nspawn!=s+1u) { fprintf(stderr,"%s:%d: tank not spawned\n",__FILE__,line);failures++; }
    return TANK;
}

int main(void)
{
    static const uint16_t with[]={0x0C,0x12,0xA8}, without[]={0x0C,0xA8}, extra[]={0x30,0x12};
    unsigned n,k;
    float dx,dy,dz;
    g_xbox_mem_offset=(uintptr_t)memory;
    stock_yard();
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_TANK",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    play_scene();
    container(0x11,with,3,NULL,0);
    resources(1,1,1,0x002560C0u);
    WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==0);
    pak_mounted=1;
    actor(0,0x12,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    CALL(pc_mapmaker_tank_event,fake,0x00255A94u,0x16u,0,0,1);
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,1)==0x1234u);
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- mount ------------------------------------------------------------ */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1 && mount_args[0]==0x0049DF7Cu && mount_args[1]==0 && mount_args[2]==1u && pak_mounted);
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);    /* already mounted */
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x006D7880u)=57u;                   /* one slot left: kept for stock */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1);
    WORD_AT(0x006D7880u)=56u;
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x0C,with,3,NULL,0);                           /* a preset set */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x11,without,2,extra,2);                       /* slot 12 */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==3 && pak_mounted);
    pak_mounted=0;
    container(0x11,with,3,NULL,0);
    BYTE_AT(CONT+8u+8u+0x16u)=1u;                            /* slot count 1: id 0x0C only */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==3);
    WORD_AT(CONT)=0;                                          /* unreadable container */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4 && pak_mounted);
    pak_mounted=0;
    container(0x11,with,3,NULL,0);
    BYTE_AT(0x007DE455u)=3u;                                  /* not story */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();WORD_AT(0x0079094Cu)=4u;                     /* the castle itself */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();
    set_env("TSFP_MAPMAKER_TANK","0");
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    actor(0,0x12,40.0f,2.0f,30.0f,1.5f);pak_mounted=1;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_TANK",NULL);
    mount_ok=0;
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==5 && !pak_mounted);
    mount_ok=1;

    /* ---- ai_start: the placeholder becomes the tank ------------------------- */
    actor(0,0x12,40.0f,2.0f,30.0f,1.5f);
    READY();
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* pak missing */
    CHECK(nspawn==0 && !tnk_lv.n);
    pak_mounted=1;
    resources(0,1,1,0x002560C0u);
    READY();
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,1,0x00251450u);                                     /* another init */
    READY();
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,0,1,0x002560C0u);
    READY();
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,0,0x002560C0u);
    READY();
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,1,0x002560C0u);
    READY();
    actor(1,0x13,40.0f,2.0f,30.0f,1.5f);                               /* another character */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(nspawn==0);
    /* ---- request: the models are queued at story level init ---------------------- */
    req_state[3]=req_state[4]=req_state[5]=0;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* never requested */
    CHECK(nspawn==0);
    WORD_AT(RES+3u*0x30u+4u)=0x00539000u;WORD_AT(RES+4u*0x30u+4u)=1u;WORD_AT(RES+5u*0x30u+4u)=1u;   /* loaded before */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==1 && tnk_lv.n==1u && tnk_lv.t[0].obj==TANK);
    resources(1,1,1,0x002560C0u);memset(&tnk_lv,0,sizeof tnk_lv);nspawn=0;
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);         /* no tank in the set */
    CHECK(nrequest==0);
    container(0x11,with,3,NULL,0);
    pak_mounted=0;
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    pak_mounted=1;
    resources(1,0,1,0x002560C0u);
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    resources(1,1,1,0x002560C0u);
    WORD_AT(0x0079094Cu)=4u;                                           /* the castle requests its own */
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    play_scene();
    set_env("TSFP_MAPMAKER_TANK","0");
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    set_env("TSFP_MAPMAKER_TANK",NULL);
    req_reads=0;                                                       /* requested, file not read */
    n=originals;
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==3 && requested[0]==0x281u && requested[1]==0x297u && requested[2]==0x298u && req_after_original==(int)n+1);
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    req_reads=1;resources(1,1,1,0x002560C0u);
    CALL(pc_mapmaker_tank_request,fake,0x001CDC25u,0,0,0,1);
    req_state[5]=1;                                                    /* the wreck alone not read */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    req_state[5]=2;
    WORD_AT(RES+4u*0x30u+0x14u)=PATH(7);                               /* a second model path, unrequested */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    req_state[7]=2;
    spawn_ok=0;
    FLOAT_AT(0x0078B580u)=-9.0f;FLOAT_AT(0x0078B588u)=-3.2f;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==1 && !tnk_lv.n);
    CHECK(FLOAT_AT(0x0078B580u)==-9.0f && FLOAT_AT(0x0078B588u)==-3.2f);   /* nothing rebased */
    spawn_ok=1;
    n=originals;
    FLOAT_AT(0x0078B580u)=77.0f;WORD_AT(0x0078B594u)=0x55u;WORD_AT(0x0075F468u)=0x66u;   /* the statics before */
    FLOAT_AT(0x0051B9C8u+0x10u)=88.0f;
    WORD_AT(TANK+0x28u)=0x00400001u;                                   /* the init hides it (fake) */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==2 && tnk_lv.n==1u && tnk_lv.t[0].obj==TANK && spawn_after_original==(int)n+1);   /* after the AI started */
    CHECK(WORD_AT(TANK+0x28u)==0x00400011u);                           /* shown before its fight, other flags kept */
    CHECK(FLOAT_AT(0x0078B580u)==77.0f && WORD_AT(0x0078B594u)==0x55u && WORD_AT(0x0075F468u)==0x66u && !tnk_lv.act);   /* restored */
    CHECK(FLOAT_AT(0x0051B9C8u+0x10u)==88.0f && tnk_lv.t[0].block[0]==tnk.tabs+0xA8u && tnk_lv.t[0].sounds[0]==0x5002u);
    CHECK(FLOAT_AT(tnk.tabs+0xA8u+0x10u)==2.0f && tnk_lv.t[0].block[3]==0x0051BB44u);   /* its own tables, as the init left them */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* the same placeholder again */
    CHECK(nspawn==2 && tnk_lv.n==1u);
    tnk_enter(1u);                                                    /* tank 1's statics for the hooks below */
    CHECK(spawn_args[0]==0x281u && spawn_args[3]==0 && spawn_args[4]==0);
    CHECK(NEAR(spawn_pos[0],40.0f) && NEAR(spawn_pos[1],2.0f) && NEAR(spawn_pos[2],30.0f));
    CHECK(NEAR(spawn_rot[0],0.0f) && NEAR(spawn_rot[1],1.5f) && NEAR(spawn_rot[2],0.0f));
    dx=40.0f+12.05f;dy=2.0f;dz=30.0f-1.5f;
    CHECK(NEAR(FLOAT_AT(0x0078B580u),-9.0f+dx) && NEAR(FLOAT_AT(0x0078B584u),2.0f) && NEAR(FLOAT_AT(0x0078B588u),-3.2f+dz));
    actor(2,0x12,0.0f,0.0f,0.0f,0.0f);
    WORD_AT(0x0079094Cu)=102u;                                        /* not in the editor */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(nspawn==2);
    play_scene();

    /* ---- text: the bar labels ---------------------------------------------- */
    {
        uint32_t s=CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0);
        CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Castle Tank"));
        s=CALL(pc_mapmaker_tank_text,fake,0x0014CD83u,0x1635u,0,0,0);
        CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Tank exposed"));
    }
    orig_eax=0x4321u;
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x00300000u,0x165Bu,0,0,1)==0x4321u);   /* other caller */
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD83u,0x165Bu,0,0,1)==0x4321u);
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x1636u,0,0,1)==0x4321u);
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x1635u,0,0,1)==0x4321u);   /* timer id, bar call */
    BYTE_AT(0x007DE455u)=3u;
    CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,1)==0x4321u);
    play_scene();

    /* ---- update: the placeholder leaves on the tank's first frame ----------- */
    WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,0x00570000u,0,0,1);   /* another object */
    CHECK(nremove==0 && !TK.updates);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1 && remove_arg==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0 && TK.updates==1u);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1 && TK.updates==1u);

    /* ---- draw: log only, each tank's first draw ------------------------------- */
    CALL(pc_mapmaker_tank_draw,fake,0x000A2B3Cu,0x00570000u,0,0,1);    /* another prop */
    CHECK(!TK.drawn);
    CALL(pc_mapmaker_tank_draw,fake,0x000A2B3Cu,TANK,0,0,1);
    CHECK(TK.drawn==1u);
    CALL(pc_mapmaker_tank_draw,fake,0x000A2B3Cu,TANK,0,0,1);
    CHECK(TK.drawn==1u && WORD_AT(TANK+0x28u)==0x00400011u);

    /* ---- wake ---------------------------------------------------------------- */
    players(100.0f,100.0f,1.0f,40.0f+20.0f,30.0f+20.0f,0.0f);           /* far; near but dead */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==0 && TK.waited==1u);   /* logged once */
    players(100.0f,100.0f,1.0f,40.0f+20.0f,30.0f+20.0f,0.5f);           /* 28.3 m, alive */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);
    players(40.0f+22.0f,30.0f+22.0f,1.0f,100.0f,100.0f,1.0f);           /* 31.1 m */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==0);
    players(40.0f+29.0f,30.0f,1.0f,100.0f,100.0f,1.0f);                 /* x only */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);
    players(40.0f,30.0f-29.0f,1.0f,100.0f,100.0f,1.0f);                 /* z only */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);

    /* ---- area: the castle courtyard around the spawn ------------------------ */
    g_ecx=TANK+0x64u;
    CHECK(CALL(pc_mapmaker_tank_area,fake,0x00255697u,0,0,0,0)==1u);   /* the spawn: castle centre */
    CHECK(AREA(-12.05f+dx,1.5f+dz)==1u && AREA(-12.05f,1.5f)==0);       /* world frame: moved by -d */
    CHECK(AREA(-14.0f+dx,-8.0f+dz)==1u && AREA(-16.0f+dx,-8.0f+dz)==0); /* first rectangle: x -15..-3 */
    CHECK(AREA(-20.0f+dx,10.0f+dz)==1u && AREA(-20.0f+dx,5.0f+dz)==0);  /* the third reaches x -21.8 */
    CHECK(AREA(-2.0f+dx,0.0f+dz)==0 && AREA(-12.0f+dx,14.0f+dz)==0 && AREA(-12.0f+dx,-11.0f+dz)==0);
    tnk_lv.shifted=1;                                                  /* inside a castle-frame call */
    CHECK(AREA(-12.05f,1.5f)==1u && AREA(-12.05f+dx,1.5f+dz)==0);
    CHECK(AREA(-3.0f,0.0f)==0 && AREA(-3.01f,0.0f)==1u && AREA(-17.5f,0.0f)==0 && AREA(-17.49f,0.0f)==1u);   /* strictly inside */
    CHECK(AREA(-10.0f,13.0f)==0 && AREA(-10.0f,12.99f)==1u && AREA(-10.0f,-10.0f)==0 && AREA(-10.0f,-9.99f)==1u);
    FLOAT_AT(0x0051BB58u+4u)=-6.8f;FLOAT_AT(0x0051BB58u+12u)=-10.0f;  /* z given high to low */
    CHECK(AREA(-14.0f,-8.0f)==1u);
    FLOAT_AT(0x0051BB58u+20u)=50.0f;                                   /* the table is read, not copied */
    CHECK(AREA(-10.0f,40.0f)==1u);
    stock_yard();
    tnk_lv.shifted=0;
    /* the drive target search (only its call site): every point counts after 64 outside */
    TK.misses=0;
    for(k=1;k<64u;k++) CHECK(area_at(-60.0f,0.0f,0x00254848u,__LINE__)==0);
    CHECK(area_at(-12.0f+dx,0.0f+dz,0x00254848u,__LINE__)==1u && TK.misses==63u);   /* inside: not a miss */
    n=tnk_lv.logs;
    CHECK(area_at(-60.0f,0.0f,0x00254848u,__LINE__)==1u && tnk_lv.logs==n+1u);         /* the 64th: logged once */
    CHECK(area_at(-60.0f,0.0f,0x00254848u,__LINE__)==1u && tnk_lv.logs==n+1u);
    for(k=0;k<100u;k++) CHECK(AREA(-60.0f,0.0f)==0);                    /* other callers: never forced */
    g_edi=TANK;target_writes=0;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);    /* a new search */
    target_writes=1;
    CHECK(TK.misses==0 && area_at(-60.0f,0.0f,0x00254848u,__LINE__)==0);
    TK.misses=70u;
    g_edi=0x00570000u;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);    /* not our tank */
    CHECK(TK.misses==70u);
    TK.misses=0;

    /* ---- fight-state lines: at frame start, drive target and state entry ------ */
    TK.seen_state=0;tnk_lv.logs=0;
    WORD_AT(0x0078B594u)=1u;
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,0x00570000u,0,0,1);   /* another object */
    CHECK(TK.seen_state==0 && tnk_lv.logs==0);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(TK.seen_state==1u && tnk_lv.logs==1u);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(tnk_lv.logs==1u);                                            /* unchanged: no line */
    WORD_AT(0x0078B594u)=6u;g_edi=TANK;target_writes=0;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(TK.seen_state==6u && tnk_lv.logs==2u);
    WORD_AT(0x0078B594u)=3u;WORD_AT(0x0078B598u)=3u;pre_eax=TANK;
    CALL(pc_mapmaker_tank_state,fake,0x00255BD5u,0,0,0,1);
    CHECK(TK.seen_state==3u && tnk_lv.logs==3u);
    pre_eax=0x00570000u;WORD_AT(0x0078B594u)=4u;
    CALL(pc_mapmaker_tank_state,fake,0x00255BD5u,0,0,0,1);           /* not our tank */
    CHECK(TK.seen_state==3u);
    for(k=0;k<60u;k++) { WORD_AT(0x0078B594u)=k&1u?3u:4u;CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1); }
    CHECK(tnk_lv.logs==49u && TK.seen_state==3u);                 /* 48 lines, then one "used up" */
    target_writes=1;tnk_lv.logs=0;

    /* ---- target: castle frame ------------------------------------------------- */
    actor(3,0x0C,45.0f,2.5f,33.0f,0.0f);
    WORD_AT(0x0078B5B0u)=ENT(3);
    FLOAT_AT(TANK+0x64u)=41.0f;FLOAT_AT(TANK+0x68u)=2.0f;FLOAT_AT(TANK+0x6Cu)=31.0f;
    FLOAT_AT(0x0078B580u)=50.0f;FLOAT_AT(0x0078B584u)=2.0f;FLOAT_AT(0x0078B588u)=35.0f;
    g_edi=TANK;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(NEAR(seen_tank[0],41.0f-dx) && NEAR(seen_tank[1],2.0f-dy) && NEAR(seen_tank[2],31.0f-dz));
    CHECK(NEAR(seen_target[0],45.0f-dx) && NEAR(seen_target[1],2.5f-dy) && NEAR(seen_target[2],33.0f-dz));
    CHECK(NEAR(seen_goal[0],50.0f-dx) && NEAR(seen_goal[2],35.0f-dz));
    CHECK(NEAR(FLOAT_AT(0x0078B580u),10.0f+dx) && NEAR(FLOAT_AT(0x0078B584u),0.5f+dy) && NEAR(FLOAT_AT(0x0078B588u),-2.0f+dz));
    CHECK(FLOAT_AT(TANK+0x64u)==41.0f && FLOAT_AT(TANK+0x68u)==2.0f && FLOAT_AT(TANK+0x6Cu)==31.0f);   /* exactly */
    CHECK(FLOAT_AT(ENT(3)+0x64u)==45.0f && FLOAT_AT(ENT(3)+0x68u)==2.5f && FLOAT_AT(ENT(3)+0x6Cu)==33.0f);
    target_writes=0;
    FLOAT_AT(0x0078B580u)=50.3f;FLOAT_AT(0x0078B584u)=2.1f;FLOAT_AT(0x0078B588u)=35.7f;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(FLOAT_AT(0x0078B580u)==50.3f && FLOAT_AT(0x0078B584u)==2.1f && FLOAT_AT(0x0078B588u)==35.7f);   /* bit for bit */
    /* values that do not survive (v - d) + d: restored, not recomputed */
    FLOAT_AT(TANK+0x64u)=0.1f;FLOAT_AT(TANK+0x68u)=0.1f;FLOAT_AT(TANK+0x6Cu)=2.3f;
    FLOAT_AT(0x0078B580u)=2.1f;FLOAT_AT(0x0078B584u)=0.1f;FLOAT_AT(0x0078B588u)=2.3f;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(FLOAT_AT(TANK+0x64u)==0.1f && FLOAT_AT(TANK+0x68u)==0.1f && FLOAT_AT(TANK+0x6Cu)==2.3f);
    CHECK(FLOAT_AT(0x0078B580u)==2.1f && FLOAT_AT(0x0078B584u)==0.1f && FLOAT_AT(0x0078B588u)==2.3f);
    FLOAT_AT(TANK+0x64u)=41.0f;FLOAT_AT(TANK+0x68u)=2.0f;FLOAT_AT(TANK+0x6Cu)=31.0f;
    target_writes=1;
    WORD_AT(0x0078B5B0u)=0;                                            /* no target: tank only */
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(NEAR(seen_tank[0],41.0f-dx) && NEAR(FLOAT_AT(0x0078B580u),10.0f+dx));
    WORD_AT(0x0078B5B0u)=ENT(3);
    g_edi=0x00570000u;FLOAT_AT(0x00570000u+0x64u)=41.0f;                                              /* not our tank */
    FLOAT_AT(0x0078B580u)=50.0f;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(NEAR(seen_tank[0],41.0f) && NEAR(FLOAT_AT(0x0078B580u),10.0f));

    /* ---- state 7: aim points move with the arena -------------------------------- */
    for(k=0;k<5u;k++) FLOAT_AT(GUNS+k*0x4Cu+0x10u)=1.0f;
    WORD_AT(0x0078B594u)=3u;WORD_AT(0x0078B598u)=1u;
    pre_eax=TANK;
    CALL(pc_mapmaker_tank_state,fake_state,0x00255BD5u,0,0,0,2);     /* state 3: unshifted */
    CHECK(NEAR(FLOAT_AT(GUNS+0x10u),-5.3f) && NEAR(seen_tank[0],41.0f-dx));   /* inner call shifted alone */
    CHECK(NEAR(FLOAT_AT(0x0078B580u),10.0f+dx));
    WORD_AT(0x0078B594u)=7u;WORD_AT(0x0078B598u)=3u;
    pre_eax=TANK;
    CALL(pc_mapmaker_tank_state,fake_state,0x00255BD5u,0,0,0,2);
    for(k=0;k<5u;k++) CHECK(NEAR(FLOAT_AT(GUNS+k*0x4Cu+0x10u),-5.3f+dx) && NEAR(FLOAT_AT(GUNS+k*0x4Cu+0x14u),-1.4f+dy) &&
                            NEAR(FLOAT_AT(GUNS+k*0x4Cu+0x18u),19.9f+dz));
    CHECK(NEAR(seen_tank[0],41.0f-dx) && NEAR(seen_target[0],45.0f-dx));
    CHECK(NEAR(FLOAT_AT(0x0078B580u),10.0f+dx) && NEAR(FLOAT_AT(0x0078B588u),-2.0f+dz));     /* shifted once */
    CHECK(FLOAT_AT(TANK+0x64u)==41.0f && FLOAT_AT(ENT(3)+0x64u)==45.0f && !tnk_lv.shifted);
    WORD_AT(0x0078B598u)=7u;                                           /* no change: stock does nothing */
    pre_eax=TANK;
    CALL(pc_mapmaker_tank_state,fake,0x00255BD5u,0,0,0,1);
    CHECK(NEAR(FLOAT_AT(GUNS+0x10u),-5.3f+dx));

    /* ---- gun: the cannon tracks a player target in state 4 ------------------- */
    players(44.0f,31.0f,1.0f,47.0f,36.0f,1.0f);
    WORD_AT(PENT(0)+0x7Cu)=0x00590000u;WORD_AT(0x00590000u+8u)=0;
    WORD_AT(PENT(1)+0x7Cu)=0x00590100u;WORD_AT(0x00590100u+8u)=1u;
    FLOAT_AT(PLAYERS+0xB4u)=44.5f;FLOAT_AT(PLAYERS+0xB8u)=3.6f;FLOAT_AT(PLAYERS+0xBCu)=31.5f;
    FLOAT_AT(PLAYERS+0x1584u+0xB4u)=47.5f;FLOAT_AT(PLAYERS+0x1584u+0xB8u)=3.7f;FLOAT_AT(PLAYERS+0x1584u+0xBCu)=36.5f;
    cannon(1.0f,2.0f,3.0f);                                           /* stock: the actor's +0xBB4 */
    WORD_AT(0x0078B594u)=4u;WORD_AT(0x0078B5B0u)=PENT(1);
    g_edi=TANK;g_ebx=4u;n=TK.aims;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(gun_seen[0]==47.5f && gun_seen[1]==3.7f && gun_seen[2]==36.5f);          /* set before the turn */
    CHECK(FLOAT_AT(CANNON)==47.5f && FLOAT_AT(CANNON+8u)==36.5f && TK.aims==n+1u);  /* the new stock aim logged */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(TK.aims==n+1u);                                          /* our own point: no line */
    FLOAT_AT(PLAYERS+0x1584u+0xB4u)=48.0f;                             /* the player moves: tracked */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(gun_seen[0]==48.0f && FLOAT_AT(CANNON)==48.0f && TK.aims==n+1u);
    cannon(9.0f,2.0f,3.0f);                                            /* stock re-aims after a shot */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==48.0f && TK.aims==n+2u);
    WORD_AT(0x0078B5B0u)=PENT(0);cannon(9.0f,2.0f,3.0f);              /* the first player */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(gun_seen[0]==44.5f && gun_seen[1]==3.6f && gun_seen[2]==31.5f);
    WORD_AT(0x0078B5B0u)=PENT(1);
    TK.aims=TNK_AIM_LOGS;cannon(9.0f,2.0f,3.0f);                  /* budget used: still tracked */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==48.0f && TK.aims==TNK_AIM_LOGS);
    /* left to stock: other guns, other states, another object, AI targets, no target, the castle */
    cannon(9.0f,2.0f,3.0f);g_ebx=3u;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==9.0f);
    g_ebx=4u;WORD_AT(0x0078B594u)=3u;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    WORD_AT(0x0078B594u)=7u;                                           /* state 7 drives at the cannon's point */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==9.0f);
    WORD_AT(0x0078B594u)=4u;g_edi=0x00570000u;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==9.0f);
    g_edi=TANK;WORD_AT(0x00590100u+8u)=2u;                             /* an AI: index past the players */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    WORD_AT(0x00590100u+8u)=0xFFFFFFFFu;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    WORD_AT(0x00590100u+8u)=1u;WORD_AT(PENT(1)+0x7Cu)=0;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    WORD_AT(PENT(1)+0x7Cu)=0x00590100u;WORD_AT(0x0078B5B0u)=0;
    WORD_AT(0x7Cu)=0x00590000u;                                       /* what a null target would read */
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    WORD_AT(0x7Cu)=0;
    CHECK(FLOAT_AT(CANNON)==9.0f);
    WORD_AT(0x0078B5B0u)=PENT(1);WORD_AT(0x0079094Cu)=4u;
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==9.0f);
    play_scene();
    CALL(pc_mapmaker_tank_gun,fake_gun,0x00255899u,0,0,0,1);
    CHECK(FLOAT_AT(CANNON)==48.0f);
    WORD_AT(0x0078B5B0u)=ENT(3);

    /* ---- event, wreck ------------------------------------------------------------ */
    CALL(pc_mapmaker_tank_event,fake,0x00255A94u,0x16u,0,0,0);
    CHECK(tnk_lv.events==1);
    CALL(pc_mapmaker_tank_event,fake,0x0029E068u,3u,0,0,1);          /* someone else's */
    WORD_AT(0x0079094Cu)=4u;
    CALL(pc_mapmaker_tank_event,fake,0x00255A94u,0x16u,0,0,1);       /* the castle keeps it */
    play_scene();
    g_esi=TANK;
    CALL(pc_mapmaker_tank_wreck,fake_wreck,0x00255E78u,0,0,0,1);
    CHECK(NEAR(FLOAT_AT(TANK+0x64u),40.0f) && NEAR(FLOAT_AT(TANK+0x68u),2.0f-50.0f) && NEAR(FLOAT_AT(TANK+0x6Cu),30.0f));
    g_esi=0x00570000u;FLOAT_AT(0x00570000u+0x68u)=7.0f;
    CALL(pc_mapmaker_tank_wreck,fake_wreck,0x00255E78u,0,0,0,1);
    CHECK(FLOAT_AT(0x00570000u+0x68u)==20.0f);

    /* ---- the castle (scene 4) and a new level ------------------------------------ */
    WORD_AT(0x0079094Cu)=4u;
    CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,1);
    g_ecx=TANK+0x64u;
    CALL(pc_mapmaker_tank_area,fake,0x00255697u,0,0,0,1);
    g_edi=TANK;FLOAT_AT(TANK+0x64u)=41.0f;
    CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
    CHECK(NEAR(seen_tank[0],41.0f));
    play_scene();
    WORD_AT(0x006D7C4Cu)=1u;                                           /* the original does nothing */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(tnk_lv.n==1u && tnk_lv.t[0].obj==TANK);
    WORD_AT(0x006D7C4Cu)=0;
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);           /* next level: no tank */
    CHECK(!tnk_lv.n && !tnk_lv.act);
    CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,1);
    g_ecx=TANK+0x64u;
    CALL(pc_mapmaker_tank_area,fake,0x00255697u,0,0,0,1);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1);

    /* a full level again: one tank, its placeholder removed once */
    level_with_tank(__LINE__);
    CHECK(tnk_lv.n==1u && tnk_lv.t[0].obj==TANK && nspawn==3);
    WORD_AT(A(0)+0xBF0u)=REC(5);                                      /* the pool entry was reused */
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1);
    CHECK(ai_originals>0);

    /* ---- diagnostics: the placeholder's character and its spawn --------------------- */
    if(tnk_lv.act) tnk_leave(tnk_lv.act,0);
    play_scene();memset(&tnk_lv,0,sizeof tnk_lv);
    orig_eax=0x00450000u;
    CALL(pc_mapmaker_tank_create,fake,0x000DAD80u,0x12u,0,3u,1);
    CHECK(tnk_lv.created==1u && tnk_lv.logs==1u);
    CALL(pc_mapmaker_tank_create,fake,0x000DAD80u,0x13u,0,3u,1);     /* another character */
    orig_eax=0;
    CALL(pc_mapmaker_tank_create,fake,0x000DAD80u,0x12u,0,4u,1);     /* failed: logged too */
    CHECK(tnk_lv.created==2u && tnk_lv.logs==2u);
    WORD_AT(0x0079094Cu)=4u;
    CALL(pc_mapmaker_tank_create,fake,0x000DAD80u,0x12u,0,3u,1);
    CHECK(tnk_lv.created==2u);
    play_scene();
    actor(6,0x12,1.0f,2.0f,3.0f,0.0f);WORD_AT(REC(6)+0xCu)=0x77u;
    actor(7,0x13,1.0f,2.0f,3.0f,0.0f);
    n=tnk_lv.logs;
    CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(7),0,0,1);      /* not a tank placeholder */
    CHECK(!tnk_lv.nseen);
    CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(6),0,0,1);      /* waiting: logged once */
    CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(6),0,0,1);
    CHECK(tnk_lv.nseen==1u && tnk_lv.seen[0].actor==A(6) && tnk_lv.seen[0].tries==2u && !tnk_lv.seen[0].started && tnk_lv.logs==n+1u);
    pak_mounted=0;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(6),0,0,1);   /* its AI start (no tank: pak missing) */
    CHECK(tnk_lv.seen[0].started);
    n=tnk_lv.logs;
    CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(6),0,0,1);
    CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(6),0,0,1);
    CHECK(tnk_lv.seen[0].told==2u && tnk_lv.logs==n+1u);              /* "reached" once */
    WORD_AT(A(6))=0;                                                  /* no entity: said so */
    n=tnk_lv.refused;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(6),0,0,1);
    CHECK(tnk_lv.refused==n+1u);
    WORD_AT(A(6))=ENT(6);
    for(k=0;k<9u;k++) { actor(8u+(k&1u),0x12,0,0,0,0);WORD_AT(ENT(8u+(k&1u))+0x7Cu)=A(10u+k);
                        WORD_AT(A(10u+k))=ENT(8u+(k&1u));HALF_AT(A(10u+k)+0x48u)=0x12u;
                        CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(8u+(k&1u)),0,0,1); }
    CHECK(tnk_lv.nseen==8u);                                          /* bounded */
    pak_mounted=1;orig_eax=0;

    if(tnk_lv.act) tnk_leave(tnk_lv.act,0);
    WORD_AT(0x006D7880u)=10u;pak_mounted=0;
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);           /* a new level */
    CHECK(!tnk_lv.n && !tnk_lv.act);
    resources(1,1,1,0x002560C0u);READY();
    {
        static uint32_t b0[0x19],s0[0x1C],t0[0x224/4];
        unsigned s=nspawn,r=nremove;
        float sx;
        for(k=0;k<0x19u;k++) WORD_AT(0x0078B560u+k*4u)=0xA0A0A000u+k;
        for(k=0;k<0x1Cu;k++) WORD_AT(0x0075F468u+k*4u)=0xB0B0B000u+k;
        for(k=0;k<0x89u;k++) WORD_AT(0x0051B920u+k*4u)=0xC0C0C000u+k;
        memcpy(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0);
        memcpy(s0,(void *)((uintptr_t)0x0075F468u+g_xbox_mem_offset),sizeof s0);
        memcpy(t0,(void *)((uintptr_t)0x0051B920u+g_xbox_mem_offset),sizeof t0);
        for(k=0;k<5u;k++) {
            actor(4u+k,0x12,10.0f*(float)k,1.0f,5.0f,0.0f);
            spawn_obj=0x00440000u+k*0x1000u;
            CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(4u+k),0,0,1);
        }
        spawn_obj=TANK;
        CHECK(tnk_lv.n==4u && nspawn==s+4u && !tnk_lv.act);          /* the fifth stays a bot */
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0));
        CHECK(!memcmp(s0,(void *)((uintptr_t)0x0075F468u+g_xbox_mem_offset),sizeof s0));
        CHECK(!memcmp(t0,(void *)((uintptr_t)0x0051B920u+g_xbox_mem_offset),sizeof t0));
        for(k=0;k<4u;k++) {
            struct tnk_one *t=&tnk_lv.t[k];
            uint32_t tabs=tnk.tabs+k*0x224u;
            CHECK(t->obj==0x00440000u+k*0x1000u && t->place==A(4u+k) && t->place_ent==ENT(4u+k));
            CHECK(WORD_AT(t->obj+0x28u)==0x11u && !t->drawn);           /* each shown */
            CHECK(t->block[0]==tabs+0xA8u && t->block[1]==tabs+0x78u && t->block[2]==tabs && t->block[3]==0x0051BB44u);
            CHECK(FLOAT_AT(tabs+0xA8u+0x10u)==(float)(s+1u+k) && WORD_AT(tabs+0x68u)==0 && WORD_AT(tabs+4u)==0xC0C0C001u);
            CHECK(t->sounds[0]==0x5000u+s+1u+k);
            CHECK(NEAR(t->d[0],10.0f*(float)k+12.05f) && NEAR(t->d[1],1.0f) && NEAR(t->d[2],3.5f) && NEAR(t->spawn[0],10.0f*(float)k));
            memcpy(&sx,&t->block[8],4);
            CHECK(NEAR(sx,-9.0f+t->d[0]));                              /* its first drive target */
            t->block[13]=20u+k;                                         /* [0x0078B594] */
        }
        CALL(pc_mapmaker_tank_draw,fake,0x000A2B3Cu,0x00442000u,0,0,1);   /* tank 3's first draw */
        CHECK(tnk_lv.t[2].drawn && !tnk_lv.t[0].drawn && !tnk_lv.t[1].drawn && !tnk_lv.t[3].drawn && !tnk_lv.act);
        /* the update: tank 3's statics in, its changes kept, the statics put back */
        entry_set=33u;
        CALL(pc_mapmaker_tank_update,fake_entry,0x00000100u,0x00442000u,0,0,1);
        CHECK(entry_state==22u && entry_guns==tnk.tabs+2u*0x224u+0xA8u);
        CHECK(tnk_lv.t[2].block[13]==33u && tnk_lv.t[2].sounds[0]==33u && !tnk_lv.act);
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0));
        CHECK(!memcmp(s0,(void *)((uintptr_t)0x0075F468u+g_xbox_mem_offset),sizeof s0));
        CHECK(nremove==r+1u && remove_arg==ENT(6) && !tnk_lv.t[2].place && tnk_lv.t[1].place);   /* its own placeholder */
        entry_set=34u;                                                 /* a later frame */
        CALL(pc_mapmaker_tank_update,fake_entry,0x00000100u,0x00442000u,0,0,1);
        CHECK(entry_state==33u && tnk_lv.t[2].block[13]==34u && !tnk_lv.act && nremove==r+1u);
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0));
        /* tank 1 damaged inside tank 2's frame */
        nest_obj=0x00440000u;entry_set=44u;
        CALL(pc_mapmaker_tank_update,fake_entry,0x00000100u,0x00441000u,0,0,2);   /* and the inner damage */
        nest_obj=0;
        CHECK(inner_state==20u && entry_state==21u && tnk_lv.t[0].block[13]==99u && tnk_lv.t[1].block[13]==44u && tnk_lv.t[1].block[14]==0x77u+44u);
        CHECK(nremove==r+2u && remove_arg==ENT(5) && !tnk_lv.act);
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0));
        /* the other entries */
        entry_set=23u;
        CALL(pc_mapmaker_tank_slot7,fake_entry,0x00000100u,0x00443000u,0,0,1);
        CHECK(entry_state==23u && entry_guns==tnk.tabs+3u*0x224u+0xA8u);
        CALL(pc_mapmaker_tank_slot2,fake_entry,0x00000100u,0x00443000u,0,0,1);
        CHECK(entry_state==23u);
        WORD_AT(0x00570400u)=0x00443000u;
        CALL(pc_mapmaker_tank_damage,fake_entry,0x00000100u,0x00570400u,0,0,1);
        CHECK(entry_state==23u);
        WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+0xFF4u)=0x00443000u;
        CALL(pc_mapmaker_tank_bar,fake_entry,0x0014CD2Du,0,0,0,1);
        CHECK(entry_state==23u && !tnk_lv.act);
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0));
        /* not ours: the statics as they are */
        CALL(pc_mapmaker_tank_slot7,fake_entry,0x00000100u,0x00570000u,0,0,1);
        CHECK(entry_state==0xA0A0A00Du);
        WORD_AT(0x0078B594u)=0xA0A0A00Du;WORD_AT(0x0075F468u)=0xB0B0B000u;
        WORD_AT(PLAYER+0xFF4u)=ENT(3);
        CALL(pc_mapmaker_tank_bar,fake_entry,0x0014CD2Du,0,0,0,1);
        CHECK(entry_state==0xA0A0A00Du);
        WORD_AT(0x0078B594u)=0xA0A0A00Du;WORD_AT(0x0075F468u)=0xB0B0B000u;
        WORD_AT(0x00570400u)=0x00444000u;                            /* the fifth placeholder made no tank */
        CALL(pc_mapmaker_tank_damage,fake_entry,0x00000100u,0x00570400u,0,0,1);
        CHECK(entry_state==0xA0A0A00Du);
        WORD_AT(0x0078B594u)=0xA0A0A00Du;WORD_AT(0x0075F468u)=0xB0B0B000u;
        /* the inner hooks act on the active tank only */
        tnk_enter(2u);
        CHECK(WORD_AT(0x0078B594u)==44u && tnk_lv.act==2u);
        CHECK(AREA(-12.05f+tnk_lv.t[1].d[0],1.5f+tnk_lv.t[1].d[2])==1u && AREA(-12.05f+tnk_lv.t[0].d[0]-9.0f,1.5f+tnk_lv.t[0].d[2])==0);
        FLOAT_AT(0x00441000u+0x64u)=10.0f;FLOAT_AT(0x00441000u+0x6Cu)=5.0f;
        g_edi=0x00440000u;WORD_AT(0x0078B5B0u)=0;FLOAT_AT(0x00440000u+0x64u)=3.0f;
        CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);    /* tank 1 while tank 2 runs: stock */
        CHECK(seen_tank[0]==3.0f);
        g_edi=0x00441000u;
        CALL(pc_mapmaker_tank_target,fake_target,0x002559CBu,0,0,0,1);
        CHECK(NEAR(seen_tank[0],10.0f-tnk_lv.t[1].d[0]));
        g_esi=0x00441000u;
        CALL(pc_mapmaker_tank_wreck,fake_wreck,0x00255E78u,0,0,0,1);
        CHECK(NEAR(FLOAT_AT(0x00441000u+0x64u),10.0f) && NEAR(FLOAT_AT(0x00441000u+0x68u),1.0f-50.0f) && NEAR(FLOAT_AT(0x00441000u+0x6Cu),5.0f));
        g_esi=0x00442000u;FLOAT_AT(0x00442000u+0x68u)=7.0f;
        CALL(pc_mapmaker_tank_wreck,fake_wreck,0x00255E78u,0,0,0,1);    /* not the active one */
        CHECK(FLOAT_AT(0x00442000u+0x68u)==20.0f);
        tnk_leave(2u,0);
        CHECK(!memcmp(b0,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof b0) && !tnk_lv.act);
        WORD_AT(0x0079094Cu)=4u;                                       /* the castle: no swaps */
        CALL(pc_mapmaker_tank_slot7,fake_entry,0x00000100u,0x00443000u,0,0,1);
        CHECK(entry_state==0xA0A0A00Du);
        play_scene();
    }

    /* ---- v0.4.0-mapmaker.28: a removed placeholder's entity and pool entry are
     * reused by the next placeholder; 3 and 4 tanks, every refusal logged ---- */
    if(tnk_lv.act) tnk_leave(tnk_lv.act,0);
    WORD_AT(0x006D7880u)=10u;pak_mounted=0;bar_extra=0;bar_adds=0;
    tnk_lv.logged=99u;
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);           /* a new level */
    CHECK(!tnk_lv.n && bar_adds==1u && bar_extra==tnk_bar_boss);    /* the bosses area's bars */
    CHECK(tnk_lv.logged==1u);                                         /* general lines: per level */
    resources(1,1,1,0x002560C0u);READY();
    {
        unsigned s=nspawn,r=nremove,refused;
        uint32_t ent=0,id=0,b1,b3;
        story_names[1]="Iron Duke";story_names[2]=0;story_names[3]="Third Tank";story_names[4]="Fourth";
        /* tanks 1 and 2: actors 20/21, records 20/21 (ids 0x101, 0x102) */
        actor(20,0x12,10.0f,0.5f,10.0f,0.0f);WORD_AT(REC(20)+8u)=0x101u;
        spawn_obj=0x00450000u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);
        actor(21,0x12,30.0f,0.5f,10.0f,0.0f);WORD_AT(REC(21)+8u)=0x102u;
        spawn_obj=0x00451000u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(21),0,0,1);
        CHECK(tnk_lv.n==2u && nspawn==s+2u && tnk_lv.t[0].place_id==0x101u && tnk_lv.t[1].place_id==0x102u);
        CHECK(bar_extra(0,&ent,&id)==1 && ent==0 && id==0x101u);     /* its AI still stands there: no prop bar */
        WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+0xFF4u)=0;
        CALL(pc_mapmaker_tank_update,fake,0x00000100u,0x00450000u,0,0,1);   /* tank 1's placeholder leaves */
        CHECK(nremove==r+1u && remove_arg==ENT(20) && !tnk_lv.t[0].place);
        CHECK(bar_extra(0,&ent,&id)==1 && ent==0x00450000u && id==0x101u);
        CHECK(bar_extra(1,&ent,&id)==1 && ent==0 && id==0x102u && bar_extra(2,&ent,&id)==0);
        /* the same Story AI again (its record and id): no second tank, logged */
        refused=tnk_lv.refused;
        actor(22,0x12,10.0f,0.5f,10.0f,0.0f);WORD_AT(A(22)+0xBF0u)=REC(20);
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(22),0,0,1);
        CHECK(tnk_lv.n==2u && nspawn==s+2u && tnk_lv.refused==refused+1u);
        /* tank 2's placeholder again (not removed yet): no second tank, logged */
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(21),0,0,1);
        CHECK(tnk_lv.n==2u && nspawn==s+2u && tnk_lv.refused==refused+2u);
        WORD_AT(A(21)+0xBF0u)=0;                                       /* no record: the entity still decides */
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(21),0,0,1);
        CHECK(tnk_lv.n==2u && nspawn==s+2u);
        WORD_AT(A(21)+0xBF0u)=REC(21);refused=tnk_lv.refused-2u;
        /* the third placeholder gets tank 1's pool entry and entity, its own record (id 0x103) */
        actor(20,0x12,50.0f,0.5f,10.0f,0.0f);WORD_AT(A(20)+0xBF0u)=REC(23);WORD_AT(REC(23)+8u)=0x103u;
        n=tnk_lv.logs;
        CALL(pc_mapmaker_tank_spawn,fake,0x0027F982u,ENT(20),0,0,1);      /* a fresh diagnostics record */
        CHECK(tnk_lv.seen[0].actor==A(20) && tnk_lv.seen[0].rec==REC(23) && tnk_lv.seen[0].id==0x103u && !tnk_lv.seen[0].started);
        CHECK(tnk_lv.seen[0].tries==1u && tnk_lv.logs==n+1u);            /* its "waiting" line */
        spawn_obj=0x00452000u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);
        CHECK(tnk_lv.n==3u && nspawn==s+3u && tnk_lv.t[2].obj==0x00452000u && tnk_lv.t[2].place_ent==ENT(20) && tnk_lv.t[2].place_id==0x103u);
        CHECK(tnk_lv.seen[0].started && tnk_lv.refused==refused+2u);
        CHECK(NEAR(tnk_lv.t[2].spawn[0],50.0f) && !tnk_lv.act);
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);  /* started again */
        CHECK(tnk_lv.n==3u && tnk_lv.refused==refused+3u);
        CALL(pc_mapmaker_tank_update,fake,0x00000100u,0x00452000u,0,0,1); /* tank 3 removes its own placeholder */
        CHECK(nremove==r+2u && remove_arg==ENT(20) && !tnk_lv.t[2].place);
        /* the fourth gets tank 2's pool entry, entity and record (the record rewritten: id 0x104) */
        CALL(pc_mapmaker_tank_update,fake,0x00000100u,0x00451000u,0,0,1);
        CHECK(nremove==r+3u && remove_arg==ENT(21));
        actor(21,0x12,70.0f,0.5f,10.0f,0.0f);WORD_AT(REC(21)+8u)=0x104u;
        spawn_obj=0x00453000u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(21),0,0,1);
        CHECK(tnk_lv.n==4u && nspawn==s+4u && tnk_lv.t[3].obj==0x00453000u && tnk_lv.t[3].place_id==0x104u);
        /* a fifth: at most 4, logged */
        actor(24,0x12,90.0f,0.5f,10.0f,0.0f);WORD_AT(REC(24)+8u)=0x105u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(24),0,0,1);
        CHECK(tnk_lv.n==4u && nspawn==s+4u && tnk_lv.refused==refused+4u);
        spawn_obj=TANK;
        /* the bosses area's bars: each tank (placeholder gone) with its own record id */
        CHECK(bar_extra(2,&ent,&id)==1 && ent==0x00452000u && id==0x103u);
        CHECK(bar_extra(3,&ent,&id)==1 && ent==0 && id==0x104u && bar_extra(4,&ent,&id)==0);
        WORD_AT(0x0079094Cu)=4u;CHECK(bar_extra(0,&ent,&id)==0);play_scene();
        HALF_AT(0x00452000u+0x94u)=0x298u;                             /* no longer the tank prop: no bar */
        CHECK(bar_extra(2,&ent,&id)==1 && ent==0);
        HALF_AT(0x00452000u+0x94u)=0x281u;
        /* the stock bar's label: the focus tank's Story AI name */
        WORD_AT(PLAYER+0xFF4u)=0x00450000u;
        b1=CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0);
        WORD_AT(PLAYER+0xFF4u)=0x00452000u;
        b3=CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0);
        CHECK(b1 && b3 && b1!=b3 && !strcmp((const char *)((uintptr_t)b1+g_xbox_mem_offset),"Iron Duke"));
        CHECK(!strcmp((const char *)((uintptr_t)b3+g_xbox_mem_offset),"Third Tank"));
        WORD_AT(PLAYER+0xFF4u)=0x00451000u;                            /* tank 2: its AI is unnamed */
        CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0)==tnk.label);
        WORD_AT(PLAYER+0xFF4u)=ENT(3);                                 /* not a tank */
        CHECK(CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0)==tnk.label);
        story_names[3]="A name that is longer than thirty-one characters";
        WORD_AT(PLAYER+0xFF4u)=0x00452000u;
        b3=CALL(pc_mapmaker_tank_text,fake,0x0014CD42u,0x165Bu,0,0,0);
        CHECK(strlen((const char *)((uintptr_t)b3+g_xbox_mem_offset))==31u && !strcmp((const char *)((uintptr_t)b1+g_xbox_mem_offset),"Iron Duke"));
        WORD_AT(PLAYER+0xFF4u)=0;
        /* refusals have their own bounded budget */
        for(k=0;k<20u;k++) CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(24),0,0,1);
        CHECK(tnk_lv.refused==TNK_REFUSE_LOGS);
    }

    /* ---- the pause menu's restart (sub_001CDD60, no pak mount, no level init):
     * the placeholders start again in the same pool entries with the same
     * spawn records and each becomes a tank again ---------------------------- */
    {
        static uint32_t live[TNK_BLOCK_WORDS];
        unsigned s=nspawn,m=nmount,q=nrequest;
        int requested=tnk_lv.requested=1;                              /* the level init requested the models */
        actor(20,0x12,10.0f,0.5f,10.0f,0.0f);WORD_AT(A(20)+0xBF0u)=REC(20);WORD_AT(REC(20)+8u)=0x101u;
        actor(21,0x12,70.0f,0.5f,10.0f,0.0f);WORD_AT(REC(21)+8u)=0x104u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);  /* the run before still known: no tank */
        CHECK(tnk_lv.n==4u && nspawn==s);
        memcpy(live,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof live);
        tnk_enter(2u);                                                  /* a tank's statics in at the restart */
        restart_saw=0;
        CALL(pc_mapmaker_tank_restart,fake_restart,0x0003D4E6u,0,0,0,1);
        CHECK(restart_saw==1u);                                         /* forgotten before the original */
        CHECK(!tnk_lv.n && !tnk_lv.act && !tnk_lv.nseen && !tnk_lv.refused && !tnk_lv.logs && tnk_lv.requested==requested);
        CHECK(!memcmp(live,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof live));   /* the outside statics back */
        CHECK(nmount==m && nrequest==q);                                /* nothing mounted or requested again */
        spawn_obj=0x00450000u;                                          /* the props come back at the same addresses */
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);
        CHECK(tnk_lv.n==1u && nspawn==s+1u && tnk_lv.t[0].obj==0x00450000u && tnk_lv.t[0].place==A(20) &&
              tnk_lv.t[0].place_rec==REC(20) && tnk_lv.t[0].place_id==0x101u && NEAR(tnk_lv.t[0].spawn[0],10.0f));
        spawn_obj=0x00451000u;
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(21),0,0,1);
        CHECK(tnk_lv.n==2u && nspawn==s+2u && tnk_lv.t[1].place_rec==REC(21) && tnk_lv.t[1].place_id==0x104u);
        CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(20),0,0,1);  /* started again in the same run: no tank */
        CHECK(tnk_lv.n==2u && nspawn==s+2u);
        spawn_obj=TANK;
        set_env("TSFP_MAPMAKER_LIMITS",NULL);
        CALL(pc_mapmaker_tank_restart,fake,0x0003D4E6u,0,0,0,1);        /* off: the original only */
        CHECK(tnk_lv.n==2u);
        set_env("TSFP_MAPMAKER_LIMITS","1");
    }

    /* ---- .55: TNT on a stunned tank (sub_00228B50 from sub_000F0430 0x000F049A:
     * stock calls the castle's sub_00256040 only in scene 4, for 'KNAT') ---- */
    {
        static uint32_t live[TNK_BLOCK_WORDS];
        uint32_t t1=0x00450000u,t2=0x00451000u,xp=0x00571000u;
        unsigned m=nmine;
        if(tnk_lv.act) tnk_leave(tnk_lv.act,0);
        CHECK(tnk_lv.n==2u && tnk_lv.t[0].obj==t1 && tnk_lv.t[1].obj==t2);
        play_scene();
        memcpy(live,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof live);
        tnk_lv.t[0].block[13]=3u;tnk_lv.t[1].block[13]=9u;             /* tank 2 stunned by its weak point */
        FLOAT_AT(t1+0x1CCu)=30.0f;FLOAT_AT(t2+0x1CCu)=30.0f;
        WORD_AT(xp+0x158u)=t2;WORD_AT(xp+0x15Cu)=16u;                  /* TNT stuck on its part 16 */
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CHECK(nmine==m+1u && mine_args[0]==t2 && mine_args[1]==xp);    /* the castle's call, with tank 2's statics */
        CHECK(mine_state==9u && mine_guns==tnk_lv.t[1].block[0]);
        CHECK(NEAR(FLOAT_AT(t2+0x1CCu),19.9f) && FLOAT_AT(t1+0x1CCu)==30.0f && tnk_lv.t[1].tnt_hurt==1u && !tnk_lv.t[1].tnt_not);
        CHECK(!tnk_lv.act && !memcmp(live,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof live));
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);          /* a second charge */
        CHECK(nmine==m+2u && NEAR(FLOAT_AT(t2+0x1CCu),9.8f) && tnk_lv.t[1].tnt_hurt==2u);
        /* not stunned, or not on parts 15..17: stock refuses, logged once */
        WORD_AT(xp+0x158u)=t1;
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CHECK(nmine==m+3u && mine_args[0]==t1 && mine_state==3u && FLOAT_AT(t1+0x1CCu)==30.0f && tnk_lv.t[0].tnt_not==1u && !tnk_lv.t[0].tnt_hurt);
        WORD_AT(xp+0x158u)=t2;WORD_AT(xp+0x15Cu)=0xFFFFFFFFu;
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CHECK(nmine==m+4u && NEAR(FLOAT_AT(t2+0x1CCu),9.8f) && tnk_lv.t[1].tnt_not==1u);
        /* the killing charge: its death state is kept by tank 2 */
        WORD_AT(xp+0x15Cu)=17u;
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CHECK(nmine==m+5u && tnk_lv.t[1].block[13]==10u && tnk_lv.t[0].block[13]==3u && !tnk_lv.act);
        CHECK(!memcmp(live,(void *)((uintptr_t)0x0078B560u+g_xbox_mem_offset),sizeof live));
        /* inside tank 1's frame (a charge going off during its update) */
        tnk_lv.t[1].block[13]=9u;FLOAT_AT(t2+0x1CCu)=30.0f;
        tnk_enter(1u);
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CHECK(nmine==m+6u && mine_state==9u && NEAR(FLOAT_AT(t2+0x1CCu),19.9f) && tnk_lv.act==1u && WORD_AT(0x0078B594u)==3u);
        tnk_leave(1u,0);
        /* not ours: stock only */
        WORD_AT(xp+0x158u)=ENT(3);
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        WORD_AT(xp+0x158u)=0;
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,0,0,0,1);
        CHECK(nmine==m+6u);
        WORD_AT(xp+0x158u)=t2;
        WORD_AT(0x0079094Cu)=4u;                                       /* the castle: its own sub_001E4030 */
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        play_scene();BYTE_AT(0x007DE455u)=5u;                          /* not story mode */
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        play_scene();
        set_env("TSFP_MAPMAKER_LIMITS",NULL);
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);
        set_env("TSFP_MAPMAKER_LIMITS","1");
        CHECK(nmine==m+6u && NEAR(FLOAT_AT(t2+0x1CCu),19.9f));
        CALL(pc_mapmaker_tank_mine,fake,0x000F049Fu,xp,0,0,1);          /* and back on */
        CHECK(nmine==m+7u);
    }

    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker tank ok\n");
    return 0;
}
