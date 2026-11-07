/* Asset-free contract for src/interface/pc_mapmaker_vehicles.inc (story
 * assets wave 3: the Machine Wars future tank and the Scotland the Brave
 * truck as variants of the MapMaker vehicle). Fake guest memory holds a map
 * container (map info, piece chunk with vehicle / pickup items), the editor's
 * piece list and nodes, the scene-100 resource table, requests and pak
 * mounts; the fake originals pop their return address like the lifted
 * callees and every hook call checks esp. */
#define TSFP_MAPMAKER_VEHICLES_TEST 1
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00A00000/4], g_eax, g_ebp, g_ebx, g_ecx, g_edx, g_esi, g_esp;
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
STUB(sub_0003DFA0,1) STUB(sub_00075B70,2) STUB(sub_00071B10,3) STUB(sub_00071C50,4) STUB(sub_00079DE0,5)
STUB(sub_0007D4A0,6) STUB(sub_00071800,7) STUB(sub_00078590,8) STUB(sub_000783D0,9) STUB(sub_00079D60,10)
STUB(sub_00075E70,11) STUB(sub_00077320,12) STUB(sub_0003D7B0,13) STUB(sub_00333700,14) STUB(sub_002FDF50,15)
STUB(sub_000622D0,16) STUB(sub_00062260,17) STUB(sub_000631C0,18) STUB(sub_00096220,19) STUB(sub_000B3B20,20) STUB(sub_000810C0,21)

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define EDITOR 0x00200000u
#define PIECES 0x00280000u
#define NODES 0x00290000u
#define MENU 0x00301000u
#define HEAP 0x00400000u
#define RESTAB 0x00500000u
#define REQS 0x00510000u
#define PATHS 0x00520000u
#define FILES 0x00530000u
#define VREC 0x00560000u
#define TEXTS 0x00700000u

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t heap_next=HEAP;
static uint32_t xbox_HeapAlloc(uint32_t size,uint32_t align)
{
    uint32_t p=(heap_next+align-1u)&~(align-1u);
    heap_next=p+size;
    return p;
}
/* recorded calls */
static uint32_t options[16][2],binds[8],mounts[8],textured[16],templates[16];
static unsigned nhud,hud_id;
static unsigned noptions,nbinds,nchoices,nends,ndone,nframe,nmounts,ntextured,ntemplates,originals;
static uint32_t mounted_paths[8];
static unsigned nmounted;
static int mount_fails;
static uint32_t pak_tex_crc[16],pak_tex_n;         /* what a pak mounted next lists */
static uint32_t crc_name(const char *s)
{
    uint32_t c=0xFFFFFFFFu;
    int k;
    for(;*s;s++) { c^=(uint8_t)*s; for(k=0;k<8;k++) c=c&1u?(c>>1)^0xEDB88320u:c>>1; }
    return ~c;
}
/* the models: row r, request path PATHS + r*0x100, request REQS + r*0x100 */
static const uint32_t models[]={0xBu,0xCu,0xDu,0x54Du,0x54Eu,0x54Fu,7u,0xEu};
#define NMODELS (sizeof models/sizeof models[0])
static unsigned row_of(uint32_t m) { unsigned r; for(r=0;r<NMODELS;r++) if(models[r]==m) return r; return 99u; }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00333700) r=1u;
    else if(f==sub_002FDF50) r=0u;
    else if(f==sub_0003D7B0) r=TEXTS+(x[0]&0xFFFFu)*0x10u;
    else if(f==sub_00071800) { if(noptions<16u) { options[noptions][0]=x[0];options[noptions][1]=x[1]; } noptions++; }
    else if(f==sub_00078590) { if(nbinds<8u) binds[nbinds]=x[0]; nbinds++; }
    else if(f==sub_00079DE0) nchoices++;
    else if(f==sub_000783D0) nends++;
    else if(f==sub_00079D60) { ndone++;CHECK(x[4]==0x002FB3C0u && x[0]==TEXTS+0xB5Eu*0x10u); }
    else if(f==sub_00075B70) { nframe++;CHECK(x[0]==0x8400u && x[4]==0x002FB3C0u); }
    else if(f==sub_00062260) {
        unsigned k;
        r=0xFFFFFFFFu;
        for(k=0;k<nmounted;k++) if(!strcmp((const char *)((uintptr_t)mounted_paths[k]+g_xbox_mem_offset),(const char *)((uintptr_t)x[0]+g_xbox_mem_offset))) r=k;
    }
    else if(f==sub_000622D0) {
        CHECK(x[1]==0u && x[2]==1u);                  /* kept open: index only */
        if(nmounts<8u) mounts[nmounts]=x[0];
        nmounts++;
        if(mount_fails) r=0xFFFFFFFFu;
        else {
            if(nmounted<8u) {
                uint32_t slot=0x00907000u+nmounted*0x1Cu,index=0x00540000u+nmounted*0x1000u;
                WORD_AT(slot+8u)=pak_tex_n;WORD_AT(slot+0x10u)=index;
                for(i=0;i<(int)pak_tex_n;i++) WORD_AT(index+(uint32_t)i*16u)=pak_tex_crc[i];
                mounted_paths[nmounted++]=x[0];
            }
            WORD_AT(0x006D7880u)++;
        }
    }
    else if(f==sub_000631C0) r=x[0]>=PATHS && x[0]<PATHS+NMODELS*0x100u?REQS+(x[0]-PATHS):0u;
    else if(f==sub_00096220) { if(ntextured<16u) textured[ntextured]=x[0]; ntextured++; }
    else if(f==sub_000B3B20) { if(ntemplates<16u) templates[ntemplates]=x[0]; ntemplates++; }
    else if(f==sub_000810C0) { nhud++;hud_id=x[0];WORD_AT(0x004D2D50u+x[0]*0x18u+0x10u)=1u; }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_vehicles.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}
