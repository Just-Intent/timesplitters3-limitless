/* Asset-free contract for src/pc_mapmaker_container.inc (MapMaker map
 * container 0x7FE0 -> 0x7FFE0): persistent buffers, size-tier load and save
 * (stock size first, stock-identical saves for maps that fit), CRC past
 * 64 KB, the editor capacity behind TSFP_MAPMAKER_LIMITS=1 and the System
 * Link refusal. Guest memory, the file and the guest functions are fakes. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x1000000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }
static uint8_t *host(uint32_t a) { return (uint8_t *)memory+a; }

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* ---- runtime heap ---- */
static uint32_t heap_next=0x00A00000u, heap_sizes[8];
static unsigned heap_allocs;
uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment)
{
    uint32_t p=(heap_next+alignment-1)&~(alignment-1);
    heap_next=p+size;
    memset(host(p),0xCD,size);           /* dirty: the hooks must zero */
    if(heap_allocs<8) heap_sizes[heap_allocs]=size;
    heap_allocs++;
    return p;
}

/* ---- guest functions ---- */
/* Distinct bodies: identical empty functions may be folded to one address. */
static volatile int fake_marker;
void sub_0003DFA0(void) { fake_marker=1; }
void sub_000827B0(void) { fake_marker=2; }
void sub_000827F0(void) { fake_marker=3; }
void sub_0035E960(void) { fake_marker=4; }
void sub_00025280(void) { fake_marker=5; }
void sub_00336600(void) { fake_marker=6; }
static uint32_t mode_827f0=2, network=0, arg_827b0=99, call_args[12];
static uint32_t file_body;                           /* file = body + 20-byte signature */
static uint8_t file_data[0x80000];
static int file_present;
static uint32_t reads, read_sizes[8], read_flags[8];
static void undo_ring_original(void) { failures++; } /* the hook replaces it */
void pc_mapmaker_container_undo_ring(void (*original)(void));

