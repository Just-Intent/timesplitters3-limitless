/* Asset-free contract for src/interface/pc_mapmaker_mothership.inc: the
 * intro's Timesplitter mothership in MapMaker story play. l_64_ch.pak mount
 * at the level's pak mounts, the model requests, the ship created above a
 * placeholder Story AI (id 0x75) straight in its attack state, up to four
 * ships each running with its own copy of the statics (firing box and dust
 * height rebased in that copy, the stock constants untouched), the placeholder
 * removed on each ship's first frame, the fall shifted to the map floor and
 * the bar label. Fake guest memory; fake originals pop their return address like
 * the lifted callees, and every hook call checks esp. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[0x00900000/4], g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define BYTE_AT(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define HALF_AT(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t p,size_t n) { return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p; }
typedef struct GuestRegisters { uint32_t a,c,d,sp,b,si,di; } GuestRegisters;
static void save_registers(GuestRegisters *r) { r->a=g_eax;r->c=g_ecx;r->d=g_edx;r->sp=g_esp;r->b=g_ebx;r->si=g_esi;r->di=g_edi; }
static void restore_registers(GuestRegisters *r) { g_eax=r->a;g_ecx=r->c;g_edx=r->d;g_esp=r->sp;g_ebx=r->b;g_esi=r->si;g_edi=r->di; }

#define HEAP 0x00300000u
#define CONT 0x00200000u
#define POOL 0x00400000u
#define ENTS 0x00480000u
#define RECS 0x004C0000u
#define PLAYER 0x00500000u
#define RES 0x00520000u
#define SHIP 0x00560000u
#define ESP0 0x00600000u
#define A(i) (POOL+(uint32_t)(i)*0xEA0u)
#define ENT(i) (ENTS+(uint32_t)(i)*0x400u)
#define REC(i) (RECS+(uint32_t)(i)*0x100u)
#define PATH(r) (0x00530000u+(uint32_t)(r)*0x40u)
#define REQ(r) (0x00534000u+(uint32_t)(r)*0x40u)

/* distinct bodies: identical ones may be folded */
void sub_0003DFA0(void) { g_ecx=5; }   /* pc_mapmaker_limits.inc */
void sub_00062260(void) { g_ecx=1; }
void sub_000622D0(void) { g_ecx=2; }
void sub_000A6F90(void) { g_ecx=3; }
void sub_0015DDA0(void) { g_ecx=4; }
void sub_000A5E40(void) { g_ecx=6; }
void sub_000631C0(void) { g_ecx=7; }
void sub_00096330(void) { g_ecx=8; }
static uint32_t heap_next=HEAP;
static int heap_fail;
uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment) { uint32_t p;(void)alignment;if(heap_fail)return 0;p=heap_next;heap_next+=(bytes+15u)&~15u;return p; }

