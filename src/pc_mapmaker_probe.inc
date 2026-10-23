/* US 45410066 MapMaker capacity-meter probe (read-only instrumentation).
 *
 * sub_0030C210 paints the MapMaker capacity meter through the shared bar
 * primitive sub_000814B0 (STATUS.md #114 audit). The meter painter wrapper
 * marks a scope; bars drawn inside it are attributed to the meter without
 * guessing the painter's internal return address.
 *
 * Off unless TSFP_MAPMAKER_PROBE is a nonzero flag set:
 *   1  meter calls: caller, scene, ecx, six stack words, each bar's fraction,
 *      rectangle and colours
 *   2  also 64-byte snapshots of ecx and the first four stack words when they
 *      point into guest RAM (candidate budget/editor structures)
 *   4  also bars drawn outside the meter scope while MapMaker is active
 *   8  also, while MapMaker is active, each menu laid out by sub_00073750
 *      (header, item list with type/flags/callback/rectangle and 64 raw
 *      bytes) and every left click's guest-UI pointer position and hit item
 *  16  with 1: each meter record also lists the budgets behind the meter:
 *      the map container's used bytes (chunk 0x00C77668 +0x248 of
 *      [0x00784014]) and the categories sub_002FA720 folds into editor+0x270,
 *      recomputed from the placed-piece list, plus editor+0x270/+0x274
 *  32  in every scene, the game's heap state once a second when it changed:
 *      arena base/size, Level heap low/top/end pointers, shared free count
 *      (signed) and its minimum since the scene started, Eternal heap pointer
 *      and free, per-frame display buffer size and peak, and the two reserve
 *      flags (research/capacity-constraints.md, play-time memory)
 *  64  while a MapMaker map is played (scene 100), the AI navigation counts
 *      once a second when they changed: regular pads [0x007B6664], links
 *      [0x007B6660], cover pads [0x007B65C0], pad groups [0x007B666C] and the
 *      pool pointers (gaps give each pool's size), engine objects taken
 *      [0x007B4FAC] of the pool capacity [0x0072F5E4], logic tags [0x0075EA90]
 * Output: TSFP_MAPMAKER_PROBE_LOG (binary, truncated per process), else stderr.
 * Records are emitted when their content changes, at most four per second,
 * with the number of unchanged repeats; the file is capped at 4 MiB.
 *
 * "MapMaker is active" means scene 102 (the editor: every meter record of the
 * first Windows run, 2026-10-22, reported it) or scene 100 (kept for map play).
 *
 * Never writes guest memory, guest registers or the guest stack. Requires the
 * includer's WORD_AT, FLOAT_AT, guest_writable, g_esp, g_ecx and GetTickCount64. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAPMAKER_PROBE_METER 1u
#define MAPMAKER_PROBE_DUMPS 2u
#define MAPMAKER_PROBE_OTHER 4u
#define MAPMAKER_PROBE_MENUS 8u
#define MAPMAKER_PROBE_BUDGETS 16u
#define MAPMAKER_PROBE_HEAP 32u
#define MAPMAKER_PROBE_POOLS 64u
#define MAPMAKER_PROBE_ALL 127u
#define MAPMAKER_PROBE_HEAP_INTERVAL 1000u
#define MAPMAKER_PROBE_MENU_SLOTS 16u
#define MAPMAKER_PROBE_MENU_ITEMS 64u
#define MAPMAKER_PROBE_MENU_INTERVAL 1000u
#define MAPMAKER_PROBE_SCENE_PLAY 100
#define MAPMAKER_PROBE_SCENE_EDITOR 102
#define MAPMAKER_PROBE_CAP (4u*1024u*1024u)
#define MAPMAKER_PROBE_INTERVAL 250u
#define MAPMAKER_PROBE_BARS 8u
#define MAPMAKER_PROBE_OTHERS 16u

typedef struct MapMakerProbeText { char text[6144]; size_t used; } MapMakerProbeText;
typedef struct MapMakerProbeOther {
    uint32_t ret; char key[160]; unsigned long long last, repeats;
} MapMakerProbeOther;
static struct {
    int initialized;
    unsigned flags;
    FILE *file;
    size_t written;
    int capped;
    unsigned depth;
    unsigned long long calls;
    /* Current meter scope */
    uint32_t caller, ecx, args[6];
    int32_t scene;
    unsigned bars;
    MapMakerProbeText key;
    /* Last emitted meter record */
    char last[6144];
    unsigned long long last_time, repeats, emitted;
    MapMakerProbeOther others[MAPMAKER_PROBE_OTHERS];
    struct { uint32_t menu; uint32_t hash; unsigned long long last, repeats; } menus[MAPMAKER_PROBE_MENU_SLOTS];
} mapmaker_probe;
static char mapmaker_probe_menu_text[32768];

