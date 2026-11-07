/* Asset-free contract for the word draw buckets of src/interface/pc_mapmaker_rooms.inc
 * (stage 6 of rooms past 255): pc_mapmaker_rooms_buckets (sub_00086C40),
 * pc_mapmaker_rooms_bucket_draw (sub_00086480 / sub_000865E0) and
 * pc_mapmaker_rooms_view_lists (sub_000897D0).
 *
 * The stock functions are modelled from text.asm with their byte tables
 * (rows of byte room ids, byte counts, 10 sorted rooms per depth), the patched
 * readers with rooms.json's word reads from the staging row. For random views
 * of up to 255 rooms, the hooks give exactly the stock draw sequence (rooms,
 * order, reader calls, the other calls and their arguments) and the stock room
 * lists; past 255 (rooms above 0xFF, more than 255 in one depth) every room is
 * drawn once with its full id and the guest stack stays balanced. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_esp, g_ebx;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
static uint16_t u16(uint32_t a) { return *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void set16(uint32_t a,uint16_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=v; }
static uint8_t *u8(uint32_t a) { return (uint8_t *)((uintptr_t)a+g_xbox_mem_offset); }

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* ---- event log: what the guest would see ---- */
typedef struct { char kind; uint32_t a,b,c,d; } event;
#define EVENTS 40000
static event got[EVENTS],want[EVENTS];
static unsigned got_n,want_n;
static void log_to(event *e,unsigned *n,char kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    if(*n<EVENTS) { e[*n].kind=kind;e[*n].a=a;e[*n].b=b;e[*n].c=c;e[*n].d=d; }
    (*n)++;
}
#define GOT(k,a,b,c,d) log_to(got,&got_n,(k),(a),(b),(c),(d))
#define WANT(k,a,b,c,d) log_to(want,&want_n,(k),(a),(b),(c),(d))

#define VIEWS 0x00400000u
#define RECORDS 0x00200000u
#define STACK 0x00100000u
#define VIEWPORT 0x00300000u
#define RET 0x001CED27u
#define RECT(view,r) ((view)+MAPMAKER_ROOMS_VIEW_RECTS+12u*(r))

void sub_00157440(void) {}
void sub_00157970(void) {}
void sub_0003DFA0(void) {}
void sub_0001A010(void) { GOT('A',WORD_AT(g_esp+4),WORD_AT(g_esp+8),WORD_AT(g_esp+12),WORD_AT(g_esp+16));g_esp+=4u; }
void sub_000A79B0(void) { GOT('G',WORD_AT(g_esp+4),0,0,0);g_esp+=4u; }
void sub_003442B0(void) { GOT('P',0,0,0,0);g_esp+=4u; }
void sub_00344350(void) { GOT('Q',0,0,0,0);g_esp+=4u; }
void sub_00094250(void) { GOT('L',0,0,0,0);g_esp+=4u; }   /* room light spread callees (not reached here) */
void sub_00094290(void) { GOT('M',0,0,0,0);g_esp+=4u; }
void sub_00356800(void) { GOT('N',0,0,0,0);g_esp+=4u; }
void sub_00086AE0(void);
/* sub_00087720(room, out, max): the view room's neighbours (fixed per room). */
static uint32_t neighbours[16],neighbour_n;
void sub_00087720(void)
{
    uint32_t room=WORD_AT(g_esp+4),out=WORD_AT(g_esp+8),max=WORD_AT(g_esp+12),i;
    GOT('N',room,max,0,0);
    for(i=0;i<neighbour_n && i<max;i++) WORD_AT(out+4u*i)=neighbours[i];
    g_eax=i;
    g_esp+=4u;
}
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t args[8],esp=g_esp;
    int i;
    va_start(a,n);
    for(i=0;i<n;i++) args[i]=va_arg(a,uint32_t);
    va_end(a);
    for(i=n;i>0;i--) { g_esp-=4u;WORD_AT(g_esp)=args[i-1]; }
    g_esp-=4u;WORD_AT(g_esp)=0xCA11CA11u;
    f();
    if(g_esp!=esp-4u*(uint32_t)n) { fprintf(stderr,"guest stack unbalanced in a call\n");failures++; }
    g_esp=esp;
    return g_eax;
}
#include "pc_mapmaker_rooms.inc"

