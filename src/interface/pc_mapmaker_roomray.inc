/* US 45410066 rooms: one step of a ray through the room portals, in host code.
 *
 *   WRAP sub_00088430 -> pc_mapmaker_rooms_ray_step   (area rooms)
 *
 * sub_00088430(origin, dir, room, from, segment, hit, rest) finds where the
 * segment origin + dir leaves `room`: it tests every portal of the room's
 * list (record +4 of [0x00708788] + room*0xB4: {n, ids}) against the
 * portal's plane (+8 normal, first vertex +0x18) and its polygon (+0x14
 * vertices, projected on the plane's two minor axes), keeps the nearest
 * crossing into another room than `from`, and returns that room (or `room`).
 * With a crossing it writes the hit point to `hit` and, for a bounded
 * segment, the rest of the vector to `rest`. Its callers step room by room:
 * sub_000888A0 (up to 100 steps, from 20 places: the five light rays of
 * every drawn object, sub_0008CC20 from sub_00094A00, and AI sight checks),
 * sub_00088950 (line of sight) and 10 more. On an open grid of small rooms a
 * ray crosses many rooms of about 14 portals each, so every drawn character
 * cost thousands of lifted portal tests per frame (the lag toward many Story
 * AI). This is the same function in host code: the same float steps in the
 * same order (single precision, no contraction), the same tests and
 * results; rooms.json's room bound (0x00088455: 0x3FF) is kept.
 * TSFP_MAPMAKER_ROOM_RAY=0 runs the original.
 *
 * Requires the includer's WORD_AT, MAPMAKER_PTR_OK (or guest_writable),
 * g_esp, g_eax. */
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

#define MAPMAKER_RAY_ROOM_MAX 0x3FFu          /* rooms.json rooms-00088455 (stock 0xC8) */
#define MAPMAKER_RAY_K_LOW 0x0047D394u        /* -0.57: minor-axis choice */
#define MAPMAKER_RAY_K_HIGH 0x0047D390u       /* 0.57 */
#define MAPMAKER_RAY_K_FAR 0x00475D5Cu        /* FLT_MAX: nearest crossing so far */

static unsigned long long mapmaker_ray_steps,mapmaker_ray_portals;

static float mapmaker_ray_f(uint32_t a) { float f; uint32_t w=WORD_AT(a); memcpy(&f,&w,4); return f; }
static void mapmaker_ray_set_f(uint32_t a,float f) { uint32_t w; memcpy(&w,&f,4); WORD_AT(a)=w; }
static int mapmaker_ray_mode(void)
{
    static int mode=-1;
    if(mode<0) { const char *e=getenv("TSFP_MAPMAKER_ROOM_RAY");mode=!(e && e[0]=='0'); }
    return mode;
}

