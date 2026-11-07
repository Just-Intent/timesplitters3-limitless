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
#define EFF1_K(i) (0x00900000u+(uint32_t)(i)*0x10000u)   /* boss i's effect blocks [0x0078B090] / [0x0078B098], 0x55F0 each */
#define EFF0_K(i) (EFF1_K(i)+0x8000u)
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
void sub_0028D0C0(void) { g_ecx=55; }
void sub_0028D710(void) { g_ecx=6; }
void sub_000EFBD0(void) { g_ecx=7; }
void sub_0015DDA0(void) { g_ecx=8; }
void sub_0005F460(void) { g_ecx=9; }
void sub_00087310(void) { g_ecx=10; }
void sub_000C3C50(void) { g_ecx=11; }
static int bosses_area=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)hook;return strcmp(area,"bosses") || bosses_area; }

static int pak_mounted, mount_ok=1, alloc_ok=1, parts_ok=1;
static unsigned nmount, nalloc, nparts, ndraw, nexplode, nremove, alloc_idx;
static unsigned reinit_n;               /* sub_0028D0C0 alone: a restart's set-up in place */
static uint32_t reinit_seen[8][5];      /* state, effect blocks, count, particle block it saw */
static uint32_t draw_state[8], draw_parts[8];
static uint32_t mount_arg, draw_scene, explode_args[8], remove_arg;
/* MapMaker rooms for the room query sub_00087310(box, out, max): boxes min
 * xyz, max xyz; the count of those the box overlaps */
static float rooms[8][6];
static unsigned nrooms, nquery, nanim;
static uint32_t query_box, anim_args[4];
static float query_last[6];
static void room(unsigned i,float x0,float y0,float z0,float x1,float y1,float z1)
{
    rooms[i][0]=x0;rooms[i][1]=y0;rooms[i][2]=z0;rooms[i][3]=x1;rooms[i][4]=y1;rooms[i][5]=z1;
    if(i>=nrooms) nrooms=i+1u;
}
static void one_big_room(void) { nrooms=0;room(0,-1000.0f,-1000.0f,-1000.0f,1000.0f,1000.0f,1000.0f); }
/* The floor finder sub_000C3C50(point, up, radius): the floor of (x, z), the
 * room (> 0) when it is at most up above the point, height in [0x004DCB00];
 * the stock also writes [0x004DCAFC] and [0x00733088]. No floor map: none. */
static int (*floor_of)(float x,float z,float *y);
/* ... or the highest floor at most `top` (the point + up) at (x, z) */
static int (*floor_below)(float x,float top,float z,float *y);
static unsigned nfloor;
/* the stock grid: 5 x 7 cells (type 0 on even rows), outer, 14 edges (10 used).
 * The type-0 cell nearest the outer centre is column 3; 310.0 is in column 4
 * (grid_gap 1: in no cell, and cell 17 not a platform; 2: cell 18 not a
 * platform). The build leaves -1 in the cell indices. */
static int grid_gap;
static void build_grid(uint32_t st)
{
    unsigned k;
    for(k=0;k<4u;k++) WORD_AT(st+0x1190u+k*4u)=0xFFFFFFFFu;
    for(k=0;k<35u;k++) {
        uint32_t c=st+0x970u+k*0x28u;
        float x0=283.45f+(float)(k%7u)*6.0f+(grid_gap==1 && k%7u==4u?3.4f:0.0f),z0=61.2f+(float)(k/7u)*8.0f;
        SETF(c,x0);SETF(c+4u,z0);SETF(c+8u,x0+3.22f);SETF(c+0xCu,z0+3.2f);
        SETF(c+0x10u,x0+1.61f);SETF(c+0x14u,z0+1.6f);SETF(c+0x18u,1.61f);SETF(c+0x1Cu,1.6f);
        WORD_AT(c+0x20u)=((k/7u)%2u || (grid_gap==1 && k==17u) || (grid_gap==2 && k==18u))?1u:0u;WORD_AT(c+0x24u)=0;
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
        if(alloc_ok) {   /* boss n of this level: its own blocks */
            WORD_AT(0x0078B090u)=EFF1_K(alloc_idx);WORD_AT(0x0078B098u)=EFF0_K(alloc_idx);WORD_AT(0x0078B08Cu)=0x1F4u;
            WORD_AT(0x0078B080u)=0xA0u+alloc_idx;WORD_AT(0x0078B0ECu)=0xB0u+alloc_idx;WORD_AT(0x0078B188u)=0xC0u+alloc_idx;
            WORD_AT(0x0075F5E8u)=CST+alloc_idx*0x3000u;build_grid(CST+alloc_idx*0x3000u);
        }
        alloc_idx++;
        return 0;
    }
    if(f==sub_0028D0C0) {   /* every field and the arena grid */
        if(reinit_n<8u) {
            reinit_seen[reinit_n][0]=WORD_AT(0x0075F5E8u);reinit_seen[reinit_n][1]=WORD_AT(0x0078B090u);
            reinit_seen[reinit_n][2]=WORD_AT(0x0078B098u);reinit_seen[reinit_n][3]=WORD_AT(0x0078B08Cu);reinit_seen[reinit_n][4]=WORD_AT(0x0078B0D0u);
        }
        reinit_n++;WORD_AT(0x0078B080u)=0xD0u;build_grid(WORD_AT(0x0075F5E8u));return 0;
    }
    if(f==sub_002A62C0) { nparts++;WORD_AT(0x0078B0D0u)=parts_ok?(alloc_idx<=1u?PRT:0x00740000u+(alloc_idx-1u)*0x8000u):0u;return 0; }
    if(f==sub_0028D710) { if(ndraw<8u) { draw_state[ndraw]=WORD_AT(0x0075F5E8u);draw_parts[ndraw]=WORD_AT(0x0078B0D0u); }
                          ndraw++;draw_scene=WORD_AT(0x0079094Cu);return 0; }
    if(f==sub_000EFBD0) { nexplode++;memcpy(explode_args,x,sizeof explode_args);return 0; }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_0005F460) { nanim++;memcpy(anim_args,x,sizeof anim_args);return 0; }
    if(f==sub_000C3C50) {
        float p[3],y;
        nfloor++;
        if(x[1]!=0x3FA00000u || x[2]!=0x3DCCCCCDu || x[0]<g_esp || (x[0]>=query_box && x[0]<query_box+0x38u)) {
            fprintf(stderr,"floor finder arguments\n");exit(4);
        }
        memcpy(p,(const void *)((uintptr_t)x[0]+g_xbox_mem_offset),12);
        WORD_AT(0x004DCAFCu)=0xDEADu;WORD_AT(0x00733088u)=0xBEEFu;
        if(floor_below) { if(!floor_below(p[0],p[1]+1.25f,p[2],&y)) return 0; }
        else if(!floor_of || !floor_of(p[0],p[2],&y) || y>p[1]+1.25f) return 0;
        memcpy((void *)((uintptr_t)0x004DCB00u+g_xbox_mem_offset),&y,4);
        return 3u;
    }
    if(f==sub_00087310) {
        uint32_t found=0,r,j;
        float b[6];
        nquery++;query_box=x[0];
        if(x[1]!=x[0]+0x18u || x[2]!=8u || x[0]<g_esp || (x[0]&15u)) { fprintf(stderr,"room query arguments\n");exit(4); }
        for(r=0;r<6u;r++) { memcpy(&b[r],(const void *)((uintptr_t)x[0]+r*4u+g_xbox_mem_offset),4);query_last[r]=b[r]; }
        for(r=0;r<nrooms;r++) {
            for(j=0;j<3u;j++) if(!(b[j]<rooms[r][3u+j] && b[3u+j]>rooms[r][j])) break;
            if(j==3u) found++;
        }
        return found;
    }
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
/* stock state 0: x = [0x0049F3D8], the cell of (x, own z) by sub_002A9330 into
 * +0x1198/+0x119C, the boss on that cell's centre (index -1: cell -8) */
