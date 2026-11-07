/* US 45410066: sub_00073750 runs after item rectangles are laid out and
 * before sub_0007A7B0 consumes menu actions. Keep callbacks, transitions,
 * sound and disabled-item behavior owned by that original dispatcher. */
#include "pc_menu_mouse.h"
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY) || defined(TSFP_MENU_MOUSE_TEST)
#ifndef TSFP_MENU_MOUSE_TEST
#include "pc_controls.h"
#endif
extern void d3d8_display_output_shape(unsigned,unsigned,unsigned *,unsigned *);
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
extern void sub_00074760(void),sub_00076EF0(void),sub_00076510(void),sub_0003FCF0(void),sub_002E6D40(void);
#include "pc_menu_text.inc"

static PcMenuMouse menu_mouse;

/* Some frontend callbacks (notably sub_002D2260) scan the original pad
 * pressed edges to identify which player confirmed. A menu action alone is
 * insufficient: with no source pad that callback reads mapping[-1]. Scope
 * the mouse's player-1 Confirm edge to this original menu dispatch, including
 * its callbacks, so it cannot leak into another menu or gameplay. */
typedef struct MenuMouseDispatch {
    uint32_t menu, pressed, injected, pressed_address;
    PcMenuPointer pointer;
    unsigned actions;
    int sampled;
    uint32_t close_pause,tab_menu,tab_target;
} MenuMouseDispatch;
static MenuMouseDispatch *menu_mouse_dispatch;

/* sub_000427B0 reads logical network input in LAN/Online menus, rather
 * than the physical controller edge used by profile selection. Remember the
 * exact destination: callbacks may change network mode before we restore it. */
static void menu_mouse_press(MenuMouseDispatch *s,uint32_t buttons)
{
    if(!s->injected) {
        s->pressed_address=WORD_AT(0x0076B0FCu)?0x006B94D8u:0x006B93E0u;
        s->pressed=WORD_AT(s->pressed_address);
    }
    s->injected|=buttons;
    WORD_AT(s->pressed_address)|=buttons;
}

/* The resource table (pc_mapmaker_restable.inc, below) owns the meter wrapper. */
#define MAPMAKER_RESTABLE 1
#include "pc_mapmaker_probe.inc"
#include "pc_mapmaker_pointer.inc"
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
/* MapMaker capacity area includes (begin) */
#include "pc_mapmaker_memory.inc"
#include "pc_mapmaker_pads.inc"
#include "pc_mapmaker_container.inc"
#include "pc_mapmaker_objects.inc"
#include "pc_mapmaker_validate.inc"
#include "pc_mapmaker_editor.inc"
#include "pc_mapmaker_rooms.inc"
#include "pc_mapmaker_actors.inc"
#include "pc_mapmaker_story_character.inc"
#include "pc_mapmaker_aitemplates.inc"
#include "pc_mapmaker_grid.inc"
#include "pc_mapmaker_widescreen.inc"
#include "pc_mapmaker_bugfix.inc"
#include "pc_mapmaker_sky.inc"
#include "pc_mapmaker_skymask.inc"
#include "pc_mapmaker_roof.inc"
#include "pc_mapmaker_tileflags.inc"
#include "pc_mapmaker_skymusic.inc"
#include "pc_mapmaker_camera.inc"
#include "pc_mapmaker_openview.inc"
#include "pc_mapmaker_bosses.inc"
#include "pc_mapmaker_characters.inc"
#include "pc_mapmaker_realbosses.inc"
#include "pc_mapmaker_robot.inc"
#include "pc_mapmaker_crowe.inc"
#include "pc_mapmaker_tank.inc"
#include "pc_mapmaker_khallos.inc"
#include "pc_mapmaker_worm.inc"
#include "pc_mapmaker_mothership.inc"
#include "pc_mapmaker_bigdraw.inc"
#include "pc_mapmaker_sets.inc"
#include "pc_mapmaker_matchbots.inc"
#include "pc_mapmaker_weaponset.inc"
#include "pc_mapmaker_tilesets.inc"
#include "pc_mapmaker_restable.inc"
/* MapMaker capacity area includes (end) */

/* Every area hook is reached through this gate (generated wrappers in
 * patches/*-mapmaker-meter-wrappers.patch). For diagnosis:
 *   TSFP_MAPMAKER_AREA_OFF=container,pads  run those areas' originals instead
 *   TSFP_MAPMAKER_AREA_TRACE=1             log each hook's first 4 calls per
 *                                          return address to stderr */
int pc_mapmaker_area_enabled(const char *area,const char *hook)
{
    static int initialized,trace;
    static char off[256];
    static struct { const char *hook; uint32_t ret; unsigned calls; } seen[256];
    static unsigned seen_n;
    size_t n=strlen(area);
    const char *p;
    int enabled=1;
    if(!initialized) {
        const char *o=getenv("TSFP_MAPMAKER_AREA_OFF"),*t=getenv("TSFP_MAPMAKER_AREA_TRACE");
        initialized=1;
        if(o) { off[0]=',';strncpy(off+1,o,sizeof off-3);strcat(off,","); }
        trace=t && t[0]=='1';
    }
    for(p=off;off[0] && (p=strstr(p,area))!=NULL;p+=n)
        if(p[-1]==',' && p[n]==',') { enabled=0;break; }
    /* The container, actors, story, grid and rooms code patches assume their
     * hooks (buffer sizes, the AI cap in code is 63 and only the hook keeps 30,
     * the Story character gate byte is 0 = choose freely until a hook sets it,
     * the grid settings block is empty until its hook writes it, light rooms
     * past 0xFF live in the rooms hooks' side table in every scene), so those
     * areas cannot be switched off; the request is ignored and said so once. */
    if(!enabled) {
        static const char *const fixed[]={"container","actors","story","grid","rooms"};
        static int warned[sizeof fixed/sizeof fixed[0]];
        for(unsigned i=0;i<sizeof fixed/sizeof fixed[0];i++) if(!strcmp(area,fixed[i])) {
            if(!warned[i]) { warned[i]=1;fprintf(stderr,"[mapmaker-area] %s cannot be switched off; ignored\n",area);fflush(stderr); }
            enabled=1;break;
        }
    }
    if(trace) {
        uint32_t ret=guest_writable(g_esp,4)?WORD_AT(g_esp):0u;
        unsigned i;
        for(i=0;i<seen_n && !(seen[i].hook==hook && seen[i].ret==ret);i++) {}
        if(i==seen_n && seen_n<sizeof seen/sizeof seen[0]) { seen[i].hook=hook;seen[i].ret=ret;seen_n++; }
        if(i<seen_n && seen[i].calls++<4) {
            fprintf(stderr,"[mapmaker-area] %s %s ret=0x%08X%s\n",area,hook,ret,enabled?"":" (off)");
            fflush(stderr);
        }
    }
    return enabled;
}
#include "pc_menu_sliders.inc"
static void menu_team_arrow(unsigned direction);
#include "pc_menu_choices.inc"

