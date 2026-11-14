/* Execute the production mounted-gun/camera viewfinder and view-static
 * widescreen paths (src/interface/pc_display_ui.inc) over a flat guest image. */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_TLS
static uint8_t *mem;
static uint32_t g_esp;
#define WORD_AT(a) (*(uint32_t *)(mem+(uint32_t)(a)))
static int guest_writable(uint32_t a,size_t n){return a>=0x1000u && a+n<=0x800000u;}
static int wide=1,views=1,flushes;
static float hud=.75f;
static int pc_display_wide_now(void){return wide;}
static float pc_display_hud_scale(void){return hud;}
static void sub_00022FF0(void){flushes++;}
static void sub_001CDAE0(void){}
static uint32_t guest_call(void (*f)(void),unsigned n,...)
{
    (void)n;
    if(f==sub_001CDAE0)return (uint32_t)views;
    f();return 0;
}
static unsigned placement_count;
static void host_SetTwoDPlacement(int place,uint32_t tag)
{
    (void)place;(void)tag;
    placement_count++;
}
#include "interface/pc_ui_layout.h"
static int ui_placement;static uint32_t ui_tag;
/* PRODUCTION */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);exit(1);}}while(0)

#define STACK 0x00700000u
static void set_view(uint32_t right,uint32_t bottom,uint32_t left)
{
    uint32_t view=0x00600000u;
    WORD_AT(0x007E71C8u)=view;
    WORD_AT(view)=left;WORD_AT(view+4u)=right;WORD_AT(view+0x14u)=0;WORD_AT(view+0x18u)=bottom;
}
/* Guest call frame: return address then integer/float arguments. */
static void frame(uint32_t caller,int32_t x0,int32_t y0,int32_t x1,int32_t y1)
{
    g_esp=STACK;WORD_AT(g_esp)=caller;
    WORD_AT(g_esp+4u)=(uint32_t)x0;WORD_AT(g_esp+8u)=(uint32_t)y0;
    WORD_AT(g_esp+12u)=(uint32_t)x1;WORD_AT(g_esp+16u)=(uint32_t)y1;
}
static int32_t seen[2];static int seen_place;static uint32_t seen_tag;
static void draw(void)
{
    seen[0]=(int32_t)WORD_AT(g_esp+4u);seen[1]=(int32_t)WORD_AT(g_esp+12u);
    seen_place=ui_placement;seen_tag=ui_tag;
}
static void rect(uint32_t caller,int32_t x0,int32_t x1,int32_t want0,int32_t want1)
{
    frame(caller,x0,10,x1,20);
    ui_placement=0;ui_tag=0x4444u;
    CHECK(viewfinder_primitive(draw));
    CHECK(seen[0]==want0 && seen[1]==want1);
    CHECK(seen_place==1 && seen_tag==caller);
    CHECK(ui_placement==0 && ui_tag==0x4444u);
    CHECK((int32_t)WORD_AT(g_esp+4u)==x0 && (int32_t)WORD_AT(g_esp+12u)==x1);
}
static int vf_seen;
static void viewfinder_body(void){vf_seen=viewfinder_on;}
static int run_viewfinder(void)
{
    vf_seen=-1;pc_display_viewfinder(viewfinder_body);
    CHECK(viewfinder_on==0);
    return vf_seen;
}
static void label(float x,int want)
{
    uint32_t bits;memcpy(&bits,&x,4);
    frame(0x0014C5CCu,0,0,0,0);WORD_AT(g_esp+4u)=bits;
    ui_placement=0;
    CHECK(viewfinder_label(draw));
    CHECK(seen_place==want && ui_placement==0);
}

/* sub_000F8DC0 as the US code runs it: the tile width comes from the bound
 * record, two sub_00153B40 calls per tile. */
