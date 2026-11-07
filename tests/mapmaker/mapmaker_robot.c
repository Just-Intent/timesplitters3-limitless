/* Asset-free contract for src/interface/pc_mapmaker_robot.inc: the lv14.pak
 * mount, one state block per robot (up to four) at level start, the class
 * 0x80000 override at AI start, the robot's globals (state pointer and three
 * statics) swapped around each entry into its code (handler, messages, two
 * callbacks, nesting included), the pad fallback, the dropped level event /
 * music change with the removal of the robot that died, the death
 * explosions, the restart reset and the single-bar rule. Fake guest memory;
 * fake originals pop their return address like the lifted callees, and every
 * hook call checks esp. */
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
#define ST(k) (0x00700100u+(uint32_t)(k)*0x100u)   /* fake robot states (0x4C) */
#define PT 0x00710000u                  /* fake particle block (0x7D00) */
#define PT2 0x00720000u                 /* a block someone else made this level */
#define STAT 0x0075F5ECu                /* the robot's three statics */
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

static int pak_mounted, mount_ok=1, alloc_ok=1, parts_ok=1;
static unsigned alloc_limit=99, level_allocs, nmount, nalloc, nparts, nreset, nremove, nmode, ndraw;
static uint32_t mount_arg, remove_arg, mode_arg, draw_args[2], draw_scene, reset_seen[8], alloc_parts;
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
    if(f==sub_00292630) {   /* [0x0078B1DC] = a new block, set by sub_00292550 */
        nalloc++;alloc_parts=WORD_AT(0x0078B0D0u);
        if(alloc_ok==2) WORD_AT(0x0078B1DCu)=0x100u;
        else if(alloc_ok && level_allocs<alloc_limit) { WORD_AT(0x0078B1DCu)=ST(level_allocs);WORD_AT(ST(level_allocs)+0x48u)=0;level_allocs++; }
        return 0;
    }
    if(f==sub_002A62C0) { nparts++;if(parts_ok) WORD_AT(0x0078B0D0u)=PT;return 0; }
    if(f==sub_00292550) { if(nreset<8u) reset_seen[nreset]=WORD_AT(0x0078B1DCu);nreset++;WORD_AT(WORD_AT(0x0078B1DCu)+0x48u)=0;return 0; }
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

/* A call from inside a fake original into a hook: ret and args pushed below esp. */
static void inner(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,uint32_t a2,int line)
{
    uint32_t sp=g_esp;
    g_esp-=12u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a1;WORD_AT(g_esp+8u)=a2;
    hook(orig);
    if(g_esp!=sp-8u) { fprintf(stderr,"%s:%d: inner esp %+d\n",__FILE__,line,(int)(g_esp-(sp-8u)));failures++; }
    g_esp=sp;
}
/* The robot's code: records the state pointer and statics it sees, counts its
 * statics up (1, 2, 3), and may reach another robot's code or fire event 2. */
static uint32_t seen_state, seen_stat[3], seen_after[4];
static uint32_t nest_ent, inner_state, inner_stat[3], seen_room;
static float seen_x;
static int dies_now;
static void fake_inner(void)
{
    int i;
    originals++;inner_state=WORD_AT(0x0078B1DCu);
    for(i=0;i<3;i++) { inner_stat[i]=WORD_AT(STAT+(uint32_t)i*4u);WORD_AT(STAT+(uint32_t)i*4u)+=100u; }
    g_esp+=4u;
}
static void fake_robot(void)
{
    int i;
    originals++;
    seen_state=WORD_AT(0x0078B1DCu);
    seen_x=FLOAT_AT(WORD_AT(g_esp+4u)+0x64u);seen_room=WORD_AT(WORD_AT(g_esp+4u)+0x34u);   /* where the AI finds it */
    for(i=0;i<3;i++) { seen_stat[i]=WORD_AT(STAT+(uint32_t)i*4u);WORD_AT(STAT+(uint32_t)i*4u)+=(uint32_t)i+1u; }
    if(nest_ent) {
        inner(pc_mapmaker_robot_message,fake_inner,0x0029400Au,nest_ent,7u,__LINE__);
        seen_after[0]=WORD_AT(0x0078B1DCu);
        for(i=0;i<3;i++) seen_after[1+i]=WORD_AT(STAT+(uint32_t)i*4u);
    }
    if(dies_now==2) WORD_AT(WORD_AT(WORD_AT(g_esp+4u)+0x7Cu)+0xBF0u)=REC(8);   /* its pool entry reused meanwhile */
    if(dies_now) inner(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,__LINE__);
    g_esp+=4u;
}

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
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;level_allocs=0;
    WORD_AT(0x0078B1DCu)=0x00123450u;WORD_AT(0x0078B0D0u)=0x00123460u;   /* stale pointers */
    call(pc_mapmaker_robot_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1,line);
    return nmount-m;
}
/* a level with robots on actors 3.. (n of them) started */
static const uint16_t robot_set[]={0x0C,0x42,0xA8};
static void robot_level(unsigned n,int line)
{
    unsigned k;
    container(0x11,robot_set,3);
    play_scene();
    set_parts=0;
    level(line);
    for(k=0;k<n;k++) {
        actor(3+k,0x42,0x5F2,k);
        call(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3+k),0,0,1,line);
        if(k<4u && seen_class[1]!=0x80000u) { fprintf(stderr,"%s:%d: robot %u class 0x%X\n",__FILE__,line,k,(unsigned)seen_class[1]);failures++; }
    }
}
/* ---- v0.4.0-mapmaker.40: the robot through openings lower than it -------- */
/* Room 1 (pads 0-3), the low door (y 0..3) to room 2 (pads 4, 5: the player),
 * a tall opening (0..10) to room 3 (pads 6, 7), and room 3 to room 2 through
 * another tall opening when 'way_round'. The robot's box top: 6. */