/* ---- the patched readers (rooms.json): count byte [0x702DB0+b], words from the staging row ---- */
static uint32_t reader_esp_bad;
static void reader(int which)
{
    uint32_t b=g_eax,count=*u8(0x00702DB0u+b),row=MAPMAKER_ROOMS_BUCKET_ROW+b*0u,i;
    GOT('C',(uint32_t)which,b,0,0);
    for(i=0;i<count;i++) GOT('R',(uint32_t)which,b,u16(row+2u*i),0);
    if(WORD_AT(g_esp)!=0x0008683Eu) reader_esp_bad++;
    g_esp+=4u;
}
static void reader_a(void) { reader(0); }
static void reader_b(void) { reader(1); }
static void call_reader(int which,int32_t b)
{
    uint32_t esp=g_esp;
    g_esp-=4u;WORD_AT(g_esp)=0x0008683Eu;
    g_eax=(uint32_t)b;
    pc_mapmaker_rooms_bucket_draw(which?reader_b:reader_a);
    if(g_esp!=esp) { fprintf(stderr,"reader hook left esp at %+d\n",(int)(g_esp-esp));failures++;g_esp=esp; }
}
/* sub_00086AE0(deepest) -> sub_000867F0(eax = deepest): the reader order (text.asm 0x0008682D..0x0008692F). */
void sub_00086AE0(void)
{
    int32_t esi=(int32_t)WORD_AT(g_esp+4),edi;
    uint32_t view=WORD_AT(0x007B0CC0u);
    GOT('D',(uint32_t)esi,WORD_AT(view+0xD28u),0,0);
    for(edi=esi+1;edi<=(int32_t)WORD_AT(view+0xD28u);edi++) call_reader(0,edi);
    for(edi=(int32_t)WORD_AT(view+0xD28u);edi>esi;edi--) call_reader(1,edi);
    for(edi=0;edi<=esi;edi++) call_reader(0,edi);
    for(edi=esi;edi>=0;edi--) call_reader(1,edi);
    g_esp+=4u;
}

/* ---- stock sub_00086C40 with its byte tables (text.asm 0x00086C40..0x00086E0E) ---- */
static void stock_draw(int32_t esi,int32_t deepest_now,uint8_t counts[],uint8_t rows[][256])
{
    int32_t edi;
    WANT('D',(uint32_t)esi,(uint32_t)deepest_now,0,0);
#define STOCK_READ(w,b) do { uint32_t i_; WANT('C',(w),(uint32_t)(b),0,0); \
        for(i_=0;i_<counts[(b)];i_++) WANT('R',(w),(uint32_t)(b),rows[(b)][i_],0); } while(0)
    for(edi=esi+1;edi<=deepest_now;edi++) STOCK_READ(0u,edi);
    for(edi=deepest_now;edi>esi;edi--) STOCK_READ(1u,edi);
    for(edi=0;edi<=esi;edi++) STOCK_READ(0u,edi);
    for(edi=esi;edi>=0;edi--) STOCK_READ(1u,edi);
}
static void stock_buckets(uint32_t view)
{
    static uint8_t counts[256],rows[256][256],scount[256],srows[256][10];
    uint32_t rooms=WORD_AT(0x007B975Cu),edx;
    int32_t ebx=(int32_t)WORD_AT(view+0xD28u),flag=0,d28=ebx;
    uint32_t vp=WORD_AT(0x007E71C8u);
    memset(counts,0,0x50);memset(scount,0,0x50);
    for(edx=1;edx<rooms+1u;edx++) {
        int32_t b=(int32_t)WORD_AT(RECT(view,edx));
        if(b<0) continue;
        if(*u8(WORD_AT(0x00708788u)+edx*0xB4u+0xB2u)&8u) {
            flag=1;
            if(scount[b]>=10) continue;
            srows[b][scount[b]++]=(uint8_t)edx;
        } else {
            rows[b][counts[b]]=(uint8_t)edx;counts[b]++;
        }
    }
    if(flag && ebx>=0) {
        int32_t k;
        for(k=0;k<=ebx;k++) {
            if(!scount[k]) continue;
            d28++;
            counts[d28]=scount[k];
            memcpy(rows[d28],srows[k],scount[k]);
        }
    }
    if(d28>=0x50) d28=0x4F;
    WANT('A',WORD_AT(vp),WORD_AT(vp+0x14u),WORD_AT(vp+4u),WORD_AT(vp+0x18u));
    WANT('G',MAPMAKER_ROOMS_ID_GLOBAL,0,0,0);
    WANT('P',0,0,0,0);
    stock_draw(ebx,d28,counts,rows);
    WANT('Q',0,0,0,0);
}

