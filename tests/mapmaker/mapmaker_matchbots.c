/* Asset-free contract for src/interface/pc_mapmaker_matchbots.inc (more
 * Arcade bots per MapMaker match). Fake guest memory holds the setup block
 * 0x0078AD58, the live block 0x007DE400 and its mirror 0x00904000, the
 * 10-row Bots work copy 0x0075FD90, a profile and the editor's custom set;
 * the fake originals follow the stock code's reads and writes (see the doc)
 * and pop their return address like the lifted callees; every hook call
 * checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
static volatile uint32_t stub_ran;
#define STUB(name,n) void name(void) { stub_ran=n; }
STUB(sub_0003DFA0,1) STUB(sub_00074670,2) STUB(sub_00071970,3) STUB(sub_00076EF0,4) STUB(sub_002BFFD0,5)
STUB(sub_0003D7B0,6) STUB(sub_002BE650,7) STUB(sub_00071800,8) STUB(sub_000D7780,9)
STUB(sub_002C0310,10) STUB(sub_00069640,11) STUB(sub_002BE600,12) STUB(sub_002BE270,13)
STUB(sub_00303350,14)
STUB(sub_00075B70,15) STUB(sub_00071BE0,16) STUB(sub_00079D60,17) STUB(sub_002C1830,18) STUB(sub_00079710,19)
STUB(sub_000780E0,20) STUB(sub_00078320,21) STUB(sub_000792B0,22) STUB(sub_00078380,23) STUB(sub_000781A0,24)
STUB(sub_00077140,25) STUB(sub_00077360,26) STUB(sub_00074A00,27) STUB(sub_00074460,28)

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define EDITOR 0x00200000u
#define ESET (EDITOR+0x11860u)
#define WORK 0x0075FD90u
#define PARENT 0x00300000u
#define PANE 0x00301000u
#define ITEMS 0x00302000u
#define PROFILE 0x00305000u
#define SETUP 0x0078AD58u
#define LIVE 0x007DE400u
#define MIRROR 0x00904000u
#define INDEX 0x0078A068u
#define LOADED 0x0078AEACu

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t selected;
static unsigned stores,originals,updates;
static uint8_t profile_rows[10*0x1C];
static void hook_store_entry(void);
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00074670) r=(x[0]==PARENT && x[1]==0x0052372Cu)?PANE:0u;
    else if(f==sub_00071970) r=(x[0]==PANE && x[1]<10u && x[2]==0u)?ITEMS+x[1]*0x10u:0u;
    else if(f==sub_00076EF0) { if(x[0]==PANE) selected=x[1]; }
    else if(f==sub_002BFFD0) hook_store_entry();
    else if(f==sub_00069640) r=PROFILE;
    else if(f==sub_002BE600) r=0x7000u+x[0]*16u+(x[1]&0xFu);
    else if(f==sub_002BE270) r=x[0]!=77u;                     /* character 77 is locked */
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_sets.inc"
#include "pc_mapmaker_matchbots.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t a0,int line)
{
    uint32_t sp=g_esp;
    g_esp-=8u;WORD_AT(g_esp)=0x00123456u;WORD_AT(g_esp+4u)=a0;
    hook(orig);
    if(g_esp!=sp-4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-(sp-4u)));failures++; }
    g_esp=sp;
}
#define CALL(h,o,a0) call_hook(h,o,a0,__LINE__)
static void fake_plain(void) { originals++;g_esp+=4u; }
static uint32_t row_at(unsigned k) { return WORK+k*0x1Cu; }

