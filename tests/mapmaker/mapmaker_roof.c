/* Asset-free contract for src/interface/pc_mapmaker_roof.inc (MapMaker "Open roof" and
 * "Open air" tiles): inert without TSFP_MAPMAKER_LIMITS=1 and outside the
 * editor; the palette's last tile category gets two entries only while the
 * menu is built; hover shows the open mask and the pick sees the stock one;
 * the picked piece's masks lose the ceiling (and side) bits; rebuilds store
 * the mark in record word +0x26 and the open mask in +8; loads put the marks
 * back; reused pieces and resets forget them; placing/picking up copies a
 * piece and the mark follows it to the copy (place -> save -> play reads +8
 * -> reload); freed pieces forget it; the open entries and pieces get their
 * own names and the next list numbers 46 / 47 (palette caption "%d. ",
 * grid label "%d.%s"); records with item sub-records (start points) are
 * walked whole, so a play -> editor reload -> copy -> play round trip keeps
 * every open tile; every hook pops only its return address. */
#define TSFP_MAPMAKER_ROOF_TEST 1
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) { g_ecx=6; }
static uint32_t guest_call(void (*f)(void),int n,...) { (void)f;(void)n;return 0; }
static uint32_t heap_result=0x00300000u;
static int heap_calls;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p=heap_result;
    (void)alignment;heap_calls++;
    if(p) { memset((char *)memory+p,0xCD,bytes);heap_result+=0x1000u; }
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_roof.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

/* ---- fake guest ---------------------------------------------------------- */
#define ED 0x00100000u            /* editor object [0x007844A8] */
#define PIECES 0x00110000u        /* editor pieces, 0x48 bytes */
#define C 0x00200000u             /* container */
#define MENU 0x00140000u
#define ITEMS 0x00150000u         /* menu items, 0x40 apart */
#define PAL 0x00160000u           /* palette [0x00778D34] */
#define PROP 0x00161000u
#define INST 0x00162000u
#define STOCK_PREVIEW 0x020001C7u
static const uint32_t full_counts[8]={11,6,4,5,7,5,5,2},basic_counts[3]={11,1,4};

static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg0,uint32_t arg1)
{
    uint32_t r=g_eax;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=arg0;WORD_AT(ESP0+8)=arg1;
    g_eax=r;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
}
static uint32_t piece(unsigned i) { return PIECES+i*0x48u; }
static void make_piece(unsigned i,uint32_t tile,int x,int y,int z,int rot,uint32_t next)
{
    uint32_t p=piece(i);
    memset((char *)memory+p,0,0x48);
    FLOAT_AT(p)=(float)x;FLOAT_AT(p+4)=(float)y;FLOAT_AT(p+8)=(float)z;
    WORD_AT(p+0xC)=(uint32_t)rot;WORD_AT(p+0x10)=tile;WORD_AT(p+0x40)=next;
}
static void make_tables(void)
{
    unsigned i,j;
    for(i=0;i<8;i++) {
        uint32_t list=0x0052D000u+i*0x40u;
        WORD_AT(MAPMAKER_ROOF_CATS_FULL+i*12u)=0xA2Fu+i;WORD_AT(MAPMAKER_ROOF_CATS_FULL+i*12u+4)=list;
        WORD_AT(MAPMAKER_ROOF_CATS_FULL+i*12u+8)=full_counts[i];
        for(j=0;j<full_counts[i];j++) WORD_AT(list+j*4u)=i*10u+j;
    }
    for(i=0;i<3;i++) {
        WORD_AT(MAPMAKER_ROOF_CATS_BASIC+i*12u)=0xA2Fu+i;WORD_AT(MAPMAKER_ROOF_CATS_BASIC+i*12u+4)=0x0052D000u+i*0x40u;
        WORD_AT(MAPMAKER_ROOF_CATS_BASIC+i*12u+8)=basic_counts[i];
    }
}
static uint32_t tables_sum(void)
{
    uint32_t h=0,i;
    for(i=0;i<(8u+3u)*3u;i++) h=h*31u+WORD_AT(MAPMAKER_ROOF_CATS_FULL+i*4u);
    for(i=0;i<0x200u/4u;i++) h=h*31u+WORD_AT(0x0052D000u+i*4u);
    return h;
}

/* sub_00316B50: for each category, for each entry, sub_000792B0 (the menu
 * add, return address 0x00316C84) returning a new item with +0x14 = entry. */
