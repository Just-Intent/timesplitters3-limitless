/* Asset-free contract for src/interface/pc_mapmaker_crowe.inc: the split
 * Crowe of story mission 10 as a MapMaker Story AI. The lv14.pak mount, the
 * scene 14 state at level init, the class 0x40000 at AI start, the arena
 * relocation and the shifted floor constants during the fight call, the
 * skipped intro, the turret prop, the dropped level marker, the removal after
 * the death, the effect draw, the bar and the meter. Fake guest memory; fake
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
#define PLAYER 0x00500000u
#define ESP0 0x00600000u
#define ESP1 0x00610000u
#define CST 0x00700000u                 /* fake Crowe state (0x2E04) */
#define PRT 0x00710000u                 /* fake particle block */
#define OLDPRT 0x00720000u
#define R 10u
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_0005A130(void) { g_ecx=2; }
void sub_00062260(void) { g_ecx=3; }
void sub_0028D6D0(void) { g_ecx=4; }
void sub_002A62C0(void) { g_ecx=5; }
void sub_0028D710(void) { g_ecx=6; }
void sub_000EFBD0(void) { g_ecx=7; }
void sub_0015DDA0(void) { g_ecx=8; }
static int bosses_area=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)hook;return strcmp(area,"bosses") || bosses_area; }

