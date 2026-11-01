/* Asset-free contract for src/interface/pc_mapmaker_tank.inc: the castle tank
 * in MapMaker story play. prop6.pak mount at the level's pak mounts, the tank
 * created at a placeholder Story AI (id 0x12), the placeholder removed on the
 * tank's first frame, the proximity wake, no castle gate areas, the castle
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
#define GUNS 0x00540000u
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
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }

static int pak_mounted, mount_ok=1, spawn_ok=1, originals;
static unsigned nmount, nspawn, nremove;
static uint32_t mount_args[3], spawn_args[5], remove_arg;
static float spawn_pos[3], spawn_rot[3];
static int spawn_after_original;
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
        for(i=0;i<3;i++) FLOAT_AT(TANK+0x64u+(uint32_t)i*4u)=spawn_pos[i];
        FLOAT_AT(0x0078B580u)=-9.0f;FLOAT_AT(0x0078B584u)=spawn_pos[1];FLOAT_AT(0x0078B588u)=-3.2f;
        WORD_AT(0x0078B594u)=0;WORD_AT(0x0078B598u)=0;WORD_AT(0x0078B560u)=GUNS;
        return TANK;
    }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
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
    for(i=0;i<3;i++) { seen_tank[i]=FLOAT_AT(TANK+0x64u+(uint32_t)i*4u);seen_goal[i]=FLOAT_AT(0x0078B580u+(uint32_t)i*4u); }
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
static int ai_originals;
static void fake_ai(void) { originals++;ai_originals++;g_esp+=4u; }

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
    pak_mounted=0;WORD_AT(0x006D7880u)=29u;                   /* one slot left: kept for stock */
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1);
    WORD_AT(0x006D7880u)=28u;
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
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* pak missing */
    CHECK(nspawn==0 && !tnk_lv.obj);
    pak_mounted=1;
    resources(0,1,1,0x002560C0u);
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,1,0x00251450u);                                     /* another init */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,0,1,0x002560C0u);
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,0,0x002560C0u);
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,1,0x002560C0u);
    actor(1,0x13,40.0f,2.0f,30.0f,1.5f);                               /* another character */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(nspawn==0);
    spawn_ok=0;
    FLOAT_AT(0x0078B580u)=-9.0f;FLOAT_AT(0x0078B588u)=-3.2f;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==1 && !tnk_lv.obj && !tnk_lv.place);
    CHECK(FLOAT_AT(0x0078B580u)==-9.0f && FLOAT_AT(0x0078B588u)==-3.2f);   /* nothing rebased */
    spawn_ok=1;
    n=originals;
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==2 && tnk_lv.obj==TANK && spawn_after_original==(int)n+1);   /* after the AI started */
    CHECK(spawn_args[0]==0x281u && spawn_args[3]==0 && spawn_args[4]==0);
    CHECK(NEAR(spawn_pos[0],40.0f) && NEAR(spawn_pos[1],2.0f) && NEAR(spawn_pos[2],30.0f));
    CHECK(NEAR(spawn_rot[0],0.0f) && NEAR(spawn_rot[1],1.5f) && NEAR(spawn_rot[2],0.0f));
    dx=40.0f+12.05f;dy=2.0f;dz=30.0f-1.5f;
    CHECK(NEAR(FLOAT_AT(0x0078B580u),-9.0f+dx) && NEAR(FLOAT_AT(0x0078B584u),2.0f) && NEAR(FLOAT_AT(0x0078B588u),-3.2f+dz));
    actor(2,0x12,0.0f,0.0f,0.0f,0.0f);                                 /* a second placeholder */
    CALL(pc_mapmaker_tank_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(nspawn==2 && tnk_lv.obj==TANK);
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
    CHECK(nremove==0);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1 && remove_arg==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1);

    /* ---- wake ---------------------------------------------------------------- */
    players(100.0f,100.0f,1.0f,40.0f+20.0f,30.0f+20.0f,0.0f);           /* far; near but dead */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==0);
    players(100.0f,100.0f,1.0f,40.0f+20.0f,30.0f+20.0f,0.5f);           /* 28.3 m, alive */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);
    players(40.0f+22.0f,30.0f+22.0f,1.0f,100.0f,100.0f,1.0f);           /* 31.1 m */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==0);
    players(40.0f+29.0f,30.0f,1.0f,100.0f,100.0f,1.0f);                 /* x only */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);
    players(40.0f,30.0f-29.0f,1.0f,100.0f,100.0f,1.0f);                 /* z only */
    CHECK(CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,0)==1u);

    /* ---- area: no castle gate --------------------------------------------- */
    g_ecx=TANK+0x64u;
    pre_eax=1u;
    CHECK(CALL(pc_mapmaker_tank_area,fake,0x00255697u,0,0,0,0)==0);

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
    g_edi=0x00570000u;                                                 /* not our tank */
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
    CHECK(tnk_lv.obj==TANK);
    WORD_AT(0x006D7C4Cu)=0;
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_tank_mount,fake,0x001CDB8Au,0,0,0,1);           /* next level: no tank */
    CHECK(!tnk_lv.obj);
    CALL(pc_mapmaker_tank_wake,fake,0x00255580u,0,0,0,1);
    g_ecx=TANK+0x64u;
    CALL(pc_mapmaker_tank_area,fake,0x00255697u,0,0,0,1);
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1);

    /* a full level again: one tank, its placeholder removed once */
    level_with_tank(__LINE__);
    CHECK(tnk_lv.obj==TANK && nspawn==3);
    WORD_AT(A(0)+0xBF0u)=REC(5);                                      /* the pool entry was reused */
    CALL(pc_mapmaker_tank_update,fake,0x00000100u,TANK,0,0,1);
    CHECK(nremove==1);
    CHECK(ai_originals>0);

    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker tank ok\n");
    return 0;
}