static void mapmaker_probe_append(MapMakerProbeText *t,const char *format,...)
{
    va_list list;int count;
    if(t->used>=sizeof t->text-1)return;
    va_start(list,format);
    count=vsnprintf(t->text+t->used,sizeof t->text-t->used,format,list);
    va_end(list);
    if(count<0)return;
    t->used+=(size_t)count<sizeof t->text-t->used?(size_t)count:sizeof t->text-1-t->used;
}

/* Append raw text, honouring the cap. */
static void mapmaker_probe_write_text(const char *text,size_t length)
{
    FILE *out=mapmaker_probe.file?mapmaker_probe.file:stderr;
    if(mapmaker_probe.capped || !length)return;
    if(mapmaker_probe.written+length>MAPMAKER_PROBE_CAP) {
        fputs("[mapmaker-probe] log cap reached; further records dropped\n",out);
        fflush(out);mapmaker_probe.capped=1;return;
    }
    fwrite(text,1,length,out);fflush(out);
    mapmaker_probe.written+=length;
}

static void mapmaker_probe_write(const char *format,...)
{
    char line[8192];va_list list;int count;
    if(mapmaker_probe.capped)return;
    va_start(list,format);
    count=vsnprintf(line,sizeof line,format,list);
    va_end(list);
    if(count<=0)return;
    mapmaker_probe_write_text(line,(size_t)count<sizeof line?(size_t)count:sizeof line-1);
}

static unsigned mapmaker_probe_flags(void)
{
    if(!mapmaker_probe.initialized) {
        const char *text=getenv("TSFP_MAPMAKER_PROBE"),*path;
        char *end=NULL;unsigned long value=text?strtoul(text,&end,0):0;
        mapmaker_probe.initialized=1;
        mapmaker_probe.flags=(text && end!=text && !*end)?(unsigned)(value&MAPMAKER_PROBE_ALL):0;
        if(!mapmaker_probe.flags)return 0;
        path=getenv("TSFP_MAPMAKER_PROBE_LOG");
        /* Binary: identical bytes and cap accounting on every host. */
        if(path && *path)mapmaker_probe.file=fopen(path,"wb");
        mapmaker_probe_write("[mapmaker-probe] v2 flags=%u read-only; meter sub_0030C210, bar sub_000814B0; output %s\n",
            mapmaker_probe.flags,mapmaker_probe.file?path:"stderr");
    }
    return mapmaker_probe.flags;
}

static int32_t mapmaker_probe_scene(void)
{
    return guest_writable(0x0079094Cu,4)?(int32_t)WORD_AT(0x0079094Cu):-1;
}
static int mapmaker_probe_active(void)
{
    int32_t scene=mapmaker_probe_scene();
    return scene==MAPMAKER_PROBE_SCENE_EDITOR || scene==MAPMAKER_PROBE_SCENE_PLAY;
}

/* Bar arguments: fraction (float), x, y, w, h, four colour words. */
static void mapmaker_probe_bar_text(MapMakerProbeText *t,uint32_t sp)
{
    uint32_t w[10];float fraction;unsigned i;
    if(!guest_writable(sp,sizeof w)) { mapmaker_probe_append(t," <stack unreadable>");return; }
    for(i=0;i<10;i++)w[i]=WORD_AT(sp+i*4u);
    memcpy(&fraction,&w[1],sizeof fraction);
    mapmaker_probe_append(t," ret=0x%08X fraction=%.6f (0x%08X) rect=%d,%d,%d,%d colours=0x%08X 0x%08X 0x%08X 0x%08X",
        w[0],(double)fraction,w[1],(int32_t)w[2],(int32_t)w[3],(int32_t)w[4],(int32_t)w[5],w[6],w[7],w[8],w[9]);
}

