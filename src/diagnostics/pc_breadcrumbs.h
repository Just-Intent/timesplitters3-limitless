#ifndef TSFP_PC_BREADCRUMBS_H
#define TSFP_PC_BREADCRUMBS_H
/* Bounded numeric events only. Producers never allocate, wait or write files.
 * Every shared field is atomic: a helper can read while a producer is stopped. */
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
typedef volatile LONG64 PcCrumbAtomic;
static inline int64_t pc_crumb_load(PcCrumbAtomic *p){return InterlockedCompareExchange64(p,0,0);}
static inline void pc_crumb_store(PcCrumbAtomic *p,int64_t v){InterlockedExchange64(p,v);}
static inline int pc_crumb_lock(PcCrumbAtomic *p){return InterlockedCompareExchange64(p,1,0)==0;}
#else
#include <stdatomic.h>
typedef _Atomic int64_t PcCrumbAtomic;
_Static_assert(ATOMIC_LLONG_LOCK_FREE==2,"Crash breadcrumbs require lock-free shared atomics");
static inline int64_t pc_crumb_load(PcCrumbAtomic *p){return atomic_load(p);}
static inline void pc_crumb_store(PcCrumbAtomic *p,int64_t v){atomic_store(p,v);}
static inline int pc_crumb_lock(PcCrumbAtomic *p){int64_t expected=0;return atomic_compare_exchange_strong(p,&expected,1);}
#endif
#define PC_CRUMB_COUNT 64
#define PC_CRUMB_FIELDS 6
enum {PC_CRUMB_STARTED=1,PC_CRUMB_GUEST_START,PC_CRUMB_GUEST_STOP,PC_CRUMB_MODAL,PC_CRUMB_GAME=10};
typedef struct PcCrumbSlot {PcCrumbAtomic sequence,ms,event,value;} PcCrumbSlot;
typedef struct PcBreadcrumbs {
    PcCrumbAtomic writer,next;
    int have_game,have_match; int32_t match[4]; int32_t game[PC_CRUMB_FIELDS]; /* accessed only by the producer owning writer */
    PcCrumbSlot slots[PC_CRUMB_COUNT];
} PcBreadcrumbs;
static inline void pc_crumb_append_locked(PcBreadcrumbs *b,int64_t ms,int event,int32_t value){
    int64_t next=pc_crumb_load(&b->next)+1;
    PcCrumbSlot *s=&b->slots[(next-1)%PC_CRUMB_COUNT];
    pc_crumb_store(&s->sequence,0);
    pc_crumb_store(&s->ms,ms);pc_crumb_store(&s->event,event);pc_crumb_store(&s->value,value);
    pc_crumb_store(&s->sequence,next);pc_crumb_store(&b->next,next);
}
static inline void pc_crumb_add(PcBreadcrumbs *b,int64_t ms,int event,int32_t value){
    if(!pc_crumb_lock(&b->writer))return;
    pc_crumb_append_locked(b,ms,event,value);pc_crumb_store(&b->writer,0);
}
static inline void pc_crumb_game(PcBreadcrumbs *b,int64_t ms,const int32_t values[PC_CRUMB_FIELDS]){
    if(!pc_crumb_lock(&b->writer))return;
    for(int i=0;i<PC_CRUMB_FIELDS;i++)if(!b->have_game || b->game[i]!=values[i]){
        pc_crumb_append_locked(b,ms,PC_CRUMB_GAME+i,values[i]);b->game[i]=values[i];
    }
    b->have_game=1;pc_crumb_store(&b->writer,0);
}
static inline const char *pc_crumb_name(int64_t event){
    switch(event){case PC_CRUMB_STARTED:return "capture-started";case PC_CRUMB_GUEST_START:return "guest-start";
    case PC_CRUMB_GUEST_STOP:return "guest-stop";case PC_CRUMB_MODAL:return "modal-depth";
    case PC_CRUMB_GAME:return "scene-id";case PC_CRUMB_GAME+1:return "game-mode";
    case PC_CRUMB_GAME+2:return "menu-mode";case PC_CRUMB_GAME+3:return "paused";
    case PC_CRUMB_GAME+4:return "network-mode";case PC_CRUMB_GAME+5:return "cutscene";case 20:return "model-address";case 21:return "node-index";case 22:return "traversal-depth";case 30:return "human-count";case 31:return "bot-count";case 32:return "difficulty";case 33:return "player-capacity";case 40:return "allocation-event";case 41:return "allocation-address";case 42:return "allocation-bytes";default:return "unknown";}
}
static inline void pc_crumb_report(FILE *out,PcBreadcrumbs *b){
    int64_t last=pc_crumb_load(&b->next),first=last>PC_CRUMB_COUNT?last-PC_CRUMB_COUNT+1:1;
    fputs("Gameplay breadcrumbs (milliseconds since capture initialization)\nBounded sampled history; brief transitions and interrupted writes can be absent.\n",out);
    for(int64_t n=first;n<=last;n++){
        PcCrumbSlot *s=&b->slots[(n-1)%PC_CRUMB_COUNT];
        if(pc_crumb_load(&s->sequence)!=n)continue;
        int64_t ms=pc_crumb_load(&s->ms),event=pc_crumb_load(&s->event),value=pc_crumb_load(&s->value);
        if(pc_crumb_load(&s->sequence)==n)fprintf(out,"%lld %lld %s %lld\n",(long long)n,(long long)ms,pc_crumb_name(event),(long long)value);
    }
}
static inline void pc_crumb_model(PcBreadcrumbs *b,int64_t ms,uint32_t model,uint32_t node,unsigned depth){
    if(!pc_crumb_lock(&b->writer))return;
    pc_crumb_append_locked(b,ms,20,(int32_t)model);
    pc_crumb_append_locked(b,ms,21,(int32_t)node);
    pc_crumb_append_locked(b,ms,22,(int32_t)depth);
    pc_crumb_store(&b->writer,0);
}
static inline void pc_crumb_match(PcBreadcrumbs *b,int64_t ms,const int32_t values[4]){
    if(!pc_crumb_lock(&b->writer))return;
    for(unsigned i=0;i<4;i++)if(!b->have_match||b->match[i]!=values[i]){
        pc_crumb_append_locked(b,ms,30+i,values[i]);b->match[i]=values[i];
    }
    b->have_match=1;pc_crumb_store(&b->writer,0);
}
static inline void pc_crumb_allocation(PcBreadcrumbs *b,int64_t ms,uint32_t event,uint32_t address,uint32_t bytes){
    if(!pc_crumb_lock(&b->writer))return;
    pc_crumb_append_locked(b,ms,40,(int32_t)event);pc_crumb_append_locked(b,ms,41,(int32_t)address);pc_crumb_append_locked(b,ms,42,(int32_t)bytes);
    pc_crumb_store(&b->writer,0);
}
/* Guest allocation headroom (read by the MapMaker probe, flag 32). Fed by the
 * allocation observer, which the runtime calls under its arena lock, so there
 * is one writer at a time; readers take plain 32-bit snapshots. Event kind 1
 * = runtime heap (xbox_HeapAlloc), 3 = physical/contiguous window; bit 8 =
 * release (only successful allocations are reported). Ends are guest
 * addresses one past the highest block seen. */
