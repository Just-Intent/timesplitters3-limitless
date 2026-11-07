/* US 45410066 AI navigation: the group-level route search in C.
 *
 *   WRAP sub_0009B540 -> pc_mapmaker_pads_group_route   (area pads)
 *
 * sub_0009B540(start pad, target pad, int16 *route, max) is the A* over pad
 * groups that every AI runs when its target pad changes (sub_001596F0, from
 * sub_0015D500 "move to pad"; chasing AIs re-plan whenever the player steps
 * onto another pad, twice when the target is unreachable). Its nodes are
 * 16-byte records in the pool 0x008E0000 (pads.json, 8192 nodes): +0 next,
 * +2 parent, +4 pad, +6 link (int16), +8 g, +0xC f (floats); open list
 * [0x00712048] sorted by f (a new node goes after equal f), closed list
 * [0x00712040], free list [0x0070E170], bump counter [0x00711FF8]. Every
 * expansion walks the open and the closed list once per link of the group
 * (sub_0009B2F0 0x0009B453, sub_0009B160), the sorted insert walks the open
 * list (sub_0009B090) and the goal test walks it again (sub_0009B190), so a
 * search over a map of ~1,000 groups took millions of lifted list steps.
 *
 * This is the same algorithm in host code on the same guest memory: the same
 * x87 arithmetic as the lift has it (double, rounded to float where the
 * guest stores), the same lists, node order, free list and counter, the same
 * route words and return value, the same globals ([0x00715EDC] target,
 * [0x0071204C] target group, [0x00715F48] the goal node's f). Membership
 * counts per link (open, closed) and per group (open) skip a walk whose
 * answer is "none"; a walk that can succeed is done as the original does it.
 * The node insert keeps pc_mapmaker_pads_group_node's bound (no node past
 * 8192). TSFP_MAPMAKER_ASTAR=0 runs the lifted original; =1 is the default;
 * =2 walks every list (no counts) for comparison. stderr: the first two
 * searches and then every 4096th, with totals.
 *
 * Requires the includer's WORD_AT, MAPMAKER_PTR_OK (pc_mapmaker_limits.inc; fixtures
 * fall back to guest_writable) and g_esp, g_eax. */
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) guest_writable((uint32_t)(p),(size_t)(n))
#endif
/* Pointer checks here run per list step: never guest_writable (a VirtualQuery
 * each call in the game), only the range test of pc_mapmaker_limits.inc. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* no fused multiply-add: every product is rounded as the guest rounds it */
#if defined(_MSC_VER)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#define MAPMAKER_ASTAR_POOL 0x008E0000u       /* pads.json pads-astar-group-pool (old 0x0070E178) */
#define MAPMAKER_ASTAR_NODES 8192u            /* = MAPMAKER_PADS_GROUP_NODES */
#define MAPMAKER_ASTAR_FREE 0x0070E170u
#define MAPMAKER_ASTAR_COUNTER 0x00711FF8u
#define MAPMAKER_ASTAR_CLOSED 0x00712040u
#define MAPMAKER_ASTAR_OPEN 0x00712048u
#define MAPMAKER_ASTAR_GOAL_GROUP 0x0071204Cu
#define MAPMAKER_ASTAR_TARGET 0x00715EDCu
#define MAPMAKER_ASTAR_GOAL_F 0x00715F48u
#define MAPMAKER_ASTAR_PADS 0x00715F2Cu       /* -> pads, 0x20 each: +8 group, +0xC/+0x10/+0x14 position */
#define MAPMAKER_ASTAR_LINKS 0x00715F30u      /* -> links, 0x14 each: +0 flags, +4/+8 pads, +0xC factor */
#define MAPMAKER_ASTAR_GROUPS 0x007B6770u     /* -> group pointers: {n, links, ..., +0x18 + n*4: link ids} */
#define MAPMAKER_ASTAR_MASK 0x004D9788u       /* link flags that close a link */
#define MAPMAKER_ASTAR_DOORS 0x00715F50u      /* nonzero: flag-2 links may be crossed backwards */
#define MAPMAKER_ASTAR_K_BLOCKED 0x0047D408u  /* 500000.0: cost of a flag-0x20000 link */
#define MAPMAKER_ASTAR_K_SLOW 0x00475D18u     /* 3.0: flag-0x40 links */
#define MAPMAKER_ASTAR_STEPS 0x10000u         /* a list walk longer than the pool is a cycle: stop */

