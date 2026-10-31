/* US 45410066 MapMaker Story AI groups: duplicate a Story AI and keep
 * linked copies in step under a leader (docs/mapmaker/mapmaker-aitemplates.md).
 * Opt-in with TSFP_MAPMAKER_LIMITS=1 (mapmaker_limits.limits); with it off
 * every hook runs its original only.
 *
 * Editor (scene 102). The Story AI list opens the edit screen sub_0031CB90
 * through menu table 0x0052E088 (build sub_0031D140), with sub_0031CAC0 as
 * both its back callback and the callback of its last row ("Done", text
 * 0xD73). Rows are added just before that row; their callback is also
 * sub_0031CAC0, told apart by the highlighted row id at menu+0x18C (the menu
 * engine sets menu+0x18C to menu+0x1E8 = -1 before a back callback, see
 * 0x0007B1D7, and to the row id while highlighting, 0x0007AF66):
 *   not linked:   "Duplicate Story AI" (an independent full copy: every
 *                 setting and every point) and "Add linked copy";
 *   leader:       "Add linked copy", the group-logic toggle and "Unlink
 *                 (next copy leads)";
 *   linked copy:  "Add linked copy" and "Unlink copy".
 * Type 5 (game mode 7 singleton) gets no rows. A copy is made with the
 * guest's own add, sub_0031BE70 (the editor area's limit/colour policy), then
 * the fields are copied; at the limit (255) or without container room the
 * stock messages appear on the edit screen, which stays open.
 *
 * Groups. The first "Add linked copy" on an unlinked AI "Guard" makes it the
 * leader and adds "Guard 2" (next free number among all AI names); more
 * copies from any member join the same group. Entry +0x50 (unused by the
 * game): low word = leader index + 1 on a copy, bit 16 = group logic on a
 * leader. Members share character +0x28, variant +0x2C, toggle +0x30, type
 * +0x34, spawn count +0x38, delay +0x3C, team +0x44, weapon +0x48 and
 * behaviour +0x4C; each keeps its own name, colour, points, facing (+0x54) and
 * patrol mode (+0x5C). When an edit screen opens on a member (sub_0031CB90,
 * list and map paths) its shared fields are remembered; when it closes
 * (sub_0031CAC0), or at the next open, save/compile or delete, a change is
 * copied to the whole group. Unlinking or deleting (sub_0031C8B0) the leader
 * promotes its first copy (group logic off); a group left with one member
 * dissolves.
 *
 * Save format. The compiler (sub_003006C0) zeroes the container and never
 * writes Story AI chunk (0x00C7766C) bytes +0x32..+0x33; after it runs, the
 * word +0x32 gets 0x4000 (this format) | leader ordinal + 1 on a copy, and
 * 0x4000 | 0x8000 (group logic) on a leader (ordinal = index; the writer
 * stores the loop index at +0x22). The editor loader (sub_00301A90) does not
 * touch entry +0x50, so after it runs every loaded entry gets its link from
 * its chunk, or 0. The earlier template format (plain link to a point-less
 * "All X" entry, no 0x4000) is converted at load: its first copy becomes the
 * leader and the template stays an unlinked point-less AI. A stock game reads
 * every member as an independent AI.
 *
 * Play (scene 100). sub_00240C50 (level start) builds index -> leader from
 * the chunks. Logic on the leader's id (R + t + 1, R = [0x00783A50]) means
 * "any member" when the leader has group logic (or for an old-format
 * template); otherwise logic stays per AI:
 *  - sub_0033E560 (event delivery): an event of a copy is delivered again
 *    with the spawn record's id (+8) set to the leader id, then restored;
 *  - sub_00163780 (action type 2 on an id): repeated for every copy's id;
 *  - sub_0033E620 (spawn gate, action 0x11): a copy also waits for the
 *    leader's gate;
 *  - sub_00339600 (trigger 0x14 poll on the AI's actors): tried with each
 *    copy's id until it fires or starts its delay.
 * Logic on a copy's own id is unchanged.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax,
 * g_xbox_mem_offset, mapmaker_limits/mapmaker_limits_read and
 * MAPMAKER_GUEST_RET/MAPMAKER_PTR_OK (pc_mapmaker_limits.inc). */