static void mapmaker_probe_dump(MapMakerProbeText *t,const char *name,uint32_t address)
{
    unsigned i;
    if(address<0x10000u || address>=0x04000000u || !guest_writable(address,64))return;
    mapmaker_probe_append(t,"  dump %s@0x%08X:",name,address);
    for(i=0;i<16;i++)mapmaker_probe_append(t,"%s%08X",i==8?"  ":" ",WORD_AT(address+i*4u));
    mapmaker_probe_append(t,"\n");
}

/* Called by the existing sub_000814B0 wrapper, before the original bar. */
static void mapmaker_probe_bar(void)
{
    unsigned flags=mapmaker_probe_flags();
    if(!flags)return;
    if(mapmaker_probe.depth) {
        if(mapmaker_probe.bars<MAPMAKER_PROBE_BARS) {
            mapmaker_probe_append(&mapmaker_probe.key,"  bar[%u]",mapmaker_probe.bars);
            mapmaker_probe_bar_text(&mapmaker_probe.key,g_esp);
            mapmaker_probe_append(&mapmaker_probe.key,"\n");
        }
        mapmaker_probe.bars++;
        return;
    }
    if((flags&MAPMAKER_PROBE_OTHER) && mapmaker_probe_active() &&
       guest_writable(g_esp,4)) {
        MapMakerProbeText text;MapMakerProbeOther *slot=NULL;
        uint32_t ret=WORD_AT(g_esp);unsigned i;
        unsigned long long now=GetTickCount64();
        text.used=0;text.text[0]=0;
        mapmaker_probe_bar_text(&text,g_esp);
        for(i=0;i<MAPMAKER_PROBE_OTHERS && !slot;i++)
            if(mapmaker_probe.others[i].ret==ret || !mapmaker_probe.others[i].ret)slot=&mapmaker_probe.others[i];
        if(!slot)return;
        if(slot->ret==ret && !strcmp(slot->key,text.text)) { slot->repeats++;return; }
        if(slot->ret==ret && now-slot->last<MAPMAKER_PROBE_INTERVAL) { slot->repeats++;return; }
        mapmaker_probe_write("[mapmaker-probe] t=%llu other-bar scene=%d%s repeats=%llu\n",
            now,(int)mapmaker_probe_scene(),text.text,slot->repeats);
        slot->ret=ret;slot->last=now;slot->repeats=0;
        snprintf(slot->key,sizeof slot->key,"%s",text.text);
    }
}

/* Flag 16, read-only. The map container: dword 0x00C77667, version 0x12D,
 * then {tag, size, data} chunks from offset 8, ended by 0x00C7766A; the
 * map-info chunk 0x00C77668 holds the used byte count at data +0x248. */
static uint32_t mapmaker_probe_container_used(void)
{
    uint32_t c,off,tag,size;
    if(!guest_writable(0x00784014u,4)) return 0;
    c=WORD_AT(0x00784014u);
    if(!guest_writable(c,0x7FE0) || WORD_AT(c)!=0x00C77667u || WORD_AT(c+4)!=0x12Du) return 0;
    for(off=8;off+8<=0x7FE0;off+=8+size) {
        tag=WORD_AT(c+off);size=WORD_AT(c+off+4);
        if(tag==0x00C7766Au || size>0x7FE0) return 0;
        if(tag==0x00C77668u) return size>=0x24C ? WORD_AT(c+off+8+0x248) : 0;
    }
    return 0;
}
/* The categories of sub_002FA720 (limits from its reciprocal constants).
 * Pieces: list at [0x0078400C] linked by +0x40, type [[piece+0x2C]], costs
 * in the 0x140-byte rows at 0x00529400 (+0, +4, +8); sub-entries from +0x3C
 * linked by +0x14, counted when their first dword is above 9. The seventh
 * category (sub_002FA060) is not recomputed: that would run guest code. */
