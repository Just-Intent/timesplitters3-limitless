/* US 45410066 MapMaker: the game's other skies, weather and music in the Map
 * Settings lists (docs/mapmaker/mapmaker-sky-music.md has the inventory and
 * the evidence for every address below).
 *
 * Stock facts (CONFIRMED in the XBE and the game paks):
 *  - Map Settings (sub_003133E0) builds three list rows with sub_00071800
 *    (text, value) and binds each with sub_00078590(&editor+field): music
 *    (+0x210, values -1, 0, 1..57 of the jukebox list 0x00537210 {music id,
 *    text}; bind returns to 0x00313768), sky (+0x220, -1 and records 58..67
 *    from 0x0052D150; 0x00313843), weather (+0x224, 0..3 from 0x0052D1A8;
 *    0x00313906). Only the rebuild sub_003006C0 reads the three fields
 *    (0x00300784/0x00300795/0x003007A5 -> info +0x0E word, +0x10 word, +0x12
 *    byte); the loader sub_00301A90 writes them back (0x00301C5D..0x00301C7C).
 *  - Music ids 1..0xC9 name files in pak/musicts.pak (0x00535530), which
 *    every level mounts; the list offers 56 distinct files, 28 others (menu,
 *    intro, jingles, Far Future boss, 20 cutscene tracks) are never offered.
 *    Scene 100 plays sub_00045DE0(eax = id, [esp+4] = tileset music) from
 *    0x00045FB7; eax is -1 only when the map's own index is -1 (a game setup
 *    override is used only when its entry is not -1, 0x00045E93).
 *  - Sky records 0..67 (0x005149A8, model at +4); MapMaker play mounts only
 *    its tileset pak, l_100.pak, gun.pak, chr.pak (sub_00062790), so the 17
 *    story/arcade skies whose model is in a level pak cannot be drawn there.
 *    Their textures (and the sandstorm's) are in no level pak at all: the
 *    load pass registers a read model's textures (sub_00096220, 0x0006355D;
 *    sky files' texture block +0x48 is 0x10, so never deferred to creation),
 *    which queues "textures/%06x.xbt" (sub_000637E0); the texture pass
 *    (sub_00063900, 0x0003D5E5) loads each from any mounted pak
 *    (sub_00018B90). 14 skies' and the sandstorm's are only in the level's
 *    memory pak (arcademem/l_20_arm.pak, ...), which story levels mount whole
 *    (sub_00062610(scene, 1) -> sub_000622D0(path, 1, 1), 0x000628B1).
 *  - Weather type 5 (sandstorm, story scenes 2/26/64): sub_00119960 sets
 *    [0x0073D9AC] and requests the four intro_sandstorm models (0x004E62F8,
 *    stride 0x44); sub_0011B430 allocates its buffers, sub_0011B310 creates
 *    its props and resets the weather list [0x007AC804] to 0x004E63F8 (type
 *    0); the frame code draws it only while the current entry has type 5
 *    (0x0011BC4F, 0x0011BCB5). Scene 2's list 0x00507898 has one type-5 entry.
 *  - The sandstorm is tied to scene 2's rooms: the four emitters 0x004E62E8
 *    (0x44 each: rooms +0..+0xC, model +0x10, rate +0x14, wind +0x18, prop
 *    +0x34) hold rooms {2,3,7} {6,26,27} {5,4} {19}. Only a player whose room
 *    list matches one (0x0011AD69, 0x0011A66B) spawns sand grains (sub_001186F0:
 *    on that room's floor zones) and animates/fades in the emitter's prop
 *    (created at the origin, 0x00118A0C). The grains (60 x 0x24 ring
 *    [0x007AC874], index [0x007AC878]) age and move in sub_0011ACF0 and are
 *    drawn by sub_0011B110 (texture 0x303) in each view whose flag
 *    [0x007AC82C+4v] the haze update (sub_0011A600) set: first the 8 haze
 *    layers (sub_0011A930, texture 0x328), then one sprite per grain
 *    (sub_0001D5F0: position, colour sub_00154FE0(0x7F643C10, fade), size
 *    age/3 + 1.5, angle). Scenes 2/26/64 use fog row 7 (level +0x14).
 *
 * With TSFP_MAPMAKER_LIMITS=1:
 *  - rows (WRAP sub_00078590, the three bind return addresses, scene 102):
 *    before the bind, add 28 music rows (values 58..85), 17 sky rows (values
 *    = the sky records, table in pc_mapmaker_sky.inc) and a Sandstorm row
 *    (value 4); labels live in one xbox_HeapAlloc block.
 *  - compile (WRAP sub_003006C0, after the original): a stock build must
 *    never see these values, so the rebuilt info gets the stock fallback
 *    (music -1, the sky's MapMaker twin, weather 0) and the row number in info
 *    flag bits 24..28 (music), 19..23 (sky), 29..30 (weather). The fallback is
 *    written even with the switch off (an out-of-range field is never saved).
 *  - load (WRAP sub_00301A90, after the original, scene 102): the bits turn
 *    the editor fields back into the extra values.
 *  - play (scene 100): mount (WRAP sub_00062790) only starts a new level
 *    (there the scene may still be the old one and the map is not read;
 *    v0.4.0-mapmaker.25 mounted there and no extra sky or sandstorm showed).
 *    The sky's / sandstorm's level pak and then its textures' memory pak are
 *    mounted index-only like chr.pak (sub_000622D0(path, 0, 1); both or the
 *    stock choice is kept), at most four, keeping 8 of the 64 mount
 *    slots free, by the first of: the cloud setup (pc_mapmaker_sky.inc via
 *    mapmaker_sky.extra_prepare), the map start, the weather load. level
 *    (WRAP sub_0023F390) gives the map start the extra sky record in info
 *    +0x10 while it runs (it sets the level sky and requests the model
 *    before the load pass); sky_built (WRAP sub_0011D2C0, probe) logs what
 *    each view's sky prop was built from and the textures not loaded; weather_load (WRAP sub_00119960,
 *    after) turns the sandstorm on and requests its models; weather_start
 *    (WRAP sub_0011B310, after) and weather_list (WRAP sub_00119AB0, the
 *    level setup's list, which runs after the weather start) keep scene 2's
 *    type-5 entry once the sandstorm buffers exist; weather_frame (WRAP
 *    sub_0011BBC0) hides scene 2's emitter rooms while the stock frame runs
 *    (MapMaker rooms are piece numbers: a stray match would fade in scene 2's
 *    props at the origin) and then blows grains around each outdoor player
 *    into the stock ring, which the stock code ages, moves and draws; the
 *    stock draw is nearly transparent (grain alpha 16/255, haze quads 2/255),
 *    so haze (WRAP sub_0011A930) and grain_colour (WRAP sub_00154FE0) raise
 *    the alpha of the sandstorm's own two calls (return-address tests), and
 *    the map start (level, above) gives the map scene 2's fog row 7;
 *    music (WRAP sub_00045DE0)
 *    plays the extra track when the map's index is -1.
 * A stock or older build loads such a map with the fallbacks. Requires the
 * includer's WORD_AT, guest_call, g_eax, g_esp, g_xbox_mem_offset,
 * xbox_HeapAlloc, mapmaker_limits/mapmaker_limits_read, MAPMAKER_PTR_OK and
 * pc_mapmaker_sky.inc (mapmaker_sky_extras, mapmaker_sky_info). */
#include <stdio.h>
#include <string.h>

