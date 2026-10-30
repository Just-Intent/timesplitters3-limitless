/* Asset-free contract for src/pc_mapmaker_actors.inc (MapMaker actors
 * area): 64 characters in a local MapMaker story map. The spawner's alive
 * gate, the AI slot guard (every scene), the 64-slot actor pool (scene 100),
 * the level-start AI clamp, the live-list append guards, the AI target and
 * enemy candidate arrays and the per-opponent kill words. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x1000000/4], g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(int16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters { uint32_t a,b,c,d,si,di,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->b=g_ebx;r->c=g_ecx;r->d=g_edx;r->si=g_esi;r->di=g_edi;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ebx=r->b;g_ecx=r->c;g_edx=r->d;g_esi=r->si;g_edi=r->di;g_esp=r->sp; }

static volatile int stub_id;
void sub_0003DFA0(void) { stub_id=1; }
void sub_001CD040(void) { stub_id=2; }
void sub_001638F0(void) { stub_id=3; }
void sub_003533F0(void) { stub_id=4; }

/* Fakes for the guest calls. */
static uint32_t heap_next=0x00A00000u, alloc_size, alloc_heap, flags_1cd040, network_call;
static unsigned allocs;
static uint32_t pick_args[4], pick_list[80], pick_esp;
static unsigned picks;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t args[4]={0},p=0;
    GuestRegisters r;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<4;i++) args[i]=va_arg(a,uint32_t);
    va_end(a);
    save_registers(&r);
    if(f==sub_0003DFA0) {
        p=heap_next;heap_next+=(args[0]+15u)&~15u;
        memset((char *)memory+p,0xCD,args[0]);      /* dirty: the hook must clear it */
        alloc_size=args[0];alloc_heap=args[1];allocs++;
    }
    else if(f==sub_001CD040) p=flags_1cd040;
    else if(f==sub_003533F0) p=network_call;
    else if(f==sub_001638F0) {
        unsigned k;
        memcpy(pick_args,args,sizeof pick_args);
        for(k=0;k<args[1] && k<80u;k++) pick_list[k]=WORD_AT(args[2]+k*4u);
        pick_esp=g_esp;picks++;
        p=0x00C0FFEEu;
    }
    restore_registers(&r);
    return p;
}

#include "pc_mapmaker_actors.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

#define SCENE 0x0079094Cu
#define NETWORK 0x0076B108u
#define COUNT 0x0074C37Cu
#define HUMANS 0x00790950u
#define USED 0x007A2958u
#define POOLPTR 0x007356D8u
#define MODE 0x007DE455u
#define SUBMODE 0x007DE456u
#define LEVEL 0x0074C36Cu
#define POOL 0x00400000u
#define STACK 0x00700000u
#define LIST MAPMAKER_ACTORS_LIST
#define ENTRY(i) (POOL+(uint32_t)(i)*0xEA0u)

static void set_env(const char *value)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_ALIVE",value?value:"");
#else
    if(value) setenv("TSFP_MAPMAKER_ALIVE",value,1); else unsetenv("TSFP_MAPMAKER_ALIVE");
#endif
    mapmaker_actors.initialized=0;
}

/* Fake originals pop their return address, as the lifted callee does. */
static int originals;
static uint32_t count_seen;
static void fake_check(void) { originals++; g_eax=1u; g_esp+=4u; }
static void fake_slot(void) { originals++; g_eax=0x1234u; g_esp+=4u; }
static void fake_plain(void) { originals++; count_seen=WORD_AT(COUNT); g_eax=0x5555u; g_esp+=4u; }
static void fake_pool(void) { originals++; WORD_AT(POOLPTR)=0x00600000u; g_esp+=4u; }
/* sub_000644D0: the stock add at 0x000645CC in round states 6 and 7. */
static void fake_kill(void)
{
    uint32_t killer=WORD_AT(g_esp+4),victim=WORD_AT(g_esp+8),points=WORD_AT(g_esp+12);
    originals++;
    if(killer && (WORD_AT(0x007DE30Cu)==6u || WORD_AT(0x007DE30Cu)==7u))
        HALF_AT(killer+0xA10u+victim*2u)=(int16_t)(HALF_AT(killer+0xA10u+victim*2u)+(int16_t)points);
    g_esp+=4u;
}
#define HOOK(h,f) do { uint32_t hook_esp_=g_esp; h(f); CHECK(g_esp==hook_esp_+4u); g_esp=hook_esp_; } while(0)
#define SIDE(k,v) (MAPMAKER_ACTORS_SIDE+((k)*32u+((v)-32u))*2u)
/* sub_0027BA40 model: candidates (another team, not +0x24&8, not +0x200)
 * go to a 32-entry array (overflow flagged); the lowest score +0x204
 * wins, first in list order on ties; returns the object [entry]. */
