/* Asset-free contract for src/interface/pc_mapmaker_aitemplates.inc: Story AI
 * duplicate / leader groups in the editor (rows, actions, numbered names,
 * group-logic toggle, propagation, promotion on unlink/delete, save and load
 * of the link, old template format) and "whole group" logic in play.
 * Fake guest memory; fake originals pop their return address like the lifted
 * callees, and every hook call checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

#define ED 0x00100000u
#define CONT 0x00200000u
#define HEAP 0x00300000u
#define MENU 0x00380000u
#define ACTORS 0x00400000u
#define ESP0 0x00600000u
#define R 10u
#define E(i) (ED+0x11950u+(uint32_t)(i)*0xE4u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=1; }
void sub_0031BE70(void) { g_ecx=2; }
void sub_00300670(void) { g_ecx=3; }
void sub_00079D60(void) { g_ecx=4; }
void sub_0007A270(void) { g_ecx=5; }
void sub_0003D7B0(void) { g_ecx=6; }
void sub_003C85D4(void) { g_ecx=7; }
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p=heap_next;(void)alignment;heap_next+=(bytes+15u)&~15u;return p; }

static int has_room=1, messages, name_counter;
static uint32_t room_bytes, last_sprintf_value, last_message_menu, last_message_text;
static unsigned nrows;
static struct { uint32_t text, id, cb; } rows[16];
static unsigned guest_calls;

static void fake_add(void)   /* sub_0031BE70 -> editor sai_add -> sub_0031BB60(0, 0) */
{
    uint32_t c=WORD_AT(ED+0x280u),e=E(c);
    if(c>=255u) return;
    memset((void *)((uintptr_t)e+g_xbox_mem_offset),0,0xE4);
    snprintf((char *)((uintptr_t)e+g_xbox_mem_offset),32,"Story AI %d",++name_counter);
    WORD_AT(e+0x20u)=1;WORD_AT(e+0x30u)=1;WORD_AT(e+0x38u)=1;WORD_AT(e+0x40u)=0xFFFFFFFFu;
    WORD_AT(e+0x44u)=2;WORD_AT(e+0x4Cu)=2;WORD_AT(e+0x60u)=c%50u;
    WORD_AT(ED+0x280u)=c+1u;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_0031BE70) fake_add();
    else if(f==sub_00300670) { room_bytes=x[0];r=(uint32_t)has_room; }
    else if(f==sub_0003D7B0) r=0x00700000u+x[0];
    else if(f==sub_003C85D4) last_sprintf_value=x[2];
    else if(f==sub_0007A270) { messages++;last_message_menu=x[0];last_message_text=x[1]; }
    else if(f==sub_00079D60) { if(nrows<16) { rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].cb=x[4]; } nrows++; }
    else {                                         /* a guest function: push args and a return address */
        GuestRegisters s;
        save_registers(&s);
        g_esp-=4u*(uint32_t)(n+1);
        WORD_AT(g_esp)=0xDEADBEEFu;
        for(i=0;i<n;i++) WORD_AT(g_esp+4u+4u*(uint32_t)i)=x[i];
        guest_calls++;
        f();
        r=g_eax;
        restore_registers(&s);
    }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_aitemplates.inc"

static int failures, esp_errors;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
/* call fn(orig) as the lifted caller does: return address at [esp], args above */
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
static const char *name_of(uint32_t i) { return (const char *)((uintptr_t)E(i)+g_xbox_mem_offset); }
static const char *text_of(uint32_t a) { return (const char *)((uintptr_t)a+g_xbox_mem_offset); }