static uint32_t seen_list[64],seen_count;
static void add_original(void) { g_eax=ITEMS+seen_count*0x40u;WORD_AT(g_eax+0x14)=WORD_AT(g_esp+8);g_esp+=4u; }
static void init_original(void)
{
    uint32_t table=WORD_AT(WORD_AT(0x007844A8u)+0x200u)?MAPMAKER_ROOF_CATS_FULL:MAPMAKER_ROOF_CATS_BASIC;
    uint32_t rows=table==MAPMAKER_ROOF_CATS_FULL?8u:3u,i,j,save=g_esp,ret=WORD_AT(g_esp);
    seen_count=0;
    g_esp=0x00610000u;WORD_AT(g_esp)=0x00316BEEu;WORD_AT(g_esp+8)=0;   /* another item of this menu first */
    pc_mapmaker_roof_menu_add(add_original);
    if(g_esp!=0x00610004u) esp_errors++;
    for(i=0;i<rows;i++)
        for(j=0;j<WORD_AT(table+i*12u+8u);j++) {
            uint32_t entry=WORD_AT(WORD_AT(table+i*12u+4u)+j*4u);
            g_esp=0x00610000u;WORD_AT(g_esp)=MAPMAKER_ROOF_RET_MENU_ADD;WORD_AT(g_esp+4)=0;WORD_AT(g_esp+8)=entry;
            pc_mapmaker_roof_menu_add(add_original);
            if(g_esp!=0x00610004u) esp_errors++;
            if(seen_count<64) seen_list[seen_count]=g_eax;
            seen_count++;
        }
    g_esp=save;(void)ret;g_esp+=4u;
}
static void other_menu_add(void)                    /* another menu's add: not counted */
{
    g_esp=0x00610000u;WORD_AT(g_esp)=0x00071234u;WORD_AT(g_esp+8)=MAPMAKER_ROOF_BASE_TILE;
    pc_mapmaker_roof_menu_add(add_original);
    if(g_esp!=0x00610004u) esp_errors++;
    g_esp=ESP0;
}
static void build_menu(void) { hook(pc_mapmaker_roof_menu_init,init_original,0x00075E80u,MENU,0); }

/* sub_00316920: the stock hover sets the preview mask (or keeps it for the
 * same tile id). */
static uint32_t last_hover=0xFFFFFFFFu;
static void hover_original(void)
{
    uint32_t id=WORD_AT(WORD_AT(g_esp+4)+0x18C);
    if(id!=last_hover) WORD_AT(INST+0x30)=STOCK_PREVIEW+id;
    last_hover=id;g_esp+=4u;
}
static void highlight(uint32_t item) { WORD_AT(MENU+0x198)=item;WORD_AT(MENU+0x18C)=WORD_AT(item+0x14); }
static void hover(uint32_t item) { highlight(item);hook(pc_mapmaker_roof_menu_hover,hover_original,0x00074500u,MENU,0); }

/* sub_00316500: reads the preview, creates the held piece. */
static uint32_t pick_saw;
static unsigned next_piece=20;
static void pick_original(void)
{
    uint32_t id=WORD_AT(WORD_AT(g_esp+4)+0x18C);
    pick_saw=WORD_AT(INST+0x30);
    make_piece(next_piece,id,5,6,0,0,0);
    WORD_AT(ED+0x2F7C)=piece(next_piece++);
    g_esp+=4u;
}
static void pick(uint32_t item) { highlight(item);hook(pc_mapmaker_roof_menu_pick,pick_original,0x00073E00u,MENU,0); }

/* sub_002FC680 / sub_002FC470. */
static uint32_t mask_value;
static void mask_original(void) { g_eax=mask_value;g_esp+=4u; }
static uint32_t mask(uint32_t p) { mask_value=0xFFFFFFFFu;hook(pc_mapmaker_roof_mask,mask_original,0x00300CA9u,p,0);return g_eax; }
static uint32_t reduced(uint32_t p) { mask_value=0x0B000000u;hook(pc_mapmaker_roof_reduced,mask_original,0x00300BFBu,p,0);return g_eax; }

/* sub_003006C0: writes one record per list piece: 0x28 bytes + pads*8
 * (+0x24, here tile % 3 pads) + the piece's items (count +0x1E; here the
 * fixture's piece word +0x3C), item k being ((k+1)*4 + 0x1B) & ~3 bytes of
 * junk with its count byte +0x14 = k+1 (0x00300D60, 0x00300D93..0x0030116E). */