#include <stdio.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define AIT_EDITOR_PTR 0x007844A8u
#define AIT_EDITOR_SIZE 0x58390u
#define AIT_SAI 0x11950u                /* editor.json: 255 x 0xE4 */
#define AIT_STRIDE 0xE4u
#define AIT_COUNT 0x280u
#define AIT_CAP 255u
#define AIT_SELECTED 0x00762F94u        /* byte: entry being edited */
#define AIT_SCENE 0x0079094Cu
#define AIT_CONTAINER 0x00784014u
#define AIT_SAI_BASE 0x00783A50u        /* R: Story AI ids are R + index + 1 */
#define AIT_CLOSE 0x0031CAC0u           /* edit screen close (list path) */
#define AIT_ROWS_AT 0x0031D083u         /* return from sub_000784A0 before "Done" */
#define AIT_LINK 0x50u
#define AIT_LINK_MASK 0xFFFFu           /* +0x50 low word: leader index + 1 (a copy) */
#define AIT_GROUP 0x10000u              /* +0x50 bit 16: group logic (a leader) */
#define AIT_TYPE 0x34u
#define AIT_POINTS 0x58u
#define AIT_PATROL 0x5Cu
#define AIT_FACING 0x54u
#define AIT_POINT0 0x64u                /* 8 x 16 bytes */
#define AIT_TAG_SAI 0x00C7766Cu
#define AIT_TAG_END 0x00C7766Au
#define AIT_MAGIC 0x00C77667u
#define AIT_SCAN 0x00080000u            /* container scan bound: 512 KB */
#define AIT_CHUNK_INDEX 0x22u
#define AIT_CHUNK_LINK 0x32u
#define AIT_SAVE_NEW 0x4000u            /* chunk word: leader format */
#define AIT_SAVE_GROUP 0x8000u          /* chunk word: leader with group logic */
#define AIT_SAVE_LINK 0x3FFFu           /* chunk word: leader ordinal + 1 (a copy) */
#define AIT_LEVEL_SLOTS 1024u           /* = MAPMAKER_LEVEL_SAI_SLOTS */
#define AIT_ROW_DUPLICATE 0x41540001u
#define AIT_ROW_LINKED 0x41540002u
#define AIT_ROW_UNLINK 0x41540003u
#define AIT_ROW_LOGIC 0x41540004u
#define AIT_CHUNK_BYTES 0x78u           /* largest Story AI chunk: 8 + 0x70 (8 points) */
#define AIT_LABELS 6u

static const uint32_t ait_shared[]={0x28u,0x2Cu,0x30u,0x34u,0x38u,0x3Cu,0x44u,0x48u,0x4Cu};
#define AIT_SHARED (sizeof ait_shared/sizeof ait_shared[0])

extern void sub_0031BE70(void),sub_00300670(void),sub_00079D60(void),sub_0007A270(void),
            sub_0003D7B0(void),sub_003C85D4(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

static struct {
    int pending;
    uint32_t editor, index, colour, shared[AIT_SHARED];
    uint32_t text;                      /* guest strings: AIT_LABELS x 32 bytes */
    unsigned dropped_links, converted, logged;
} ait;

static struct {
    int active;
    unsigned n;                         /* highest linked index + 1 */
    int16_t template_of[AIT_LEVEL_SLOTS];   /* copy -> leader (or old template) */
    uint8_t group[AIT_LEVEL_SLOTS];         /* leader whose id means "any member" */
} ait_level;

static void ait_log(const char *what,unsigned a,unsigned b)
{
    if(ait.logged>=16u) return;
    ait.logged++;
    fprintf(stderr,"[mapmaker] story AI groups: %s (%u, %u)\n",what,a,b);
    fflush(stderr);
}

/* ---- editor entries --------------------------------------------------- */
static int ait_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }
static uint32_t ait_editor(void)
{
    uint32_t e=WORD_AT(AIT_EDITOR_PTR);
    return e && MAPMAKER_PTR_OK(e,AIT_EDITOR_SIZE)?e:0u;
}
static uint32_t ait_count(uint32_t e) { uint32_t c=WORD_AT(e+AIT_COUNT); return c<=AIT_CAP?c:0u; }
static uint32_t ait_entry(uint32_t e,uint32_t i) { return e+AIT_SAI+i*AIT_STRIDE; }
static uint32_t ait_selected(void) { return WORD_AT(AIT_SELECTED)&0xFFu; }
static uint32_t ait_type(uint32_t e,uint32_t i) { return WORD_AT(ait_entry(e,i)+AIT_TYPE); }
static uint32_t ait_raw(uint32_t e,uint32_t i) { return WORD_AT(ait_entry(e,i)+AIT_LINK)&AIT_LINK_MASK; }
static const char *ait_name(uint32_t e,uint32_t i) { return (const char *)((uintptr_t)ait_entry(e,i)+g_xbox_mem_offset); }

