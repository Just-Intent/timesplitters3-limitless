/* Asset-free contract for src/interface/pc_mapmaker_validate.inc: the MapMaker map
 * validator in front of the editor loader (refuse the stock way) and the
 * scene-100 level setup (hide bad chunks for the call, restore after).
 * Valid maps include what the editor and its glitches produce: many items per
 * tile, any position bytes, 8 patrol points, 500 pieces, dangling links. The
 * map container is the stock 32 KB. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x01000000/4], g_eax, g_ecx, g_edx, g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
void sub_00333700(void) { g_ecx=1; }
void sub_002FDF50(void) { g_ecx=2; }
static unsigned pad_calls;
static uint32_t pads_a(uint32_t id) { return id%3u; }
static uint32_t pads_b(uint32_t id) { return id==44u?2u:1u; }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x=0;
    va_start(a,n);
    if(n) x=va_arg(a,uint32_t);
    va_end(a);
    pad_calls++;
    if(x>=45u) { fprintf(stderr,"pad count asked for tile %u\n",(unsigned)x);exit(3); }
    return f==sub_00333700?pads_a(x):f==sub_002FDF50?pads_b(x):0u;
}
#include "pc_mapmaker_validate.inc"

static int failures, esp_errors;
#define ESP0 0x00F00000u
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)

/* ---- map builder ------------------------------------------------------- */
#define C 0x00200000u
#define SPAN 0x8000u                 /* the stock map container */
#define T(n) (0x00C77600u+(n))
typedef struct Map {
    uint32_t tileset, words, title;            /* title: chars before the NUL at +0x162 */
    int32_t header_pieces;
    int pieces, items, before_header, second_chunk;
    uint32_t bad_tile;                         /* tile id of piece 1 (0 = default) */
    int32_t item_id;                           /* id of item 0 of piece 0 */
    uint32_t item_k;                           /* byte +0x14 of the last item */
    uint32_t patrol, name;
    uint32_t rec_type; int32_t rec_count, rec_idx;
    uint32_t s73, s74, s75;
    int malformed, no_end, truncate;           /* truncate: piece chunk size short by N bytes */
} Map;
static Map def(void)
{
    Map m;
    memset(&m,0,sizeof m);
    m.tileset=3;m.words=15;m.title=20;m.header_pieces=6;m.pieces=6;m.items=3;m.item_id=0x2C;
    m.patrol=8;m.name=0x23;m.rec_type=3;m.rec_count=13;m.rec_idx=6;   /* idx = piece count: saver's "not found" */
    m.s73=0x4B00;m.s74=2000;m.s75=3000;
    return m;
}
static uint32_t cur, chunk_of[16];
static uint32_t begin(uint32_t tag) { uint32_t c=cur;WORD_AT(c)=tag;WORD_AT(c+4)=0;cur+=8;return c; }
static void finish(uint32_t c) { WORD_AT(c+4)=cur-(c+8u); }
static void fill(uint32_t n,uint8_t v) { while(n--) BYTE_AT(cur++)=v; }
static void pieces_chunk(const Map *m,int n)
{
    int i,j;
    uint32_t c=begin(T(0x69)),p,id,pads;
    chunk_of[1]=c;
    for(i=0;i<n;i++) {
        id=i==1 && m->bad_tile?m->bad_tile:(uint32_t)(i*7)%45u;
        p=cur;fill(0x28,0xA5);                 /* any position/rotation bytes */
        WORD_AT(p)=id;BYTE_AT(p+0x1E)=(uint8_t)m->items;BYTE_AT(p+0x1F)=0;
        pads=pads_a(id)+pads_b(id);fill(pads*8u,0x11);
        for(j=0;j<m->items;j++) {
            uint32_t k=j==m->items-1?m->item_k:(uint32_t)j%3u,q=cur;
            fill(0x18u+4u*k,0x7F);
            BYTE_AT(q+0xE)=(uint8_t)(i==0 && j==0?m->item_id:0x0A+j);
            BYTE_AT(q+0xF)=(uint8_t)((i==0 && j==0?m->item_id:0)>>8);
            BYTE_AT(q+0x14)=(uint8_t)k;
        }
    }
    finish(c);
    if(m->truncate) WORD_AT(c+4)-=(uint32_t)m->truncate;
    cur=c+8u+WORD_AT(c+4);
}
static void build(const Map *m)
{
    uint32_t c,i;
    memset((char *)memory+C,0,SPAN);
    WORD_AT(C)=0x00C77667u;WORD_AT(C+4)=0x12Du;
    cur=C+8u;
    if(m->before_header) { Map b=*m;b.bad_tile=45;pieces_chunk(&b,2); }  /* junk: not read */
    c=begin(T(0x68));chunk_of[0]=c;fill(0x3E0,0);
    WORD_AT(c+8+4)=(uint32_t)m->header_pieces;
    BYTE_AT(c+8+0xA)=(uint8_t)m->tileset;BYTE_AT(c+8+0xB)=(uint8_t)(m->tileset>>8);
    BYTE_AT(c+8+0x42)=(uint8_t)m->words;
    for(i=0;i<m->title && i<0xE0u;i++) BYTE_AT(c+8+0x162+i)='T';
    finish(c);
    pieces_chunk(m,m->pieces);
    if(m->second_chunk) pieces_chunk(m,m->second_chunk);
    c=begin(T(0x6B));fill(12,0);finish(c);
    c=begin(0);fill(4,0);finish(c);                       /* a chunk the editor area hid */
    c=begin(T(0x6C));chunk_of[2]=c;fill(0xE4,0);
    for(i=0;i<m->name;i++) BYTE_AT(c+8+i)='N';
    BYTE_AT(c+8+0x2C)=(uint8_t)m->patrol;BYTE_AT(c+8+0x2D)=(uint8_t)(m->patrol>>8);
    finish(c);
    c=begin(T(0x6F));chunk_of[3]=c;fill(0x90,0);
    WORD_AT(c+8+0x80)=m->rec_type;
    if(m->rec_type==3u) WORD_AT(c+8+0x20)=(uint32_t)m->rec_count; else WORD_AT(c+8+0x70)=(uint32_t)m->rec_count;
    for(i=0;(int32_t)i<m->rec_count && i<13u;i++) WORD_AT(c+8+0x24+8*i)=(uint32_t)(i==0?m->rec_idx:(int32_t)i+(m->rec_type==0xAu));
    finish(c);
    c=begin(T(0x73));chunk_of[4]=c;fill(m->s73,0x33);finish(c);
    c=begin(T(0x74));chunk_of[5]=c;fill(m->s74,0x34);finish(c);
    c=begin(T(0x75));chunk_of[6]=c;fill(m->s75,0x35);finish(c);
    c=begin(0x12345678u);fill(16,9);finish(c);            /* not a loader tag */
    if(m->malformed) { c=begin(T(0x6D));chunk_of[7]=c;WORD_AT(c+4)=0x00100000u;return; }
    if(m->no_end) {                                       /* chunks up to the container end, no end tag */
        c=begin(T(0x72));WORD_AT(c+4)=C+SPAN-(c+8u);cur=C+SPAN;return;
    }
    WORD_AT(cur)=T(0x6A);WORD_AT(cur+4)=0;
}