static void mapmaker_probe_budgets(MapMakerProbeText *k)
{
    uint32_t piece,editor,sum[3]={0,0,0},pieces=0,deep=0,extra=0,i,j;
    int32_t global=0;
    float e270=0,e274=0;
    piece=guest_writable(0x0078400Cu,4)?WORD_AT(0x0078400Cu):0;
    for(i=0;piece && i<4096 && guest_writable(piece,0x44);i++,piece=WORD_AT(piece+0x40)) {
        uint32_t tp=WORD_AT(piece+0x2c),sub;
        pieces++;
        if(guest_writable(tp,4) && WORD_AT(tp)<1024u) {
            uint32_t row=0x00529400u+WORD_AT(tp)*0x140u;
            if(guest_writable(row,12)) { sum[0]+=WORD_AT(row);sum[1]+=WORD_AT(row+4);sum[2]+=WORD_AT(row+8); }
        }
        sub=WORD_AT(piece+0x3c);
        for(j=0;sub && j<4096 && guest_writable(sub,0x18);j++,sub=WORD_AT(sub+0x14))
            if((int32_t)WORD_AT(sub)>9) deep++;
    }
    if(guest_writable(0x00762CB8u,4)) global=(int32_t)WORD_AT(0x00762CB8u);
    editor=guest_writable(0x007844A8u,4)?WORD_AT(0x007844A8u):0;
    if(guest_writable(editor,0x3074)) { /* the word at +0x306E is read as a dword */
        extra=WORD_AT(editor+0x306e)&0xFFFFu;
        e270=FLOAT_AT(editor+0x270);e274=FLOAT_AT(editor+0x274);
    }
    mapmaker_probe_append(k,"  budgets container=%u/32736 cost0=%u/2400 cost1=%u/1024 cost2=%u/4400"
        " global=%d/300 pieces=%u/200 deep=%u+%u/50 e270=%.4f e274=%.4f\n",
        mapmaker_probe_container_used(),sum[0],sum[1],sum[2],(int)global,pieces,deep,extra,
        (double)e270,(double)e274);
}

/* Flag 32, read-only, called every frame. sub_0003DFA0 heaps: arena at
 * [0x007E72D4]/[0x007E72D8]; Level heap (1, bottom-up) [0x006B837C] and
 * heap 0 (top-down) [0x006B8388] share the end [0x006B8380] and the free
 * count [0x006B8384], which goes negative when they overlap (the retail
 * allocator does not stop); Eternal heap (2) [0x006B8370], free
 * [0x006B8378]; display buffer size [0x006B83AC], peak [0x006B83B0];
 * reserves [0x004C81B4] (0x266000) and [0x007DE398]&0x40 (0x131800). */
static struct {
    int32_t scene, min_free;
    int have;
    unsigned long long last;
    uint32_t key[12];
} mapmaker_probe_heap_state;
static uint32_t mapmaker_probe_word(uint32_t a) { return guest_writable(a,4)?WORD_AT(a):0u; }
static void mapmaker_probe_heap(void)
{
    uint32_t key[12];
    int32_t scene,free_now;
    unsigned long long now;
    if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_HEAP)) return;
    scene=mapmaker_probe_scene();
    free_now=(int32_t)mapmaker_probe_word(0x006B8384u);
    if(!mapmaker_probe_heap_state.have || scene!=mapmaker_probe_heap_state.scene) {
        mapmaker_probe_heap_state.have=1;mapmaker_probe_heap_state.scene=scene;
        mapmaker_probe_heap_state.min_free=free_now;mapmaker_probe_heap_state.last=0;
        memset(mapmaker_probe_heap_state.key,0xff,sizeof mapmaker_probe_heap_state.key);
    }
    if(free_now<mapmaker_probe_heap_state.min_free) mapmaker_probe_heap_state.min_free=free_now;
    key[0]=mapmaker_probe_word(0x007E72D4u);key[1]=mapmaker_probe_word(0x007E72D8u);
    key[2]=mapmaker_probe_word(0x006B837Cu);key[3]=mapmaker_probe_word(0x006B8388u);
    key[4]=mapmaker_probe_word(0x006B8380u);key[5]=(uint32_t)free_now;
    key[6]=(uint32_t)mapmaker_probe_heap_state.min_free;
    key[7]=mapmaker_probe_word(0x006B8370u);key[8]=mapmaker_probe_word(0x006B8378u);
    key[9]=mapmaker_probe_word(0x006B83ACu);key[10]=mapmaker_probe_word(0x006B83B0u);
    key[11]=(mapmaker_probe_word(0x004C81B4u)?1u:0u)|((mapmaker_probe_word(0x007DE398u)&0x40u)?2u:0u);
    now=GetTickCount64();
    if(!memcmp(key,mapmaker_probe_heap_state.key,sizeof key)) return;
    if(mapmaker_probe_heap_state.last && now-mapmaker_probe_heap_state.last<MAPMAKER_PROBE_HEAP_INTERVAL) return;
    mapmaker_probe_write("[mapmaker-probe] t=%llu heap scene=%d arena=0x%08X+%u level=0x%08X..0x%08X end=0x%08X"
        " free=%d min_free=%d eternal=0x%08X free=%u display=%u peak=%u reserves=%u\n",
        now,(int)scene,key[0],key[1],key[2],key[3],key[4],(int32_t)key[5],(int32_t)key[6],
        key[7],key[8],key[9],key[10],key[11]);
    memcpy(mapmaker_probe_heap_state.key,key,sizeof key);
    mapmaker_probe_heap_state.last=now;
}