static void fake_start(uint32_t ent)
{
    uint32_t st=WORD_AT(0x0075F5E8u),r,c,cell;
    int32_t i;
    float x=F(0x0049F3D8u),z=F(ent+0x6Cu);
    for(r=0;r<5u;r++) {
        cell=st+0x970u+r*7u*0x28u;
        if(!(z>F(cell+4u) && F(cell+0xCu)>z)) continue;
        for(c=0;c<7u;c++) { cell=st+0x970u+(r*7u+c)*0x28u;if(x>F(cell) && F(cell+8u)>x) { WORD_AT(st+0x1198u)=r;WORD_AT(st+0x119Cu)=c;break; } }
        break;
    }
    i=(int32_t)WORD_AT(st+0x1198u)*7+(int32_t)WORD_AT(st+0x119Cu);
    cell=st+0x970u+(uint32_t)(i*0x28);
    SETF(ent+0x64u,F(cell+0x10u));SETF(ent+0x6Cu,F(cell+0x14u));
}
static uint32_t live_particles, seen_parts_on, seen_state_ptr, seen_parts_ptr;
static CrwPrev prev_k;
#define FXW(fx,a) (*(uint32_t *)((uint8_t *)&(fx)+((a)-0x0078B080u)))   /* a static in an instance's saved copy */
static unsigned w_removed, restart_seen_n;
static uint32_t restart_seen_state;
static void fake_restart(void)   /* sub_001CDD60: the level heap set up again */
{
    originals++;restart_seen_n=crw_n;restart_seen_state=WORD_AT(0x0075F5E8u);alloc_idx=0;g_esp+=4u;
}
static void fake_fight(void)
{
    unsigned k;
    uint32_t a=WORD_AT(WORD_AT(g_esp+4u)+0x7Cu);
    originals++;
    for(k=0;k<7u;k++) seen_const[k]=F(cad[k]);
    seen_state=WORD_AT(a+0x20u);seen_next=WORD_AT(a+0x9D0u);
    seen_state_ptr=WORD_AT(0x0075F5E8u);seen_parts_ptr=WORD_AT(0x0078B0D0u);
    seen_parts_on=WORD_AT(CST+0x2E00u);WORD_AT(0x0078B0CCu)=live_particles;   /* sub_002A7D60 recounts */
    if(seen_state==0u && WORD_AT(0x0075F5E8u)==CST) fake_start(WORD_AT(g_esp+4u));
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
/* the egg drop's six arguments: hazard, cell, two positions, row, col */
static void egg_call(uint32_t hazard,uint32_t row,uint32_t col,uint32_t ret,int expect_orig,int line)
{
    WORD_AT(ESP0+0x10u)=ESP1;WORD_AT(ESP0+0x14u)=row;WORD_AT(ESP0+0x18u)=col;
    call(pc_mapmaker_crowe_eggs,fake,ret,hazard,CST+0x970u,ESP1,expect_orig,line);
    if(WORD_AT(ESP0+0x14u)!=row || WORD_AT(ESP0+0x18u)!=col) { fprintf(stderr,"%s:%d: egg arguments changed\n",__FILE__,line);failures++; }
}
#define EGG(h,r,c,ret,n) egg_call(h,r,c,ret,n,__LINE__)
/* stock sub_002A9D00: the cell lookups clear every cell's +0x24 (no egg of
 * its own is ever out: they are skipped) */
static void fake_cells(void)
{
    uint32_t st=WORD_AT(g_esp+4u)-4u,r,c,cell;
    unsigned k;
    float x=F(g_esp+8u),z=F(g_esp+0xCu);
    originals++;
    for(k=0;k<35u;k++) WORD_AT(st+0x970u+k*0x28u+0x24u)=0;
    for(r=0;r<5u;r++) {                                   /* sub_002A9330: the player's cell, no write when none */
        cell=st+0x970u+r*7u*0x28u;
        if(!(z>F(cell+4u) && F(cell+0xCu)>z)) continue;
        for(c=0;c<7u;c++) { cell=st+0x970u+(r*7u+c)*0x28u;if(x>F(cell) && F(cell+8u)>x) { WORD_AT(st+0x1190u)=r;WORD_AT(st+0x1194u)=c;break; } }
        break;
    }
    g_esp+=4u;
}
static uint32_t FB(float f) { uint32_t u;memcpy(&u,&f,4);return u; }
static unsigned blocked_cells(uint32_t st)
{
    unsigned k,n=0;
    for(k=0;k<35u;k++) n+=WORD_AT(st+0x970u+k*0x28u+0x24u)!=0;
    return n;
}
#define BLOCKED(st,r,c) WORD_AT((st)+0x970u+((r)*7u+(c))*0x28u+0x24u)
#define CELL(r,c) ((r)*7u+(c))
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
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;alloc_idx=0;
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

/* entries into a boss's code: what it sees */
static uint32_t entry_state, entry_parts, nest_before, nest_inner, nest_after;
static void fake_entry(void) { originals++;entry_state=WORD_AT(0x0075F5E8u);entry_parts=WORD_AT(0x0078B0D0u);g_esp+=4u; }
static void fake_fight_nest(void)   /* boss A's fight sends boss B a message */
{
    originals++;
    nest_before=WORD_AT(0x0075F5E8u);
    WORD_AT(0x0078B0CCu)=77u;                               /* A changes its live count during the call */
    inner(pc_mapmaker_crowe_message,fake_entry,0x0027D3EEu,ENT(1));
    nest_inner=entry_state;nest_after=WORD_AT(0x0075F5E8u);
    CHECK(WORD_AT(0x0078B0CCu)==77u);                      /* A's values survive B's call */
    g_esp+=4u;
}
/* boss A's fight hits another area's boss (a Crow walker: its own statics
 * swapped in), whose damage code hits A again (A's hit callback) */
static MapmakerFx other;
static uint32_t foreign_seen, again_parts, again_count, foreign_after;
static void fake_entry_again(void)
{
    originals++;again_parts=WORD_AT(0x0078B0D0u);again_count=WORD_AT(0x0078B0CCu);
    WORD_AT(0x0078B0CCu)=again_count+1u;g_esp+=4u;
}
static void fake_fight_foreign(void)
{
    MapmakerFx *p;
    originals++;
    WORD_AT(0x0078B0CCu)=55u;                               /* A's live count */
    p=mapmaker_fx_enter(&other);                            /* the walker's damage callback */
    foreign_seen=WORD_AT(0x0078B0D0u);
    WORD_AT(PLAYER+0x300u)=ENT(0);WORD_AT(PLAYER+0x304u)=ENT(5);
    inner(pc_mapmaker_crowe_hit,fake_entry_again,0x000BA3E0u,PLAYER+0x300u);
    foreign_after=WORD_AT(0x0078B0D0u);
    WORD_AT(0x0078B0CCu)=3u;                                /* the walker's count */
    mapmaker_fx_leave(p);
    CHECK(WORD_AT(0x0078B0CCu)==56u && WORD_AT(0x0075F5E8u)==CST);   /* A's update inside kept */
    g_esp+=4u;
}
static void multi(void)
{
    unsigned k;
    WORD_AT(0x0078B1D8u)=0x5A5A5A5Au;                       /* a walker-only static outside */
    level();
    for(k=0;k<6u;k++) actor(k,0x46,0x5E8,k);
    actor(4,0x0C,0x5E8,4);                                   /* by model */
    nalloc=0;
    WORD_AT(0x0078B090u)=0x00777000u;WORD_AT(0x0078B0D0u)=0x00778000u;WORD_AT(0x0075F5E8u)=0;   /* another boss's blocks */
    for(k=0;k<6u;k++) CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(k),0,0,1);
    CHECK(crw_n==4u && nalloc==3u);                        /* bosses 1-3 allocated at their AI start */
    for(k=0;k<4u;k++) CHECK(crw_inst[k].record==REC(k) && crw_inst[k].state==CST+k*0x3000u && WORD_AT(REC(k)+4u)==0x40000u);
    CHECK(WORD_AT(REC(4)+4u)==0 && WORD_AT(REC(4))==0x11000000u && WORD_AT(REC(5)+4u)==0);   /* the fifth and sixth: bots */
    CHECK(WORD_AT(0x0078B090u)==0x00777000u && WORD_AT(0x0078B0D0u)==0x00778000u && WORD_AT(0x0075F5E8u)==0);
    CHECK(FXW(crw_inst[1].fx,0x0078B090u)==EFF1_K(1) && FXW(crw_inst[1].fx,0x0078B0D0u)==0x00748000u && FXW(crw_inst[1].fx,0x0078B080u)==0xA1u);
    CHECK(crw_inst[3].state==CST+0x9000u && FXW(crw_inst[3].fx,0x0078B188u)==0xC3u && FXW(crw_inst[2].fx,0x0078B0ECu)==0xB2u);
    CHECK(FXW(crw_inst[2].fx,0x0078B1D8u)==0x5A5A5A5Au && !mapmaker_fx_holder);   /* the rest of the window from outside */
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);   /* the same record again */
    CHECK(crw_n==4u && nalloc==3u && crw_inst[2].record==REC(2));
    /* each fight with its own blocks; each arena under its own spawn */
    for(k=0;k<4u;k++) { SETF(ENT(k)+0x64u,10.0f+40.0f*k);SETF(ENT(k)+0x68u,4.0f+k);SETF(ENT(k)+0x6Cu,-20.0f);WORD_AT(A(k)+0x20u)=0; }
    /* the map under them: bosses 0 and 1 on open floor, boss 2 over nothing (its
     * start in no room: not checked), boss 3 with its columns 0 and 1 off the map */
    nrooms=0;
    room(0,-20.0f,0.0f,-40.0f,65.0f,20.0f,0.0f);
    room(1,115.0f,0.0f,-40.0f,150.0f,20.0f,0.0f);
    for(k=0;k<4u;k++) {
        CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(k),0,0,1);
        CHECK(seen_state_ptr==CST+k*0x3000u && seen_parts_ptr==(k?0x00740000u+k*0x8000u:PRT));
        CHECK(near_eq(seen_const[0],4.0f+k) && consts_stock() && crw_cur==-1);
        CHECK(near_eq(F(CST+k*0x3000u+0x970u+18u*0x28u+0x10u),10.0f+40.0f*k));   /* its start cell at its spawn */
        CHECK(near_eq(crw_inst[k].dy,4.0f+k-86.0f) && crw_inst[k].relocated);
    }
    one_big_room();
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].off[k] && !crw_inst[1].off[k] && !crw_inst[2].off[k] && crw_inst[3].off[k]==(k%7u<2u));
    for(k=0;k<4u;k++) {                                       /* each boss's cells in its own grid */
        prev_k=crw_enter((int)k);
        CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+k*0x3000u+4u,FB(10.0f+40.0f*k),FB(-20.0f),1);
        CHECK(blocked_cells(CST+k*0x3000u)==(k==3u?10u:0u));
        crw_leave(prev_k);
    }
    /* boss 2's walk is blocked: its cells ahead, nobody else's */
    WORD_AT(A(2)+0x20u)=9u;WORD_AT(CST+0x6000u+0x1D24u)=0u;WORD_AT(CST+0x6000u+0x2DE8u)=10u;
    SETF(CST+0x6000u+0x1CECu,90.0f);SETF(CST+0x6000u+0x1CF0u,-4.0f);
    for(k=0;k<91u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(2),0,0,1);
    CHECK(WORD_AT(A(2)+0x20u)==10u && crw_inst[2].wall_left[CELL(3u,4u)]==1800u && crw_inst[2].wall_left[CELL(4u,4u)]==1800u);
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].wall_left[k] && !crw_inst[1].wall_left[k] && !crw_inst[3].wall_left[k]);
    prev_k=crw_enter(2);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+0x6004u,FB(90.0f),FB(-20.0f),1);
    CHECK(blocked_cells(CST+0x6000u)==2u);
    crw_leave(prev_k);
    prev_k=crw_enter(0);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,FB(10.0f),FB(-20.0f),1);
    CHECK(blocked_cells(CST)==0);
    crw_leave(prev_k);
    memset(crw_inst[2].wall_left,0,sizeof crw_inst[2].wall_left);
    WORD_AT(A(2)+0x20u)=0;
    CHECK(WORD_AT(0x0078B090u)==0x00777000u && WORD_AT(0x0078B0D0u)==0x00778000u && WORD_AT(0x0075F5E8u)==0);
    /* a value a boss's call leaves behind is its own next time */
    WORD_AT(A(1)+0x20u)=4u;live_particles=9u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(1),0,0,1);
    CHECK(FXW(crw_inst[1].fx,0x0078B0CCu)==9u && WORD_AT(0x0078B0CCu)!=9u);
    live_particles=0;
    /* one boss's code inside another's: each sees its own, the first gets its own back */
    WORD_AT(A(0)+0x20u)=4u;
    CALL(pc_mapmaker_crowe_fight,fake_fight_nest,0x0027DE3Eu,ENT(0),0,0,2);   /* its original and the inner message */
    CHECK(nest_before==CST && nest_inner==CST+0x3000u && nest_after==CST && crw_cur==-1 && FXW(crw_inst[0].fx,0x0078B0CCu)==77u);
    /* A again inside another area's boss inside A's fight: A's latest values, everyone's back */
    memset(&other,0,sizeof other);FXW(other,0x0078B0D0u)=0x00799000u;
    CALL(pc_mapmaker_crowe_fight,fake_fight_foreign,0x0027DE3Eu,ENT(0),0,0,2);
    CHECK(foreign_seen==0x00799000u && again_parts==PRT && again_count==55u && foreign_after==0x00799000u);
    CHECK(FXW(other,0x0078B0CCu)==3u && FXW(crw_inst[0].fx,0x0078B0CCu)==56u && !mapmaker_fx_holder && crw_cur==-1);
    CHECK(WORD_AT(0x0078B0D0u)==0x00778000u && WORD_AT(0x0075F5E8u)==0);
    /* messages, the think callback, the hit callback: the boss's own */
    CALL(pc_mapmaker_crowe_message,fake_entry,0x0027D3EEu,ENT(2),2u,0,1);
    CHECK(entry_state==CST+0x6000u && entry_parts==0x00750000u && WORD_AT(0x0075F5E8u)==0);
    CALL(pc_mapmaker_crowe_think,fake_entry,0x000A7760u,ENT(3),0,0,1);
    CHECK(entry_state==CST+0x9000u);
    WORD_AT(PLAYER+0x200u)=ENT(1);WORD_AT(PLAYER+0x204u)=ENT(5);
    CALL(pc_mapmaker_crowe_hit,fake_entry,0x000BA3E0u,PLAYER+0x200u,0,0,1);
    CHECK(entry_state==CST+0x3000u);
    entry_state=0x1234u;
    CALL(pc_mapmaker_crowe_message,fake_entry,0x0027D3EEu,ENT(5),2u,0,1);   /* a bot Crowe: stock */
    CHECK(entry_state==0);
    /* the draw: each boss with its own blocks */
    ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==4u);
    for(k=0;k<4u;k++) CHECK(draw_state[k]==CST+k*0x3000u);
    CHECK(WORD_AT(0x0078B0D0u)==0x00778000u && WORD_AT(0x0075F5E8u)==0);
    /* one boss dies: removed alone, the others fight and are drawn */
    WORD_AT(A(1)+0x20u)=0x12u;live_particles=0;
    w_removed=nremove;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(1),0,0,1);
    CHECK(nremove==w_removed+1u && remove_arg==ENT(1) && crw_inst[1].removed && !crw_inst[0].removed && !crw_inst[2].removed);
    ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==3u && draw_state[0]==CST && draw_state[1]==CST+0x6000u);
    WORD_AT(A(2)+0x20u)=4u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(2),0,0,1);
    CHECK(seen_state_ptr==CST+0x6000u && nremove==w_removed+1u);
    /* another scene: no Crowe draw */
    WORD_AT(0x0079094Cu)=14u;ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==0);
    WORD_AT(0x0079094Cu)=100u;
    /* a restart from the pause menu (sub_001CDD60, no level init) frees only
     * heap 0: every Crowe keeps its Level heap blocks and record and sets the
     * blocks up again in place (zeroed: the last run poisoned them; the stock
     * tail sub_0028D0C0, then its particle block), with no allocation; the
     * same records find them at the AI start and relocate again */
    WORD_AT(0x0075F5E8u)=0x00740000u;nalloc=0;nparts=0;
    for(k=0;k<4u;k++) {
        WORD_AT(CST+k*0x3000u+0x40u)=0xDEAD0000u+k;WORD_AT(CST+k*0x3000u+0x2E00u)=0x2Fu;
        WORD_AT(EFF1_K(k)+0x55ECu)=0xBEEF0000u+k;WORD_AT(EFF0_K(k)+4u)=0xBEEF0010u+k;
    }
    WORD_AT(PRT+0x7CFCu)=0xFEEDu;WORD_AT(0x00740000u+3u*0x8000u+0x10u)=0xFEEEu;WORD_AT(EFF1_K(0)+0x55F0u)=0xC0FFEEu;   /* past its block: kept */
    reinit_n=0;crw.logged=99u;
    (void)mapmaker_fx_enter(&crw_inst[1].fx);                         /* a call that never came back */
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);
    CHECK(restart_seen_n==4u && restart_seen_state==0 && crw_n==4u && nalloc==0 && nparts==0 && reinit_n==4u && !mapmaker_fx_holder && crw.logged<99u);
    for(k=0;k<4u;k++) {
        CHECK(crw_inst[k].state==CST+k*0x3000u && crw_inst[k].record==REC(k) && !crw_inst[k].relocated && !crw_inst[k].actor);
        CHECK(reinit_seen[k][0]==CST+k*0x3000u && reinit_seen[k][1]==EFF1_K(k) && reinit_seen[k][2]==EFF0_K(k) && reinit_seen[k][3]==0x1F4u);
        CHECK(reinit_seen[k][4]==0x00778000u);                             /* the particle block comes after the tail */
        CHECK(FXW(crw_inst[k].fx,0x0078B0D0u)==(k?0x00740000u+k*0x8000u:PRT) && FXW(crw_inst[k].fx,0x0078B090u)==EFF1_K(k));
        CHECK(FXW(crw_inst[k].fx,0x0078B098u)==EFF0_K(k) && FXW(crw_inst[k].fx,0x0078B08Cu)==0x1F4u && FXW(crw_inst[k].fx,0x0078B080u)==0xD0u);
        CHECK(!WORD_AT(CST+k*0x3000u+0x40u) && !WORD_AT(CST+k*0x3000u+0x2E00u) && !WORD_AT(EFF1_K(k)+0x55ECu) && !WORD_AT(EFF0_K(k)+4u));
    }
    CHECK(!WORD_AT(PRT+0x7CFCu) && !WORD_AT(0x00740000u+3u*0x8000u+0x10u) && WORD_AT(EFF1_K(0)+0x55F0u)==0xC0FFEEu);
    CHECK(!crw_inst[1].removed && !crw_inst[2].eggs);
    CHECK(WORD_AT(0x0078B090u)==0x00777000u && WORD_AT(0x0078B0D0u)==0x00778000u && WORD_AT(0x0075F5E8u)==0);   /* the outside statics */
    for(k=0;k<4u;k++) { actor(k,0x46,0x5E8,k);CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(k),0,0,1); }
    CHECK(crw_n==4u && nalloc==0 && crw_inst[0].state==CST && crw_inst[3].state==CST+0x9000u && WORD_AT(REC(0)+4u)==0x40000u);
    CHECK(!crw_inst[1].removed && !crw_inst[0].relocated && WORD_AT(0x0075F5E8u)==0 && crw_inst[2].actor==A(2));
    for(k=0;k<4u;k++) { SETF(ENT(k)+0x64u,20.0f+40.0f*k);SETF(ENT(k)+0x68u,4.0f);SETF(ENT(k)+0x6Cu,-20.0f); }
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(1),0,0,1);
    CHECK(crw_inst[1].relocated && seen_state_ptr==CST+0x3000u && near_eq(F(CST+0x3000u+0x970u+18u*0x28u+0x10u),60.0f));
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);    /* restarted again: the same blocks */
    CHECK(crw_n==4u && nalloc==0 && crw_inst[1].state==CST+0x3000u && !crw_inst[1].relocated);
    FXW(crw_inst[2].fx,0x0078B08Cu)=0x1234u;                              /* an effect count of neither size: it and the rest dropped */
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);
    CHECK(crw_n==2u && crw_inst[1].state==CST+0x3000u && !crw_inst[2].state && !crw_inst[2].record && !crw_inst[3].state && nalloc==0);
    crw_inst[0].state=0x100u;                                              /* not a usable block */
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);
    CHECK(!crw_n && !crw_inst[0].record && !crw_inst[1].state);
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);      /* none left: bot behaviour, no allocation */
    CHECK(!crw_n && nalloc==0);
    level();
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    WORD_AT(0x0079094Cu)=14u;                                              /* mission 10's own restart: stock */
    k=reinit_n;
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);
    CHECK(!crw_n && reinit_n==k);
    CALL(pc_mapmaker_crowe_restart,fake_restart,0x0003D4F0u,0,0,0,1);    /* no Crowe: nothing */
    CHECK(!crw_n && reinit_n==k);
    WORD_AT(0x0079094Cu)=100u;
    /* the next level: no boss, the outside values live; a stale state pointer cleared */
    WORD_AT(0x0075F5E8u)=0x00740000u;crw.logged=20u;
    level();
    CHECK(WORD_AT(0x0075F5E8u)==0);
    CHECK(crw_cur==-1 && crw_n==1u && !crw_inst[1].state);
    CHECK(crw.logged<4u);                                    /* a log budget per level */
    for(k=0;k<6u;k++) actor(k,0x0C,0x200,k);
    WORD_AT(PLAYER+0xFF4u)=0;
}

