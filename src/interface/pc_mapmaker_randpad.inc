/* US 45410066 AI navigation: a random pad in a ring, in host code.
 *
 *   WRAP sub_0006A8B0 -> pc_mapmaker_pads_random   (area pads)
 *
 * sub_0006A8B0(ring, position) tries count/2 random pads (pads [0x00715F2C],
 * count [0x007B6664]) and returns the id (+0) of the first enabled one (flag
 * bit 0; in scene 100 also bit 0x10) whose squared distance lies in the
 * ring (bits 0..7: inner radius, with 0x40000000; bits 8..15: outer, with
 * 0x20000000), or 0. Bots and AIs wander with it (sub_002B4260, three rings
 * per decision; sub_002B5B90). On a full map few of 16,000 pads lie in a
 * 20..40 ring, so a decision cost up to 3 x 8,000 lifted tries, each with the
 * game's random number sub_00153B40 (state [0x004E7950] = state x 0x343FD +
 * 0x269EC3, 64-bit, counter [0x007497C8]) and the x87 scaling to an index.
 * This is the same function in host code: the same draws (the generator is
 * stepped here exactly as sub_00153B40 does), index = floor(r x count /
 * 2^32) as the exact x87 product and __ftol give, the same single-precision
 * distance and tests; the draw is also left where the original leaves it
 * (the caller's argument slot). TSFP_MAPMAKER_RANDOM_PAD=0 runs the original.
 * stderr: `[mapmaker] random pad: n calls, m tries` (first, every 65536th).
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

#define MAPMAKER_RANDPAD_PADS 0x00715F2Cu
#define MAPMAKER_RANDPAD_COUNT 0x007B6664u
#define MAPMAKER_RANDPAD_SCENE 0x0079094Cu
#define MAPMAKER_RANDPAD_K_HALF 0x00475CD4u      /* 0.5 */
#define MAPMAKER_RANDPAD_RNG_LO 0x004E7950u
#define MAPMAKER_RANDPAD_RNG_HI 0x004E7954u
#define MAPMAKER_RANDPAD_RNG_CALLS 0x007497C8u

static unsigned long long mapmaker_randpad_calls,mapmaker_randpad_tries;

static float mapmaker_randpad_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static int mapmaker_randpad_mode(void)
{
    static int mode=-1;
    if(mode<0) { const char *e=getenv("TSFP_MAPMAKER_RANDOM_PAD");mode=!(e && e[0]=='0'); }
    return mode;
}
/* sub_00153B40: the game's generator, returns the new low dword */
static uint32_t mapmaker_randpad_draw(void)
{
    unsigned long long s=(unsigned long long)WORD_AT(MAPMAKER_RANDPAD_RNG_LO)|(unsigned long long)WORD_AT(MAPMAKER_RANDPAD_RNG_HI)<<32;
    WORD_AT(MAPMAKER_RANDPAD_RNG_CALLS)+=1u;
    s=s*0x343FDull+0x269EC3ull;
    WORD_AT(MAPMAKER_RANDPAD_RNG_LO)=(uint32_t)s;WORD_AT(MAPMAKER_RANDPAD_RNG_HI)=(uint32_t)(s>>32);
    return (uint32_t)s;
}

/* returns the pad id or 0; *slot gets the last draw (the original stores it in its argument slot) */
static uint32_t mapmaker_randpad_pick(uint32_t ring,uint32_t pos,uint32_t slot)
{
    float half=mapmaker_randpad_f(MAPMAKER_RANDPAD_K_HALF),t,outer=0.0f,inner=0.0f,ox,oy,oz;
    uint32_t pads=WORD_AT(MAPMAKER_RANDPAD_PADS),p=0,scene=WORD_AT(MAPMAKER_RANDPAD_SCENE);
    int32_t count=(int32_t)WORD_AT(MAPMAKER_RANDPAD_COUNT),tries,i;
    int ok=0;
    t=(float)count;t=t*half;tries=(int32_t)t;   /* cvttss2si */
    if(ring&0x40000000u) { t=(float)(int32_t)(ring&0xFFu);inner=t*t; }
    if(ring&0x20000000u) { t=(float)(int32_t)((uint32_t)((int32_t)ring>>8)&0xFFu);outer=t*t; }
    if(tries<=0) return 0;
    ox=mapmaker_randpad_f(pos);oy=mapmaker_randpad_f(pos+4u);oz=mapmaker_randpad_f(pos+8u);
    for(i=0;i<tries;i++) {
        uint32_t r=mapmaker_randpad_draw(),idx;
        float dx,dy,dz,d,u;
        WORD_AT(slot)=r;
        mapmaker_randpad_tries++;
        idx=(uint32_t)(((unsigned long long)r*(unsigned long long)(uint32_t)count)>>32);
        p=pads+idx*0x20u;
        dy=mapmaker_randpad_f(p+0x10u)-oy;dx=mapmaker_randpad_f(p+0xCu)-ox;dz=mapmaker_randpad_f(p+0x14u)-oz;
        u=dy*dy;d=dz*dz;d=d+u;u=dx*dx;d=d+u;
        ok=1;
        if(d>outer) ok=0;
        else if(inner>d) ok=0;
        if(scene==100u && !(*(uint8_t *)((uintptr_t)p+4u+g_xbox_mem_offset)&0x10u)) { ok=0;continue; }
        if(!ok) continue;
        if(*(uint8_t *)((uintptr_t)p+4u+g_xbox_mem_offset)&1u) break;
        ok=0;
    }
    return ok?WORD_AT(p):0u;
}

void pc_mapmaker_pads_random(void (*original)(void))
{
    uint32_t pads=WORD_AT(MAPMAKER_RANDPAD_PADS),pos;
    int32_t count=(int32_t)WORD_AT(MAPMAKER_RANDPAD_COUNT);
    if(!mapmaker_randpad_mode() || !MAPMAKER_PTR_OK(g_esp,12u) || !MAPMAKER_PTR_OK(pos=WORD_AT(g_esp+8u),12u) ||
       (count>0 && !MAPMAKER_PTR_OK(pads,(size_t)count*0x20u))) { original();return; }
    mapmaker_randpad_calls++;
    g_eax=mapmaker_randpad_pick(WORD_AT(g_esp+4u),pos,g_esp+4u);
    if(mapmaker_randpad_calls==1u || !(mapmaker_randpad_calls&0xFFFFu))
        fprintf(stderr,"[mapmaker] random pad: %llu calls, %llu tries (host code)\n",mapmaker_randpad_calls,mapmaker_randpad_tries);
    MAPMAKER_GUEST_RET();                         /* sub_0006A8B0 ends in a plain ret */
}
