/* Asset-free contract for src/interface/pc_mapmaker_creatures.inc (story
 * creatures wave 2: the generic deployer driven by pc_mapmaker_creatures.def).
 * Fake guest memory holds a map container (map info with the map's own set,
 * the sets chunk with slots 11-20), the rows' registration tables, the anim
 * id map, the character table, the resource map/table and the pak mounts.
 * The fake originals pop their return address like the lifted callees and
 * every hook call checks esp. */
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
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
void sub_0003DFA0(void) { g_ecx=1; }
void sub_0005A130(void) { g_ecx=2; }
void sub_00062260(void) { g_ecx=3; }
void sub_000A5E40(void) { g_ecx=4; }
void sub_000631C0(void) { g_ecx=5; }

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define POOL 0x00400000u
#define RECS 0x00480000u
#define RESTAB 0x00500000u
#define REQ_READ 0x00510000u
#define REQ_NONE 0x00510100u

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

static uint32_t mounted_paths[8],requests[16];
static unsigned nmounted,nmounts,nrequests,originals;
static int mount_fails,models_read;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[4]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<4;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) {
        unsigned k;
        r=0xFFFFFFFFu;
        for(k=0;k<nmounted;k++) if(mounted_paths[k]==x[0]) r=k;
    } else if(f==sub_0005A130) {
        CHECK(n==1);
        nmounts++;
        if(!mount_fails) { if(nmounted<8u) mounted_paths[nmounted++]=x[0]; WORD_AT(0x006D7880u)++;r=1u; }
    } else if(f==sub_000A5E40) { CHECK(n==1);if(nrequests<16u) requests[nrequests]=x[0]; nrequests++; }
    else if(f==sub_000631C0) r=models_read?REQ_READ:REQ_NONE;
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_creatures.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value) setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    crt.read=0;
}
static uint32_t call_hook(void (*hook)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,int line)
{
    uint32_t sp=g_esp;
    g_esp-=12u;WORD_AT(g_esp)=ret;WORD_AT(g_esp+4u)=a0;WORD_AT(g_esp+8u)=a1;
    hook(orig);
    if(g_esp!=sp-8u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-(sp-8u)));failures++; }
    g_esp=sp;
    return g_eax;
}
#define CALL(h,o,ret,a0,a1) call_hook(h,o,ret,a0,a1,__LINE__)
static void fake_plain(void) { originals++;g_esp+=4u; }
/* sub_000DAC30: the character table models the call saw */
static uint32_t seen_model[2];
static void fake_create(void)
{
    uint32_t id=WORD_AT(g_esp+4u);
    originals++;
    seen_model[0]=WORD_AT(0x0053A3CCu+id*0x30u+4u);seen_model[1]=WORD_AT(0x0053A3CCu+id*0x30u+8u);
    g_eax=0x00777000u;
    g_esp+=4u;
}
/* sub_0015B830: what the record held when the original ran */
static uint32_t seen_class[2];
static void fake_ai_start(void)
{
    uint32_t a=WORD_AT(g_esp+4u),rec=WORD_AT(a+0xBF0u);
    originals++;
    seen_class[0]=WORD_AT(rec);seen_class[1]=WORD_AT(rec+4u);
    g_esp+=4u;
}

