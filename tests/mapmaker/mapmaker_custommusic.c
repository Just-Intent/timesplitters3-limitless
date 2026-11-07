/* Asset-free contract for src/interface/pc_mapmaker_custommusic.inc through
 * the pc_mapmaker_skymusic.inc hooks that call it: imported rows after the
 * extra stock rows; the rebuild keeps the stock -1 and appends the name
 * chunk before the terminator (used and the return value follow); the
 * editor load turns it back, a missing file stays the tileset music; play
 * starts the host track only for the map's own -1 or the Arcade choice with
 * the guest's row 0, makes eax 0, honours "music off", and a jingle channel
 * or a scene change ends it; Arcade rows, show and store keep the stock
 * fields at row 0 and the choice in pc-music.ini. argv[1] holds Alpha.wav
 * and beta_song.wav (written by tests/mapmaker/test_mapmaker_custommusic.py). */
#define PC_CUSTOM_MUSIC_TEST 1
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
void sub_00079D60(void) { g_ecx=1; }
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
#define CALLS 128
static struct { void (*f)(void); int n; uint32_t a[6]; } calls[CALLS];
static int call_count;
static uint32_t music_off, item_addr, profile_addr;
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    int i;
    uint32_t arg[6]={0};
    va_start(a,n);
    for(i=0;i<n && i<6;i++) arg[i]=va_arg(a,uint32_t);
    va_end(a);
    if(call_count<CALLS) { calls[call_count].f=f;calls[call_count].n=n;memcpy(calls[call_count].a,arg,sizeof arg); }
    call_count++;
    if(f==sub_00062260) return 0xFFFFFFFFu;
    if(f==sub_00045A70) return music_off;
    if(f==sub_00071940) return arg[1]==0x0Eu?item_addr:0u;
    if(f==sub_00069640) return profile_addr;
    return 0;
}
static uint32_t heap_next=0x00300000u;
static int heap_calls;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment)
{
    uint32_t p;
    (void)alignment;heap_calls++;
    p=heap_next;heap_next+=(bytes+15u)&~15u;
    memset((char *)memory+p,0xCD,bytes);
    return p;
}
/* host output of the test player */
static unsigned submits, resets;
int pc_music_test_submit(const uint8_t *pcm,uint32_t bytes,uint32_t rate,uint32_t channels,int32_t volume)
{ (void)pcm;(void)bytes;(void)rate;(void)channels;(void)volume;submits++;return 1; }
int pc_music_test_position(uint64_t *played,uint32_t *queued) { *played=0;*queued=0;return 1; }
static int32_t volume_now=1;
void pc_music_test_volume(int32_t volume) { volume_now=volume; }
void pc_music_test_reset(void) { resets++; }
/* host decoder adapter (pc_music_codec.c): none here, so .mp3 is not listed */
struct PcMusicCodec;
int pc_music_codec_available(void) { return 0; }
struct PcMusicCodec *pc_music_codec_open(FILE *f,uint32_t *r,uint32_t *c) { (void)r;(void)c;if(f) fclose(f);return NULL; }
long pc_music_codec_read(struct PcMusicCodec *d,int16_t *o,unsigned n) { (void)d;(void)o;(void)n;return -1; }
int pc_music_codec_rewind(struct PcMusicCodec *d) { (void)d;return 0; }
void pc_music_codec_close(struct PcMusicCodec *d) { (void)d; }

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

