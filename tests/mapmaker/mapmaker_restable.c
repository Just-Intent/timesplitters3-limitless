/* Asset-free contract for src/interface/pc_mapmaker_restable.inc: the MapMaker
 * editor resource panel replaces the capacity meter while the raised limits
 * are on. Collapsed by default to the fullest row; a click on it (A over the
 * game pointer) or the I key expands it to every row, titles left and
 * "used/max" right-aligned in one measured column, over a backdrop, with a
 * line pitch that fits the measured glyph. The rows show the numbers the
 * editor's own checks compare (pc_mapmaker_limits.inc, the per-category
 * maxima, Story AI and logic limits), coloured near and at their caps, inside
 * the 640 x 480 canvas, its left third and left of the toolbar (4:3 and wide
 * outputs). The stock meter is kept otherwise.
 * Built by tests/mapmaker/test_mapmaker_restable.py, which passes the
 * editor/objects limits read from their sources with -D. */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_mapmaker_pointer.h"

static uint32_t memory[0x800000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

#define TEXT_BUF 0x00600000u
static uint32_t heap_result=TEXT_BUF,heap_bytes;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { (void)alignment;heap_bytes=bytes;return heap_result; }

/* The host toggle key (pc_controls_mapmaker_table_key). */
static int table_key;
int pc_controls_mapmaker_table_key(void) { return table_key; }

/* Recorded calls: sub_0003FA10 (font), sub_0003FCF0 (measure &w, &h, text, 0),
 * sub_0001EFB0 (backdrop x0, y0, x1, y1, colour), sub_00041E30 (x, y, text,
 * colour, outline). The measure returns visible characters x measure_k wide
 * (0: nothing, so the estimate applies) and measure_h tall. */
static volatile int stub_seen;  /* distinct bodies: the linker must not fold the stubs */
void sub_0003DFA0(void) { stub_seen=1; }
void sub_0003FA10(void) { stub_seen=2; }
void sub_0003FCF0(void) { stub_seen=3; }
void sub_0001EFB0(void) { stub_seen=4; }
void sub_00041E30(void) { stub_seen=5; }
#define MAX_DRAWS 48
static struct { float x,y; char text[64]; uint32_t colour,outline,align,font; } draws[MAX_DRAWS];
static unsigned ndraws,font_calls,measures,rects;
static int rect[4];
static uint32_t rect_colour,rect_font;
static uint32_t font_now=7u,font_seen;
static float measure_k=5.f,measure_h=9.6f;
static unsigned visible(const char *s)
{
    unsigned n=0;
    while(*s) {
        if(s[0]=='\v' && s[1]=='s') { s+=2;while(*s>='0' && *s<='9') s++;continue; }
        n++;s++;
    }
    return n;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t r=0;
    va_start(a,n);
    if(f==sub_0003FA10 && n==1) {
        r=font_now;font_now=va_arg(a,uint32_t);font_calls++;
        if(font_calls==1) font_seen=font_now;
    } else if(f==sub_0003FCF0 && n==4) {
        uint32_t wp=va_arg(a,uint32_t),hp=va_arg(a,uint32_t),text=va_arg(a,uint32_t);
        unsigned v=visible((const char *)((uintptr_t)text+g_xbox_mem_offset));
        if(wp>=g_esp && hp>=g_esp && font_now==0x12u) {  /* scratch at the stack, drawing font */
            FLOAT_AT(wp)=(float)v*measure_k;FLOAT_AT(hp)=measure_h;
        }
        measures++;
    } else if(f==sub_0001EFB0 && n==5) {
        unsigned i;
        for(i=0;i<4;i++) rect[i]=(int)va_arg(a,uint32_t);
        rect_colour=va_arg(a,uint32_t);rect_font=font_now;rects++;
    } else if(f==sub_00041E30 && n==5) {
        uint32_t x=va_arg(a,uint32_t),y=va_arg(a,uint32_t),text=va_arg(a,uint32_t);
        if(ndraws<MAX_DRAWS) {
            memcpy(&draws[ndraws].x,&x,4);memcpy(&draws[ndraws].y,&y,4);
            snprintf(draws[ndraws].text,sizeof draws[ndraws].text,"%s",(const char *)((uintptr_t)text+g_xbox_mem_offset));
            draws[ndraws].colour=va_arg(a,uint32_t);draws[ndraws].outline=va_arg(a,uint32_t);
            draws[ndraws].align=WORD_AT(0x007E7260u);draws[ndraws].font=font_now;
        }
        ndraws++;
    }
    va_end(a);
    return r;
}

/* The probe scope (pc_mapmaker_probe.inc), as far as the table uses it. */
#define MAPMAKER_PROBE_METER 1u
static unsigned probe_flags;
static unsigned mapmaker_probe_flags(void) { return probe_flags; }
static void mapmaker_probe_meter(void (*original)(void)) { original(); }

#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif
#include "pc_mapmaker_restable.inc"
#include "interface/pc_ui_layout.h"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static int originals;
static void fake_limits(void) { originals++; }
static void fake_meter(void) { originals++; g_esp+=4u; }
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    mapmaker_table.initialized=0;
    mapmaker_budget.initialized=0;
}

