/* Asset-free contract for src/interface/pc_mapmaker_realbosses.inc: the
 * "Giant Trophy Monster" picker row (row 150 -> id 0x15) and its names in the
 * editor, the lv1_3.pak mount at a MapMaker level start, the class 0x80
 * override at AI start, the boss bar label and the single-bar rule with the
 * Boss flag. Fake guest memory; fake originals pop their return address like
 * the lifted callees, and every hook call checks esp. */
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

#define HEAP 0x00300000u
#define ITEM 0x00340000u
#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define ESP0 0x00600000u
#define R 10u
#define SKST 0x00700100u                /* fake skeleton state (0x4C7C) */
#define SKPT 0x00710000u                /* fake particle block (0x7D00) */
#define WKST 0x00720000u                /* fake Crow walker state (0x64DC) */
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_00079D60(void) { g_ecx=2; }
void sub_00071630(void) { g_ecx=3; }
void sub_00071C70(void) { g_ecx=4; }
void sub_0005A130(void) { g_ecx=5; }
void sub_00062260(void) { g_ecx=6; }
void sub_00287E00(void) { g_ecx=7; }
void sub_00059DB0(void) { g_ecx=8; }
void sub_000EA9C0(void) { g_ecx=9; }
void sub_0010EB90(void) { g_ecx=10; }
void sub_00139740(void) { g_ecx=11; }
void sub_002E4B80(void) { g_ecx=12; }
void sub_002976F0(void) { g_ecx=13; }
void sub_0015DDA0(void) { g_ecx=14; }
void sub_0029CCB0(void) { g_ecx=15; }
static unsigned nprincess;
static uint32_t princess_ent;
void sub_0029CEF0(void) { nprincess++;princess_ent=WORD_AT(g_esp+4u);g_esp+=4u; }   /* pops like the lifted callee */
static unsigned nmansion_msg;
static uint32_t mansion_msg[2];
void sub_00286D30(void)                       /* mansion message handler: death = state 6, -1 */
{
    nmansion_msg++;mansion_msg[0]=WORD_AT(g_esp+4u);mansion_msg[1]=WORD_AT(g_esp+8u);
    g_eax=mansion_msg[1]==2u?0xFFFFFFFFu:0u;g_esp+=4u;
}
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }
static int bosses_area=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)hook;return strcmp(area,"bosses") || bosses_area; }

static unsigned nrows, nenable, nmount;
static struct { uint32_t text, id, flags, cb; char label[32]; } rows[8];
static uint32_t enable_item, enable_flag=0xEEu, mount_arg;
static int pak_mounted, skel_mounted, walk_mounted, mount_ok=1, alloc_ok=1, player_dead;
static unsigned nwalk_alloc, nremove, nfx;
static uint32_t remove_arg, fx_scene;
static unsigned nalloc, nstart, nshot, neffect, nbar;
static uint32_t start_args[2], shot_args[12], effect_args[2], bar_args[3], bar_esi, bar_edi, alloc_scene;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00079D60) {
        if(nrows<8) {
            rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].flags=x[2];rows[nrows].cb=x[4];
            memcpy(rows[nrows].label,(const char *)((uintptr_t)x[0]+g_xbox_mem_offset),31);rows[nrows].label[31]=0;
        }
        nrows++;return ITEM;
    }
    if(f==sub_00071630) return ITEM;
    if(f==sub_00071C70) { enable_flag=(nenable++?enable_flag:0u)|x[1];enable_item=x[0];return 0; }   /* any row disabled shows */
    if(f==sub_00062260) return ((x[0]==0x0049D77Cu && pak_mounted) || (x[0]==0x0049D6FCu && skel_mounted) ||
                                (x[0]==0x0049D9A8u && walk_mounted))?3u:0xFFFFFFFFu;
    if(f==sub_0005A130) {
        nmount++;mount_arg=x[0];
        if(mount_ok) { if(x[0]==0x0049D6FCu) skel_mounted=1; else if(x[0]==0x0049D9A8u) walk_mounted=1; else pak_mounted=1; }
        return mount_ok?1u:0u;
    }
    if(f==sub_00287E00) {   /* sub_0003DFA0 twice, then sub_00287860 */
        nalloc++;alloc_scene=WORD_AT(0x0079094Cu);
        if(alloc_ok) { WORD_AT(0x0078B1ECu)=SKST;WORD_AT(0x0078B0D0u)=SKPT;WORD_AT(SKST+0x4160u)=0; }
        return 0;
    }
    if(f==sub_00059DB0) { nstart++;start_args[0]=x[0];start_args[1]=x[1];return x[1]==0x19Cu?0x77u:0x66u; }
    if(f==sub_000EA9C0) { nshot++;memcpy(shot_args,x,sizeof shot_args);return 0x1234u; }
    if(f==sub_0010EB90) { neffect++;effect_args[0]=x[0];effect_args[1]=x[1];return 0; }
    if(f==sub_00139740) {
        nbar++;memcpy(bar_args,x,sizeof bar_args);bar_esi=g_esi;bar_edi=g_edi;
        WORD_AT(x[2])=1u;return 0;
    }
    if(f==sub_002E4B80) return (uint32_t)player_dead;
    if(f==sub_002976F0) { nwalk_alloc++;if(alloc_ok) WORD_AT(0x0078B1E8u)=WKST;WORD_AT(0x0078B0D0u)=SKPT;return 0; }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_0029CCB0) { nfx++;fx_scene=WORD_AT(0x0079094Cu);return 0; }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_realbosses.inc"