#define SMX_SCENE 0x0079094Cu
#define SMX_EDITOR 0x007844A8u
#define SMX_CONTAINER 0x00784014u
#define SMX_ED_MUSIC 0x210u
#define SMX_ED_SKY 0x220u
#define SMX_ED_WEATHER 0x224u
#define SMX_INFO_MUSIC 0x0Eu                 /* word: jukebox index, -1 tileset */
#define SMX_INFO_SKY 0x10u                   /* word: sky record, -1 tileset */
#define SMX_INFO_WEATHER 0x12u               /* byte: 0..3 */
#define SMX_INFO_FLAGS 0x244u
#define SMX_MUSIC_SHIFT 24u
#define SMX_MUSIC_MASK 0x1F000000u
#define SMX_WEATHER_SHIFT 29u
#define SMX_WEATHER_MASK 0x60000000u
#define SMX_MASK (MAPMAKER_SKY_EXTRA_MASK|SMX_MUSIC_MASK|SMX_WEATHER_MASK)
#define SMX_MUSIC_STOCK_LAST 57               /* jukebox list 0..57 */
#define SMX_MUSIC_FIRST 58                    /* editor value of the first extra track */
#define SMX_WEATHER_STOCK_LAST 3u
#define SMX_WEATHER_FIRST 4u
#define SMX_RET_MUSIC_BIND 0x00313768u       /* sub_003133E0 -> sub_00078590(&editor+0x210) */
#define SMX_RET_SKY_BIND 0x00313843u         /* ... sub_00078590(&editor+0x220) */
#define SMX_RET_WEATHER_BIND 0x00313906u     /* ... sub_00078590(&editor+0x224) */
#define SMX_RET_MUSIC_PLAY 0x00045FBCu       /* sub_00045EB0 -> sub_00045DE0 (scene 100 path) */
#define SMX_MOUNTS 0x006D7880u               /* mounted paks (count) */
#define SMX_MOUNT_MAX 64u                    /* paktable.json: 64 slots at 0x00907000 */
#define SMX_MOUNT_KEEP 8u
#define SMX_MOUNT_SKIP 0x006D7C4Cu           /* sub_00062790 does nothing when set */
#define SMX_SKY_TABLE 0x005149A8u
#define SMX_RESMAP 0x008CA000u               /* model -> resource row (rooms area moved it) */
#define SMX_RESMAP_MODELS 0xA00u
#define SMX_RES 0x00791D4Cu                  /* resource table, 0x30 per row */
#define SMX_SANDSTORM 0x0073D9ACu
#define SMX_SAND_MODELS 0x004E62F8u          /* 4 x 0x44, model id at +0 */
#define SMX_WX_COUNT 0x007AC800u
#define SMX_WX_LIST 0x007AC804u
#define SMX_WX_INDEX 0x007AC808u
#define SMX_WX_BUF1 0x007AC824u              /* sandstorm buffers (sub_0011B430) */
#define SMX_WX_BUF2 0x007AC874u
#define SMX_WX_DEFAULT 0x004E63F8u
#define SMX_SAND_LIST 0x00507898u            /* scene 2: one entry, type 5 */
#define SMX_SAND_EMITTERS 0x004E62E8u        /* 4 x 0x44: rooms +0..+0xC (0 = none) */
#define SMX_SAND_EMITTER_COUNT 4u
#define SMX_SAND_ROOM_SLOTS 4u
#define SMX_SAND_PROP 0x34u                  /* emitter: the prop sub_001189C0 created */
#define SMX_WX_DRAWN 0x007AC82Cu             /* per view: sub_0011B110 draws when set */
#define SMX_WX_EXPOSED 0x007AC844u           /* per player float: 1 outdoors (sub_00119B50), decays */
#define SMX_WX_RING 0x007AC878u              /* next grain of the ring [SMX_WX_BUF2] */
#define SMX_GRAINS 60u                       /* 0x870 / 0x24 */
#define SMX_GRAIN 0x24u                      /* pos +0, velocity +0xC, angle +0x18, spin +0x1C, age +0x20 (-1 dead) */
#define SMX_PLAYERS_N 0x007B0CE0u
#define SMX_PLAYERS 0x007B0CE4u
#define SMX_PLAYER 0x3000u                 /* view stride (rooms.json: 0xFFC -> 0x3000) */
#define SMX_PL_FLAGS 0x21Cu                  /* weather runs for sign bit + 0x400 (0x0011A68F) */
#define SMX_PL_POS 0x27Cu
#define SMX_PLAYERS_MAX 4u
#define SMX_STEPS 0x004E7944u                /* frames this update (rate x steps, 0x0011ADC2) */
#define SMX_TEX_GLOBAL 0x004D2D64u           /* sub_00080F20: handle of global texture id at + id*0x18 */
#define SMX_TEX_GRAIN 0x303u
#define SMX_TEX_HAZE 0x328u
#define SMX_GRAIN_SPREAD 5.0f                /* metres around the player (.31: 12, too sparse to see) */
#define SMX_GRAIN_UPWIND 4.0f
#define SMX_GRAIN_EVERY 2u                   /* update steps per grain: 30/s, a 2 s life in the 60 ring */
#define SMX_RET_HAZE 0x0011B143u             /* sub_0011B110 -> sub_0011A930(eax 0x7F5A1E05, 8 layers) */
#define SMX_RET_GRAIN_COLOUR 0x0011B247u     /* sub_0011B110 -> sub_00154FE0(0x7F643C10, fade) per grain */
#define SMX_GRAIN_COLOUR 0x7F643C10u         /* sand R G B, alpha 0x10 (low byte) */
#define SMX_GRAIN_ALPHA 0x60u                /* MapMaker grain alpha (stock 0x10: 6% at the peak) */
#define SMX_HAZE_ALPHA 0x50u                 /* MapMaker haze weight (stock 5: quad alpha <= 2 of 255) */
#define SMX_LEVEL_FOG 0x14u                  /* level entry: fog row of 0x00515778 (0x3C each, sub_0006A710) */
#define SMX_SAND_FOG 7u                      /* scenes 2/26/64: colour 0x2F210F, 25..235.5 m */
static const float smx_wind[3]={0.53f,0.0f,-0.85f};   /* scene 2 emitter 1 (+0x18) */
#define SMX_PATH_MAX 32u
#define SMX_SCAN 0x00080000u
#define SMX_LEVEL100 0x00518778u             /* level table entry of scene 100 (+0x10 sky) */
#define SMX_STATE_FLAGS 0x007DE398u          /* 0x800000: sub_00062790 mounts for ... */
#define SMX_PENDING_SCENE 0x007DE394u        /* ... this scene byte */