static void call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,int line)
{
    uint32_t sp=g_esp;
    g_esp-=12u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a0;WORD_AT(g_esp+8u)=a1;
    hook(orig);
    if(g_esp!=sp-8u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-(sp-8u)));failures++; }
    g_esp=sp;
}
#define CALL(h,o,ret,a0,a1) call_hook(h,o,ret,a0,a1,__LINE__)
static void fake_plain(void) { originals++;g_esp+=4u; }
static uint32_t fake_answer;
static void fake_answer_fn(void) { originals++;g_eax=fake_answer;g_esp+=4u; }
/* the create: the builder reads the record's +0x28..+0x34 */
static uint32_t seen_rec[4];
#define BLOCK 0x00580000u
#define GUN0 0x00581000u
#define GUN1 0x00581100u
static uint32_t def_flags=0x17u;                    /* the builder copies def +0xA8 to block +0x58 */
static void fake_create(void)
{
    uint32_t obj=WORD_AT(g_esp+4u),rec=WORD_AT(g_esp+8u);
    originals++;
    WORD_AT(obj+0x7Cu)=BLOCK;WORD_AT(BLOCK+0x58u)=def_flags;WORD_AT(BLOCK+0x140u)=2u;
    /* the builder: block +0x50 = the def, two guns with the templates' pitch limits */
    WORD_AT(BLOCK+0x50u)=0x004E8658u;WORD_AT(BLOCK+0x164u)=2u;
    WORD_AT(BLOCK+0x144u)=GUN0;WORD_AT(BLOCK+0x154u)=GUN1;
    FLOAT_AT(GUN0+0x34u)=0.0f;FLOAT_AT(GUN0+0x38u)=0.001f;FLOAT_AT(GUN1+0x34u)=-45.0f;FLOAT_AT(GUN1+0x38u)=0.0f;
    seen_rec[0]=WORD_AT(rec+0x28u);seen_rec[1]=WORD_AT(rec+0x2Cu);seen_rec[2]=WORD_AT(rec+0x30u);seen_rec[3]=WORD_AT(rec+0x34u);
    g_esp+=4u;
}