static unsigned enemy_calls,enemy_overflow,enemy_count_ok;
static uint32_t enemy_expect;
static void fake_enemies(void)
{
    uint32_t self=WORD_AT(WORD_AT(g_esp+4)+0x7Cu),team=WORD_AT(self+0x84u),count=WORD_AT(0x0074C37Cu);
    uint32_t i,e,n=0,best=0xFFFFFFFFu,winner=0;
    enemy_calls++;
    if(count!=enemy_expect) enemy_count_ok=0;
    for(i=0;i<count;i++) {
        e=WORD_AT(MAPMAKER_ACTORS_LIST+i*4u);
        if((BYTE_AT(e+0x24u)&8u) || WORD_AT(e+0x84u)==team || WORD_AT(e+0x200u)) continue;
        if(++n>32u) enemy_overflow=1;
        if(WORD_AT(e+0x204u)<best) { best=WORD_AT(e+0x204u);winner=WORD_AT(e); }
    }
    g_eax=winner;g_esp+=4u;
}
static uint32_t create_entity,create_slot;
static void fake_create(void)
{
    originals++;
    if(create_entity) WORD_AT(ENTRY(create_slot))=create_entity;
    g_eax=create_entity;g_esp+=4u;
}
static void call_setup(uint32_t a1,uint32_t a2,uint32_t a3)
{
    g_esp=STACK;WORD_AT(STACK)=0x00123456u;WORD_AT(STACK+4)=a1;WORD_AT(STACK+8)=a2;WORD_AT(STACK+12)=a3;
    originals=0;g_eax=0;
}

/* The two stock comparisons of sub_00164010 with the gate. */
static int spawner_allows(int flagged)
{
    int32_t gate=(int32_t)WORD_AT(MAPMAKER_ACTORS_GATE);
    if(!(gate<0x14) && !flagged) return 0;
    if(WORD_AT(SCENE)==100u && !(gate<=0x14)) return 0;
    return 1;
}
static int gate_allows(uint32_t count,int flagged)
{
    call_setup(0,0,0);
    WORD_AT(COUNT)=count;
    HOOK(pc_mapmaker_actors_spawn_gate,fake_check);
    CHECK(originals==1 && g_eax==1u);
    return spawner_allows(flagged);
}
static int last_allowed(int flagged)
{
    int c,last=-1;
    for(c=0;c<=80;c++) if(gate_allows((uint32_t)c,flagged)) last=c;
    return last;
}

static void mode(int story) { BYTE_AT(MODE)=story?10u:5u; }
static void pool_record(uint32_t scene)
{
    uint32_t saved=WORD_AT(SCENE);
    WORD_AT(SCENE)=scene;WORD_AT(0x007DE304u)=1u;
    call_setup(0,0,0);
    if(scene==100u) HOOK(pc_mapmaker_actors_pool,fake_pool);
    else { WORD_AT(POOLPTR)=POOL;HOOK(pc_mapmaker_actors_pool,fake_plain); }
    WORD_AT(SCENE)=saved;
}
static void actors(uint32_t humans,uint32_t used,int story)
{
    WORD_AT(POOLPTR)=mapmaker_actors.pool_ptr;WORD_AT(HUMANS)=humans;WORD_AT(USED)=used;
    mode(story);
    memset((char *)memory+WORD_AT(POOLPTR),0,64u*0xEA0u+0x100u);
}
static void occupy(uint32_t slot,int story)
{
    WORD_AT(WORD_AT(POOLPTR)+slot*0xEA0u+(story?0x10u:0x18u))=0xA0000000u+slot;
}
static void fill(uint32_t humans,uint32_t used,int story)
{
    uint32_t i;
    actors(humans,used,story);
    for(i=humans;i<humans+used;i++) occupy(i,story);
}
static int slot_call(void)
{
    call_setup(0,0,0);
    HOOK(pc_mapmaker_actors_slot,fake_slot);
    if(originals) { CHECK(g_eax==0x1234u); return 1; }
    CHECK(g_eax==0xFFFFFFFFu);
    return 0;
}