static const struct { uint32_t id; const char *label; } smx_music[]={
    {0x18u,"Front End Menu"},{0x01u,"Intro"},{0x2Bu,"Game Over"},{0x33u,"Mission Complete"},
    {0xC1u,"Challenge Win"},{0xC2u,"Challenge: Far Future"},{0x49u,"Boss: Far Future"},
    {0x4Au,"Boss: Far Future (Faster)"},{0x4Bu,"Cutscene: Prologue"},{0x50u,"Cutscene: Marine Base"},
    {0x55u,"Cutscene 3"},{0x5Au,"Cutscene: Cavern"},{0x5Fu,"Cutscene: Depot"},{0x64u,"Cutscene 6"},
    {0x69u,"Cutscene: Train"},{0x6Eu,"Cutscene 8"},{0x73u,"Cutscene: Mansion"},
    {0x78u,"Cutscene: Two Crows"},{0x7Du,"Cutscene: Rooftop"},{0x82u,"Cutscene: Lab Intro"},
    {0x87u,"Cutscene: Lift Exit"},{0x8Cu,"Cutscene: Cortez"},{0x91u,"Cutscene: Meet R-110"},
    {0x96u,"Cutscene 16"},{0x9Bu,"Cutscene 18"},{0xA0u,"Cutscene: Sub Cavern"},
    {0xA5u,"Cutscene 19a"},{0xAAu,"Cutscene: Anya's Lab"},
};
#define SMX_MUSIC_COUNT ((uint32_t)(sizeof smx_music/sizeof smx_music[0]))
static const struct { const char *label, *pak, *mem; uint32_t list; } smx_weather[]={
    {"Sandstorm","pak/challenge/l_64_ch.pak","pak/challengemem/l_64_chm.pak",SMX_SAND_LIST},
};
#define SMX_WEATHER_COUNT ((uint32_t)(sizeof smx_weather/sizeof smx_weather[0]))
#define SMX_PATHS (2u*(MAPMAKER_SKY_EXTRA_COUNT+SMX_WEATHER_COUNT))   /* level and texture paks */
#define SMX_TEXTURES 0x004B85C8u             /* loaded textures: +0 rows (0x34, id +0x1C), +0xC end (sub_00018B90) */
#define SMX_TEX_STRIDE 0x34u
#define SMX_TEX_ID 0x1Cu
#define SMX_TEX_ROWS_MAX 0x2000u
#define SMX_TEX_LIST_MAX 64u                 /* model file: +0 -> {slot, id, -, flags} x n, -1 */
#define SMX_TEX_LOGGED 8u

extern void sub_00071800(void),sub_00062260(void),sub_000622D0(void),sub_000A5E40(void),sub_000631C0(void);

static struct {
    uint32_t labels;                         /* guest: every row label */
    uint32_t music_text[SMX_MUSIC_COUNT], sky_text[MAPMAKER_SKY_EXTRA_COUNT], weather_text[SMX_WEATHER_COUNT];
    uint32_t paths;                          /* guest: pak paths, SMX_PATH_MAX each */
    const char *path_of[SMX_PATHS];
    uint32_t sky_row;                        /* this level: extra sky row whose pak was tried */
    uint32_t level_logs;                     /* this level: sky probe lines */
    uint32_t frames;                         /* this level: weather frames seen (probe) */
    uint32_t tex_total, tex_loaded;          /* last sky probe: textures named, loaded */
    int sand_ready;                          /* this level: the sandstorm pak is mounted */
    int sand_on;                             /* this level: sandstorm flag set by us */
    uint32_t grains;                         /* this level: grains emitted */
    uint32_t sand_steps;                     /* update steps not yet turned into grains */
    uint32_t haze_boosts, grain_boosts;      /* this level: sandstorm draws made more opaque */
    uint32_t tileset_fog;                    /* this level: the fog row the map start chose */
    uint32_t rng;                            /* host generator: the guest's stays untouched */
    unsigned long long rows, logged;
} smx;

static int smx_on(void)
{
    mapmaker_limits_read();
    return mapmaker_limits.limits;
}
static uint32_t smx_scene(void) { return WORD_AT(SMX_SCENE); }
static void smx_log(const char *what,uint32_t a,uint32_t b)
{
    if(smx.logged++>=96u) return;
    fprintf(stderr,"[mapmaker] skymusic: %s (%u, %u)\n",what,a,b);
    fflush(stderr);
}
static void smx_logs(const char *what,const char *pak,uint32_t a)
{
    if(smx.logged++>=96u) return;
    fprintf(stderr,"[mapmaker] skymusic: %s: %s (%u)\n",what,pak,a);
    fflush(stderr);
}
static uint8_t *smx_byte(uint32_t a) { return (uint8_t *)((uintptr_t)a+g_xbox_mem_offset); }
static void smx_set16(uint32_t a,uint32_t v) { smx_byte(a)[0]=(uint8_t)v;smx_byte(a)[1]=(uint8_t)(v>>8); }
static int smx_get16(uint32_t a) { return (int16_t)(smx_byte(a)[0]|smx_byte(a)[1]<<8); }

/* Rows of the three lists: 1..count, or 0. */
static uint32_t smx_bits(uint32_t flags,uint32_t mask,uint32_t shift,uint32_t count)
{
    uint32_t k=(flags&mask)>>shift;
    return k<=count?k:0u;
}
static uint32_t smx_sky_row(int sky)
{
    uint32_t k;
    for(k=0;k<MAPMAKER_SKY_EXTRA_COUNT;k++) if(mapmaker_sky_extras[k].sky==sky) return k+1u;
    return 0;
}

/* Every label copied once into guest memory. */
static int smx_label_storage(void)
{
    uint32_t k,n=0,p;
    if(smx.labels) return 1;
    for(k=0;k<SMX_MUSIC_COUNT;k++) n+=(uint32_t)strlen(smx_music[k].label)+1u;
    for(k=0;k<MAPMAKER_SKY_EXTRA_COUNT;k++) n+=(uint32_t)strlen(mapmaker_sky_extras[k].label)+1u;
    for(k=0;k<SMX_WEATHER_COUNT;k++) n+=(uint32_t)strlen(smx_weather[k].label)+1u;
    if(!(p=xbox_HeapAlloc(n,4u))) { smx_log("no memory for the row labels",n,0);return 0; }
    smx.labels=p;
#define SMX_COPY(dst,text) do { size_t l_=strlen(text)+1u;memcpy(smx_byte(p),(text),l_);(dst)=p;p+=(uint32_t)l_; } while(0)
    for(k=0;k<SMX_MUSIC_COUNT;k++) SMX_COPY(smx.music_text[k],smx_music[k].label);
    for(k=0;k<MAPMAKER_SKY_EXTRA_COUNT;k++) SMX_COPY(smx.sky_text[k],mapmaker_sky_extras[k].label);
    for(k=0;k<SMX_WEATHER_COUNT;k++) SMX_COPY(smx.weather_text[k],smx_weather[k].label);
#undef SMX_COPY
    return 1;
}

/* WRAP sub_00078590 (menu: bind the last list item to a dword). Before the
 * stock bind of the music, sky and weather lists, append the extra rows so
 * the bind finds a stored extra value among them. */
void pc_mapmaker_skymusic_rows(void (*original)(void))
{
    uint32_t ret=MAPMAKER_PTR_OK(g_esp,4u)?WORD_AT(g_esp):0u,k;
    if((ret!=SMX_RET_MUSIC_BIND && ret!=SMX_RET_SKY_BIND && ret!=SMX_RET_WEATHER_BIND) ||
       !smx_on() || smx_scene()!=102u || !smx_label_storage()) { original();return; }
    if(ret==SMX_RET_MUSIC_BIND)
        for(k=0;k<SMX_MUSIC_COUNT;k++) guest_call(sub_00071800,2,smx.music_text[k],(uint32_t)SMX_MUSIC_FIRST+k);
    else if(ret==SMX_RET_SKY_BIND)
        for(k=0;k<MAPMAKER_SKY_EXTRA_COUNT;k++) guest_call(sub_00071800,2,smx.sky_text[k],(uint32_t)mapmaker_sky_extras[k].sky);
    else
        for(k=0;k<SMX_WEATHER_COUNT;k++) guest_call(sub_00071800,2,smx.weather_text[k],SMX_WEATHER_FIRST+k);
    if(!smx.rows++) smx_log("extra music, sky and weather rows offered",SMX_MUSIC_COUNT,MAPMAKER_SKY_EXTRA_COUNT);
    original();
}