/* The player-1 pause/status flags cover both Start and Back/Tab menus.
 * Scene alone cannot distinguish them from gameplay. */
static int menu_mouse_context(void)
{
    uint32_t pause=WORD_AT(0x00761C48u);
    return (int32_t)WORD_AT(0x0079094Cu)>100 ||
        (guest_writable(pause,4) && (WORD_AT(pause)&1u));
}
/* Root pause/status screens have no menu Back callback. Use the original
 * pause toggle only when it will close a normal active player-1 menu;
 * death/completion/modal states keep their original handling. */
static int menu_mouse_can_close_pause(uint32_t menu)
{
    uint32_t pause=WORD_AT(0x00761C48u),root=WORD_AT(0x00761BBCu);
    return (int32_t)WORD_AT(0x0079094Cu)<=100 &&
        guest_writable(pause,4) && (WORD_AT(pause)&7u)==1u &&
        (int32_t)WORD_AT(0x00761BB8u)<=0 && guest_writable(root,0x23c) &&
        WORD_AT(root+0x170)==menu &&
        guest_writable(menu,0x23c) && WORD_AT(menu+0x1b0)==3 &&
        !WORD_AT(menu+0x1c0) && !(WORD_AT(menu+0x40)&0x2000u);
}
void pc_display_menu_mouse_mode(void)
{
    int enabled=menu_mouse_context();
    mapmaker_probe_heap(); /* read-only; inert unless TSFP_MAPMAKER_PROBE has flag 32 */
    mapmaker_probe_pools(); /* read-only; flag 64 */
    pc_mapmaker_sky_room_mask_flush(); /* summary of TSFP_MAPMAKER_ROOM_MASK_CLEAR; idle otherwise */
    pc_controls_menu_mode(enabled);
    if(!enabled)pc_controls_text_mode(0);
    if(!enabled) {
        memset(&menu_mouse,0,sizeof menu_mouse);
        menu_slider_drag=0;
    }
}

/* Pause tab labels are rendered by the parent, separately from menu rows.
 * Cache only actual draws and defer replacing the child until dispatch ends. */
typedef struct MenuMouseTab {
    uint32_t menu,source,target;
    float x,y,w,h;
    unsigned sw,sh;
    unsigned long long time;
} MenuMouseTab;
static MenuMouseTab menu_mouse_tabs[12];
extern void sub_00079860(void),sub_002E49F0(void),sub_002E4A30(void);
static int menu_mouse_tab_index(uint32_t descriptor)
{
    for(unsigned i=0;i<12;i++)
        if(WORD_AT(0x005247C4u+i*12)==descriptor && descriptor) return (int)i;
    return -1;
}
static int menu_mouse_tabs_active(uint32_t menu)
{
    uint32_t pause=WORD_AT(0x00761C48u),root=WORD_AT(0x00761BBCu);
    return (int32_t)WORD_AT(0x0079094Cu)<=100 &&
        guest_writable(pause,0x268) && (WORD_AT(pause)&7u)==1u &&
        (int32_t)WORD_AT(0x00761BB8u)<=0 && guest_writable(root,0x23c) &&
        WORD_AT(root+0x170)==menu && guest_writable(menu,0x23c) &&
        WORD_AT(menu+0x38)==0 && WORD_AT(menu+0x1b0)==3 &&
        !(WORD_AT(menu+0x40)&0x2000u) &&
        !((WORD_AT(0x00789640u)|WORD_AT(0x007894FCu))&1u) &&
        menu_mouse_tab_index(WORD_AT(menu+4))>=0 && guest_call(sub_00074760,1,menu);
}
static int menu_mouse_tab_contains(const MenuMouseTab *t,uint32_t menu,
    float x,float y,unsigned sw,unsigned sh)
{
    return t->menu==menu && t->source==WORD_AT(menu+4) &&
        t->target!=t->source && t->sw==sw && t->sh==sh &&
        GetTickCount64()-t->time<=100 &&
        x>=t->x && x<t->x+t->w && y>=t->y && y<t->y+t->h;
}
static int menu_mouse_tab_text(void)
{
    /* sub_002E4FF0's EBP is a generated local, not g_ebp. Its saved
       one-based loop index is at +0x2c at this exact text call site. */
    uint32_t index=WORD_AT(g_esp+0x2c);
    if(index<1 || index>12) return 0;
    --index;
    uint32_t root=WORD_AT(0x00761BBCu);
    if(!guest_writable(root,0x23c)) return 0;
    uint32_t menu=WORD_AT(root+0x170);
    if(!menu_mouse_tabs_active(menu)) return 0;
    MenuMouseTab *t=&menu_mouse_tabs[index];
    t->menu=menu;t->source=WORD_AT(menu+4);t->target=WORD_AT(0x005247C4u+index*12);
    t->x=FLOAT_AT(g_esp+4);t->y=FLOAT_AT(g_esp+8);
    uint32_t text=WORD_AT(g_esp+12);
    GuestRegisters r;save_registers(&r);g_esp-=32;
    guest_call(sub_0003FCF0,4,g_esp,g_esp+4,text,0u);
    t->w=FLOAT_AT(g_esp);t->h=FLOAT_AT(g_esp+4);restore_registers(&r);
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&t->sw,&t->sh);
    t->time=GetTickCount64();
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();float x,y;
    return t->sh && pc_menu_mouse_position(pointer,(float)t->sw/t->sh,
        (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y) &&
        menu_mouse_tab_contains(t,menu,x,y,t->sw,t->sh);
}
static uint32_t menu_mouse_tab_hit(uint32_t menu,float x,float y,unsigned sw,unsigned sh)
{
    if(menu_mouse_tabs_active(menu))
        for(unsigned i=0;i<12;i++)
            if(menu_mouse_tab_contains(&menu_mouse_tabs[i],menu,x,y,sw,sh)) return i+1;
    return 0;
}
static void menu_mouse_change_tab(uint32_t menu,uint32_t target)
{
    if(!target || target>12 || !menu_mouse_tabs_active(menu)) return;
    MenuMouseTab *t=&menu_mouse_tabs[target-1];
    if(t->menu!=menu || t->source!=WORD_AT(menu+4) || GetTickCount64()-t->time>100) return;
    GuestRegisters r;save_registers(&r);
    uint32_t player=WORD_AT(0x007BA9E0u),pause=WORD_AT(0x00761C48u);
    WORD_AT(0x007BA9E0u)=0;
    /* Match the original previous/next-tab helpers' transition state, then
       let the original replacement routine own construction and animation. */
    int index=(int)target-1,old=menu_mouse_tab_index(t->source);
    uint32_t first=guest_call(sub_002E49F0,0),last=guest_call(sub_002E4A30,0);
    WORD_AT(pause+0x264)=index==(int)first?1u:index==(int)last?2u:0u;
    FLOAT_AT(pause+(index<old?0x25c:0x260))=1.f;
    guest_call(sub_00079860,3,WORD_AT(0x00761BBCu),t->target,UINT32_MAX);
    WORD_AT(0x007BA9E0u)=player;restore_registers(&r);
}