/* Leader index of copy i, or -1: the link must name another entry that is
 * itself unlinked, and neither may be type 5. */
static int ait_link(uint32_t e,uint32_t i,uint32_t count)
{
    uint32_t l=ait_raw(e,i);
    if(!l || l>count || l-1u==i || ait_type(e,i)==5u) return -1;
    if(ait_raw(e,l-1u) || ait_type(e,l-1u)==5u) return -1;
    return (int)(l-1u);
}
static int ait_has_copies(uint32_t e,uint32_t t,uint32_t count)
{
    uint32_t k;
    for(k=0;k<count;k++) if(ait_link(e,k,count)==(int)t) return 1;
    return 0;
}
/* Leader of i's group (i itself when it leads), or -1 when not grouped. */
static int ait_root(uint32_t e,uint32_t i,uint32_t count)
{
    int t=ait_link(e,i,count);
    if(t>=0) return t;
    return ait_has_copies(e,i,count)?(int)i:-1;
}
/* Group logic is a leader's flag: drop it wherever it no longer applies. */
static void ait_tidy(uint32_t e,uint32_t count)
{
    uint32_t k,a;
    for(k=0;k<count;k++) {
        a=ait_entry(e,k)+AIT_LINK;
        if((WORD_AT(a)&AIT_GROUP) && (ait_raw(e,k) || !ait_has_copies(e,k,count))) WORD_AT(a)&=~AIT_GROUP;
    }
}
/* The leader leaves (unlink or delete): its first copy leads the rest, with
 * group logic off (its own logic keeps meaning "this AI"). */
static void ait_promote(uint32_t e,uint32_t leader,uint32_t count)
{
    uint32_t k;
    int next=-1;
    for(k=0;k<count;k++) {
        if(ait_link(e,k,count)!=(int)leader) continue;
        if(next<0) { next=(int)k;WORD_AT(ait_entry(e,k)+AIT_LINK)=0; }
        else WORD_AT(ait_entry(e,k)+AIT_LINK)=(uint32_t)next+1u;
    }
    WORD_AT(ait_entry(e,leader)+AIT_LINK)=0;
    ait_tidy(e,count);
}
static void ait_copy_shared(uint32_t e,uint32_t from,uint32_t to)
{
    uint32_t a=ait_entry(e,from),b=ait_entry(e,to),type;
    unsigned i;
    for(i=0;i<AIT_SHARED;i++) WORD_AT(b+ait_shared[i])=WORD_AT(a+ait_shared[i]);
    /* sub_0031CAC0: only types 1 and 3 keep more than one point */
    type=WORD_AT(b+AIT_TYPE);
    if(WORD_AT(b+AIT_POINTS)>1u && type!=1u && type!=3u) WORD_AT(b+AIT_POINTS)=1u;
}
/* Every setting and point; the copy keeps its own name, in-use flag, index,
 * colour and link. */
static void ait_copy_all(uint32_t e,uint32_t from,uint32_t to)
{
    uint32_t a=ait_entry(e,from),b=ait_entry(e,to),o;
    for(o=0x28u;o<0x60u;o+=4u) if(o!=AIT_LINK) WORD_AT(b+o)=WORD_AT(a+o);
    for(o=AIT_POINT0;o<AIT_STRIDE;o+=4u) WORD_AT(b+o)=WORD_AT(a+o);
}
static void ait_sync_from(uint32_t e,uint32_t s,uint32_t count)
{
    int root=ait_root(e,s,count);
    uint32_t k;
    if(root<0) return;
    for(k=0;k<count;k++)
        if(k!=s && ((int)k==root || ait_link(e,k,count)==root)) ait_copy_shared(e,s,k);
}
static void ait_snapshot(uint32_t e,uint32_t s,uint32_t count)
{
    unsigned i;
    ait.pending=0;
    if(s>=count || ait_root(e,s,count)<0) return;
    for(i=0;i<AIT_SHARED;i++) ait.shared[i]=WORD_AT(ait_entry(e,s)+ait_shared[i]);
    ait.editor=e;ait.index=s;ait.colour=WORD_AT(ait_entry(e,s)+0x60u);ait.pending=1;
}
/* A remembered group member whose shared settings changed since its edit
 * screen opened is copied to the rest of its group. The member must still be
 * the same entry (same editor object, index in range, same colour id).
 * keep: the edit screen may still be open (a compile for undo or save), so
 * remember the current values instead of forgetting the member. */