/* ---- the live block ---------------------------------------------------- */
static void live_block(unsigned humans,unsigned bots,uint8_t scene)
{
    unsigned a,k;
    memset((void *)((uintptr_t)LIVE+g_xbox_mem_offset),0,0xA0);
    BYTE_AT(LIVE)=(uint8_t)humans;BYTE_AT(LIVE+2u)=(uint8_t)bots;BYTE_AT(LIVE+0x54u)=scene;
    for(a=0;a<4u;a++) for(k=0;k<16u;k++) BYTE_AT(LIVE+0x14u+a*16u+k)=(uint8_t)(a*40u+k+1u);
}
static void fake_live(void)
{
    /* sub_000670C0: rep movsd pending -> live (the test prepared the live block itself) */
    originals++;g_esp+=4u;
}
static void live_tests(void)
{
    unsigned a,k;
    /* no extras: mirror 0..15 = the block, 16+ zero */
    memset(&mb,0,sizeof mb);
    memset((void *)((uintptr_t)MIRROR+g_xbox_mem_offset),0xEE,0x100);
    live_block(2,10,0x26);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    for(a=0;a<4u;a++) for(k=0;k<64u;k++)
        CHECK(BYTE_AT(MIRROR+a*64u+k)==(k<16u?(uint8_t)(a*40u+k+1u):0u));
    CHECK(BYTE_AT(LIVE+2u)==10u);
    /* the MapMaker match whose start added bots: 16+ from the side arrays */
    mb.extra_valid=1;mb.humans=2;mb.bots=20;
    for(a=0;a<4u;a++) for(k=0;k<64u;k++) mb.extra[a][k]=(uint8_t)(0x80u+a*8u+k);
    live_block(2,20,0x64);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    for(a=0;a<4u;a++) for(k=0;k<64u;k++)
        CHECK(BYTE_AT(MIRROR+a*64u+k)==(k<16u?(uint8_t)(a*40u+k+1u):k<22u?(uint8_t)(0x80u+a*8u+k):0u));
    CHECK(BYTE_AT(LIVE+2u)==20u);
    /* another match (humans differ): no extras; a count past 16 is clamped */
    live_block(3,20,0x64);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    CHECK(BYTE_AT(MIRROR+16u)==0u && BYTE_AT(LIVE+2u)==13u);
    /* System Link session: stock */
    WORD_AT(0x0076B108u)=1u;
    live_block(2,20,0x64);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    CHECK(BYTE_AT(MIRROR+16u)==0u && BYTE_AT(LIVE+2u)==14u);
    WORD_AT(0x0076B108u)=0u;
    /* not a MapMaker map */
    live_block(2,20,0x26);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    CHECK(BYTE_AT(MIRROR+16u)==0u);
    /* restart: mirror 0..15 (team changes in play) back into the block, then the copy */
    live_block(2,20,0x64);
    CALL(pc_mapmaker_matchbots_live,fake_live,0);
    BYTE_AT(MIRROR+64u+3u)=0x5Au;BYTE_AT(MIRROR+64u+17u)=0x5Bu;
    CALL(pc_mapmaker_matchbots_restart,fake_plain,0x00310000u);
    CHECK(BYTE_AT(LIVE+0x24u+3u)==0x5Au && BYTE_AT(LIVE+0x34u)==(uint8_t)(2u*40u+1u));
    CHECK(BYTE_AT(LIVE+0x24u+15u)==BYTE_AT(MIRROR+64u+15u) && BYTE_AT(LIVE+0x34u+0u)==BYTE_AT(MIRROR+128u));
}

