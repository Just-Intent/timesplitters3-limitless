/* Asset-free contract for src/pc_mapmaker_grid.inc: seven floors and the
 * 80x80x8 occupancy grid only with TSFP_MAPMAKER_LIMITS=1, the stock grid
 * functions otherwise; the settings block bit-exact in stock mode; removal
 * keeps the cells of overlapping pieces (last placed wins, only inside the
 * removed box); overlap and stacked items only with their own switches, in
 * the editor, at the documented call sites, with the deny list. Every hook
 * must pop its return address (fake originals pop theirs). */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0xA00000/4], g_eax, g_ecx, g_edx, g_ebx, g_ebp, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define U16_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

void sub_0003DFA0(void) { g_ecx=0x11; }
static uint32_t heap_next=0x00500000u, last_alloc;
static unsigned allocs;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t size,p=0;
    va_start(a,n);
    if(f==sub_0003DFA0 && n==2) {
        size=va_arg(a,uint32_t);(void)va_arg(a,uint32_t);
        p=heap_next;heap_next+=(size+15u)&~15u;
        memset((char *)memory+p,0xCD,size);       /* dirty: the hook must clear it */
        last_alloc=size;allocs++;
    }
    va_end(a);
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_grid.inc"

static int failures, esp_errors;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define ESP0 0x00600000u
#define GRID 0x00784004u
#define PLACED 0x0078400Cu
#define ED 0x00100000u
#define SCENE 0x0079094Cu

static void set_env(const char *limits,const char *overlap,const char *stack)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_LIMITS",limits?limits:"");
    _putenv_s("TSFP_MAPMAKER_OVERLAP",overlap?overlap:"");
    _putenv_s("TSFP_MAPMAKER_STACK_ITEMS",stack?stack:"");
    _putenv_s("TSFP_MAPMAKER_POOLS","");
#else
    if(limits) setenv("TSFP_MAPMAKER_LIMITS",limits,1); else unsetenv("TSFP_MAPMAKER_LIMITS");
    if(overlap) setenv("TSFP_MAPMAKER_OVERLAP",overlap,1); else unsetenv("TSFP_MAPMAKER_OVERLAP");
    if(stack) setenv("TSFP_MAPMAKER_STACK_ITEMS",stack,1); else unsetenv("TSFP_MAPMAKER_STACK_ITEMS");
    unsetenv("TSFP_MAPMAKER_POOLS");
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    memset(&mapmaker_grid,0,sizeof mapmaker_grid);
}

/* ---- fake originals (stock behaviour; each pops its return address) ---- */
static unsigned orig_calls;
static void stock_cells(uint32_t piece,uint32_t value)
{
    uint32_t desc=WORD_AT(piece+0x2Cu+WORD_AT(piece+0xCu)*4u);
    int32_t w=(int32_t)WORD_AT(desc+0x14u),d=(int32_t)WORD_AT(desc+0x18u),h=(int32_t)WORD_AT(desc+0x1Cu),i,j,l;
    for(l=0;l<h;l++) for(j=0;j<d;j++) for(i=0;i<w;i++) {
        int32_t lv=(int32_t)(((float)l+FLOAT_AT(piece+8u))-(-2.f)),y=(int32_t)((float)j+FLOAT_AT(piece+4u));
        int32_t x=(int32_t)((float)i+FLOAT_AT(piece)),idx=x+40*(y+40*lv);
        if(idx>=0 && idx<0x2580) WORD_AT(WORD_AT(GRID)+(uint32_t)idx*4u)=value;  /* flat bound only */
    }
}
static void orig_alloc(void)
{
    uint32_t p=guest_call(sub_0003DFA0,2,0x9600u,1u);
    memset((char *)memory+p,0,0x9600);WORD_AT(GRID)=p;g_eax=0;orig_calls++;g_esp+=4u;
}
static void orig_reset(void) { memset((char *)memory+WORD_AT(GRID),0,0x9600);orig_calls++;g_esp+=4u; }
static void orig_insert(void) { stock_cells(WORD_AT(g_esp+4u),WORD_AT(g_esp+4u));orig_calls++;g_esp+=4u; }
static void orig_remove(void) { stock_cells(WORD_AT(g_esp+4u),0);orig_calls++;g_esp+=4u; }
static void orig_lookup(void)
{
    int32_t x=(int32_t)FLOAT_AT(g_esp+4u),y=(int32_t)FLOAT_AT(g_esp+8u),z=(int32_t)FLOAT_AT(g_esp+12u)+2;
    g_eax=(x<0 || x>=40 || y<0 || y>=40 || z<0 || z>=6)?0u:WORD_AT(WORD_AT(GRID)+(uint32_t)(x+40*(y+40*z))*4u);
    orig_calls++;g_esp+=4u;
}
static uint32_t sound_arg;
static void orig_sound(void) { sound_arg=WORD_AT(g_esp+4u);orig_calls++;g_esp+=4u; }
static void orig_bar(void) { orig_calls++;g_esp+=4u; }
static uint32_t placement_result, probe_result, cell_extra_refusal, valid_result;
static void orig_placement(void) { g_eax=placement_result;orig_calls++;g_esp+=4u; }
static void orig_probe(void) { g_eax=probe_result;WORD_AT(ED+0x2F80u)=0x77u;orig_calls++;g_esp+=4u; }
static void orig_find(void)  /* first item of the piece with the cell */
{
    uint32_t item=WORD_AT(WORD_AT(g_esp+4u)+0x3Cu);
    uint16_t x=U16_AT(g_esp+8u),y=U16_AT(g_esp+10u),z=U16_AT(g_esp+12u);
    for(;item;item=WORD_AT(item+0x14u))
        if(U16_AT(item+0xCu)==x && U16_AT(item+0xEu)==y && U16_AT(item+0x10u)==z) break;
    g_eax=item;orig_calls++;g_esp+=4u;
}
static void orig_valid(void) { g_eax=(g_eax&~0xFFu)|(valid_result&0xFFu);orig_calls++;g_esp+=4u; }
/* Story AI array as editor.json relocates it: editor+0x11950, 0xE4 each,
 * point count +0x58, points {piece, x, y, z} x 0x10 from +0x64. */
