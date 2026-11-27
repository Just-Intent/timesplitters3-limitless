/* Asset-free contract for the MapMaker Disco tileset variant
 * (src/interface/pc_mapmaker_disco.inc with its pc_mapmaker_sky.inc marker and
 * pc_mapmaker_skymusic.inc defaults): the Map Settings Disco row (editor
 * +0x204 = 6) is stored as info flag bit 31 only on base Virtual and given
 * back on load; in play a Disco map's map start mounts
 * pak/arcademem/l_56_arm.pak index-only once per level and the texture hook
 * rewrites only fresh rows of the aliased Virtual ids whose Disco texture the
 * mounted index holds; no mount, a missing id, another tileset, no marker or
 * the switch off keep the stock ids; the variant's sky (59) and music (0xBA)
 * replace only the map's own -1; in the editor the menu close hands the base
 * tileset to the original and asks for a reload on a Virtual <-> Disco
 * switch, and the stream hook unloads and reloads the Virtual set (stock
 * unload, set marked not current) only when the loaded look differs. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00920000/4], g_eax, g_ebx, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp; }

void sub_0003DFA0(void) { g_ecx=6; }
void sub_00079D60(void) { g_ecx=1; }  /* distinct bodies: identical ones may be folded */
void sub_0007D4F0(void) { g_ecx=2; }
void sub_00078560(void) { g_ecx=3; }
void sub_00071800(void) { g_ecx=4; }
void sub_00062260(void) { g_ecx=5; }
void sub_000622D0(void) { g_ecx=7; }
void sub_000A5E40(void) { g_ecx=8; }
void sub_000631C0(void) { g_ecx=9; }
void sub_000717D0(void) { g_ecx=10; }
void sub_00071940(void) { g_ecx=11; }
void sub_00069640(void) { g_ecx=12; }
void sub_00045A70(void) { g_ecx=13; }
void sub_00061D80(void) { g_ecx=14; }
int recomp_audio_output_submit(uint32_t s,const uint8_t *p,uint32_t b,uint32_t r,uint32_t c,uint32_t bits,int32_t v) { (void)s;(void)p;(void)b;(void)r;(void)c;(void)bits;(void)v;return 0; }
int recomp_audio_output_position(uint32_t s,uint64_t *p,uint32_t *q) { (void)s;(void)p;(void)q;return 0; }
int recomp_audio_output_set_volume(uint32_t s,int32_t v) { (void)s;(void)v;return 0; }
void recomp_audio_output_reset_voice(uint32_t s) { (void)s; }
int recomp_audio_output_take_discontinuity(uint32_t s) { (void)s;return 0; }
struct PcMusicCodec;
int pc_music_codec_available(void) { return 0; }
struct PcMusicCodec *pc_music_codec_open(FILE *f,uint32_t *r,uint32_t *c) { (void)r;(void)c;if(f) fclose(f);return NULL; }
long pc_music_codec_read(struct PcMusicCodec *d,int16_t *o,unsigned n) { (void)d;(void)o;(void)n;return -1; }
int pc_music_codec_rewind(struct PcMusicCodec *d) { (void)d;return 0; }
void pc_music_codec_close(struct PcMusicCodec *d) { (void)d; }