#define ED 0x00100000u
#define C 0x00200000u
#define INFO(c) ((c)+0x20u+8u)
#define TERM(c) (INFO(c)+0x3DCu)
#define USED (0x3DCu+0x20u+8u+8u)       /* through the terminator */
static uint8_t B(uint32_t a) { return *((uint8_t *)memory+a); }
static int H(uint32_t a) { return (int16_t)(B(a)|B(a+1)<<8); }
static void setH(uint32_t a,int v) { *((uint8_t *)memory+a)=(uint8_t)v;*((uint8_t *)memory+a+1)=(uint8_t)((uint32_t)v>>8); }
static const char *text(uint32_t a) { return (const char *)memory+a; }
static void make_container(uint32_t c,int music)
{
    memset((char *)memory+c,0,0x1000);
    WORD_AT(c)=0x00C77667u;WORD_AT(c+4)=0x12Du;
    WORD_AT(c+8)=0x00C77669u;WORD_AT(c+12)=0x10u;
    WORD_AT(c+0x20)=0x00C77668u;WORD_AT(c+0x24)=0x3DCu;
    setH(INFO(c)+0x0E,music);setH(INFO(c)+0x10,-1);
    WORD_AT(INFO(c)+0x244)=0x00050000u;
    WORD_AT(INFO(c)+0x248)=USED;
    WORD_AT(TERM(c))=0x00C7766Au;WORD_AT(TERM(c)+4)=TERM(c)-c;
}
static void put_chunk(uint32_t c,const char *name)   /* as the rebuild writes it */
{
    uint32_t d=TERM(c)+8u;
    WORD_AT(TERM(c))=0x00C776F7u;WORD_AT(TERM(c)+4)=0x44u;
    memset((char *)memory+d,0,0x44);
    *((uint8_t *)memory+d)=1;*((uint8_t *)memory+d+2)=(uint8_t)strlen(name);
    memcpy((char *)memory+d+4,name,strlen(name));
    WORD_AT(d+0x44)=0x00C7766Au;WORD_AT(d+0x48)=d+0x44-c;
    WORD_AT(INFO(c)+0x248)=d+0x44-c+8u;
}
static int originals, calls_at_original;
static uint32_t eax_at_original;
static void plain_original(void) { originals++;calls_at_original=call_count;eax_at_original=g_eax;g_esp+=4u; }
static void rebuild_original(void)
{
    uint32_t c=WORD_AT(g_esp+4);
    setH(INFO(c)+0x0E,(int)WORD_AT(ED+0x210));
    WORD_AT(INFO(c)+0x244)=0x00050000u;
    g_eax=USED;
    originals++;g_esp+=4u;
}
static void load_original(void)
{
    uint32_t c=WORD_AT(g_esp+4);
    WORD_AT(ED+0x210)=(uint32_t)H(INFO(c)+0x0E);
    originals++;g_esp+=4u;
}
static uint32_t stored_by_stock;                 /* sub_002C7430: item 0xE -> the three fields */
static void store_original(void)
{
    uint32_t v=WORD_AT(item_addr+0xC8u);
    WORD_AT(0x007613D4u)=v;WORD_AT(0x007613D0u)=v;WORD_AT(profile_addr+0x114u)=v;stored_by_stock=v;
    originals++;g_esp+=4u;
}
static void show_original(void) { WORD_AT(item_addr+0xC8u)=WORD_AT(0x007613D4u);originals++;g_esp+=4u; }
static void hook(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t eax)
{
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4)=a0;WORD_AT(ESP0+8)=a1;WORD_AT(ESP0+12)=a2;
    g_eax=eax;call_count=0;originals=0;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"esp %+d after a hook\n",(int)(g_esp-ESP0));esp_errors++; }
    if(originals!=1) { fprintf(stderr,"original ran %d times\n",originals);failures++; }
    g_esp=ESP0;
}
static uint32_t music(uint32_t ret,uint32_t eax) { hook(pc_mapmaker_skymusic_music,plain_original,ret,0x1A0u,0,0,eax);return eax_at_original; }
static int imported_rows(void)
{
    int k,n=0;
    for(k=0;k<call_count && k<CALLS;k++) if(calls[k].f==sub_00071800 && calls[k].a[1]>=0x100u) n++;
    return n;
}

