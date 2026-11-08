/* US 45410066 MapMaker logic: a "Delay" on actions (docs/mapmaker/mapmaker-limitless.md "Logic: Delay on actions";
 * design and evidence: docs/agent-context/limitless-logic.md).
 *
 * Stock facts (CONFIRMED in the disassembly):
 *  - An operation (0x6E chunk, editor array +0x1FC70 after editor.json, 0x220
 *    bytes, count editor +0x41E0) lists its action nodes as dwords at +0x8C
 *    (count +0x88, 50 slots) and has one more dword per slot at +0x154 that
 *    nothing uses: sub_003208F0 (add a node to an op) writes 0 there
 *    (0x00320944), the node delete fix-up sub_00322D40 shifts it with the
 *    slot (0x00322F65), both loaders and the writer copy the op whole, and
 *    play (sub_0033AA80) copies its low word to runtime op +0x194 + 2k
 *    (0x0033AC14), which nothing reads. This area stores the delay there, in
 *    tenths of a second (0 = none, the stock value).
 *  - Runtime logic object [0x00765E44]: +0 runtime ops (0x25E each, count
 *    +0x7C): +0x12E word action count, +0x130 + 2k word action node, +0x194
 *    + 2k word (the copy above), +0x25C byte flags (bit 0: fire this frame);
 *    runtime actions at +0x8C (0x24 each, count +0x88, +0 node, +0x10 fn).
 *  - sub_0033CF80 (no arguments, plain ret), called once a frame by the
 *    logic tick sub_0033D1B0 (level desc +0x110), fires: for each op with
 *    flag bit 0, clear it and call every action's fn. sub_0033AA80(desc)
 *    builds the runtime logic at every level start and restart
 *    (sub_00340A60).
 *  - Whole game ticks this frame [0x004E7944] (timers add it,
 *    sub_0033CCB0); ticks per second [0x004E8CEC] (timer triggers multiply
 *    their seconds by it, 0x00340BBA).
 *  - Editor: the trigger/action edit screen is built by sub_00328D20 (menu
 *    0x0052EA48); its node is editor +0x11858; it calls sub_000784A0 at
 *    0x00329B5C right before its "Done" row, whose callback is sub_00328BF0
 *    (cdecl menu, plain ret). The action picker of an op rebuilds the op's
 *    action list in sub_00324B60 (clear, then sub_003208F0 per ticked row),
 *    which zeroes every slot's +0x154.
 *
 * Hooks (area "logic"):
 *  - pc_mapmaker_logic_setup     WRAP sub_0033AA80: empty the queue; note
 *    whether this level has delays (Story play only) and log one summary.
 *  - pc_mapmaker_logic_fire      WRAP sub_0033CF80: releases due actions
 *    through the original (their op flagged alone, its list cut to them),
 *    then hides this frame's delayed actions from the original and queues
 *    them. No delays, or not Story play: the original only.
 *  - pc_mapmaker_logic_rows      WRAP sub_000784A0: the "Delay:" row on an
 *    action's edit screen (TSFP_MAPMAKER_LIMITS=1, editor).
 *  - pc_mapmaker_logic_done      WRAP sub_00328BF0: a click on that row
 *    steps the delay; the screen stays open (the original is not run).
 *  - pc_mapmaker_logic_actions   WRAP sub_00324B60: keeps the delays of an
 *    op's actions across the picker's rebuild.
 * The delay belongs to the action: the row sets it in every op slot that
 * holds the action. TSFP_MAPMAKER_LOGIC_TRACE=1 logs the editor screens the
 * hooks see (for a playtest). Requires the includer's WORD_AT, g_esp,
 * g_xbox_mem_offset, guest_call, xbox_HeapAlloc. */
#ifndef PC_MAPMAKER_LOGIC_INC
#define PC_MAPMAKER_LOGIC_INC 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) ((((uint32_t)(p))>=0x00010000u && ((uint32_t)(p))<0x04000000u-(uint32_t)(n)) || \
                              (((uint32_t)(p))>=0x80000000u && ((uint32_t)(p))<0x84000000u-(uint32_t)(n)))