static int skew;
#define ITEM_BYTES(k) ((((k)+1u)*4u+0x1Bu)&~3u)
static void compile_original(void)
{
    uint32_t c=WORD_AT(g_esp+4),p=WORD_AT(0x0078400Cu),r=c+0x10u,start,first=p;
    memset((char *)memory+c,0,0x2000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77668u;WORD_AT(c+12)=0;              /* (empty) info chunk */
    WORD_AT(r)=0x00C77669u;start=r+8u;r=start;
    for(;p;p=WORD_AT(p+0x40)) {
        uint32_t pads=WORD_AT(p+0x10)%3u,items=WORD_AT(p+0x3C),k;
        WORD_AT(r)=WORD_AT(p+0x10)+(skew && p==first?1u:0u);
        WORD_AT(r+4)=((uint32_t)(int32_t)FLOAT_AT(p)&0xFFu)|((uint32_t)(int32_t)FLOAT_AT(p+4)&0xFFu)<<8|
                     ((uint32_t)(int32_t)FLOAT_AT(p+8)&0xFFu)<<16|(WORD_AT(p+0xC)&0xFFu)<<24;
        WORD_AT(r+8)=0x03FFFFFFu;
        WORD_AT(r+0x1C)=items<<16;
        WORD_AT(r+0x24)=0x28u+pads*8u;
        r+=0x28u+pads*8u;
        for(k=0;k<items;k++) {
            memset((char *)memory+r,0x77,ITEM_BYTES(k));
            *((unsigned char *)memory+r+0x14)=(unsigned char)(k+1u);
            r+=ITEM_BYTES(k);
        }
    }
    WORD_AT(start-4)=r-start;
    WORD_AT(r)=0x00C7766Au;
    g_esp+=4u;
}
static void compile(void) { hook(pc_mapmaker_roof_compile,compile_original,0x00313B01u,C,3u); }
static uint32_t record(unsigned n)
{
    uint32_t r=C+0x18u;
    while(n--) {
        uint32_t items=WORD_AT(r+0x1C)>>16,k;
        r+=WORD_AT(r+0x24)&0xFFFFu;
        for(k=0;k<items;k++) r+=ITEM_BYTES(k);
    }
    return r;
}
/* The editor start after a play (sub_00302A60 -> sub_00301A90 at
 * 0x00303076): a new heap, so new piece pointers, made in record order. */
static void reload_pieces(unsigned base,unsigned n)
{
    unsigned i;
    for(i=0;i<n;i++) {
        uint32_t r=record(i),xyz=WORD_AT(r+4);
        make_piece(base+i,WORD_AT(r),(int8_t)(xyz&0xFFu),(int8_t)(xyz>>8&0xFFu),(int8_t)(xyz>>16&0xFFu),
                   (int8_t)(xyz>>24),i+1u<n?piece(base+i+1u):0u);
        WORD_AT(piece(base+i)+0x3C)=WORD_AT(r+0x1C)>>16;
    }
    WORD_AT(0x0078400Cu)=piece(base);
}

static void plain_original(void) { g_esp+=4u; }
static void load_original(void) { g_esp+=4u; }   /* the pieces are made by the test */
static void load(void) { hook(pc_mapmaker_roof_load,load_original,0x0032BB6Eu,C,0); }
static int count_record(uint32_t r,void *ctx) { (void)r;(void)ctx;return 1; }
static uint32_t new_result;
static void new_original(void) { g_eax=new_result;g_esp+=4u; }

/* sub_003037A0 / pickup: the editor copies the 0x48 bytes into a new piece,
 * then sub_002F95E0(old, new) (move) or sub_00308120(src, dst) (copy), and
 * frees the old one with sub_00308480. */
static void copy_bytes(uint32_t from,uint32_t to) { memcpy((char *)memory+to,(char *)memory+from,0x48); }
static void move_piece(uint32_t from,uint32_t to)
{
    copy_bytes(from,to);
    hook(pc_mapmaker_roof_move,plain_original,0x00303883u,from,to);
}
static void copy_piece(uint32_t from,uint32_t to)
{
    copy_bytes(from,to);
    hook(pc_mapmaker_roof_copy,plain_original,0x002F9BBBu,from,to);
}
static void free_piece(uint32_t p) { hook(pc_mapmaker_roof_piece_free,plain_original,0x00308579u,p,0); }
/* sub_0008A780 0x0008A91D: play takes the record's +8 as the room mask. */
static uint32_t play_mask(unsigned n) { return WORD_AT(record(n)+8); }

/* sub_0003D7B0: the stock text lookup. */
#define STOCK_TEXT 0x00400000u
static void text_original(void) { g_eax=STOCK_TEXT+WORD_AT(g_esp+4);g_esp+=4u; }
static const char *text(uint32_t ret,uint32_t id)
{
    hook(pc_mapmaker_roof_text,text_original,ret,id,0);
    return g_eax>=STOCK_TEXT?"stock":(const char *)memory+g_eax;
}
static uint32_t text_ptr(uint32_t ret) { hook(pc_mapmaker_roof_text,text_original,ret,0xA3Fu,0);return g_eax; }

/* sub_00151030(format, ...) at 0x00311724: "%d.%s" with the tile number and
 * the name; returns the number it printed. */
static uint32_t printed;
static void format_original(void) { printed=WORD_AT(g_esp+8);g_esp+=4u; }
static uint32_t format(uint32_t ret,uint32_t fmt,uint32_t number,uint32_t name)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=fmt;WORD_AT(ESP0+8)=number;WORD_AT(ESP0+12)=name;
    pc_mapmaker_roof_format(format_original);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after the format hook\n",(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
    return printed;
}
/* sub_0032D9F0 (eax number or -1, ecx name) at 0x0032E5DE: "%d. " + name. */
static uint32_t drawn;
static void caption_original(void) { drawn=g_eax;g_esp+=4u; }
static uint32_t caption(uint32_t ret,uint32_t number,uint32_t name)
{
    g_eax=number;g_ecx=name;
    hook(pc_mapmaker_roof_caption,caption_original,ret,0,0);
    return drawn;
}