/* WRAP sub_003006C0 (rebuild the container from the editor: container, mode).
 * After the original: stock fallbacks in the stock fields, rows in the flags. */
void pc_mapmaker_skymusic_compile(void (*original)(void))
{
    uint32_t container,ed,info,music=0,sky=0,weather=0,flags;
    int32_t m,s,w;
    if(!MAPMAKER_PTR_OK(g_esp,8u)) { original();return; }
    container=WORD_AT(g_esp+4u);
    original();
    ed=WORD_AT(SMX_EDITOR);
    if(!MAPMAKER_PTR_OK(ed,SMX_ED_WEATHER+4u) || !(info=mapmaker_sky_info(container))) return;
    m=(int32_t)WORD_AT(ed+SMX_ED_MUSIC);
    s=(int32_t)WORD_AT(ed+SMX_ED_SKY);
    w=(int32_t)WORD_AT(ed+SMX_ED_WEATHER);
    if(m>=SMX_MUSIC_FIRST && m<SMX_MUSIC_FIRST+(int32_t)SMX_MUSIC_COUNT) music=(uint32_t)(m-SMX_MUSIC_FIRST)+1u;
    if(music || m<-1 || m>SMX_MUSIC_STOCK_LAST) smx_set16(info+SMX_INFO_MUSIC,0xFFFFu);
    if((sky=smx_sky_row(s))!=0) smx_set16(info+SMX_INFO_SKY,(uint32_t)mapmaker_sky_extras[sky-1u].fallback);
    if(w>=(int32_t)SMX_WEATHER_FIRST && w<(int32_t)(SMX_WEATHER_FIRST+SMX_WEATHER_COUNT)) weather=(uint32_t)w-SMX_WEATHER_FIRST+1u;
    if(weather || w<0 || w>(int32_t)SMX_WEATHER_STOCK_LAST) *smx_byte(info+SMX_INFO_WEATHER)=0;
    flags=WORD_AT(info+SMX_INFO_FLAGS)&~SMX_MASK;
    if(smx_on())
        flags|=sky<<MAPMAKER_SKY_EXTRA_SHIFT|music<<SMX_MUSIC_SHIFT|weather<<SMX_WEATHER_SHIFT;
    WORD_AT(info+SMX_INFO_FLAGS)=flags;
}

/* WRAP sub_00301A90 (editor: load a container; cdecl container, mode). */
void pc_mapmaker_skymusic_load(void (*original)(void))
{
    uint32_t container,ed,info,flags,k;
    if(!MAPMAKER_PTR_OK(g_esp,8u)) { original();return; }
    container=WORD_AT(g_esp+4u);
    original();
    if(!smx_on() || smx_scene()!=102u) return;
    ed=WORD_AT(SMX_EDITOR);
    if(!MAPMAKER_PTR_OK(ed,SMX_ED_WEATHER+4u) || !(info=mapmaker_sky_info(container))) return;
    flags=WORD_AT(info+SMX_INFO_FLAGS);
    if((k=smx_bits(flags,SMX_MUSIC_MASK,SMX_MUSIC_SHIFT,SMX_MUSIC_COUNT))!=0) WORD_AT(ed+SMX_ED_MUSIC)=(uint32_t)SMX_MUSIC_FIRST+k-1u;
    if((k=smx_bits(flags,MAPMAKER_SKY_EXTRA_MASK,MAPMAKER_SKY_EXTRA_SHIFT,MAPMAKER_SKY_EXTRA_COUNT))!=0)
        WORD_AT(ed+SMX_ED_SKY)=(uint32_t)mapmaker_sky_extras[k-1u].sky;
    if((k=smx_bits(flags,SMX_WEATHER_MASK,SMX_WEATHER_SHIFT,SMX_WEATHER_COUNT))!=0) WORD_AT(ed+SMX_ED_WEATHER)=SMX_WEATHER_FIRST+k-1u;
}

/* The map's info in play, or 0 (switch off, other scene, no map). */
static uint32_t smx_play_info(void)
{
    if(smx_scene()!=100u || !smx_on()) return 0;
    return mapmaker_sky_info(WORD_AT(SMX_CONTAINER));
}

/* A pak path in guest memory (kept: the mount table may refer to it). */
static uint32_t smx_path(const char *pak)
{
    uint32_t k,n=SMX_PATHS;
    if(!smx.paths && !(smx.paths=xbox_HeapAlloc(n*SMX_PATH_MAX,4u))) return 0;
    for(k=0;k<n;k++) {
        if(smx.path_of[k] && !strcmp(smx.path_of[k],pak)) return smx.paths+k*SMX_PATH_MAX;
        if(!smx.path_of[k]) {
            if(strlen(pak)>=SMX_PATH_MAX) return 0;
            memcpy(smx_byte(smx.paths+k*SMX_PATH_MAX),pak,strlen(pak)+1u);
            smx.path_of[k]=pak;
            return smx.paths+k*SMX_PATH_MAX;
        }
    }
    return 0;
}
/* Mount a level pak index-only (entries read on demand, as chr.pak); 1 when mounted. */
static int smx_mount(const char *pak)
{
    uint32_t path=smx_path(pak),count;
    int32_t slot;
    if(!path) { smx_log("no memory for a pak path",0,0);return 0; }
    if((slot=(int32_t)guest_call(sub_00062260,1,path))!=-1) { smx_logs("pak already mounted",pak,(uint32_t)slot);return 1; }
    if((count=WORD_AT(SMX_MOUNTS))+SMX_MOUNT_KEEP>SMX_MOUNT_MAX) {
        smx_logs("pak mount table too full; not mounted",pak,count);
        return 0;
    }
    if((slot=(int32_t)guest_call(sub_000622D0,3,path,0u,1u))<0) { smx_logs("pak did not mount",pak,count);return 0; }
    smx_logs("mounted pak (slot)",pak,(uint32_t)slot);
    return 1;
}
/* A model's level pak and then its textures' pak (mem 0: none needed); 1
 * when both are mounted. Without the textures pak the model would be read
 * but its textures (textures/<id>.xbt) found in no pak: drawn not at all or
 * only in part (v0.4.0-mapmaker.27), so the caller keeps the stock choice. */
static int smx_mount_pair(const char *pak,const char *mem)
{
    if(!smx_mount(pak)) return 0;
    if(!mem || smx_mount(mem)) return 1;
    smx_logs("textures pak not mounted; the model's textures would be missing",mem,0);
    return 0;
}

/* 1 when scene 100's resource table has a row for the model (sub_000A5E40
 * requests only those). */
static int smx_model_ok(uint32_t model)
{
    uint32_t t=WORD_AT(SMX_RES);
    if(model>=SMX_RESMAP_MODELS || !t) return 0;
    return (int16_t)smx_get16(SMX_RESMAP+model*2u)>=0;
}
static uint32_t smx_sky_model(uint32_t row) { return WORD_AT(SMX_SKY_TABLE+(uint32_t)mapmaker_sky_extras[row-1u].sky*0x34u+4u); }

/* The map's extra sky row with its pak mounted now (mounting it if needed),
 * or 0. Called by the cloud setup (pc_mapmaker_sky.inc) and the map start;
 * mounts at most once per level. */
