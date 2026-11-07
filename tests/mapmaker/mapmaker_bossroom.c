/* Asset-free contract for src/interface/pc_mapmaker_bossroom.inc: per view,
 * a giant boss with a reached listed room that is missing from the camera
 * room's prop list is appended to a copy of that list in the view slot's own
 * buffer; nothing else in guest memory changes. The Python side checks the
 * stderr text. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }
void sub_0003DFA0(void) { g_ecx=1; }
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_bossroom.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stdout,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define VIEW0 0x00500000u
#define VIEW1 0x00505000u
#define PROPS 0x00400000u
#define PROP(i) (PROPS+(uint32_t)(i)*0x2E8u)
#define POOL 0x00600000u
#define NROOMS 20u
#define CAM 7u
#define BUF0 0x008D4000u
#define BUF1 0x008D5000u

static void set_limits(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_LIMITS",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_LIMITS",value,1); else unsetenv("TSFP_MAPMAKER_LIMITS");
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void set_off(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_BOSSROOM",value?value:"");
#else
    if(value)setenv("TSFP_MAPMAKER_BOSSROOM",value,1); else unsetenv("TSFP_MAPMAKER_BOSSROOM");
#endif
    bsr.read=0;
}
static void depth(uint32_t view,uint32_t room,int32_t d) { WORD_AT(view+0x1000u+room*12u)=(uint32_t)d; }
static uint32_t prop(unsigned i,uint32_t type,uint16_t model,int8_t listed,uint32_t r0,uint32_t r1)
{
    uint32_t p=PROP(i);
    WORD_AT(p+0x20u)=type;
    *(uint16_t *)((uintptr_t)(p+0x94u)+g_xbox_mem_offset)=model;
    *(int8_t *)((uintptr_t)(p+0x30u)+g_xbox_mem_offset)=listed;
    WORD_AT(p+0x38u)=r0;WORD_AT(p+0x3Cu)=r1;
    return p;
}
/* sub_0009E4B0's result for room r: a pool chunk at slot r*8, count, cap 8. */
static void relink(uint32_t r,unsigned count,const uint32_t *ids)
{
    unsigned i;
    WORD_AT(0x008CD040u+r*4u)=POOL+r*32u;WORD_AT(0x008CC000u+r*4u)=count;WORD_AT(0x008CF0C0u+r*4u)=8u;
    for(i=0;i<count;i++) WORD_AT(POOL+r*32u+i*4u)=ids[i];
}
static uint64_t checksum_outside(void)
{
    uint64_t h=1469598103934665603ull;size_t i;
    for(i=0;i<sizeof memory/4;i++) {
        if(i==(0x008CD040u+CAM*4u)/4u || i==(0x008CC000u+CAM*4u)/4u) continue;
        if(i>=BUF0/4u && i<(BUF0+8u*0x1000u)/4u) continue;
        h^=memory[i];h*=1099511628211ull;
    }
    return h;
}
static int listed_in(uint32_t r,uint32_t p)
{
    uint32_t n=WORD_AT(0x008CC000u+r*4u),l=WORD_AT(0x008CD040u+r*4u),i;
    for(i=0;i<n;i++) if(WORD_AT(l+i*4u)==p) return 1;
    return 0;
}
#define PHASE(s) do { fflush(stderr);puts(s);fflush(stdout); } while(0)

