/* Asset-free contract for src/interface/pc_mapmaker_robot.inc: the lv14.pak
 * mount, the per-level state, the class 0x80000 override at AI start, the
 * pad fallback, the dropped level event / music change with the removal, the
 * death explosions, the restart reset and the single-bar rule. Fake guest
 * memory; fake originals pop their return address like the lifted callees,
 * and every hook call checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00800000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
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
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define CAM 0x00510000u
#define ESP0 0x00600000u
#define R 10u
#define ST 0x00700100u                  /* fake robot state (0x4C) */
#define PT 0x00710000u                  /* fake particle block (0x7D00) */
#define PT2 0x00720000u                 /* a block someone else made this level */
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=9; }
void sub_0005A130(void) { g_ecx=1; }
void sub_00062260(void) { g_ecx=2; }
void sub_00292630(void) { g_ecx=3; }
void sub_002A62C0(void) { g_ecx=4; }
void sub_00292550(void) { g_ecx=5; }
void sub_0015DDA0(void) { g_ecx=6; }
void sub_00019D70(void) { g_ecx=7; }
void sub_002A7F30(void) { g_ecx=8; }

static int pak_mounted, mount_ok=1, alloc_ok=1;
static unsigned nmount, nalloc, nparts, nreset, nremove, nmode, ndraw;
static uint32_t mount_arg, remove_arg, mode_arg, draw_args[2], draw_scene;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) return (x[0]==0x0049DC78u && pak_mounted)?3u:0xFFFFFFFFu;
    if(f==sub_0005A130) { nmount++;mount_arg=x[0];if(mount_ok && x[0]==0x0049DC78u) pak_mounted=1;return mount_ok?1u:0u; }
    if(f==sub_00292630) {
        nalloc++;
        if(alloc_ok==2) WORD_AT(0x0078B1DCu)=0x100u;
        else if(alloc_ok) { WORD_AT(0x0078B1DCu)=ST;WORD_AT(ST+0x48u)=0; }
        return 0;
    }
    if(f==sub_002A62C0) { nparts++;if(alloc_ok) WORD_AT(0x0078B0D0u)=PT;return 0; }
    if(f==sub_00292550) { nreset++;WORD_AT(ST+0x48u)=0;return 0; }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_00019D70) { nmode++;mode_arg=x[0];return 0; }
    if(f==sub_002A7F30) { ndraw++;draw_args[0]=x[0];draw_args[1]=x[1];draw_scene=WORD_AT(0x0079094Cu);return 0; }
    fprintf(stderr,"unexpected guest_call\n");exit(4);
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_robot.inc"

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t orig_eax, seen_class[2], seen_focus, seen_parts, set_parts;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
static void fake_ai(void)
{
    uint32_t rec=WORD_AT(WORD_AT(g_esp+4u)+0xBF0u);
    originals++;seen_class[0]=WORD_AT(rec);seen_class[1]=WORD_AT(rec+4u);g_esp+=4u;
}
static void fake_hud(void) { originals++;seen_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }
/* the level init: sees the particle pointer; an inner area may make the block */
static void fake_level(void) { originals++;seen_parts=WORD_AT(0x0078B0D0u);if(set_parts) WORD_AT(0x0078B0D0u)=set_parts;g_esp+=4u; }
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
    rbt.read=0;
}
/* map container: map-info chunk with the set (index, members) */
static void container(uint32_t set_index,const uint16_t *ids,unsigned n)
{
    uint32_t c=CONT+8u,k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x00080000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    WORD_AT(c)=0x00C77668u;WORD_AT(c+4u)=0x18u+20u*4u;
    HALF_AT(c+8u+0x14u)=(uint16_t)set_index;BYTE_AT(c+8u+0x16u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(c+8u+0x18u+k*4u)=ids[k];
    c+=8u+0x18u+20u*4u;
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
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
static void play_scene(void) { WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u; }
/* a MapMaker level start: anim mount, level init; returns mounts made */
static unsigned level(int line)
{
    unsigned m=nmount;
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    WORD_AT(0x0078B1DCu)=0x00123450u;WORD_AT(0x0078B0D0u)=0x00123460u;   /* stale pointers */
    call(pc_mapmaker_robot_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1,line);
    return nmount-m;
}
/* level with the robot (actor 3) started */
static const uint16_t robot_set[]={0x0C,0x42,0xA8};
static void robot_level(int line)
{
    container(0x11,robot_set,3);
    play_scene();
    set_parts=0;
    level(line);
    actor(3,0x42,0x5F2,0);
    call(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1,line);
    if(seen_class[1]!=0x80000u) { fprintf(stderr,"%s:%d: robot class 0x%X\n",__FILE__,line,(unsigned)seen_class[1]);failures++; }
}

int main(void)
{
    static const uint16_t with[]={0x0C,0x42,0xA8}, without[]={0x0C,0xA8};
    unsigned k,n;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x00783A50u)=R;
    WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(0x007B0CC0u)=CAM;WORD_AT(CAM+0x208u)=0x00515000u;
    WORD_AT(0x00790950u)=1u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    container(0x11,with,3);
    play_scene();
    CHECK(level(__LINE__)==0 && nalloc==0 && nparts==0);
    CHECK(WORD_AT(0x0078B0D0u)==0x00123460u);                 /* untouched */
    actor(0,0x42,0x5F2,0);pak_mounted=1;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    orig_eax=5;
    CHECK(CALL(pc_mapmaker_robot_nav,fake,0x00293949u,0x46Eu,0,0,1)==5u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CHECK(level(__LINE__)==0 && nalloc==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- level start: the set decides --------------------------------------- */
    container(0x11,without,2);
    CHECK(level(__LINE__)==0 && nalloc==0 && nparts==0);
    CHECK(WORD_AT(0x0078B1DCu)==0x00123450u);                 /* no robot: its pointer untouched */
    container(0x0D,with,3);                                   /* a preset set plays */
    CHECK(level(__LINE__)==0 && nalloc==0);
    container(0x11,with,3);
    BYTE_AT(0x007DE455u)=4u;                                  /* not story */
    CHECK(level(__LINE__)==0 && nalloc==0);
    play_scene();
    WORD_AT(0x0079094Cu)=14u;                                 /* the story level itself */
    CHECK(level(__LINE__)==0 && nalloc==0 && WORD_AT(0x0078B0D0u)==0x00123460u);
    play_scene();
    CHECK(level(__LINE__)==1 && mount_arg==0x0049DC78u && pak_mounted);
    CHECK(seen_parts==0);                                     /* cleared before the stock init */
    CHECK(nalloc==1 && nparts==1 && rbt_lvl.state==ST && WORD_AT(0x0078B0D0u)==PT);
    container(0x11,without,2);container_extra(with+1,1);      /* slot 11 */
    CHECK(level(__LINE__)==1 && nalloc==2 && rbt_lvl.state==ST);
    WORD_AT(0x00784014u)=0;                                   /* unreadable container: assumed */
    CHECK(level(__LINE__)==1 && nalloc==3);
    container(0x11,with,3);
    pak_mounted=1;n=nmount;                                   /* the split Crowe area mounted it */
    WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_robot_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==n);
    pak_mounted=0;WORD_AT(0x006D7880u)=57u;                   /* table nearly full */
    CALL(pc_mapmaker_robot_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==n && !pak_mounted);
    WORD_AT(0x006D7880u)=56u;
    CALL(pc_mapmaker_robot_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==n+1 && pak_mounted);
    set_parts=PT2;n=nparts;                                   /* the skeleton/walker made the block */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(nparts==n && WORD_AT(0x0078B0D0u)==PT2 && rbt_lvl.state==ST);
    set_parts=0;
    alloc_ok=0;                                               /* no level memory */
    WORD_AT(0x0078B1DCu)=0x00123450u;                         /* an earlier level's block */
    set_parts=PT2;                                            /* the particles exist */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(!rbt_lvl.state);
    set_parts=0;
    alloc_ok=2;                                               /* an unusable pointer */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(!rbt_lvl.state);
    alloc_ok=1;
    container(0x11,with+1,1);                                 /* the robot in slot 1 only */
    CHECK(level(__LINE__)==1 && rbt_lvl.state==ST);
    container(0x11,with,3);
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    n=nalloc;
    CHECK(level(__LINE__)==0 && nalloc==n);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- AI start --------------------------------------------------------- */
    CHECK(level(__LINE__)==1);
    actor(1,0x0C,0x400,0);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && !rbt_lvl.record);
    actor(2,0x30,0x5F2,1);                                    /* the robot model on another id */
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80000u && rbt_lvl.record==REC(2) && rbt_lvl.actor==A(2));
    actor(3,0x42,0x5F2,2);                                    /* a second robot */
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && rbt_lvl.record==REC(2));
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);   /* the same one again (restart) */
    CHECK(seen_class[1]==0x80000u && rbt_lvl.record==REC(2));
    CHECK(level(__LINE__)==1 && !rbt_lvl.record);              /* a new level forgets it */
    pak_mounted=0;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.record);
    pak_mounted=1;
    WORD_AT(0x0078B1DCu)=0x00123450u;                         /* the state is not this level's */
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.record);
    WORD_AT(0x0078B1DCu)=ST;
    BYTE_AT(0x007DE455u)=4u;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.record);
    play_scene();
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.record);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80000u && rbt_lvl.record==REC(3));

    /* ---- pads: the seven existence tests get -1 ---------------------------- */
    orig_eax=7;
    for(k=0;k<7u;k++) CHECK(CALL(pc_mapmaker_robot_nav,fake,rbt_nav_rets[k],0x465u,0,0,0)==0xFFFFFFFFu);
    CHECK(rbt_nav_rets[0]==0x00293949u && rbt_nav_rets[6]==0x00293CD0u);
    CHECK(CALL(pc_mapmaker_robot_nav,fake,0x00293986u,0x46Eu,0,0,1)==7u);    /* the index fetches */
    CHECK(CALL(pc_mapmaker_robot_nav,fake,0x00293CE5u,0x690u,0,0,1)==7u);
    CHECK(CALL(pc_mapmaker_robot_nav,fake,0x0015D565u,0x3E9u,0,0,1)==7u);    /* everyone else */
    WORD_AT(0x0079094Cu)=14u;
    CHECK(CALL(pc_mapmaker_robot_nav,fake,0x00293949u,0x46Eu,0,0,1)==7u);
    play_scene();

    /* ---- death: music, event, removal, explosions --------------------------- */
    WORD_AT(PLAYER+0xFF4u)=ENT(3);
    CALL(pc_mapmaker_robot_music,fake,0x00255E9Du,0u,0,0,1);                 /* the tank's: stock */
    CALL(pc_mapmaker_robot_music,fake,0x002944D9u,0u,0,0,0);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_music,fake,0x002944D9u,0u,0,0,1);
    play_scene();
    /* explosions only while dying, before the removal */
    n=ndraw;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n);
    WORD_AT(ST+0x48u)=1u;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE87u,0,0,0,1);                    /* another caller */
    CHECK(ndraw==n);
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1 && draw_args[0]==0x00515000u && draw_args[1]==0x40200000u && nmode==1 && mode_arg==1u);
    WORD_AT(0x00790950u)=2u;                                  /* split screen: stock draws none */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1);
    WORD_AT(0x00790950u)=1u;
    WORD_AT(0x0078B1DCu)=0x00123450u;                         /* not this level's state */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1);
    WORD_AT(0x0078B1DCu)=ST;
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1);
    play_scene();
    /* the event: another boss's stays, the robot's is dropped and it is removed */
    CALL(pc_mapmaker_robot_event,fake,0x0029E068u,3u,0,0,1);
    CHECK(nremove==0);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,1);
    CHECK(nremove==0);
    play_scene();
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,0);
    CHECK(nremove==1 && remove_arg==ENT(3) && WORD_AT(PLAYER+0xFF4u)==0 && rbt_lvl.removed);
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,0);
    CHECK(nremove==1);                                        /* once */
    n=ndraw;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n);                                          /* gone: no frozen particles */
    /* a soft restart resets the state */
    n=nreset;
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);
    CHECK(nreset==n);
    play_scene();
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);
    CHECK(nreset==n+1 && !rbt_lvl.removed && WORD_AT(ST+0x48u)==0);
    WORD_AT(0x0078B1DCu)=0x00123450u;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);
    CHECK(nreset==n+1);
    WORD_AT(0x0078B1DCu)=ST;
    /* the focus moved on before the removal: it is not cleared */
    robot_level(__LINE__);
    WORD_AT(PLAYER+0xFF4u)=ENT(1);
    n=nremove;
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,0);
    CHECK(nremove==n+1 && WORD_AT(PLAYER+0xFF4u)==ENT(1));

    /* ---- the stock bar is never drawn for the robot ------------------------- */
    robot_level(__LINE__);
    WORD_AT(PLAYER+0xFF4u)=ENT(3);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3));
    WORD_AT(ENT(6)+0x7Cu)=A(3);HALF_AT(ENT(6)+0x94u)=0x400u;  /* not the robot's model */
    WORD_AT(PLAYER+0xFF4u)=ENT(6);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(6));
    actor(4,0x15,0x5BE,3);                                    /* another boss */
    WORD_AT(PLAYER+0xFF4u)=ENT(4);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(4));
    actor(5,0x42,0x5F2,4);                                    /* a second robot (bot class) */
    WORD_AT(PLAYER+0xFF4u)=ENT(5);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(5));
    WORD_AT(PLAYER+0xFF4u)=ENT(3);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(3));
    play_scene();
    WORD_AT(PLAYER+0xFF4u)=0;
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0);

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("mapmaker robot ok\n");
    return 0;
}
