/* Asset-free contract for src/interface/pc_mapmaker_weaponset.inc (custom
 * weapon set slots 7-20). Fake guest memory holds a map container (map info,
 * piece chunk with weapon/ammo items, Story AI chunks), the editor's piece
 * list and nodes, the weapon slot pane and the play set 0x006F1E28; the fake
 * originals follow the stock code's reads and writes (see the doc) and pop
 * their return address like the lifted callees; every hook call checks esp. */
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
STUB(sub_0003DFA0,1) STUB(sub_00064E70,2) STUB(sub_00069640,3) STUB(sub_000BE860,4) STUB(sub_0006B2D0,5)
STUB(sub_00333700,6) STUB(sub_002FDF50,7) STUB(sub_00074460,8) STUB(sub_0003D7B0,9) STUB(sub_00071800,10)
STUB(sub_00079DE0,11) STUB(sub_0007D4A0,12) STUB(sub_000716D0,13) STUB(sub_00078590,14) STUB(sub_000783D0,15)
STUB(sub_00303350,16)

#define ESP0 0x00600000u
#define CONT 0x00100000u
#define SHADOWC 0x00180000u
#define EDITOR 0x00200000u
#define PIECES 0x00280000u
#define NODES 0x00290000u
#define PANE 0x00301000u
#define PROFILE 0x00302000u
#define PICKUP 0x00303000u
#define TEXTS 0x00700000u
#define FILTERED 0x2Au                  /* sub_00064E70 removes this weapon */

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static uint32_t calls[64][3],options[32][2],preloads[32],selected[2];
static unsigned ncalls,noptions,npreloads,nselect,originals;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},r=0;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00333700) r=1u;
    else if(f==sub_002FDF50) r=0u;
    else if(f==sub_00064E70) r=x[1]==FILTERED;
    else if(f==sub_00069640) r=PROFILE;
    else if(f==sub_000BE860) r=0x00720000u+x[0]*0x100u+x[1];
    else if(f==sub_0006B2D0) { if(npreloads<32u) preloads[npreloads]=x[0]; npreloads++; }
    else if(f==sub_00074460) { if(x[0]==PANE && x[2]==0u) { selected[0]=x[1];nselect++; } }
    else if(f==sub_0003D7B0) {
        r=TEXTS+(x[0]&0xFFFFu)*0x10u;
        snprintf((char *)((uintptr_t)r+g_xbox_mem_offset),16,"T%X",(unsigned)x[0]);
    }
    else if(f==sub_00303350) r=0x00710000u+x[0];
    else if(f==sub_00071800) { if(noptions<32u) { options[noptions][0]=x[0];options[noptions][1]=x[1]; } noptions++; }
    if(f==sub_00079DE0 || f==sub_0007D4A0 || f==sub_000716D0 || f==sub_00078590 || f==sub_000783D0 || f==sub_00071800) {
        if(ncalls<64u) {
            calls[ncalls][0]=f==sub_00079DE0?1u:f==sub_0007D4A0?2u:f==sub_000716D0?3u:f==sub_00078590?4u:f==sub_000783D0?5u:6u;
            calls[ncalls][1]=x[0];calls[ncalls][2]=x[1];
        }
        ncalls++;
    }
    return r;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_weaponset.inc"

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
/* One weapon/ammo/health item: id, rotation, cell x, slot byte. */
typedef struct { int id,rot,cell,slot; } Item;
typedef struct { int x,y,z,floor; int nitems; Item items[4]; } Piece;
static uint32_t info_at,piece_items[8][4],node_at[8][4];
static uint32_t used_now(uint32_t c) { return WORD_AT(c+0x10u+0x248u); }
/* header, map info (0x3DC), piece chunk, Story AI chunks with weapon slots, terminator */
static void container_at(uint32_t c,const Piece *ps,unsigned np,const int *sai,unsigned nsai,int index)
{
    uint32_t p,start;
    unsigned i,j;
    memset((void *)((uintptr_t)c+g_xbox_mem_offset),0,0x80010u);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4u)=0x12Du;
    WORD_AT(c+8u)=0x00C77668u;WORD_AT(c+0xCu)=0x3DCu;
    info_at=c+0x10u;WORD_AT(info_at+4u)=np;HALF_AT(info_at+0x40u)=(int16_t)index;
    BYTE_AT(info_at+0x42u)=6u;
    for(i=0;i<6u;i++) HALF_AT(info_at+0x44u+2u*i)=(int16_t)(10+i);
    p=c+0x3ECu;WORD_AT(p)=0x00C77669u;start=p+8u;p=start;
    for(i=0;i<np;i++) {
        WORD_AT(p)=3u;BYTE_AT(p+4u)=(uint8_t)ps[i].x;BYTE_AT(p+5u)=(uint8_t)ps[i].y;BYTE_AT(p+6u)=(uint8_t)ps[i].z;
        BYTE_AT(p+7u)=(uint8_t)ps[i].floor;HALF_AT(p+0x1Eu)=(int16_t)ps[i].nitems;
        p+=0x28u+8u;
        for(j=0;j<(unsigned)ps[i].nitems;j++) {
            const Item *it=&ps[i].items[j];
            HALF_AT(p+0xEu)=(int16_t)it->id;HALF_AT(p+0x10u)=(int16_t)it->rot;BYTE_AT(p+0x14u)=1u;
            BYTE_AT(p+0x15u)=(uint8_t)it->cell;BYTE_AT(p+0x16u)=1u;BYTE_AT(p+0x17u)=2u;BYTE_AT(p+0xCu)=(uint8_t)it->slot;
            piece_items[i][j]=p;p+=0x1Cu;
        }
    }
    WORD_AT(start-4u)=p-start;
    for(i=0;i<nsai;i++) {
        WORD_AT(p)=0x00C7766Cu;WORD_AT(p+4u)=0x78u;HALF_AT(p+8u+0x26u)=(int16_t)sai[i];p+=8u+0x78u;
    }
    WORD_AT(p)=0x00C7766Au;WORD_AT(p+4u)=p-c;
    WORD_AT(c+0x10u+0x248u)=p-c+8u;
}
#define container(ps,np,sai,nsai,index) container_at(CONT,ps,np,sai,nsai,index)
/* the editor's list, in reverse order of the container's pieces */
static void editor_list(const Piece *ps,unsigned np,const uint32_t *slots)
{
    unsigned i,j,s=0;
    uint32_t prev=0;
    memset((void *)((uintptr_t)PIECES+g_xbox_mem_offset),0,0x20000u);
    for(i=0;i<np;i++) {
        uint32_t piece=PIECES+i*0x100u,nprev=0;
        FLOAT_AT(piece)=(float)ps[i].x+0.75f;FLOAT_AT(piece+4u)=(float)ps[i].y;FLOAT_AT(piece+8u)=(float)ps[i].z+0.25f;
        BYTE_AT(piece+0xCu)=(uint8_t)ps[i].floor;WORD_AT(piece+0x40u)=prev;prev=piece;
        for(j=0;j<(unsigned)ps[i].nitems;j++) {
            uint32_t n=NODES+(i*4u+j)*0x40u;
            const Item *it=&ps[i].items[j];
            memset((void *)((uintptr_t)n+g_xbox_mem_offset),0,0x40u);
            WORD_AT(n)=(uint32_t)it->id;HALF_AT(n+8u)=(int16_t)it->rot;
            HALF_AT(n+0xCu)=(int16_t)it->cell;HALF_AT(n+0xEu)=1;HALF_AT(n+0x10u)=2;
            WORD_AT(n+0x30u)=slots?slots[s]:0u;s++;
            if(nprev) WORD_AT(nprev+0x14u)=n; else WORD_AT(piece+0x3Cu)=n;
            nprev=n;node_at[i][j]=n;
        }
    }
    WORD_AT(0x0078400Cu)=prev;
}
static uint32_t compile_used;
static void fake_compile(void) { originals++;g_eax=compile_used=used_now(CONT);g_esp+=4u; }
static const Piece map1[3]={
    {4,5,6,0,3,{{0x12,2,1,0},{0x12,2,1,0},{0x18,0,3,0}}},     /* two identical weapon items, ammo */
    {7,1,2,1,2,{{0x23,0,1,0},{0x12,4,2,0}}},                 /* health, weapon */
    {9,9,9,2,1,{{0x1C,6,5,0}}},
};
#define TAG(s) (0x57530000u|(s))
static const uint32_t map1_slots[7]={TAG(9),TAG(0),TAG(20),TAG(9),9u,TAG(14)};
static void clear_ed(void) { memset(ws.ed,0,sizeof ws.ed); }