/* ---- fake originals ------------------------------------------------------ */
static unsigned orig_calls;
static void fake_plain(void) { orig_calls++;g_esp+=4u; }
static void fake_close(void)                     /* sub_0031CAC0: stock point clamp */
{
    uint32_t s=WORD_AT(0x00762F94u)&0xFFu,e=E(s),t=WORD_AT(e+0x34u);
    orig_calls++;
    if(WORD_AT(e+0x58u)>1u && t!=1u && t!=3u) WORD_AT(e+0x58u)=1u;
    g_esp+=4u;
}
static void fake_compile(void)                   /* sub_003006C0: zero, one chunk per entry */
{
    uint32_t c=WORD_AT(g_esp+4u),n=WORD_AT(ED+0x280u),i,p;
    orig_calls++;
    memset((void *)((uintptr_t)c+g_xbox_mem_offset),0,0x2000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4u)=0x12Du;
    p=c+8u;
    WORD_AT(p)=0x00C77668u;WORD_AT(p+4u)=0x10u;p+=0x18u;      /* an unrelated chunk first */
    for(i=0;i<n;i++) {
        uint32_t sz=(WORD_AT(E(i)+0x58u)*4u+0x53u)&~0xFu;
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=sz;
        WORD_AT(p+8u+0x20u)=(WORD_AT(E(i)+0x34u)&0xFFu)|(i<<16);  /* +0x20 type, +0x22 index */
        p+=8u+sz;
    }
    WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=0;
    g_eax=c;g_esp+=4u;
}
static int load_append;
static uint32_t load_skew;                       /* entry index + 1 with a wrong +0x24 */
static void fake_load(void)                      /* sub_00301A90: reset, append, +0x50 untouched */
{
    uint32_t c=WORD_AT(g_esp+4u)+8u,n=load_append?WORD_AT(ED+0x280u):0u,first=n;
    orig_calls++;
    while(WORD_AT(c)!=0x00C7766Au) {
        if(WORD_AT(c)==0x00C7766Cu) {
            WORD_AT(E(n)+0x24u)=(uint32_t)(int32_t)(int16_t)(WORD_AT(c+8u+0x20u)>>16)+(load_skew && n==first+load_skew-1u?1u:0u);
            WORD_AT(E(n)+0x34u)=WORD_AT(c+8u+0x20u)&0xFFu;
            WORD_AT(E(n)+0x50u)=0xBADu;n++;
        }
        c+=8u+WORD_AT(c+4u);
    }
    WORD_AT(ED+0x280u)=n;
    g_esp+=4u;
}
static void fake_delete(void)                    /* sub_0031C8B0(i): memmove down */
{
    uint32_t i=WORD_AT(g_esp+4u),n=WORD_AT(ED+0x280u);
    orig_calls++;
    memmove((void *)((uintptr_t)E(i)+g_xbox_mem_offset),(void *)((uintptr_t)E(i+1u)+g_xbox_mem_offset),(n-i-1u)*0xE4u);
    WORD_AT(ED+0x280u)=n-1u;
    g_esp+=4u;
}
static uint32_t seen[16];
static unsigned nseen;
static void fake_event(void)                     /* sub_0033E560(actor): logs the record id */
{
    uint32_t actor=WORD_AT(g_esp+4u),rec=WORD_AT(WORD_AT(actor+0x7Cu)+0xBF0u);
    if(nseen<16) seen[nseen]=WORD_AT(rec+8u);
    nseen++;g_esp+=4u;
}
static void fake_remove(void) { if(nseen<16) seen[nseen]=WORD_AT(g_esp+4u); nseen++;g_esp+=4u; }
static uint32_t gate_open[16], found[16], now;
static void fake_gate(void)
{
    uint32_t id=WORD_AT(g_esp+4u);
    if(nseen<16) seen[nseen]=id;
    nseen++;g_eax=id-R<16u?gate_open[id-R]:1u;g_esp+=4u;
}
static void fake_alert(void)                     /* sub_00339600 state machine */
{
    uint32_t t=WORD_AT(g_esp+4u),id=WORD_AT(t+0x14u);
    if(nseen<16) seen[nseen]=id;
    nseen++;g_esp+=4u;g_eax=0;
    if(WORD_AT(t+0x18u)==7u && (int32_t)WORD_AT(t+0x1Cu)>=(int32_t)now) return;
    if(!(id-R<16u && found[id-R])) return;
    if(WORD_AT(t+0x18u)!=7u) { WORD_AT(t+0x18u)=7u;WORD_AT(t+0x1Cu)=now+5u;return; }
    WORD_AT(t+0x18u)=3u;WORD_AT(t+0x1Cu)=0;g_eax=1;
}