static uint32_t smx_prepare_sky(uint32_t info)
{
    uint32_t k=smx_bits(WORD_AT(info+SMX_INFO_FLAGS),MAPMAKER_SKY_EXTRA_MASK,MAPMAKER_SKY_EXTRA_SHIFT,MAPMAKER_SKY_EXTRA_COUNT);
    if(!k || smx_scene()!=100u || !smx_on()) return 0;
    if(smx.sky_row!=k) {
        smx.sky_row=k;
        smx_log("map has extra sky (row, record)",k,(uint32_t)mapmaker_sky_extras[k-1u].sky);
        mapmaker_sky.extra_ready=smx_mount_pair(mapmaker_sky_extras[k-1u].pak,mapmaker_sky_extras[k-1u].mem);
    }
    return mapmaker_sky.extra_ready?k:0u;
}

/* WRAP sub_00062790 (the level's pak mounts, no args): a new level. The
 * mounts are made later, where the scene is 100 and the map is read: here
 * the original may mount for a pending scene ([0x007DE394] while
 * [0x007DE398] has 0x800000) and [0x0079094C] can still be the old one. */
void pc_mapmaker_skymusic_mount(void (*original)(void))
{
    uint32_t pending=WORD_AT(SMX_STATE_FLAGS)&0x00800000u?(uint32_t)(int32_t)(int8_t)*smx_byte(SMX_PENDING_SCENE):smx_scene();
    if(WORD_AT(SMX_MOUNT_SKIP)) { original();return; }   /* the original does nothing */
    original();
    mapmaker_sky.extra_ready=0;mapmaker_sky.extra_prepare=smx_prepare_sky;
    smx.sky_row=0;smx.sand_ready=0;smx.sand_on=0;smx.level_logs=0;smx.frames=0;
    smx.tex_total=0;smx.tex_loaded=0;smx.grains=0;
    smx.sand_steps=SMX_GRAIN_EVERY-1u;smx.haze_boosts=0;smx.grain_boosts=0;smx.tileset_fog=0;
    if(pending==100u && smx_on()) smx_log("new MapMaker level (scene now, mounted paks)",smx_scene(),WORD_AT(SMX_MOUNTS));
}

/* The sandstorm may be drawn this frame: armed by us this level (TSFP_MAPMAKER_LIMITS
 * on, a sandstorm map), scene 100 and the stock flag still set. */
static int smx_sand_live(void) { return smx.sand_on==1 && smx_scene()==100u && WORD_AT(SMX_SANDSTORM); }

/* After the map start: a sandstorm map gets scene 2's fog row. The map start
 * writes the tileset's row (sub_00311870, 0x0023F40A) into scene 100's level
 * entry +0x14; the level start applies the row (sub_001CDE30 -> sub_0011D3C0
 * -> sub_0011D240: copy of 0x00515778 + row*0x3C to 0x007AC760, then
 * sub_0011CF00 -> sub_00022A20 colour, near, far), and so does a restart
 * (sub_001CDD60). The next map start writes the tileset's row again. */
static void smx_sand_fog(void)
{
    if(!smx_sand_live()) return;
    smx.tileset_fog=WORD_AT(SMX_LEVEL100+SMX_LEVEL_FOG);
    WORD_AT(SMX_LEVEL100+SMX_LEVEL_FOG)=SMX_SAND_FOG;
    smx_log("map start: sandstorm fog row (row, tileset's row)",SMX_SAND_FOG,smx.tileset_fog);
}

/* WRAP sub_0023F390 (map start: level sky, weather, fog from the info chunk;
 * no args). An extra sky is put in info +0x10 while it runs, so the stock
 * code sets the level's sky and requests its model before the load pass.
 * A sandstorm map then gets the story sandstorm's fog (smx_sand_fog). */
static void smx_level_sky(void (*original)(void))
{
    uint32_t info=smx_play_info(),k,model;
    int old;
    if(!info || !(k=smx_prepare_sky(info))) {
        if(info && smx_bits(WORD_AT(info+SMX_INFO_FLAGS),MAPMAKER_SKY_EXTRA_MASK,MAPMAKER_SKY_EXTRA_SHIFT,MAPMAKER_SKY_EXTRA_COUNT))
            smx_log("map start: extra sky pak not mounted; MapMaker sky kept",(uint32_t)smx_get16(info+SMX_INFO_SKY)&0xFFFFu,0);
        original();
        return;
    }
    model=smx_sky_model(k);
    if(!smx_model_ok(model)) {
        smx_log("map start: extra sky model has no resource row; MapMaker sky kept",model,(uint32_t)mapmaker_sky_extras[k-1u].sky);
        original();
        return;
    }
    old=smx_get16(info+SMX_INFO_SKY);
    fprintf(stderr,"[mapmaker] skymusic: map start: extra sky row %u resolved to record %d model 0x%X (resource row %d), map's stock sky %d, level sky before %u\n",
            k,mapmaker_sky_extras[k-1u].sky,model,smx_get16(SMX_RESMAP+model*2u),old,WORD_AT(SMX_LEVEL100+0x10u));
    smx_set16(info+SMX_INFO_SKY,(uint32_t)mapmaker_sky_extras[k-1u].sky);
    original();
    smx_set16(info+SMX_INFO_SKY,(uint32_t)old);
    smx_log("map start: level sky record after, model requested",WORD_AT(SMX_LEVEL100+0x10u),model);
}
void pc_mapmaker_skymusic_level(void (*original)(void))
{
    smx_level_sky(original);
    smx_sand_fog();
}

/* 1 when the texture id is in the loaded-texture table (sub_00018B90 looks
 * up and stores ids there, row +0x1C, and writes the row number, or -1, into
 * the model's entry +0 at 0x00018C91; a texture found in no mounted pak never
 * is). The entry's row is tried first, then the whole table. */
static int smx_texture_loaded(uint32_t slot,uint32_t id)
{
    uint32_t t=WORD_AT(SMX_TEXTURES),rows,end,k;
    if(!MAPMAKER_PTR_OK(t,0x10u) || !(rows=WORD_AT(t))) return 0;
    end=WORD_AT(t+0x0Cu);
    if(end>SMX_TEX_ROWS_MAX) end=SMX_TEX_ROWS_MAX;
    if(slot<end && MAPMAKER_PTR_OK(rows+slot*SMX_TEX_STRIDE+SMX_TEX_ID,4u) &&
       WORD_AT(rows+slot*SMX_TEX_STRIDE+SMX_TEX_ID)==id) return 1;
    for(k=0;k<end;k++) {
        uint32_t a=rows+k*SMX_TEX_STRIDE+SMX_TEX_ID;
        if(!MAPMAKER_PTR_OK(a,4u)) return 0;
        if(WORD_AT(a)==id) return 1;
    }
    return 0;
}
/* After the load pass: each texture the sky model's file names and whether
 * it was loaded; the missing ones by id. The file's list (+0: 16-byte entries
 * {id, id, -, flags}, -1 ends) is the one in memory: the texture pass has
 * replaced each entry's first word by its loaded-table row (or -1), so the id
 * is the second word (v0.4.0-mapmaker.28 logged rows 0x692.. as "ids"; the
 * files' ids are 24-bit, 0x855357 ...). 0x40000000 is skipped as sub_00096330
 * does. */
