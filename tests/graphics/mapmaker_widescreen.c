/* Asset-free contract for src/interface/pc_mapmaker_widescreen.inc (MapMaker editor on
 * non-4:3 displays), run through the production placement scopes of
 * pc_display_ui.inc with the deferred batch harness of ui_layout.c.
 *
 * Geometry model (US 45410066, see the .inc): the editor draws in guest
 * pixels, cell -> pixel x = (cell - left) * (640 / view_cells); the pointer
 * picks cell = pointer * view_cells / 640 + left (sub_002F8140). On the host a
 * squeezed draw lands at W/2 + (gx*W/640 - W/2) * k (k = hud scale), a
 * stretched one at gx*W/640; AUTO stretches a full-width flat batch (the grid
 * line list) and squeezes textured ones. The mouse maps through
 * pc_menu_mouse_position. Every field batch must use the pointer's transform. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
#include "pc_menu_mouse.h"
#include "pc_mapmaker_widescreen.inc"

enum { KIND_FLAT_FULL=1, KIND_TEXTURED=2, KIND_BACKDROP=3, KIND_PIECES=4 /* textured, outside the scope */ };
static int kinds[1024];
static void emit(int kind)
{
    if(pending>=0) sub_00022FF0();
    pending=1;kinds[draw_count]=kind;
}
/* Host x of guest x for a batch drawn with placement p. */
static int squeezed(int p,int kind) { return p==0 ? kind!=KIND_FLAT_FULL && kind!=KIND_BACKDROP : p!=1; }
static double host_x(double gx,int squeeze,double W,double k)
{
    double x=gx*W/640.;
    return squeeze ? W*.5+(x-W*.5)*k : x;
}

/* sub_0001EFB0 at 0x0030A57B: the full-canvas backdrop. */
static void backdrop(void) { emit(KIND_BACKDROP);g_esp+=4; }
/* sub_0030A550: backdrop, then every grid line in one flat line list. */
static void grid_lines(void)
{
    uint32_t saved=g_esp;
    g_esp=0;stack[0]=0x0030A580u;stack[1]=0;stack[2]=0;stack[3]=640;stack[4]=480;stack[5]=0x07060C7Fu;
    if(!pc_display_transition_overlay(backdrop)) backdrop();
    CHECK(g_esp==4);
    g_esp=saved;
    emit(KIND_FLAT_FULL);
    g_esp+=4;
}
/* sub_00310A20: grid (nested sub_0030A550), floor shade, tiles, cursor. */
static void grid_view(void)
{
    uint32_t saved=g_esp;
    g_esp=saved-4;stack[g_esp/4]=0x00310A1Fu; /* nested call's return address */
    pc_mapmaker_grid_lines(grid_lines);
    CHECK(g_esp==saved);
    emit(KIND_FLAT_FULL);  /* full-view floor shade (sub_00310310) */
    emit(KIND_TEXTURED);   /* tiles (sub_0030D7B0 -> sub_00308C00) */
    emit(KIND_TEXTURED);   /* cursor/selection */
    g_esp+=4;
}
/* sub_003111F0: field, then the HUD at the enclosing placement. */
static int hud_placement;
static void editor_frame(int direct)
{
    draw_count=0;pending=-1;g_esp=0x40;stack[0x10]=0x00311267u;
    if(direct) { pc_mapmaker_grid_lines(grid_lines);emit(KIND_PIECES); } /* sub_0030B480 pieces */
    else pc_mapmaker_grid_view(grid_view);
    CHECK(g_esp==0x44);
    hud_placement=placement;
    emit(KIND_TEXTURED);
    sub_00022FF0();
}

static void set_env(const char *v)
{
#ifdef _WIN32
    _putenv_s("TSFP_MAPMAKER_WIDESCREEN",v?v:"");
#else
    if(v)setenv("TSFP_MAPMAKER_WIDESCREEN",v,1); else unsetenv("TSFP_MAPMAKER_WIDESCREEN");
#endif
    memset(&mapmaker_widescreen,0,sizeof mapmaker_widescreen);
}

