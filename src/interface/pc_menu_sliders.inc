/* US 45410066 custom menu sliders. Share capture, hit testing and drag
 * ownership, while each setting retains its original draft/commit behavior. */
extern void sub_00045C60(void),sub_00021780(void),sub_00074720(void),sub_00071A60(void);
typedef struct MenuSliderTrack {
    uint32_t menu,descriptor,item,id,player;
    float x,y,w,h;
    unsigned sw,sh;
    uint64_t time;
} MenuSliderTrack;
enum { MENU_MUSIC, MENU_SOUND, MENU_SENSITIVITY, MENU_FOV, MENU_KEYBOARD_SENSITIVITY, MENU_WEAPON_FOV, MENU_SLIDER_COUNT };
static MenuSliderTrack menu_slider_tracks[MENU_SLIDER_COUNT];
static uint32_t menu_slider_drag;
static MenuSliderTrack menu_slider_owner;

/* Capture the drawn bar, not the label or guessed row bounds. */
static void menu_slider_capture(unsigned kind,uint32_t menu,uint32_t item,
    float x,float y,float w,float h)
{
    MenuSliderTrack *t=&menu_slider_tracks[kind];
    memset(t,0,sizeof *t);
    if(!guest_writable(menu,0x23c) || !guest_writable(item,0x10c) ||
       WORD_AT(item+0x88)!=menu || !isfinite(x) || !isfinite(y) ||
       !isfinite(w) || !isfinite(h) || fabsf(x)>100000 || fabsf(y)>100000 ||
       w<=0 || h<=0 || w>100000 || h>100000) return;
    t->menu=menu;t->descriptor=WORD_AT(menu+4);t->item=item;t->id=WORD_AT(item+0x14);
    t->player=WORD_AT(0x007BA9E0u);
    t->x=x;t->y=y;t->w=w;t->h=h;
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&t->sw,&t->sh);
    t->time=GetTickCount64();
}

/* Music slider ticks past 16 (pc_custom_music.h): the guest word and the
 * profile (+0x120, sub_002C39B0/sub_002C3AC0) keep 0..16; the ticks past 16
 * are pc_music_slider_extra(), counted only while the word is 16. Both audio
 * layouts (descriptor 0x0052377C, opened by Settings 0x002CBE1A and the pause
 * menu 0x002EC215) share this bar, tick and the mouse drag below. */
#define MENU_MUSIC_WORD 0x005236BCu
#define MENU_MUSIC_TICKS (PC_MUSIC_SLIDER_STOCK+PC_MUSIC_SLIDER_EXTRA_MAX)
static unsigned menu_music_position(void)
{
    uint32_t value=WORD_AT(MENU_MUSIC_WORD);
    if(value<PC_MUSIC_SLIDER_STOCK) return value;
    return PC_MUSIC_SLIDER_STOCK+(unsigned)pc_music_slider_extra();
}
static void menu_music_click(void)
{
    if(WORD_AT(0x007BA16Cu) && guest_writable(WORD_AT(0x007BA178u),12))
        guest_call(sub_00021780,1,WORD_AT(WORD_AT(0x007BA178u)+8));
}
/* A position 0..MENU_MUSIC_TICKS: the guest word never exceeds 16. */
static void menu_music_set(unsigned position)
{
    unsigned value=position<PC_MUSIC_SLIDER_STOCK?position:PC_MUSIC_SLIDER_STOCK;
    unsigned extra=position>PC_MUSIC_SLIDER_STOCK?position-PC_MUSIC_SLIDER_STOCK:0;
    if(extra>PC_MUSIC_SLIDER_EXTRA_MAX) extra=PC_MUSIC_SLIDER_EXTRA_MAX;
    if(menu_music_position()==value+extra) return;
    if(WORD_AT(MENU_MUSIC_WORD)!=value) {WORD_AT(MENU_MUSIC_WORD)=value;guest_call(sub_00045C60,0);}
    pc_music_slider_extra_set((int)extra);
    menu_music_click();
}
/* sub_002C3720, the audio menu's tick (reached through pc_display_menu_lookup).
 * It steps the focused Music item (id 1, enabled) by menu+0xC4 0x40000 Left /
 * 0x80000 Right, Left first, clamped 0..16, then calls sub_00045C60 and the
 * click sub_00021780. Past 16 the original sees 16: Right there adds a tick;
 * Left takes one away and the 15 the original stored goes back to 16. */