#define ED 0x00320000u
#define TYPEA 0x00330000u
#define TYPEB 0x00330010u
#define STACK 0x00700000u
#define MENU 0x006FEBE8u
#define POINTER (0x007BA260u+1u*0x1E0u)
static uint32_t make_list(uint32_t base,uint32_t type_cell,unsigned n)
{
    unsigned i;
    for(i=0;i<n;i++) {
        uint32_t p=base+i*0x50u;
        WORD_AT(p+0x2c)=type_cell;WORD_AT(p+0x3c)=0;WORD_AT(p+0x40)=i+1<n?p+0x50u:0u;
    }
    return n?base:0u;
}
/* n items of catalogue id `id` hanging off one piece, from base. */
static void add_items(uint32_t piece,uint32_t base,uint32_t id,unsigned n)
{
    unsigned i;
    uint32_t head=WORD_AT(piece+0x3c);
    for(i=0;i<n;i++) {
        uint32_t s=base+i*0x20u;
        WORD_AT(s)=id;WORD_AT(s+0x14)=head;head=s;
    }
    WORD_AT(piece+0x3c)=head;
}
static void set_category(uint32_t id,uint16_t category)
{
    uint32_t row=0x00524F58u+id*0xC4u;
    WORD_AT(row)=(WORD_AT(row)&0xFFFFu)|((uint32_t)category<<16);
}
static void meter_at(float x,float y)
{
    g_esp=STACK;WORD_AT(STACK)=0x00311748u;FLOAT_AT(STACK+4u)=x;FLOAT_AT(STACK+8u)=y;
    ndraws=0;font_calls=0;originals=0;measures=0;rects=0;memset(rect,0,sizeof rect);
}
static void draw(void) { meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter); }
static void wrap(char *out,size_t n,const char *s) { snprintf(out,n,"\vs60%s\vs100",s); }
static int drawn(const char *text)
{
    char want[64];
    unsigned i;
    wrap(want,sizeof want,text);
    for(i=0;i<ndraws && i<MAX_DRAWS;i++) if(!strcmp(draws[i].text,want)) return (int)i;
    return -1;
}
static uint32_t colour_of(const char *text) { int i=drawn(text);return i<0?0xDEADu:draws[i].colour; }

/* One editor tick of the input hook (after sub_00073870): c4 pressed bits,
 * c8 held/released bits, the game pointer at x, y. */
static void editor_tick(uint32_t c4,uint32_t c8,float x,float y)
{
    WORD_AT(MENU+0xc4)=c4;WORD_AT(MENU+0xc8)=c8;
    FLOAT_AT(POINTER+0x20)=x;FLOAT_AT(POINTER+0x24)=y;
    mapmaker_table_input(MENU);
}
#define A_EDGE 0x00100010u   /* press: bit 4 and its repeat bit */
#define A_HELD 0x00000010u
#define A_UP 0x00100000u     /* release edge in +0xC8 */

/* Stock category maxima (0x00524BE0 + c*24 + 4); categories_apply raises them. */
static void stock_categories(void)
{
    static const uint32_t m[37]={32,32,16,1,1,1,4,1,32,32,2,2,2,2,2,2,2,2,2,16,16,12,1,16,50,20,20,8,8,8,10,32,20,66,66,4,4};
    unsigned i;
    for(i=0;i<37;i++) WORD_AT(0x00524BE0u+i*24u+4u)=m[i];
}