#define MPADS 0x00580000u
#define MLINKS 0x00581000u
#define MRECS 0x00582000u
#define MLISTS 0x00583000u
#define MPTRS 0x00584000u
#define MPORT 0x00585000u
static void SETF(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static uint32_t seen_goal, seen_goal_slot;
static void fake_goal(void) { originals++;seen_goal=WORD_AT(g_esp+8u);seen_goal_slot=WORD_AT(WORD_AT(g_esp+4u)+0x7Cu)+0x684u;seen_goal_slot=WORD_AT(seen_goal_slot);g_esp+=4u; }
static void portal(uint32_t id,uint32_t ra,uint32_t rb,float x0,float z0,float x1,float z1,float top)
{
    uint32_t pt=MPORT+id*0x100u;
    WORD_AT(MPTRS+id*4u)=pt;WORD_AT(pt)=ra;WORD_AT(pt+4u)=rb;HALF_AT(pt+0x14u)=4u;
    SETF(pt+0x18u,x0);SETF(pt+0x1Cu,0.0f);SETF(pt+0x20u,z0);
    SETF(pt+0x24u,x0);SETF(pt+0x28u,top);SETF(pt+0x2Cu,z0);
    SETF(pt+0x30u,x1);SETF(pt+0x34u,top);SETF(pt+0x38u,z1);
    SETF(pt+0x3Cu,x1);SETF(pt+0x40u,0.0f);SETF(pt+0x44u,z1);
}
static void low_door_map(float door,int way_round)
{
    static const float xz[8][2]={{0,0},{0,5},{10,0},{0,10},{12,0},{20,0},{0,12},{20,20}};
    static const int16_t rooms[8]={1,1,1,1,2,2,3,3};
    static const uint32_t links[9][2]={{0,1},{0,2},{0,3},{1,3},{2,4},{4,5},{3,6},{6,7},{7,5}};
    uint32_t k;
    memset((void *)((uintptr_t)MPADS+g_xbox_mem_offset),0,0x10000u);
    WORD_AT(0x00715F2Cu)=MPADS;WORD_AT(0x007B6664u)=8u;
    for(k=0;k<8u;k++) {
        uint32_t pd=MPADS+k*0x20u;
        WORD_AT(pd)=0x3E9u+k;BYTE_AT(pd+4u)=1u;HALF_AT(pd+0xAu)=(uint16_t)rooms[k];
        SETF(pd+0xCu,xz[k][0]);SETF(pd+0x10u,0.0f);SETF(pd+0x14u,xz[k][1]);
    }
    WORD_AT(0x00715F30u)=MLINKS;WORD_AT(0x007B6660u)=9u;
    for(k=0;k<9u;k++) { WORD_AT(MLINKS+k*0x14u)=0x10010u;WORD_AT(MLINKS+k*0x14u+4u)=links[k][0];WORD_AT(MLINKS+k*0x14u+8u)=links[k][1]; }
    if(!way_round) WORD_AT(MLINKS+8u*0x14u)=0;                /* no link 7-5 */
    WORD_AT(0x007B975Cu)=3u;WORD_AT(0x00708788u)=MRECS;WORD_AT(0x0070878Cu)=MPTRS;WORD_AT(0x007B9754u)=2u;
    portal(0,1,2,11.0f,-1.0f,11.0f,1.0f,door);                 /* the door between pads 2 and 4 */
    portal(1,3,1,1.0f,11.0f,3.0f,11.0f,10.0f);
    portal(2,3,2,20.0f,10.0f,22.0f,10.0f,10.0f);
    portal(3,1,2,11.0f,29.0f,11.0f,31.0f,10.0f);              /* rooms 1-2 again, tall, far from the door */
    portal(4,2,1,-1.0f,10.5f,1.0f,10.5f,3.0f);                /* rooms 1-2, low, next to the 1-3 opening */
    WORD_AT(0x007B9754u)=4u;
    for(k=1;k<=3u;k++) WORD_AT(MRECS+k*0xB4u+4u)=MLISTS+k*0x40u;
    WORD_AT(MLISTS+0x40u)=4u;WORD_AT(MLISTS+0x44u)=0u;WORD_AT(MLISTS+0x48u)=1u;WORD_AT(MLISTS+0x4Cu)=3u;WORD_AT(MLISTS+0x50u)=4u;
    WORD_AT(MLISTS+0x80u)=4u;WORD_AT(MLISTS+0x84u)=0u;WORD_AT(MLISTS+0x88u)=2u;WORD_AT(MLISTS+0x8Cu)=3u;WORD_AT(MLISTS+0x90u)=4u;
    WORD_AT(MLISTS+0xC0u)=2u;WORD_AT(MLISTS+0xC4u)=1u;WORD_AT(MLISTS+0xC8u)=2u;
}
static void goal_at(uint32_t at,uint32_t goal,int expect_orig,int line)
{
    WORD_AT(A(3)+0x678u)=at;WORD_AT(A(3)+0x684u)=goal;seen_goal=seen_goal_slot=0xFFFFu;
    call(pc_mapmaker_robot_goal,fake_goal,0x00293D27u,ENT(3),goal,0,expect_orig,line);
    if(WORD_AT(ESP0+8u)!=goal) { fprintf(stderr,"%s:%d: goal arg slot not put back\n",__FILE__,line);failures++; }
}
static void low_door(void)
{
    /* the player beyond the low door, reachable the long way round: the robot
     * is steered room by room through the tall openings */
    low_door_map(3.0f,1);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,3.0f);SETF(ENT(3)+0x1C4u,3.0f);
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==3u && seen_goal_slot==3u && rbt_walk[0].height==6.0f && rbt_walk[0].reached==8u);
    CHECK(rbt_walk[0].blocked==2u && rbt_nav.crossings==3u);  /* the door link, seen from both pads */
    goal_at(3,5,1,__LINE__);
    CHECK(seen_goal==6u);                                     /* across the tall opening */
    goal_at(6,5,1,__LINE__);
    CHECK(seen_goal==7u);
    goal_at(7,5,1,__LINE__);
    CHECK(seen_goal==5u && seen_goal_slot==5u);               /* the last crossing: stock */
    goal_at(4,5,1,__LINE__);                                  /* pushed through the door after all: from there */
    CHECK(seen_goal==5u && rbt_walk[0].from==0u);
    /* only the low door leads there: it waits at the door (the pad nearest the player) */
    low_door_map(3.0f,0);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,3.0f);SETF(ENT(3)+0x1C4u,3.0f);
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==2u && seen_goal_slot==2u && rbt_walk[0].reached==6u && !rbt_walk[0].reach[5]);
    goal_at(2,5,1,__LINE__);
    CHECK(seen_goal==2u);
    goal_at(4,5,1,__LINE__);                                  /* pushed through the door: its set from there */
    CHECK(seen_goal==5u && rbt_walk[0].from==4u && rbt_walk[0].reached==2u);
    goal_at(0,7,1,__LINE__);                                  /* a pad it reaches: through the tall opening */
    CHECK(seen_goal==3u);
    /* a door as tall as the robot: stock (the slack), everywhere */
    low_door_map(5.8f,0);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,3.0f);SETF(ENT(3)+0x1C4u,3.0f);
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==5u && rbt_walk[0].reached==8u && !rbt_walk[0].blocked);
    /* no usable box: 6.0; a smaller robot passes the door */
    low_door_map(3.0f,0);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,0.0f);SETF(ENT(3)+0x1C4u,0.0f);
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==2u && rbt_walk[0].height==6.0f);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,1.0f);SETF(ENT(3)+0x1C4u,1.0f);
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==5u && rbt_walk[0].height==2.0f);
    /* unknown rooms are passable */
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,3.0f);SETF(ENT(3)+0x1C4u,3.0f);
    HALF_AT(MPADS+4u*0x20u+0xAu)=0;
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==5u);
    HALF_AT(MPADS+4u*0x20u+0xAu)=2u;
    /* every other caller, another entity, a goal outside the table: stock */
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,3.0f);SETF(ENT(3)+0x1C4u,3.0f);
    WORD_AT(A(3)+0x678u)=0;WORD_AT(A(3)+0x684u)=5u;
    call(pc_mapmaker_robot_goal,fake_goal,0x0015D000u,ENT(3),5u,0,1,__LINE__);
    CHECK(seen_goal==5u && WORD_AT(A(3)+0x684u)==5u);
    actor(9,0x0C,0x400,9);WORD_AT(A(9)+0x678u)=0;
    call(pc_mapmaker_robot_goal,fake_goal,0x00293D27u,ENT(9),5u,0,1,__LINE__);
    CHECK(seen_goal==5u);
    goal_at(0,8,1,__LINE__);
    CHECK(seen_goal==8u);
    WORD_AT(0x0079094Cu)=14u;                                 /* mission 10 itself */
    goal_at(0,5,1,__LINE__);
    CHECK(seen_goal==5u);
    play_scene();
    robot_level(0,__LINE__);                                  /* a level without robots: stock */
    call(pc_mapmaker_robot_goal,fake_goal,0x00293D27u,ENT(3),5u,0,1,__LINE__);
    CHECK(seen_goal==5u);
}
/* v0.4.0-mapmaker.42 report: "Robots still walked through a one-floor
 * opening". MapMaker floors are 5 apart; the robot's real box top is 14.51
 * (its model radius 6.5 + centre 8.01, the user's .41 log). Its hall (room 1,
 * pads 0-2 in a line) opens to room 2 (pads 3-4) by a one-floor door
 * (4.5, links 0-3 and 1-3) and, at the far end, a three-floor opening (15, link 2-4).
 * The stock route to the far end of its own room is free to cut through the
 * door, so the robot is given one link it fits through at a time. */