static uint32_t rng=12345u;
static uint32_t rnd(uint32_t n) { rng=rng*1103515245u+12345u;return (rng>>8)%n; }

static void setup_view(uint32_t view,uint32_t rooms,int32_t deepest,int sorted_pct,int depth_range)
{
    uint32_t r;
    WORD_AT(view+0xD28u)=(uint32_t)deepest;
    WORD_AT(view+0x214u)=rnd(2);
    for(r=0;r<=rooms+1u;r++) {
        int32_t d=rnd(5)==0?-1:(int32_t)rnd((uint32_t)depth_range);
        WORD_AT(RECT(view,r))=(uint32_t)d;
        set16(RECT(view,r)+4u,(uint16_t)(int16_t)((int)rnd(700)-60));    /* x0 */
        set16(RECT(view,r)+8u,(uint16_t)(int16_t)((int)rnd(700)-60));    /* x1 */
        *u8(RECORDS+r*0xB4u+0xB2u)=(uint8_t)((int)rnd(100)<sorted_pct?8u:0u)|(uint8_t)(rnd(2)?0x10u:0u);
    }
}
static int same_events(const char *what)
{
    unsigned i,n=got_n<want_n?got_n:want_n;
    if(got_n>EVENTS || want_n>EVENTS) { fprintf(stderr,"%s: event log overflow\n",what);return 0; }
    for(i=0;i<n;i++)
        if(got[i].kind!=want[i].kind || got[i].a!=want[i].a || got[i].b!=want[i].b ||
           got[i].c!=want[i].c || got[i].d!=want[i].d) {
            fprintf(stderr,"%s: event %u: got %c %X %X %X %X, want %c %X %X %X %X\n",what,i,
                    got[i].kind,got[i].a,got[i].b,got[i].c,got[i].d,want[i].kind,want[i].a,want[i].b,want[i].c,want[i].d);
            return 0;
        }
    if(got_n!=want_n) { fprintf(stderr,"%s: %u events, want %u\n",what,got_n,want_n);return 0; }
    return 1;
}
static void run_buckets(uint32_t view)
{
    g_esp=STACK;WORD_AT(g_esp)=RET;
    WORD_AT(0x007B0CC0u)=view;
    pc_mapmaker_rooms_buckets(NULL);
    CHECK(g_esp==STACK+4u);
}

static void test_buckets_match_stock(void)
{
    int round,bad=0;
    uint32_t view=VIEWS+2u*MAPMAKER_ROOMS_VIEW_STRIDE;
    WORD_AT(0x00708788u)=RECORDS;
    WORD_AT(0x007E71C8u)=VIEWPORT;
    for(round=0;round<400 && !bad;round++) {
        uint32_t rooms=round<20?(uint32_t)round:rnd(256);
        int32_t deepest=round%7==0?0x4F:(int32_t)rnd(0x50)-(round%13==0?1:0);
        int pct=round%5==0?60:10;
        WORD_AT(VIEWPORT)=rnd(100);WORD_AT(VIEWPORT+4)=rnd(100);WORD_AT(VIEWPORT+0x14)=rnd(100);WORD_AT(VIEWPORT+0x18)=rnd(100);
        WORD_AT(0x007B975Cu)=rooms;
        setup_view(view,rooms,deepest,pct,round%3==0?4:0x50);
        WORD_AT(0x0070AAD0u)=7;WORD_AT(0x006B7ACCu)=9;
        got_n=want_n=0;
        stock_buckets(view);
        run_buckets(view);
        if(!same_events("stock draw")) { fprintf(stderr,"round %d rooms %u deepest %d\n",round,rooms,deepest);bad=1; }
        CHECK(WORD_AT(view+0xD28u)==(uint32_t)deepest);           /* restored after drawing */
        CHECK(WORD_AT(0x0070AAD0u)==0);
        CHECK(WORD_AT(0x006B7ACCu)==(WORD_AT(view+0x214u)?9u:0u));
    }
    CHECK(!bad);
    CHECK(!reader_esp_bad);
    CHECK(mapmaker_rooms_bk.depth==0);
}