static void make_ai(uint32_t i,uint32_t type,uint32_t weapon,uint32_t points,const char *name)
{
    uint32_t e=E(i),k;
    memset((void *)((uintptr_t)e+g_xbox_mem_offset),0,0xE4);
    snprintf((char *)((uintptr_t)e+g_xbox_mem_offset),32,"%s",name);
    WORD_AT(e+0x20u)=1;WORD_AT(e+0x28u)=7u+i;WORD_AT(e+0x2Cu)=1;WORD_AT(e+0x30u)=1;
    WORD_AT(e+0x34u)=type;WORD_AT(e+0x38u)=3;WORD_AT(e+0x3Cu)=20;WORD_AT(e+0x44u)=2;
    WORD_AT(e+0x48u)=weapon;WORD_AT(e+0x4Cu)=1;WORD_AT(e+0x54u)=4u+i;WORD_AT(e+0x58u)=points;
    WORD_AT(e+0x5Cu)=1;WORD_AT(e+0x60u)=i;
    for(k=0;k<points;k++) { WORD_AT(e+0x64u+16u*k)=0x00500000u+0x100u*i+k;WORD_AT(e+0x68u+16u*k)=0x00010002u*(k+1u); }
}
static int same_points(uint32_t a,uint32_t b)
{
    return WORD_AT(E(a)+0x58u)==WORD_AT(E(b)+0x58u) &&
           !memcmp((void *)((uintptr_t)E(a)+0x64u+g_xbox_mem_offset),(void *)((uintptr_t)E(b)+0x64u+g_xbox_mem_offset),0x80);
}
/* data of the k-th Story AI chunk in the fake container */
static uint32_t sai_data(unsigned k)
{
    uint32_t c=CONT+8u;
    while(WORD_AT(c)!=0x00C7766Au) { if(WORD_AT(c)==0x00C7766Cu && !k--) return c+8u; c+=8u+WORD_AT(c+4u); }
    return 0;
}
static uint32_t saved_link(unsigned k) { return WORD_AT(sai_data(k)+0x30u)>>16; }
static void set_saved_link(unsigned k,uint32_t v) { uint32_t d=sai_data(k); WORD_AT(d+0x30u)=(WORD_AT(d+0x30u)&0xFFFFu)|(v<<16); }
static void select_ai(uint32_t i) { WORD_AT(0x00762F94u)=(WORD_AT(0x00762F94u)&~0xFFu)|i; }
static void press(uint32_t row) { WORD_AT(MENU+0x18Cu)=row; }
static void rows_for(uint32_t s,uint32_t ret,uint32_t cb)
{
    nrows=0;select_ai(s);
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=0x40000000u;WORD_AT(ESP0+0x24u)=cb;
    pc_mapmaker_aitemplates_rows(fake_plain);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"rows: esp %+d\n",(int)(g_esp-ESP0));esp_errors++; }
}

