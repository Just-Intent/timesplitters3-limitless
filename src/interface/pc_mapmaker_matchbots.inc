/* US 45410066 MapMaker Arcade matches: up to 20 bots (stock 10) and 24
 * characters (stock 16). See docs/mapmaker/mapmaker-matchbots.md for every
 * limit, site and its evidence.
 *
 * Stock: the Bots screen (0x0052375C) edits a 10-row work copy 0x0075FD90
 * (0x1C per row); the match start sub_002D2C70 turns the rows into bots after
 * the humans in the setup block 0x0078AD58 (0xA0 bytes: +0 humans, +2 bots,
 * 16-byte per-actor arrays +0x14 character, +0x24 team, +0x34 difficulty,
 * +0x44 variant). The block is copied whole to the pending block 0x007DE340
 * (sub_000658D0) and from there to the live block 0x007DE400 (sub_000670C0),
 * which the level reads by actor index. So:
 *  - matchbots.json moves every indexed reference to the live arrays (26
 *    sites) to a 64-entry mirror at 0x00904000 (4 x 64 bytes);
 *    pc_mapmaker_matchbots_live (after sub_000670C0, every scene) copies the
 *    block's 16 entries into it and, for the MapMaker match whose start added
 *    them, entries 16+ from the side arrays; pc_mapmaker_matchbots_restart
 *    (before sub_002EF9F0, the only live -> setup copy) writes entries 0..15
 *    back into the block. The rank table 0x007DE4C0 (16 places, followed by
 *    the team scores) is relocated to 0x00904100 (64) and the pair table is
 *    sized humans + 20 outside story mode (matchbots.json);
 *  - Bots screen page 2 (rows 11-20): Down on row 10 / Up on row 11 swaps the
 *    work copy with ten rows kept here (scenes 100/101, MapMaker Arcade mode 9
 *    or a Test map pause, no System Link, TSFP_MAPMAKER_LIMITS=1). While page
 *    2 is shown the per-frame store sub_002BFFD0 keeps the rows here instead
 *    of writing the profile's rows 1-10. The editor's own screen (scene 102)
 *    is a 20-row list in pc_mapmaker_sets.inc (custom set members 11-20);
 *  - match start: after the stock roster, rows 11-20 (scene 102: custom set
 *    members 11-20 with rows 1-10's team and difficulty) become more bots with
 *    the stock rules, until 24 characters or 20 bots. Index < 16 goes into the
 *    setup block, every index into the side arrays.
 * System Link (session flag [0x0076B108]) and story-type matches stay stock.
 *
 * Extension memory: 0x00904000..0x009040FF live mirror, 0x00904100..
 * 0x009041FF rank table (matchbots.json), 0x00904200..0x009042FF reserved.
 * Requires the includer's WORD_AT, guest_call, g_esp, g_xbox_mem_offset,
 * pc_mapmaker_limits.inc and pc_mapmaker_sets.inc (sets_select, sets_title,
 * sets_chunk). */
#include <stdio.h>
#include <string.h>

#define MB_SCENE 0x0079094Cu
#define MB_NET 0x0076B108u              /* System Link session (sub_003533F0) */
#define MB_MODE 0x0078AD2Cu             /* 5 Arcade custom / Test map, 9 MapMaker Arcade, 0xB-0xD network */
#define MB_HUMANS 0x005236C4u           /* local players (sub_002D2C70 roster start) */
#define MB_SETUP 0x0078AD58u
#define MB_LIVE 0x007DE400u
#define MB_ARRAYS 0x14u                 /* +0x14 character, +0x24 team, +0x34 difficulty, +0x44 variant */
#define MB_MIRROR 0x00904000u           /* 4 x 64 bytes, same order */
#define MB_WORK 0x0075FD90u
#define MB_ROW 0x1Cu                    /* +0 character, +4 team, +8 difficulty, +0xC on, +0x10 override, +0x14 portrait, +0x18 variant */
#define MB_ROWS 10u
#define MB_INDEX 0x0078A068u            /* set index of this frame (sub_002C0490) */
#define MB_LOADED 0x0078AEACu           /* index last loaded into the work copy */
#define MB_PANE 0x0052372Cu
#define MB_CHARS 0x0053A3DCu            /* character table +0x30 each: skill dword */
#define MB_ACTORS 24u                   /* arcade stats 0x0075CCC0 and the awards frame hold 26 */
#define MB_BOTS 20u
#define MB_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define MB_P(a) ((uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00069640(void),sub_002BE600(void),sub_002BE270(void);