/* Live characters: entry i = pool slot i; record at 0x00680000 + i*0x100. */
static void live(uint32_t count)
{
    uint32_t i;
    WORD_AT(POOLPTR)=POOL;
    memset((char *)memory+POOL,0,64u*0xEA0u);
    for(i=0;i<64u;i++) {
        WORD_AT(LIST+i*4u)=i<count?ENTRY(i):0u;
        WORD_AT(ENTRY(i)+8u)=i;
        WORD_AT(ENTRY(i)+0xBF0u)=0x00680000u+i*0x100u;
        memset((char *)memory+0x00680000u+i*0x100u,0,0x100u);
    }
    WORD_AT(COUNT)=count;
}

static int test_pick(void (*hook)(void (*)(void)),int kind,uint32_t count,uint32_t humans)
{
    /* kinds: 0 type (+0x48), 1 team AI (+0x28), 2 group (+8, humans too),
     * 3 team (+0x28, humans too), 4 group AI (+8) */
    uint32_t self=ENTRY(12),i,n=0,expect[80];
    int humans_too=kind==2 || kind==3;
    live(count);WORD_AT(HUMANS)=humans;
    WORD_AT(0x00780000u+0x7Cu)=self;
    WORD_AT(0x00680000u+12u*0x100u+0x74u)=7u;                /* self record: selector 7 */
    WORD_AT(0x007B0C48u)=0x00690000u;WORD_AT(0x00690014u)=0xDEF0u;
    for(i=0;i<count;i++) {
        uint32_t r=0x00680000u+i*0x100u;
        HALF_AT(ENTRY(i)+0x48u)=(i%3u)==0u?7:3;
        WORD_AT(r+8u)=(i%5u)==0u?7u:1u;WORD_AT(r+0x28u)=(i%4u)==0u?7u:2u;
    }
    WORD_AT(0x00680000u+12u*0x100u+8u)=7u;HALF_AT(self+0x48u)=7;   /* self matches: skipped */
    for(i=(humans_too?0u:humans);i<count;i++) {
        int match;
        if(i==12u) continue;
        if(humans_too && i<humans) { expect[n++]=ENTRY(i);continue; }
        match=kind==0?(i%3u)==0u:(kind==2 || kind==4)?(i%5u)==0u:(i%4u)==0u;
        if(match) expect[n++]=ENTRY(i);
    }
    picks=0;call_setup(0x00780000u,0,0);
    HOOK(hook,fake_plain);
    if(count<=32u) return originals==1 && picks==0;
    if(originals || picks!=1 || g_eax!=0x00C0FFEEu) return 0;
    if(pick_args[0]!=self || pick_args[1]!=n || pick_args[3]!=((kind==1 || kind==4)?0u:0xDEF0u)) return 0;
    if(pick_esp>STACK-0x100u || pick_args[2]!=STACK-0x100u) return 0;
    for(i=0;i<n;i++) if(pick_list[i]!=expect[i]) return 0;
    return 1;
}