/* The expanded panel's geometry contract on the current draws. */
static void check_columns(unsigned n,float line_min)
{
    unsigned i;
    float value_right,title_right=0.f,title_x=draws[0].x;
    CHECK(ndraws==1u+2u*n && rects==1);
    CHECK(rect_colour==MAPMAKER_TABLE_PANEL && rect_font==0x12u);
    value_right=draws[2].x+(float)visible(draws[2].text)*(measure_k>0?measure_k:MAPMAKER_TABLE_GLYPH);
    for(i=0;i<n;i++) {
        unsigned t=1u+2u*i,v=t+1u;
        float k=measure_k>0?measure_k:MAPMAKER_TABLE_GLYPH;
        float vr=draws[v].x+(float)visible(draws[v].text)*k,tr=draws[t].x+(float)visible(draws[t].text)*k;
        CHECK(draws[t].x==title_x);                               /* titles left-aligned */
        CHECK(fabsf(vr-value_right)<1e-3f);                       /* values right-aligned in one column */
        CHECK(draws[t].y==draws[v].y);                            /* same line */
        CHECK(draws[t].colour==draws[v].colour && draws[t].outline==0x80u && draws[v].outline==0x80u);
        CHECK(draws[t].font==0x12u && draws[v].font==0x12u && draws[t].align==0u);
        if(tr>title_right) title_right=tr;
        CHECK(fabsf(draws[t].y-(draws[0].y+(float)(i+1u)*(draws[1].y-draws[0].y)))<1e-3f);  /* even pitch */
    }
    CHECK(draws[1].y-draws[0].y>=line_min-1e-3f);                 /* rows clear of the glyph height */
    for(i=0;i<n;i++)                                              /* every value clear of every title */
        CHECK(draws[2u+2u*i].x>=title_right+MAPMAKER_TABLE_GAP-1e-3f);
    /* The backdrop holds every line. */
    CHECK((float)rect[0]<=title_x-MAPMAKER_TABLE_PAD+1e-3f && (float)rect[2]>=value_right+MAPMAKER_TABLE_PAD-1e-3f);
    CHECK((float)rect[1]<=draws[0].y && (float)rect[3]>=draws[2u*n-1u].y+(draws[1].y-draws[0].y));
    /* \vs reads every digit after it (0x00040082): no text may start with one. */
    for(i=0;i<ndraws && i<MAX_DRAWS;i++)
        CHECK(!strncmp(draws[i].text,"\vs60",4) && !(draws[i].text[4]>='0' && draws[i].text[4]<='9'));
}