/* Flag 64, read-only, called every frame. */
static struct { uint32_t key[12]; unsigned long long last; } mapmaker_probe_pools_state;
static void mapmaker_probe_pools(void)
{
    uint32_t key[12];
    unsigned long long now;
    if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_POOLS) || mapmaker_probe_scene()!=100) return;
    key[0]=mapmaker_probe_word(0x007B6664u);key[1]=mapmaker_probe_word(0x007B6660u);
    key[2]=mapmaker_probe_word(0x007B65C0u);key[3]=mapmaker_probe_word(0x007B666Cu);
    key[4]=mapmaker_probe_word(0x00715F2Cu);key[5]=mapmaker_probe_word(0x00715F30u);
    key[6]=mapmaker_probe_word(0x00715F3Cu);key[7]=mapmaker_probe_word(0x00715F34u);
    key[8]=mapmaker_probe_word(0x00715F38u);
    /* Engine objects taken / pool capacity (taken without a bound check) and logic tags. */
    key[9]=mapmaker_probe_word(0x007B4FACu);key[10]=mapmaker_probe_word(0x0072F5E4u);
    key[11]=mapmaker_probe_word(0x0075EA90u);
    now=GetTickCount64();
    if(!memcmp(key,mapmaker_probe_pools_state.key,sizeof key)) return;
    if(mapmaker_probe_pools_state.last && now-mapmaker_probe_pools_state.last<MAPMAKER_PROBE_HEAP_INTERVAL) return;
    mapmaker_probe_write("[mapmaker-probe] t=%llu pools pads=%u links=%u cover=%u groups=%u"
        " pads@0x%08X links@0x%08X aidata@0x%08X cover@0x%08X runtime@0x%08X"
        " objects=%u/%u tags=%u\n",
        now,key[0],key[1],key[2],key[3],key[4],key[5],key[6],key[7],key[8],key[9],key[10],key[11]);
    memcpy(mapmaker_probe_pools_state.key,key,sizeof key);
    mapmaker_probe_pools_state.last=now;
}

