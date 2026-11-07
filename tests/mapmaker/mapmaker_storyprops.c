/* Asset-free contract for src/interface/pc_mapmaker_storyprops.inc (story
 * assets wave 1: the mission 1 power cell as a drum variant, the prison
 * fixed gun as a fixed gun variant). Fake guest memory holds a map container
 * (map info, piece chunk with drum / fixed gun / pickup items), the editor's
 * piece list and nodes, the scene-100 resource table and pak mounts; the
 * fake originals pop their return address like the lifted callees and every
 * hook call checks esp. */
#define TSFP_MAPMAKER_STORYPROPS_TEST 1
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
STUB(sub_000622D0,16) STUB(sub_00062260,17) STUB(sub_000631C0,18) STUB(sub_00096220,19)

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define EDITOR 0x00200000u
#define PIECES 0x00280000u
#define NODES 0x00290000u
#define MENU 0x00301000u
#define HEAP 0x00400000u
#define RESTAB 0x00500000u
#define REQS 0x00510000u
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
static uint32_t options[16][2],binds[8],mounts[8],textured[8];
static unsigned noptions,nbinds,nchoices,nends,ndone,nframe,nmounts,ntextured,originals;
static uint32_t mounted_paths[8];
static unsigned nmounted;
static int mount_fails;
static uint32_t pak_tex_crc[8],pak_tex_n;          /* what a pak mounted next lists */
static uint32_t crc_name(const char *s)
{
    uint32_t c=0xFFFFFFFFu;
    int k;
    for(;*s;s++) { c^=(uint8_t)*s; for(k=0;k<8;k++) c=c&1u?(c>>1)^0xEDB88320u:c>>1; }
    return ~c;
}
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
    else if(f==sub_000631C0) r=x[0]==0x00520000u?REQS:x[0]==0x00520100u?REQS+0x100u:0u;
    else if(f==sub_00096220) { if(ntextured<8u) textured[ntextured]=x[0]; ntextured++; }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_storyprops.inc"

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
            WORD_AT(n+0x30u)=ps[i].items[j].var?0x53500001u:junk;
            if(nprev) WORD_AT(nprev+0x14u)=n; else WORD_AT(pc+0x3Cu)=n;
            nprev=n;node_at[i][j]=n;
        }
    }
}
static const Piece map[2]={
    {3,0,5,1,3,{{0x49,0,4,1},{0x49,0,2,0},{0x11,0,3,0}}},     /* power cell, drum, pickup */
    {4,0,5,1,3,{{0x33,0x100,1,1},{0x32,0,2,0},{0x35,0,3,1}}}, /* prison gun, MapMaker gun, floor gun (not ours) */
};