/* ---- fake mount table: 0x00907000, 0x1C per slot, +8 count, +0x10 index ---- */
#define PAKTABLE 0x00907000u
#define INDEXES 0x00880000u
#define DISCO_PAK "pak/arcademem/l_56_arm.pak"
/* zlib CRC-32 of textures/<id>.xbt for the five Disco ids (python zlib.crc32) */
static const uint32_t disco_crc[5]={0x0EA7B674u,0x6343A6D9u,0x0F70412Du,0xD55180C7u,0x545733B5u};
static int index_skip=-1;                          /* this Disco texture is missing from the pak */
static char mounted[16][32];
static int mounted_count, mount_fails, mounts, unloads;
static uint32_t mount_args[3], unload_arg, unload_ret;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    int i;
    uint32_t arg[6]={0};
    va_start(a,n);
    for(i=0;i<n && i<6;i++) arg[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) {
        for(i=0;i<mounted_count;i++) if(!strcmp(mounted[i],(const char *)memory+arg[0])) return (uint32_t)i;
        return 0xFFFFFFFFu;
    }
    if(f==sub_000622D0) {
        uint32_t slot=(uint32_t)mounted_count,ix=INDEXES+slot*0x1000u,k,m=0;
        mounts++;memcpy(mount_args,arg,sizeof mount_args);
        if(mount_fails || mounted_count>=16) return 0xFFFFFFFFu;
        strncpy(mounted[mounted_count],(const char *)memory+arg[0],31);
        WORD_AT(0x006D7880u)++;
        for(k=0;k<40u;k++) { WORD_AT(ix+m*16u)=0x11110000u+k;m++; }   /* other entries */
        if(!strcmp(mounted[slot],DISCO_PAK))
            for(k=0;k<5u;k++) if((int)k!=index_skip) { WORD_AT(ix+m*16u)=disco_crc[k];m++; }
        WORD_AT(PAKTABLE+slot*0x1Cu+0x08u)=m;WORD_AT(PAKTABLE+slot*0x1Cu+0x10u)=ix;
        return (uint32_t)mounted_count++;
    }
    if(f==sub_00061D80) { unloads++;unload_arg=arg[0];return unload_ret; }
    return 0;
}
static uint32_t heap_next=0x00300000u;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p=heap_next;
    (void)alignment;heap_next+=(bytes+15u)&~15u;
    memset((char *)memory+p,0xCD,bytes);
    return p;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_sky.inc"
#include "pc_mapmaker_skymusic.inc"

static int failures, esp_errors;
#define ESP0 0x00600000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
}

