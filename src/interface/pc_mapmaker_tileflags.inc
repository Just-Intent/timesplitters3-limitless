/* US 45410066 MapMaker tile roof flags from a right click (docs/mapmaker/mapmaker-sky.md,
 * "Tile roof menu"). User request (2026-11-01): in Tiles mode a right click on
 * any placed tile opens a menu to flag it No roof or Open air, instead of
 * selecting it. The flags are the roof area's per-piece marks
 * (pc_mapmaker_roof.inc: mask, save word +0x26, load match, copy/move), which
 * already work for every tile id (the 46/47 tile-8 palette presets were
 * removed on 2026-11-07; maps that used them load with these flags).
 * Second request (2026-11-01): Open air is split into Open air (bounded:
 * invisible walls keep you from falling off the edge) and Open air
 * (unbounded: falling off the edge is possible and kills the player).
 *
 * CONFIRMED (decode, tests/mapmaker/test_mapmaker_tileflags.py):
 *  - the editor's per-frame input dispatch sub_003063C0 jumps on toolbar slot
 *    editor+0x254 (table 0x00306444): slot 0, Tiles, calls sub_00303E10
 *    (0x003063D9, its only caller). There Back (menu +0xC4 & 0x20, what a
 *    right click sends) rotates a held piece (+0x2F7C, sub_00303D00(1),
 *    0x003042A8) and otherwise flips the selection bit 2 of the piece under
 *    the pointer (+0x2F18; byte +0x14, count +0x2F9C; 0x003042E4..0x0030431E).
 *  - Objects mode opens a placed object's options with sub_002FB230(desc)
 *    (0x003051DB): it sets editor +0x25C |= 0x4000000 and pushes the menu with
 *    sub_00079860(root, desc, -1) (0x002FB2A5; menu +4 = desc, compared at
 *    0x000798B4). For slot 0 the toolbar table 0x0052CD78 has no open/close
 *    callbacks and +8 = -1, so its close sub_002FB3C0 only clears the flag.
 *  - Descriptor 0x0052DECC {sub_00319300, sub_003192E0, 0, 0} is the pickup
 *    options menu: build(menu) lays out the frame (sub_00075B70(0x8400, 0, 50,
 *    0, sub_002FB3C0), menu +0x218 = 1, sub_00071B10(1, 0.5, 1, 1),
 *    sub_00071C50(0x20000)), its rows, a Done row (text 0xB5E, sub_00079D60
 *    with callback sub_002FB3C0), sub_00075E70 and the position call
 *    sub_00077320([0x007BB1E8], [0x0049F7E8] - menu +0x68); its tick(menu)
 *    closes on Back (sub_00071880 / sub_00078980).
 *
 * This file: a descriptor of its own on the heap {sub_00319300, sub_003192E0,
 * 0, 0}, so the stock build and tick run for it and these hooks know it by
 * menu +4.
 *  - click (pc_mapmaker_tileflags_click, from the pointer's right-click release
 *    on the editor screen): Tiles mode, nothing held, a piece under the
 *    pointer, no editor menu open -> remembers the piece; Back is not sent.
 *  - tiles (sub_00303E10): with a remembered piece still under the pointer,
 *    opens the menu with sub_002FB230 and skips that frame's tile input.
 *  - build (sub_00319300): for this descriptor, the stock frame and Done row
 *    around one choice row "Roof": Normal / No roof / Open air (bounded) /
 *    Open air (unbounded) (built like the weaponset area's pickup Weapon
 *    choice), bound to a heap word holding the row index.
 *  - tick (sub_003192E0): applies the choice to the piece (and, when that
 *    piece is selected, to every selected piece) with the roof marks; rows
 *    map to modes 0 / 1 / 3 / 2 (tf_modes).
 * Gamepad B and Escape (Back) still select tiles. Requires pc_mapmaker_roof.inc
 * (included before), WORD_AT, FLOAT_AT, guest_call, g_esp, xbox_HeapAlloc,
 * MAPMAKER_PTR_OK / MAPMAKER_STACK_OK / MAPMAKER_GUEST_RET. */