int main(int argc,char **argv)
{
    char ini[4200],name[80];
    int k;
    if(argc<3) return 2;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x007844A8u)=ED;WORD_AT(0x00784014u)=C;
    item_addr=0x00500000u;profile_addr=0x00510000u;
    set_env("TSFP_MUSIC_DIR",argv[1]);
    snprintf(ini,sizeof ini,"%s/pc-display.ini",argv[2]);
    set_env("TSFP_DISPLAY_CONFIG",ini);

    /* ---- editor rows ---- */
    set_env("TSFP_MAPMAKER_LIMITS","1");
    WORD_AT(0x0079094Cu)=102u;
    hook(pc_mapmaker_skymusic_rows,plain_original,0x00313768u,ED+0x210u,0,0,0x55u);
    CHECK(call_count==30 && calls_at_original==30 && imported_rows()==2);
    CHECK(calls[28].a[1]==0x100u && calls[29].a[1]==0x101u && calls[27].a[1]==85u);
    CHECK(!strcmp(text(calls[28].a[0]),"Imported: Alpha") && !strcmp(text(calls[29].a[0]),"Imported: beta song"));
    hook(pc_mapmaker_skymusic_rows,plain_original,0x00313843u,ED+0x220u,0,0,0);    /* sky list: none */
    CHECK(imported_rows()==0);

    /* ---- rebuild ---- */
    make_container(C,-1);WORD_AT(ED+0x210)=0x101u;
    hook(pc_mapmaker_skymusic_compile,rebuild_original,0x0032C6FFu,C,3u,0,0);
    CHECK(H(INFO(C)+0x0E)==-1 && (WORD_AT(INFO(C)+0x244)&0x1F000000u)==0u);
    CHECK(WORD_AT(TERM(C))==0x00C776F7u && WORD_AT(TERM(C)+4)==0x44u && B(TERM(C)+8)==1u && B(TERM(C)+10)==13u);
    CHECK(!strcmp(text(TERM(C)+12),"beta_song.wav"));
    CHECK(WORD_AT(TERM(C)+8+0x44)==0x00C7766Au && WORD_AT(INFO(C)+0x248)==USED+0x4Cu && g_eax==USED+0x4Cu);
    CHECK(cmu_chunk_name(C,name) && !strcmp(name,"beta_song.wav"));
    make_container(C,-1);WORD_AT(ED+0x210)=5u;                      /* a stock track: no chunk */
    hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,C,3u,0,0);
    CHECK(WORD_AT(TERM(C))==0x00C7766Au && WORD_AT(INFO(C)+0x248)==USED && H(INFO(C)+0x0E)==5);
    make_container(C,-1);WORD_AT(ED+0x210)=0x140u;                  /* not a list entry */
    hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,C,3u,0,0);
    CHECK(WORD_AT(TERM(C))==0x00C7766Au && H(INFO(C)+0x0E)==-1);
    make_container(C,-1);WORD_AT(INFO(C)+0x248)=USED+16u;WORD_AT(ED+0x210)=0x100u;   /* used disagrees */
    hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,C,3u,0,0);
    CHECK(WORD_AT(TERM(C))==0x00C7766Au);
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    make_container(C,-1);WORD_AT(ED+0x210)=0x100u;                  /* switch off: no chunk */
    hook(pc_mapmaker_skymusic_compile,rebuild_original,0x00313B01u,C,3u,0,0);
    CHECK(WORD_AT(TERM(C))==0x00C7766Au && H(INFO(C)+0x0E)==-1);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- editor load ---- */
    make_container(C,-1);put_chunk(C,"Alpha.wav");
    hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,C,2u,0,0);
    CHECK(WORD_AT(ED+0x210)==0x100u);
    make_container(C,-1);put_chunk(C,"gone.wav");
    hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,C,2u,0,0);
    CHECK(WORD_AT(ED+0x210)==0xFFFFFFFFu);
    make_container(C,-1);put_chunk(C,"ALPHA.WAV");                   /* names match without case */
    hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,C,2u,0,0);
    CHECK(WORD_AT(ED+0x210)==0x100u);
    make_container(C,-1);put_chunk(C,"Alpha.wav");*((uint8_t *)memory+TERM(C)+8)=2;   /* other version */
    hook(pc_mapmaker_skymusic_load,load_original,0x0032B4A4u,C,2u,0,0);
    CHECK(WORD_AT(ED+0x210)==0xFFFFFFFFu);

    /* an emptied folder offers nothing (the entries stay listed, missing) */
    set_env("TSFP_MUSIC_DIR",argv[2]);
    hook(pc_mapmaker_skymusic_rows,plain_original,0x00313768u,ED+0x210u,0,0,0x55u);
    CHECK(call_count==28 && imported_rows()==0);
    set_env("TSFP_MUSIC_DIR",argv[1]);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- MapMaker play ---- */
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,-1);put_chunk(C,"beta_song.wav");
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0u && pc_music_playing());
    WORD_AT(0x005236BCu)=8u;pc_music_pump(10);
    CHECK(volume_now==pc_music_db(0.5f*0.65f) && submits>0u);
    WORD_AT(0x0064BDC0u)=1u;pc_music_pump(20);
    CHECK(volume_now==-10000 && pc_music_playing());
    WORD_AT(0x0064BDC0u)=0u;
    hook(pc_custom_music_channel,plain_original,0x0003D000u,0u,0,0,0);       /* channel 0: keeps playing */
    CHECK(pc_music_playing());
    hook(pc_custom_music_channel,plain_original,0x0003D000u,1u,0,0,0);       /* a jingle ends it */
    CHECK(!pc_music_playing());
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0u && pc_music_playing());
    WORD_AT(0x0079094Cu)=0x53u;pc_music_pump(30);                            /* back to the menus */
    CHECK(!pc_music_playing());
    WORD_AT(0x0079094Cu)=100u;
    /* a setup's stock Arcade choice (eax 7): the map's own imported track still plays */
    CHECK(music(0x00045FBCu,7u)==0u && pc_music_playing());
    setH(INFO(C)+0x0E,4);                                                    /* the map's own stock row */
    CHECK(music(0x00045FBCu,4u)==4u && !pc_music_playing());
    setH(INFO(C)+0x0E,-1);
    WORD_AT(0x0079094Cu)=101u;                                               /* not the play scene */
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());
    WORD_AT(0x0079094Cu)=100u;
    /* every list build logs; a play decision after many of them is still logged */
    WORD_AT(0x0079094Cu)=102u;
    for(k=0;k<60;k++) hook(pc_mapmaker_skymusic_rows,plain_original,0x00313768u,ED+0x210u,0,0,0x55u);
    WORD_AT(0x0079094Cu)=100u;
    make_container(C,-1);
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());   /* logs "names no imported track" */
    make_container(C,-1);put_chunk(C,"beta_song.wav");
    CHECK(music(0x00045F2Cu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());   /* another caller */
    music_off=1;
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());
    music_off=0;
    make_container(C,-1);put_chunk(C,"gone.wav");
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    make_container(C,-1);put_chunk(C,"beta_song.wav");
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());
    set_env("TSFP_MAPMAKER_LIMITS","1");
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0u && pc_music_playing());
    CHECK(music(0x00045F2Cu,0x20u)==0x20u && !pc_music_playing());         /* any other level music stops it */

    /* ---- Arcade ---- */
    WORD_AT(0x0079094Cu)=0x53u;
    g_ebx=1u;hook(pc_custom_music_arcade_rows,plain_original,0x002C8047u,0,0,0,0);
    CHECK(call_count==0);                                                   /* the row loop did not run */
    g_ebx=0u;hook(pc_custom_music_arcade_rows,plain_original,0x002C8070u,0,0,0,0);
    CHECK(call_count==0);                                                   /* another caller */
    hook(pc_custom_music_arcade_rows,plain_original,0x002C8047u,0,0,0,0);
    CHECK(call_count==2 && calls_at_original==2 && calls[0].f==sub_000717D0 && !strcmp(text(calls[1].a[0]),"Imported: beta song"));
    WORD_AT(item_addr+0xC8u)=59u;                                           /* the second imported row */
    hook(pc_custom_music_arcade_store,store_original,0x002C76CAu,0x00400000u,1u,profile_addr,0);
    CHECK(stored_by_stock==59u && WORD_AT(0x007613D0u)==0u && WORD_AT(0x007613D4u)==0u && WORD_AT(profile_addr+0x114u)==0u);
    CHECK(pc_music_arcade_load(name,sizeof name) && !strcmp(name,"beta_song.wav"));
    hook(pc_custom_music_arcade_show,show_original,0x002EFC33u,0x00400000u,profile_addr,0,0);
    CHECK(WORD_AT(item_addr+0xC8u)==59u);
    /* play: stock Arcade with row 0, then the MapMaker caller with the profile at row 0 */
    WORD_AT(0x0079094Cu)=12u;WORD_AT(0x007613D0u)=0u;
    CHECK(music(0x00045F4Bu,0xFFFFFFFFu)==0u && pc_music_playing());
    WORD_AT(0x007613D0u)=4u;                                                 /* a System Link host's stock pick */
    CHECK(music(0x00045F4Bu,0x30u)==0x30u && !pc_music_playing());
    WORD_AT(0x0079094Cu)=100u;WORD_AT(0x00761404u)=1u;*((uint8_t *)memory+0x007DE455u)=0;
    make_container(C,-1);
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0u && pc_music_playing());
    /* the Arcade choice's file is gone: the map's own track plays instead */
    pc_music_arcade_save("gone.wav");cmu.arcade_loaded=0;
    make_container(C,-1);put_chunk(C,"Alpha.wav");
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0u && pc_music_playing() && !strcmp(pc_music_error(),""));
    pc_music_arcade_save("beta_song.wav");cmu.arcade_loaded=0;
    make_container(C,-1);
    CHECK(music(0x00045F2Cu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());   /* the story caller */
    *((uint8_t *)memory+0x007DE455u)=0x0Au;                                  /* story: no Arcade choice */
    CHECK(music(0x00045FBCu,0xFFFFFFFFu)==0xFFFFFFFFu && !pc_music_playing());
    *((uint8_t *)memory+0x007DE455u)=0;
    /* a stock row clears the choice; a menu without item 0xE changes nothing */
    WORD_AT(item_addr+0xC8u)=3u;
    hook(pc_custom_music_arcade_store,store_original,0x002C76CAu,0x00400000u,1u,profile_addr,0);
    CHECK(WORD_AT(0x007613D0u)==3u && !pc_music_arcade_load(name,sizeof name));
    WORD_AT(item_addr+0xC8u)=58u;
    hook(pc_custom_music_arcade_store,store_original,0x002C76CAu,0x00400000u,1u,profile_addr,0);
    CHECK(pc_music_arcade_load(name,sizeof name) && !strcmp(name,"Alpha.wav"));
    k=(int)item_addr;item_addr=0;
    hook(pc_custom_music_arcade_store,plain_original,0x002C76CAu,0x00400000u,1u,profile_addr,0);
    item_addr=(uint32_t)k;
    CHECK(pc_music_arcade_load(name,sizeof name) && !strcmp(name,"Alpha.wav"));
    WORD_AT(item_addr+0xC8u)=70u;                                            /* past the offered rows */
    hook(pc_custom_music_arcade_store,store_original,0x002C76CAu,0x00400000u,1u,profile_addr,0);
    CHECK(!pc_music_arcade_load(name,sizeof name));
    pc_music_stop();

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp errors\n",failures,esp_errors);return 1; }
    printf("mapmaker custommusic ok\n");
    return 0;
}