static struct {
    int page,opened;                    /* page: the work copy shows rows 11-20 */
    uint32_t page_index,rows_index;
    int rows_valid;
    uint8_t rows[MB_ROWS*MB_ROW],save[MB_ROWS*MB_ROW];
    int extra_valid;
    unsigned humans,bots;
    uint8_t extra[4][64];
    unsigned logged;
} mb;

static void mb_log(const char *what,int a,int b)
{
    if(mb.logged>=12u) return;
    mb.logged++;
    fprintf(stderr,"[mapmaker] matchbots: %s (%d, %d)\n",what,a,b);
    fflush(stderr);
}
static int mb_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
/* MapMaker Arcade setup or pause (scenes 101/100), local only */
static int mb_screen_ok(void)
{
    uint32_t scene=WORD_AT(MB_SCENE),mode=WORD_AT(MB_MODE);
    if((scene!=100u && scene!=101u) || WORD_AT(MB_NET)) return 0;
    if(!(mode==9u || (mode==5u && MB_B(MB_SETUP+0x54u)==0x64u))) return 0;
    return mb_on();
}
static void mb_portraits(void)
{
    unsigned k;
    for(k=0;k<MB_ROWS;k++) {
        uint32_t r=MB_WORK+k*MB_ROW;
        WORD_AT(r+0x14u)=guest_call(sub_002BE600,2,WORD_AT(r),WORD_AT(r+0x18u));
    }
}
/* rows 1-10 back into the work copy (page 2 kept here) */
static void mb_unswap(void)
{
    if(!mb.page) return;
    memcpy(mb.rows,MB_P(MB_WORK),sizeof mb.rows);
    memcpy(MB_P(MB_WORK),mb.save,sizeof mb.save);
    mb.page=0;
}
/* rows 11-20 for a set index: the map's members 11-20 for MapMaker Arcade's
 * map set (index 0x11, mode 9), else empty; team, difficulty and flags from
 * rows 1-10 (the fill's template). */
static void mb_init_rows(uint32_t index)
{
    uint32_t data=0;
    unsigned k,n=0;
    if(index==0x11u && WORD_AT(MB_MODE)==9u && (data=sets_chunk(WORD_AT(SETS_CONTAINER)))) n=SETS_B(data+3u);
    memcpy(mb.rows,MB_P(MB_WORK),sizeof mb.rows);
    for(k=0;k<MB_ROWS;k++) {
        uint8_t *r=mb.rows+k*MB_ROW;
        int32_t id=-1,variant=-1;
        if(k<n && SETS_H(data+4u+4u*k)>=0) {
            id=SETS_H(data+4u+4u*k);variant=(int8_t)SETS_B(data+4u+4u*k+3u);
            if(!guest_call(sub_002BE270,1,(uint32_t)id)) variant=-1;     /* as the fill sub_002C0310 */
        }
        memcpy(r,&id,4);memcpy(r+0x18,&variant,4);
        id=-1;memcpy(r+0x10,&id,4);
    }
    mb.rows_index=index;mb.rows_valid=1;
}

/* WRAP sub_002C2C20 (cdecl menu): the Bots screen's builder (screen opens). */
void pc_mapmaker_matchbots_open(void (*original)(void))
{
    mb_unswap();
    mb.opened=1;
    original();
}