static void ait_flush(uint32_t e,int keep)
{
    uint32_t count=ait_count(e),s=ait.index;
    unsigned i;
    if(!ait.pending) return;
    ait.pending=0;
    if(ait.editor!=e || s>=count || WORD_AT(ait_entry(e,s)+0x60u)!=ait.colour) return;
    for(i=0;i<AIT_SHARED;i++)
        if(WORD_AT(ait_entry(e,s)+ait_shared[i])!=ait.shared[i]) {
            ait_sync_from(e,s,count);
            break;
        }
    if(keep) ait_snapshot(e,s,count);
}

/* ---- copy names: "<base> N" ---------------------------------------------- */
#define AIT_BASE_MAX 27u                /* 31 - " 999" */
/* Length of name without a trailing " <1-3 digits>", or 0 when it has none. */
static size_t ait_unnumbered(const char *n,size_t len)
{
    size_t d=0;
    while(d<len && d<3u && n[len-1u-d]>='0' && n[len-1u-d]<='9') d++;
    if(!d || d>=len || n[len-1u-d]!=' ') return 0;
    return len-1u-d;
}
/* Number N of name == base + " N" (N 1..999), 1 for name == base, else 0. */
static unsigned ait_number_of(const char *name,const char *base,size_t bl)
{
    size_t k;
    unsigned v=0;
    if(strncmp(name,base,bl)) return 0;
    if(!name[bl]) return 1;
    if(name[bl]!=' ' || !name[bl+1u]) return 0;
    for(k=bl+1u;name[k];k++) {
        if(name[k]<'0' || name[k]>'9' || k>bl+3u) return 0;
        v=v*10u+(unsigned)(name[k]-'0');
    }
    return v;
}
static size_t ait_strnlen(const char *s,size_t max) { size_t n=0; while(n<max && s[n]) n++; return n; }
/* Names copy c of the group led by root: the leader's name, without its own
 * number when another member carries the same base (a promoted "Guard 2"
 * next to "Guard 3"), plus the smallest number >= 2 no Story AI uses. */
static void ait_copy_name(uint32_t e,uint32_t root,uint32_t c,uint32_t count)
{
    char base[32],out[32];
    static uint8_t used[1000];
    size_t bl=ait_strnlen(ait_name(e,root),31u),u;
    uint32_t k;
    unsigned n;
    memcpy(base,ait_name(e,root),bl);base[bl]=0;
    if((u=ait_unnumbered(base,bl))!=0)
        for(k=0;k<count;k++) {
            char name[32];
            if(k==root || k==c || ait_root(e,k,count)!=(int)root) continue;
            memcpy(name,ait_name(e,k),31);name[31]=0;
            if(ait_number_of(name,base,u)>1u) { bl=u;base[bl]=0;break; }
        }
    if(bl>AIT_BASE_MAX) { bl=AIT_BASE_MAX;base[bl]=0; }
    memset(used,0,sizeof used);
    for(k=0;k<count;k++) {
        char name[32];
        if(k==c) continue;
        memcpy(name,ait_name(e,k),31);name[31]=0;
        used[ait_number_of(name,base,bl)]=1;
    }
    for(n=2;n<999u && used[n];n++) {}
    snprintf(out,sizeof out,"%s %u",base,n);
    memset((char *)ait_name(e,c),0,32);
    memcpy((char *)ait_name(e,c),out,strlen(out));
}

/* ---- edit screen rows -------------------------------------------------- */
static uint32_t ait_text(unsigned i)
{
    static const char *const label[AIT_LABELS]={"Duplicate Story AI","Add linked copy","Unlink copy",
        "Logic means: this AI","Logic means: whole group","Unlink (next copy leads)"};
    unsigned k;
    if(!ait.text) {
        ait.text=xbox_HeapAlloc(32u*AIT_LABELS,4u);
        if(!ait.text) return 0;
        for(k=0;k<AIT_LABELS;k++) {
            char *p=(char *)((uintptr_t)(ait.text+32u*k)+g_xbox_mem_offset);
            memset(p,0,32);
            memcpy(p,label[k],strlen(label[k]));
        }
    }
    return ait.text+32u*i;
}
static void ait_row(unsigned label,uint32_t id)
{
    uint32_t text=ait_text(label);
    if(text) guest_call(sub_00079D60,6,text,id,0u,0u,AIT_CLOSE,0u);
}