#define SAI(s) (ED+0x11950u+(uint32_t)(s)*0xE4u)
static void orig_cell(void)  /* refusals 3.1.4 and 3.1.5 (0x0033482A..0x00334893), plus an injected other refusal */
{
    uint32_t piece=WORD_AT(g_esp+4u),item=WORD_AT(piece+0x3Cu),s,k;
    uint16_t x=U16_AT(g_esp+8u),y=U16_AT(g_esp+10u),z=U16_AT(g_esp+12u);
    g_eax=(g_eax&~0xFFu)|(cell_extra_refusal?0u:1u);
    for(;item;item=WORD_AT(item+0x14u))
        if(U16_AT(item+0xCu)==x && U16_AT(item+0xEu)==y && U16_AT(item+0x10u)==z) g_eax&=~0xFFu;
    for(s=0;(int32_t)s<(int32_t)WORD_AT(ED+0x280u);s++)
        for(k=0;(int32_t)k<(int32_t)WORD_AT(SAI(s)+0x58u);k++) {
            uint32_t pt=SAI(s)+0x64u+k*0x10u;
            if(WORD_AT(pt)==piece && U16_AT(pt+4u)==x && U16_AT(pt+6u)==y && U16_AT(pt+8u)==z) g_eax&=~0xFFu;
        }
    orig_calls++;g_esp+=4u;
}
static void orig_sai_point(void)  /* sub_002F8B40: first (sai << 16) | point in the cell, or -1 */
{
    uint32_t piece=WORD_AT(g_esp+4u),s,k;
    uint16_t x=U16_AT(g_esp+8u),y=U16_AT(g_esp+10u),z=U16_AT(g_esp+12u);
    g_eax=0xFFFFFFFFu;
    for(s=0;(int32_t)s<(int32_t)WORD_AT(ED+0x280u) && g_eax==0xFFFFFFFFu;s++)
        for(k=0;(int32_t)k<(int32_t)WORD_AT(SAI(s)+0x58u);k++) {
            uint32_t pt=SAI(s)+0x64u+k*0x10u;
            if(WORD_AT(pt)==piece && U16_AT(pt+4u)==x && U16_AT(pt+6u)==y && U16_AT(pt+8u)==z) { g_eax=(s<<16)|k;break; }
        }
    orig_calls++;g_esp+=4u;
}

