/* US 45410066: imported music in MapMaker's and Arcade's music choices
 * (docs/mapmaker/mapmaker-custom-music.md; host side pc_custom_music.h).
 * Included by pc_mapmaker_skymusic.inc, whose rows/compile/load/music hooks
 * call the cmu_* functions below; three wrappers of its own (area "music").
 *
 * Stock facts (CONFIRMED in the disassembly, docs table):
 *  - sub_00045DE0(eax = music id, [esp+4] tileset/level id, [esp+8]/[esp+0xC]
 *    the two jingles) starts the level music: channel 0 gets eax, 1 and 2
 *    the jingles (sub_00045D80 -> sub_00028890(channel, id); id 0 = silence,
 *    [0x0064BD40 + 4 ch] = -1). Callers: MapMaker play 0x00045FB7 (return
 *    0x00045FBC; eax is the profile's Arcade choice unless that entry is -1,
 *    then the map's own index, sub_00045E60) and stock Arcade 0x00045F46
 *    (return 0x00045F4B; eax = jukebox[[0x007613D0]]).
 *  - sub_000289C0(channel) plays a channel: [0x004B9134] = channel; the end of
 *    a match plays a jingle channel (1 or 2) this way.
 *  - Player object 0x0064BD98: +0x28 pause state (sub_00029EA0 sets 1,
 *    sub_00029EE0 clears). Music slider [0x005236BC] 0..16 (default 11):
 *    sub_00045B60 = slider/16 x 100 x track level (0x00535858: 60..70) / 100.
 *  - Arcade setup menu sub_002C78F0: item 0xE (text 0x1831) gets the 58
 *    jukebox rows with sub_000717D0(text) (values = row numbers 0..57) in a
 *    loop that runs only when ebx is 0; the next call is sub_00071BE0
 *    (return 0x002C8047). sub_002C71C0(menu, profile) shows [0x007613D4] in
 *    item 0xE (+0xC8); sub_002C7430(menu, flag, profile) stores item 0xE in
 *    [0x007613D4], [0x007613D0] and profile +0x114 (row 0 = level music).
 *
 * Design:
 *  - MapMaker (TSFP_MAPMAKER_LIMITS=1, scene 102): after the 28 extra stock
 *    rows, one row per present track (value 0x100 + list index, label
 *    "Imported: name"). The rebuild leaves the stock fallback -1 (the
 *    tileset's music; pc_mapmaker_skymusic.inc writes it for any value past
 *    57) and appends chunk 0x00C776F7 {word 1, byte length, byte 0, 64-byte
 *    file name} before the terminator. A stock build skips the unknown chunk
 *    by its size and plays the tileset music, so the map is NOT moved to the
 *    64 KB tier (pc_mapmaker_container.inc). The editor load turns the name
 *    back into the row; a missing file leaves the tileset music.
 *  - Arcade: rows "Imported: name" after the 58 stock rows (values 58..),
 *    each mapped to its list index. A store of such a row keeps the name in
 *    pc-music.ini and writes row 0 (level music) to the three stock fields,
 *    so the profile, System Link and a stock build never see it; the menu's
 *    show step moves item 0xE back to the imported row.
 *  - Play: at sub_00045DE0 any imported track stops. From the MapMaker or
 *    Arcade caller, the Arcade choice (only when the guest's choice is row 0)
 *    or else the map's chunk (only when the map's index is -1) names the
 *    track; if the file is present and the game's music is on (sub_00045A70
 *    returns 0) the host player starts and eax becomes 0, so channel 0 is
 *    silent and the jingles stay stock. The player ends the track when the
 *    scene changes or a jingle channel starts (WRAP sub_000289C0), follows
 *    the slider and pauses with the game's music.
 * Requires the includer's WORD_AT, guest_call, g_eax, g_ebx, g_esp,
 * g_xbox_mem_offset, xbox_HeapAlloc, MAPMAKER_PTR_OK, mapmaker_limits_read. */