/* ---- v0.4.0-mapmaker.34: the Crowes walked into walls and off the map ---- */

static void cells_at(float px,float pz) { CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,FB(px),FB(pz),1); }
/* v0.4.0-mapmaker.40: one room, the boss's floor at y 4 with a lower floor
 * (y -1) beyond x 13 (columns 5, 6), a raised block (y 9) under cell 1,1, a
 * small step (4.5) under cell 4,0 and a gap between cells 0,2 and 0,3 */
static int ledge_floor(float x,float z,float *y)
{
    if(x>13.0f) *y=-1.0f;
    else if(z<-31.0f && x>0.5f && x<1.5f) return 0;
    else if(x<-11.0f && z>-8.0f) *y=4.5f;
    else if(fabsf(x+8.0f)<3.0f && fabsf(z+28.0f)<3.0f) *y=9.0f;
    else *y=4.0f;
    return 1;
}
static int no_floor(float x,float z,float *y) { (void)x;(void)z;(void)y;return 0; }
static void ledges(void)
{
    unsigned k;
    uint32_t a=A(0),st=CST;
    CHECK(level_gives_class());
    one_big_room();
    floor_of=ledge_floor;nfloor=0;
    WORD_AT(0x004DCB00u)=0x1111u;WORD_AT(0x004DCAFCu)=0x2222u;WORD_AT(0x00733088u)=0x3333u;
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(a+0x20u)=0;nquery=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nquery==35u && WORD_AT(st+0x1198u)==2u && WORD_AT(st+0x119Cu)==4u);
    /* the start cell, every cell in a room, the midpoints between good cells */
    CHECK(nfloor==1u+35u+(5u*4u+4u*5u-2u-2u)+1u);
    CHECK(crw_inst[0].ground_ok==1 && near_eq(crw_inst[0].ground,4.0f));
    CHECK(WORD_AT(0x004DCB00u)==0x1111u && WORD_AT(0x004DCAFCu)==0x2222u && WORD_AT(0x00733088u)==0x3333u);
    for(k=0;k<35u;k++) CHECK(crw_inst[0].off[k]==(k%7u>=5u || k==CELL(1u,1u)));
    CHECK(!crw_inst[0].off[CELL(4u,0u)]);                          /* a step, not a ledge */
    CHECK(crw_inst[0].gap_h[0][2]==1u);
    for(k=0;k<5u*6u;k++) CHECK(crw_inst[0].gap_h[k/6u][k%6u]==(k==2u));
    for(k=0;k<4u*7u;k++) CHECK(!crw_inst[0].gap_v[k/7u][k%7u]);
    prev_k=crw_enter(0);
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==11u && BLOCKED(st,2u,5u)==1u && BLOCKED(st,2u,6u)==1u && BLOCKED(st,1u,1u)==1u);
    CHECK(!BLOCKED(st,2u,3u) && !BLOCKED(st,2u,0u) && !BLOCKED(st,4u,0u) && !BLOCKED(st,0u,4u));
    /* the boss on row 0: the cells beyond the gap in line with it */
    WORD_AT(st+0x1198u)=0u;WORD_AT(st+0x119Cu)=4u;
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==14u && BLOCKED(st,0u,2u)==1u && BLOCKED(st,0u,1u)==1u && BLOCKED(st,0u,0u)==1u && !BLOCKED(st,0u,3u));
    WORD_AT(st+0x1198u)=0u;WORD_AT(st+0x119Cu)=2u;                  /* ... from the other side */
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==13u && BLOCKED(st,0u,3u)==1u && BLOCKED(st,0u,4u)==1u && !BLOCKED(st,0u,1u));
    WORD_AT(st+0x1198u)=2u;WORD_AT(st+0x119Cu)=4u;
    crw_leave(prev_k);
    /* no floor under the start cell: the probe is not trusted, the rooms still count */
    CHECK(level_gives_class());
    floor_of=no_floor;nfloor=0;
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nfloor==1u+9u && !crw_inst[0].ground_ok);                /* the spawn height, then climbing 9 search heights */
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].off[k]);
    for(k=0;k<5u*6u;k++) CHECK(!crw_inst[0].gap_h[k/6u][k%6u]);
    /* the boss's own floor a step lower than its spawn height: still its floor */
    CHECK(level_gives_class());
    floor_of=ledge_floor;
    SETF(ENT(0)+0x68u,4.9f);
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].off[CELL(2u,5u)] && !crw_inst[0].off[CELL(2u,3u)] && !crw_inst[0].off[CELL(4u,0u)]);
    CHECK(level_gives_class());                                   /* back on y 4, no floor map */
    floor_of=NULL;
    SETF(ENT(0)+0x68u,4.0f);
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    memset(crw_inst[0].off,0,sizeof crw_inst[0].off);
    memset(crw_inst[0].gap_h,0,sizeof crw_inst[0].gap_h);memset(crw_inst[0].gap_v,0,sizeof crw_inst[0].gap_v);
    WORD_AT(a+0x20u)=4u;
}
/* v0.4.0-mapmaker.42 ("the splitter crows aren't moving at all now"):
 * floors at y -20 and 0.5 (-4.5 beyond x 13), one room from y -0.5 to 5 */