static void call(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,int n,const uint32_t *args,int line)
{
    int i;
    g_esp=ESP0;WORD_AT(ESP0)=ret;
    for(i=0;i<n;i++) WORD_AT(ESP0+4u+4u*(uint32_t)i)=args[i];
    orig_calls=0;
    hook(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d after hook\n",__FILE__,line,(int)(g_esp-ESP0));esp_errors++; }
}
#define CALL(hook,orig,ret,...) do { const uint32_t a_[]={0,__VA_ARGS__}; call(hook,orig,ret,(int)(sizeof a_/4)-1,a_+1,__LINE__); } while(0)
static uint32_t fbits(float f) { uint32_t u;memcpy(&u,&f,4);return u; }
static uint32_t lookup(float x,float y,float z)
{
    CALL(pc_mapmaker_grid_lookup,orig_lookup,0x00300000u,fbits(x),fbits(y),fbits(z));
    return g_eax;
}

/* Piece nodes 0x48 B at 0x00200000 + k*0x100, footprint desc at +0x80. */
static uint32_t piece(unsigned k,float x,float y,float z,int w,int d,int h)
{
    uint32_t p=0x00200000u+k*0x100u,desc=p+0x80u;
    memset((char *)memory+p,0,0x100);
    FLOAT_AT(p)=x;FLOAT_AT(p+4u)=y;FLOAT_AT(p+8u)=z;WORD_AT(p+0xCu)=0;
    WORD_AT(p+0x2Cu)=desc;WORD_AT(desc+0x14u)=(uint32_t)w;WORD_AT(desc+0x18u)=(uint32_t)d;WORD_AT(desc+0x1Cu)=(uint32_t)h;
    return p;
}
static void place(uint32_t *list,unsigned n)
{
    unsigned i;
    WORD_AT(PLACED)=n?list[0]:0u;
    for(i=0;i<n;i++) WORD_AT(list[i]+0x40u)=i+1<n?list[i+1]:0u;
}

static void reset_world(void)
{
    memset(memory,0,sizeof memory);
    heap_next=0x00500000u;allocs=0;last_alloc=0;
    FLOAT_AT(0x004760CCu)=5.f;FLOAT_AT(0x004788B8u)=40.f;FLOAT_AT(0x004760ACu)=60.f;
    WORD_AT(0x00478394u)=0x3C888889u;               /* 1/60 as in the XBE */
    WORD_AT(0x007844A8u)=ED;WORD_AT(SCENE)=102u;
    /* catalogue categories: id 5 weapon (8), 6 teleporter (14), 7 cover prop (33), 8 start (0),
     * 9 ammo (9), 10 flag (4), 11 turret (27), 12 teleporter (18) */
    U16_AT(0x00524F5Au+5u*0xC4u)=8;U16_AT(0x00524F5Au+6u*0xC4u)=14;
    U16_AT(0x00524F5Au+7u*0xC4u)=33;U16_AT(0x00524F5Au+8u*0xC4u)=0;
    U16_AT(0x00524F5Au+9u*0xC4u)=9;U16_AT(0x00524F5Au+10u*0xC4u)=4;
    U16_AT(0x00524F5Au+11u*0xC4u)=27;U16_AT(0x00524F5Au+12u*0xC4u)=18;
    U16_AT(ED+0x2F10u)=0xFFFFu;                      /* no story AI held */
}

static void test_stock(void)
{
    uint32_t a,b,list[2];
    set_env(NULL,NULL,NULL);reset_world();
    CALL(pc_mapmaker_grid_alloc,orig_alloc,0x00303029u,0);
    CHECK(orig_calls==1 && last_alloc==0x9600u && WORD_AT(GRID)==0x00500000u);
    /* settings block: the image constants, bit for bit */
    CHECK(WORD_AT(MAPMAKER_GRID_CFG_FLOORS)==fbits(5.f) && WORD_AT(MAPMAKER_GRID_CFG_EXTENT)==fbits(40.f));
    CHECK(WORD_AT(MAPMAKER_GRID_CFG_ZOOM)==fbits(60.f) && WORD_AT(MAPMAKER_GRID_CFG_ZOOM_INV)==0x3C888889u);
    a=piece(0,10,10,0,3,3,1);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,a);CHECK(orig_calls==1);
    CHECK(lookup(11,11,0)==a && orig_calls==1);
    b=piece(1,11,11,0,3,3,1);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,b);
    list[0]=a;place(list,1);                          /* b unlinked, as sub_00308480 does */
    CALL(pc_mapmaker_grid_remove,orig_remove,0x0030851Au,b);
    CHECK(orig_calls==1 && lookup(11,11,0)==0);       /* stock: the shared cells read as free */
    /* floors: stock refusal at floor 2 stays */
    WORD_AT(ED+0x264u)=2;WORD_AT(ED+0x268u)=1;
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(orig_calls==1 && sound_arg==0x1576u && WORD_AT(ED+0x264u)==2u && WORD_AT(ED+0x268u)==1u);
    /* menu: 7 bars drawn by the patched loop, floors 3 and 4 off screen */
    g_ebx=3;FLOAT_AT(ESP0+4u+0x18u)=123.f;
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x0030BFB5u,0);
    CHECK(orig_calls==1 && FLOAT_AT(g_esp+0x18u)==MAPMAKER_GRID_HIDDEN_Y);
    g_ebx=4;FLOAT_AT(ESP0+4u+0x18u)=123.f;
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x0030BFB5u,0);
    CHECK(FLOAT_AT(g_esp+0x18u)==MAPMAKER_GRID_HIDDEN_Y);
    g_ebx=2;FLOAT_AT(ESP0+4u+0x18u)=123.f;
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x0030BFB5u,0);
    CHECK(FLOAT_AT(g_esp+0x18u)==123.f);
    g_ebx=(uint32_t)-2;FLOAT_AT(ESP0+4u+0x18u)=123.f;
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x0030BFB5u,0);
    CHECK(FLOAT_AT(g_esp+0x18u)==123.f);
    g_ebx=3;FLOAT_AT(ESP0+4u+0x18u)=123.f;              /* another caller of the sprite select */
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x00012345u,0);
    CHECK(orig_calls==1 && FLOAT_AT(g_esp+0x18u)==123.f);
    /* placement: no upper guard in stock mode */
    WORD_AT(ED+0x264u)=2;a=piece(2,0,0,1,1,1,2);WORD_AT(ED+0x2F7Cu)=a;placement_result=1;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(orig_calls==1 && g_eax==1u);
    /* no overlap, no stacking */
    probe_result=1;CALL(pc_mapmaker_overlap_cell,orig_probe,MAPMAKER_GRID_RET_PROBE,0,0,0);
    CHECK(g_eax==1u && WORD_AT(ED+0x2F80u)==0x77u);
}