#define PC_CUSTOM_MUSIC_IMPLEMENTATION 1
#include "pc_custom_music.h"

#define CMU_TAG 0x00C776F7u                 /* fork-only; no game code uses it */
#define CMU_END 0x00C7766Au
#define CMU_MAGIC 0x00C77667u
#define CMU_VERSION 0x12Du
#define CMU_INFO 0x00C77668u
#define CMU_CHUNK 0x44u                     /* +0 word 1, +2 length, +3 0, +4 name[64] */
#define CMU_SCAN 0x7FFE0u                   /* container body (pc_mapmaker_container.inc) */
#define CMU_SAVE_COMPILE_RET 0x0032C6FFu
#define CMU_VALUE 0x100u                    /* editor music value of list entry k: 0x100 + k */
#define CMU_SCENE 0x0079094Cu
#define CMU_EDITOR 0x007844A8u
#define CMU_CONTAINER 0x00784014u
#define CMU_ED_MUSIC 0x210u
#define CMU_RET_MAP 0x00045FBCu             /* sub_00045EB0 0x00045FB7 -> sub_00045DE0 */
#define CMU_RET_ARCADE 0x00045F4Bu          /* sub_00045EB0 0x00045F46 -> sub_00045DE0 */
#define CMU_RET_ARCADE_ROWS 0x002C8047u     /* sub_002C78F0 0x002C8042 -> sub_00071BE0 */
#define CMU_ARCADE_ITEM 0x0Eu
#define CMU_ITEM_VALUE 0xC8u
#define CMU_PROFILE_MUSIC 0x114u
#define CMU_ARCADE_FIRST 58u                /* rows 0..57 are the jukebox list */
#define CMU_ARCADE_INDEX 0x007613D0u
#define CMU_ARCADE_SHOWN 0x007613D4u
#define CMU_SETUP_LINK 0x00761404u          /* sub_00045E60: profile choice used when set ... */
#define CMU_MODE 0x007DE455u                /* ... and this byte is not 10 */
#define CMU_SLIDER 0x005236BCu              /* music volume 0..16 */
#define CMU_SLIDER_MAX 16u
#define CMU_PAUSE 0x0064BDC0u               /* player 0x0064BD98 +0x28 */
#define CMU_LABEL_BYTES 56u
#define CMU_PREFIX "Imported: "

extern void sub_00071800(void),sub_000717D0(void),sub_00071940(void),sub_00069640(void),sub_00045A70(void);

static struct {
    PcMusicList list;
    int scanned, arcade_loaded;
    uint32_t labels;                        /* guest: CMU_LABEL_BYTES per list entry */
    unsigned offer[PC_MUSIC_MAX_TRACKS], offered;   /* Arcade rows 58.. -> list entry */
    char arcade[PC_MUSIC_NAME_MAX];         /* the Arcade choice, "" = stock */
    volatile uint32_t scene;                /* scene the track plays in */
    unsigned logged;
} cmu;

static void cmu_log(const char *what,const char *name,int a)
{
    if(cmu.logged++>=48u) return;
    fprintf(stderr,"[music] %s: %s (%d)\n",what,name?name:"",a);
    fflush(stderr);
}
static uint8_t *cmu_byte(uint32_t a) { return (uint8_t *)((uintptr_t)a+g_xbox_mem_offset); }
static uint32_t cmu_label(unsigned k) { return cmu.labels+k*CMU_LABEL_BYTES; }