static struct {
    uint32_t gen;
    unsigned long long searches,failed,expansions,walks,skipped,max_expansions;
    double ms;
    struct { uint32_t gen; uint16_t open,closed; } link[0x10000];
    struct { uint32_t gen; uint16_t open; } group[0x10000];
    int walk_all;
} mapmaker_astar;

static uint32_t mapmaker_astar_node(int32_t i) { return MAPMAKER_ASTAR_POOL+(uint32_t)i*16u; }
static int16_t mapmaker_astar_w(uint32_t a) { return (int16_t)*(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void mapmaker_astar_set_w(uint32_t a,int32_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=(uint16_t)v; }
static float mapmaker_astar_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static uint32_t mapmaker_astar_bits(float f) { uint32_t w; memcpy(&w,&f,4); return w; }
static int mapmaker_astar_node_ok(int32_t i) { return i>=0 && MAPMAKER_PTR_OK(mapmaker_astar_node(i),16u); }

/* group of a pad (movsx word pad+8) */
static int32_t mapmaker_astar_group(int32_t pad)
{
    uint32_t a=WORD_AT(MAPMAKER_ASTAR_PADS)+(uint32_t)pad*0x20u+8u;
    return MAPMAKER_PTR_OK(a,2u)?mapmaker_astar_w(a):-1;
}

/* membership counts (one generation per search) */
static uint16_t *mapmaker_astar_link_count(int32_t link,int closed)
{
    uint16_t k=(uint16_t)link;
    if(mapmaker_astar.link[k].gen!=mapmaker_astar.gen) {
        mapmaker_astar.link[k].gen=mapmaker_astar.gen;mapmaker_astar.link[k].open=mapmaker_astar.link[k].closed=0;
    }
    return closed?&mapmaker_astar.link[k].closed:&mapmaker_astar.link[k].open;
}
static uint16_t *mapmaker_astar_group_count(int32_t group)
{
    uint16_t k=(uint16_t)group;
    if(mapmaker_astar.group[k].gen!=mapmaker_astar.gen) { mapmaker_astar.group[k].gen=mapmaker_astar.gen;mapmaker_astar.group[k].open=0; }
    return &mapmaker_astar.group[k].open;
}
static void mapmaker_astar_count(int32_t node,int closed,int delta)
{
    uint32_t n=mapmaker_astar_node(node);
    uint16_t *c=mapmaker_astar_link_count(mapmaker_astar_w(n+6u),closed);
    *c=(uint16_t)(*c+delta);
    if(!closed) { c=mapmaker_astar_group_count(mapmaker_astar_group(mapmaker_astar_w(n+4u)));*c=(uint16_t)(*c+delta); }
}

/* sub_0009B160 / the open walk of 0x0009B453: first node of a list with link */
static int32_t mapmaker_astar_find(uint32_t head,int32_t link,int closed)
{
    int32_t i=(int32_t)WORD_AT(head);
    unsigned steps=0;
    if(!mapmaker_astar.walk_all && (link!=(int16_t)link || !*mapmaker_astar_link_count(link,closed))) {
        mapmaker_astar.skipped++;
        return -1;
    }
    mapmaker_astar.walks++;
    for(;i>=0 && mapmaker_astar_node_ok(i) && steps<MAPMAKER_ASTAR_STEPS;steps++) {
        uint32_t n=mapmaker_astar_node(i);
        if(mapmaker_astar_w(n+6u)==link) return i;
        i=mapmaker_astar_w(n);
    }
    return -1;
}

/* sub_0009B200(eax = &head, esi = node): unlink, then onto the free list */
static void mapmaker_astar_remove(uint32_t head,int32_t node,int closed)
{
    int32_t e=(int32_t)WORD_AT(head);
    unsigned steps=0;
    if(!mapmaker_astar_node_ok(node)) return;
    if(e==node) {
        WORD_AT(head)=(uint32_t)(int32_t)mapmaker_astar_w(mapmaker_astar_node(node));
        mapmaker_astar_count(node,closed,-1);
    } else
        for(;e>=0 && mapmaker_astar_node_ok(e) && steps<MAPMAKER_ASTAR_STEPS;steps++) {
            int32_t next=mapmaker_astar_w(mapmaker_astar_node(e));
            if(next==node) {
                mapmaker_astar_set_w(mapmaker_astar_node(e),mapmaker_astar_w(mapmaker_astar_node(node)));
                mapmaker_astar_count(node,closed,-1);
                break;
            }
            e=next;
        }
    mapmaker_astar_set_w(mapmaker_astar_node(node),(int32_t)WORD_AT(MAPMAKER_ASTAR_FREE));
    WORD_AT(MAPMAKER_ASTAR_FREE)=(uint32_t)node;
}

/* sub_0009B090(parent, pad, link, g) with xmm0 = f, behind pc_mapmaker_pads_group_node's bound */
static void mapmaker_astar_insert(int32_t parent,int32_t pad,int32_t link,uint32_t g,float f)
{
    int32_t node=(int32_t)WORD_AT(MAPMAKER_ASTAR_FREE),head=(int32_t)WORD_AT(MAPMAKER_ASTAR_OPEN);
    uint32_t n;
    if(node<0 && WORD_AT(MAPMAKER_ASTAR_COUNTER)>=MAPMAKER_ASTAR_NODES) {
#ifdef MAPMAKER_PADS_GROUP_NODES
        mapmaker_pads.group_refused++;
#endif
        return;
    }
    if(node>=0) {
        if(!mapmaker_astar_node_ok(node)) return;
        WORD_AT(MAPMAKER_ASTAR_FREE)=(uint32_t)(int32_t)mapmaker_astar_w(mapmaker_astar_node(node));
    } else {
        node=(int32_t)WORD_AT(MAPMAKER_ASTAR_COUNTER);
        WORD_AT(MAPMAKER_ASTAR_COUNTER)=(uint32_t)node+1u;
        if(!mapmaker_astar_node_ok(node)) return;
    }
    n=mapmaker_astar_node(node);
    if(head<0) {
        WORD_AT(MAPMAKER_ASTAR_OPEN)=(uint32_t)node;
        mapmaker_astar_set_w(n,-1);
    } else if(mapmaker_astar_node_ok(head) && mapmaker_astar_f(mapmaker_astar_node(head)+0xCu)>f) {
        WORD_AT(MAPMAKER_ASTAR_OPEN)=(uint32_t)node;
        mapmaker_astar_set_w(n,head);
    } else {
        int32_t prev=head,next=-1;
        unsigned steps=0;
        mapmaker_astar.walks++;
        for(;steps<MAPMAKER_ASTAR_STEPS && mapmaker_astar_node_ok(prev);steps++) {
            next=mapmaker_astar_w(mapmaker_astar_node(prev));
            if(next<0 || !mapmaker_astar_node_ok(next) || mapmaker_astar_f(mapmaker_astar_node(next)+0xCu)>f) break;
            prev=next;
        }
        mapmaker_astar_set_w(mapmaker_astar_node(prev),node);
        mapmaker_astar_set_w(n,next);
    }
    mapmaker_astar_set_w(n+2u,parent);mapmaker_astar_set_w(n+4u,pad);mapmaker_astar_set_w(n+6u,link);
    WORD_AT(n+8u)=g;WORD_AT(n+0xCu)=mapmaker_astar_bits(f);
    mapmaker_astar_count(node,0,1);
}

/* sub_0009B270(edx = link): link cost */
static float mapmaker_astar_link_cost(uint32_t link)
{
    uint32_t flags=WORD_AT(link),pads=WORD_AT(MAPMAKER_ASTAR_PADS);
    uint32_t p2=pads+WORD_AT(link+8u)*0x20u,p1=pads+WORD_AT(link+4u)*0x20u;
    double a,b,c,s;
    float r;
    if(flags&0x20000u) return mapmaker_astar_f(MAPMAKER_ASTAR_K_BLOCKED);
    a=(double)mapmaker_astar_f(p2+0xCu)-(double)mapmaker_astar_f(p1+0xCu);
    b=(double)mapmaker_astar_f(p2+0x10u)-(double)mapmaker_astar_f(p1+0x10u);
    c=(double)mapmaker_astar_f(p2+0x14u)-(double)mapmaker_astar_f(p1+0x14u);
    s=c*c;s=s+b*b;s=s+a*a;
    s=sqrt(s);
    s=s*(double)mapmaker_astar_f(link+0xCu);
    s=s*(double)mapmaker_astar_f(link+0xCu);
    r=(float)s;
    if(flags&0x40u) r=r*mapmaker_astar_f(MAPMAKER_ASTAR_K_SLOW);
    return r;
}

/* sub_0009B190(&open, group): the first open node in that group */
static int32_t mapmaker_astar_goal(int32_t group)
{
    int32_t i=(int32_t)WORD_AT(MAPMAKER_ASTAR_OPEN);
    unsigned steps=0;
    if(!mapmaker_astar.walk_all && !*mapmaker_astar_group_count(group)) { mapmaker_astar.skipped++;return -1; }
    mapmaker_astar.walks++;
    for(;i>=0 && mapmaker_astar_node_ok(i) && steps<MAPMAKER_ASTAR_STEPS;steps++) {
        uint32_t n=mapmaker_astar_node(i);
        if(mapmaker_astar_group(mapmaker_astar_w(n+4u))==group) {
            WORD_AT(MAPMAKER_ASTAR_GOAL_F)=WORD_AT(n+0xCu);
            return mapmaker_astar_w(n+6u);
        }
        i=mapmaker_astar_w(n);
    }
    return -1;
}

/* sub_0009B2F0(eax = pad, parent, g): expand the pad's group */
static int32_t mapmaker_astar_expand(int32_t pad,int32_t parent,uint32_t g_bits)
{
    uint32_t pads=WORD_AT(MAPMAKER_ASTAR_PADS),from=pads+(uint32_t)pad*0x20u,grp,list,links=WORD_AT(MAPMAKER_ASTAR_LINKS);
    int32_t group=mapmaker_astar_group(pad),n,k;
    float gpar;
    memcpy(&gpar,&g_bits,4);
    mapmaker_astar.expansions++;
    grp=WORD_AT(WORD_AT(MAPMAKER_ASTAR_GROUPS)+(uint32_t)group*4u);
    if(!MAPMAKER_PTR_OK(grp,8u)) return mapmaker_astar_goal((int32_t)WORD_AT(MAPMAKER_ASTAR_GOAL_GROUP));
    list=grp+WORD_AT(grp)*4u+0x18u;
    n=(int32_t)WORD_AT(grp+4u);
    for(k=0;k<n;k++) {
        int32_t id=(int32_t)WORD_AT(list+4u*(uint32_t)k),near_pad,to_pad,found;
        uint32_t L=links+(uint32_t)id*0x14u,flags,np,tp,target;
        float dx,dy,dz,c,g_new,f;
        double s,a,b,e,h;
        if(!MAPMAKER_PTR_OK(L,0x14u)) continue;
        flags=WORD_AT(L);
        if(!(flags&0x10000u) || (flags&WORD_AT(MAPMAKER_ASTAR_MASK))) continue;
        to_pad=(int32_t)WORD_AT(L+4u);
        if(mapmaker_astar_group(to_pad)==group) { near_pad=to_pad;to_pad=(int32_t)WORD_AT(L+8u); }
        else {
            if((flags&2u) && !WORD_AT(MAPMAKER_ASTAR_DOORS)) continue;
            near_pad=(int32_t)WORD_AT(L+8u);
        }
        np=pads+(uint32_t)near_pad*0x20u;
        dx=(float)((double)mapmaker_astar_f(np+0xCu)-(double)mapmaker_astar_f(from+0xCu));
        dy=(float)((double)mapmaker_astar_f(np+0x10u)-(double)mapmaker_astar_f(from+0x10u));
        dz=(float)((double)mapmaker_astar_f(np+0x14u)-(double)mapmaker_astar_f(from+0x14u));
        c=mapmaker_astar_link_cost(L);
        s=(double)dz*(double)dz;s=s+(double)dy*(double)dy;s=s+(double)dx*(double)dx;
        s=sqrt(s);
        s=(double)c+s;
        s=s+(double)gpar;
        g_new=(float)s;
        target=pads+WORD_AT(MAPMAKER_ASTAR_TARGET)*0x20u;tp=pads+(uint32_t)to_pad*0x20u;
        a=(double)mapmaker_astar_f(target+0xCu)-(double)mapmaker_astar_f(tp+0xCu);
        b=(double)mapmaker_astar_f(target+0x10u)-(double)mapmaker_astar_f(tp+0x10u);
        e=(double)mapmaker_astar_f(target+0x14u)-(double)mapmaker_astar_f(tp+0x14u);
        h=e*e;h=h+b*b;h=h+a*a;
        h=sqrt(h);
        h=h*(double)mapmaker_astar_f(L+0xCu);
        h=h+(double)g_new;
        f=(float)h;
        found=mapmaker_astar_find(MAPMAKER_ASTAR_OPEN,id,0);
        if(found>=0) {
            if(!(mapmaker_astar_f(mapmaker_astar_node(found)+0xCu)>f)) continue;
            mapmaker_astar_remove(MAPMAKER_ASTAR_OPEN,found,0);
        } else {
            found=mapmaker_astar_find(MAPMAKER_ASTAR_CLOSED,id,1);
            if(found>=0) {
                uint32_t fn=mapmaker_astar_node(found);
                if(!(mapmaker_astar_f(fn+0xCu)>f) || mapmaker_astar_w(fn+4u)!=to_pad) continue;
                mapmaker_astar_remove(MAPMAKER_ASTAR_CLOSED,found,1);
            }
        }
        mapmaker_astar_insert(parent,to_pad,id,mapmaker_astar_bits(g_new),f);
    }
    return mapmaker_astar_goal((int32_t)WORD_AT(MAPMAKER_ASTAR_GOAL_GROUP));
}

/* sub_0009B540(start, target, route, max) -> route words */
static uint32_t mapmaker_astar_search(int32_t start,int32_t target,uint32_t route,int32_t max)
{
    int32_t esi=-1,goal,count=0,before=(int32_t)mapmaker_astar.expansions;
    WORD_AT(MAPMAKER_ASTAR_TARGET)=(uint32_t)target;
    WORD_AT(MAPMAKER_ASTAR_OPEN)=0xFFFFFFFFu;WORD_AT(MAPMAKER_ASTAR_CLOSED)=0xFFFFFFFFu;
    WORD_AT(MAPMAKER_ASTAR_FREE)=0xFFFFFFFFu;WORD_AT(MAPMAKER_ASTAR_COUNTER)=0;
    WORD_AT(MAPMAKER_ASTAR_GOAL_GROUP)=(uint32_t)mapmaker_astar_group(target);
    if(++mapmaker_astar.gen==0) {                       /* wrapped: clear the stamps */
        memset(mapmaker_astar.link,0,sizeof mapmaker_astar.link);memset(mapmaker_astar.group,0,sizeof mapmaker_astar.group);
        mapmaker_astar.gen=1;
    }
    goal=mapmaker_astar_expand(start,-1,0u);
    for(;;) {
        int32_t node=(int32_t)WORD_AT(MAPMAKER_ASTAR_OPEN);
        uint32_t n,g;
        if(node<0) { mapmaker_astar.failed++;return 0; }
        if(goal>=0) break;
        if(!mapmaker_astar_node_ok(node)) return 0;
        n=mapmaker_astar_node(node);
        esi=node;
        WORD_AT(MAPMAKER_ASTAR_OPEN)=(uint32_t)(int32_t)mapmaker_astar_w(n);
        g=WORD_AT(n+8u);
        mapmaker_astar_set_w(n,(int32_t)(uint16_t)WORD_AT(MAPMAKER_ASTAR_CLOSED));
        WORD_AT(MAPMAKER_ASTAR_CLOSED)=(uint32_t)node;
        mapmaker_astar_count(node,0,-1);mapmaker_astar_count(node,1,1);
        goal=mapmaker_astar_expand(mapmaker_astar_w(n+4u),node,g);
    }
    if(mapmaker_astar.expansions-(unsigned long long)before>mapmaker_astar.max_expansions)
        mapmaker_astar.max_expansions=mapmaker_astar.expansions-(unsigned long long)before;
    if(max>0) {
        if(MAPMAKER_PTR_OK(route,2u)) mapmaker_astar_set_w(route,goal);
        count=1;max--;
    }
    while(esi>=0 && max>0 && mapmaker_astar_node_ok(esi)) {
        uint32_t n=mapmaker_astar_node(esi);
        if(MAPMAKER_PTR_OK(route+2u*(uint32_t)count,2u)) mapmaker_astar_set_w(route+2u*(uint32_t)count,mapmaker_astar_w(n+6u));
        esi=mapmaker_astar_w(n+2u);
        count++;max--;
    }
    return (uint32_t)count;
}

static int mapmaker_astar_mode(void)
{
    static int mode=-1;
    if(mode<0) {
        const char *e=getenv("TSFP_MAPMAKER_ASTAR");
        mode=e && e[0]=='0'?0:e && e[0]=='2'?2:1;
    }
    return mode;
}

void pc_mapmaker_pads_group_route(void (*original)(void))
{
    uint32_t start,target,route,max;
    clock_t t0;
    int mode=mapmaker_astar_mode();
    if(!mode || !MAPMAKER_STACK_OK(20) || !MAPMAKER_PTR_OK(WORD_AT(MAPMAKER_ASTAR_PADS),0x20u)) { original();return; }
    start=WORD_AT(g_esp+4u);target=WORD_AT(g_esp+8u);route=WORD_AT(g_esp+12u);max=WORD_AT(g_esp+16u);
    mapmaker_astar.walk_all=mode==2;
    t0=clock();
    g_eax=mapmaker_astar_search((int32_t)start,(int32_t)target,route,(int32_t)max);
    mapmaker_astar.ms+=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;
    mapmaker_astar.searches++;
    if(mapmaker_astar.searches<=2 || !(mapmaker_astar.searches&4095u))
        fprintf(stderr,"[mapmaker] group routes: %llu searches (%llu found no route), %llu expansions (most %llu), "
                "%llu list walks, %llu skipped by counts, %.0f ms in all%s\n",
                mapmaker_astar.searches,mapmaker_astar.failed,mapmaker_astar.expansions,mapmaker_astar.max_expansions,
                mapmaker_astar.walks,mapmaker_astar.skipped,mapmaker_astar.ms,mode==2?" (every list walked)":"");
    MAPMAKER_GUEST_RET();                       /* sub_0009B540 ends in a plain ret */
}