/* Past 255: 1500 rooms, 700 of them at depth 2 and 300 sorted (10 kept per depth). */
static void test_buckets_wide(void)
{
    uint32_t view=VIEWS,rooms=1500,r,i,seen_a=0,calls_a2=0,last=0,ok_order=1,high=0;
    static uint8_t drawn[2048];
    WORD_AT(0x007B975Cu)=rooms;
    WORD_AT(0x00708788u)=RECORDS;
    WORD_AT(0x007E71C8u)=VIEWPORT;
    WORD_AT(view+0xD28u)=5;
    for(r=0;r<=rooms;r++) {
        WORD_AT(RECT(view,r))=r%2?2u:(uint32_t)(r%5);
        *u8(RECORDS+r*0xB4u+0xB2u)=r%5==4u?8u:0u;
    }
    got_n=0;
    run_buckets(view);
    memset(drawn,0,sizeof drawn);
    for(i=0;i<got_n && i<EVENTS;i++) {
        if(got[i].kind=='C' && got[i].a==0 && got[i].b==2) calls_a2++;
        if(got[i].kind!='R' || got[i].a!=0) continue;
        r=got[i].c;
        if(r>rooms || drawn[r]) ok_order=0; else drawn[r]=1;
        if(got[i].b==2) { if(r<=last) ok_order=0; last=r; }
        if(r>0xFFu) high++;
        seen_a++;
    }
    /* odd rooms: depth 2; even: r%5 (sorted when r%5==4 -> 10 kept, at depth 6) */
    {
        uint32_t want_depth2=0,want_total=0,want_sorted=0,per_depth[8]={0};
        for(r=1;r<=rooms;r++) {
            if(*u8(RECORDS+r*0xB4u+0xB2u)&8u) {
                uint32_t d=WORD_AT(RECT(view,r));
                if(per_depth[d]<10) { per_depth[d]++;want_sorted++; }
                continue;
            }
            want_total++;
            if(WORD_AT(RECT(view,r))==2u) want_depth2++;
        }
        CHECK(seen_a==want_total+want_sorted);
        CHECK(calls_a2==(want_depth2+254u)/255u && calls_a2>=3u);
    }
    CHECK(ok_order);
    CHECK(high>900u);
    CHECK(!reader_esp_bad);
    CHECK(WORD_AT(view+0xD28u)==5u);
    CHECK(mapmaker_rooms_bk.segmented>0);
}

/* A reader reached without sub_00086C40 draws nothing. */
/* A portal walk 80+ deep ([view+0xD28] >= 0x50): stock passes the unclamped
 * depth to sub_00086AE0, whose readers took count bytes past 0x702DB0+0x4F (the
 * room tree header). The C lists hold depths 0..0x4F only, so deeper reader
 * calls return without drawing. */
static void test_buckets_deep_walk(void)
{
    uint32_t view=VIEWS+MAPMAKER_ROOMS_VIEW_STRIDE,r,i,deep=0,drawn=0;
    WORD_AT(0x00708788u)=RECORDS;WORD_AT(0x007E71C8u)=VIEWPORT;
    WORD_AT(0x007B975Cu)=200;
    setup_view(view,200,0x58,0,0x50);
    for(r=0;r<=201;r++) *u8(RECORDS+r*0xB4u+0xB2u)=0;
    memset(u8(0x00702DB0u+0x50u),0xFF,0x20);           /* what the stock reader would count */
    got_n=0;
    run_buckets(view);
    for(i=0;i<got_n && i<EVENTS;i++) {
        if(got[i].kind=='C' && got[i].b>=0x50u) deep++;
        if(got[i].kind=='R') drawn++;
    }
    CHECK(deep==0 && drawn>0 && WORD_AT(view+0xD28u)==0x58u && !reader_esp_bad);
    CHECK(mapmaker_rooms_bk.depth==0);
}
static void test_reader_outside(void)
{
    *u8(0x00702DB0u+3u)=77;
    got_n=0;
    g_esp=STACK;
    call_reader(0,3);
    CHECK(got_n==1 && got[0].kind=='C');
}