int main(void)
{
    uint32_t i;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(0x0079094Cu)=102u;

    /* ---- limits off: nothing changes ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);
    make_ai(0,1,3,2,"Guard");WORD_AT(ED+0x280u)=1;
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==0);
    press(0x41540001u);select_ai(0);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==1);

    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- rows ---- */
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==2 && rows[0].id==0x41540001u && rows[1].id==0x41540002u);
    CHECK(rows[0].cb==0x0031CAC0u && rows[1].cb==0x0031CAC0u);
    CHECK(!strcmp(text_of(rows[0].text),"Duplicate Story AI") && !strcmp(text_of(rows[1].text),"Add linked copy"));
    rows_for(0,0x0031D084u,0x0031CAC0u); CHECK(nrows==0);        /* another spacer call */
    rows_for(0,0x0031D083u,0x002FB3C0u); CHECK(nrows==0);        /* map path */
    WORD_AT(0x0079094Cu)=100u; rows_for(0,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);
    WORD_AT(0x0079094Cu)=102u;
    WORD_AT(E(0)+0x34u)=5u; rows_for(0,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);
    WORD_AT(E(0)+0x34u)=1u;
    rows_for(1,0x0031D083u,0x0031CAC0u); CHECK(nrows==0);        /* past the count */

    /* ---- duplicate ---- */
    press(0x41540001u);select_ai(0);orig_calls=0;name_counter=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==2);
    for(i=0x28u;i<0x60u;i+=4u) if(i!=0x50u) CHECK(WORD_AT(E(1)+i)==WORD_AT(E(0)+i));
    CHECK(same_points(0,1) && WORD_AT(E(1)+0x50u)==0 && WORD_AT(E(0)+0x50u)==0);
    CHECK(!strcmp(name_of(1),"Story AI 1") && WORD_AT(E(1)+0x60u)==1u && !strcmp(name_of(0),"Guard"));
    /* back: menu+0x18C = -1 */
    press(0xFFFFFFFFu);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==2);
    /* type 5 never duplicated */
    WORD_AT(E(0)+0x34u)=5u;press(0x41540001u);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==2);
    WORD_AT(E(0)+0x34u)=1u;WORD_AT(E(0)+0x58u)=2u;                /* undo the stock clamp */
    /* limit: message on the edit screen, which stays open */
    WORD_AT(ED+0x280u)=255u;messages=0;orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==0 && messages==1 && last_sprintf_value==255u && last_message_menu==MENU && WORD_AT(ED+0x280u)==255u);
    press(0x41540002u);messages=0;                                /* a linked copy too */
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==0 && messages==1 && WORD_AT(ED+0x280u)==255u);
    WORD_AT(ED+0x280u)=254u;messages=0;                           /* one slot is enough */
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && messages==0 && WORD_AT(ED+0x280u)==255u && WORD_AT(E(254)+0x50u)==1u);
    WORD_AT(E(254)+0x50u)=0;orig_calls=0;press(0x41540001u);
    WORD_AT(ED+0x280u)=2u;
    /* no container room */
    has_room=0;press(0x41540001u);messages=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==0 && messages==1 && last_message_text==0x00700000u+0x9B6u && WORD_AT(ED+0x280u)==2u);
    has_room=1;

    /* ---- first linked copy of 0: 0 leads, copy 2 "Guard 2" ---- */
    press(0x41540002u);select_ai(0);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(ED+0x280u)==3u && room_bytes==0x78u);
    CHECK(WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u && WORD_AT(E(1)+0x50u)==0);
    CHECK(!strcmp(name_of(2),"Guard 2") && !strcmp(name_of(0),"Guard") && WORD_AT(E(2)+0x60u)==2u);
    CHECK(same_points(0,2) && WORD_AT(E(2)+0x54u)==WORD_AT(E(0)+0x54u) && WORD_AT(E(2)+0x48u)==3u);
    rows_for(0,0x0031D083u,0x0031CAC0u);                         /* leader */
    CHECK(nrows==3 && rows[0].id==0x41540002u && rows[1].id==0x41540004u && rows[2].id==0x41540003u);
    CHECK(!strcmp(text_of(rows[1].text),"Logic means: this AI") && !strcmp(text_of(rows[2].text),"Unlink (next copy leads)"));
    rows_for(2,0x0031D083u,0x0031CAC0u);                         /* copy */
    CHECK(nrows==2 && rows[0].id==0x41540002u && rows[1].id==0x41540003u && !strcmp(text_of(rows[1].text),"Unlink copy"));
    rows_for(1,0x0031D083u,0x0031CAC0u);                         /* unlinked */
    CHECK(nrows==2 && rows[0].id==0x41540001u);
    /* linked copy of a copy: same leader, next number */
    press(0x41540002u);select_ai(2);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==4u && WORD_AT(E(3)+0x50u)==1u && same_points(2,3) && !strcmp(name_of(3),"Guard 3"));
    /* from the leader again, with "Guard 4" taken by an unrelated AI */
    snprintf((char *)name_of(1),32,"Guard 4");
    press(0x41540002u);select_ai(0);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==5u && WORD_AT(E(4)+0x50u)==1u && same_points(0,4) && !strcmp(name_of(4),"Guard 5"));
    snprintf((char *)name_of(1),32,"Story AI 1");
    /* duplicate of a linked copy (not offered, still safe): an independent AI */
    press(0x41540001u);select_ai(3);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==6u && WORD_AT(E(5)+0x50u)==0 && same_points(3,5) && WORD_AT(E(3)+0x50u)==1u);
    WORD_AT(ED+0x280u)=5u;
    /* group logic: a leader's toggle; nothing on a copy */
    press(0x41540004u);select_ai(0);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(ED+0x280u)==5u);
    rows_for(0,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==3 && !strcmp(text_of(rows[1].text),"Logic means: whole group"));
    rows_for(2,0x0031D083u,0x0031CAC0u);                         /* the flag does not break the link */
    CHECK(nrows==2 && rows[1].id==0x41540003u);
    press(0x41540004u);select_ai(2);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(2)+0x50u)==1u && WORD_AT(E(0)+0x50u)==0x10000u);
    /* a leader with group logic gets further copies */
    press(0x41540002u);select_ai(3);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==6u && WORD_AT(E(5)+0x50u)==1u && !strcmp(name_of(5),"Guard 4"));
    WORD_AT(ED+0x280u)=5u;

    /* ---- propagation on close: edit copy 3 ---- */
    select_ai(3);
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);
    WORD_AT(E(3)+0x48u)=9u;WORD_AT(E(3)+0x34u)=0u;WORD_AT(E(3)+0x54u)=6u;
    press(0xFFFFFFFFu);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(3)+0x58u)==1u);                               /* stock clamp of the edited one */
    CHECK(WORD_AT(E(0)+0x48u)==9u && WORD_AT(E(2)+0x48u)==9u && WORD_AT(E(4)+0x48u)==9u);
    CHECK(WORD_AT(E(0)+0x34u)==0u && WORD_AT(E(0)+0x58u)==1u && WORD_AT(E(2)+0x58u)==1u);
    CHECK(WORD_AT(E(0)+0x54u)==4u && WORD_AT(E(1)+0x48u)==3u);  /* facing per copy; unrelated AI */
    CHECK(!strcmp(name_of(0),"Guard") && !strcmp(name_of(2),"Guard 2") && WORD_AT(E(0)+0x60u)==0u);
    CHECK(WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(2)+0x50u)==1u);
    /* no change: nothing copied */
    WORD_AT(E(4)+0x4Cu)=5u;                                       /* out of step on purpose */
    select_ai(0);
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(4)+0x4Cu)==5u);
    WORD_AT(E(4)+0x4Cu)=1u;
    /* map path: open, edit, then compile (undo/save) with the screen open, twice */
    select_ai(4);
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);
    WORD_AT(E(4)+0x4Cu)=4u;
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(WORD_AT(E(0)+0x4Cu)==4u && WORD_AT(E(2)+0x4Cu)==4u);
    WORD_AT(E(4)+0x4Cu)=3u;
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(WORD_AT(E(0)+0x4Cu)==3u && WORD_AT(E(3)+0x4Cu)==3u);
    /* a different entry at the remembered index (new map): ignored */
    select_ai(4);
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);
    WORD_AT(E(4)+0x60u)=40u;WORD_AT(E(4)+0x4Cu)=2u;
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(WORD_AT(E(0)+0x4Cu)==3u);
    WORD_AT(E(4)+0x60u)=4u;WORD_AT(E(4)+0x4Cu)=3u;
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);           /* forget it */

    /* ---- save: chunk +0x32 = 0x4000 | leader + 1, leader 0x4000 | 0x8000 ---- */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(sai_data(4) && !sai_data(5));
    CHECK(saved_link(0)==0xC000u && saved_link(1)==0 && saved_link(2)==0x4001u && saved_link(3)==0x4001u && saved_link(4)==0x4001u);
    CHECK(WORD_AT(CONT+8u)==0x00C77668u && WORD_AT(CONT+0x10u)==0 && WORD_AT(CONT+0x1Cu)==0); /* other chunk untouched */
    WORD_AT(E(0)+0x50u)=0;                                        /* leader without group logic */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(saved_link(0)==0x4000u && saved_link(2)==0x4001u);
    WORD_AT(E(0)+0x50u)=0x10000u;
    /* invalid links are not saved: chained, self, type 5, a lone group flag */
    WORD_AT(E(1)+0x50u)=3u;                                       /* -> 2, itself a copy */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(saved_link(1)==0 && saved_link(2)==0x4001u);
    WORD_AT(E(1)+0x50u)=2u;                                       /* itself */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(saved_link(1)==0);
    WORD_AT(E(1)+0x50u)=1u;WORD_AT(E(1)+0x34u)=5u;               /* type 5 */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(saved_link(1)==0);
    WORD_AT(E(1)+0x50u)=0x10000u;WORD_AT(E(1)+0x34u)=1u;         /* no copies */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    CHECK(saved_link(1)==0);
    WORD_AT(E(1)+0x50u)=0;

    /* ---- load: links back from the chunks; stale +0x50 cleared ---- */
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    for(i=0;i<5;i++) WORD_AT(E(i)+0x50u)=0x77u;
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(ED+0x280u)==5u);
    CHECK(WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(1)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u &&
          WORD_AT(E(3)+0x50u)==1u && WORD_AT(E(4)+0x50u)==1u);
    /* odd data: chained link, past the count, group bit on a copy, lone group flag */
    set_saved_link(3,0x4003u);                                    /* 3 -> 2, a copy */
    set_saved_link(4,0x4000u|40u);
    set_saved_link(1,0xC000u);
    ait.dropped_links=0;
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(E(3)+0x50u)==0 && WORD_AT(E(4)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u && ait.dropped_links==2u);
    CHECK(WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(1)+0x50u)==0);
    set_saved_link(4,0xC001u);                                    /* a copy word with the group bit */
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(E(4)+0x50u)==0 && ait.dropped_links==4u);
    set_saved_link(3,0x4001u);set_saved_link(4,0x4001u);set_saved_link(1,0);
    /* the group flag goes when no copy survives the load */
    set_saved_link(2,0);set_saved_link(3,0);set_saved_link(4,0);
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(E(0)+0x50u)==0);
    set_saved_link(2,0x4001u);set_saved_link(3,0x4001u);set_saved_link(4,0x4001u);
    /* an entry that does not hold its chunk's saved index */
    load_skew=3u;                                                 /* entry 2 */
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(E(2)+0x50u)==0 && WORD_AT(E(3)+0x50u)==1u);
    load_skew=0;
    /* appended after entries already there: links point into the loaded run */
    WORD_AT(ED+0x280u)=2u;load_append=1;
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(ED+0x280u)==7u && WORD_AT(E(2)+0x50u)==0x10000u && WORD_AT(E(4)+0x50u)==3u &&
          WORD_AT(E(6)+0x50u)==3u && WORD_AT(E(3)+0x50u)==0);
    load_append=0;
    /* the old template format: 1 = "All Guard", 0/2/3 linked to it */
    set_saved_link(0,2u);set_saved_link(1,0);set_saved_link(2,2u);set_saved_link(3,2u);set_saved_link(4,0);
    ait.converted=0;
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(ED+0x280u)==5u && ait.converted==1u);
    CHECK(WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(1)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u &&
          WORD_AT(E(3)+0x50u)==1u && WORD_AT(E(4)+0x50u)==0);
    set_saved_link(0,0);set_saved_link(1,0);set_saved_link(2,4u);set_saved_link(3,0);set_saved_link(4,4u);
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);              /* template 3 after its first copy */
    CHECK(WORD_AT(E(2)+0x50u)==0 && WORD_AT(E(3)+0x50u)==0 && WORD_AT(E(4)+0x50u)==3u && WORD_AT(E(0)+0x50u)==0);
    set_saved_link(0,4u);
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);              /* the lowest copy leads */
    CHECK(WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u && WORD_AT(E(4)+0x50u)==1u && WORD_AT(E(3)+0x50u)==0);
    /* back to: leader 0 (group logic), copies 2, 3, 4 */
    set_saved_link(0,0xC000u);set_saved_link(1,0);set_saved_link(2,0x4001u);set_saved_link(3,0x4001u);set_saved_link(4,0x4001u);
    HOOK(pc_mapmaker_aitemplates_load,fake_load,CONT);
    CHECK(WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(4)+0x50u)==1u);
    for(i=0;i<5;i++) WORD_AT(E(i)+0x24u)=0;

    /* ---- unlink a copy ---- */
    press(0x41540003u);select_ai(4);orig_calls=0;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(orig_calls==1 && WORD_AT(E(4)+0x50u)==0 && WORD_AT(ED+0x280u)==5u && WORD_AT(E(0)+0x50u)==0x10000u);
    select_ai(0);
    HOOK(pc_mapmaker_aitemplates_open,fake_plain,MENU);
    WORD_AT(E(0)+0x48u)=11u;press(0xFFFFFFFFu);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(3)+0x48u)==11u && WORD_AT(E(2)+0x48u)==11u && WORD_AT(E(4)+0x48u)==9u);
    /* ---- unlink the leader: copy 2 leads, group logic off ---- */
    press(0x41540003u);select_ai(0);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(2)+0x50u)==0 && WORD_AT(E(3)+0x50u)==3u);
    rows_for(2,0x0031D083u,0x0031CAC0u);
    CHECK(nrows==3 && rows[1].id==0x41540004u);
    /* the last copy leaves: the group dissolves */
    WORD_AT(E(2)+0x50u)=0x10000u;
    press(0x41540003u);select_ai(3);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(3)+0x50u)==0 && WORD_AT(E(2)+0x50u)==0);
    /* a one-copy group whose leader unlinks: both independent */
    WORD_AT(E(3)+0x50u)=3u;
    press(0x41540003u);select_ai(2);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(E(3)+0x50u)==0 && WORD_AT(E(2)+0x50u)==0);

    /* ---- delete: the leader (2 of 0, 3), a lower AI, then the last copy ---- */
    WORD_AT(E(2)+0x50u)=0x10000u;WORD_AT(E(0)+0x50u)=3u;WORD_AT(E(3)+0x50u)=3u;
    HOOK(pc_mapmaker_aitemplates_delete,fake_delete,2u);
    CHECK(WORD_AT(ED+0x280u)==4u && !strcmp(name_of(2),"Guard 3"));
    CHECK(WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(2)+0x50u)==1u && WORD_AT(E(1)+0x50u)==0 && WORD_AT(E(3)+0x50u)==0);
    WORD_AT(E(0)+0x50u)=0x10000u;
    WORD_AT(E(3)+0x50u)=3u;                                       /* 3 -> 2, a copy (ignored) */
    HOOK(pc_mapmaker_aitemplates_delete,fake_delete,1u);
    CHECK(WORD_AT(ED+0x280u)==3u && WORD_AT(E(1)+0x50u)==1u && WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(2)+0x50u)==2u);
    WORD_AT(E(2)+0x50u)=0;
    HOOK(pc_mapmaker_aitemplates_delete,fake_delete,1u);
    CHECK(WORD_AT(ED+0x280u)==2u && WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(1)+0x50u)==0);
    /* renumbering keeps the group flag of a leader above the deleted AI */
    make_ai(0,1,1,1,"X");make_ai(1,1,1,1,"L");make_ai(2,1,1,1,"L 2");WORD_AT(ED+0x280u)=3u;
    WORD_AT(E(1)+0x50u)=0x10000u;WORD_AT(E(2)+0x50u)=2u;
    HOOK(pc_mapmaker_aitemplates_delete,fake_delete,0u);
    CHECK(WORD_AT(E(0)+0x50u)==0x10000u && WORD_AT(E(1)+0x50u)==1u);
    /* deleting a copy: a stray link to it is cleared, the lone leader dissolves */
    make_ai(0,1,1,1,"X");make_ai(1,1,1,1,"L");make_ai(2,1,1,1,"L 2");WORD_AT(ED+0x280u)=3u;
    WORD_AT(E(1)+0x50u)=0x10000u;WORD_AT(E(2)+0x50u)=2u;WORD_AT(E(0)+0x50u)=3u;  /* 0 -> 2, a copy */
    HOOK(pc_mapmaker_aitemplates_delete,fake_delete,2u);
    CHECK(WORD_AT(ED+0x280u)==2u && WORD_AT(E(0)+0x50u)==0 && WORD_AT(E(1)+0x50u)==0);

    /* ---- names: a promoted "Guard 2" leading "Guard 3"; default and long names ---- */
    make_ai(0,1,1,1,"Guard 2");make_ai(1,1,1,1,"Guard 3");WORD_AT(ED+0x280u)=2u;
    WORD_AT(E(1)+0x50u)=1u;
    press(0x41540002u);select_ai(0);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==3u && !strcmp(name_of(2),"Guard 4") && WORD_AT(E(2)+0x50u)==1u);
    make_ai(0,1,1,1,"Story AI 3");WORD_AT(ED+0x280u)=1u;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(!strcmp(name_of(1),"Story AI 3 2"));
    make_ai(0,1,1,1,"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");WORD_AT(ED+0x280u)=1u;
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(!strcmp(name_of(1),"ABCDEFGHIJKLMNOPQRSTUVWXYZ0 2"));
    /* a copy made from a member without points takes the leader's points */
    make_ai(0,1,1,2,"P");make_ai(1,1,1,0,"P 2");WORD_AT(ED+0x280u)=2u;WORD_AT(E(1)+0x50u)=1u;
    press(0x41540002u);select_ai(1);
    HOOK(pc_mapmaker_aitemplates_close,fake_close,MENU);
    CHECK(WORD_AT(ED+0x280u)==3u && same_points(0,2) && WORD_AT(E(2)+0x50u)==1u && !strcmp(name_of(2),"P 3"));

    /* ---- play: level start builds the table ---- */
    WORD_AT(0x0079094Cu)=100u;WORD_AT(0x00783A50u)=R;WORD_AT(0x00784014u)=CONT;
    make_ai(0,0,1,1,"A");make_ai(1,0,1,1,"A 2");make_ai(2,0,1,1,"A 3");
    make_ai(3,2,1,1,"B");make_ai(4,2,1,1,"B 2");make_ai(5,0,1,1,"C");
    make_ai(6,0,1,1,"D");make_ai(7,0,1,1,"D 2");
    WORD_AT(ED+0x280u)=8u;
    WORD_AT(E(0)+0x50u)=0x10000u;WORD_AT(E(3)+0x50u)=0x10000u;
    WORD_AT(E(1)+0x50u)=1u;WORD_AT(E(2)+0x50u)=1u;WORD_AT(E(4)+0x50u)=4u;WORD_AT(E(7)+0x50u)=7u;
    WORD_AT(0x0079094Cu)=102u;
    HOOK(pc_mapmaker_aitemplates_compile,fake_compile,CONT);
    WORD_AT(0x0079094Cu)=100u;
    CHECK(saved_link(6)==0x4000u && saved_link(7)==0x4007u);
    set_saved_link(5,0x4002u);                                    /* 5 -> 1, a copy: ignored */
    memset(&ait_level,0x55,sizeof ait_level);
    HOOK(pc_mapmaker_aitemplates_level,fake_plain,0);
    CHECK(ait_level.active && ait_level.template_of[1]==0 && ait_level.template_of[2]==0 &&
          ait_level.template_of[4]==3 && ait_level.template_of[5]==-1 && ait_level.template_of[0]==-1 &&
          ait_level.template_of[7]==6);
    CHECK(ait_level.group[0] && ait_level.group[3] && !ait_level.group[1] && !ait_level.group[5] && !ait_level.group[6]);

    /* event: a copy's event is delivered again as the leader's */
    {
        uint32_t actor=ACTORS,a=ACTORS+0x100u,rec=ACTORS+0x2000u;
        WORD_AT(actor+0x7Cu)=a;WORD_AT(a+0xBF0u)=rec;
        WORD_AT(rec+8u)=R+3u;nseen=0;                             /* index 2 */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==2 && seen[0]==R+3u && seen[1]==R+1u && WORD_AT(rec+8u)==R+3u);
        WORD_AT(rec+8u)=R+5u;nseen=0;                             /* index 4 -> leader 3 */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==2 && seen[1]==R+4u && WORD_AT(rec+8u)==R+5u);
        WORD_AT(rec+8u)=R+4u;nseen=0;                             /* the leader's own actor */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
        WORD_AT(rec+8u)=R+6u;nseen=0;                             /* not linked */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
        WORD_AT(rec+8u)=R+8u;nseen=0;                             /* a copy, leader without group logic */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
        WORD_AT(rec+8u)=500u;nseen=0;                             /* an object id */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
        WORD_AT(0x0079094Cu)=4u;WORD_AT(rec+8u)=R+3u;nseen=0;     /* a story level */
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
        WORD_AT(0x0079094Cu)=100u;
        WORD_AT(actor+0x7Cu)=0;nseen=0;
        HOOK(pc_mapmaker_aitemplates_event,fake_event,actor);
        CHECK(nseen==1);
    }
    /* remove (action 2) on the leader id: every copy too */
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+1u);
    CHECK(nseen==3 && seen[0]==R+1u && seen[1]==R+2u && seen[2]==R+3u);
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+2u);
    CHECK(nseen==1);
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+4u);
    CHECK(nseen==2 && seen[1]==R+5u);
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+7u);  /* no group logic */
    CHECK(nseen==1);
    /* spawn gate: a copy also waits for the leader's spawn action */
    memset(gate_open,0,sizeof gate_open);
    gate_open[3]=1;gate_open[1]=0;nseen=0;
    HOOK(pc_mapmaker_aitemplates_spawn_gate,fake_gate,R+3u);
    CHECK(g_eax==0 && nseen==2 && seen[1]==R+1u);
    gate_open[1]=1;nseen=0;
    HOOK(pc_mapmaker_aitemplates_spawn_gate,fake_gate,R+3u);
    CHECK(g_eax==1 && nseen==2);
    gate_open[3]=0;nseen=0;
    HOOK(pc_mapmaker_aitemplates_spawn_gate,fake_gate,R+3u);
    CHECK(g_eax==0 && nseen==1);
    gate_open[6]=1;gate_open[1]=0;nseen=0;
    HOOK(pc_mapmaker_aitemplates_spawn_gate,fake_gate,R+6u);
    CHECK(g_eax==1 && nseen==1);
    gate_open[8]=1;gate_open[7]=0;nseen=0;                        /* no group logic */
    HOOK(pc_mapmaker_aitemplates_spawn_gate,fake_gate,R+8u);
    CHECK(g_eax==1 && nseen==1);
    /* trigger 0x14 on the leader: any copy */
    {
        uint32_t t=ACTORS+0x3000u;
        memset(found,0,sizeof found);now=100u;
        WORD_AT(t+0x14u)=R+1u;WORD_AT(t+0x18u)=3u;WORD_AT(t+0x1Cu)=0;nseen=0;
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(g_eax==0 && nseen==3 && seen[1]==R+2u && seen[2]==R+3u && WORD_AT(t+0x14u)==R+1u && WORD_AT(t+0x18u)==3u);
        found[3]=1;nseen=0;                                       /* copy 2 seen: delay starts */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(g_eax==0 && nseen==3 && WORD_AT(t+0x18u)==7u && WORD_AT(t+0x14u)==R+1u);
        now=103u;nseen=0;                                         /* still waiting */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(g_eax==0 && WORD_AT(t+0x18u)==7u);
        now=110u;nseen=0;                                         /* delay over: fires */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(g_eax==1 && nseen==3 && WORD_AT(t+0x18u)==3u && WORD_AT(t+0x14u)==R+1u);
        found[2]=1;found[3]=0;WORD_AT(t+0x18u)=3u;nseen=0;        /* first copy: stops there */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(nseen==2 && WORD_AT(t+0x18u)==7u);
        WORD_AT(t+0x14u)=R+2u;WORD_AT(t+0x18u)=3u;found[2]=0;nseen=0; /* a copy's own trigger */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(nseen==1);
        WORD_AT(t+0x14u)=R+7u;WORD_AT(t+0x18u)=3u;nseen=0;        /* leader without group logic */
        HOOK(pc_mapmaker_aitemplates_alert,fake_alert,t);
        CHECK(nseen==1);
    }
    /* an old-format map played without re-saving: the template means any copy */
    set_saved_link(6,0);set_saved_link(7,7u);
    HOOK(pc_mapmaker_aitemplates_level,fake_plain,0);
    CHECK(ait_level.group[6] && ait_level.template_of[7]==6 && ait_level.group[0]);
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+7u);
    CHECK(nseen==2 && seen[1]==R+8u);
    /* no group logic anywhere: nothing active */
    set_saved_link(0,0x4000u);set_saved_link(3,0x4000u);set_saved_link(7,0x4007u);
    HOOK(pc_mapmaker_aitemplates_level,fake_plain,0);
    CHECK(!ait_level.active && ait_level.template_of[1]==0);
    /* limits off at level start: no table */
    set_saved_link(0,0xC000u);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    HOOK(pc_mapmaker_aitemplates_level,fake_plain,0);
    CHECK(!ait_level.active);
    nseen=0;HOOK(pc_mapmaker_aitemplates_remove,fake_remove,R+1u);
    CHECK(nseen==1);

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    puts("mapmaker aitemplates ok");
    return 0;
}