#endif
#ifndef MAPMAKER_GUEST_RET
#define MAPMAKER_GUEST_RET() (g_esp+=4u)
#endif
#ifndef LOGIC_LIMITS_ON
#define LOGIC_LIMITS_ON() (mapmaker_limits_read(),mapmaker_limits.limits)
#endif

#define LOGIC_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define LOGIC_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define LOGIC_SCENE 0x0079094Cu
#define LOGIC_MODE 0x007DE455u                  /* byte: 10 = Story */
#define LOGIC_STORY 10u
#define LOGIC_PLAY 100u
#define LOGIC_EDIT 102u
#define LOGIC_OBJ 0x00765E44u
#define LOGIC_TICKS 0x004E7944u                 /* whole game ticks this frame */
#define LOGIC_TPS 0x004E8CECu                   /* ticks per second */
#define LOGIC_TPS_DEFAULT 60u
#define LOGIC_RT_OP 0x25Eu
#define LOGIC_RT_OPS 0x7Cu
#define LOGIC_RT_ACT_COUNT 0x12Eu
#define LOGIC_RT_ACT_NODE 0x130u
#define LOGIC_RT_DELAY 0x194u
#define LOGIC_RT_FLAGS 0x25Cu
#define LOGIC_SLOTS 50u
#define LOGIC_RT_OPS_MAX 1024u                  /* sanity bound (120 in the editor) */
#define LOGIC_QUEUE 256u
#define LOGIC_MAX_TICKS 600u                    /* one frame never ages the queue more */
/* editor */
#define LOGIC_EDITOR 0x007844A8u
#define LOGIC_ED_SIZE 0x74C00u
#define LOGIC_ED_OPS 0x1FC70u                   /* editor.json: 120 x 0x220 */
#define LOGIC_ED_OP_COUNT 0x41E0u
#define LOGIC_ED_OPS_CAP 120u
#define LOGIC_ED_OP 0x220u
#define LOGIC_ED_ACT_COUNT 0x88u
#define LOGIC_ED_ACT_NODE 0x8Cu
#define LOGIC_ED_DELAY 0x154u
#define LOGIC_ED_NODES 0x2FB70u                 /* editor.json: 520 x 0xB4 */
#define LOGIC_ED_NODE_COUNT 0x81A4u
#define LOGIC_ED_NODES_CAP 520u
#define LOGIC_ED_NODE 0xB4u
#define LOGIC_ED_CURRENT 0x11858u               /* node on the edit screen */
#define LOGIC_KIND_ACTION 2u
#define LOGIC_ROWS_AT 0x00329B61u               /* return from sub_000784A0 in sub_00328D20 */
#define LOGIC_DONE 0x00328BF0u                  /* the screen's Done callback */
#define LOGIC_ROW 0x4C474401u                   /* our row id (menu +0x18C) */
#define LOGIC_MENU_ROW 0x18Cu
#define LOGIC_LABEL 48u

extern void sub_00079D60(void);

static const uint16_t logic_steps[]={0,5,10,20,30,50,100,150,300,600};  /* tenths of a second */
#define LOGIC_STEPS (sizeof logic_steps/sizeof logic_steps[0])

typedef struct { uint16_t op, node; int32_t left; } LogicDelayed;
static struct {
    LogicDelayed q[LOGIC_QUEUE];
    unsigned n;
    int delays;                 /* this level has delayed actions and is Story play */
    unsigned overflow, released, queued, logged, summaries;
} logic_play;
static struct {
    uint32_t label;             /* guest text of the row */
    unsigned shown, trace_n, notes, unused;
    uint32_t trace_ret[32];
    int trace;                  /* -1 unread */
} logic_ed={0,0,0,0,0,{0},-1};

static void logic_log(const char *format,unsigned a,unsigned b,unsigned c)
{
    if(logic_play.logged>=32u) return;
    logic_play.logged++;
    fprintf(stderr,"[mapmaker] logic: ");
    fprintf(stderr,format,a,b,c);
    fputc('\n',stderr);
    fflush(stderr);
}
/* "0.5 s", "5 s", "1 min" */
static void logic_format(char *out,size_t n,unsigned tenths)
{
    if(!tenths) snprintf(out,n,"none");
    else if(tenths%600u==0u) snprintf(out,n,"%u min",tenths/600u);
    else if(tenths%10u) snprintf(out,n,"%u.%u s",tenths/10u,tenths%10u);
    else snprintf(out,n,"%u s",tenths/10u);
}
/* The step after `tenths` (values off the list go to the next larger step). */
static unsigned logic_next(unsigned tenths)
{
    unsigned i;
    for(i=0;i<LOGIC_STEPS;i++) if(logic_steps[i]>tenths) return logic_steps[i];
    return 0;
}