#define TF_EDITOR 0x007844A8u
#define TF_SLOT 0x254u                  /* toolbar slot; 0 = Tiles */
#define TF_FLAGS 0x25Cu
#define TF_MENU_OPEN 0x4000000u         /* set by sub_002FB230 */
#define TF_UNDER 0x2F18u                /* piece under the pointer */
#define TF_HELD 0x2F7Cu
#define TF_LIST 0x0078400Cu             /* editor pieces, next +0x40 */
#define TF_SELECTED 2u                  /* piece byte +0x14 */
#define TF_LIST_STEPS 4096u
#define TF_CLOSE 0x002FB3C0u            /* the options menus' Done / close callback */
#define TF_BUILD 0x00319300u
#define TF_TICK 0x003192E0u
#define TF_DONE_TEXT 0xB5Eu
#define TF_POS_BASE 0x0049F7E8u
#define TF_POS_LIST 0x007BB1E8u
#define TF_TEXT_BYTES 24u
#define TF_CHOICES 4u
static const char tf_texts[1u+TF_CHOICES][TF_TEXT_BYTES]={"Roof","Normal","No roof","Open air (bounded)","Open air (unbounded)"};
/* Choice row index <-> roof mode (pc_mapmaker_roof.inc): the rows are in the
 * order the user asked for, the modes keep their saved values (0x5202 =
 * unbounded for maps saved before the split). The map is its own inverse. */
static const uint8_t tf_modes[TF_CHOICES]={0u,MAPMAKER_ROOF_OPEN_ROOF,MAPMAKER_ROOF_OPEN_BOUNDED,MAPMAKER_ROOF_OPEN_AIR};

extern void sub_002FB230(void),sub_00075B70(void),sub_00071B10(void),sub_00071C50(void),sub_00079DE0(void),
    sub_0007D4A0(void),sub_00071800(void),sub_00078590(void),sub_000783D0(void),sub_00079D60(void),
    sub_00075E70(void),sub_00077320(void),sub_0003D7B0(void);

static struct {
    uint32_t desc, value, texts;       /* heap: descriptor (16), choice word, strings */
    uint32_t request, piece, tile;
    unsigned applied, opened, changed, group, failed;
} tf;

static uint32_t tf_editor(void)
{
    uint32_t e;
    mapmaker_limits_read();
    if(!mapmaker_limits.limits || WORD_AT(0x0079094Cu)!=102u) return 0;
    e=WORD_AT(TF_EDITOR);
    return MAPMAKER_PTR_OK(e,0x2FA0u)?e:0u;
}
static int tf_alloc(void)
{
    if(!tf.desc) {
        uint32_t d=xbox_HeapAlloc(16u,4u),v=xbox_HeapAlloc(4u,4u),t=xbox_HeapAlloc(sizeof tf_texts,4u);
        if(!d || !v || !t || !MAPMAKER_PTR_OK(d,16u) || !MAPMAKER_PTR_OK(v,4u) || !MAPMAKER_PTR_OK(t,sizeof tf_texts)) {
            if(tf.failed++<4u) fprintf(stderr,"[mapmaker] tileflags: no memory for the tile roof menu\n");
            return 0;
        }
        WORD_AT(d)=TF_BUILD;WORD_AT(d+4u)=TF_TICK;WORD_AT(d+8u)=0;WORD_AT(d+12u)=0;
        memcpy((void *)((uintptr_t)t+g_xbox_mem_offset),tf_texts,sizeof tf_texts);
        tf.desc=d;tf.value=v;tf.texts=t;
    }
    return 1;
}
static uint32_t tf_text(unsigned k) { return tf.texts+k*TF_TEXT_BYTES; }

/* The pointer's right click on the editor screen: 1 = taken (no Back). */
int pc_mapmaker_tileflags_click(void)
{
    uint32_t e=tf_editor(),piece;
    if(!e || WORD_AT(e+TF_SLOT)!=0u || WORD_AT(e+TF_HELD) || (WORD_AT(e+TF_FLAGS)&TF_MENU_OPEN)) return 0;
    if(!(piece=WORD_AT(e+TF_UNDER)) || !MAPMAKER_PTR_OK(piece,0x48u)) return 0;
    tf.request=piece;
    return 1;
}