static int two_floors(float x,float top,float z,float *y)
{
    float hi=x>13.0f?-4.5f:0.5f;
    (void)z;
    if(hi<=top) *y=hi;
    else if(-20.0f<=top) *y=-20.0f;
    else return 0;
    return 1;
}
/* one floor at y 4 whose finder misses points on the tile seams (the cells'
 * x midpoints, 6 apart) and 0.25 short of them */
static int seam_floor(float x,float z,float *y)
{
    float m=fmodf(x+11.0f+600.0f,6.0f),n=fmodf(x+11.25f+600.0f,6.0f);
    (void)z;
    if(m<0.05f || m>5.95f || n<0.05f || n>5.95f) return 0;
    *y=4.0f;
    return 1;
}
static void reference_floor(void)
{
    unsigned k;
    uint32_t a=A(0),st=CST;
    /* the spawn 10 under the floor the player walks on (arena floor -9.49,
     * player floor 0.5): the reference is the floor above it, the cells are
     * compared with that, and the drop beyond x 13 still blocks */
    CHECK(level_gives_class());
    nrooms=0;room(0,-1000.0f,-0.5f,-1000.0f,1000.0f,5.0f,1000.0f);
    floor_of=NULL;floor_below=two_floors;nfloor=0;
    WORD_AT(0x004DCB00u)=0x1111u;WORD_AT(0x004DCAFCu)=0x2222u;WORD_AT(0x00733088u)=0x3333u;
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,-9.49f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].ground_ok==2 && near_eq(crw_inst[0].ground,0.5f));
    CHECK(WORD_AT(0x004DCB00u)==0x1111u && WORD_AT(0x004DCAFCu)==0x2222u && WORD_AT(0x00733088u)==0x3333u);
    for(k=0;k<35u;k++) CHECK(crw_inst[0].off[k]==(k%7u>=5u));
    for(k=0;k<5u*6u;k++) CHECK(!crw_inst[0].gap_h[k/6u][k%6u]);
    prev_k=crw_enter(0);
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==10u && !BLOCKED(st,2u,3u) && !BLOCKED(st,0u,4u) && !BLOCKED(st,4u,0u));
    crw_leave(prev_k);
    /* the spawn on its own floor (-9.49 over -9.5), the player's floor 10 up:
     * the boss's floor counts, not the higher one */
    CHECK(level_gives_class());
    one_big_room();
    floor_below=NULL;floor_of=ledge_floor;
    SETF(ENT(0)+0x68u,4.01f);
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].ground_ok==1 && near_eq(crw_inst[0].ground,4.0f) && crw_inst[0].off[CELL(1u,1u)] && !crw_inst[0].off[CELL(2u,3u)]);
    /* tile seams: no gap, nothing blocked, the walk is free */
    CHECK(level_gives_class());
    floor_of=seam_floor;nfloor=0;
    SETF(ENT(0)+0x68u,4.0f);
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nfloor==1u+35u+30u*2u+28u);
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].off[k]);
    for(k=0;k<5u*6u;k++) CHECK(!crw_inst[0].gap_h[k/6u][k%6u]);
    for(k=0;k<4u*7u;k++) CHECK(!crw_inst[0].gap_v[k/7u][k%7u]);
    prev_k=crw_enter(0);
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==0);
    crw_leave(prev_k);
    CHECK(level_gives_class());                                   /* back on y 4, no floor map */
    floor_of=NULL;
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    memset(crw_inst[0].off,0,sizeof crw_inst[0].off);
    memset(crw_inst[0].gap_h,0,sizeof crw_inst[0].gap_h);memset(crw_inst[0].gap_v,0,sizeof crw_inst[0].gap_v);
    WORD_AT(a+0x20u)=4u;
}
static void walls(void)
{
    unsigned k;
    uint32_t a=A(0),st=CST;
    float z;
    /* a walk that stops getting nearer its target ends where the boss stands
     * (no teleport onto the target: into walls, off the map) as stock ends it
     * on arrival; the cells ahead in its direction stay blocked */
    WORD_AT(a+0x20u)=9u;SETF(st+0x1CECu,10.0f);SETF(st+0x1CF0u,-4.0f);   /* two rows on */
    WORD_AT(st+0x1D24u)=0u;WORD_AT(st+0x2DE8u)=10u;SETF(st+0x2DF0u,3.0f);
    WORD_AT(st+0x1198u)=2u;WORD_AT(st+0x119Cu)=4u;
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    memset(crw_inst[0].wall_left,0,sizeof crw_inst[0].wall_left);
    nanim=0;crw_inst[0].air=crw_inst[0].stall=0;
    for(k=0;k<90u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==9u && nanim==0 && crw_inst[0].stall==89u);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(F(ENT(0)+0x64u),10.0f) && near_eq(F(ENT(0)+0x6Cu),-20.0f) && near_eq(F(ENT(0)+0x68u),4.0f));
    CHECK(nanim==1u && anim_args[0]==ENT(0) && anim_args[1]==0x810u && anim_args[2]==0 && anim_args[3]==0x3F99999Au);
    CHECK(WORD_AT(a+0x20u)==10u && WORD_AT(a+0x9D0u)==10u && WORD_AT(a+0x9D4u)==9u && F(a+0x9D8u)==0.0f && F(st+0x2DF0u)==0.0f);
    CHECK(!crw_inst[0].air && !crw_inst[0].stall);
    CHECK(crw_inst[0].wall_left[CELL(3u,4u)]==1800u && crw_inst[0].wall_left[CELL(4u,4u)]==1800u);
    CHECK(!crw_inst[0].wall_left[CELL(2u,4u)] && !crw_inst[0].wall_left[CELL(1u,4u)] && !crw_inst[0].wall_left[CELL(3u,3u)]);
    prev_k=crw_enter(0);
    cells_at(10.0f,-20.0f);
    CHECK(blocked_cells(st)==2u && BLOCKED(st,3u,4u)==1u && BLOCKED(st,4u,4u)==1u);
    crw_leave(prev_k);
    /* they open again after 1800 of the boss's fight calls */
    for(k=0;k<1799u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].wall_left[CELL(3u,4u)]==1u);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    prev_k=crw_enter(0);
    cells_at(10.0f,-20.0f);
    CHECK(blocked_cells(st)==0);
    crw_leave(prev_k);
    /* the other directions: 1 col-, 2 row-, 3 col+; the state after is 4 when the walk's is not one */
    WORD_AT(a+0x20u)=9u;WORD_AT(st+0x1D24u)=1u;WORD_AT(st+0x2DE8u)=9u;
    for(k=0;k<91u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==4u && nanim==2u);
    for(k=0;k<4u;k++) CHECK(crw_inst[0].wall_left[CELL(2u,k)]==1800u);
    CHECK(!crw_inst[0].wall_left[CELL(2u,5u)] && !crw_inst[0].wall_left[CELL(2u,6u)]);
    memset(crw_inst[0].wall_left,0,sizeof crw_inst[0].wall_left);
    WORD_AT(a+0x20u)=9u;WORD_AT(st+0x1D24u)=3u;WORD_AT(st+0x2DE8u)=0x12u;
    for(k=0;k<91u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==4u && crw_inst[0].wall_left[CELL(2u,5u)]==1800u && crw_inst[0].wall_left[CELL(2u,6u)]==1800u);
    CHECK(!crw_inst[0].wall_left[CELL(2u,3u)]);
    memset(crw_inst[0].wall_left,0,sizeof crw_inst[0].wall_left);
    WORD_AT(a+0x20u)=9u;WORD_AT(st+0x1D24u)=2u;WORD_AT(st+0x2DE8u)=10u;
    for(k=0;k<91u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].wall_left[CELL(1u,4u)]==1800u && crw_inst[0].wall_left[CELL(0u,4u)]==1800u && !crw_inst[0].wall_left[CELL(3u,4u)]);
    memset(crw_inst[0].wall_left,0,sizeof crw_inst[0].wall_left);
    /* the boss in no cell: the walk still ends, nothing is blocked */
    WORD_AT(a+0x20u)=9u;WORD_AT(st+0x1198u)=0xFFFFFFFFu;WORD_AT(st+0x2DE8u)=10u;
    for(k=0;k<91u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==10u && nanim==5u);
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].wall_left[k]);
    WORD_AT(st+0x1198u)=2u;
    /* a walk that keeps getting nearer is left alone until 600 calls */
    WORD_AT(a+0x20u)=9u;WORD_AT(st+0x1D24u)=0u;SETF(st+0x1CF0u,400.0f);
    for(k=0,z=-20.0f;k<599u;k++,z+=0.3f) { SETF(ENT(0)+0x6Cu,z);CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1); }
    CHECK(WORD_AT(a+0x20u)==9u && nanim==5u && crw_inst[0].air==599u && !crw_inst[0].stall);
    SETF(ENT(0)+0x6Cu,z);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==10u && nanim==6u && near_eq(F(ENT(0)+0x6Cu),z));
    /* each walk counts from its own start */
    WORD_AT(a+0x20u)=9u;SETF(ENT(0)+0x6Cu,-20.0f);
    for(k=0;k<80u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    WORD_AT(a+0x20u)=4u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    WORD_AT(a+0x20u)=9u;
    for(k=0;k<80u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(WORD_AT(a+0x20u)==9u && nanim==6u);
    /* ... the 600 too: two long walks one after the other */
    for(k=0,z=-20.0f;k<500u;k++,z+=0.3f) { SETF(ENT(0)+0x6Cu,z);CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1); }
    WORD_AT(a+0x20u)=4u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    WORD_AT(a+0x20u)=9u;
    for(k=0,z=-20.0f;k<500u;k++,z+=0.3f) { SETF(ENT(0)+0x6Cu,z);CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1); }
    CHECK(WORD_AT(a+0x20u)==9u && nanim==6u && crw_inst[0].air==500u);
    SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(a+0x20u)=4u;
    memset(crw_inst[0].wall_left,0,sizeof crw_inst[0].wall_left);

    /* cells over walls / off the map: the relocated grid (columns at x -14 + 6c,
     * rows at z -36 + 8r, boss at 2,4 = (10, -20), y 4) over rooms: columns 2-4,
     * column 5 but row 2, column 6; column 0 only on a floor above */
    CHECK(level_gives_class());
    nrooms=0;
    room(0,-5.0f,0.0f,-40.0f,13.0f,10.0f,0.0f);
    room(1,13.5f,0.0f,-40.0f,18.5f,10.0f,-24.0f);
    room(2,13.5f,0.0f,-16.0f,18.5f,10.0f,0.0f);
    room(3,19.0f,0.0f,-40.0f,25.0f,10.0f,0.0f);
    room(4,-17.0f,20.0f,-40.0f,-11.0f,30.0f,0.0f);
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(a+0x20u)=0;nquery=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nquery==35u && WORD_AT(st+0x1198u)==2u && WORD_AT(st+0x119Cu)==4u);
    for(k=0;k<35u;k++) CHECK(crw_inst[0].off[k]==(k%7u<2u || k==CELL(2u,5u)));
    prev_k=crw_enter(0);
    /* blocked: the 11 cells in no room, and 2,6 beyond the hole at 2,5 in line with the boss */
    cells_at(10.0f,-12.0f);                                  /* the player inside, cell 3,4: stock's cell kept */
    CHECK(blocked_cells(st)==12u && BLOCKED(st,2u,6u)==1u && BLOCKED(st,2u,5u)==1u && BLOCKED(st,2u,0u)==1u);
    CHECK(!BLOCKED(st,1u,6u) && !BLOCKED(st,2u,3u) && !BLOCKED(st,2u,2u));
    CHECK(WORD_AT(st+0x1190u)==3u && WORD_AT(st+0x1194u)==4u);
    cells_at(-14.0f,-12.0f);                                 /* inside cell 3,0, which is in no room: still stock's */
    CHECK(WORD_AT(st+0x1190u)==3u && WORD_AT(st+0x1194u)==0u);
    /* the boss elsewhere: what lies beyond in its line */
    WORD_AT(st+0x1198u)=0u;WORD_AT(st+0x119Cu)=3u;
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==11u && !BLOCKED(st,2u,6u));
    WORD_AT(st+0x1198u)=2u;WORD_AT(st+0x119Cu)=6u;
    cells_at(10.0f,-12.0f);
    CHECK(blocked_cells(st)==14u && BLOCKED(st,2u,4u)==1u && BLOCKED(st,2u,3u)==1u && BLOCKED(st,2u,2u)==1u && !BLOCKED(st,1u,6u));
    WORD_AT(st+0x1198u)=2u;WORD_AT(st+0x119Cu)=4u;
    /* the player outside the arena: the nearest cell in a room stands in */
    WORD_AT(st+0x1190u)=0xFFFFFFFFu;WORD_AT(st+0x1194u)=0xFFFFFFFFu;
    cells_at(60.0f,-30.0f);                                  /* +x: 1,6 */
    CHECK(WORD_AT(st+0x1190u)==1u && WORD_AT(st+0x1194u)==6u);
    cells_at(-30.0f,-4.0f);                                  /* -x: 4,0 is in no room, 4,2 is the nearest that is */
    CHECK(WORD_AT(st+0x1190u)==4u && WORD_AT(st+0x1194u)==2u);
    cells_at(10.0f,-80.0f);                                  /* -z: 0,4 */
    CHECK(WORD_AT(st+0x1190u)==0u && WORD_AT(st+0x1194u)==4u);
    cells_at(NAN,-4.0f);                                     /* not a point: left alone */
    CHECK(WORD_AT(st+0x1190u)==0u && WORD_AT(st+0x1194u)==4u);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901C4u,CST+4u,FB(60.0f),FB(-30.0f),1);   /* another caller: stock */
    CHECK(WORD_AT(st+0x1190u)==0u && blocked_cells(st)==0);
    crw_leave(prev_k);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,FB(60.0f),FB(-30.0f),1);   /* outside the boss's code */
    CHECK(WORD_AT(st+0x1190u)==0u && blocked_cells(st)==0);
    /* no room under the start cell: the query is not trusted, nothing is off */
    CHECK(level_gives_class());
    nrooms=0;
    WORD_AT(a+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].off[k]);
    /* every cell off: the player's stand-in is the nearest cell */
    for(k=0;k<35u;k++) crw_inst[0].off[k]=1u;
    prev_k=crw_enter(0);
    cells_at(60.0f,-4.0f);
    CHECK(WORD_AT(st+0x1190u)==4u && WORD_AT(st+0x1194u)==6u && blocked_cells(st)==35u);
    crw_leave(prev_k);
    memset(crw_inst[0].off,0,sizeof crw_inst[0].off);
    one_big_room();
    WORD_AT(a+0x20u)=4u;
}

