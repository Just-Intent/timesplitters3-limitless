/* Asset-free contract for src/interface/pc_mapmaker_animslots.inc: the anim
 * slot budget of a MapMaker level. Fake guest memory holds the six boss paks'
 * registration tables (their US XBE names and ids, checked against the XBE by
 * test_mapmaker_animslots.py) and the scene's own tables; the fake
 * sub_0005B980 does what the stock one does per entry (id map, slot count,
 * one fixup per anim file, the alias of a name already fixed up) and notes a
 * slot past 940, where the stock load crashed. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00800000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

void sub_0005B980(void) { g_ecx=11; }
void sub_0003DFA0(void) { g_ecx=1; }
static uint32_t guest_call(void (*f)(void),int n,...);

#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_animslots.inc"

#define USED 0x006D77F8u
#define MAP 0x007E03C0u
#define NAMES 0x00700000u
#define GEN1 0x00600000u                /* the scene's own tables */
#define GEN2 0x00640000u
#define ESP0 0x00680000u
#define ESP1 0x00690000u
#define PREFIX 0x0049C690u
#define NFILES 2048u

struct ent { const char *name; uint32_t id; };
/* sub_001CA2F0's tables of the boss paks (US XBE, test_fixture_tables_match_the_xbe) */
static const struct ent tab_lv1_3[]={
    {"mattackheadbutt",0x127},{"mattackleft",0x128},{"mattackright",0x129},{"mhitreact",0x12A},{"mdeath",0x12B},
    {"mrun",0x12C},{"mroar",0x12D},{"mcharge",0x12E},{"balconyfallzombie",0x159},{"balconyfallzombieidle",0x15A},
    {"balconyfallzombiebreak",0x15B},{"entermooseman",0x15C},{"entermoosemanidle",0x15D},{"moosepanel",0x15F},
    {"jbgrosskillit",0x160},{"jblookout",0x193},
};
static const struct ent tab_lv1_6[]={
    {"jbeverybodyout",0x16A},{"princess_start",0x19C},{"princess_low_idle1",0x19D},
    {"princess_attack_big_stamp",0x19E},{"princess_attack_feeble_throw",0x19F},{"princess_attack_headbite",0x1A0},
    {"princess_attack_swipe_long",0x1A1},{"princess_attack_swipe_short",0x1A2},{"princess_raise_up",0x1A3},
    {"princess_raise_attack1",0x1A4},{"princess_raise_react",0x1A5},{"princess_raise_react_end",0x1A6},
    {"courtyard_porch_frame_anim",0x1A7},{"rubblecleared",0x1A8},{"princess_eyeoverwrite",0x1A9},
    {"jbgrossend",0x1AE},{"jbwayout",0x1B6},{"jbclearrubble",0x1B9},
};
static const struct ent tab_lv81[]={
    {"irisdoors_small_open",0x6B9},{"r110mrspacemanpose",0x6BA},{"r110_stand_idle1",0x6BE},
    {"r110_normalwalk",0x6BF},{"r110_run",0x6C0},{"crowewalkertostill",0x6EE},{"crowewalkerwalk",0x6EF},
    {"crowewalkerwalkback",0x6F0},{"crowewalkertowalk",0x6F1},{"crowewalkerjumpleft",0x6F2},
    {"crowewalkerjumpright",0x6F3},{"crowewalkerturnleft",0x6F4},{"crowewalkerturnright",0x6F5},
    {"crowewalkerstun",0x6F6},{"crowewalkerstunscript",0x6F7},{"crowewalkerstillslowfire",0x6F8},
    {"crowewalkerdeath",0x6F9},{"crowewalker180turnrightidle",0x6FA},{"crowewalker180turnright",0x6FB},
    {"crowewalker180turnleft",0x6FC},
};
static const struct ent tab_lv14[]={
    {"r110_run",0x7DB},{"r110_normalwalk",0x7DC},{"r110_stand_idle1",0x7DD},{"talking1",0x7E4},
    {"cortezchutedrop",0x7EC},{"cortezclimbintochute",0x7ED},{"ffp2StandDoor_Open2",0x7EE},
    {"ffp2_Hanger_Door_Open",0x7EF},{"medi_floatidle",0x7F0},{"medi_traverse1",0x7F1},
    {"medi_panictraverse",0x7F2},{"medi_uplinked1",0x7F3},{"medi_death",0x7F4},{"sendroid_ball_idle",0x7F5},
    {"sendroid_fireloop",0x7F6},{"sendroid_rollloop",0x7F7},{"sendroid_rollto",0x7F8},
    {"sendroid_rolltransformNOROT",0x7F9},{"sendroidtransformidle",0x7FA},{"sendroid_transformfrom",0x7FB},
    {"sentrydroiddrop",0x7FC},{"sentrydroiddrop01",0x7FD},{"sendroid_death1",0x7FE},
    {"giantrob_startthroughdoor",0x807},{"giantrob_walkcycle",0x808},{"giantrob_death",0x809},
    {"groinprobe_to",0x80A},{"groinprobe_loop",0x80B},{"groinprobe_from",0x80C},{"splitcrowewalk",0x80E},
    {"splitcrowestandtowalk",0x80F},{"splitcrowewalktostand",0x810},{"splitcrowetantrum",0x811},
    {"splitcrowe90turn",0x812},{"splitcrowefirestill",0x813},{"splitcrowe90turnback",0x814},
    {"splitcrowedeath",0x815},{"eggnoise",0x816},{"vent_cover",0x817},{"r110_liftstartidle",0x818},
    {"jbscaredwalk",0x819},
};
static const struct ent tab_lv11[]={
    {"train_ambient_cam",0xB99},{"LadderTanker",0xB9A},{"11_ht_stealthwalklook",0xB9B},{"11_tippergunaway",0xB9C},
    {"11_tippergunout",0xB9D},{"11_tipperstandidle",0xB9E},{"11_tipperstealthidle",0xB9F},
    {"11_tipperdance",0xBA0},{"helicopter_crash00v04",0xBA1},{"11_htclimbsladder",0xBA2},
    {"11_htdownladder",0xBA3},{"11_povengine_cam",0xBA4},{"11_soldierthrow",0xBA5},{"11_tipperattack",0xBA6},
    {"11_ht_tippercheckgun",0xBA7},{"11_ht_roofwalk",0xBB3},{"11_ht_roofbalanceidle",0xBB4},
    {"11_npc_wagonflameon",0xBCF},{"11_npc_wagonexplode",0xBD0},{"11_npc_rooffall",0xBD1},
    {"11_npc_rooffallbck",0xBD2},{"bazookablast",0xBD3},{"bazookaidle",0xBD4},{"bazookaleftstrafe",0xBD5},
    {"bazookarightstrafe",0xBD6},{"solcrouchidle",0xBD7},{"11_craneswing",0xBDD},{"11_jumprollsoldier",0xBDE},
    {"11_sitsoldier",0xBDF},{"11_fallsoldier",0xBE0},{"11_cranejumper",0xBE1},{"11_tipperchokerun",0xBF6},
    {"khallosidle",0xBFB},
};
static const struct ent tab_lv1_2[]={
    {"flight_cycle",0x13E},{"jbeatinghim",0x144},{"zomthroughwall",0x145},{"barney_barney",0x147},
    {"barney_barney_idle",0x148},{"barney_worm",0x149},{"scientisttreeidle_01",0x14A},
    {"scientisttreeidle_02",0x14B},{"scientisttreeidleslip",0x14C},{"sctistofftree",0x14D},
    {"scientisttreebarney",0x14E},{"sctistfire",0x14F},{"sctisthelpme",0x150},{"sctiststaying",0x151},
    {"poolnurseidle",0x153},{"poolnursereact",0x154},{"poolscientistidle",0x155},{"poolscientistreact",0x156},
    {"ghostgirlidle",0x157},{"ghostgirlflame",0x158},{"jblookout",0x193},{"wrmburst",0x194},{"wrmidle",0x195},
    {"wrmflaming",0x196},{"wrmattack",0x197},{"wrmburrow",0x198},{"wrmhitreact",0x199},{"wrmtantrum",0x19A},
    {"wrmdeath",0x19B},{"jbgrossend",0x1AE},
};
#define N(t) ((uint32_t)(sizeof t/sizeof t[0]))
static const struct { uint32_t table; const struct ent *e; uint32_t n; const char *needed; } bosses[]={
    {0x0048C1B0u,tab_lv1_3,N(tab_lv1_3),"=mattackheadbutt =mattackleft =mattackright =mhitreact =mdeath =mrun =mroar =mcharge"},
    {0x0048C738u,tab_lv1_6,N(tab_lv1_6),"princess_"},
    {0x00484F68u,tab_lv81,N(tab_lv81),"crowewalker"},
    {0x0047E938u,tab_lv14,N(tab_lv14),"giantrob_ splitcrowe =eggnoise"},
    {0x00484410u,tab_lv11,N(tab_lv11),"=khallosidle"},
    {0x0048BEC8u,tab_lv1_2,N(tab_lv1_2),"=barney_worm wrm"},
};
#define LV1_3 0
#define LV1_6 1
#define LV81 2
#define LV14 3
#define LV11 4
#define LV1_2 5