/* Rescan the folder; entries keep their index, so stored values stay valid. */
static unsigned cmu_scan(void)
{
    unsigned k,n=pc_music_scan(&cmu.list);
    cmu.scanned=1;
    if(!cmu.list.count) return 0;            /* no folder or no tracks: nothing to label */
    if(!cmu.labels && !(cmu.labels=xbox_HeapAlloc(PC_MUSIC_MAX_TRACKS*CMU_LABEL_BYTES,4u))) {
        cmu_log("no memory for the row labels","",0);
        return 0;
    }
    for(k=0;k<cmu.list.count;k++) {
        char text[PC_MUSIC_LABEL_MAX];
        pc_music_label(cmu.list.name[k],text,sizeof text);
        snprintf((char *)cmu_byte(cmu_label(k)),CMU_LABEL_BYTES,"%s%s",CMU_PREFIX,text);
    }
    return n;
}
static void cmu_arcade_read(void)
{
    if(cmu.arcade_loaded) return;
    cmu.arcade_loaded=1;
    if(!pc_music_arcade_load(cmu.arcade,sizeof cmu.arcade)) cmu.arcade[0]=0;
}

/* ---- the map chunk ----------------------------------------------------- */
/* The terminator of a container, or 0; *chunk gets the first CMU_TAG data,
 * *info the map-info data. */
static uint32_t cmu_walk(uint32_t c,uint32_t *chunk,uint32_t *info)
{
    uint32_t p,end;
    unsigned steps;
    if(chunk) *chunk=0;
    if(info) *info=0;
    if(!c || !MAPMAKER_PTR_OK(c,CMU_SCAN+8u) || WORD_AT(c)!=CMU_MAGIC || WORD_AT(c+4u)!=CMU_VERSION) return 0;
    end=c+CMU_SCAN;
    for(p=c+8u,steps=0;steps<0x40000u && end-p>=8u;steps++) {
        uint32_t t=WORD_AT(p),size=WORD_AT(p+4u);
        if(t==CMU_END) return p;
        if(size>end-p-8u) return 0;
        if(t==CMU_TAG && chunk && !*chunk && size>=CMU_CHUNK) *chunk=p+8u;
        if(t==CMU_INFO && info && !*info && size>=0x24Cu) *info=p+8u;
        p+=8u+size;
    }
    return 0;
}
/* The track a container names, or 0. */
static int cmu_chunk_name(uint32_t c,char *out)
{
    uint32_t data;
    unsigned n;
    if(!cmu_walk(c,&data,NULL) || !data) return 0;
    n=cmu_byte(data+2u)[0];
    if(cmu_byte(data)[0]!=1u || cmu_byte(data)[1]!=0 || !n || n>=PC_MUSIC_NAME_MAX) return 0;
    memcpy(out,cmu_byte(data+4u),n);
    out[n]=0;
    return pc_music_name_ok(out);
}

/* Map Settings' music list (scene 102, switch on), after the extra rows. */
static void cmu_rows(void)
{
    unsigned k,n=0;
    cmu_scan();                              /* files added since the last list show up */
    if(!cmu.labels) return;
    for(k=0;k<cmu.list.count;k++)
        if(cmu.list.present[k]) { guest_call(sub_00071800,2,cmu_label(k),CMU_VALUE+k);n++; }
    if(n) cmu_log("imported tracks offered in Map Settings","",(int)n);
}

/* After the rebuild (sub_003006C0): append the chunk when the editor's music
 * is an imported row. ret = the rebuild's return address. */