void pc_display_mapmaker_meter(void (*original)(void))
{
    unsigned flags=mapmaker_probe_flags(),i;
    int outer;
    if(!(flags&MAPMAKER_PROBE_METER)) { original();return; }
    outer=!mapmaker_probe.depth;
    if(outer) {
        MapMakerProbeText *k=&mapmaker_probe.key;
        mapmaker_probe.calls++;
        mapmaker_probe.caller=guest_writable(g_esp,4)?WORD_AT(g_esp):0;
        mapmaker_probe.ecx=g_ecx;
        for(i=0;i<6;i++)
            mapmaker_probe.args[i]=guest_writable(g_esp+4u+i*4u,4)?WORD_AT(g_esp+4u+i*4u):0;
        mapmaker_probe.scene=mapmaker_probe_scene();
        mapmaker_probe.bars=0;
        k->used=0;k->text[0]=0;
        mapmaker_probe_append(k," caller=0x%08X scene=%d ecx=0x%08X args=",
            mapmaker_probe.caller,(int)mapmaker_probe.scene,mapmaker_probe.ecx);
        for(i=0;i<6;i++)mapmaker_probe_append(k,"%s0x%08X",i?" ":"",mapmaker_probe.args[i]);
        mapmaker_probe_append(k,"\n");
        if(flags&MAPMAKER_PROBE_DUMPS) {
            static const char *names[]={"arg0","arg1","arg2","arg3"};
            mapmaker_probe_dump(k,"ecx",mapmaker_probe.ecx);
            for(i=0;i<4;i++)mapmaker_probe_dump(k,names[i],mapmaker_probe.args[i]);
        }
    }
    mapmaker_probe.depth++;
    original();
    mapmaker_probe.depth--;
    if(outer) {
        unsigned long long now=GetTickCount64();
        MapMakerProbeText *k=&mapmaker_probe.key;
        mapmaker_probe_append(k,"  bars=%u\n",mapmaker_probe.bars);
        if(mapmaker_probe_flags()&MAPMAKER_PROBE_BUDGETS) mapmaker_probe_budgets(k);
        if(mapmaker_probe.emitted && !strcmp(k->text,mapmaker_probe.last)) {
            mapmaker_probe.repeats++;return;
        }
        if(mapmaker_probe.emitted && now-mapmaker_probe.last_time<MAPMAKER_PROBE_INTERVAL) {
            mapmaker_probe.repeats++;return;
        }
        mapmaker_probe_write("[mapmaker-probe] t=%llu meter call=%llu repeats=%llu\n%s",
            now,mapmaker_probe.calls,mapmaker_probe.repeats,k->text);
        memcpy(mapmaker_probe.last,k->text,k->used+1);
        mapmaker_probe.last_time=now;mapmaker_probe.repeats=0;mapmaker_probe.emitted++;
    }
}

/* Flag 8: menus laid out while MapMaker runs. Called by the existing
 * sub_00073750 wrapper after the original layout, before any of its checks,
 * with the return address read before the original ran. Reads only. */
static uint32_t mapmaker_probe_text_hash(const char *text,size_t length)
{
    uint32_t h=2166136261u;size_t i;
    for(i=0;i<length;i++) { h^=(unsigned char)text[i];h*=16777619u; }
    return h;
}
static void mapmaker_probe_menu(uint32_t menu,uint32_t caller)
{
    size_t used=0;unsigned i,n,slot=MAPMAKER_PROBE_MENU_SLOTS;
    uint32_t item,hash;unsigned long long now;
    if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS) || !mapmaker_probe_active())return;
    if(!guest_writable(menu,0x23c))return;
#define MENU_APPEND(...) do { int c_=snprintf(mapmaker_probe_menu_text+used,sizeof mapmaker_probe_menu_text-used,__VA_ARGS__); \
    if(c_>0)used+=(size_t)c_<sizeof mapmaker_probe_menu_text-used?(size_t)c_:sizeof mapmaker_probe_menu_text-1-used; } while(0)
    MENU_APPEND(" menu=0x%08X caller=0x%08X descriptor=0x%08X +38=%08X +40=%08X +170=%08X state(+1B0)=%u +1C0=%08X"
        " selected(+18C)=%u highlighted(+198)=0x%08X input(+C4)=%08X +228=%08X rect=%.1f,%.1f,%.1f,%.1f\n",
        menu,caller,WORD_AT(menu+4),WORD_AT(menu+0x38),WORD_AT(menu+0x40),WORD_AT(menu+0x170),
        WORD_AT(menu+0x1b0),WORD_AT(menu+0x1c0),WORD_AT(menu+0x18c),WORD_AT(menu+0x198),
        WORD_AT(menu+0xc4),WORD_AT(menu+0x228),(double)FLOAT_AT(menu+0xe4),(double)FLOAT_AT(menu+0xe8),
        (double)FLOAT_AT(menu+0xec),(double)FLOAT_AT(menu+0xf0));
    item=WORD_AT(menu+0x178);
    for(n=0;item && n<MAPMAKER_PROBE_MENU_ITEMS && guest_writable(item,0x10c);n++,item=WORD_AT(item+4)) {
        MENU_APPEND("  item[%u]=0x%08X type=%u id=%u flags=%08X callback=0x%08X owner(+88)=0x%08X rect=%.1f,%.1f,%.1f,%.1f\n   raw:",
            n,item,WORD_AT(item),WORD_AT(item+0x14),WORD_AT(item+0x28),WORD_AT(item+0x38),WORD_AT(item+0x88),
            (double)FLOAT_AT(item+0x78),(double)FLOAT_AT(item+0x7c),(double)FLOAT_AT(item+0x80),(double)FLOAT_AT(item+0x84));
        for(i=0;i<16;i++)MENU_APPEND("%s%08X",i==8?"  ":" ",WORD_AT(item+i*4u));
        MENU_APPEND("\n");
    }
    MENU_APPEND("  items=%u%s\n",n,item?" (truncated)":"");