/* Is every field batch on the pointer's transform, with tiles on the grid? */
static int aligned(double W,double H,double left,double cells)
{
    double k=4.*H/(3.*W);int ok=1;
    if(k>1.)k=1.; /* portrait: no horizontal squeeze */
    for(int d=0;d<draw_count-1;d++) /* last batch is the HUD */
        if(kinds[d]!=KIND_BACKDROP && !squeezed(drawn[d],kinds[d]) && k!=1.) ok=0;
    /* Find the line list and the tile batch. */
    int lines=-1,tiles=-1;
    for(int d=0;d<draw_count-1;d++) {
        if(kinds[d]==KIND_FLAT_FULL && lines<0) lines=d;
        if((kinds[d]==KIND_TEXTURED || kinds[d]==KIND_PIECES) && tiles<0) tiles=d;
    }
    if(lines<0 || tiles<0) return 0;
    const double mice[]={.5,.5-.5*k+.001,.5+.5*k-.001,.5-.25*k,.5+.37*k};
    for(unsigned m=0;m<sizeof mice/sizeof mice[0];m++) {
        PcMenuPointer p={(float)(mice[m]*W),(float)(H*.5),(float)W,(float)H,1,0,0,0};
        float gx,gy;
        if(!pc_menu_mouse_position(p,(float)(W/H),640.f,480.f,&gx,&gy)) return 0;
        if(gx<0.f || gx>640.f) return 0;
        double cell=floor(gx*cells/640.+left);
        double g0=(cell-left)*640./cells,g1=(cell+1.-left)*640./cells;
        double t0=host_x(g0,squeezed(drawn[tiles],kinds[tiles]),W,k);
        double t1=host_x(g1,squeezed(drawn[tiles],kinds[tiles]),W,k);
        double l0=host_x(g0,squeezed(drawn[lines],kinds[lines]),W,k);
        double l1=host_x(g1,squeezed(drawn[lines],kinds[lines]),W,k);
        if(!(p.x>=t0-1e-3 && p.x<=t1+1e-3)) ok=0;   /* tile under the mouse */
        if(fabs(t0-l0)>1e-3 || fabs(t1-l1)>1e-3) ok=0; /* tile on its grid cell */
    }
    return ok;
}

int main(void)
{
    static const double wins[][2]={{640,480},{1280,960},{1536,869},{1920,1080},
        {2560,1080},{3440,1440},{5120,1440},{1080,1920}};
    /* Zoom (view width in cells) and pan extremes; the centre clamp
       sub_002F9F70 keeps the view centre on the 40x40 grid. */
    static const double views[][2]={{9,20},{-30,60},{10,60},{-10,30},{25,30}};
    set_env(NULL);scene=102;
    for(unsigned w=0;w<sizeof wins/sizeof wins[0];w++) {
        hud_scale=(float)(4.*wins[w][1]/(3.*wins[w][0]));wide=hud_scale!=1.f;
        for(int direct=0;direct<2;direct++) {
            selection_count=0;placement=0;tag=0;
            editor_frame(direct);
            CHECK(hud_placement==0 && placement==0);       /* enclosing placement restored */
            if(!wide) CHECK(selection_count==0);           /* 4:3: untouched, no flushes */
            if(wide) {
                for(int d=0;d<draw_count;d++)
                    if(kinds[d]==KIND_BACKDROP) CHECK(drawn[d]==1); /* backdrop fills the window */
                    else if(kinds[d]==KIND_PIECES) CHECK(drawn[d]==0); /* outside: unchanged */
                    else if(d<draw_count-1) CHECK(drawn[d]==2);
                CHECK(drawn[draw_count-1]==0);              /* HUD keeps AUTO */
            }
            for(unsigned v=0;v<sizeof views/sizeof views[0];v++)
                CHECK(aligned(wins[w][0],wins[w][1],views[v][0],views[v][1]));
        }
    }
    /* Other scenes (MapMaker play 100, front end 101, story) and the switch
       leave every placement alone. */
    hud_scale=.75f;wide=1;
    const uint32_t scenes[]={100,101,7,0};
    for(unsigned s=0;s<4;s++) {
        scene=scenes[s];selection_count=0;editor_frame(0);
        for(int d=0;d<draw_count;d++) CHECK(drawn[d]==(kinds[d]==KIND_BACKDROP?1:0));
    }
    scene=102;set_env("0");selection_count=0;editor_frame(0);
    for(int d=0;d<draw_count;d++) CHECK(drawn[d]==(kinds[d]==KIND_BACKDROP?1:0));
    CHECK(!aligned(1920,1080,9,20)); /* the original misalignment, at 16:9 */
    set_env("1");editor_frame(0);CHECK(aligned(1920,1080,9,20));
    puts("mapmaker widescreen ok");
    return 0;
}