/* sub_000784A0(spacing): the edit screen calls it once at 0x0031D07E, just
 * before its "Done" row whose callback is the builder's second argument. */
void pc_mapmaker_aitemplates_rows(void (*original)(void))
{
    uint32_t r=0,cb=0,e,s,count;
    if(MAPMAKER_STACK_OK(0x28)) { r=WORD_AT(g_esp);cb=WORD_AT(g_esp+0x24u); } /* [esp+0x20] after the return */
    original();
    if(r!=AIT_ROWS_AT || cb!=AIT_CLOSE || !ait_on() || WORD_AT(AIT_SCENE)!=102u || !(e=ait_editor())) return;
    count=ait_count(e);s=ait_selected();
    if(s>=count || ait_type(e,s)==5u) return;
    if(ait_link(e,s,count)>=0) {
        ait_row(1,AIT_ROW_LINKED);
        ait_row(2,AIT_ROW_UNLINK);
    } else if(ait_has_copies(e,s,count)) {
        ait_row(1,AIT_ROW_LINKED);
        ait_row(WORD_AT(ait_entry(e,s)+AIT_LINK)&AIT_GROUP?4u:3u,AIT_ROW_LOGIC);
        ait_row(5,AIT_ROW_UNLINK);
    } else {
        ait_row(0,AIT_ROW_DUPLICATE);
        ait_row(1,AIT_ROW_LINKED);
    }
}

/* One new entry through the guest's add (sub_0031BE70 -> the editor area's
 * sai_add, menu 0 = no list rebuild); its index, or -1. */
static int ait_add(uint32_t e)
{
    uint32_t c=ait_count(e);
    guest_call(sub_0031BE70,1,0u);
    if(ait_count(e)!=c+1u) { ait_log("add failed",c,ait_count(e));return -1; }
    WORD_AT(ait_entry(e,c)+AIT_LINK)=0;
    return (int)c;
}
/* Returns 0 when refused (message shown, edit screen stays open). */
static int ait_action(uint32_t e,uint32_t row,uint32_t s,uint32_t menu)
{
    uint32_t count,from,k;
    int root,c;
    ait_flush(e,0);
    count=ait_count(e);
    root=ait_root(e,s,count);
    if(row==AIT_ROW_UNLINK) {
        if(root==(int)s) ait_promote(e,s,count);
        else if(root>=0) { WORD_AT(ait_entry(e,s)+AIT_LINK)=0;ait_tidy(e,count); }
        return 1;
    }
    if(row==AIT_ROW_LOGIC) {
        if(root==(int)s) WORD_AT(ait_entry(e,s)+AIT_LINK)^=AIT_GROUP;
        return 1;
    }
    if(count+1u>AIT_CAP) {
        guest_call(sub_003C85D4,3,e,guest_call(sub_0003D7B0,1,0xBB0u),AIT_CAP);
        guest_call(sub_0007A270,3,menu,e,0u);
        return 0;
    }
    if(!guest_call(sub_00300670,1,AIT_CHUNK_BYTES)) {
        guest_call(sub_0007A270,3,menu,guest_call(sub_0003D7B0,1,0x9B6u),0u);
        return 0;
    }
    if(row==AIT_ROW_DUPLICATE) {
        if((c=ait_add(e))>=0) ait_copy_all(e,s,(uint32_t)c);
        return 1;
    }
    if(root<0) root=(int)s;                      /* first copy: s leads */
    /* points from this AI, or from the first member that has some */
    from=s;
    if(!WORD_AT(ait_entry(e,s)+AIT_POINTS))
        for(k=0;k<count;k++)
            if(ait_root(e,k,count)==root && WORD_AT(ait_entry(e,k)+AIT_POINTS)) { from=k;break; }
    if((c=ait_add(e))<0) return 1;
    ait_copy_all(e,from,(uint32_t)c);
    for(k=0;k<AIT_SHARED;k++)
        WORD_AT(ait_entry(e,c)+ait_shared[k])=WORD_AT(ait_entry(e,s)+ait_shared[k]);
    WORD_AT(ait_entry(e,c)+AIT_LINK)=(uint32_t)root+1u;
    ait_copy_name(e,(uint32_t)root,(uint32_t)c,ait_count(e));
    return 1;
}