static int pak_mounted, mount_ok=1, spawn_ok=1, originals, req_reads=1, req_state[8];
static unsigned nmount, nspawn, nremove, nrequest;
static uint32_t mount_args[3], spawn_args[5], remove_arg, requested[8];
static float spawn_pos[3], spawn_rot[3];
static uint32_t spawn_obj=SHIP;
static unsigned ntex;
static uint32_t tex_args[4][2];
static int is_pak(uint32_t p) { return p && !strcmp((const char *)((uintptr_t)p+g_xbox_mem_offset),"pak/challenge/l_64_ch.pak"); }
static uint32_t guest_call(void (*f)(void),int n,...)
{
    va_list a;
    uint32_t x[12]={0};
    int i;
    if(n>12) { fprintf(stderr,"guest_call with %d args\n",n);exit(3); }
    va_start(a,n);
    for(i=0;i<n && i<12;i++) x[i]=va_arg(a,uint32_t);
    va_end(a);
    if(f==sub_00062260) return (is_pak(x[0]) && pak_mounted)?4u:0xFFFFFFFFu;
    if(f==sub_000622D0) {
        nmount++;memcpy(mount_args,x,sizeof mount_args);
        if(mount_ok && is_pak(x[0])) pak_mounted=1;
        return mount_ok?7u:0xFFFFFFFFu;
    }
    if(f==sub_000A6F90) {   /* the object, then its init (sub_0021A710 case 0x539: state 0) */
        nspawn++;memcpy(spawn_args,x,sizeof spawn_args);
        for(i=0;i<3;i++) { spawn_pos[i]=FLOAT_AT(x[1]+(uint32_t)i*4u);spawn_rot[i]=FLOAT_AT(x[2]+(uint32_t)i*4u); }
        if(!spawn_ok) return 0;
        for(i=0;i<3;i++) FLOAT_AT(spawn_obj+0x64u+(uint32_t)i*4u)=spawn_pos[i];
        WORD_AT(spawn_obj+0x1A0u)=0;HALF_AT(spawn_obj+0x94u)=0x539u;
        WORD_AT(0x0075C0E4u)=0;                                 /* the init's engine sound level */
        return spawn_obj;
    }
    if(f==sub_0015DDA0) { nremove++;remove_arg=x[0];return 0; }
    if(f==sub_000A5E40) {
        int16_t r=(int16_t)HALF_AT(0x008CA000u+x[0]*2u);
        if(nrequest<8u) requested[nrequest]=x[0];
        nrequest++;
        if(r>=0 && r<8) req_state[r]=req_reads?2:1;
        return 0;
    }
    if(f==sub_00096330) { if(ntex<4u) { tex_args[ntex][0]=x[0];tex_args[ntex][1]=x[1]; } ntex++;return 0; }
    if(f==sub_000631C0) {
        for(i=0;i<8;i++) if(x[0]==PATH(i) && req_state[i]) { WORD_AT(REQ(i)+0x10u)=req_state[i]==2?0x00538000u:0;return REQ(i); }
        return 0;
    }
    return 0;
}
#include "pc_mapmaker_budget.inc"
#include "pc_mapmaker_limits.inc"
#include "pc_mapmaker_mothership.inc"

static int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: CHECK(%s)\n",__FILE__,__LINE__,#x);failures++; } } while(0)
#define NEAR(a,b) ((a)-(b)<0.001f && (b)-(a)<0.001f)
static uint32_t orig_eax, pre_eax=0xDDDDu;
static void fake(void) { originals++;g_eax=orig_eax;g_esp+=4u; }
static int ai_originals;
static void fake_ai(void) { originals++;ai_originals++;g_esp+=4u; }
/* sub_00217050: records the height and firing box it saw; state 3 falls 1 m, below 6.6 -> 4 */
static float seen_y, seen_box[5];
static void fake_update(void)
{
    uint32_t s=WORD_AT(g_esp+4u);
    originals++;seen_y=FLOAT_AT(s+0x68u);
    memcpy(seen_box,(void *)((uintptr_t)0x0049EA84u+g_xbox_mem_offset),sizeof seen_box);
    if(WORD_AT(s+0x1A0u)==3u) {
        FLOAT_AT(s+0x68u)-=1.0f;
        if(FLOAT_AT(s+0x68u)<6.6f) WORD_AT(s+0x1A0u)=4u;
    }
    g_esp+=4u;
}