static void compile_tests(void)
{
    static uint8_t before[0x80010];
    static const uint32_t own[7]={TAG(0),TAG(0),TAG(0),TAG(0),TAG(0),TAG(0)};
    uint32_t used0,data,term;
    unsigned k;
    int none[1]={5},high[2]={-1,9};
    /* a stock map: nothing appended, byte-identical */
    container(map1,3,none,1,0xF);editor_list(map1,3,own);clear_ed();used0=used_now(CONT);
    memcpy(before,(void *)((uintptr_t)CONT+g_xbox_mem_offset),sizeof before);
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x0032C6FFu,CONT,1u);
    CHECK(!memcmp(before,(void *)((uintptr_t)CONT+g_xbox_mem_offset),sizeof before));
    CHECK(g_eax==used0);
    /* slots 7-20 used: chunk before the terminator */
    container(map1,3,none,1,0xF);editor_list(map1,3,own);clear_ed();
    ws.ed[0]=0x21;ws.ed[5]=FILTERED;ws.ed[13]=0x30;used0=used_now(CONT);term=CONT+used0-8u;
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(WORD_AT(term)==0x00C776F6u && WORD_AT(term+4u)==0x20u);
    data=term+8u;
    CHECK(HALF_AT(data)==1 && BYTE_AT(data+2u)==6u && BYTE_AT(data+3u)==14u);
    for(k=0;k<14u;k++) CHECK(HALF_AT(data+4u+2u*k)==ws.ed[k]);
    CHECK(WORD_AT(term+0x28u)==0x00C7766Au && WORD_AT(term+0x2Cu)==term+0x28u-CONT);
    CHECK(used_now(CONT)==used0+0x28u && g_eax==used0+0x28u);
    CHECK(ws_chunk(CONT)==data && ws_walk(CONT,0,0,0)==term+0x28u);
    /* a Story AI on slot 10: written */
    container(map1,3,high,2,0xF);editor_list(map1,3,own);clear_ed();used0=used_now(CONT);
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(ws_chunk(CONT)!=0 && used_now(CONT)==used0+0x28u);
    /* items: slots from the nodes, paired by position whatever the list order */
    container(map1,3,none,1,0xF);editor_list(map1,3,map1_slots);clear_ed();used0=used_now(CONT);
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(BYTE_AT(piece_items[0][0]+0xCu)==9u && BYTE_AT(piece_items[0][1]+0xCu)==0u);   /* identical keys in order */
    CHECK(BYTE_AT(piece_items[0][2]+0xCu)==20u);                                    /* ammo */
    CHECK(BYTE_AT(piece_items[1][0]+0xCu)==0u);                                     /* health: not a weapon slot */
    CHECK(BYTE_AT(piece_items[1][1]+0xCu)==0u);                                     /* untagged node value */
    CHECK(BYTE_AT(piece_items[2][0]+0xCu)==14u);
    CHECK(ws_chunk(CONT)!=0 && used_now(CONT)==used0+0x28u);
    /* the second identical item carries the slot when only it has one */
    {
        static const uint32_t second[7]={TAG(0),TAG(11),TAG(0),TAG(0),TAG(0),TAG(0)};
        container(map1,3,none,1,0xF);editor_list(map1,3,second);clear_ed();
        CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
        CHECK(BYTE_AT(piece_items[0][0]+0xCu)==0u && BYTE_AT(piece_items[0][1]+0xCu)==11u);
    }
    /* a node moved elsewhere (no pair): its record keeps its own slot */
    container(map1,3,none,1,0xF);editor_list(map1,3,map1_slots);clear_ed();
    FLOAT_AT(PIECES+2u*0x100u)=55.0f;
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(BYTE_AT(piece_items[2][0]+0xCu)==0u && BYTE_AT(piece_items[0][0]+0xCu)==9u);
    /* the save compile keeps the container hook's low-word rule */
    container(map1,3,none,1,0xF);editor_list(map1,3,own);clear_ed();ws.ed[1]=5;
    {
        uint32_t t=CONT+WORD_AT(CONT+0x10u+0x248u)-8u,u=0x10000u-0x28u;
        WORD_AT(t)=0x00C77670u;WORD_AT(t+4u)=u-8u-t+CONT-8u;
        WORD_AT(CONT+u-8u)=0x00C7766Au;WORD_AT(CONT+u-4u)=u-8u;WORD_AT(CONT+0x10u+0x248u)=u;
    }
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x0032C6FFu,CONT,1u);
    CHECK(used_now(CONT)==0x10000u && g_eax==0x10004u);
    /* no room: nothing written, the item bytes cleared again, logged */
    container(map1,3,none,1,0xF);editor_list(map1,3,map1_slots);clear_ed();
    {
        uint32_t t=CONT+WORD_AT(CONT+0x10u+0x248u)-8u,u=0x7FFE0u-0x18u;
        WORD_AT(t)=0x00C77670u;WORD_AT(t+4u)=u-8u-t+CONT-8u;
        WORD_AT(CONT+u-8u)=0x00C7766Au;WORD_AT(CONT+u-4u)=u-8u;WORD_AT(CONT+0x10u+0x248u)=u;
    }
    used0=used_now(CONT);
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(used_now(CONT)==used0 && !ws_chunk(CONT) && g_eax==used0);
    CHECK(BYTE_AT(piece_items[0][0]+0xCu)==0u && BYTE_AT(piece_items[2][0]+0xCu)==0u);
    /* not a container: original only */
    WORD_AT(CONT)=0;originals=0;
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    CHECK(originals==1u);
}

