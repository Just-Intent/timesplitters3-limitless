/* US 45410066 AI navigation: the nearest pad, in host code.
 *
 *   WRAP sub_0009BCF0 -> pc_mapmaker_pads_nearest   (area pads)
 *
 * sub_0009BCF0(position, min distance) returns the index of the nearest
 * enabled pad (flag bit 0 of pad +4; pads [0x00715F2C], 0x20 each, position
 * +0xC/+0x10/+0x14, count [0x007B6664]) at least min distance away (squared
 * bound min^2 + 0.01 when min > 0, else 0), or -1. 37 call sites: lost pads
 * of AIs and players (sub_0015D1F0), AI states (sub_0027BEC0, up to 30
 * frames per AI), teleports, bot goals... With the 16,384-pad pool of a
 * full map each call was a lifted walk over every pad (unrolled by 4).
 * This is the same walk in host code: the same squared distances (the
 * first pad of each group of four sums z^2 + x^2 + y^2, the others and the
 * tail x^2 + y^2 + z^2, as the original), the strict "nearer" test, so the
 * lowest index wins ties, and the same bound. TSFP_MAPMAKER_NEAR_PAD=0 runs
 * the original. stderr: `[mapmaker] nearest pad: n calls, m pads tested`
 * (first call, then every 65536th).
 *
 * Requires the includer's WORD_AT, MAPMAKER_PTR_OK (or guest_writable),
 * MAPMAKER_GUEST_RET, g_esp, g_eax. */
#ifndef MAPMAKER_PTR_OK
#define MAPMAKER_PTR_OK(p,n) guest_writable((uint32_t)(p),(size_t)(n))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#define MAPMAKER_NEAR_PADS 0x00715F2Cu
#define MAPMAKER_NEAR_COUNT 0x007B6664u
#define MAPMAKER_NEAR_K_FAR 0x00475D5Cu     /* FLT_MAX */
#define MAPMAKER_NEAR_K_PAD 0x00475D1Cu     /* 0.01 */

static unsigned long long mapmaker_near_calls,mapmaker_near_pads;

static float mapmaker_near_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static int mapmaker_near_mode(void)
{
    static int mode=-1;
    if(mode<0) { const char *e=getenv("TSFP_MAPMAKER_NEAR_PAD");mode=!(e && e[0]=='0'); }
    return mode;
}

static int32_t mapmaker_near_find(uint32_t pos,float min)
{
    const float zero=0.0f;
    float best=mapmaker_near_f(MAPMAKER_NEAR_K_FAR),low,ox,oy,oz;
    uint32_t pads=WORD_AT(MAPMAKER_NEAR_PADS);
    int32_t n=(int32_t)WORD_AT(MAPMAKER_NEAR_COUNT),i,found=-1,unrolled;
    if(zero<min || min!=min) { low=min*min;low=low+mapmaker_near_f(MAPMAKER_NEAR_K_PAD); } else low=zero;
    if(n<=0) return -1;
    if(!MAPMAKER_PTR_OK(pos,12u) || !MAPMAKER_PTR_OK(pads,(size_t)n*0x20u)) return -2;
    ox=mapmaker_near_f(pos);oy=mapmaker_near_f(pos+4u);oz=mapmaker_near_f(pos+8u);
    unrolled=n>=4?((n-4)/4+1)*4:0;               /* groups of four while i < n - 3 */
    mapmaker_near_pads+=(unsigned long long)n;
    for(i=0;i<n;i++) {
        uint32_t p=pads+(uint32_t)i*0x20u;
        float dx,dy,dz,d,t;
        if(!(*(uint8_t *)((uintptr_t)p+4u+g_xbox_mem_offset)&1u)) continue;
        dx=mapmaker_near_f(p+0xCu)-ox;dy=mapmaker_near_f(p+0x10u)-oy;dz=mapmaker_near_f(p+0x14u)-oz;
        if(i<unrolled && !(i&3)) {               /* 0x0009BD48: z^2 + x^2, + y^2 */
            t=dx*dx;d=dz*dz;d=d+t;t=dy*dy;d=d+t;
        } else {                                  /* 0x0009BD93.., tail 0x0009BE99: x^2 + y^2, + z^2 */
            t=dy*dy;d=dx*dx;d=d+t;t=dz*dz;d=d+t;
        }
        if(best>d && !(d<low || d!=d || low!=low)) { best=d;found=i; }
    }
    return found;
}

void pc_mapmaker_pads_nearest(void (*original)(void))
{
    int32_t r;
    float min;
    uint32_t w;
    if(!mapmaker_near_mode() || !MAPMAKER_PTR_OK(g_esp,12u)) { original();return; }
    w=WORD_AT(g_esp+8u);memcpy(&min,&w,4);
    r=mapmaker_near_find(WORD_AT(g_esp+4u),min);
    if(r==-2) { original();return; }              /* a pointer the range test refuses: the original */
    mapmaker_near_calls++;
    if(mapmaker_near_calls==1u || !(mapmaker_near_calls&0xFFFFu))
        fprintf(stderr,"[mapmaker] nearest pad: %llu calls, %llu pads tested (host code)\n",mapmaker_near_calls,mapmaker_near_pads);
    g_eax=(uint32_t)r;
    MAPMAKER_GUEST_RET();                         /* sub_0009BCF0 ends in a plain ret */
}