/* ---- match start ------------------------------------------------------- */
static unsigned start_bots;
static void setup_block(unsigned humans,unsigned bots)
{
    unsigned a,k;
    memset((void *)((uintptr_t)SETUP+g_xbox_mem_offset),0,0xA0);
    WORD_AT(0x005236C4u)=humans;
    start_bots=bots;BYTE_AT(SETUP+2u)=0xEEu;                 /* the roster writes the count */BYTE_AT(SETUP+0x54u)=0x64u;BYTE_AT(SETUP+0x55u)=3u;BYTE_AT(SETUP+0x56u)=0u;
    for(a=0;a<4u;a++) for(k=0;k<humans+bots && k<16u;k++) BYTE_AT(SETUP+0x14u+a*16u+k)=(uint8_t)(0x10u*a+k);
}
static void put_row(uint8_t *r,int32_t id,int32_t team,int32_t diff,int32_t on,int32_t over,int32_t variant)
{
    memset(r,0,0x1C);
    memcpy(r,&id,4);memcpy(r+4,&team,4);memcpy(r+8,&diff,4);memcpy(r+0xC,&on,4);memcpy(r+0x10,&over,4);memcpy(r+0x18,&variant,4);
}
static void fake_start(void)
{
    /* sub_002D2C70: the stock roster (rows 1-10) writes the bot count (the bytes are set up) */
    originals++;BYTE_AT(SETUP+2u)=(uint8_t)start_bots;g_esp+=4u;
}
static void page2_rows(void)
{
    unsigned k;
    for(k=0;k<10u;k++) put_row(mb.rows+k*0x1C,(int32_t)(30u+k),(int32_t)k,(int32_t)(k%4u),1,-1,(int32_t)(k%3u)-1);
    put_row(mb.rows+2u*0x1C,-1,0,0,1,-1,-1);                  /* empty */
    put_row(mb.rows+4u*0x1C,34,1,1,0,-1,0);                   /* switched off */
    put_row(mb.rows+5u*0x1C,35,1,1,0,1,0);                    /* off, but the override is on */
    put_row(mb.rows+6u*0x1C,36,1,1,1,0,0);                    /* on, but the override is off */
    mb.rows_valid=1;mb.rows_index=0xC;
}
static void match_tests(void)
{
    unsigned k,n;
    static const unsigned used[]={0,1,3,5,7,8,9};
    /* MapMaker Arcade from the front end, 2 players + 10 stock bots, a team game */
    memset(&mb,0,sizeof mb);
    WORD_AT(0x0079094Cu)=101u;WORD_AT(0x0078AD2Cu)=9u;
    setup_block(2,10);WORD_AT(SETUP+0x58u)=1u;
    WORD_AT(PROFILE+0x10u)=0u;
    page2_rows();
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(originals>0 && BYTE_AT(SETUP+2u)==17u && mb.extra_valid && mb.humans==2u && mb.bots==17u);
    for(k=0;k<7u;k++) {
        unsigned j=used[k],idx=12u+k,t=j%2u+1u;
        int32_t v=(int32_t)(j%3u)-1;
        if(j==5u) v=0;
        CHECK(mb.extra[0][idx]==30u+j && mb.extra[1][idx]==t && mb.extra[2][idx]==(j==5u?1u:j%4u) &&
              mb.extra[3][idx]==(uint8_t)v);
        if(idx<16u) CHECK(BYTE_AT(SETUP+0x14u+idx)==30u+j && BYTE_AT(SETUP+0x24u+idx)==t && BYTE_AT(SETUP+0x44u+idx)==(uint8_t)v);
    }
    for(k=0;k<12u;k++) CHECK(mb.extra[0][k]==k && mb.extra[1][k]==0x10u+k);   /* stock entries copied */
    CHECK(BYTE_AT(SETUP+0x14u+15u)==30u+used[3]);
    CHECK(BYTE_AT(SETUP+0x24u)==0x10u && BYTE_AT(SETUP+0x34u)==0x20u && BYTE_AT(SETUP+0x44u)==0x30u);  /* 16 per array */
    /* sub-mode 4 teams by 4; difficulty from the character's skill when the profile says so */
    setup_block(1,10);WORD_AT(SETUP+0x58u)=1u;BYTE_AT(SETUP+0x56u)=4u;WORD_AT(PROFILE+0x10u)=1u;
    WORD_AT(0x0053A3DCu+30u*0x30u)=1u;WORD_AT(0x0053A3DCu+31u*0x30u)=18u;
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(mb.extra[1][11]==1u && mb.extra[2][11]==4u && mb.extra[2][12]==0xFFu && mb.extra[1][13]==4u);  /* (15-18)/3 = -1 */
    BYTE_AT(SETUP+0x55u)=5u;                                  /* type 5: the row's difficulty */
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(mb.extra[2][11]==0u && mb.extra[2][12]==1u);
    /* not a team game: team 0 */
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(mb.extra[1][11]==0u && mb.extra[1][17]==0u && BYTE_AT(SETUP+2u)==17u);
    /* 24 characters at most: 4 players + 12 stock (n 16) leave room for 8 */
    page2_rows();
    for(k=0;k<10u;k++) put_row(mb.rows+k*0x1C,(int32_t)(50u+k),0,0,1,-1,0);
    setup_block(4,12);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(BYTE_AT(SETUP+2u)==20u && mb.extra[0][23]==57u && mb.extra[0][24]==0u);
    /* 20 bots at most: 1 player + 10 stock + 10 rows = 20 */
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(BYTE_AT(SETUP+2u)==20u && mb.extra[0][20]==59u);
    setup_block(1,12);                                        /* 13 + 8 = 21 characters, 20 bots */
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(BYTE_AT(SETUP+2u)==20u && mb.extra[0][20]==57u && mb.extra[0][21]==0u);
    /* nothing added: no extras, the block untouched */
    for(k=0;k<10u;k++) put_row(mb.rows+k*0x1C,-1,0,0,1,-1,-1);
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(!mb.extra_valid && BYTE_AT(SETUP+2u)==10u);
    /* refused: System Link, network modes, switch off, story type, monkeys, other maps */
    page2_rows();
    for(n=0;n<7u;n++) {
        setup_block(1,10);WORD_AT(0x0078AD2Cu)=9u;
        if(n==0) WORD_AT(0x0076B108u)=1u;
        if(n==1) WORD_AT(0x0078AD2Cu)=0xBu;
        if(n==2) set_env("TSFP_MAPMAKER_LIMITS",NULL);
        if(n==3) BYTE_AT(SETUP+0x55u)=10u;
        if(n==4) BYTE_AT(SETUP+0x56u)=0x13u;
        if(n==5) { WORD_AT(0x0078AD2Cu)=5u;BYTE_AT(SETUP+0x54u)=0x26u; }
        if(n==6) WORD_AT(0x0079094Cu)=1u;
        mb.extra_valid=1;
        CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
        CHECK(!mb.extra_valid && BYTE_AT(SETUP+2u)==10u);
        WORD_AT(0x0076B108u)=0u;set_env("TSFP_MAPMAKER_LIMITS","1");WORD_AT(0x0079094Cu)=101u;
    }
    /* a Test map pause (mode 5 on a MapMaker map) counts */
    setup_block(1,10);WORD_AT(0x0078AD2Cu)=5u;WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(mb.extra_valid && BYTE_AT(SETUP+2u)==17u);
    /* the editor's Test map: custom set members 11-20, rows 1-10's team and difficulty */
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784024u)=0xCu;WORD_AT(0x007844A8u)=EDITOR;
    for(k=0;k<20u;k++) { HALF_AT(ESET+4u+8u*k)=(int16_t)(k<10u?k:(k%2u?-1:(int)(60u+k)));HALF_AT(ESET+4u+8u*k+6u)=(int16_t)(k%3u); }
    for(k=0;k<10u;k++) { WORD_AT(row_at(k)+4u)=k+1u;WORD_AT(row_at(k)+8u)=3u; }
    setup_block(1,10);WORD_AT(SETUP+0x58u)=1u;WORD_AT(PROFILE+0x10u)=0u;
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(BYTE_AT(SETUP+2u)==15u && mb.extra[0][11]==70u && mb.extra[0][12]==72u && mb.extra[1][11]==2u &&
          mb.extra[1][12]==2u && mb.extra[2][11]==3u && mb.extra[3][11]==(uint8_t)(10u%3u));
    WORD_AT(0x0078AD2Cu)=0xCu;                                /* a network mode: stock */
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(!mb.extra_valid && BYTE_AT(SETUP+2u)==10u);
    WORD_AT(0x0078AD2Cu)=5u;
    WORD_AT(0x00784024u)=3u;                                  /* a preset: stock */
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(!mb.extra_valid && BYTE_AT(SETUP+2u)==10u);
    WORD_AT(0x0079094Cu)=101u;WORD_AT(0x0078AD2Cu)=9u;
}

