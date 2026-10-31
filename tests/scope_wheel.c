/* Mouse-wheel scope zoom (src/pc_scope_wheel.h). Guest memory is a host
 * buffer; offsets are US guest addresses relative to it (player 0x10000,
 * its camera 0x40000). */
#include "pc_scope_wheel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define I(a) (*(int32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
#define VIEW() pc_scope_view(0x10000,0x40000,offset)
#define UP PC_SCOPE_WHEEL_UP_KEY
#define DOWN PC_SCOPE_WHEEL_DOWN_KEY
enum { K_UP=0x26, K_DOWN=0x28, K_LEFT=0x25, K_RIGHT=0x27, K_PRIOR=0x21, K_NEXT=0x22 };

static void defaults(int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS])
{
    memset(keys,0,sizeof(int)*PC_ACTION_COUNT*PC_BINDING_SLOTS);
    keys[9][0]=1;                                    /* Fire: Mouse 1 */
    keys[10][0]=K_UP;keys[10][1]=K_PRIOR;keys[10][2]='Q';  /* zoom in */
    keys[11][0]=K_DOWN;keys[11][1]=K_NEXT;keys[11][2]='V'; /* zoom out */
    keys[12][0]=K_LEFT;keys[12][1]=DOWN;             /* previous weapon */
    keys[13][0]=K_RIGHT;keys[13][1]=UP;              /* next weapon */
    keys[23][0]=2;                                   /* Aim: Mouse 2 */
}