/* ---- fake guest ---------------------------------------------------------- */
#define ED 0x00100000u          /* editor object [0x007844A8] */
#define C 0x00200000u           /* map container [0x00784014] */
#define INFO(c) ((c)+0x20u+8u)
#define LIST 0x00400000u        /* a model's texture list */
#define STREAMS 0x0052EC88u
#define NOW 0x007DE314u
#define PREV 0x007DE310u
#define FLAG 0x80000000u
static uint8_t B(uint32_t a) { return *((uint8_t *)memory+a); }
static int H(uint32_t a) { return (int16_t)(B(a)|B(a+1)<<8); }
static void setH(uint32_t a,int v) { *((uint8_t *)memory+a)=(uint8_t)v;*((uint8_t *)memory+a+1)=(uint8_t)((uint32_t)v>>8); }
static void make_container(uint32_t c,uint32_t tileset,uint32_t flags,int music,int sky)
{
    memset((char *)memory+c,0,0x1000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77669u;WORD_AT(c+12)=0x10u;
    WORD_AT(c+0x20)=0x00C77668u;WORD_AT(c+0x24)=0x3DCu;
    WORD_AT(INFO(c)+0x08)=0x5A5Au|tileset<<16;
    setH(INFO(c)+0x0E,music);setH(INFO(c)+0x10,sky);
    WORD_AT(INFO(c)+0x244)=flags;
    WORD_AT(INFO(c)+0x3DC)=0x00C7766Au;
}
/* A fresh list: the five aliased ids, a mask kept, a registered row, an embedded row. */
static const uint32_t fresh[][2]={
    {0xE871CCu,0xE871CCu},{0x25AE6Eu,0x25AE6Eu},{0xB2A030u,0xB2A030u},{0x294554u,0x294554u},
    {0x79B187u,0x79B187u},{0x666443u,0x666443u},{0x0692u,0x294554u},{0x00401230u,0xE871CCu},{0x5A89BCu,0x5A89BCu},
    {0x25AE6Eu,0x00ABCDEFu},                       /* word 0 equal to an aliased id, word 1 not: not fresh */
};
#define FRESH ((uint32_t)(sizeof fresh/sizeof fresh[0]))
static void make_list(void)
{
    uint32_t k;
    memset((char *)memory+LIST,0,0x200);
    for(k=0;k<FRESH;k++) { WORD_AT(LIST+k*16u)=fresh[k][0];WORD_AT(LIST+k*16u+4u)=fresh[k][1]; }
    WORD_AT(LIST+FRESH*16u)=0xFFFFFFFFu;
    WORD_AT(LIST+FRESH*16u+16u)=0xE871CCu;WORD_AT(LIST+FRESH*16u+20u)=0xE871CCu;   /* past the end */
}
static uint32_t W0(uint32_t k) { return WORD_AT(LIST+k*16u); }
static uint32_t W1(uint32_t k) { return WORD_AT(LIST+k*16u+4u); }
static int list_stock(void)
{
    uint32_t k;
    for(k=0;k<FRESH;k++) if(W0(k)!=fresh[k][0] || W1(k)!=fresh[k][1]) return 0;
    return WORD_AT(LIST+FRESH*16u+16u)==0xE871CCu;
}
static int originals, seen_sky, seen_tileset;
static uint32_t eax_at_original, now_at_original;
static void plain_original(void) { originals++;eax_at_original=g_eax;g_esp+=4u; }
static void level_original(void) { seen_sky=H(INFO(WORD_AT(0x00784014u))+0x10);originals++;g_esp+=4u; }
static uint32_t rebuild_flags=0x00050000u;
static void rebuild_original(void)
{
    uint32_t c=WORD_AT(g_esp+4);
    WORD_AT(INFO(c)+0x08)=0x5A5Au|WORD_AT(0x00784020u)<<16;
    setH(INFO(c)+0x0E,(int)WORD_AT(ED+0x210));setH(INFO(c)+0x10,(int)WORD_AT(ED+0x220));
    WORD_AT(INFO(c)+0x244)=rebuild_flags;
    originals++;g_esp+=4u;
}
static void load_original(void)                 /* sub_00301A90: +0x204 = [0x00784020] = header tileset */
{
    uint32_t c=WORD_AT(g_esp+4);
    WORD_AT(0x00784020u)=WORD_AT(INFO(c)+0x08)>>16;WORD_AT(ED+0x204)=WORD_AT(0x00784020u);
    WORD_AT(ED+0x210)=(uint32_t)H(INFO(c)+0x0E);WORD_AT(ED+0x220)=(uint32_t)H(INFO(c)+0x10);
    originals++;g_esp+=4u;
}
static void close_original(void)                /* sub_002FC0A0 */
{
    seen_tileset=(int)WORD_AT(ED+0x204);
    if(WORD_AT(ED+0x204)!=WORD_AT(0x00784020u)) { WORD_AT(0x00784020u)=WORD_AT(ED+0x204);WORD_AT(ED+0x238)|=0x12u; }
    originals++;g_esp+=4u;
}
static void stream_original(void)               /* sub_00338180 -> sub_00061E90 */
{
    uint32_t name=WORD_AT(STREAMS+WORD_AT(g_esp+4)*0x18u);
    now_at_original=WORD_AT(NOW);
    if(WORD_AT(NOW)==name) g_eax=0;
    else { WORD_AT(PREV)=WORD_AT(NOW);WORD_AT(NOW)=name;g_eax=1; }
    originals++;g_esp+=4u;
}
static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t arg0,uint32_t arg1,uint32_t eax)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=arg0;WORD_AT(ESP0+8)=arg1;
    g_eax=eax;originals=0;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    if(originals!=1) { fprintf(stderr,"original ran %d times\n",originals);failures++; }
    g_esp=ESP0;
}
static void new_level(void) { hook(pc_mapmaker_skymusic_mount,plain_original,0x001CDB8Au,0,0,0); }
static void map_start(void) { hook(pc_mapmaker_skymusic_level,level_original,0x0006C963u,0,0,0); }
static void textures(void) { hook(pc_mapmaker_disco_textures,plain_original,0x0009628Fu+5u,LIST,0x00401000u,0); }
static uint32_t music(uint32_t eax) { hook(pc_mapmaker_skymusic_music,plain_original,0x00045FBCu,0x1A0u,0,eax);return eax_at_original; }
static void compile(void) { hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,C,3u,0); }
static void load(void) { hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,C,2u,0); }
static void menu_close(void) { hook(pc_mapmaker_disco_menu_close,close_original,0x00313316u,0,0,0); }
static void stream(uint32_t t) { hook(pc_mapmaker_disco_stream,stream_original,0x002FC26Au,t,0,0); }
static void frame(void) { hook(pc_mapmaker_skymusic_weather_frame,plain_original,0x001CE564u,0,0,0); }
static void unmount_all(void) { mounted_count=0;WORD_AT(0x006D7880u)=12u;mounts=0; }
/* A level of a map in play: new level, then the map start. */
static void play(uint32_t tileset,uint32_t flags,int music_index,int sky)
{
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,tileset,flags,music_index,sky);unmount_all();
    new_level();map_start();
}