static void compile_tests(void)
{
    container(map,2);editor(map,2,0x12345678u);
    BYTE_AT(rec_at[0][0]+0xCu)=0;BYTE_AT(rec_at[1][0]+0xCu)=0;
    BYTE_AT(rec_at[0][2]+0xCu)=0x55u;                   /* the pickup's byte is weaponset's */
    BYTE_AT(rec_at[0][1]+0xCu)=0x77u;                   /* a stale byte on a stock drum */
    BYTE_AT(rec_at[1][2]+0xCu)=0x5Au;                   /* 0x35's +0xC is its own (0x00301147) */
    originals=0;
    CALL(pc_mapmaker_storyprops_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(originals==1u);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==1u && BYTE_AT(rec_at[0][1]+0xCu)==0u && BYTE_AT(rec_at[1][0]+0xCu)==1u);
    CHECK(BYTE_AT(rec_at[1][1]+0xCu)==0u && BYTE_AT(rec_at[0][2]+0xCu)==0x55u && BYTE_AT(rec_at[1][2]+0xCu)==0x5Au);
    /* an untagged 1 is stock */
    WORD_AT(node_at[1][0]+0x30u)=1u;
    CALL(pc_mapmaker_storyprops_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[1][0]+0xCu)==0u);
    WORD_AT(node_at[1][0]+0x30u)=0x53500001u;
    /* a stale tag value stays stock */
    WORD_AT(node_at[0][0]+0x30u)=0x53500002u;
    CALL(pc_mapmaker_storyprops_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==0u);
    WORD_AT(node_at[0][0]+0x30u)=0x53500001u;
    /* switch off: the original only */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    BYTE_AT(rec_at[0][0]+0xCu)=0;
    CALL(pc_mapmaker_storyprops_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    CALL(pc_mapmaker_storyprops_compile,fake_plain,0x0032C6FFu,CONT,0);
    CHECK(BYTE_AT(rec_at[0][0]+0xCu)==1u);
}
static void load_tests(void)
{
    container(map,2);editor(map,2,0xDEADBEEFu);
    WORD_AT(node_at[0][0]+0x30u)=0x53500001u;              /* garbage everywhere first */
    WORD_AT(node_at[1][0]+0x30u)=0x11u;
    WORD_AT(node_at[0][1]+0x30u)=0x53500001u;
    WORD_AT(node_at[0][2]+0x30u)=0x57530003u;              /* weaponset's pickup slot: untouched */
    WORD_AT(node_at[1][2]+0x30u)=0x1234u;                  /* 0x35: not ours, untouched */
    BYTE_AT(rec_at[0][1]+0xCu)=2u;                          /* an unknown byte loads as stock */
    CALL(pc_mapmaker_storyprops_load,fake_plain,0x00303076u,CONT,0);
    CHECK(WORD_AT(node_at[0][0]+0x30u)==0x53500001u && WORD_AT(node_at[1][0]+0x30u)==0x53500001u);
    CHECK(WORD_AT(node_at[0][1]+0x30u)==0x53500000u && WORD_AT(node_at[1][1]+0x30u)==0x53500000u);
    CHECK(WORD_AT(node_at[0][2]+0x30u)==0x57530003u && WORD_AT(node_at[1][2]+0x30u)==0x1234u);
    /* a node that moved off its record (other cell) keeps stock */
    BYTE_AT(node_at[1][0]+0xCu)=9u;
    CALL(pc_mapmaker_storyprops_load,fake_plain,0x00303076u,CONT,0);
    CHECK(WORD_AT(node_at[1][0]+0x30u)==0x53500000u);
    BYTE_AT(node_at[1][0]+0xCu)=1u;
}
static void menu_tests(void)
{
    uint32_t cat,d;
    container(map,2);editor(map,2,0);
    /* the category entry: a copy with our descriptor for a selected drum */
    WORD_AT(0x00524BE0u+26u*24u)=0x9D6u;WORD_AT(0x00524BE0u+26u*24u+4u)=0x14u;WORD_AT(0x00524BE0u+26u*24u+8u)=0;
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][1];
    fake_answer=0x00524BE0u+26u*24u;
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051B5u,0x49u,0);
    cat=g_eax;
    CHECK(cat!=fake_answer && cat==sp.cat && WORD_AT(cat)==0x9D6u && WORD_AT(cat+4u)==0x14u && (d=WORD_AT(cat+8u))==sp.desc);
    CHECK(WORD_AT(d)==0x00319300u && WORD_AT(d+4u)==0x003192E0u && !WORD_AT(d+8u) && !WORD_AT(d+12u));
    CHECK(WORD_AT(0x00524BE0u+26u*24u+8u)==0u);            /* the stock table is untouched */
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051D7u,0x49u,0);CHECK(g_eax==sp.cat);
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x00305000u,0x49u,0);CHECK(g_eax==fake_answer);   /* other caller */
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051B5u,0x4Au,0);CHECK(g_eax==fake_answer);   /* other prop */
    WORD_AT(EDITOR+0x2F1Cu)=node_at[1][0];
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051B5u,0x49u,0);CHECK(g_eax==fake_answer);   /* not the selection */
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][1];
    WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051B5u,0x49u,0);CHECK(g_eax==fake_answer);
    WORD_AT(0x0079094Cu)=102u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_storyprops_category,fake_answer_fn,0x003051B5u,0x49u,0);CHECK(g_eax==fake_answer);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* the drum menu: frame, a Type choice bound to the node, Done; the original never runs */
    WORD_AT(MENU+4u)=sp.desc;WORD_AT(node_at[0][1]+0x30u)=0xABCDu;
    noptions=nbinds=nchoices=nends=ndone=nframe=originals=0;
    CALL(pc_mapmaker_storyprops_build,fake_plain,0x00078F74u,MENU,0);
    CHECK(originals==0u && nframe==1u && nchoices==1u && nends==1u && ndone==1u && WORD_AT(MENU+0x218u)==1u);
    CHECK(noptions==2u && options[0][1]==0x53500000u && options[1][1]==0x53500001u);
    CHECK(!strcmp((const char *)((uintptr_t)options[1][0]+g_xbox_mem_offset),"Power cell (big explosion)"));
    CHECK(nbinds==1u && binds[0]==node_at[0][1]+0x30u && WORD_AT(node_at[0][1]+0x30u)==0x53500000u);
    WORD_AT(MENU+4u)=0x0052DECCu;                           /* the pickup options: stock */
    originals=0;CALL(pc_mapmaker_storyprops_build,fake_plain,0x00078F74u,MENU,0);CHECK(originals==1u);
    /* the fixed gun options: a Model choice after the weapon choice */
    WORD_AT(EDITOR+0x2F1Cu)=node_at[1][1];WORD_AT(node_at[1][1]+0x30u)=0x53500001u;
    noptions=nbinds=nchoices=nends=originals=0;
    CALL(pc_mapmaker_storyprops_turret_options,fake_plain,0x00318A8Cu,0,0);
    CHECK(originals==1u && nchoices==1u && nends==1u && noptions==2u && options[0][1]==0x53500000u && options[1][1]==0x53500001u);
    CHECK(nbinds==1u && binds[0]==node_at[1][1]+0x30u && WORD_AT(node_at[1][1]+0x30u)==0x53500001u);
    CHECK(!strcmp((const char *)((uintptr_t)options[1][0]+g_xbox_mem_offset),"Prison gun (mission 1)"));
    noptions=originals=0;
    CALL(pc_mapmaker_storyprops_turret_options,fake_plain,0x00318973u,0,0);       /* the first item's end */
    CHECK(originals==1u && noptions==0u);
    WORD_AT(EDITOR+0x2F1Cu)=node_at[0][1];                 /* a drum: nothing */
    CALL(pc_mapmaker_storyprops_turret_options,fake_plain,0x00318A8Cu,0,0);
    CHECK(originals==2u && noptions==0u);
}