static void load_tests(void)
{
    static const uint32_t junk[7]={9u,TAG(3),0x12345678u,TAG(9),TAG(9),77u};
    unsigned k;
    int none[1]={0};
    /* save with slots, then load into a junk editor */
    container(map1,3,none,1,0xF);editor_list(map1,3,map1_slots);clear_ed();ws.ed[2]=0x22;ws.ed[13]=0x31;
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    editor_list(map1,3,junk);clear_ed();ws.ed[4]=9;
    CALL(pc_mapmaker_weaponset_load,fake_plain,0x00301000u,CONT,0u);
    CHECK(ws.ed[2]==0x22 && ws.ed[13]==0x31 && ws.ed[4]==0);
    CHECK(WORD_AT(node_at[0][0]+0x30u)==TAG(9) && WORD_AT(node_at[0][1]+0x30u)==TAG(0));
    CHECK(WORD_AT(node_at[0][2]+0x30u)==TAG(20) && WORD_AT(node_at[2][0]+0x30u)==TAG(14));
    CHECK(WORD_AT(node_at[1][1]+0x30u)==TAG(0));                 /* stale value reset */
    CHECK(WORD_AT(node_at[1][0]+0x30u)==TAG(9));                 /* health node untouched */
    /* without the chunk: empty slots, own slots */
    container(map1,3,none,1,0xF);editor_list(map1,3,junk);ws.ed[3]=4;
    CALL(pc_mapmaker_weaponset_load,fake_plain,0x00301000u,CONT,0u);
    for(k=0;k<14u;k++) CHECK(ws.ed[k]==0);
    CHECK(WORD_AT(node_at[0][0]+0x30u)==TAG(0) && WORD_AT(node_at[2][0]+0x30u)==TAG(0));
    /* not a container: untouched */
    ws.ed[3]=4;WORD_AT(CONT)=0;
    CALL(pc_mapmaker_weaponset_load,fake_plain,0x00301000u,CONT,0u);
    CHECK(ws.ed[3]==4);
    /* Map Settings close: slots 7-20 count as a change against the undo shadow */
    container_at(SHADOWC,map1,3,none,1,0xF);editor_list(map1,3,0);clear_ed();ws.ed[6]=0x21;
    WORD_AT(0x00765E38u)=SHADOWC;
    {
        /* write the shadow's chunk through the compile hook */
        uint32_t sp=g_esp;
        g_esp-=12u;WORD_AT(g_esp)=0x00337677u;WORD_AT(g_esp+4u)=SHADOWC;WORD_AT(g_esp+8u)=1u;
        compile_used=used_now(SHADOWC);
        pc_mapmaker_weaponset_compile(fake_plain);
        g_esp=sp;
    }
    CHECK(ws_chunk(SHADOWC)!=0);
    fake_answer=0;CALL(pc_mapmaker_weaponset_changed,fake_answer_fn,0x00313358u,0u,0u);CHECK(g_eax==0u);
    ws.ed[6]=0x22;CALL(pc_mapmaker_weaponset_changed,fake_answer_fn,0x00313358u,0u,0u);CHECK(g_eax==1u);
    ws.ed[6]=0x21;fake_answer=1;CALL(pc_mapmaker_weaponset_changed,fake_answer_fn,0x00313358u,0u,0u);CHECK(g_eax==1u);
    fake_answer=0;container_at(SHADOWC,map1,3,none,1,0xF);       /* shadow without the chunk */
    CALL(pc_mapmaker_weaponset_changed,fake_answer_fn,0x00313358u,0u,0u);CHECK(g_eax==1u);
    clear_ed();CALL(pc_mapmaker_weaponset_changed,fake_answer_fn,0x00313358u,0u,0u);CHECK(g_eax==0u);
    /* new map */
    ws.ed[9]=7;ws.page=2;
    CALL(pc_mapmaker_weaponset_reset,fake_plain,0x00302000u,0u,0u);
    CHECK(ws.ed[9]==0 && ws.page==0);
    /* held item defaults: a weapon/ammo pickup starts on its own slot */
    WORD_AT(EDITOR+0x2F34u)=0x15u;WORD_AT(EDITOR+0x2F64u)=TAG(12);
    CALL(pc_mapmaker_weaponset_held,fake_plain,0x00316FE5u,0u,0u);
    CHECK(WORD_AT(EDITOR+0x2F64u)==TAG(0));
    WORD_AT(EDITOR+0x2F34u)=0x35u;WORD_AT(EDITOR+0x2F64u)=1u;
    CALL(pc_mapmaker_weaponset_held,fake_plain,0x00316FE5u,0u,0u);
    CHECK(WORD_AT(EDITOR+0x2F64u)==1u);
}

