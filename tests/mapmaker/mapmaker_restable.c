/* Asset-free contract for src/interface/pc_mapmaker_restable.inc: the MapMaker
 * editor resource table replaces the capacity meter while the raised limits
 * are on; its rows show the numbers the editor's own checks compare
 * (pc_mapmaker_limits.inc, the per-category maxima, Story AI and logic
 * limits), coloured near and at their caps, inside the 640 x 480 canvas and
 * its left third (4:3 and wide outputs). The stock meter is kept otherwise.
 * Built by tests/mapmaker/test_mapmaker_restable.py, which passes the
 * editor/objects limits read from their sources with -D. */
#include <math.h>
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

#define TEXT_BUF 0x00600000u
static uint32_t heap_result=TEXT_BUF;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { (void)bytes;(void)alignment;return heap_result; }

/* Recorded draws: sub_0003FA10 (font) and sub_00041E30 (x, y, text, colour, outline). */
void sub_0003DFA0(void) {}
void sub_0003FA10(void) {}
void sub_00041E30(void) {}
static struct { float x,y; char text[64]; uint32_t colour,outline,align; } draws[32];
static unsigned ndraws,font_calls;
static uint32_t font_now=7u,font_seen;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t r=0;
    va_start(a,n);
    if(f==sub_0003FA10 && n==1) {
        r=font_now;font_now=va_arg(a,uint32_t);font_calls++;
        if(font_calls==1) font_seen=font_now;
    } else if(f==sub_00041E30 && n==5) {
        uint32_t x=va_arg(a,uint32_t),y=va_arg(a,uint32_t),text=va_arg(a,uint32_t);
        if(ndraws<32) {
            memcpy(&draws[ndraws].x,&x,4);memcpy(&draws[ndraws].y,&y,4);
            snprintf(draws[ndraws].text,sizeof draws[ndraws].text,"%s",(const char *)((uintptr_t)text+g_xbox_mem_offset));
            draws[ndraws].colour=va_arg(a,uint32_t);draws[ndraws].outline=va_arg(a,uint32_t);
            draws[ndraws].align=WORD_AT(0x007E7260u);
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
    memset(&mapmaker_table,0,sizeof mapmaker_table);
    mapmaker_budget.initialized=0;
}

#define ED 0x00320000u
#define TYPEA 0x00330000u
#define TYPEB 0x00330010u
#define STACK 0x00700000u
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
    ndraws=0;font_calls=0;originals=0;
}
static int drawn(const char *text) { unsigned i; for(i=0;i<ndraws && i<32;i++) if(!strcmp(draws[i].text,text)) return (int)i; return -1; }
static uint32_t colour_of(const char *row)
{
    char text[64];
    int i;
    snprintf(text,sizeof text,"\vs75%s\vs100",row);
    i=drawn(text);
    return i<0?0xDEADu:draws[i].colour;
}

