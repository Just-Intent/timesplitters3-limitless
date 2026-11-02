/* Asset-free contract for src/interface/pc_mapmaker_khallos.inc: Khallos'
 * flying chair (story mission 4) in MapMaker story play. prop3.pak and
 * lv11.pak mounts, the chair model request, the chair created at a
 * placeholder Story AI (id 0x33) and rebased to it, the rider bound from a
 * nearby Khallos (id 2), the placeholder removed on the first frame, the
 * proximity wake through the start-line constant, the moved frame of the
 * update with its clamped steering box and the world-frame calls out of it,
 * the hit that would trigger the train's death, the dead chair removed, the
 * dropped level events and the bar label. Fake guest memory; fake originals
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
#define PLAYERS 0x00510000u
#define RES 0x00520000u
#define CHAIR 0x00560000u
#define VEC 0x00570000u
#define JETE 0x00577000u
#define PLAYER 0x005A0000u
#define ESP0 0x00600000u
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define PENT(i) (0x00580000u+(uint32_t)(i)*0x400u)
#define PL(i) (PLAYERS+(uint32_t)(i)*0x1584u)
#define PAK 0x0049E1D0u
#define ANIM 0x0049D9C8u
#define LIVE 0x0078F004u
#define LINE 0x00500088u
#define GAP 0x00500094u

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define NEAR(a,b) ((a)-(b)<0.001f && (b)-(a)<0.001f)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=5; }   /* pc_mapmaker_limits.inc */
void sub_00062260(void) { g_ecx=1; }
void sub_000622D0(void) { g_ecx=2; }
void sub_000A6F90(void) { g_ecx=3; }
void sub_0015DDA0(void) { g_ecx=4; }
void sub_000A5E40(void) { g_ecx=6; }
void sub_000631C0(void) { g_ecx=7; }
void sub_0005A130(void) { g_ecx=8; }
void sub_000A9070(void) { g_ecx=9; }
void sub_0010E2A0(void) { g_ecx=10; }
void sub_0010EB90(void) { g_ecx=11; }
/* row 3 = the chair: path PATH(3), request REQ(3) whose +0x10 holds the file when read */
#define PATH(r) (0x00530000u+(uint32_t)(r)*0x40u)
#define REQ(r) (0x00534000u+(uint32_t)(r)*0x40u)
static int req_state[8], req_reads=1, req_after_original;
static unsigned nrequest;
static uint32_t requested;
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }

