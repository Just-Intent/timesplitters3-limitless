/* Whole-chain contract for the Story AI character choice names of slots
 * 1-20 (src/interface/pc_mapmaker_sets.inc with
 * src/interface/pc_mapmaker_realbosses.inc). The guest stack is real: every
 * call pushes its return address, the callee pops it, cdecl callers pop the
 * arguments; the stock routines on the name path are emulated from the US
 * XBE code (sub_0031CB90's choice loop 0x0031CCF3..0x0031CD32, sub_00303350
 * case 3, sub_002BE650, the tail jumps of sub_002C0290 / sub_002BFEF0) and
 * the generated wrappers chain the hooks in the integrator's order (areas
 * earlier in ORDER are inner: realbosses inside sets). A fake name source
 * that does not go through these return addresses is what hid the
 * slot 11-20 boss names before. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }
static void push(uint32_t v) { g_esp-=4u;WORD_AT(g_esp)=v; }
static uint32_t pop(void) { uint32_t v=WORD_AT(g_esp);g_esp+=4u;return v; }

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* unused by this contract; distinct bodies so the linker does not fold them */
static volatile uint32_t stub_ran;
#define STUB(name,n) void name(void) { stub_ran=n;g_esp+=4u; }
STUB(sub_0003DFA0,1) STUB(sub_00079D60,2) STUB(sub_00071630,3) STUB(sub_00071C70,4) STUB(sub_0005A130,5)
STUB(sub_00062260,6) STUB(sub_00287E00,7) STUB(sub_00059DB0,8) STUB(sub_000EA9C0,9) STUB(sub_0010EB90,10)
STUB(sub_00139740,11) STUB(sub_002E4B80,12) STUB(sub_0029CEF0,13) STUB(sub_00074670,14) STUB(sub_00071970,15)
STUB(sub_00076EF0,16) STUB(sub_002BFFD0,17) STUB(sub_000D7780,18) STUB(sub_002C0310,19) STUB(sub_00286D30,20)
STUB(sub_00075B70,21) STUB(sub_00071BE0,22) STUB(sub_002C1830,23) STUB(sub_00079710,24) STUB(sub_000780E0,25)
STUB(sub_00078320,26) STUB(sub_000792B0,27) STUB(sub_00078380,28) STUB(sub_000781A0,29) STUB(sub_00077140,30)
STUB(sub_00077360,31) STUB(sub_00074A00,32) STUB(sub_00074460,33)
STUB(sub_002976F0,34) STUB(sub_0015DDA0,35) STUB(sub_0029CCB0,36) STUB(sub_00019D70,37) STUB(sub_002A7F30,38) STUB(sub_0005F460,39) STUB(sub_002F7920,40)  /* realbosses: walker (wake: roar), skeleton fire */
STUB(sub_00287860,41) STUB(sub_00297690,42) STUB(sub_002BD480,43)  /* realbosses: the stock skeleton/walker resets at a restart, the walker grid */
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    static uint32_t next=0x00400000u;
    uint32_t p=next;(void)alignment;next+=(bytes+15u)&~15u;return p;
}
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)area;(void)hook;return 1; }

#define CALLBACK 0x0012F000u            /* PC_DISPLAY_MENU_CALLBACK stand-in */
static uint32_t guest_call(void (*f)(void),int n,...)
{
    GuestRegisters r;
    uint32_t x[12],result;
    int i;
    va_list a;
    save_registers(&r);
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    for(i=n;i;i--) push(x[i-1]);
    push(CALLBACK);f();result=g_eax;
    restore_registers(&r);
    return result;
}

/* the wrapped entry points (defined after the hooks) */
void sub_0003D7B0(void),sub_002BE600(void),sub_002BE300(void),sub_002C0290(void),sub_00312950(void),
    sub_00303350(void),sub_00071800(void),sub_00078590(void);

#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_realbosses.inc"
#include "pc_mapmaker_sets.inc"

#define EDITOR 0x00200000u
#define ESET (EDITOR+0x11860u)
#define VALUE 0x00305000u
#define EMPTY 0x004766D8u               /* sub_0003D7B0(0) */
#define TEXT(id) (0x00800000u+(uint32_t)(id))
#define ROW_NAME(row) TEXT(0x100u+(row))
#define ROW_OF(id,variant) (((uint32_t)(id)*2u+((variant)==-1?0u:1u))%150u)