static void smx_sky_textures(uint32_t file,uint32_t rec)
{
    uint32_t list,k,id,slot,total=0,loaded=0,named=0;
    smx.tex_total=smx.tex_loaded=0;
    if(!file || !MAPMAKER_PTR_OK(file,0x10u) || !(list=WORD_AT(file))) {
        smx_log("sky textures not checked: model file not read (record, file)",rec,file);
        return;
    }
    for(k=0;k<SMX_TEX_LIST_MAX && MAPMAKER_PTR_OK(file+list+k*16u,16u);k++) {
        slot=WORD_AT(file+list+k*16u);
        if((id=WORD_AT(file+list+k*16u+4u))==0xFFFFFFFFu) break;
        if(id==0x40000000u || slot==0x40000000u) continue;
        total++;
        if(smx_texture_loaded(slot,id)) { loaded++;continue; }
        if(named++<SMX_TEX_LOGGED && smx.logged++<96u) {
            fprintf(stderr,"[mapmaker] skymusic: sky texture not loaded: textures/%06x.xbt (row %d, record %u; in no mounted pak?)\n",id,(int)slot,rec);
            fflush(stderr);
        }
    }
    smx.tex_total=total;smx.tex_loaded=loaded;
    smx_log("sky textures loaded (loaded, total)",loaded,total);
}

/* WRAP sub_0011D2C0 (a view's sky prop; cdecl view): probe only. Logs the
 * record and model the view's sky was built from and whether the model's
 * file was read (resource row +4 loaded, or its request holds data), and,
 * once per level, which of its textures were loaded. */
void pc_mapmaker_skymusic_sky_built(void (*original)(void))
{
    uint32_t view=MAPMAKER_PTR_OK(g_esp,8u)?WORD_AT(g_esp+4u):0u,info,rec,model,row,e=0,req=0,data=0,prop;
    original();
    if(!(info=smx_play_info()) ||
       !smx_bits(WORD_AT(info+SMX_INFO_FLAGS),MAPMAKER_SKY_EXTRA_MASK,MAPMAKER_SKY_EXTRA_SHIFT,MAPMAKER_SKY_EXTRA_COUNT) ||
       smx.level_logs>=2u) return;
    smx.level_logs++;
    rec=WORD_AT(SMX_LEVEL100+0x10u);
    model=rec<68u?WORD_AT(SMX_SKY_TABLE+rec*0x34u+4u):0xFFFFFFFFu;
    prop=MAPMAKER_PTR_OK(view,0xE08u)?WORD_AT(view+0xE04u):0u;
    row=model<SMX_RESMAP_MODELS?(uint32_t)smx_get16(SMX_RESMAP+model*2u)&0xFFFFu:0xFFFFu;
    if(row<0x8000u && WORD_AT(SMX_RES)) e=WORD_AT(SMX_RES)+row*0x30u;
    if(e && MAPMAKER_PTR_OK(e,0x30u) && WORD_AT(e+0x10u)) {
        req=guest_call(sub_000631C0,1,WORD_AT(e+0x10u));
        data=req && MAPMAKER_PTR_OK(req,0x14u)?WORD_AT(req+0x10u):0u;
    }
    fprintf(stderr,"[mapmaker] skymusic: sky built: record %u model 0x%X prop 0x%08X prop model 0x%X row %d loaded %u request 0x%08X data 0x%08X\n",
            rec,model,prop,prop && MAPMAKER_PTR_OK(prop,0x98u)?WORD_AT(prop+0x94u):0u,(int)(int16_t)row,
            e && MAPMAKER_PTR_OK(e,8u)?WORD_AT(e+4u):0u,req,data);
    fflush(stderr);
    if(smx.level_logs==1u) smx_sky_textures(data,rec);
}

/* WRAP sub_00119960 (weather load, no args; zeroes the weather flags, then
 * story scenes set theirs). A sandstorm map: mount its pak, then flag and
 * models, as 0x00119A47. */
void pc_mapmaker_skymusic_weather_load(void (*original)(void))
{
    uint32_t info,k,model;
    original();
    if(!(info=smx_play_info()) ||
       !(k=smx_bits(WORD_AT(info+SMX_INFO_FLAGS),SMX_WEATHER_MASK,SMX_WEATHER_SHIFT,SMX_WEATHER_COUNT))) return;
    if(!smx.sand_ready && !(smx.sand_ready=smx_mount_pair(smx_weather[k-1u].pak,smx_weather[k-1u].mem))) { smx_log("sandstorm pak or its textures pak not mounted; no sandstorm",0,0);return; }
    for(k=0;k<4u;k++) {
        model=WORD_AT(SMX_SAND_MODELS+k*0x44u);
        if(model!=0xFFFFFFFFu && !smx_model_ok(model)) { smx_log("sandstorm model has no resource row; no sandstorm",model,0);return; }
    }
    WORD_AT(SMX_SANDSTORM)=1u;
    for(k=0;k<4u;k++) {
        model=WORD_AT(SMX_SAND_MODELS+k*0x44u);
        if(model!=0xFFFFFFFFu) guest_call(sub_000A5E40,1,model);
    }
    smx.sand_on=1;
    smx_log("sandstorm on, models requested (first, count)",WORD_AT(SMX_SAND_MODELS),4u);
}

/* The sandstorm may be drawn: armed by us this level and its buffers exist. */
static int smx_sand_list_ok(void)
{
    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) return 0;
    if(!WORD_AT(SMX_WX_BUF1) || !WORD_AT(SMX_WX_BUF2)) { smx_log("sandstorm buffers missing; not drawn",WORD_AT(SMX_WX_BUF1),WORD_AT(SMX_WX_BUF2));return 0; }
    return 1;
}

/* WRAP sub_0011B310 (weather start: resets the list to the type-0 default,
 * creates the sandstorm props when [0x0073D9AC]). */
void pc_mapmaker_skymusic_weather_start(void (*original)(void))
{
    original();
    if(!smx_sand_list_ok()) return;
    WORD_AT(SMX_WX_LIST)=smx_weather[0].list;
    WORD_AT(SMX_WX_COUNT)=1u;
    WORD_AT(SMX_WX_INDEX)=0;
    smx_log("weather start: sandstorm list set (first prop, list)",WORD_AT(SMX_SAND_MODELS+0x24u),smx_weather[0].list);
}

/* WRAP sub_00119AB0 (cdecl count, list: the level's weather list). Its only
 * caller, the level setup sub_0006D7A0 (from sub_0006EC10 at 0x0003D64A,
 * right after the weather start), passes scene 100's empty list, which
 * selects the type-0 default: a sandstorm map gets its type-5 list instead. */
void pc_mapmaker_skymusic_weather_list(void (*original)(void))
{
    if(MAPMAKER_PTR_OK(g_esp,12u) && smx_sand_list_ok()) {
        WORD_AT(g_esp+4u)=1u;
        WORD_AT(g_esp+8u)=smx_weather[0].list;
        smx_log("level setup: sandstorm list kept (count, list)",1u,smx_weather[0].list);
    }
    original();
}

static float smx_rand(void)                  /* [0, 1) */
{
    smx.rng=smx.rng*1103515245u+12345u;
    return (float)((smx.rng>>16)&0x7FFFu)/32768.0f;
}
/* Sand grains for a MapMaker map, written into the stock ring as the stock
 * emitter does (0x0011ADE6..0x0011AED7): position, velocity = wind x 6 with
 * jitter, 0..1.2 up, a random angle and spin, age 0. One grain per
 * SMX_GRAIN_EVERY update steps (at most 4 a frame) for each player the
 * weather runs for (the stock player test) and who is outdoors
 * ([0x007AC844+4p] > 0, the stock rain test sub_00119B50), in a 10 m box
 * around the player, upwind so the grains blow past. The draw
 * (sub_0011B110) needs no room or zone: a grain is a sprite of size
 * age/3 + 1.5 m at its position whose alpha rises and falls over its 3 s
 * life (peak near 0.9 s). .31 made one per step: 60 a second in a ring of
 * 60, so each was overwritten after 1 s, spread over 24 x 24 m. Returns the
 * count. */