typedef struct MenuMouseFooter {
    uint32_t menu, descriptor[4];
    float x,y,w,h;
    unsigned sw,sh;
    unsigned long long time;
} MenuMouseFooter;
static MenuMouseFooter menu_mouse_footer[4];
static uint32_t menu_mouse_footer_drawing;

typedef struct MenuMouseProfilePrompt {
    uint32_t menu,item,caption,button;
    float x,y,w,h;
    unsigned sw,sh;
    unsigned long long time;
} MenuMouseProfilePrompt;
static MenuMouseProfilePrompt menu_mouse_profile[4];
static int menu_mouse_profile_active(uint32_t menu)
{
    return (int32_t)WORD_AT(0x0079094Cu)>100 && guest_writable(menu,0x23c) &&
        WORD_AT(menu+4)==0x004E8D54u && WORD_AT(menu+0x38)==0 && WORD_AT(menu+0x1b0)==3 &&
        guest_call(sub_00074760,1,menu);
}
static int menu_mouse_profile_contains(const MenuMouseProfilePrompt *f,uint32_t menu,
    float x,float y,unsigned sw,unsigned sh)
{
    if(f->menu!=menu || !guest_writable(f->item,0x10c) ||
       WORD_AT(f->item+0x88)!=menu || WORD_AT(f->item+0x20)!=f->caption ||
       (WORD_AT(f->item+0x28)&0x21u) || (WORD_AT(f->item+0x2c)&0x20u) ||
       f->sw!=sw || f->sh!=sh || GetTickCount64()-f->time>100) return 0;
    return x>=f->x && x<f->x+f->w && y>=f->y && y<f->y+f->h;
}
static int menu_mouse_profile_hit(uint32_t menu,float x,float y,unsigned sw,unsigned sh)
{
    if(menu_mouse_profile_active(menu))
        for(unsigned i=0;i<4;i++)
            if(menu_mouse_profile_contains(&menu_mouse_profile[i],menu,x,y,sw,sh)) return (int)i;
    return -1;
}
static float menu_mouse_measure_prefix(uint32_t text,unsigned length,float *height)
{
    GuestRegisters r;save_registers(&r);
    g_esp-=640;
    memcpy(&WORD_AT(g_esp+32),&WORD_AT(text),length);
    ((char *)&WORD_AT(g_esp+32))[length]=0;
    guest_call(sub_0003FCF0,4,g_esp,g_esp+4,g_esp+32,1u);
    float width=FLOAT_AT(g_esp);*height=FLOAT_AT(g_esp+4);
    restore_registers(&r);return width;
}
/* Profile prompts are centered text rows rather than footer descriptors.
 * Measure original button-token spans in their current font; the existing
 * prompt measurement wrapper accounts for current keyboard bindings. */
static int menu_mouse_profile_text(void (*original)(void))
{
    uint32_t frame=g_esp,menu=g_edi,item=g_esi,text=WORD_AT(frame+12);
    if(WORD_AT(frame)!=0x000763C3u || !menu_mouse_profile_active(menu) ||
       !guest_writable(item,0x10c) || WORD_AT(item+0x88)!=menu ||
       ((int32_t)WORD_AT(item+0x14)!=-4 && (int32_t)WORD_AT(item+0x14)!=-5) ||
       !guest_writable(text,512) || frame<1024 || !guest_writable(frame-1024,1024)) return 0;
    const char *str=(const char *)&WORD_AT(text);
    const char *end=memchr(str,0,512);
    if(!end || memchr(str,'\n',(size_t)(end-str))) return 0;
    unsigned length=(unsigned)(end-str),starts[4],stops[4],buttons[4],count=0;
    for(unsigned i=0;i+1<length;i++) {
        if(str[i]=='\v' && str[i+1]>='1' && str[i+1]<='4') {
            static const unsigned map[]={1,0,3,2};
            if(count==4) return 0;
            starts[count]=i;buttons[count++]=map[str[i+1]-'1'];i++;
        }
    }
    if(!count) return 0;
    float height,width=menu_mouse_measure_prefix(text,length,&height);
    float origin=FLOAT_AT(frame+4),y=FLOAT_AT(frame+8),positions[4];
    uint32_t alignment=WORD_AT(0x007E7260u);
    if(alignment==1)origin-=width*.5f;else if(alignment==2)origin-=width;
    unsigned sw,sh;d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&sw,&sh);
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();float x=0,py=0;
    int valid=sh && pc_menu_mouse_position(pointer,(float)sw/sh,
        (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&py),hover=-1;
    for(unsigned i=0;i<count;i++) {
        unsigned stop=i+1<count?starts[i+1]:length;
        while(stop>starts[i] && str[stop-1]==' ')stop--;
        stops[i]=stop;
        float ignored;
        float a=origin+menu_mouse_measure_prefix(text,starts[i],&ignored);
        float b=origin+menu_mouse_measure_prefix(text,stop,&ignored);
        positions[i]=a;
        MenuMouseProfilePrompt *f=&menu_mouse_profile[buttons[i]];
        f->menu=menu;f->item=item;f->caption=WORD_AT(item+0x20);f->button=buttons[i];
        f->x=a;f->y=y;f->w=b-a;f->h=height;
        f->sw=sw;f->sh=sh;f->time=GetTickCount64();
        if(valid && menu_mouse_profile_contains(f,menu,x,py,sw,sh)) hover=(int)i;
    }
    if(hover<0) return 0;
    GuestRegisters before;save_registers(&before);
    uint32_t color=WORD_AT(frame+16);
    /* Render disjoint spans once each, preserving alpha and exact advances.
       Keep strings above their scratch call frame, clear of nested callees. */
    for(unsigned i=0;i<count;i++) {
        restore_registers(&before);g_esp=frame-640;
        memcpy(&WORD_AT(g_esp),&WORD_AT(frame),20);
        memcpy(&WORD_AT(frame-512),str+starts[i],stops[i]-starts[i]);
        ((char *)&WORD_AT(frame-512))[stops[i]-starts[i]]=0;
        WORD_AT(g_esp+12)=frame-512;FLOAT_AT(g_esp+4)=positions[i];
        WORD_AT(g_esp+16)=(int)i==hover?0x405a7f00u|(color&0xffu):color;
        WORD_AT(0x007E7260u)=0;original();
    }
    WORD_AT(0x007E7260u)=alignment;restore_registers(&before);g_esp=frame+4;
    return 1;
}