/* ---- stock originals (pop their return address) --------------------------- */
static void sub_0003D7B0_gen(void)                  /* text id -> string */
{
    uint32_t id=WORD_AT(g_esp+4u);
    g_eax=id?TEXT(id):EMPTY;g_esp+=4u;
}
static void sub_002BE600_gen(void)                  /* (id, variant) -> Arcade row or -1 */
{
    int32_t id=(int32_t)WORD_AT(g_esp+4u),variant=(int32_t)WORD_AT(g_esp+8u);
    g_eax=(id==0x15 || id==0x28 || id==0x1FF || id<0)?0xFFFFFFFFu:ROW_OF(id,variant);
    g_esp+=4u;
}
static void sub_002BE300_gen(void)                  /* row -> text id; 0 for rows >= 150 and -1 */
{
    uint32_t row=WORD_AT(g_esp+4u);
    g_eax=row>=150u?0u:0x100u+row;g_esp+=4u;
}
static void sub_00312950_gen(void)                  /* scene 102: the editor set */
{
    g_eax=WORD_AT(0x0079094Cu)==102u?WORD_AT(0x007844A8u)+0x11860u:0x00762DC0u;g_esp+=4u;
}
static void sub_002BFEF0(void) { sub_00312950(); } /* scene 102: jmp sub_00312950 */
static void sub_002C0290_gen(void)                  /* index -> set; >= 0xC: jmp sub_002BFEF0 */
{
    uint32_t index=WORD_AT(g_esp+4u);
    if((int32_t)index>=0xC) { g_eax=index;sub_002BFEF0();return; }
    g_eax=0x0053E41Cu+index*0xA4u;g_esp+=4u;
}
static void sub_003129D0(void) { g_eax=WORD_AT(0x00784024u);g_esp+=4u; }
static unsigned nchoice;
static void sub_00078590_gen(void) { nchoice++;g_esp+=4u; }
static uint32_t options[32][2];
static unsigned noptions;

/* ---- generated wrappers, realbosses inner, sets outer -------------------- */
void sub_0003D7B0(void) { pc_mapmaker_realbosses_text(sub_0003D7B0_gen); }
void sub_002BE600(void) { pc_mapmaker_realbosses_row_of(sub_002BE600_gen); }
void sub_002BE300(void) { pc_mapmaker_realbosses_row_text(sub_002BE300_gen); }
void sub_002C0290(void) { pc_mapmaker_sets_set_ptr(sub_002C0290_gen); }
void sub_00312950(void) { pc_mapmaker_sets_editor_set(sub_00312950_gen); }
void sub_00078590(void) { pc_mapmaker_sets_sai_choice(sub_00078590_gen); }

/* ---- lifted stock routines on the name path ------------------------------ */
void sub_00071800(void)                             /* (text, value) -> option */
{
    if(noptions<32u) { options[noptions][0]=WORD_AT(g_esp+4u);options[noptions][1]=WORD_AT(g_esp+8u); }
    noptions++;g_eax=0x00310000u+noptions;g_esp+=4u;
}
static void sub_002BE650(void)                      /* sub_002BE300(sub_002BE600(id, variant)) */
{
    uint32_t variant=WORD_AT(g_esp+8u),id=WORD_AT(g_esp+4u);
    push(variant);push(id);
    push(0x002BE65Fu);sub_002BE600();
    push(g_eax);
    push(0x002BE665u);sub_002BE300();
    g_esp+=0xCu;
    g_esp+=4u;
}
void sub_00303350(void)                             /* item -> name; case 3: slot items 0x38..0x41 */
{
    uint32_t item,set;
    push(g_esi);
    item=WORD_AT(g_esp+8u);
    if(item<0x38u || item>0x41u) { g_eax=0;g_esi=pop();g_esp+=4u;return; }
    g_esi=item;
    push(0x003033A3u);sub_003129D0();
    push(g_eax);
    push(0x003033A9u);sub_002C0290();
    set=g_eax;
    push((uint32_t)(int32_t)HALF_AT(set+g_esi*8u-0x1B6u));
    push((uint32_t)(int32_t)HALF_AT(set+g_esi*8u-0x1BCu));
    push(0x003033C0u);sub_002BE650();
    g_esp+=0xCu;
    g_esi=pop();
    WORD_AT(g_esp+4u)=g_eax;
    sub_0003D7B0();                                 /* jmp: pops this routine's return address */
}
/* sub_0031CB90 0x0031CBAF..0x0031CD35: the Story AI character choice */
static void build_choice(void)
{
    uint32_t sp=g_esp,ebx;
    unsigned k;
    noptions=0;nchoice=0;
    push(0x0031CBB4u);sub_003129D0();
    push(g_eax);
    push(0x0031CBBAu);sub_002C0290();
    g_esp+=4u;
    ebx=g_eax+4u;
    for(k=0;k<10u;k++,ebx+=8u) {
        push(k);
        if(HALF_AT(ebx)==-1) { push(0xD7Cu);push(0x0031CD14u);sub_0003D7B0(); }
        else { push(0x38u+k);push(0x0031CD08u);sub_00303350(); }
        g_esp+=4u;
        push(g_eax);
        push(0x0031CD1Du);sub_00071800();
        g_esp+=8u;
    }
    push(VALUE);
    push(0x0031CD32u);sub_00078590();
    g_esp+=4u;
    if(g_esp!=sp) { fprintf(stderr,"choice esp %+d\n",(int)(g_esp-sp));failures++;g_esp=sp; }
}
/* sub_00075540: the choice shows the option whose value is the current one */
static uint32_t shown(void)
{
    unsigned i;
    for(i=0;i<noptions && i<32u;i++) if(options[i][1]==WORD_AT(VALUE)) return options[i][0];
    return 0;
}
static uint32_t label(unsigned k)
{
    unsigned i;
    for(i=0;i<noptions && i<32u;i++) if(options[i][1]==k) return options[i][0];
    return 0;
}
static const char *str(uint32_t p) { return (const char *)((uintptr_t)p+g_xbox_mem_offset); }

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void member(unsigned k,int16_t id,int16_t variant)
{
    HALF_AT(ESET+4u+8u*k)=id;HALF_AT(ESET+4u+8u*k+6u)=variant;
}