static uint32_t call(void (*fn)(void (*)(void)),void (*orig)(void),uint32_t ret,uint32_t a1,uint32_t a2,uint32_t a3,int expect_orig,int line)
{
    int before=originals;
    g_esp=ESP0;WORD_AT(ESP0)=ret;WORD_AT(ESP0+4u)=a1;WORD_AT(ESP0+8u)=a2;WORD_AT(ESP0+12u)=a3;
    g_eax=pre_eax;
    fn(orig);
    if(g_esp!=ESP0+4u) { fprintf(stderr,"%s:%d: esp %+d\n",__FILE__,line,(int)(g_esp-ESP0));failures++; }
    if(expect_orig>=0 && originals-before!=expect_orig) { fprintf(stderr,"%s:%d: original ran %d times\n",__FILE__,line,originals-before);failures++; }
    g_esp=ESP0;pre_eax=0xDDDDu;
    return g_eax;
}
#define CALL(fn,orig,ret,a1,a2,a3,n) call(fn,orig,ret,a1,a2,a3,n,__LINE__)
static void set_env(const char *name,const char *value)
{
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1); else unsetenv(name);
#endif
    memset(&mapmaker_limits,0,sizeof mapmaker_limits);
    msh.read=0;
}
static void container(uint32_t set_index,const uint16_t *ids,unsigned n,const uint16_t *extra,unsigned ne)
{
    uint32_t c=CONT+8u,k;
    memset((void *)((uintptr_t)CONT+g_xbox_mem_offset),0,0x00080000u);
    WORD_AT(0x00784014u)=CONT;WORD_AT(CONT)=0x00C77667u;
    WORD_AT(c)=0x00C77668u;WORD_AT(c+4u)=0x18u+20u*4u;
    HALF_AT(c+8u+0x14u)=(uint16_t)set_index;BYTE_AT(c+8u+0x16u)=(uint8_t)n;
    for(k=0;k<n;k++) HALF_AT(c+8u+0x18u+k*4u)=ids[k];
    c+=8u+0x18u+20u*4u;
    if(extra) {
        WORD_AT(c)=0x00C776F5u;WORD_AT(c+4u)=0x30u;
        HALF_AT(c+8u)=1u;BYTE_AT(c+10u)=10u;BYTE_AT(c+11u)=(uint8_t)ne;
        for(k=0;k<ne;k++) HALF_AT(c+12u+k*4u)=extra[k];
        c+=8u+0x30u;
    }
    WORD_AT(c)=0x00C7766Au;WORD_AT(c+4u)=0;
}
static void actor(unsigned i,uint32_t id,float x,float y,float z,float yaw)
{
    WORD_AT(A(i))=ENT(i);HALF_AT(A(i)+0x48u)=(uint16_t)id;WORD_AT(A(i)+0xBF0u)=REC(i);
    WORD_AT(ENT(i)+0x7Cu)=A(i);
    FLOAT_AT(ENT(i)+0x64u)=x;FLOAT_AT(ENT(i)+0x68u)=y;FLOAT_AT(ENT(i)+0x6Cu)=z;FLOAT_AT(ENT(i)+0x70u)=yaw;
}
/* scene-100 resource table: 0x539 (row 3, init 0x0021A710) and 0x53A (row 4) */
static void resources(int ship,int wreck,uint32_t init)
{
    uint32_t m;
    for(m=0;m<0xA00u;m++) HALF_AT(0x008CA000u+m*2u)=0xFFFFu;
    WORD_AT(0x00791D4Cu)=RES;
    if(ship) { HALF_AT(0x008CA000u+0x539u*2u)=3;WORD_AT(RES+3u*0x30u+0x2Cu)=init; }
    if(wreck) HALF_AT(0x008CA000u+0x53Au*2u)=4;
    for(m=0;m<8u;m++) { WORD_AT(RES+m*0x30u+0x10u)=PATH(m);WORD_AT(RES+m*0x30u+0x14u)=0;WORD_AT(RES+m*0x30u+4u)=0;req_state[m]=0; }
}
/* the stock firing box and dust height (0x0049EA84..0x0049EA94) */
static const float stock_box[5]={-45.5f,-88.04f,-47.8f,-67.0f,-56.0f};
static void stock_constants(void) { memcpy((void *)((uintptr_t)0x0049EA84u+g_xbox_mem_offset),stock_box,sizeof stock_box); }
static int constants_stock(void) { return !memcmp((void *)((uintptr_t)0x0049EA84u+g_xbox_mem_offset),stock_box,sizeof stock_box); }
static void play_scene(void) { WORD_AT(0x0079094Cu)=100u;BYTE_AT(0x007DE455u)=10u; }
/* word i of ship k's copy of the statics, as a float */
static float stf(unsigned k,unsigned i) { float f;memcpy(&f,&msh_lv.s[k].st[i],4);return f; }
/* an entry's original: records the statics it ran with, changes them, and can
 * run another ship's damage inside */