void pc_display_menu_mouse_footer(void (*original)(void))
{
    uint32_t menu=WORD_AT(g_esp+4),previous=menu_mouse_footer_drawing;
    if(guest_writable(menu,0x23c) && WORD_AT(menu+0x38)==0) {
        memset(menu_mouse_footer,0,sizeof menu_mouse_footer);
        menu_mouse_footer_drawing=menu;
    } else menu_mouse_footer_drawing=0;
    original();
    menu_mouse_footer_drawing=previous;
}
/* Share the visible, enabled hit area between clicks and hover feedback. */
static int menu_mouse_footer_contains(const MenuMouseFooter *f,uint32_t menu,
    unsigned index,float x,float y,unsigned sw,unsigned sh)
{
    if((WORD_AT(menu+0x40)&0x04000000u) || f->menu!=menu || f->sw!=sw || f->sh!=sh ||
       GetTickCount64()-f->time>100 ||
       memcmp(f->descriptor,&WORD_AT(menu+0x128+index*16),16) ||
       f->descriptor[0]>3 || f->descriptor[2]!=0 || !(f->w>0) || !(f->h>0)) return 0;
    return x>=f->x && x<f->x+f->w && y>=f->y && y<f->y+f->h &&
        guest_call(sub_00076510,2,menu,index)!=0;
}

/* Called from the existing text wrapper. Capture only the footer's actual
 * draw call, after formatting, wrapping and font selection. Thus translated
 * bindings and a second footer line use exactly the displayed hit areas. */
static int menu_mouse_footer_text(void)
{
    uint32_t menu=menu_mouse_footer_drawing,index=g_esi;
    uint32_t caller=WORD_AT(g_esp);
    GuestRegisters r;
    if(caller==0x002E51F9u) return menu_mouse_tab_text();
    if(!menu || index>=4 ||
       (caller!=0x000767E4u && caller!=0x002D91B9u && caller!=0x002F4108u)) return 0;
    /* The two themed renderers pack visible prompts; the generic renderer
     * preserves all four slot indices. Recover the original action slot. */
    if(caller!=0x000767E4u) {
        unsigned visible=0,slot;
        for(slot=0;slot<4;slot++) {
            uint32_t entry=menu+0x128+slot*16;
            if(WORD_AT(entry)!=UINT32_MAX && WORD_AT(entry+8)!=2) {
                if(visible++==index) break;
            }
        }
        if(slot==4) return 0;
        index=slot;
    }
    MenuMouseFooter *f=&menu_mouse_footer[index];
    f->menu=menu;
    memcpy(f->descriptor,&WORD_AT(menu+0x128+index*16),16);
    f->x=FLOAT_AT(g_esp+4);f->y=FLOAT_AT(g_esp+8);
    uint32_t text=WORD_AT(g_esp+12);
    save_registers(&r);
    g_esp-=32;
    guest_call(sub_0003FCF0,4,g_esp,g_esp+4,text,0u);
    f->w=FLOAT_AT(g_esp);f->h=FLOAT_AT(g_esp+4);
    restore_registers(&r);
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&f->sw,&f->sh);
    f->time=GetTickCount64();
    /* Sample without draining input: drawing can run between wheel packets
       and the next menu tick. Focus loss invalidates this pointer immediately. */
    if(menu_mouse_context() && WORD_AT(menu+0x1b0)==3 && guest_call(sub_00074760,1,menu)) {
        PcMenuPointer pointer=pc_controls_menu_hover_pointer();
        float x,y;
        if(f->sh && pc_menu_mouse_position(pointer,(float)f->sw/f->sh,
            (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y))
            return menu_mouse_footer_contains(f,menu,index,x,y,f->sw,f->sh);
    }
    return 0;
}

static int menu_mouse_footer_hit(uint32_t menu,float x,float y,unsigned sw,unsigned sh)
{
    for(unsigned i=0;i<4;i++)
        if(menu_mouse_footer_contains(&menu_mouse_footer[i],menu,i,x,y,sw,sh))
            return (int)menu_mouse_footer[i].descriptor[0];
    return -1;
}

#include "pc_menu_team.inc"
#include "pc_menu_characters.inc"
#include "pc_menu_mapmaker_browser.inc"
#include "pc_menu_briefing.inc"

/* Footer shortcuts are handled by the menu's tick callback BEFORE row
 * navigation. Supply their edges just after original pad input is gathered. */
void pc_display_menu_mouse_input(void (*original)(void))
{
    uint32_t menu=WORD_AT(g_esp+4);
    MenuMouseDispatch *s=menu_mouse_dispatch;
    GuestRegisters r;
    float x,y,gw,gh;
    unsigned sw,sh;
    original();
    mapmaker_pointer_input(menu); /* MapMaker mouse; TSFP_MAPMAKER_POINTER=0 turns it off */
    mapmaker_table_input(menu);   /* MapMaker resource table: a click on it or I toggles it */
    menu_text_input(menu);
    if(!s || s->sampled || s->menu!=menu || !menu_mouse_context() || !guest_writable(menu,0x23c) ||
       WORD_AT(menu+0x38)!=0 || WORD_AT(menu+0x1b0)!=3) return;
    save_registers(&r);
    if(!guest_call(sub_00074760,1,menu)) {restore_registers(&r);return;}
    pc_controls_menu_mode(1);
    s->pointer=pc_controls_menu_pointer();
    if(s->pointer.wheel && mapmaker_pointer_wheel(menu,s->pointer.wheel)) s->pointer.wheel=0; /* prototype */
    if((WORD_AT(menu+0xc4)&0x3Fu) && !s->pointer.wheel) menu_mouse.wheel_pending=0;
    s->actions=pc_menu_mouse_actions(&menu_mouse,menu,s->pointer,GetTickCount64());s->sampled=1;
    /* MapMaker lighting list: its tick reads Back before the row hook. */
    if((s->actions&4) && mapmaker_pointer_tick_back(menu)) s->actions&=~4u;
    /* Text-only pages (including long Objectives) scroll in sub_00073750
     * using held Up/Down, before the later row-navigation hook. One wheel
     * notch supplies one tick; original input clears it on the next tick. */
    if((s->actions&24) && (WORD_AT(menu+0x40)&8u))
        WORD_AT(menu+0xc8)=(WORD_AT(menu+0xc8)&~0xFu)|((s->actions&8)?1u:2u);
    if(!(s->actions&2)) {restore_registers(&r);return;}
    gw=(float)WORD_AT(0x004E8CF0u);gh=(float)WORD_AT(0x004E8CF4u);
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(sh && pc_menu_mouse_position(s->pointer,(float)sw/sh,gw,gh,&x,&y)) {
        uint32_t tab=menu_mouse_tab_hit(menu,x,y,sw,sh);
        if(tab) {
            s->tab_menu=menu;s->tab_target=tab;s->actions=0;
            restore_registers(&r);return;
        }
        int button=menu_mouse_profile_hit(menu,x,y,sw,sh);
        if(button<0) button=menu_mouse_footer_hit(menu,x,y,sw,sh);
        if(button>=0) {
            static const uint32_t raw[]={0x80u,0x120u,0x10u,0x40u};
            uint32_t action=0x10u<<(unsigned)button;
            menu_mouse_press(s,raw[button]);
            WORD_AT(menu+0xc4)=(WORD_AT(menu+0xc4)&~0x000F000Fu)|action|(action<<16);
            WORD_AT(menu+0xc8)|=action;
            s->actions=0; /* a footer click must never also select a row */
            menu_mouse.mouse_navigation=1;
        } else if(!menu_map_browser_click(s,menu,x,y)) menu_briefing_click(s,menu);
    }
    restore_registers(&r);
}