static void cmu_compile(uint32_t c,uint32_t ret)
{
    uint32_t ed=WORD_AT(CMU_EDITOR),m,term,info,used,data,nterm;
    const char *name;
    size_t n;
    if(!MAPMAKER_PTR_OK(ed,CMU_ED_MUSIC+4u)) return;
    m=WORD_AT(ed+CMU_ED_MUSIC);
    if(m<CMU_VALUE || m-CMU_VALUE>=cmu.list.count) return;
    name=cmu.list.name[m-CMU_VALUE];
    if(!(term=cmu_walk(c,NULL,&info)) || !info) return;
    used=WORD_AT(info+0x248u);
    if(term!=c+used-8u || used+8u+CMU_CHUNK+4u>CMU_SCAN) { cmu_log("no room for the imported track; not saved",name,(int)used);return; }
    n=strlen(name);
    WORD_AT(term)=CMU_TAG;WORD_AT(term+4u)=CMU_CHUNK;data=term+8u;
    memset(cmu_byte(data),0,CMU_CHUNK);
    cmu_byte(data)[0]=1u;cmu_byte(data+2u)[0]=(uint8_t)n;
    memcpy(cmu_byte(data+4u),name,n);
    nterm=data+CMU_CHUNK;
    WORD_AT(nterm)=CMU_END;WORD_AT(nterm+4u)=nterm-c;
    used=nterm-c+8u;
    WORD_AT(info+0x248u)=used;
    if(g_eax) {
        g_eax=used;
        if(ret==CMU_SAVE_COMPILE_RET && !(g_eax&0xFFFFu)) g_eax|=4u;   /* as the container hook */
    }
}

/* After the editor load (sub_00301A90, scene 102, switch on). */
static void cmu_load(uint32_t c)
{
    uint32_t ed=WORD_AT(CMU_EDITOR);
    char name[PC_MUSIC_NAME_MAX];
    int k;
    if(!MAPMAKER_PTR_OK(ed,CMU_ED_MUSIC+4u) || !cmu_chunk_name(c,name)) return;
    cmu_scan();
    if((k=pc_music_find(&cmu.list,name))<0) { cmu_log("map's imported track is missing; tileset music",name,0);return; }
    WORD_AT(ed+CMU_ED_MUSIC)=CMU_VALUE+(uint32_t)k;
}

/* ---- play -------------------------------------------------------------- */
/* Player thread: guest reads only. */
static int cmu_watch(void *context,float *gain,int *paused)
{
    uint32_t slider;
    (void)context;
    if(WORD_AT(CMU_SCENE)!=cmu.scene) return 0;
    slider=WORD_AT(CMU_SLIDER);
    *gain=(float)(slider>CMU_SLIDER_MAX?CMU_SLIDER_MAX:slider)/(float)CMU_SLIDER_MAX;
    *paused=WORD_AT(CMU_PAUSE)!=0;
    return 1;
}

/* WRAP sub_00045DE0, before pc_mapmaker_skymusic.inc's own rule; returns 1
 * when an imported track plays (eax is then 0). */
static int cmu_music(int limits)
{
    uint32_t ret=MAPMAKER_PTR_OK(g_esp,4u)?WORD_AT(g_esp):0u,scene=WORD_AT(CMU_SCENE);
    char map_name[PC_MUSIC_NAME_MAX];
    const char *name=NULL;
    int arcade;
    pc_music_stop();                         /* a new level music ends an imported track */
    if(ret!=CMU_RET_MAP && ret!=CMU_RET_ARCADE) return 0;
    arcade=ret==CMU_RET_ARCADE ? WORD_AT(CMU_ARCADE_INDEX)==0u :
           WORD_AT(CMU_SETUP_LINK) && cmu_byte(CMU_MODE)[0]!=0x0Au &&
           WORD_AT(guest_call(sub_00069640,0)+CMU_PROFILE_MUSIC)==0u;
    if(arcade) { cmu_arcade_read();if(cmu.arcade[0]) name=cmu.arcade; }
    if(!name && ret==CMU_RET_MAP && limits && scene==100u && g_eax==0xFFFFFFFFu &&
       cmu_chunk_name(WORD_AT(CMU_CONTAINER),map_name)) name=map_name;
    if(!name) return 0;
    cmu_scan();
    if(pc_music_find(&cmu.list,name)<0) { cmu_log("imported track is missing; stock music",name,0);return 0; }
    if(guest_call(sub_00045A70,0)) { cmu_log("music is switched off",name,0);return 0; }
    cmu.scene=scene;
    if(!pc_music_start(name,cmu_watch,NULL)) { cmu_log("imported track did not start (not a playable WAV?)",name,0);return 0; }
    cmu_log(arcade && name==cmu.arcade?"Arcade: imported track":"map: imported track",name,(int)scene);
    g_eax=0;
    return 1;
}