static int failures, originals;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t orig_eax, seen_class[2], seen_focus;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
static void fake_ai(void) { originals++;seen_class[0]=WORD_AT(REC(0));seen_class[1]=WORD_AT(REC(0)+4u);g_esp+=4u; }
static void fake_ai_any(void)
{
    uint32_t rec=WORD_AT(WORD_AT(g_esp+4u)+0xBF0u);
    originals++;seen_class[0]=WORD_AT(rec);seen_class[1]=WORD_AT(rec+4u);g_esp+=4u;
}
static void fake_hud(void) { originals++;seen_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }
static void fake_pick(void)
{   /* the stock callback stores data read past the Arcade table for row 150 */
    originals++;WORD_AT(0x0075FD90u+2u*0x1Cu)=0x12345u;WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)=150u;
    WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)=7u;g_esp+=4u;
}
static void fake_pick_skel(void)
{
    originals++;WORD_AT(0x0075FD90u+4u*0x1Cu)=0x54321u;WORD_AT(0x0075FD90u+4u*0x1Cu+0x14u)=151u;
    WORD_AT(0x0075FD90u+4u*0x1Cu+0x18u)=9u;g_esp+=4u;
}
static uint32_t seen_scene;
static void fake_scene(void) { originals++;seen_scene=WORD_AT(0x0079094Cu);g_esp+=4u; }
static void fake_highlight(void) { originals++;WORD_AT(0x0078AEA8u)=WORD_AT(0x00700000u);g_esp+=4u; }
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
    rbs.read=0;
}
static void arcade_table(void)
{
    unsigned i;
    for(i=0;i<150u;i++) WORD_AT(0x00521A18u+i*0x30u+4u)=0x30u+i%64u;
    WORD_AT(0x00521A18u+43u*0x30u+4u)=0xA7u;                 /* trophy monster row */
    WORD_AT(0x00521A18u+144u*0x30u+4u)=0xA6u;                /* skeleton cowboy row */
    WORD_AT(0x00521A18u+90u*0x30u+4u)=0xA8u;                 /* giant robot row */
    for(i=0;i<43u;i++) if(WORD_AT(0x00521A18u+i*0x30u+4u)==0xA7u) WORD_AT(0x00521A18u+i*0x30u+4u)=0x20u;
}
/* map container: map-info chunk (custom set), then Story AI chunks */
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

/* One level with a set: anims, level init, then a skeleton (actor 6) and a
 * trophy monster (actor 7) start. expect: 3 both real bosses, 2 skeleton
 * only, 1 trophy only, 0 neither. */
static void level_with(uint32_t index,const uint16_t *ids,unsigned n,const uint16_t *extra,unsigned ne,int expect,int line)
{
    static const uint8_t none[]={0,0};
    unsigned m=nmount,al=nalloc;
    int fails=failures;
    container(index,ids,n,none,2);
    if(extra) container_extra(extra,ne);
    pak_mounted=skel_mounted=0;WORD_AT(0x006D7880u)=10u;
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    call(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1,line);
    call(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1,line);
    actor(6,0x28,0x5E9,0);actor(7,0x15,0x5BE,1);
    call(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(6),0,0,1,line);
    CHECK(seen_class[1]==((expect&2)?0x20000u:0u));
    call(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(7),0,0,1,line);
    CHECK(seen_class[1]==((expect&1)?0x80u:0u));
    CHECK(skel_mounted==!!(expect&2) && pak_mounted==!!(expect&1));
    CHECK(nmount-m==(unsigned)(!!(expect&2)+!!(expect&1)));
    CHECK(nalloc-al==(unsigned)!!(expect&2) && (rbs_skel.state==SKST)==!!(expect&2));
    CHECK(((expect&2)?rbs_skel.record==REC(6):!rbs_skel.record));
    m=nmansion_msg;                                       /* its death reaches the mansion handler */
    call(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(6),2u,0,(expect&2)?0:1,line);
    CHECK(nmansion_msg-m==(unsigned)!!(expect&2));
    if(failures!=fails) fprintf(stderr,"  (level_with from line %d)\n",line);
}
#define LEVEL_WITH(i,ids,n,ex,ne,e) level_with(i,ids,n,ex,ne,e,__LINE__)