/* ---- fake loaders ------------------------------------------------------ */
static unsigned loader_calls;
static uint32_t magic_seen;
static void fake_editor(void) { loader_calls++;magic_seen=WORD_AT(C);g_eax=magic_seen==0x00C77667u;g_esp+=4u; }
static unsigned hidden_seen, end_at_seen;
static uint32_t tags_seen[64];
static unsigned n_seen;
static void fake_level(void)
{
    uint32_t c=C+8u;
    unsigned n=0;
    loader_calls++;hidden_seen=0;end_at_seen=0;n_seen=0;memset(tags_seen,0,sizeof tags_seen);
    while(c<C+SPAN-0x20u && n<64u) {
        tags_seen[n++]=WORD_AT(c);n_seen=n;
        if(WORD_AT(c)==T(0x6A)) { end_at_seen=c-C;break; }
        if(!WORD_AT(c)) hidden_seen++;
        c+=8u+WORD_AT(c+4);
    }
    g_esp+=4u;
}

/* editor: the hook on sub_00301A90(container) always runs the loader once,
 * which pops the return address. 1 = accepted (magic intact), 0 = refused
 * (magic cleared, the loader's own mismatch path). */
static int call_editor(uint32_t container)
{
    loader_calls=0;magic_seen=0x1234u;
    g_esp=ESP0;WORD_AT(g_esp)=0x00300ABCu;WORD_AT(g_esp+4u)=container;
    pc_mapmaker_validate_editor(fake_editor);
    if(loader_calls!=1 || g_esp!=ESP0+4u) { esp_errors++;fprintf(stderr,"editor hook esp/calls\n"); }
    g_esp=ESP0;
    return magic_seen==0x00C77667u;
}
static int editor(const Map *m)
{
    int r;
    build(m);
    r=call_editor(C);
    if(!r && (magic_seen!=0 || g_eax!=0)) { esp_errors++;fprintf(stderr,"refusal not stock\n"); }
    return r;
}
/* level: snapshot during the call; checks everything is put back. */
static uint32_t snapshot[SPAN/4];
static void level(const Map *m)
{
    build(m);
    memcpy(snapshot,(char *)memory+C,sizeof snapshot);
    loader_calls=0;
    g_esp=ESP0;
    pc_mapmaker_validate_level(fake_level);
    if(g_esp!=ESP0+4u || loader_calls!=1) { esp_errors++;fprintf(stderr,"level hook esp/calls\n"); }
    if(memcmp(snapshot,(char *)memory+C,sizeof snapshot)) { failures++;fprintf(stderr,"level hook did not restore the map\n"); }
    g_esp=ESP0;
}
static int tag_hidden(uint32_t chunk)
{
    unsigned i;
    uint32_t c=C+8u;
    for(i=0;i<n_seen;i++) { if(c==chunk) return tags_seen[i]==0; if(tags_seen[i]==T(0x6A)) break; c+=8u+WORD_AT(c+4); }
    return -1;
}