int main(void)
{
    uint32_t k;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x00784014u)=C;
    for(k=0;k<6u;k++) WORD_AT(STREAMS+k*0x18u)=0x0049F000u+k*0x40u;   /* stream set names */
    WORD_AT(0x00791D4Cu)=0x00700000u;
    for(k=0;k<0xA00u;k++) setH(0x008CA000u+k*2u,(int)k);
    for(k=0;k<68u;k++) WORD_AT(0x005149A8u+k*0x34u+4u)=0x700u+k;

    /* ---- switch off: nothing mounted, nothing rewritten, no default, no row stored ---- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);set_env("TSFP_MAPMAKER_POOLS",NULL);
    play(5u,FLAG,-1,-1);
    CHECK(mounts==0 && seen_sky==-1);
    make_list();textures();CHECK(list_stock());
    CHECK(music(0xFFFFFFFFu)==0xFFFFFFFFu);
    WORD_AT(0x0079094Cu)=102u;WORD_AT(0x00784020u)=5u;WORD_AT(ED+0x204)=6u;
    make_container(C,5u,0,0,0);compile();
    CHECK((WORD_AT(INFO(C)+0x244)&FLAG)==0u);

    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- editor: the Disco row is stored only on base Virtual and comes back ---- */
    WORD_AT(0x0079094Cu)=102u;
    WORD_AT(0x00784020u)=5u;WORD_AT(ED+0x204)=6u;compile();
    CHECK(WORD_AT(INFO(C)+0x244)==(0x00050000u|FLAG) && (WORD_AT(INFO(C)+0x08)>>16)==5u);
    rebuild_flags=0x00050000u|FLAG;                     /* a stale bit is cleared for plain Virtual */
    WORD_AT(ED+0x204)=5u;compile();CHECK(WORD_AT(INFO(C)+0x244)==0x00050000u);
    WORD_AT(0x00784020u)=2u;WORD_AT(ED+0x204)=6u;compile();   /* not Virtual: never stored */
    CHECK(WORD_AT(INFO(C)+0x244)==0x00050000u);
    rebuild_flags=0x00050000u;
    make_container(C,5u,FLAG,-1,-1);load();
    CHECK(WORD_AT(ED+0x204)==6u && WORD_AT(0x00784020u)==5u);
    make_container(C,5u,0,-1,-1);load();CHECK(WORD_AT(ED+0x204)==5u);
    make_container(C,2u,FLAG,-1,-1);load();CHECK(WORD_AT(ED+0x204)==2u);   /* marker without its base */
    make_container(C,5u,FLAG,-1,-1);load();compile();
    CHECK(WORD_AT(INFO(C)+0x244)==(0x00050000u|FLAG));  /* round trip */

    /* ---- play: a Disco map mounts its textures pak and rewrites fresh rows ---- */
    play(5u,FLAG,-1,-1);
    CHECK(mounts==1 && mounted_count==1 && !strcmp(mounted[0],DISCO_PAK));
    CHECK(mount_args[1]==0u && mount_args[2]==1u);      /* index-only, as chr.pak */
    CHECK(seen_sky==59 && H(INFO(C)+0x10)==-1);          /* default sky only while the map start runs */
    make_list();textures();
    CHECK(W0(0)==0x7DDE70u && W1(0)==0x7DDE70u && W0(1)==0x0D9139u && W1(1)==0x0D9139u);
    CHECK(W0(3)==0x7B0662u && W1(3)==0x7B0662u && W0(4)==0x3C2652u && W0(5)==0xF05B7Cu && W1(5)==0xF05B7Cu);
    CHECK(W0(2)==0xB2A030u && W0(8)==0x5A89BCu);        /* masks and the light bar are kept */
    CHECK(W0(6)==0x0692u && W1(6)==0x294554u);          /* registered row: untouched */
    CHECK(W0(7)==0x00401230u && W1(7)==0xE871CCu);      /* embedded row: untouched */
    CHECK(W0(9)==0x25AE6Eu && W1(9)==0x00ABCDEFu);      /* words differ: untouched */
    CHECK(WORD_AT(LIST+FRESH*16u+16u)==0xE871CCu);      /* the list ends at -1 */
    CHECK(dsc.entries==5u && dsc.lists==1u);
    textures();CHECK(dsc.entries==5u && dsc.lists==1u); /* already rewritten: nothing again */
    frame();CHECK(dsc.frame_logged==1);
    k=dsc.logged;map_start();CHECK(mounts==1 && dsc.logged==k);   /* once per level */
    CHECK(music(0xFFFFFFFFu)==0xBAu);                   /* default track for the map's -1 */
    CHECK(music(0x1Au)==0x1Au);                         /* a stock choice is kept */
    make_container(C,5u,FLAG|(3u<<24),-1,-1);           /* an extra track row wins */
    CHECK(music(0xFFFFFFFFu)==smx_music[2].id);
    make_container(C,5u,FLAG,-1,-1);
    hook(pc_mapmaker_skymusic_music,plain_original,0x00045F00u,0x1A0u,0,0xFFFFFFFFu);   /* another caller */
    CHECK(eax_at_original==0xFFFFFFFFu);
    play(5u,FLAG,-1,61);CHECK(seen_sky==61);            /* the map's own sky is kept */
    play(5u,FLAG|(1u<<19),-1,-1);                       /* an extra sky row wins over the default */
    CHECK(seen_sky!=59);
    CHECK(mapmaker_sky_index(INFO(C))==5);
    make_container(C,5u,FLAG,-1,-1);CHECK(mapmaker_sky_index(INFO(C))==59);   /* cloud setup's sky */
    make_container(C,5u,0,-1,-1);CHECK(mapmaker_sky_index(INFO(C))!=59);

    /* ---- play: fallbacks keep the stock ids ---- */
    index_skip=2;play(5u,FLAG,-1,-1);                   /* red plush missing: framed square kept */
    make_list();textures();
    CHECK(W0(0)==0x7DDE70u && W0(3)==0x294554u && W1(3)==0x294554u && W0(5)==0xF05B7Cu && dsc.live_n==4u);
    index_skip=-1;
    mount_fails=1;play(5u,FLAG,-1,-1);                  /* no pak: plain Virtual */
    CHECK(mounts==1 && dsc.play_ready==0);
    make_list();textures();CHECK(list_stock());
    CHECK(music(0xFFFFFFFFu)==0xBAu);                   /* defaults are the map's look, not the pak's */
    mount_fails=0;
    WORD_AT(0x006D7880u)=57u;mounted_count=0;mounts=0;make_container(C,5u,FLAG,-1,-1);
    new_level();map_start();                            /* table near full: not mounted */
    CHECK(mounts==0 && dsc.play_ready==0);
    play(5u,0,-1,-1);                                   /* plain Virtual */
    CHECK(mounts==0 && seen_sky==-1);
    make_list();textures();CHECK(list_stock());
    CHECK(music(0xFFFFFFFFu)==0xFFFFFFFFu);
    play(2u,FLAG,-1,-1);                                /* marker on another tileset: ignored */
    CHECK(mounts==0 && seen_sky==-1);
    make_list();textures();CHECK(list_stock());
    play(5u,FLAG,-1,-1);
    WORD_AT(0x0079094Cu)=101u;make_list();textures();CHECK(list_stock());   /* other scene */
    WORD_AT(0x0079094Cu)=100u;new_level();make_list();textures();CHECK(list_stock());   /* next level: until its map start */

    /* ---- editor: menu close and the Virtual stream ---- */
    WORD_AT(0x0079094Cu)=102u;unmount_all();
    WORD_AT(NOW)=0xFFFFFFFFu;dsc.stream_variant=0;dsc.editor_ready=0;
    WORD_AT(0x00784020u)=1u;WORD_AT(ED+0x204)=6u;WORD_AT(ED+0x238)=0;
    menu_close();                                       /* Lab -> Disco: the original sees Virtual */
    CHECK(seen_tileset==5 && WORD_AT(0x00784020u)==5u && WORD_AT(ED+0x204)==6u && WORD_AT(ED+0x238)==0x12u);
    stream(5u);                                         /* a new set: loaded as Disco */
    CHECK(mounts==1 && unloads==0 && WORD_AT(NOW)==WORD_AT(STREAMS+5u*0x18u) && dsc.stream_variant==1 && dsc.editor_ready==1);
    make_list();textures();CHECK(W0(0)==0x7DDE70u && W0(4)==0x3C2652u);
    WORD_AT(ED+0x238)=0;menu_close();                   /* Disco -> Disco: nothing */
    CHECK(WORD_AT(ED+0x238)==0u && WORD_AT(ED+0x204)==6u);
    WORD_AT(ED+0x204)=5u;menu_close();                  /* Disco -> Virtual: same tileset, reload asked */
    CHECK(seen_tileset==5 && WORD_AT(ED+0x204)==5u && WORD_AT(ED+0x238)==0x12u);
    WORD_AT(PREV)=0x1234u;
    stream(5u);                                         /* loaded as Disco: unloaded and read again */
    CHECK(unloads==1 && unload_arg==WORD_AT(STREAMS+5u*0x18u) && now_at_original==0xFFFFFFFFu);
    CHECK(WORD_AT(NOW)==WORD_AT(STREAMS+5u*0x18u) && WORD_AT(PREV)==WORD_AT(STREAMS+5u*0x18u));
    CHECK(dsc.stream_variant==0 && dsc.editor_ready==0);
    make_list();textures();CHECK(list_stock());
    stream(5u);CHECK(unloads==1 && dsc.stream_variant==0);   /* same look: no reload */
    WORD_AT(ED+0x204)=6u;WORD_AT(ED+0x238)=0;menu_close();CHECK(WORD_AT(ED+0x238)==0x12u);
    unload_ret=DSC_STREAM_BUSY;stream(5u);              /* still loading: nothing is forced */
    CHECK(unloads==2 && now_at_original==WORD_AT(STREAMS+5u*0x18u) && dsc.stream_variant==0 && dsc.editor_ready==0);
    unload_ret=0;stream(5u);
    CHECK(unloads==3 && dsc.stream_variant==1 && dsc.editor_ready==1);
    WORD_AT(0x00784020u)=1u;make_list();textures();CHECK(list_stock());   /* another tileset current */
    WORD_AT(0x00784020u)=5u;make_list();textures();CHECK(W0(0)==0x7DDE70u);
    stream(1u);                                         /* Lab streams: no alias, no unload */
    CHECK(unloads==3 && dsc.stream_variant==0 && dsc.editor_ready==0);
    WORD_AT(ED+0x204)=6u;mount_fails=1;unmount_all();WORD_AT(NOW)=0xFFFFFFFFu;
    stream(5u);                                         /* pak missing in the editor: plain Virtual */
    CHECK(dsc.stream_variant==0 && dsc.editor_ready==0);
    mount_fails=0;
    WORD_AT(0x0079094Cu)=100u;WORD_AT(ED+0x204)=6u;WORD_AT(ED+0x238)=0;WORD_AT(0x00784020u)=5u;
    menu_close();CHECK(seen_tileset==6);                /* not the editor: untouched */
    WORD_AT(NOW)=0xFFFFFFFFu;unloads=0;stream(5u);CHECK(unloads==0 && dsc.stream_variant==0);

    if(failures || esp_errors) return 1;
    printf("mapmaker disco ok\n");
    return 0;
}