void pc_display_menu_audio_tick(void (*original)(void))
{
    uint32_t menu=WORD_AT(g_esp+4),input=0,before=WORD_AT(MENU_MUSIC_WORD),after;
    int music=0,extra=pc_music_slider_extra();
    if(guest_writable(menu,0x23c)) input=WORD_AT(menu+0xc4)&0xC0000u;
    if(input && guest_call(sub_00074720,1,menu)) {
        uint32_t item=guest_call(sub_00071A60,1,menu);
        music=guest_writable(item,0x2c) && WORD_AT(item+0x14)==1 && !(WORD_AT(item+0x28)&1u);
    }
    original();
    after=WORD_AT(MENU_MUSIC_WORD);
    if(music && before==PC_MUSIC_SLIDER_STOCK) {
        if(input&0x40000u) {
            if(extra>0) {
                WORD_AT(MENU_MUSIC_WORD)=PC_MUSIC_SLIDER_STOCK;
                if(after!=PC_MUSIC_SLIDER_STOCK) guest_call(sub_00045C60,0);
                pc_music_slider_extra_set(extra-1);
            }
        } else if(after==PC_MUSIC_SLIDER_STOCK && extra<PC_MUSIC_SLIDER_EXTRA_MAX) {
            pc_music_slider_extra_set(extra+1);
            menu_music_click();
        }
    }
    if(before<PC_MUSIC_SLIDER_STOCK && after==PC_MUSIC_SLIDER_STOCK && extra) pc_music_slider_extra_set(0);
}

/* The bar painter sub_000814B0 takes (fraction, x, y, width, height, ...); the
 * audio painters call it at 0x002C35F4 / 0x002C3704 with the word x 1/16.
 * The Music bar gets position / MENU_MUSIC_TICKS: same rectangle, finer steps. */
static uint32_t menu_slider_audio_id;

/* Audio painters use these exact integer offsets in both menu layouts. */
void pc_display_menu_audio_draw(void (*original)(void),int compact)
{
    uint32_t menu=WORD_AT(g_esp+4),item=WORD_AT(g_esp+8),rect=WORD_AT(g_esp+12);
    uint32_t previous_id=menu_slider_audio_id;
    menu_slider_audio_id=0;
    if(guest_writable(item,0x10c) && guest_writable(rect,16)) {
        uint32_t id=WORD_AT(item+0x14);
        menu_slider_audio_id=id;
        if(id==1 || id==3) {
            float left=FLOAT_AT(rect),top=FLOAT_AT(rect+4);
            float right=FLOAT_AT(rect+8),bottom=FLOAT_AT(rect+12);
            memset(&menu_slider_tracks[id==3],0,sizeof(MenuSliderTrack));
            if(isfinite(left) && isfinite(top) && isfinite(right) && isfinite(bottom) &&
               fabsf(left)<100000 && fabsf(top)<100000 && fabsf(right)<100000 && fabsf(bottom)<100000 &&
               right-left>65 && bottom>top)
                menu_slider_capture(id==3,menu,item,(float)(int)(left+45.f),
                    (float)(int)(top+(bottom-top)*(compact?.65f:.5f)),
                    (float)(int)(right-left-65.f),compact?10.f:20.f);
        }
    }
    original();
    menu_slider_audio_id=previous_id;
}

/* Sensitivity width depends on translated label/font metrics. Capture its
 * actual call to the shared bar painter within this narrowly scoped wrapper.
 * Other callers (including the MapMaker capacity meter) stay display-only. */