int main(void)
{
    uint32_t pieces;
    unsigned i;
    g_xbox_mem_offset=(uintptr_t)memory;
    stock_categories();
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(0x00762CB8u)=700;                          /* pad groups */
    WORD_AT(ED+0x2f34)=(uint32_t)-1;                   /* nothing held from the palette */
    WORD_AT(ED+0x280)=37;                              /* Story AI */
    WORD_AT(ED+0x41E0)=12;                             /* logic ops */
    WORD_AT(0x007E7260u)=0x55u;                        /* text alignment, restored after */
    WORD_AT(0x0079094Cu)=102u;                         /* the editor scene */
    WORD_AT(0x007BA9E0u)=1u;                           /* pointer of player 1 */
    /* Tile type 1: 10 pads, 9 cover, 20 links, 10,000 B; held type 2: 2, 9, 20, 1,000 B. */
    WORD_AT(TYPEA)=1;WORD_AT(TYPEB)=2;
    WORD_AT(0x00529400u+0x140u)=10;WORD_AT(0x00529404u+0x140u)=9;WORD_AT(0x00529408u+0x140u)=20;WORD_AT(0x00529410u+0x140u)=10000;
    WORD_AT(0x00529400u+0x280u)=2;WORD_AT(0x00529404u+0x280u)=9;WORD_AT(0x00529408u+0x280u)=20;WORD_AT(0x00529410u+0x280u)=1000;
    {   /* tile 1: one 3-slot connection point per side (catalogue count row + 0x14, entries row + 0x18) */
        unsigned k;
        WORD_AT(0x00529414u+0x140u)=4u;
        for(k=0;k<4;k++) { unsigned char *e=(unsigned char *)memory+0x00529418u+0x140u+k*6u; e[0]=(unsigned char)(k+1u);e[4]=3; }
    }
    pieces=make_list(0x00400000u,TYPEA,90);
    WORD_AT(0x0078400Cu)=pieces;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    WORD_AT(ED+0x306c)=5u<<16;                         /* 5 free-standing objects (counted as items) */
    /* Catalogue categories (US XBE): 17 weapon (8), 29 keycard red (10), 35 health (19),
     * 67 mover (25), 73 prop (26), 53/55/50 turrets (27, 28, 29), 10 teleporter (14), 0 start (0). */
    set_category(17,8);set_category(29,10);set_category(35,19);set_category(67,25);set_category(73,26);
    set_category(53,27);set_category(55,28);set_category(50,29);set_category(10,14);set_category(0,0);
    add_items(pieces,0x00500000u,17,30);               /* 30 weapons */
    add_items(pieces+0x50u,0x00501000u,29,3);          /* 3 keycards */
    add_items(pieces+0xA0u,0x00502000u,35,7);          /* 7 health */
    add_items(pieces+0xF0u,0x00503000u,67,150);        /* 150 movers: amber (>= 90% of 160) */
    add_items(pieces+0x140u,0x00506000u,73,4);         /* 4 props */
    add_items(pieces+0x190u,0x00507000u,53,24);        /* turret type 27 full (24/24) */
    add_items(pieces+0x1E0u,0x00508000u,55,2);
    add_items(pieces+0x230u,0x00509000u,10,1);         /* 1 teleporter */
    add_items(pieces+0x280u,0x0050A000u,0,2);          /* start points: no category row, not items */
    set_category(90,8);                                /* bytes past the 80-entry catalogue */
    add_items(pieces+0x2D0u,0x0050B000u,90,1);         /* past the catalogue: ignored */
    WORD_AT(ED+0x278)=mapmaker_placed_memory(pieces)+mapmaker_budget_apply(1000u);

    /* ---- the host toggle key policy (pc_mapmaker_pointer.h) ---- */
    {
        int bound[4]={'W','E',0,'R'};
        CHECK(PC_MAPMAKER_TABLE_KEY=='I');
        CHECK(pc_mapmaker_table_key(1,bound,4) && !pc_mapmaker_table_key(0,bound,4));
        CHECK(pc_mapmaker_table_key(1,NULL,0));
        bound[2]='I';
        CHECK(!pc_mapmaker_table_key(1,bound,4));                /* an action bound to I keeps it */
    }

    /* ---- limits off: the stock meter, no table ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_TABLE",NULL);
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(originals==1 && ndraws==0 && rects==0 && g_esp==STACK+4u);
    /* Nothing drawn: a click there is the editor's. */
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE && WORD_AT(MENU+0xc8)==A_HELD);
    editor_tick(0,A_UP,30.f,40.f);

    /* ---- limits on, but the check has not run for this editor ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    draw();
    CHECK(originals==1 && ndraws==0);

    /* ---- collapsed (the default): one line, the fullest row ---- */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(mapmaker_limits_view.valid && mapmaker_limits_view.editor==ED);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));             /* placement allowed */
    draw();
    CHECK(originals==0);                               /* replaces the stock meter */
    CHECK(g_esp==STACK+4u);                            /* and does its ret */
    CHECK(heap_bytes>=MAPMAKER_TABLE_SLOTS*MAPMAKER_TABLE_TEXT);
    CHECK(!mapmaker_table.expanded && ndraws==1 && rects==1);
    CHECK(font_seen==0x12u && font_now==7u && font_calls==2);    /* meter font, then restored */
    CHECK(WORD_AT(0x007E7260u)==0x55u && draws[0].align==0u);    /* left aligned while drawing */
    /* Turrets (type 27 at its maximum) beat movers (150/160, amber) and pad groups (68%). */
    CHECK(drawn("+ Fullest: Turrets 26/72")==0);
    if(drawn("+ Fullest: Turrets 26/72")) fprintf(stderr,"collapsed: '%s'\n",draws[0].text);
    CHECK(draws[0].colour==MAPMAKER_TABLE_FULL && draws[0].outline==0x80u);
    CHECK(draws[0].x==22.f+MAPMAKER_TABLE_PAD && draws[0].y==36.f+MAPMAKER_TABLE_PAD);
    CHECK(rect[0]==22 && rect[1]==36);
    CHECK((float)rect[2]>=draws[0].x+(float)visible(draws[0].text)*measure_k+MAPMAKER_TABLE_PAD-1.f);
    CHECK((float)rect[3]>=draws[0].y+measure_h && (float)rect[3]<=36.f+2.f*MAPMAKER_TABLE_PAD+MAPMAKER_TABLE_LINE+1.f);
    CHECK(rect[2]<200);                                /* compact: far left of the toolbar (x 388) */
    CHECK(WORD_AT(STACK)==0x00311748u && FLOAT_AT(STACK+4u)==22.f && FLOAT_AT(STACK+8u)==36.f);  /* caller's frame kept */

    /* ---- the click toggle: A over the panel, taken from the editor ---- */
    editor_tick(A_EDGE,A_HELD,400.f,40.f);                       /* elsewhere: the editor's */
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE && WORD_AT(MENU+0xc8)==A_HELD);
    editor_tick(0,A_UP,400.f,40.f);
    draw();
    editor_tick(0,0,30.f,40.f);                                  /* hover only */
    CHECK(!mapmaker_table.expanded);
    draw();
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(mapmaker_table.expanded);
    CHECK(!(WORD_AT(MENU+0xc4)&A_EDGE) && !(WORD_AT(MENU+0xc8)&A_HELD));
    WORD_AT(MENU+0xc4)=0;
    draw();
    editor_tick(0x00100000u,A_HELD,200.f,300.f);                 /* held (repeat), moved off: still taken */
    CHECK(mapmaker_table.expanded && WORD_AT(MENU+0xc4)==0 && WORD_AT(MENU+0xc8)==0);
    draw();
    editor_tick(0,A_UP,200.f,300.f);                             /* the release too */
    CHECK(WORD_AT(MENU+0xc8)==0 && !mapmaker_table.swallow);
    draw();
    editor_tick(A_EDGE|0x20u,A_HELD,200.f,300.f);                /* the next press is the editor's */
    CHECK(WORD_AT(MENU+0xc4)==(A_EDGE|0x20u) && WORD_AT(MENU+0xc8)==A_HELD && mapmaker_table.expanded);
    editor_tick(0,A_UP,200.f,300.f);
    CHECK(WORD_AT(MENU+0xc8)==A_UP);
    /* Other buttons on the panel pass through. */
    draw();
    editor_tick(0x00200020u,0x20u,30.f,40.f);
    CHECK(mapmaker_table.expanded && WORD_AT(MENU+0xc4)==0x00200020u);

    /* ---- expanded: two columns ---- */
    draw();
    CHECK(originals==0 && g_esp==STACK+4u && font_now==7u && WORD_AT(0x007E7260u)==0x55u);
    CHECK(measures==1u+2u*MAPMAKER_TABLE_ROWS);
    {
        /* Placed counts: 90 tiles; items 30+3+7+150+4+24+2+1 (ids > 9; 1 id-90 item too) + 5 free;
         * pads 900, cover 810, links 90*20 + 90*12/2 (half the side slots); memory placed estimate / 1024 rounded up. */
        uint32_t items=30u+3u+7u+150u+4u+24u+2u+1u+1u+5u;
        uint32_t kb=(mapmaker_placed_memory(pieces)+1023u)/1024u;
        static const char *titles[15]={"Tiles","Items","Objects","Pickups","Pads","Cover","Links","Groups",
                                      "Memory","Story AI","Logic","Movers","Props","Turrets","Teleports"};
        char want[15][32],text[64];
        snprintf(want[0],32," 90/1023");
        snprintf(want[1],32," %u/500",(unsigned)items);
        snprintf(want[2],32," %u/2047",(unsigned)(90u+items+128u));
        snprintf(want[3],32," 40/768");            /* weapons + keycards + health */
        snprintf(want[4],32," 900/7680");
        snprintf(want[5],32," 810/9984");
        snprintf(want[6],32," 2340/32256");
        snprintf(want[7],32," 700/1024");
        snprintf(want[8],32," %u/2000 KB",(unsigned)kb);
        snprintf(want[9],32," 37/511");
        snprintf(want[10],32," 12/120");
        snprintf(want[11],32," 150/160");
        snprintf(want[12],32," 4/160");
        snprintf(want[13],32," 26/72");
        snprintf(want[14],32," 1/40");
        CHECK(drawn("- Resources")==0 && draws[0].colour==MAPMAKER_TABLE_FULL);
        for(i=0;i<15;i++) {
            wrap(text,sizeof text,titles[i]);
            CHECK(1u+2u*i<ndraws && !strcmp(draws[1u+2u*i].text,text));
            wrap(text,sizeof text,want[i]);
            CHECK(2u+2u*i<ndraws && !strcmp(draws[2u+2u*i].text,text));
            if(2u+2u*i<ndraws && strcmp(draws[2u+2u*i].text,text))
                fprintf(stderr,"row %u: '%s' want '%s'\n",i,draws[2u+2u*i].text+4,want[i]);
        }
        check_columns(15,measure_h+MAPMAKER_TABLE_LEAD);
        CHECK(draws[0].x==22.f+MAPMAKER_TABLE_PAD && draws[0].y==36.f+MAPMAKER_TABLE_PAD);
        CHECK(rect[2]<200 && rect[3]<260);                       /* compact and above the floor buttons */
        /* The same numbers as the check: the meter fraction is the fullest of the capped rows. */
        CHECK(fabsf(FLOAT_AT(ED+0x270)-700.f/1024.f)<1e-4f);
        CHECK(colour_of("Tiles")==MAPMAKER_TABLE_OK && colour_of(want[0])==MAPMAKER_TABLE_OK);
        CHECK(colour_of("Movers")==MAPMAKER_TABLE_NEAR && colour_of(want[11])==MAPMAKER_TABLE_NEAR);
        CHECK(colour_of("Turrets")==MAPMAKER_TABLE_FULL);       /* turret type 27 at 24/24 */
        CHECK(colour_of("Props")==MAPMAKER_TABLE_OK);
        WORD_AT(0x007497B0u)=0x20u;                               /* blink phase */
        draw();
        CHECK(colour_of("Turrets")==MAPMAKER_TABLE_BLINK && colour_of(want[13])==MAPMAKER_TABLE_BLINK);
        WORD_AT(0x007497B0u)=0;
        /* The game's measure fails: estimated widths, still one column. */
        measure_k=0.f;
        draw();
        check_columns(15,measure_h+MAPMAKER_TABLE_LEAD);
        measure_k=5.f;
        /* A taller glyph: the pitch follows it. */
        measure_h=14.f;
        draw();
        check_columns(15,14.f+MAPMAKER_TABLE_LEAD);
        measure_h=0.f;                                            /* no height: the default pitch */
        draw();
        CHECK(fabsf(draws[1].y-draws[0].y-MAPMAKER_TABLE_LINE)<1e-3f);
        measure_h=9.6f;
    }

    /* ---- the key toggle: once per press, only while the panel is up ---- */
    draw();
    table_key=1;editor_tick(0,0,400.f,300.f);
    CHECK(!mapmaker_table.expanded);
    draw();
    editor_tick(0,0,400.f,300.f);                                /* still held */
    CHECK(!mapmaker_table.expanded);
    table_key=0;editor_tick(0,0,400.f,300.f);
    draw();
    table_key=1;editor_tick(0,0,400.f,300.f);
    CHECK(mapmaker_table.expanded);
    table_key=0;editor_tick(0,0,400.f,300.f);
    draw();
    table_key=1;editor_tick(0,0,400.f,300.f);
    CHECK(!mapmaker_table.expanded);
    table_key=0;editor_tick(0,0,400.f,300.f);
    /* Kept for the session: the next draw is still collapsed. */
    draw();
    CHECK(ndraws==1 && drawn("+ Fullest: Turrets 26/72")==0);

    /* ---- the toggle is the editor screen's only ---- */
    WORD_AT(MENU+0x178)=0x00650000u;                            /* a menu with rows (pause, lists) */
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE);
    editor_tick(0,A_UP,30.f,40.f);
    WORD_AT(MENU+0x178)=0;WORD_AT(MENU+0x170)=0x00650000u;      /* a child menu open */
    draw();
    table_key=1;editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE);
    table_key=0;editor_tick(0,A_UP,30.f,40.f);
    WORD_AT(MENU+0x170)=0;WORD_AT(0x0079094Cu)=100u;            /* play */
    draw();
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE);
    WORD_AT(0x0079094Cu)=102u;
    editor_tick(0,A_UP,30.f,40.f);
    /* Not drawn for a while (a mode without the meter): clicks are the editor's. */
    draw();
    for(i=0;i<MAPMAKER_TABLE_STALE+1u;i++) editor_tick(0,0,400.f,300.f);
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE);
    table_key=1;editor_tick(0,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded);
    table_key=0;editor_tick(0,A_UP,30.f,40.f);
    /* TSFP_MAPMAKER_TABLE=0: no toggle either. */
    draw();
    set_env("TSFP_MAPMAKER_TABLE","0");set_env("TSFP_MAPMAKER_LIMITS","1");
    editor_tick(A_EDGE,A_HELD,30.f,40.f);
    CHECK(!mapmaker_table.expanded && WORD_AT(MENU+0xc4)==A_EDGE);
    editor_tick(0,A_UP,30.f,40.f);
    set_env("TSFP_MAPMAKER_TABLE",NULL);set_env("TSFP_MAPMAKER_LIMITS","1");
    pc_display_mapmaker_limits(fake_limits);
    mapmaker_table.expanded=1;

    /* A refused cap turns its row red at the placed count (placed + held over). */
    WORD_AT(0x00762CB8u)=1024;                                    /* groups at the cap: allowed, red (full) */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    draw();
    CHECK(colour_of(" 1024/1024")==MAPMAKER_TABLE_FULL && colour_of("Groups")==MAPMAKER_TABLE_FULL);
    WORD_AT(0x00762CB8u)=922;                                     /* 90% (922/1024) */
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(colour_of(" 922/1024")==MAPMAKER_TABLE_NEAR);
    WORD_AT(0x00762CB8u)=700;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,800);         /* held: 800 x 9 cover = 7200 + 810 */
    WORD_AT(0x00529404u+0x280u)=12;                               /* 9600 + 810 > 9984: refused on cover */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    CHECK(mapmaker_limits_view.over&MAPMAKER_CAP_COVER);
    draw();
    CHECK(colour_of(" 810/9984")==MAPMAKER_TABLE_FULL);            /* placed shown, refusal coloured */
    CHECK(colour_of(" 900/7680")==MAPMAKER_TABLE_OK);
    WORD_AT(0x00529404u+0x280u)=9;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    /* Pieces: 1023 placed + 1 held refused (template links 0 so only the
     * pieces pass; the 2047-slot object pool keeps room), the row at
     * 1023/1023 red, objects 1156/2047 (1023 + 5 free objects + 128) not. */
    WORD_AT(0x00529408u+0x280u)=0;
    WORD_AT(0x0078400Cu)=make_list(0x00400000u,TYPEB,1023);
    pc_display_mapmaker_limits(fake_limits);
    CHECK(mapmaker_limits_view.over==MAPMAKER_CAP_PIECES);
    draw();
    CHECK(colour_of(" 1023/1023")==MAPMAKER_TABLE_FULL);
    CHECK(colour_of(" 1156/2047")==MAPMAKER_TABLE_OK);
    /* Collapsed: the only red row. */
    mapmaker_table.expanded=0;
    draw();
    CHECK(drawn("+ Fullest: Tiles 1023/1023")==0 && draws[0].colour==MAPMAKER_TABLE_FULL);
    mapmaker_table.expanded=1;
    WORD_AT(0x00529408u+0x280u)=20;
    /* Engine objects: pieces + items + 128 reserved (64 characters, 64 for effects), as the check. */
    WORD_AT(0x0078400Cu)=make_list(0x00400000u,TYPEB,340);
    WORD_AT(ED+0x306c)=500u<<16;                                  /* 340 + 500 + 128 = 968 */
    WORD_AT(ED+0x2f7c)=0;
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(drawn(" 968/2047")>=0);
    CHECK(colour_of(" 500/500")==MAPMAKER_TABLE_FULL);
    CHECK(colour_of(" 968/2047")==MAPMAKER_TABLE_OK);
    /* Collapsed with no red row: the fullest amber row by ratio, items 92% over
     * groups 90.8% (objects 45%). */
    mapmaker_table.expanded=0;
    WORD_AT(ED+0x306c)=460u<<16;                                  /* items 460/500, objects 928/2047 */
    WORD_AT(0x0078400Cu)=make_list(0x00400000u,TYPEB,340);
    WORD_AT(0x00762CB8u)=930;                                     /* groups 930/1024 */
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(drawn("+ Fullest: Items 460/500")==0 && draws[0].colour==MAPMAKER_TABLE_NEAR);
    if(drawn("+ Fullest: Items 460/500")) fprintf(stderr,"collapsed: '%s'\n",draws[0].text);
    WORD_AT(0x00762CB8u)=950;                                     /* groups 92.8%, after items: groups now */
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(drawn("+ Fullest: Groups 950/1024")==0);
    WORD_AT(ED+0x306c)=420u<<16;WORD_AT(0x00762CB8u)=930;        /* items 84%, objects 43% (ok) */
    pc_display_mapmaker_limits(fake_limits);
    draw();
    CHECK(drawn("+ Fullest: Groups 930/1024")==0);
    WORD_AT(0x00762CB8u)=700;
    mapmaker_table.expanded=1;
    WORD_AT(ED+0x306c)=5u<<16;
    WORD_AT(0x0078400Cu)=pieces;WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    pc_display_mapmaker_limits(fake_limits);

    /* ---- the stock meter instead ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);                         /* limits turned off after a check */
    draw();
    CHECK(originals==1 && ndraws==0 && rects==0);
    editor_tick(A_EDGE,A_HELD,30.f,40.f);                         /* the stock meter's place: the editor's */
    CHECK(WORD_AT(MENU+0xc4)==A_EDGE && WORD_AT(MENU+0xc8)==A_HELD);
    editor_tick(0,A_UP,30.f,40.f);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    probe_flags=MAPMAKER_PROBE_METER;                             /* the probe measures the stock meter */
    draw();
    CHECK(originals==1 && ndraws==0);
    probe_flags=0;
    WORD_AT(0x007844A8u)=ED+0x1000u;                              /* another editor object */
    draw();
    CHECK(originals==1 && ndraws==0);
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(0x007844A8u)=0x05000000u;                             /* a frame whose check could not run */
    pc_display_mapmaker_limits(fake_limits);
    WORD_AT(0x007844A8u)=ED;
    draw();
    CHECK(originals==1 && ndraws==0);
    pc_display_mapmaker_limits(fake_limits);
    heap_result=0;mapmaker_table.text=0;                          /* no text buffer */
    draw();
    CHECK(originals==1 && ndraws==0);
    heap_result=TEXT_BUF;
    set_env("TSFP_MAPMAKER_TABLE","0");
    set_env("TSFP_MAPMAKER_LIMITS","1");
    draw();
    CHECK(originals==1 && ndraws==0);
    set_env("TSFP_MAPMAKER_TABLE",NULL);set_env("TSFP_MAPMAKER_LIMITS","1");
    draw();
    CHECK(originals==0 && ndraws==1u+2u*MAPMAKER_TABLE_ROWS);

    /* ---- layout: 4:3 and wide outputs, collapsed and expanded ----
     * The meter's own position (sub_0030C3C0): safe-area margins of the 640 x 480
     * canvas, x 22 or 32, y 36 or 24, plus the HUD slide. The editor HUD is drawn
     * in the centred 4:3 box on a wide output (mapmaker-widescreen.md), so the
     * guest rectangle maps to output = box_left + guest * h / 480. Widths are the
     * estimate for the longest strings the caps allow. */
    {
        static const float pos[][2]={{22.f,36.f},{32.f,24.f},{22.f-40.f,36.f-64.f},{600.f,470.f}};
        static const unsigned outputs[][2]={{640,480},{1024,768},{1920,1080},{2560,1080},{3840,1080}};
        const float header_w=(float)strlen("+ Fullest: Teleports 32256/32256 KB")*MAPMAKER_TABLE_GLYPH;
        const float title_w=(float)strlen("Teleports")*MAPMAKER_TABLE_GLYPH;
        const float value_w=(float)strlen("32256/32256 KB")*MAPMAKER_TABLE_GLYPH;
        const float line=mapmaker_table_line(9.6f);
        unsigned p,o,e;
        CHECK(line>=9.6f+MAPMAKER_TABLE_LEAD-1e-4f && line<=12.f);    /* fits the 60% glyph, not looser */
        CHECK(MAPMAKER_TABLE_SCALE>=60 && MAPMAKER_TABLE_SCALE<=65);
        /* The unmeasured pitch fits font 0x12 (~16 units at 100%) at this scale. */
        CHECK(MAPMAKER_TABLE_LINE>=16.f*(float)MAPMAKER_TABLE_SCALE/100.f+MAPMAKER_TABLE_LEAD-1e-4f);
        CHECK(mapmaker_table_line(40.f)==MAPMAKER_TABLE_LINE && mapmaker_table_line(NAN)==MAPMAKER_TABLE_LINE);
        for(e=0;e<2;e++)
            for(p=0;p<sizeof pos/sizeof pos[0];p++) {
                MapMakerTableBox b;
                unsigned lines=e?1u+MAPMAKER_TABLE_ROWS:1u;
                mapmaker_table_layout(pos[p][0],pos[p][1],lines,e?(float)strlen("- Resources")*MAPMAKER_TABLE_GLYPH:header_w,
                                      title_w,value_w,line,&b);
                CHECK(b.x0>=MAPMAKER_TABLE_MARGIN && b.y0>=MAPMAKER_TABLE_MARGIN);
                CHECK(b.y1<=480.f-MAPMAKER_TABLE_MARGIN+1e-3f && b.x1<=640.f-MAPMAKER_TABLE_MARGIN+1e-3f);
                CHECK(mapmaker_table_line_y(&b,lines-1u)+line<=b.y1+1e-3f);
                if(e) CHECK(b.value_right+MAPMAKER_TABLE_PAD<=b.x1+1e-3f &&
                            b.value_right-value_w>=b.text_x+title_w+MAPMAKER_TABLE_GAP-1e-3f);
                if(p<2) {
                    /* At the meter: left third (the renderer's side anchor), clear of the
                     * toolbar (x 388-599, y 50-55) and the floor buttons (x 585). */
                    CHECK(b.x0==pos[p][0] && b.y0==pos[p][1]);
                    CHECK(pc_ui_side_anchor(b.x0,b.x1,640.f)==0.f);
                    CHECK(b.x1<388.f);
                    if(e) CHECK(b.y1<240.f);                   /* the upper half */
                    else CHECK(b.y1-b.y0<=line+2.f*MAPMAKER_TABLE_PAD+1e-3f);
                }
                for(o=0;o<sizeof outputs/sizeof outputs[0];o++) {
                    float w=(float)outputs[o][0],h=(float)outputs[o][1];
                    float k=h/480.f<w/640.f?h/480.f:w/640.f,left=(w-640.f*k)*.5f,top=(h-480.f*k)*.5f;
                    float x0=left+b.x0*k,x1=left+b.x1*k,y0=top+b.y0*k,y1=top+b.y1*k;
                    CHECK(x0>=left && x1<=left+640.f*k && y0>=top && y1<=top+480.f*k);
                    CHECK(x0>=0.f && x1<=w && y0>=0.f && y1<=h);
                    CHECK(line*k>=11.f);                         /* rows no tighter than at 640 x 480 */
                }
            }
        {
            MapMakerTableBox b;
            mapmaker_table_layout(NAN,INFINITY,1u+MAPMAKER_TABLE_ROWS,header_w,title_w,value_w,line,&b);
            CHECK(isfinite(b.x0) && isfinite(b.y0));
        }
    }
    /* The text fits its slots: the longest header at the largest values. */
    {
        char text[MAPMAKER_TABLE_TEXT];
        int len=snprintf(text,sizeof text,"\vs%d+ Fullest: %s %u/%u%s\vs100",MAPMAKER_TABLE_SCALE,"Teleports",
                         4294967295u,4294967295u," KB");
        CHECK(len<(int)MAPMAKER_TABLE_TEXT);
    }

    if(failures) return 1;
    printf("mapmaker restable ok\n");
    return 0;
}