/* sub_0031CAC0(menu): edit screen close, also the added rows' callback. */
void pc_mapmaker_aitemplates_close(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,row,s;
    if(!ait_on() || !(e=ait_editor())) { original();return; }
    row=menu && MAPMAKER_PTR_OK(menu,0x190u)?WORD_AT(menu+0x18Cu):0u;
    s=ait_selected();
    if((row==AIT_ROW_DUPLICATE || row==AIT_ROW_LINKED || row==AIT_ROW_UNLINK || row==AIT_ROW_LOGIC) &&
       s<ait_count(e) && ait_type(e,s)!=5u && !ait_action(e,row,s,menu)) {
        MAPMAKER_GUEST_RET();                     /* sub_0031CAC0: ret */
        return;
    }
    original();
    ait_flush(e,0);                               /* after the stock point clamp */
}

/* sub_0031CB90(menu, close, flags): edit screen build, list and map paths. */
void pc_mapmaker_aitemplates_open(void (*original)(void))
{
    uint32_t e;
    if(ait_on() && (e=ait_editor())) {
        ait_flush(e,0);
        ait_snapshot(e,ait_selected(),ait_count(e));
    }
    original();
}

/* sub_0031C8B0(index): delete one Story AI (logic fix-up, then memmove). A
 * leader hands its group to its first copy first. */
void pc_mapmaker_aitemplates_delete(void (*original)(void))
{
    uint32_t i=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0xFFFFFFFFu,e,count,k,l,a;
    if(!ait_on() || !(e=ait_editor()) || i>=AIT_CAP) { original();return; }
    ait_flush(e,0);
    count=ait_count(e);
    if(i<count && ait_root(e,i,count)==(int)i) ait_promote(e,i,count);
    original();
    count=ait_count(e);
    for(k=0;k<count;k++) {                        /* a linked entry is a copy: no flag */
        a=ait_entry(e,k)+AIT_LINK;
        l=WORD_AT(a)&AIT_LINK_MASK;
        if(l==i+1u) WORD_AT(a)=0;
        else if(l>i+1u) WORD_AT(a)=l-1u;
    }
    ait_tidy(e,count);
}

/* ---- save / load -------------------------------------------------------- */
static uint32_t ait_get16(uint32_t a) { const uint8_t *p=(const uint8_t *)((uintptr_t)a+g_xbox_mem_offset); return p[0]|(uint32_t)p[1]<<8; }
static void ait_put16(uint32_t a,uint32_t v) { uint8_t *p=(uint8_t *)((uintptr_t)a+g_xbox_mem_offset); p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }

/* Visit each Story AI chunk's data in order; stops at the end tag, a malformed
 * size, the scan bound or after max chunks. Returns the number visited. */
typedef void (*AitChunkFn)(uint32_t data,unsigned ordinal,void *ctx);
static unsigned ait_walk(uint32_t container,unsigned max,AitChunkFn fn,void *ctx)
{
    uint32_t c,end;
    unsigned n=0,steps;
    if(!container || !MAPMAKER_PTR_OK(container,AIT_SCAN) || WORD_AT(container)!=AIT_MAGIC) return 0;
    c=container+8u;end=container+AIT_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==AIT_TAG_END || sz>end-c-8u) break;
        if(tag==AIT_TAG_SAI && sz>=AIT_CHUNK_LINK+2u) {
            if(n>=max) break;
            fn(c+8u,n++,ctx);
        }
        c+=8u+sz;
    }
    return n;
}

typedef struct { uint32_t e, count; } AitCompile;
static void ait_compile_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    const AitCompile *x=(const AitCompile *)ctx;
    uint32_t i=ait_get16(data+AIT_CHUNK_INDEX),v=0;
    int t=i<x->count?ait_link(x->e,i,x->count):-1;
    (void)ordinal;
    if(t>=0) v=AIT_SAVE_NEW|((uint32_t)t+1u);
    else if(i<x->count && ait_type(x->e,i)!=5u && ait_has_copies(x->e,i,x->count))
        v=AIT_SAVE_NEW|(WORD_AT(ait_entry(x->e,i)+AIT_LINK)&AIT_GROUP?AIT_SAVE_GROUP:0u);
    ait_put16(data+AIT_CHUNK_LINK,v);
}
/* sub_003006C0(container, flags): compile the editor into a container. */
void pc_mapmaker_aitemplates_compile(void (*original)(void))
{
    uint32_t container=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e;
    AitCompile x;
    if(!ait_on() || !(e=ait_editor())) { original();return; }
    ait_flush(e,1);
    original();
    x.e=e;x.count=ait_count(e);
    ait_walk(container,x.count,ait_compile_chunk,&x);
}