/* ---- weapon set screen ------------------------------------------------- */
static uint32_t got;
static void fake_get(void) { originals++;g_eax=0x99u;g_esp+=4u; }
static void fake_set(void) { originals++;got=WORD_AT(g_esp+4u)<<16|WORD_AT(g_esp+8u);g_esp+=4u; }
static void fake_title(void) { originals++;g_eax=TEXTS+0x0E86u*0x10u;strcpy((char *)((uintptr_t)g_eax+g_xbox_mem_offset),"Weapons");g_esp+=4u; }
static const char *title_text(void) { return (const char *)((uintptr_t)WS_TITLE+g_xbox_mem_offset); }
static void frame(int32_t value,uint32_t input)
{
    WORD_AT(PANE+0x18Cu)=(uint32_t)value;WORD_AT(PANE+0xC4u)=input;WORD_AT(PANE+0xC8u)=input&0xFu;
    CALL(pc_mapmaker_weaponset_screen,fake_plain,0x0007AD7Bu,PANE,0u);
}
static uint32_t row(uint32_t r)
{
    uint32_t sp=g_esp;
    g_esp-=8u;WORD_AT(g_esp)=0x002D3D8Cu;WORD_AT(g_esp+4u)=r;
    pc_mapmaker_weaponset_get(fake_get);
    if(g_esp!=sp-4u) { fprintf(stderr,"row esp\n");failures++; }
    g_esp=sp;
    return g_eax;
}
static void screen_tests(void)
{
    clear_ed();
    for(unsigned k=0;k<14u;k++) ws.ed[k]=(int16_t)(0x20+k);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784028u)=0xFu;
    CALL(pc_mapmaker_weaponset_build,fake_plain,0x0007A000u,0x00300000u,0u);
    CHECK(ws.page==0);
    CALL(pc_mapmaker_weaponset_title,fake_title,0x002D4254u,0xE86u,0u);
    CHECK(g_eax==WS_TITLE && !strcmp(title_text(),"Weapons  (1-6)"));
    CALL(pc_mapmaker_weaponset_title,fake_title,0x002D4259u,0xE86u,0u);
    CHECK(g_eax!=WS_TITLE);
    /* page 1 rows are stock */
    originals=0;CHECK(row(2)==0x99u && originals==1u);
    /* Down on row 6 -> slots 7-12, cursor on row 1 */
    nselect=0;frame(5,0x00020002u);
    CHECK(ws.page==1 && nselect==1u && selected[0]==0u && WORD_AT(PANE+0x18Cu)==0u && (WORD_AT(PANE+0xC4u)&0x000F000Fu)==0u);
    CHECK(!strcmp(title_text(),"Weapons  (7-12)"));
    CHECK(row(0)==0x20u && row(5)==0x25u);
    /* a pick and an X clear land in slots 7-12 */
    CALL(pc_mapmaker_weaponset_set,fake_set,0x002BF95Au,2u,0x33u);
    CHECK(ws.ed[2]==0x33);
    CALL(pc_mapmaker_weaponset_set,fake_set,0x002D41C7u,4u,0u);
    CHECK(ws.ed[4]==0);
    got=0;CALL(pc_mapmaker_weaponset_set,fake_set,0x002BF95Bu,1u,7u);   /* other caller: stock */
    CHECK(got==(1u<<16|7u) && ws.ed[1]==0x21);
    /* pages 3 and 4 (15-20), no page 5 */
    frame(5,0x00020002u);CHECK(ws.page==2 && row(0)==0x26u && !strcmp(title_text(),"Weapons  (13-18)"));
    frame(5,0x00020002u);CHECK(ws.page==3 && row(0)==0x28u && row(5)==0x2Du && !strcmp(title_text(),"Weapons  (15-20)"));
    CALL(pc_mapmaker_weaponset_set,fake_set,0x002BF95Au,5u,0x3Fu);CHECK(ws.ed[13]==0x3F);
    nselect=0;frame(5,0x00020002u);CHECK(ws.page==3 && nselect==0u && WORD_AT(PANE+0xC4u)==0x00020002u);
    /* the selector row and middle rows: stock */
    frame(-5,0x00020002u);frame(3,0x00010001u);CHECK(ws.page==3 && WORD_AT(PANE+0xC4u)==0x00010001u);
    /* Up on row 1 -> back, cursor on row 6 */
    frame(0,0x00010001u);CHECK(ws.page==2 && selected[0]==5u && WORD_AT(PANE+0x18Cu)==5u);
    frame(0,0x00010001u);frame(0,0x00010001u);CHECK(ws.page==0 && !strcmp(title_text(),"Weapons  (1-6)"));
    frame(0,0x00010001u);CHECK(ws.page==0 && WORD_AT(PANE+0xC4u)==0x00010001u);
    originals=0;CHECK(row(1)==0x99u && originals==1u);
    /* reopening resets */
    frame(5,0x00020002u);CHECK(ws.page==1);
    CALL(pc_mapmaker_weaponset_build,fake_plain,0x0007A000u,0x00300000u,0u);CHECK(ws.page==0);
    /* a preset set: no paging */
    WORD_AT(0x00784028u)=3u;frame(5,0x00020002u);CHECK(ws.page==0 && WORD_AT(PANE+0xC4u)==0x00020002u);
    CALL(pc_mapmaker_weaponset_title,fake_title,0x002D4254u,0xE86u,0u);CHECK(g_eax!=WS_TITLE);
    WORD_AT(0x00784028u)=0xFu;
    /* switch off: no paging */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    frame(5,0x00020002u);CHECK(ws.page==0 && WORD_AT(PANE+0xC4u)==0x00020002u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* outside the editor: stock */
    frame(5,0x00020002u);CHECK(ws.page==1);
    WORD_AT(0x0079094Cu)=101u;originals=0;
    CHECK(row(0)==0x99u && originals==1u);
    frame(0,0x00010001u);CHECK(ws.page==0);
    WORD_AT(0x0079094Cu)=102u;
}