static int pak_mounted, mount_ok=1, alloc_ok=1, parts_ok=1;
static unsigned nmount, nalloc, nparts, ndraw, nexplode, nremove;
static uint32_t mount_arg, draw_scene, explode_args[8], remove_arg;
/* the stock grid: 5 x 7 cells (type 0 on even row and column), outer, 14 edges (10 used) */
static void build_grid(uint32_t st)
{
    unsigned k;
    for(k=0;k<35u;k++) {
        uint32_t c=st+0x970u+k*0x28u;
        float x0=283.45f+(float)(k%7u)*6.0f,z0=61.2f+(float)(k/7u)*8.0f;
        SETF(c,x0);SETF(c+4u,z0);SETF(c+8u,x0+3.22f);SETF(c+0xCu,z0+3.2f);
        SETF(c+0x10u,x0+1.61f);SETF(c+0x14u,z0+1.6f);SETF(c+0x18u,1.61f);SETF(c+0x1Cu,1.6f);
        WORD_AT(c+0x20u)=((k%7u)%2u || (k/7u)%2u)?1u:0u;WORD_AT(c+0x24u)=0;
    }
    SETF(st+0xEE8u,283.45f);SETF(st+0xEECu,61.2f);SETF(st+0xEF0u,326.71f);SETF(st+0xEF4u,100.7f);
    SETF(st+0xEF8u,305.08f);SETF(st+0xEFCu,80.95f);SETF(st+0xF00u,21.63f);SETF(st+0xF04u,19.75f);
    WORD_AT(st+0xF08u)=4u;WORD_AT(st+0xF0Cu)=2u;
    for(k=0;k<14u;k++) {
        uint32_t c=st+0xF60u+k*0x28u;
        memset((void *)((uintptr_t)c+g_xbox_mem_offset),0,0x28u);
        if(k<10u) { SETF(c,290.0f+(float)k);SETF(c+4u,62.0f);SETF(c+8u,291.0f+(float)k);SETF(c+0xCu,63.0f);
                    SETF(c+0x10u,290.5f+(float)k);SETF(c+0x14u,62.5f);SETF(c+0x18u,0.5f);SETF(c+0x1Cu,0.5f); }
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
    if(f==sub_00062260) return (x[0]==0x0049DC78u && pak_mounted)?4u:0xFFFFFFFFu;
    if(f==sub_0005A130) { nmount++;mount_arg=x[0];if(mount_ok) pak_mounted=1;return mount_ok?1u:0u; }
    if(f==sub_0028D6D0) {   /* sub_002A8300, [0x0075F5E8] = alloc, sub_0028D0C0 */
        nalloc++;
        if(alloc_ok) { WORD_AT(0x0078B090u)=0x00730000u;WORD_AT(0x0078B098u)=0x00731000u;WORD_AT(0x0075F5E8u)=CST;build_grid(CST); }
        return 0;
    }
    if(f==sub_002A62C0) { nparts++;WORD_AT(0x0078B0D0u)=parts_ok?PRT:0u;return 0; }
    if(f==sub_0028D710) { ndraw++;draw_scene=WORD_AT(0x0079094Cu);return 0; }
    if(f==sub_000EFBD0) { nexplode++;memcpy(explode_args,x,sizeof explode_args);return 0; }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_crowe.inc"

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t seen_class[2], seen_focus, seen_state, seen_next;
static float seen_const[7], seen_near, seen_after_near, seen_spark;
static void fake(void) { originals++;g_esp+=4u; }
static void fake_ai(void)
{
    uint32_t rec=WORD_AT(WORD_AT(g_esp+4u)+0xBF0u);
    originals++;seen_class[0]=WORD_AT(rec);seen_class[1]=WORD_AT(rec+4u);g_esp+=4u;
}
static void fake_hud(void) { originals++;seen_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }
/* nested calls (the fight's callees) on their own stack */
static void inner(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1)
{
    uint32_t sp=g_esp;
    g_esp=ESP1;WORD_AT(ESP1)=ret;WORD_AT(ESP1+4u)=a1;
    fn(orig);
    if(g_esp!=ESP1+4u) { fprintf(stderr,"inner esp %+d\n",(int)(g_esp-ESP1));failures++; }
    if(WORD_AT(ESP1+4u)!=a1) { fprintf(stderr,"inner arg slot changed\n");failures++; }
    g_esp=sp;
}
static void fake_near(void) { seen_near=F(0x0049F3C4u);g_esp+=4u; }
static void fake_spark(void) { seen_spark=F(g_esp+4u);g_esp+=4u; }
static const uint32_t cad[7]={0x0049F3C4u,0x0049F4CCu,0x0049F4D0u,0x0049F4D4u,0x0049F3CCu,0x0049F3C8u,0x0049F3D8u};
static void fake_fight(void)
{
    unsigned k;
    uint32_t a=WORD_AT(WORD_AT(g_esp+4u)+0x7Cu);
    originals++;
    for(k=0;k<7u;k++) seen_const[k]=F(cad[k]);
    seen_state=WORD_AT(a+0x20u);seen_next=WORD_AT(a+0x9D0u);
    inner(pc_mapmaker_crowe_near,fake_near,0x00290BE7u,ENT(0));
    seen_after_near=F(0x0049F3C4u);
    inner(pc_mapmaker_crowe_sparks,fake_spark,0x00290C02u,0x42ACCCCDu);   /* 86.4 */
    g_esp+=4u;
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
    crw.read=0;
}
static void stock_consts(void)
{
    static const uint32_t bits[7]={0x42AC0000u,0x42AC68F6u,0x42B08000u,0x42B0CCCDu,0x439B3EB8u,0x42A1E666u,0x439B0000u};
    unsigned k;
    for(k=0;k<7u;k++) WORD_AT(cad[k])=bits[k];
}
static int consts_stock(void)
{
    static const uint32_t bits[7]={0x42AC0000u,0x42AC68F6u,0x42B08000u,0x42B0CCCDu,0x439B3EB8u,0x42A1E666u,0x439B0000u};
    unsigned k;
    for(k=0;k<7u;k++) if(WORD_AT(cad[k])!=bits[k]) return 0;
    return 1;
}
/* map container: map-info chunk (custom set), Story AI chunks */
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
        c+=8u+0x44u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
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
    memset((void *)((uintptr_t)A(i)+g_xbox_mem_offset),0,0xEA0u);
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);HALF_AT(ENT(i)+0x94u)=(uint16_t)model;
    WORD_AT(REC(i))=0x11000000u;WORD_AT(REC(i)+4u)=0;WORD_AT(REC(i)+8u)=R+index+1u;
}
static void level(void)
{
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CALL(pc_mapmaker_crowe_level,fake,0x0015E81Du,0,0,0,1);
}
/* does a level with this set give the Crowe its class? */
static int level_gives_class(void)
{
    level();
    actor(0,0x46,0x5E8,0);
    seen_class[1]=0xEEu;
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    return seen_class[1]==0x40000u;
}