/* WRAP sub_00303E10 (Tiles mode input, cdecl one argument). */
void pc_mapmaker_tileflags_tiles(void (*original)(void))
{
    uint32_t piece=tf.request,e;
    if(!piece) { original();return; }
    tf.request=0;
    if(!(e=tf_editor()) || WORD_AT(e+TF_SLOT)!=0u || WORD_AT(e+TF_UNDER)!=piece || WORD_AT(e+TF_HELD) ||
       (WORD_AT(e+TF_FLAGS)&TF_MENU_OPEN) || !MAPMAKER_PTR_OK(piece,0x48u) || !MAPMAKER_STACK_OK(8u) || !tf_alloc()) { original();return; }
    tf.piece=piece;tf.tile=WORD_AT(piece+0x10u);
    tf.group=(WORD_AT(piece+0x14u)&TF_SELECTED)!=0;
    tf.applied=mapmaker_roof_mode(piece);
    tf.applied=tf.applied<TF_CHOICES?tf_modes[tf.applied]:0u;     /* the row of the tile's mode */
    WORD_AT(tf.value)=tf.applied;
    guest_call(sub_002FB230,1,tf.desc);
    if(tf.opened++<4u) fprintf(stderr,"[mapmaker] tileflags: roof menu opened for tile %u (%s)\n",(unsigned)tf.tile,
                               tf.group?"and the selected tiles":"this tile");
    MAPMAKER_GUEST_RET();                               /* no tile input this frame */
}

/* WRAP sub_00319300 (options menu build, cdecl menu). */
void pc_mapmaker_tileflags_build(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,y;
    unsigned k;
    float f;
    if(!tf.desc || !MAPMAKER_PTR_OK(menu,0x220u) || WORD_AT(menu+4u)!=tf.desc) { original();return; }
    guest_call(sub_00075B70,5,0x8400u,0u,0x42480000u,0u,TF_CLOSE);
    WORD_AT(menu+0x218u)=1u;
    guest_call(sub_00071B10,4,1u,0x3F000000u,1u,1u);
    guest_call(sub_00071C50,1,0x20000u);
    guest_call(sub_00079DE0,5,tf_text(0),1u,0x08000800u,0u,0u);
    guest_call(sub_0007D4A0,2,0u,0u);
    for(k=0;k<TF_CHOICES;k++) guest_call(sub_00071800,2,tf_text(k+1u),(uint32_t)k);
    guest_call(sub_00078590,1,tf.value);
    guest_call(sub_000783D0,0);
    guest_call(sub_00079D60,6,guest_call(sub_0003D7B0,1,TF_DONE_TEXT),0xFFFFFFFFu,0u,0x40000000u,TF_CLOSE,0u);
    guest_call(sub_00075E70,0);
    f=FLOAT_AT(TF_POS_BASE)-FLOAT_AT(menu+0x68u);
    memcpy(&y,&f,4);
    guest_call(sub_00077320,2,WORD_AT(TF_POS_LIST),y);
    MAPMAKER_GUEST_RET();
}

static void tf_mark(uint32_t piece,unsigned mode)
{
    if(mapmaker_roof_mode(piece)!=mode) mapmaker_roof_set(piece,mode);
}
/* WRAP sub_003192E0 (options menu tick, cdecl menu): apply the choice. */
void pc_mapmaker_tileflags_tick(void (*original)(void))
{
    uint32_t menu=MAPMAKER_STACK_OK(8u)?WORD_AT(g_esp+4u):0u,v,p;
    unsigned steps,n=0;
    if(tf.desc && MAPMAKER_PTR_OK(menu,8u) && WORD_AT(menu+4u)==tf.desc && tf.piece && (v=WORD_AT(tf.value))<TF_CHOICES &&
       v!=tf.applied && tf_editor() && MAPMAKER_PTR_OK(tf.piece,0x48u) && WORD_AT(tf.piece+0x10u)==tf.tile) {
        tf_mark(tf.piece,tf_modes[v]);n=1;
        if(tf.group)
            for(steps=0,p=WORD_AT(TF_LIST);p && steps<TF_LIST_STEPS && MAPMAKER_PTR_OK(p,0x48u);steps++,p=WORD_AT(p+0x40u))
                if(p!=tf.piece && (WORD_AT(p+0x14u)&TF_SELECTED)) { tf_mark(p,tf_modes[v]);n++; }
        tf.applied=v;
        if(tf.changed++<8u) fprintf(stderr,"[mapmaker] tileflags: %u tile%s set to %s\n",n,n==1u?"":"s",tf_texts[v+1u]);
    }
    original();
}

#ifdef TSFP_MAPMAKER_TILEFLAGS_TEST
static void tf_test_reset(void) { memset(&tf,0,sizeof tf); }
#endif
