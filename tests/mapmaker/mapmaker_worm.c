/* Asset-free contract for src/interface/pc_mapmaker_worm.inc: the mansion
 * worm of story mission 5 as a MapMaker Story AI. The lv1_2.pak mount, the
 * worm block and the stock init with its stand-ins at level init, the noted
 * spawn record, the first tick (spawn, garden offsets, route pads, enable),
 * the shifted constants and swapped route around the stock update, the head
 * lookup, the hit callback's stand-ins and the draw. Fake guest memory; fake
 * originals pop their return address like the lifted callees, and every hook
 * call checks esp. */
#include <math.h>
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
static float F(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void SETF(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int near_eq(float a,float b) { return fabsf(a-b)<0.01f; }

#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define ESP0 0x00600000u
#define ESP1 0x00610000u
#define ESP2 0x00620000u
#define WST 0x00700000u                 /* fake worm block (0x1590) */
#define SCR 0x00710000u                 /* fake scratch (0x300) */
#define OBJS 0x00720000u                /* the map's object table */
#define PADS 0x00730000u                /* the map's regular pads */
#define R 10u
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define W(i) (WST+(uint32_t)(i)*0x450u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_0005B980(void) { g_ecx=11; }
void sub_0005A130(void) { g_ecx=2; }
void sub_00062260(void) { g_ecx=3; }
void sub_001D1E80(void) { g_ecx=4; }
void sub_001D2350(void) { g_ecx=5; }
void sub_001DAAB0(void) { g_ecx=6; }
void sub_001DA990(void) { g_ecx=7; }
void sub_00378400(void) { g_ecx=8; }
void sub_0010E2A0(void) { g_ecx=9; }
void sub_0009A420(void) { g_ecx=10; }
static int register_on=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)area;return strcmp(hook,"pc_mapmaker_worm_register") || register_on; }

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static int pak_mounted, trophy_mounted, skeleton_mounted, mount_ok=1, alloc_ok=1, scratch_ok=1, bind=1, die;
static unsigned nmount, nalloc, ninit, nupdate, ndraw, nrelease, neffect, npadcalls;
static uint32_t mount_arg, scratch_size, effect_args[2], init_objects, init_route_done, refuse=0xFFFFFFFFu;
static uint32_t seen_route[6], seen_enable, seen_wall, tag_ret[2], seen_props, seen_extra, seen_script;
static float seen_const[11], seen_draw[2];
static const uint32_t cad[11]={0x0047D78Cu,0x0049DF08u,0x0049DF10u,0x0049DE40u,0x0049DF0Cu,0x0047D69Cu,
                               0x0049DE3Cu,0x0049DE34u,0x0049DE38u,0x0049DDD8u,0x0049DDD4u};
static const uint32_t cbits[11]={0x41D80000u,0xC0198937u,0xC29A6666u,0xC25ECCCDu,0x421E0000u,0xC0C00000u,
                                 0xC29E0000u,0xC0FCCCCDu,0x42213333u,0xC2794189u,0x41873333u};
static void stock_consts(void) { unsigned k;for(k=0;k<11u;k++) WORD_AT(cad[k])=cbits[k];WORD_AT(0x0049DE08u)=0xC257999Au; }
static int consts_stock(void)
{
    unsigned k;
    for(k=0;k<11u;k++) if(WORD_AT(cad[k])!=cbits[k]) return 0;
    return WORD_AT(0x0049DE08u)==0xC257999Au;
}
static const uint32_t route_ids[6]={0x4FF,0x514,0x515,0x51D,0x516,0x500};
static void stock_route(void) { unsigned k;for(k=0;k<6u;k++) WORD_AT(0x004FDC24u+k*4u)=route_ids[k];WORD_AT(0x004FDC58u)=1; }
static int route_stock(void) { unsigned k;for(k=0;k<6u;k++) if(WORD_AT(0x004FDC24u+k*4u)!=route_ids[k]) return 0;return 1; }

/* nested calls (the update's head lookup, the draw's callees) on their own stack */
static void fake(void) { originals++;g_esp+=4u; }
static int tag_originals;
static void fake_tag(void) { tag_originals++;g_eax=0;g_esp+=4u; }
static uint32_t inner(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t sp0,uint32_t ret,uint32_t a1)
{
    uint32_t sp=g_esp,eax=g_eax;
    g_esp=sp0;WORD_AT(sp0)=ret;WORD_AT(sp0+4u)=a1;
    fn(orig);
    if(g_esp!=sp0+4u) { fprintf(stderr,"inner esp %+d\n",(int)(g_esp-sp0));failures++; }
    eax=g_eax;g_esp=sp;
    return eax;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_worm.inc"

/* the stock init sub_001D2350 (the parts the area relies on) */
static void fake_init(void)
{
    static const float start[5][3]={{-71.67f,-2.4f,12.97f},{-71.44f,-2.4f,38.99f},{-58.68f,-2.4f,28.4f},
                                    {-71.0f,-2.4f,3.3f},{-72.44f,-2.4f,33.48f}};
    uint32_t k,j,obj;
    ninit++;init_objects=WORD_AT(0x007B9784u);init_route_done=WORD_AT(0x004FDC58u);
    WORD_AT(0x0075B4B8u)=1;
    for(k=0;k<5u;k++) {
        uint32_t w=W(k);
        memset((void *)((uintptr_t)w+g_xbox_mem_offset),0,0x450u);
        SETF(w,start[k][0]);SETF(w+4u,start[k][1]);SETF(w+8u,start[k][2]);
        SETF(w+0xCu,start[k][0]);SETF(w+0x10u,start[k][1]);SETF(w+0x14u,start[k][2]);
        for(j=0;j<64u;j++) { SETF(w+0x3Cu+j*12u,start[k][0]);SETF(w+0x40u+j*12u,start[k][1]);SETF(w+0x44u+j*12u,start[k][2]); }
        WORD_AT(w+0x24u)=k==2u?8u:1u;
        WORD_AT(w+0x43Cu)=0x00740000u+k;WORD_AT(w+0x440u)=0xFFFFFFFFu;WORD_AT(w+0x444u)=0xFFFFFFFFu;
    }
    for(k=0;k<5u;k++) WORD_AT(0x0078FAE0u+k*4u)=0;
    WORD_AT(0x0075B4BCu)=0;WORD_AT(0x004FDC20u)=1;
    if(WORD_AT(0x004FDC58u)) { for(k=0;k<6u;k++) WORD_AT(0x004FDC24u+k*4u)=0xFFFFFFFFu;WORD_AT(0x004FDC58u)=0; }
    obj=WORD_AT(WORD_AT(0x007B9784u)+0x18u*4u);
    WORD_AT(obj+0x28u)|=0x10u;WORD_AT(obj+0x19Cu)=0x001D1EA0u;
}
/* the stock update sub_001DAAB0: constants and route seen, worm 0's head
 * bound through the lookup (sub_001D9AF0 clears slots 3 and 4) */
static void fake_update(void)
{
    unsigned k;
    uint32_t ent;
    nupdate++;
    for(k=0;k<11u;k++) seen_const[k]=F(cad[k]);
    for(k=0;k<6u;k++) seen_route[k]=WORD_AT(0x004FDC24u+k*4u);
    seen_enable=WORD_AT(0x0075B4BCu);
    if(!seen_enable) return;
    {   /* the 'SFCC' script trigger at 0x001DC45A */
        int before=originals;
        inner(pc_mapmaker_worm_script,fake,ESP1,0x001DC45Fu,0x53464343u);
        seen_script=(uint32_t)(originals-before);
    }
    if(!WORD_AT(0x0078FAE0u) && bind && WORD_AT(W(0)+0x24u)!=4u) {   /* stock: a gone worm (state 4) is not bound again */
        tag_ret[1]=inner(pc_mapmaker_worm_tag,fake_tag,ESP1,0x001D9AFDu,0x57524D32u);   /* 'WRM2' */
        ent=tag_ret[0]=inner(pc_mapmaker_worm_tag,fake_tag,ESP1,0x001D9AFDu,0x57524D31u);
        if(ent) {
            WORD_AT(ent+0x160u)=0;WORD_AT(ent+0x164u)=0;WORD_AT(ent+0x19Cu)=0x001D1F30u;
            SETF(ent+0x68u,F(ent+0x68u)-5.0f);
            WORD_AT(0x0078FAE0u)=ent;
        }
    }
    if(die && WORD_AT(0x0078FAE0u)) { WORD_AT(W(0)+0x24u)=4u;WORD_AT(0x0078FAE0u)=0; }   /* state 4: head removed */
}
/* the stock registration sub_0005B980: the table and count it is given */
static uint32_t reg_seen[2], reg_nested[3];
static unsigned nregister;
static void fake_reg(void) { originals++;reg_seen[0]=WORD_AT(g_esp+8u);reg_seen[1]=WORD_AT(g_esp+12u);g_esp+=4u; }
/* the stock hit callback sub_001D1F30: what it sees; a hit while surfaced */
static uint32_t hit_seen[5];
static void fake_hit(void)
{
    uint32_t info=WORD_AT(g_esp+4u),ent=WORD_AT(info),script=WORD_AT(0x0075B0C4u);
    originals++;
    hit_seen[0]=script;hit_seen[1]=WORD_AT(0x007B9784u);hit_seen[2]=WORD_AT(0x0078F590u);hit_seen[3]=WORD_AT(ent+0x60u);
    if(script==SCR) { WORD_AT(SCR+0x3Cu)++;hit_seen[4]=WORD_AT(SCR+0x3Cu);WORD_AT(W(0)+0x24u)=0xDu; }
    g_esp+=4u;
}
/* the stock draw sub_001DA990: its first two callees, the draw constants */
static void fake_draw(void)
{
    int before;
    ndraw++;
    seen_draw[0]=F(0x0049DDD8u);seen_draw[1]=F(0x0049DDD4u);seen_wall=WORD_AT(0x0049DE08u);
    before=originals;inner(pc_mapmaker_worm_props,fake,ESP1,0x001DA998u,0);seen_props=(uint32_t)(originals-before);
    before=originals;inner(pc_mapmaker_worm_extra,fake,ESP1,0x001DA9A9u,0);seen_extra=(uint32_t)(originals-before);
    {   /* sub_001D9B80: a surfaced head at the worm, or at the fixed burst point (y 0) */
        uint32_t head=WORD_AT(0x0078FAE0u),w=WORD_AT(0x0075B4B4u);
        if(head && w==WST && WORD_AT(w+0x24u)>=8u) {
            if(WORD_AT(w+0x44Cu)==2u) { SETF(head+0x64u,F(0x0049DDD8u));SETF(head+0x68u,0.0f);SETF(head+0x6Cu,F(0x0049DDD4u)); }
            else { SETF(head+0x64u,F(w));SETF(head+0x68u,F(w+4u));SETF(head+0x6Cu,F(w+8u)); }
        }
    }
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_0005B980) { nregister++;reg_nested[0]=x[0];reg_nested[1]=x[1];reg_nested[2]=x[2];return 0; }
    if(f==sub_00062260) return ((x[0]==0x0049D7BCu && pak_mounted) || (x[0]==0x0049D77Cu && trophy_mounted) ||
                                (x[0]==0x0049D6FCu && skeleton_mounted))?4u:0xFFFFFFFFu;
    if(f==sub_0005A130) { nmount++;mount_arg=x[0];if(mount_ok) pak_mounted=1;return mount_ok?1u:0u; }
    if(f==sub_001D1E80) { nalloc++;WORD_AT(0x0075B4B4u)=alloc_ok?WST:0u;return 0; }
    if(f==sub_0003DFA0) { scratch_size=x[0];if(scratch_ok) memset((void *)((uintptr_t)SCR+g_xbox_mem_offset),0xEE,x[0]);return scratch_ok?SCR:0u; }
    if(f==sub_00378400) { neffect++;effect_args[0]=x[0];effect_args[1]=x[1];return 0x77u; }
    if(f==sub_001D2350) { fake_init();return 0; }
    if(f==sub_0010E2A0) { nrelease++;return 0; }
    if(f==sub_001DAAB0) { fake_update();return 0; }
    if(f==sub_001DA990) { fake_draw();return 0; }
    if(f==sub_0009A420) {
        npadcalls++;
        if(x[0]==refuse || x[0]>=WORD_AT(0x007B6664u)) return 0;   /* the flag is the area's own test */
        return PADS+x[0]*0x20u;
    }
    return 0;
}

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
    wrm.read=0;
}
/* map container: map-info chunk (custom set) */
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
static void actor(unsigned i,uint32_t id,uint32_t model,float x,float y,float z)
{
    memset((void *)((uintptr_t)A(i)+g_xbox_mem_offset),0,0xEA0u);
    memset((void *)((uintptr_t)ENT(i)+g_xbox_mem_offset),0,0x400u);
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);WORD_AT(A(i)+0x678u)=0xFFFFFFFFu;
    WORD_AT(ENT(i)+0x7Cu)=A(i);HALF_AT(ENT(i)+0x94u)=(uint16_t)model;
    SETF(ENT(i)+0x64u,x);SETF(ENT(i)+0x68u,y);SETF(ENT(i)+0x6Cu,z);
    WORD_AT(ENT(i)+0x160u)=0x000D8E30u;WORD_AT(ENT(i)+0x60u)=0x1234u;
    WORD_AT(REC(i))=0x11000000u;WORD_AT(REC(i)+4u)=0;WORD_AT(REC(i)+8u)=R+i+1u;
}
/* pads relative to the spawn (10, 4, -20): flags bit 0 = valid */
static const float pad_at[12][3]={{12,4,-20},{12.5f,4,-20},{10,4,-15},{5,4,-20},{10,4,-26},{16,4,-26},
                                  {25,4,-20},{10,12,-18},{8,4,-22},{3,4,-28},{10,4,-35},{10,4,10}};