/* ---- play --------------------------------------------------------------- */
static int logic_story_play(void)
{
    return WORD_AT(LOGIC_SCENE)==LOGIC_PLAY && LOGIC_B(LOGIC_MODE)==LOGIC_STORY;
}
static uint32_t logic_obj(void)
{
    uint32_t o=WORD_AT(LOGIC_OBJ);
    return o && MAPMAKER_PTR_OK(o,0x1960u)?o:0u;
}
static uint32_t logic_ops(uint32_t obj,uint32_t *count)
{
    uint32_t ops=WORD_AT(obj),n=WORD_AT(obj+LOGIC_RT_OPS);
    if(n>LOGIC_RT_OPS_MAX || (n && !MAPMAKER_PTR_OK(ops,n*LOGIC_RT_OP))) n=0;
    *count=n;
    return ops;
}
static uint32_t logic_actions(uint32_t op)
{
    uint32_t n=LOGIC_H(op+LOGIC_RT_ACT_COUNT);
    return n<=LOGIC_SLOTS?n:0u;     /* a negative / huge word: nothing to delay (the original runs) */
}
static int32_t logic_ticks(unsigned tenths)
{
    uint32_t tps=WORD_AT(LOGIC_TPS);
    if(!tps || tps>1000u) tps=LOGIC_TPS_DEFAULT;
    return (int32_t)((tenths*tps+5u)/10u);
}

/* WRAP sub_0033AA80 (cdecl desc): runtime logic built (level start/restart). */
void pc_mapmaker_logic_setup(void (*original)(void))
{
    uint32_t obj,ops,count,i,k,acts=0,delayed=0,lo=0,hi=0;
    original();
    memset(logic_play.q,0,sizeof logic_play.q);
    logic_play.n=0;logic_play.delays=0;logic_play.overflow=0;
    if(WORD_AT(LOGIC_SCENE)!=LOGIC_PLAY || !(obj=logic_obj())) return;
    ops=logic_ops(obj,&count);
    for(i=0;i<count;i++) {
        uint32_t op=ops+i*LOGIC_RT_OP,n=logic_actions(op);
        for(k=0;k<n;k++) {
            unsigned d=LOGIC_H(op+LOGIC_RT_DELAY+2u*k);
            acts++;
            if(d) { delayed++;if(!lo || d<lo) lo=d;if(d>hi) hi=d; }
        }
    }
    if(!delayed) return;
    logic_play.delays=logic_story_play();
    {
        char a[16],b[16];
        logic_format(a,sizeof a,lo);logic_format(b,sizeof b,hi);
        if(logic_play.summaries<64u) {
            logic_play.summaries++;
            fprintf(stderr,"[mapmaker] logic: %u operations, %u actions, %u with a delay (%s..%s)%s\n",
                    (unsigned)count,(unsigned)acts,(unsigned)delayed,a,b,
                    logic_play.delays?"":"; not a Story map: the delays are ignored");
            fflush(stderr);
        }
    }
}

/* Fire the queued actions whose time has come: each such op alone flagged,
 * its list cut to the due nodes (50 at most a pass; the rest next frame),
 * through the original; everything put back. */