int main(void)
{
    uint32_t h0,roof_item,air_item,stock_item,p;
    g_xbox_mem_offset=(uintptr_t)memory;
    make_tables();h0=tables_sum();
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x00784014u)=C;WORD_AT(ED+0x200)=1;
    WORD_AT(0x00778D34u)=PAL;WORD_AT(PAL+0x84)=PROP;WORD_AT(PROP+4)=INST;WORD_AT(INST+0x30)=STOCK_PREVIEW;
    make_piece(0,3,1,1,0,0,piece(1));make_piece(1,MAPMAKER_ROOF_BASE_TILE,2,1,0,1,piece(2));make_piece(2,7,3,1,1,2,0);
    WORD_AT(piece(0)+0x3C)=2u;                              /* a start point and a weapon on the first tile */
    WORD_AT(0x0078400Cu)=piece(0);

    /* ---- limits off: stock ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_POOLS",NULL);
    WORD_AT(0x0079094Cu)=102u;
    build_menu();
    CHECK(seen_count==45u && tables_sum()==h0 && heap_calls==0 && !mapmaker_roof.labels);
    CHECK(!mapmaker_roof.item[0] && !mapmaker_roof.item[1]);
    mapmaker_roof_set(piece(1),MAPMAKER_ROOF_OPEN_AIR);   /* even a stale mark does nothing */
    CHECK(mask(piece(1))==0xFFFFFFFFu && reduced(piece(1))==0x0B000000u);
    compile();
    CHECK((WORD_AT(record(1)+0x24)>>16)==0 && WORD_AT(record(1)+8)==0x03FFFFFFu);
    mapmaker_roof_test_reset();

    /* ---- limits on, editor: the palette ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    build_menu();
    CHECK(seen_count==47u && tables_sum()==h0 && heap_calls==2 && mapmaker_roof.labels);
    CHECK(WORD_AT(seen_list[45]+0x14)==MAPMAKER_ROOF_BASE_TILE && WORD_AT(seen_list[46]+0x14)==MAPMAKER_ROOF_BASE_TILE);
    CHECK(WORD_AT(seen_list[44]+0x14)==71u);                /* the last stock entry stays in place */
    CHECK(mapmaker_roof.item[0]==seen_list[45] && mapmaker_roof.item[1]==seen_list[46]);
    roof_item=seen_list[45];air_item=seen_list[46];stock_item=seen_list[1];
    other_menu_add();
    CHECK(mapmaker_roof.item[0]==roof_item && mapmaker_roof.item[1]==air_item);
    WORD_AT(ED+0x200)=0;                                    /* the 3-category palette */
    build_menu();
    CHECK(seen_count==18u && tables_sum()==h0 && heap_calls==2);
    CHECK(mapmaker_roof.item[0]==seen_list[16] && mapmaker_roof.item[1]==seen_list[17]);
    WORD_AT(ED+0x200)=1;
    build_menu();
    roof_item=seen_list[45];air_item=seen_list[46];

    /* hover: preview open, back to stock on leaving */
    hover(stock_item);
    CHECK(WORD_AT(INST+0x30)==STOCK_PREVIEW+1u);
    hover(air_item);
    CHECK(WORD_AT(INST+0x30)==((STOCK_PREVIEW+8u)&~0x02FFFFFFu));
    hover(roof_item);                                       /* same id: the stock keeps the mask */
    CHECK(WORD_AT(INST+0x30)==((STOCK_PREVIEW+8u)&~0x02000000u));
    hover(stock_item);
    CHECK(WORD_AT(INST+0x30)==STOCK_PREVIEW+1u);
    hover(air_item);
    highlight(seen_list[0]+0x10000u);WORD_AT(MENU+0x18C)=MAPMAKER_ROOF_BASE_TILE;  /* id 8 elsewhere */
    hook(pc_mapmaker_roof_menu_hover,hover_original,0x00074500u,MENU,0);
    CHECK(WORD_AT(INST+0x30)==STOCK_PREVIEW+8u);            /* restored for the stock tile 8 */

    /* names: the palette caption of a highlighted open entry */
    hover(air_item);
    CHECK(!strcmp(text(MAPMAKER_ROOF_RET_CAPTION,0xA3Fu),"Small Open (Open Air)"));
    CHECK(!strcmp(text(0x0032E5D9u,0xA3Fu),"stock"));          /* another caller */
    CHECK(!strcmp(text(MAPMAKER_ROOF_RET_CAPTION,0xA40u),"stock")); /* another tile */
    hover(roof_item);
    CHECK(!strcmp(text(MAPMAKER_ROOF_RET_CAPTION,0xA3Fu),"Small Open (No Roof)"));
    WORD_AT(0x0079094Cu)=100u;
    CHECK(!strcmp(text(MAPMAKER_ROOF_RET_CAPTION,0xA3Fu),"stock"));
    WORD_AT(0x0079094Cu)=102u;
    hover(stock_item);
    CHECK(!strcmp(text(MAPMAKER_ROOF_RET_CAPTION,0xA3Fu),"stock"));
    /* the caption's number: 46 / 47 after the stock 1..45; none stays none */
    {
        uint32_t roof_name=mapmaker_roof.labels,air_name=mapmaker_roof.labels+MAPMAKER_ROOF_LABEL_BYTES;
        CHECK(caption(MAPMAKER_ROOF_RET_CAPTION_DRAW,1u,roof_name)==46u);
        CHECK(caption(MAPMAKER_ROOF_RET_CAPTION_DRAW,1u,air_name)==47u);
        CHECK(caption(MAPMAKER_ROOF_RET_CAPTION_DRAW,1u,STOCK_TEXT+0xA3Fu)==1u);
        CHECK(caption(MAPMAKER_ROOF_RET_CAPTION_DRAW,0xFFFFFFFFu,roof_name)==0xFFFFFFFFu); /* 3-category palette */
        CHECK(caption(0x0032E719u,7u,air_name)==7u);                                   /* another row */
        CHECK(format(MAPMAKER_ROOF_RET_HOVER_FORMAT,MAPMAKER_ROOF_HOVER_FORMAT,1u,roof_name)==46u);
        CHECK(format(MAPMAKER_ROOF_RET_HOVER_FORMAT,MAPMAKER_ROOF_HOVER_FORMAT,1u,air_name)==47u);
        CHECK(format(MAPMAKER_ROOF_RET_HOVER_FORMAT,MAPMAKER_ROOF_HOVER_FORMAT,1u,STOCK_TEXT+0xA3Fu)==1u);
        CHECK(format(0x00311000u,MAPMAKER_ROOF_HOVER_FORMAT,1u,roof_name)==1u);         /* another caller */
        CHECK(format(MAPMAKER_ROOF_RET_HOVER_FORMAT,0x0049F838u,1u,roof_name)==1u);     /* another format */
        CHECK(!strcmp((const char *)memory+roof_name,"Small Open (No Roof)") &&
              !strcmp((const char *)memory+air_name,"Small Open (Open Air)"));
    }

    /* pick: the stock code sees the stock preview; the held piece is marked */
    hover(air_item);
    pick(air_item);
    CHECK(pick_saw==STOCK_PREVIEW+8u);
    CHECK(WORD_AT(INST+0x30)==((STOCK_PREVIEW+8u)&~0x02FFFFFFu));
    p=WORD_AT(ED+0x2F7C);
    CHECK(mapmaker_roof_mode(p)==MAPMAKER_ROOF_OPEN_AIR && mapmaker_roof.placed==1u);
    pick(stock_item);
    CHECK(mapmaker_roof_mode(WORD_AT(ED+0x2F7C))==0);
    hover(roof_item);pick(roof_item);
    CHECK(mapmaker_roof_mode(WORD_AT(ED+0x2F7C))==MAPMAKER_ROOF_OPEN_ROOF);

    /* masks */
    CHECK(mask(p)==~0x02FFFFFFu && reduced(p)==0x09000000u);
    CHECK(mask(WORD_AT(ED+0x2F7C))==~0x02000000u);
    CHECK(mask(piece(0))==0xFFFFFFFFu && reduced(piece(0))==0x0B000000u);
    WORD_AT(0x0079094Cu)=100u;
    CHECK(mask(p)==0xFFFFFFFFu && reduced(p)==0x0B000000u);
    WORD_AT(0x0079094Cu)=102u;

    /* place: the held piece is copied into a new list piece; the mark moves
     * with it, the freed held piece forgets it (reused pointer stays closed) */
    {
        uint32_t held=WORD_AT(ED+0x2F7C),placed=piece(30),dup=piece(31),list=WORD_AT(0x0078400Cu);
        move_piece(held,placed);
        CHECK(mapmaker_roof_mode(placed)==MAPMAKER_ROOF_OPEN_ROOF);
        free_piece(held);
        CHECK(mapmaker_roof_mode(held)==0 && mapmaker_roof.freed==1u);
        CHECK(mask(placed)==~0x02000000u);
        WORD_AT(piece(2)+0x40)=placed;WORD_AT(placed+0x40)=0;
        FLOAT_AT(placed)=4.0f;FLOAT_AT(placed+4)=2.0f;
        WORD_AT(ED+0x2F18)=placed;                             /* selected piece's name */
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_PIECE_NAME,0xA3Fu),"Small Open (No Roof)"));
        WORD_AT(ED+0x2F18)=piece(0);
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_PIECE_NAME,0xA3Fu),"stock"));
        /* save (rebuild, here the hand-over in scene 100): marker and open
         * mask in the record; play reads +8 */
        WORD_AT(0x0079094Cu)=100u;compile();WORD_AT(0x0079094Cu)=102u;
        CHECK((WORD_AT(record(3)+0x24)>>16)==0x5201u);
        CHECK(play_mask(3)==(0x03FFFFFFu&~0x02000000u) && (play_mask(3)&0x01000000u));
        CHECK(play_mask(0)==0x03FFFFFFu && play_mask(1)==0x03FFFFFFu && play_mask(2)==0x03FFFFFFu);
        /* copy (pick up a copy): both keep the mark */
        copy_piece(placed,dup);
        CHECK(mapmaker_roof_mode(dup)==MAPMAKER_ROOF_OPEN_ROOF && mapmaker_roof_mode(placed)==MAPMAKER_ROOF_OPEN_ROOF);
        free_piece(dup);
        CHECK(mapmaker_roof_mode(dup)==0 && mapmaker_roof_mode(placed)==MAPMAKER_ROOF_OPEN_ROOF);
        copy_piece(piece(0),dup);                              /* a closed source marks nothing */
        CHECK(mapmaker_roof_mode(dup)==0);
        /* reload: the reset forgets, the load matches the record again */
        hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
        CHECK(mapmaker_roof_mode(placed)==0);
        load();
        CHECK(mapmaker_roof_mode(placed)==MAPMAKER_ROOF_OPEN_ROOF);
        compile();
        CHECK((WORD_AT(record(3)+0x24)>>16)==0x5201u && play_mask(3)==(0x03FFFFFFu&~0x02000000u));
        /* open air through the same path: walls and ceiling gone, floor kept */
        mapmaker_roof_set(placed,MAPMAKER_ROOF_OPEN_AIR);
        move_piece(placed,dup);free_piece(placed);
        CHECK(mapmaker_roof_mode(dup)==MAPMAKER_ROOF_OPEN_AIR && mapmaker_roof_mode(placed)==0);
        WORD_AT(piece(2)+0x40)=dup;
        compile();
        CHECK((WORD_AT(record(3)+0x24)>>16)==0x5202u && play_mask(3)==0x01000000u);
        WORD_AT(piece(2)+0x40)=0;WORD_AT(0x0078400Cu)=list;
        mapmaker_roof_set(dup,0);
    }

    /* rebuild: the placed list piece 1 is open air */
    mapmaker_roof_set(piece(1),MAPMAKER_ROOF_OPEN_AIR);
    compile();
    CHECK((WORD_AT(record(1)+0x24)>>16)==0x5202u);
    CHECK((WORD_AT(record(1)+0x24)&0xFFFFu)==0x28u+8u*(MAPMAKER_ROOF_BASE_TILE%3u));
    CHECK(WORD_AT(record(1)+8)==(0x03FFFFFFu&~0x02FFFFFFu));
    CHECK((WORD_AT(record(0)+0x24)>>16)==0 && WORD_AT(record(0)+8)==0x03FFFFFFu);
    CHECK((WORD_AT(record(2)+0x24)>>16)==0 && WORD_AT(record(2)+8)==0x03FFFFFFu);
    CHECK(mapmaker_roof.mismatch==0u);
    skew=1;compile();skew=0;                                /* records not in list order: nothing stored */
    CHECK(mapmaker_roof.mismatch==1u && (WORD_AT(record(1)+0x24)>>16)==0 && WORD_AT(record(1)+8)==0x03FFFFFFu);

    /* reset forgets; load restores from the records */
    compile();
    hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
    CHECK(mapmaker_roof.marked==0u && mapmaker_roof_mode(piece(1))==0);
    load();
    CHECK(mapmaker_roof_mode(piece(1))==MAPMAKER_ROOF_OPEN_AIR && mapmaker_roof_mode(piece(0))==0 && mapmaker_roof_mode(piece(2))==0);
    CHECK(mapmaker_roof.restored==2u);                       /* 1 from the place test */
    hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
    FLOAT_AT(piece(1)+4)=9.0f;                              /* moved: no match */
    load();
    CHECK(mapmaker_roof_mode(piece(1))==0 && mapmaker_roof.unmatched==1u);
    FLOAT_AT(piece(1)+4)=1.0f;
    WORD_AT(record(1)+0x24)=(WORD_AT(record(1)+0x24)&0xFFFFu)|0x52070000u;   /* unknown mode: ignored */
    load();
    CHECK(mapmaker_roof_mode(piece(1))==0 && mapmaker_roof.unmatched==1u);
    WORD_AT(record(1)+0x24)=(WORD_AT(record(1)+0x24)&0xFFFFu)|0x52010000u;
    WORD_AT(C)=0;load();                                    /* refused container: nothing */
    CHECK(mapmaker_roof_mode(piece(1))==0);
    WORD_AT(C)=0x00C77667u;load();
    CHECK(mapmaker_roof_mode(piece(1))==MAPMAKER_ROOF_OPEN_ROOF);
    CHECK(mapmaker_roof_records(C,count_record,NULL)==3);
    WORD_AT(record(0)+0x24)=0x0010u;load();                 /* malformed record size: stops */
    CHECK(mapmaker_roof.restored==3u && mapmaker_roof_records(C,count_record,NULL)==-1);
    WORD_AT(record(0)+0x24)=0x1000u;
    CHECK(mapmaker_roof_records(C,count_record,NULL)==-1);  /* past the chunk */

    /* a reused piece pointer starts closed */
    new_result=piece(1);
    hook(pc_mapmaker_roof_piece_new,new_original,0x003021C6u,3u,0);
    CHECK(mapmaker_roof_mode(piece(1))==0);

    /* ---- play -> editor -> copy -> play (v0.4.0-mapmaker.7 report) ----
     * A start point (items) on the first tile, open tiles after it, one
     * open tile with an item of its own. */
    hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
    make_piece(40,3,0,0,0,0,piece(41));WORD_AT(piece(40)+0x3C)=3u;
    make_piece(41,MAPMAKER_ROOF_BASE_TILE,1,0,0,0,piece(42));
    make_piece(42,MAPMAKER_ROOF_BASE_TILE,2,0,0,1,piece(43));WORD_AT(piece(42)+0x3C)=1u;
    make_piece(43,7,3,0,0,0,0);
    WORD_AT(0x0078400Cu)=piece(40);
    mapmaker_roof_set(piece(41),MAPMAKER_ROOF_OPEN_ROOF);mapmaker_roof_set(piece(42),MAPMAKER_ROOF_OPEN_AIR);
    {
        unsigned long long mism=mapmaker_roof.mismatch;
        uint32_t roof_name=mapmaker_roof.labels,air_name=mapmaker_roof.labels+MAPMAKER_ROOF_LABEL_BYTES;
        compile();                                          /* Play: the hand-over (sub_00312DF0) */
        CHECK(mapmaker_roof.mismatch==mism);
        CHECK((WORD_AT(record(1)+0x24)>>16)==0x5201u && (WORD_AT(record(2)+0x24)>>16)==0x5202u);
        CHECK((WORD_AT(record(0)+0x24)>>16)==0 && (WORD_AT(record(3)+0x24)>>16)==0);
        CHECK(play_mask(1)==(0x03FFFFFFu&~0x02000000u) && play_mask(2)==0x01000000u && play_mask(3)==0x03FFFFFFu);
        /* back to the editor: new pieces from the records */
        hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
        reload_pieces(50,4);
        load();
        CHECK(mapmaker_roof_mode(piece(51))==MAPMAKER_ROOF_OPEN_ROOF && mapmaker_roof_mode(piece(52))==MAPMAKER_ROOF_OPEN_AIR);
        CHECK(mapmaker_roof_mode(piece(50))==0 && mapmaker_roof_mode(piece(53))==0);
        CHECK(mapmaker_roof_mode(piece(41))==0 && mapmaker_roof_mode(piece(42))==0);
        /* the label over the grid: the piece under the cursor, else the held one */
        WORD_AT(ED+0x2F18)=piece(51);WORD_AT(ED+0x2F7C)=0;
        CHECK(text_ptr(MAPMAKER_ROOF_RET_HOVER)==roof_name);
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_HOVER,0xA3Fu),"Small Open (No Roof)"));
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_HOVER,0xA40u),"stock"));
        WORD_AT(ED+0x2F18)=piece(50);
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_HOVER,0xA3Fu),"stock"));
        /* copy the open-air tile: a held copy, placed as a new piece */
        copy_piece(piece(52),piece(54));WORD_AT(ED+0x2F7C)=piece(54);WORD_AT(ED+0x2F18)=0;
        CHECK(text_ptr(MAPMAKER_ROOF_RET_HOVER)==air_name);
        CHECK(format(MAPMAKER_ROOF_RET_HOVER_FORMAT,MAPMAKER_ROOF_HOVER_FORMAT,1u,text_ptr(MAPMAKER_ROOF_RET_HOVER))==47u);
        WORD_AT(ED+0x2F7C)=0;
        CHECK(!strcmp(text(MAPMAKER_ROOF_RET_HOVER,0xA3Fu),"stock"));    /* nothing under the cursor */
        move_piece(piece(54),piece(55));free_piece(piece(54));
        FLOAT_AT(piece(55))=4.0f;WORD_AT(piece(53)+0x40)=piece(55);WORD_AT(piece(55)+0x40)=0;
        CHECK(mapmaker_roof_mode(piece(55))==MAPMAKER_ROOF_OPEN_AIR && mapmaker_roof_mode(piece(52))==MAPMAKER_ROOF_OPEN_AIR);
        compile();                                          /* Play again */
        CHECK(mapmaker_roof.mismatch==mism);
        CHECK((WORD_AT(record(1)+0x24)>>16)==0x5201u && (WORD_AT(record(2)+0x24)>>16)==0x5202u &&
              (WORD_AT(record(4)+0x24)>>16)==0x5202u);
        CHECK(play_mask(1)==(0x03FFFFFFu&~0x02000000u) && play_mask(2)==0x01000000u && play_mask(4)==0x01000000u);
        CHECK(play_mask(0)==0x03FFFFFFu && play_mask(3)==0x03FFFFFFu);
        /* and back once more (undo and Load map reload through the same loader) */
        hook(pc_mapmaker_roof_reset,plain_original,0x00301B4Fu,0,0);
        reload_pieces(60,5);
        load();
        CHECK(mapmaker_roof_mode(piece(61))==MAPMAKER_ROOF_OPEN_ROOF && mapmaker_roof_mode(piece(62))==MAPMAKER_ROOF_OPEN_AIR &&
              mapmaker_roof_mode(piece(64))==MAPMAKER_ROOF_OPEN_AIR && mapmaker_roof_mode(piece(60))==0 && mapmaker_roof_mode(piece(63))==0);
        CHECK(mapmaker_roof_records(C,count_record,NULL)==5);
    }

    /* allocation failure: no entries, counted */
    mapmaker_roof.list=0;heap_result=0;
    build_menu();
    CHECK(seen_count==45u && mapmaker_roof.no_list==1u && tables_sum()==h0);

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    puts("mapmaker roof ok");
    return 0;
}