static void pads(unsigned n)
{
    unsigned k;
    WORD_AT(0x00715F2Cu)=PADS;WORD_AT(0x007B6664u)=n;
    for(k=0;k<12u;k++) {
        uint32_t p=PADS+k*0x20u;
        WORD_AT(p)=0x3E9u+k;WORD_AT(p+4u)=k==8u?0u:1u;
        SETF(p+0xCu,pad_at[k][0]);SETF(p+0x10u,pad_at[k][1]);SETF(p+0x14u,pad_at[k][2]);
    }
}
static void level(void)
{
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_worm_level,fake,0x0015E81Du,0,0,0,1);
}
static int level_notes(void)
{
    level();
    actor(0,0x2C,0x5EA,10.0f,4.0f,-20.0f);
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(0),0,0,1);
    return wrm_lv.record==REC(0);
}

int main(void)
{
    static const uint16_t with[]={0x0C,0x2C,0xA8}, without[]={0x0C,0x28,0xA8};
    static const uint32_t ring[6]={9,4,5,0,2,3};
    unsigned k;
    int before;
    uint32_t w;
    g_xbox_mem_offset=(uintptr_t)memory;
    stock_consts();stock_route();
    WORD_AT(0x007B9784u)=OBJS;
    pads(12);
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    container(0x11,with,3);
    CHECK(!level_notes() && nmount==0 && nalloc==0 && ninit==0);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    CHECK(nupdate==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- the set decides ---------------------------------------------- */
    container(0x11,without,3);
    CHECK(!level_notes() && nmount==0 && nalloc==0);
    container(0x12,with,3);                            /* a profile set: not the map's members */
    CHECK(!level_notes() && nalloc==0);
    container(0x11,with,3);
    BYTE_AT(0x007DE455u)=3u;                           /* not story */
    CHECK(!level_notes() && nalloc==0);
    BYTE_AT(0x007DE455u)=10u;
    WORD_AT(0x0079094Cu)=1u;                           /* mission 5 itself */
    CHECK(!level_notes() && nalloc==0);
    WORD_AT(0x0079094Cu)=100u;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CHECK(!level_notes() && nalloc==0 && nmount==0);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- level: mount, block, the stock init with stand-ins ---------------- */
    CHECK(level_notes());
    CHECK(nmount==1 && mount_arg==0x0049D7BCu && pak_mounted && nalloc==1 && ninit==1);
    CHECK(scratch_size==0x300u && wrm_lv.scratch==SCR && wrm_lv.state==WST);
    CHECK(neffect==1 && effect_args[0]==0x001D6290u && effect_args[1]==5u && wrm_lv.effect==0x77u);
    CHECK(init_objects==SCR+0x50u && init_route_done==0);     /* fake table, ids not converted */
    CHECK(WORD_AT(0x007B9784u)==OBJS && WORD_AT(0x004FDC58u)==1u && route_stock());
    CHECK((WORD_AT(SCR+0x100u+0x28u)&0x10u) && WORD_AT(SCR+0x100u+0x19Cu)==0x001D1EA0u);   /* the stand-in object */
    CHECK(WORD_AT(OBJS+0x18u*4u)==0);                         /* the map's table untouched */
    CHECK(WORD_AT(SCR)==0 && WORD_AT(SCR+0x3Cu)==0 && WORD_AT(SCR+0x2FCu)==0);              /* scratch zeroed */
    for(k=1;k<5u;k++) CHECK(WORD_AT(W(k)+0x24u)==4u);
    CHECK(WORD_AT(W(0)+0x24u)==1u && nrelease==4 && WORD_AT(0x0075B4BCu)==0);
    /* mount: once; never when already mounted; not into a full table */
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x006D7880u)=60u;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1 && !pak_mounted);
    WORD_AT(0x006D7880u)=10u;
    /* with lv1_3 or lv1_6: mounted (register keeps lv1_2 to the worm's names) */
    pak_mounted=0;trophy_mounted=1;skeleton_mounted=1;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    /* without register: never mounted with either (they share two Jo-Beth names) */
    register_on=0;
    pak_mounted=0;skeleton_mounted=0;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==2 && !pak_mounted);
    trophy_mounted=0;skeleton_mounted=1;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==2 && !pak_mounted);
    skeleton_mounted=0;
    CALL(pc_mapmaker_worm_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==3 && pak_mounted);
    register_on=1;

    /* ---- register: lv1_2's table gives only the worm's names ------------- */
    CALL(pc_mapmaker_worm_register,fake_reg,0x001CAD7Au,0x0049C690u,0x0048BEC8u,0x1Eu,1);
    CHECK(reg_seen[0]==0x0048BEC8u+21u*0x18u && reg_seen[1]==8u);                  /* wrmburst..wrmdeath */
    CHECK(nregister==1 && reg_nested[0]==0x0049C690u && reg_nested[1]==0x0048BEC8u+5u*0x18u && reg_nested[2]==1u);  /* barney_worm */
    CHECK(WORD_AT(ESP0+8u)==0x0048BEC8u && WORD_AT(ESP0+12u)==0x1Eu && WORD_AT(ESP0+4u)==0x0049C690u);
    CALL(pc_mapmaker_worm_register,fake_reg,0x001CAD7Au,0x0049C690u,0x0048C1B0u,0x10u,1);   /* lv1_3: stock */
    CHECK(reg_seen[0]==0x0048C1B0u && reg_seen[1]==0x10u && nregister==1);
    CALL(pc_mapmaker_worm_register,fake_reg,0x001CAD7Au,0x0049C690u,0x0048C738u,0x1Eu,1);   /* another table, same count: stock */
    CHECK(reg_seen[0]==0x0048C738u && reg_seen[1]==0x1Eu && nregister==1);
    CALL(pc_mapmaker_worm_register,fake_reg,0x001C9601u,0x0049C690u,0x0048BEC8u,0x1Du,1);   /* another count: stock */
    CHECK(reg_seen[1]==0x1Du && nregister==1);
    WORD_AT(0x0079094Cu)=1u;                                  /* mission 5: the whole table */
    CALL(pc_mapmaker_worm_register,fake_reg,0x001CAD7Au,0x0049C690u,0x0048BEC8u,0x1Eu,1);
    CHECK(reg_seen[0]==0x0048BEC8u && reg_seen[1]==0x1Eu && nregister==1);
    WORD_AT(0x0079094Cu)=100u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);                     /* off: stock */
    CALL(pc_mapmaker_worm_register,fake_reg,0x001CAD7Au,0x0049C690u,0x0048BEC8u,0x1Eu,1);
    CHECK(reg_seen[1]==0x1Eu && nregister==1);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- AI start: one worm per map ------------------------------------- */
    level();
    actor(1,0x0C,0x200,0,0,0);                               /* another character first: not noted */
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(1),0,0,1);
    CHECK(!wrm_lv.record);
    actor(0,0x2C,0x5EA,10.0f,4.0f,-20.0f);
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(0),0,0,1);
    actor(1,0x0C,0x200,0,0,0);
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(1),0,0,1);
    CHECK(wrm_lv.record==REC(0) && wrm_lv.actor==A(0));
    actor(2,0x2C,0x5EA,0,0,0);
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(2),0,0,1);
    CHECK(wrm_lv.record==REC(0) && wrm_lv.actor==A(0));      /* a second one stays a bot */
    CALL(pc_mapmaker_worm_ai_start,fake,0x0015F981u,A(0),0,0,1);
    CHECK(wrm_lv.record==REC(0) && WORD_AT(REC(0))==0x11000000u);   /* class kept */

    /* ---- tick: not the head, then the head ----------------------------- */
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(1),0,0,1);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(2),0,0,1);
    CHECK(nupdate==0 && !wrm_lv.ready);
    /* the tag lookup before the worm is placed: stock */
    tag_originals=0;
    CHECK(inner(pc_mapmaker_worm_tag,fake_tag,ESP2,0x001D9AFDu,0x57524D31u)==0 && tag_originals==1);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    CHECK(wrm_lv.ready && nupdate==1 && seen_enable==1u);
    /* worm 0 at the spawn, the garden around it */
    CHECK(near_eq(F(W(0)),10.0f) && near_eq(F(W(0)+4u),4.0f) && near_eq(F(W(0)+8u),-20.0f));
    CHECK(near_eq(F(W(0)+0xCu),10.0f) && near_eq(F(W(0)+0x14u),-20.0f));
    CHECK(near_eq(F(W(0)+0x3Cu),10.0f) && near_eq(F(W(0)+0x3Cu+63u*12u+8u),-20.0f) && near_eq(F(W(0)+0x3Cu+63u*12u+4u),4.0f));
    CHECK(!near_eq(F(W(0)+0x3Cu+64u*12u),10.0f));            /* the trail ends at 64 */
    CHECK(near_eq(wrm_lv.dx,76.45f) && near_eq(wrm_lv.dz,-36.75f) && near_eq(wrm_lv.dy,6.4f));
    /* during the update: the constants follow the spawn, the route is this map's */
    CHECK(near_eq(seen_const[1],4.001f) && near_eq(seen_const[2],-0.75f) && near_eq(seen_const[3],20.75f));
    CHECK(near_eq(seen_const[4],2.75f) && near_eq(seen_const[5],-42.75f) && near_eq(seen_const[6],-2.55f));
    CHECK(near_eq(seen_const[7],-44.65f) && near_eq(seen_const[8],3.55f));
    CHECK(near_eq(seen_const[9],14.136f) && near_eq(seen_const[10],-19.85f));
    CHECK(near_eq(seen_const[0],27.0f-36.75f));                 /* the state-2 burst line follows z */
    for(k=0;k<6u;k++) CHECK(seen_route[k]==ring[k]);
    CHECK(consts_stock() && route_stock());                  /* restored after the call */
    CHECK(seen_script==0);                                   /* the mansion script trigger dropped */
    before=originals;inner(pc_mapmaker_worm_script,fake,ESP2,0x001DC45Fu,0x53464343u);   /* outside the update: */
    CHECK(originals-before==0);                              /* still this level's worm site */
    before=originals;inner(pc_mapmaker_worm_script,fake,ESP2,0x0021C060u,0x53464343u);
    CHECK(originals-before==1);                              /* another caller: stock */
    WORD_AT(0x0079094Cu)=1u;
    before=originals;inner(pc_mapmaker_worm_script,fake,ESP2,0x001DC45Fu,0x53464343u);
    CHECK(originals-before==1);                              /* mission 5: stock */
    WORD_AT(0x0079094Cu)=100u;
    /* the head: bound through 'WRM1', 'WRM2' finds nothing, slot 3 kept */
    CHECK(tag_ret[0]==ENT(0) && tag_ret[1]==0 && WORD_AT(0x0078FAE0u)==ENT(0));
    CHECK(WORD_AT(ENT(0)+0x160u)==0x000D8E30u && WORD_AT(ENT(0)+0x164u)==0);
    /* every later tick: the update again, no think, nothing moves */
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    CHECK(nupdate==2 && near_eq(F(W(0)),10.0f) && near_eq(seen_const[1],4.001f) && consts_stock());
    /* a constant that is not stock is left alone */
    SETF(0x0049DF0Cu,1.5f);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    CHECK(near_eq(seen_const[4],1.5f) && near_eq(F(0x0049DF0Cu),1.5f) && near_eq(seen_const[2],-0.75f));
    stock_consts();
    /* a stale worm block (another level's): stock */
    WORD_AT(0x0075B4B4u)=0x00740000u;
    w=nupdate;
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    CHECK(nupdate==w);
    WORD_AT(0x0075B4B4u)=WST;
    /* another scene: stock */
    WORD_AT(0x0079094Cu)=1u;
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    WORD_AT(0x0079094Cu)=100u;
    CHECK(nupdate==3);

    /* ---- the tag lookup: only the worm's own call -------------------------- */
    tag_originals=0;
    CHECK(inner(pc_mapmaker_worm_tag,fake_tag,ESP2,0x001D927Cu,0x57524D31u)==0 && tag_originals==1);   /* 'HTBJ' site */
    CHECK(inner(pc_mapmaker_worm_tag,fake_tag,ESP2,0x001D9AFDu,0x57524D31u)==ENT(0) && tag_originals==1);
    CHECK(inner(pc_mapmaker_worm_tag,fake_tag,ESP2,0x001D9AFDu,0x57524D35u)==0 && tag_originals==1);

    /* ---- hit: the stand-ins only during the callback ------------------------ */
    WORD_AT(0x0075B0C4u)=0x00123450u;WORD_AT(0x0078F590u)=0x5u;
    WORD_AT(SCR+0x3Cu)=4u;                                   /* four kills: the fifth would fire the marker */
    WORD_AT(SCR+0x200u)=ENT(0);
    CALL(pc_mapmaker_worm_hit,fake_hit,0x000BA3E0u,SCR+0x200u,0,0,1);
    CHECK(hit_seen[0]==SCR && hit_seen[1]==SCR+0x50u && hit_seen[2]==0x77u && hit_seen[3]==0x57524D31u);
    CHECK(hit_seen[4]==1u);                                  /* the counter started at 0 */
    CHECK(WORD_AT(0x0075B0C4u)==0x00123450u && WORD_AT(0x007B9784u)==OBJS && WORD_AT(0x0078F590u)==5u);
    CHECK(WORD_AT(ENT(0)+0x60u)==0x1234u && WORD_AT(SCR+0x3Cu)==0 && WORD_AT(W(0)+0x24u)==0xDu);
    WORD_AT(W(0)+0x24u)=1u;
    actor(3,0x0C,0x200,0,0,0);                               /* another entity's hit: stock */
    WORD_AT(SCR+0x200u)=ENT(3);hit_seen[0]=0;
    CALL(pc_mapmaker_worm_hit,fake_hit,0x000BA3E0u,SCR+0x200u,0,0,1);
    CHECK(hit_seen[0]==0x00123450u && hit_seen[3]==0x1234u);

    /* ---- draw ---------------------------------------------------------- */
    CALL(pc_mapmaker_worm_fx,fake,0x001CEB00u,0,0,0,1);       /* another frame call */
    CHECK(ndraw==0);
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1 && near_eq(seen_draw[0],14.136f) && near_eq(seen_draw[1],-19.85f) && seen_wall==0x7F7FFFFFu);
    CHECK(seen_props==0 && seen_extra==0 && consts_stock());
    /* surfaced: the head at the worm (the spawn floor); after a state-2 burst the fixed point's y 0 + dy */
    WORD_AT(W(0)+0x24u)=9u;WORD_AT(W(0)+0x44Cu)=0;
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(near_eq(F(ENT(0)+0x68u),4.0f) && near_eq(F(ENT(0)+0x64u),10.0f) && wrm_lv.surfaced);
    WORD_AT(W(0)+0x24u)=1u;
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(!wrm_lv.surfaced);
    WORD_AT(W(0)+0x24u)=9u;WORD_AT(W(0)+0x44Cu)=2u;
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(near_eq(F(ENT(0)+0x68u),6.4f) && near_eq(F(ENT(0)+0x64u),14.136f) && near_eq(F(ENT(0)+0x6Cu),-19.85f));
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(near_eq(F(ENT(0)+0x68u),6.4f));                    /* set by the draw each frame, offset once */
    WORD_AT(W(0)+0x24u)=1u;WORD_AT(W(0)+0x44Cu)=0;
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    before=originals;
    CHECK(inner(pc_mapmaker_worm_props,fake,ESP2,0x001DA998u,0)==0 || 1);
    CHECK(originals-before==1);                              /* outside this area's draw: stock */
    before=originals;inner(pc_mapmaker_worm_extra,fake,ESP2,0x001DA9A9u,0);CHECK(originals-before==1);
    before=originals;inner(pc_mapmaker_worm_props,fake,ESP2,0x001CEE31u,0);CHECK(originals-before==1);

    /* ---- death: state 4 removes the head; the slot is not restored -------- */
    die=1;
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    die=0;
    CHECK(WORD_AT(0x0078FAE0u)==0 && WORD_AT(W(0)+0x24u)==4u);
    WORD_AT(ENT(0)+0x160u)=0;
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    CHECK(WORD_AT(ENT(0)+0x160u)==0);
    /* a reused pool entry with another record is not the worm */
    WORD_AT(A(0)+0xBF0u)=REC(5);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    WORD_AT(A(0)+0xBF0u)=REC(0);

    /* ---- route: a refused pad, few pads, the head's own pad, none --------- */
    refuse=5;
    CHECK(level_notes());
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    CHECK(wrm_lv.ready);
    {
        static const uint32_t ring2[6]={9,4,10,0,2,3};       /* 5 refused: 10 instead (4 and 10 tie at -pi/2) */
        for(k=0;k<6u;k++) CHECK(seen_route[k]==ring2[k]);
    }
    refuse=0xFFFFFFFFu;
    pads(4);                                                 /* pads 0..3: 0, 2, 3 (1 too close) */
    CHECK(level_notes());
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    {
        static const uint32_t ring3[6]={0,2,3,0,2,3};
        for(k=0;k<6u;k++) CHECK(seen_route[k]==ring3[k]);
    }
    pads(12);
    for(k=0;k<12u;k++) SETF(PADS+k*0x20u+0x10u,40.0f);     /* no pad on this floor */
    CHECK(level_notes());
    WORD_AT(A(0)+0x678u)=7u;                                 /* the head's own pad */
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,0);
    for(k=0;k<6u;k++) CHECK(seen_route[k]==7u);
    CHECK(level_notes());
    WORD_AT(A(0)+0x678u)=0xFFFFFFFFu;
    w=nupdate;
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);   /* none: a bot */
    CHECK(wrm_lv.failed && !wrm_lv.ready && nupdate==w && WORD_AT(0x0075B4BCu)==0);
    w=ndraw;
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==w);
    w=nupdate;
    pads(12);
    /* a spawn that is not a point */
    CHECK(level_notes());
    SETF(ENT(0)+0x64u,NAN);WORD_AT(A(0)+0x678u)=7u;           /* a pad would be there */
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    CHECK(wrm_lv.failed && nupdate==w);

    /* ---- allocation failures ------------------------------------------- */
    alloc_ok=0;w=ninit;
    CHECK(!level_notes() && !wrm_lv.state && WORD_AT(0x0075B4B4u)==0 && ninit==w);
    alloc_ok=1;scratch_ok=0;
    CHECK(!level_notes() && !wrm_lv.state && WORD_AT(0x0075B4B4u)==0 && ninit==w);
    scratch_ok=1;
    /* the next level without the worm forgets this one */
    CHECK(level_notes());
    container(0x11,without,3);
    level();
    CHECK(!wrm_lv.record && !wrm_lv.state);
    CALL(pc_mapmaker_worm_tick,fake,0x000A77E0u,ENT(0),0,0,1);
    CALL(pc_mapmaker_worm_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(consts_stock() && route_stock() && WORD_AT(0x007B9784u)==OBJS);

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker worm ok");
    return 0;
}
