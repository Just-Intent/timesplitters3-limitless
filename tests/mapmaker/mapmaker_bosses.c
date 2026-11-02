/* Asset-free contract for src/interface/pc_mapmaker_bosses.inc: the Story AI
 * "Boss" row (cycle, stays open), the per-entry table through delete, reset,
 * compile (chunk +0x29) and load, and in play the health multiplier and the
 * nearest-boss bar drawn through sub_00139740; plus the read-only creature
 * probe. Fake guest memory; fake originals pop their return address like the
 * lifted callees, and every hook call checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x01000000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di,bp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi;r->bp=g_ebp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di;g_ebp=r->bp; }

#define ED 0x00100000u
#define CONT 0x00200000u
#define HEAP 0x00300000u
#define MENU 0x00380000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define ESP0 0x00600000u
#define RESTAB 0x00680000u
#define R 10u
#define E(i) (ED+0x58390u+(uint32_t)(i)*0xE4u)
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define HUD0 0x007A6160u
#define PLAYER 0x00700000u

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_00079D60(void) { g_ecx=4; }
void sub_00139740(void) { g_ecx=8; }
void sub_00071800(void) { g_ecx=16; }
void sub_0013A400(void) { g_ecx=32; }
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p=heap_next;(void)alignment;heap_next+=(bytes+15u)&~15u;return p; }

#define MARGIN 0x007A614Cu
static unsigned nrows, bars, bar_draw=1, nlog, nitems, bound_items;
static struct { uint32_t text, id, cb; } rows[16];
static struct { uint32_t text, value; } items[16];
typedef struct { uint32_t esi, edi, text, frac, out, esp, margin, smooth[3]; char label[32]; } Bar;
static Bar bar, blog[8];                         /* bar = first call of a frame */
static unsigned nmeters;                         /* sub_0013A400(actor, 0, 0x7F, 0x6E) calls */
static struct { uint32_t a, b, c, d, esp; } meter;
/* bar_model: sub_00139740's drain rule (0x001398B8..0x00139AAF), fill = the
 * solid length 1 - (damage - drain x fade) it passes to sub_00130400. */
static int bar_model;
static float fills[8];
static float clamp01(float f) { return f<0.0f?0.0f:f>1.0f?1.0f:f; }
static void model_bar(uint32_t hud,uint32_t ent,unsigned k)
{
    float d=clamp01(1.0f-FLOAT_AT(ent+0x1CCu)/FLOAT_AT(ent+0x1D8u)),prev=FLOAT_AT(hud+0x171Cu);
    float drain=FLOAT_AT(hud+0x1724u),fade=FLOAT_AT(hud+0x172Cu),hit=d>prev?d-prev:0.0f;
    if(fade>0.0f) { if(hit>0.0f) { fade=(drain*fade+hit)/(drain+hit);drain+=hit; } }
    else if(hit>0.0f) { drain=hit;fade=1.0f; }
    if(k<8) fills[k]=clamp01(1.0f-(d-drain*fade));
    FLOAT_AT(hud+0x171Cu)=d;FLOAT_AT(hud+0x1724u)=drain;FLOAT_AT(hud+0x172Cu)=fade;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00079D60) { if(nrows<16) { rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].cb=x[4]; } nrows++; }
    else if(f==sub_00071800) { if(nitems<16) { items[nitems].text=x[0];items[nitems].value=x[1]; } nitems++; }
    else if(f==sub_0013A400) {                    /* overhead meter: cdecl (actor, 0, 0x7F, 0x6E) */
        meter.a=x[0];meter.b=x[1];meter.c=x[2];meter.d=x[3];meter.esp=g_esp;nmeters++;
        g_esi=g_edi=g_ebx=0xDEADu;
    }
    else if(f==sub_00139740) {                    /* esi = HUD block, edi = entity; (text, fraction, int *) */
        Bar b;
        unsigned k;
        b.esi=g_esi;b.edi=g_edi;b.text=x[0];b.frac=x[1];b.out=x[2];b.esp=g_esp;b.margin=WORD_AT(MARGIN);
        for(k=0;k<3;k++) b.smooth[k]=WORD_AT(g_esi+0x171Cu+k*8u);
        memcpy(b.label,(const char *)((uintptr_t)x[0]+g_xbox_mem_offset),31);b.label[31]=0;
        if(nlog<8) blog[nlog]=b;
        if(!nlog) bar=b;
        nlog++;bars++;
        if(bar_draw && x[2]>=0x10000u && x[2]<sizeof memory) WORD_AT(x[2])=1;
        if(bar_model) model_bar(g_esi,g_edi,nlog-1u);
        else for(k=0;k<3;k++) WORD_AT(g_esi+0x171Cu+k*8u)=g_edi+k;   /* the bar's own smoothing */
        g_esi=g_edi=0xDEADu;                      /* clobbers; the hook must restore */
    }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_bosses.inc"

static uint32_t proxy_from, proxy_to;
static uint32_t test_proxy(uint32_t ent) { return ent==proxy_from?proxy_to:0u; }
static int failures, esp_errors;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void hook1(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg,int line)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=arg;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
}
#define HOOK(fn,orig,arg) hook1(fn,orig,0x00123456u,(uint32_t)(arg),__LINE__)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static const char *text_of(uint32_t a) { return (const char *)((uintptr_t)a+g_xbox_mem_offset); }
static uint8_t byte_at(uint32_t a) { return *(uint8_t *)((uintptr_t)a+g_xbox_mem_offset); }

