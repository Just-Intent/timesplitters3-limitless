/* US 45410066 MapMaker play build: the three portal passes of sub_0008A780
 * in C, with the same results and without their O(portals^2) scans.
 *
 *   WRAP sub_000895F0 -> pc_mapmaker_rooms_portal_link   (area rooms; acts
 *                        only on the call from sub_0008A780, return 0x0008BAB2)
 *
 * sub_0008A780 (MapMaker play map build) joins the portals of neighbouring
 * pieces after copying them into the portal table [0x0070878C] (entries
 * 1..count, count [0x007B9754]; rooms 1..R, R [0x007B975C], room record r at
 * [0x00708788] + r*0xB4, its portal list pointer at +4: {n, ids...}):
 *
 *   1. pairing 0x0008AE03..0x0008B388: for i = 1..count-1 with a room < 0,
 *      for j = i+1..count with a room < 0: opposite or parallel normals
 *      (dot < -0.999 or > 0.001), centroids, box minima and box maxima
 *      within sqrt(0.2501), sqrt(0.2501) and sqrt(1.2501): each takes the
 *      other's room, the smaller box is flagged off (byte esp+0x1280+id) and
 *      every room list's id of it becomes the kept one; repeated until a pass
 *      joins nothing. Every unpaired pair was tested: count^2/2 tests with
 *      two vertex loops each (lifted: about a minute at 10,000 portals).
 *   2. removal 0x0008B38E..0x0008B449: a portal still unpaired leaves the
 *      first place it has in each room list (flag off when found).
 *   3. coplanar merge 0x0008B460..0x0008BA5B: for each wall-like quad
 *      (|ny| <= 0.9, 4 vertices, paired, flagged) the later portals between
 *      the same two rooms (up to 24 in all) become one quad of their box,
 *      with a recomputed normal (rooms swapped when it turned), and the others
 *      leave the room lists (flag off). Again a scan over all later portals.
 *
 * rooms.json makes the three loop counts (0x0008AE03, 0x0008B38E,
 * 0x0008B43E: mov ecx,[0x007B9754]) read MAPMAKER_PORTALS_ZERO, so the guest
 * skips all three, and this hook does them, in the same order, before the
 * first reader (sub_000895F0, portal bounds), with the same float operations
 * (single precision, no contraction; rsqrtss is 1/sqrtf as the lift has it)
 * and the same iteration order. Only the candidate search differs: pairing
 * asks a grid of centroid cells (a pair can only join within 0.51 per axis,
 * so the 27 cells around a portal hold every candidate; tested in ascending
 * order, so joins happen in the original order), the merge asks a sort by
 * room pair. TSFP_MAPMAKER_PORTAL_INDEX=0 tests every pair as the original
 * did (same results, for comparison: the stderr state word must be equal).
 * Guest registers after the skipped loops differ from the original's; nothing
 * reads them (sub_0003DFA0 and sub_000895F0 save what they use, the tail of
 * sub_0008A780 sets its own; xmm1/2/6 enter pass 3 as 0, FLT_MAX, -FLT_MAX in
 * both).
 *
 * Requires the includer's WORD_AT, MAPMAKER_PTR_OK (pc_mapmaker_limits.inc; fixtures
 * fall back to guest_writable) and g_esp. */
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