/* ---- the map ----------------------------------------------------------- */
static void chunk(uint32_t *c,uint32_t tag,uint32_t size)
{
    WORD_AT(*c)=tag;WORD_AT(*c+4u)=size;*c+=8u+size;
}
/* The map: set index setno, members ids[0..n-1] (slots 1-10 in the map info, 11+ in the sets chunk). */
static void map(uint32_t setno,const uint16_t *ids,unsigned n)
{
    uint32_t c=CONT+8u,info,sets;
    unsigned k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x2000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    info=c+8u;chunk(&c,0x00C77668u,0x40u);
    HALF_AT(info+0x14u)=(uint16_t)setno;BYTE_AT(info+0x16u)=(uint8_t)(n<10u?n:10u);
    for(k=0;k<n && k<10u;k++) { HALF_AT(info+0x18u+k*4u)=ids[k];BYTE_AT(info+0x1Bu+k*4u)=0xFFu; }
    if(n>10u) {
        sets=c+8u;chunk(&c,0x00C776F5u,0x30u);
        HALF_AT(sets)=1u;BYTE_AT(sets+2u)=10u;BYTE_AT(sets+3u)=(uint8_t)(n-10u);
        for(k=10u;k<n;k++) HALF_AT(sets+4u+(k-10u)*4u)=ids[k];
    }
    chunk(&c,0x00C7766Cu,0x50u);                        /* a Story AI chunk */
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static void anim_map_clear(void)
{
    memset((void *)((uintptr_t)0x007E03C0u+g_xbox_mem_offset),0xFF,0xC03u*2u);
}
/* register (map) every kept anim id of row k: as sub_001CA2F0 would after the mount */
static void anim_map_row(unsigned k,uint16_t slot)
{
    unsigned i;
    for(i=0;i<crt_kinds[k].n;i++)
        HALF_AT(0x007E03C0u+HALF_AT(crt_kinds[k].table+(crt_kinds[k].first+i)*0x18u+0x10u)*2u)=slot;
}
static void tables(void)
{
    unsigned k,i;
    for(k=0;k<CRT_KINDS;k++)
        for(i=0;i<crt_kinds[k].count;i++)
            HALF_AT(crt_kinds[k].table+i*0x18u+0x10u)=(uint16_t)(0x100u+k*0x100u+i);
}
/* character table: models for the carriers and the looks */
static void chars(void)
{
    static const uint32_t m[][3]={{0x2Fu,0x619u,0xFFFFFFFFu},{0x30u,0x619u,0xFFFFFFFFu},{0x4Du,0x607u,0xFFFFFFFFu},
                                  {0x17u,0x5C8u,0x5C9u},{0x59u,0x613u,0x614u}};
    unsigned k;
    for(k=0;k<sizeof m/sizeof m[0];k++) { WORD_AT(0x0053A3CCu+m[k][0]*0x30u+4u)=m[k][1];WORD_AT(0x0053A3CCu+m[k][0]*0x30u+8u)=m[k][2]; }
    /* resource rows for the look models: path and LOD path, not yet loaded */
    WORD_AT(0x00791D4Cu)=RESTAB;
    memset((void *)((uintptr_t)0x008CA000u+g_xbox_mem_offset),0xFF,0xA00u*2u);
    {
        static const uint32_t looks[]={0x5C8u,0x5C9u,0x613u,0x614u};
        for(k=0;k<4u;k++) {
            HALF_AT(0x008CA000u+looks[k]*2u)=(uint16_t)k;
            WORD_AT(RESTAB+k*0x30u+0x4u)=0;WORD_AT(RESTAB+k*0x30u+0x10u)=0x00520000u+k*0x40u;WORD_AT(RESTAB+k*0x30u+0x14u)=0;
        }
    }
    WORD_AT(REQ_READ+0x10u)=0x00530000u;WORD_AT(REQ_NONE+0x10u)=0;
}
static uint32_t actor(unsigned i,uint32_t id)
{
    uint32_t a=POOL+i*0xEA0u,rec=RECS+i*0x100u;
    HALF_AT(a+0x48u)=(uint16_t)id;WORD_AT(a+0xBF0u)=rec;
    WORD_AT(rec)=0x11000000u;WORD_AT(rec+4u)=0;WORD_AT(rec+8u)=0x1000u+i;
    return a;
}
static unsigned kind_of(uint32_t id) { return crt_kind_of(id); }

int main(void)
{
    uint32_t a;
    unsigned k;
    g_xbox_mem_offset=(uintptr_t)memory;
    g_esp=ESP0;
    tables();chars();anim_map_clear();
    {
        static const uint16_t set[]={0x0C,0x2F,0x44,0x4D,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x30};   /* 0x30 in slot 12 */
        map(0x11u,set,sizeof set/sizeof set[0]);
    }
    /* the table: three rows, as documented */
    CHECK(CRT_KINDS==3u);
    CHECK(kind_of(0x2Fu)==0 && kind_of(0x4Du)==1 && kind_of(0x30u)==2 && kind_of(0x17u)==CRT_KINDS && kind_of(0x59u)==CRT_KINDS);
    CHECK(crt_kinds[0].lo==0x50000000u && crt_kinds[0].hi==0 && crt_kinds[0].look==0x17u);
    CHECK(crt_kinds[1].lo==0x10000000u && crt_kinds[1].hi==0x4000u && crt_kinds[1].look==CRT_NO_LOOK);
    CHECK(crt_kinds[2].lo==0x10000000u && crt_kinds[2].hi==0x8000u && crt_kinds[2].look==0x59u);

    /* ---- switch off: every hook is the original, nothing else --------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u;
    originals=0;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CALL(pc_mapmaker_creatures_request,fake_plain,0x0015E7C5u,0,0);
    CHECK(originals==2 && nmounts==0 && nrequests==0);
    a=actor(0,0x2Fu);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u && WORD_AT(RECS)==0x11000000u);
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x2Fu,0);
    CHECK(seen_model[0]==0x619u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    set_env("TSFP_MAPMAKER_POOLS","0");                  /* limits need the pools */
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CHECK(nmounts==0);
    set_env("TSFP_MAPMAKER_POOLS",NULL);

    /* ---- other scenes and modes: stock --------------------------------- */
    WORD_AT(0x0079094Cu)=6u;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    a=actor(0,0x4Du);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(nmounts==0 && seen_class[0]==0x11000000u);
    WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=3u;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CHECK(nmounts==0);
    BYTE_AT(0x007DE455u)=10u;

    /* ---- a preset set: no creature --------------------------------------- */
    {
        static const uint16_t set[]={0x2F,0x4D,0x30};
        map(3u,set,3u);
        CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
        CHECK(nmounts==0);
        WORD_AT(0x00784014u)=0;                          /* unreadable: no mount either */
        CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
        CHECK(nmounts==0);
    }

    /* ---- a set with one creature: only its pak --------------------------- */
    {
        static const uint16_t set[]={0x0C,0x4D};
        map(0x11u,set,2u);
        WORD_AT(0x006D7880u)=20u;
        CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
        CHECK(nmounts==1 && nmounted==1 && mounted_paths[0]==0x0049DA84u);
        nmounts=0;nmounted=0;
    }

    /* ---- the map's own set ----------------------------------------------- */
    {
        static const uint16_t set[]={0x0C,0x2F,0x44,0x4D,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x30};
        map(0x11u,set,sizeof set/sizeof set[0]);
    }
    WORD_AT(0x006D7880u)=60u;                            /* 4 of 64 free: refused */
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CHECK(nmounts==0);
    WORD_AT(0x006D7880u)=20u;
    mount_fails=1;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CHECK(nmounts==3 && nmounted==0);
    mount_fails=0;nmounts=0;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CHECK(nmounts==3 && nmounted==3 && WORD_AT(0x006D7880u)==23u);
    CHECK(mounted_paths[0]==0x0049D968u && mounted_paths[1]==0x0049DA84u && mounted_paths[2]==0x0049DB58u);
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);   /* again: nothing more */
    CHECK(nmounts==3);

    /* ---- request: the look's models when the look is not in the set ----- */
    nrequests=0;
    CALL(pc_mapmaker_creatures_request,fake_plain,0x0015E7C5u,0,0);
    CHECK(nrequests==2 && requests[0]==0x613u && requests[1]==0x614u);   /* 0x17 is a member: its preload asks */

    /* ---- create: the look's models for this call only --------------------- */
    models_read=0;
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x30u,0);
    CHECK(seen_model[0]==0x619u && seen_model[1]==0xFFFFFFFFu);     /* not read: its own */
    models_read=1;
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x30u,0);
    CHECK(seen_model[0]==0x613u && seen_model[1]==0x614u && g_eax==0x00777000u);
    CHECK(WORD_AT(0x0053A3CCu+0x30u*0x30u+4u)==0x619u && WORD_AT(0x0053A3CCu+0x30u*0x30u+8u)==0xFFFFFFFFu);   /* put back */
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x2Fu,0);
    CHECK(seen_model[0]==0x5C8u && seen_model[1]==0x5C9u && WORD_AT(0x0053A3CCu+0x2Fu*0x30u+4u)==0x619u);
    HALF_AT(0x008CA000u)=4u;WORD_AT(RESTAB+4u*0x30u+0x4u)=1u;   /* a ready model 0 (character 0xFF has 0/0) */
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x4Du,0);   /* own look */
    CHECK(seen_model[0]==0x607u);
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x17u,0);   /* not a creature */
    CHECK(seen_model[0]==0x5C8u);
    WORD_AT(RESTAB+0x4u)=1u;models_read=0;                  /* loaded before: no request data needed */
    WORD_AT(RESTAB+0x30u+0x4u)=1u;
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x2Fu,0);
    CHECK(seen_model[0]==0x5C8u);

    /* ---- ai_start: the class once the kept anims resolve ---------------- */
    a=actor(0,0x2Fu);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);      /* anims not registered */
    anim_map_row(0,0x40u);
    HALF_AT(0x007E03C0u+HALF_AT(crt_kinds[0].table+(crt_kinds[0].first+3u)*0x18u+0x10u)*2u)=0xFFFFu;   /* one missing */
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u);
    anim_map_row(0,0x40u);anim_map_row(1,0x41u);anim_map_row(2,0x42u);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x50000000u && seen_class[1]==0);
    for(k=1;k<4u;k++) {                                       /* several per map */
        a=actor(k,0x2Fu);
        CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
        CHECK(seen_class[0]==0x50000000u && WORD_AT(RECS+k*0x100u)==0x50000000u);
    }
    a=actor(4,0x4Du);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x10000000u && seen_class[1]==0x4000u);
    a=actor(5,0x30u);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x10000000u && seen_class[1]==0x8000u);
    a=actor(6,0x59u);                                         /* the Arcade Freak: stock */
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u && seen_class[1]==0);
    a=actor(7,0x17u);                                         /* an Arcade zombie: stock */
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u);
    WORD_AT(POOL+8u*0xEA0u+0xBF0u)=0;HALF_AT(POOL+8u*0xEA0u+0x48u)=0x2Fu;     /* no record */
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,POOL+8u*0xEA0u,0);
    /* pak not mounted (another level's mount table): bot */
    nmounted=0;
    a=actor(0,0x4Du);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u);
    nmounted=3;

    /* ---- opt out --------------------------------------------------------- */
    set_env("TSFP_MAPMAKER_CREATURES","0");
    a=actor(0,0x4Du);
    CALL(pc_mapmaker_creatures_ai_start,fake_ai_start,0x0015BA00u,a,0);
    CHECK(seen_class[0]==0x11000000u);
    nmounts=0;nrequests=0;nmounted=0;
    CALL(pc_mapmaker_creatures_anims,fake_plain,0x0005C506u,0,0);
    CALL(pc_mapmaker_creatures_request,fake_plain,0x0015E7C5u,0,0);
    CHECK(nmounts==0 && nrequests==0);
    models_read=1;
    CALL(pc_mapmaker_creatures_create,fake_create,0x000D7800u,0x30u,0);
    CHECK(seen_model[0]==0x619u);
    set_env("TSFP_MAPMAKER_CREATURES",NULL);

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("ok\n");
    return 0;
}