static int pak_mounted, anim_mounted, mount_ok=1, anim_ok=1, spawn_ok=1, rise_variant, originals;
static unsigned nmount, nanim, nspawn, nremove, ndestroy, nfree, nstop;
static uint32_t mount_args[3], anim_arg, spawn_args[5], remove_arg[4], destroy_arg, free_arg, stop_args[8];
static float spawn_pos[3], spawn_rot[3];
static int spawn_after_original;
static unsigned *chairs_now;                               /* the area's chair count (set in main) */
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0},obj,cn;
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) return ((x[0]==PAK && pak_mounted) || (x[0]==ANIM && anim_mounted))?4u:0xFFFFFFFFu;
    if(f==sub_000622D0) {
        nmount++;memcpy(mount_args,x,sizeof mount_args);
        if(mount_ok && x[0]==PAK) pak_mounted=1;
        return mount_ok?7u:0xFFFFFFFFu;
    }
    if(f==sub_0005A130) { nanim++;anim_arg=x[0];if(anim_ok && x[0]==ANIM) anim_mounted=1;return anim_ok?1u:0; }
    if(f==sub_000A6F90) {   /* sub_000A6EC0: the object at pos/rot, then init case 0x376 */
        nspawn++;memcpy(spawn_args,x,sizeof spawn_args);
        spawn_after_original=originals;
        for(i=0;i<3;i++) { spawn_pos[i]=FLOAT_AT(x[1]+(uint32_t)i*4u);spawn_rot[i]=FLOAT_AT(x[2]+(uint32_t)i*4u); }
        if(!spawn_ok) return 0;
        cn=chairs_now?*chairs_now:0u;
        obj=CHAIR+cn*0x1000u;                       /* a new object per chair of the level */
        for(i=0;i<3;i++) FLOAT_AT(obj+0x64u+(uint32_t)i*4u)=spawn_pos[i];
        memset((void *)((uintptr_t)0x0078F000u+g_xbox_mem_offset),0,0x70);
        if(rise_variant) { FLOAT_AT(LIVE)=-600.0f;WORD_AT(0x0078F054u)=JETE+0x80u;WORD_AT(JETE+0x80u)=0x0078F054u;return obj; }   /* never sets [0x0078F000]; its jet (sub_0010E1C0) */
        WORD_AT(0x0078F000u)=obj;FLOAT_AT(LIVE)=-576.2f;FLOAT_AT(LIVE+4u)=7.5f;FLOAT_AT(LIVE+8u)=0.0f;
        FLOAT_AT(obj+0x1CCu)=60.0f;WORD_AT(0x0078F054u)=JETE+cn*0x100u;
        WORD_AT(0x0078DF50u)=0x00576000u+cn*0x100u;           /* the death's emitters */
        WORD_AT(JETE+cn*0x100u)=0x0078F054u;WORD_AT(0x00576000u+cn*0x100u)=0x0078DF50u;   /* +0: owner slot (sub_00111AE0) */
        return obj;
    }
    if(f==sub_0015DDA0) { if(nremove<4u) remove_arg[nremove]=x[0];nremove++;return 0; }
    if(f==sub_000A9070) { ndestroy++;destroy_arg=x[0];return 0; }
    if(f==sub_0010E2A0) {   /* sub_0010E070: the owner slot is cleared */
        nfree++;free_arg=x[0];
        if(x[0] && WORD_AT(x[0])) { WORD_AT(WORD_AT(x[0]))=0;WORD_AT(x[0])=0; }
        return 0;
    }
    if(f==sub_0010EB90) { if(nstop<8u) stop_args[nstop]=x[0];nstop++;CHECK(x[1]==0);return 0; }
    if(f==sub_000A5E40) {
        int16_t r=(int16_t)HALF_AT(0x008CA000u+x[0]*2u);
        requested=x[0];nrequest++;req_after_original=originals;
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
#define BOS_BAR_PROXY 1                                    /* pc_mapmaker_bosses.inc's hook, as in the build */
static uint32_t (*bos_bar_proxy)(uint32_t entity);
#include "pc_mapmaker_khallos.inc"
/* chair k's own copy of the chair statics / emitters (in guest memory only during its code) */
#define CW(k,a) (khl_ch[k].block[((a)-0x0078F000u)/4u])
#define CF(k,a) (*(float *)&khl_ch[k].block[((a)-0x0078F000u)/4u])

static uint32_t orig_eax, pre_eax=0xDDDDu;
static unsigned inner_events, nworld;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
static void fake_ai(void) { originals++;g_esp+=4u; }
static void fake_event(void) { inner_events++;g_esp+=4u; }
/* sub_000DAC30: the character id's models as it reads them (+4, +8), the entity's model word */
#define CHARS 0x0053A3CCu
static uint32_t made_model, made_lod, made_33;
static void fake_create(void)
{
    uint32_t id=WORD_AT(g_esp+4u);
    originals++;
    made_model=WORD_AT(CHARS+id*0x30u+4u);made_lod=WORD_AT(CHARS+id*0x30u+8u);made_33=WORD_AT(CHARS+0x33u*0x30u+4u);
    HALF_AT(ENT(0)+0x94u)=(uint16_t)made_model;g_eax=ENT(0);
    g_esp+=4u;
}
/* sub_0014CC80: the focus the stock bar saw */
static uint32_t hud_focus;
static void fake_hud(void) { originals++;hud_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }

/* A call from inside a fake original into a hook: ret and args pushed below esp. */
static void inner(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,int line)
{
    uint32_t sp=g_esp;
    g_esp-=16u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a1;WORD_AT(g_esp+8u)=a2;WORD_AT(g_esp+12u)=a3;
    hook(orig);
    if(g_esp!=sp-12u) { fprintf(stderr,"%s:%d: inner esp %+d\n",__FILE__,line,(int)(g_esp-(sp-12u)));failures++; }
    g_esp=sp;
}
/* What a call out of the update saw: its position argument, player 0's y/z,
 * the chair's y/z and live y/z, and whether the hook still said "inside". */
static uint32_t rec_ptr;
static float rec_pos[3], rec_p[2], rec_obj[2], rec_live[2];
static int rec_inside;
static uint32_t world_eax;                                  /* what the sound calls return */
static void fake_world(void)
{
    int i;
    nworld++;g_eax=world_eax;
    for(i=0;i<3;i++) rec_pos[i]=rec_ptr?FLOAT_AT(rec_ptr+(uint32_t)i*4u):0.0f;
    for(i=0;i<2;i++) { rec_p[i]=FLOAT_AT(PL(0)+0xB8u+(uint32_t)i*4u);rec_obj[i]=FLOAT_AT((khl_lv.cur>=0?khl_ch[khl_lv.cur].obj:CHAIR)+0x68u+(uint32_t)i*4u);rec_live[i]=FLOAT_AT(LIVE+4u+(uint32_t)i*4u); }
    rec_inside=khl_lv.inside;
    g_esp+=4u;
}
struct seen { float pos[3], p[2], obj[2], live[2]; int inside; };
static void keep(struct seen *s) { memcpy(s->pos,rec_pos,sizeof rec_pos);memcpy(s->p,rec_p,sizeof rec_p);memcpy(s->obj,rec_obj,sizeof rec_obj);memcpy(s->live,rec_live,sizeof rec_live);s->inside=rec_inside; }
static void call_world(void (*hook)(void (*)(void)),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t pos,struct seen *s,int line)
{
    uint32_t before[3];
    int i;
    for(i=0;i<3;i++) before[i]=pos?WORD_AT(pos+(uint32_t)i*4u):0;
    rec_ptr=pos;
    inner(hook,fake_world,ret,a1,a2,a3,line);
    keep(s);
    for(i=0;i<3;i++) if(pos && WORD_AT(pos+(uint32_t)i*4u)!=before[i]) { fprintf(stderr,"%s:%d: position not restored\n",__FILE__,line);failures++; }
}
/* sub_001EEBD0: the start line, the controller at its target this frame (or
 * climbing 5 m a frame when dead), and the calls out of it. */
static float seen_span, seen_line, seen_gap, seen_live[3], seen_obj[3], seen_p[3], after_p[2], after_live[2], after_obj[2];
static int seen_inside, fire=1;
static uint32_t seen_jet, seen_owner, foreign_jet;
static unsigned nupdate;
static struct seen s_flame, s_flash, s_flash2, s_shot, s_sound, s_engine;
static void fake_update(void)
{
    uint32_t obj=WORD_AT(g_esp+4u),base=WORD_AT(0x007B0C48u),p=base,i;
    float y;
    originals++;nupdate++;
    seen_line=FLOAT_AT(LINE);seen_gap=FLOAT_AT(GAP);seen_span=FLOAT_AT(0x005000B8u);
    if(BYTE_AT(0x007DE455u)==10u && WORD_AT(0x00790950u)>1u && FLOAT_AT(base+0xA8u)>FLOAT_AT(base+0x1584u+0xA8u)) p=base+0x1584u;
    if(!WORD_AT(0x0078F04Cu)) {
        if(FLOAT_AT(p+0xA8u)>FLOAT_AT(LINE)) { g_esp+=4u;return; }
        WORD_AT(0x0078F04Cu)=1u;WORD_AT(PLAYER+0xFF4u)=obj;
        inner(pc_mapmaker_khallos_event,fake_event,0x001EEC72u,0x29u,0,0,__LINE__);
    }
    for(i=0;i<3u;i++) { seen_live[i]=FLOAT_AT(LIVE+i*4u);seen_obj[i]=FLOAT_AT(obj+0x64u+i*4u);seen_p[i]=FLOAT_AT(p+0xB4u+i*4u); }
    seen_inside=khl_lv.inside;
    seen_jet=WORD_AT(0x0078F054u);seen_owner=seen_jet?WORD_AT(seen_jet):0xFFFFFFFFu;
    if(foreign_jet) WORD_AT(0x0078F054u)=foreign_jet;                /* an emitter someone else owns lands in the slot */
    FLOAT_AT(VEC)=seen_live[0];FLOAT_AT(VEC+4u)=seen_live[1]-0.3f;FLOAT_AT(VEC+8u)=seen_live[2];
    call_world(pc_mapmaker_khallos_flame,0x001EED7Au,JETE,VEC,0,VEC,&s_flame,__LINE__);
    for(i=0;i<2u;i++) { after_p[i]=FLOAT_AT(p+0xB8u+i*4u);after_live[i]=FLOAT_AT(LIVE+4u+i*4u);after_obj[i]=FLOAT_AT(obj+0x68u+i*4u); }
    y=FLOAT_AT(obj+0x1CCu)<0.0f?seen_live[1]+5.0f:10.0f;
    FLOAT_AT(LIVE)=FLOAT_AT(p+0xB4u)-FLOAT_AT(GAP);FLOAT_AT(LIVE+4u)=y;FLOAT_AT(LIVE+8u)=FLOAT_AT(obj+0x1CCu)<0.0f?seen_live[2]:0.0f;
    for(i=0;i<3u;i++) FLOAT_AT(obj+0x64u+i*4u)=FLOAT_AT(LIVE+i*4u);
    if(fire) {
        FLOAT_AT(VEC+0x20u)=FLOAT_AT(LIVE);FLOAT_AT(VEC+0x24u)=FLOAT_AT(LIVE+4u);FLOAT_AT(VEC+0x28u)=FLOAT_AT(LIVE+8u)+1.0f;
        call_world(pc_mapmaker_khallos_flash,0x001EF689u,2u,0x40000000u,VEC+0x20u,VEC+0x20u,&s_flash,__LINE__);
        call_world(pc_mapmaker_khallos_flash,0x001EF6EAu,2u,0x40000000u,VEC+0x20u,VEC+0x20u,&s_flash2,__LINE__);
        call_world(pc_mapmaker_khallos_shot,0x001EF75Bu,0x35u,VEC+0x20u,VEC+0x40u,VEC+0x20u,&s_shot,__LINE__);
        for(i=0;i<3u;i++) FLOAT_AT(VEC+0x100u+0x1Cu+i*4u)=FLOAT_AT(obj+0x64u+i*4u);
        call_world(pc_mapmaker_khallos_sound,0x001EF7B4u,VEC+0x100u,0,0,VEC+0x11Cu,&s_sound,__LINE__);
    }
    for(i=0;i<3u;i++) FLOAT_AT(0x00715F98u+i*4u)=FLOAT_AT(obj+0x64u+i*4u);
    call_world(pc_mapmaker_khallos_engine,0x001EF824u,obj+0x22Cu,0x00715F98u,0x394u,0x00715F98u,&s_engine,__LINE__);
    g_esp+=4u;
}
/* sub_00263260: the seated Khallos reads the chair and its live position, writes its sway. */
static uint32_t rider_saw;
static float rider_live;
static void fake_rider(void) { originals++;rider_saw=WORD_AT(0x0078F000u);rider_live=FLOAT_AT(LIVE);WORD_AT(0x0078F060u)=0x5EA7u;g_esp+=4u; }
/* sub_001EF840: health -= damage, events 0x12/0x13, the train's death below 0. */
static unsigned train_death;
static float seen_damage;
static void fake_damage(void)
{
    uint32_t r=WORD_AT(g_esp+4u),obj=WORD_AT(0x0078F000u);
    originals++;
    seen_damage=FLOAT_AT(r+0x20u);
    if(obj && FLOAT_AT(obj+0x1CCu)>=0.0f && WORD_AT(r+0x30u) && WORD_AT(r+0x28u) && WORD_AT(0x0078F04Cu)==1u) {
        FLOAT_AT(obj+0x1CCu)-=FLOAT_AT(r+0x20u);
        if(35.0f>=FLOAT_AT(obj+0x1CCu)) inner(pc_mapmaker_khallos_event,fake_event,0x001EF9A2u,0x12u,0,0,__LINE__);
        if(15.0f>=FLOAT_AT(obj+0x1CCu)) inner(pc_mapmaker_khallos_event,fake_event,0x001EF9C2u,0x13u,0,0,__LINE__);
        if(0.0f>FLOAT_AT(obj+0x1CCu)) { train_death++;inner(pc_mapmaker_khallos_event,fake_event,0x001EFAECu,4u,0,0,__LINE__); }
    }
    g_esp+=4u;
}

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
    khl.read=0;
}
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
    memset((void *)((uintptr_t)A(i)+g_xbox_mem_offset),0,0xEA0u);
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(A(i)+0x14u)=0x11000000u;WORD_AT(A(i)+0x9D0u)=0xFFFFFFFFu;
    WORD_AT(ENT(i)+0x7Cu)=A(i);
    FLOAT_AT(ENT(i)+0x64u)=x;FLOAT_AT(ENT(i)+0x68u)=y;FLOAT_AT(ENT(i)+0x6Cu)=z;FLOAT_AT(ENT(i)+0x70u)=yaw;
}
static void resources(int chair,uint32_t init)
{
    uint32_t m;
    for(m=0;m<0xA00u;m++) HALF_AT(0x008CA000u+m*2u)=0xFFFFu;
    WORD_AT(0x00791D4Cu)=RES;
    if(chair) { HALF_AT(0x008CA000u+0x376u*2u)=3;WORD_AT(RES+3u*0x30u+0x2Cu)=init; }
    for(m=0;m<8u;m++) { WORD_AT(RES+m*0x30u+0x10u)=PATH(m);WORD_AT(RES+m*0x30u+0x14u)=0;WORD_AT(RES+m*0x30u+4u)=0;req_state[m]=0; }
    HALF_AT(0x0078B860u+0x376u*2u)=1;WORD_AT(RES+1u*0x30u+0x2Cu)=0x001F4E00u;   /* the stock map: stale */
}
/* two players: entity (x, z, health) for the wake; +0xA8 and +0xB4..BC */
static void players(float x0,float z0,float h0,float x1,float z1,float h1)
{
    WORD_AT(0x00790950u)=2u;WORD_AT(0x007B0C48u)=PLAYERS;
    WORD_AT(PL(0)+0x14u)=PENT(0);WORD_AT(PL(1)+0x14u)=PENT(1);
    FLOAT_AT(PENT(0)+0x64u)=x0;FLOAT_AT(PENT(0)+0x6Cu)=z0;FLOAT_AT(PENT(0)+0x1CCu)=h0;
    FLOAT_AT(PENT(1)+0x64u)=x1;FLOAT_AT(PENT(1)+0x6Cu)=z1;FLOAT_AT(PENT(1)+0x1CCu)=h1;
    FLOAT_AT(PL(0)+0xA8u)=x0;FLOAT_AT(PL(0)+0xB4u)=x0;FLOAT_AT(PL(0)+0xB8u)=3.7f;FLOAT_AT(PL(0)+0xBCu)=z0;
    FLOAT_AT(PL(1)+0xA8u)=x1+1000.0f;FLOAT_AT(PL(1)+0xB4u)=x1;FLOAT_AT(PL(1)+0xB8u)=0.1f;FLOAT_AT(PL(1)+0xBCu)=z1;
}
static void play_scene(void) { WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u; }
static void constants(void) { FLOAT_AT(LINE)=-535.0f;FLOAT_AT(GAP)=11.0f;FLOAT_AT(0x005000B8u)=30.0f;FLOAT_AT(0x0049E4BCu)=-1.2f; }
/* chair obj's cached rotation (+0x274): identity but for its up row; its angles */
static void pose(uint32_t obj,float ux,float uy,float uz,float yaw,float pitch,float roll)
{
    unsigned i;
    for(i=0;i<12u;i++) FLOAT_AT(obj+0x274u+i*4u)=0.0f;
    FLOAT_AT(obj+0x274u)=1.0f;FLOAT_AT(obj+0x29Cu)=1.0f;
    FLOAT_AT(obj+0x284u)=ux;FLOAT_AT(obj+0x288u)=uy;FLOAT_AT(obj+0x28Cu)=uz;
    FLOAT_AT(obj+0x70u)=yaw;FLOAT_AT(obj+0xC0u)=pitch;FLOAT_AT(obj+0xC8u)=roll;
}
/* a new level: chair placeholder at (40, 2, 30), Khallos 3 m off */
static const uint16_t with_both[]={0x0C,0x33,0x02};
static void level(int rider_first,int line)
{
    unsigned s=nspawn;
    play_scene();
    container(0x11,with_both,3,NULL,0);
    resources(1,0x001F4E00u);
    pak_mounted=0;anim_mounted=0;WORD_AT(0x006D7880u)=10u;WORD_AT(0x006D7C4Cu)=0;
    call(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1,line);
    call(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1,line);
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);
    actor(1,0x02,42.0f,2.0f,32.0f,0.0f);
    WORD_AT(A(1)+0x10u)=1u;                                       /* MapMaker's class 0x10000000/1 */
    if(rider_first) call(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1,line);
    call(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1,line);
    if(!rider_first) call(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1,line);
    if(nspawn!=s+1u) { fprintf(stderr,"%s:%d: chair not spawned\n",__FILE__,line);failures++; }
}
#define UPDATE(n) CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR,0,0,n)
/* a new level whose set holds the chair only; the placeholder made (as Khallos when it can be) and started */
static const uint16_t chair_set[]={0x0C,0x33};
static void own_level(int line)
{
    play_scene();
    container(0x11,chair_set,2,NULL,0);
    resources(1,0x001F4E00u);
    HALF_AT(0x008CA000u+0x582u*2u)=4;                             /* Khallos' row */
    pak_mounted=0;anim_mounted=0;WORD_AT(0x006D7880u)=10u;WORD_AT(0x006D7C4Cu)=0;
    WORD_AT(CHARS+2u*0x30u+4u)=0x582u;WORD_AT(CHARS+2u*0x30u+8u)=0xFFFFFFFFu;
    WORD_AT(CHARS+0x33u*0x30u+4u)=0x619u;WORD_AT(CHARS+0x33u*0x30u+8u)=0x61Au;
    call(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1,line);
    call(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1,line);
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);HALF_AT(ENT(0)+0x94u)=0;
    call(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,1u,1,line);
    call(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1,line);
}