/* ---- map and editor ---------------------------------------------------- */
typedef struct { int id,rot,cell,var; } Item;
typedef struct { int x,y,z,floor; int nitems; Item items[4]; } Piece;
static uint32_t rec_at[4][4],node_at[4][4];
static void container(const Piece *ps,unsigned np)
{
    uint32_t c=CONT,p,start;
    unsigned i,j;
    memset((void *)((uintptr_t)c+g_xbox_mem_offset),0,0x80010u);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4u)=0x12Du;
    WORD_AT(c+8u)=0x00C77668u;WORD_AT(c+0xCu)=0x3DCu;
    WORD_AT(c+0x10u+4u)=np;
    p=c+0x10u+0x3DCu;
    WORD_AT(p)=0x00C77669u;start=p+8u;p=start;
    for(i=0;i<np;i++) {
        WORD_AT(p)=7u;BYTE_AT(p+4u)=(uint8_t)ps[i].x;BYTE_AT(p+5u)=(uint8_t)ps[i].y;BYTE_AT(p+6u)=(uint8_t)ps[i].z;
        BYTE_AT(p+7u)=(uint8_t)ps[i].floor;HALF_AT(p+0x1Eu)=(int16_t)ps[i].nitems;
        p+=0x28u+8u;                                   /* one pad (sub_00333700 = 1) */
        for(j=0;j<(unsigned)ps[i].nitems;j++) {
            rec_at[i][j]=p;
            HALF_AT(p+0xEu)=(int16_t)ps[i].items[j].id;HALF_AT(p+0x10u)=(int16_t)ps[i].items[j].rot;
            BYTE_AT(p+0x15u)=(uint8_t)ps[i].items[j].cell;BYTE_AT(p+0x16u)=1u;BYTE_AT(p+0x17u)=2u;
            BYTE_AT(p+0xCu)=(uint8_t)ps[i].items[j].var;
            p+=0x18u;
        }
    }
    WORD_AT(start-4u)=p-start;
    WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=p-c;
    WORD_AT(0x00784014u)=c;
}
static void editor(const Piece *ps,unsigned np,uint32_t junk)
{
    unsigned i,j;
    uint32_t prev=0;
    WORD_AT(0x0078400Cu)=0;
    for(i=0;i<np;i++) {
        uint32_t pc=PIECES+i*0x100u,nprev=0;
        memset((void *)((uintptr_t)pc+g_xbox_mem_offset),0,0x100);
        FLOAT_AT(pc)=(float)ps[i].x+0.75f;FLOAT_AT(pc+4u)=(float)ps[i].y;FLOAT_AT(pc+8u)=(float)ps[i].z+0.25f;
        BYTE_AT(pc+0xCu)=(uint8_t)ps[i].floor;
        if(prev) WORD_AT(prev+0x40u)=pc; else WORD_AT(0x0078400Cu)=pc;
        prev=pc;
        for(j=0;j<(unsigned)ps[i].nitems;j++) {
            uint32_t n=NODES+i*0x400u+j*0x40u;
            memset((void *)((uintptr_t)n+g_xbox_mem_offset),0,0x40);
            WORD_AT(n)=(uint32_t)ps[i].items[j].id;
            BYTE_AT(n+8u)=(uint8_t)ps[i].items[j].rot;BYTE_AT(n+9u)=(uint8_t)(ps[i].items[j].rot>>8);
            BYTE_AT(n+0xCu)=(uint8_t)ps[i].items[j].cell;BYTE_AT(n+0xEu)=1u;BYTE_AT(n+0x10u)=2u;
            WORD_AT(n+0x30u)=ps[i].items[j].var?0x56480000u|(uint32_t)ps[i].items[j].var:junk;
            if(nprev) WORD_AT(nprev+0x14u)=n; else WORD_AT(pc+0x3Cu)=n;
            nprev=n;node_at[i][j]=n;
        }
    }
}
static const Piece map[2]={
    {3,0,5,1,3,{{0x2B,0,4,1},{0x2B,0,2,0},{0x11,0,3,2}}},     /* tank, theme vehicle, pickup (its +0xC: weaponset's) */
    {4,0,5,1,2,{{0x2B,0x100,1,2},{0x2B,0,2,3}}},              /* truck with gun, truck */
};