static void one_floor_door_map(int tall)
{
    static const float xz[5][2]={{0,0},{0,8},{0,16},{10,0},{10,16}};
    static const int16_t rooms[5]={1,1,1,2,2};
    static const uint32_t links[6][2]={{0,1},{1,3},{1,2},{0,3},{2,4},{3,4}};
    uint32_t k;
    memset((void *)((uintptr_t)MPADS+g_xbox_mem_offset),0,0x10000u);
    WORD_AT(0x00715F2Cu)=MPADS;WORD_AT(0x007B6664u)=5u;
    for(k=0;k<5u;k++) {
        uint32_t pd=MPADS+k*0x20u;
        WORD_AT(pd)=0x3E9u+k;BYTE_AT(pd+4u)=1u;HALF_AT(pd+0xAu)=(uint16_t)rooms[k];
        SETF(pd+0xCu,xz[k][0]);SETF(pd+0x10u,0.0f);SETF(pd+0x14u,xz[k][1]);
    }
    WORD_AT(0x00715F30u)=MLINKS;WORD_AT(0x007B6660u)=6u;
    for(k=0;k<6u;k++) { WORD_AT(MLINKS+k*0x14u)=0x10010u;WORD_AT(MLINKS+k*0x14u+4u)=links[k][0];WORD_AT(MLINKS+k*0x14u+8u)=links[k][1]; }
    if(!tall) WORD_AT(MLINKS+4u*0x14u)=0;                     /* no link 2-4 */
    WORD_AT(0x007B975Cu)=2u;WORD_AT(0x00708788u)=MRECS;WORD_AT(0x0070878Cu)=MPTRS;WORD_AT(0x007B9754u)=1u;
    portal(0,1,2,5.0f,-1.0f,5.0f,1.0f,4.5f);                   /* one floor */
    portal(1,2,1,5.0f,15.0f,5.0f,17.0f,15.0f);                 /* three floors */
    for(k=1;k<=2u;k++) { WORD_AT(MRECS+k*0xB4u+4u)=MLISTS+k*0x40u;WORD_AT(MLISTS+k*0x40u)=2u;WORD_AT(MLISTS+k*0x40u+4u)=0u;WORD_AT(MLISTS+k*0x40u+8u)=1u; }
}
static float link_open(uint32_t a,uint32_t b)
{
    uint32_t e;
    for(e=rbt_nav.off[a];e<rbt_nav.off[a+1u];e++) if(rbt_nav.adj[e]==b) return rbt_nav.open[e];
    return -1.0f;                                              /* not a link: the stock route picks the way */
}
static void one_floor_door(void)
{
    static const uint32_t route[]={1,2,4,3};
    uint32_t at=0,k;
    one_floor_door_map(1);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,8.01f);SETF(ENT(3)+0x1C4u,6.5f);
    for(k=0;k<4u;k++) {
        goal_at(at,3,1,__LINE__);
        if(seen_goal!=route[k] || link_open(at,seen_goal)<rbt_walk[0].height-RBT_FIT_SLACK) {
            fprintf(stderr,"%s:%d: hop %u from pad %u: goal %u (opening %.2f) not pad %u\n",__FILE__,__LINE__,k,at,seen_goal,(double)link_open(at,seen_goal),route[k]);
            failures++;
            break;
        }
        at=seen_goal;
    }
    CHECK(rbt_walk[0].height>14.5f && rbt_walk[0].height<14.52f && rbt_walk[0].reached==5u && rbt_walk[0].blocked==4u && rbt_nav.told);
    CHECK(rbt_nav.crossings==3u && link_open(0,3)==4.5f && link_open(1,3)==4.5f && link_open(2,4)==15.0f);
    CHECK(seen_goal_slot==3u);
    goal_at(1,4,1,__LINE__);                                  /* the door is as short a way as the hall: not it */
    CHECK(seen_goal==2u);
    /* only the one-floor door: it waits at the pad nearest the player, one link at a time */
    one_floor_door_map(0);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,8.01f);SETF(ENT(3)+0x1C4u,6.5f);
    goal_at(2,3,1,__LINE__);
    CHECK(seen_goal==1u && seen_goal_slot==1u && rbt_walk[0].reached==3u && !rbt_walk[0].reach[3]);
    goal_at(1,3,1,__LINE__);
    CHECK(seen_goal==0u);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==0u);                                      /* it stays at the door */
    /* the same door for a robot of one floor: through it */
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,2.0f);SETF(ENT(3)+0x1C4u,2.0f);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==3u);
}
/* v0.4.0-mapmaker.44 report: "still walks through openings only one floor
 * tall" (every MapMaker tile side is a portal of one floor, 5.00 in the
 * user's .43 log). Two columns of three stacked one-floor tiles: rooms 1/3/5
 * (x 0..5) and 2/4/6 (x 5..10), joined at x = 5 by one portal per floor
 * ('floors' of them: bottom 0..5, then 5..10, 10..15, or only the top one
 * when floors < 0), each column's floors open between its rooms unless
 * 'slab'. 'plane': the upper portals lie elsewhere (1: z 20..30 of that wall,
 * 2: across the room). 'same': the upper portals join rooms 1-2 too (one tall
 * room each side; 2: on the left only). Pads 0, 1 in room 1, 2, 3 in room 2; links 0-1, 1-2 (the
 * doorway), 2-3. */