#define MAPMAKER_PORTALS_ZERO 0x008D2200u      /* rooms.json loop counts read this word; never written but 0 */
#define MAPMAKER_PORTALS_RET 0x0008BAB2u       /* return of sub_0008A780's call of sub_000895F0 */
#define MAPMAKER_PORTALS_FRAME 0x1Cu           /* g_esp + this = the loops' esp (return + 6 pushed words) */
#define MAPMAKER_PORTALS_FLAGS 0x1280u         /* rooms.json: flag bytes at esp+0x1280 */
#define MAPMAKER_PORTALS_FLAG_BYTES 0x4000u
#define MAPMAKER_PORTALS_TABLE 0x0070878Cu
#define MAPMAKER_PORTALS_COUNT 0x007B9754u
#define MAPMAKER_PORTALS_ROOMS 0x007B975Cu
#define MAPMAKER_PORTALS_RECORDS 0x00708788u
#define MAPMAKER_PORTALS_GROUP 24              /* 0x0008B524: cmp ebp,0x18 */
#define MAPMAKER_PORTALS_MAX 0x10000u          /* sanity bound on the count (the compile guard keeps <= 0x3FFF) */
/* the guest's float constants, read where the original reads them */
#define MAPMAKER_PORTALS_K_ANTI 0x0047D3A4u    /* -0.999 */
#define MAPMAKER_PORTALS_K_PARA 0x00475DBCu    /* 0.001 (also the merge's turn tolerance) */
#define MAPMAKER_PORTALS_K_BIG 0x00475D5Cu     /* FLT_MAX */
#define MAPMAKER_PORTALS_K_SMALL 0x00478618u   /* -FLT_MAX */
#define MAPMAKER_PORTALS_K_ONE 0x00475C78u     /* 1.0 */
#define MAPMAKER_PORTALS_K_NEAR 0x0047D3A0u    /* 0.2501 */
#define MAPMAKER_PORTALS_K_FAR 0x0047D39Cu     /* 1.2501 */
#define MAPMAKER_PORTALS_K_WALL 0x00476040u    /* 0.9 */
#define MAPMAKER_PORTALS_K_WALL_NEG 0x0047D398u /* -0.9 */
#define MAPMAKER_PORTALS_K_LEN 0x004765F4u     /* 1e-7 */

typedef struct {
    uint32_t ptr;
    int ok;                      /* pointer and vertices readable */
    float n[3],c[3],mn[3],mx[3],size;
    int cell_ok;
    long long cell[3];
} mapmaker_portal;

static struct {
    unsigned long long builds,pairs,dropped,removed,groups,absorbed,tests,missed;
    int pending,brute_env;
} mapmaker_portals;

static float mapmaker_portals_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static void mapmaker_portals_set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static int32_t mapmaker_portals_i(uint32_t a) { return (int32_t)WORD_AT(a); }
static int16_t mapmaker_portals_count16(uint32_t a)
{
    return (int16_t)(uint16_t)(*(uint16_t *)((uintptr_t)a+g_xbox_mem_offset));
}

/* One portal's centroid, box and box size, exactly as both vertex loops of
 * the pairing test compute them (0x0008AF50 for i, 0x0008B040 for j). */
static void mapmaker_portals_measure(mapmaker_portal *p,uint32_t ptr,float pos_max,float neg_max,float one)
{
    float s0=0.0f,s1=0.0f,s2=0.0f,inv,d0,d1,d2,t;
    int k,cnt;
    memset(p,0,sizeof *p);
    p->ptr=ptr;
    if(!MAPMAKER_PTR_OK(ptr,0x18u)) return;
    cnt=mapmaker_portals_count16(ptr+0x14u);
    if(cnt>0 && !MAPMAKER_PTR_OK(ptr+0x18u,(size_t)cnt*12u)) return;
    p->ok=1;
    p->n[0]=mapmaker_portals_f(ptr+8u);p->n[1]=mapmaker_portals_f(ptr+0xCu);p->n[2]=mapmaker_portals_f(ptr+0x10u);
    p->mn[0]=p->mn[1]=p->mn[2]=pos_max;
    p->mx[0]=p->mx[1]=p->mx[2]=neg_max;
    for(k=0;k<cnt;k++) {
        uint32_t v=ptr+0x18u+12u*(uint32_t)k;
        float x=mapmaker_portals_f(v),y=mapmaker_portals_f(v+4u),z=mapmaker_portals_f(v+8u);
        s0=x+s0;s1=y+s1;s2=s2+z;
        if(x>p->mx[0]) p->mx[0]=x;
        if(y>p->mx[1]) p->mx[1]=y;
        if(z>p->mx[2]) p->mx[2]=z;
        if(p->mn[0]>x) p->mn[0]=x;
        if(p->mn[1]>y) p->mn[1]=y;
        if(p->mn[2]>z) p->mn[2]=z;
    }
    inv=one/(float)cnt;
    p->c[0]=inv*s0;p->c[1]=inv*s1;p->c[2]=inv*s2;
    d0=p->mx[0]-p->mn[0];d1=p->mx[1]-p->mn[1];d2=p->mx[2]-p->mn[2];
    p->size=d2*d2;t=d1*d1;p->size=p->size+t;t=d0*d0;p->size=p->size+t;
    p->cell_ok=1;
    for(k=0;k<3;k++) {
        double c=(double)p->c[k];
        if(!(c>-1e12 && c<1e12)) { p->cell_ok=0;break; }   /* NaN or inf: joins nothing; huge: no cell */
        p->cell[k]=(long long)floor(c);
    }
}