/* WRAP sub_000289C0 (play music channel [esp+4]): a jingle channel ends the
 * imported track, as it replaces the stock level music. */
void pc_custom_music_channel(void (*original)(void))
{
    if(MAPMAKER_PTR_OK(g_esp,8u) && WORD_AT(g_esp+4u)!=0u && pc_music_playing()) {
        pc_music_stop();
        cmu_log("jingle channel: imported track ended","",(int)WORD_AT(g_esp+4u));
    }
    original();
}

/* WRAP sub_00071BE0: after the Arcade music rows (return 0x002C8047, the
 * row loop ran: ebx 0), add the imported rows. */
void pc_custom_music_arcade_rows(void (*original)(void))
{
    unsigned k;
    if(MAPMAKER_PTR_OK(g_esp,4u) && WORD_AT(g_esp)==CMU_RET_ARCADE_ROWS && g_ebx==0u) {
        cmu_scan();
        cmu.offered=0;
        if(cmu.labels)
            for(k=0;k<cmu.list.count;k++)
                if(cmu.list.present[k]) { guest_call(sub_000717D0,1,cmu_label(k));cmu.offer[cmu.offered++]=k; }
    }
    original();
}

/* WRAP sub_002C71C0 (menu, profile): show the imported Arcade choice. */
void pc_custom_music_arcade_show(void (*original)(void))
{
    uint32_t menu=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp+4u):0u,item;
    unsigned p;
    int k;
    original();
    cmu_arcade_read();
    if(!menu || !cmu.offered || !cmu.arcade[0] || (k=pc_music_find(&cmu.list,cmu.arcade))<0) return;
    for(p=0;p<cmu.offered && cmu.offer[p]!=(unsigned)k;p++) {}
    if(p==cmu.offered || !(item=guest_call(sub_00071940,2,menu,CMU_ARCADE_ITEM)) || !MAPMAKER_PTR_OK(item,CMU_ITEM_VALUE+4u)) return;
    WORD_AT(item+CMU_ITEM_VALUE)=CMU_ARCADE_FIRST+p;
}

/* WRAP sub_002C7430 (menu, flag, profile): keep an imported row host-side. */
void pc_custom_music_arcade_store(void (*original)(void))
{
    uint32_t menu=0,profile=0,item,v;
    if(MAPMAKER_PTR_OK(g_esp,16u)) { menu=WORD_AT(g_esp+4u);profile=WORD_AT(g_esp+12u); }
    original();
    if(!menu || !(item=guest_call(sub_00071940,2,menu,CMU_ARCADE_ITEM)) || !MAPMAKER_PTR_OK(item,CMU_ITEM_VALUE+4u)) return;
    cmu_arcade_read();
    v=WORD_AT(item+CMU_ITEM_VALUE);
    if(v>=CMU_ARCADE_FIRST && v-CMU_ARCADE_FIRST<cmu.offered) {
        const char *name=cmu.list.name[cmu.offer[v-CMU_ARCADE_FIRST]];
        WORD_AT(CMU_ARCADE_SHOWN)=0;WORD_AT(CMU_ARCADE_INDEX)=0;
        if(MAPMAKER_PTR_OK(profile,CMU_PROFILE_MUSIC+4u)) WORD_AT(profile+CMU_PROFILE_MUSIC)=0;
        if(strcmp(cmu.arcade,name)) {
            strcpy(cmu.arcade,name);
            if(!pc_music_arcade_save(name)) cmu_log("Arcade choice not saved",name,0);
            cmu_log("Arcade: imported track chosen",name,(int)v);
        }
    } else if(cmu.arcade[0]) {
        cmu.arcade[0]=0;
        if(!pc_music_arcade_save("")) cmu_log("Arcade choice not cleared","",0);
    }
}