static void fake_read(void)
{
    /* sub_00025280(dev, path, name, buffer, body, flags): error 7 when the file
     * is larger than body + two signatures, 8 when shorter than body. */
    uint32_t buf=call_args[3],body=call_args[4];
    if(reads<8) { read_sizes[reads]=body;read_flags[reads]=call_args[5]; }
    reads++;
    if(call_args[2]!=0x0047DF80u) { g_eax=9;return; }
    if(!file_present) { g_eax=8;return; }
    if(file_body+20u>body+40u) { g_eax=7;return; }
    if(file_body+20u<body) { g_eax=8;return; }
    memcpy(host(buf),file_data,body);
    g_eax=1;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    GuestRegisters r;
    int i;
    uint32_t result;
    va_start(a,n);
    for(i=0;i<n && i<12;i++) call_args[i]=va_arg(a,uint32_t);
    va_end(a);
    save_registers(&r);
    if(f==sub_000827B0) arg_827b0=call_args[0];
    else if(f==sub_000827F0) g_eax=mode_827f0;
    else if(f==sub_0035E960) g_eax=network;
    else if(f==sub_00025280) fake_read();
    else if(f==sub_00336600) pc_mapmaker_container_undo_ring(undo_ring_original);
    else if(f==sub_0003DFA0) g_eax=0;
    result=g_eax;
    restore_registers(&r);
    return result;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_container.inc"

static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

/* zlib CRC-32, as sub_001504F0 (table 0x004E74B8, init -1, final not). */
static uint32_t crc32_of(uint32_t a,uint32_t n)
{
    uint32_t c=0xFFFFFFFFu,i,k;
    for(i=0;i<n;i++) { c^=host(a)[i]; for(k=0;k<8;k++) c=c&1u?(c>>1)^0xEDB88320u:c>>1; }
    return ~c;
}
static unsigned crc_calls, check_calls;
static void fake_crc(void) { crc_calls++; g_eax=crc32_of(WORD_AT(g_esp+4),WORD_AT(g_esp+8)); }
static void fake_crc_check(void)
{
    check_calls++;
    g_eax=crc32_of(WORD_AT(g_esp+4),WORD_AT(g_esp+8))==WORD_AT(g_esp+12);
}

/* A compiled container: header, map info (0x3DC bytes, used at +0x248),
 * filler chunk, terminator whose size field is its own offset. */
#define ED_STACK 0x00700000u
static void make_map(uint32_t c,uint32_t used,uint32_t seed)
{
    uint32_t i,fill=used-8u-(8u+0x3DCu)-8u-8u;
    memset(host(c),0,MAPMAKER_CONTAINER_N);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77668u;WORD_AT(c+12)=0x3DCu;
    WORD_AT(c+16+0x248)=used;
    WORD_AT(c+16+0x3DC)=0x00C77669u;WORD_AT(c+16+0x3DC+4)=fill;
    for(i=0;i<fill;i++) host(c+16+0x3DC+8)[i]=(uint8_t)(i*31u+seed);
    WORD_AT(c+used-8)=0x00C7766Au;WORD_AT(c+used-4)=used-8u;
}
/* What the save state stores: CRC over used bytes at c + used. */
static void seal(uint32_t c)
{
    uint32_t used=WORD_AT(c+16+0x248);
    WORD_AT(c+used)=crc32_of(c,used);
}
static void file_from(uint32_t c,uint32_t body)
{
    memcpy(file_data,host(c),body);file_body=body;file_present=1;
}
static void stack(uint32_t ret,int n,...)
{
    va_list a;
    int i;
    g_esp=ED_STACK;WORD_AT(g_esp)=ret;
    va_start(a,n);
    for(i=0;i<n;i++) WORD_AT(g_esp+4u+4u*(uint32_t)i)=va_arg(a,uint32_t);
    va_end(a);
}
static void dummy(void) {}
static uint32_t seen_size;
static void record_arg4(void) { seen_size=WORD_AT(g_esp+4u+16u); }
static void record_arg5(void) { seen_size=WORD_AT(g_esp+4u+20u); }
static void record_arg1(void) { seen_size=WORD_AT(g_esp+8u); }
static uint32_t find_result;
static void fake_find(void) { g_eax=find_result; }
static uint32_t compile_result;
static void fake_compile(void) { g_eax=compile_result; }
static int sends;
static void fake_send(void) { sends++; }
static unsigned editor_originals;
static void fake_editor_init(void) { editor_originals++; }

/* Load the slot entry 0x00765A88 into the container through the hook. */
static uint32_t load(void)
{
    reads=0;
    stack(0x0032BBFAu,2,0x00765A88u,WORD_AT(0x00784014u));
    pc_mapmaker_container_load(dummy);
    return g_eax;
}
static uint32_t save_tier(uint32_t c)
{
    stack(0x00190A54u,6,10u,0x0047DF80u,0x00600000u,c,MAPMAKER_CONTAINER_STOCK,2u);
    pc_mapmaker_container_save_sync(record_arg4);
    return seen_size;
}

int main(void)
{
    uint32_t c,ring,info,i;
    static uint8_t snap[0x80000];
    g_xbox_mem_offset=(uintptr_t)memory;
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);

    /* ---- buffers: once from the runtime heap, zeroed, reused ---- */
    WORD_AT(0x00784014u)=0;WORD_AT(0x00765C28u)=7;WORD_AT(0x00765C30u)=7;WORD_AT(0x00765C20u)=7;
    pc_mapmaker_container_frontend_init(dummy);
    c=WORD_AT(0x00784014u);ring=WORD_AT(0x00765C24u);
    CHECK(heap_allocs==2 && heap_sizes[0]==MAPMAKER_CONTAINER_N+MAPMAKER_CONTAINER_SCRATCH &&
          heap_sizes[1]==2u*MAPMAKER_CONTAINER_N);
    CHECK(c>=0x00A00000u && ring>=0x00A00000u && g_eax==0);
    CHECK(WORD_AT(c)==0 && WORD_AT(c+MAPMAKER_CONTAINER_N-4)==0 && WORD_AT(ring+2u*MAPMAKER_CONTAINER_N-4)==0);
    CHECK(WORD_AT(0x00765C28u)==0 && WORD_AT(0x00765C30u)==0 && WORD_AT(0x00765C20u)==0);
    WORD_AT(c+0x40000)=5;WORD_AT(0x00784014u)=0;
    pc_mapmaker_container_frontend_init(dummy);
    CHECK(heap_allocs==2 && WORD_AT(0x00784014u)==c && WORD_AT(c+0x40000)==0);
    WORD_AT(0x00784014u)=0;
    pc_mapmaker_container_editor_init(fake_editor_init);
    CHECK(editor_originals==1 && WORD_AT(0x00784014u)==c && heap_allocs==2);
    pc_mapmaker_container_editor_init(fake_editor_init);
    CHECK(editor_originals==2);

    /* ---- meter constant only with the editor limits ---- */
    WORD_AT(0x0049F7ACu)=0x38002008u;
    mapmaker_container_editor_apply();
    CHECK(WORD_AT(0x0049F7ACu)==0x38002008u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    pc_mapmaker_container_editor_init(fake_editor_init);
    CHECK(FLOAT_AT(0x0049F7ACu)>0.99f/(float)MAPMAKER_CONTAINER_L && FLOAT_AT(0x0049F7ACu)<1.01f/(float)MAPMAKER_CONTAINER_L);
    WORD_AT(0x0049F7ACu)=0x38002008u;
    set_env("TSFP_MAPMAKER_LIMITS",NULL);

    /* ---- load: stock file, stock size only, buffer cleared, 16-bit word ---- */
    make_map(c,0x1000u,1);seal(c);file_from(c,MAPMAKER_CONTAINER_STOCK);
    memcpy(snap,host(c),MAPMAKER_CONTAINER_STOCK);
    memset(host(c),0xEE,MAPMAKER_CONTAINER_N);
    WORD_AT(0x00765A88u+0x64)=0xAABB1234u;WORD_AT(0x00765A88u+0x68)=0x00000000u;
    CHECK(load()==1 && reads==1 && read_sizes[0]==MAPMAKER_CONTAINER_STOCK && read_flags[0]==2u);
    CHECK(arg_827b0==1u);
    CHECK(!memcmp(snap,host(c),MAPMAKER_CONTAINER_STOCK) && WORD_AT(c+MAPMAKER_CONTAINER_N-4)==0);
    CHECK(WORD_AT(0x00765A88u+0x64)==0x10041234u && WORD_AT(0x00765A88u+0x68)==0); /* +0x66 = used+4 */
    CHECK(WORD_AT(0x00762CBCu)==0);

    /* larger tier: 0x7FE0 refused (7), 0xFFE0 too large still (7), 0x1FFE0 read */
    make_map(c,0x18000u,2);seal(c);file_from(c,0x1FFE0u);
    memcpy(snap,host(c),0x1FFE0u);
    memset(host(c),0xEE,MAPMAKER_CONTAINER_N);
    CHECK(load()==1 && reads==3 && read_sizes[0]==MAPMAKER_CONTAINER_STOCK && read_sizes[1]==0xFFE0u &&
          read_sizes[2]==0x1FFE0u);
    CHECK(!memcmp(snap,host(c),0x1FFE0u) && WORD_AT(c+0x1FFE0u)==0);
    CHECK((WORD_AT(0x00765A88u+0x64)>>16)==((0x18004u)&0xFFFFu) && WORD_AT(0x00765A88u+0x68)==0);
    /* the load check sees the full length */
    stack(0x0032B42Au,3,c,0x8000u,WORD_AT(c+0x8000u));
    check_calls=0;
    pc_mapmaker_container_crc_check(fake_crc_check);
    CHECK(g_eax==1 && check_calls==1 && WORD_AT(g_esp+8)==0x8000u);
    host(c+0x12345)[0]^=1;
    stack(0x0032B42Au,3,c,0x8000u,WORD_AT(c+0x8000u));
    pc_mapmaker_container_crc_check(fake_crc_check);
    CHECK(g_eax==0);
    host(c+0x12345)[0]^=1;
    /* the same file in a network session: refused after the stock size */
    network=1;
    CHECK(load()==7 && reads==1);
    network=0;
    /* the largest tier */
    make_map(c,MAPMAKER_CONTAINER_B-4u,3);seal(c);file_from(c,MAPMAKER_CONTAINER_B);
    CHECK(load()==1 && reads==5 && read_sizes[4]==MAPMAKER_CONTAINER_B);
    /* no file: the read error, once */
    file_present=0;
    CHECK(load()==8 && reads==1);
    /* network flags: info+8 set -> second read with |0x14 */
    make_map(c,0x1000u,4);host(c+16+8)[0]=1;seal(c);file_from(c,MAPMAKER_CONTAINER_STOCK);
    network=1;
    CHECK(load()==1 && reads==2 && read_flags[0]==2u && read_flags[1]==0x16u && read_sizes[1]==MAPMAKER_CONTAINER_STOCK);
    CHECK(WORD_AT(0x00762CBCu)==1);
    mode_827f0=0xAu;                                   /* flags 0xE: info+8 set, one read */
    make_map(c,0x1000u,4);seal(c);file_from(c,MAPMAKER_CONTAINER_STOCK);
    WORD_AT(0x00762CBCu)=0;
    CHECK(load()==1 && reads==1 && read_flags[0]==0xEu && host(c+16+8)[0]==1 && WORD_AT(0x00762CBCu)==1);
    mode_827f0=2;network=0;

    /* ---- save tiers: stock size for maps that fit ---- */
    make_map(c,0x1000u,5);CHECK(save_tier(c)==MAPMAKER_CONTAINER_STOCK);
    make_map(c,MAPMAKER_CONTAINER_STOCK-4u,5);CHECK(save_tier(c)==MAPMAKER_CONTAINER_STOCK);
    make_map(c,MAPMAKER_CONTAINER_STOCK,5);CHECK(save_tier(c)==0xFFE0u);
    make_map(c,0x10000u,5);CHECK(save_tier(c)==0x1FFE0u);
    make_map(c,MAPMAKER_CONTAINER_B-4u,5);CHECK(save_tier(c)==MAPMAKER_CONTAINER_B);
    WORD_AT(c+16+0x248)=MAPMAKER_CONTAINER_B;CHECK(save_tier(c)==MAPMAKER_CONTAINER_STOCK);
    make_map(c,0x18000u,5);
    stack(0x00190A55u,6,10u,0x0047DF80u,0x00600000u,c,MAPMAKER_CONTAINER_STOCK,2u);
    pc_mapmaker_container_save_sync(record_arg4);
    CHECK(seen_size==MAPMAKER_CONTAINER_STOCK);                 /* other callers untouched */
    stack(0x001909E3u,7,10u,0x0047DF80u,0x00600000u,0x1234u,c,MAPMAKER_CONTAINER_STOCK+20u,2u);
    pc_mapmaker_container_save_async(record_arg5);
    CHECK(seen_size==0x1FFE0u+20u);
    stack(0x00190A1Fu,4,c,MAPMAKER_CONTAINER_STOCK+20u,0x00600000u,2u);
    pc_mapmaker_container_save_sign(record_arg1);
    CHECK(seen_size==0x1FFE0u+20u);

    /* ---- save CRC past 64 KB: real CRC at used, caller's store a no-op ---- */
    make_map(c,0x18000u,6);
    memcpy(snap,host(c),0x18000u);
    stack(0x0032C724u,2,c,0x8000u);
    pc_mapmaker_container_crc(fake_crc);
    CHECK(WORD_AT(c+0x18000u)==crc32_of(c,0x18000u) && WORD_AT(g_esp+8)==0x8000u);
    WORD_AT(c+0x8000u)=g_eax;                                    /* the caller's store */
    CHECK(!memcmp(snap,host(c),0x18000u));
    /* the saved image loads back through the large tier */
    file_from(c,mapmaker_container_tier(c));
    memset(host(c),0,MAPMAKER_CONTAINER_N);
    CHECK(load()==1);
    stack(0x0032B42Au,3,c,0x8004u-4u,WORD_AT(c+0x8000u));
    pc_mapmaker_container_crc_check(fake_crc_check);
    CHECK(g_eax==1 && !memcmp(snap,host(c),0x18000u));
    /* small maps: the original, untouched */
    make_map(c,0x1000u,7);
    crc_calls=0;
    stack(0x0032C724u,2,c,0x1000u);
    pc_mapmaker_container_crc(fake_crc);
    CHECK(crc_calls==1 && g_eax==crc32_of(c,0x1000u));
    make_map(c,0x18000u,7);
    stack(0x00190950u,2,c,0x8000u);                              /* another caller */
    pc_mapmaker_container_crc(fake_crc);
    CHECK(g_eax==crc32_of(c,0x8000u));
    make_map(c,0x1000u,7);seal(c);
    stack(0x0032B42Au,3,c,0x1000u,WORD_AT(c+0x1000u));
    check_calls=0;
    pc_mapmaker_container_crc_check(fake_crc_check);
    CHECK(check_calls==1 && g_eax==1);
    make_map(c,0x18000u,7);WORD_AT(c+16+0x248)=MAPMAKER_CONTAINER_B;
    stack(0x0032B42Au,3,c,4u,0u);check_calls=0;
    pc_mapmaker_container_crc_check(fake_crc_check);
    CHECK(g_eax==0 && check_calls==0);                          /* past the buffer: fail */

    /* ---- a 64 KB multiple still saves (ax would be 0) ---- */
    compile_result=0x20000u;
    stack(0x0032C6FFu,2,c,0xBu);pc_mapmaker_container_compile(fake_compile);CHECK(g_eax==0x20004u);
    stack(0x0032C6F2u,1,c);pc_mapmaker_container_compiled_size(fake_compile);CHECK(g_eax==0x20004u);
    stack(0x00336689u,2,c,5u);pc_mapmaker_container_compile(fake_compile);CHECK(g_eax==0x20000u);
    compile_result=0x12344u;
    stack(0x0032C6FFu,2,c,0xBu);pc_mapmaker_container_compile(fake_compile);CHECK(g_eax==0x12344u);
    make_map(c,0x20000u,8);
    memcpy(snap,host(c),0x20000u);
    stack(0x0032C724u,2,c,4u);
    pc_mapmaker_container_crc(fake_crc);
    WORD_AT(c+4u)=g_eax;
    CHECK(WORD_AT(c+0x20000u)==crc32_of(c,0x20000u) && !memcmp(snap,host(c),0x20000u));

    /* ---- editor capacity: stock unless TSFP_MAPMAKER_LIMITS=1 ---- */
    make_map(c,0x7000u,9);info=c+16;find_result=info;
    for(i=0;i<4;i++) {
        static const uint32_t sites[4]={0x00300685u,0x003035D2u,0x00303927u,0x00304432u};
        stack(sites[i],2,c,0x00C77668u);
        pc_mapmaker_container_find(fake_find);
        CHECK(g_eax==info);
    }
    set_env("TSFP_MAPMAKER_LIMITS","1");
    stack(0x00303927u,2,c,0x00C77668u);
    pc_mapmaker_container_find(fake_find);
    CHECK(g_eax!=info && (int32_t)(WORD_AT(g_eax+0x248)+0x1000u)<(int32_t)MAPMAKER_CONTAINER_STOCK);
    WORD_AT(info+0x248)=MAPMAKER_CONTAINER_L-0x1Cu;               /* used + 0x1C == L: refused */
    stack(0x003035D2u,2,c,0x00C77668u);
    pc_mapmaker_container_find(fake_find);
    CHECK((int32_t)(WORD_AT(g_eax+0x248)+0x1Cu)>=(int32_t)MAPMAKER_CONTAINER_STOCK);
    WORD_AT(info+0x248)=MAPMAKER_CONTAINER_L-0x1Du;               /* one byte less: allowed */
    stack(0x003035D2u,2,c,0x00C77668u);
    pc_mapmaker_container_find(fake_find);
    CHECK((int32_t)(WORD_AT(g_eax+0x248)+0x1Cu)<(int32_t)MAPMAKER_CONTAINER_STOCK);
    CHECK(WORD_AT(info+0x248)==MAPMAKER_CONTAINER_L-0x1Du);       /* the map is not touched */
    stack(0x00301A4Du,2,c,0x00C77668u);                            /* compiler: real info */
    pc_mapmaker_container_find(fake_find);
    CHECK(g_eax==info);
    find_result=0;
    stack(0x00303927u,2,c,0x00C77668u);
    pc_mapmaker_container_find(fake_find);
    CHECK(g_eax==0);
    set_env("TSFP_MAPMAKER_POOLS","0");                            /* limits need the pools */
    find_result=info;
    stack(0x00303927u,2,c,0x00C77668u);
    pc_mapmaker_container_find(fake_find);
    CHECK(g_eax==info);
    set_env("TSFP_MAPMAKER_POOLS",NULL);set_env("TSFP_MAPMAKER_LIMITS",NULL);

    /* ---- System Link: the host never sends a map past the stock body ---- */
    make_map(c,0x1000u,10);
    stack(0x0035645Au,5,0x00774180u,0x30u,c,0x8000u,1u);sends=0;
    pc_mapmaker_container_net_send(fake_send);CHECK(sends==1);
    make_map(c,MAPMAKER_CONTAINER_STOCK-4u,10);
    stack(0x003563B9u,5,0x00774180u,0x30u,c,0x8000u,1u);
    pc_mapmaker_container_net_send(fake_send);CHECK(sends==2);
    make_map(c,MAPMAKER_CONTAINER_STOCK,10);
    stack(0x003563B9u,5,0x00774180u,0x30u,c,0x8000u,1u);
    pc_mapmaker_container_net_send(fake_send);CHECK(sends==2);
    stack(0x0035645Au,5,0x00774180u,0x30u,c,0x8000u,1u);
    pc_mapmaker_container_net_send(fake_send);CHECK(sends==2);
    stack(0x00356483u,5,0x00774180u,0x3Bu,0x00774040u,0x140u,1u);
    pc_mapmaker_container_net_send(fake_send);CHECK(sends==3);

    /* ---- undo ring: reused, zeroed, lists cleared ---- */
    WORD_AT(ring+16)=3;WORD_AT(0x00765C28u)=1;
    pc_mapmaker_container_undo_ring(undo_ring_original);
    CHECK(WORD_AT(0x00765C24u)==ring && WORD_AT(ring+16)==0 && WORD_AT(0x00765C28u)==0 && heap_allocs==2);

    if(failures) return 1;
    puts("mapmaker container ok");
    return 0;
}