#define LOGIC_GONE 0xFFFFu
static void logic_release(void (*original)(void),uint32_t ops,uint32_t count)
{
    static uint8_t flags[LOGIC_RT_OPS_MAX];
    uint16_t saved[LOGIC_SLOTS],node[LOGIC_SLOTS];
    uint32_t i,j,k,due=0;
    for(j=0;j<logic_play.n;j++) if(logic_play.q[j].left<=0) due++;
    if(!due) return;
    for(i=0;i<count;i++) {
        uint32_t f=ops+i*LOGIC_RT_OP+LOGIC_RT_FLAGS;
        flags[i]=LOGIC_B(f);LOGIC_B(f)=(uint8_t)(flags[i]&0xFEu);
    }
    for(i=0;i<count;i++) {
        uint32_t op=ops+i*LOGIC_RT_OP,n=0,old=LOGIC_H(op+LOGIC_RT_ACT_COUNT);
        for(j=0;j<logic_play.n && n<LOGIC_SLOTS;j++)
            if(logic_play.q[j].op==i && logic_play.q[j].left<=0) { node[n++]=logic_play.q[j].node;logic_play.q[j].op=LOGIC_GONE; }
        if(!n) continue;
        for(k=0;k<LOGIC_SLOTS;k++) saved[k]=LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k);
        for(k=0;k<n;k++) LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k)=node[k];
        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=(uint16_t)n;
        LOGIC_B(op+LOGIC_RT_FLAGS)|=1u;
        guest_call(original,0);
        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=(uint16_t)old;
        for(k=0;k<LOGIC_SLOTS;k++) LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k)=saved[k];
        logic_play.released+=n;
    }
    for(i=0;i<count;i++) LOGIC_B(ops+i*LOGIC_RT_OP+LOGIC_RT_FLAGS)=flags[i];
    for(i=j=0;j<logic_play.n;j++) if(logic_play.q[j].op!=LOGIC_GONE) logic_play.q[i++]=logic_play.q[j];
    logic_play.n=i;
}

/* WRAP sub_0033CF80 (no arguments): the logic's action pass. */
void pc_mapmaker_logic_fire(void (*original)(void))
{
    static uint16_t saved[LOGIC_RT_OPS_MAX][LOGIC_SLOTS],count_saved[LOGIC_RT_OPS_MAX];
    static uint8_t cut[LOGIC_RT_OPS_MAX];
    uint32_t obj,ops,count,i,j,k,ticks;
    if(!logic_play.delays || !logic_story_play() || !(obj=logic_obj())) { original();return; }
    ops=logic_ops(obj,&count);
    ticks=WORD_AT(LOGIC_TICKS);
    if(ticks>LOGIC_MAX_TICKS) ticks=LOGIC_MAX_TICKS;
    for(j=0;j<logic_play.n;j++) {
        if(logic_play.q[j].op>=count) logic_play.q[j].op=LOGIC_GONE;   /* cannot happen within a level */
        else logic_play.q[j].left-=(int32_t)ticks;
    }
    for(i=j=0;j<logic_play.n;j++) if(logic_play.q[j].op!=LOGIC_GONE) logic_play.q[i++]=logic_play.q[j];
    logic_play.n=i;
    logic_release(original,ops,count);
    /* This frame's firing ops: queue their delayed actions and hide them. */
    for(i=0;i<count;i++) {
        uint32_t op=ops+i*LOGIC_RT_OP,n,m=0;
        cut[i]=0;
        if(!(LOGIC_B(op+LOGIC_RT_FLAGS)&1u) || !(n=logic_actions(op))) continue;
        for(k=0;k<n;k++) if(LOGIC_H(op+LOGIC_RT_DELAY+2u*k)) break;
        if(k==n) continue;
        for(k=0;k<LOGIC_SLOTS;k++) saved[i][k]=LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k);
        count_saved[i]=LOGIC_H(op+LOGIC_RT_ACT_COUNT);
        for(k=0;k<n;k++) {
            unsigned d=LOGIC_H(op+LOGIC_RT_DELAY+2u*k);
            if(d && logic_play.n<LOGIC_QUEUE) {
                LogicDelayed *q=&logic_play.q[logic_play.n++];
                q->op=(uint16_t)i;q->node=saved[i][k];q->left=logic_ticks(d);
                logic_play.queued++;
                continue;
            }
            if(d && !logic_play.overflow++)
                logic_log("more than %u delayed actions waiting; action %u of operation %u fires now",LOGIC_QUEUE,k+1u,i+1u);
            LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*m)=saved[i][k];m++;
        }
        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=(uint16_t)m;
        cut[i]=1;
    }
    original();
    for(i=0;i<count;i++) if(cut[i]) {
        uint32_t op=ops+i*LOGIC_RT_OP;
        for(k=0;k<LOGIC_SLOTS;k++) LOGIC_H(op+LOGIC_RT_ACT_NODE+2u*k)=saved[i][k];
        LOGIC_H(op+LOGIC_RT_ACT_COUNT)=count_saved[i];
    }
}