#undef MENU_APPEND
    hash=mapmaker_probe_text_hash(mapmaker_probe_menu_text,used);
    now=GetTickCount64();
    for(i=0;i<MAPMAKER_PROBE_MENU_SLOTS;i++)
        if(mapmaker_probe.menus[i].menu==menu) { slot=i;break; }
    if(slot==MAPMAKER_PROBE_MENU_SLOTS)
        for(i=0;i<MAPMAKER_PROBE_MENU_SLOTS;i++)
            if(!mapmaker_probe.menus[i].menu) { slot=i;break; }
    if(slot==MAPMAKER_PROBE_MENU_SLOTS)return;
    if(mapmaker_probe.menus[slot].menu==menu &&
       (mapmaker_probe.menus[slot].hash==hash || now-mapmaker_probe.menus[slot].last<MAPMAKER_PROBE_MENU_INTERVAL)) {
        mapmaker_probe.menus[slot].repeats++;return;
    }
    mapmaker_probe_write("[mapmaker-probe] t=%llu menu-layout repeats=%llu\n",now,mapmaker_probe.menus[slot].repeats);
    mapmaker_probe_write_text(mapmaker_probe_menu_text,used);
    mapmaker_probe.menus[slot].menu=menu;mapmaker_probe.menus[slot].hash=hash;
    mapmaker_probe.menus[slot].last=now;mapmaker_probe.menus[slot].repeats=0;
}
/* Flag 8: every left click the menu hook evaluates in MapMaker, in guest UI
 * coordinates, with the item (if any) the generic hit test chose. */
static void mapmaker_probe_click(uint32_t menu,float x,float y,uint32_t hit,unsigned actions)
{
    if(!(actions&2u) || !(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS) ||
       !mapmaker_probe_active())return;
    uint32_t player=guest_writable(0x007BA9E0u,4)?WORD_AT(0x007BA9E0u):4u;
    uint32_t block=0x007BA260u+(player<4?player:0)*0x1E0u;
    int known=player<4 && guest_writable(block,0x28) && guest_writable(menu,0x23c);
    /* After hit testing: which menu, where the game's own pointer is, and the
     * engine's selection, to tell PC hit failures from game-side ones. */
    mapmaker_probe_write("[mapmaker-probe] t=%llu click menu=0x%08X pointer=%.1f,%.1f hit=0x%08X type=%u"
        " descriptor=0x%08X game=%.1f,%.1f mode(+10)=%u selected=%d highlighted=0x%08X"
        " parent(+2C)=0x%08X style=0x%08X/%08X scroll(+5C,+6C,+70,+84,+194)=%.1f,%.1f,%.1f,%.1f,%d\n",
        GetTickCount64(),menu,(double)x,(double)y,hit,hit && guest_writable(hit,4)?WORD_AT(hit):0u,
        known?WORD_AT(menu+4):0u,known?(double)FLOAT_AT(block+0x20):0.,known?(double)FLOAT_AT(block+0x24):0.,
        known?WORD_AT(block+0x10):0u,known?(int)WORD_AT(menu+0x18c):-1,known?WORD_AT(menu+0x198):0u,
        known?WORD_AT(menu+0x2c):0u,known?WORD_AT(menu+0x1c):0u,
        known && guest_writable(WORD_AT(menu+0x1c),4)?WORD_AT(WORD_AT(menu+0x1c)):0u,
        known?(double)FLOAT_AT(menu+0x5c):0.,known?(double)FLOAT_AT(menu+0x6c):0.,
        known?(double)FLOAT_AT(menu+0x70):0.,known?(double)FLOAT_AT(menu+0x84):0.,
        known?(int)WORD_AT(menu+0x194):0);
}

#ifdef TSFP_MAPMAKER_PROBE_TEST
static void mapmaker_probe_reset(void)
{
    if(mapmaker_probe.file)fclose(mapmaker_probe.file);
    memset(&mapmaker_probe,0,sizeof mapmaker_probe);
}
#endif