static uint32_t smx_sand_emit(void)
{
    uint32_t ring=WORD_AT(SMX_WX_BUF2),base=WORD_AT(SMX_PLAYERS),n=WORD_AT(SMX_PLAYERS_N),steps=WORD_AT(SMX_STEPS);
    uint32_t p,i,pl,flags,g,e,made=0;
    float x,y,z,s;
    if(!MAPMAKER_PTR_OK(ring,SMX_GRAINS*SMX_GRAIN) || (int32_t)steps<=0) return 0;
    if(steps>4u*SMX_GRAIN_EVERY) steps=4u*SMX_GRAIN_EVERY;
    steps+=smx.sand_steps%SMX_GRAIN_EVERY;
    smx.sand_steps=steps%SMX_GRAIN_EVERY;
    steps/=SMX_GRAIN_EVERY;                  /* grains per player this frame */
    if(n>SMX_PLAYERS_MAX) n=SMX_PLAYERS_MAX;
    if(!smx.rng) smx.rng=0x5A4D5358u;
    for(p=0;p<n;p++) {
        pl=base+p*SMX_PLAYER;
        if(!MAPMAKER_PTR_OK(pl,SMX_PL_POS+12u)) continue;
        flags=WORD_AT(pl+SMX_PL_FLAGS);
        if(!(flags&0x80000000u) || !(flags&0x400u) || !(FLOAT_AT(SMX_WX_EXPOSED+p*4u)>0.0f)) continue;
        x=FLOAT_AT(pl+SMX_PL_POS);y=FLOAT_AT(pl+SMX_PL_POS+4u);z=FLOAT_AT(pl+SMX_PL_POS+8u);
        for(i=0;i<steps;i++) {
            g=WORD_AT(SMX_WX_RING);
            if(g>=SMX_GRAINS) g=0;
            e=ring+g*SMX_GRAIN;
            FLOAT_AT(e)=x-smx_wind[0]*SMX_GRAIN_UPWIND+(smx_rand()*2.0f-1.0f)*SMX_GRAIN_SPREAD;
            FLOAT_AT(e+4u)=y+smx_rand()*3.0f-1.0f+0.3f;
            FLOAT_AT(e+8u)=z-smx_wind[2]*SMX_GRAIN_UPWIND+(smx_rand()*2.0f-1.0f)*SMX_GRAIN_SPREAD;
            s=(smx_rand()+1.0f)*0.5f*6.0f;
            FLOAT_AT(e+0x0Cu)=smx_wind[0]*s+(smx_rand()-0.5f)*0.25f;
            FLOAT_AT(e+0x10u)=smx_rand()*1.2f;
            FLOAT_AT(e+0x14u)=smx_wind[2]*s+(smx_rand()-0.5f)*0.25f;
            FLOAT_AT(e+0x18u)=(smx_rand()-0.5f)*6.2831853f;
            FLOAT_AT(e+0x1Cu)=(smx_rand()-0.5f)*1.5707963f;
            FLOAT_AT(e+0x20u)=0.0f;
            WORD_AT(SMX_WX_RING)=(g+1u)%SMX_GRAINS;
            made++;
        }
    }
    smx.grains+=made;
    return made;
}

/* WRAP sub_0011BBC0 (weather frame update, no args; called every frame from
 * sub_001CE420 at 0x001CE55F/0x001CE596 with no scene test; it runs the
 * sandstorm (sub_0011ACF0) only while [0x0073D9AC] and the current list
 * entry has type 5, 0x0011BC4F). On a sandstorm map: while the original runs
 * the emitters' scene-2 rooms read 0 (every slot is tested, 0 skipped), so no
 * MapMaker piece matches them; they are put back after, unchanged for story
 * levels. Then, while the entry has type 5, our grains (smx_sand_emit). Logs,
 * on the 1st, 60th and 600th frame of the level, the list, the entry's type,
 * the flag, the buffers, and what is drawn: live grains, views flagged for
 * the draw, player 1's exposure and the grain/haze texture handles, then
 * smx_sand_probe. */
static void smx_sand_probe(void);
void pc_mapmaker_skymusic_weather_frame(void (*original)(void))
{
    uint32_t list,index,type,k,alive=0,made=0,rooms[SMX_SAND_EMITTER_COUNT*SMX_SAND_ROOM_SLOTS],ring;
    if(smx.sand_on!=1 || smx_scene()!=100u || !WORD_AT(SMX_SANDSTORM)) { original();return; }
    for(k=0;k<SMX_SAND_EMITTER_COUNT*SMX_SAND_ROOM_SLOTS;k++) {
        uint32_t a=SMX_SAND_EMITTERS+(k/SMX_SAND_ROOM_SLOTS)*0x44u+(k%SMX_SAND_ROOM_SLOTS)*4u;
        rooms[k]=WORD_AT(a);WORD_AT(a)=0;
    }
    original();
    for(k=0;k<SMX_SAND_EMITTER_COUNT*SMX_SAND_ROOM_SLOTS;k++)
        WORD_AT(SMX_SAND_EMITTERS+(k/SMX_SAND_ROOM_SLOTS)*0x44u+(k%SMX_SAND_ROOM_SLOTS)*4u)=rooms[k];
    list=WORD_AT(SMX_WX_LIST);index=WORD_AT(SMX_WX_INDEX);
    type=MAPMAKER_PTR_OK(list+index*0x28u,4u)?WORD_AT(list+index*0x28u):0xFFFFFFFFu;
    if(type==5u) made=smx_sand_emit();
    smx.frames++;
    if(smx.frames!=1u && smx.frames!=60u && smx.frames!=600u) return;
    fprintf(stderr,"[mapmaker] skymusic: weather frame %u: list 0x%08X index %u type %d sandstorm %u buffers 0x%08X 0x%08X (type 5 draws)\n",
            smx.frames,list,index,(int)type,WORD_AT(SMX_SANDSTORM),WORD_AT(SMX_WX_BUF1),WORD_AT(SMX_WX_BUF2));
    ring=WORD_AT(SMX_WX_BUF2);
    if(MAPMAKER_PTR_OK(ring,SMX_GRAINS*SMX_GRAIN))
        for(k=0;k<SMX_GRAINS;k++) if(FLOAT_AT(ring+k*SMX_GRAIN+0x20u)>=0.0f) alive++;
    fprintf(stderr,"[mapmaker] skymusic: sandstorm drawn: grains alive %u of %u (this frame %u, level %u), views flagged %u %u %u %u, "
            "player 1 outdoors %.2f, textures 0x303 0x%08X 0x328 0x%08X, scene-2 props 0x%08X 0x%08X 0x%08X 0x%08X (not animated in MapMaker)\n",
            alive,SMX_GRAINS,made,smx.grains,WORD_AT(SMX_WX_DRAWN),WORD_AT(SMX_WX_DRAWN+4u),WORD_AT(SMX_WX_DRAWN+8u),WORD_AT(SMX_WX_DRAWN+12u),
            (double)FLOAT_AT(SMX_WX_EXPOSED),WORD_AT(SMX_TEX_GLOBAL+SMX_TEX_GRAIN*0x18u),WORD_AT(SMX_TEX_GLOBAL+SMX_TEX_HAZE*0x18u),
            WORD_AT(SMX_SAND_EMITTERS+SMX_SAND_PROP),WORD_AT(SMX_SAND_EMITTERS+0x44u+SMX_SAND_PROP),
            WORD_AT(SMX_SAND_EMITTERS+0x88u+SMX_SAND_PROP),WORD_AT(SMX_SAND_EMITTERS+0xCCu+SMX_SAND_PROP));
    smx_sand_probe();
    fflush(stderr);
}

