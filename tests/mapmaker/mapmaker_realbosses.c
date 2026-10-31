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

static uint32_t memory[0x00800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

#define HEAP 0x00300000u
#define ITEM 0x00340000u
#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define ESP0 0x00600000u
#define R 10u
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
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }
static int bosses_area=1;
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)hook;return strcmp(area,"bosses") || bosses_area; }

static unsigned nrows, nenable, nmount;
static struct { uint32_t text, id, flags, cb; char label[32]; } rows[8];
static uint32_t enable_item, enable_flag=0xEEu, mount_arg;
static int pak_mounted, mount_ok=1;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0};
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00079D60) {
        if(nrows<8) {
            rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].flags=x[2];rows[nrows].cb=x[4];
            memcpy(rows[nrows].label,(const char *)((uintptr_t)x[0]+g_xbox_mem_offset),31);rows[nrows].label[31]=0;
        }
        nrows++;return ITEM;
    }
    if(f==sub_00071630) return ITEM;
    if(f==sub_00071C70) { nenable++;enable_item=x[0];enable_flag=x[1];return 0; }
    if(f==sub_00062260) return (x[0]==0x0049D77Cu && pak_mounted)?3u:0xFFFFFFFFu;
    if(f==sub_0005A130) { nmount++;mount_arg=x[0];if(mount_ok) pak_mounted=1;return mount_ok?1u:0u; }
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
static void fake_hud(void) { originals++;seen_focus=WORD_AT(PLAYER+0xFF4u);g_esp+=4u; }
static void fake_pick(void)
{   /* the stock callback stores data read past the Arcade table for row 150 */
    originals++;WORD_AT(0x0075FD90u+2u*0x1Cu)=0x12345u;WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)=150u;
    WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)=7u;g_esp+=4u;
}
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
static void actor(unsigned i,uint32_t id,uint32_t model,uint32_t index)
{
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);HALF_AT(ENT(i)+0x94u)=(uint16_t)model;
    WORD_AT(REC(i))=0x11000000u;WORD_AT(REC(i)+4u)=0;WORD_AT(REC(i)+8u)=R+index+1u;
}

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
    container(0xC,with,3,boss_off,2);
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
    CHECK(nrows==1 && rows[0].id==150u && rows[0].flags==0x20900u && rows[0].cb==0x002C1C90u);
    CHECK(!strcmp(rows[0].label,"Giant Trophy Monster"));
    CHECK(nenable==1 && enable_item==ITEM && enable_flag==0);
    /* pick: row 150 -> id 0x15, variant -1; other slots untouched */
    for(k=0;k<10u;k++) { WORD_AT(0x0075FD90u+k*0x1Cu)=0x40u+k;WORD_AT(0x0075FD90u+k*0x1Cu+0x14u)=k;WORD_AT(0x0075FD90u+k*0x1Cu+0x18u)=0; }
    CALL(pc_mapmaker_realbosses_pick,fake_pick,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu)==0x15u && WORD_AT(0x0075FD90u+2u*0x1Cu+0x18u)==0xFFFFFFFFu);
    CHECK(WORD_AT(0x0075FD90u+2u*0x1Cu+0x14u)==150u);
    CHECK(WORD_AT(0x0075FD90u+3u*0x1Cu)==0x43u && WORD_AT(0x0075FD90u+3u*0x1Cu+0x18u)==0);
    CHECK(WORD_AT(0x0075FD90u+9u*0x1Cu)==0x49u);
    /* highlight: the preview row never becomes 150 */
    WORD_AT(0x00700000u)=150u;
    CALL(pc_mapmaker_realbosses_highlight,fake_highlight,0x00077000u,0x00330000u,0,0,1);
    CHECK(WORD_AT(0x0078AEA8u)==43u);
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
    orig_eax=7u;
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0xA8u,0xFFFFFFFFu,0,1)==7u);
    CHECK(CALL(pc_mapmaker_realbosses_row_of,fake,0x002C054Cu,0x16u,0xFFFFFFFFu,0,1)==7u);
    /* names */
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,150u,0,0,0)==0x52420015u);
    orig_eax=0xE20u;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,149u,0,0,1)==0xE20u);
    orig_eax=0;
    CHECK(CALL(pc_mapmaker_realbosses_row_text,fake,0x002C0907u,0xFFFFFFFFu,0,0,1)==0);
    orig_eax=0x1234u;
    k=CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0x52420015u,0,0,0);
    CHECK(k==rows[0].text);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x002C090Du,0xE20u,0,0,1)==0x1234u);
    CHECK(CALL(pc_mapmaker_realbosses_text,fake,0x0014CED5u,0xDCCu,0,0,1)==0x1234u);   /* not in play */
    CALL(pc_mapmaker_realbosses_model_request,fake,0x000D77AFu,0x5BEu,0,0,1);           /* log only */
    /* editor scene: nothing of play runs */
    container(0xC,with,3,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);

    /* ---- play: anim pak mount -------------------------------------------- */
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=9u;                   /* not story */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    BYTE_AT(0x007DE455u)=10u;
    container(0xC,without,2,boss_off,2);
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(3,with,3,boss_off,2);                                      /* preset set: stale members */
    CALL(pc_mapmaker_realbosses_anims,fake,0x0005C506u,0,0,0,1);
    CHECK(nmount==0);
    container(0xC,with,3,boss_off,2);
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
    CHECK(nmount==2);
    container(0xC,with,3,boss_on,2);

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
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* no Boss flag: stock bar */
    CHECK(seen_focus==ENT(0) && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    actor(0,0x15,0x5BE,1);
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* Boss flag: bosses draws */
    CHECK(seen_focus==0 && WORD_AT(PLAYER+0xFF4u)==ENT(0));
    bosses_area=0;                                                        /* no bosses area: keep stock */
    CALL(pc_mapmaker_realbosses_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);
    CHECK(seen_focus==ENT(0));
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
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_realbosses_hud,fake_hud,0x0014F25Bu,0,0,0,1);        /* not in play */
    CHECK(seen_focus==ENT(0));

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker realbosses ok");
    return 0;
}
