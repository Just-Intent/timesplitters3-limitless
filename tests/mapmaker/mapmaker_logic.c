/* Asset-free contract for src/interface/pc_mapmaker_logic.inc (action Delay):
 * play (Story only, queue, release through the original, lists restored,
 * other ops untouched, overflow, restart, stock maps unchanged) and editor
 * (the Delay row on an action's edit screen, the step cycle in every op slot,
 * the picker rebuild keeping the delays). Fake guest memory; fake originals
 * pop their return address like the lifted callees; every hook call checks
 * esp. Prints one line per check group; the Python side compares the text. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x01000000/4], g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int limits_on=1;
#define LOGIC_LIMITS_ON() (limits_on)

#define HEAP 0x00300000u
static uint32_t heap_next=HEAP;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p=heap_next;(void)alignment;heap_next+=(bytes+15u)&~15u;return p; }

void sub_00079D60(void) { g_esp+=4u; }
static unsigned nrows;
static struct { uint32_t text, id, cb; } rows[8];
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[8]={0},sp=g_esp;
    int i;
    va_start(a,n);
    for(i=0;i<n && i<8;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00079D60) { if(nrows<8) { rows[nrows].text=x[0];rows[nrows].id=x[1];rows[nrows].cb=x[4]; } nrows++; }
    for(i=n-1;i>=0;i--) { g_esp-=4u;WORD_AT(g_esp)=x[i]; }
    g_esp-=4u;WORD_AT(g_esp)=0x0000CA11u;
    f();
    g_esp=sp;
    return 0;
}
#include "pc_mapmaker_logic.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { printf("%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define B8(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define H16(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

#define ESP0 0x00600000u
#define OBJ 0x00200000u
#define RTOPS 0x00210000u
#define RTACTS 0x00220000u
#define ED 0x00100000u
#define MENU 0x00380000u
#define OPHDR 0x00390000u
#define RTOP(i) (RTOPS+(uint32_t)(i)*0x25Eu)
#define EDOP(i) (ED+0x1FC70u+(uint32_t)(i)*0x220u)
#define EDNODE(i) (ED+0x2FB70u+(uint32_t)(i)*0xB4u)

static int esp_errors;
static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg,int line)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=arg;
    fn(orig);
    if(g_esp!=ESP0+4u) { printf("%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));esp_errors++; }
    g_esp=ESP0;
}
#define HOOK(fn,orig,ret,arg) hook(fn,orig,ret,(uint32_t)(arg),__LINE__)

/* ---- play fakes ---------------------------------------------------------- */
static unsigned frame, nfired, fire_calls, setup_calls;
static struct { unsigned frame, node; } fired[1024];
/* sub_0033CF80, as the stock code: flagged ops, clear the flag, every action
 * of the list (found by node among the runtime actions); plain ret. */
static void orig_fire(void)
{
    uint32_t n=WORD_AT(OBJ+0x7Cu),i,k;
    fire_calls++;
    for(i=0;i<n;i++) {
        uint32_t op=WORD_AT(OBJ)+i*0x25Eu,f=B8(op+0x25Cu);
        int16_t c;
        if(!(f&1u)) continue;
        B8(op+0x25Cu)=(uint8_t)(f&0xFEu);
        c=(int16_t)H16(op+0x12Eu);
        for(k=0;(int)k<c;k++) {
            uint32_t node=H16(op+0x130u+2u*k),j,e=0;
            for(j=0;j<WORD_AT(OBJ+0x88u);j++) if(WORD_AT(WORD_AT(OBJ+0x8Cu)+j*0x24u)==node) { e=WORD_AT(OBJ+0x8Cu)+j*0x24u;break; }
            if(e && nfired<1024) { fired[nfired].frame=frame;fired[nfired].node=node;nfired++; }
        }
    }
    g_esp+=4u;
}
static void orig_setup(void) { setup_calls++;g_esp+=4u; }
static void level(unsigned ops,unsigned scene,unsigned mode)
{
    unsigned j;
    memset((void *)((uintptr_t)RTOPS+g_xbox_mem_offset),0,0x20000);
    WORD_AT(0x0079094Cu)=scene;B8(0x007DE455u)=(uint8_t)mode;
    WORD_AT(0x00765E44u)=OBJ;WORD_AT(OBJ)=RTOPS;WORD_AT(OBJ+0x7Cu)=ops;
    WORD_AT(OBJ+0x88u)=40u;WORD_AT(OBJ+0x8Cu)=RTACTS;
    for(j=0;j<40u;j++) WORD_AT(RTACTS+j*0x24u)=100u+j;   /* runtime action j = node 100 + j */
    WORD_AT(0x004E7944u)=1u;WORD_AT(0x004E8CECu)=60u;
    nfired=0;frame=0;
}
static void act(unsigned op,unsigned k,unsigned node,unsigned tenths)
{
    H16(RTOP(op)+0x130u+2u*k)=(uint16_t)node;H16(RTOP(op)+0x194u+2u*k)=(uint16_t)tenths;
    if(H16(RTOP(op)+0x12Eu)<=k) H16(RTOP(op)+0x12Eu)=(uint16_t)(k+1u);
}
static void setup(void) { HOOK(pc_mapmaker_logic_setup,orig_setup,0x00340A83u,0x0078CC00u); }
static void tick(void) { HOOK(pc_mapmaker_logic_fire,orig_fire,0x0033D1C9u,0);frame++; }
static unsigned fired_at(unsigned node)
{
    unsigned i;
    for(i=0;i<nfired;i++) if(fired[i].node==node) return fired[i].frame;
    return 9999u;
}
static unsigned fired_count(unsigned node)
{
    unsigned i,n=0;
    for(i=0;i<nfired;i++) n+=fired[i].node==node;
    return n;
}