int main(void)
{
    static const uint16_t with[]={0x0C,0x46,0xA8}, without[]={0x0C,0x28,0xA8}, x_with[]={0xFFFF,0x46};
    static const uint8_t boss_off[]={0,0}, boss_bar[]={0xB1,0}, boss_sub[]={0xB5,0};
    unsigned k;
    float gx,gz;
    g_xbox_mem_offset=(uintptr_t)memory;
    one_big_room();
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
    pak_mounted=0;WORD_AT(0x006D7880u)=57u;
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1 && !pak_mounted);
    WORD_AT(0x006D7880u)=56u;
    CALL(pc_mapmaker_crowe_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    /* TSFP_MAPMAKER_REAL_BOSS=0: no pak, no state */
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    nmount=nalloc=0;
    CHECK(!level_gives_class() && nmount==0 && nalloc==0 && !crw_inst[0].state);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* class: needs the pak and this level's state; one per map; opt-out */
    level();
    actor(0,0x46,0x5E8,0);actor(1,0x46,0x5E8,1);actor(2,0x0C,0x200,1);actor(3,0x0C,0x5E8,1);
    pak_mounted=0;
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[1]==0 && !crw_inst[0].record);
    pak_mounted=1;
    CHECK(WORD_AT(0x0075F5E8u)==0 && crw_inst[0].state==CST && FXW(crw_inst[0].fx,0x0078B0D0u)==PRT && crw_n==1u);   /* only its copy */
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[1]==0 && !crw_inst[0].record);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(2),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x40000u && crw_inst[0].record==REC(0) && crw_inst[0].actor==A(0));
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* same record again */
    CHECK(seen_class[1]==0x40000u && crw_inst[0].record==REC(0));
    CHECK(crw_n==1u && nalloc==1u);                       /* more split Crowes: the section below */

    /* ---- the fight: relocation and the floor ------------------------------ */
    CHECK(crw_inst[0].boss==0);
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(seen_state==0);
    /* the start cell: column 4 (holds 310.0), its platform nearest the outer centre z 80.95:
     * row 2 = cell 18, centre (309.06, 78.8); not cell 17, the platform nearest the centre */
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
    CHECK(near_eq(crw_inst[0].dy,4.0f-86.0f));
    /* every moved cell asked the room query at the boss's height (one big room: none off),
     * with a body box on the guest stack below the hook's frame */
    CHECK(nquery==35u && query_box+0x38u<=ESP0 && WORD_AT(ESP0+4u)==ENT(0));
    CHECK(near_eq(query_last[0],F(CST+0x970u+34u*0x28u+0x10u)-0.3f) && near_eq(query_last[3],F(CST+0x970u+34u*0x28u+0x10u)+0.3f));
    CHECK(near_eq(query_last[2],F(CST+0x970u+34u*0x28u+0x14u)-0.3f) && near_eq(query_last[5],F(CST+0x970u+34u*0x28u+0x14u)+0.3f));
    CHECK(near_eq(query_last[1],4.25f) && near_eq(query_last[4],5.5f));
    for(k=0;k<35u;k++) CHECK(!crw_inst[0].off[k]);
    /* stock state 0 left the boss on the spawn point and the start cell */
    CHECK(near_eq(F(ENT(0)+0x64u),10.0f) && near_eq(F(ENT(0)+0x6Cu),-20.0f) && near_eq(F(ENT(0)+0x68u),4.0f));
    CHECK(WORD_AT(CST+0x1198u)==2u && WORD_AT(CST+0x119Cu)==4u && crw_inst[0].row==2u && crw_inst[0].col==4u && !crw_inst[0].place);
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
    WORD_AT(0x0075F5E8u)=0x00740000u;                  /* another value outside: the boss's own during the call */
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(seen_const[0],4.0f) && consts_stock() && seen_state_ptr==CST && seen_parts_ptr==PRT);
    CHECK(WORD_AT(0x0075F5E8u)==0x00740000u && crw_cur==-1);   /* the outside value back */
    WORD_AT(0x0075F5E8u)=0;
    /* outside the fight the callee hooks are stock */
    inner(pc_mapmaker_crowe_sparks,fake_spark,0x00290C02u,0x42ACCCCDu);
    CHECK(near_eq(seen_spark,86.4f));

    /* ---- turret, level marker, effects (called inside the boss's code) ---- */
    prev_k=crw_enter(0);
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
    /* the egg drop (v0.4.0-mapmaker.28 crash): skipped, the six arguments the caller's */
    CALL(pc_mapmaker_crowe_eggs,fake,0x00291E84u,CST+0x11A0u,CST+0x970u,ESP1,0);
    CHECK(crw.eggs==1u && WORD_AT(ESP0+4u)==CST+0x11A0u);
    CALL(pc_mapmaker_crowe_eggs,fake,0x00291E89u,CST+0x11A0u,0,0,1);     /* another caller */
    WORD_AT(0x0079094Cu)=14u;
    CALL(pc_mapmaker_crowe_eggs,fake,0x00291E84u,CST+0x11A0u,0,0,1);     /* mission 10: its eggs */
    WORD_AT(0x0079094Cu)=100u;
    CHECK(crw.eggs==1u);
    crw_leave(prev_k);
    CALL(pc_mapmaker_crowe_part,fake,0x0028ECF1u,1u,0,0,1);              /* outside a boss's code: stock */
    CHECK(nexplode==1);
    CALL(pc_mapmaker_crowe_eggs,fake,0x00291E84u,CST+0x11A0u,0,0,1);
    CHECK(crw.eggs==1u);
    ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1 && draw_scene==100u);
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE91u,0,0,0,1);
    CHECK(ndraw==1);
    WORD_AT(0x0075F5E8u)=0x00740000u;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==2 && draw_state[1]==CST && draw_parts[1]==PRT && WORD_AT(0x0075F5E8u)==0x00740000u);
    WORD_AT(0x0075F5E8u)=0;ndraw=1;

    /* ---- the skipped egg still blocks its cells (v0.4.0-mapmaker.31: state 5
     * took the same cell every time and the boss never walked) -------------- */
    prev_k=crw_enter(0);
    memset(crw_inst[0].egg_left,0,sizeof crw_inst[0].egg_left);
    k=crw_inst[0].eggs;
    EGG(CST+0x11A0u+2u*0x168u,2u,6u,0x00291E84u,0);
    CHECK(crw_inst[0].egg_left[2]==120u && crw_inst[0].egg_row[2]==2u && crw_inst[0].egg_col[2]==6u);
    EGG(CST+0x11A0u+2u*0x168u,0u,0u,0x00291E84u,0);                     /* that slot's egg still out: no new one */
    CHECK(crw_inst[0].egg_left[2]==120u && crw_inst[0].egg_row[2]==2u && crw_inst[0].egg_col[2]==6u);
    EGG(CST+0x11A0u+3u*0x168u+4u,1u,1u,0x00291E84u,0);                  /* not a hazard */
    EGG(CST+0x11A0u+3u*0x168u,5u,1u,0x00291E84u,0);                     /* no such row */
    EGG(CST+0x11A0u+3u*0x168u,1u,7u,0x00291E84u,0);                     /* no such column */
    EGG(CST+0x11A0u+8u*0x168u,1u,1u,0x00291E84u,0);                     /* past the eight */
    CHECK(!crw_inst[0].egg_left[3] && !crw_inst[0].egg_left[0] && crw_inst[0].eggs==k+6u);
    EGG(CST+0x11A0u+4u*0x168u,0u,0u,0x00291E84u,0);                     /* a corner */
    CHECK(crw_inst[0].egg_left[4]==120u);
    EGG(CST+0x11A0u+5u*0x168u,1u,1u,0x00291E89u,1);                     /* not the drop: stock, nothing kept */
    CHECK(!crw_inst[0].egg_left[5]);
    /* the lookups clear the blocks; the boss's skipped eggs block theirs again: the cell, above, below, left, right */
    for(k=0;k<35u;k++) WORD_AT(CST+0x970u+k*0x28u+0x24u)=7u;
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,0,0,1);
    CHECK(blocked_cells(CST)==7u && BLOCKED(CST,2u,6u)==1u && BLOCKED(CST,1u,6u)==1u && BLOCKED(CST,3u,6u)==1u && BLOCKED(CST,2u,5u)==1u);
    CHECK(BLOCKED(CST,0u,0u)==1u && BLOCKED(CST,1u,0u)==1u && BLOCKED(CST,0u,1u)==1u && !BLOCKED(CST,1u,5u) && !BLOCKED(CST,3u,5u));
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x00290E67u,CST+4u,0,0,1);  /* state 0's lookup */
    CHECK(blocked_cells(CST)==7u);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901C4u,CST+4u,0,0,1);  /* another caller: stock */
    CHECK(blocked_cells(CST)==0);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+0x3004u,0,0,1);   /* another boss's state */
    CHECK(blocked_cells(CST)==0 && blocked_cells(CST+0x3000u)==0);
    crw_leave(prev_k);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,0,0,1);  /* outside the boss's code: stock */
    CHECK(blocked_cells(CST)==0);
    /* the eggs' time runs out with the boss's own fight calls */
    WORD_AT(A(0)+0x20u)=4u;
    for(k=0;k<119u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(crw_inst[0].egg_left[2]==1u && crw_inst[0].egg_left[4]==1u);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(!crw_inst[0].egg_left[2] && !crw_inst[0].egg_left[4]);
    prev_k=crw_enter(0);
    CALL(pc_mapmaker_crowe_cells,fake_cells,0x002901BFu,CST+4u,0,0,1);
    CHECK(blocked_cells(CST)==0);
    crw_leave(prev_k);

    walls();
    ledges();
    reference_floor();

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
    crw_inst[0].boss=CRW_BOSS_SUB;
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);
    crw_inst[0].boss=CRW_BOSS_BAR;
    CALL(pc_mapmaker_crowe_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);

    /* ---- the end: effects play out without new smoke, then removed once ---- */
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    WORD_AT(A(0)+0x20u)=0x12u;
    WORD_AT(CST+0x2E00u)=0x2Fu;live_particles=3u;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==0 && seen_parts_on==0 && WORD_AT(CST+0x2E00u)==0x2Fu && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    CHECK(near_eq(seen_const[0],4.0f) && consts_stock());     /* the floor still follows the spawn */
    ndraw=0;
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1);                                          /* still drawn while they play out */
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==0 && crw_inst[0].fading==2u);
    live_particles=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==1 && remove_arg==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0 && WORD_AT(CST+0x2E00u)==0x2Fu);
    CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);
    CHECK(ndraw==1);                                          /* gone: nothing left to freeze */
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==1);
    /* particles that never die out (another boss's): removed after 300 calls */
    CHECK(level_gives_class());
    WORD_AT(A(0)+0x20u)=0x12u;live_particles=5u;
    for(k=0;k<299u;k++) CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==1);
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(nremove==2);
    live_particles=0;

    /* ---- several split Crowes: each its own blocks, arena and end -------- */
    multi();

    /* ---- Boss flag from the Story AI chunk; the bosses area must be on ---- */
    container(0x11,with,3,boss_bar,2);
    CHECK(level_gives_class() && crw_inst[0].boss==CRW_BOSS_BAR);
    container(0x11,with,3,boss_sub,2);
    CHECK(level_gives_class() && crw_inst[0].boss==CRW_BOSS_SUB);
    bosses_area=0;
    CHECK(level_gives_class() && crw_inst[0].boss==0);
    bosses_area=1;

    /* ---- a spawn that is not a point keeps mission 10's coordinates ------- */
    SETF(ENT(0)+0x64u,NAN);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+0x10u),309.06f) && near_eq(seen_const[0],86.0f) && consts_stock());

    /* ---- no cell holds 310.0: stock reads cell -8, the boss is put back ---- */
    grid_gap=1;
    CHECK(level_gives_class());
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    /* the platform nearest the centre (cell 18, centre 312.46; cell 17 is not one) is centred on the spawn */
    CHECK(near_eq(F(CST+0x970u+18u*0x28u+0x10u),10.0f) && near_eq(F(CST+0x970u+18u*0x28u+0x14u),-20.0f));
    CHECK(near_eq(F(ENT(0)+0x64u),10.0f) && near_eq(F(ENT(0)+0x6Cu),-20.0f));
    CHECK(WORD_AT(CST+0x1198u)==2u && WORD_AT(CST+0x119Cu)==4u && !crw_inst[0].place && consts_stock());
    /* ---- the start column's nearest cell is not a platform: the nearest platform in it ---- */
    grid_gap=2;
    CHECK(level_gives_class());
    SETF(ENT(0)+0x64u,10.0f);SETF(ENT(0)+0x68u,4.0f);SETF(ENT(0)+0x6Cu,-20.0f);
    WORD_AT(A(0)+0x20u)=0;
    CALL(pc_mapmaker_crowe_fight,fake_fight,0x0027DE3Eu,ENT(0),0,0,1);
    /* column 4: rows 0 (62.8) and 4 (94.8) are platforms; row 4 is nearer 80.95 = cell 32 */
    CHECK(near_eq(F(CST+0x970u+32u*0x28u+0x10u),10.0f) && near_eq(F(CST+0x970u+32u*0x28u+0x14u),-20.0f));
    CHECK(near_eq(F(ENT(0)+0x64u),10.0f) && near_eq(F(ENT(0)+0x6Cu),-20.0f));
    CHECK(WORD_AT(CST+0x1198u)==4u && WORD_AT(CST+0x119Cu)==4u && crw_inst[0].row==4u && consts_stock());
    grid_gap=0;

    /* ---- allocation failures --------------------------------------------- */
    alloc_ok=0;
    CHECK(!level_gives_class() && !crw_inst[0].state);
    alloc_ok=1;                                              /* no Crowe this level, even if the AI start could allocate */
    CALL(pc_mapmaker_crowe_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(!crw_n && !crw_inst[0].record && WORD_AT(REC(0)+4u)==0);
    WORD_AT(0x0078B0D0u)=OLDPRT;parts_ok=0;
    CHECK(!level_gives_class() && !crw_inst[0].state && WORD_AT(0x0078B0D0u)==OLDPRT && WORD_AT(0x0075F5E8u)==0);
    parts_ok=1;
    /* the next level without the Crowe forgets this one */
    CHECK(level_gives_class());
    container(0x11,without,3,boss_off,2);
    level();
    CHECK(!crw_inst[0].record && !crw_inst[0].state);
    WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_crowe_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(0));
    CALL(pc_mapmaker_crowe_event,fake,0x002923A8u,0,0,0,1);
    for(k=0;k<1;k++) CALL(pc_mapmaker_crowe_fx,fake,0x001CEE8Cu,0,0,0,1);

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker crowe ok");
    return 0;
}