int main(void)
{
    Map m;
    g_xbox_mem_offset=(uintptr_t)memory;

    /* ---- editor: valid maps (stock-made and glitch-made) are accepted ---- */
    m=def();CHECK(editor(&m)==1 && pad_calls>0);
    m=def();m.items=40;m.item_k=200;m.s73=0;m.s74=0;m.s75=0;CHECK(editor(&m)==1); /* many items on one tile */
    m=def();m.header_pieces=500;m.pieces=500;m.items=0;m.s73=0;m.s74=0;m.s75=0;CHECK(editor(&m)==1);
    m=def();m.header_pieces=4;CHECK(editor(&m)==1);                 /* fewer read than present */
    m=def();m.header_pieces=-5;CHECK(editor(&m)==1);                /* loop skipped by the loader */
    m=def();m.item_id=79;CHECK(editor(&m)==1);
    m=def();m.bad_tile=44;CHECK(editor(&m)==1);
    m=def();m.tileset=6;m.words=17;CHECK(editor(&m)==1);
    m=def();m.title=0xE0;CHECK(editor(&m)==1);                      /* the editor loader forces the NUL */
    m=def();m.rec_idx=1023;CHECK(editor(&m)==1);
    m=def();m.rec_idx=0;m.rec_count=-1;CHECK(editor(&m)==1);
    m=def();m.rec_type=0xA;m.rec_count=10;m.rec_idx=1024;CHECK(editor(&m)==1);
    m=def();m.rec_type=7;m.rec_count=99;m.rec_idx=-9;CHECK(editor(&m)==1); /* other types: no links */
    m=def();m.patrol=0;m.name=0;CHECK(editor(&m)==1);
    m=def();m.s73=0;m.s74=0;m.s75=0;CHECK(editor(&m)==1);

    /* ---- editor: crafted / damaged maps are refused the stock way ---- */
    m=def();m.tileset=7;CHECK(editor(&m)==0);
    m=def();m.words=18;CHECK(editor(&m)==0);
    m=def();m.header_pieces=1025;m.pieces=6;CHECK(editor(&m)==0);
    m=def();m.second_chunk=6;CHECK(editor(&m)==1);
    m=def();m.header_pieces=7;CHECK(editor(&m)==0);                 /* reads past the piece chunk */
    m=def();m.truncate=4;CHECK(editor(&m)==0);
    m=def();m.items=0;m.truncate=0x30;CHECK(editor(&m)==0);          /* last piece header cut: valid id */
    m=def();m.bad_tile=45;CHECK(editor(&m)==0);
    m=def();m.bad_tile=0xFFFFFFFFu;CHECK(editor(&m)==0);
    m=def();m.item_id=80;CHECK(editor(&m)==0);
    m=def();m.item_id=-1;CHECK(editor(&m)==0);
    m=def();m.items=1;m.item_k=255;m.truncate=8;CHECK(editor(&m)==0);
    m=def();m.before_header=1;CHECK(editor(&m)==1);                 /* not read before a header */
    m=def();m.patrol=9;CHECK(editor(&m)==0);
    m=def();m.name=0x24;CHECK(editor(&m)==0);
    m=def();m.rec_count=14;CHECK(editor(&m)==0);
    m=def();m.rec_idx=-1;CHECK(editor(&m)==0);
    m=def();m.rec_idx=1024;CHECK(editor(&m)==0);
    m=def();m.rec_type=0xA;m.rec_count=11;CHECK(editor(&m)==0);
    m=def();m.rec_type=0xA;m.rec_count=10;m.rec_idx=0;CHECK(editor(&m)==0);
    m=def();m.rec_type=0xA;m.rec_count=10;m.rec_idx=1025;CHECK(editor(&m)==0);
    m=def();m.s73=0x4B01;CHECK(editor(&m)==0);
    m=def();m.s74=2001;CHECK(editor(&m)==0);
    m=def();m.s75=3001;CHECK(editor(&m)==0);
    m=def();m.malformed=1;CHECK(editor(&m)==0);
    m=def();m.no_end=1;CHECK(editor(&m)==0);
    /* a container the validator cannot see whole: left to the loader */
    m=def();m.tileset=7;build(&m);WORD_AT(sizeof memory-0x100u)=0x00C77667u;
    CHECK(call_editor((uint32_t)(sizeof memory-0x100u))==1 && WORD_AT(C)==0x00C77667u);
    CHECK(call_editor(0)==1 && WORD_AT(C)==0x00C77667u);    /* no container: the loader decides */
    CHECK(call_editor(C)==0);                                /* the same map in view: refused */

    /* ---- level setup: only scene 100's one-time pass ---- */
    WORD_AT(0x00784014u)=C;WORD_AT(0x0079094Cu)=100u;WORD_AT(0x006F32ECu)=0;
    m=def();level(&m);CHECK(hidden_seen==1 && end_at_seen);         /* only the chunk already hidden */
    m=def();m.title=0xE0;level(&m);
    CHECK(tag_hidden(chunk_of[0])==1 && tag_hidden(chunk_of[1])==1); /* header and its pieces */
    m=def();m.header_pieces=501;m.pieces=501;m.items=0;m.s73=0;m.s74=0;m.s75=0;level(&m);
    CHECK(tag_hidden(chunk_of[0])==1 && tag_hidden(chunk_of[1])==1 && tag_hidden(chunk_of[2])==0);
    m=def();m.header_pieces=500;m.pieces=500;m.items=0;m.s73=0;m.s74=0;m.s75=0;level(&m);
    CHECK(hidden_seen==1);
    m=def();m.bad_tile=45;level(&m);
    CHECK(tag_hidden(chunk_of[0])==0 && tag_hidden(chunk_of[1])==1 && hidden_seen==2);
    m=def();m.item_id=-3;level(&m);CHECK(tag_hidden(chunk_of[1])==1);
    m=def();m.patrol=9;level(&m);CHECK(tag_hidden(chunk_of[2])==1 && hidden_seen==2);
    m=def();m.name=0x30;m.rec_count=40;m.s74=5000;level(&m);        /* editor-only reads: kept */
    CHECK(hidden_seen==1);
    m=def();m.malformed=1;level(&m);
    CHECK(end_at_seen==chunk_of[7]-C && hidden_seen==1);           /* map ends at the bad chunk */
    WORD_AT(0x006F32ECu)=1;m=def();m.bad_tile=45;level(&m);CHECK(hidden_seen==1);
    WORD_AT(0x006F32ECu)=0;WORD_AT(0x0079094Cu)=102u;level(&m);CHECK(hidden_seen==1);
    WORD_AT(0x0079094Cu)=100u;

    if(failures || esp_errors) { fprintf(stderr,"%d failures, %d esp/contract errors\n",failures,esp_errors);return 1; }
    printf("mapmaker validate ok (%u refused/reported)\n",mapmaker_validate_refused);
    return 0;
}