/* ---- the Bots screen --------------------------------------------------- */
static uint32_t test_index;
static void fake_store(void)
{
    stores++;
    memcpy(profile_rows,(void *)((uintptr_t)WORK+g_xbox_mem_offset),sizeof profile_rows);
    g_esp+=4u;
}
static void hook_store_entry(void)
{
    uint32_t sp=g_esp;
    g_esp-=4u;WORD_AT(g_esp)=0x002C05CDu;
    pc_mapmaker_matchbots_store(fake_store);
    if(g_esp!=sp) { fprintf(stderr,"store esp %+d\n",(int)(g_esp-sp));failures++; }
    g_esp=sp;
}
static void fake_update(void)
{
    unsigned k;
    updates++;
    WORD_AT(INDEX)=test_index;
    if(WORD_AT(LOADED)!=test_index) {                         /* sub_002C0310 reload of rows 1-10 */
        for(k=0;k<10u;k++) put_row((uint8_t *)((uintptr_t)row_at(k)+g_xbox_mem_offset),(int32_t)(test_index*10u+k),
                                   (int32_t)k,2,1,-1,(int32_t)(k%2u));
    }
    WORD_AT(LOADED)=test_index;
    /* tail jump to sub_002BFFD0 with this function's return address */
    pc_mapmaker_matchbots_store(fake_store);
}
static void press(int32_t row,uint32_t input)
{
    WORD_AT(PANE+0x18Cu)=(uint32_t)row;WORD_AT(PANE+0xC4u)=input;WORD_AT(PANE+0xC8u)=input&0xFu;
}
static void map_chunk(void)
{
    uint32_t p=CONT+0x3ECu,d;
    unsigned k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x2000);
    WORD_AT(CONT)=0x00C77667u;WORD_AT(CONT+4u)=0x12Du;WORD_AT(CONT+8u)=0x00C77668u;WORD_AT(CONT+0xCu)=0x3DCu;
    WORD_AT(p)=0x00C776F5u;WORD_AT(p+4u)=0x30u;d=p+8u;
    HALF_AT(d)=1;BYTE_AT(d+2u)=10u;BYTE_AT(d+3u)=10u;
    for(k=0;k<10u;k++) { HALF_AT(d+4u+4u*k)=(int16_t)(k==3u?-1:(k==4u?77:(int)(70u+k)));BYTE_AT(d+4u+4u*k+3u)=(uint8_t)(k==1u?0xFFu:k%2u); }
    p=d+0x30u;WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=p-CONT;
    WORD_AT(0x00784014u)=CONT;
}
static void screen_tests(void)
{
    uint8_t page1[10*0x1C];
    int32_t v;
    unsigned k,s;
    memset(&mb,0,sizeof mb);
    WORD_AT(0x0079094Cu)=101u;WORD_AT(0x0078AD2Cu)=9u;WORD_AT(0x0076B108u)=0u;
    BYTE_AT(SETUP+0x54u)=0x64u;
    map_chunk();
    WORD_AT(PARENT+0x204u)=0x00303000u;memcpy((void *)((uintptr_t)0x00303000u+g_xbox_mem_offset),"Bots",5);
    /* open on the map's set (0x11): rows 11-20 from the map's members 11-20 */
    test_index=0x11u;WORD_AT(LOADED)=0u;
    CALL(pc_mapmaker_matchbots_open,fake_plain,PARENT);
    press(0,0);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(updates==1u && mb.rows_valid && mb.rows_index==0x11u && !mb.page);
    memcpy(&v,mb.rows+0u*0x1C,4);CHECK(v==70);
    memcpy(&v,mb.rows+3u*0x1C,4);CHECK(v==-1);
    memcpy(&v,mb.rows+4u*0x1C+0x18,4);CHECK(v==-1);           /* locked character: no variant */
    memcpy(&v,mb.rows+1u*0x1C+0x18,4);CHECK(v==-1);
    memcpy(&v,mb.rows+5u*0x1C+0x18,4);CHECK(v==1);
    memcpy(&v,mb.rows+2u*0x1C+0x10,4);CHECK(v==-1);
    memcpy(&v,mb.rows+2u*0x1C+4,4);CHECK(v==2);               /* team from row 3 of page 1 */
    CHECK(!strcmp((const char *)((uintptr_t)0x00903700u+g_xbox_mem_offset),"Bots  (1-10)") && WORD_AT(PARENT+0x204u)==0x00903700u);
    /* Down on row 10: page 2 shown, rows 1-10 stored first and kept */
    memcpy(page1,(void *)((uintptr_t)WORK+g_xbox_mem_offset),sizeof page1);
    s=stores;
    press(9,0x00020002u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(mb.page && stores==s+1u && !memcmp(mb.save,page1,sizeof page1));
    CHECK(WORD_AT(row_at(0))==70u && WORD_AT(row_at(0)+0x14u)==0x7000u+70u*16u+0u);
    CHECK(selected==ITEMS && !(WORD_AT(PANE+0xC4u)&0x000F000Fu) && !(WORD_AT(PANE+0xC8u)&0xFu));
    CHECK(!memcmp(profile_rows,page1,sizeof page1));         /* the page-2 store did not reach the profile */
    CHECK(!strcmp((const char *)((uintptr_t)0x00903700u+g_xbox_mem_offset),"Bots  (11-20)"));
    /* an edit on page 2 is kept here, every frame, not in the profile */
    WORD_AT(row_at(2))=88u;press(2,0);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    memcpy(&v,mb.rows+2u*0x1C,4);CHECK(v==88 && stores==s+1u && !memcmp(profile_rows,page1,sizeof page1));
    /* Up on row 11: rows 1-10 back */
    press(0,0x00010001u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page && !memcmp((void *)((uintptr_t)WORK+g_xbox_mem_offset),page1,sizeof page1));
    CHECK(selected==ITEMS+9u*0x10u && stores==s+2u);
    memcpy(&v,mb.rows+2u*0x1C,4);CHECK(v==88);
    /* leave on page 2: the next open restores rows 1-10 (index 0 would not reload) */
    press(9,0x00020002u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(mb.page);
    WORD_AT(row_at(5))=99u;
    CALL(pc_mapmaker_matchbots_open,fake_plain,PARENT);
    CHECK(!mb.page && !memcmp((void *)((uintptr_t)WORK+g_xbox_mem_offset),page1,sizeof page1));
    memcpy(&v,mb.rows+5u*0x1C,4);CHECK(v==99);
    /* reopening the map's set reloads rows 11-20 from the map, as stock reloads rows 1-10 */
    WORD_AT(LOADED)=0u;press(0,0);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    memcpy(&v,mb.rows+5u*0x1C,4);CHECK(v==75);
    /* leave on page 2 and start the match: rows 1-10 back before the roster */
    press(9,0x00020002u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(mb.page);
    setup_block(1,10);
    CALL(pc_mapmaker_matchbots_match_start,fake_start,0);
    CHECK(!mb.page && !memcmp((void *)((uintptr_t)WORK+g_xbox_mem_offset),page1,sizeof page1) && mb.extra[0][11]==70u);
    /* the set index changes while page 2 is shown: the reload wins, the profile store runs */
    press(9,0x00020002u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(mb.page);
    s=stores;test_index=0xCu;press(4,0);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page && stores==s+1u && WORD_AT(row_at(0))==120u && mb.rows_index==0xCu);
    memcpy(&v,mb.rows,4);CHECK(v==-1);                        /* a profile set: rows 11-20 start empty */
    /* the editor's screen (scene 102) and other modes pass through */
    WORD_AT(0x0079094Cu)=102u;updates=0;press(9,0x00020002u);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(updates==1u && !mb.page);
    WORD_AT(0x0079094Cu)=101u;WORD_AT(0x0078AD2Cu)=5u;BYTE_AT(SETUP+0x54u)=0x26u;
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page);
    WORD_AT(0x0078AD2Cu)=9u;WORD_AT(0x0076B108u)=1u;
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page);
    WORD_AT(0x0076B108u)=0u;set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* a pending reload (index differs from the loaded one): no flip this frame */
    BYTE_AT(SETUP+0x54u)=0x64u;WORD_AT(INDEX)=0xCu;WORD_AT(LOADED)=0u;
    CALL(pc_mapmaker_matchbots_screen,fake_update,PARENT);
    CHECK(!mb.page);
    for(k=0;k<10u;k++) CHECK(WORD_AT(row_at(k))==120u+k);
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    WORD_AT(0x007844A8u)=EDITOR;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    live_tests();
    match_tests();
    screen_tests();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker matchbots ok\n");
    return 0;
}