int main(void)
{
    /* slot ids: bosses in both halves, plain Arcade ids, an id without a row, empty */
    static const int16_t ids[20]={0x30,0x31,0x15,0x34,0x28,-1,0x1FF,0x37,0x38,0x39,
                                  0x40,0x41,0x15,0x43,0x28,-1,0x1FF,0x47,0x48,0x49};
    unsigned k,pass;
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=0x00600000u;
    WORD_AT(0x007844A8u)=EDITOR;WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0x11u;
    WORD_AT(ESET)=10u;
    for(k=0;k<20u;k++) member(k,ids[k],(int16_t)(ids[k]==0x15 || ids[k]==0x28 || k%2u?-1:1));
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    for(pass=0;pass<2u;pass++) {                    /* reopening: stock names stay members 0..9 */
        WORD_AT(VALUE)=12u;
        build_choice();
        CHECK(noptions==20u && nchoice==1u);
        for(k=0;k<20u;k++) CHECK(options[k][1]==k);
        CHECK(label(2) && !strcmp(str(label(2)),"Giant Trophy Monster"));
        CHECK(label(12)==label(2));                 /* slot 13 = slot 3 */
        CHECK(label(4) && !strcmp(str(label(4)),"Giant Skeleton"));
        CHECK(label(14)==label(4));                 /* slot 15 = slot 5 */
        CHECK(shown()==label(2));                   /* the selected value of a slot-13 Story AI */
        WORD_AT(VALUE)=14u;CHECK(shown()==label(4));
        for(k=0;k<20u;k++) {
            int id=ids[k];
            int variant=HALF_AT(ESET+4u+8u*k+6u);
            if(id==-1) CHECK(label(k)==TEXT(0xD7Cu));
            else if(id==0x1FF) CHECK(label(k)==EMPTY);
            else if(id!=0x15 && id!=0x28) CHECK(label(k)==ROW_NAME(ROW_OF(id,variant)));
        }
        CHECK(!sets.name_shift);
    }
    /* the switch off: realbosses names nothing, the extra options only for a
       Story AI already on slot 11-20; both halves alike */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(VALUE)=3u;build_choice();CHECK(noptions==10u);
    WORD_AT(VALUE)=12u;build_choice();
    CHECK(noptions==20u && label(12)==label(2) && label(12)==EMPTY && label(10)==ROW_NAME(ROW_OF(0x40,1)));
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* a preset set (index < 0xC): stock only */
    WORD_AT(0x00784024u)=4u;
    for(k=0;k<10u;k++) HALF_AT(0x0053E41Cu+4u*0xA4u+4u+8u*k)=(int16_t)(0x20+k);
    build_choice();CHECK(noptions==10u && label(2)==ROW_NAME(ROW_OF(0x22,0)));
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker sets names ok\n");
    return 0;
}