/* ---- stock sub_000897D0 with byte rows (text.asm 0x000897D0..0x00089A1C) ---- */
static uint16_t want_list[6][MAPMAKER_ROOMS_VIEW_LIST_WORDS],want_e08[6],want_e0a[6];
static void stock_view_lists(void)
{
    static uint8_t counts[256],rows[256][256];
    uint32_t v,views=WORD_AT(0x007B0CE0u);
    for(v=0;v<views;v++) {
        uint32_t view=WORD_AT(0x007B0CE4u)+v*MAPMAKER_ROOMS_VIEW_STRIDE,rooms=WORD_AT(0x007B975Cu),r,i;
        int32_t d28,b;
        uint16_t e08=0;
        if(!(*u8(view+0x21Cu)&0x20u)) { want_e08[v]=u16(view+0xE08u);want_e0a[v]=u16(view+0xE0Au);
                                         memcpy(want_list[v],u8(view+MAPMAKER_ROOMS_VIEW_LIST),sizeof want_list[v]);continue; }
        memcpy(want_list[v],u8(view+MAPMAKER_ROOMS_VIEW_LIST),sizeof want_list[v]);
        d28=(int32_t)WORD_AT(view+0xD28u);
        if(d28>=0x50) d28=0x4F;
        for(b=0;b<=d28;b++) counts[b]=0;
        for(r=1;r<rooms+1u;r++) {
            int32_t d=(int32_t)WORD_AT(RECT(view,r));
            if(d<0 || d>=0x50) continue;
            rows[d][counts[d]++]=(uint8_t)r;
        }
        for(b=d28;b>=0;b--)
            for(i=0;i<counts[b];i++) {
                uint32_t room=rows[b][i];
                int16_t x1=(int16_t)u16(RECT(view,room)+8u),x0=(int16_t)u16(RECT(view,room)+4u);
                if(x1<0 || (int32_t)x0>=(int32_t)WORD_AT(0x004E8CF0u) || x1<=x0) continue;
                want_list[v][e08++]=(uint16_t)room;
            }
        want_e0a[v]=e08;
        if(WORD_AT(view+0x2A0u)!=MAPMAKER_ROOMS_ID_GLOBAL) {
            WANT('N',WORD_AT(view+0x2A0u),0x1E,0,0);
            for(i=0;i<neighbour_n;i++) {
                uint32_t j;
                int found=0;
                for(j=0;j<e08;j++) if((uint32_t)(int32_t)(int16_t)want_list[v][j]==neighbours[i]) found=1;
                if(found) continue;
                if((int16_t)e08<(int16_t)MAPMAKER_ROOMS_VIEW_LIST_CAP) want_list[v][e08++]=(uint16_t)neighbours[i];
            }
        }
        want_e08[v]=e08;
    }
}
static void run_view_lists(void)
{
    g_esp=STACK;WORD_AT(g_esp)=0x001CE372u;
    pc_mapmaker_rooms_view_lists(NULL);
    CHECK(g_esp==STACK+4u);
}
static void test_view_lists_match_stock(void)
{
    int round,bad=0;
    WORD_AT(0x007B0CE4u)=VIEWS;
    WORD_AT(0x0070877Cu)=0;
    got_n=0;
    run_view_lists();                              /* the first call only arms the flag */
    CHECK(WORD_AT(0x0070877Cu)==1 && got_n==0);
    for(round=0;round<300 && !bad;round++) {
        uint32_t views=1u+rnd(6),v,rooms=round<10?(uint32_t)round:rnd(256);
        WORD_AT(0x007B0CE0u)=views;
        WORD_AT(0x007B975Cu)=rooms;
        WORD_AT(0x004E8CF0u)=300u+rnd(340);
        neighbour_n=rnd(12);
        for(v=0;v<neighbour_n;v++) neighbours[v]=rnd(4)?rnd(rooms+2u):(uint32_t)(MAPMAKER_ROOMS_IDS_REAL+2u+rnd(11));
        for(v=0;v<views;v++) {
            uint32_t view=VIEWS+v*MAPMAKER_ROOMS_VIEW_STRIDE;
            setup_view(view,rooms,(int32_t)rnd(0x58),5,0x50);
            *u8(view+0x21Cu)=rnd(4)?0x20u:0u;
            WORD_AT(view+0x2A0u)=rnd(5)?rnd(rooms+1u):MAPMAKER_ROOMS_ID_GLOBAL;
            memset(u8(view+MAPMAKER_ROOMS_VIEW_LIST),0xEE,2u*MAPMAKER_ROOMS_VIEW_LIST_WORDS);
            set16(view+0xE08u,0x55);set16(view+0xE0Au,0x66);
        }
        got_n=want_n=0;
        stock_view_lists();
        run_view_lists();
        if(!same_events("view list calls")) bad=1;
        for(v=0;v<views;v++) {
            uint32_t view=VIEWS+v*MAPMAKER_ROOMS_VIEW_STRIDE;
            if(u16(view+0xE08u)!=want_e08[v] || u16(view+0xE0Au)!=want_e0a[v] ||
               memcmp(u8(view+MAPMAKER_ROOMS_VIEW_LIST),want_list[v],2u*want_e08[v])) {
                fprintf(stderr,"view list: round %d view %u: count %u/%u want %u/%u\n",round,v,
                        u16(view+0xE08u),u16(view+0xE0Au),want_e08[v],want_e0a[v]);
                bad=1;
            }
            if((int32_t)WORD_AT(view+0xD28u)>=0x50 && (*u8(view+0x21Cu)&0x20u)) bad=1;   /* capped at 0x4F */
        }
    }
    CHECK(!bad);
}
/* Past 255: rooms above 0xFF are listed with their full id; the list stops at its
 * shadow capacity: 20 rooms more than the list holds, all with rects below the list. */