/* needed: the boss's own anims (words: a name prefix, or =name exactly) */
static int needed(int b,const char *name)
{
    const char *w=bosses[b].needed;
    while(*w) {
        size_t n=strcspn(w," ");
        if(*w=='=' ? strlen(name)==n-1u && !strncmp(name,w+1,n-1u) : !strncmp(name,w,n)) return 1;
        w+=n;while(*w==' ') w++;
    }
    return 0;
}

/* ---- the stock registration, per entry --------------------------------- */
static struct { char name[48]; int fixed, fixups; } files[NFILES];
static unsigned nfiles;
static uint32_t ptrs[940];               /* the anim each slot points at (file index + 1) */
static unsigned nreg[0xC03], overflow, originals, nested, lastn;
static uint32_t name_at;
static unsigned file_of(const char *name)
{
    unsigned i;
    for(i=0;i<nfiles;i++) if(!strcmp(files[i].name,name)) return i;
    if(nfiles>=NFILES) { fprintf(stderr,"too many files\n");exit(3); }
    strncpy(files[nfiles].name,name,sizeof files[0].name-1u);
    return nfiles++;
}
static const char *str(uint32_t a) { return (const char *)((uintptr_t)a+g_xbox_mem_offset); }
static void stock_entry(uint32_t e)
{
    uint32_t id=HALF_AT(e+0x10u),slot;
    unsigned f=file_of(str(WORD_AT(e)));
    if(id>=0xC03u) { fprintf(stderr,"id 0x%X\n",(unsigned)id);exit(3); }
    nreg[id]++;
    slot=HALF_AT(MAP+id*2u);
    if(slot==0xFFFFu) { slot=WORD_AT(USED);HALF_AT(MAP+id*2u)=(uint16_t)slot; }   /* 0x00057FC1 */
    if(slot>=940u) { overflow++;return; }                     /* writes [0x007E3954]: the crash */
    ptrs[slot]=f+1u;                                          /* 0x00057FE4 */
    if(files[f].fixed) return;                                /* flag 4: no second fixup, no count */
    files[f].fixed=1;files[f].fixups++;
    if(slot==WORD_AT(USED)) WORD_AT(USED)=slot+1u;            /* 0x000581EC */
}
static void fake_orig(void)
{
    uint32_t t=WORD_AT(g_esp+8u),n=WORD_AT(g_esp+12u),k;
    if(g_esp==ESP0) originals++;   /* the level's own call (not a nested one) */
    lastn=n;
    CHECK(WORD_AT(g_esp+4u)==PREFIX);                         /* the caller's path prefix */
    for(k=0;k<n;k++) stock_entry(t+k*0x18u);
    g_esp+=4u;
}
static void hook(void (*original)(void)) { if(ans_register()) return;original(); }   /* pc_mapmaker_worm_register */
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[3]={0},sp=g_esp;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<3;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f!=sub_0005B980 || n!=3) { fprintf(stderr,"unexpected guest_call\n");exit(3); }
    nested++;
    g_esp=ESP1;WORD_AT(ESP1)=0x0005B9F0u;WORD_AT(ESP1+4u)=x[0];WORD_AT(ESP1+8u)=x[1];WORD_AT(ESP1+12u)=x[2];
    hook(fake_orig);                                          /* the wrapper again: the nested call */
    if(g_esp!=ESP1+4u) { fprintf(stderr,"nested esp %+d\n",(int)(g_esp-ESP1));failures++; }
    g_esp=sp;
    return 0;
}