/* Stock category maxima (0x00524BE0 + c*24 + 4); categories_apply raises them. */
static void stock_categories(void)
{
    static const uint32_t m[37]={32,32,16,1,1,1,4,1,32,32,2,2,2,2,2,2,2,2,2,16,16,12,1,16,50,20,20,8,8,8,10,32,20,66,66,4,4};
    unsigned i;
    for(i=0;i<37;i++) WORD_AT(0x00524BE0u+i*24u+4u)=m[i];
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
    /* Tile type 1: 10 pads, 9 cover, 20 links, 10,000 B; held type 2: 2, 9, 20, 1,000 B. */
    WORD_AT(TYPEA)=1;WORD_AT(TYPEB)=2;
    WORD_AT(0x00529400u+0x140u)=10;WORD_AT(0x00529404u+0x140u)=9;WORD_AT(0x00529408u+0x140u)=20;WORD_AT(0x00529410u+0x140u)=10000;
    WORD_AT(0x00529400u+0x280u)=2;WORD_AT(0x00529404u+0x280u)=9;WORD_AT(0x00529408u+0x280u)=20;WORD_AT(0x00529410u+0x280u)=1000;
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

    /* ---- limits off: the stock meter, no table ---- */
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_TABLE",NULL);
    pc_display_mapmaker_limits(fake_limits);
    meter_at(22.f,36.f);
    pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0 && g_esp==STACK+4u);

    /* ---- limits on, but the check has not run for this editor ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    meter_at(22.f,36.f);
    pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);

    /* ---- the table ---- */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(mapmaker_limits_view.valid && mapmaker_limits_view.editor==ED);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));             /* placement allowed */
    meter_at(22.f,36.f);
    pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==0);                               /* replaces the stock meter */
    CHECK(g_esp==STACK+4u);                            /* and does its ret */
    CHECK(ndraws==MAPMAKER_TABLE_ROWS);
    CHECK(font_seen==0x12u && font_now==7u && font_calls==2);    /* meter font, then restored */
    CHECK(WORD_AT(0x007E7260u)==0x55u && draws[0].align==0u);    /* left aligned while drawing */
    {
        /* Placed counts: 90 tiles; items 30+3+7+150+4+24+2+1 (ids > 9; 1 id-90 item too) + 5 free;
         * pads 900, cover 810, links 90*20 + 16*90; memory placed estimate / 1024 rounded up. */
        uint32_t items=30u+3u+7u+150u+4u+24u+2u+1u+1u+5u;
        uint32_t kb=(mapmaker_placed_memory(pieces)+1023u)/1024u;
        char want[15][48];
        snprintf(want[0],48,"Tiles: 90/511");
        snprintf(want[1],48,"Items: %u/500",(unsigned)items);
        snprintf(want[2],48,"Engine objects: %u/1023",(unsigned)(90u+items+64u));
        snprintf(want[3],48,"Pickups: 40/768");            /* weapons + keycards + health */
        snprintf(want[4],48,"Regular pads: 900/7680");
        snprintf(want[5],48,"Cover pads: 810/9984");
        snprintf(want[6],48,"Links: 3240/32256");
        snprintf(want[7],48,"Pad groups: 700/1024");
        snprintf(want[8],48,"Memory: %u/2000 KB",(unsigned)kb);
        snprintf(want[9],48,"Story AI: 37/511");
        snprintf(want[10],48,"Logic: 12/120");
        snprintf(want[11],48,"Movers: 150/160");
        snprintf(want[12],48,"Props: 4/160");
        snprintf(want[13],48,"Turrets: 26/72");
        snprintf(want[14],48,"Teleporters: 1/40");
        for(i=0;i<15;i++) {
            char text[64];
            snprintf(text,sizeof text,"\vs75%s\vs100",want[i]);
            CHECK(i<ndraws && !strcmp(draws[i].text,text));
            if(i<ndraws && strcmp(draws[i].text,text)) fprintf(stderr,"row %u: '%s' want '%s'\n",i,draws[i].text+4,want[i]);
            CHECK(draws[i].outline==0x80u);
            CHECK(draws[i].x==22.f && draws[i].y==36.f+12.f*(float)i);
        }
        /* The same numbers as the check: the meter fraction is the fullest of them. */
        CHECK(fabsf(FLOAT_AT(ED+0x270)-700.f/1024.f)<1e-4f);       /* pad groups are the fullest row */
        CHECK(colour_of(want[0])==MAPMAKER_TABLE_OK);
        CHECK(colour_of(want[11])==MAPMAKER_TABLE_NEAR);          /* movers 150/160 */
        CHECK(colour_of(want[13])==MAPMAKER_TABLE_FULL);          /* turret type 27 at 24/24 */
        CHECK(colour_of(want[12])==MAPMAKER_TABLE_OK);
        WORD_AT(0x007497B0u)=0x20u;                               /* blink phase */
        meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
        CHECK(colour_of(want[13])==MAPMAKER_TABLE_BLINK);
        WORD_AT(0x007497B0u)=0;
    }

    /* A refused cap turns its row red at the placed count (placed + held over). */
    WORD_AT(0x00762CB8u)=1024;                                    /* groups at the cap: allowed, red (full) */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(!(WORD_AT(ED+0x25c)&0x400000u));
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(colour_of("Pad groups: 1024/1024")==MAPMAKER_TABLE_FULL);
    WORD_AT(0x00762CB8u)=922;                                     /* 90% (922/1024) */
    pc_display_mapmaker_limits(fake_limits);
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(colour_of("Pad groups: 922/1024")==MAPMAKER_TABLE_NEAR);
    WORD_AT(0x00762CB8u)=700;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,800);         /* held: 800 x 9 cover = 7200 + 810 */
    WORD_AT(0x00529404u+0x280u)=12;                               /* 9600 + 810 > 9984: refused on cover */
    pc_display_mapmaker_limits(fake_limits);
    CHECK(WORD_AT(ED+0x25c)&0x400000u);
    CHECK(mapmaker_limits_view.over&MAPMAKER_CAP_COVER);
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(colour_of("Cover pads: 810/9984")==MAPMAKER_TABLE_FULL);  /* placed shown, refusal coloured */
    CHECK(colour_of("Regular pads: 900/7680")==MAPMAKER_TABLE_OK);
    WORD_AT(0x00529404u+0x280u)=9;
    WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    /* Pieces: 511 placed + 1 held refused, the row at 511/511 red. */
    WORD_AT(0x0078400Cu)=make_list(0x00400000u,TYPEB,511);
    pc_display_mapmaker_limits(fake_limits);
    CHECK(mapmaker_limits_view.over&MAPMAKER_CAP_PIECES);
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(colour_of("Tiles: 511/511")==MAPMAKER_TABLE_FULL);
    /* Engine objects: pieces + items + 64 characters, as the check. */
    WORD_AT(0x0078400Cu)=make_list(0x00400000u,TYPEB,400);
    WORD_AT(ED+0x306c)=500u<<16;                                  /* 400 + 500 + 64 = 964 */
    WORD_AT(ED+0x2f7c)=0;
    pc_display_mapmaker_limits(fake_limits);
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(drawn("\vs75Engine objects: 964/1023\vs100")>=0);
    CHECK(colour_of("Items: 500/500")==MAPMAKER_TABLE_FULL);
    CHECK(colour_of("Engine objects: 964/1023")==MAPMAKER_TABLE_NEAR);
    WORD_AT(ED+0x306c)=5u<<16;
    WORD_AT(0x0078400Cu)=pieces;WORD_AT(ED+0x2f7c)=make_list(0x00480000u,TYPEB,1);
    pc_display_mapmaker_limits(fake_limits);

    /* ---- the stock meter instead ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);                         /* limits turned off after a check */
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    probe_flags=MAPMAKER_PROBE_METER;                             /* the probe measures the stock meter */
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    probe_flags=0;
    WORD_AT(0x007844A8u)=ED+0x1000u;                              /* another editor object */
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    WORD_AT(0x007844A8u)=ED;
    WORD_AT(0x007844A8u)=0x05000000u;                             /* a frame whose check could not run */
    pc_display_mapmaker_limits(fake_limits);
    WORD_AT(0x007844A8u)=ED;
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    pc_display_mapmaker_limits(fake_limits);
    heap_result=0;memset(&mapmaker_table,0,sizeof mapmaker_table);  /* no text buffer */
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    heap_result=TEXT_BUF;
    set_env("TSFP_MAPMAKER_TABLE","0");
    set_env("TSFP_MAPMAKER_LIMITS","1");
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==1 && ndraws==0);
    set_env("TSFP_MAPMAKER_TABLE",NULL);set_env("TSFP_MAPMAKER_LIMITS","1");
    meter_at(22.f,36.f);pc_display_mapmaker_meter(fake_meter);
    CHECK(originals==0 && ndraws==MAPMAKER_TABLE_ROWS);

    /* ---- layout: 4:3 and wide outputs ----
     * The meter's own position (sub_0030C3C0): safe-area margins of the 640 x 480
     * canvas, x 22 or 32, y 36 or 24, plus the HUD slide. The editor HUD is drawn
     * in the centred 4:3 box on a wide output (mapmaker-widescreen.md), so the
     * guest rectangle maps to output = box_left + guest * h / 480. */
    {
        static const float pos[][2]={{22.f,36.f},{32.f,24.f},{22.f-40.f,36.f-64.f},{600.f,470.f}};
        static const unsigned outputs[][2]={{640,480},{1024,768},{1920,1080},{2560,1080},{3840,1080}};
        unsigned p,o;
        for(p=0;p<sizeof pos/sizeof pos[0];p++) {
            float xs[MAPMAKER_TABLE_ROWS],ys[MAPMAKER_TABLE_ROWS],right;
            right=mapmaker_table_layout(pos[p][0],pos[p][1],MAPMAKER_TABLE_ROWS,25u,xs,ys);
            CHECK(xs[0]>=MAPMAKER_TABLE_MARGIN && ys[0]>=MAPMAKER_TABLE_MARGIN);
            CHECK(ys[MAPMAKER_TABLE_ROWS-1]+MAPMAKER_TABLE_LINE<=480.f-MAPMAKER_TABLE_MARGIN+1e-3f);
            CHECK(right<=640.f-MAPMAKER_TABLE_MARGIN+1e-3f && right-xs[0]<0.98f*640.f);
            if(p<2) {
                /* At the meter: left third (the renderer's side anchor), clear of the
                 * toolbar (x 388-599, y 50-55) and floor buttons (x 585). */
                CHECK(xs[0]==pos[p][0] && ys[0]==pos[p][1]);
                CHECK(pc_ui_side_anchor(xs[0],right,640.f)==0.f);
                CHECK(right<388.f);
            }
            for(o=0;o<sizeof outputs/sizeof outputs[0];o++) {
                float w=(float)outputs[o][0],h=(float)outputs[o][1];
                float k=h/480.f<w/640.f?h/480.f:w/640.f,left=(w-640.f*k)*.5f,top=(h-480.f*k)*.5f;
                float x0=left+xs[0]*k,x1=left+right*k,y0=top+ys[0]*k,y1=top+(ys[MAPMAKER_TABLE_ROWS-1]+MAPMAKER_TABLE_LINE)*k;
                CHECK(x0>=left && x1<=left+640.f*k && y0>=top && y1<=top+480.f*k);
                CHECK(x0>=0.f && x1<=w && y0>=0.f && y1<=h);
                CHECK(MAPMAKER_TABLE_LINE*k>=12.f);   /* rows no tighter than at 640 x 480 */
            }
        }
        {
            float xs[MAPMAKER_TABLE_ROWS],ys[MAPMAKER_TABLE_ROWS];
            mapmaker_table_layout(NAN,INFINITY,MAPMAKER_TABLE_ROWS,25u,xs,ys);
            CHECK(isfinite(xs[0]) && isfinite(ys[0]));
        }
    }
    /* The text fits its buffer: the longest row at the largest values. */
    {
        char line[MAPMAKER_TABLE_TEXT];
        int len=snprintf(line,sizeof line,"\vs%d%s: %u/%u%s\vs100",MAPMAKER_TABLE_SCALE,"Engine objects",4294967295u,4294967295u," KB");
        CHECK(len<(int)MAPMAKER_TABLE_TEXT);
    }

    if(failures) return 1;
    printf("mapmaker restable ok\n");
    return 0;
}