static uint32_t entry_dc, entry_set, nest_obj, inner_dc;
static float entry_x;
static void fake_inner(void) { originals++;inner_dc=WORD_AT(0x0075C0DCu);WORD_AT(0x0075C0DCu)=99u;g_esp+=4u; }
void pc_mapmaker_mothership_damage(void (*original)(void));
static void fake_entry(void)
{
    originals++;
    entry_dc=WORD_AT(0x0075C0DCu);entry_x=FLOAT_AT(0x0049EA90u);
    if(nest_obj) {
        WORD_AT(0x0078D520u)=0x77u+entry_set;                          /* changed before the inner call */
        WORD_AT(0x00570400u)=nest_obj;
        g_esp-=8u;WORD_AT(g_esp)=0x00219460u;WORD_AT(g_esp+4u)=0x00570400u;
        pc_mapmaker_mothership_damage(fake_inner);
        g_esp+=4u;
        if(WORD_AT(0x0075C0DCu)!=entry_dc) entry_dc=0xBADu;          /* the outer ship's statics are back */
    }
    WORD_AT(0x0075C0DCu)=entry_set;WORD_AT(0x0078D500u)=entry_set;WORD_AT(0x0078D360u)=entry_set;
    g_esp+=4u;
}
static const uint16_t with[]={0x0C,0x75,0xA8}, without[]={0x0C,0xA8}, extra[]={0x30,0x75};
/* a new level with the placeholder at (40, 2, 30) facing 1.5 */
static void level_with_ship(int line)
{
    unsigned s=nspawn;
    play_scene();
    container(0x11,with,3,NULL,0);
    resources(1,1,0x0021A710u);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;WORD_AT(0x006D7C4Cu)=0;
    call(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1,line);
    call(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1,line);
    actor(0,0x75,40.0f,2.0f,30.0f,1.5f);
    call(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1,line);
    if(nspawn!=s+1u) { fprintf(stderr,"%s:%d: ship not spawned\n",__FILE__,line);failures++; }
}