/* ---- editor ------------------------------------------------------------- */
static uint32_t logic_editor(void)
{
    uint32_t e=WORD_AT(LOGIC_EDITOR);
    return e && MAPMAKER_PTR_OK(e,LOGIC_ED_SIZE)?e:0u;
}
static uint32_t logic_ed_ops(uint32_t e) { uint32_t n=WORD_AT(e+LOGIC_ED_OP_COUNT); return n<=LOGIC_ED_OPS_CAP?n:LOGIC_ED_OPS_CAP; }
static uint32_t logic_ed_op(uint32_t e,uint32_t i) { return e+LOGIC_ED_OPS+i*LOGIC_ED_OP; }
static uint32_t logic_ed_acts(uint32_t op) { uint32_t n=WORD_AT(op+LOGIC_ED_ACT_COUNT); return n<=LOGIC_SLOTS?n:LOGIC_SLOTS; }
/* The action node on the edit screen: its index, or -1 (a trigger, a new
 * node not yet in the array, anything else). */
static int logic_ed_current(uint32_t e)
{
    uint32_t p=WORD_AT(e+LOGIC_ED_CURRENT),base=e+LOGIC_ED_NODES,n=WORD_AT(e+LOGIC_ED_NODE_COUNT),i;
    if(n>LOGIC_ED_NODES_CAP) n=LOGIC_ED_NODES_CAP;
    if(p<base || (p-base)%LOGIC_ED_NODE || (i=(p-base)/LOGIC_ED_NODE)>=n) return -1;
    return WORD_AT(p)==LOGIC_KIND_ACTION?(int)i:-1;
}
/* The action's delay (first op slot holding it); *used = op slots holding it. */
static unsigned logic_ed_delay(uint32_t e,uint32_t node,unsigned *used,int skip_op)
{
    uint32_t i,k,ops=logic_ed_ops(e);
    unsigned d=0,u=0;
    for(i=0;i<ops;i++) {
        uint32_t op=logic_ed_op(e,i),n=logic_ed_acts(op);
        if((int)i==skip_op) continue;
        for(k=0;k<n;k++) if(WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k)==node) {
            if(!u++) d=WORD_AT(op+LOGIC_ED_DELAY+4u*k)&0xFFFFu;
        }
    }
    if(used) *used=u;
    return d;
}
static void logic_ed_set(uint32_t e,uint32_t node,unsigned tenths)
{
    uint32_t i,k,ops=logic_ed_ops(e);
    for(i=0;i<ops;i++) {
        uint32_t op=logic_ed_op(e,i),n=logic_ed_acts(op);
        for(k=0;k<n;k++) if(WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k)==node) WORD_AT(op+LOGIC_ED_DELAY+4u*k)=tenths;
    }
}
static uint32_t logic_label(uint32_t e,uint32_t node)
{
    char text[LOGIC_LABEL],value[16];
    unsigned used,d=logic_ed_delay(e,node,&used,-1);
    if(!logic_ed.label && !(logic_ed.label=xbox_HeapAlloc(LOGIC_LABEL,4u))) return 0;
    logic_format(value,sizeof value,d);
    if(used) snprintf(text,sizeof text,"Delay: %s",value);
    else snprintf(text,sizeof text,"Delay: add to an operation first");
    memset((void *)((uintptr_t)logic_ed.label+g_xbox_mem_offset),0,LOGIC_LABEL);
    memcpy((void *)((uintptr_t)logic_ed.label+g_xbox_mem_offset),text,strlen(text));
    return logic_ed.label;
}
static int logic_trace(void)
{
    if(logic_ed.trace<0) { const char *t=getenv("TSFP_MAPMAKER_LOGIC_TRACE");logic_ed.trace=t && t[0]=='1' && !t[1]; }
    return logic_ed.trace;
}
static void logic_trace_screen(uint32_t e,uint32_t ret)
{
    uint32_t p,i;
    if(!logic_trace() || !e) return;
    for(i=0;i<logic_ed.trace_n;i++) if(logic_ed.trace_ret[i]==ret) return;
    if(logic_ed.trace_n>=32u) return;
    logic_ed.trace_ret[logic_ed.trace_n++]=ret;
    p=WORD_AT(e+LOGIC_ED_CURRENT);
    fprintf(stderr,"[mapmaker] logic trace: menu end ret=0x%08X node=0x%08X",ret,p);
    if(MAPMAKER_PTR_OK(p,0x10u)) fprintf(stderr," kind %u subject %u verb %u",WORD_AT(p),WORD_AT(p+4u),WORD_AT(p+8u));
    fprintf(stderr," index %d%s\n",logic_ed_current(e),ret==LOGIC_ROWS_AT?" (edit screen: Delay row point)":"");
    fflush(stderr);
}