static uint32_t menu_slider_draw_menu,menu_slider_draw_item;
void pc_display_menu_sensitivity_draw(void (*original)(void))
{
    uint32_t previous_menu=menu_slider_draw_menu,previous_item=menu_slider_draw_item;
    menu_slider_draw_menu=WORD_AT(g_esp+4);menu_slider_draw_item=WORD_AT(g_esp+8);
    memset(&menu_slider_tracks[MENU_SENSITIVITY],0,sizeof(MenuSliderTrack));
    original();
    menu_slider_draw_menu=previous_menu;menu_slider_draw_item=previous_item;
}
void pc_display_menu_slider_bar(void (*original)(void))
{
    mapmaker_probe_bar(); /* read-only; inert unless TSFP_MAPMAKER_PROBE */
    if(menu_slider_audio_id==1 && (WORD_AT(g_esp)==0x002C35F9u || WORD_AT(g_esp)==0x002C3709u))
        FLOAT_AT(g_esp+4)=(float)menu_music_position()/(float)MENU_MUSIC_TICKS;
    if(menu_slider_draw_menu && WORD_AT(g_esp)==0x002EA3E0u &&
       guest_writable(menu_slider_draw_item,0x10c) &&
       WORD_AT(menu_slider_draw_item+0x14)==9 && WORD_AT(0x007BA9E0u)<4)
        menu_slider_capture(MENU_SENSITIVITY,menu_slider_draw_menu,menu_slider_draw_item,
            (float)(int32_t)WORD_AT(g_esp+8),(float)(int32_t)WORD_AT(g_esp+12),
            (float)(int32_t)WORD_AT(g_esp+16),(float)(int32_t)WORD_AT(g_esp+20));
    original();
}
static void menu_slider_fov_pointer(float x,float left,float width);
static void menu_slider_weapon_fov_pointer(float x,float left,float width);
static void menu_slider_keyboard_pointer(float x,float left,float width);
extern void sub_00045390(void),sub_00045820(void);
static void menu_slider_set(const MenuSliderTrack *track,unsigned kind,float x)
{
    if(kind==MENU_KEYBOARD_SENSITIVITY) {
        menu_slider_keyboard_pointer(x,track->x,track->w);
    } else if(kind==MENU_WEAPON_FOV) {
        menu_slider_weapon_fov_pointer(x,track->x,track->w);
    } else if(kind==MENU_FOV) {
        menu_slider_fov_pointer(x,track->x,track->w);
    } else if(kind==MENU_SENSITIVITY) {
        uint32_t address=0x007BA3DCu+track->player*0x1E0u;
        float value=(float)pc_menu_slider_value(x,track->x,track->w,20)*.05f;
        if(FLOAT_AT(address)!=value) {
            FLOAT_AT(address)=value;
            /* Original sub_002EAB30 preview; Accept/Cancel still own saving. */
            if(WORD_AT(0x007BA16Cu) && guest_writable(WORD_AT(0x007BA178u),12)) {
                uint32_t preview=WORD_AT(WORD_AT(0x007BA178u)+8),sp=g_esp;
                g_esp-=0x30;
                guest_call(sub_00045390,1,g_esp);
                WORD_AT(g_esp)=preview;
                guest_call(sub_00045820,1,g_esp);
                g_esp=sp;
            }
        }
    } else if(kind==MENU_MUSIC) {
        menu_music_set(pc_menu_slider_value(x,track->x,track->w,MENU_MUSIC_TICKS));
    } else {
        unsigned value=pc_menu_slider_value(x,track->x,track->w,16);
        uint32_t volume=0x005236C0u;
        if(WORD_AT(volume)!=value) {
            WORD_AT(volume)=value;
            menu_music_click();
        }
    }
}

static int menu_slider_track_active(const MenuSliderTrack *t,uint32_t menu,unsigned sw,unsigned sh)
{
    return t->menu==menu && t->descriptor==WORD_AT(menu+4) &&
        t->player==WORD_AT(0x007BA9E0u) &&
        t->sw==sw && t->sh==sh && GetTickCount64()-t->time<=100 &&
        guest_writable(t->item,0x10c) && WORD_AT(t->item+0x88)==menu &&
        WORD_AT(t->item+0x14)==t->id &&
        !(WORD_AT(t->item+0x28)&5u) && t->w>0 && t->h>0;
}

static int menu_slider_pointer(uint32_t menu,PcMenuPointer p,unsigned actions,
    float x,float y,unsigned sw,unsigned sh)
{
    MenuSliderTrack *track=NULL;
    if(!p.left || p.right || menu_mouse.blocked) menu_slider_drag=0;
    for(unsigned i=0;i<MENU_SLIDER_COUNT;i++) {
        MenuSliderTrack *t=&menu_slider_tracks[i];
        if(!menu_slider_track_active(t,menu,sw,sh)) continue;
        if(menu_slider_drag==t->item && menu_slider_owner.menu==t->menu &&
           menu_slider_owner.descriptor==t->descriptor && menu_slider_owner.id==t->id &&
           menu_slider_owner.player==t->player && menu_slider_owner.sw==sw &&
           menu_slider_owner.sh==sh) {track=t;break;}
        if((actions&2) && !p.right &&
           x>=t->x && x<=t->x+t->w && y>=t->y && y<t->y+t->h &&
           x>=FLOAT_AT(menu+0xe4) && x<FLOAT_AT(menu+0xec) &&
           y>=FLOAT_AT(menu+0xe8) && y<FLOAT_AT(menu+0xf0)) {
            menu_slider_drag=t->item;menu_slider_owner=*t;track=t;break;
        }
    }
    if(!track) {menu_slider_drag=0;return 0;}
    /* Capture lasts only while held in this active menu. Moving over labels,
     * the other slider or footer cannot retarget it or synthesize Confirm. */
    menu_mouse.mouse_navigation=1;
    menu_mouse.wheel_pending=0;menu_mouse.scrolling=0;
    WORD_AT(menu+0xc4)&=~0x001F001Fu;
    WORD_AT(menu+0xc8)&=~0x1Fu;
    if(WORD_AT(menu+0x198)!=track->item) guest_call(sub_00076EF0,2,menu,track->item);
    if((actions&2) && (track-menu_slider_tracks==MENU_FOV || track-menu_slider_tracks==MENU_WEAPON_FOV))
        menu_video_click_sound(menu,track->item);
    menu_slider_set(track,(unsigned)(track-menu_slider_tracks),x);
    return 1;
}
