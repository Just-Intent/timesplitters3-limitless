/* US 45410066 MapMaker rooms: the room tree query in host code.
 *
 *   called from pc_mapmaker_rooms_query (WRAP sub_00087310, pc_mapmaker_rooms.inc)
 *
 * Requires the includer's WORD_AT and g_xbox_mem_offset. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t mapmaker_roomq_u16(uint32_t a) { return *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void mapmaker_roomq_set_u16(uint32_t a,uint16_t v) { *(uint16_t *)((uintptr_t)a+g_xbox_mem_offset)=v; }
static void *mapmaker_roomq_host(uint32_t a) { return (void *)((uintptr_t)a+g_xbox_mem_offset); }

/* sub_00157440(tree, min, max, sub_00087260, list): if the box overlaps the
 * tree's box (+8 min, +0x14 max; sub_00047560), sub_001571C0 walks node 1 of
 * the implicit binary tree (+0 u16 nodes, children 2n and 2n + 1): a node
 * with both top bits set is a leaf (+4 u16 lists, offset node & 0x3FFF:
 * count, rooms) given to the callback; else axis node >> 14 splits the node
 * box at min + (max - min) * ((node & 0x3FFF) * [0x0047DD70]) (1/16383),
 * left (child box max = split) when split >= box min, then right (child box
 * min = split) when box max >= split, in that order. The callback
 * sub_00087260 lists each room once (record +0xB2 bit 0x10, set before the
 * test) as room * 2 | (box overlaps record +0x10..+0x24). sub_00047560
 * fails an axis only on a strict "greater" (NaN overlaps), the walk's two
 * tests are comiss/jb (NaN goes neither way). The callback always returns 0,
 * so the walk visits every reachable leaf. Profiled .77: the lifted walk and
 * callback were about 7% of a HugeMap frame (per AI room lists and
 * collision). TSFP_MAPMAKER_ROOM_QUERY=0 calls the guest walk. */
#if defined(_MSC_VER)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif
#define MAPMAKER_ROOMS_SPLIT_K 0x0047DD70u
#define MAPMAKER_ROOMS_WALK_DEPTH 40u               /* a built tree is a few levels deep; a cycle-free guard only */
static unsigned long long mapmaker_rooms_walk_calls,mapmaker_rooms_walk_leaves;
static int mapmaker_rooms_query_mode(void)
{
    static int mode=-1;
    if(mode<0) { const char *e=getenv("TSFP_MAPMAKER_ROOM_QUERY");mode=!(e && e[0]=='0'); }
    return mode;
}
static float mapmaker_rooms_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
/* sub_00047560(a min, a max, b min, b max) */
static int mapmaker_rooms_overlap(const float *amin,const float *amax,uint32_t bmin,uint32_t bmax)
{
    unsigned k;
    for(k=0;k<3u;k++)
        if(amin[k]>mapmaker_rooms_f(bmax+k*4u) || mapmaker_rooms_f(bmin+k*4u)>amax[k]) return 0;
    return 1;
}
/* sub_00087260(min, max, list, count, rooms) */
static void mapmaker_rooms_walk_leaf(const float *qmin,const float *qmax,uint32_t list,uint32_t rooms,uint32_t count)
{
    uint32_t n=mapmaker_roomq_u16(list),i,records=WORD_AT(0x00708788u);
    mapmaker_rooms_walk_leaves++;
    for(i=0;i<count;i++) {
        uint32_t room=mapmaker_roomq_u16(rooms+i*2u),rec=records+room*0xB4u;
        uint8_t *flag=(uint8_t *)mapmaker_roomq_host(rec+0xB2u);
        if(*flag&0x10u) continue;
        *flag|=0x10u;
        mapmaker_roomq_set_u16(list+2u+n*2u,(uint16_t)(room*2u|(uint32_t)mapmaker_rooms_overlap(qmin,qmax,rec+0x10u,rec+0x1Cu)));
        n++;
    }
    mapmaker_roomq_set_u16(list,(uint16_t)n);
}
/* sub_001571C0(tree, min, max, node, node min, node max, callback, list) */
static void mapmaker_rooms_walk(uint32_t tree,const float *qmin,const float *qmax,uint32_t node,
                                const float *nmin,const float *nmax,uint32_t list,unsigned depth)
{
    uint32_t v=mapmaker_roomq_u16(WORD_AT(tree)+node*2u);
    if((v&0xC000u)==0xC000u) {
        uint32_t at=WORD_AT(tree+4u)+(v&0x3FFFu)*2u;
        mapmaker_rooms_walk_leaf(qmin,qmax,list,at+2u,mapmaker_roomq_u16(at));
        return;
    }
    {
        unsigned axis=v>>14;
        float t=(float)(int32_t)(v&0x3FFFu),split,box[3];
        if(depth>=MAPMAKER_ROOMS_WALK_DEPTH) return;    /* not reached by a built tree */
        t=t*mapmaker_rooms_f(MAPMAKER_ROOMS_SPLIT_K);
        split=nmax[axis]-nmin[axis];split=split*t;split=split+nmin[axis];
        if(split>=qmin[axis]) {
            memcpy(box,nmax,sizeof box);box[axis]=split;
            mapmaker_rooms_walk(tree,qmin,qmax,node*2u,nmin,box,list,depth+1u);
        }
        if(qmax[axis]>=split) {
            memcpy(box,nmin,sizeof box);box[axis]=split;
            mapmaker_rooms_walk(tree,qmin,qmax,node*2u+1u,box,nmax,list,depth+1u);
        }
    }
}
/* sub_00157440(tree, min, max, sub_00087260, list) */
static void mapmaker_rooms_tree_query(uint32_t tree,uint32_t min,uint32_t max,uint32_t list)
{
    float qmin[3],qmax[3],tmin[3],tmax[3];
    unsigned k;
    for(k=0;k<3u;k++) {
        qmin[k]=mapmaker_rooms_f(min+k*4u);qmax[k]=mapmaker_rooms_f(max+k*4u);
        tmin[k]=mapmaker_rooms_f(tree+8u+k*4u);tmax[k]=mapmaker_rooms_f(tree+0x14u+k*4u);
    }
    mapmaker_rooms_walk_calls++;
    if(mapmaker_rooms_overlap(qmin,qmax,tree+8u,tree+0x14u))
        mapmaker_rooms_walk(tree,qmin,qmax,1u,tmin,tmax,list,0u);
    if(mapmaker_rooms_walk_calls==1u || !(mapmaker_rooms_walk_calls&0xFFFFFu))
        fprintf(stderr,"[mapmaker] room queries: %llu calls, %llu leaves (host code)\n",
                mapmaker_rooms_walk_calls,mapmaker_rooms_walk_leaves);
}