/* The pairing test of 0x0008AE87..0x0008B20C for i < j. */
static int mapmaker_portals_join_test(const mapmaker_portal *i,const mapmaker_portal *j,
                                      float anti,float para,float near_,float far_)
{
    float dot,t,u,d0,d1,d2;
    dot=j->n[2]*i->n[2];t=j->n[1]*i->n[1];dot=dot+t;t=j->n[0]*i->n[0];dot=dot+t;
    if(!(anti>dot) && !(dot>para)) return 0;
    d0=j->c[0]-i->c[0];d1=j->c[1]-i->c[1];d2=j->c[2]-i->c[2];
    t=d2*d2;u=d1*d1;t=t+u;u=d0*d0;t=t+u;
    if(!(near_>=t)) return 0;
    d0=j->mn[0]-i->mn[0];d1=j->mn[1]-i->mn[1];d2=j->mn[2]-i->mn[2];
    t=d2*d2;u=d1*d1;t=t+u;u=d0*d0;t=t+u;
    if(!(near_>=t)) return 0;
    d0=j->mx[0]-i->mx[0];d1=j->mx[1]-i->mx[1];d2=j->mx[2]-i->mx[2];
    t=d2*d2;u=d1*d1;t=t+u;u=d0*d0;t=t+u;
    return far_>=t;
}

/* Every room list: replace (all = 1) or drop the first of (all = 0) an id. */
static uint32_t mapmaker_portals_list(int32_t k)
{
    uint32_t at=WORD_AT(MAPMAKER_PORTALS_RECORDS)+(uint32_t)k*0xB4u+0xB8u;
    return MAPMAKER_PTR_OK(at,4u)?WORD_AT(at):0u;
}
static void mapmaker_portals_replace(int32_t rooms,uint32_t from,uint32_t to)
{
    int32_t k,n,e;
    for(k=0;k<rooms;k++) {
        uint32_t list=mapmaker_portals_list(k);
        if(!MAPMAKER_PTR_OK(list,4u)) continue;
        n=mapmaker_portals_i(list);
        if(n>0 && !MAPMAKER_PTR_OK(list,4u+(size_t)n*4u)) continue;
        for(e=0;e<n;e++) if(WORD_AT(list+4u+4u*(uint32_t)e)==from) WORD_AT(list+4u+4u*(uint32_t)e)=to;
    }
}
static int mapmaker_portals_remove(int32_t rooms,uint32_t id)
{
    int32_t k,n,m;
    int found=0;
    for(k=0;k<rooms;k++) {
        uint32_t list=mapmaker_portals_list(k);
        if(!MAPMAKER_PTR_OK(list,4u)) continue;
        n=mapmaker_portals_i(list);
        if(n<=0 || !MAPMAKER_PTR_OK(list,4u+(size_t)n*4u)) continue;
        for(m=0;m<n && WORD_AT(list+4u+4u*(uint32_t)m)!=id;m++) {}
        if(m==n) continue;
        if(m<n-1) memmove((char *)((uintptr_t)list+4u+4u*(uint32_t)m+g_xbox_mem_offset),
                          (char *)((uintptr_t)list+8u+4u*(uint32_t)m+g_xbox_mem_offset),(size_t)(n-1-m)*4u);
        WORD_AT(list)=(uint32_t)(n-1);
        found=1;
    }
    return found;
}