enum { PC_USAGE_HEAP_LIVE, PC_USAGE_HEAP_PEAK, PC_USAGE_HEAP_END_PEAK,
       PC_USAGE_PHYS_LIVE, PC_USAGE_PHYS_PEAK, PC_USAGE_PHYS_END_PEAK,
       PC_USAGE_WORDS };
static inline void pc_allocation_usage_note(volatile uint32_t u[PC_USAGE_WORDS],uint32_t event,uint32_t address,uint32_t bytes){
    unsigned kind=event&0xFFu,live=kind==1u?PC_USAGE_HEAP_LIVE:kind==3u?PC_USAGE_PHYS_LIVE:PC_USAGE_WORDS;
    if(live==PC_USAGE_WORDS)return;
    if(event&0x100u) { u[live]=u[live]>=bytes?u[live]-bytes:0u;return; }
    u[live]+=bytes;
    if(u[live]>u[live+1u])u[live+1u]=u[live];
    if(address+bytes>u[live+2u])u[live+2u]=address+bytes;
}
/* Copy of the counters above (zero before the first allocation). */
void pc_capture_allocation_usage(uint32_t out[PC_USAGE_WORDS]);
void pc_capture_allocation(uint32_t event,uint32_t address,uint32_t bytes);
void pc_capture_match(unsigned humans,unsigned bots,unsigned difficulty,unsigned capacity);
void pc_capture_model(uint32_t model,uint32_t node,unsigned depth);
/* Both native capture adapters implement this; called on the guest thread. */
void pc_capture_gameplay(int32_t scene,uint32_t mode,uint32_t menu,int paused,int network,int cutscene);
#endif