static void compile_tests(void)
{
    container(map,2);editor(map,2,0x12345678u);
    BYTE_AT(rec_at[0][0]+0xCu)=0;BYTE_AT(rec_at[1][0]+0xCu)=0;BYTE_AT(rec_at[1][1]+0xCu)=0;
    BYTE_AT(rec_at[0][2]+0xCu)=0x55u;                   /* the pickup's byte is weaponset's */
    BYTE_AT(rec_at[0][1]+0xCu)=0x77u;                   /* a stale byte on a theme vehicle */
    originals=0;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(originals==1u);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==1u && BYTE_AT(rec_at[0][1]+0xCu)==0u && BYTE_AT(rec_at[1][0]+0xCu)==2u);
    CHECK(BYTE_AT(rec_at[1][1]+0xCu)==3u && BYTE_AT(rec_at[0][2]+0xCu)==0x55u);
    /* an untagged value is the theme vehicle; a tagged value out of range too */
    WORD_AT(node_at[1][0]+0x30u)=2u;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[1][0]+0xCu)==0u);
    WORD_AT(node_at[1][0]+0x30u)=0x56480004u;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[1][0]+0xCu)==0u);
    WORD_AT(node_at[1][0]+0x30u)=0x56480002u;
    /* the storyprops tag is not ours */
    WORD_AT(node_at[0][0]+0x30u)=0x53500001u;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==0u && BYTE_AT(rec_at[1][0]+0xCu)==2u);
    WORD_AT(node_at[0][0]+0x30u)=0x56480001u;
    /* switch off: the original only */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    BYTE_AT(rec_at[0][0]+0xCu)=0;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=100u;                           /* outside the editor: nothing */
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==0u);
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_vehicles_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==1u);
}
static void load_tests(void)
{
    container(map,2);editor(map,2,0xDEADBEEFu);
    WORD_AT(node_at[0][0]+0x30u)=0x56480003u;              /* garbage everywhere first */
    WORD_AT(node_at[1][0]+0x30u)=0x11u;
    WORD_AT(node_at[0][1]+0x30u)=0x56480001u;
    WORD_AT(node_at[0][2]+0x30u)=0x57530003u;              /* weaponset's pickup slot: untouched */
    BYTE_AT(rec_at[0][1]+0xCu)=9u;                          /* an unknown byte loads as the theme vehicle */
    CALL(pc_mapmaker_vehicles_load,fake_plain,0x00303076u,CONT,0);
    CHECK(WORD_AT(node_at[0][0]+0x30u)==0x56480001u && WORD_AT(node_at[1][0]+0x30u)==0x56480002u);
    CHECK(WORD_AT(node_at[0][1]+0x30u)==0x56480000u && WORD_AT(node_at[1][1]+0x30u)==0x56480003u);
    CHECK(WORD_AT(node_at[0][2]+0x30u)==0x57530003u);
    /* a node that moved off its record (other cell) keeps the theme vehicle */
    BYTE_AT(node_at[1][0]+0xCu)=9u;
    CALL(pc_mapmaker_vehicles_load,fake_plain,0x00303076u,CONT,0);
    CHECK(WORD_AT(node_at[1][0]+0x30u)==0x56480000u);
    BYTE_AT(node_at[1][0]+0xCu)=1u;
    CALL(pc_mapmaker_vehicles_load,fake_plain,0x00303076u,CONT,0);
    CHECK(WORD_AT(node_at[1][0]+0x30u)==0x56480002u);
}
static void menu_tests(void)
{
    uint32_t cat,d;
    container(map,2);editor(map,2,0);
    /* the category entry: a copy with our descriptor for a selected vehicle */
    WORD_AT(0x00524BE0u+35u*24u)=0x9D7u;WORD_AT(0x00524BE0u+35u*24u+4u)=4u;WORD_AT(0x00524BE0u+35u*24u+8u)=0;
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][1];
    fake_answer=0x00524BE0u+35u*24u;
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x2Bu,0);
    cat=g_eax;
    CHECK(cat!=fake_answer && cat==vh.cat && WORD_AT(cat)==0x9D7u && WORD_AT(cat+4u)==4u && (d=WORD_AT(cat+8u))==vh.desc);
    CHECK(WORD_AT(d)==0x00319300u && WORD_AT(d+4u)==0x003192E0u && !WORD_AT(d+8u) && !WORD_AT(d+12u));
    CHECK(WORD_AT(0x00524BE0u+35u*24u+8u)==0u);            /* the stock table is untouched */
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051D7u,0x2Bu,0);CHECK(g_eax==vh.cat);
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x00305000u,0x2Bu,0);CHECK(g_eax==fake_answer);   /* other caller */
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x49u,0);CHECK(g_eax==fake_answer);   /* other item */
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][2];
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x2Bu,0);CHECK(g_eax==fake_answer);   /* not the selection */
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][1];
    WORD_AT(0x00524BE0u+35u*24u+8u)=0x0052DECCu;            /* an entry with its own menu is kept */
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x2Bu,0);CHECK(g_eax==fake_answer);
    WORD_AT(0x00524BE0u+35u*24u+8u)=0;
    WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x2Bu,0);CHECK(g_eax==fake_answer);
    WORD_AT(0x0079094Cu)=102u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_vehicles_category,fake_answer_fn,0x003051B5u,0x2Bu,0);CHECK(g_eax==fake_answer);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* the vehicle menu: frame, a Vehicle choice of four bound to the node, Done; the original never runs */
    WORD_AT(MENU+4u)=vh.desc;WORD_AT(node_at[0][1]+0x30u)=0xABCDu;
    noptions=nbinds=nchoices=nends=ndone=nframe=originals=0;
    CALL(pc_mapmaker_vehicles_build,fake_plain,0x00078F74u,MENU,0);
    CHECK(originals==0u && nframe==1u && nchoices==1u && nends==1u && ndone==1u && WORD_AT(MENU+0x218u)==1u);
    CHECK(noptions==4u && options[0][1]==0x56480000u && options[1][1]==0x56480001u);
    CHECK(options[2][1]==0x56480002u && options[3][1]==0x56480003u);
    CHECK(!strcmp((const char *)((uintptr_t)options[1][0]+g_xbox_mem_offset),"Future tank (Machine Wars)"));
    CHECK(!strcmp((const char *)((uintptr_t)options[2][0]+g_xbox_mem_offset),"Castle truck + Vickers gun"));
    CHECK(nbinds==1u && binds[0]==node_at[0][1]+0x30u && WORD_AT(node_at[0][1]+0x30u)==0x56480000u);
    WORD_AT(node_at[0][1]+0x30u)=0x56480002u;               /* a kept choice stays */
    CALL(pc_mapmaker_vehicles_build,fake_plain,0x00078F74u,MENU,0);CHECK(WORD_AT(node_at[0][1]+0x30u)==0x56480002u);
    WORD_AT(MENU+4u)=0x0052DECCu;                           /* the pickup options: stock */
    originals=0;CALL(pc_mapmaker_vehicles_build,fake_plain,0x00078F74u,MENU,0);CHECK(originals==1u);
}