/* ---- fake originals ------------------------------------------------------ */
static unsigned orig_calls;
static void fake_plain(void) { orig_calls++;g_esp+=4u; }
static void fake_compile(void)                   /* sub_003006C0: zero, one chunk per entry */
{
    uint32_t c=WORD_AT(g_esp+4u),n=WORD_AT(ED+0x280u),i,p;
    orig_calls++;
    memset((void *)((uintptr_t)c+g_xbox_mem_offset),0,0x2000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4u)=0x12Du;
    p=c+8u;
    WORD_AT(p)=0x00C77668u;WORD_AT(p+4u)=0x30u;p+=0x38u;      /* an unrelated chunk first */
    for(i=0;i<n;i++) {
        uint32_t sz=(WORD_AT(E(i)+0x58u)*4u+0x53u)&~0xFu;
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=sz;
        memcpy((char *)((uintptr_t)p+8u+g_xbox_mem_offset),(const char *)((uintptr_t)E(i)+g_xbox_mem_offset),31);
        WORD_AT(p+8u+0x20u)=(WORD_AT(E(i)+0x34u)&0xFFu)|(i<<16);  /* +0x20 type, +0x22 index */
        WORD_AT(p+8u+0x28u)=1u|(0x0007u<<16);                     /* +0x28 toggle, +0x2A weapon */
        *(uint8_t *)((uintptr_t)p+8u+0x30u+g_xbox_mem_offset)=(uint8_t)WORD_AT(E(i)+0x4Cu);   /* health byte */
        *(uint8_t *)((uintptr_t)p+8u+0x31u+g_xbox_mem_offset)=0x5Au;
        p+=8u+sz;
    }
    WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=0;
    g_eax=c;g_esp+=4u;
}
static uint32_t load_skew;                       /* entry index + 1 with a wrong +0x24 */
static void fake_load(void)                      /* sub_00301A90: append entries from chunks */
{
    uint32_t c=WORD_AT(g_esp+4u)+8u,n=WORD_AT(ED+0x280u),first=n;
    orig_calls++;
    while(WORD_AT(c)!=0x00C7766Au) {
        if(WORD_AT(c)==0x00C7766Cu) {
            memset((void *)((uintptr_t)E(n)+g_xbox_mem_offset),0,0xE4);
            WORD_AT(E(n)+0x24u)=(uint32_t)(int32_t)(int16_t)(WORD_AT(c+8u+0x20u)>>16)+(load_skew && n==first+load_skew-1u?1u:0u);
            WORD_AT(E(n)+0x34u)=WORD_AT(c+8u+0x20u)&0xFFu;
            WORD_AT(E(n)+0x4Cu)=*(uint8_t *)((uintptr_t)c+8u+0x30u+g_xbox_mem_offset);   /* 0x0030260A */
            WORD_AT(E(n)+0x60u)=n%50u;n++;
        }
        c+=8u+WORD_AT(c+4u);
    }
    WORD_AT(ED+0x280u)=n;
    g_esp+=4u;
}
static void fake_reset(void) { orig_calls++;WORD_AT(ED+0x280u)=0;g_esp+=4u; }
static void fake_delete(void)                    /* sub_0031C8B0(i): memmove down */
{
    uint32_t i=WORD_AT(g_esp+4u),n=WORD_AT(ED+0x280u);
    orig_calls++;
    if(i>=n) { g_esp+=4u;return; }
    memmove((void *)((uintptr_t)E(i)+g_xbox_mem_offset),(void *)((uintptr_t)E(i+1u)+g_xbox_mem_offset),(n-i-1u)*0xE4u);
    WORD_AT(ED+0x280u)=n-1u;
    g_esp+=4u;
}
static unsigned copy_mode;                      /* aitemplates inside the close: 1 duplicate, 2 linked, 3 not a copy */
static void fake_copy(void)                      /* sub_0031CAC0 + aitemplates: append a copy of the selected AI */
{
    uint32_t n=WORD_AT(ED+0x280u),s=WORD_AT(0x00762F94u)&0xFFFFu,o;
    orig_calls++;
    if(copy_mode && s<n) {
        memcpy((void *)((uintptr_t)E(n)+g_xbox_mem_offset),(const void *)((uintptr_t)E(s)+g_xbox_mem_offset),0xE4);
        snprintf((char *)((uintptr_t)E(n)+g_xbox_mem_offset),32,"Copy %u",(unsigned)n);
        WORD_AT(E(n)+0x50u)=copy_mode==2u?s+1u:0u;WORD_AT(E(n)+0x60u)=40u+n;
        WORD_AT(E(n)+0x58u)=0;for(o=0x64u;o<0xE4u;o+=4u) WORD_AT(E(n)+o)=0;   /* ait_unplace */
        if(copy_mode==3u) WORD_AT(E(n)+0x28u)^=1u;
        WORD_AT(ED+0x280u)=n+1u;
    }
    g_esp+=4u;
}
static int stock_bar;
static void fake_hud(void)                       /* sub_0014CC80: stock bar flag in HUD block +8 */
{
    orig_calls++;
    if(stock_bar) WORD_AT(g_ebx+8u)|=1u; else WORD_AT(g_ebx+8u)&=~1u;
    g_esp+=4u;
}

static void make_ai(uint32_t i,uint32_t type,const char *name)
{
    uint32_t e=E(i);
    memset((void *)((uintptr_t)e+g_xbox_mem_offset),0,0xE4);
    snprintf((char *)((uintptr_t)e+g_xbox_mem_offset),32,"%s",name);
    WORD_AT(e+0x20u)=1;WORD_AT(e+0x34u)=type;WORD_AT(e+0x58u)=1;WORD_AT(e+0x60u)=i;
}
static uint32_t sai_data(unsigned k)
{
    uint32_t c=CONT+8u;
    while(WORD_AT(c)!=0x00C7766Au) { if(WORD_AT(c)==0x00C7766Cu && !k--) return c+8u; c+=8u+WORD_AT(c+4u); }
    return 0;
}
static uint8_t saved_flag(unsigned k) { return byte_at(sai_data(k)+0x29u); }
static void set_saved_flag(unsigned k,uint8_t v) { *(uint8_t *)((uintptr_t)sai_data(k)+0x29u+g_xbox_mem_offset)=v; }
static void select_ai(uint32_t i) { WORD_AT(0x00762F94u)=(WORD_AT(0x00762F94u)&~0xFFu)|i; }
static void press(uint32_t row) { WORD_AT(MENU+0x18Cu)=row; }
static void rows_for(uint32_t s,uint32_t ret,uint32_t cb)
{
    nrows=0;select_ai(s);
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=0x40000000u;WORD_AT(ESP0+0x24u)=cb;
    pc_mapmaker_bosses_rows(fake_plain);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"rows: esp %+d\n",(int)(g_esp-ESP0));esp_errors++; }
}
static const char *row_label(void) { return nrows>=1?text_of(rows[0].text):""; }
static const char *display_label(void) { return nrows==2 && rows[1].id==0x424F5302u?text_of(rows[1].text):""; }
static void click(uint32_t s)                    /* the boss row's callback */
{
    select_ai(s);press(0x424F5301u);orig_calls=0;
    HOOK(pc_mapmaker_bosses_close,fake_plain,MENU);
}
static unsigned level_of(uint32_t s) { rows_for(s,0x0031D083u,0x0031CAC0u); return bos_get(ED,s); }
static void click_display(uint32_t s)            /* the health display row's callback */
{
    select_ai(s);press(0x424F5302u);orig_calls=0;
    HOOK(pc_mapmaker_bosses_close,fake_plain,MENU);
}

static void copy_row(uint32_t s,uint32_t row,unsigned mode)
{
    select_ai(s);press(row);copy_mode=mode;orig_calls=0;
    HOOK(pc_mapmaker_bosses_close,fake_copy,MENU);
}