/* ---- tables in guest memory --------------------------------------------- */
static uint32_t put_name(const char *s)
{
    uint32_t a=name_at;
    size_t n=strlen(s)+1u;
    memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),s,n);
    name_at+=(uint32_t)((n+3u)&~3u);
    return a;
}
static void put_table(uint32_t t,const struct ent *e,uint32_t n)
{
    uint32_t k;
    for(k=0;k<n;k++) {
        WORD_AT(t+k*0x18u)=put_name(e[k].name);
        WORD_AT(t+k*0x18u+8u)=0x82u;WORD_AT(t+k*0x18u+12u)=0x1800u;
        HALF_AT(t+k*0x18u+0x10u)=(uint16_t)e[k].id;
    }
}
static char gnames[1300][12];
/* the scene's own anims: names g<first>.. with ids 0x200+ (no boss pak's: 0x1BA..0x6B8 free); entry 0 optionally another */
static void put_gen(uint32_t t,uint32_t first,uint32_t n,const struct ent *e0)
{
    uint32_t k;
    for(k=0;k<n;k++) {
        uint32_t g=first+k;
        snprintf(gnames[g],sizeof gnames[0],"g%u",(unsigned)g);
        WORD_AT(t+k*0x18u)=put_name(gnames[g]);
        HALF_AT(t+k*0x18u+0x10u)=(uint16_t)(0x200u+g);
    }
    if(e0) { WORD_AT(t)=put_name(e0->name);HALF_AT(t+0x10u)=(uint16_t)e0->id; }
}
static uint32_t gen_id(uint32_t g) { return 0x200u+g; }

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void reg(uint32_t table,uint32_t count,unsigned *norig)
{
    unsigned before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=0x001CB1C8u;WORD_AT(ESP0+4u)=PREFIX;WORD_AT(ESP0+8u)=table;WORD_AT(ESP0+12u)=count;
    hook(fake_orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d (table 0x%X)\n",(int)(g_esp-ESP0),(unsigned)table);failures++; }
    CHECK(WORD_AT(ESP0+4u)==PREFIX && WORD_AT(ESP0+8u)==table && WORD_AT(ESP0+12u)==count);   /* as pushed */
    if(norig) *norig+=originals-before;
}
/* a level load: the id map reset (sub_0005C420), then the tables in sub_001CA2F0's order
 * (lv14, the scene's paks, lv11, lv81, lv1_2, lv1_3, lv1_6, grenade.pak's 16 last) */
static unsigned load(uint32_t ngen,int all_bosses,unsigned *norig)
{
    memset((void *)((uintptr_t)MAP+g_xbox_mem_offset),0xFF,0xC03u*2u);
    WORD_AT(USED)=0x23u;
    memset(ptrs,0,sizeof ptrs);memset(nreg,0,sizeof nreg);
    { unsigned i;for(i=0;i<nfiles;i++) { files[i].fixed=0;files[i].fixups=0; } }
    overflow=0;nested=0;
    if(norig) *norig=0;
    reg(bosses[LV14].table,bosses[LV14].n,norig);
    reg(GEN1,ngen,norig);
    reg(bosses[LV11].table,bosses[LV11].n,norig);
    reg(bosses[LV81].table,bosses[LV81].n,norig);
    reg(bosses[LV1_2].table,bosses[LV1_2].n,norig);
    if(all_bosses) reg(bosses[LV1_3].table,bosses[LV1_3].n,norig);
    reg(bosses[LV1_6].table,bosses[LV1_6].n,norig);
    reg(GEN2,16u,norig);
    return WORD_AT(USED);
}
/* id -> its anim: registered, in the slot table, pointing at that name's file */
static int valid(uint32_t id,const char *name)
{
    uint32_t slot=HALF_AT(MAP+id*2u);
    return slot!=0xFFFFu && slot<940u && ptrs[slot] && !strcmp(files[ptrs[slot]-1u].name,name);
}
static int fixups_ok(void) { unsigned i;for(i=0;i<nfiles;i++) if(files[i].fixups>1) return 0;return 1; }

int main(void)
{
    unsigned b,k,used,norig,stock_used;
    g_xbox_mem_offset=(uintptr_t)memory;
    name_at=NAMES;
    for(b=0;b<6u;b++) put_table(bosses[b].table,bosses[b].e,bosses[b].n);
    put_gen(GEN1,0,800u,&tab_lv11[32]);                       /* the scene's paks also name khallosidle (shared) */
    put_gen(GEN2,1000u,16u,NULL);
    WORD_AT(0x0079094Cu)=100u;
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- stock (limits off): every boss pak's whole table overflows ------ */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    stock_used=load(800u,1,&norig);
    CHECK(norig==8u && nested==0 && overflow>0);              /* slot 940 reached: the .34 crash */

    /* ---- scene 100: each boss only its own anims, every id once ---------- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    used=load(800u,1,&norig);
    CHECK(!overflow && used<=940u && fixups_ok());
    CHECK(norig==2u);                                         /* the scene's paks fit: stock */
    CHECK(used==0x23u+800u+16u+8u+12u+15u+12u+0u+9u);         /* khallosidle: the scene pak's copy */
    for(b=0;b<6u;b++) for(k=0;k<bosses[b].n;k++) {
        const struct ent *e=&bosses[b].e[k];
        if(needed((int)b,e->name)) {
            if(!valid(e->id,e->name)) { fprintf(stderr,"boss %u: %s 0x%X not valid\n",b,e->name,(unsigned)e->id);failures++; }
            if(nreg[e->id]!=1u) { fprintf(stderr,"boss %u: %s 0x%X registered %u times\n",b,e->name,(unsigned)e->id,nreg[e->id]);failures++; }
        } else if(HALF_AT(MAP+e->id*2u)!=0xFFFFu) {
            fprintf(stderr,"boss %u: %s 0x%X registered (not the boss's)\n",b,e->name,(unsigned)e->id);failures++;
        }
    }
    for(k=1;k<800u;k++) if(!valid(gen_id(k),gnames[k])) { fprintf(stderr,"gen %u not valid\n",k);failures++;break; }
    for(k=0;k<16u;k++) if(!valid(gen_id(1000u+k),gnames[1000u+k])) { fprintf(stderr,"grenade %u not valid\n",k);failures++;break; }
    CHECK(nreg[0xBFBu]==1u && valid(0xBFBu,"khallosidle"));  /* shared: the first copy kept */
    CHECK(nested==8u+12u+15u+12u+0u+9u);
    /* lv1_3 left out (the .34 set): the same, 8 fewer */
    CHECK(load(800u,0,&norig)==used-8u && !overflow);
    CHECK(ans.kept && ans.dropped && ans.shared);

    /* ---- a scene that does not fit: never past 940, the shared id once --- */
    put_gen(GEN1,0,900u,&tab_lv11[32]);
    put_gen(GEN2,1000u,16u,NULL);
    WORD_AT(GEN2+3u*0x18u)=WORD_AT(GEN1+5u*0x18u);HALF_AT(GEN2+3u*0x18u+0x10u)=(uint16_t)gen_id(5u);   /* g5 again */
    used=load(900u,1,&norig);
    CHECK(!overflow && used==940u && fixups_ok());
    CHECK(norig==0u);                                         /* both scene tables entry by entry */
    CHECK(nreg[gen_id(5u)]==1u && valid(gen_id(5u),"g5"));
    for(k=0;k<12u;k++) CHECK(valid(bosses[LV14].e[k<3u?23u+k:26u+k].id,bosses[LV14].e[k<3u?23u+k:26u+k].name));   /* lv14's, registered first */
    CHECK(ans.full>0);
    put_gen(GEN1,0,800u,&tab_lv11[32]);
    put_gen(GEN2,1000u,16u,NULL);

    /* ---- story levels: stock, whatever the tables ------------------------ */
    WORD_AT(0x0079094Cu)=1u;
    CHECK(load(800u,1,&norig)==stock_used && norig==8u && nested==0 && overflow>0);
    WORD_AT(0x0079094Cu)=100u;
    /* a boss pak's address with another count: not that table, stock (fits) */
    load(10u,0,NULL);
    nested=0;
    { unsigned o=0;reg(bosses[LV1_3].table,3u,&o);CHECK(o==1u && nested==0 && lastn==3u); }

    /* ---- the story creatures' paks (pc_mapmaker_creatures.def): only each row's run ---- */
    {
        static const struct { uint32_t table, count, first, n; } crt[]={
            {0x00488850u,0x2Au,30u,4u},{0x00482610u,0x46u,3u,11u},{0x00480C60u,0x49u,41u,6u}};
        unsigned c;
        for(c=0;c<3u;c++) {
            uint32_t t=crt[c].table;
            put_gen(t,1020u+c*0x50u,crt[c].count,NULL);         /* fresh names; ids below */
            for(k=0;k<crt[c].count;k++) HALF_AT(t+k*0x18u+0x10u)=(uint16_t)(0xA00u+c*0x80u+k);
        }
        load(10u,0,NULL);
        for(c=0;c<3u;c++) {
            unsigned o=0;
            nested=0;
            reg(crt[c].table,crt[c].count,&o);
            CHECK(o==0u && nested==crt[c].n);
            for(k=0;k<crt[c].count;k++) {
                int in=k>=crt[c].first && k<crt[c].first+crt[c].n;
                CHECK((HALF_AT(MAP+(0xA00u+c*0x80u+k)*2u)!=0xFFFFu)==in);
            }
        }
    }
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("ok");
    return 0;
}