static void test_wide(void)
{
    uint32_t a,b,c,r,q,list[4],g;
    set_env("1",NULL,NULL);reset_world();
    CALL(pc_mapmaker_grid_alloc,orig_alloc,0x00303029u,0);
    CHECK(orig_calls==0 && allocs==1 && last_alloc==80u*80u*8u*4u && WORD_AT(GRID)==0x00500000u);
    g=WORD_AT(GRID);
    CHECK(WORD_AT(g)==0 && WORD_AT(g+80u*80u*8u*4u-4u)==0);   /* cleared */
    CHECK(FLOAT_AT(MAPMAKER_GRID_CFG_FLOORS)==7.f && FLOAT_AT(MAPMAKER_GRID_CFG_EXTENT)==80.f);
    CHECK(FLOAT_AT(MAPMAKER_GRID_CFG_ZOOM)==100.f && fabsf(FLOAT_AT(MAPMAKER_GRID_CFG_ZOOM_INV)-.01f)<1e-7f);
    /* far corner, top levels */
    a=piece(0,78,79,4,2,1,2);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,a);
    CHECK(orig_calls==0);
    CHECK(lookup(78,79,4)==a && lookup(79,79,5)==a && lookup(79,79,6)==0 && lookup(80,79,4)==0 && lookup(79,80,4)==0);
    CHECK(lookup(1,1,-2.5f)==0);
    CHECK(lookup(NAN,1,0)==0 && lookup(1,1,NAN)==0);
    /* a piece past the edge does not wrap into the next row */
    b=piece(1,79,10,0,2,1,1);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,b);
    CHECK(lookup(79,10,0)==b && lookup(0,11,0)==0);
    /* stock layout is not used: cell (0,11) in 40-wide indexing is free too */
    CHECK(WORD_AT(g+(0u+80u*(11u+80u*2u))*4u)==0);
    CALL(pc_mapmaker_grid_remove,orig_remove,0x0030851Au,a);
    CHECK(orig_calls==0 && lookup(78,79,4)==0 && lookup(79,79,5)==0);
    /* reset clears the whole wide grid */
    WORD_AT(g+(79u+80u*(79u+80u*7u))*4u)=0x1234u;
    CALL(pc_mapmaker_grid_reset,orig_reset,0x002FEC40u,0);
    CHECK(orig_calls==1 && WORD_AT(g+(79u+80u*(79u+80u*7u))*4u)==0);
    /* removal keeps overlapping pieces, last placed wins, only inside the box */
    a=piece(0,10,10,0,3,3,1);b=piece(1,12,12,0,3,3,1);r=piece(2,11,11,0,3,3,1);q=piece(3,30,30,0,1,1,1);
    list[0]=a;list[1]=b;list[2]=r;list[3]=q;place(list,4);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,a);CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,b);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,r);CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,q);
    c=0xBEEF0u;WORD_AT(g+(10u+80u*(10u+80u*2u))*4u)=c;  /* a's cell outside r's box, owned by someone else */
    list[2]=q;place(list,3);                          /* r unlinked */
    CALL(pc_mapmaker_grid_remove,orig_remove,0x0030851Au,r);
    CHECK(lookup(11,11,0)==a && lookup(12,12,0)==b && lookup(13,13,0)==b && lookup(12,11,0)==a);
    CHECK(lookup(13,11,0)==0 && lookup(11,13,0)==0 && lookup(10,10,0)==c && lookup(30,30,0)==q);
    CHECK(lookup(14,14,0)==b);
    /* defensive: a removed piece still on the list never comes back */
    list[2]=r;list[3]=q;place(list,4);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,r);
    CALL(pc_mapmaker_grid_remove,orig_remove,0x0030851Au,r);
    CHECK(lookup(13,11,0)==0 && lookup(12,12,0)==b && lookup(11,11,0)==a);
    /* floors: up to 4 */
    WORD_AT(ED+0x264u)=2;WORD_AT(ED+0x268u)=1;
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(orig_calls==1 && sound_arg==0x157Du && WORD_AT(ED+0x264u)==3u && WORD_AT(ED+0x268u)==2u);
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(sound_arg==0x157Du && WORD_AT(ED+0x264u)==4u && WORD_AT(ED+0x268u)==3u);
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(sound_arg==0x1576u && WORD_AT(ED+0x264u)==4u);
    WORD_AT(ED+0x264u)=(uint32_t)-2;WORD_AT(ED+0x268u)=(uint32_t)-2;  /* floor down refused at -2 */
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(sound_arg==0x1576u && WORD_AT(ED+0x264u)==(uint32_t)-2);
    WORD_AT(ED+0x264u)=2;
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00301234u,0x1576u);   /* another refusal */
    CHECK(sound_arg==0x1576u && WORD_AT(ED+0x264u)==2u);
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x157Du);
    CHECK(sound_arg==0x157Du && WORD_AT(ED+0x264u)==2u);
    /* the 7-bar menu shows every bar */
    g_ebx=4;FLOAT_AT(ESP0+4u+0x18u)=123.f;
    CALL(pc_mapmaker_grid_floor_bar,orig_bar,0x0030BFB5u,0);
    CHECK(FLOAT_AT(g_esp+0x18u)==123.f);
    /* placement guard: top level 5 */
    placement_result=1;
    WORD_AT(ED+0x264u)=4;a=piece(0,0,0,0,1,1,2);WORD_AT(a+0x40u)=0;WORD_AT(ED+0x2F7Cu)=a;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(orig_calls==1 && g_eax==1u);                /* floors 4..5 */
    WORD_AT(a+0x80u+0x1Cu)=3;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==0u);                                 /* 4..6 */
    b=piece(1,0,0,1,1,1,1);WORD_AT(a+0x80u+0x1Cu)=1;WORD_AT(a+0x40u)=b;WORD_AT(ED+0x264u)=3;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==1u);                                 /* 3 and 4 */
    FLOAT_AT(b+8u)=3.f;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==0u);                                 /* second held piece at 6 */
    WORD_AT(SCENE)=100u;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==1u);
    WORD_AT(SCENE)=102u;placement_result=0;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==0u);
    /* the limits alone do not allow overlap */
    probe_result=1;CALL(pc_mapmaker_overlap_cell,orig_probe,MAPMAKER_GRID_RET_PROBE,0,0,0);
    CHECK(g_eax==1u);
}