/* ---- slot choices ------------------------------------------------------ */
static void choice_tests(void)
{
    uint32_t value=0x00305000u,n=NODES+0x800u;
    unsigned k;
    char want[48];
    clear_ed();ws.ed[0]=0x21;ws.ed[13]=0x30;
    for(k=0;k<0x46u;k++) WORD_AT(0x004FB190u+k*0x54u)=0x2000u+k;
    WORD_AT(0x0079094Cu)=102u;
    /* Story AI weapon choice: 14 more options, values 6..19 */
    noptions=0;ncalls=0;originals=0;WORD_AT(value)=2u;
    CALL(pc_mapmaker_weaponset_sai_choice,fake_plain,0x0031CDB7u,value,0u);
    CHECK(originals==1u && noptions==14u);
    for(k=0;k<14u;k++) {
        CHECK(options[k][1]==6u+k && options[k][0]==WS_TEXT+k*0x30u);
        snprintf(want,sizeof want,"%u: T%X",7u+k,(unsigned)(0x2000u+(uint32_t)ws.ed[k]));
        CHECK(!strcmp((const char *)((uintptr_t)options[k][0]+g_xbox_mem_offset),want));
    }
    noptions=0;CALL(pc_mapmaker_weaponset_sai_choice,fake_plain,0x0031CD32u,value,0u);CHECK(noptions==0u);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    noptions=0;CALL(pc_mapmaker_weaponset_sai_choice,fake_plain,0x0031CDB7u,value,0u);CHECK(noptions==0u);
    WORD_AT(value)=11u;                                  /* a map made with the switch on */
    noptions=0;CALL(pc_mapmaker_weaponset_sai_choice,fake_plain,0x0031CDB7u,value,0u);CHECK(noptions==14u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
    /* pickup options: a slot choice for weapon/ammo pickups */
    memset((void *)((uintptr_t)n+g_xbox_mem_offset),0,0x40u);
    WORD_AT(n)=0x13u;WORD_AT(n+0x30u)=0xDEADBEEFu;WORD_AT(EDITOR+0x2F1Cu)=n;
    noptions=0;ncalls=0;originals=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193A2u,0u,0u);
    CHECK(originals==1u && WORD_AT(n+0x30u)==TAG(0) && noptions==15u);
    CHECK(calls[0][0]==1u && calls[0][1]==TEXTS+0xD76u*0x10u && calls[0][2]==1u);
    CHECK(calls[1][0]==2u && calls[2][0]==3u && calls[2][1]==0xB9u);
    CHECK(options[0][0]==0x00710000u+0x13u && options[0][1]==TAG(0));
    for(k=1;k<15u;k++) CHECK(options[k][1]==TAG(6u+k) && options[k][0]==WS_TEXT+(k-1u)*0x30u);
    CHECK(ncalls==20u && calls[18][0]==4u && calls[18][1]==n+0x30u && calls[19][0]==5u);
    WORD_AT(n+0x30u)=TAG(12);noptions=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193A2u,0u,0u);
    CHECK(WORD_AT(n+0x30u)==TAG(12) && noptions==15u);
    /* other items, other callers, switch off: stock */
    WORD_AT(n)=0x23u;noptions=0;originals=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193A2u,0u,0u);CHECK(noptions==0u && originals==1u);
    WORD_AT(n)=0x13u;noptions=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193DBu,0u,0u);CHECK(noptions==0u);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    WORD_AT(n+0x30u)=TAG(0);noptions=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193A2u,0u,0u);CHECK(noptions==0u);
    WORD_AT(n+0x30u)=TAG(8);noptions=0;
    CALL(pc_mapmaker_weaponset_options,fake_plain,0x003193A2u,0u,0u);CHECK(noptions==15u);
    set_env("TSFP_MAPMAKER_LIMITS","1");
}