/* Frame probe: the newest grain's fields, view 0's first haze layer, the
 * boosted draws and the fog row, and the four scene-2 props (position +0x64,
 * instance +4: alpha byte +0x1FA, flags +0x1F6, which only the stock emitter
 * code sets, 0x0011AF9D/0x0011AFAB). */
static void smx_sand_probe(void)
{
    uint32_t ring=WORD_AT(SMX_WX_BUF2),haze=WORD_AT(SMX_WX_BUF1),pl=WORD_AT(SMX_PLAYERS),k,g,prop,inst;
    g=(WORD_AT(SMX_WX_RING)+SMX_GRAINS-1u)%SMX_GRAINS;
    if(MAPMAKER_PTR_OK(ring,SMX_GRAINS*SMX_GRAIN)) {
        uint32_t e=ring+g*SMX_GRAIN;
        fprintf(stderr,"[mapmaker] skymusic: sandstorm grain %u: pos %.2f %.2f %.2f velocity %.2f %.2f %.2f angle %.2f spin %.2f age %.2f",
                g,(double)FLOAT_AT(e),(double)FLOAT_AT(e+4u),(double)FLOAT_AT(e+8u),(double)FLOAT_AT(e+0x0Cu),(double)FLOAT_AT(e+0x10u),
                (double)FLOAT_AT(e+0x14u),(double)FLOAT_AT(e+0x18u),(double)FLOAT_AT(e+0x1Cu),(double)FLOAT_AT(e+0x20u));
        if(MAPMAKER_PTR_OK(pl,SMX_PL_POS+12u))
            fprintf(stderr,"; player 1 at %.2f %.2f %.2f",(double)FLOAT_AT(pl+SMX_PL_POS),(double)FLOAT_AT(pl+SMX_PL_POS+4u),(double)FLOAT_AT(pl+SMX_PL_POS+8u));
        fputc('\n',stderr);
    }
    fprintf(stderr,"[mapmaker] skymusic: sandstorm haze: view 0 layer 0 offset %.2f %.2f depth %.2f alpha %.2f; weight %u (stock 5), "
            "grain alpha 0x%02X (stock 0x10), boosted draws haze %u grains %u, fog row %u (tileset's %u)\n",
            MAPMAKER_PTR_OK(haze,0x30u)?(double)FLOAT_AT(haze):0.0,MAPMAKER_PTR_OK(haze,0x30u)?(double)FLOAT_AT(haze+4u):0.0,
            MAPMAKER_PTR_OK(haze,0x30u)?(double)FLOAT_AT(haze+8u):0.0,MAPMAKER_PTR_OK(haze,0x30u)?(double)FLOAT_AT(haze+0x2Cu):0.0,
            SMX_HAZE_ALPHA,SMX_GRAIN_ALPHA,smx.haze_boosts,smx.grain_boosts,WORD_AT(SMX_LEVEL100+SMX_LEVEL_FOG),smx.tileset_fog);
    for(k=0;k<SMX_SAND_EMITTER_COUNT;k++) {
        prop=WORD_AT(SMX_SAND_EMITTERS+k*0x44u+SMX_SAND_PROP);
        if(!MAPMAKER_PTR_OK(prop,0x70u)) { fprintf(stderr,"[mapmaker] skymusic: scene-2 prop %u: none (0x%08X)\n",k+1u,prop);continue; }
        inst=WORD_AT(prop+4u);
        fprintf(stderr,"[mapmaker] skymusic: scene-2 prop %u: 0x%08X at %.2f %.2f %.2f, rooms %d, instance 0x%08X alpha %d flags 0x%04X\n",
                k+1u,prop,(double)FLOAT_AT(prop+0x64u),(double)FLOAT_AT(prop+0x68u),(double)FLOAT_AT(prop+0x6Cu),(int)(int8_t)*smx_byte(prop+0x30u),
                inst,MAPMAKER_PTR_OK(inst,0x200u)?(int)*smx_byte(inst+0x1FAu):-1,MAPMAKER_PTR_OK(inst,0x200u)?(unsigned)(smx_byte(inst+0x1F6u)[0]|smx_byte(inst+0x1F6u)[1]<<8):0u);
    }
}

/* WRAP sub_0011A930 (the weather haze: eax = colour 0xRRGGBB00 | weight,
 * [esp+4] layers; sandstorm 0x7F5A1E05 from 0x0011B13E, snow 0x60707F05 /
 * 0x60707Fxx). Each quad blends four layers; its alpha is
 * ((sum of layer alpha) x weight + 1) / 8 (0x0011AB4C..0x0011AB82), at most
 * 2 of 255 for the sandstorm's weight 5. On a MapMaker sandstorm map the
 * sandstorm's call gets SMX_HAZE_ALPHA (at most 40); snow and story levels
 * keep theirs. */
void pc_mapmaker_skymusic_haze(void (*original)(void))
{
    if(MAPMAKER_PTR_OK(g_esp,4u) && WORD_AT(g_esp)==SMX_RET_HAZE && smx_sand_live() && (g_eax&0xFFu)<SMX_HAZE_ALPHA) {
        g_eax=(g_eax&0xFFFFFF00u)|SMX_HAZE_ALPHA;
        smx.haze_boosts++;
    }
    original();
}

/* WRAP sub_00154FE0 (cdecl colour, fade: the colour's low byte, its alpha,
 * times fade clamped to 0..1; menus use it too). The grain draw calls it for
 * each live grain with 0x7F643C10 (0x0011B23D): alpha 16 of 255 at the
 * peak, so .31's grains were drawn but nearly transparent. On a MapMaker
 * sandstorm map that call gets SMX_GRAIN_ALPHA; every other caller, scene
 * and colour is untouched. The caller pops its own arguments and does not
 * read the colour again. */
void pc_mapmaker_skymusic_grain_colour(void (*original)(void))
{
    if(MAPMAKER_PTR_OK(g_esp,12u) && WORD_AT(g_esp)==SMX_RET_GRAIN_COLOUR && WORD_AT(g_esp+4u)==SMX_GRAIN_COLOUR && smx_sand_live()) {
        WORD_AT(g_esp+4u)=(SMX_GRAIN_COLOUR&0xFFFFFF00u)|SMX_GRAIN_ALPHA;
        smx.grain_boosts++;
    }
    original();
}

/* WRAP sub_00045DE0 (start the level music: eax = music id, -1 = the
 * tileset's, [esp+4]). */
void pc_mapmaker_skymusic_music(void (*original)(void))
{
    uint32_t info,k;
    if(MAPMAKER_PTR_OK(g_esp,4u) && WORD_AT(g_esp)==SMX_RET_MUSIC_PLAY && g_eax==0xFFFFFFFFu &&
       (info=smx_play_info())!=0 &&
       (k=smx_bits(WORD_AT(info+SMX_INFO_FLAGS),SMX_MUSIC_MASK,SMX_MUSIC_SHIFT,SMX_MUSIC_COUNT))!=0) {
        g_eax=smx_music[k-1u].id;
        smx_log("music: extra track (row, id)",k,g_eax);
    }
    original();
}