static void test_stock(void)
{
    unsigned f;
    level(2,100,10);
    act(0,0,100,0);act(0,1,101,0);act(1,0,102,0);
    setup();
    CHECK(setup_calls==1 && !logic_play.delays);
    B8(RTOP(0)+0x25Cu)=1u;
    fire_calls=0;tick();
    CHECK(fire_calls==1 && nfired==2 && fired_at(100)==0 && fired_at(101)==0);
    for(f=0;f<5;f++) tick();
    CHECK(fire_calls==6 && nfired==2);
    printf("stock map: original only, %u calls\n",fire_calls);
}

static void test_delays(void)
{
    unsigned f;
    uint8_t before[0x25E];
    level(3,100,10);
    act(0,0,100,0);act(0,1,101,10);act(0,2,102,5);   /* now, 1 s, 0.5 s */
    act(1,0,103,0);
    act(2,0,104,20);
    setup();
    CHECK(logic_play.delays==1);
    memcpy(before,(void *)((uintptr_t)RTOP(0)+g_xbox_mem_offset),sizeof before);
    B8(RTOP(0)+0x25Cu)=1u;
    tick();                                          /* frame 0 */
    CHECK(fired_at(100)==0 && fired_count(101)==0 && fired_count(102)==0);
    CHECK(logic_play.n==2);
    CHECK(!memcmp(before,(void *)((uintptr_t)RTOP(0)+g_xbox_mem_offset),0x25C));   /* list, count, delays put back */
    CHECK(!(B8(RTOP(0)+0x25Cu)&1u));
    for(f=1;f<=60;f++) {
        if(f==30) B8(RTOP(1)+0x25Cu)=1u;             /* another op fires in the same frame as a release */
        tick();
    }
    CHECK(fired_at(102)==30 && fired_at(103)==30 && fired_at(101)==60);
    CHECK(fired_count(100)==1 && fired_count(101)==1 && fired_count(102)==1 && fired_count(103)==1);
    CHECK(fired_count(104)==0 && logic_play.n==0);
    CHECK(!memcmp(before,(void *)((uintptr_t)RTOP(0)+g_xbox_mem_offset),0x25C));
    /* a frame of 3 ticks ages the queue by 3 */
    B8(RTOP(2)+0x25Cu)=1u;
    tick();                                          /* frame 61, queued for 120 ticks */
    WORD_AT(0x004E7944u)=3u;
    for(f=0;f<39;f++) tick();
    CHECK(fired_count(104)==0);
    tick();
    CHECK(fired_count(104)==1 && fired_at(104)==101);
    WORD_AT(0x004E7944u)=0u;                         /* a native subtick: no time passes */
    B8(RTOP(2)+0x25Cu)=1u;
    for(f=0;f<200;f++) tick();
    CHECK(fired_count(104)==1 && logic_play.n==1);
    /* restart: the queue is emptied, the old action never fires */
    setup();
    WORD_AT(0x004E7944u)=1u;
    for(f=0;f<200;f++) tick();
    CHECK(fired_count(104)==1 && logic_play.n==0);
    printf("story delays: 0.5 s at frame %u, 1 s at frame %u, 2 s at 3 ticks a frame at frame %u\n",
           fired_at(102),fired_at(101),fired_at(104));
}