/* ---- play fixtures --------------------------------------------------------- */
static void setf(uint32_t a,float f) { FLOAT_AT(a)=f; }
static void make_actor(uint32_t slot,uint32_t index,float x,float health)
{
    uint32_t a=A(slot),e=ENT(slot),r=REC(slot);
    WORD_AT(a)=e;WORD_AT(e+0x7Cu)=a;WORD_AT(a+0xBF0u)=r;WORD_AT(r+8u)=R+index+1u;
    WORD_AT(a+0x48u)=0x28u;
    setf(e+0x64u,x);setf(e+0x68u,0.0f);setf(e+0x6Cu,0.0f);
    setf(e+0x1D8u,1.0f);setf(e+0x1CCu,health);
}
static void list_set(uint32_t n)
{
    uint32_t i;
    WORD_AT(0x0074C37Cu)=n;WORD_AT(0x00790950u)=1u;
    for(i=0;i<n;i++) WORD_AT(0x00900100u+i*4u)=A(i);
}
static void play_container(void)
{
    /* chunks: index 0 boss x4 "Big Robot", 1 none, 3 x1 unnamed, 2 bad flag,
     * 4 Titan (x8, no boss), 5 boss x2 + Brute (x4), 6 health byte not Tough
     * Guy (extra ignored), 7 bad extra mark */
    uint32_t p=CONT+8u,k;
    static const uint32_t idx[9]={0,1,3,2,4,5,6,7,8};
    static const uint8_t flag[9]={0xB2,0,0xB0,0x77,0,0xB1,0,0,0xB5};
    static const uint8_t extra[9]={0,0,0,0,0xA3,0xA1,0xA2,0x93,0};
    static const uint8_t health[9]={2,2,2,2,4,4,3,4,2};
    static const char *const name[9]={"Big Robot","Guard","","Odd","Titan","Duo","Weak","Bad","Sub"};
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x2000);
    WORD_AT(CONT)=0x00C77667u;
    for(k=0;k<9;k++) {
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=0x50u;
        snprintf((char *)((uintptr_t)p+8u+g_xbox_mem_offset),32,"%s",name[k]);
        WORD_AT(p+8u+0x20u)=1u|(idx[k]<<16);
        *(uint8_t *)((uintptr_t)p+8u+0x21u+g_xbox_mem_offset)=extra[k];
        *(uint8_t *)((uintptr_t)p+8u+0x29u+g_xbox_mem_offset)=flag[k];
        *(uint8_t *)((uintptr_t)p+8u+0x30u+g_xbox_mem_offset)=health[k];
        p+=0x58u;
    }
    WORD_AT(p)=0x00C7766Au;
    WORD_AT(0x00784014u)=CONT;
}
static int hud_frame(uint32_t hud)              /* returns 1 when our bar was drawn */
{
    unsigned before=bars,k;
    g_ebx=hud;g_esi=0x51u;g_edi=0xD1u;orig_calls=0;nlog=0;
    WORD_AT(MARGIN)=20u;
    for(k=0;k<3;k++) WORD_AT(hud+0x171Cu+k*8u)=0x5100u+k;  /* the stock bar's smoothing */
    HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
    CHECK(orig_calls==1);
    CHECK(g_ebx==hud && g_esi==0x51u && g_edi==0xD1u);
    CHECK(WORD_AT(MARGIN)==20u);                  /* margin put back */
    for(k=0;k<3;k++) CHECK(WORD_AT(hud+0x171Cu+k*8u)==0x5100u+k);
    return bars!=before;
}
/* one game tick (sub_0014B090 on the live HUD block) and one HUD frame */
static void drain_frame(uint32_t hud)
{
    float fade=FLOAT_AT(hud+0x172Cu)-(1.0f/60.0f)*FLOAT_AT(0x00475D10u);
    uint32_t live[3];
    unsigned k;
    FLOAT_AT(hud+0x2Cu)+=1.0f/60.0f;
    FLOAT_AT(hud+0x172Cu)=fade<0.0f?0.0f:fade;
    for(k=0;k<3;k++) live[k]=WORD_AT(hud+0x171Cu+k*8u);
    g_ebx=hud;orig_calls=0;nlog=0;
    HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
    CHECK(orig_calls==1);
    for(k=0;k<3;k++) CHECK(WORD_AT(hud+0x171Cu+k*8u)==live[k]);   /* the stock bar's own state */
}
static float fill_of(uint32_t ent)
{
    unsigned k;
    for(k=0;k<nlog && k<8;k++) if(blog[k].edi==ent) return fills[k];
    return -1.0f;
}
static int near(float a,float b) { return a>b-0.002f && a<b+0.002f; }
static int margin_ok(unsigned k,unsigned slot,unsigned step) { return k<nlog && blog[k].margin==20u+slot*step; }
/* sub_0014D1F0's fake: the meters must be drawn before it (its loop runs first) */
static unsigned meters_at_orig;
static void fake_meters(void) { orig_calls++;meters_at_orig=nmeters;g_esp+=4u; }
static unsigned meters_frame(void)
{
    nmeters=0;meters_at_orig=99;orig_calls=0;memset(&meter,0,sizeof meter);
    g_ebx=0xB1u;g_esi=0x51u;g_edi=0xD1u;
    HOOK(pc_mapmaker_bosses_meters,fake_meters,0);
    CHECK(orig_calls==1 && meters_at_orig==nmeters);
    CHECK(g_ebx==0xB1u && g_esi==0x51u && g_edi==0xD1u);
    return nmeters;
}
static void meter_model(uint32_t slot)          /* model instance: +0x8C skeleton header, +0x10 nodes */
{
    uint32_t inst=0x00500000u+slot*0x100u;
    WORD_AT(ENT(slot)+4u)=inst;WORD_AT(inst+0x8Cu)=0x00510000u;WORD_AT(inst+0x10u)=0x00520000u;
    WORD_AT(ENT(slot)+0x268u)=0;
}
/* the health list row: options appended before the original binds the field */
static void bind_fake(void) { orig_calls++;bound_items=nitems;g_esp+=4u; }
static void health_rows(uint32_t s,uint32_t ret,uint32_t field)
{
    nitems=0;bound_items=99;select_ai(s);orig_calls=0;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=field;
    pc_mapmaker_bosses_health_rows(bind_fake);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"health rows: esp %+d\n",(int)(g_esp-ESP0));esp_errors++; }
    CHECK(orig_calls==1);
}

