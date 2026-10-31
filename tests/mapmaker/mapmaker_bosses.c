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
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p=heap_next;(void)alignment;heap_next+=(bytes+15u)&~15u;return p; }

static unsigned nrows, bars, bar_draw=1;
static struct { uint32_t text, id, cb; } rows[16];
static struct { uint32_t esi, edi, text, frac, out, esp; char label[32]; } bar;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00079D60) { if(nrows<16) { rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].cb=x[4]; } nrows++; }
    else if(f==sub_00139740) {                    /* esi = HUD block, edi = entity; (text, fraction, int *) */
        bars++;bar.esi=g_esi;bar.edi=g_edi;bar.text=x[0];bar.frac=x[1];bar.out=x[2];bar.esp=g_esp;
        memcpy(bar.label,(const char *)((uintptr_t)x[0]+g_xbox_mem_offset),31);bar.label[31]=0;
        if(bar_draw && x[2]>=0x10000u && x[2]<sizeof memory) WORD_AT(x[2])=1;
        g_esi=g_edi=0xDEADu;                      /* clobbers; the hook must restore */
    }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_bosses.inc"

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
static const char *row_label(void) { return nrows==1?text_of(rows[0].text):""; }
static void click(uint32_t s)                    /* the boss row's callback */
{
    select_ai(s);press(0x424F5301u);orig_calls=0;
    HOOK(pc_mapmaker_bosses_close,fake_plain,MENU);
}
static unsigned level_of(uint32_t s) { rows_for(s,0x0031D083u,0x0031CAC0u); return bos_get(ED,s); }

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
    /* chunks: index 0 boss x4 "Big Robot", 1 none, 3 x1 unnamed, 2 bad flag */
    uint32_t p=CONT+8u,k;
    static const uint32_t idx[4]={0,1,3,2};
    static const uint8_t flag[4]={0xB2,0,0xB0,0x77};
    static const char *const name[4]={"Big Robot","Guard","","Odd"};
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x2000);
    WORD_AT(CONT)=0x00C77667u;
    for(k=0;k<4;k++) {
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=0x50u;
        snprintf((char *)((uintptr_t)p+8u+g_xbox_mem_offset),32,"%s",name[k]);
        WORD_AT(p+8u+0x20u)=1u|(idx[k]<<16);
        *(uint8_t *)((uintptr_t)p+8u+0x29u+g_xbox_mem_offset)=flag[k];
        p+=0x58u;
    }
    WORD_AT(p)=0x00C7766Au;
    WORD_AT(0x00784014u)=CONT;
}
static int hud_frame(uint32_t hud)              /* returns 1 when our bar was drawn */
{
    unsigned before=bars;
    g_ebx=hud;g_esi=0x51u;g_edi=0xD1u;orig_calls=0;
    HOOK(pc_mapmaker_bosses_hud,fake_hud,0);
    CHECK(orig_calls==1);
    CHECK(g_ebx==hud && g_esi==0x51u && g_edi==0xD1u);
    return bars!=before;
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

    set_env("TSFP_MAPMAKER_LIMITS","1");

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

    /* ---- delete shifts the later settings ---- */
    HOOK(pc_mapmaker_bosses_delete,fake_delete,1);
    CHECK(WORD_AT(ED+0x280u)==3u);
    WORD_AT(E(1)+0x60u)=2u;                                        /* the moved entry keeps its colour */
    CHECK(level_of(0)==1u && level_of(1)==4u && level_of(2)==0u);
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

    /* ---- compile: +0x29 = 0xB0 | (level - 1) ---- */
    HOOK(pc_mapmaker_bosses_compile,fake_compile,CONT);
    CHECK(saved_flag(0)==0xB0u && saved_flag(1)==0xB3u && saved_flag(2)==0u);
    CHECK(byte_at(sai_data(0)+0x28u)==1u && byte_at(sai_data(0)+0x2Au)==7u);  /* neighbours kept */

    /* ---- reset + load restore them; bad values are ignored ---- */
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    CHECK(WORD_AT(ED+0x280u)==0 && bos.level[0]==0 && bos.level[1]==0);
    set_saved_flag(2,0x77u);
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    CHECK(WORD_AT(ED+0x280u)==3u);
    CHECK(level_of(0)==1u && level_of(1)==4u && level_of(2)==0u);
    CHECK(bos.bad_flags==1u);
    HOOK(pc_mapmaker_bosses_reset,fake_reset,0);
    load_skew=2;                                                    /* entry 1 holds the wrong index */
    HOOK(pc_mapmaker_bosses_load,fake_load,CONT);
    load_skew=0;
    CHECK(level_of(0)==1u && level_of(1)==0u);
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

    CHECK(hud_frame(HUD0));
    CHECK(bar.edi==ENT(2) && bar.esi==HUD0 && !strcmp(bar.label,"Boss") && bar.frac==0);
    CHECK(bar.esp<ESP0 && bar.out<ESP0 && bar.out>=bar.esp);        /* out slot on the guest stack */
    CHECK(WORD_AT(HUD0+8u)&1u);                                     /* shown like the stock bar */
    CHECK(FLOAT_AT(ENT(1)+0x1D8u)==4.0f && FLOAT_AT(ENT(1)+0x1CCu)==4.0f);   /* x4 */
    CHECK(FLOAT_AT(ENT(2)+0x1D8u)==1.0f);                           /* x1 unchanged */
    CHECK(FLOAT_AT(ENT(3)+0x1D8u)==1.0f && FLOAT_AT(ENT(4)+0x1D8u)==1.0f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1D8u)==4.0f);       /* scaled once */
    setf(ENT(1)+0x1CCu,3.0f);
    CHECK(hud_frame(HUD0) && FLOAT_AT(ENT(1)+0x1CCu)==3.0f);       /* damage kept */

    setf(ENT(2)+0x1CCu,0.0f);                                       /* dead: the next one */
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1) && !strcmp(bar.label,"Big Robot"));
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

    /* two bosses: the nearer one */
    setf(ENT(2)+0x1CCu,1.0f);
    CHECK(hud_frame(HUD0) && bar.edi==ENT(2));
    setf(ENT(2)+0x64u,12.0f);
    CHECK(hud_frame(HUD0) && bar.edi==ENT(1));
    setf(ENT(2)+0x64u,5.0f);

    /* the stock bar wins; wrong HUD block, mode, scene, player, not drawn */
    stock_bar=1;CHECK(!hud_frame(HUD0));stock_bar=0;
    CHECK(!hud_frame(HUD0+0x177Cu));
    WORD_AT(PLAYER+4u)=1u;CHECK(hud_frame(HUD0+0x177Cu));WORD_AT(PLAYER+4u)=0;
    WORD_AT(PLAYER+4u)=7u;CHECK(!hud_frame(HUD0+7u*0x177Cu));WORD_AT(PLAYER+4u)=0;
    *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=3u;CHECK(!hud_frame(HUD0));
    *(uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)=10u;
    WORD_AT(0x0079094Cu)=1u;CHECK(!hud_frame(HUD0));WORD_AT(0x0079094Cu)=100u;
    bar_draw=0;CHECK(hud_frame(HUD0) && !(WORD_AT(HUD0+8u)&1u));bar_draw=1;
    WORD_AT(ENT(1)+0x7Cu)=0x1234u;WORD_AT(ENT(2)+0x7Cu)=0x1234u;   /* entity no longer this actor */
    CHECK(!hud_frame(HUD0));
    WORD_AT(ENT(1)+0x7Cu)=A(1);WORD_AT(ENT(2)+0x7Cu)=A(2);
    list_set(2);CHECK(hud_frame(HUD0) && bar.edi==ENT(1));         /* only the listed */
    list_set(5);

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
        *(uint16_t *)((uintptr_t)0x0078B860u+0x5E9u*2u+g_xbox_mem_offset)=3u;
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

    if(esp_errors) { fprintf(stderr,"%d esp errors\n",esp_errors);return 1; }
    if(failures) return 1;
    puts("mapmaker bosses ok");
    return 0;
}