static void test_not_story(void)
{
    level(1,100,3);
    act(0,0,100,50);act(0,1,101,0);
    setup();
    CHECK(logic_play.delays==0);
    B8(RTOP(0)+0x25Cu)=1u;
    fire_calls=0;tick();
    CHECK(fire_calls==1 && fired_at(100)==0 && fired_at(101)==0);
    printf("other modes: delays ignored\n");
}

static void test_overflow(void)
{
    unsigned f;
    level(1,100,10);
    act(0,0,100,600);act(0,1,101,0);                 /* 1 min, fired every frame */
    setup();
    for(f=0;f<300;f++) { B8(RTOP(0)+0x25Cu)=1u;tick(); }
    CHECK(logic_play.n==LOGIC_QUEUE && logic_play.overflow==300u-LOGIC_QUEUE);
    CHECK(fired_count(100)==300u-LOGIC_QUEUE && fired_count(101)==300u);
    printf("overflow: %u queued, %u fired at once\n",logic_play.n,fired_count(100));
}

static void test_many_due(void)
{
    unsigned f;
    level(1,100,10);
    act(0,0,100,10);
    setup();
    for(f=0;f<1;f++) { }
    /* 70 copies of one action all due in the same frame: 50 a pass, the rest next frame */
    for(f=0;f<70;f++) { LogicDelayed *q=&logic_play.q[logic_play.n++];q->op=0;q->node=100;q->left=1; }
    tick();
    CHECK(fired_count(100)==50 && logic_play.n==20);
    tick();
    CHECK(fired_count(100)==70 && logic_play.n==0);
    printf("many due: 50 a pass\n");
}

/* ---- editor --------------------------------------------------------------- */
static unsigned rows_calls, done_calls, picker_calls;
static void orig_rows(void) { rows_calls++;g_esp+=4u; }
static void orig_done(void) { done_calls++;g_esp+=4u; }
static uint32_t picker_list[8], picker_n;
/* sub_00324B60: clear the current op's list, then sub_003208F0 per ticked
 * row (slot node, delay 0). */
static void orig_picker(void)
{
    uint32_t op=EDOP(WORD_AT(OPHDR+0xA4u)),k;
    picker_calls++;
    WORD_AT(op+0x88u)=0;
    for(k=0;k<picker_n;k++) { WORD_AT(op+0x8Cu+4u*k)=picker_list[k];WORD_AT(op+0x154u+4u*k)=0;WORD_AT(op+0x88u)=k+1u; }
    g_esp+=4u;
}
static void ed_op(unsigned i,unsigned n,const uint32_t *nodes,const uint32_t *delays)
{
    unsigned k;
    WORD_AT(EDOP(i)+0x88u)=n;
    for(k=0;k<n;k++) { WORD_AT(EDOP(i)+0x8Cu+4u*k)=nodes[k];WORD_AT(EDOP(i)+0x154u+4u*k)=delays?delays[k]:0u; }
}
static const char *text(uint32_t a) { return (const char *)((uintptr_t)a+g_xbox_mem_offset); }
static void open_screen(unsigned node,uint32_t ret)
{
    nrows=0;
    WORD_AT(ED+0x11858u)=EDNODE(node);
    HOOK(pc_mapmaker_logic_rows,orig_rows,ret,0x40000000u);
}
static void click(uint32_t row)
{
    WORD_AT(MENU+0x18Cu)=row;
    HOOK(pc_mapmaker_logic_done,orig_done,0x0007A000u,MENU);
}