int main(void)
{
    unsigned k;
    g_xbox_mem_offset=(uintptr_t)memory;
    stock_constants();
    set_env("TSFP_MAPMAKER_POOLS",NULL);
    set_env("TSFP_MAPMAKER_MOTHERSHIP",NULL);

    /* ---- off: every hook is the original ---------------------------------- */
    set_env("TSFP_MAPMAKER_LIMITS",NULL);
    play_scene();
    container(0x11,with,3,NULL,0);
    resources(1,1,0x0021A710u);
    WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nmount==0 && nrequest==0);
    actor(0,0x75,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    orig_eax=0x1234u;
    CHECK(CALL(pc_mapmaker_mothership_text,fake,0x0014D09Fu,0x164Bu,0,0,1)==0x1234u);
    set_env("TSFP_MAPMAKER_LIMITS","1");

    /* ---- mount ------------------------------------------------------------ */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1 && is_pak(mount_args[0]) && mount_args[1]==0 && mount_args[2]==1u && pak_mounted);
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);    /* already mounted */
    CHECK(nmount==1);
    pak_mounted=0;WORD_AT(0x006D7880u)=57u;                         /* table nearly full */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==1);
    WORD_AT(0x006D7880u)=56u;
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2 && pak_mounted);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x0C,with,3,NULL,0);                                  /* a preset set */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==2);
    container(0x11,without,2,extra,2);                              /* slot 12 */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==3 && pak_mounted);
    pak_mounted=0;
    WORD_AT(CONT)=0;                                                 /* unreadable container */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4 && pak_mounted);
    pak_mounted=0;
    container(0x11,with,3,NULL,0);
    BYTE_AT(0x007DE455u)=3u;                                         /* not story */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();WORD_AT(0x0079094Cu)=2u;                            /* the intro itself */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    play_scene();
    WORD_AT(0x006D7C4Cu)=1u;                                         /* the original does nothing */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    WORD_AT(0x006D7C4Cu)=0;
    set_env("TSFP_MAPMAKER_MOTHERSHIP","0");
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==4);
    set_env("TSFP_MAPMAKER_MOTHERSHIP",NULL);
    mount_ok=0;
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(nmount==5 && !pak_mounted);
    mount_ok=1;

    /* ---- requests --------------------------------------------------------- */
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);    /* pak missing */
    CHECK(nrequest==0);
    pak_mounted=1;
    resources(1,0,0x0021A710u);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);    /* no wreck row */
    CHECK(nrequest==0);
    resources(1,1,0x0021A710u);
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==0);
    container(0x11,with,3,NULL,0);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(nrequest==2 && requested[0]==0x539u && requested[1]==0x53Au && msh_lv.requested);

    /* ---- ai_start: the placeholder becomes the ship ------------------------ */
    resources(1,1,0x0021A710u);                                       /* nothing requested */
    actor(0,0x75,40.0f,2.0f,30.0f,1.5f);
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0 && !msh_lv.n);
    req_reads=0;                                                      /* requested, not read */
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    req_reads=1;
    resources(1,1,0x002560C0u);                                       /* another init */
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    resources(1,1,0x0021A710u);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    actor(0,0x74,40.0f,2.0f,30.0f,1.5f);                              /* other character */
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    actor(0,0x75,40.0f,2.0f,30.0f,1.5f);
    pak_mounted=0;
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    pak_mounted=1;
    WORD_AT(ENT(0)+0x68u)=0x7FC00000u;                               /* unusable position (NaN) */
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==0);
    FLOAT_AT(ENT(0)+0x68u)=2.0f;
    spawn_ok=0;
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==1 && !msh_lv.n && constants_stock());
    spawn_ok=1;
    ai_originals=0;
    BYTE_AT(REQ(3)+2u)=0x10u;BYTE_AT(REQ(4)+2u)=0;msh_lv.textured=0;ntex=0;   /* the ship's textures left to its creation */
    FLOAT_AT(0x004D2D50u+0x3BCu*0x18u+0x14u)=0;
    WORD_AT(0x0075C0E4u)=0x66u;WORD_AT(0x0078D500u)=0x67u;           /* the statics before */
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==2 && msh_lv.n==1u && msh_lv.s[0].obj==SHIP && ai_originals==1 && !msh_lv.act);
    CHECK(ntex==1u && tex_args[0][0]==0x00538000u && tex_args[0][1]==1u && msh_lv.textured);   /* the ship only */
    CHECK(WORD_AT(0x0075C0E4u)==0x66u && WORD_AT(0x0078D500u)==0x67u && constants_stock());   /* put back */
    CHECK(msh_lv.s[0].st[7]==0 && msh_lv.s[0].st[10]==0x67u);        /* the init's zero, kept for the ship */
    CHECK(spawn_args[0]==0x539u && spawn_args[3]==0 && spawn_args[4]==0);
    CHECK(NEAR(spawn_pos[0],40.0f) && NEAR(spawn_pos[1],32.0f) && NEAR(spawn_pos[2],30.0f));
    CHECK(NEAR(spawn_rot[0],0.0f) && NEAR(spawn_rot[1],1.5f) && NEAR(spawn_rot[2],0.0f));
    CHECK(WORD_AT(SHIP+0x1A0u)==2u);                                  /* attack: no fly-in */
    CHECK(NEAR(stf(0,1),-30.0f) && NEAR(stf(0,2),90.0f));            /* its box: z */
    CHECK(NEAR(stf(0,3),-20.0f) && NEAR(stf(0,4),100.0f));           /* x */
    CHECK(NEAR(stf(0,0),2.0f));                                       /* dust on the floor */
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(nspawn==2 && msh_lv.n==1u && ntex==1u);                     /* the same placeholder again */

    /* ---- text ------------------------------------------------------------- */
    orig_eax=0x1234u;
    {
        uint32_t s=CALL(pc_mapmaker_mothership_text,fake,0x0014D09Fu,0x164Bu,0,0,0);
        CHECK(s && s!=0x1234u && !strcmp((const char *)((uintptr_t)s+g_xbox_mem_offset),"Timesplitter Mothership"));
    }
    CHECK(CALL(pc_mapmaker_mothership_text,fake,0x0014D0A0u,0x164Bu,0,0,1)==0x1234u);   /* other caller */
    CHECK(CALL(pc_mapmaker_mothership_text,fake,0x0014D09Fu,0x164Cu,0,0,1)==0x1234u);   /* other text */
    WORD_AT(0x0079094Cu)=2u;
    CHECK(CALL(pc_mapmaker_mothership_text,fake,0x0014D09Fu,0x164Bu,0,0,1)==0x1234u);   /* the intro */
    play_scene();

    /* ---- update: placeholder removed once, fall shifted to the floor ------- */
    WORD_AT(PLAYER+0xFF4u)=ENT(0);WORD_AT(0x007B0C7Cu)=PLAYER;
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,0x00570000u,0,0,1);   /* another object */
    CHECK(nremove==0);
    FLOAT_AT(SHIP+0x68u)=32.0f;
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(nremove==1 && remove_arg==ENT(0) && WORD_AT(PLAYER+0xFF4u)==0 && NEAR(seen_y,32.0f));
    CHECK(NEAR(seen_box[0],2.0f) && NEAR(seen_box[1],-30.0f) && NEAR(seen_box[4],100.0f) && constants_stock() && !msh_lv.act);
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(nremove==1);                                                /* once */
    WORD_AT(SHIP+0x1A0u)=3u;                                          /* shot down */
    for(k=0;k<40u && WORD_AT(SHIP+0x1A0u)==3u;k++) CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(WORD_AT(SHIP+0x1A0u)==4u);
    CHECK(FLOAT_AT(SHIP+0x68u)<8.6f+0.001f && FLOAT_AT(SHIP+0x68u)>7.6f-0.001f);   /* 6.6 above the floor (2) */
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(NEAR(seen_y,FLOAT_AT(SHIP+0x68u)-2.0f));                    /* crash: still shifted */
    WORD_AT(SHIP+0x1A0u)=5u;
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(NEAR(seen_y,FLOAT_AT(SHIP+0x68u)));                         /* wreck: real position */
    WORD_AT(0x0079094Cu)=2u;WORD_AT(SHIP+0x1A0u)=3u;
    CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,SHIP,0,0,1);
    CHECK(NEAR(seen_y,FLOAT_AT(SHIP+0x68u)+1.0f));                    /* other scene: stock */
    play_scene();

    /* ---- the next level restores the constants ------------------------------ */
    container(0x11,without,2,NULL,0);
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(!msh_lv.n && constants_stock());
    CALL(pc_mapmaker_mothership_text,fake,0x0014D09Fu,0x164Bu,0,0,1);   /* no ship this level */
    level_with_ship(__LINE__);
    CHECK(constants_stock() && msh_lv.n==1u);                        /* the constants are never changed */
    WORD_AT(0x0079094Cu)=2u;                                         /* the intro: stock values */
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(constants_stock());
    level_with_ship(__LINE__);
    memset(&msh_lv,0,sizeof msh_lv);                                  /* a second spawn, no restore between */
    actor(0,0x75,-10.0f,5.0f,-20.0f,0.0f);
    CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(0),0,0,1);
    CHECK(msh_lv.s[0].obj==SHIP && NEAR(stf(0,0),5.0f) && NEAR(stf(0,3),-70.0f) && constants_stock());
    WORD_AT(0x0079094Cu)=2u;
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CHECK(constants_stock());                                         /* still the stock values */
    play_scene();

    /* ---- several ships: each runs with its own statics --------------------------- */
    container(0x11,with,3,NULL,0);resources(1,1,0x0021A710u);
    pak_mounted=0;WORD_AT(0x006D7880u)=10u;
    CALL(pc_mapmaker_mothership_mount,fake,0x001CDB8Au,0,0,0,1);
    CALL(pc_mapmaker_mothership_request,fake,0x001CDC25u,0,0,0,1);
    CHECK(!msh_lv.n && !msh_lv.act);
    {
        static uint32_t r0[31];
        unsigned s=nspawn,r=nremove,t0=ntex;
        BYTE_AT(REQ(3)+2u)=0x10u;BYTE_AT(REQ(4)+2u)=0x10u;
        for(k=0;k<3u;k++) WORD_AT(0x0075C0DCu+k*4u)=0xA0A0A000u+k;
        WORD_AT(0x0075C0ECu)=0xA0A0A0ECu;WORD_AT(0x0078D360u)=0xA0A0A360u;
        for(k=0;k<21u;k++) WORD_AT(0x0078D500u+k*4u)=0xB0B0B000u+k;
        msh_save(r0);
        for(k=0;k<5u;k++) {
            actor(2u+k,0x75,100.0f*(float)k,1.0f+(float)k,-50.0f,0.0f);
            spawn_obj=0x00440000u+k*0x1000u;
            CALL(pc_mapmaker_mothership_ai_start,fake_ai,0x0015F981u,A(2u+k),0,0,1);
        }
        spawn_obj=SHIP;
        CHECK(msh_lv.n==4u && nspawn==s+4u && !msh_lv.act);         /* the fifth stays a bot */
        CHECK(ntex==t0+2u);                                           /* once this level, ship and wreck */
        {
            uint32_t now[31];
            msh_save(now);
            CHECK(!memcmp(now,r0,sizeof now) && constants_stock());
        }
        for(k=0;k<4u;k++) {
            struct msh_one *m=&msh_lv.s[k];
            CHECK(m->obj==0x00440000u+k*0x1000u && m->place==A(2u+k) && NEAR(m->floor,1.0f+(float)k));
            CHECK(NEAR(stf(k,0),1.0f+(float)k) && NEAR(stf(k,1),-110.0f) && NEAR(stf(k,3),100.0f*(float)k-60.0f));
            CHECK(m->st[5]==0xA0A0A000u && m->st[7]==0 && m->st[8]==0xA0A0A0ECu && m->st[9]==0xA0A0A360u && m->st[10]==0xB0B0B000u);
            m->st[5]=20u+k;                                            /* [0x0075C0DC] */
        }
        /* ship 3's update: its statics in, its changes kept, the statics put back */
        entry_set=33u;
        CALL(pc_mapmaker_mothership_update,fake_entry,0x001234u,0x00442000u,0,0,1);
        CHECK(entry_dc==22u && NEAR(entry_x,140.0f) && msh_lv.s[2].st[5]==33u && msh_lv.s[2].st[9]==33u && msh_lv.s[2].st[10]==33u);
        CHECK(nremove==r+1u && remove_arg==ENT(4) && !msh_lv.s[2].place && msh_lv.s[1].place && !msh_lv.act);
        {
            uint32_t now[31];
            msh_save(now);
            CHECK(!memcmp(now,r0,sizeof now) && constants_stock());
        }
        entry_set=34u;                                                 /* a later frame */
        CALL(pc_mapmaker_mothership_update,fake_entry,0x001234u,0x00442000u,0,0,1);
        CHECK(entry_dc==33u && msh_lv.s[2].st[5]==34u && nremove==r+1u && !msh_lv.act && constants_stock());
        /* ship 1 damaged inside ship 2's frame */
        nest_obj=0x00440000u;entry_set=44u;
        CALL(pc_mapmaker_mothership_update,fake_entry,0x001234u,0x00441000u,0,0,2);
        nest_obj=0;
        CHECK(inner_dc==20u && entry_dc==21u && msh_lv.s[0].st[5]==99u && msh_lv.s[1].st[5]==44u && msh_lv.s[1].st[18]==0x77u+44u);
        CHECK(!msh_lv.act && constants_stock() && WORD_AT(0x0075C0DCu)==0xA0A0A000u);
        /* slot 10 and the damage of ship 4 */
        entry_set=23u;
        CALL(pc_mapmaker_mothership_slot10,fake_entry,0x001234u,0x00443000u,0,0,1);
        CHECK(entry_dc==23u && NEAR(entry_x,240.0f));
        WORD_AT(0x00570400u)=0x00443000u;
        CALL(pc_mapmaker_mothership_damage,fake_entry,0x001234u,0x00570400u,0,0,1);
        CHECK(entry_dc==23u && NEAR(entry_x,240.0f) && !msh_lv.act && constants_stock());
        /* not ours (another prop's damage, the fifth placeholder, the intro): as they are */
        WORD_AT(0x00570400u)=0x00570000u;
        CALL(pc_mapmaker_mothership_damage,fake_entry,0x001234u,0x00570400u,0,0,1);
        CHECK(entry_dc==0xA0A0A000u);
        WORD_AT(0x0075C0DCu)=0xA0A0A000u;
        CALL(pc_mapmaker_mothership_slot10,fake_entry,0x001234u,0x00444000u,0,0,1);
        CHECK(entry_dc==0xA0A0A000u);
        WORD_AT(0x0075C0DCu)=0xA0A0A000u;
        WORD_AT(0x0079094Cu)=2u;
        CALL(pc_mapmaker_mothership_slot10,fake_entry,0x001234u,0x00443000u,0,0,1);
        CHECK(entry_dc==0xA0A0A000u);
        WORD_AT(0x0075C0DCu)=0xA0A0A000u;
        play_scene();
        /* each ship falls to its own floor */
        WORD_AT(0x00441000u+0x1A0u)=3u;FLOAT_AT(0x00441000u+0x68u)=12.0f;
        CALL(pc_mapmaker_mothership_update,fake_update,0x001234u,0x00441000u,0,0,1);
        CHECK(NEAR(seen_y,10.0f) && NEAR(seen_box[0],2.0f) && NEAR(seen_box[3],40.0f) && constants_stock());
    }

    if(failures) { fprintf(stderr,"%d failures\n",failures);return 1; }
    puts("mapmaker mothership ok");
    return 0;
}