int main(void)
{
    static const uint16_t with[]={0x0C,0x46,0xA8}, without[]={0x0C,0x28,0xA8}, x_with[]={0xFFFF,0x46};
    static const uint8_t boss_off[]={0,0}, boss_bar[]={0xB1,0}, boss_sub[]={0xB5,0};
    unsigned k;
    float gx,gz;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x00783A50u)=R;
    stock_consts();
    WORD_AT(0x007B0C7Cu)=PLAYER;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    container(0x11,with,3,boss_off,2);
    CHECK(!level_gives_class() && nmount==0 && nalloc==0);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(consts_stock() && near_eq(seen_const[0],86.0f));
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");               /* limits need the pools */
    CHECK(!level_gives_class() && nmount==0 && nalloc==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- the set decides ---------------------------------------------- */
    container(0x11,without,3,boss_off,2);
    CHECK(!level_gives_class() && nmount==0 && nalloc==0);
    container(0x12,with,3,boss_off,2);                 /* a profile set: not the map's members */
    CHECK(!level_gives_class() && nmount==0 && nalloc==0);
    container(0x11,without,3,boss_off,2);container_extra(x_with,2);   /* slot 12 */
    CHECK(level_gives_class() && nmount==1 && mount_arg==0x0049DC78u && nalloc==1 && nparts==1);
    container(0x11,with,3,boss_off,2);
    BYTE_AT(0x007DE455u)=3u;                            /* not story */
    CHECK(!level_gives_class());
    BYTE_AT(0x007DE455u)=10u;
    WORD_AT(0x0079094Cu)=14u;                           /* mission 10 itself */
    CHECK(!level_gives_class());
    WORD_AT(0x0079094Cu)=100u;
    WORD_AT(0x00784014u)=0;                             /* unreadable container: assumed */
    CHECK(level_gives_class());
    container(0x11,with,3,boss_off,2);

    /* mount: once; never when already mounted; not into a full table */
    nmount=0;
    level();
    CHECK(nmount==1 && pak_mounted);
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x006D7880u)=29u;
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1 && !pak_mounted);
    WORD_AT(0x006D7880u)=28u;
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    /* TSFP_MAPMAKER_REAL_BOSS=0: no pak, no state */
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    nmount=nalloc=0;
    CHECK(!level_gives_class() && nmount==0 && nalloc==0 && !crw_lv.state);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* class: needs the pak and this level's state; one per map; opt-out */
    level();
    actor(0,0x46,0x5E8,0);actor(1,0x46,0x5E8,1);actor(2,0x0C,0x200,1);actor(3,0x0C,0x5E8,1);
    pak_mounted=0;
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[1]==0 && !crw_lv.record);
    pak_mounted=1;
    WORD_AT(0x0075F5E8u)=0x00740000u;                   /* not this level's state */
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[1]==0 && !crw_lv.record);
    WORD_AT(0x0075F5E8u)=CST;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[1]==0 && !crw_lv.record);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x40000u && crw_lv.record==REC(0) && crw_lv.actor==A(0));
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* same record again */
    CHECK(seen_class[1]==0x40000u && crw_lv.record==REC(0));
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(1),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && crw_lv.record==REC(0));
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(3),0,0,1);   /* by model: a second one */
    CHECK(seen_class[1]==0 && crw_lv.record==REC(0));

    /* ---- the fight: relocation and the floor ------------------------------ */
    CHECK(crw_lv.boss==0);
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(seen_state==0);
    /* the type-0 cell nearest the outer centre (305.08, 80.95): row 2, col 4 = cell 18, centre (309.06, 78.8) */
    gx=F(CST+0x970u+18u*0x28u+0x10u);gz=F(CST+0x970u+18u*0x28u+0x14u);
    CHECK(near_eq(gx,10.0f) && near_eq(gz,-20.0f));
    CHECK(near_eq(F(CST+0x970u+18u*0x28u),10.0f-1.61f) && near_eq(F(CST+0x970u+18u*0x28u+8u),10.0f+1.61f));
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+4u),-21.6f) && near_eq(F(CST+0x970u+18u*0x28u+0xCu),-18.4f));
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+0x18u),1.61f) && WORD_AT(CST+0x970u+18u*0x28u+0x20u)==0);   /* sizes, type kept */
    CHECK(near_eq(F(CST+0x970u+34u*0x28u+0x10u),10.0f+(283.45f+36.0f+1.61f-309.06f)));
    CHECK(near_eq(F(CST+0x970u),283.45f-299.06f) && near_eq(F(CST+0x974u),61.2f-98.8f));
    CHECK(near_eq(F(CST+0xEF8u),305.08f-299.06f) && near_eq(F(CST+0xEFCu),80.95f-98.8f) && near_eq(F(CST+0xF00u),21.63f));
    CHECK(near_eq(F(CST+0xF60u),290.0f-299.06f) && near_eq(F(CST+0xF60u+9u*0x28u+0x14u),62.5f-98.8f));
    CHECK(WORD_AT(CST+0xF60u+10u*0x28u)==0 && WORD_AT(CST+0xF60u+13u*0x28u+0x14u)==0);   /* unused edges */
    CHECK(near_eq(crw_lv.dy,4.0f-86.0f));
    /* during the call: the arena constants follow the floor and the grid */
    CHECK(near_eq(seen_const[0],4.0f) && near_eq(seen_const[1],86.205f-82.0f) && near_eq(seen_const[2],88.25f-82.0f));
    CHECK(near_eq(seen_const[3],88.4f-82.0f) && near_eq(seen_const[4],310.49f-299.06f) && near_eq(seen_const[5],80.95f-98.8f));
    CHECK(near_eq(seen_const[6],310.0f-299.06f));
    CHECK(near_eq(seen_near,86.0f) && near_eq(seen_after_near,4.0f));    /* a distance inside sub_0028DD50 */
    CHECK(near_eq(seen_spark,86.4f-82.0f));
    CHECK(consts_stock());                                               /* restored after the call */
    /* the grid moves once per level (state 0 again, e.g. a respawn elsewhere) */
    SETF(ENT(0)+0x64u,50.0f);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+0x10u),10.0f) && consts_stock());
    SETF(ENT(0)+0x64u,10.0f);
    /* no intro cutscene: state 1 becomes 2 before the original */
    WORD_AT(A(0)+0x20u)=1u;WORD_AT(A(0)+0x9D0u)=0xFFFFFFFFu;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(seen_state==2u && seen_next==2u && WORD_AT(A(0)+0x9D4u)==1u);
    WORD_AT(A(0)+0x20u)=4u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(seen_state==4u);
    /* a constant that is not stock is left alone */
    SETF(0x0049F4D4u,1.5f);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(seen_const[3],1.5f) && near_eq(F(0x0049F4D4u),1.5f) && near_eq(seen_const[2],6.25f));
    stock_consts();
    /* not the boss, or another scene: stock */
    actor(5,0x0C,0x200,0);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(5),0,0,1);
    CHECK(near_eq(seen_const[0],86.0f));
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(seen_const[0],86.0f));
    WORD_AT(0x0079094Cu)=100u;
    WORD_AT(0x0075F5E8u)=0x00740000u;                  /* not this level's state */
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(seen_const[0],86.0f));
    WORD_AT(0x0075F5E8u)=CST;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(seen_const[0],4.0f) && consts_stock());
    /* outside the fight the callee hooks are stock */
    inner(pc_mapmaker_crowe_sparks,fake_spark,0x00290C02u,0x42ACCCCDu);
    CHECK(near_eq(seen_spark,86.4f));

    /* ---- turret, level marker, effects ------------------------------------ */
    CALL(pc_mapmaker_crowe_part,fake,0x0028ECF1u,1u,0x3F800000u,0,0);
    CHECK(nexplode==1 && explode_args[0]==0x20u && explode_args[1]==0x17u && explode_args[2]==CST+0x2BFCu+0x5Cu);
    CHECK(!explode_args[3] && !explode_args[4] && !explode_args[5] && !explode_args[6] && explode_args[7]==1u);
    CALL(pc_mapmaker_crowe_part,fake,0x0028ECF1u,3u,0,0,0);              /* no such turret: nothing */
    CHECK(nexplode==1);
    CALL(pc_mapmaker_crowe_part,fake,0x0028ECF6u,1u,0,0,1);              /* another caller */
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_crowe_part,fake,0x0028ECF1u,1u,0,0,1);
    WORD_AT(0x0079094Cu)=100u;
    CHECK(nexplode==1);
    CALL(pc_mapmaker_crowe_event,fake,0x002923A8u,0,0,0,0);
    CALL(pc_mapmaker_crowe_event,fake,0x00236592u,0,0,0,1);              /* the other caller (sub_00236070) */
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_crowe_event,fake,0x002923A8u,0,0,0,1);
    WORD_AT(0x0079094Cu)=100u;
    ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1 && draw_scene==100u);
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE91u,0,0,0,1);
    CHECK(ndraw==1);
    WORD_AT(0x0075F5E8u)=0x00740000u;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1);
    WORD_AT(0x0075F5E8u)=CST;

    /* ---- bar and meter ---------------------------------------------------- */
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_crowe_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    WORD_AT(PLAYER+0xFF4u)=ENT(5);HALF_AT(ENT(5)+0x94u)=0x5E8u;
    CALL(pc_mapmaker_crowe_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(5));
    HALF_AT(ENT(5)+0x94u)=0x200u;
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(5),0,0x7Fu,1);
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D25Fu,A(0),0,0x7Fu,1);
    crw_lv.boss=CRW_BOSS_SUB;
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);
    crw_lv.boss=CRW_BOSS_BAR;
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);

    /* ---- the end: removed once, focus dropped ----------------------------- */
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    WORD_AT(A(0)+0x20u)=0x12u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,0);
    CHECK(nremove==1 && remove_arg==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==1);

    /* ---- Boss flag from the Story AI chunk; the bosses area must be on ---- */
    container(0x11,with,3,boss_bar,2);
    CHECK(level_gives_class() && crw_lv.boss==CRW_BOSS_BAR);
    container(0x11,with,3,boss_sub,2);
    CHECK(level_gives_class() && crw_lv.boss==CRW_BOSS_SUB);
    bosses_area=0;
    CHECK(level_gives_class() && crw_lv.boss==0);
    bosses_area=1;

    /* ---- a spawn that is not a point keeps mission 10's coordinates ------- */
    SETF(ENT(0)+0x64u,NAN);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+0x10u),309.06f) && near_eq(seen_const[0],86.0f) && consts_stock());

    /* ---- allocation failures --------------------------------------------- */
    alloc_ok=0;
    CHECK(!level_gives_class() && !crw_lv.state);
    alloc_ok=1;
    WORD_AT(0x0078B0D0u)=OLDPRT;parts_ok=0;
    CHECK(!level_gives_class() && !crw_lv.state && WORD_AT(0x0078B0D0u)==OLDPRT && WORD_AT(0x0075F5E8u)==0);
    parts_ok=1;
    /* the next level without the Crowe forgets this one */
    CHECK(level_gives_class());
    container(0x11,without,3,boss_off,2);
    level();
    CHECK(!crw_lv.record && !crw_lv.state);
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_crowe_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(0));
    CALL(pc_mapmaker_crowe_event,fake,0x002923A8u,0,0,0,1);
    for(k=0;k<1;k++) CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker crowe ok");
    return 0;
}
