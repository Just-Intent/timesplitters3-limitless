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
 *   4  also bars drawn outside the meter scope while scene 100 is active
 *   8  also, while scene 100 is active, each menu laid out by sub_00073750
 *      (header, item list with type/flags/callback/rectangle and 64 raw
 *      bytes) and every left click's guest-UI pointer position and hit item
 * Output: TSFP_MAPMAKER_PROBE_LOG (binary, truncated per process), else stderr.
 * Records are emitted when their content changes, at most four per second,
 * with the number of unchanged repeats; the file is capped at 4 MiB.
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
#define MAPMAKER_PROBE_ALL 15u
#define MAPMAKER_PROBE_MENU_SLOTS 16u
#define MAPMAKER_PROBE_MENU_ITEMS 64u
#define MAPMAKER_PROBE_MENU_INTERVAL 1000u
#define MAPMAKER_PROBE_SCENE 100
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
    if((flags&MAPMAKER_PROBE_OTHER) && mapmaker_probe_scene()==MAPMAKER_PROBE_SCENE &&
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
        mapmaker_probe_write("[mapmaker-probe] t=%llu other-bar scene=100%s repeats=%llu\n",
            now,text.text,slot->repeats);
        slot->ret=ret;slot->last=now;slot->repeats=0;
        snprintf(slot->key,sizeof slot->key,"%s",text.text);
    }
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
    if(!(mapmaker_probe_flags()&MAPMAKER_PROBE_MENUS) || mapmaker_probe_scene()!=MAPMAKER_PROBE_SCENE)return;
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
       mapmaker_probe_scene()!=MAPMAKER_PROBE_SCENE)return;
    mapmaker_probe_write("[mapmaker-probe] t=%llu click menu=0x%08X pointer=%.1f,%.1f hit=0x%08X type=%u\n",
        GetTickCount64(),menu,(double)x,(double)y,hit,hit && guest_writable(hit,4)?WORD_AT(hit):0u);
}

#ifdef TSFP_MAPMAKER_PROBE_TEST
static void mapmaker_probe_reset(void)
{
    if(mapmaker_probe.file)fclose(mapmaker_probe.file);
    memset(&mapmaker_probe,0,sizeof mapmaker_probe);
}
#endif