/* Centroid cell grid: chained buckets over 27 neighbouring cells. */
typedef struct { long long key[3]; int32_t head; } mapmaker_portals_bucket;
static uint32_t mapmaker_portals_hash(const long long *c)
{
    unsigned long long h=(unsigned long long)c[0]*0x9E3779B97F4A7C15ull ^ (unsigned long long)c[1]*0xC2B2AE3D27D4EB4Full ^
                         (unsigned long long)c[2]*0x165667B19E3779F9ull;
    return (uint32_t)(h>>32 ^ h);
}
static int32_t *mapmaker_portals_slot(mapmaker_portals_bucket *b,uint32_t mask,const long long *c,int add)
{
    uint32_t h=mapmaker_portals_hash(c)&mask;
    for(;;h=(h+1u)&mask) {
        if(b[h].head==-2) {
            if(!add) return NULL;
            memcpy(b[h].key,c,sizeof b[h].key);b[h].head=-1;
            return &b[h].head;
        }
        if(!memcmp(b[h].key,c,sizeof b[h].key)) return &b[h].head;
    }
}
static int mapmaker_portals_cmp_u32(const void *a,const void *b)
{
    uint32_t x=*(const uint32_t *)a,y=*(const uint32_t *)b;
    return x<y?-1:x>y;
}
typedef struct { unsigned long long key; uint32_t id; } mapmaker_portals_pairkey;
static int mapmaker_portals_cmp_pair(const void *a,const void *b)
{
    const mapmaker_portals_pairkey *x=a,*y=b;
    if(x->key!=y->key) return x->key<y->key?-1:1;
    return x->id<y->id?-1:x->id>y->id;
}

/* Pass 3's merge of list[0..n-1] (0x0008B544..0x0008BA43). */
static void mapmaker_portals_merge(const uint32_t *ptr,const uint32_t *list,int n,int32_t rooms,uint8_t *flags,
                                   float pos_max,float neg_max,float one,float tol,float len_min)
{
    const float zero=0.0f;                     /* xmm1: 0 on entry to pass 3 */
    float mx0=neg_max,mx1=neg_max,mx2=neg_max,mn0=pos_max,mn1=pos_max,mn2=pos_max;
    float o0,o1,o2,dx,dy,dz,nx,ny,nz,t,u,len;
    uint32_t f=ptr[list[0]],r0,r1;
    int m,k,turned=0;
    for(m=0;m<n;m++) {
        uint32_t e=ptr[list[m]];
        int cnt=mapmaker_portals_count16(e+0x14u);
        if(cnt>0 && !MAPMAKER_PTR_OK(e+0x18u,(size_t)cnt*12u)) continue;
        for(k=0;k<cnt;k++) {
            uint32_t v=e+0x18u+12u*(uint32_t)k;
            float x=mapmaker_portals_f(v),y=mapmaker_portals_f(v+4u),z=mapmaker_portals_f(v+8u);
            if(x>mx0) mx0=x;
            if(y>mx1) mx1=y;
            if(z>mx2) mx2=z;
            if(mn0>x) mn0=x;
            if(mn1>y) mn1=y;
            if(mn2>z) mn2=z;
        }
    }
    /* the merged quad: (max x,max y,max z) (max x,min y,max z) (min x,min y,min z) (min x,max y,min z) */
    mapmaker_portals_set_f(f+0x1Cu,mx1);mapmaker_portals_set_f(f+0x40u,mx1);
    dy=mx1-mn1;
    mapmaker_portals_set_f(f+0x30u,mn0);mapmaker_portals_set_f(f+0x38u,mn2);
    mapmaker_portals_set_f(f+0x28u,mn1);mapmaker_portals_set_f(f+0x34u,mn1);
    o0=mapmaker_portals_f(f+8u);o1=mapmaker_portals_f(f+0xCu);
    mapmaker_portals_set_f(f+0x3Cu,mn0);
    o2=mapmaker_portals_f(f+0x10u);
    mapmaker_portals_set_f(f+0x18u,mx0);mapmaker_portals_set_f(f+0x24u,mx0);
    dx=mx0-mn0;
    mapmaker_portals_set_f(f+0x20u,mx2);mapmaker_portals_set_f(f+0x2Cu,mx2);
    dz=mx2-mn2;
    mapmaker_portals_set_f(f+0x44u,mn2);
    /* normal (dz*dy - 0*0, dx*0 - dz*0, 0*0 - dx*dy) */
    t=dz*dy;u=zero*zero;nx=t-u;
    t=dz*zero;u=dx*zero;ny=u-t;
    t=dx*dy;u=zero*zero;nz=u-t;
    mapmaker_portals_set_f(f+8u,nx);mapmaker_portals_set_f(f+0x10u,nz);
    t=nz*nz;len=nx*nx;len=len+t;t=ny*ny;len=len+t;
    mapmaker_portals_set_f(f+0xCu,ny);
    if(len>len_min) {
        float r=1.0f/sqrtf(len);           /* rsqrtss as lifted */
        r=r*one;
        nx=r*nx;ny=ny*r;nz=r*nz;
        mapmaker_portals_set_f(f+8u,nx);mapmaker_portals_set_f(f+0xCu,ny);mapmaker_portals_set_f(f+0x10u,nz);
    }
    t=o0-tol;if(t>nx) turned=1;
    t=o0+tol;if(nx>t) turned=1;
    t=o1-tol;if(t>ny) turned=1;
    t=o1+tol;if(ny>t) turned=1;
    t=o2-tol;if(t>nz) turned=1;
    t=o2+tol;if(nz>t) turned=1;
    if(turned) { r0=WORD_AT(f);r1=WORD_AT(f+4u);WORD_AT(f)=r1;WORD_AT(f+4u)=r0; }
    for(m=1;m<n;m++)
        if(mapmaker_portals_remove(rooms,list[m])) flags[list[m]]=0;
}