int main(void)
{
    static const uint16_t chair_only[]={0x0C,0x33}, neither[]={0x0C,0xA8}, rider_only[]={0x0C,0x02}, extra[]={0x30,0x33};
    unsigned n,k;
    float dy,dz,tx;
    uint32_t r;
    g_xbox_mem_offset=(uintptr_t)memory;
    chairs_now=&khl_lv.n;
    constants();
    WORD_AT(0x007B0C7Cu)=PLAYER;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_KHALLOS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    play_scene();
    container(0x11,with_both,3,NULL,0);
    resources(1,0x001F4E00u);
    WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nmount==0 && nanim==0 && nrequest==0);
    pak_mounted=1;
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    CALL(pc_mapmaker_khallos_event,fake,0x001EEC72u,0x29u,0,0,1);
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_khallos_text,fake,0x0014CDA6u,0xFF1u,0,0,1)==0x1234u);
    CALL(pc_mapmaker_khallos_flame,fake,0x001EED7Au,JETE,VEC,0,1);
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- mount: prop3.pak -------------------------------------------------- */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1 && mount_args[0]==PAK && mount_args[1]==0 && mount_args[2]==1u && pak_mounted);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);    /* already mounted */
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x006D7880u)=57u;                       /* 7 free: kept for stock */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1);
    WORD_AT(0x006D7880u)=56u;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    container(0x11,rider_only,2,NULL,0);                          /* Khallos alone: no chair */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x0C,with_both,3,NULL,0);                           /* a preset set */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x11,neither,2,extra,2);                            /* slot 12 */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==3 && pak_mounted);
    pak_mounted=0;
    container(0x11,with_both,3,NULL,0);
    BYTE_AT(CONT+8u+8u+0x16u)=1u;                                 /* slot count 1: id 0x0C only */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==3);
    WORD_AT(CONT)=0;                                              /* unreadable container */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4 && pak_mounted);
    pak_mounted=0;
    container(0x11,with_both,3,NULL,0);
    BYTE_AT(0x007DE455u)=3u;                                      /* not story */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();WORD_AT(0x0079094Cu)=11u;                        /* the train itself */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();
    set_env("TSFP_MAPMAKER_KHALLOS","0");
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nmount==4 && nanim==0 && nrequest==0);
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);pak_mounted=1;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_KHALLOS",NULL);
    mount_ok=0;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==5 && !pak_mounted);
    mount_ok=1;
    WORD_AT(0x006D7C4Cu)=1u;                                      /* the original does nothing */
    khl_ch[0].obj=0x1234u;khl_lv.n=1u;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==5 && khl_ch[0].obj==0x1234u);
    WORD_AT(0x006D7C4Cu)=0;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==6 && !khl_ch[0].obj && !khl_lv.n);                              /* a new level forgets the chair */

    /* ---- anims: lv11.pak with the chair in the set (its placeholder rides it) ---- */
    container(0x11,rider_only,2,NULL,0);                          /* Khallos alone: no chair */
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==0);
    container(0x11,neither,2,extra,2);                            /* the chair in slot 12 */
    WORD_AT(0x006D7880u)=57u;
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==0);
    WORD_AT(0x006D7880u)=10u;
    anim_ok=0;
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==1 && !anim_mounted);
    anim_ok=1;
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==2 && anim_arg==ANIM && anim_mounted);
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);    /* already mounted */
    CHECK(nanim==2);
    anim_mounted=0;
    container(0x11,chair_only,2,NULL,0);                          /* the chair without Khallos */
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==3 && anim_mounted);
    anim_mounted=0;
    WORD_AT(CONT)=0;                                              /* unreadable */
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==4 && anim_mounted);
    anim_mounted=0;
    container(0x11,with_both,3,NULL,0);
    WORD_AT(0x0079094Cu)=102u;                                    /* the editor */
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nanim==4);
    play_scene();

    /* ---- request ------------------------------------------------------------ */
    container(0x11,neither,2,NULL,0);
    pak_mounted=1;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    container(0x11,with_both,3,NULL,0);
    pak_mounted=0;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    pak_mounted=1;
    resources(0,0x001F4E00u);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    resources(1,0x001F4E00u);
    WORD_AT(0x0079094Cu)=11u;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    play_scene();
    req_reads=0;
    n=originals;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==1 && requested==0x376u && req_after_original==(int)n+1 && khl_lv.requested);

    /* ---- ai_start: the placeholder becomes the chair ------------------------- */
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* requested, not read */
    CHECK(nspawn==0);
    req_reads=1;resources(1,0x001F4E00u);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* never requested */
    CHECK(nspawn==0);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    WORD_AT(RES+3u*0x30u+0x14u)=PATH(7);                                 /* a second path, unrequested */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    req_state[7]=2;
    pak_mounted=0;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    pak_mounted=1;
    WORD_AT(RES+3u*0x30u+0x2Cu)=0x002560C0u;                             /* another init */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    WORD_AT(RES+3u*0x30u+0x2Cu)=0x001F4E00u;
    actor(2,0x34,40.0f,2.0f,30.0f,1.5f);                                 /* another character */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(nspawn==0);
    resources(1,0x001F4E00u);
    WORD_AT(RES+3u*0x30u+4u)=0x00539000u;                                /* loaded before */
    spawn_ok=0;
    WORD_AT(0x0078F000u)=0;FLOAT_AT(LIVE)=-1.0f;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==1 && !khl_lv.n && FLOAT_AT(LIVE)==-1.0f && ndestroy==0);
    spawn_ok=1;rise_variant=1;FLOAT_AT(LIVE)=-1.0f;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==2 && !khl_lv.n && ndestroy==1 && destroy_arg==CHAIR && FLOAT_AT(LIVE)==-1.0f);   /* outside values back */
    CHECK(khl.slots && !WORD_AT(khl.slots) && !WORD_AT(JETE+0x80u));    /* its emitter lost its owner, no slot holds it */
    rise_variant=0;
    n=originals;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==3 && khl_lv.n==1u && khl_ch[0].obj==CHAIR && spawn_after_original==(int)n+1);
    CHECK(spawn_args[0]==0x376u && spawn_args[3]==0 && spawn_args[4]==0);
    CHECK(NEAR(spawn_pos[0],40.0f) && NEAR(spawn_pos[1],2.0f) && NEAR(spawn_pos[2],30.0f));
    CHECK(NEAR(spawn_rot[0],0.0f) && NEAR(spawn_rot[1],1.5f) && NEAR(spawn_rot[2],0.0f));
    CHECK(CF(0,LIVE)==40.0f && CF(0,LIVE+4u)==3.0f && CF(0,LIVE+8u)==30.0f && CW(0,0x0078F000u)==CHAIR && FLOAT_AT(LIVE)!=40.0f);   /* spawn + lift */
    CHECK(FLOAT_AT(CHAIR+0x64u)==40.0f && FLOAT_AT(CHAIR+0x68u)==3.0f && FLOAT_AT(CHAIR+0x6Cu)==30.0f);
    actor(2,0x33,10.0f,1.0f,-20.0f,0.0f);                                /* a second placeholder: a second chair */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(nspawn==4 && khl_lv.n==2u && khl_ch[0].obj==CHAIR && khl_ch[1].obj==CHAIR+0x1000u && khl_ch[1].place==A(2));
    CHECK(CF(1,LIVE)==10.0f && CF(1,LIVE+4u)==2.0f && CF(1,LIVE+8u)==-20.0f && CW(1,0x0078F000u)==CHAIR+0x1000u);
    CHECK(CF(0,LIVE)==40.0f && CW(0,0x0078F054u)==JETE && CW(1,0x0078F054u)==JETE+0x100u);   /* each its own */
    CHECK(khl_ch[0].smoke[0]==0x00576000u && khl_ch[1].smoke[0]==0x00576100u);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);   /* the same placeholder again */
    CHECK(nspawn==4 && khl_lv.n==2u);
    for(k=0;k<2u;k++) {                                                  /* up to four; the fifth stays a bot */
        actor(6+k,0x33,(float)k,1.0f,0.0f,0.0f);
        CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(6+k),0,0,1);
    }
    CHECK(nspawn==6 && khl_lv.n==4u && khl_ch[3].obj==CHAIR+0x3000u);
    actor(8,0x33,5.0f,1.0f,0.0f,0.0f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(8),0,0,1);
    CHECK(nspawn==6 && khl_lv.n==4u && khl_lv.extras==1u);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(8),0,0,1);
    CHECK(nspawn==6);
    play_scene();

    /* ---- text: the bar label -------------------------------------------------- */
    r=CALL(pc_mapmaker_khallos_text,fake,0x0014CDA6u,0xFF1u,0,0,0);
    CHECK(r && !strcmp((const char *)((uintptr_t)r+g_xbox_mem_offset),"Khallos"));
    orig_eax=0x4321u;
    CHECK(CALL(pc_mapmaker_khallos_text,fake,0x00300000u,0xFF1u,0,0,1)==0x4321u);   /* other caller */
    CHECK(CALL(pc_mapmaker_khallos_text,fake,0x0014CDA6u,0xFF2u,0,0,1)==0x4321u);
    BYTE_AT(0x007DE455u)=3u;
    CHECK(CALL(pc_mapmaker_khallos_text,fake,0x0014CDA6u,0xFF1u,0,0,1)==0x4321u);
    play_scene();

    /* ---- rider: a Khallos near the chair, with lv11.pak ------------------------ */
    level(1,__LINE__);                                                   /* Khallos started first */
    CHECK(khl_ch[0].rider==A(1) && WORD_AT(A(1)+0x10u)==0 && WORD_AT(A(1)+0x14u)==8u && WORD_AT(A(1)+0x9D0u)==0);
    level(0,__LINE__);                                                   /* Khallos started after */
    CHECK(khl_ch[0].rider==A(1) && WORD_AT(A(1)+0x14u)==8u);
    actor(3,0x02,40.5f,2.0f,30.5f,0.0f);                                 /* nearer, but a rider exists */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CHECK(khl_ch[0].rider==A(1) && WORD_AT(A(3)+0x14u)==0x11000000u);
    /* the nearest of two */
    play_scene();
    container(0x11,with_both,3,NULL,0);
    resources(1,0x001F4E00u);pak_mounted=0;anim_mounted=0;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    actor(1,0x02,43.0f,2.0f,33.0f,0.0f);                                 /* 4.2 m */
    actor(3,0x02,41.0f,2.0f,29.0f,0.0f);                                 /* 1.4 m */
    actor(4,0x02,40.0f,2.0f,35.5f,0.0f);                                 /* 5.5 m */
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(4),0,0,1);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(khl_ch[0].rider==A(3) && WORD_AT(A(1)+0x14u)==0x11000000u && WORD_AT(A(4)+0x14u)==0x11000000u);
    /* too far, or the entry reused, or no anims: no rider */
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    actor(4,0x02,40.0f,2.0f,35.5f,0.0f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(4),0,0,1);
    actor(5,0x34,40.5f,2.0f,30.5f,0.0f);                                 /* near, but not Khallos */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(5),0,0,1);
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(khl_ch[0].obj==CHAIR && !khl_ch[0].rider && WORD_AT(A(4)+0x14u)==0x11000000u && WORD_AT(A(5)+0x14u)==0x11000000u);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    actor(1,0x02,41.0f,2.0f,31.0f,0.0f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    WORD_AT(A(1)+0xBF0u)=REC(6);                                         /* the pool entry was reused */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(khl_ch[0].obj==CHAIR && !khl_ch[0].rider && WORD_AT(A(1)+0x14u)==0x11000000u);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    anim_mounted=0;
    actor(1,0x02,41.0f,2.0f,31.0f,0.0f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(khl_ch[0].obj==CHAIR && !khl_ch[0].rider && WORD_AT(A(1)+0x14u)==0x11000000u);

    /* ---- update: placeholder removed, waiting, the wake ------------------------- */
    level(1,__LINE__);
    constants();
    players(100.0f,100.0f,1.0f,100.0f,100.0f,1.0f);                       /* far */
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,0x00570000u,0,0,1);   /* another object */
    CHECK(nremove==0 && seen_line==-535.0f);
    n=nupdate;
    UPDATE(1);
    CHECK(nremove==1 && remove_arg[0]==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0);
    CHECK(seen_line<-1e30f && FLOAT_AT(LINE)==-535.0f && !CW(0,0x0078F04Cu));   /* forced "not yet", restored */
    CHECK(CF(0,LIVE+4u)==3.0f && CF(0,LIVE+8u)==30.0f);                  /* not moved */
    UPDATE(1);
    CHECK(nremove==1);
    players(40.0f+20.0f,30.0f+20.0f,0.0f,100.0f,100.0f,1.0f);             /* 28 m but dead */
    UPDATE(1);
    CHECK(!CW(0,0x0078F04Cu));
    players(40.0f-1.0f,30.0f+31.0f,1.0f,100.0f,100.0f,1.0f);              /* 31 m */
    UPDATE(1);
    CHECK(!CW(0,0x0078F04Cu));
    players(100.0f,100.0f,1.0f,40.0f+29.0f,30.0f,1.0f);                   /* the second player, 29 m (x) */
    WORD_AT(0x00790950u)=2u;
    inner_events=0;
    UPDATE(1);
    CHECK(CW(0,0x0078F04Cu)==1u && WORD_AT(PLAYER+0xFF4u)==CHAIR && seen_line>1e30f && FLOAT_AT(LINE)==-535.0f);
    CHECK(inner_events==0 && khl_lv.events>=1u);                          /* event 0x29 dropped */
    CHECK(nupdate==n+5u);

    /* ---- update: the moved frame ------------------------------------------------- */
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);                        /* first player steered */
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    FLOAT_AT(PL(0)+0xB8u)=0.1f;FLOAT_AT(PL(0)+0xBCu)=33.1f;              /* values that do not survive (v-d)+d */
    FLOAT_AT(PL(1)+0xB8u)=2.3f;FLOAT_AT(PL(1)+0xBCu)=100.3f;
    CF(0,LIVE)=40.0f;CF(0,LIVE+4u)=4.0f;CF(0,LIVE+8u)=31.0f;FLOAT_AT(CHAIR+0x68u)=4.0f;FLOAT_AT(CHAIR+0x6Cu)=31.0f;
    dy=2.0f+1.0f-7.5f;dz=33.1f;tx=45.0f-11.0f;
    UPDATE(1);
    CHECK(seen_inside==1 && !khl_lv.inside);
    CHECK(NEAR(seen_gap,45.0f-tx) && FLOAT_AT(GAP)==11.0f && FLOAT_AT(LINE)==-535.0f);
    CHECK(NEAR(seen_live[1],4.0f-dy) && NEAR(seen_live[2],31.0f-dz) && NEAR(seen_obj[1],4.0f-dy) && NEAR(seen_obj[2],31.0f-dz));
    CHECK(NEAR(seen_p[0],45.0f) && NEAR(seen_p[1],0.1f-dy) && NEAR(seen_p[2],0.0f));
    /* after a call out: the moved frame again, bit for bit */
    CHECK(after_p[0]==seen_p[1] && after_p[1]==seen_p[2] && after_live[0]==seen_live[1] && after_live[1]==seen_live[2]);
    CHECK(after_obj[0]==seen_obj[1] && after_obj[1]==seen_obj[2]);
    CHECK(NEAR(CF(0,LIVE),tx) && NEAR(CF(0,LIVE+4u),10.0f+dy) && NEAR(CF(0,LIVE+8u),dz));   /* world target */
    CHECK(NEAR(FLOAT_AT(CHAIR+0x64u),tx) && NEAR(FLOAT_AT(CHAIR+0x68u),10.0f+dy) && NEAR(FLOAT_AT(CHAIR+0x6Cu),dz));
    CHECK(FLOAT_AT(PL(0)+0xB8u)==0.1f && FLOAT_AT(PL(0)+0xBCu)==33.1f);   /* players bit for bit */
    CHECK(FLOAT_AT(PL(1)+0xB8u)==2.3f && FLOAT_AT(PL(1)+0xBCu)==100.3f && FLOAT_AT(PL(0)+0xB4u)==45.0f);
    /* every call out of it saw the world */
    CHECK(NEAR(s_flame.pos[0],40.0f) && NEAR(s_flame.pos[1],4.0f-0.3f) && NEAR(s_flame.pos[2],31.0f));
    CHECK(s_flame.p[0]==0.1f && s_flame.p[1]==33.1f && NEAR(s_flame.obj[0],4.0f) && NEAR(s_flame.live[1],31.0f) && !s_flame.inside);
    CHECK(NEAR(s_flash.pos[1],10.0f+dy) && NEAR(s_flash.pos[2],dz+1.0f) && NEAR(s_flash.obj[0],10.0f+dy) && s_flash.p[0]==0.1f);
    CHECK(NEAR(s_flash2.pos[2],dz+1.0f) && NEAR(s_flash2.live[1],dz));
    CHECK(NEAR(s_shot.pos[0],tx) && NEAR(s_shot.pos[2],dz+1.0f) && s_shot.p[1]==33.1f && NEAR(s_shot.obj[1],dz));
    CHECK(NEAR(s_sound.pos[1],10.0f+dy) && NEAR(s_sound.pos[2],dz) && !s_sound.inside);
    CHECK(NEAR(s_engine.pos[1],10.0f+dy) && NEAR(s_engine.pos[2],dz) && NEAR(s_engine.live[0],10.0f+dy) && s_engine.p[1]==33.1f);
    /* the steering box: x and z clamped around the spawn */
    players(80.0f,90.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    UPDATE(1);
    CHECK(NEAR(CF(0,LIVE),40.0f+12.0f) && NEAR(CF(0,LIVE+8u),30.0f+12.0f) && NEAR(seen_gap,80.0f-52.0f));
    players(-10.0f,-50.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    UPDATE(1);
    CHECK(NEAR(CF(0,LIVE),40.0f-12.0f) && NEAR(CF(0,LIVE+8u),30.0f-12.0f));
    /* the second player when the first one's +0xA8 is larger (story, two players) */
    players(45.0f,33.0f,1.0f,50.0f,25.0f,1.0f);
    FLOAT_AT(PL(0)+0xA8u)=10.0f;FLOAT_AT(PL(1)+0xA8u)=5.0f;
    UPDATE(1);
    CHECK(NEAR(CF(0,LIVE),50.0f-11.0f) && NEAR(CF(0,LIVE+8u),25.0f) && NEAR(seen_p[2],0.0f));
    WORD_AT(0x00790950u)=1u;                                              /* one player: always the first */
    UPDATE(1);
    CHECK(NEAR(CF(0,LIVE),45.0f-11.0f) && NEAR(CF(0,LIVE+8u),33.0f));
    WORD_AT(0x00790950u)=2u;
    /* calls out of other code, or from other places: untouched */
    CHECK(khl_lv.d[1]!=0.0f && khl_lv.d[2]!=0.0f);                     /* a frame they could wrongly use */
    nworld=0;
    FLOAT_AT(VEC)=1.0f;FLOAT_AT(VEC+4u)=2.0f;FLOAT_AT(VEC+8u)=3.0f;
    FLOAT_AT(VEC+0x11Cu)=1.0f;FLOAT_AT(VEC+0x120u)=2.0f;FLOAT_AT(VEC+0x124u)=3.0f;
#define UNMOVED(hook,ret,a1,a2,a3,pos) do { rec_ptr=(pos);rec_pos[1]=rec_pos[2]=0; \
        CALL(hook,fake_world,ret,a1,a2,a3,-1);CHECK(rec_pos[1]==2.0f && rec_pos[2]==3.0f); } while(0)
    UNMOVED(pc_mapmaker_khallos_flame,0x001EED7Au,JETE,VEC,0,VEC);     /* not inside the update */
    UNMOVED(pc_mapmaker_khallos_shot,0x001EF75Bu,0x35u,VEC,0,VEC);
    UNMOVED(pc_mapmaker_khallos_engine,0x001EF824u,0,VEC,0,VEC);
    khl_lv.inside=1;
    UNMOVED(pc_mapmaker_khallos_flame,0x00123456u,JETE,VEC,0,VEC);     /* other callers */
    UNMOVED(pc_mapmaker_khallos_flash,0x001EF68Au,2u,0,VEC,VEC);
    UNMOVED(pc_mapmaker_khallos_shot,0x001EF75Cu,0x35u,VEC,0,VEC);
    UNMOVED(pc_mapmaker_khallos_sound,0x001EF7B5u,VEC+0x100u,0,0,VEC+0x11Cu);
    UNMOVED(pc_mapmaker_khallos_engine,0x001EF825u,0,VEC,0,VEC);
    khl_lv.inside=0;
    CHECK(nworld==8u && FLOAT_AT(VEC+4u)==2.0f && FLOAT_AT(VEC+8u)==3.0f);

    /* ---- damage: hits, and the hit that would trigger the train's death ---------- */
    {
        uint32_t hit=0x00575000u;
        WORD_AT(hit)=CHAIR;WORD_AT(hit+0x28u)=VEC;WORD_AT(hit+0x30u)=VEC+0x40u;
        FLOAT_AT(CHAIR+0x1CCu)=60.0f;FLOAT_AT(hit+0x20u)=20.0f;
        inner_events=0;n=khl_lv.events;
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==40.0f && inner_events==0 && khl_lv.events==n);
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==20.0f && inner_events==0 && khl_lv.events==n+1u);   /* 0x12 dropped */
        FLOAT_AT(hit+0x20u)=8.0f;
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==12.0f && inner_events==0 && khl_lv.events==n+3u);   /* 0x12 and 0x13 */
        WORD_AT(hit+0x30u)=0;FLOAT_AT(hit+0x20u)=50.0f;                   /* no direction: stock ignores it */
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==12.0f && seen_damage==50.0f);
        WORD_AT(hit+0x30u)=VEC+0x40u;
        CW(0,0x0078F04Cu)=0;                                           /* not active: stock ignores it */
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==12.0f && seen_damage==50.0f);
        CW(0,0x0078F04Cu)=1u;
        khl_ch[0].smoke[0]=0x00576000u;khl_ch[0].smoke[1]=0;khl_ch[0].smoke[2]=0x00576100u;
        WORD_AT(PLAYER+0xFF4u)=CHAIR;nstop=0;
        FLOAT_AT(hit+0x20u)=12.5f;
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        CHECK(train_death==0 && seen_damage==12.0f && FLOAT_AT(hit+0x20u)==12.5f);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==-0.5f && WORD_AT(PLAYER+0xFF4u)==0);
        CHECK(nstop==3u && stop_args[0]==0x00576000u && stop_args[1]==0 && stop_args[2]==0x00576100u);
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);   /* dead: stock ignores it */
        CHECK(train_death==0 && FLOAT_AT(CHAIR+0x1CCu)==-0.5f && nstop==3u);
        WORD_AT(0x0078F000u)=0x00570000u;WORD_AT(hit)=0x00570000u;WORD_AT(0x0078F04Cu)=1u;                                 /* someone else's chair */
        FLOAT_AT(0x00570000u+0x1CCu)=5.0f;
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BC000u,hit,0,0,1);
        WORD_AT(hit)=CHAIR;
        CHECK(nstop==3u && seen_damage==12.5f && FLOAT_AT(0x00570000u+0x1CCu)==-7.5f);
    }

    /* ---- the dead chair climbs and is removed, with its rider ------------------ */
    fire=0;
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    CF(0,LIVE+4u)=3.0f;
    WORD_AT(PLAYER+0xFF4u)=CHAIR;
    for(k=0;k<4u;k++) UPDATE(1);                                          /* 3 + 5 a frame: 23 m */
    CHECK(ndestroy==1 && !khl_ch[0].removed);
    UPDATE(1);                                                            /* 28 m: past 25 above the spawn */
    CHECK(ndestroy==2 && destroy_arg==CHAIR && khl_ch[0].removed && nfree==1 && free_arg==JETE && !CW(0,0x0078F054u));
    CHECK(!CW(0,0x0078F000u) && WORD_AT(PLAYER+0xFF4u)==0 && nremove==2 && remove_arg[1]==ENT(1) && !khl_ch[0].rider);
    n=nupdate;
    UPDATE(0);                                                            /* removed: skipped */
    CHECK(nupdate==n && ndestroy==2);
    /* removal by time when it cannot climb */
    level(1,__LINE__);
    UPDATE(1);
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    CW(0,0x0078F04Cu)=1u;
    FLOAT_AT(CHAIR+0x1CCu)=-1.0f;
    actor(1,0x02,0.0f,0.0f,0.0f,0.0f);WORD_AT(A(1)+0xBF0u)=REC(7);       /* the rider's entry was reused */
    n=ndestroy;
    for(k=0;k<239u;k++) { CF(0,LIVE+4u)=3.0f;UPDATE(1); }
    CHECK(ndestroy==n && !khl_ch[0].removed);
    CF(0,LIVE+4u)=3.0f;
    UPDATE(1);
    CHECK(ndestroy==n+1u && khl_ch[0].removed && nremove==3u);               /* only the placeholder: rider gone */
    fire=1;

    /* ---- events ------------------------------------------------------------------ */
    n=khl_lv.events;
    CALL(pc_mapmaker_khallos_event,fake,0x001EEC72u,0x29u,0,0,0);
    CALL(pc_mapmaker_khallos_event,fake,0x001EF9A2u,0x12u,0,0,0);
    CALL(pc_mapmaker_khallos_event,fake,0x001EF9C2u,0x13u,0,0,0);
    CALL(pc_mapmaker_khallos_event,fake,0x001EFAECu,4u,0,0,0);
    CHECK(khl_lv.events==n+4u);
    CALL(pc_mapmaker_khallos_event,fake,0x00255A94u,0x16u,0,0,1);       /* someone else's */
    CALL(pc_mapmaker_khallos_event,fake,0x001EEC6Du,0x29u,0,0,1);
    WORD_AT(0x0079094Cu)=11u;
    CALL(pc_mapmaker_khallos_event,fake,0x001EEC72u,0x29u,0,0,1);       /* the train keeps it */
    play_scene();

    /* ---- two chairs: each runs with its own globals ------------------------------------ */
    level(1,__LINE__);                                                   /* chair 0 at (40, 2, 30), rider A(1) */
    actor(12,0x33,41.0f,2.0f,31.0f,0.0f);                               /* a chair next to chair 0: the rider is taken */
    WORD_AT(0x0078DF54u)=0x0BADu;                                        /* a leftover emitter outside */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(12),0,0,1);
    CHECK(khl_lv.n==2u && khl_ch[0].rider==A(1) && !khl_ch[1].rider && !khl_ch[1].smoke[1] && WORD_AT(0x0078DF54u)==0x0BADu);
    level(1,__LINE__);
    actor(10,0x33,-30.0f,0.0f,-30.0f,0.0f);
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(10),0,0,1);
    CHECK(khl_lv.n==2u && khl_ch[1].obj==CHAIR+0x1000u && !khl_ch[1].rider);   /* the one Khallos rides chair 0 */
    players(-28.0f,-31.0f,1.0f,100.0f,100.0f,1.0f);                       /* near chair 1 only */
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    FLOAT_AT(LIVE)=999.0f;WORD_AT(0x0078F04Cu)=7u;WORD_AT(0x0078F000u)=0x00123450u;   /* outside values */
    n=nremove;
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(NEAR(s_flame.obj[0],1.0f) && NEAR(s_flame.obj[1],-30.0f));    /* the world call saw chair 1, unshifted */
    CHECK(nremove==n+1u && remove_arg[n<4u?n:3u]==ENT(10));             /* its own placeholder */
    CHECK(seen_live[0]==-30.0f && CW(1,0x0078F04Cu)==1u && CW(0,0x0078F04Cu)==0);   /* chair 1 woke, chair 0 waits */
    CHECK(NEAR(CF(1,LIVE),-39.0f) && NEAR(CF(1,LIVE+4u),3.5f) && NEAR(CF(1,LIVE+8u),-31.0f));   /* around its own spawn */
    CHECK(CF(0,LIVE)==40.0f && CF(0,LIVE+4u)==3.0f);
    CHECK(FLOAT_AT(LIVE)==999.0f && WORD_AT(0x0078F04Cu)==7u && WORD_AT(0x0078F000u)==0x00123450u);   /* put back */
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR,0,0,1);   /* chair 0: still waiting */
    CHECK(seen_line<-1e30f && CW(0,0x0078F04Cu)==0 && WORD_AT(0x0078F04Cu)==7u);
    /* a hit on chair 1 runs with chair 1's globals; chair 0 untouched */
    {
        uint32_t hit=0x00575000u;
        WORD_AT(hit)=CHAIR+0x1000u;WORD_AT(hit+0x28u)=VEC;WORD_AT(hit+0x30u)=VEC+0x40u;FLOAT_AT(hit+0x20u)=70.0f;
        FLOAT_AT(CHAIR+0x1CCu)=60.0f;nstop=0;train_death=0;
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BB63Au,hit,0,0,1);
        CHECK(train_death==0 && FLOAT_AT(CHAIR+0x1000u+0x1CCu)==-10.0f && FLOAT_AT(CHAIR+0x1CCu)==60.0f);
        CHECK(nstop==3u && stop_args[0]==0x00576100u && WORD_AT(0x0078F04Cu)==7u);
        WORD_AT(hit)=CHAIR;                                               /* chair 0 is not active: ignored */
        CALL(pc_mapmaker_khallos_damage,fake_damage,0x000BB63Au,hit,0,0,1);
        CHECK(FLOAT_AT(CHAIR+0x1CCu)==60.0f && nstop==3u);
    }
    /* the seated Khallos runs with the globals of the chair he rides */
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(1),0,0,1);
    CHECK(rider_saw==CHAIR && rider_live==40.0f && CW(0,0x0078F060u)==0x5EA7u && WORD_AT(0x0078F060u)!=0x5EA7u);
    CHECK(WORD_AT(0x0078F000u)==0x00123450u);
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(10),0,0,1);   /* not a rider */
    CHECK(rider_saw==0x00123450u && khl_lv.cur==-1);
    WORD_AT(0x0078F060u)=0;
    CALL(pc_mapmaker_khallos_rider_pose,fake_rider,0x000A5D5Eu,ENT(1),0,0,1);   /* its pose callback too */
    CHECK(rider_saw==CHAIR && rider_live==40.0f && WORD_AT(0x0078F060u)==0 && WORD_AT(0x0078F000u)==0x00123450u);
    CALL(pc_mapmaker_khallos_rider_pose,fake_rider,0x000A5D5Eu,ENT(10),0,0,1);
    CHECK(rider_saw==0x00123450u && khl_lv.cur==-1);
    /* emitter owners: each chair's emitters point at its own guest slot, the stock slot only inside its code */
    CHECK(khl.slots && WORD_AT(JETE)==khl.slots && WORD_AT(khl.slots)==JETE);
    CHECK(WORD_AT(JETE+0x100u)==khl.slots+8u && WORD_AT(khl.slots+8u)==JETE+0x100u && WORD_AT(0x00576100u)==khl.slots+12u);
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(seen_jet==JETE+0x100u && seen_owner==0x0078F054u && WORD_AT(JETE+0x100u)==khl.slots+8u);
    WORD_AT(0x0078F054u)=0x0BADu;                                        /* someone else's outside value */
    WORD_AT(WORD_AT(JETE+0x100u))=0;WORD_AT(JETE+0x100u)=0;              /* the engine frees chair 1's jet meanwhile */
    CHECK(WORD_AT(0x0078F054u)==0x0BADu && WORD_AT(khl.slots)==JETE);    /* nothing else touched */
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(seen_jet==0 && WORD_AT(0x0078F054u)==0x0BADu);                 /* chair 1 sees it gone, not a dangling pointer */
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR,0,0,1);
    WORD_AT(0x0078F054u)=0;
    WORD_AT(0x00577800u)=0x00123000u;foreign_jet=0x00577800u;           /* owned by 0x00123000 */
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    foreign_jet=0;
    CHECK(WORD_AT(0x00577800u)==0x00123000u && WORD_AT(khl.slots+8u)==0x00577800u);   /* kept, its owner untouched */
    WORD_AT(khl.slots+8u)=0;
    /* chair 1 dies and flies off: only chair 1 is removed */
    n=ndestroy;
    for(k=0;k<6u && !khl_ch[1].removed;k++) CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(khl_ch[1].removed && !khl_ch[0].removed && ndestroy==n+1u && destroy_arg==CHAIR+0x1000u && !WORD_AT(khl.slots+8u));   /* its jet was freed before */
    CHECK(!CW(1,0x0078F000u) && CW(0,0x0078F000u)==CHAIR && khl_ch[0].rider==A(1));
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,0);   /* removed: skipped */
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR,0,0,1);        /* chair 0 runs on */
    /* character creation and AI start of the placeholder are logged */
    n=khl_lv.creates;
    orig_eax=0x00577700u;
    CALL(pc_mapmaker_khallos_create,fake,0x0015DFDAu,0x33u,0,5u,1);
    CALL(pc_mapmaker_khallos_create,fake,0x0015DFDAu,0x02u,0,6u,1);
    CALL(pc_mapmaker_khallos_create,fake,0x0015DFDAu,0x40u,0,7u,1);     /* someone else */
    CHECK(khl_lv.creates==n+2u);
    orig_eax=0;
    CALL(pc_mapmaker_khallos_create,fake,0x0015DFDAu,0x33u,0,5u,1);     /* a failed one too */
    CHECK(khl_lv.creates==n+3u);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_khallos_create,fake,0x0015DFDAu,0x33u,0,5u,1);
    CHECK(khl_lv.creates==n+3u);
    play_scene();
    n=khl_lv.seen;
    actor(11,0x33,0.0f,0.0f,0.0f,0.0f);WORD_AT(A(11))=0;                 /* no entity: logged, stays a bot */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(11),0,0,1);
    CHECK(khl_lv.seen==n+1u && khl_lv.n==2u);

    /* ---- the placeholder's pool entry reused before the chair's first frame ---------- */
    level(1,__LINE__);
    WORD_AT(A(0)+0xBF0u)=REC(5);
    n=nremove;
    UPDATE(1);
    CHECK(nremove==n && !khl_ch[0].place);
    UPDATE(1);
    CHECK(nremove==n);

    /* ---- the train (scene 11) and a new level --------------------------------------- */
    level(1,__LINE__);
    WORD_AT(0x0079094Cu)=11u;
    n=nremove;
    UPDATE(1);
    CHECK(nremove==n && seen_line==-535.0f);
    play_scene();
    container(0x11,neither,2,NULL,0);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);           /* next level: no chair */
    CHECK(!khl_lv.n && khl.slots && !WORD_AT(khl.slots) && !WORD_AT(khl.slots+8u) && !WORD_AT(khl.slots+12u));
    UPDATE(1);
    CHECK(nremove==n && seen_line==-535.0f);

    /* ---- the placeholder made as Khallos rides his own chair (.27: "not actually visible") ---- */
    req_reads=1;
    bos_bar_proxy=0;
    n=nrequest;
    own_level(__LINE__);
    CHECK(bos_bar_proxy==khl_bar_entity && anim_mounted);
    CHECK(nrequest==n+2u && requested==0x582u);                          /* the chair, then his model (no +8) */
    CHECK(made_model==0x582u && made_lod==0xFFFFFFFFu);                  /* made with Khallos' models ... */
    CHECK(WORD_AT(CHARS+0x33u*0x30u+4u)==0x619u && WORD_AT(CHARS+0x33u*0x30u+8u)==0x61Au);   /* ... for that call only */
    CHECK(khl_lv.n==1u && khl_ch[0].own && khl_ch[0].rider==A(0) && khl_ch[0].rider_ent==ENT(0) && khl_ch[0].rider_rec==REC(0));
    CHECK(WORD_AT(A(0)+0x10u)==0 && WORD_AT(A(0)+0x14u)==8u && WORD_AT(A(0)+0x9D0u)==0);
    actor(1,0x02,41.0f,2.0f,31.0f,0.0f);                                 /* a Khallos next to it: a bot */
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(khl_ch[0].rider==A(0) && WORD_AT(A(1)+0x14u)==0x11000000u);
    CHECK(khl_bar_entity(ENT(0))==CHAIR && !khl_bar_entity(ENT(1)) && !khl_bar_entity(CHAIR));   /* his bar shows the chair */
    khl_ch[0].removed=1;CHECK(!khl_bar_entity(ENT(0)));khl_ch[0].removed=0;
    constants();
    players(100.0f,100.0f,1.0f,100.0f,100.0f,1.0f);                       /* far */
    n=nremove;
    UPDATE(1);UPDATE(1);
    CHECK(nremove==n);                                                   /* he stays */
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(0),0,0,1);
    CHECK(rider_saw==CHAIR);
    /* hits on him go to the chair while it fights; his health is put back */
    FLOAT_AT(ENT(0)+0x1D8u)=20.0f;FLOAT_AT(ENT(0)+0x1CCu)=15.0f;FLOAT_AT(CHAIR+0x1CCu)=60.0f;
    UPDATE(1);                                                           /* waiting: nothing taken */
    CHECK(FLOAT_AT(ENT(0)+0x1CCu)==20.0f && FLOAT_AT(CHAIR+0x1CCu)==60.0f);
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    UPDATE(1);                                                           /* wakes */
    CHECK(CW(0,0x0078F04Cu)==1u);
    FLOAT_AT(ENT(0)+0x1CCu)=15.0f;
    UPDATE(1);
    CHECK(FLOAT_AT(CHAIR+0x1CCu)==55.0f && FLOAT_AT(ENT(0)+0x1CCu)==20.0f && khl_ch[0].hits==1u);
    UPDATE(1);
    CHECK(FLOAT_AT(CHAIR+0x1CCu)==55.0f);                                /* unhurt: nothing */
    FLOAT_AT(CHAIR+0x1CCu)=3.0f;FLOAT_AT(ENT(0)+0x1CCu)=16.0f;nstop=0;WORD_AT(PLAYER+0xFF4u)=CHAIR;
    UPDATE(1);                                                           /* shot down through him */
    CHECK(FLOAT_AT(CHAIR+0x1CCu)==-1.0f && nstop==3u && WORD_AT(PLAYER+0xFF4u)==0 && FLOAT_AT(ENT(0)+0x1CCu)==20.0f);
    FLOAT_AT(ENT(0)+0x1CCu)=10.0f;
    UPDATE(1);                                                           /* dead: nothing more taken */
    CHECK(FLOAT_AT(CHAIR+0x1CCu)==-1.0f && nstop==3u && FLOAT_AT(ENT(0)+0x1CCu)==20.0f);
    /* killed by one hit: his whole health goes to the chair, he is not revived */
    own_level(__LINE__);
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    UPDATE(1);
    FLOAT_AT(ENT(0)+0x1D8u)=20.0f;FLOAT_AT(ENT(0)+0x1CCu)=-5.0f;FLOAT_AT(CHAIR+0x1CCu)=60.0f;
    UPDATE(1);
    CHECK(FLOAT_AT(CHAIR+0x1CCu)==40.0f && FLOAT_AT(ENT(0)+0x1CCu)==-5.0f);
    /* the stock bar: hidden for a seated chair, the focus put back */
    own_level(__LINE__);
    WORD_AT(PLAYER+0xFF4u)=CHAIR;hud_focus=1u;
    CALL(pc_mapmaker_khallos_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(hud_focus==0 && WORD_AT(PLAYER+0xFF4u)==CHAIR);
    WORD_AT(PLAYER+0xFF4u)=ENT(5);
    CALL(pc_mapmaker_khallos_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(hud_focus==ENT(5));
    WORD_AT(PLAYER+0xFF4u)=CHAIR;
    WORD_AT(A(0)+0xBF0u)=REC(9);                                         /* his entry reused: not seated */
    CALL(pc_mapmaker_khallos_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(hud_focus==CHAIR);
    WORD_AT(A(0)+0xBF0u)=REC(0);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_khallos_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(hud_focus==CHAIR);
    play_scene();
    /* not made as Khallos: no lv11.pak, his model unread, no chair possible, a fifth chair, another id, opted out */
    own_level(__LINE__);
    anim_mounted=0;made_model=0;
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    anim_mounted=1;req_state[4]=0;
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    req_state[4]=2;WORD_AT(CHARS+2u*0x30u+8u)=0x583u;                    /* a second model, unread */
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    WORD_AT(CHARS+2u*0x30u+8u)=0xFFFFFFFFu;
    pak_mounted=0;
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    pak_mounted=1;
    khl_lv.n=KHL_MAX;
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    khl_lv.n=1u;
    WORD_AT(CHARS+0x34u*0x30u+4u)=0x620u;
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x34u,0,2u,1);
    CHECK(made_model==0x620u && made_33==0x619u);                        /* another id: nothing swapped */
    set_env("TSFP_MAPMAKER_KHALLOS","0");
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x619u);
    set_env("TSFP_MAPMAKER_KHALLOS",NULL);
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x582u);                                           /* all there again */
    /* his second model is requested and swapped too */
    WORD_AT(CHARS+2u*0x30u+8u)=0x583u;HALF_AT(0x008CA000u+0x583u*2u)=5;
    n=nrequest;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==n+3u && requested==0x583u);
    CALL(pc_mapmaker_khallos_create,fake_create,0x0015DFDAu,0x33u,0,2u,1);
    CHECK(made_model==0x582u && made_lod==0x583u && WORD_AT(CHARS+0x33u*0x30u+8u)==0x61Au);
    WORD_AT(CHARS+2u*0x30u+8u)=0xFFFFFFFFu;
    /* made as Khallos but lv11.pak gone by its AI start: no rider, removed as before */
    actor(2,0x33,-20.0f,2.0f,-20.0f,0.0f);HALF_AT(ENT(2)+0x94u)=0x582u;
    anim_mounted=0;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(khl_lv.n==2u && !khl_ch[1].own && !khl_ch[1].rider && WORD_AT(A(2)+0x14u)==0x11000000u);
    anim_mounted=1;
    /* a soldier-model placeholder: no own rider, removed on its first frame */
    play_scene();
    container(0x11,chair_only,2,NULL,0);
    resources(1,0x001F4E00u);pak_mounted=0;anim_mounted=0;
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_khallos_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    actor(0,0x33,40.0f,2.0f,30.0f,1.5f);HALF_AT(ENT(0)+0x94u)=0x619u;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(khl_lv.n==1u && !khl_ch[0].own && !khl_ch[0].rider);
    players(100.0f,100.0f,1.0f,100.0f,100.0f,1.0f);
    n=nremove;
    UPDATE(1);
    CHECK(nremove==n+1u);
    /* Khallos in the set: the preload asks for his models, not this area */
    play_scene();
    container(0x11,with_both,3,NULL,0);
    resources(1,0x001F4E00u);pak_mounted=1;
    n=nrequest;
    CALL(pc_mapmaker_khallos_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==n+1u && requested==0x376u);

    /* ---- .28: the chair turns to the player anywhere, the rider sits on his seat, its sounds load ---- */
    own_level(__LINE__);
    CHECK(khl_lv.sounds==1);
    constants();
    FLOAT_AT(ENT(0)+0x1D8u)=20.0f;FLOAT_AT(ENT(0)+0x1CCu)=20.0f;FLOAT_AT(CHAIR+0x1CCu)=60.0f;
    pose(CHAIR,0.5f,1.0f,-0.25f,135.0f,4.0f,-6.0f);                     /* tilted */
    FLOAT_AT(ENT(0)+0x64u)=40.0f;FLOAT_AT(ENT(0)+0x68u)=2.0f;FLOAT_AT(ENT(0)+0x6Cu)=30.0f;   /* left where he stood */
    players(100.0f,100.0f,1.0f,100.0f,100.0f,1.0f);
    UPDATE(1);                                                           /* waiting: the handler seats him */
    CHECK(seen_span==30.0f && FLOAT_AT(ENT(0)+0x64u)==40.0f && !khl_ch[0].status);
    players(45.0f,33.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    world_eax=0xFFFFFFFFu;WORD_AT(CHAIR+0x22Cu)=0xFFFFFFFFu;            /* sounds not loaded: they fail */
    UPDATE(1);                                                           /* wakes and flies */
    CHECK(seen_span==360.0f && FLOAT_AT(0x005000B8u)==30.0f);            /* no yaw clamp inside, put back after */
    CHECK(NEAR(FLOAT_AT(ENT(0)+0x64u),CF(0,LIVE)-0.6f) && NEAR(FLOAT_AT(ENT(0)+0x68u),CF(0,LIVE+4u)-1.2f) &&
          NEAR(FLOAT_AT(ENT(0)+0x6Cu),CF(0,LIVE+8u)+0.3f));               /* live position + seat x up row */
    CHECK(FLOAT_AT(ENT(0)+0x70u)==135.0f && FLOAT_AT(ENT(0)+0xC4u)==135.0f && FLOAT_AT(ENT(0)+0xC0u)==4.0f && FLOAT_AT(ENT(0)+0xC8u)==-6.0f);
    CHECK(NEAR(khl_ch[0].off[0],40.0f-(CF(0,LIVE)-0.6f)) && NEAR(khl_ch[0].off[2],30.0f-(CF(0,LIVE+8u)+0.3f)));
    CHECK(khl_ch[0].status==1u && khl_ch[0].frames==1u);
    CHECK(khl_ch[0].shots==1u && khl_ch[0].shot_fail==1u && khl_ch[0].engine==1u && khl_ch[0].engine_off==1u);
    world_eax=5u;WORD_AT(CHAIR+0x22Cu)=3u;
    UPDATE(1);
    CHECK(khl_ch[0].shots==2u && khl_ch[0].shot_fail==1u && khl_ch[0].engine==2u && khl_ch[0].engine_off==1u);
    for(k=0;k<KHL_STATUS_EVERY;k++) UPDATE(1);
    CHECK(khl_ch[0].status==2u);                                         /* a line every 300 frames */
    memcpy((void *)((uintptr_t)0x0049E4BCu+g_xbox_mem_offset),"\xFF\xFF\xFF\x7F",4);   /* the seat unreadable (NaN): the stock -1.2 */
    FLOAT_AT(ENT(0)+0x68u)=50.0f;
    UPDATE(1);
    CHECK(NEAR(FLOAT_AT(ENT(0)+0x68u),CF(0,LIVE+4u)-1.2f));
    FLOAT_AT(0x0049E4BCu)=-2.0f;
    UPDATE(1);
    CHECK(NEAR(FLOAT_AT(ENT(0)+0x68u),CF(0,LIVE+4u)-2.0f));             /* the game's own value */
    FLOAT_AT(0x0049E4BCu)=-1.2f;
    /* a class or a state the handler cannot sit in is put back */
    WORD_AT(A(0)+0x10u)=1u;WORD_AT(A(0)+0x14u)=0x11000000u;WORD_AT(A(0)+0x9D0u)=0xFFFFFFFFu;
    UPDATE(1);
    CHECK(!WORD_AT(A(0)+0x10u) && WORD_AT(A(0)+0x14u)==8u && WORD_AT(A(0)+0x9D0u)==0);
    n=khl_ch[0].calls;
    WORD_AT(A(0)+0x9D0u)=5u;
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(0),0,0,1);
    CHECK(WORD_AT(A(0)+0x9D0u)==0 && khl_ch[0].calls==n+1u);
    WORD_AT(A(0)+0x9D0u)=1u;
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(0),0,0,1);
    CHECK(WORD_AT(A(0)+0x9D0u)==1u);                                     /* sitting down: the handler's own */
    WORD_AT(A(0)+0x9D0u)=0xFFFFFFFFu;WORD_AT(A(0)+0x20u)=3u;
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(0),0,0,1);
    CHECK(WORD_AT(A(0)+0x9D0u)==0);
    WORD_AT(A(0)+0x9D0u)=0xFFFFFFFFu;WORD_AT(A(0)+0x20u)=1u;
    CALL(pc_mapmaker_khallos_rider,fake_rider,0x0027DAA0u,ENT(0),0,0,1);
    CHECK(WORD_AT(A(0)+0x9D0u)==0xFFFFFFFFu && khl_ch[0].calls==n+4u);  /* seated: untouched */
    /* a second chair: its rider sits on his own chair, chair 0's stays */
    actor(2,0x33,-20.0f,2.0f,-20.0f,0.0f);HALF_AT(ENT(2)+0x94u)=0x582u;
    CALL(pc_mapmaker_khallos_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(khl_lv.n==2u && khl_ch[1].own && khl_ch[1].rider_ent==ENT(2));
    FLOAT_AT(ENT(2)+0x1D8u)=20.0f;FLOAT_AT(ENT(2)+0x1CCu)=20.0f;FLOAT_AT(CHAIR+0x1000u+0x1CCu)=60.0f;
    pose(CHAIR+0x1000u,0.0f,1.0f,0.0f,-40.0f,1.0f,2.0f);
    dy=FLOAT_AT(ENT(0)+0x64u);dz=FLOAT_AT(ENT(0)+0x6Cu);
    players(-15.0f,-22.0f,1.0f,100.0f,100.0f,1.0f);
    FLOAT_AT(PL(1)+0xA8u)=1000.0f;
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(CW(1,0x0078F04Cu)==1u && seen_span==360.0f);
    CHECK(NEAR(FLOAT_AT(ENT(2)+0x64u),CF(1,LIVE)) && NEAR(FLOAT_AT(ENT(2)+0x68u),CF(1,LIVE+4u)-1.2f) && NEAR(FLOAT_AT(ENT(2)+0x6Cu),CF(1,LIVE+8u)));
    CHECK(FLOAT_AT(ENT(2)+0x70u)==-40.0f && FLOAT_AT(ENT(2)+0xC8u)==2.0f && khl_ch[1].status==1u && khl_ch[1].shots==1u);
    CHECK(FLOAT_AT(ENT(0)+0x64u)==dy && FLOAT_AT(ENT(0)+0x6Cu)==dz && FLOAT_AT(ENT(0)+0x70u)==135.0f);
    /* a dead rider is not moved */
    FLOAT_AT(ENT(2)+0x1CCu)=-1.0f;FLOAT_AT(ENT(2)+0x64u)=7.0f;
    CALL(pc_mapmaker_khallos_update,fake_update,0x00000100u,CHAIR+0x1000u,0,0,1);
    CHECK(FLOAT_AT(ENT(2)+0x64u)==7.0f);
    /* sub_00045670: the chair's two level-11 sounds load in a map holding it, nothing else changes */
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x393u,0,0,1)==4u);
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x394u,0,0,1)==4u);
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x395u,0,0,1)==0);
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x43Cu,0,0,1)==0);
    orig_eax=0x80000004u;
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x393u,0,0,1)==0x80000004u);
    orig_eax=0;
    WORD_AT(0x0079094Cu)=11u;
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x393u,0,0,1)==0);   /* the train: stock */
    play_scene();
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CHECK(CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x393u,0,0,1)==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    container(0x11,neither,2,NULL,0);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(!khl_lv.sounds && CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x393u,0,0,1)==0);
    container(0x11,chair_only,2,NULL,0);
    set_env("TSFP_MAPMAKER_KHALLOS","0");
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(!khl_lv.sounds);
    set_env("TSFP_MAPMAKER_KHALLOS",NULL);
    CALL(pc_mapmaker_khallos_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(khl_lv.sounds && CALL(pc_mapmaker_khallos_sound_load,fake,0x0002787Fu,0x394u,0,0,1)==4u);
    world_eax=0;

    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker khallos ok\n");
    return 0;
}