#define TABLE 0x00100000u
#define RECORDS 0x00101000u
#define TEXTURE 0x336u
static unsigned tiles,tile_width,static_place;
static uint32_t guest_rand(void)
{
    uint32_t lo=WORD_AT(0x004E7950u),hi=WORD_AT(0x004E7954u),n=WORD_AT(0x007497C8u);
    pc_ui_rand_advance(&lo,&hi,&n,1);
    WORD_AT(0x004E7950u)=lo;WORD_AT(0x004E7954u)=hi;WORD_AT(0x007497C8u)=n;
    return lo;
}
static void noise(void)
{
    uint32_t tex=WORD_AT(g_esp+4u),rec=WORD_AT(WORD_AT(0x004B85C8u))+tex*0x34u;
    int32_t x0=(int32_t)WORD_AT(g_esp+8u),y0=(int32_t)WORD_AT(g_esp+12u);
    int32_t x1=(int32_t)WORD_AT(g_esp+16u),y1=(int32_t)WORD_AT(g_esp+20u);
    unsigned w=WORD_AT(rec+0x24u)>>16,h=WORD_AT(rec+0x28u)>>16;
    tiles=0;tile_width=w;static_place=(unsigned)ui_placement;
    if(!w || !h)return;
    for(int32_t x=x0;x<x1;x+=(int32_t)w)
        for(int32_t y=y0;y<y1;y+=(int32_t)h){guest_rand();guest_rand();tiles++;}
}
static void static_frame(void)
{
    g_esp=STACK;WORD_AT(g_esp)=0x000FEB53u;WORD_AT(g_esp+4u)=TEXTURE;
    WORD_AT(g_esp+8u)=0;WORD_AT(g_esp+12u)=0;WORD_AT(g_esp+16u)=639;WORD_AT(g_esp+20u)=479;
    WORD_AT(g_esp+24u)=0x7Fu;
    WORD_AT(0x004B85C8u)=TABLE;WORD_AT(TABLE)=RECORDS;WORD_AT(TABLE+8u)=0;WORD_AT(TABLE+0xCu)=0x400u;
    uint32_t rec=RECORDS+TEXTURE*0x34u;
    WORD_AT(rec+0x24u)=0x00400011u;WORD_AT(rec+0x28u)=0x00400022u; /* w=h=64 */
    WORD_AT(0x004E7950u)=0x12345678u;WORD_AT(0x004E7954u)=0x9ABCDEF0u;WORD_AT(0x007497C8u)=7u;
}

int main(void)
{
    mem=calloc(1,0x800000u);CHECK(mem);
    WORD_AT(0x0079094Cu)=100;set_view(639,479,0);
    /* Viewfinder scope: only a wide, single, complete view. */
    CHECK(run_viewfinder()==1);
    wide=0;CHECK(run_viewfinder()==0);wide=1;
    hud=1.25f;CHECK(run_viewfinder()==0);hud=.75f;
    views=2;CHECK(run_viewfinder()==0);views=1;
    set_view(319,479,0);CHECK(run_viewfinder()==0);set_view(639,479,0);
    puts("phase scope");

    /* Primitives at 16:9 (k = 0.75). */
    viewfinder_on=1;
    rect(0x0014C3D7u,0,640,0,640);       /* top bar: full width */
    rect(0x0014C437u,19,622,14,627);     /* green tint: frame span, edges kept */
    rect(0x0014C484u,19,622,14,627);     /* scanline */
    rect(0x0014C40Au,0,20,0,15);         /* left bar: left edge */
    rect(0x0014C422u,621,640,626,640);   /* right bar: right edge */
    rect(0x0014C516u,17,57,13,43);       /* left bracket keeps 4:3 shape */
    rect(0x0014C562u,583,623,597,627);   /* right bracket */
    rect(0x0014C753u,300,340,305,335);   /* centred piece fits about the centre */
    hud=.375f;                           /* 32:9 */
    rect(0x0014C40Au,0,20,0,8);
    hud=.75f;
    frame(0x0014C12Cu,17,0,57,0);CHECK(!viewfinder_primitive(draw)); /* aim reticle */
    frame(0x0014C962u,17,0,57,0);CHECK(!viewfinder_primitive(draw));
    viewfinder_on=0;
    frame(0x0014C516u,17,0,57,0);CHECK(!viewfinder_primitive(draw));
    viewfinder_on=1;
    label(30.f,3);label(300.f,2);label(560.f,4);
    viewfinder_on=0;
    puts("phase primitives");

    /* View static: same texel size over the window, guest random sequence kept. */
    static_frame();
    hud=1.f;wide=0;
    pc_display_view_static(noise);
    unsigned stock_tiles=tiles;
    uint32_t want_lo=WORD_AT(0x004E7950u),want_hi=WORD_AT(0x004E7954u),want_n=WORD_AT(0x007497C8u);
    CHECK(stock_tiles==10u*8u && tile_width==64u);
    static_frame();hud=.75f;wide=1;placement_count=0;ui_placement=0;
    pc_display_view_static(noise);
    CHECK(tile_width==48u && tiles==14u*8u && static_place==1u);
    CHECK(ui_placement==0 && placement_count==2);
    CHECK(WORD_AT(RECORDS+TEXTURE*0x34u+0x24u)==0x00400011u);
    CHECK(WORD_AT(0x004E7950u)==want_lo && WORD_AT(0x004E7954u)==want_hi && WORD_AT(0x007497C8u)==want_n);
    static_frame();WORD_AT(g_esp+16u)=319;pc_display_view_static(noise);
    CHECK(tile_width==64u);                 /* partial rectangle: stock */
    static_frame();WORD_AT(g_esp+4u)=0x500u;placement_count=0;
    pc_display_view_static(noise);CHECK(placement_count==0); /* invalid texture */
    puts("phase static");
    puts("PASS: viewfinder and view static fill a wide window at 4:3 proportions");
    return 0;
}