static void test_editor(void)
{
    static const uint32_t a[]={3,5},b[]={5,7},c[]={9};
    unsigned i,steps[12],n;
    char seen[256]="";
    memset((void *)((uintptr_t)ED+g_xbox_mem_offset),0,0x74C00);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x007844A8u)=ED;
    WORD_AT(ED+0x41E0u)=3u;WORD_AT(ED+0x81A4u)=12u;
    for(i=0;i<12;i++) WORD_AT(EDNODE(i))=i==4?1u:2u;  /* node 4 is a trigger */
    ed_op(0,2,a,NULL);ed_op(1,2,b,NULL);ed_op(2,1,c,NULL);
    /* an action in two ops */
    open_screen(5,0x00329B61u);
    CHECK(rows_calls==1 && nrows==1 && rows[0].id==0x4C474401u && rows[0].cb==0x00328BF0u);
    CHECK(nrows && !strcmp(text(rows[0].text),"Delay: none"));
    n=0;
    do {
        done_calls=0;click(0x4C474401u);
        CHECK(done_calls==0);
        steps[n++]=WORD_AT(EDOP(0)+0x154u+4u);
        CHECK(WORD_AT(EDOP(1)+0x154u)==steps[n-1]);           /* every op slot of the action */
        CHECK(WORD_AT(EDOP(0)+0x154u)==0 && WORD_AT(EDOP(1)+0x158u)==0);
        if(n==1) CHECK(!strcmp(text(rows[0].text),"Delay: 0.5 s"));
        if(n==6) CHECK(!strcmp(text(rows[0].text),"Delay: 10 s"));
        if(n==9) CHECK(!strcmp(text(rows[0].text),"Delay: 1 min"));
    } while(steps[n-1] && n<12);
    for(i=0;i<n;i++) { char s[8];snprintf(s,sizeof s,"%u ",steps[i]);strcat(seen,s); }
    CHECK(n==10 && !strcmp(text(rows[0].text),"Delay: none"));
    printf("editor steps: %s\n",seen);
    /* another row: the stock Done */
    done_calls=0;click(0);
    CHECK(done_calls==1);
    /* a trigger, a wrong screen, limits off, not the editor: no row */
    open_screen(4,0x00329B61u);CHECK(nrows==0);
    open_screen(5,0x0031D083u);CHECK(nrows==0);
    limits_on=0;open_screen(5,0x00329B61u);CHECK(nrows==0);limits_on=1;
    WORD_AT(0x0079094Cu)=100u;open_screen(5,0x00329B61u);CHECK(nrows==0);WORD_AT(0x0079094Cu)=102u;
    /* a node outside the array (a new action on the scratch copy): no row */
    nrows=0;WORD_AT(ED+0x11858u)=ED+0x11618u;HOOK(pc_mapmaker_logic_rows,orig_rows,0x00329B61u,0);CHECK(nrows==0);
    /* a node past the count (a slot left over after a delete) */
    WORD_AT(EDNODE(20))=2u;open_screen(20,0x00329B61u);CHECK(nrows==0);
    /* an action in no op: the row says so, a click stores nothing */
    open_screen(11,0x00329B61u);
    CHECK(nrows==1 && !strcmp(text(rows[0].text),"Delay: add to an operation first"));
    click(0x4C474401u);
    for(i=0;i<3;i++) CHECK(WORD_AT(EDOP(i)+0x154u)==0 && WORD_AT(EDOP(i)+0x158u)==0);
    printf("editor row: actions only, edit screen only\n");
}

static void test_picker(void)
{
    static const uint32_t a[]={3,5},d[]={20,0},b[]={7},e[]={50};
    memset((void *)((uintptr_t)ED+g_xbox_mem_offset),0,0x74C00);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x007844A8u)=ED;
    WORD_AT(ED+0x41E0u)=2u;WORD_AT(ED+0x81A4u)=12u;
    ed_op(0,2,a,d);ed_op(1,1,b,e);
    WORD_AT(0x00762F98u)=OPHDR;WORD_AT(OPHDR+0xA4u)=0;
    picker_list[0]=5;picker_list[1]=7;picker_list[2]=3;picker_n=3;
    g_esp=ESP0;WORD_AT(ESP0)=0x003282C3u;
    pc_mapmaker_logic_actions(orig_picker);
    CHECK(g_esp==ESP0+4u);g_esp=ESP0;
    CHECK(picker_calls==1 && WORD_AT(EDOP(0)+0x88u)==3);
    CHECK(WORD_AT(EDOP(0)+0x154u)==0 && WORD_AT(EDOP(0)+0x158u)==50 && WORD_AT(EDOP(0)+0x15Cu)==20);
    printf("picker rebuild: delays kept (0 50 20)\n");
}

int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    test_stock();
    test_delays();
    test_not_story();
    test_overflow();
    test_many_due();
    test_editor();
    test_picker();
    if(esp_errors) failures++;
    printf(failures?"mapmaker logic FAILED (%d)\n":"mapmaker logic ok\n",failures);
    return failures?1:0;
}