/* scene-100 resources: model m in row row_of(m), its request path PATHS + row*0x100 */
static void resources(int read_mask,uint8_t deferred)
{
    unsigned r;
    WORD_AT(0x00791D4Cu)=RESTAB;
    for(r=0;r<0xA00u;r++) HALF_AT(0x008CA000u+r*2u)=-1;
    for(r=0;r<NMODELS;r++) {
        uint32_t f=FILES+r*0x1000u,q=REQS+r*0x100u;
        HALF_AT(0x008CA000u+models[r]*2u)=(int16_t)(10u+r);
        WORD_AT(RESTAB+(10u+r)*0x30u+4u)=0;WORD_AT(RESTAB+(10u+r)*0x30u+0x10u)=PATHS+r*0x100u;
        WORD_AT(q+0x10u)=(read_mask>>r)&1?f:0u;BYTE_AT(q+2u)=deferred;
        /* each file lists 2 textures (rows {id, id, 0, 0}, -1 ends) */
        WORD_AT(f)=0x10u;
        WORD_AT(f+0x10u)=WORD_AT(f+0x14u)=0x100000u+r*2u;WORD_AT(f+0x20u)=WORD_AT(f+0x24u)=0x100001u+r*2u;
        WORD_AT(f+0x30u)=0xFFFFFFFFu;
    }
}
static void fill_record(uint32_t rec,uint32_t model,uint32_t def)
{
    memset((void *)((uintptr_t)rec+g_xbox_mem_offset),0,0x40);
    WORD_AT(rec)=0xFFFFFFFFu;WORD_AT(rec+8u)=model;WORD_AT(rec+0x24u)=def;
    WORD_AT(rec+0x28u)=0;WORD_AT(rec+0x2Cu)=1u;WORD_AT(rec+0x30u)=0xFFFFFFFFu;WORD_AT(rec+0x34u)=0x77u;
    FLOAT_AT(rec+0x10u)=1.0f;                         /* the pad y */
}
static int near(float a,float b) { return a>b-1e-4f && a<b+1e-4f; }
/* the fill's vehicle case: the def call, then +0x24 = def, +0x28 0, +0x2C 1, +0x30 -1 (0x00241D3C..0x00241D4D) */
static void fill_def(uint32_t rec,uint32_t item,int line)
{
    g_ebp=rec;fill_record(rec,0x894u,5u);g_esi=item;
    call_hook(pc_mapmaker_vehicles_def,fake_answer_fn,0x00241D39u,0x2Bu,0,line);
    WORD_AT(rec+0x24u)=g_eax;WORD_AT(rec+0x28u)=0;WORD_AT(rec+0x2Cu)=1u;WORD_AT(rec+0x30u)=0xFFFFFFFFu;
}
#define FILL(rec,item) fill_def(rec,item,__LINE__)
static void play_tests(void)
{
    unsigned i;
    WORD_AT(0x0079094Cu)=100u;
    container(map,2);
    resources(0xFF,0x10u);
    /* mount: both stream paks, kept open; once each */
    WORD_AT(0x006D7880u)=40u;nmounts=nmounted=0;originals=0;
    pak_tex_n=0;
    for(i=0;i<16u;i++) { char name[32];snprintf(name,sizeof name,"textures/%06x.xbt",0x100000u+i);pak_tex_crc[pak_tex_n++]=crc_name(name); }
    pak_tex_n=15u;                                          /* one texture of the last model missing */
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);
    CHECK(originals==1u && nmounts==3u && vh_lv.want[1]==1u && vh_lv.want[2]==1u && vh_lv.want[3]==1u && vh_lv.want[0]==1u);
    CHECK(!strcmp((const char *)((uintptr_t)mounts[0]+g_xbox_mem_offset),"pak/stream/lv5/prop1.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounts[1]+g_xbox_mem_offset),"pak/stream/lv4/prop3.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounts[2]+g_xbox_mem_offset),"pak/story/l_5_st.pak"));
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==3u);
    /* the texture queue's load: the tank, its two guns, the truck and the Vickers, once */
    ntextured=0;originals=0;
    CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);
    CHECK(originals==1u && ntextured==5u);
    CHECK(textured[0]==PATHS+0u*0x100u && textured[1]==PATHS+1u*0x100u && textured[2]==PATHS+2u*0x100u);
    CHECK(textured[3]==PATHS+6u*0x100u && textured[4]==PATHS+7u*0x100u);
    CHECK(!(BYTE_AT(REQS+2u)&0x10u) && (BYTE_AT(REQS+3u*0x100u+2u)&0x10u));   /* gibs: never drawn here */
    BYTE_AT(REQS+2u)=0x10u;
    CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==5u && originals==2u);
    /* preload: the variant model is requested, then its def and gun templates */
    g_esi=rec_at[0][0];fake_answer=0x894u;
    CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0x00524F58u+0x2Bu*0xC4u,0);CHECK(g_eax==0xBu);
    g_esi=rec_at[0][1];CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x894u);
    g_esi=rec_at[1][0];CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==7u);
    g_esi=rec_at[1][1];CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==7u);
    CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5D0u,0,0);CHECK(g_eax==0x894u);   /* other caller */
    g_esi=rec_at[0][2];fake_answer=0x123u;                  /* a pickup with a +0xC byte: never ours */
    CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x123u);
    ntemplates=0;fake_answer=5u;
    g_esi=rec_at[0][0];CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(g_eax==2u);
    CHECK(ntemplates==2u && templates[0]==0x004E8300u+0x8Cu && templates[1]==0x004E8300u+2u*0x8Cu);
    CHECK(nhud==1u && hud_id==0x125u && vh_lv.reticle_logged);    /* the tank reticle, once */
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(nhud==1u);ntemplates=2u;
    g_esi=rec_at[1][0];CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(g_eax==0u);
    CHECK(ntemplates==3u && templates[2]==0x004E8300u);
    g_esi=rec_at[1][1];CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(g_eax==0u && ntemplates==3u);
    g_esi=rec_at[0][1];CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(g_eax==5u && ntemplates==3u);
    g_esi=rec_at[0][0];g_ebp=VREC;fill_record(VREC,0x894u,5u);  /* another caller (a ready variant and a record in ebp) */
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F630u,0x2Bu,0);CHECK(g_eax==5u && ntemplates==3u && WORD_AT(VREC+8u)==0x894u);
    /* fill: def and model of a read variant; ebp = the vehicle record */
    FILL(VREC,rec_at[0][0]);CHECK(g_eax==2u && WORD_AT(VREC+8u)==0xBu);
    FILL(VREC+0x40u,rec_at[0][1]);CHECK(g_eax==5u && WORD_AT(VREC+0x48u)==0x894u);
    FILL(VREC+0x80u,rec_at[1][0]);CHECK(g_eax==0u && WORD_AT(VREC+0x88u)==7u);
    FILL(VREC+0xC0u,rec_at[1][1]);CHECK(g_eax==0u && WORD_AT(VREC+0xC8u)==7u);
    CHECK(vh_lv.nrec==3u);
    /* spawn height: each story model's depth below its origin + 0.01; the theme vehicle stays at the pad */
    CHECK(near(FLOAT_AT(VREC+0x10u),1.0f+2.375f) && near(FLOAT_AT(VREC+0x50u),1.0f));
    CHECK(near(FLOAT_AT(VREC+0x90u),1.0f+0.945f) && near(FLOAT_AT(VREC+0xD0u),1.0f+0.945f));
    CHECK(vh_lv.lift_logged[0] && vh_lv.lift_logged[1]);
    /* create: the story fields before the builder; the stock buggy and other callers untouched */
    for(i=0;i<4u;i++) { WORD_AT(VREC+i*0x40u+0x28u)=0;WORD_AT(VREC+i*0x40u+0x2Cu)=1u;WORD_AT(VREC+i*0x40u+0x30u)=0xFFFFFFFFu; }
    originals=0;
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC);
    CHECK(originals==1u && seen_rec[0]==4u && seen_rec[1]==4u && seen_rec[2]==1u && seen_rec[3]==2u);
    CHECK(WORD_AT(BLOCK+0x58u)==0x07u && vh_lv.entry_logged);   /* the tank opens to players, tracked bit kept */
    /* its camera: a def copy whose preset index reaches a copy of preset 2 with the MapMaker distance/height */
    {
        uint32_t d=WORD_AT(BLOCK+0x50u),p;
        CHECK(d && d!=0x004E8658u && d==vh.tank_def);
        CHECK(!memcmp((char *)memory+d,(char *)memory+0x004E8658u,0xACu) && !memcmp((char *)memory+d+0xB0u,(char *)memory+0x004E8658u+0xB0u,0x28u));
        p=0x004DD420u+(WORD_AT(d+0xACu)<<5);
        CHECK(p==vh.tank_preset && near(FLOAT_AT(p),10.0f) && near(FLOAT_AT(p+4u),3.0f));
        CHECK(!memcmp((char *)memory+p+8u,(char *)memory+0x004DD460u+8u,0x18u));
        CHECK(WORD_AT(0x004E8658u+0xACu)==2u && near(FLOAT_AT(0x004DD460u),15.0f));     /* the story def and preset */
        CHECK(near(FLOAT_AT(GUN0+0x34u),-10.0f) && near(FLOAT_AT(GUN0+0x38u),20.0f));
        CHECK(near(FLOAT_AT(GUN1+0x34u),-45.0f) && near(FLOAT_AT(GUN1+0x38u),15.0f) && vh_lv.view_logged);
    }
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC+0x40u);
    CHECK(seen_rec[0]==0u && seen_rec[1]==1u && seen_rec[2]==0xFFFFFFFFu && seen_rec[3]==0x77u);
    CHECK(WORD_AT(BLOCK+0x58u)==0x17u);                         /* other vehicles keep their flags */
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC+0x80u);
    CHECK(seen_rec[0]==0u && seen_rec[1]==1u && seen_rec[2]==0u && seen_rec[3]==0xFFFFFFFFu);
    CHECK(WORD_AT(BLOCK+0x58u)==0x17u);
    CHECK(WORD_AT(BLOCK+0x50u)==0x004E8658u && near(FLOAT_AT(GUN0+0x38u),0.001f));   /* not a tank: stock */
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC+0xC0u);
    CHECK(seen_rec[0]==0u && seen_rec[1]==1u && seen_rec[2]==0xFFFFFFFFu && seen_rec[3]==0xFFFFFFFFu);
    WORD_AT(VREC+0x30u)=0xFFFFFFFFu;
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006CE68u,0x00570000u,VREC);   /* a story level's create */
    CHECK(seen_rec[2]==0xFFFFFFFFu && WORD_AT(BLOCK+0x58u)==0x17u);
    /* restart: no mount, no fill; the records keep their values and are set again */
    WORD_AT(VREC+0x28u)=0;WORD_AT(VREC+0x30u)=0xFFFFFFFFu;
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC);
    CHECK(seen_rec[0]==4u && seen_rec[2]==1u && WORD_AT(BLOCK+0x58u)==0x07u);
    CHECK(near(FLOAT_AT(VREC+0x10u),1.0f+2.375f));                /* the raised y is kept, not raised again */
    /* a record whose def or model changed is left alone */
    WORD_AT(VREC+0x24u)=5u;WORD_AT(VREC+0x30u)=0xFFFFFFFFu;
    CALL(pc_mapmaker_vehicles_create,fake_create,0x0006E949u,0x00570000u,VREC);
    CHECK(seen_rec[2]==0xFFFFFFFFu && WORD_AT(BLOCK+0x58u)==0x17u);
    WORD_AT(VREC+0x24u)=2u;
    /* the playtest overrides, read once: a valid value replaces the lift, a bad one is ignored */
    set_env("TSFP_MAPMAKER_TANK_LIFT","1.5");set_env("TSFP_MAPMAKER_TRUCK_LIFT","9");
    vh.lift_read[0]=vh.lift_read[1]=0;vh_lv.lift_logged[0]=vh_lv.lift_logged[1]=0;vh_lv.nrec=0;
    FILL(VREC,rec_at[0][0]);CHECK(near(FLOAT_AT(VREC+0x10u),2.5f));
    FILL(VREC+0xC0u,rec_at[1][1]);CHECK(near(FLOAT_AT(VREC+0xD0u),1.945f));
    set_env("TSFP_MAPMAKER_TANK_LIFT",NULL);set_env("TSFP_MAPMAKER_TRUCK_LIFT",NULL);vh.lift_read[0]=vh.lift_read[1]=0;
    /* every tank of the level is opened (the log line once) */
    vh_lv.entry_logged=0;FILL(VREC,rec_at[0][0]);
    {
        uint32_t sp=g_esp;
        g_esp-=12u;WORD_AT(g_esp)=0x0006E949u;WORD_AT(g_esp+4u)=0x00570000u;WORD_AT(g_esp+8u)=VREC;
        def_flags=0x17u;
        pc_mapmaker_vehicles_create(fake_create);
        g_esp=sp;
    }
    CHECK(WORD_AT(BLOCK+0x58u)==0x07u);
    /* the gun model not read: the truck with a gun stays the theme vehicle; the plain truck does not need it */
    resources(0xFF&~(1u<<7),0);
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);              /* new level */
    ntextured=0;CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u);   /* not deferred */
    FILL(VREC+0x80u,rec_at[1][0]);CHECK(g_eax==5u && WORD_AT(VREC+0x88u)==0x894u);
    FILL(VREC+0xC0u,rec_at[1][1]);CHECK(g_eax==0u && WORD_AT(VREC+0xC8u)==7u);
    /* the tank's AA gun not read: the tank stays */
    resources(0xFF&~(1u<<2),0);
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);
    FILL(VREC,rec_at[0][0]);CHECK(g_eax==5u && WORD_AT(VREC+8u)==0x894u);
    /* a model loaded already (resource +4) counts as read */
    WORD_AT(RESTAB+(10u+2u)*0x30u+4u)=1u;
    g_ebp=VREC;fill_record(VREC,0x894u,5u);
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x00241D39u,0x2Bu,0);CHECK(g_eax==2u && WORD_AT(VREC+8u)==0xBu);
    /* no resource row for a gib: no tank at the preload or the fill */
    resources(0xFF,0);HALF_AT(0x008CA000u+0x54Eu*2u)=-1;
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);
    g_esi=rec_at[0][0];fake_answer=0x894u;
    CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x894u);
    ntemplates=0;fake_answer=5u;
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x0023F637u,0x2Bu,0);CHECK(g_eax==5u && ntemplates==0u);
    g_ebp=VREC;fill_record(VREC,0x894u,5u);
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x00241D39u,0x2Bu,0);CHECK(g_eax==5u && WORD_AT(VREC+8u)==0x894u);
    resources(0xFF,0x10u);
    /* a map without variants mounts nothing and swaps nothing */
    {
        Piece plain[2];
        memcpy(plain,map,sizeof plain);
        plain[0].items[0].var=0;plain[1].items[0].var=0;plain[1].items[1].var=0;
        container(plain,2);nmounts=nmounted=0;
        CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
        g_esi=rec_at[0][0];fake_answer=0x894u;
        CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x894u);
        ntextured=0;CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u);
    }
    /* only trucks: only the truck pak */
    {
        Piece trucks[2];
        memcpy(trucks,map,sizeof trucks);
        trucks[0].items[0].var=0;
        container(trucks,2);nmounts=nmounted=0;
        CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);
        CHECK(nmounts==1u && !strcmp((const char *)((uintptr_t)mounts[0]+g_xbox_mem_offset),"pak/stream/lv4/prop3.pak"));
    }
    /* the mount table nearly full, a failed mount, the skip flag, the switch off, the editor: no mount */
    container(map,2);nmounts=nmounted=0;
    WORD_AT(0x006D7880u)=57u;CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    WORD_AT(0x006D7880u)=40u;mount_fails=1;
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==3u && nmounted==0u);
    g_esi=rec_at[0][0];fake_answer=0x894u;
    CALL(pc_mapmaker_vehicles_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x894u);
    g_ebp=VREC;fill_record(VREC,0x894u,5u);fake_answer=5u;
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x00241D39u,0x2Bu,0);CHECK(g_eax==5u);
    ntextured=0;CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u);
    mount_fails=0;nmounts=0;
    WORD_AT(0x006D7C4Cu)=1u;originals=0;
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u && originals==1u);
    WORD_AT(0x006D7C4Cu)=0;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_vehicles_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==3u);
    /* the camera override: valid values replace the MapMaker distance and height, others are ignored */
    set_env("TSFP_MAPMAKER_TANK_CAMERA","6,2");vh.tank_def=0;vh.cam_read=0;vh_lv.view_logged=0;
    WORD_AT(BLOCK+0x50u)=0x004E8658u;WORD_AT(BLOCK+0x164u)=2u;vh_tank_view(BLOCK);
    CHECK(near(FLOAT_AT(vh.tank_preset),6.0f) && near(FLOAT_AT(vh.tank_preset+4u),2.0f));
    set_env("TSFP_MAPMAKER_TANK_CAMERA","6");vh.tank_def=0;vh.cam_read=0;
    WORD_AT(BLOCK+0x50u)=0x004E8658u;vh_tank_view(BLOCK);
    CHECK(near(FLOAT_AT(vh.tank_preset),10.0f) && near(FLOAT_AT(vh.tank_preset+4u),3.0f));
    set_env("TSFP_MAPMAKER_TANK_CAMERA",NULL);
    /* the reticle is not requested without its pak */
    {
        unsigned before=nhud,keep=nmounted;
        WORD_AT(0x004D2D50u+0x125u*0x18u+0x10u)=0;nmounted=0;vh_lv.reticle_logged=0;
        vh_reticle();CHECK(nhud==before && vh_lv.reticle_logged);nmounted=keep;
    }
    /* outside play the hooks run the original only */
    WORD_AT(0x0079094Cu)=102u;ntextured=0;originals=0;
    CALL(pc_mapmaker_vehicles_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u && originals==1u);
    g_esi=rec_at[0][0];fake_answer=5u;g_ebp=VREC;fill_record(VREC,0x894u,5u);
    CALL(pc_mapmaker_vehicles_def,fake_answer_fn,0x00241D39u,0x2Bu,0);CHECK(g_eax==5u && WORD_AT(VREC+8u)==0x894u);
    WORD_AT(0x0079094Cu)=100u;
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    WORD_AT(0x007844A8u)=EDITOR;
    {   /* def 2 (0xD8 bytes, camera preset +0xAC = 2) and the five chase camera presets */
        unsigned k;
        for(k=0;k<0xD8u;k+=4u) WORD_AT(0x004E8658u+k)=0x1000u+k;
        WORD_AT(0x004E8658u+0xACu)=2u;
        for(k=0;k<5u*8u;k++) FLOAT_AT(0x004DD420u+k*4u)=(float)k+0.5f;
        FLOAT_AT(0x004DD460u)=15.0f;FLOAT_AT(0x004DD464u)=4.6f;
    }
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    compile_tests();
    load_tests();
    menu_tests();
    play_tests();
    vh_test_reset();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker vehicles ok\n");
    return 0;
}