int main(void)
{
    unsigned char *guest=calloc(1,0x80000);
    ptrdiff_t offset=(ptrdiff_t)guest;
    int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    PcScopeWheel w;
    CHECK(guest);
    memset(&w,0,sizeof w);

    /* Guest state: player 1 (+4 == 0) aiming (0x20) through a scope (0x40)
     * whose zoom range is 4..30 degrees (sniper-like). */
    I(0x10004)=0;U(0x10008)=0x40000;U(0x1000c)=0x60;
    F(0x40390)=30;F(0x40394)=4;
    CHECK(VIEW()==1);
    U(0x1000c)=0x20;CHECK(VIEW()==0);          /* iron sight / plain aim */
    U(0x1000c)=0x40;CHECK(VIEW()==0);          /* scope flag left over after aim ended */
    U(0x1000c)=0x60|0x10;CHECK(VIEW()==1);
    F(0x40394)=30;CHECK(VIEW()==0);            /* collapsed range: no zoomable scope */
    F(0x40394)=NAN;CHECK(VIEW()==0);
    F(0x40394)=4;U(0x10008)=0x48000;CHECK(VIEW()==0); /* not this player's camera */
    U(0x10008)=0x40000;
    I(0x10004)=1;CHECK(VIEW()==-1);            /* split-screen player 2 */
    I(0x10004)=-1;CHECK(VIEW()==-1);           /* AI */
    I(0x10004)=0;CHECK(VIEW()==1);
    CHECK(pc_scope_view(0,0x40000,offset)==-1);

    /* Default bindings: Wheel up = Next weapon, Wheel down = Previous weapon. */
    defaults(keys);
    pc_scope_wheel_bindings(&w,(const int (*)[PC_BINDING_SLOTS])keys);
    CHECK(w.route_up && w.route_down && w.zoom_in_key==K_UP && w.zoom_out_key==K_DOWN);

    /* Not scoped: the wheel changes weapons as before. */
    pc_scope_wheel_state(&w,0,1000);
    CHECK(pc_scope_wheel_input(&w,1,1000)==1 && pc_scope_wheel_input(&w,-2,1000)==-2);
    CHECK(pc_scope_wheel_key(&w,UP,1,1,1000)==1 && pc_scope_wheel_key(&w,K_UP,0,1,1000)==0);

    /* Scoped: wheel up zooms in for one short hold and never switches. */
    pc_scope_wheel_state(&w,1,1000);
    CHECK(pc_scope_wheel_input(&w,1,1010)==0);
    CHECK(pc_scope_wheel_key(&w,UP,1,1,1010)==0);          /* a queued switch is held back */
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1010)==1);        /* zoom in held */
    CHECK(pc_scope_wheel_key(&w,K_DOWN,0,1,1010)==0);
    CHECK(pc_scope_wheel_key(&w,'Q',1,1,1010)==1);         /* real Q still works */
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1010+PC_SCOPE_WHEEL_MS-1)==1);
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1010+PC_SCOPE_WHEEL_MS)==0); /* one notch = one step */
    CHECK(pc_scope_wheel_key(&w,K_UP,1,1,1010+PC_SCOPE_WHEEL_MS)==1);
    CHECK(pc_scope_wheel_key(&w,K_RIGHT,1,1,1010)==1);     /* keyboard weapon switch untouched */

    /* Wheel down zooms out; notches add up; a reversal starts over; capped. */
    pc_scope_wheel_state(&w,1,1100);
    CHECK(pc_scope_wheel_input(&w,-1,1100)==0 && pc_scope_wheel_input(&w,-1,1110)==0);
    CHECK(pc_scope_wheel_key(&w,K_DOWN,0,1,1100+2*PC_SCOPE_WHEEL_MS-1)==1);
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1120)==0);
    CHECK(pc_scope_wheel_key(&w,DOWN,1,1,1120)==0);
    CHECK(pc_scope_wheel_input(&w,1,1120)==0);
    CHECK(pc_scope_wheel_key(&w,K_DOWN,0,1,1130)==0 && pc_scope_wheel_key(&w,K_UP,0,1,1130)==1);
    CHECK(pc_scope_wheel_input(&w,3,1130)==0 && w.until==1120+4*PC_SCOPE_WHEEL_MS);
    CHECK(pc_scope_wheel_input(&w,40,1140)==0 && w.until<=1140+PC_SCOPE_WHEEL_MAX_MS);

    /* Menus / unfocused windows: keys pass through unchanged. */
    CHECK(pc_scope_wheel_key(&w,K_UP,0,0,1150)==0 && pc_scope_wheel_key(&w,UP,1,0,1150)==1);

    /* Leaving the scope cancels the hold, even if aiming resumes at once. */
    pc_scope_wheel_state(&w,0,1160);
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1165)==0);
    pc_scope_wheel_state(&w,1,1170);
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,1175)==0);

    /* Another local player's update never changes player 1's state. */
    pc_scope_wheel_state(&w,-1,1180);
    CHECK(pc_scope_wheel_input(&w,1,1190)==0);
    pc_scope_wheel_state(&w,0,1300);pc_scope_wheel_state(&w,-1,1310);
    CHECK(pc_scope_wheel_input(&w,1,1320)==1);

    /* Stale state (menus, cutscenes: no player update) never redirects. */
    pc_scope_wheel_state(&w,1,2000);
    CHECK(pc_scope_wheel_input(&w,1,2000+PC_SCOPE_WHEEL_FRESH_MS)==1);
    CHECK(pc_scope_wheel_key(&w,UP,1,1,2000+PC_SCOPE_WHEEL_FRESH_MS)==1);

    /* Rebinding: a wheel direction bound to something else is not hijacked. */
    defaults(keys);keys[13][1]=0;keys[9][1]=UP;            /* Wheel up = Fire */
    pc_scope_wheel_bindings(&w,(const int (*)[PC_BINDING_SLOTS])keys);
    pc_scope_wheel_state(&w,1,3000);
    CHECK(!w.route_up && w.route_down);
    CHECK(pc_scope_wheel_input(&w,1,3000)==1 && pc_scope_wheel_key(&w,UP,1,1,3000)==1);
    CHECK(pc_scope_wheel_key(&w,K_UP,0,1,3000)==0);
    CHECK(pc_scope_wheel_input(&w,-1,3000)==0);
    defaults(keys);keys[3][0]=UP;                          /* also bound to Y action */
    pc_scope_wheel_bindings(&w,(const int (*)[PC_BINDING_SLOTS])keys);
    CHECK(!w.route_up && w.route_down);
    defaults(keys);keys[13][1]=0;keys[12][1]=0;keys[12][2]=UP;keys[13][2]=DOWN; /* swapped */
    pc_scope_wheel_bindings(&w,(const int (*)[PC_BINDING_SLOTS])keys);
    CHECK(w.route_up && w.route_down);
    /* No zoom key bound: the notch is swallowed rather than switching. */
    defaults(keys);memset(keys[10],0,sizeof keys[10]);keys[10][0]=DOWN;keys[12][1]=0;
    pc_scope_wheel_bindings(&w,(const int (*)[PC_BINDING_SLOTS])keys);
    pc_scope_wheel_state(&w,1,4000);
    CHECK(w.zoom_in_key==0 && w.route_up && !w.route_down);
    CHECK(pc_scope_wheel_input(&w,1,4000)==0 && pc_scope_wheel_key(&w,UP,1,1,4000)==0);
    CHECK(pc_scope_wheel_input(&w,-1,4000)==-1);           /* wheel down = zoom in: not a switch */
    pc_scope_wheel_bindings(&w,NULL);
    CHECK(!w.route_up && !w.route_down && pc_scope_wheel_input(&w,1,4000)==1);

    puts("PASS: scope wheel zoom");
    free(guest);
    return 0;
}