/* WRAP sub_000784A0 (spacing): on the trigger/action edit screen it runs
 * right before the "Done" row; an action gets the Delay row there. */
void pc_mapmaker_logic_rows(void (*original)(void))
{
    uint32_t r=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp):0u,e,text;
    int node;
    original();
    if(WORD_AT(LOGIC_SCENE)!=LOGIC_EDIT || !(e=logic_editor())) return;
    logic_trace_screen(e,r);
    if(r!=LOGIC_ROWS_AT || !LOGIC_LIMITS_ON() || (node=logic_ed_current(e))<0) return;
    if(!(text=logic_label(e,(uint32_t)node))) return;
    guest_call(sub_00079D60,6,text,LOGIC_ROW,0u,0u,LOGIC_DONE,0u);
    if(logic_ed.shown++<4u) logic_log("Delay row shown for action node %u",(unsigned)node,0,0);
}

/* WRAP sub_00328BF0 (cdecl menu): the edit screen's Done; our row's click. */
void pc_mapmaker_logic_done(void (*original)(void))
{
    uint32_t menu=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp+4u):0u,e;
    unsigned used,d;
    int node;
    if(!menu || !MAPMAKER_PTR_OK(menu,LOGIC_MENU_ROW+4u) || WORD_AT(menu+LOGIC_MENU_ROW)!=LOGIC_ROW ||
       !(e=logic_editor()) || (node=logic_ed_current(e))<0) { original();return; }
    d=logic_ed_delay(e,(uint32_t)node,&used,-1);
    if(used) {
        d=logic_next(d);
        logic_ed_set(e,(uint32_t)node,d);
        if(logic_ed.notes++<8u) logic_log("action node %u delay %u tenths of a second (%u operations)",(unsigned)node,d,used);
    } else if(logic_ed.unused++<4u) logic_log("action node %u is in no operation; Delay not stored",(unsigned)node,0,0);
    logic_label(e,(uint32_t)node);              /* the open screen shows the new text */
    MAPMAKER_GUEST_RET();                       /* sub_00328BF0: ret; the screen stays open */
}

/* WRAP sub_00324B60 (ebx = picker menu): rebuilds the current op's action
 * list, which writes 0 into every slot's delay. Put the delays back: the
 * slot's own if the action was there before, else the action's delay in
 * another op. */
void pc_mapmaker_logic_actions(void (*original)(void))
{
    uint32_t e=logic_editor(),cur,op_i,op,n,k,node[LOGIC_SLOTS],delay[LOGIC_SLOTS],before=0,j;
    if(!e || WORD_AT(LOGIC_SCENE)!=LOGIC_EDIT || !MAPMAKER_PTR_OK(cur=WORD_AT(0x00762F98u),0xA8u) ||
       (op_i=WORD_AT(cur+0xA4u))>=logic_ed_ops(e)) { original();return; }
    op=logic_ed_op(e,op_i);
    n=logic_ed_acts(op);
    for(k=0;k<n;k++) { node[k]=WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k);delay[k]=WORD_AT(op+LOGIC_ED_DELAY+4u*k)&0xFFFFu;before++; }
    original();
    n=logic_ed_acts(op);
    for(k=0;k<n;k++) {
        uint32_t id=WORD_AT(op+LOGIC_ED_ACT_NODE+4u*k),d;
        for(j=0;j<before && node[j]!=id;j++) {}
        d=j<before?delay[j]:logic_ed_delay(e,id,NULL,(int)op_i);
        WORD_AT(op+LOGIC_ED_DELAY+4u*k)=d;
    }
}
#endif