int main(void)
{
    uint32_t i,e;
    unsigned drops;
    g_xbox_mem_offset=(uintptr_t)memory;
    set_env(NULL);
    WORD_AT(NETWORK)=0;

    /* ---- actor pool ---- */
    WORD_AT(SCENE)=100u;WORD_AT(0x007DE304u)=1u;
    WORD_AT(0x00790958u)=0xFFFFFFFFu;WORD_AT(0x00790954u)=0xFFFFFFFFu;flags_1cd040=0x1000u;
    WORD_AT(0x007356D0u)=7u;WORD_AT(0x007356D4u)=7u;WORD_AT(0x007B0C44u)=7u;
    WORD_AT(0x00735630u)=7u;WORD_AT(0x007331A8u+0x870u*4u-4u)=7u;WORD_AT(0x00735368u+0xB1u*4u-4u)=7u;
    WORD_AT(0x00735368u+0xB1u*4u)=9u;                        /* past the clear: kept */
    allocs=0;call_setup(0,0,0);
    HOOK(pc_mapmaker_actors_pool,fake_pool);
    CHECK(originals==0 && allocs==1 && alloc_size==((64u*0xEA0u+15u)&~15u) && alloc_heap==1u);
    e=WORD_AT(POOLPTR);
    CHECK(e!=0 && e!=0x00600000u && mapmaker_actors.pool_ptr==e && mapmaker_actors.pool_slots==64u);
    CHECK(WORD_AT(e)==0 && WORD_AT(e+63u*0xEA0u+0xE9Cu)==0);   /* cleared */
    CHECK(WORD_AT(0x007356D0u)==1u && WORD_AT(0x007356D4u)==0 && WORD_AT(0x007B0C44u)==0);
    CHECK(WORD_AT(0x00735630u)==0 && WORD_AT(0x007331A8u+0x870u*4u-4u)==0 && WORD_AT(0x00735368u+0xB1u*4u-4u)==0);
    CHECK(WORD_AT(0x00735368u+0xB1u*4u)==9u);
    flags_1cd040=0;call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool,fake_pool);
    CHECK(WORD_AT(0x007356D0u)==0);
    WORD_AT(0x007DE304u)=0;allocs=0;call_setup(0,0,0);
    HOOK(pc_mapmaker_actors_pool,fake_pool);
    CHECK(allocs==0 && originals==0);
    /* clear: the recorded 64 slots in scene 100 */
    WORD_AT(0x007DE304u)=1u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool,fake_pool);
    e=WORD_AT(POOLPTR);memset((char *)memory+e,0x11,64u*0xEA0u);
    call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool_clear,fake_plain);
    CHECK(originals==0 && WORD_AT(e+63u*0xEA0u+0xE9Cu)==0 && WORD_AT(e)==0);
    WORD_AT(SCENE)=1u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool_clear,fake_plain);
    CHECK(originals==1);
    WORD_AT(SCENE)=100u;WORD_AT(POOLPTR)=0x00450000u;call_setup(0,0,0);
    HOOK(pc_mapmaker_actors_pool_clear,fake_plain);
    CHECK(originals==1);
    /* other scenes: the original, recorded with the stock size */
    pool_record(1u);CHECK(originals==1 && mapmaker_actors.pool_ptr==POOL && mapmaker_actors.pool_slots==32u);
    pool_record(101u);CHECK(mapmaker_actors.pool_slots==4u);

    /* ---- spawner gate ---- */
    WORD_AT(SCENE)=1u;mode(1);
    CHECK(last_allowed(0)==19);                 /* story level: stock */
    CHECK(last_allowed(1)==80);
    gate_allows(25u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==25u);
    WORD_AT(SCENE)=100u;
    CHECK(last_allowed(0)==63);                 /* local MapMaker story map: 64 characters */
    CHECK(last_allowed(1)==64);
    gate_allows(50u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==6u);
    mode(0);
    CHECK(last_allowed(0)==31);                 /* other modes in scene 100: 32 */
    CHECK(last_allowed(1)==32);
    mode(1);WORD_AT(NETWORK)=1u;
    CHECK(last_allowed(0)==19);                 /* System Link: stock */
    CHECK(last_allowed(1)==20);
    WORD_AT(NETWORK)=0;
    WORD_AT(SCENE)=102u;gate_allows(25u,0);CHECK(WORD_AT(MAPMAKER_ACTORS_GATE)==25u);
    WORD_AT(SCENE)=100u;
    set_env("24");CHECK(last_allowed(0)==23);CHECK(last_allowed(1)==24);
    set_env("20");CHECK(last_allowed(0)==19);
    set_env("64");CHECK(last_allowed(0)==63);
    set_env("40");mode(0);CHECK(last_allowed(0)==31);mode(1);CHECK(last_allowed(0)==39);
    set_env("65");CHECK(last_allowed(0)==63);
    set_env("0");CHECK(last_allowed(0)==63);
    set_env("12x");CHECK(last_allowed(0)==63);
    set_env(NULL);

    /* ---- AI slot guard ---- */
    WORD_AT(SCENE)=100u;WORD_AT(0x007DE304u)=1u;
    call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool,fake_pool);  /* 64-slot pool recorded */
    fill(1u,30u,1);CHECK(slot_call()==1);       /* past the stock 30 */
    fill(1u,62u,1);CHECK(slot_call()==1);       /* slot 63, the last */
    fill(1u,63u,1);CHECK(slot_call()==0);       /* 63 AI slots used: refused, as stock at 30 */
    drops=mapmaker_actors.drops[MAPMAKER_ACTORS_DROP_SLOT];
    CHECK(drops==0u);
    fill(2u,62u,1);CHECK(slot_call()==0);       /* slot 64: past the pool */
    CHECK(mapmaker_actors.drops[MAPMAKER_ACTORS_DROP_SLOT]==drops+1u);
    WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x10u)=0;WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x14u)=0;
    CHECK(slot_call()==1);                      /* a freed slot is reused */
    fill(4u,59u,1);CHECK(slot_call()==1);       /* slot 63 */
    fill(4u,60u,1);CHECK(slot_call()==0);       /* slot 64 */
    fill(4u,60u,1);WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x18u)=1u;WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x10u)=0;
    WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x14u)=0;CHECK(slot_call()==1);   /* story: +0x10/+0x14 */
    fill(4u,60u,1);WORD_AT(WORD_AT(POOLPTR)+9u*0xEA0u+0x14u)=0;CHECK(slot_call()==0);
    /* scene 100, other modes: 32 and the pair table (humans + 10) */
    fill(2u,9u,0);CHECK(slot_call()==1);
    fill(2u,10u,0);CHECK(slot_call()==0);
    /* scene 100, System Link: 32 characters, 30 AI */
    WORD_AT(NETWORK)=1u;
    fill(1u,30u,1);CHECK(slot_call()==0);
    fill(1u,29u,1);CHECK(slot_call()==1);
    fill(4u,28u,1);CHECK(slot_call()==0);       /* slot 32 */
    WORD_AT(NETWORK)=0;
    /* a pool that was not recorded: 32 slots */
    e=mapmaker_actors.pool_ptr;fill(1u,31u,1);mapmaker_actors.pool_ptr=0;CHECK(slot_call()==0);mapmaker_actors.pool_ptr=e;
    WORD_AT(POOLPTR)=0;CHECK(slot_call()==1);   /* no pool: the original */
    /* story level (scene 1): stock 30 AI, 32 slots */
    pool_record(1u);WORD_AT(SCENE)=1u;
    fill(1u,30u,1);CHECK(slot_call()==0);
    fill(1u,29u,1);CHECK(slot_call()==1);
    fill(4u,28u,1);CHECK(slot_call()==0);
    fill(4u,27u,1);CHECK(slot_call()==1);
    WORD_AT(SCENE)=100u;

    /* ---- level-start AI clamp ---- */
    WORD_AT(LEVEL)=0x00660000u;WORD_AT(HUMANS)=1u;
    pool_record(1u);WORD_AT(POOLPTR)=POOL;
    WORD_AT(0x00660030u)=40u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_placed,fake_plain);
    CHECK(originals==1 && WORD_AT(0x00660030u)==31u);
    WORD_AT(0x00660030u)=20u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_placed,fake_plain);
    CHECK(originals==1 && WORD_AT(0x00660030u)==20u);
    WORD_AT(0x007DE304u)=1u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool,fake_pool);
    WORD_AT(0x00660030u)=70u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_placed,fake_plain);
    CHECK(WORD_AT(0x00660030u)==63u);

    /* ---- live list appends ---- */
    WORD_AT(COUNT)=63u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_list_add,fake_plain);CHECK(originals==1);
    WORD_AT(COUNT)=64u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_list_add,fake_plain);CHECK(originals==0);
    call_setup(0,0,0);HOOK(pc_mapmaker_actors_list_readd,fake_plain);CHECK(originals==0 && g_eax==0);
    WORD_AT(COUNT)=10u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_list_readd,fake_plain);CHECK(originals==1);

    /* ---- AI target candidates ---- */
    CHECK(test_pick(pc_mapmaker_actors_pick_type,0,32u,2u));
    CHECK(test_pick(pc_mapmaker_actors_pick_type,0,64u,2u));
    CHECK(test_pick(pc_mapmaker_actors_pick_team_ai,1,64u,2u));
    CHECK(test_pick(pc_mapmaker_actors_pick_group,2,64u,4u));
    CHECK(test_pick(pc_mapmaker_actors_pick_team,3,50u,4u));
    CHECK(test_pick(pc_mapmaker_actors_pick_group_ai,4,40u,1u));
    /* selector < 0: the default at once */
    live(40u);WORD_AT(0x00780000u+0x7Cu)=ENTRY(12);WORD_AT(0x00680000u+12u*0x100u+0x74u)=0xFFFFFFFFu;
    picks=0;call_setup(0x00780000u,0,0);HOOK(pc_mapmaker_actors_pick_group,fake_plain);
    CHECK(picks==0 && originals==0 && g_eax==0xDEF0u);
    call_setup(0x00780000u,0,0);HOOK(pc_mapmaker_actors_pick_team_ai,fake_plain);
    CHECK(picks==0 && originals==0 && g_eax==0);

    /* ---- AI enemy choice: the stock argmin over every candidate ---- */
    for(i=0;i<64u;i++) WORD_AT(ENTRY(i))=0x00C00000u+i*16u;    /* objects */
    {
        static const uint32_t seeds[]={1u,7u,12345u,999u,4242u,31337u};
        unsigned s,round,two_pass=0;
        for(s=0;s<sizeof seeds/sizeof seeds[0];s++) for(round=0;round<4u;round++) {
            uint32_t r=seeds[s]*2654435761u+round,cnt=round==3u?64u:60u-round*9u,want=0,best=0xFFFFFFFFu,ncand=0;
            live(cnt);
            for(i=0;i<64u;i++) WORD_AT(ENTRY(i))=0x00C00000u+i*16u;
            WORD_AT(0x00780000u+0x7Cu)=ENTRY(3);
            for(i=0;i<cnt;i++) {
                r=r*1103515245u+12345u;
                WORD_AT(ENTRY(i)+0x84u)=(i==3u || (r>>28)==0u)?1u:2u;
                BYTE_AT(ENTRY(i)+0x24u)=((r>>20)&15u)==0u?8u:0u;
                WORD_AT(ENTRY(i)+0x200u)=((r>>12)&15u)==0u?1u:0u;
                WORD_AT(ENTRY(i)+0x204u)=(r>>4)&(round==1u?3u:255u);   /* round 1: many ties */
            }
            if(round==2u) WORD_AT(ENTRY(55)+0x204u)=0,WORD_AT(ENTRY(55)+0x84u)=2u,BYTE_AT(ENTRY(55)+0x24u)=0,WORD_AT(ENTRY(55)+0x200u)=0;
            for(i=0;i<cnt;i++) {
                e=ENTRY(i);
                if(WORD_AT(e+0x84u)==1u || (BYTE_AT(e+0x24u)&8u)) continue;
                ncand++;                                        /* the hook's superset */
                if(WORD_AT(e+0x200u)) continue;
                if(WORD_AT(e+0x204u)<best) { best=WORD_AT(e+0x204u);want=WORD_AT(e); }
            }
            enemy_calls=0;enemy_overflow=0;enemy_count_ok=1;enemy_expect=cnt;
            call_setup(0x00780000u,0,0);HOOK(pc_mapmaker_actors_enemies,fake_enemies);
            CHECK(g_eax==want && !enemy_overflow && enemy_calls==(ncand>32u?2u:1u) && enemy_count_ok);
            if(ncand>32u) two_pass++;
            for(i=0;i<cnt;i++) CHECK(WORD_AT(LIST+i*4u)==ENTRY(i));            /* list restored */
            CHECK(WORD_AT(COUNT)==cnt);
        }
        CHECK(two_pass>=12u);                                       /* most cases take two passes */
    }
    /* early returns: no array, one call */
    live(60u);WORD_AT(0x00780000u+0x7Cu)=ENTRY(3);
    for(i=0;i<60u;i++) WORD_AT(ENTRY(i)+0x84u)=i==3u?1u:2u,WORD_AT(ENTRY(i)+0x204u)=0;
    BYTE_AT(0x007DE458u)=0x40u;enemy_calls=0;
    call_setup(0x00780000u,0,0);HOOK(pc_mapmaker_actors_enemies,fake_enemies);
    CHECK(enemy_calls==1u && enemy_overflow);                  /* (the fake does not model the early return) */
    BYTE_AT(0x007DE458u)=0;WORD_AT(0x0074C388u)=99u;enemy_calls=0;
    call_setup(0x00780000u,1u,0);HOOK(pc_mapmaker_actors_enemies,fake_enemies);
    CHECK(enemy_calls==1u);
    WORD_AT(0x0074C388u)=3u;enemy_calls=0;enemy_overflow=0;
    call_setup(0x00780000u,1u,0);HOOK(pc_mapmaker_actors_enemies,fake_enemies);
    CHECK(enemy_calls==2u && !enemy_overflow);
    live(33u);for(i=0;i<33u;i++) WORD_AT(ENTRY(i)+0x84u)=i==3u?1u:2u;
    enemy_calls=0;call_setup(0x00780000u,0,0);HOOK(pc_mapmaker_actors_enemies,fake_enemies);
    CHECK(enemy_calls==1u);

    /* ---- per-opponent kill words: victims 32..63 in the side table ---- */
    live(50u);
    WORD_AT(0x007DE30Cu)=6u;BYTE_AT(SUBMODE)=0;
    memset((char *)memory+MAPMAKER_ACTORS_SIDE,0,64u*32u*2u);
    HALF_AT(ENTRY(1)+0xA10u+40u*2u)=5;                          /* = +0xA60 */
    call_setup(ENTRY(1),40u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==1 && HALF_AT(ENTRY(1)+0xA60u)==5 && HALF_AT(SIDE(1u,40u))==3);
    call_setup(ENTRY(1),40u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(HALF_AT(SIDE(1u,40u))==6);
    WORD_AT(0x007DE30Cu)=7u;HALF_AT(ENTRY(1)+0xA10u+33u*2u)=-2;
    call_setup(ENTRY(1),33u,0x10004u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(HALF_AT(ENTRY(1)+0xA52u)==-2 && HALF_AT(SIDE(1u,33u))==4);
    WORD_AT(ENTRY(1)+8u)=45u;call_setup(ENTRY(1),63u,2u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(HALF_AT(SIDE(45u,63u))==2);WORD_AT(ENTRY(1)+8u)=1u;
    call_setup(ENTRY(1),20u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==1 && HALF_AT(ENTRY(1)+0xA10u+20u*2u)==3);  /* ids < 32: the actor's words */
    WORD_AT(0x007DE30Cu)=5u;HALF_AT(ENTRY(1)+0xA60u)=9;
    call_setup(ENTRY(1),40u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==1 && HALF_AT(ENTRY(1)+0xA60u)==9 && HALF_AT(SIDE(1u,40u))==6);
    WORD_AT(0x007DE30Cu)=6u;call_setup(0,40u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==1);
    BYTE_AT(SUBMODE)=0x13u;call_setup(ENTRY(1),40u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==0 && HALF_AT(ENTRY(1)+0xA60u)==9 && HALF_AT(SIDE(1u,40u))==6);
    BYTE_AT(SUBMODE)=0x12u;call_setup(ENTRY(1),20u,3u);HOOK(pc_mapmaker_actors_kill,fake_kill);
    CHECK(originals==1);
    BYTE_AT(SUBMODE)=0;

    /* creation clears the new actor's side row */
    for(i=0;i<32u;i++) HALF_AT(SIDE(40u,32u+i))=(int16_t)(i+1u);
    HALF_AT(SIDE(41u,40u))=11;
    create_entity=0x00C00400u;create_slot=40u;
    call_setup(0,0,40u);HOOK(pc_mapmaker_actors_create,fake_create);
    CHECK(originals==1 && HALF_AT(SIDE(40u,32u))==0 && HALF_AT(SIDE(40u,63u))==0 && HALF_AT(SIDE(41u,40u))==11);
    HALF_AT(SIDE(40u,50u))=4;create_entity=0;
    call_setup(0,0,40u);HOOK(pc_mapmaker_actors_create,fake_create);
    CHECK(HALF_AT(SIDE(40u,50u))==4);                          /* creation failed: kept */
    create_entity=0x00C00400u;create_slot=41u;WORD_AT(ENTRY(40))=0;
    call_setup(0,0,40u);HOOK(pc_mapmaker_actors_create,fake_create);
    CHECK(HALF_AT(SIDE(40u,50u))==4);                          /* not this slot's object */
    create_slot=40u;
    call_setup(0,0,70u);HOOK(pc_mapmaker_actors_create,fake_create);
    CHECK(originals==1 && HALF_AT(SIDE(40u,50u))==4);

    /* kills on other teams */
    live(50u);WORD_AT(HUMANS)=2u;WORD_AT(USED)=48u;
    memset((char *)memory+MAPMAKER_ACTORS_SIDE,0,64u*32u*2u);
    for(i=0;i<64u;i++) WORD_AT(ENTRY(i)+0x84u)=(i%2u)?2u:1u;
    e=ENTRY(4);                                                 /* team 1, slot 4 */
    for(i=0;i<32u;i++) HALF_AT(e+0xA10u+i*2u)=(int16_t)(i+1u);
    for(i=32u;i<64u;i++) HALF_AT(SIDE(4u,i))=100;
    HALF_AT(e+0xA50u)=100;HALF_AT(e+0xA52u)=1000;HALF_AT(e+0xA60u)=-7;
    network_call=0;WORD_AT(0x007DE474u)=0;BYTE_AT(0x007DE458u)=0;
    call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK(originals==0 && g_eax==272u+900u);                   /* odd j < 32: 2+..+32; odd 33..49: 9 x 100 */
    network_call=1;call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK(g_eax==1272u);
    network_call=0;BYTE_AT(0x007DE458u)=0x10u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK((int32_t)g_eax==1172-1000);
    BYTE_AT(0x007DE458u)=0x20u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK(g_eax==1172u-5u);                                     /* own word: id 4 */
    WORD_AT(e+8u)=40u;HALF_AT(SIDE(40u,40u))=7;HALF_AT(SIDE(40u,41u))=30;
    call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK(g_eax==272u+30u-7u);                                  /* slot 40: its side row, own word */
    WORD_AT(e+8u)=4u;WORD_AT(0x007DE474u)=1u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK((int32_t)g_eax==-7);
    WORD_AT(0x007DE474u)=0;BYTE_AT(0x007DE458u)=0;
    WORD_AT(USED)=30u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_score,fake_plain);
    CHECK(originals==1);                                        /* <= 32: the original */
    /* times killed: the side table's column */
    WORD_AT(USED)=48u;WORD_AT(e+8u)=40u;
    memset((char *)memory+MAPMAKER_ACTORS_SIDE,0,64u*32u*2u);
    HALF_AT(SIDE(3u,40u))=2;HALF_AT(SIDE(45u,40u))=5;HALF_AT(SIDE(55u,40u))=50;HALF_AT(SIDE(3u,41u))=9;
    call_setup(e,0,0);HOOK(pc_mapmaker_actors_deaths,fake_plain);CHECK(originals==0 && g_eax==7u);
    WORD_AT(USED)=38u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_deaths,fake_plain);CHECK(g_eax==2u);
    WORD_AT(USED)=48u;
    WORD_AT(e+8u)=4u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_deaths,fake_plain);CHECK(originals==1);
    WORD_AT(e+8u)=40u;BYTE_AT(0x007DE458u)=0x40u;call_setup(e,0,0);HOOK(pc_mapmaker_actors_deaths,fake_plain);
    CHECK(originals==1);
    BYTE_AT(0x007DE458u)=0;

    /* ---- radar blips ---- */
    WORD_AT(0x007490F8u)=255u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_blip,fake_plain);CHECK(originals==1);
    WORD_AT(0x007490F8u)=256u;drops=mapmaker_actors.drops[MAPMAKER_ACTORS_DROP_BLIP];
    call_setup(0,0,0);HOOK(pc_mapmaker_actors_blip,fake_plain);
    CHECK(originals==0 && g_eax==256u && mapmaker_actors.drops[MAPMAKER_ACTORS_DROP_BLIP]==drops+1u);

    /* ---- patrol table copy (refreshed by the pool hook) ---- */
    for(i=0;i<0x600u;i+=4u) WORD_AT(0x00520C38u+i)=0x10000u+i;
    memset((char *)memory+MAPMAKER_ACTORS_PATROL,0,0xC00u);
    WORD_AT(MAPMAKER_ACTORS_PATROL+65u*40u)=0x77u;
    pool_record(1u);
    for(i=0;i<32u*40u;i+=4u) CHECK(WORD_AT(MAPMAKER_ACTORS_PATROL+i)==0x10000u+i);
    for(i=32u*40u;i<65u*40u;i+=4u) CHECK(WORD_AT(MAPMAKER_ACTORS_PATROL+i)==0xFFFFFFFFu);
    CHECK(WORD_AT(MAPMAKER_ACTORS_PATROL+65u*40u)==0x77u);
    /* the pool hook (any scene) and the pool clear reset the side table */
    HALF_AT(SIDE(5u,40u))=3;pool_record(1u);CHECK(HALF_AT(SIDE(5u,40u))==0);
    HALF_AT(SIDE(5u,40u))=3;WORD_AT(SCENE)=1u;call_setup(0,0,0);HOOK(pc_mapmaker_actors_pool_clear,fake_plain);
    CHECK(HALF_AT(SIDE(5u,40u))==0);WORD_AT(SCENE)=100u;

    /* ---- per-death effect list: 64 in wide play, the stock 30 elsewhere ---- */
    WORD_AT(SCENE)=100u;WORD_AT(NETWORK)=0;mode(1);
    WORD_AT(0x0075F6E8u)=40u;call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==1);                                        /* wide: the original (bound 64) */
    mode(0);call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==0 && g_eax==0x00777000u);                  /* arcade: refused at 30, as stock */
    WORD_AT(0x0075F6E8u)=30u;call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==0);
    WORD_AT(0x0075F6E8u)=29u;call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==1);
    mode(1);WORD_AT(SCENE)=1u;WORD_AT(0x0075F6E8u)=30u;call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==0);                                        /* story level: 30 */
    WORD_AT(SCENE)=100u;WORD_AT(NETWORK)=1u;call_setup(0x00777000u,0,0);HOOK(pc_mapmaker_actors_effects,fake_plain);
    CHECK(originals==0);                                        /* System Link: 30 */
    WORD_AT(NETWORK)=0;


    if(failures) { fprintf(stderr,"%d failures\n",failures); return 1; }
    printf("mapmaker actors ok\n");
    return 0;
}