void pc_display_menu_mouse_dispatch(void (*original)(void))
{
    MenuMouseDispatch state={0};
    state.menu=WORD_AT(g_esp+4);
    pc_display_menu_mouse_mode();
    MenuMouseDispatch *previous=menu_mouse_dispatch;
    menu_mouse_dispatch=&state;
    menu_character_input(&state);
    menu_text_focus(state.menu);
    original();
    if(state.injected)
        WORD_AT(state.pressed_address)=(WORD_AT(state.pressed_address)&~state.injected)|(state.pressed&state.injected);
    menu_mouse_dispatch=previous;
    /* Wait until the original dispatcher and its callbacks have returned.
     * Closing it during row processing would invalidate its current menu. */
    if(state.close_pause && previous) previous->close_pause=state.close_pause;
    else if(state.close_pause && menu_mouse_can_close_pause(state.close_pause)) {
        GuestRegisters r;
        save_registers(&r);
        guest_call(sub_002E6D40,1,0u);
        restore_registers(&r);
    }
    if(state.tab_target && previous) {
        previous->tab_menu=state.tab_menu;previous->tab_target=state.tab_target;
    } else if(state.tab_target && !state.close_pause) {
        menu_mouse_change_tab(state.tab_menu,state.tab_target);
    }
    pc_display_menu_mouse_mode();
    if(menu_mouse_context())menu_text_events(state.menu);
}