/* ---- play -------------------------------------------------------------- */
static uint32_t stock_count;
static void fake_play_set(void)
{
    unsigned k;
    originals++;
    WORD_AT(0x006F1E28u)=0x425u;WORD_AT(0x006F1E2Cu)=stock_count;
    for(k=0;k<6u;k++) WORD_AT(0x006F1E30u+4u*k)=k==3u?0u:0x10u+k;
    WORD_AT(0x006F1E30u+4u*6u)=0x38u;
    g_eax=0x006F1E28u;g_esp+=4u;
}
static void fake_other_set(void) { originals++;g_eax=0x00518F28u;g_esp+=4u; }
static uint32_t play_set(uint32_t ret,void (*orig)(void))
{
    uint32_t sp=g_esp;
    g_esp-=4u;WORD_AT(g_esp)=ret;
    pc_mapmaker_weaponset_play_set(orig);
    if(g_esp!=sp) { fprintf(stderr,"play set esp\n");failures++; }
    g_esp=sp;
    return g_eax;
}
static uint32_t id_at(uint32_t set,unsigned k) { return WORD_AT(set+8u+4u*k); }
static void play_map(int index)
{
    static const Piece pm[2]={{1,1,1,0,2,{{0x12,0,0,9},{0x18,0,1,16}}},{2,2,2,0,2,{{0x13,0,0,0},{0x16,0,1,3}}}};
    int none[1]={8};
    container(pm,2,none,1,index);editor_list(pm,2,0);clear_ed();
    ws.ed[2]=0x21;ws.ed[5]=FILTERED;ws.ed[9]=0x22;ws.ed[13]=0x30;
    WORD_AT(0x0079094Cu)=102u;
    CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(0x0079094Cu)=100u;ws.valid=0;
}
static void play_tests(void)
{
    uint32_t s,k;
    for(k=0;k<0x46u;k++) { WORD_AT(0x004FB194u+k*0x54u)=k==0x22u?0xFFFFFFFFu:0x100u+k;WORD_AT(0x004FA138u+k*0x3Cu)=k; }
    WORD_AT(0x004F9BACu)=0x00400000u;
    for(k=0;k<0x46u;k++) WORD_AT(0x00400000u+k*0x29Cu+0x14u)=k==0x30u?0u:1u;
    WORD_AT(0x00761404u)=0;BYTE_AT(0x007DE456u)=4u;
    play_map(0xF);
    /* the editor test's set: 20 slots, filtered, the appended 0x38 after them */
    stock_count=7u;s=play_set(0x0006BF34u,fake_play_set);
    CHECK(s==WS_BIGSET && WORD_AT(s)==0x425u && WORD_AT(s+4u)==21u);
    for(k=0;k<6u;k++) CHECK(id_at(s,k)==(k==3u?0u:0x10u+k));
    CHECK(id_at(s,8)==0x21u && id_at(s,11)==0u && id_at(s,15)==0x22u && id_at(s,19)==0x30u && id_at(s,6)==0u);
    CHECK(id_at(s,20)==0x38u);
    stock_count=6u;s=play_set(0x0006BF34u,fake_play_set);CHECK(WORD_AT(s+4u)==20u);
    /* the AI random pick: used ids only */
    s=play_set(0x002B50D5u,fake_play_set);
    CHECK(s==WS_COMPACT && WORD_AT(s+4u)==8u && id_at(s,0)==0x10u && id_at(s,2)==0x12u && id_at(s,4)==0x15u && id_at(s,5)==0x21u && id_at(s,7)==0x30u);
    /* another set: stock */
    CHECK(play_set(0x0006BF34u,fake_other_set)==0x00518F28u);
    /* slot lookups for the Story AI fill and the vetoes */
    g_eax=0;CALL(pc_mapmaker_weaponset_slot,fake_answer_fn,0x00242064u,8u,0u);CHECK(g_eax==0x21u);
    CALL(pc_mapmaker_weaponset_slot,fake_answer_fn,0x00242064u,11u,0u);CHECK(g_eax==(uint32_t)FILTERED);  /* raw, as stock */
    fake_answer=0x77u;originals=0;
    CALL(pc_mapmaker_weaponset_slot,fake_answer_fn,0x00242064u,3u,0u);CHECK(g_eax==0x77u && originals==1u);
    CALL(pc_mapmaker_weaponset_slot,fake_answer_fn,0x00242064u,20u,0u);CHECK(g_eax==0x77u);
    /* vetoes follow the record's slot */
    {
        static const Piece pv[1]={{1,1,1,0,4,{{0x12,0,0,9},{0x12,0,1,16},{0x18,0,2,20},{0x19,0,3,9}}}};
        int none[1]={0};
        uint32_t w9,w16,a20,a9,p;
        container(pv,1,none,1,0xF);
        p=ws_chunk(WORD_AT(0x00784014u));(void)p;
        w9=piece_items[0][0];w16=piece_items[0][1];a20=piece_items[0][2];a9=piece_items[0][3];
        /* re-add the chunk of play_map's slots: compile into this container */
        WORD_AT(0x0079094Cu)=102u;editor_list(pv,1,0);
        CALL(pc_mapmaker_weaponset_compile,fake_compile,0x00337677u,CONT,1u);
        BYTE_AT(w9+0xCu)=9u;BYTE_AT(w16+0xCu)=16u;BYTE_AT(a20+0xCu)=20u;BYTE_AT(a9+0xCu)=9u;
        ws.ed[9]=0x22;ws.valid=0;WORD_AT(0x0079094Cu)=100u;
        CHECK(ws_play() && ws.ids[2]==0x21);
        fake_answer=0x55u;
        CALL(pc_mapmaker_weaponset_weapon_veto,fake_answer_fn,0x002406F0u,w9,0u);CHECK(g_eax==1u);
        CALL(pc_mapmaker_weaponset_weapon_veto,fake_answer_fn,0x002406F0u,w16,0u);CHECK(g_eax==0u);   /* no pickup model */
        CALL(pc_mapmaker_weaponset_ammo_veto,fake_answer_fn,0x002406F0u,a20,0u);CHECK(g_eax==0u);    /* no ammo */
        CALL(pc_mapmaker_weaponset_ammo_veto,fake_answer_fn,0x002406F0u,a9,0u);CHECK(g_eax==1u);
        CALL(pc_mapmaker_weaponset_ammo_veto,fake_answer_fn,0x002406F0u,w9,0u);CHECK(g_eax==0x55u);  /* weapon item: original */
        BYTE_AT(w9+0xCu)=0u;
        CALL(pc_mapmaker_weaponset_weapon_veto,fake_answer_fn,0x002406F0u,w9,0u);CHECK(g_eax==0x55u);
        BYTE_AT(w9+0xCu)=9u;
        /* the pickup takes slot - 1 */
        memset((void *)((uintptr_t)PICKUP+g_xbox_mem_offset),0,0x58u);HALF_AT(PICKUP+0x10u)=1;
        CALL(pc_mapmaker_weaponset_pickup,fake_plain,0x002414D1u,PICKUP,w9);CHECK(HALF_AT(PICKUP+0x10u)==8);
        HALF_AT(PICKUP+0x10u)=1;
        CALL(pc_mapmaker_weaponset_pickup,fake_plain,0x002414D1u,PICKUP,a20);CHECK(HALF_AT(PICKUP+0x10u)==19);
        HALF_AT(PICKUP+0x10u)=1;BYTE_AT(w9+0xCu)=0u;
        CALL(pc_mapmaker_weaponset_pickup,fake_plain,0x002414D1u,PICKUP,w9);CHECK(HALF_AT(PICKUP+0x10u)==1);
        BYTE_AT(w9+0xCu)=9u;
        /* preload: the extra-slot pickups' weapons */
        npreloads=0;originals=0;
        CALL(pc_mapmaker_weaponset_preload,fake_plain,0x0006C963u,0u,0u);
        CHECK(originals==1u && npreloads==4u && preloads[0]==0x00720000u+0x200u+0x21u &&
              preloads[1]==0x00720000u+0x200u+0x22u && preloads[2]==0x00724000u+0x30u && preloads[3]==0x00724000u+0x21u);
        /* inactive set: vetoes refuse, no preload */
        HALF_AT(WORD_AT(0x00784014u)+0x10u+0x40u)=3;ws.valid=0;
        CALL(pc_mapmaker_weaponset_weapon_veto,fake_answer_fn,0x002406F0u,w9,0u);CHECK(g_eax==0u);
        npreloads=0;CALL(pc_mapmaker_weaponset_preload,fake_plain,0x0006C963u,0u,0u);CHECK(npreloads==0u);
        CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
        HALF_AT(WORD_AT(0x00784014u)+0x10u+0x40u)=0xF;ws.valid=0;
    }
    /* front end: the Arcade setup's choice decides */
    play_map(0xF);
    WORD_AT(0x00761404u)=1u;WORD_AT(0x0078AD2Cu)=9u;BYTE_AT(0x007DE455u)=3u;
    WORD_AT(PROFILE+0x6A4u)=0xFu;CHECK(play_set(0x0006BF34u,fake_play_set)==WS_BIGSET);
    WORD_AT(PROFILE+0x6A4u)=2u;CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
    WORD_AT(0x0078AD2Cu)=6u;CHECK(play_set(0x0006BF34u,fake_play_set)==WS_BIGSET);       /* other types: the map's */
    HALF_AT(info_at+0x40u)=0xC;ws.valid=0;CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
    WORD_AT(0x00761404u)=0;CHECK(play_set(0x0006BF34u,fake_play_set)==WS_BIGSET);         /* editor test: >= 10 */
    /* no chunk / not in play: stock */
    {
        int none[1]={0};
        container(map1,3,none,1,0xF);ws.valid=0;
        CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
        play_map(0xF);WORD_AT(0x0079094Cu)=102u;
        CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u && ws.valid==0);
        /* the next level: the cache follows the scene change and the preload */
        play_map(0xF);CHECK(play_set(0x0006BF34u,fake_play_set)==WS_BIGSET);
        WORD_AT(0x0079094Cu)=101u;CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
        container(map1,3,none,1,0xF);WORD_AT(0x0079094Cu)=100u;
        CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
        play_map(0xF);CHECK(play_set(0x0006BF34u,fake_play_set)==WS_BIGSET);
        container(map1,3,none,1,0xF);
        CALL(pc_mapmaker_weaponset_preload,fake_plain,0x0006C963u,0u,0u);
        CHECK(play_set(0x0006BF34u,fake_play_set)==0x006F1E28u);
    }
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
    screen_tests();
    choice_tests();
    play_tests();
    if(failures) { fprintf(stderr,"%d failure(s)\n",failures);return 1; }
    printf("mapmaker weaponset ok\n");
    return 0;
}