static uint32_t sp_rooms[16][2], sp_n;
static void wall_y(uint32_t id,uint32_t ra,uint32_t rb,float x0,float z0,float x1,float z1,float bottom,float top)
{
    uint32_t pt=MPORT+id*0x100u;
    portal(id,ra,rb,x0,z0,x1,z1,top);
    SETF(pt+0x1Cu,bottom);SETF(pt+0x40u,bottom);
    sp_rooms[id][0]=ra;sp_rooms[id][1]=rb;if(id+1u>sp_n) sp_n=id+1u;
}
static void floor_at(uint32_t id,uint32_t ra,uint32_t rb,float x0,float x1,float y)
{
    uint32_t pt=MPORT+id*0x100u;
    portal(id,ra,rb,x0,0.0f,x1,10.0f,y);
    SETF(pt+0x1Cu,y);SETF(pt+0x40u,y);
    SETF(pt+0x2Cu,10.0f);SETF(pt+0x38u,0.0f);                 /* a rectangle at height y */
    sp_rooms[id][0]=ra;sp_rooms[id][1]=rb;if(id+1u>sp_n) sp_n=id+1u;
}
static void stacked_map(int floors,int slab,int plane,int same)
{
    static const float xz[4][2]={{0,5},{2,5},{8,5},{10,5}};
    static const int16_t rooms[4]={1,1,2,2};
    static const uint32_t links[3][2]={{0,1},{1,2},{2,3}};
    uint32_t k,r,i,n;
    memset((void *)((uintptr_t)MPADS+g_xbox_mem_offset),0,0x10000u);
    memset(sp_rooms,0,sizeof sp_rooms);sp_n=0;
    WORD_AT(0x00715F2Cu)=MPADS;WORD_AT(0x007B6664u)=4u;
    for(k=0;k<4u;k++) {
        uint32_t pd=MPADS+k*0x20u;
        WORD_AT(pd)=0x3E9u+k;BYTE_AT(pd+4u)=1u;HALF_AT(pd+0xAu)=(uint16_t)rooms[k];
        SETF(pd+0xCu,xz[k][0]);SETF(pd+0x10u,0.0f);SETF(pd+0x14u,xz[k][1]);
    }
    WORD_AT(0x00715F30u)=MLINKS;WORD_AT(0x007B6660u)=3u;
    for(k=0;k<3u;k++) { WORD_AT(MLINKS+k*0x14u)=0x10010u;WORD_AT(MLINKS+k*0x14u+4u)=links[k][0];WORD_AT(MLINKS+k*0x14u+8u)=links[k][1]; }
    WORD_AT(0x007B975Cu)=6u;WORD_AT(0x00708788u)=MRECS;WORD_AT(0x0070878Cu)=MPTRS;
    for(k=0;k<16u;k++) WORD_AT(MPTRS+k*4u)=0;
    wall_y(0,1,2,5.0f,0.0f,5.0f,10.0f,0.0f,5.0f);
    for(k=1;k<=2u;k++) {
        uint32_t a=same?1u:1u+2u*k,b=same==1?2u:2u+2u*k;
        if(!(floors<0 ? k==2u : (int)k<floors)) continue;
        if(plane==1) wall_y(k,a,b,5.0f,20.0f,5.0f,30.0f,5.0f*(float)k,5.0f*(float)k+5.0f);
        else if(plane==2) wall_y(k,a,b,0.0f,10.0f,5.0f,10.0f,5.0f*(float)k,5.0f*(float)k+5.0f);
        else wall_y(k,a,b,5.0f,0.0f,5.0f,10.0f,5.0f*(float)k,5.0f*(float)k+5.0f);
    }
    if(!slab) {
        if(!same) { floor_at(3,1,3,0.0f,5.0f,5.0f);floor_at(5,3,5,0.0f,5.0f,10.0f); }
        floor_at(4,2,4,5.0f,10.0f,5.0f);floor_at(6,4,6,5.0f,10.0f,10.0f);
    }
    WORD_AT(0x007B9754u)=6u;
    for(r=1;r<=6u;r++) {
        WORD_AT(MRECS+r*0xB4u+4u)=MLISTS+r*0x40u;
        for(i=sp_n,n=0;i-->0;) if(WORD_AT(MPTRS+i*4u) && (sp_rooms[i][0]==r || sp_rooms[i][1]==r)) WORD_AT(MLISTS+r*0x40u+4u+4u*n++)=i;
        WORD_AT(MLISTS+r*0x40u)=n;
    }
}
static void stacked_level(int floors,int slab,int plane,int same)
{
    stacked_map(floors,slab,plane,same);
    robot_level(1,__LINE__);
    SETF(ENT(3)+0x1ACu,8.01f);SETF(ENT(3)+0x1C4u,6.5f);       /* 14.51, the user's log */
}
static void stacked_floors(void)
{
    /* three open floors: 15, the robot fits through, every link stock */
    stacked_level(3,0,0,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==3u && rbt_walk[0].reached==4u && !rbt_walk[0].blocked && link_open(1,2)==15.0f && rbt_nav.stacked==1u);
    CHECK(rbt_stacked(3)==RBT_OPEN && rbt_stacked(0)==15.0f);  /* a floor portal is no doorway; cached */
    /* one floor (the .43 log): 5, closed; it waits at the doorway */
    stacked_level(1,0,0,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && rbt_walk[0].reached==2u && link_open(1,2)==5.0f);
    /* two floors: 10, still lower than it */
    stacked_level(2,0,0,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==10.0f);
    /* three doors, but floor slabs between them (no open floor) */
    stacked_level(3,1,0,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f && !rbt_nav.stacked);
    /* the floor above it closed (no second door), the third open: no gap jumped */
    stacked_level(-1,0,0,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f);
    /* the upper openings elsewhere on that wall, or on another wall */
    stacked_level(3,0,1,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f);
    stacked_level(3,0,2,0);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f);
    /* one tall room on each side, a portal per floor between the same rooms */
    stacked_level(3,1,0,1);
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==3u && link_open(1,2)==15.0f);
    stacked_level(3,0,0,2);                                   /* one tall room on one side only */
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==3u && link_open(1,2)==15.0f);
    stacked_level(3,1,0,2);                                   /* ... and slabs on the other: closed */
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f);
    stacked_level(-1,1,0,1);                                  /* a wall between its doors: no gap jumped */
    goal_at(0,3,1,__LINE__);
    CHECK(seen_goal==1u && link_open(1,2)==5.0f);
}
/* The robot moved by its animation (sub_0015F0D0) through the one-floor
 * doorway: put back before its AI runs; through three open floors: kept. */