/* FNV-1a over the portal rooms, flags and room lists: equal in both modes. */
static uint32_t mapmaker_portals_state(const uint32_t *ptr,uint32_t count,int32_t rooms,const uint8_t *flags)
{
    uint32_t h=2166136261u,p;
    int32_t k,n,e;
#define MAPMAKER_PORTALS_MIX(w) do { uint32_t w_=(w); int b_; for(b_=0;b_<4;b_++) { h^=(w_>>(8*b_))&0xFFu;h*=16777619u; } } while(0)
    for(p=1;p<=count;p++) {
        if(!MAPMAKER_PTR_OK(ptr[p],0x18u)) { MAPMAKER_PORTALS_MIX(0xFFFFFFFFu);continue; }
        MAPMAKER_PORTALS_MIX(WORD_AT(ptr[p]));MAPMAKER_PORTALS_MIX(WORD_AT(ptr[p]+4u));
        MAPMAKER_PORTALS_MIX(WORD_AT(ptr[p]+8u));MAPMAKER_PORTALS_MIX(WORD_AT(ptr[p]+0xCu));
        MAPMAKER_PORTALS_MIX(WORD_AT(ptr[p]+0x10u));MAPMAKER_PORTALS_MIX(flags[p]);
    }
    for(k=0;k<rooms;k++) {
        uint32_t list=mapmaker_portals_list(k);
        if(!MAPMAKER_PTR_OK(list,4u)) continue;
        n=mapmaker_portals_i(list);
        MAPMAKER_PORTALS_MIX((uint32_t)n);
        if(n>0 && MAPMAKER_PTR_OK(list,4u+(size_t)n*4u))
            for(e=0;e<n;e++) MAPMAKER_PORTALS_MIX(WORD_AT(list+4u+4u*(uint32_t)e));
    }
#undef MAPMAKER_PORTALS_MIX
    return h;
}

static int mapmaker_portals_brute(void)
{
    const char *e=getenv("TSFP_MAPMAKER_PORTAL_INDEX");
    return e && e[0]=='0';
}