static void test_view_lists_wide(void)
{
    const uint32_t many=MAPMAKER_ROOMS_VIEW_LIST_WORDS+20u;
    uint32_t view=VIEWS,r,i,high=0,ok=1;
    CHECK(MAPMAKER_ROOMS_VIEW_RECTS+12u*(many+1u)<=MAPMAKER_ROOMS_VIEW_LIST);
    WORD_AT(0x007B0CE0u)=1;
    WORD_AT(0x007B975Cu)=many;
    WORD_AT(0x004E8CF0u)=640;
    *u8(view+0x21Cu)=0x20;
    WORD_AT(view+0xD28u)=3;
    WORD_AT(view+0x2A0u)=5;
    neighbour_n=2;neighbours[0]=many;neighbours[1]=0x20;          /* the last room is past the full list */
    for(r=1;r<=many;r++) {
        WORD_AT(RECT(view,r))=r<150?3u:2u;
        set16(RECT(view,r)+4u,10);set16(RECT(view,r)+8u,20);
    }
    run_view_lists();
    CHECK(u16(view+0xE08u)==MAPMAKER_ROOMS_VIEW_LIST_WORDS && u16(view+0xE0Au)==MAPMAKER_ROOMS_VIEW_LIST_WORDS);   /* full: no neighbour added */
    for(i=0;i<u16(view+0xE08u);i++) {
        uint16_t room=u16(view+MAPMAKER_ROOMS_VIEW_LIST+2u*i);
        if(room>0xFFu) high++;
        if(room!=i+1u) ok=0;                                        /* depth 3 (1..149), then depth 2 */
    }
    CHECK(ok && high>0);
    /* a short list takes the neighbour above 0xFF */
    WORD_AT(0x007B975Cu)=100;neighbours[0]=0x150;
    run_view_lists();
    CHECK(u16(view+0xE08u)==101u && u16(view+MAPMAKER_ROOMS_VIEW_LIST+200u)==0x150u);
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    test_buckets_match_stock();
    test_buckets_wide();
    test_buckets_deep_walk();
    test_reader_outside();
    test_view_lists_match_stock();
    test_view_lists_wide();
    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    printf("mapmaker rooms buckets ok\n");
    return 0;
}