/* The function; returns the room in eax. Pointers are guest addresses. */
static uint32_t mapmaker_ray_step(uint32_t o,uint32_t d,uint32_t room,uint32_t from,uint32_t segment,
                                  uint32_t hit_out,uint32_t rest_out)
{
    const float zero=0.0f,k_low=mapmaker_ray_f(MAPMAKER_RAY_K_LOW),k_high=mapmaker_ray_f(MAPMAKER_RAY_K_HIGH);
    float best=mapmaker_ray_f(MAPMAKER_RAY_K_FAR),saved[3]={0,0,0};
    float ox,oy,oz,dx,dy,dz;
    uint32_t list,result=room,table,k;
    int32_t n,count;
    mapmaker_ray_steps++;
    if(!room || (int32_t)room>(int32_t)MAPMAKER_RAY_ROOM_MAX) return room;          /* test edi; jg */
    if(!MAPMAKER_PTR_OK(WORD_AT(0x00708788u)+room*0xB4u+4u,4u)) return room;
    list=WORD_AT(WORD_AT(0x00708788u)+room*0xB4u+4u);
    if(!list) return room;
    if(!MAPMAKER_PTR_OK(d,12u) || !MAPMAKER_PTR_OK(o,12u)) return room;
    dx=mapmaker_ray_f(d);dy=mapmaker_ray_f(d+4u);dz=mapmaker_ray_f(d+8u);
    if(!(dx!=zero || dx!=dx) && !(dy!=zero || dy!=dy) && !(dz!=zero || dz!=dz)) return room;   /* a zero vector */
    if(!MAPMAKER_PTR_OK(list,4u)) return room;
    n=(int32_t)WORD_AT(list);
    if(n<=0 || !MAPMAKER_PTR_OK(list,4u+(size_t)n*4u)) return room;
    ox=mapmaker_ray_f(o);oy=mapmaker_ray_f(o+4u);oz=mapmaker_ray_f(o+8u);
    count=(int32_t)WORD_AT(0x007B9754u);table=WORD_AT(0x0070878Cu);
    for(k=0;k<(uint32_t)n;k++) {
        int32_t id=(int32_t)WORD_AT(list+4u+4u*k),cnt;
        uint32_t P,r0,other;
        float nx,ny,nz,vx,vy,vz,a,b,c,dist,dd,dd2,num,t,hx,hy,hz,len,h[3];
        int ea,eb,i;
        if(id<0 || id>count) continue;
        if(!MAPMAKER_PTR_OK(table+(uint32_t)id*4u,4u)) continue;
        P=WORD_AT(table+(uint32_t)id*4u);
        if(!P || !MAPMAKER_PTR_OK(P,0x24u)) continue;
        mapmaker_ray_portals++;
        r0=WORD_AT(P);
        other=r0==room?WORD_AT(P+4u):r0;
        if(other==from) continue;
        nx=mapmaker_ray_f(P+8u);ny=mapmaker_ray_f(P+0xCu);nz=mapmaker_ray_f(P+0x10u);
        vx=mapmaker_ray_f(P+0x18u);vy=mapmaker_ray_f(P+0x1Cu);vz=mapmaker_ray_f(P+0x20u);
        /* 0x0008851C: distance of the origin to the plane and the vector's dot */
        a=oy-vy;b=oz-vz;c=ox-vx;
        c=c*nx;a=ny*a;b=nz*b;
        dist=a+b;dist=dist+c;
        a=ny*dy;b=dx*nx;
        dd=dz*nz;dd=dd+a;dd=dd+b;
        if(dist>zero && dd>zero) continue;
        if(zero>dist && zero>dd) continue;
        /* 0x0008859D */
        dd2=dz*nz;a=ny*dy;dd2=dd2+a;dd2=dd2+b;
        if(r0==room && zero>=dd2) continue;
        if(WORD_AT(P+4u)==room && dd2>=zero) continue;
        /* 0x000885D4 */
        a=vy-oy;c=vz-oz;c=c*nz;a=a*ny;num=a+c;c=vx-ox;c=c*nx;num=num+c;
        if(zero>num && dd2>zero) continue;
        if(num>zero && zero>dd2) continue;
        if(!segment) {
            float an=num,ad=dd2;
            uint32_t w;
            memcpy(&w,&an,4);w&=0x7FFFFFFFu;memcpy(&an,&w,4);
            memcpy(&w,&ad,4);w&=0x7FFFFFFFu;memcpy(&ad,&w,4);
            if(an>ad) continue;
        }
        t=num/dd2;
        hx=dx*t;hy=dy*t;hz=dz*t;
        len=hz*hz;a=hy*hy;len=len+a;a=hx*hx;len=len+a;
        if(len>best) continue;
        h[0]=ox+hx;h[1]=hy+oy;h[2]=oz+hz;
        /* 0x000886AE: the two axes the polygon is projected on (eb = edx, ea = esi) */
        if(k_low>nx) { eb=1;ea=2; }
        else if(nx>k_high) { ea=1;eb=2; }
        else if(k_low>nz) { ea=1;eb=0; }
        else if(nz>k_high) { ea=0;eb=1; }
        else if(k_low>ny) { ea=0;eb=2; }
        else { eb=0;ea=2; }
        cnt=(int16_t)*(uint16_t *)((uintptr_t)P+0x14u+g_xbox_mem_offset);
        if(cnt>0) {
            uint32_t prev=P+0x18u+12u*(uint32_t)(cnt-1),cur=P+0x18u;
            int outside=0;
            if(!MAPMAKER_PTR_OK(cur,(size_t)cnt*12u)) continue;
            for(i=0;i<cnt;i++,prev=cur,cur+=12u) {
                float s,u;
                s=mapmaker_ray_f(prev+4u*(uint32_t)ea)-mapmaker_ray_f(cur+4u*(uint32_t)ea);
                u=h[eb]-mapmaker_ray_f(prev+4u*(uint32_t)eb);
                s=s*u;
                u=mapmaker_ray_f(cur+4u*(uint32_t)eb)-mapmaker_ray_f(prev+4u*(uint32_t)eb);
                u=u*(h[ea]-mapmaker_ray_f(prev+4u*(uint32_t)ea));
                s=s+u;
                if(s>zero) { outside=1;break; }
            }
            if(outside) continue;
        }
        best=len;result=other;
        saved[0]=h[0];saved[1]=h[1];saved[2]=h[2];
    }
    if(result==room) return room;
    if(rest_out && !segment && MAPMAKER_PTR_OK(rest_out,12u)) {
        float r;
        r=ox+dx;r=r-saved[0];mapmaker_ray_set_f(rest_out,r);
        r=oy+dy;r=r-saved[1];mapmaker_ray_set_f(rest_out+4u,r);
        r=oz+dz;r=r-saved[2];mapmaker_ray_set_f(rest_out+8u,r);
    }
    if(hit_out && MAPMAKER_PTR_OK(hit_out,12u)) {
        mapmaker_ray_set_f(hit_out,saved[0]);mapmaker_ray_set_f(hit_out+4u,saved[1]);mapmaker_ray_set_f(hit_out+8u,saved[2]);
    }
    return result;
}

void pc_mapmaker_rooms_ray_step(void (*original)(void))
{
    if(!mapmaker_ray_mode() || !MAPMAKER_PTR_OK(g_esp,32u)) { original();return; }
    g_eax=mapmaker_ray_step(WORD_AT(g_esp+4u),WORD_AT(g_esp+8u),WORD_AT(g_esp+12u),WORD_AT(g_esp+16u),
                            WORD_AT(g_esp+20u),WORD_AT(g_esp+24u),WORD_AT(g_esp+28u));
    if(mapmaker_ray_steps==1u || !(mapmaker_ray_steps&0xFFFFFu))
        fprintf(stderr,"[mapmaker] room ray steps: %llu steps, %llu portal tests (host code)\n",
                mapmaker_ray_steps,mapmaker_ray_portals);
    MAPMAKER_GUEST_RET();                        /* sub_00088430 ends in a plain ret */
}