static void test_overlap(void)
{
    uint32_t a,b,list[2];
    set_env(NULL,"1",NULL);reset_world();
    CALL(pc_mapmaker_grid_alloc,orig_alloc,0x00303029u,0);
    CHECK(orig_calls==1 && last_alloc==0x9600u);      /* stock grid */
    probe_result=1;
    CALL(pc_mapmaker_overlap_cell,orig_probe,MAPMAKER_GRID_RET_PROBE,0,0,0);
    CHECK(orig_calls==1 && g_eax==0u && WORD_AT(ED+0x2F80u)==0x77u);
    CALL(pc_mapmaker_overlap_cell,orig_probe,0x002FC700u,0,0,0);      /* connection builder */
    CHECK(g_eax==1u);
    WORD_AT(SCENE)=100u;
    CALL(pc_mapmaker_overlap_cell,orig_probe,MAPMAKER_GRID_RET_PROBE,0,0,0);
    CHECK(g_eax==1u);
    WORD_AT(SCENE)=102u;probe_result=0;
    CALL(pc_mapmaker_overlap_cell,orig_probe,MAPMAKER_GRID_RET_PROBE,0,0,0);
    CHECK(g_eax==0u);
    /* stock 6 levels: z 3 is the top */
    placement_result=1;WORD_AT(ED+0x264u)=2;a=piece(0,0,0,0,1,1,2);WORD_AT(ED+0x2F7Cu)=a;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==1u);
    FLOAT_AT(a+8u)=1.f;
    CALL(pc_mapmaker_grid_placement,orig_placement,0x00306EC6u,0);
    CHECK(g_eax==0u);
    /* no extra floors without the limits */
    WORD_AT(ED+0x264u)=2;
    CALL(pc_mapmaker_grid_floor_sound,orig_sound,0x00306E99u,0x1576u);
    CHECK(sound_arg==0x1576u && WORD_AT(ED+0x264u)==2u);
    /* removal in the stock layout keeps the other piece */
    a=piece(0,10,10,0,3,3,1);b=piece(1,11,11,0,3,3,1);
    CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,a);CALL(pc_mapmaker_grid_insert,orig_insert,0x00307FAAu,b);
    CHECK(orig_calls==1 && lookup(11,11,0)==b);
    list[0]=a;place(list,1);
    CALL(pc_mapmaker_grid_remove,orig_remove,0x0030851Au,b);
    CHECK(lookup(11,11,0)==a && lookup(12,12,0)==a && lookup(13,13,0)==0 && lookup(10,10,0)==a);
}