int main(void)
{
    uint32_t skel,human,walker,trophy,object,empty,cam_ids[2],r3_ids[1];uint64_t before;unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x0072F5E8u)=PROPS;WORD_AT(0x007B975Cu)=NROOMS;WORD_AT(0x007B4FACu)=30u;
    WORD_AT(0x00715FB4u)=POOL;WORD_AT(0x0072F298u)=100u;
    WORD_AT(VIEW0+0x2A0u)=CAM;WORD_AT(VIEW1+0x2A0u)=CAM;WORD_AT(VIEW0+0x214u)=0;WORD_AT(VIEW1+0x214u)=1u;
    for(i=0;i<=0x40Cu;i++) { depth(VIEW0,i,-1);depth(VIEW1,i,-1); }
    depth(VIEW0,CAM,0);depth(VIEW0,3,2);depth(VIEW1,CAM,0);depth(VIEW1,3,1);
    for(i=0;i<NROOMS;i++) prop(i,1u,(uint16_t)(0x908u+i),1,i,0);   /* the rooms' own props: never touched */
    prop(5,8u,0x5E9u,1,3,0);   /* a room's own prop with a boss's model, type and list: below the first index */
    skel=prop(22,8u,0x5E9u,2,3,4);human=prop(23,8u,0x634u,2,3,4);walker=prop(24,8u,0x637u,1,9,0);
    trophy=prop(25,8u,0x5BEu,2,CAM,3);object=prop(26,1u,0x5E9u,1,3,0);empty=prop(27,8u,0x5F2u,0,3,0);
    cam_ids[0]=human;cam_ids[1]=trophy;relink(CAM,2,cam_ids);
    r3_ids[0]=human;relink(3,1,r3_ids);

    /* Switch off, the opt-out, other scenes: nothing changes. */
    set_off(NULL);set_limits(NULL);WORD_AT(SCENE)=100u;
    before=checksum_outside();
    mapmaker_bossroom_view(VIEW0);
    set_limits("1");set_off("0");mapmaker_bossroom_view(VIEW0);
    set_off(NULL);WORD_AT(SCENE)=1u;mapmaker_bossroom_view(VIEW0);
    WORD_AT(SCENE)=100u;
    CHECK(WORD_AT(0x008CC000u+CAM*4u)==2u && WORD_AT(0x008CD040u+CAM*4u)==POOL+CAM*32u && checksum_outside()==before);
    PHASE("phase off");

    /* Play: the skeleton (reached room 3, not linked anywhere) goes into a copy
     * of the camera room's list; the trophy is there already; the walker's
     * rooms are not reached; a human, an object and a boss with no list stay out. */
    mapmaker_bossroom_view(VIEW0);
    CHECK(WORD_AT(0x008CD040u+CAM*4u)==BUF0 && WORD_AT(0x008CC000u+CAM*4u)==3u);
    CHECK(WORD_AT(BUF0)==human && WORD_AT(BUF0+4u)==trophy && WORD_AT(BUF0+8u)==skel);
    CHECK(WORD_AT(POOL+CAM*32u+8u)==0u);            /* the pool chunk is not written */
    CHECK(checksum_outside()==before);
    CHECK(!listed_in(CAM,walker) && !listed_in(CAM,object) && !listed_in(CAM,empty) && !listed_in(CAM,PROP(5)));
    mapmaker_bossroom_view(VIEW0);                   /* again: already there */
    CHECK(WORD_AT(0x008CC000u+CAM*4u)==3u);
    mapmaker_bossroom_view(VIEW1);                   /* another view, same camera room: there */
    CHECK(WORD_AT(0x008CD040u+CAM*4u)==BUF0 && WORD_AT(0x008CC000u+CAM*4u)==3u);
    CHECK(bsr.adds==1u);
    PHASE("phase add");

    /* Next frame: the relink puts the lists back; now linked to room 3 (logged
     * once: the linked state changed). Slot 1 uses its own buffer. */
    r3_ids[0]=skel;relink(3,1,r3_ids);relink(CAM,2,cam_ids);
    mapmaker_bossroom_view(VIEW1);
    CHECK(WORD_AT(0x008CD040u+CAM*4u)==BUF1 && WORD_AT(0x008CC000u+CAM*4u)==3u && WORD_AT(BUF1+8u)==skel);
    relink(CAM,2,cam_ids);mapmaker_bossroom_view(VIEW0);   /* same state: quiet */
    CHECK(bsr.adds==3u);
    PHASE("phase relink");

    /* No real camera room; a capacity past the buffer; nothing reached. */
    relink(CAM,2,cam_ids);
    WORD_AT(VIEW0+0x2A0u)=0x40Cu;mapmaker_bossroom_view(VIEW0);
    WORD_AT(VIEW0+0x2A0u)=0u;mapmaker_bossroom_view(VIEW0);
    WORD_AT(VIEW0+0x2A0u)=CAM;
    CHECK(bsr.no_room==4u && WORD_AT(0x008CD040u+CAM*4u)==POOL+CAM*32u);
    WORD_AT(0x008CF0C0u+CAM*4u)=0x401u;mapmaker_bossroom_view(VIEW0);
    CHECK(bsr.full==1u && WORD_AT(0x008CD040u+CAM*4u)==POOL+CAM*32u && WORD_AT(0x008CC000u+CAM*4u)==2u);
    WORD_AT(0x008CF0C0u+CAM*4u)=8u;
    depth(VIEW0,3,-1);mapmaker_bossroom_view(VIEW0);
    CHECK(WORD_AT(0x008CD040u+CAM*4u)==POOL+CAM*32u);
    depth(VIEW0,3,2);
    PHASE("phase skips");

    /* A new level (new bucket pool): logged afresh. A pool near its bound shows in the line. */
    WORD_AT(0x00715FB4u)=POOL+0x10000u;WORD_AT(0x0072F298u)=0x2990u;
    r3_ids[0]=human;relink(3,1,r3_ids);
    mapmaker_bossroom_view(VIEW0);
    CHECK(bsr.adds==1u && WORD_AT(0x008CD040u+CAM*4u)==BUF0);
    PHASE("phase level");

    /* Bad tables: nothing. */
    relink(CAM,2,cam_ids);
    WORD_AT(0x007B4FACu)=5000u;mapmaker_bossroom_view(VIEW0);
    WORD_AT(0x007B4FACu)=30u;WORD_AT(0x0072F5E8u)=0x03FFF000u;mapmaker_bossroom_view(VIEW0);   /* past guest RAM */
    WORD_AT(0x0072F5E8u)=PROPS;
    {
        unsigned long long views=bsr.views,no_room=bsr.no_room;
        mapmaker_bossroom_view(0x00000100u);
        CHECK(bsr.views==views && bsr.no_room==no_room);
    }
    CHECK(WORD_AT(0x008CD040u+CAM*4u)==POOL+CAM*32u);
    PHASE("phase bad");

    /* The pit skeleton's satellites: its gas bottle at its mouth (in no room),
     * a ragbug over a reached room, its rack over an unreached one. With the
     * skeleton reached the bottle and the ragbug ride along; the rack stays;
     * with the skeleton unreached the bottle stays out too. */
    {
        uint32_t bottle,bug,rack;
        WORD_AT(0x00715FB4u)=POOL+0x20000u;WORD_AT(0x0072F298u)=100u;WORD_AT(0x007B4FACu)=31u;
        bottle=prop(28,1u,0x24Au,0,0,0);bug=prop(29,8u,0x6CBu,1,3,0);rack=prop(30,1u,0x246u,1,9,0);
        r3_ids[0]=human;relink(3,1,r3_ids);relink(CAM,2,cam_ids);
        mapmaker_bossroom_view(VIEW0);
        CHECK(listed_in(CAM,skel) && listed_in(CAM,bottle) && listed_in(CAM,bug) && !listed_in(CAM,rack));
        CHECK(bsr.satellites==2u && WORD_AT(0x008CC000u+CAM*4u)==5u);
        relink(CAM,2,cam_ids);depth(VIEW0,3,-1);
        *(uint16_t *)((uintptr_t)(trophy+0x94u)+g_xbox_mem_offset)=0x634u;   /* no boss reached this view */
        mapmaker_bossroom_view(VIEW0);
        CHECK(!listed_in(CAM,skel) && !listed_in(CAM,bottle) && !listed_in(CAM,bug));
        *(uint16_t *)((uintptr_t)(trophy+0x94u)+g_xbox_mem_offset)=0x5BEu;
        depth(VIEW0,3,2);WORD_AT(0x007B4FACu)=30u;
    }
    PHASE("phase satellites");

    if(failures)return 1;
    puts("mapmaker bossroom ok");
    return 0;
}