typedef struct { uint32_t index[AIT_CAP], word[AIT_CAP]; } AitLoad;
static void ait_load_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    AitLoad *x=(AitLoad *)ctx;
    x->index[ordinal]=(uint32_t)(int32_t)(int16_t)ait_get16(data+AIT_CHUNK_INDEX);
    x->word[ordinal]=ait_get16(data+AIT_CHUNK_LINK);
}
/* sub_00301A90(container, flags): editor load (appends the chunks). Leader
 * format words carry 0x4000; a plain link is the old template format: the
 * template's first copy leads, the template stays an unlinked AI. */
void pc_mapmaker_aitemplates_load(void (*original)(void))
{
    static AitLoad x;
    static int16_t lead[AIT_CAP];
    static uint8_t ok[AIT_CAP];
    uint32_t container=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,e,count,base,k,l,w;
    unsigned n;
    original();
    if(!ait_on() || !(e=ait_editor())) return;
    ait.pending=0;
    n=ait_walk(container,AIT_CAP,ait_load_chunk,&x);
    count=ait_count(e);
    if(n>count) n=count;
    base=count-n;
    for(k=0;k<n;k++) lead[k]=-1;
    for(k=0;k<n;k++) {                            /* a valid link: to an unlinked chunk */
        w=x.word[k];l=w&AIT_SAVE_LINK;
        ok[k]=l && (w&AIT_SAVE_GROUP)==0u && l-1u<n && l-1u!=k && !(x.word[l-1u]&AIT_SAVE_LINK) &&
              WORD_AT(ait_entry(e,base+k)+0x24u)==x.index[k];
        if(l && !ok[k]) { ait.dropped_links++;ait_log("link dropped at load",k,w); }
        if(ok[k] && !(w&AIT_SAVE_NEW) && lead[l-1u]<0) lead[l-1u]=(int16_t)k;
    }
    for(k=0;k<n;k++) {
        uint32_t entry=ait_entry(e,base+k),value=0;
        w=x.word[k];l=w&AIT_SAVE_LINK;
        if(ok[k] && (w&AIT_SAVE_NEW)) value=base+l;
        else if(ok[k]) {                          /* old template format */
            if(lead[l-1u]!=(int16_t)k) value=base+(uint32_t)lead[l-1u]+1u;
            else { ait.converted++;ait_log("old template group converted",l-1u,k); }
        } else if(!l && (w&(AIT_SAVE_NEW|AIT_SAVE_GROUP))==(AIT_SAVE_NEW|AIT_SAVE_GROUP)) value=AIT_GROUP;
        WORD_AT(entry+AIT_LINK)=value;
    }
    ait_tidy(e,count);
}

/* ---- play (scene 100) ---------------------------------------------------- */
typedef struct { uint32_t index[AIT_LEVEL_SLOTS], word[AIT_LEVEL_SLOTS]; } AitLevel;
static void ait_level_chunk(uint32_t data,unsigned ordinal,void *ctx)
{
    AitLevel *x=(AitLevel *)ctx;
    x->index[ordinal]=ait_get16(data+AIT_CHUNK_INDEX);
    x->word[ordinal]=ait_get16(data+AIT_CHUNK_LINK);
}
/* sub_00240C50: a MapMaker map starts; build index -> leader. A leader's id
 * means "any member" with group logic (0x8000) or for an old template. */