void pc_display_menu_mouse(void (*original)(void))
{
    uint32_t menu=WORD_AT(g_esp+4),item,hit=0;
    float x,y,gw,gh;
    unsigned actions,sw,sh,choice_direction;
    GuestRegisters r;
    PcMenuPointer pointer;
    uint32_t caller=WORD_AT(g_esp);
    original();
    mapmaker_probe_menu(menu,caller); /* read-only; inert unless TSFP_MAPMAKER_PROBE has flag 8 */
    if(!menu_mouse_context() || !guest_writable(menu,0x23c) ||
       WORD_AT(menu+0x38)!=0 || WORD_AT(menu+0x1b0)!=3) {
        if(menu_slider_owner.menu==menu) menu_slider_drag=0;
        return;
    }
    save_registers(&r);
    if(!guest_call(sub_00074760,1,menu)) {
        if(menu_slider_owner.menu==menu) menu_slider_drag=0;
        restore_registers(&r); return;
    }
    pc_controls_menu_mode(1);
    if(menu_mouse_dispatch && menu_mouse_dispatch->menu==menu && menu_mouse_dispatch->sampled) {
        pointer=menu_mouse_dispatch->pointer;actions=menu_mouse_dispatch->actions;
    } else {
        pointer=pc_controls_menu_pointer();
        if(pointer.wheel && mapmaker_pointer_wheel(menu,pointer.wheel)) pointer.wheel=0; /* prototype */
        if((WORD_AT(menu+0xc4)&0x3Fu) && !pointer.wheel) menu_mouse.wheel_pending=0;
        actions=pc_menu_mouse_actions(&menu_mouse,menu,pointer,GetTickCount64());
    }
    /* Menu rectangles are absolute pixels on the full UI canvas. The
     * current viewport during paused input can still be the cropped world
     * camera, so using it offsets the cursor from the later menu drawing. */
    gw=(float)WORD_AT(0x004E8CF0u);gh=(float)WORD_AT(0x004E8CF4u);
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!sh || !pc_menu_mouse_position(pointer,(float)sw/sh,gw,gh,&x,&y)) {
        menu_mouse.wheel_pending=0;
        menu_slider_drag=0;
        restore_registers(&r); return;
    }
    if(menu_slider_pointer(menu,pointer,actions,x,y,sw,sh)) {
        restore_registers(&r);return;
    }
    /* Use final screen rectangles, including scroll offsets and submenu
     * animations. Ignore labels, disabled rows and clipped-away portions. */
    if(x>=FLOAT_AT(menu+0xe4) && x<FLOAT_AT(menu+0xec) &&
       y>=FLOAT_AT(menu+0xe8) && y<FLOAT_AT(menu+0xf0)) {
        item=WORD_AT(menu+0x178);
        for(unsigned n=0;item && n<256 && guest_writable(item,0x10c);n++,item=WORD_AT(item+4)) {
            uint32_t type=WORD_AT(item),flags=WORD_AT(item+0x28);
            if((type==4 || type==7 || type==8) && !(flags&5) &&
               x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&
               y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) { hit=item; break; }
        }
    }
    choice_direction=menu_choice_hit(menu,x,y,sw,sh,&hit);
    mapmaker_probe_click(menu,x,y,hit,actions); /* read-only diagnostic */
    if(!hit && (actions&2) && !mapmaker_pointer_parent_press(menu,x,y))
        mapmaker_pointer_scroll_press(menu,x,y); /* MapMaker prototype */
    /* The original dispatcher applies directional repeats after this hook.
     * Do not let a previously held direction fight an actively chosen mouse
     * row. A fresh keyboard/controller direction immediately takes ownership
     * once the pointer stops moving. Retain ownership between mouse packets. */
    if((hit && (actions&3)) || (actions&28)) menu_mouse.mouse_navigation=1;
    else if(WORD_AT(menu+0xc4)&0xFu) menu_mouse.mouse_navigation=0;
    if(menu_mouse.mouse_navigation) WORD_AT(menu+0xc4)&=~0x000F000Fu;
    if(actions&24) {
        /* Use the game's row navigation, including clipping/scrolling and
         * disabled rows. Do not synthesize Confirm or hold a direction. */
        WORD_AT(menu+0xc8)&=~0xFu;
        WORD_AT(menu+0xc4)|=(actions&8)?0x10001u:0x20002u;
    }
    if(hit && (actions&3)) {
        if(WORD_AT(menu+0x198)!=hit) guest_call(sub_00076EF0,2,menu,hit);
        if(actions&2) {
            if(choice_direction) {
                /* Exactly one native Previous/Next edge, not Confirm. Native
                 * callbacks retain wrapping, disabled choices and saving. */
                WORD_AT(menu+0xc4)=(WORD_AT(menu+0xc4)&~0x000F001Fu)|
                    (choice_direction<<16);
                WORD_AT(menu+0xc8)&=~0x1Fu;
                menu_mouse.wheel_pending=0;
                restore_registers(&r);return;
            }
            /* MapMaker editor menus act on A, not on this confirm (prototype). */
            if(!mapmaker_pointer_row_click()) {
            if(menu_mouse_dispatch && menu_mouse_dispatch->menu==menu &&
               !menu_mouse_dispatch->injected) {
                menu_mouse_press(menu_mouse_dispatch,0x80u);
            }
            WORD_AT(menu+0xc4)|=0x100010u; /* confirm press + repeat edge */
            WORD_AT(menu+0xc8)|=0x10u;
            }
        }
    }
    /* The MapMaker editor screen uses the right button to pan (prototype). */
    if((actions&4) && !(mapmaker_pointer_enabled() && mapmaker_pointer_context(menu))) {
        if(menu_mouse_dispatch && menu_mouse_dispatch->menu==menu &&
           menu_mouse_can_close_pause(menu)) menu_mouse_dispatch->close_pause=menu;
        else WORD_AT(menu+0xc4)|=0x20u; /* original submenu Back action */
    }
    restore_registers(&r);
}
#else
void pc_display_menu_character_draw(void (*original)(void)) { original(); }
static void menu_character_quad(void) {}
void pc_display_menu_choice_draw(void (*original)(void)) { original(); }
void pc_display_menu_team_draw(void (*original)(void)) { original(); }
void pc_display_menu_team_tick(void (*original)(void)) { original(); }
void pc_display_menu_choice_arrow(void (*original)(void),unsigned direction) { (void)direction;original(); }
void pc_display_menu_audio_draw(void (*original)(void),int compact) { (void)compact;original(); }
void pc_display_menu_sensitivity_draw(void (*original)(void)) { original(); }
void pc_display_menu_slider_bar(void (*original)(void)) { original(); }
void pc_display_mapmaker_meter(void (*original)(void)) { original(); }
void pc_display_mapmaker_memory(void (*original)(void)) { original(); }
void pc_display_mapmaker_pools(void (*original)(void)) { original(); }
void pc_display_mapmaker_limits(void (*original)(void)) { original(); }
int pc_mapmaker_area_enabled(const char *area,const char *hook) { (void)area;(void)hook;return 1; }
/* MapMaker capacity area stubs (begin) */
void pc_mapmaker_pads_group_node(void (*original)(void)) { original(); }
void pc_mapmaker_pads_pad_node(void (*original)(void)) { original(); }
void pc_mapmaker_pads_cover_topup(void (*original)(void)) { original(); }
void pc_mapmaker_container_frontend_init(void (*original)(void)) { original(); }
void pc_mapmaker_container_editor_init(void (*original)(void)) { original(); }
void pc_mapmaker_container_undo_ring(void (*original)(void)) { original(); }
void pc_mapmaker_container_load(void (*original)(void)) { original(); }
void pc_mapmaker_container_save_async(void (*original)(void)) { original(); }
void pc_mapmaker_container_save_sign(void (*original)(void)) { original(); }
void pc_mapmaker_container_save_sync(void (*original)(void)) { original(); }
void pc_mapmaker_container_crc(void (*original)(void)) { original(); }
void pc_mapmaker_container_crc_check(void (*original)(void)) { original(); }
void pc_mapmaker_container_compile(void (*original)(void)) { original(); }
void pc_mapmaker_container_compiled_size(void (*original)(void)) { original(); }
void pc_mapmaker_container_find(void (*original)(void)) { original(); }
void pc_mapmaker_container_net_send(void (*original)(void)) { original(); }
void pc_mapmaker_objects_turret_create(void (*original)(void)) { original(); }
void pc_mapmaker_objects_turret_init(void (*original)(void)) { original(); }
void pc_mapmaker_objects_list_add(void (*original)(void)) { original(); }
void pc_mapmaker_objects_register(void (*original)(void)) { original(); }
void pc_mapmaker_objects_pool(void (*original)(void)) { original(); }
void pc_mapmaker_objects_twin_pool(void (*original)(void)) { original(); }
void pc_mapmaker_objects_teleporters(void (*original)(void)) { original(); }
void pc_mapmaker_objects_tag(void (*original)(void)) { original(); }
void pc_mapmaker_objects_physics(void (*original)(void)) { original(); }
void pc_mapmaker_objects_prop_break(void (*original)(void)) { original(); }
void pc_mapmaker_editor_sai_add(void (*original)(void)) { original(); }
void pc_mapmaker_editor_load(void (*original)(void)) { original(); }
void pc_mapmaker_editor_level_load(void (*original)(void)) { original(); }
void pc_mapmaker_editor_logic_can_add(void (*original)(void)) { original(); }
void pc_mapmaker_editor_logic_full(void (*original)(void)) { original(); }
void pc_mapmaker_editor_objective(void (*original)(void)) { original(); }
void pc_mapmaker_editor_node_room(void (*original)(void)) { original(); }
void pc_mapmaker_editor_logic_room(void (*original)(void)) { original(); }
void pc_mapmaker_editor_logic_slot(void (*original)(void)) { original(); }
void pc_mapmaker_editor_free_place(void (*original)(void)) { original(); }
void pc_mapmaker_editor_free_move(void (*original)(void)) { original(); }
void pc_mapmaker_editor_sai_point(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_query(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_compile(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_piece(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_props(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_light_reset(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_light_unlink(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_light_colour(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_light_link(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_light_create(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_buckets(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_view_lists(void (*original)(void)) { original(); }
void pc_mapmaker_rooms_bucket_draw(void (*original)(void)) { original(); }
void pc_mapmaker_actors_spawn_gate(void (*original)(void)) { original(); }
void pc_mapmaker_actors_slot(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pool(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pool_clear(void (*original)(void)) { original(); }
void pc_mapmaker_actors_placed(void (*original)(void)) { original(); }
void pc_mapmaker_actors_list_add(void (*original)(void)) { original(); }
void pc_mapmaker_actors_list_readd(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pick_type(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pick_team_ai(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pick_group(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pick_team(void (*original)(void)) { original(); }
void pc_mapmaker_actors_pick_group_ai(void (*original)(void)) { original(); }
void pc_mapmaker_actors_enemies(void (*original)(void)) { original(); }
void pc_mapmaker_actors_kill(void (*original)(void)) { original(); }
void pc_mapmaker_actors_score(void (*original)(void)) { original(); }
void pc_mapmaker_actors_deaths(void (*original)(void)) { original(); }
void pc_mapmaker_actors_create(void (*original)(void)) { original(); }
void pc_mapmaker_actors_blip(void (*original)(void)) { original(); }
void pc_mapmaker_actors_effects(void (*original)(void)) { original(); }
void pc_mapmaker_story_char_roster(void (*original)(void)) { original(); }
void pc_mapmaker_story_char_preview(void (*original)(void)) { original(); }
void pc_mapmaker_story_char_confirm(void (*original)(void)) { original(); }
void pc_mapmaker_story_char_config(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_rows(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_close(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_open(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_delete(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_compile(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_load(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_level(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_event(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_remove(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_spawn_gate(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_alert(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_menu_item(void (*original)(void)) { original(); }
void pc_mapmaker_aitemplates_removed(void (*original)(void)) { original(); }
void pc_mapmaker_grid_alloc(void (*original)(void)) { original(); }
void pc_mapmaker_grid_reset(void (*original)(void)) { original(); }
void pc_mapmaker_grid_insert(void (*original)(void)) { original(); }
void pc_mapmaker_grid_remove(void (*original)(void)) { original(); }
void pc_mapmaker_grid_lookup(void (*original)(void)) { original(); }
void pc_mapmaker_grid_placement(void (*original)(void)) { original(); }
void pc_mapmaker_grid_floor_sound(void (*original)(void)) { original(); }
void pc_mapmaker_grid_floor_bar(void (*original)(void)) { original(); }
void pc_mapmaker_overlap_cell(void (*original)(void)) { original(); }
void pc_mapmaker_stack_find(void (*original)(void)) { original(); }
void pc_mapmaker_stack_cell(void (*original)(void)) { original(); }
void pc_mapmaker_stack_valid(void (*original)(void)) { original(); }
void pc_mapmaker_stack_sai_point(void (*original)(void)) { original(); }
void pc_mapmaker_grid_view(void (*original)(void)) { original(); }
void pc_mapmaker_grid_lines(void (*original)(void)) { original(); }
void pc_mapmaker_grid_preview_floor(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_net_fragment(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_turret_sync(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_net_death(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_bot_goal(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_hud(void (*original)(void)) { original(); }
void pc_mapmaker_bugfix_hud_turret(void (*original)(void)) { original(); }
void pc_mapmaker_validate_level(void (*original)(void)) { original(); }
void pc_mapmaker_sky_clouds(void (*original)(void)) { original(); }
void pc_mapmaker_sky_settings(void (*original)(void)) { original(); }
void pc_mapmaker_sky_reset(void (*original)(void)) { original(); }
void pc_mapmaker_sky_compile(void (*original)(void)) { original(); }
void pc_mapmaker_probe_room_draw(void (*original)(void)) { original(); }
void pc_mapmaker_probe_prop_draw(void (*original)(void)) { original(); }
void pc_mapmaker_sky_room_mask(void (*original)(void)) { original(); }
void pc_mapmaker_roof_mask(void (*original)(void)) { original(); }
void pc_mapmaker_roof_reduced(void (*original)(void)) { original(); }
void pc_mapmaker_roof_piece_new(void (*original)(void)) { original(); }
void pc_mapmaker_roof_reset(void (*original)(void)) { original(); }
void pc_mapmaker_roof_compile(void (*original)(void)) { original(); }
void pc_mapmaker_roof_load(void (*original)(void)) { original(); }
void pc_mapmaker_roof_menu_init(void (*original)(void)) { original(); }
void pc_mapmaker_roof_menu_add(void (*original)(void)) { original(); }
void pc_mapmaker_roof_menu_hover(void (*original)(void)) { original(); }
void pc_mapmaker_roof_menu_pick(void (*original)(void)) { original(); }
void pc_mapmaker_roof_move(void (*original)(void)) { original(); }
void pc_mapmaker_roof_copy(void (*original)(void)) { original(); }
void pc_mapmaker_roof_piece_free(void (*original)(void)) { original(); }
void pc_mapmaker_roof_text(void (*original)(void)) { original(); }
void pc_mapmaker_roof_format(void (*original)(void)) { original(); }
void pc_mapmaker_tileflags_tiles(void (*original)(void)) { original(); }
void pc_mapmaker_tileflags_build(void (*original)(void)) { original(); }
void pc_mapmaker_tileflags_tick(void (*original)(void)) { original(); }
void pc_mapmaker_roof_play(void (*original)(void)) { original(); }
void pc_mapmaker_roof_rooms(void (*original)(void)) { original(); }
void pc_mapmaker_roof_draw(void (*original)(void)) { original(); }
void pc_mapmaker_roof_tick(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_rows(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_compile(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_load(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_mount(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_level(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_weather_load(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_weather_start(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_music(void (*original)(void)) { original(); }
void pc_mapmaker_roof_caption(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_sky_built(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_weather_frame(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_weather_list(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_haze(void (*original)(void)) { original(); }
void pc_mapmaker_skymusic_grain_colour(void (*original)(void)) { original(); }
void pc_mapmaker_camera_gun_update(void (*original)(void)) { original(); }
void pc_mapmaker_openview_walk(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_picker_end(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_pick(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_highlight(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_row_of(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_row_text(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_text(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_model_request(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_anims(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_hud(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_meter(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_level(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_princess(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_risen(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_throw(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_event(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_message(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_tag(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_fx(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_face(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_create(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_skel_slot(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_walk_fight(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_walk_message(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_walk_damage(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_walk_slot(void (*original)(void)) { original(); }
void pc_mapmaker_realbosses_restart(void (*original)(void)) { original(); }
void pc_mapmaker_robot_anims(void (*original)(void)) { original(); }
void pc_mapmaker_robot_level(void (*original)(void)) { original(); }
void pc_mapmaker_robot_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_robot_nav(void (*original)(void)) { original(); }
void pc_mapmaker_robot_music(void (*original)(void)) { original(); }
void pc_mapmaker_robot_event(void (*original)(void)) { original(); }
void pc_mapmaker_robot_fx(void (*original)(void)) { original(); }
void pc_mapmaker_robot_reset(void (*original)(void)) { original(); }
void pc_mapmaker_robot_hud(void (*original)(void)) { original(); }
void pc_mapmaker_robot_handler(void (*original)(void)) { original(); }
void pc_mapmaker_robot_message(void (*original)(void)) { original(); }
void pc_mapmaker_robot_callback(void (*original)(void)) { original(); }
void pc_mapmaker_robot_damage(void (*original)(void)) { original(); }
void pc_mapmaker_robot_goal(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_anims(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_level(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_fight(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_near(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_sparks(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_part(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_event(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_fx(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_hud(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_meter(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_think(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_message(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_hit(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_eggs(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_cells(void (*original)(void)) { original(); }
void pc_mapmaker_crowe_restart(void (*original)(void)) { original(); }
void pc_mapmaker_tank_mount(void (*original)(void)) { original(); }
void pc_mapmaker_tank_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_tank_update(void (*original)(void)) { original(); }
void pc_mapmaker_tank_wake(void (*original)(void)) { original(); }
void pc_mapmaker_tank_area(void (*original)(void)) { original(); }
void pc_mapmaker_tank_target(void (*original)(void)) { original(); }
void pc_mapmaker_tank_state(void (*original)(void)) { original(); }
void pc_mapmaker_tank_wreck(void (*original)(void)) { original(); }
void pc_mapmaker_tank_event(void (*original)(void)) { original(); }
void pc_mapmaker_tank_text(void (*original)(void)) { original(); }
void pc_mapmaker_tank_request(void (*original)(void)) { original(); }
void pc_mapmaker_tank_gun(void (*original)(void)) { original(); }
void pc_mapmaker_tank_create(void (*original)(void)) { original(); }
void pc_mapmaker_tank_spawn(void (*original)(void)) { original(); }
void pc_mapmaker_tank_slot7(void (*original)(void)) { original(); }
void pc_mapmaker_tank_slot2(void (*original)(void)) { original(); }
void pc_mapmaker_tank_damage(void (*original)(void)) { original(); }
void pc_mapmaker_tank_bar(void (*original)(void)) { original(); }
void pc_mapmaker_tank_draw(void (*original)(void)) { original(); }
void pc_mapmaker_tank_restart(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_mount(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_anims(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_request(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_update(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_damage(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_event(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_text(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_flame(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_flash(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_shot(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_sound(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_engine(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_rider(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_rider_pose(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_create(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_hud(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_sound_load(void (*original)(void)) { original(); }
void pc_mapmaker_khallos_restart(void (*original)(void)) { original(); }
void pc_mapmaker_worm_anims(void (*original)(void)) { original(); }
void pc_mapmaker_worm_level(void (*original)(void)) { original(); }
void pc_mapmaker_worm_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_worm_tick(void (*original)(void)) { original(); }
void pc_mapmaker_worm_tag(void (*original)(void)) { original(); }
void pc_mapmaker_worm_hit(void (*original)(void)) { original(); }
void pc_mapmaker_worm_fx(void (*original)(void)) { original(); }
void pc_mapmaker_worm_props(void (*original)(void)) { original(); }
void pc_mapmaker_worm_extra(void (*original)(void)) { original(); }
void pc_mapmaker_worm_script(void (*original)(void)) { original(); }
void pc_mapmaker_worm_register(void (*original)(void)) { original(); }
void pc_mapmaker_worm_gone(void (*original)(void)) { original(); }
void pc_mapmaker_worm_restart(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_mount(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_request(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_ai_start(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_update(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_text(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_slot10(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_damage(void (*original)(void)) { original(); }
void pc_mapmaker_mothership_restart(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_rows(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_close(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_delete(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_reset(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_compile(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_load(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_level(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_hud(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_health_rows(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_meters(void (*original)(void)) { original(); }
void pc_mapmaker_bosses_restart(void (*original)(void)) { original(); }
void pc_mapmaker_characters_eligible(void (*original)(void)) { original(); }
void pc_mapmaker_characters_preload(void (*original)(void)) { original(); }
void pc_mapmaker_sets_compile(void (*original)(void)) { original(); }
void pc_mapmaker_sets_load(void (*original)(void)) { original(); }
void pc_mapmaker_sets_reset(void (*original)(void)) { original(); }
void pc_mapmaker_sets_changed(void (*original)(void)) { original(); }
void pc_mapmaker_sets_screen(void (*original)(void)) { original(); }
void pc_mapmaker_sets_set_ptr(void (*original)(void)) { original(); }
void pc_mapmaker_sets_editor_set(void (*original)(void)) { original(); }
void pc_mapmaker_sets_store(void (*original)(void)) { original(); }
void pc_mapmaker_sets_sai_choice(void (*original)(void)) { original(); }
void pc_mapmaker_sets_play_set(void (*original)(void)) { original(); }
void pc_mapmaker_sets_preload(void (*original)(void)) { original(); }
void pc_mapmaker_sets_match_start(void (*original)(void)) { original(); }
void pc_mapmaker_sets_icon(void (*original)(void)) { original(); }
void pc_mapmaker_sets_build(void (*original)(void)) { original(); }
void pc_mapmaker_sets_row_draw(void (*original)(void)) { original(); }
void pc_mapmaker_sets_row_action(void (*original)(void)) { original(); }
void pc_mapmaker_sets_pane(void (*original)(void)) { original(); }
void pc_mapmaker_sets_pick(void (*original)(void)) { original(); }
void pc_mapmaker_sets_spawn_masks(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_live(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_restart(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_match_start(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_screen(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_open(void (*original)(void)) { original(); }
void pc_mapmaker_matchbots_store(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_compile(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_load(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_reset(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_changed(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_held(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_build(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_screen(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_get(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_set(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_options(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_title(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_sai_choice(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_play_set(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_slot(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_weapon_veto(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_ammo_veto(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_pickup(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_preload(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_row(void (*original)(void)) { original(); }
void pc_mapmaker_weaponset_label(void (*original)(void)) { original(); }
void pc_mapmaker_bigdraw_rect(void (*original)(void)) { original(); }
void pc_mapmaker_tilesets_menu_row(void (*original)(void)) { original(); }
void pc_mapmaker_tilesets_play_pak(void (*original)(void)) { original(); }
void pc_mapmaker_memory_heap_alloc(void (*original)(void)) { original(); }
void pc_mapmaker_memory_heap_alloc_aligned(void (*original)(void)) { original(); }
void pc_mapmaker_memory_arena(void (*original)(void)) { original(); }
void pc_mapmaker_memory_level_reset(void (*original)(void)) { original(); }
void pc_display_viewfinder(void (*original)(void)) { original(); }
void pc_display_view_static(void (*original)(void)) { original(); }
void pc_custom_music_channel(void (*original)(void)) { original(); }
void pc_custom_music_arcade_rows(void (*original)(void)) { original(); }
void pc_custom_music_arcade_show(void (*original)(void)) { original(); }
void pc_custom_music_arcade_store(void (*original)(void)) { original(); }
/* MapMaker capacity area stubs (end) */
static int menu_mouse_profile_text(void (*original)(void)) { (void)original;return 0; }
void pc_display_menu_mouse_mode(void) {}
static int menu_mouse_footer_text(void) { return 0; }
void pc_display_menu_mouse_footer(void (*original)(void)) { original(); }
void pc_display_menu_mouse_input(void (*original)(void)) { original(); }
void pc_display_menu_mouse(void (*original)(void)) { original(); }
void pc_display_menu_mouse_dispatch(void (*original)(void)) { original(); }
#endif