/* WRAP sub_002C0490 (cdecl menu): the Bots screen's per-frame update. */
void pc_mapmaker_matchbots_screen(void (*original)(void))
{
    uint32_t menu,pane,in,index;
    int32_t row;
    int keep,opened=mb.opened;
    mb.opened=0;
    if(!MAPMAKER_STACK_OK(8) || !mb_screen_ok()) { mb_unswap();original();return; }
    menu=WORD_AT(g_esp+4u);
    pane=MAPMAKER_PTR_OK(menu,0x208u)?guest_call(sub_00074670,2,menu,MB_PANE):0u;
    if(pane && MAPMAKER_PTR_OK(pane,0x200u) && mb.rows_valid && WORD_AT(MB_LOADED)==WORD_AT(MB_INDEX)) {
        row=(int32_t)WORD_AT(pane+0x18Cu);in=WORD_AT(pane+0xC4u);
        if(!mb.page && row==(int32_t)MB_ROWS-1 && (in&0x00020002u)) {
            guest_call(sub_002BFFD0,0);                       /* keep rows 1-10's last change */
            memcpy(mb.save,MB_P(MB_WORK),sizeof mb.save);
            memcpy(MB_P(MB_WORK),mb.rows,sizeof mb.rows);
            mb_portraits();
            mb.page=1;mb.page_index=WORD_AT(MB_INDEX);
        } else if(mb.page && row==0 && (in&0x00010001u)) {
            mb_unswap();
        } else row=-1;
        if(row>=0) {
            WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;
            sets_select(pane,mb.page?0u:MB_ROWS-1u);
        }
    }
    if(MAPMAKER_PTR_OK(menu,0x208u)) { keep=sets.page;sets.page=mb.page;sets_title(menu);sets.page=keep; }
    original();                                               /* pops the return address */
    index=WORD_AT(MB_INDEX);
    if(!mb.rows_valid || index!=mb.rows_index || (opened && index==0x11u && WORD_AT(MB_MODE)==9u))
        if(!mb.page) mb_init_rows(index);
}

/* WRAP sub_002BFFD0 (no arguments, plain ret): stores the work copy into the
 * profile's rows 1-10 every frame. On page 2 keep the rows here instead. */
void pc_mapmaker_matchbots_store(void (*original)(void))
{
    if(mb.page && WORD_AT(MB_SCENE)!=102u) {
        if(WORD_AT(MB_INDEX)==mb.page_index) {
            memcpy(mb.rows,MB_P(MB_WORK),sizeof mb.rows);
            MAPMAKER_GUEST_RET();
            return;
        }
        mb.page=0;                                            /* the index changed: rows 1-10 were reloaded */
    }
    original();
}

/* ---- match start ------------------------------------------------------- */
static void mb_place(unsigned n,uint8_t id,uint8_t team,uint8_t diff,uint8_t variant)
{
    uint8_t v[4];
    unsigned a;
    v[0]=id;v[1]=team;v[2]=diff;v[3]=variant;
    for(a=0;a<4u;a++) {
        if(n<16u) MB_B(MB_SETUP+MB_ARRAYS+a*16u+n)=v[a];
        mb.extra[a][n]=v[a];
    }
}
/* One more bot (stock roster 0x002D378D..0x002D3817): team = row team %
 * (4 in sub-mode 4, else 2) + 1 in a team game (flags +0x58 bit 0), else 0;
 * difficulty = (15 - character skill) / 3 when the profile's +0x10 is set
 * and the type byte +0x55 is not 5, else the row's difficulty. */