/* scene-100 resources: 0x53D row 5 (path 0x00520000), 0xE6 row 6 (path 0x00520100) */
static void resources(int cell_read,int gun_read,uint8_t deferred)
{
    WORD_AT(0x00791D4Cu)=RESTAB;
    HALF_AT(0x008CA000u+0x53Du*2u)=5;HALF_AT(0x008CA000u+0xE6u*2u)=6;
    WORD_AT(RESTAB+5u*0x30u+4u)=0;WORD_AT(RESTAB+5u*0x30u+0x10u)=0x00520000u;
    WORD_AT(RESTAB+6u*0x30u+4u)=0;WORD_AT(RESTAB+6u*0x30u+0x10u)=0x00520100u;
    WORD_AT(REQS+0x10u)=cell_read?0x00530000u:0u;BYTE_AT(REQS+2u)=deferred;
    WORD_AT(REQS+0x110u)=gun_read?0x00531000u:0u;BYTE_AT(REQS+0x102u)=deferred;
    /* the cell's file lists 3 textures (rows {id, id, 0, 0}, -1 ends), the gun's 1 */
    WORD_AT(0x00530000u)=0x10u;
    WORD_AT(0x00530010u)=WORD_AT(0x00530014u)=0x2A6ABDu;WORD_AT(0x00530020u)=WORD_AT(0x00530024u)=0x450CF9u;
    WORD_AT(0x00530030u)=WORD_AT(0x00530034u)=0x123456u;WORD_AT(0x00530040u)=0xFFFFFFFFu;
    WORD_AT(0x00531000u)=0x20u;WORD_AT(0x00531020u)=WORD_AT(0x00531024u)=0x91B0F7u;WORD_AT(0x00531030u)=0xFFFFFFFFu;
}
static void play_tests(void)
{
    WORD_AT(0x0079094Cu)=100u;
    container(map,2);
    resources(1,1,0x10u);
    /* mount: the model paks, then their memory paks (the textures), kept open; once each */
    WORD_AT(0x006D7880u)=40u;nmounts=nmounted=0;originals=0;
    pak_tex_crc[0]=crc_name("textures/2a6abd.xbt");pak_tex_crc[1]=crc_name("textures/450cf9.xbt");
    pak_tex_crc[2]=crc_name("textures/91b0f7.xbt");pak_tex_n=3u;
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);
    CHECK(originals==1u && nmounts==4u && sp_lv.want[0]==1u && sp_lv.want[1]==1u);
    CHECK(!strcmp((const char *)((uintptr_t)mounts[0]+g_xbox_mem_offset),"pak/challenge/l_64_ch.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounts[1]+g_xbox_mem_offset),"pak/story/l_26_st.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounts[2]+g_xbox_mem_offset),"pak/challengemem/l_64_chm.pak"));
    CHECK(!strcmp((const char *)((uintptr_t)mounts[3]+g_xbox_mem_offset),"pak/storymem/l_26_stm.pak"));
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==4u);
    /* the texture queue's load: both deferred models registered the load pass's way, once */
    ntextured=0;originals=0;
    CALL(pc_mapmaker_storyprops_textures,fake_plain,0x0003D5EAu,0,0);
    CHECK(originals==1u && ntextured==2u && textured[0]==0x00520000u && textured[1]==0x00520100u);
    CHECK(!(BYTE_AT(REQS+2u)&0x10u) && !(BYTE_AT(REQS+0x102u)&0x10u));
    BYTE_AT(REQS+2u)=0x10u;                                 /* once per level, even if marked again */
    CALL(pc_mapmaker_storyprops_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==2u && originals==2u);
    /* preload: the variant's model is requested */
    g_esi=rec_at[0][0];fake_answer=0x8B1u;
    CALL(pc_mapmaker_storyprops_preload_model,fake_answer_fn,0x0023F5DBu,0x00524F58u+0x49u*0xC4u,0);CHECK(g_eax==0x53Du);
    g_esi=rec_at[0][1];CALL(pc_mapmaker_storyprops_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x8B1u);
    g_esi=rec_at[1][0];fake_answer=0x8A0u;
    CALL(pc_mapmaker_storyprops_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0xE6u);
    CALL(pc_mapmaker_storyprops_preload_model,fake_answer_fn,0x0023F5D0u,0,0);CHECK(g_eax==0x8A0u);   /* other caller */
    /* fill: power cell model, raised by its half height (record +0x14 = ebp + 0x14) */
    g_esi=rec_at[0][0];fake_answer=0x8B1u;g_ebp=0x00560000u;FLOAT_AT(g_ebp+0x14u)=1.0f;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x53Du);
    CHECK(FLOAT_AT(g_ebp+0x14u)>1.736f && FLOAT_AT(g_ebp+0x14u)<1.738f);
    FLOAT_AT(g_ebp+0x14u)=1.0f;g_esi=rec_at[0][1];          /* a plain drum keeps its height */
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u && FLOAT_AT(g_ebp+0x14u)==1.0f);
    g_esi=rec_at[0][0];
    g_esi=rec_at[0][1];CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u);
    g_esi=rec_at[0][0];CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x49u,0);CHECK(g_eax==0x8B1u);  /* wrong case */
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002F0000u,0x49u,0);CHECK(g_eax==0x8B1u);           /* editor caller */
    g_esi=rec_at[1][0];fake_answer=0x8A0u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002F0000u,0x33u,0);CHECK(g_eax==0x8A0u);
    /* turret: model then gun type 7 for that item only */
    g_esi=rec_at[1][0];fake_answer=0x8A0u;
    FLOAT_AT(g_ebp+0x14u)=1.0f;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x33u,0);CHECK(g_eax==0xE6u && FLOAT_AT(g_ebp+0x14u)==1.0f);
    fake_answer=9u;
    CALL(pc_mapmaker_storyprops_gun_type,fake_answer_fn,0x00241AA6u,0,0x33u);CHECK(g_eax==7u);
    CALL(pc_mapmaker_storyprops_gun_type,fake_answer_fn,0x00241AA6u,0,0x33u);CHECK(g_eax==9u);   /* used once */
    fake_answer=0x8A0u;CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x33u,0);CHECK(g_eax==0xE6u);
    g_esi=rec_at[1][1];fake_answer=9u;                     /* another item's gun type: stock */
    CALL(pc_mapmaker_storyprops_gun_type,fake_answer_fn,0x00241AA6u,0,0x32u);CHECK(g_eax==9u);
    g_esi=rec_at[1][2];fake_answer=0x8FFu;                 /* 0x35 carries its own +0xC: never ours */
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x35u,0);CHECK(g_eax==0x8FFu);
    g_esi=rec_at[1][0];
    g_esi=rec_at[1][1];fake_answer=0x8A0u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x32u,0);CHECK(g_eax==0x8A0u);
    fake_answer=8u;CALL(pc_mapmaker_storyprops_gun_type,fake_answer_fn,0x00241AA6u,0,0x32u);CHECK(g_eax==8u);
    g_esi=rec_at[1][0];fake_answer=0x8A0u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x33u,0);
    fake_answer=9u;CALL(pc_mapmaker_storyprops_gun_type,fake_answer_fn,0x00241A14u,0,0x33u);CHECK(g_eax==9u);   /* 0x35's call */
    /* a model the load pass did not read stays stock, and is not registered */
    resources(0,1,0);
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);           /* new level */
    ntextured=0;CALL(pc_mapmaker_storyprops_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u);   /* gun: not deferred */
    g_esi=rec_at[0][0];fake_answer=0x8B1u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u);
    g_esi=rec_at[1][0];fake_answer=0x8A0u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x002419C4u,0x33u,0);CHECK(g_eax==0xE6u);   /* load pass registered */
    WORD_AT(RESTAB+5u*0x30u+4u)=1u;                         /* resource loaded */
    g_esi=rec_at[0][0];fake_answer=0x8B1u;
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x53Du);
    /* no resource row: stock */
    HALF_AT(0x008CA000u+0x53Du*2u)=-1;
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);
    CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u);
    CALL(pc_mapmaker_storyprops_preload_model,fake_answer_fn,0x0023F5DBu,0,0);CHECK(g_eax==0x8B1u);
    resources(1,1,0x10u);
    /* a map without variants mounts nothing and swaps nothing */
    {
        Piece plain[2];
        memcpy(plain,map,sizeof plain);
        plain[0].items[0].var=0;plain[1].items[0].var=0;
        container(plain,2);nmounts=nmounted=0;
        CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
        g_esi=rec_at[0][0];
        CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u);
    }
    /* the mount table nearly full, a failed mount, the switch off, the editor: no mount */
    container(map,2);nmounts=nmounted=0;
    WORD_AT(0x006D7880u)=57u;CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    WORD_AT(0x006D7880u)=40u;mount_fails=1;                 /* the models fail: no texture paks either */
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==2u && nmounted==0u);
    g_esi=rec_at[0][0];CALL(pc_mapmaker_storyprops_model,fake_answer_fn,0x00241787u,0x49u,0);CHECK(g_eax==0x8B1u);
    ntextured=0;CALL(pc_mapmaker_storyprops_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u);   /* no model pak */
    mount_fails=0;nmounts=0;
    WORD_AT(0x006D7C4Cu)=1u;originals=0;
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u && originals==1u);
    WORD_AT(0x006D7C4Cu)=0;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==0u);
    WORD_AT(0x0079094Cu)=100u;
    CALL(pc_mapmaker_storyprops_mount,fake_plain,0x001CDB8Au,0,0);CHECK(nmounts==4u);
    /* the texture hook outside play runs the original only */
    WORD_AT(0x0079094Cu)=102u;ntextured=0;originals=0;
    CALL(pc_mapmaker_storyprops_textures,fake_plain,0x0003D5EAu,0,0);CHECK(ntextured==0u && originals==1u);
    WORD_AT(0x0079094Cu)=100u;
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    WORD_AT(0x007844A8u)=EDITOR;
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    compile_tests();
    load_tests();
    menu_tests();
    play_tests();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker storyprops ok\n");
    return 0;
}