static void place(uint32_t ent,float x,uint32_t room) { SETF(ent+0x64u,x);SETF(ent+0x68u,0.0f);SETF(ent+0x6Cu,5.0f);WORD_AT(ent+0x34u)=room; }
static void tick(uint32_t ent,int line) { call(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ent,0,0,1,line); }
/* in room 1 at x 3, wherever it was (a jump there first) */
static void in_room1(int line) { place(ENT(3),-20.0f,1u);tick(ENT(3),line);place(ENT(3),3.0f,1u);tick(ENT(3),line); }
static void hold(void)
{
    nest_ent=0;dies_now=0;
    stacked_level(1,0,0,0);
    place(ENT(3),2.0f,1u);tick(ENT(3),__LINE__);
    place(ENT(3),3.0f,1u);tick(ENT(3),__LINE__);
    CHECK(seen_x==3.0f && FLOAT_AT(ENT(3)+0x64u)==3.0f);       /* inside its room: free */
    SETF(A(3)+0xAF8u,6.0f);SETF(A(3)+0xAFCu,12.0f);SETF(A(3)+0xB00u,5.0f);   /* the eye the room trace saw */
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==3.0f && FLOAT_AT(ENT(3)+0x6Cu)==5.0f && WORD_AT(ENT(3)+0x34u)==1u);
    CHECK(seen_x==3.0f && seen_room==1u && rbt_walk[0].holds==1u);   /* its AI sees it back */
    CHECK(FLOAT_AT(A(3)+0xAF8u)==3.0f && FLOAT_AT(A(3)+0xAFCu)==12.0f && FLOAT_AT(A(3)+0xB00u)==5.0f);   /* next trace from there */
    place(ENT(3),7.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==3.0f && rbt_walk[0].holds==2u);
    /* v0.4.0-mapmaker.45: its room traced at its eye stays 1 through the doorway: held all the same */
    place(ENT(3),6.0f,1u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==3.0f && WORD_AT(ENT(3)+0x34u)==1u && rbt_walk[0].holds==3u);
    place(ENT(3),5.5f,0u);tick(ENT(3),__LINE__);               /* no room known: held too */
    CHECK(FLOAT_AT(ENT(3)+0x64u)==3.0f && rbt_walk[0].holds==4u);
    /* beside the doorway (the wall's portal spans z 0..10): free */
    place(ENT(3),3.0f,1u);SETF(ENT(3)+0x6Cu,15.0f);tick(ENT(3),__LINE__);
    place(ENT(3),6.0f,1u);SETF(ENT(3)+0x6Cu,15.0f);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f && rbt_walk[0].holds==4u);
    /* a floor above (y 6): the one-floor portal is below it, free */
    place(ENT(3),3.0f,1u);SETF(ENT(3)+0x68u,6.0f);tick(ENT(3),__LINE__);
    place(ENT(3),6.0f,1u);SETF(ENT(3)+0x68u,6.0f);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f && rbt_walk[0].holds==4u);
    place(ENT(3),2.0f,1u);tick(ENT(3),__LINE__);               /* back on its floor (a jump: kept) */
    place(ENT(3),40.0f,2u);tick(ENT(3),__LINE__);              /* a teleport: kept */
    CHECK(FLOAT_AT(ENT(3)+0x64u)==40.0f && WORD_AT(ENT(3)+0x34u)==2u);
    place(ENT(3),3.0f,0u);tick(ENT(3),__LINE__);               /* back again: kept */
    CHECK(FLOAT_AT(ENT(3)+0x64u)==3.0f && rbt_walk[0].holds==4u);
    /* the restart puts it back at its spawn: no previous position */
    in_room1(__LINE__);
    CALL(pc_mapmaker_robot_reset,fake,0x0015E7B6u,0,0,0,1);
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f);
    /* respawned (the same record again): likewise */
    in_room1(__LINE__);
    call(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1,__LINE__);
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f);
    /* removed: not held; another entity, mission 10: untouched */
    in_room1(__LINE__);
    rbt_inst[0].removed=1;
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f);
    rbt_inst[0].removed=0;
    actor(9,0x0C,0x400,9);
    place(ENT(9),3.0f,1u);tick(ENT(9),__LINE__);
    place(ENT(9),6.0f,2u);tick(ENT(9),__LINE__);
    CHECK(FLOAT_AT(ENT(9)+0x64u)==6.0f);
    stacked_level(1,0,0,0);
    place(ENT(3),3.0f,1u);tick(ENT(3),__LINE__);
    WORD_AT(0x0079094Cu)=14u;
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f);
    play_scene();
    /* three open floors: through */
    stacked_level(3,0,0,0);
    place(ENT(3),3.0f,1u);tick(ENT(3),__LINE__);
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f && rbt_walk[0].hold_room==2u && !rbt_walk[0].holds && rbt_walk[0].passes==1u);
    /* a robot of one floor: through the one-floor doorway */
    stacked_level(1,0,0,0);
    SETF(ENT(3)+0x1ACu,2.0f);SETF(ENT(3)+0x1C4u,2.0f);
    place(ENT(3),3.0f,1u);tick(ENT(3),__LINE__);
    place(ENT(3),6.0f,2u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f);
    /* a triangular opening (5,0,0) (5,5,0) (5,5,10): through its plane inside
     * its bounds but outside it (z 9, knee height 1): free; inside (z 1): held */
    stacked_level(1,0,0,0);
    HALF_AT(MPORT+0x14u)=3u;
    place(ENT(3),3.0f,1u);SETF(ENT(3)+0x6Cu,9.0f);tick(ENT(3),__LINE__);
    place(ENT(3),6.0f,1u);SETF(ENT(3)+0x6Cu,9.0f);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f && !rbt_walk[0].holds);
    place(ENT(3),6.0f,1u);SETF(ENT(3)+0x6Cu,1.0f);tick(ENT(3),__LINE__);   /* a jump there: kept */
    place(ENT(3),3.0f,1u);SETF(ENT(3)+0x6Cu,1.0f);tick(ENT(3),__LINE__);   /* the other way through it */
    CHECK(FLOAT_AT(ENT(3)+0x64u)==6.0f && rbt_walk[0].holds==1u);
    /* a diagonal doorway (5,0)-(10,10): a move beside it inside its bounds is
     * free (its line would meet the portal farther on); through it: held */
    stacked_level(1,0,0,0);
    SETF(MPORT+0x30u,10.0f);SETF(MPORT+0x38u,10.0f);SETF(MPORT+0x3Cu,10.0f);SETF(MPORT+0x44u,10.0f);
    SETF(MPORT+0x20u,0.0f);SETF(MPORT+0x2Cu,0.0f);
    place(ENT(3),6.0f,1u);tick(ENT(3),__LINE__);
    place(ENT(3),7.0f,1u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==7.0f && !rbt_walk[0].holds);
    place(ENT(3),9.0f,1u);tick(ENT(3),__LINE__);
    CHECK(FLOAT_AT(ENT(3)+0x64u)==7.0f && rbt_walk[0].holds==1u);
}
static void statics(uint32_t a,uint32_t b,uint32_t c) { WORD_AT(STAT)=a;WORD_AT(STAT+4u)=b;WORD_AT(STAT+8u)=c; }
#define STATICS_ARE(a,b,c) (WORD_AT(STAT)==(a) && WORD_AT(STAT+4u)==(b) && WORD_AT(STAT+8u)==(c))

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
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(0),0,0,1);
    CHECK(seen_state==0x00123450u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CHECK(level(__LINE__)==0 && nalloc==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- level start: the set decides; four state blocks ------------------- */
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
    CHECK(nalloc==4 && nparts==1 && WORD_AT(0x0078B0D0u)==PT && alloc_parts==PT);
    CHECK(rbt_lvl.blocks==4 && rbt_inst[0].state==ST(0) && rbt_inst[1].state==ST(1) && rbt_inst[3].state==ST(3));
    CHECK(WORD_AT(0x0078B1DCu)==ST(0) && !rbt_lvl.n);           /* outside any robot: the first block */
    container(0x11,without,2);container_extra(with+1,1);      /* slot 11 */
    CHECK(level(__LINE__)==1 && nalloc==8 && rbt_lvl.blocks==4);
    WORD_AT(0x00784014u)=0;                                   /* unreadable container: assumed */
    CHECK(level(__LINE__)==1 && nalloc==12);
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
    set_parts=PT2;n=nparts;level_allocs=0;                    /* the skeleton/walker made the block */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(nparts==n && WORD_AT(0x0078B0D0u)==PT2 && rbt_lvl.blocks==4);
    set_parts=0;
    alloc_limit=2;level_allocs=0;                             /* memory for two only */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(rbt_lvl.blocks==2 && rbt_inst[1].state==ST(1) && !rbt_inst[2].state && WORD_AT(0x0078B1DCu)==ST(0));
    alloc_limit=99;
    alloc_ok=0;                                               /* no level memory */
    set_parts=PT2;
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(!rbt_lvl.blocks && !rbt_inst[0].state && !WORD_AT(0x0078B1DCu));
    set_parts=0;
    alloc_ok=2;                                               /* an unusable pointer */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(!rbt_lvl.blocks);
    alloc_ok=1;parts_ok=0;                                    /* no particle memory */
    n=nalloc;
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    CHECK(!rbt_lvl.blocks && nalloc==n);
    parts_ok=1;
    container(0x11,with+1,1);                                 /* the robot in slot 1 only */
    CHECK(level(__LINE__)==1 && rbt_lvl.blocks==4);
    container(0x11,with,3);
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    n=nalloc;
    CHECK(level(__LINE__)==0 && nalloc==n);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- AI start: up to four robots, each its own state -------------------- */
    CHECK(level(__LINE__)==1);
    actor(1,0x0C,0x400,0);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && !rbt_lvl.n);
    actor(2,0x30,0x5F2,1);                                    /* the robot model on another id */
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80000u && rbt_lvl.n==1 && rbt_inst[0].record==REC(2) && rbt_inst[0].actor==A(2));
    rbt_inst[0].statics[1]=77u;
    for(k=0;k<4u;k++) {                                       /* three more get states; the fifth does not */
        actor(3+k,0x42,0x5F2,2+k);
        CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3+k),0,0,1);
    }
    CHECK(rbt_lvl.n==4 && rbt_inst[1].record==REC(3) && rbt_inst[3].record==REC(5) && rbt_inst[3].actor==A(5));
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && rbt_lvl.extras==1);   /* robot 5: bot */
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);   /* the same one again (restart) */
    CHECK(seen_class[1]==0x80000u && rbt_lvl.n==4 && rbt_inst[0].record==REC(2) && !rbt_inst[0].statics[1]);
    CHECK(level(__LINE__)==1 && !rbt_lvl.n && !rbt_inst[0].record);   /* a new level forgets them */
    alloc_limit=1;level_allocs=0;                             /* one block: one robot */
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    alloc_limit=99;
    actor(3,0x42,0x5F2,0);actor(4,0x42,0x5F2,1);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(4),0,0,1);
    CHECK(rbt_lvl.n==1 && seen_class[1]==0);
    CHECK(level(__LINE__)==1);
    actor(3,0x42,0x5F2,0);                                    /* its record as spawned again */
    pak_mounted=0;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.n);
    pak_mounted=1;
    alloc_ok=0;level_allocs=0;
    CALL(pc_mapmaker_robot_level,fake_level,0x0015E81Du,0,0,0,1);
    alloc_ok=1;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);   /* no state this level */
    CHECK(seen_class[1]==0 && !rbt_lvl.n);
    CHECK(level(__LINE__)==1);
    BYTE_AT(0x007DE455u)=4u;
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.n);
    play_scene();
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[1]==0 && !rbt_lvl.n);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_robot_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80000u && rbt_inst[0].record==REC(3));

    /* ---- the robot's code runs with that robot's globals -------------------- */
    robot_level(3,__LINE__);                                  /* robots 0..2 on actors 3..5 */
    statics(0xA0u,0xA1u,0xA2u);                               /* outside values */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(3),0,0,1);
    CHECK(seen_state==ST(0) && seen_stat[0]==0 && seen_stat[1]==0 && seen_stat[2]==0);
    CHECK(WORD_AT(0x0078B1DCu)==ST(0) && STATICS_ARE(0xA0u,0xA1u,0xA2u) && rbt_lvl.cur==-1);
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(4),0,0,1);
    CHECK(seen_state==ST(1) && seen_stat[0]==0 && seen_stat[2]==0);
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(3),0,0,1);
    CHECK(seen_state==ST(0) && seen_stat[0]==1u && seen_stat[1]==2u && seen_stat[2]==3u);   /* its own, kept */
    CALL(pc_mapmaker_robot_message,fake_robot,0x0027D44Cu,ENT(5),2u,0,1);
    CHECK(seen_state==ST(2) && seen_stat[0]==0);
    CALL(pc_mapmaker_robot_callback,fake_robot,0x000A5D5Eu,ENT(4),0,0,1);
    CHECK(seen_state==ST(1) && seen_stat[0]==1u && seen_stat[2]==3u);
    WORD_AT(0x00575000u)=ENT(5);                              /* hit record: entity at +0 */
    CALL(pc_mapmaker_robot_damage,fake_robot,0x0009E7A0u,0x00575000u,0,0,1);
    CHECK(seen_state==ST(2) && seen_stat[0]==1u);
    CHECK(WORD_AT(0x0078B1DCu)==ST(0) && STATICS_ARE(0xA0u,0xA1u,0xA2u));
    /* nested: robot 0's code reaches robot 2's messages */
    nest_ent=ENT(5);
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(3),0,0,2);
    nest_ent=0;
    CHECK(seen_state==ST(0) && inner_state==ST(2) && inner_stat[0]==2u && inner_stat[2]==6u);
    CHECK(seen_after[0]==ST(0) && seen_after[1]==3u && seen_after[2]==6u && seen_after[3]==9u);   /* robot 0's again */
    CHECK(rbt_inst[2].statics[0]==102u && rbt_inst[0].statics[0]==3u && STATICS_ARE(0xA0u,0xA1u,0xA2u));
    /* not one of ours: the original as is */
    actor(9,0x42,0x5F2,9);
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(9),0,0,1);
    CHECK(seen_state==ST(0) && seen_stat[0]==0xA0u);
    statics(0xA0u,0xA1u,0xA2u);
    WORD_AT(A(4)+0xBF0u)=REC(8);                              /* robot 1's pool entry reused */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(4),0,0,1);
    CHECK(seen_stat[0]==0xA0u);
    WORD_AT(A(4)+0xBF0u)=REC(4);
    statics(0xA0u,0xA1u,0xA2u);
    actor(8,0x42,0x5F2,8);WORD_AT(A(8)+0xBF0u)=REC(4);        /* another pool entry with robot 1's record */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(8),0,0,1);
    CHECK(seen_stat[0]==0xA0u);
    statics(0xA0u,0xA1u,0xA2u);
    WORD_AT(0x0079094Cu)=14u;                                 /* mission 10 itself */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(3),0,0,1);
    CHECK(seen_stat[0]==0xA0u);
    play_scene();
    statics(0xA0u,0xA1u,0xA2u);

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

    /* ---- death: music, event, removal of the robot that died, explosions ---- */
    WORD_AT(PLAYER+0xFF4u)=ENT(4);
    CALL(pc_mapmaker_robot_music,fake,0x00255E9Du,0u,0,0,1);                 /* the tank's: stock */
    CALL(pc_mapmaker_robot_music,fake,0x002944D9u,0u,0,0,0);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_music,fake,0x002944D9u,0u,0,0,1);
    play_scene();
    /* explosions only while one is dying, before its removal */
    n=ndraw;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n);
    WORD_AT(ST(1)+0x48u)=1u;                                  /* robot 1 dies */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE87u,0,0,0,1);                    /* another caller */
    CHECK(ndraw==n);
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1 && draw_args[0]==0x00515000u && draw_args[1]==0x40200000u && nmode==1 && mode_arg==1u);
    WORD_AT(ST(2)+0x48u)=1u;                                  /* robot 2 too: still one draw a frame */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+2);
    WORD_AT(ST(2)+0x48u)=0;
    WORD_AT(0x00790950u)=2u;                                  /* split screen: stock draws none */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+2);
    WORD_AT(0x00790950u)=1u;
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+2);
    play_scene();
    /* the event: another boss's stays; a robot's is dropped and that robot removed */
    CALL(pc_mapmaker_robot_event,fake,0x0029E068u,3u,0,0,1);
    CHECK(nremove==0);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,1);
    CHECK(nremove==0);
    play_scene();
    dies_now=1;
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(4),0,0,1);   /* robot 1's own frame */
    CHECK(nremove==1 && remove_arg==ENT(4) && WORD_AT(PLAYER+0xFF4u)==0 && rbt_inst[1].removed && !rbt_inst[0].removed);
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(4),0,0,1);
    CHECK(nremove==1);                                        /* once */
    dies_now=0;
    CALL(pc_mapmaker_robot_event,fake,0x002944E5u,2u,0x3F000000u,0,0);   /* outside any robot: dropped only */
    CHECK(nremove==1 && !rbt_inst[0].removed && !rbt_inst[2].removed);
    n=ndraw;
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n);                                          /* gone: no frozen particles */
    WORD_AT(ST(2)+0x48u)=1u;                                  /* but another dying robot draws */
    CALL(pc_mapmaker_robot_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==n+1);
    WORD_AT(PLAYER+0xFF4u)=ENT(3);dies_now=1;                 /* robot 2 dies, robot 0 has the focus */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(5),0,0,1);
    dies_now=0;
    CHECK(nremove==2 && remove_arg==ENT(5) && WORD_AT(PLAYER+0xFF4u)==ENT(3) && !rbt_inst[0].removed);
    dies_now=2;                                               /* robot 0 dies, its pool entry reused meanwhile */
    CALL(pc_mapmaker_robot_handler,fake_robot,0x0027DE1Du,ENT(3),0,0,1);
    dies_now=0;
    CHECK(nremove==2 && rbt_inst[0].removed);
    WORD_AT(A(3)+0xBF0u)=REC(3);
    /* a soft restart resets every robot's state and statics */
    n=nreset;
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);
    CHECK(nreset==n);
    play_scene();
    WORD_AT(ST(1)+0x48u)=1u;WORD_AT(ST(3)+0x48u)=1u;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);
    CHECK(nreset==n+4 && reset_seen[n]==ST(0) && reset_seen[n+1]==ST(1) && reset_seen[n+3]==ST(3));
    CHECK(!rbt_inst[1].removed && !rbt_inst[2].removed && WORD_AT(ST(1)+0x48u)==0 && WORD_AT(ST(3)+0x48u)==0);
    CHECK(!rbt_inst[0].statics[0] && !rbt_inst[2].statics[0] && WORD_AT(0x0078B1DCu)==ST(0));
    robot_level(0,__LINE__);
    n=nreset;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);   /* states but no robot yet: reset too */
    CHECK(nreset==n+4);
    container(0x11,without,2);level(__LINE__);
    n=nreset;
    CALL(pc_mapmaker_robot_reset,fake,0x001CDD79u,0,0,0,1);   /* no robot map */
    CHECK(nreset==n);

    /* ---- the stock bar is never drawn for any of the robots ------------------- */
    robot_level(5,__LINE__);                                  /* robots on 3..6; actor 7 a bot */
    WORD_AT(PLAYER+0xFF4u)=ENT(3);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3));
    WORD_AT(PLAYER+0xFF4u)=ENT(6);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(6));
    WORD_AT(ENT(9)+0x7Cu)=A(3);HALF_AT(ENT(9)+0x94u)=0x400u;  /* not the robot's model */
    WORD_AT(PLAYER+0xFF4u)=ENT(9);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(9));
    actor(1,0x15,0x5BE,3);                                    /* another boss */
    WORD_AT(PLAYER+0xFF4u)=ENT(1);
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(1));
    WORD_AT(PLAYER+0xFF4u)=ENT(7);                            /* the fifth robot (bot class) */
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(7));
    WORD_AT(PLAYER+0xFF4u)=ENT(3);
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(3));
    play_scene();
    WORD_AT(PLAYER+0xFF4u)=0;
    CALL(pc_mapmaker_robot_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0);

    low_door();
    one_floor_door();
    stacked_floors();
    hold();

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("mapmaker robot ok\n");
    return 0;
}