static void mb_bot(unsigned n,int32_t id,int32_t variant,int32_t team_field,int32_t diff_field,uint32_t profile)
{
    uint8_t team=0,diff;
    if(WORD_AT(MB_SETUP+0x58u)&1u) team=(uint8_t)(team_field%(MB_B(MB_SETUP+0x56u)==4u?4:2)+1);
    if(profile && WORD_AT(profile+0x10u) && MB_B(MB_SETUP+0x55u)!=5u)
        diff=(uint8_t)((15-(int32_t)WORD_AT(MB_CHARS+(uint32_t)(uint8_t)id*0x30u))/3);
    else diff=(uint8_t)diff_field;
    mb_place(n,(uint8_t)id,team,diff,(uint8_t)variant);
}
static void mb_extras(void)
{
    uint32_t scene=WORD_AT(MB_SCENE),mode=WORD_AT(MB_MODE),profile,e=0;
    unsigned humans,bots,n,k,added=0,dropped=0,a;
    mb.extra_valid=0;
    if(WORD_AT(MB_NET) || mode==0xBu || mode==0xCu || mode==0xDu) return;
    if(MB_B(MB_SETUP+0x55u)==10u || MB_B(MB_SETUP+0x56u)==0x13u) return;    /* story type, monkey sub-mode */
    if(scene==102u) {                                         /* editor Test map: custom set members 11-20 */
        if((int32_t)WORD_AT(SETS_INDEX)<0xC || !(e=sets_editor())) return;
    } else if(scene==100u || scene==101u) {
        if(!(mode==9u || (mode==5u && MB_B(MB_SETUP+0x54u)==0x64u)) || !mb.rows_valid) return;
    } else return;
    if(!mb_on()) return;
    humans=WORD_AT(MB_HUMANS);bots=MB_B(MB_SETUP+2u);n=humans+bots;
    if(humans>4u || n>16u) { mb_log("unexpected stock roster; no extra bots",(int)humans,(int)bots);return; }
    memset(mb.extra,0,sizeof mb.extra);
    for(a=0;a<4u;a++) for(k=0;k<16u;k++) mb.extra[a][k]=MB_B(MB_SETUP+MB_ARRAYS+a*16u+k);
    profile=guest_call(sub_00069640,0);
    for(k=0;k<MB_ROWS;k++) {
        int32_t id,variant,team,diff;
        if(e) {                                               /* team and difficulty of row k (slot k+1) */
            uint32_t m=SETS_MEMBER(e+SETS_EDITOR_SET,SETS_STOCK+k),row=MB_WORK+k*MB_ROW;
            id=SETS_H(m);variant=SETS_H(m+6u);
            team=(int32_t)WORD_AT(row+4u);diff=(int32_t)WORD_AT(row+8u);
            if(id<0) continue;
        } else {
            const uint8_t *r=mb.rows+k*MB_ROW;
            int32_t on,over;
            memcpy(&id,r,4);memcpy(&team,r+4,4);memcpy(&diff,r+8,4);
            memcpy(&on,r+0xC,4);memcpy(&over,r+0x10,4);memcpy(&variant,r+0x18,4);
            if(over!=-1) on=over;                             /* as [esi] / [esi-4] at 0x002D36F0 */
            if(id<0 || !on) continue;
        }
        if(n>=MB_ACTORS || bots>=MB_BOTS) { dropped++;continue; }
        mb_bot(n,id,variant,team,diff,profile);
        n++;bots++;added++;
    }
    if(dropped) mb_log("bot rows past the 24-character limit skipped",(int)dropped,(int)n);
    if(!added) return;
    MB_B(MB_SETUP+2u)=(uint8_t)bots;
    mb.humans=humans;mb.bots=bots;mb.extra_valid=1;
    mb_log("extra bots added (added, characters)",(int)added,(int)n);
}
/* WRAP sub_002D2C70 (cdecl, no arguments): match start. Chained with
 * pc_mapmaker_sets_match_start and the story area's hook; order free. */
void pc_mapmaker_matchbots_match_start(void (*original)(void))
{
    mb_unswap();
    original();
    mb_extras();
}

/* ---- live block -------------------------------------------------------- */
/* WRAP sub_000670C0 (no arguments; pending -> live copy at 0x00067167, then
 * the live setup; also the end of sub_00067280). Must be wrapped wherever
 * matchbots.json is applied: the level reads the mirror. */
void pc_mapmaker_matchbots_live(void (*original)(void))
{
    unsigned a,k,humans,bots,n;
    int wide;
    original();
    humans=MB_B(MB_LIVE);bots=(int8_t)MB_B(MB_LIVE+2u)>0?MB_B(MB_LIVE+2u):0u;n=humans+bots;
    wide=mb.extra_valid && !WORD_AT(MB_NET) && MB_B(MB_LIVE+0x54u)==0x64u && humans==mb.humans && bots==mb.bots &&
         n<=MB_ACTORS;
    for(a=0;a<4u;a++) for(k=0;k<64u;k++)
        MB_B(MB_MIRROR+a*64u+k)=k<16u?MB_B(MB_LIVE+MB_ARRAYS+a*16u+k):(wide && k<n?mb.extra[a][k]:0u);
    if(!wide && n>16u && humans<=16u) {
        MB_B(MB_LIVE+2u)=(uint8_t)(16u-humans);
        mb_log("more than 16 characters without their extra bots; clamped",(int)humans,(int)bots);
    }
}
/* WRAP sub_002EF9F0 (cdecl menu; copies the live block to the setup block):
 * entries 0..15 of the mirror (team changes in play) back into the block. */
void pc_mapmaker_matchbots_restart(void (*original)(void))
{
    unsigned a,k;
    for(a=0;a<4u;a++) for(k=0;k<16u;k++) MB_B(MB_LIVE+MB_ARRAYS+a*16u+k)=MB_B(MB_MIRROR+a*64u+k);
    original();
}