void pc_mapmaker_aitemplates_level(void (*original)(void))
{
    static AitLevel x;
    static uint32_t word_of[AIT_LEVEL_SLOTS];
    static uint8_t present[AIT_LEVEL_SLOTS];
    unsigned n,k,groups=0;
    original();
    memset(&ait_level,0,sizeof ait_level);
    if(!ait_on()) return;
    n=ait_walk(WORD_AT(AIT_CONTAINER),AIT_LEVEL_SLOTS,ait_level_chunk,&x);
    memset(present,0,sizeof present);
    for(k=0;k<AIT_LEVEL_SLOTS;k++) ait_level.template_of[k]=-1;
    for(k=0;k<n;k++)
        if(x.index[k]<AIT_LEVEL_SLOTS) { present[x.index[k]]=1;word_of[x.index[k]]=x.word[k]; }
    for(k=0;k<AIT_LEVEL_SLOTS;k++) {
        uint32_t w,l,t;
        if(!present[k] || !(l=(w=word_of[k])&AIT_SAVE_LINK)) continue;
        t=l-1u;
        if((w&AIT_SAVE_GROUP) || t>=AIT_LEVEL_SLOTS || t==k || !present[t] || (word_of[t]&AIT_SAVE_LINK)) {
            ait_log("link ignored in play",k,w);
            continue;
        }
        ait_level.template_of[k]=(int16_t)t;
        if(w&AIT_SAVE_NEW) ait_level.group[t]=(uint8_t)((word_of[t]&(AIT_SAVE_NEW|AIT_SAVE_GROUP))==(AIT_SAVE_NEW|AIT_SAVE_GROUP));
        else ait_level.group[t]=1;                /* old template: "any copy" */
        if(k+1u>ait_level.n) ait_level.n=k+1u;
    }
    for(k=0;k<AIT_LEVEL_SLOTS;k++) groups+=ait_level.group[k];
    ait_level.active=groups!=0;
}
static int ait_play(void) { return ait_level.active && WORD_AT(AIT_SCENE)==100u && mapmaker_limits.limits; }
static uint32_t ait_play_index(uint32_t id) { return id-WORD_AT(AIT_SAI_BASE)-1u; }
static uint32_t ait_play_id(uint32_t index) { return WORD_AT(AIT_SAI_BASE)+index+1u; }
/* Leader of copy i when its id means the whole group, or -1. */
static int ait_play_leader(uint32_t i)
{
    int t;
    if(i>=AIT_LEVEL_SLOTS || (t=ait_level.template_of[i])<0 || !ait_level.group[t]) return -1;
    return t;
}

/* sub_0033E560(actor): flag kind-2 triggers/actions of the actor's id. */
void pc_mapmaker_aitemplates_event(void (*original)(void))
{
    uint32_t actor=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,a,record,id;
    int t;
    original();
    if(!ait_play() || !MAPMAKER_PTR_OK(actor,0x80u)) return;
    a=WORD_AT(actor+0x7Cu);
    if(!MAPMAKER_PTR_OK(a,0xBF4u)) return;
    record=WORD_AT(a+0xBF0u);
    if(!MAPMAKER_PTR_OK(record,12u)) return;
    id=WORD_AT(record+8u);
    if((t=ait_play_leader(ait_play_index(id)))<0) return;
    WORD_AT(record+8u)=ait_play_id((uint32_t)t);
    guest_call(original,1,actor);
    WORD_AT(record+8u)=id;
}

/* sub_00163780(id): action type 2 on every live actor of that id. */
void pc_mapmaker_aitemplates_remove(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,t,k;
    original();
    if(!ait_play() || (t=ait_play_index(id))>=AIT_LEVEL_SLOTS || !ait_level.group[t]) return;
    for(k=0;k<ait_level.n;k++)
        if(ait_level.template_of[k]==(int)t) guest_call(original,1,ait_play_id(k));
}

/* sub_0033E620(id): 1 when a type 2/3/4 AI may spawn (action 0x11). */
void pc_mapmaker_aitemplates_spawn_gate(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,r;
    int t;
    original();
    r=g_eax;
    if(!r || !ait_play() || (t=ait_play_leader(ait_play_index(id)))<0) return;
    g_eax=guest_call(original,1,ait_play_id((uint32_t)t));
}

/* sub_00339600(trigger): trigger 0x14 poll; id at +0x14, state at +0x18
 * (7 = found, waiting for its delay). */
void pc_mapmaker_aitemplates_alert(void (*original)(void))
{
    uint32_t trigger=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0u,id,t,k,state,r;
    if(!ait_play() || !MAPMAKER_PTR_OK(trigger,0x20u) ||
       (t=ait_play_index(id=WORD_AT(trigger+0x14u)))>=AIT_LEVEL_SLOTS || !ait_level.group[t]) {
        original();
        return;
    }
    state=WORD_AT(trigger+0x18u);
    original();
    r=g_eax;
    for(k=0;k<ait_level.n && !r && !(state!=7u && WORD_AT(trigger+0x18u)==7u);k++) {
        if(ait_level.template_of[k]!=(int)t) continue;
        WORD_AT(trigger+0x14u)=ait_play_id(k);
        r=guest_call(original,1,trigger);
        WORD_AT(trigger+0x14u)=id;
    }
    g_eax=r;
}