/* The three passes on the state at 0x0008AE03 (frame = the loops' esp). */
static void mapmaker_portals_link(uint32_t frame)
{
    int32_t count=mapmaker_portals_i(MAPMAKER_PORTALS_COUNT),rooms=mapmaker_portals_i(MAPMAKER_PORTALS_ROOMS);
    uint32_t table=WORD_AT(MAPMAKER_PORTALS_TABLE),p,q,*ptr=NULL,*cand=NULL,*members=NULL;
    float anti=mapmaker_portals_f(MAPMAKER_PORTALS_K_ANTI),para=mapmaker_portals_f(MAPMAKER_PORTALS_K_PARA);
    float pos_max=mapmaker_portals_f(MAPMAKER_PORTALS_K_BIG),neg_max=mapmaker_portals_f(MAPMAKER_PORTALS_K_SMALL);
    float one=mapmaker_portals_f(MAPMAKER_PORTALS_K_ONE),near_=mapmaker_portals_f(MAPMAKER_PORTALS_K_NEAR);
    float far_=mapmaker_portals_f(MAPMAKER_PORTALS_K_FAR),wall=mapmaker_portals_f(MAPMAKER_PORTALS_K_WALL);
    float wall_neg=mapmaker_portals_f(MAPMAKER_PORTALS_K_WALL_NEG),len_min=mapmaker_portals_f(MAPMAKER_PORTALS_K_LEN);
    mapmaker_portal *por=NULL;
    mapmaker_portals_bucket *grid=NULL;
    mapmaker_portals_pairkey *keys=NULL;
    int32_t *next=NULL,*pos=NULL;
    uint8_t *flags=NULL;
    uint32_t mask=0,list[MAPMAKER_PORTALS_GROUP];
    unsigned long long pairs=0,dropped=0,removed=0,groups=0,absorbed=0,tests=0;
    int brute=mapmaker_portals_brute(),indexed=0;
    clock_t start=clock();
    mapmaker_portals.builds++;
    mapmaker_portals.pending=0;
    if(count<=0 || (uint32_t)count>=MAPMAKER_PORTALS_MAX || !MAPMAKER_PTR_OK(table,((size_t)count+1u)*4u)) {
        fprintf(stderr,"[mapmaker] portal links: %d portals, nothing to join\n",(int)count);
        return;
    }
    ptr=calloc((size_t)count+1u,sizeof *ptr);
    por=calloc((size_t)count+1u,sizeof *por);
    flags=malloc((size_t)count+1u);
    cand=calloc((size_t)count+1u,sizeof *cand);
    if(!ptr || !por || !flags || !cand) { brute=1;goto out_of_memory; }
    memset(flags,1,(size_t)count+1u);                     /* 0x0008ADF0: 0x01 bytes */
    for(p=1;p<=(uint32_t)count;p++) {
        ptr[p]=WORD_AT(table+4u*p);
        mapmaker_portals_measure(&por[p],ptr[p],pos_max,neg_max,one);
    }
    /* centroid cells (no cell for a huge centroid: every pair is then tested) */
    if(!brute) {
        uint32_t cap=64u;
        while(cap<(uint32_t)count*2u) cap<<=1;
        grid=malloc(cap*sizeof *grid);next=malloc(((size_t)count+1u)*sizeof *next);
        if(grid && next) {
            indexed=1;mask=cap-1u;
            for(q=0;q<cap;q++) grid[q].head=-2;
            for(p=(uint32_t)count;p>=1u;p--) {                /* chains ascending */
                int32_t *head;
                next[p]=-1;
                if(!por[p].ok) continue;
                if(!por[p].cell_ok) {
                    double c=(double)por[p].c[0]+(double)por[p].c[1]+(double)por[p].c[2];
                    if(c==c && (c-c)==0.0) { indexed=0;break; }   /* finite but huge: no cell, test every pair */
                    continue;                                 /* NaN/inf centroid: joins nothing */
                }
                head=mapmaker_portals_slot(grid,mask,por[p].cell,1);
                next[p]=*head;*head=(int32_t)p;
            }
        }
    }
out_of_memory:
    if(!ptr || !por || !flags || !cand) {
        fprintf(stderr,"[MAPMAKER] portal links: out of host memory for %d portals; portals left unjoined\n",(int)count);
        free(ptr);free(por);free(flags);free(cand);
        return;
    }
    /* 1. pairing (0x0008AE03): one pass joins all; the original's next pass joined nothing */
    for(p=1;count-1>0 && p<=(uint32_t)count-1u;p++) {
        uint32_t i=p,nc=0,c;
        if(!por[i].ok) continue;
        if(mapmaker_portals_i(ptr[i])>=0 && mapmaker_portals_i(ptr[i]+4u)>=0) continue;
        if(indexed) {
            int dx,dy,dz;
            if(!por[i].cell_ok) continue;
            for(dx=-1;dx<=1;dx++) for(dy=-1;dy<=1;dy++) for(dz=-1;dz<=1;dz++) {
                long long cell[3];
                int32_t *head,j;
                cell[0]=por[i].cell[0]+dx;cell[1]=por[i].cell[1]+dy;cell[2]=por[i].cell[2]+dz;
                head=mapmaker_portals_slot(grid,mask,cell,0);
                if(!head) continue;
                for(j=*head;j>=0;j=next[j]) if((uint32_t)j>i) cand[nc++]=(uint32_t)j;
            }
            qsort(cand,nc,sizeof *cand,mapmaker_portals_cmp_u32);
        } else
            for(q=i+1u;q<=(uint32_t)count;q++) cand[nc++]=q;
        for(c=0;c<nc;c++) {
            uint32_t j=cand[c],keep,drop,ri,rj;
            int32_t j0,eax;
            if(!por[j].ok) continue;
            j0=mapmaker_portals_i(ptr[j]);
            if(j0>=0 && mapmaker_portals_i(ptr[j]+4u)>=0) continue;
            tests++;
            if(!mapmaker_portals_join_test(&por[i],&por[j],anti,para,near_,far_)) continue;
            eax=mapmaker_portals_i(ptr[i]);
            ri=eax>=0?(uint32_t)eax:WORD_AT(ptr[i]+4u);
            rj=j0>=0?(uint32_t)j0:WORD_AT(ptr[j]+4u);
            if(por[i].size>por[j].size) { keep=i;drop=j; } else { keep=j;drop=i; }
            flags[drop]=0;
            if(eax>=0) WORD_AT(ptr[i]+4u)=rj; else WORD_AT(ptr[i])=rj;
            if(mapmaker_portals_i(ptr[j])>=0) WORD_AT(ptr[j]+4u)=ri; else WORD_AT(ptr[j])=ri;
            mapmaker_portals_replace(rooms,drop,keep);
            pairs++;dropped++;
        }
    }
    /* 2. removal (0x0008B38E) */
    for(p=1;p<=(uint32_t)count;p++) {
        if(!MAPMAKER_PTR_OK(ptr[p],8u)) continue;
        if(mapmaker_portals_i(ptr[p])>=0 && mapmaker_portals_i(ptr[p]+4u)>=0) continue;
        if(rooms>0 && mapmaker_portals_remove(rooms,p)) { flags[p]=0;removed++; }
    }
    /* 3. coplanar merge (0x0008B460): members of a room pair in index order */
    keys=malloc(((size_t)count+1u)*sizeof *keys);pos=malloc(((size_t)count+1u)*sizeof *pos);
    if(keys && pos && !brute) {
        uint32_t nk=0;
        for(p=1;p<=(uint32_t)count;p++) {
            int32_t a,b;
            pos[p]=-1;
            if(!MAPMAKER_PTR_OK(ptr[p],8u)) continue;
            a=mapmaker_portals_i(ptr[p]);b=mapmaker_portals_i(ptr[p]+4u);
            if(a<0 || b<0) continue;
            keys[nk].key=a<b?((unsigned long long)(uint32_t)a<<32|(uint32_t)b):((unsigned long long)(uint32_t)b<<32|(uint32_t)a);
            keys[nk].id=p;nk++;
        }
        qsort(keys,nk,sizeof *keys,mapmaker_portals_cmp_pair);
        for(q=0;q<nk;q++) pos[keys[q].id]=(int32_t)q;
        members=cand;                                      /* reused: the group's later members */
        for(p=1;count-1>0 && p<=(uint32_t)count-1u;p++) {
            uint32_t P=ptr[p],nm=0,m;
            int32_t a,b;
            float ny;
            int n=1;
            if(!MAPMAKER_PTR_OK(P,0x18u)) continue;
            ny=mapmaker_portals_f(P+0xCu);
            if(ny>wall || wall_neg>ny || mapmaker_portals_count16(P+0x14u)!=4) continue;
            a=mapmaker_portals_i(P);b=mapmaker_portals_i(P+4u);
            if(a<0 || b<0 || !flags[p] || pos[p]<0) continue;
            for(q=(uint32_t)pos[p]+1u;q<nk && keys[q].key==keys[pos[p]].key;q++) members[nm++]=keys[q].id;
            list[0]=p;
            for(m=0;m<nm && n<MAPMAKER_PORTALS_GROUP;m++) if(flags[members[m]]) list[n++]=members[m];
            if(n<=1) continue;
            mapmaker_portals_merge(ptr,list,n,rooms,flags,pos_max,neg_max,one,para,len_min);
            groups++;absorbed+=(unsigned long long)(n-1);
        }
    } else
        for(p=1;count-1>0 && p<=(uint32_t)count-1u;p++) {
            uint32_t P=ptr[p];
            int32_t a,b;
            float ny;
            int n=1;
            if(!MAPMAKER_PTR_OK(P,0x18u)) continue;
            ny=mapmaker_portals_f(P+0xCu);
            if(ny>wall || wall_neg>ny || mapmaker_portals_count16(P+0x14u)!=4) continue;
            a=mapmaker_portals_i(P);b=mapmaker_portals_i(P+4u);
            if(a<0 || b<0 || !flags[p]) continue;
            list[0]=p;
            for(q=p+1u;q<=(uint32_t)count;q++) {
                int32_t e,f;
                if(!MAPMAKER_PTR_OK(ptr[q],8u)) continue;
                e=mapmaker_portals_i(ptr[q]);
                if(e<0) continue;
                f=mapmaker_portals_i(ptr[q]+4u);
                if(f<0 || !flags[q]) continue;
                if((a==e && b==f) || (a==f && b==e)) { list[n++]=q;if(n==MAPMAKER_PORTALS_GROUP) break; }
            }
            if(n<=1) continue;
            mapmaker_portals_merge(ptr,list,n,rooms,flags,pos_max,neg_max,one,para,len_min);
            groups++;absorbed+=(unsigned long long)(n-1);
        }
    /* the flag bytes as the original leaves them (nothing reads them later) */
    if(MAPMAKER_PTR_OK(frame+MAPMAKER_PORTALS_FLAGS,MAPMAKER_PORTALS_FLAG_BYTES))
        for(p=0;p<=(uint32_t)count && p<MAPMAKER_PORTALS_FLAG_BYTES;p++)
            *(uint8_t *)((uintptr_t)frame+MAPMAKER_PORTALS_FLAGS+p+g_xbox_mem_offset)=p?flags[p]:1u;
    mapmaker_portals.pairs+=pairs;mapmaker_portals.dropped+=dropped;mapmaker_portals.removed+=removed;
    mapmaker_portals.groups+=groups;mapmaker_portals.absorbed+=absorbed;mapmaker_portals.tests+=tests;
    fprintf(stderr,"[mapmaker] portal links: %d portals, %d rooms: %llu pairs joined, %llu unpaired removed, "
            "%llu coplanar groups (%llu absorbed); %llu pair tests (%s), %.0f ms; state 0x%08X\n",
            (int)count,(int)rooms,pairs,removed,groups,absorbed,tests,
            indexed?"centroid grid":"every pair",(double)(clock()-start)*1000.0/CLOCKS_PER_SEC,
            (unsigned)mapmaker_portals_state(ptr,(uint32_t)count,rooms,flags));
    free(ptr);free(por);free(flags);free(cand);free(grid);free(next);free(keys);free(pos);
}

/* sub_000895F0 (no arguments, plain ret): portal bounds. From sub_0008A780
 * it is the first reader of the passes' results. */
void pc_mapmaker_rooms_portal_link(void (*original)(void))
{
    if(WORD_AT(g_esp)==MAPMAKER_PORTALS_RET) mapmaker_portals_link(g_esp+MAPMAKER_PORTALS_FRAME);
    original();
}

/* pc_mapmaker_rooms_compile calls these around sub_0008A780. */
static void mapmaker_portals_build_begin(void)
{
    WORD_AT(MAPMAKER_PORTALS_ZERO)=0;           /* the skipped loops' count */
    mapmaker_portals.pending=1;
}
static void mapmaker_portals_build_end(void)
{
    if(mapmaker_portals.pending) {
        mapmaker_portals.missed++;
        fprintf(stderr,"[MAPMAKER] portal links: the play build returned without its portal pass "
                "(sub_000895F0 hook not reached); portals unjoined\n");
    }
    mapmaker_portals.pending=0;
}