static void item(uint32_t addr,uint32_t id,uint16_t x,uint16_t y,uint16_t z,uint32_t next)
{
    WORD_AT(addr)=id;U16_AT(addr+0xCu)=x;U16_AT(addr+0xEu)=y;U16_AT(addr+0x10u)=z;WORD_AT(addr+0x14u)=next;
}
static void test_stack(void)
{
    uint32_t p=0x00210000u,i1=0x00211000u,i2=0x00211100u,i3=0x00211200u,i4=0x00211300u,held=0x00212000u;
    set_env(NULL,NULL,"1");reset_world();
    WORD_AT(p+0x3Cu)=i1;item(i1,5,1,2,0,i2);item(i2,9,1,2,0,i3);item(i3,9,3,3,0,i4);item(i4,8,5,5,0,0);
    /* sub_00303460 frame: [ebp+0x14] = held item node */
    g_ebp=0x00610000u;WORD_AT(g_ebp+0x14u)=held;WORD_AT(held)=5;
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==0u && mapmaker_grid.stacked==1u);
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_COUNT,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==0u && mapmaker_grid.stacked==1u);
    /* an empty cell is an ordinary placement: not counted as a stack */
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,4u|(4u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==0u && mapmaker_grid.stacked==1u);
    CALL(pc_mapmaker_stack_find,orig_find,0x002F831Bu,p,1u|(2u<<16),0u,0xFFFFFFFFu);      /* pick-up */
    CHECK(orig_calls==1 && g_eax==i1);
    WORD_AT(held)=6;                                                                          /* teleporter */
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==0u && mapmaker_grid.stacked==2u);
    WORD_AT(held)=5;                                                                          /* a start in the cell stays */
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,5u|(5u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==i4 && mapmaker_grid.stacked==2u);
    WORD_AT(held)=8;                                                                          /* start point */
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==i1);
    WORD_AT(held)=7;WORD_AT(SCENE)=100u;
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==i1);
    WORD_AT(SCENE)=102u;
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==0u);                                                        /* cover prop */
    /* validity: the occupied cell is allowed for an allowed held item */
    WORD_AT(ED+0x2F34u)=5;cell_extra_refusal=0;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==1u);
    CHECK(U16_AT(i1+0xCu)==1u && U16_AT(i2+0xCu)==1u && U16_AT(i3+0xCu)==3u);              /* restored */
    cell_extra_refusal=1;                                                                     /* refused anyway */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00304D10u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u && U16_AT(i1+0xCu)==1u && U16_AT(i2+0xCu)==1u);
    cell_extra_refusal=0;WORD_AT(ED+0x2F34u)=6;                                               /* teleporter */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==1u && U16_AT(i1+0xCu)==1u && U16_AT(i2+0xCu)==1u);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);                   /* onto a start */
    CHECK((g_eax&0xFFu)==0u && U16_AT(i4+0xCu)==5u);
    WORD_AT(ED+0x2F34u)=5;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);
    CHECK((g_eax&0xFFu)==0u && U16_AT(i4+0xCu)==5u);
    WORD_AT(ED+0x2F34u)=8;                                                                    /* start held */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(ED+0x2F34u)=0xFFFFFFFFu;                                                          /* nothing held */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(ED+0x2F34u)=5;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,3u|(3u<<16),0u);
    CHECK((g_eax&0xFFu)==1u && U16_AT(i3+0xCu)==3u);
    WORD_AT(SCENE)=100u;                                                                      /* not the editor */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(SCENE)=102u;
    /* per-frame valid bit: sub_003348C0 passed but an item is under the cursor */
    WORD_AT(ED+0x2F34u)=5;WORD_AT(ED+0x2F1Cu)=i1;WORD_AT(ED+0x25Cu)=0x10u;valid_result=1;
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==1u && WORD_AT(ED+0x25Cu)==0x14u);
    WORD_AT(ED+0x25Cu)=0x10u;valid_result=0;                                                  /* refused cell */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==0u && WORD_AT(ED+0x25Cu)==0x10u);
    valid_result=1;WORD_AT(ED+0x2F1Cu)=0;                                                     /* stock sets it */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(WORD_AT(ED+0x25Cu)==0x10u);
    WORD_AT(ED+0x2F1Cu)=i1;                                                                   /* no item held */
    CALL(pc_mapmaker_stack_valid,orig_valid,0x0030707Cu,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && WORD_AT(ED+0x25Cu)==0x10u);
    WORD_AT(ED+0x2F34u)=6;                                                                    /* teleporter */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(WORD_AT(ED+0x25Cu)==0x14u);
    WORD_AT(ED+0x25Cu)=0x10u;WORD_AT(ED+0x2F34u)=8;                                                                  /* start point */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(WORD_AT(ED+0x25Cu)==0x10u);
    WORD_AT(ED+0x2F34u)=7;WORD_AT(SCENE)=100u;                                                /* not the editor */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(WORD_AT(ED+0x25Cu)==0x10u);
    WORD_AT(SCENE)=102u;                                                                      /* cover prop */
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(WORD_AT(ED+0x25Cu)==0x14u);
    /* switch off: stock */
    set_env(NULL,NULL,NULL);
    WORD_AT(ED+0x25Cu)=0x10u;WORD_AT(ED+0x2F34u)=5;
    CALL(pc_mapmaker_stack_valid,orig_valid,MAPMAKER_STACK_RET_VALID,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==1u && WORD_AT(ED+0x25Cu)==0x10u);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(held)=5;
    CALL(pc_mapmaker_stack_find,orig_find,MAPMAKER_STACK_RET_REPLACE,p,1u|(2u<<16),0u,0xFFFFFFFFu);
    CHECK(orig_calls==1 && g_eax==i1);
}