int main(void)
{
    uint32_t i;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(0x0079094Cu)=102u;
    WORD_AT(0x00783A50u)=R;

    /* ---- limits off: nothing changes ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);
    set_env("TSFP_MAPMAKER_CREATURE_PROBE",NULL);set_env("TSFP_MAPMAKER_BOSS_RANGE",NULL);
    make_ai(0,1,"Robot");WORD_AT(ED+0x280u)=1;
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==0);
    click(0);
    CHECK(orig_calls==1);
    CHECK(bos.level[0]==0);
    health_rows(0,0x0031CFD7u,E(0)+0x4Cu);
    CHECK(nitems==0);

    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- stronger health options on the stock Health row ---- */
    health_rows(0,0x0031CFD7u,E(0)+0x4Cu);
    CHECK(nitems==4 && bound_items==4);           /* before the binding */
    {
        static const char *const want[4]={"Brute","Juggernaut","Titan","Colossus"};
        for(i=0;i<4;i++) CHECK(items[i].value==5u+i && !strcmp(text_of(items[i].text),want[i]));
    }
    health_rows(0,0x0031CFD8u,E(0)+0x4Cu); CHECK(nitems==0);        /* another binding */
    health_rows(0,0x0031CFD7u,E(0)+0x48u); CHECK(nitems==0);        /* another field */
    health_rows(1,0x0031CFD7u,E(0)+0x4Cu); CHECK(nitems==0);        /* not the selected entry */
    WORD_AT(0x0079094Cu)=100u; health_rows(0,0x0031CFD7u,E(0)+0x4Cu); CHECK(nitems==0);
    WORD_AT(0x0079094Cu)=102u;

    /* ---- rows ---- */
    make_ai(1,1,"Guard");make_ai(2,3,"Sniper");make_ai(3,5,"Solo");WORD_AT(ED+0x280u)=4;
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==1 && rows[0].id==0x424F5301u && rows[0].cb==0x0031CAC0u);
    CHECK(!strcmp(row_label(),"Boss: Off"));
    rows_for(0,0x0031D084u,0x0031CAC0u); CHECK(nrows==0);        /* another spacer call */
    rows_for(0,0x0031D083u,0x002FB3C0u); CHECK(nrows==0);        /* other callback */
    WORD_AT(0x0079094Cu)=100u; rows_for(0,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=102u;
    rows_for(3,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);        /* type 5 */
    rows_for(4,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);        /* past the count */

    /* ---- cycling: Off -> On -> x2 -> x4 -> x8 -> Off; the screen stays open ---- */
    rows_for(0,0x0031D083u,0x0031CAC0u);
    {
        uint32_t label=rows[0].text;
        static const char *const want[5]={"Boss: On","Boss: health x2","Boss: health x4","Boss: health x8","Boss: Off"};
        for(i=0;i<5;i++) {
            click(0);
            CHECK(orig_calls==0);                 /* no close: stays open */
            CHECK(!strcmp(text_of(label),want[i]));   /* rewritten in place */
            CHECK(bos_get(ED,0)==(i+1u)%5u);
        }
    }
    click(0);CHECK(level_of(0)==1u && !strcmp(row_label(),"Boss: On"));
    press(0x41540001u);select_ai(0);orig_calls=0;                  /* another row: original */
    HOOK(pc_mapmaker_bosses_close,fake_plain,MENU);
    CHECK(orig_calls==1 && bos_get(ED,0)==1u);
    press(0xFFFFFFFFu);orig_calls=0;HOOK(pc_mapmaker_bosses_close,fake_plain,MENU);   /* back */
    CHECK(orig_calls==1);
    click(3);CHECK(orig_calls==1 && bos.level[3]==0);               /* type 5: no setting */
    click(1);click(1);click(1);                                    /* Guard x4 */
    click(2);click(2);click(2);click(2);                           /* Sniper x8 */
    CHECK(level_of(1)==3u && level_of(2)==4u && level_of(0)==1u);
    click_display(2);CHECK(bos_get_sub(ED,2)==1u && bos_get_sub(ED,1)==0u);

    /* ---- delete shifts the later settings ---- */
    HOOK(pc_mapmaker_bosses_delete,fake_delete,1);
    CHECK(WORD_AT(ED+0x280u)==3u);
    WORD_AT(E(1)+0x60u)=2u;                                        /* the moved entry keeps its colour */
    CHECK(level_of(0)==1u && level_of(1)==4u && level_of(2)==0u);
    CHECK(bos_get_sub(ED,1)==1u && bos.sub[2]==0);                 /* the choice moved with it */
    CHECK(bos.level[3]==0);
    HOOK(pc_mapmaker_bosses_delete,fake_delete,7);                 /* refused / out of range */
    CHECK(level_of(1)==4u);

    /* the last slot is cleared: a new entry at the tail starts Off */
    click(2);CHECK(level_of(2)==0u);                               /* Solo is type 5 */
    WORD_AT(E(2)+0x34u)=1u;click(2);click(2);CHECK(level_of(2)==2u);
    HOOK(pc_mapmaker_bosses_delete,fake_delete,0);                 /* 3 -> 2 entries */
    CHECK(level_of(0)==4u && level_of(1)==2u);
    make_ai(2,1,"New");WORD_AT(E(2)+0x60u)=3u;WORD_AT(ED+0x280u)=3u;  /* same colour as the old tail */
    CHECK(level_of(2)==0u);
    /* back to: 0 Robot x1 (colour 0), 1 Sniper x8 (colour 2), 2 Solo type 5 */
    make_ai(0,1,"Robot");make_ai(1,1,"Sniper");WORD_AT(E(1)+0x60u)=2u;make_ai(2,5,"Solo");WORD_AT(E(2)+0x60u)=3u;
    memset(bos.level,0,sizeof bos.level);
    click(0);click(1);click(1);click(1);click(1);
    CHECK(level_of(0)==1u && level_of(1)==4u && level_of(2)==0u);

    /* ---- colour change: the slot no longer belongs to that entry ---- */
    WORD_AT(E(1)+0x60u)=9u;
    CHECK(level_of(1)==0u);
    WORD_AT(E(1)+0x60u)=2u;
    click(1);click(1);click(1);click(1);CHECK(level_of(1)==4u);

    /* ---- health display row: only while Boss is not Off; toggles in place ---- */
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==2 && rows[1].id==0x424F5302u && rows[1].cb==0x0031CAC0u && rows[1].text!=rows[0].text);
    CHECK(!strcmp(display_label(),"Health display: Boss bar") && bos_get_sub(ED,0)==0u);
    {
        uint32_t label=rows[1].text;
        click_display(0);
        CHECK(orig_calls==0 && bos_get_sub(ED,0)==1u && bos_get(ED,0)==1u);   /* stays open, level kept */
        CHECK(!strcmp(text_of(label),"Health display: Sub-boss bar"));       /* rewritten in place */
        click_display(0);
        CHECK(orig_calls==0 && bos_get_sub(ED,0)==0u && !strcmp(text_of(label),"Health display: Boss bar"));
        click_display(1);
        CHECK(bos_get_sub(ED,1)==1u);
        rows_for(1,0x0031D083u,0x0031CAC0u);
        CHECK(!strcmp(display_label(),"Health display: Sub-boss bar") && !strcmp(row_label(),"Boss: health x8"));
        click(1);                                                   /* x8 -> Off: the choice goes too */
        CHECK(bos_get(ED,1)==0u && bos_get_sub(ED,1)==0u && !strcmp(text_of(label),"Health display: (Boss off)"));
        rows_for(1,0x0031D083u,0x0031CAC0u);
        CHECK(nrows==1);                                            /* no display row while Off */
        click_display(1);
        CHECK(orig_calls==0 && bos_get(ED,1)==0u && bos.sub[1]==0u);   /* nothing while Off */
        click(1);click(1);click(1);click(1);
        CHECK(level_of(1)==4u && bos_get_sub(ED,1)==0u && !strcmp(display_label(),"Health display: Boss bar"));
        click_display(1);
        click_display(2);CHECK(orig_calls==1);                      /* type 5: the stock close */
        WORD_AT(E(1)+0x60u)=9u;CHECK(bos_get_sub(ED,1)==0u);WORD_AT(E(1)+0x60u)=2u;   /* colour check drops it */
        click(1);click(1);click(1);click(1);click_display(1);
        CHECK(level_of(1)==4u && bos_get_sub(ED,1)==1u);
    }

    /* ---- compile: +0x29 = 0xB0 | sub 4 | (level - 1); health 5..8 -> +0x30 = 4, +0x21 = 0xA0 | 1..4 ---- */
    WORD_AT(E(0)+0x4Cu)=7u;WORD_AT(E(1)+0x4Cu)=2u;WORD_AT(E(2)+0x4Cu)=12u;   /* Titan, Normal, garbage */
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(saved_flag(0)==0xB0u && saved_flag(1)==0xB7u && saved_flag(2)==0u);
    CHECK(byte_at(sai_data(0)+0x28u)==1u && byte_at(sai_data(0)+0x2Au)==7u);  /* neighbours kept */
    CHECK(byte_at(sai_data(0)+0x30u)==4u && byte_at(sai_data(0)+0x21u)==0xA3u);
    CHECK(byte_at(sai_data(1)+0x30u)==2u && byte_at(sai_data(1)+0x21u)==0u);
    CHECK(byte_at(sai_data(2)+0x30u)==4u && byte_at(sai_data(2)+0x21u)==0u);  /* stock-safe */
    CHECK(byte_at(sai_data(0)+0x20u)==1u && byte_at(sai_data(0)+0x31u)==0x5Au && byte_at(sai_data(1)+0x22u)==1u);
    WORD_AT(E(0)+0x4Cu)=4u;
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(byte_at(sai_data(0)+0x30u)==4u && byte_at(sai_data(0)+0x21u)==0u);  /* Tough Guy itself */
    WORD_AT(E(0)+0x4Cu)=8u;WORD_AT(E(2)+0x4Cu)=0u;
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(byte_at(sai_data(0)+0x30u)==4u && byte_at(sai_data(0)+0x21u)==0xA4u);  /* Colossus */

    /* ---- reset + load restore them; bad values are ignored ---- */
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    CHECK(WORD_AT(ED+0x280u)==0 && bos.level[0]==0 && bos.level[1]==0);
    set_saved_flag(2,0x77u);
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(WORD_AT(ED+0x280u)==3u);
    CHECK(level_of(0)==1u && level_of(1)==4u && level_of(2)==0u);
    CHECK(bos_get_sub(ED,0)==0u && bos_get_sub(ED,1)==1u);        /* the Sub-boss bar is loaded */
    CHECK(bos.bad_flags==1u);
    CHECK(WORD_AT(E(0)+0x4Cu)==8u && WORD_AT(E(1)+0x4Cu)==2u && WORD_AT(E(2)+0x4Cu)==0u);
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    load_skew=2;                                                    /* entry 1 holds the wrong index */
    *(uint8_t *)((uintptr_t)sai_data(1)+0x21u+g_xbox_mem_offset)=0xA1u;
    *(uint8_t *)((uintptr_t)sai_data(1)+0x30u+g_xbox_mem_offset)=4u;
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    load_skew=0;
    CHECK(level_of(0)==1u && level_of(1)==0u);
    CHECK(WORD_AT(E(0)+0x4Cu)==8u && WORD_AT(E(1)+0x4Cu)==4u);    /* skewed: stays Tough Guy */
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    *(uint8_t *)((uintptr_t)sai_data(0)+0x30u+g_xbox_mem_offset)=3u;   /* re-saved as High elsewhere */
    *(uint8_t *)((uintptr_t)sai_data(2)+0x21u+g_xbox_mem_offset)=0xA5u; /* out of range */
    *(uint8_t *)((uintptr_t)sai_data(2)+0x30u+g_xbox_mem_offset)=4u;
    i=bos.bad_extra;
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(WORD_AT(E(0)+0x4Cu)==3u && WORD_AT(E(2)+0x4Cu)==4u && bos.bad_extra==i+2u);
    *(uint8_t *)((uintptr_t)sai_data(0)+0x30u+g_xbox_mem_offset)=4u;
    *(uint8_t *)((uintptr_t)sai_data(2)+0x21u+g_xbox_mem_offset)=0u;
    *(uint8_t *)((uintptr_t)sai_data(2)+0x30u+g_xbox_mem_offset)=0u;
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(level_of(0)==1u && level_of(1)==4u && WORD_AT(E(0)+0x4Cu)==8u && WORD_AT(E(1)+0x4Cu)==5u);
    CHECK(bos_get_sub(ED,1)==1u);
    set_saved_flag(1,0xBFu);                                        /* bit 3: unknown, the whole flag ignored */
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(level_of(1)==0u && bos.sub[1]==0u);
    set_saved_flag(1,0xB7u);
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(level_of(1)==4u && bos_get_sub(ED,1)==1u);
    /* off: the compile leaves the zero */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(saved_flag(0)==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- play ---- */
    WORD_AT(0x0079094Cu)=100u;
    *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=10u;
    WORD_AT(0x007B0C7Cu)=PLAYER;WORD_AT(PLAYER+4u)=0;WORD_AT(PLAYER+0x14u)=ENT(0);
    play_container();
    make_actor(0,0,0.0f,1.0f);                                      /* the player (human), even with a boss id */
    make_actor(1,0,10.0f,1.0f);                                     /* Big Robot x4 */
    make_actor(2,3,5.0f,1.0f);                                      /* unnamed x1 */
    make_actor(3,1,2.0f,1.0f);                                      /* Guard: no boss */
    make_actor(4,2,1.0f,1.0f);                                      /* bad flag: no boss */
    list_set(5);
    HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    CHECK(bos_play.active && bos_play.level[0]==3u && bos_play.level[3]==1u && !bos_play.level[1] && !bos_play.level[2]);
    CHECK(bos_play.level[8]==2u && bos_play.sub[8]==1u && !bos_play.sub[0] && !bos_play.sub[3] && bos_play.subs==1u);
    CHECK(meters_frame()==0);                                       /* no Sub-boss AI live */

    CHECK(hud_frame(HUD0));
    CHECK(bar.edi==ENT(2) && bar.esi==HUD0 && !strcmp(bar.label,"Boss") && bar.frac==0);
    CHECK(bar.esp<ESP0 && bar.out<ESP0 && bar.out>=bar.esp);        /* out slot on the guest stack */
    CHECK(WORD_AT(HUD0+8u)&1u);                                     /* shown like the stock bar */
    CHECK(nlog==2 && blog[1].edi==ENT(1) && !strcmp(blog[1].label,"Big Robot") && blog[1].text!=bar.text);
    CHECK(margin_ok(0,0,57) && margin_ok(1,1,57));                  /* stacked upward */
    CHECK(blog[0].smooth[0]==0 && blog[1].smooth[1]==0);            /* fresh state, not the stock bar's */
    CHECK(FLOAT_AT(ENT(1)+0x1D8u)==4.0f && FLOAT_AT(ENT(1)+0x1CCu)==4.0f);   /* x4 */
    CHECK(FLOAT_AT(ENT(2)+0x1D8u)==1.0f);                           /* x1 unchanged */
    CHECK(FLOAT_AT(ENT(3)+0x1D8u)==1.0f && FLOAT_AT(ENT(4)+0x1D8u)==1.0f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1D8u)==4.0f);       /* scaled once */
    CHECK(nlog==2 && blog[0].smooth[0]==ENT(2) && blog[1].smooth[2]==ENT(1)+2u);   /* each its own state */
    setf(ENT(1)+0x1CCu,3.0f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1CCu)==3.0f);       /* damage kept */

    setf(ENT(2)+0x1CCu,0.0f);                                       /* dead: the next one */
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1) && !strcmp(bar.label,"Big Robot"));
    CHECK(nlog==1 && margin_ok(0,0,57));
    setf(ENT(1)+0x64u,40.0f);                                       /* out of range */
    CHECK(!hud_frame(HUD0));
    CHECK(!(WORD_AT(HUD0+8u)&1u));
    setf(ENT(1)+0x64u,-29.0f);setf(ENT(1)+0x6Cu,3.0f);              /* range is 3D, either side */
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1));
    setf(ENT(1)+0x6Cu,10.0f);CHECK(!hud_frame(HUD0));
    setf(ENT(1)+0x6Cu,0.0f);setf(ENT(1)+0x64u,10.0f);

    /* respawn in the same pool slot: the game set the maximum again */
    setf(ENT(1)+0x1D8u,1.0f);setf(ENT(1)+0x1CCu,1.0f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1D8u)==4.0f && FLOAT_AT(ENT(1)+0x1CCu)==4.0f);
    setf(ENT(1)+0x1D8u,1.0f);setf(ENT(1)+0x1CCu,0.5f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1CCu)==2.0f && FLOAT_AT(ENT(1)+0x1D8u)==4.0f);
    /* another record in that slot (not a boss) is left alone */
    WORD_AT(REC(1)+8u)=R+1u+1u;setf(ENT(1)+0x1D8u,1.0f);setf(ENT(1)+0x1CCu,1.0f);
    CHECK(!hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1D8u)==1.0f);
    WORD_AT(REC(1)+8u)=R+0u+1u;
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1D8u)==4.0f);

    /* two bosses: the nearer one first */
    setf(ENT(2)+0x1CCu,1.0f);
    CHECK(hud_frame(HUD0) && bar.edi==ENT(2) && nlog==2 && blog[1].edi==ENT(1));
    setf(ENT(2)+0x64u,12.0f);
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1) && nlog==2 && blog[1].edi==ENT(2));
    setf(ENT(2)+0x64u,5.0f);

    /* a Boss AI whose bar is another entity (pc_mapmaker_khallos.inc: the chair he rides) */
    make_actor(5,0,30.0f,1.0f);list_set(6);                         /* "Big Robot" x4, himself out of range */
    setf(ENT(40)+0x64u,8.0f);setf(ENT(40)+0x1D8u,60.0f);setf(ENT(40)+0x1CCu,30.0f);
    proxy_from=ENT(5);proxy_to=ENT(40);bos_bar_proxy=test_proxy;
    CHECK(hud_frame(HUD0) && nlog==3 && blog[0].edi==ENT(2) && blog[1].edi==ENT(40) && blog[2].edi==ENT(1));
    CHECK(!strcmp(blog[1].label,"Big Robot"));                      /* his name */
    CHECK(FLOAT_AT(ENT(40)+0x1D8u)==240.0f && FLOAT_AT(ENT(40)+0x1CCu)==120.0f && FLOAT_AT(ENT(5)+0x1D8u)==1.0f);   /* the chair x4 */
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(40)+0x1D8u)==240.0f);     /* once */
    setf(ENT(40)+0x1CCu,-1.0f);                                     /* the chair is down: no bar */
    CHECK(hud_frame(HUD0) && nlog==2 && blog[1].edi==ENT(1));
    setf(ENT(40)+0x1CCu,10.0f);setf(ENT(40)+0x64u,50.0f);           /* range from the chair */
    CHECK(hud_frame(HUD0) && nlog==2);
    proxy_to=0;setf(ENT(5)+0x64u,8.0f);                             /* none: his own */
    CHECK(hud_frame(HUD0) && nlog==3 && blog[1].edi==ENT(5));
    bos_bar_proxy=0;list_set(5);

    /* a stock bar keeps the bottom slot: ours above it */
    stock_bar=1;
    CHECK(hud_frame(HUD0) && nlog==2 && bar.edi==ENT(2) && margin_ok(0,1,57) && margin_ok(1,2,57));
    CHECK(WORD_AT(HUD0+8u)&1u);
    stock_bar=0;
    /* wrong HUD block, mode, scene, player, not drawn */
    CHECK(!hud_frame(HUD0+0x177Cu));
    WORD_AT(PLAYER+4u)=1u;CHECK(hud_frame(HUD0+0x177Cu));WORD_AT(PLAYER+4u)=0;
    WORD_AT(PLAYER+4u)=7u;CHECK(!hud_frame(HUD0+7u*0x177Cu));WORD_AT(PLAYER+4u)=0;
    *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=3u;CHECK(!hud_frame(HUD0));
    *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=10u;
    WORD_AT(0x0079094Cu)=1u;CHECK(!hud_frame(HUD0));WORD_AT(0x0079094Cu)=100u;
    bar_draw=0;CHECK(hud_frame(HUD0) && !(WORD_AT(HUD0+8u)&1u) && margin_ok(1,0,57));bar_draw=1;   /* not drawn: no slot used */
    WORD_AT(ENT(1)+0x7Cu)=0x1234u;WORD_AT(ENT(2)+0x7Cu)=0x1234u;   /* entity no longer this actor */
    CHECK(!hud_frame(HUD0));
    WORD_AT(ENT(1)+0x7Cu)=A(1);WORD_AT(ENT(2)+0x7Cu)=A(2);
    list_set(2);CHECK(hud_frame(HUD0) && bar.edi==ENT(1) && nlog==1);   /* only the listed */
    list_set(5);

    /* ---- stronger health and more bosses ---- */
    {
        unsigned bad=bos.bad_extra;
        make_actor(5,4,0.5f,1.0f);                                  /* Titan x8, no boss: no bar */
        make_actor(6,5,3.0f,1.0f);                                  /* Duo: boss x2 and Brute: x4 */
        make_actor(7,6,4.0f,1.0f);                                  /* extra with health High: ignored */
        make_actor(8,7,4.5f,1.0f);                                  /* bad mark: ignored */
        list_set(9);
        HOOK(pc_mapmaker_bosses_level,fake_plain,0);
        CHECK(bos_play.extra[4]==3u && !bos_play.level[4] && bos_play.extra[5]==1u && bos_play.level[5]==2u);
        CHECK(!bos_play.extra[6] && !bos_play.extra[7] && bos.bad_extra==bad+2u);
        FLOAT_AT(0x004E68F8u)=2.0f;                                 /* HUD scale: twice the step */
        CHECK(hud_frame(HUD0) && nlog==3);
        CHECK(blog[0].edi==ENT(6) && !strcmp(blog[0].label,"Duo") && blog[1].edi==ENT(2) && blog[2].edi==ENT(1));
        CHECK(margin_ok(0,0,114) && margin_ok(1,1,114) && margin_ok(2,2,114));
        CHECK(FLOAT_AT(ENT(5)+0x1D8u)==8.0f && FLOAT_AT(ENT(5)+0x1CCu)==8.0f);
        CHECK(FLOAT_AT(ENT(6)+0x1D8u)==4.0f && FLOAT_AT(ENT(7)+0x1D8u)==1.0f && FLOAT_AT(ENT(8)+0x1D8u)==1.0f);
        CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(5)+0x1D8u)==8.0f && FLOAT_AT(ENT(6)+0x1D8u)==4.0f);   /* once */
        make_actor(9,0,7.0f,1.0f);make_actor(10,3,8.0f,1.0f);       /* five bosses in range */
        list_set(11);
        CHECK(hud_frame(HUD0) && nlog==4);                          /* the cap: the farthest is left out */
        CHECK(blog[0].edi==ENT(6) && blog[1].edi==ENT(2) && blog[2].edi==ENT(9) && blog[3].edi==ENT(10));
        CHECK(margin_ok(3,3,114) && FLOAT_AT(ENT(9)+0x1D8u)==4.0f);
        CHECK(hud_frame(HUD0) && blog[2].smooth[0]==ENT(9) && blog[3].smooth[0]==ENT(10));
        stock_bar=1;
        CHECK(hud_frame(HUD0) && nlog==3 && blog[2].edi==ENT(9) && margin_ok(0,1,114) && margin_ok(2,3,114));
        WORD_AT(0x00790950u)=2u;                                    /* split screen: two bars */
        CHECK(hud_frame(HUD0) && nlog==1 && margin_ok(0,1,114));
        stock_bar=0;
        CHECK(hud_frame(HUD0) && nlog==2 && blog[1].edi==ENT(2));
        FLOAT_AT(0x004E68F8u)=1000.0f;WORD_AT(0x00790950u)=1u;      /* nonsense scale: 1 */
        CHECK(hud_frame(HUD0) && margin_ok(1,1,57));
        FLOAT_AT(0x004E68F8u)=0.0f;
        list_set(5);
        HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    }

    /* ---- health drain over frames (sub_00139740's rule, fade per tick) ---- */
    {
        unsigned n;
        float f;
        FLOAT_AT(0x00475D10u)=2.0f;bar_model=1;
        for(n=0;n<3u;n++) WORD_AT(HUD0+0x171Cu+n*8u)=0;           /* the live (stock) bar state */
        FLOAT_AT(HUD0+0x2Cu)=100.0f;
        make_actor(1,0,10.0f,1.0f);list_set(2);                     /* one boss: Big Robot x4 */
        HOOK(pc_mapmaker_bosses_level,fake_plain,0);
        drain_frame(HUD0);
        CHECK(nlog==1 && FLOAT_AT(ENT(1)+0x1D8u)==4.0f && near(fill_of(ENT(1)),1.0f));
        setf(ENT(1)+0x1CCu,3.0f);                                   /* hit: 75 % */
        drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),1.0f));                          /* the drain starts full */
        for(n=0;n<15u;n++) drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),0.875f));                        /* half-way after 0.25 s */
        for(n=0;n<16u;n++) drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),0.75f));                         /* at its health after 0.5 s */
        setf(ENT(1)+0x1CCu,1.0f);                                   /* 25 % */
        f=2.0f;
        for(n=0;n<40u;n++) {
            drain_frame(HUD0);
            CHECK(fill_of(ENT(1))<=f+0.0001f);f=fill_of(ENT(1));    /* never grows back */
        }
        CHECK(near(f,0.25f));
        /* three bosses, each its own drain; a stock bar's live state ticks on its own */
        make_actor(1,0,10.0f,1.0f);make_actor(2,3,5.0f,1.0f);make_actor(3,5,6.0f,1.0f);list_set(4);
        HOOK(pc_mapmaker_bosses_level,fake_plain,0);
        stock_bar=1;FLOAT_AT(HUD0+0x172Cu)=1.0f;
        drain_frame(HUD0);
        CHECK(nlog==3 && near(fill_of(ENT(1)),1.0f) && near(fill_of(ENT(2)),1.0f) && near(fill_of(ENT(3)),1.0f));
        CHECK(FLOAT_AT(ENT(3)+0x1D8u)==4.0f && FLOAT_AT(ENT(2)+0x1D8u)==1.0f);
        setf(ENT(1)+0x1CCu,2.0f);setf(ENT(2)+0x1CCu,0.75f);         /* 50 %, 75 %, ENT(3) untouched */
        drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),1.0f) && near(fill_of(ENT(2)),1.0f) && near(fill_of(ENT(3)),1.0f));
        for(n=0;n<15u;n++) drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),0.75f) && near(fill_of(ENT(2)),0.875f) && near(fill_of(ENT(3)),1.0f));
        CHECK(near(FLOAT_AT(HUD0+0x172Cu),1.0f-17.0f*2.0f/60.0f));   /* live fade: the stock ticks only */
        setf(ENT(3)+0x1CCu,1.0f);                                   /* the third one hit later */
        for(n=0;n<40u;n++) drain_frame(HUD0);
        CHECK(near(fill_of(ENT(1)),0.5f) && near(fill_of(ENT(2)),0.75f) && near(fill_of(ENT(3)),0.25f));
        stock_bar=0;bar_model=0;
        make_actor(1,0,10.0f,1.0f);make_actor(2,3,5.0f,1.0f);make_actor(3,1,2.0f,1.0f);list_set(5);
        HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    }

    /* ---- Sub-boss: overhead meter instead of a bottom bar ---- */
    {
        make_actor(5,8,3.0f,1.0f);meter_model(5);list_set(6);
        meter_model(1);meter_model(2);                              /* bottom-bar bosses: never a meter */
        CHECK(hud_frame(HUD0));
        for(i=0;i<nlog && i<8;i++) CHECK(blog[i].edi!=ENT(5));
        CHECK(nlog==2 && FLOAT_AT(ENT(5)+0x1D8u)==2.0f);           /* health x2 still applies */
        CHECK(meters_frame()==1 && meter.a==A(5) && meter.b==0 && meter.c==0x7Fu && meter.d==0x6Eu);
        CHECK(meter.esp==ESP0);                                     /* pushed below the return address */
        WORD_AT(ENT(5)+0x268u)=0x1234u;CHECK(meters_frame()==0);  /* a record: the stock loop draws it */
        WORD_AT(ENT(5)+0x268u)=0;
        setf(ENT(5)+0x1CCu,0.0f);CHECK(meters_frame()==0);setf(ENT(5)+0x1CCu,2.0f);   /* dead */
        WORD_AT(PLAYER+0x14u)=ENT(5);CHECK(meters_frame()==0);WORD_AT(PLAYER+0x14u)=ENT(0);
        WORD_AT(ENT(5)+4u)=0;CHECK(meters_frame()==0);meter_model(5);   /* no model instance */
        WORD_AT(PLAYER+4u)=0xFFFFFFFFu;CHECK(meters_frame()==0);WORD_AT(PLAYER+4u)=0;
        WORD_AT(0x00790950u)=6u;CHECK(meters_frame()==0);WORD_AT(0x00790950u)=1u;   /* human slots skipped */
        *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=3u;CHECK(meters_frame()==0);
        *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=10u;
        WORD_AT(0x0079094Cu)=1u;CHECK(meters_frame()==0);WORD_AT(0x0079094Cu)=100u;
        CHECK(meters_frame()==1);
        list_set(5);CHECK(meters_frame()==0);                       /* gone from the list */
    }

    /* range switch */
    setf(ENT(1)+0x64u,40.0f);setf(ENT(2)+0x1CCu,0.0f);
    set_env("TSFP_MAPMAKER_BOSS_RANGE","50");set_env("TSFP_MAPMAKER_LIMITS","1");
    HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1));
    set_env("TSFP_MAPMAKER_BOSS_RANGE","0.5");set_env("TSFP_MAPMAKER_LIMITS","1");  /* invalid: 30 */
    HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    CHECK(!hud_frame(HUD0));
    setf(ENT(1)+0x64u,10.0f);CHECK(hud_frame(HUD0) && bar.edi==ENT(1));
    set_env("TSFP_MAPMAKER_BOSS_RANGE",NULL);set_env("TSFP_MAPMAKER_LIMITS","1");
    setf(ENT(1)+0x64u,10.0f);setf(ENT(2)+0x1CCu,1.0f);

    /* switch off at level start: nothing in play */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    HOOK(pc_mapmaker_bosses_level,fake_plain,0);
    CHECK(!bos_play.active && !hud_frame(HUD0));
    list_set(6);CHECK(meters_frame()==0);list_set(5);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x00784014u)=0;HOOK(pc_mapmaker_bosses_level,fake_plain,0);   /* no container */
    CHECK(!bos_play.active && !hud_frame(HUD0));

    CHECK(bos_probe.read && !bos_probe.on && bos_probe.lines==0u);   /* off unless asked */

    /* ---- creature probe: read-only ---- */
    {
        static uint32_t snapshot[0x01000000/4];
        FILE *f;
        char line[512];
        unsigned chars=0,levels=0,models=0;
        set_env("TSFP_MAPMAKER_CREATURE_PROBE","1");
        set_env("TSFP_MAPMAKER_CREATURE_PROBE_LOG","creature-probe.log");
        remove("creature-probe.log");
        bos_probe.read=0;
        WORD_AT(0x0079094Cu)=1u;WORD_AT(0x007356D8u)=POOL;
        WORD_AT(0x00791D4Cu)=RESTAB;
        WORD_AT(0x0053A3CCu+0x28u*0x30u+4u)=0x5E9u;                 /* char 0x28 model */
        *(uint16_t *)((uintptr_t)0x008CA000u+0x5E9u*2u+g_xbox_mem_offset)=3u;
        WORD_AT(RESTAB+3u*0x30u)=0x20000u;WORD_AT(RESTAB+3u*0x30u+0x28u)=0x2000000u;
        g_ebx=HUD0;
        memcpy(snapshot,memory,sizeof memory);
        HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
        HOOK(pc_mapmaker_bosses_hud,fake_hud,0);                    /* nothing new */
        WORD_AT(HUD0+8u)=snapshot[(HUD0+8u)/4u];                    /* the fake original's own write */
        CHECK(!memcmp(snapshot,memory,sizeof memory));
        WORD_AT(0x007356D8u)=POOL+0x10u;                            /* next level */
        HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
        if(bos_probe.out!=stderr) fclose(bos_probe.out);
        bos_probe.out=stderr;
        f=fopen("creature-probe.log","r");
        CHECK(f!=NULL);
        while(f && fgets(line,sizeof line,f)) {
            if(strstr(line,"] char #")) chars++;
            if(strstr(line,"] level: scene 1 ")) levels++;
            if(strstr(line,"char 0x28 model 0x5E9 slot 3 type 0x20000") && strstr(line,"flags 0x2000000")) models++;
        }
        if(f) fclose(f);
        CHECK(chars==10u);                                          /* 5 per level, once each */
        CHECK(levels==2u);
        CHECK(models>=2u);
        set_env("TSFP_MAPMAKER_CREATURE_PROBE",NULL);
        bos_probe.read=0;
        WORD_AT(0x007356D8u)=POOL+0x20u;
        memcpy(snapshot,memory,sizeof memory);
        HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
        WORD_AT(HUD0+8u)=snapshot[(HUD0+8u)/4u];
        CHECK(!memcmp(snapshot,memory,sizeof memory) && bos_probe.lines>0u);
    }

    /* ---- duplicate / linked copy: the copy gets the source's boss settings ---- */
    WORD_AT(0x0079094Cu)=102u;
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    make_ai(0,1,"Titan");make_ai(1,1,"Guard");WORD_AT(ED+0x280u)=2u;
    WORD_AT(E(0)+0x28u)=0x15u;WORD_AT(E(0)+0x4Cu)=7u;               /* Health: Titan */
    click(0);click(0);click(0);click_display(0);                    /* x4, Sub-boss bar */
    CHECK(level_of(0)==3u && bos_get_sub(ED,0)==1u);
    copy_row(0,0x41540001u,1);                                      /* Duplicate Story AI */
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==3u);
    CHECK(level_of(2)==3u && bos_get_sub(ED,2)==1u && WORD_AT(E(2)+0x4Cu)==7u);
    copy_row(0,0x41540002u,2);                                      /* Add linked copy */
    CHECK(WORD_AT(ED+0x280u)==4u && WORD_AT(E(3)+0x50u)==1u);
    CHECK(level_of(3)==3u && bos_get_sub(ED,3)==1u && WORD_AT(E(3)+0x4Cu)==7u);
    click(3);                                                       /* later edits stay per AI */
    CHECK(level_of(3)==4u && level_of(0)==3u && level_of(2)==3u);
    copy_row(1,0x41540001u,1);                                      /* a non-boss source: Off */
    CHECK(WORD_AT(ED+0x280u)==5u && level_of(4)==0u);
    copy_row(0,0x41540001u,3);                                      /* not a copy of the source */
    CHECK(WORD_AT(ED+0x280u)==6u && level_of(5)==0u);
    copy_row(0,0x41540001u,0);                                      /* refused: nothing added */
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==6u && bos.level[6]==0u);
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(saved_flag(2)==0xB6u && byte_at(sai_data(2)+0x21u)==0xA3u && byte_at(sai_data(2)+0x30u)==4u);
    CHECK(saved_flag(3)==0xB7u && byte_at(sai_data(3)+0x21u)==0xA3u && saved_flag(4)==0u);
    HOOK(pc_mapmaker_bosses_delete,fake_delete,0);                  /* delete the source */
    CHECK(level_of(1)==3u && bos_get_sub(ED,1)==1u && level_of(2)==4u && level_of(0)==0u);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    copy_row(0,0x41540001u,1);                                      /* off: the original only */
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==6u && bos.level[5]==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    if(esp_errors) { fprintf(stderr,"%d esp errors\n",esp_errors);return 1; }
    if(failures) return 1;
    puts("mapmaker bosses ok");
    return 0;
}