int main(void)
{
    static const uint16_t with[]={0x0C,0x15,0xA8}, without[]={0x0C,0xA8};
    static const uint8_t boss_on[]={0,0xB1}, boss_off[]={0,0};
    unsigned k;
    g_xbox_mem_offset=(uintptr_t)memory;
    arcade_table();
    WORD_AT(0x00783A50u)=R;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    orig_eax=0xFFFFFFFFu;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x15u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,150u,0,0,1)==0);
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,1)==0x1234u);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    container(0x11,with,3,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    actor(0,0x15,0x5BE,1);pak_mounted=1;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* off: overhead meter drawn */
    pak_mounted=0;
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- editor, on ----------------------------------------------------- */
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1F9Fu,0,0,0,1);   /* another list */
    CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=101u;                                          /* front-end bot sets */
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_picker_end,fake,0x002C1FA5u,0,0,0,1);
    CHECK(nrows==3 && rows[0].id==150u && rows[0].flags==0x20900u && rows[0].cb==0x002C1C90u);
    CHECK(!strcmp(rows[0].label,"Giant Trophy Monster"));
    CHECK(rows[1].id==151u && rows[1].flags==0x20900u && rows[1].cb==0x002C1C90u && !strcmp(rows[1].label,"Giant Skeleton"));
    CHECK(rows[2].id==152u && rows[2].flags==0x20900u && rows[2].cb==0x002C1C90u && !strcmp(rows[2].label,"Crow Walker"));
    CHECK(nenable==3 && enable_item==ITEM && enable_flag==0);
    /* pick: row 150 -> id 0x15, variant -1; other slots untouched */
    for(k=0;k<10u;k++) { WORD_AT(0x0075FD90u+k*0x1Cu)=0x40u+k;WORD_AT(0x0075FD90u+k*0x1Cu+0x14u)=k;WORD_AT(0x0075FD90u+k*0x1Cu+0x18u)=0; }
    CALL(pc_mapmaker_realbosses_pick,fake_pick,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu)==0x15u && WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)==0xFFFFFFFFu);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)==150u);
    CHECK(WORD_AT(0x0075FD90u+3u*0x1Cu)==0x43u && WORD_AT(0x0075FD90u+3u*0x1Cu+0x18u)==0);
    CHECK(WORD_AT(0x0075FD90u+9u*0x1Cu)==0x49u);
    CALL(pc_mapmaker_realbosses_pick,fake_pick_skel,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0075FD90u+4u*0x1Cu)==0x28u && WORD_AT(0x0075FD90u+4u*0x1Cu+0x18u)==0xFFFFFFFFu);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu)==0x15u);
    /* highlight: the preview row never becomes 150 */
    WORD_AT(0x00700000u)=150u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==43u);
    WORD_AT(0x00700000u)=151u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==144u);
    WORD_AT(0x00700000u)=12u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==12u);
    /* row lookups */
    orig_eax=0xFFFFFFFFu;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x15u,0xFFFFFFFFu,0,0)==150u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x15u,0xFFFFFFFFu,0x003033C0u,0)==150u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x15u,0xFFFFFFFFu,0x000DAEB6u,1)==0xFFFFFFFFu);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x15u,0xFFFFFFFFu,0,0)==43u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1814u,0x15u,0xFFFFFFFFu,0,0)==43u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x15u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x28u,0xFFFFFFFFu,0,0)==151u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x28u,0xFFFFFFFFu,0x003033C0u,0)==151u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x28u,0xFFFFFFFFu,0,0)==144u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x28u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
    orig_eax=7u;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0xA8u,0xFFFFFFFFu,0,1)==7u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x29u,0xFFFFFFFFu,0,1)==7u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x16u,0xFFFFFFFFu,0,1)==7u);
    /* names */
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,150u,0,0,0)==0x52420015u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,151u,0,0,0)==0x52420028u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,152u,0,0,0)==0x52420064u);
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,153u,0,0,1)==0);
    orig_eax=0xE20u;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,149u,0,0,1)==0xE20u);
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,0xFFFFFFFFu,0,0,1)==0);
    orig_eax=0x1234u;
    k=CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0x52420015u,0,0,0);
    CHECK(k==rows[0].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0x52420028u,0,0,0)==rows[1].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0xE20u,0,0,1)==0x1234u);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,1)==0x1234u);   /* not in play */
    CALL(pc_mapmaker_realbosses_model_request,fake,0x000D77AFu,0x5BEu,0,0,1);           /* log only */
    /* editor scene: nothing of play runs */
    container(0x11,with,3,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);

    /* ---- play: anim pak mount -------------------------------------------- */
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=9u;                   /* not story */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    BYTE_AT(0x007DE455u)=10u;
    container(0x11,without,2,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(3,with,3,boss_off,2);                                      /* preset set: stale members */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(0x11,with,3,boss_off,2);
    WORD_AT(0x006D7880u)=29u;                                            /* table nearly full */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    WORD_AT(0x006D7880u)=28u;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==1 && mount_arg==0x0049D77Cu && pak_mounted);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);         /* already mounted */
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x00784014u)=0;                                /* container unreadable */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==4 && skel_mounted && pak_mounted && walk_mounted);     /* all, skeleton first */
    skel_mounted=walk_mounted=0;
    container(0x11,with,3,boss_on,2);

    /* ---- play: class at AI start ----------------------------------------- */
    pak_mounted=0;
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* no real boss noted yet */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);   /* no anims: bot */
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    pak_mounted=1;
    set_env("TSFP_MAPMAKER_REAL_BOSS","0");
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
    actor(0,0x0C,0x572,1);                                               /* another character */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    CHECK(rbs.real[0].record==0);
    BYTE_AT(0x007DE455u)=9u;
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    BYTE_AT(0x007DE455u)=10u;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80u);                     /* before the original */
    CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==1);
    actor(0,0x00,0x5BE,0);                                               /* by model; Story AI 0: no Boss */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(seen_class[0]==0 && seen_class[1]==0x80u);
    CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==0);

    /* ---- play: bar label and a single bar ------------------------------- */
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,0)==rows[0].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CEDAu,0xDCCu,0,0,1)==0x1234u);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCDu,0,0,1)==0x1234u);
    WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+0xFF4u)=ENT(0);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* Boss Off: no bar at all */
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* Boss flag: bosses draws */
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    bosses_area=0;                                                        /* no bosses area: as Boss Off */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(rbs.real[0].boss==0);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==0);
    bosses_area=1;
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    HALF_AT(ENT(0)+0x94u)=0x572u;                                        /* focus is another model */
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(0));
    HALF_AT(ENT(0)+0x94u)=0x5BEu;
    actor(1,0x15,0x5BE,0);WORD_AT(PLAYER+0xFF4u)=ENT(1);                  /* another, unnoted record */
    WORD_AT(A(1)+0xBF0u)=REC(7);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(1));
    WORD_AT(PLAYER+0xFF4u)=ENT(0);

    /* ---- play: no overhead meter on the real boss ------------------------ */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* hidden */
    HALF_AT(ENT(0)+0x94u)=0x572u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* by character id */
    HALF_AT(A(0)+0x48u)=0x0C;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* reused actor, other character */
    HALF_AT(ENT(0)+0x94u)=0x5BEu;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);  /* by model */
    HALF_AT(A(0)+0x48u)=0x15;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D269u,A(0),0,0x7Fu,1);  /* another caller */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(1),0,0x7Fu,1);  /* deerbeast without the class */
    actor(2,0x0C,0x572,0);WORD_AT(A(2)+0xBF0u)=REC(0);
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(2),0,0x7Fu,1);  /* other MapMaker AI keeps it */
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,0,0,0x7Fu,1);     /* bad pointer */
    BYTE_AT(0x007DE455u)=9u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* not story */
    BYTE_AT(0x007DE455u)=10u;
    WORD_AT(0x0079094Cu)=1u;
    CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* story level scene */
    WORD_AT(0x0079094Cu)=100u;

    /* ---- play: health display choice (chunk +0x29 bit 2 = Sub-boss bar) ---- */
    {
        static const uint8_t boss_sub[]={0,0xB5}, boss_bad[]={0,0xB9};
        container(0x11,with,3,boss_sub,2);
        actor(0,0x15,0x5BE,1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].record==REC(0) && rbs.real[0].boss==2);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,1);  /* Sub-boss: meter kept */
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* and no stock bottom bar */
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
        container(0x11,with,3,boss_on,2);                                     /* Boss bar: meter hidden */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==1);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
        container(0x11,with,3,boss_off,2);                                    /* Boss Off: no meter, no bar */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
        container(0x11,with,3,boss_bad,2);                                    /* unknown value: no Boss flag */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
        CHECK(rbs.real[0].boss==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(0),0,0x7Fu,0);
    }
    /* ---- play: the giant skeleton (id 0x28, model 0x5E9) ------------------ */
    {
        static const uint16_t skel[]={0x0C,0x28}, both[]={0x15,0x28};
        static const uint8_t none3[]={0,0,0}, bar3[]={0,0,0xB1}, sub3[]={0,0,0xB5};
        static const uint32_t actions[8]={8,0,6,3,7,4,2,5};
        unsigned m;
        WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
        for(k=0;k<8u;k++) WORD_AT(0x005208B8u+k*4u)=actions[k];
        WORD_AT(0x005208B8u+8u*4u)=6u;                                       /* past the table */
        /* anims: lv1_6.pak for a custom set holding 0x28 */
        container(0x11,skel,2,none3,3);
        m=nmount;skel_mounted=0;pak_mounted=0;
        WORD_AT(0x006D7880u)=29u;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m && !skel_mounted);                                   /* table nearly full */
        WORD_AT(0x006D7880u)=10u;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u && mount_arg==0x0049D6FCu && skel_mounted && !pak_mounted);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u);                                                 /* already mounted */
        container(0x11,both,2,none3,3);skel_mounted=0;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+3u && skel_mounted && pak_mounted);                  /* both bosses: both paks */
        container(0x11,without,2,none3,3);skel_mounted=0;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(!skel_mounted);
        /* level: the stock state allocation */
        container(0x11,skel,2,none3,3);
        WORD_AT(0x0078B1ECu)=0x00BAD000u;WORD_AT(0x0078B0D0u)=0x00BAD100u;   /* an earlier level's */
        BYTE_AT(0x007DE455u)=9u;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0 && !rbs_skel.state);
        BYTE_AT(0x007DE455u)=10u;
        container(0x11,without,2,none3,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0 && !rbs_skel.state && WORD_AT(0x0078B1ECu)==0x00BAD000u);
        container(0x11,skel,2,none3,3);
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==0);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==1 && !rbs_skel.state && WORD_AT(0x0078B1ECu)==0);       /* failure seen */
        alloc_ok=1;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nalloc==2 && alloc_scene==100u && rbs_skel.state==SKST && rbs_skel.parts==SKPT);
        /* ai_start: the class, one per map */
        actor(3,0x28,0x5E9,2);actor(4,0x28,0x5E9,1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);    /* no lv1_6 */
        CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && !rbs_skel.record);
        skel_mounted=1;
        WORD_AT(0x0078B1ECu)=SKST+0x10u;                                     /* someone else's state */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_skel.record);
        WORD_AT(0x0078B1ECu)=SKST;
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_skel.record);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(seen_class[0]==0 && seen_class[1]==0x20000u && rbs_skel.record==REC(3));   /* before the original */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(4),0,0,1);         /* a second one */
        CHECK(seen_class[0]==0x11000000u && seen_class[1]==0 && rbs_skel.record==REC(3) && rbs_skel.extras==1);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);         /* same one again */
        CHECK(seen_class[1]==0x20000u && rbs_skel.record==REC(3));
        actor(5,0x0C,0x5E9,0);                                               /* by model */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(5),0,0,1);
        CHECK(seen_class[1]==0 && rbs_skel.extras==2);
        /* princess: the mansion variant */
        k=nprincess;
        FLOAT_AT(ENT(3)+0x68u)=-12.5f;                                       /* spawn height: a lower floor */
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(3),0,0,1);     /* no floor read yet */
        CHECK(FLOAT_AT(ENT(3)+0x68u)==-12.5f);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,0);
        CHECK(nprincess==k+1u && princess_ent==ENT(3));
        CHECK(rbs_skel.floor_set && rbs_skel.floor==-12.5f);
        /* the fight sets the height to 0 every frame; the facing step after it restores the floor */
        FLOAT_AT(ENT(3)+0x68u)=0.0f;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,0);
        CHECK(rbs_skel.floor==-12.5f);                                      /* read once */
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(3),0,0,1);
        CHECK(FLOAT_AT(ENT(3)+0x68u)==-12.5f);
        FLOAT_AT(ENT(3)+0x68u)=0.0f;
        CALL(pc_mapmaker_realbosses_face,fake,0x0029E528u,ENT(3),0,0,1);     /* the catacombs call */
        CHECK(FLOAT_AT(ENT(3)+0x68u)==0.0f);
        FLOAT_AT(ENT(4)+0x68u)=3.0f;
        CALL(pc_mapmaker_realbosses_face,fake,0x0029D075u,ENT(4),0,0,1);     /* not the boss */
        CHECK(FLOAT_AT(ENT(4)+0x68u)==3.0f);
        FLOAT_AT(ENT(3)+0x68u)=-12.5f;
        k=nprincess;
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(4),0,0,1);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,0,0,0,1);
        WORD_AT(0x0079094Cu)=1u;                                             /* mission 5 itself */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        WORD_AT(0x0079094Cu)=100u;
        WORD_AT(0x0078B1ECu)=SKST+0x10u;                                     /* state replaced: not ours */
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        WORD_AT(0x0078B1ECu)=SKST;
        CHECK(nprincess==k);
        /* risen: the entrance animation 0x19C */
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,0)==0x77u);
        CHECK(nstart==1 && start_args[0]==ENT(3) && start_args[1]==0x19Cu);
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(4),0,0,1)==0);
        WORD_AT(0x0079094Cu)=17u;
        CHECK(CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,1)==0);
        WORD_AT(0x0079094Cu)=100u;
        /* throw: aim with scene 1, fire in the real scene, then done */
        WORD_AT(A(3)+0x9DCu)=1u;                                             /* action type 0 */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(seen_scene==100u);
        WORD_AT(A(3)+0x9DCu)=2u;WORD_AT(SKST+0x4160u)=0;                     /* type 6: aim */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(seen_scene==1u && WORD_AT(0x0079094Cu)==100u);
        WORD_AT(SKST+0x4160u)=1u;WORD_AT(SKST+0x413Cu+0x18u)=0x00ABC000u;WORD_AT(ENT(3)+0x34u)=0x5151u;
        WORD_AT(SKST+0x413Cu+0x10u)=0x7FC00000u;                            /* a NaN aim: no throw */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);
        CHECK(nshot==0 && WORD_AT(SKST+0x4160u)==2u && rbs_skel.skipped==1);
        FLOAT_AT(SKST+0x413Cu+0x10u)=0.5f;FLOAT_AT(SKST+0x413Cu)=2.0e6f;   /* far off the map: no throw */
        WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);
        CHECK(nshot==0 && WORD_AT(SKST+0x4160u)==2u);
        FLOAT_AT(SKST+0x413Cu)=4.0f;WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);   /* fire */
        CHECK(nshot==1 && WORD_AT(SKST+0x4160u)==2u);
        CHECK(shot_args[0]==0x34u && shot_args[1]==SKST+0x413Cu && shot_args[2]==SKST+0x4148u && shot_args[3]==0x3E99999Au);
        CHECK(shot_args[4]==ENT(3) && shot_args[5]==A(3) && shot_args[6]==2u && shot_args[7]==1u && shot_args[8]==0x5151u);
        CHECK(shot_args[9]==0xEu && shot_args[10]==0xCu && shot_args[11]==0x3DCCCCCDu);
        CHECK(neffect==1 && effect_args[0]==0x00ABC000u && effect_args[1]==0);
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,0);   /* done */
        CHECK(nshot==1 && WORD_AT(SKST+0x4160u)==2u);
        WORD_AT(A(3)+0x9DCu)=8u;                                             /* past the table */
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        WORD_AT(A(4)+0x9DCu)=2u;WORD_AT(SKST+0x4160u)=1u;
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(4),0x00600F00u,0,1);   /* not the boss */
        CHECK(nshot==1 && seen_scene==100u);
        /* event: its death marker is dropped */
        CALL(pc_mapmaker_realbosses_event,fake,0x0029E068u,3u,0,0,0);
        CHECK(rbs_skel.events==1);
        CALL(pc_mapmaker_realbosses_event,fake,0x00227000u,3u,0,0,1);        /* map logic */
        WORD_AT(0x0079094Cu)=1u;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029E068u,3u,0,0,1);        /* mission 5 */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(rbs_skel.events==1);
        /* message: the death (2) reaches the mansion handler, not the catacombs one */
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0x00600F00u,0)==0xFFFFFFFFu);
        CHECK(nmansion_msg==1 && mansion_msg[0]==ENT(3) && mansion_msg[1]==2u && rbs_skel.deaths==1);
        CHECK(CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),5u,0,0)==0);   /* removal: as scene 1 */
        CHECK(nmansion_msg==2 && mansion_msg[1]==5u);
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(4),2u,0,1);             /* not the boss */
        WORD_AT(0x0079094Cu)=1u;
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* mission 5: stock */
        WORD_AT(0x0079094Cu)=17u;
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* catacombs: stock */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(nmansion_msg==2);
        /* bar: Boss Off = no health shown; the stock call never sees the focus */
        WORD_AT(PLAYER+0xFF4u)=ENT(3);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3) && nbar==0);
        container(0x11,skel,2,bar3,3);                                        /* Boss flag: bosses draws */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CHECK(rbs.real[1].record==REC(3) && rbs.real[1].boss==1);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && nbar==0 && WORD_AT(PLAYER+0xFF4u)==ENT(3));
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,0);  /* meter hidden */
        container(0x11,skel,2,sub3,3);                                        /* Sub-boss: the meter */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && nbar==0);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,1);
        container(0x11,skel,2,none3,3);
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(3),0,0,1);
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(3),0,0x7Fu,0);  /* no flag: hidden */
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(4),0,0x7Fu,1);  /* the extra keeps it */
        /* the next level forgets this one */
        container(0x11,without,2,none3,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(!rbs_skel.record && !rbs_skel.state);
        CALL(pc_mapmaker_realbosses_princess,fake,0x0027DDD8u,ENT(3),0,0,1);
        CALL(pc_mapmaker_realbosses_risen,fake,0x0029D4A4u,ENT(3),0,0,1);
        CALL(pc_mapmaker_realbosses_throw,fake_scene,0x0029D055u,ENT(3),0x00600F00u,0,1);
        CHECK(nshot==1);
        CALL(pc_mapmaker_realbosses_message,fake,0x0027D3ACu,ENT(3),2u,0,1);             /* forgotten boss */
        CHECK(nmansion_msg==2);
        WORD_AT(PLAYER+0xFF4u)=ENT(0);
    }
    /* ---- play: both real bosses in one set, any slot 1-20, any order ------ */
    {   /* stock map-info form: count 10, empty slots -1 */
#define E 0xFFFFu
        static const uint16_t skel_trophy[10]={0x28,0x15,E,E,E,E,E,E,E,E}, trophy_skel[10]={0x15,0x28,E,E,E,E,E,E,E,E};
        static const uint16_t plain[10]={0x0C,0xA8,E,E,E,E,E,E,E,E}, last2[10]={0x0C,E,E,E,E,E,E,E,0x15,0x28};
        static const uint16_t trophy0[10]={0x15,E,E,E,E,E,E,E,E,E}, skel0[10]={0x28,E,E,E,E,E,E,E,E,E};
        static const uint16_t x_skel_trophy[10]={0x28,0x15,E,E,E,E,E,E,E,E}, x_trophy_skel[10]={E,E,E,0x15,E,E,E,E,E,0x28};
        static const uint16_t x_skel[10]={0x28,E,E,E,E,E,E,E,E,E}, x_trophy[2]={E,0x15}, x_plain[10]={0xA8,E,E,E,E,E,E,E,E,E};
        LEVEL_WITH(0x11,skel_trophy,10,NULL,0,3);          /* the user's set: skeleton slot 1, trophy slot 2 */
        LEVEL_WITH(0x11,trophy_skel,10,NULL,0,3);          /* reversed */
        LEVEL_WITH(0x11,last2,10,NULL,0,3);                /* slots 9 and 10 */
        LEVEL_WITH(0x11,plain,10,x_skel_trophy,10,3);      /* slots 11 and 12 */
        LEVEL_WITH(0x11,plain,10,x_trophy_skel,10,3);      /* slots 14 and 20, reversed */
        LEVEL_WITH(0x11,trophy0,10,x_skel,10,3);           /* trophy slot 1, skeleton slot 11 */
        LEVEL_WITH(0x11,skel0,10,x_trophy,2,3);            /* skeleton slot 1, trophy slot 12 (short chunk) */
        LEVEL_WITH(0x11,plain,10,x_skel,10,2);             /* skeleton only, slot 11 */
        LEVEL_WITH(0x11,plain,10,x_trophy,2,1);            /* trophy only, slot 12 */
        LEVEL_WITH(0x11,plain,10,x_plain,10,0);            /* neither */
        LEVEL_WITH(0x0C,skel_trophy,10,x_skel_trophy,10,0); /* a profile set index: the members are not played */
        LEVEL_WITH(0x11,plain,10,x_skel_trophy,0,0);       /* chunk with count 0: not the sets area's */
        container(0x11,plain,10,boss_off,2);container_extra(x_skel,10);
        {   /* a wrong chunk header is ignored as by sets_take */
            uint32_t c=CONT+8u;
            while(WORD_AT(c)!=0x00C776F5u) c+=8u+WORD_AT(c+4u);
            BYTE_AT(c+10u)=9u;
            CHECK(rbs_map_has(0x28)==0);
            BYTE_AT(c+10u)=10u;HALF_AT(c+8u)=2u;
            CHECK(rbs_map_has(0x28)==0);
            HALF_AT(c+8u)=1u;
            CHECK(rbs_map_has(0x28)==1 && rbs_map_has(0x15)==0);
        }
#undef E
    }
    /* ---- the Crow walker (id 0x64) ---------------------------------------- */
    {
        static const uint16_t walk_set[]={0x0C,0x64}, no_walk[]={0x0C,0xA8};
        static const uint8_t walk_bar[]={0,0xB1,0}, walk_none[]={0,0,0};
        uint32_t m,well,w2;
        /* editor: pick, preview, row lookups, names */
        WORD_AT(0x0079094Cu)=102u;
        WORD_AT(0x0075FD90u+6u*0x1Cu)=0x99u;WORD_AT(0x0075FD90u+6u*0x1Cu+0x14u)=152u;WORD_AT(0x0075FD90u+6u*0x1Cu+0x18u)=3u;
        CALL(pc_mapmaker_realbosses_pick,fake,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0075FD90u+6u*0x1Cu)==0x64u && WORD_AT(0x0075FD90u+6u*0x1Cu+0x18u)==0xFFFFFFFFu);
        CHECK(WORD_AT(0x0075FD90u+4u*0x1Cu)==0x28u);
        WORD_AT(0x00700000u)=152u;
        CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
        CHECK(WORD_AT(0x0078AEA8u)==90u);
        orig_eax=0xFFFFFFFFu;
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x64u,0xFFFFFFFFu,0,0)==152u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002BE65Fu,0x64u,0xFFFFFFFFu,0x003033C0u,0)==152u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C1805u,0x64u,0xFFFFFFFFu,0,0)==90u);
        CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x000668F5u,0x64u,0xFFFFFFFFu,0,1)==0xFFFFFFFFu);
        {
            uint32_t s=CALL(pc_mapmaker_realbosses_text,fake,0x00077000u,0x52420064u,0,0,0);
            CHECK(s && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Crow Walker"));
        }
        /* play: anims, level state, class */
        WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
        container(0x11,no_walk,2,walk_none,3);
        pak_mounted=skel_mounted=walk_mounted=0;WORD_AT(0x006D7880u)=10u;
        m=nmount;
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nmount==m && !walk_mounted && nwalk_alloc==0 && !rbs_walk.state);    /* not in the set */
        container(0x11,walk_set,2,walk_bar,3);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u && walk_mounted && mount_arg==0x0049D9A8u && !pak_mounted && !skel_mounted);
        CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
        CHECK(nmount==m+1u);                                                      /* already mounted */
        WORD_AT(0x0078B1E8u)=0x00123450u;                                        /* stale pointer */
        alloc_ok=0;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nwalk_alloc==1 && !rbs_walk.state);
        alloc_ok=1;
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(nwalk_alloc==2 && rbs_walk.state==WKST && !rbs_skel.state);
        actor(8,0x64,0x637,1);actor(9,0x64,0x637,2);
        walk_mounted=0;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);                             /* no anims: bot */
        walk_mounted=1;
        set_env("TSFP_MAPMAKER_REAL_BOSS","0");
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);
        set_env("TSFP_MAPMAKER_REAL_BOSS",NULL);
        WORD_AT(0x0078B1E8u)=0;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[1]==0 && !rbs_walk.record);                             /* state gone */
        WORD_AT(0x0078B1E8u)=WKST;
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);
        CHECK(seen_class[0]==0 && seen_class[1]==0x100000u && rbs_walk.record==REC(8) && rbs_walk.actor==A(8));
        for(k=0;k<16u && rbs.real[k].record!=REC(8);k++) {}
        CHECK(k<16u && rbs.real[k].boss==1u);                                    /* Boss flag noted */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(9),0,0,1);
        CHECK(seen_class[1]==0 && rbs_walk.record==REC(8));                      /* only one */
        /* WELL: a stand-in only for the walker's lookup, only when none is found */
        orig_eax=0;
        well=CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1);
        CHECK(well && well!=0xDDDDu);
        WORD_AT(well+0x28u)=0x40u;
        w2=CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1);
        CHECK(w2==well && WORD_AT(well+0x28u)==0);                               /* reused, zeroed */
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x00290000u,0x57454C4Cu,0,0,1)==0);   /* other caller */
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x50524150u,0,0,1)==0);   /* other tag */
        orig_eax=0x00654320u;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0x00654320u);   /* a real one */
        orig_eax=0;WORD_AT(0x0079094Cu)=81u;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0);   /* mission 13 */
        WORD_AT(0x0079094Cu)=100u;
        /* effects: after the frame step at its one call site, in scene 100 */
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1 && fx_scene==100u);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE90u,0,0,0,1);
        CHECK(nfx==1);
        WORD_AT(0x0078B1E8u)=0x00123450u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);                                                            /* not this level's */
        WORD_AT(0x0078B1E8u)=WKST;WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);                                                            /* stock draws it there */
        WORD_AT(0x0079094Cu)=100u;
        /* HUD and meter as the other real bosses */
        WORD_AT(PLAYER+0xFF4u)=ENT(8);
        CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
        CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(8));
        CALL(pc_mapmaker_realbosses_meter,fake,0x0014D264u,A(8),0,0x7Fu,0);
        /* level events: dropped; at the end of the death the walker is removed */
        orig_eax=0;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029AEBEu,1u,0,0,0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029B120u,1u,0,0,0);
        CHECK(nremove==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x00290000u,1u,0,0,1);           /* any other caller */
        WORD_AT(0x0079094Cu)=81u;
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,1);           /* mission 13: stock */
        WORD_AT(0x0079094Cu)=100u;
        CHECK(nremove==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1 && remove_arg==ENT(8) && WORD_AT(PLAYER+0xFF4u)==0);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1);                                                        /* once */
        CALL(pc_mapmaker_realbosses_ai_start,fake_ai_any,0x0015F981u,A(8),0,0,1);   /* same record again */
        CHECK(rbs_walk.actor==A(8));
        WORD_AT(A(8)+0xBF0u)=REC(9);                                              /* the slot now holds another */
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,0);
        CHECK(nremove==1);
        WORD_AT(A(8)+0xBF0u)=REC(8);
        /* the next level forgets this one */
        container(0x11,no_walk,2,walk_none,3);
        CALL(pc_mapmaker_realbosses_level,fake,0x0015E81Du,0,0,0,1);
        CHECK(!rbs_walk.record && !rbs_walk.state);
        CALL(pc_mapmaker_realbosses_event,fake,0x0029CB80u,2u,0,0,1);
        orig_eax=0;
        CHECK(CALL(pc_mapmaker_realbosses_tag,fake,0x0029B331u,0x57454C4Cu,0,0,1)==0);
        CALL(pc_mapmaker_realbosses_fx,fake,0x001CEE8Cu,0,0,0,1);
        CHECK(nfx==1);
        /* slots 11-20 */
        {
            static const uint16_t x_walk[2]={0xFFFFu,0x64};
            container(0x11,no_walk,2,walk_none,3);container_extra(x_walk,2);
            CHECK(rbs_map_has(0x64)==1 && rbs_map_has(0x28)==0 && rbs_map_has(0x15)==0);
            container(0x11,walk_set,2,walk_none,3);
            CHECK(rbs_map_has(0x64)==1 && rbs_map_has(0x28)==0);
        }
        WORD_AT(PLAYER+0xFF4u)=ENT(0);
    }
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* not in play */
    CHECK(seen_focus==ENT(0));

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker realbosses ok");
    return 0;
}