static void point(unsigned s,unsigned k,uint32_t piece,uint16_t x,uint16_t y,uint16_t z)
{
    uint32_t pt=SAI(s)+0x64u+k*0x10u;
    WORD_AT(pt)=piece;U16_AT(pt+4u)=x;U16_AT(pt+6u)=y;U16_AT(pt+8u)=z;
    if(WORD_AT(SAI(s)+0x58u)<k+1u) WORD_AT(SAI(s)+0x58u)=k+1u;
}
static void hold_sai(int s) { WORD_AT(ED+0x2F34u)=0xFFFFFFFFu;U16_AT(ED+0x2F10u)=(uint16_t)(int16_t)s; }
static void test_stack_sai(void)
{
    uint32_t p=0x00210000u,q=0x00210100u,i1=0x00211000u,i2=0x00211100u,i3=0x00211200u;
    set_env(NULL,NULL,"1");reset_world();
    /* cell (1,2,0): weapon + teleporter; (3,3,0): start; story AI 0: (1,2,0),(5,5,0);
     * 1: (1,2,0); 2: (3,3,0) and one point on another piece at (1,2,0) */
    WORD_AT(p+0x3Cu)=i1;item(i1,5,1,2,0,i2);item(i2,12,1,2,0,i3);item(i3,8,3,3,0,0);
    WORD_AT(ED+0x280u)=3;
    point(0,0,p,1,2,0);point(0,1,p,5,5,0);point(1,0,p,1,2,0);point(2,0,p,3,3,0);point(2,1,q,1,2,0);
    /* a held story AI may join items and other story AI ... */
    hold_sai(2);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==1u);
    CHECK(U16_AT(i1+0xCu)==1u && U16_AT(i2+0xCu)==1u && U16_AT(SAI(0)+0x68u)==1u && U16_AT(SAI(1)+0x68u)==1u);
    CHECK(U16_AT(SAI(2)+0x78u)==1u);
    /* ... but not its own points, nor a start */
    hold_sai(0);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u && U16_AT(SAI(0)+0x68u)==1u && U16_AT(SAI(1)+0x68u)==1u && U16_AT(i1+0xCu)==1u);
    hold_sai(1);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,3u|(3u<<16),0u);
    CHECK((g_eax&0xFFu)==0u && U16_AT(i3+0xCu)==3u && U16_AT(SAI(2)+0x68u)==3u);
    /* story AI points past the 8 an entry holds are left to the original */
    WORD_AT(SAI(1)+0x58u)=9;point(1,8,p,7,7,0);hold_sai(2);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,7u|(7u<<16),0u);
    CHECK((g_eax&0xFFu)==0u && U16_AT(SAI(1)+0x68u+0x80u)==7u);
    WORD_AT(SAI(1)+0x58u)=1;
    /* items held: on story AI points, still not on a start */
    WORD_AT(ED+0x2F34u)=5;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);
    CHECK((g_eax&0xFFu)==1u && U16_AT(SAI(0)+0x78u)==5u);
    WORD_AT(ED+0x2F34u)=6;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==1u && U16_AT(SAI(0)+0x68u)==1u);
    WORD_AT(ED+0x2F34u)=10;                                                                   /* flag */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(ED+0x2F34u)=11;                                                                   /* turret */
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    /* nothing held, wrong scene, switch off: stock */
    hold_sai(-1);
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,5u|(5u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    hold_sai(2);WORD_AT(SCENE)=100u;
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK((g_eax&0xFFu)==0u);
    WORD_AT(SCENE)=102u;
    /* point under the cursor (sub_002F8B40): none while a story AI is held */
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && g_eax==0xFFFFFFFFu && mapmaker_grid.stack_sai==1u);
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,9u|(9u<<16),0u);
    CHECK(orig_calls==1 && g_eax==0xFFFFFFFFu && mapmaker_grid.stack_sai==1u);              /* empty cell: not counted */
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,0x00300000u,p,5u|(5u<<16),0u);         /* other caller */
    CHECK(g_eax==1u);
    hold_sai(-1);                                                                             /* empty hands: pick-up */
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,5u|(5u<<16),0u);
    CHECK(g_eax==1u);
    WORD_AT(ED+0x2F34u)=5;                                                                    /* an item held */
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,5u|(5u<<16),0u);
    CHECK(g_eax==1u);
    hold_sai(2);WORD_AT(SCENE)=100u;
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,3u|(3u<<16),0u);
    CHECK(g_eax==(2u<<16));
    WORD_AT(SCENE)=102u;
    set_env(NULL,NULL,NULL);
    CALL(pc_mapmaker_stack_sai_point,orig_sai_point,MAPMAKER_STACK_RET_SAI_POINT,p,3u|(3u<<16),0u);
    CHECK(orig_calls==1 && g_eax==(2u<<16));
    CALL(pc_mapmaker_stack_cell,orig_cell,0x00334937u,p,1u|(2u<<16),0u);
    CHECK(orig_calls==1 && (g_eax&0xFFu)==0u);
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    test_stock();
    test_wide();
    test_overlap();
    test_stack();
    test_stack_sai();
    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    printf("mapmaker grid ok\n");
    return 0;
}
