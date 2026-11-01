/* US 45410066 MapMaker real story bosses: the giant trophy monster of story
 * mission 5 (the "deerbeast") and its giant skeleton (the mansion boss, at
 * the end of this comment) as MapMaker Story AIs.
 * Docs: docs/mapmaker/mapmaker-realbosses.md. Research basis:
 * logs/limits/features/real-bosses.md (main checkout, ignored). Area
 * `realbosses`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1.
 *
 * The creature (CONFIRMED, US XBE data and tests/mapmaker/test_mapmaker_realbosses.py):
 * character id 0x15 (character table 0x0053A3CC + 0x15*0x30: voice set
 * 0x00538B08, model 0x5BE CHR_MANSION_DEERBEAST, LOD 0x5BF), no Arcade-table
 * row. Its story behaviour is AI class 0x80 (AI +0x14 = 0x80, +0x10 = 0:
 * dispatcher sub_0027D590 0x0027DB94 -> sub_00280FA0 at 0x0027DBE2); its
 * eight animations are named only by pak/stream/lv1/anim/lv1_3.pak (string
 * 0x0049D77C, registered by sub_001CA2F0 at 0x001CADA3 when that pak is
 * mounted). The model is in pak/chr.pak, which a MapMaker map in play
 * (scene 100) mounts; sub_000A5E40 skips the story-only flag 0x2000000 in
 * scene 100 (0x000A5E83) and honours it in the editor (scene 0x66, 0x000A5E93).
 *
 * Stage 1, editor (scene 102): the custom character set picker
 * (descriptor 0x0052374C) lists Arcade rows only. This area adds one more
 * row, "Giant Trophy Monster", with the synthetic row id RBS_ROW (150, one
 * past the 150-row Arcade table):
 *  - picker_end (sub_000781A0, only its call at 0x002C1FA0 that closes the
 *    picker list, return 0x002C1FA5): before the original, add the row the
 *    way the stock loop does (sub_00079D60(text, 150, 0x20900, 0, 0x002C1C90,
 *    0) then sub_00071C70(sub_00071630(), 0): enabled).
 *  - pick (sub_002C1C90, the row callback): the original stores row 150's
 *    "id" and "variant" from past the table; afterwards every work-copy slot
 *    (0x0075FD90, 10 x 0x1C) holding row 150 gets id 0x15 and variant -1
 *    (sub_002BE270(0x15) == 0: the stock reload drops variants of this id).
 *  - row_of (sub_002BE600(id, variant) -> row or -1): for id 0x15, the slot
 *    reload in sub_002C0490 (return 0x002C054C) and the Story AI edit
 *    screen's name lookup (sub_002BE650 at 0x002BE65F, called from
 *    sub_00303350 0x003033BB -> 0x003033C0) get row 150; the slot screen's
 *    preview selection in sub_002C1570 (returns 0x002C1805/0x002C1814) gets
 *    the Arcade trophy monster row (id 0xA7), so the preview panel
 *    (sub_002C2B40 reads [0x0078AEA8] as an Arcade row) never sees 150.
 *  - highlight (sub_002C1E10, picker highlight: [0x0078AEA8] = row id):
 *    150 becomes the trophy monster row as well.
 *  - row_text (sub_002BE300(row) -> text id; stock returns 0 for row >= 150):
 *    row 150 -> RBS_TEXT; text (sub_0003D7B0(id) -> string) returns the
 *    label for RBS_TEXT. Every other id/row runs the original first thing.
 * Nothing in the editor requests model 0x5BE: model_request
 * (sub_000A5E40) logs such a request in scene 102 (the stock call skips the
 * flagged model there) and changes nothing.
 *
 * Save format: unchanged. The slot is stored as a stock custom-set member
 * (map-info chunk 0x00C77668 +0x18: word id 0x15, sbyte variant -1). No Story
 * AI chunk byte is used (+0x21 belongs to the health levels, +0x29 to the
 * Boss flag). A stock game or an older fork build loading the map: the
 * picker/slot screen shows the member with the empty text 0 (sub_002BE600 ->
 * -1, sub_002BE300(-1) -> 0 -> 0x004766D8), and play spawns id 0x15 with the
 * stock MapMaker class (0x11000000/0: sub_000D6D00 covers ids 0x16..0xA3
 * only) on the human rig - the stage 1 behaviour. Story mission 5 holds the
 * same id in its own set, so the id itself is stock data.
 *
 * Stage 2, play (scene 100, story mode byte [0x007DE455] == 10; opt out with
 * TSFP_MAPMAKER_REAL_BOSS=0 for the model-only creature):
 *  - anims (sub_001C8540, per-level anim pak mount; sub_0005C420 calls it at
 *    0x0005C501 and registers the names of every mounted anim pak with
 *    sub_001CA2F0 at 0x0005C558): after the original, when the map's custom
 *    set (index 0x11; slots 1-10 in the map-info chunk, 11-20 in the sets
 *    area's chunk 0x00C776F5, rbs_set_scan, logged as "set check found")
 *    has id 0x15 (or the container cannot be read), lv1_3.pak is not yet
 *    mounted (sub_00062260) and the 30-slot mount table [0x006D7880] has at
 *    least two free slots, mount it with sub_0005A130 (= sub_000622D0(path,
 *    1, 1): read whole, level heap) exactly as the stock per-level anim paks
 *    are; the stock level teardown (sub_00062720 from sub_0003D440) unmounts
 *    it with them.
 *  - ai_start (sub_0015B830(a), AI start; story mode copies spawn record
 *    +0/+4 to AI +0x10/+0x14 at 0x0015B9A3): before the original, for a pool
 *    entry whose character (a+0x48, the index sub_00280FA0 itself uses) is
 *    0x15 or whose entity model ([a]+0x94) is 0x5BE, and only when lv1_3.pak
 *    is mounted: record +0 = 0, +4 = 0x80 (MapMaker wrote 0x11000000/0 at
 *    0x002421A7). The AI also gets its Story AI chunk's Boss flag (+0x29,
 *    0xB0 | level) noted for the HUD hook. Without the pak it keeps the
 *    MapMaker class (stage 1) and says so once.
 *  - Level events: sub_00280FA0 and its damage callback sub_000BC4D0 cannot
 *    reach sub_002274C0 (static call graph, checked by the test), so no
 *    MapMaker logic marker can be fired by this creature; no guard is
 *    installed.
 *  - Boss bar: sub_00280FA0 sets the story focus [[0x007B0C7C]+0xFF4] itself
 *    (0x002811BE); sub_0014CC80 draws it for model 0x5BE with label text
 *    0xDCC (call 0x0014CED0 -> 0x0014CED5). In scene 100 that id is the
 *    MapMaker level's own text, so text returns the label there instead.
 *    hud (sub_0014CC80, chained INSIDE pc_mapmaker_bosses_hud): when the
 *    focus is a real boss noted by ai_start, the focus is cleared for the
 *    original call and restored after, so the creature's own bar is never
 *    drawn: with the Boss flag the bosses area draws that AI's single Boss
 *    bar (its name, health multiplier, stacking), or none for the Sub-boss
 *    bar choice; Boss Off shows no health at all (user, 2026-10-31). The
 *    Boss/health multipliers act on entity +0x1CC/+0x1D8 as for any Story AI.
 *  - meter (sub_0013A400, the overhead health meter): stock MapMaker story
 *    play draws it in sub_0014D1F0 (HUD, called at 0x001CEF19) only when the
 *    scene is 100 (0x0014D21C) and the mode byte is story (0x0014D225), for
 *    every AI-list actor (0x007A3580, filled with entity +0x7C by
 *    sub_0015BCA0) from index [0x00790950] to [0x0074C37C] whose entity has a
 *    damage-stage record at +0x268 and is not the player's own entity
 *    ([[0x007B0C7C]+0x14]): call sub_0013A400(actor, 0, 0x7F, 0x6E) at
 *    0x0014D25F, its only caller. MapMaker's own Story AIs never get such a
 *    record; the deerbeast handler makes one in its intro state
 *    (sub_0012C800 at 0x002811A3, stored at 0x002811AA), so in MapMaker the
 *    real boss gets a floating meter (placed for a human-sized actor, i.e.
 *    on its back). Mission 5 is scene 1: the scene test skips the loop. The
 *    hook skips the draw (sub_0013A400 only draws: billboard quads through
 *    sub_0001AC50, plain ret) for actors noted by ai_start whose character
 *    is 0x15 or entity model 0x5BE, unless their Story AI chose the
 *    Sub-boss bar; every other actor, scene and caller runs the original. The record itself stays (sub_0014CC80 and the damage
 *    flash in sub_0014D3A0 read it).
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax,
 * g_xbox_mem_offset, xbox_HeapAlloc, pc_mapmaker_area_enabled and
 * pc_mapmaker_limits.inc (mapmaker_limits, MAPMAKER_GUEST_RET,
 * MAPMAKER_PTR_OK). Hot hooks (text: every text lookup; hud: every HUD
 * frame; meter: per frame per recorded actor) test fixed values first and use MAPMAKER_PTR_OK only.
 *
 * Health display (bosses area row, Story AI chunk +0x29 bit 2), per real boss:
 * Boss Off = no health shown (no bottom bar, meter hidden); Boss bar = the bosses
 * area's bottom bar, meter hidden; Sub-boss bar = no bottom bar (focus
 * cleared for the stock call, bosses draws none), the stock meter kept.
 *
 * The giant skeleton of story mission 5 (the mansion boss, "Princess" in the
 * code; CONFIRMED, tests/mapmaker/test_mapmaker_realbosses.py): character id
 * 0x28 (character table: model 0x5E9 CHR_MANSION_MANSIONBOSS, no LOD), no
 * Arcade-table row; picker row RBS_SKEL_ROW (151), preview stand-in the
 * Arcade skeleton cowboy (id 0xA6). Behaviour: AI class 0x20000 (record +4;
 * sub_0027D590 0x0027DD54 -> call sub_0029FE00(entity) at 0x0027DDD3), which
 * tail-jumps to the mansion variant sub_0029CEF0 only in scene 1 and to the
 * catacombs variant sub_0029E0A0 otherwise. Its 18 anims (princess_start
 * 0x19C .. princess_eyeoverwrite 0x1A9) are named only by
 * pak/stream/lv1/anim/lv1_6.pak (string 0x0049D6FC, sub_001CA2F0 0x001CAE61);
 * their ids never collide with lv1_3's. Per-level state: sub_00287E00 =
 * [0x0078B1EC] = sub_0003DFA0(0x4C7C, 1), [0x0078B0D0] = sub_0003DFA0(0x7D00,
 * 1) (sub_002A62C0, an 800 x 0x28 particle block), then sub_00287860 sets
 * every field the fight reads before its first frame; stock calls it from
 * sub_0025F720 (story level init, sub_0015E7C0 0x0015E818) for scenes 1 and
 * 17 only. Neither block holds arena coordinates: the fight works from the
 * boss entity's own position (+0x64), its bone matrices and the players
 * ([0x007B0C48] + i*0x1584); its particle shape (sub_002A62F0(3, 1.5, 0, ..))
 * is local to a bone. So it emerges (princess_start) wherever its Story AI
 * spawns.
 *  - level (sub_0025F720): after the original, in a MapMaker story map whose
 *    custom set has id 0x28 (or an unreadable container), allocate the state
 *    with the stock sub_00287E00 (52.6 KB of the level heap, freed with the
 *    level like the stock blocks; the pointers are cleared first so a failed
 *    allocation is seen). Every level start forgets the previous level's.
 *  - ai_start: one giant skeleton per map gets record +0/+4 = 0/0x20000 when
 *    lv1_6.pak is mounted and this level's state exists; a second one (other
 *    spawn record) keeps the MapMaker class (logged, counted).
 *  - princess (sub_0029FE00(entity)): for that boss in scene 100, the mansion
 *    variant (a direct call: the callee pops the caller's return address,
 *    exactly the lifted tail jump).
 *  - message (sub_0028D0A0(entity, msg, ..), its message handler): mansion
 *    sub_00286D30 only in scene 1, else the catacombs sub_0028C560, which
 *    swallows the death message 2 (sub_0015DB20): the boss never died. For
 *    that boss in scene 100: the mansion handler (death state 6, anim 0x1A6).
 *  - risen (sub_002875F0(entity), state 1 of the fight: "has the entrance
 *    animation finished"): stock answers sub_00059DB0(entity, 0x19C) in
 *    scene 1, 0x23B in 17 and 0 elsewhere (the fight would stay in state 1);
 *    for the boss in scene 100: the scene 1 answer.
 *  - throw (sub_00287E20(entity, out)): the feeble-throw ranged attack
 *    (action table 0x005208B8[a+0x9DC] == 6, state +0x4160 aim 0 -> fire 1 ->
 *    done 2) runs only in scene 1; elsewhere every frame resets it to done.
 *    For the boss in scene 100 with action 6: aim = the original with the
 *    scene word reading 1 (its only scene read is its first instruction; its
 *    callees sub_00059E70, sub_0010E730 and sub_0010EB90 read no scene, test);
 *    fire = the stock projectile call sub_000EA9C0(0x34, s, s+0xC, 0.3,
 *    entity, a, 2, 1, entity+0x34, 0xE, 0xC, 0.1) with s = state + 0x413C,
 *    then sub_0010EB90([s+0x18], 0) and +0x4160 = 2 (0x00287FDD..0x00288016),
 *    so the projectile is made in the real scene; done = return (scene 1
 *    does nothing, 0x00287FD7). Other actions: the original (identical).
 *  - event (sub_002274C0): its death fires level event 3 (call 0x0029E063,
 *    return 0x0029E068), in MapMaker the map's own logic marker 3: dropped
 *    and logged in scene 100. No other level event is reachable (test).
 *  - Bar: the HUD case for model 0x5E9 draws the bar only in scene 1
 *    (0x0014CF05); elsewhere it shows a catacombs meter when entity +0x1A0
 *    == 1, else nothing. hud clears the focus for the stock call as for the
 *    deerbeast: Boss Off = no health shown; Boss bar = the bosses bar; Sub-
 *    boss = the meter (record made at 0x0029D101). meter treats it like the
 *    deerbeast.
 *  - Not drawn in scene 100: the boss's dust particles (sub_0029CE00, called
 *    by the frame sub_001CEBC0 only in scenes 1 and 17, 0x001CEE31/42).
 *
 * The Crow walker of story mission 13 (scene 81, nemoend; CONFIRMED, US XBE
 * data and tests): character id 0x64 (character table: voice set 0x00539948,
 * model 0x637 chrs/walkercrow, no LOD; not story-only flagged), no Arcade row;
 * picker row RBS_WALK_ROW (152) "Crow Walker", preview stand-in the Arcade
 * giant robot (id 0xA8). Behaviour: class 0x100000 (sub_0027D590 0x0027DD2A ->
 * call sub_002998E0(entity) at 0x0027DE5A, no scene test); its message handler
 * sub_0029CBF0 (0x0027D426) plays the death animation 0x6F9 in every scene, so
 * no message hook is needed. Anims: pak/stream/lv81/anim/lv81.pak (string
 * 0x0049D9A8, 94 KB, registered at 0x001CA92C). Per-level state: sub_002976F0
 * (stock from sub_0025F720 only in scene 81, 0x0025F79A): sub_002A8300 (an
 * effect block, [0x0078B090]/[0x0078B098]), sub_002A62C0 (the particle block
 * [0x0078B0D0] the skeleton also uses), [0x0078B1E8] = sub_0003DFA0(0x64DC, 1),
 * then sub_00297690 sets every field. The fight works from the entity's own
 * position and the players ([0x007B0C48]); it reads no arena coordinate.
 *  - level: as the skeleton's, with sub_002976F0; ai_start gives one
 *    spawn record per level class 0/0x100000.
 *  - tag (sub_0023FF20(tag) -> object or 0): its first frame looks up the
 *    level object tagged 'WELL' (0x57454C4C, call 0x0029B32C, return
 *    0x0029B331) and writes its +0x28 without a null test (0x0029B343), as does
 *    the end of its death (0x0029CB34); a MapMaker map has no such object. For
 *    that call in scene 100, a zeroed stand-in object (0x2E8 bytes, heap) is
 *    returned instead of 0; only the walker reads it (+0x28 flag 0x40, +0x68).
 *  - event: level event 1 at the end of its intro (returns 0x0029AEBE and
 *    0x0029B120) and event 2 when its death animation has ended (0x0029CB80)
 *    are the map's own logic markers in MapMaker: dropped. At the second, the
 *    walker is removed with sub_0015DDA0 (as the skeleton's stock death does,
 *    0x0029E069; mission 13 plays its ending instead) and the story focus is
 *    cleared if it is the walker.
 *  - fx (sub_00126560, called by the frame sub_001CEBC0 at 0x001CEE87, its
 *    only caller): the frame draws the walker's beams (sub_0029CCB0) only in
 *    scene 81 (0x001CEE8C); after the original, in scene 100 with this
 *    level's walker state, it is called too. Before that call, the wake-up:
 *    the fight's intro state 1 (AI +0x20) waits for the mission's level
 *    trigger 'SPC2'/'SPC1' (sub_00234190/sub_00234270, 0x0029B5BA..0x0029B609)
 *    and a MapMaker map has none, so once its first frame is done (state
 *    +0x98 == 1) the walker's AI gets the request AI +0x9D0 = 2 (applied by
 *    the fight at 0x002998FA): its roar 0x6F7, then the fight (state 4).
 *    State 2 plays the roar only after the current animation has finished
 *    (sub_00059DD0), and a walker that was never animated never finishes,
 *    so while its controller has no animation the wake-up starts the roar
 *    with state 2's own call sub_0005F460(entity, 0x6F7, 0, 1.0), once.
 *  - hud, meter and the bigdraw area treat model 0x637 like the others. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define RBS_SCENE 0x0079094Cu
#define RBS_MODE 0x007DE455u            /* byte: 10 = story */
#define RBS_ID 0x15u                    /* character id of the deerbeast */
#define RBS_MODEL 0x5BEu                /* CHR_MANSION_DEERBEAST */
#define RBS_LOD 0x5BFu
#define RBS_ROW 150u                    /* synthetic picker row: one past the Arcade table */
#define RBS_TEXT 0x52420015u            /* synthetic text id, only from row_text */
#define RBS_LABEL "Giant Trophy Monster"
#define RBS_TABLE 0x00521A18u           /* Arcade character table, 150 x 0x30, +4 id */
#define RBS_ROWS 150u
#define RBS_TROPHY 0xA7u                /* Arcade trophy monster: preview stand-in */
#define RBS_PICK_FLAGS 0x20900u         /* sub_002C1EA0 item flags in scene 0x66 (0x002C1EC6) */
#define RBS_PICK_CB 0x002C1C90u
#define RBS_PICKER_END 0x002C1FA5u      /* sub_002C1EA0: call sub_000781A0 at 0x002C1FA0 */
#define RBS_SLOTS 0x0075FD90u           /* work copy: 10 x 0x1C, +0 id, +0x14 row, +0x18 variant */
#define RBS_SLOT_STRIDE 0x1Cu
#define RBS_SLOT_COUNT 10u
#define RBS_PREVIEW 0x0078AEA8u         /* Arcade row shown by the preview panel */
#define RBS_RET_RELOAD 0x002C054Cu      /* sub_002C0490: slot reload */
#define RBS_RET_NAME 0x002BE65Fu        /* sub_002BE650 */
#define RBS_RET_NAME_OUTER 0x003033C0u  /* sub_00303350 case 3 (Story AI edit screen) */
#define RBS_RET_PREVIEW1 0x002C1805u    /* sub_002C1570 */
#define RBS_RET_PREVIEW2 0x002C1814u
#define RBS_BAR_TEXT 0xDCCu             /* sub_0014CC80 model 0x5BE label */
#define RBS_RET_BAR_TEXT 0x0014CED5u
#define RBS_EMPTY_TEXT 0x004766D8u      /* sub_0003D7B0(0) */
#define RBS_PAK 0x0049D77Cu             /* "pak/stream/lv1/anim/lv1_3.pak" */
#define RBS_MOUNTS 0x006D7880u          /* mounted paks, table of 30 (0x000620F9) */
#define RBS_MOUNT_MAX 64u               /* paktable.json: 64 slots at 0x00907000 */
#define RBS_MOUNT_KEEP 8u               /* leave this many slots for stock streaming */
#define RBS_CONTAINER 0x00784014u
#define RBS_MAGIC 0x00C77667u
#define RBS_TAG_INFO 0x00C77668u
#define RBS_TAG_SAI 0x00C7766Cu
#define RBS_TAG_END 0x00C7766Au
#define RBS_SCAN 0x00080000u            /* container: 512 KB */
#define RBS_INFO_SET 0x14u              /* word: set index, >= 0xC custom */
#define RBS_INFO_COUNT 0x16u            /* byte: members */
#define RBS_INFO_MEMBERS 0x18u          /* 4 bytes each, word id */
#define RBS_SET_MAX 20u                 /* slots 1-10 here, 11-20 in RBS_TAG_SETS */
#define RBS_INFO_SLOTS 10u              /* stock: count byte 10 (0x00300884), 10 members */
#define RBS_SET_MAP 0x11u               /* the map's own set; others are presets/profile sets */
#define RBS_TAG_SETS 0x00C776F5u        /* pc_mapmaker_sets.inc: +0 word 1, +2 10, +3 count, +4 members */
#define RBS_SETS_SIZE 0x30u
#define RBS_SAI_INDEX 0x22u
#define RBS_SAI_BOSS 0x29u              /* bosses area: 0xB0 | sub-boss 4 | (level - 1) */
#define RBS_BOSS_BAR 1u                 /* real[].boss: Boss-flagged, the bottom Boss bar */
#define RBS_BOSS_SUB 2u                 /* Boss-flagged with the Sub-boss bar: overhead meter */
#define RBS_SAI_MIN 0x44u
#define RBS_SAI_BASE 0x00783A50u        /* R: Story AI ids are R + index + 1 */
#define RBS_ACTOR 0xEA0u
#define RBS_ACTOR_CHAR 0x48u
#define RBS_ACTOR_RECORD 0xBF0u
#define RBS_ENT_AI 0x7Cu
#define RBS_ENT_MODEL 0x94u
#define RBS_CLASS_LO 0u                 /* record +0 -> AI +0x10 */
#define RBS_CLASS_HI 0x80u              /* record +4 -> AI +0x14: sub_00280FA0 */
#define RBS_PLAYER 0x007B0C7Cu
#define RBS_FOCUS 0xFF4u
#define RBS_REAL_MAX 16u
#define RBS_RET_METER 0x0014D264u       /* sub_0014D1F0: call sub_0013A400 at 0x0014D25F */
/* the giant skeleton (mansion boss) */
#define RBS_SKEL_ID 0x28u
#define RBS_SKEL_MODEL 0x5E9u           /* CHR_MANSION_MANSIONBOSS, no LOD */
#define RBS_SKEL_ROW 151u
#define RBS_SKEL_TEXT 0x52420028u
#define RBS_SKEL_LABEL "Giant Skeleton"
#define RBS_SKEL_STANDIN 0xA6u          /* Arcade skeleton cowboy: preview stand-in */
#define RBS_SKEL_PAK 0x0049D6FCu        /* "pak/stream/lv1/anim/lv1_6.pak" */
#define RBS_SKEL_CLASS 0x20000u         /* record +4 -> AI +0x14: sub_0029FE00 */
#define RBS_SKEL_STATE 0x0078B1ECu      /* 0x4C7C bytes, sub_00287E00 */
#define RBS_SKEL_STATE_SIZE 0x4C7Cu
#define RBS_SKEL_PARTS 0x0078B0D0u      /* 0x7D00 bytes, sub_002A62C0 */
#define RBS_SKEL_PARTS_SIZE 0x7D00u
#define RBS_SKEL_START 0x19Cu           /* princess_start: the entrance */
#define RBS_SKEL_ACTION 0x9DCu          /* a+0x9DC: index into 0x005208B8 (8 entries) */
#define RBS_SKEL_ACTIONS 0x005208B8u
#define RBS_SKEL_THROW 6u               /* action type: feeble throw */
#define RBS_SKEL_PHASE 0x4160u          /* state: 0 aim, 1 fire, 2 done */
#define RBS_SKEL_SHOT 0x413Cu           /* state: origin (+0), direction (+0xC), effect (+0x18) */
#define RBS_ENT_Y 0x68u                /* entity position +0x64 x, +0x68 y (up), +0x6C z */
#define RBS_SKEL_FIRE 0x3E88u           /* state: 1 = the mansion fight's particles are drawn */
#define RBS_VIEW 0x007B0CC0u            /* +0x208: the camera the frame draws particles for */
#define RBS_RET_FACE 0x0029D075u       /* sub_0029CEF0: height = 0 at 0x0029D06B, call sub_002895A0 at 0x0029D070 */
#define RBS_MSG_DEATH 2u                /* sub_0015DB20: message 2 = killed */
#define RBS_RET_EVENT 0x0029E068u      /* sub_0029CEF0: call sub_002274C0(3, 0) at 0x0029E063 */
/* the Crow walker */
#define RBS_WALK_ID 0x64u
#define RBS_WALK_MODEL 0x637u           /* chrs/walkercrow, no LOD */
#define RBS_WALK_ROW 152u
#define RBS_WALK_TEXT 0x52420064u
#define RBS_WALK_LABEL "Crow Walker"
#define RBS_WALK_STANDIN 0xA8u          /* Arcade giant robot: preview stand-in */
#define RBS_WALK_PAK 0x0049D9A8u        /* "pak/stream/lv81/anim/lv81.pak" */
#define RBS_WALK_CLASS 0x100000u        /* record +4 -> AI +0x14: sub_002998E0 */
#define RBS_WALK_STATE 0x0078B1E8u      /* 0x64DC bytes, sub_002976F0 */
#define RBS_WALK_STATE_SIZE 0x64DCu
#define RBS_WALK_TAG 0x57454C4Cu        /* 'WELL' */
#define RBS_RET_WELL 0x0029B331u        /* sub_002998E0: call sub_0023FF20 at 0x0029B32C */
#define RBS_OBJ_SIZE 0x2E8u             /* level object (table [0x0072F5E8], stride 0x2E8) */
#define RBS_RET_WALK_INTRO 0x0029AEBEu  /* event 1 (call 0x0029AEB9) */
#define RBS_RET_WALK_INTRO2 0x0029B120u /* event 1 (call 0x0029B11B) */
#define RBS_RET_WALK_DONE 0x0029CB80u   /* event 2 (call 0x0029CB7B): death animation ended */
#define RBS_RET_FX 0x001CEE8Cu          /* sub_001CEBC0: call sub_00126560 at 0x001CEE87 */
#define RBS_WALK_STAGE 0x98u            /* state byte: 0 before its first frame, 1 after (0x0029B597) */
#define RBS_WALK_AI_STATE 0x20u         /* AI: the fight's state (switch 0x0029B24F, table 0x0029CB90) */
#define RBS_WALK_AI_NEXT 0x9D0u         /* AI: requested state, applied first thing by the fight (0x002998FA) */
#define RBS_WALK_WAIT 1u                /* intro: waits for level trigger 'SPC2'/'SPC1' (0x0029B5A6) */
#define RBS_WALK_ROAR 2u                /* stunscript 0x6F7, then the fight, state 4 (0x0029B62E) */
#define RBS_WALK_STUN 0x6F7u            /* crowewalkerstunscript: state 2 plays it (0x0029B65E) */
#define RBS_ANIM_CUR 0xD4u              /* animation controller (entity +4): current animation, 0 = none */
#define RBS_ANIM_DONE 0x124u            /* its finished flag (sub_00059DD0 when +0x12C is 0) */
#define RBS_ANIM_ALT 0x12Cu             /* blend target animation, 0 = none */
#define RBS_ANIM_ALT_DONE 0x17Cu        /* its finished flag (sub_00059DD0 when +0x12C is set) */
#define RBS_ANIM_SIZE 0x184u
#define RBS_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define RBS_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00079D60(void),sub_00071630(void),sub_00071C70(void),sub_0005A130(void),sub_00062260(void);
extern void sub_00287E00(void),sub_0029CEF0(void),sub_00286D30(void),sub_00059DB0(void),sub_000EA9C0(void),sub_0010EB90(void);
extern void sub_002976F0(void),sub_0015DDA0(void),sub_0029CCB0(void),sub_00019D70(void),sub_002A7F30(void),sub_0005F460(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);
int pc_mapmaker_area_enabled(const char *area,const char *hook);

static struct {
    int read, optout;
    uint32_t label, skel_label, walk_label, well, extra_label[6];
    unsigned logged, meters;
    struct { uint32_t record; uint8_t boss; } real[RBS_REAL_MAX];
} rbs;
/* The giant skeleton of this level: its state blocks (0 = none this level)
 * and the one spawn record that holds the boss class. */
static struct { uint32_t state, parts, record; unsigned extras, events, throws, deaths, skipped, fire; float floor; int floor_set; } rbs_skel;
/* The Crow walker of this level: its state block, spawn record and pool entry. */
static struct { uint32_t state, record, actor; unsigned extras, events, wells, fx, seen, ticks, beats, woken, roars; } rbs_walk;

static void rbs_log(const char *what,uint32_t a,uint32_t b)
{
    if(rbs.logged>=24u) return;
    rbs.logged++;
    fprintf(stderr,"[mapmaker] realbosses: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int rbs_on(void)
{
    mapmaker_limits_read();
    if(!rbs.read) {
        const char *v=getenv("TSFP_MAPMAKER_REAL_BOSS");
        rbs.read=1;
        rbs.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int rbs_play(void)
{
    return rbs_on() && WORD_AT(RBS_SCENE)==100u && RBS_B(RBS_MODE)==10u;
}
/* The label string (Latin-1, like the game's text); allocated off the hot path. */
static uint32_t rbs_label(void)
{
    if(!rbs.label) {
        uint32_t p=xbox_HeapAlloc(sizeof RBS_LABEL,4u);
        if(p && MAPMAKER_PTR_OK(p,sizeof RBS_LABEL)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),RBS_LABEL,sizeof RBS_LABEL);
            rbs.label=p;
        } else rbs_log("no memory for the label",(uint32_t)sizeof RBS_LABEL,0);
    }
    return rbs.label;
}
static uint32_t rbs_skel_label(void)
{
    if(!rbs.skel_label) {
        uint32_t p=xbox_HeapAlloc(sizeof RBS_SKEL_LABEL,4u);
        if(p && MAPMAKER_PTR_OK(p,sizeof RBS_SKEL_LABEL)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),RBS_SKEL_LABEL,sizeof RBS_SKEL_LABEL);
            rbs.skel_label=p;
        } else rbs_log("no memory for the skeleton label",(uint32_t)sizeof RBS_SKEL_LABEL,0);
    }
    return rbs.skel_label;
}
static uint32_t rbs_walk_label(void)
{
    if(!rbs.walk_label) {
        uint32_t p=xbox_HeapAlloc(sizeof RBS_WALK_LABEL,4u);
        if(p && MAPMAKER_PTR_OK(p,sizeof RBS_WALK_LABEL)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),RBS_WALK_LABEL,sizeof RBS_WALK_LABEL);
            rbs.walk_label=p;
        } else rbs_log("no memory for the Crow walker label",(uint32_t)sizeof RBS_WALK_LABEL,0);
    }
    return rbs.walk_label;
}
/* Picker rows of the bosses ported in their own areas (pc_mapmaker_robot.inc,
 * pc_mapmaker_crowe.inc, pc_mapmaker_tank.inc), which own the play side; only
 * the editor rows live here, built like the rows above. Ids: 0x42 giant robot
 * (model 0x5F2), 0x46 split Crowe (0x5E8), 0x12 the guard robot that stands in
 * for the castle tank prop (pc_mapmaker_tank.inc). sub_002BE270 is 0 for all
 * three (byte table 0x002BE29C entry 1), so the variant is -1 as for 0x15. */
#define RBS_EXTRA_N 6u
static const struct { uint32_t row, id, text, standin; char label[16]; } rbs_extra[RBS_EXTRA_N]={
    {153u,0x42u,0x52420042u,0xA8u,"Giant Robot"},  /* preview: the Arcade giant robot */
    {154u,0x46u,0x52420046u,0x1Eu,"Split Crowe"},  /* preview: Arcade Old Crow */
    {155u,0x12u,0x52420012u,0xA8u,"Castle Tank"},  /* preview: the Arcade giant robot */
    {156u,0x33u,0x52420033u,0x02u,"Khallos (Chair)"},  /* pc_mapmaker_khallos.inc; preview: Arcade Khallos */
    {157u,0x2Cu,0x5242002Cu,0x6Bu,"Mansion Worm"}, /* pc_mapmaker_worm.inc; preview: Arcade mansion zombie nurse */
    {158u,0x75u,0x52420075u,0x4Eu,"Mothership"},   /* pc_mapmaker_mothership.inc; preview: Arcade Timesplitter */
};
static unsigned rbs_extra_by_row(uint32_t row)
{
    unsigned k;
    for(k=0;k<RBS_EXTRA_N && rbs_extra[k].row!=row;k++) {}
    return k;
}
static unsigned rbs_extra_by_id(uint32_t id)
{
    unsigned k;
    for(k=0;k<RBS_EXTRA_N && rbs_extra[k].id!=id;k++) {}
    return k;
}
static uint32_t rbs_extra_label(unsigned k)
{
    if(k>=RBS_EXTRA_N) return 0;
    if(!rbs.extra_label[k]) {
        uint32_t p=xbox_HeapAlloc(sizeof rbs_extra[k].label,4u);
        if(p && MAPMAKER_PTR_OK(p,sizeof rbs_extra[k].label)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),rbs_extra[k].label,sizeof rbs_extra[k].label);
            rbs.extra_label[k]=p;
        } else rbs_log("no memory for a boss label",rbs_extra[k].row,0);
    }
    return rbs.extra_label[k];
}
static uint32_t rbs_arcade_row(uint32_t id)
{
    uint32_t i;
    for(i=0;i<RBS_ROWS;i++) if(WORD_AT(RBS_TABLE+i*0x30u+4u)==id) return i;
    return 0;
}
static uint32_t rbs_trophy_row(void) { return rbs_arcade_row(RBS_TROPHY); }
/* The preview row for a synthetic row (or the row itself). */
static uint32_t rbs_preview_row(uint32_t row)
{
    if(row==RBS_ROW) return rbs_trophy_row();
    if(row==RBS_SKEL_ROW) return rbs_arcade_row(RBS_SKEL_STANDIN);
    if(row==RBS_WALK_ROW) return rbs_arcade_row(RBS_WALK_STANDIN);
    if(rbs_extra_by_row(row)<RBS_EXTRA_N) return rbs_arcade_row(rbs_extra[rbs_extra_by_row(row)].standin);
    return row;
}

/* ---- editor (scene 102) -------------------------------------------------- */
static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)==102u; }

/* WRAP sub_000781A0: the picker's list ends; add the Giant Trophy Monster row. */
void pc_mapmaker_realbosses_picker_end(void (*original)(void))
{
    uint32_t text;
    unsigned k;
    if(MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBS_PICKER_END && rbs_editor() && (text=rbs_label())!=0) {
        guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);
        guest_call(sub_00071C70,2,guest_call(sub_00071630,0),0u);   /* enabled, as the stock rows */
        if((text=rbs_skel_label())!=0) {
            guest_call(sub_00079D60,6,text,RBS_SKEL_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);
            guest_call(sub_00071C70,2,guest_call(sub_00071630,0),0u);
        }
        if((text=rbs_walk_label())!=0) {
            guest_call(sub_00079D60,6,text,RBS_WALK_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);
            guest_call(sub_00071C70,2,guest_call(sub_00071630,0),0u);
        }
        for(k=0;k<RBS_EXTRA_N;k++) if((text=rbs_extra_label(k))!=0) {
            guest_call(sub_00079D60,6,text,rbs_extra[k].row,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);
            guest_call(sub_00071C70,2,guest_call(sub_00071630,0),0u);
        }
    }
    original();
}

/* WRAP sub_002C1C90 (pick callback): row 150 stores id 0x15, row 151 id
 * 0x28, variant -1 (sub_002BE270 is 0 for both: byte table entry 1). */
void pc_mapmaker_realbosses_pick(void (*original)(void))
{
    uint32_t k,s,id;
    unsigned e;
    original();
    if(!rbs_editor()) return;
    for(k=0;k<RBS_SLOT_COUNT;k++) {
        s=RBS_SLOTS+k*RBS_SLOT_STRIDE;
        if(WORD_AT(s+0x14u)==RBS_ROW) id=RBS_ID;
        else if(WORD_AT(s+0x14u)==RBS_SKEL_ROW) id=RBS_SKEL_ID;
        else if(WORD_AT(s+0x14u)==RBS_WALK_ROW) id=RBS_WALK_ID;
        else if((e=rbs_extra_by_row(WORD_AT(s+0x14u)))<RBS_EXTRA_N) id=rbs_extra[e].id;
        else continue;
        if(WORD_AT(s)!=id) rbs_log(id==RBS_ID?"slot set to the giant trophy monster":
                                   id==RBS_SKEL_ID?"slot set to the giant skeleton":
                                   id==RBS_WALK_ID?"slot set to the Crow walker":"slot set to a ported story boss",k,id);
        WORD_AT(s)=id;
        WORD_AT(s+0x18u)=0xFFFFFFFFu;
    }
}

/* WRAP sub_002C1E10 (picker highlight): the preview never gets row 150/151. */
void pc_mapmaker_realbosses_highlight(void (*original)(void))
{
    original();
    if(rbs_editor()) WORD_AT(RBS_PREVIEW)=rbs_preview_row(WORD_AT(RBS_PREVIEW));
}

/* WRAP sub_002BE600 (cdecl id, variant -> Arcade row or -1). */
void pc_mapmaker_realbosses_row_of(void (*original)(void))
{
    uint32_t ret,row,id;
    if(!MAPMAKER_STACK_OK(16) || ((id=WORD_AT(g_esp+4u))!=RBS_ID && id!=RBS_SKEL_ID && id!=RBS_WALK_ID &&
       rbs_extra_by_id(id)>=RBS_EXTRA_N) || !rbs_editor()) { original();return; }
    ret=WORD_AT(g_esp);
    row=id==RBS_ID?RBS_ROW:id==RBS_SKEL_ID?RBS_SKEL_ROW:id==RBS_WALK_ID?RBS_WALK_ROW:rbs_extra[rbs_extra_by_id(id)].row;
    if(ret==RBS_RET_RELOAD || (ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)) {}
    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=rbs_preview_row(row);
    else { original();return; }
    g_eax=row;
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_002BE300 (cdecl row -> text id; 0 for rows >= 150). */
void pc_mapmaker_realbosses_row_text(void (*original)(void))
{
    uint32_t row;
    unsigned e;
    if(MAPMAKER_STACK_OK(8) && (e=rbs_extra_by_row(WORD_AT(g_esp+4u)))<RBS_EXTRA_N && rbs_editor() && rbs_extra_label(e)) {
        g_eax=rbs_extra[e].text;
        MAPMAKER_GUEST_RET();
        return;
    }
    if(!MAPMAKER_STACK_OK(8) || ((row=WORD_AT(g_esp+4u))!=RBS_ROW && row!=RBS_SKEL_ROW && row!=RBS_WALK_ROW) || !rbs_editor() ||
       !(row==RBS_ROW?rbs_label():row==RBS_SKEL_ROW?rbs_skel_label():rbs_walk_label())) { original();return; }
    g_eax=row==RBS_ROW?RBS_TEXT:row==RBS_SKEL_ROW?RBS_SKEL_TEXT:RBS_WALK_TEXT;
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0003D7B0 (cdecl text id -> string). Runs for every text lookup:
 * fixed compares only, the original first thing for every other id. */
void pc_mapmaker_realbosses_text(void (*original)(void))
{
    uint32_t id;
    if(!MAPMAKER_STACK_OK(8)) { original();return; }
    id=WORD_AT(g_esp+4u);
    if(id==RBS_TEXT) {
        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;
        MAPMAKER_GUEST_RET();
        return;
    }
    if(id==RBS_SKEL_TEXT) {
        g_eax=rbs.skel_label?rbs.skel_label:RBS_EMPTY_TEXT;
        MAPMAKER_GUEST_RET();
        return;
    }
    if(id==RBS_WALK_TEXT) {
        g_eax=rbs.walk_label?rbs.walk_label:RBS_EMPTY_TEXT;
        MAPMAKER_GUEST_RET();
        return;
    }
    if((id&0xFFFFFF00u)==0x52420000u) {               /* the ported bosses' rows */
        unsigned k;
        for(k=0;k<RBS_EXTRA_N;k++) if(rbs_extra[k].text==id) {
            g_eax=rbs.extra_label[k]?rbs.extra_label[k]:RBS_EMPTY_TEXT;
            MAPMAKER_GUEST_RET();
            return;
        }
    }
    if(id==RBS_BAR_TEXT && WORD_AT(g_esp)==RBS_RET_BAR_TEXT && rbs.label && rbs_play()) {
        g_eax=rbs.label;
        MAPMAKER_GUEST_RET();
        return;
    }
    original();
}

/* WRAP sub_000A5E40 (model request): log a deerbeast model request in the editor. */
void pc_mapmaker_realbosses_model_request(void (*original)(void))
{
    uint32_t model;
    if(MAPMAKER_STACK_OK(8) && ((model=WORD_AT(g_esp+4u))==RBS_MODEL || model==RBS_LOD || model==RBS_SKEL_MODEL) && rbs_editor())
        rbs_log("editor requested a story boss model; the stock call skips flagged models here",model,WORD_AT(g_esp));
    original();
}

/* ---- play (scene 100) ---------------------------------------------------- */
/* The map's custom set, slots 1-20: the map-info chunk holds slots 1-10, the
 * sets area's chunk RBS_TAG_SETS slots 11-20 (pc_mapmaker_sets.inc; play
 * copies them into the play set, sub_0033EBF0 hook). Stock play uses the
 * map's members only for set index 0x11 (0x00240DB4..0x00240DBE,
 * 0x0023F462..0x0023F46D); any other index plays a preset/profile set.
 * 1: custom set read, *t / *s = first slot (1-based) of the trophy monster /
 * the skeleton or 0; 0: not the map's set; -1: unreadable. */
static int rbs_set_scan(uint32_t *t,uint32_t *s,uint32_t *w)
{
    uint32_t container=WORD_AT(RBS_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k,id;
    *t=*s=*w=0;
    if(!container || !MAPMAKER_PTR_OK(container,RBS_SCAN) || WORD_AT(container)!=RBS_MAGIC) return -1;
    c=container+8u;end=container+RBS_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==RBS_TAG_END || sz>end-c-8u) break;
        if(tag==RBS_TAG_INFO && sz>=RBS_INFO_MEMBERS && !info) { info=d;isz=sz; }
        else if(tag==RBS_TAG_SETS && sz>=RBS_SETS_SIZE && !extra && RBS_H(d)==1u && RBS_B(d+2u)==RBS_INFO_SLOTS &&
                RBS_B(d+3u)>=1u && RBS_B(d+3u)<=RBS_SET_MAX-RBS_INFO_SLOTS) extra=d;   /* as sets_take */
        c+=8u+sz;
    }
    if(!info) return -1;
    if(RBS_H(info+RBS_INFO_SET)!=RBS_SET_MAP) return 0;   /* a preset or profile set */
    n=RBS_B(info+RBS_INFO_COUNT);
    if(n>RBS_INFO_SLOTS) n=RBS_INFO_SLOTS;
    if(RBS_INFO_MEMBERS+n*4u>isz) n=(isz-RBS_INFO_MEMBERS)/4u;
    for(k=0;k<n;k++) {
        id=RBS_H(info+RBS_INFO_MEMBERS+k*4u);
        if(id==RBS_WALK_ID && !*w) *w=k+1u;
        if(id==RBS_ID && !*t) *t=k+1u;
        if(id==RBS_SKEL_ID && !*s) *s=k+1u;
    }
    for(k=0;extra && k<RBS_B(extra+3u);k++) {          /* slots 11-20 */
        id=RBS_H(extra+4u+k*4u);
        if(id==RBS_WALK_ID && !*w) *w=RBS_INFO_SLOTS+k+1u;
        if(id==RBS_ID && !*t) *t=RBS_INFO_SLOTS+k+1u;
        if(id==RBS_SKEL_ID && !*s) *s=RBS_INFO_SLOTS+k+1u;
    }
    return 1;
}
/* 1: the map's custom set has the id; 0: it has not; -1: unreadable. */
static int rbs_map_has(uint32_t id)
{
    uint32_t t,s,w;
    int r=rbs_set_scan(&t,&s,&w);
    if(r<=0) return r;
    return (id==RBS_ID?t:id==RBS_SKEL_ID?s:w)!=0;
}
/* What the set check found, for the log (own small budget, once per level hook). */
static void rbs_set_report(const char *where)
{
    static unsigned n;
    uint32_t t,s,w;
    int r=rbs_set_scan(&t,&s,&w);
    if(n>=12u) return;
    n++;
    if(r<0) fprintf(stderr,"[mapmaker] realbosses: %s set check: map container unreadable; every real boss assumed\n",where);
    else if(!r) fprintf(stderr,"[mapmaker] realbosses: %s set check: not the map's own set; no real boss\n",where);
    else fprintf(stderr,"[mapmaker] realbosses: %s set check found: giant trophy monster %s%u, giant skeleton %s%u, Crow walker %s%u\n",where,
                 t?"slot ":"none ",(unsigned)t,s?"slot ":"none ",(unsigned)s,w?"slot ":"none ",(unsigned)w);
    fflush(stderr);
}
static int rbs_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,RBS_PAK)!=-1; }
static int rbs_skel_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,RBS_SKEL_PAK)!=-1; }
static int rbs_walk_pak_mounted(void) { return (int32_t)guest_call(sub_00062260,1,RBS_WALK_PAK)!=-1; }

/* WRAP sub_001C8540 (mount the level's anim paks; names registered next). */
void pc_mapmaker_realbosses_anims(void (*original)(void))
{
    int has;
    uint32_t mounted;
    original();
    if(!rbs_play() || rbs.optout) return;
    rbs_set_report("anim pak");
    if((has=rbs_map_has(RBS_SKEL_ID))!=0) {
        rbs_skel_label();                              /* the bar label, off the HUD path */
        if(rbs_skel_pak_mounted()) {}
        else if((mounted=WORD_AT(RBS_MOUNTS))+RBS_MOUNT_KEEP>RBS_MOUNT_MAX)
            rbs_log("pak mount table full; the giant skeleton keeps the bot behaviour",mounted,RBS_MOUNT_MAX);
        else if(!guest_call(sub_0005A130,1,RBS_SKEL_PAK)) rbs_log("lv1_6.pak did not mount; bot behaviour",mounted,(uint32_t)has);
        else rbs_log(has<0?"mounted lv1_6.pak (map container unreadable)":"mounted lv1_6.pak for the giant skeleton",mounted+1u,0);
    }
    if((has=rbs_map_has(RBS_WALK_ID))!=0) {
        rbs_walk_label();
        if(rbs_walk_pak_mounted()) {}
        else if((mounted=WORD_AT(RBS_MOUNTS))+RBS_MOUNT_KEEP>RBS_MOUNT_MAX)
            rbs_log("pak mount table full; the Crow walker keeps the bot behaviour",mounted,RBS_MOUNT_MAX);
        else if(!guest_call(sub_0005A130,1,RBS_WALK_PAK)) rbs_log("lv81.pak did not mount; bot behaviour",mounted,(uint32_t)has);
        else rbs_log(has<0?"mounted lv81.pak (map container unreadable)":"mounted lv81.pak for the Crow walker",mounted+1u,0);
    }
    if((has=rbs_map_has(RBS_ID))==0) return;
    rbs_label();                                       /* the bar label, off the HUD path */
    if(rbs_pak_mounted()) return;
    if((mounted=WORD_AT(RBS_MOUNTS))+RBS_MOUNT_KEEP>RBS_MOUNT_MAX) {
        rbs_log("pak mount table full; the giant trophy monster keeps the bot behaviour",mounted,RBS_MOUNT_MAX);
        return;
    }
    if(!guest_call(sub_0005A130,1,RBS_PAK)) rbs_log("lv1_3.pak did not mount; bot behaviour",mounted,(uint32_t)has);
    else rbs_log(has<0?"mounted lv1_3.pak (map container unreadable)":"mounted lv1_3.pak for the giant trophy monster",mounted+1u,0);
}

/* WRAP sub_0025F720 (story level init, no args): after the original, the
 * giant skeleton's per-level state, allocated and initialised by the stock
 * sub_00287E00 as for scenes 1 and 17. */
void pc_mapmaker_realbosses_level(void (*original)(void))
{
    int has;
    uint32_t st,pt;
    original();
    memset(&rbs_skel,0,sizeof rbs_skel);              /* a new level: nothing of the last one */
    memset(&rbs_walk,0,sizeof rbs_walk);
    rbs.logged=0;                                      /* a log budget per level */
    if(!rbs_play() || rbs.optout) return;
    rbs_set_report("level init");
    if((has=rbs_map_has(RBS_WALK_ID))!=0) {           /* before the skeleton: both share [0x0078B0D0] */
        WORD_AT(RBS_WALK_STATE)=0;
        guest_call(sub_002976F0,0);
        st=WORD_AT(RBS_WALK_STATE);
        if(!st || !MAPMAKER_PTR_OK(st,RBS_WALK_STATE_SIZE)) rbs_log("no level memory for the Crow walker; bot behaviour",st,0);
        else {
            rbs_walk.state=st;
            rbs_log(has<0?"Crow walker state allocated (map container unreadable)":"Crow walker state allocated",st,0);
        }
    }
    if((has=rbs_map_has(RBS_SKEL_ID))==0) return;
    WORD_AT(RBS_SKEL_STATE)=0;WORD_AT(RBS_SKEL_PARTS)=0;   /* stale pointers of an earlier level */
    guest_call(sub_00287E00,0);
    st=WORD_AT(RBS_SKEL_STATE);pt=WORD_AT(RBS_SKEL_PARTS);
    if(!st || !pt || !MAPMAKER_PTR_OK(st,RBS_SKEL_STATE_SIZE) || !MAPMAKER_PTR_OK(pt,RBS_SKEL_PARTS_SIZE)) {
        rbs_log("no level memory for the giant skeleton; bot behaviour",st,pt);
        return;
    }
    rbs_skel.state=st;rbs_skel.parts=pt;
    rbs_log(has<0?"giant skeleton state allocated (map container unreadable)":"giant skeleton state allocated",st,pt);
}
/* This level's skeleton state is in place and this entity is the boss. */
static int rbs_skel_boss(uint32_t ent)
{
    uint32_t a;
    if(!rbs_skel.record || !rbs_skel.state || WORD_AT(RBS_SKEL_STATE)!=rbs_skel.state) return 0;
    if(!MAPMAKER_PTR_OK(ent,0x100u) || !MAPMAKER_PTR_OK(a=WORD_AT(ent+RBS_ENT_AI),RBS_ACTOR)) return 0;
    return WORD_AT(a+RBS_ACTOR_RECORD)==rbs_skel.record && WORD_AT(RBS_SCENE)==100u && rbs_on();
}

static float rbs_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void rbs_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int rbs_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }
/* The skeleton's floor: its spawn height, read on the fight's first frame
 * (user report, v0.4.0-mapmaker.16: it hovered one floor up). */
static void rbs_skel_floor(uint32_t ent)
{
    float y;
    if(rbs_skel.floor_set || !rbs_sane(y=rbs_getf(ent+RBS_ENT_Y))) return;
    rbs_skel.floor=y;rbs_skel.floor_set=1;
    fprintf(stderr,"[mapmaker] realbosses: giant skeleton floor height %.2f (mission 5 forces 0)\n",(double)y);
    fflush(stderr);
}

/* WRAP sub_0029FE00 (cdecl entity; class 0x20000): the mansion variant for
 * the boss in scene 100. sub_0029CEF0 pops the return address at [g_esp]
 * (the lifted tail jump does the same call). */
void pc_mapmaker_realbosses_princess(void (*original)(void))
{
    if(!rbs_skel.record || !MAPMAKER_STACK_OK(8) || !rbs_skel_boss(WORD_AT(g_esp+4u))) { original();return; }
    rbs_skel_floor(WORD_AT(g_esp+4u));
    sub_0029CEF0();
}

/* WRAP sub_002895A0 (cdecl entity; the fight's facing step). Every frame the
 * mansion fight sets the entity's height to 0, the courtyard floor
 * (0x0029D06B), right before this call (0x0029D070, return 0x0029D075); a
 * MapMaker floor is elsewhere. For the boss in scene 100: its spawn height. */
void pc_mapmaker_realbosses_face(void (*original)(void))
{
    uint32_t ent;
    if(!rbs_skel.floor_set || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=RBS_RET_FACE || !rbs_skel_boss(ent=WORD_AT(g_esp+4u))) { original();return; }
    rbs_setf(ent+RBS_ENT_Y,rbs_skel.floor);
    original();
}

/* WRAP sub_0028D0A0 (cdecl entity, message, a, b; the class 0x20000 message
 * handler, called by sub_0027CEF0 at 0x0027D3A7 for sub_0015C310): stock
 * tail-jumps to the mansion handler sub_00286D30 in scene 1 and to the
 * catacombs one sub_0028C560 elsewhere, which answers -1 to messages 1 and 2
 * and does nothing. Message 2 is the death (sub_0015DB20 at 0x0015DB34, from
 * the damage code 0x000BB059 when health runs out): -1 skips the generic
 * death, so in MapMaker the skeleton never died. The mansion handler sets
 * state 6 with the death animation 0x1A6 (0x00286D7A); state 6 then drops
 * the focus, fires level event 3 (dropped by _event) and removes the AI with
 * sub_0015DDA0 (0x0029E069), as in mission 5. For the boss in scene 100: the
 * mansion handler (a direct call: the callee pops the caller's return
 * address, exactly the lifted tail jump). */
void pc_mapmaker_realbosses_message(void (*original)(void))
{
    uint32_t ent;
    if(!rbs_skel.record || !MAPMAKER_STACK_OK(20) || !rbs_skel_boss(ent=WORD_AT(g_esp+4u))) { original();return; }
    if(WORD_AT(g_esp+8u)==RBS_MSG_DEATH && rbs_skel.deaths++<2u)
        rbs_log("giant skeleton killed: the mansion death (state 6, animation 0x1A6)",ent,RBS_MSG_DEATH);
    sub_00286D30();
}

/* WRAP sub_002875F0 (cdecl entity -> entrance finished): scene 1's answer. */
void pc_mapmaker_realbosses_risen(void (*original)(void))
{
    uint32_t ent;
    if(!rbs_skel.record || !MAPMAKER_STACK_OK(8) || !rbs_skel_boss(ent=WORD_AT(g_esp+4u))) { original();return; }
    g_eax=guest_call(sub_00059DB0,2,ent,RBS_SKEL_START);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_00287E20 (cdecl entity, out): the feeble throw as in scene 1. */
void pc_mapmaker_realbosses_throw(void (*original)(void))
{
    uint32_t ent,a,act,st,phase,shot,proj;
    if(!rbs_skel.record || !MAPMAKER_STACK_OK(12) || !rbs_skel_boss(ent=WORD_AT(g_esp+4u))) { original();return; }
    a=WORD_AT(ent+RBS_ENT_AI);act=WORD_AT(a+RBS_SKEL_ACTION);
    if(act>=8u || WORD_AT(RBS_SKEL_ACTIONS+act*4u)!=RBS_SKEL_THROW) { original();return; }   /* same in both scenes */
    st=rbs_skel.state;phase=WORD_AT(st+RBS_SKEL_PHASE);
    if(phase==0) {                                     /* aim: the original reads the scene once, first */
        WORD_AT(RBS_SCENE)=1u;
        original();
        WORD_AT(RBS_SCENE)=100u;
        return;
    }
    if(phase==1) {                                     /* fire, in the real scene */
        shot=st+RBS_SKEL_SHOT;
        if(!rbs_sane(rbs_getf(shot)) || !rbs_sane(rbs_getf(shot+4u)) || !rbs_sane(rbs_getf(shot+8u)) ||
           !rbs_sane(rbs_getf(shot+0xCu)) || !rbs_sane(rbs_getf(shot+0x10u)) || !rbs_sane(rbs_getf(shot+0x14u))) {
            WORD_AT(st+RBS_SKEL_PHASE)=2u;                 /* no throw from a broken aim */
            if(rbs_skel.skipped++<4u) rbs_log("giant skeleton throw skipped: aim not a usable point",WORD_AT(shot),WORD_AT(shot+0xCu));
            MAPMAKER_GUEST_RET();
            return;
        }
        proj=guest_call(sub_000EA9C0,12,0x34u,shot,shot+0xCu,0x3E99999Au,ent,a,2u,1u,WORD_AT(ent+0x34u),0xEu,0xCu,0x3DCCCCCDu);
        guest_call(sub_0010EB90,2,WORD_AT(shot+0x18u),0u);
        WORD_AT(st+RBS_SKEL_PHASE)=2u;
        if(rbs_skel.throws++<8u) {
            fprintf(stderr,"[mapmaker] realbosses: giant skeleton feeble throw fired from (%.1f, %.1f, %.1f) dir (%.2f, %.2f, %.2f), boss at height %.1f, projectile 0x%X\n",
                    (double)rbs_getf(shot),(double)rbs_getf(shot+4u),(double)rbs_getf(shot+8u),(double)rbs_getf(shot+0xCu),
                    (double)rbs_getf(shot+0x10u),(double)rbs_getf(shot+0x14u),(double)rbs_getf(ent+RBS_ENT_Y),(unsigned)proj);
            fflush(stderr);
        }
    }
    MAPMAKER_GUEST_RET();                              /* done: scene 1 returns (0x00287FD7) */
}

/* The Crow walker's death animation has ended: remove it (sub_0015DDA0, the
 * skeleton's stock death call) and drop a story focus that is still on it. */
static void rbs_walk_remove(void)
{
    uint32_t a=rbs_walk.actor,ent,player=WORD_AT(RBS_PLAYER);
    rbs_walk.actor=0;
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || WORD_AT(a+RBS_ACTOR_RECORD)!=rbs_walk.record ||
       !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBS_ENT_AI)!=a) return;
    if(MAPMAKER_PTR_OK(player,RBS_FOCUS+4u) && WORD_AT(player+RBS_FOCUS)==ent) WORD_AT(player+RBS_FOCUS)=0;
    guest_call(sub_0015DDA0,1,ent);
    rbs_log("Crow walker defeated: removed",a,ent);
}

/* WRAP sub_0023FF20 (cdecl tag -> level object or 0): the walker's 'WELL'. */
void pc_mapmaker_realbosses_tag(void (*original)(void))
{
    uint32_t p;
    if(!rbs_walk.record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=RBS_RET_WELL || WORD_AT(g_esp+4u)!=RBS_WALK_TAG) { original();return; }
    original();
    if(g_eax || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;
    if(!rbs.well) {
        p=xbox_HeapAlloc(RBS_OBJ_SIZE,16u);
        if(p && MAPMAKER_PTR_OK(p,RBS_OBJ_SIZE)) rbs.well=p;
        else { rbs_log("no memory for the Crow walker's WELL stand-in",RBS_OBJ_SIZE,0);return; }
    }
    memset((void *)((uintptr_t)rbs.well+g_xbox_mem_offset),0,RBS_OBJ_SIZE);
    g_eax=rbs.well;
    if(!rbs_walk.wells++) rbs_log("Crow walker: no WELL object in a MapMaker map; stand-in used",rbs.well,0);
}

/* The giant skeleton's fire (user report, v0.4.0-mapmaker.17: the breath
 * played its animation but showed no fire). The frame sub_001CEBC0 draws the
 * fight's particle block [0x0078B0D0] only in scenes 1 and 17 (sub_0029CE00 at
 * 0x001CEE31/0x001CEE42); scene 1's branch: when state +0x3E88 == 1,
 * sub_00019D70(1) then sub_002A7F30([[0x007B0CC0]+0x208], 2.5). Done here
 * for this level's skeleton in scene 100, from the same frame. */
static void rbs_skel_fire(void)
{
    uint32_t st=rbs_skel.state,view=WORD_AT(RBS_VIEW);
    if(!rbs_skel.record || !st || WORD_AT(RBS_SKEL_STATE)!=st || WORD_AT(st+RBS_SKEL_FIRE)!=1u ||
       WORD_AT(RBS_SCENE)!=100u || !rbs_on() || !MAPMAKER_PTR_OK(view,0x20Cu)) return;
    guest_call(sub_00019D70,1,1u);
    guest_call(sub_002A7F30,2,WORD_AT(view+0x208u),0x40200000u);
    if(!rbs_skel.fire++) rbs_log("giant skeleton fire particles drawn",st,WORD_AT(RBS_SKEL_PARTS));
}

/* Crow walker heartbeat (user report, v0.4.0-mapmaker.21: "looks right but
 * isn't responding at all"): every 120 frame steps, 15 times per level, its
 * fight stage (state +0x98), the fight's state (AI +0x20; the entity's +0x20,
 * logged as "anim state" before mapmaker.23, is not it), its target (AI +4),
 * the aim error (state +0xE4), position, the state's health (+0x580) and the
 * AI class words (+0x10/+0x14); after mapmaker.23 also the animation
 * controller (entity +4): current animation +0xD4, blend target +0x12C and
 * the finished flag sub_00059DD0 reads (+0x17C or +0x124). Log only. */
static void rbs_walk_heartbeat(void)
{
    uint32_t a=rbs_walk.actor,ent,st=rbs_walk.state,ctl,cur=0,alt=0,done=0;
    if(!a || !st || WORD_AT(RBS_WALK_STATE)!=st || rbs_walk.beats>=15u || (rbs_walk.ticks++%120u)) return;
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || !MAPMAKER_PTR_OK(st,RBS_WALK_STATE_SIZE)) return;
    rbs_walk.beats++;
    if(MAPMAKER_PTR_OK(ctl=WORD_AT(ent+4u),RBS_ANIM_SIZE)) {
        cur=WORD_AT(ctl+RBS_ANIM_CUR);alt=WORD_AT(ctl+RBS_ANIM_ALT);
        done=WORD_AT(ctl+(alt?RBS_ANIM_ALT_DONE:RBS_ANIM_DONE));
    }
    fprintf(stderr,"[mapmaker] realbosses: Crow walker beat %u: animation 0x%X (blend 0x%X, finished %u), state request %d\n",
            rbs_walk.beats,(unsigned)cur,(unsigned)alt,(unsigned)done,(int)WORD_AT(a+RBS_WALK_AI_NEXT));
    fprintf(stderr,"[mapmaker] realbosses: Crow walker beat %u: stage %u, fight state %u, target 0x%X, aim %.0f, entity +0x20 %u, at (%.1f, %.1f, %.1f), health %.1f, class 0x%X/0x%X\n",
            rbs_walk.beats,(unsigned)RBS_B(st+RBS_WALK_STAGE),(unsigned)WORD_AT(a+RBS_WALK_AI_STATE),(unsigned)WORD_AT(a+4u),
            (double)rbs_getf(st+0xE4u),(unsigned)WORD_AT(ent+0x20u),(double)rbs_getf(ent+0x64u),(double)rbs_getf(ent+0x68u),
            (double)rbs_getf(ent+0x6Cu),(double)rbs_getf(st+0x580u),(unsigned)WORD_AT(a+0x10u),(unsigned)WORD_AT(a+0x14u));
    fflush(stderr);
}

/* The Crow walker's wake-up (user report, v0.4.0-mapmaker.22: it never moves
 * or attacks). The fight's intro state 1 (0x0029B5A6) waits for its animation
 * to end, then for the mission 13 level trigger 'SPC2' (-> 180 turn right
 * 0x6FB) or 'SPC1' (-> 180 turn left 0x6FC), looked up with sub_00234190 in
 * the level's logic table [0x0078CD90] and tested with sub_00234270; with
 * neither it returns, and the steering, walking and firing above the switch
 * run only in states above 2 (0x0029A9B2). A MapMaker map has no such
 * trigger, so the walker idled in state 1 forever. Once its first frame is
 * done (stage 1) and it waits in state 1 with no request pending, request
 * state 2 through the fight's own request word AI +0x9D0, which the fight
 * applies first thing next frame (0x002998FA..0x00299922, as the stock state
 * changes do): state 2 waits for the animation, plays crowewalkerstunscript
 * 0x6F7 and enters the fight (state 4), whose steering turns it toward the
 * player. The 180 turns are skipped: they assume the mission's arena.
 *
 * The roar (user report, v0.4.0-mapmaker.23: awake, aiming at the player,
 * but stuck in state 2 and never turning, walking or firing). State 2
 * (0x0029B62E) first waits until sub_00059DD0(entity +4) says the current
 * animation has finished, and only then plays 0x6F7 (sub_0005F460 at
 * 0x0029B664); state 4 follows once 0x6F7 is current (sub_00059D20). A new
 * animation controller has no animation and "finished" 0 (sub_00097C00:
 * +0xD4, +0x124, +0x12C = 0), and only playing one changes that
 * (sub_0005BC20). The walker gets its story class before its first frame and
 * its fight plays nothing before state 2, so in MapMaker it waited for an
 * animation that never ran (in mission 13 something else animates it
 * first). So, in state 1 or 2 after its first frame, while its controller has
 * never had an animation (+0xD4 and +0x12C both 0), the roar is started with
 * the stock call state 2 itself makes: sub_0005F460(entity, 0x6F7, 0, 1.0).
 * When it ends, state 2 sees it finished and current, and enters the fight. */
static void rbs_walk_wake(void)
{
    uint32_t a=rbs_walk.actor,ent,st=rbs_walk.state,s,ctl,r;
    if(!a || !st || WORD_AT(RBS_WALK_STATE)!=st || !MAPMAKER_PTR_OK(st,RBS_WALK_STATE_SIZE) || !MAPMAKER_PTR_OK(a,RBS_ACTOR) ||
       WORD_AT(a+RBS_ACTOR_RECORD)!=rbs_walk.record || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBS_ENT_AI)!=a) return;
    if(RBS_B(st+RBS_WALK_STAGE)!=1u) return;
    s=WORD_AT(a+RBS_WALK_AI_STATE);
    if(s==RBS_WALK_WAIT && (int32_t)WORD_AT(a+RBS_WALK_AI_NEXT)<0) {
        WORD_AT(a+RBS_WALK_AI_NEXT)=RBS_WALK_ROAR;
        if(!rbs_walk.woken++) rbs_log("Crow walker woken: no SPC1/SPC2 trigger in a MapMaker map; roar, then fight",a,ent);
    } else if(s!=RBS_WALK_ROAR) return;
    if(rbs_walk.roars || !MAPMAKER_PTR_OK(ctl=WORD_AT(ent+4u),RBS_ANIM_SIZE) || WORD_AT(ctl+RBS_ANIM_CUR) || WORD_AT(ctl+RBS_ANIM_ALT)) return;
    rbs_walk.roars++;
    r=guest_call(sub_0005F460,4,ent,RBS_WALK_STUN,0u,0x3F800000u);
    rbs_log(r?"Crow walker had never been animated: roar started (state 2 waits for an animation to finish)":
              "Crow walker roar refused by sub_0005F460 (animation not loaded or not allowed)",ent,r);
}

/* WRAP sub_00126560 (frame step before the scene-81 beam draw at 0x001CEE8C). */
void pc_mapmaker_realbosses_fx(void (*original)(void))
{
    int here=(rbs_walk.record || rbs_skel.record) && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBS_RET_FX;
    original();
    if(here) rbs_skel_fire();
    if(!rbs_walk.record) return;
    if(here) rbs_walk_heartbeat();
    if(!here || !rbs_walk.state || WORD_AT(RBS_WALK_STATE)!=rbs_walk.state || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;
    rbs_walk_wake();
    guest_call(sub_0029CCB0,0);
    if(!rbs_walk.fx++) rbs_log("Crow walker effects drawn",rbs_walk.state,0);
}

/* WRAP sub_002274C0 (cdecl event, arg): the boss's death fires level event 3
 * (0x0029E063); in MapMaker that is the map's own logic marker. Dropped. */
void pc_mapmaker_realbosses_event(void (*original)(void))
{
    uint32_t ret;
    if(rbs_walk.record && MAPMAKER_STACK_OK(12) && ((ret=WORD_AT(g_esp))==RBS_RET_WALK_INTRO || ret==RBS_RET_WALK_INTRO2 ||
       ret==RBS_RET_WALK_DONE) && WORD_AT(RBS_SCENE)==100u && rbs_on()) {
        if(rbs_walk.events++<4u) rbs_log("Crow walker level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),ret);
        if(ret==RBS_RET_WALK_DONE) rbs_walk_remove();
        MAPMAKER_GUEST_RET();
        return;
    }
    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=RBS_RET_EVENT || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) { original();return; }
    if(rbs_skel.events++<4u) rbs_log("giant skeleton level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),WORD_AT(g_esp+8u));
    MAPMAKER_GUEST_RET();
}

static uint8_t rbs_boss_flag(uint32_t index)
{
    uint32_t container=WORD_AT(RBS_CONTAINER),c,end,steps;
    if(!container || !MAPMAKER_PTR_OK(container,RBS_SCAN) || WORD_AT(container)!=RBS_MAGIC) return 0;
    c=container+8u;end=container+RBS_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u);
        if(tag==RBS_TAG_END || sz>end-c-8u) break;
        if(tag==RBS_TAG_SAI && sz>=RBS_SAI_MIN && RBS_H(c+8u+RBS_SAI_INDEX)==index) {
            uint8_t f=RBS_B(c+8u+RBS_SAI_BOSS);
            if((f&0xF8u)!=0xB0u) return 0;
            return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);
        }
        c+=8u+sz;
    }
    return 0;
}
static void rbs_note(uint32_t record,uint8_t boss)
{
    unsigned k,free_k=RBS_REAL_MAX;
    for(k=0;k<RBS_REAL_MAX;k++) {
        if(rbs.real[k].record==record) break;
        if(!rbs.real[k].record && free_k==RBS_REAL_MAX) free_k=k;
    }
    if(k==RBS_REAL_MAX) {
        if(free_k==RBS_REAL_MAX) { rbs_log("real boss table full; bar may be doubled",RBS_REAL_MAX,record);return; }
        k=free_k;
    }
    rbs.real[k].record=record;rbs.real[k].boss=boss;
}

/* WRAP sub_0015B830 (cdecl AI pool entry): give the deerbeast its class. */
/* WRAP sub_000DAC30 (cdecl character id, ..., pool slot: creates a character
 * object with sub_000A6F90, 0 on failure, 0x000DAD7C). Log only: a real boss
 * character being created in MapMaker play and what came back (user report,
 * v0.4.0-mapmaker.17: the Crow walker never appeared and no AI start was
 * logged for it). */
void pc_mapmaker_realbosses_create(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(16)?WORD_AT(g_esp+4u):0u,slot=id?WORD_AT(g_esp+12u):0u;
    static unsigned n;
    original();
    if((id!=RBS_ID && id!=RBS_SKEL_ID && id!=RBS_WALK_ID) || n>=12u || !rbs_play()) return;
    n++;
    fprintf(stderr,"[mapmaker] realbosses: character 0x%X created in pool slot %u: object 0x%X%s\n",(unsigned)id,(unsigned)slot,
            (unsigned)g_eax,g_eax?"":" (creation failed)");
    fflush(stderr);
}

void pc_mapmaker_realbosses_ai_start(void (*original)(void))
{
    uint32_t a,ent,rec,id;
    if(!MAPMAKER_STACK_OK(8) || !rbs_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || !MAPMAKER_PTR_OK(rec=WORD_AT(a+RBS_ACTOR_RECORD),12u)) { original();return; }
    ent=WORD_AT(a);
    if(rbs_walk.state && rbs_walk.seen++<12u) {          /* which characters start, while the walker is missing */
        fprintf(stderr,"[mapmaker] realbosses: AI start: character 0x%X, model 0x%X, record id 0x%X\n",(unsigned)RBS_H(a+RBS_ACTOR_CHAR),
                MAPMAKER_PTR_OK(ent,0x100u)?(unsigned)RBS_H(ent+RBS_ENT_MODEL):0u,(unsigned)WORD_AT(rec+8u));
        fflush(stderr);
    }
    if(RBS_H(a+RBS_ACTOR_CHAR)==RBS_WALK_ID || (MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_WALK_MODEL)) {
        if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: Crow walker with bot behaviour",a,0);
        else if(!rbs_walk_pak_mounted()) rbs_log("lv81.pak not mounted: Crow walker with bot behaviour",a,0);
        else if(!rbs_walk.state || WORD_AT(RBS_WALK_STATE)!=rbs_walk.state)
            rbs_log("no Crow walker state this level: bot behaviour",a,WORD_AT(RBS_WALK_STATE));
        else if(rbs_walk.record && rbs_walk.record!=rec) {
            if(rbs_walk.extras++<4u) rbs_log("only one Crow walker per map: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
        } else {
            id=WORD_AT(rec+8u);
            WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_WALK_CLASS;
            rbs_walk.record=rec;rbs_walk.actor=a;
            rbs_note(rec,pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);
            rbs_log("Crow walker gets its story AI (class 0x100000)",a,id);
        }
        original();
        return;
    }
    if(RBS_H(a+RBS_ACTOR_CHAR)==RBS_SKEL_ID || (MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_SKEL_MODEL)) {
        if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant skeleton with bot behaviour",a,0);
        else if(!rbs_skel_pak_mounted()) rbs_log("lv1_6.pak not mounted: giant skeleton with bot behaviour",a,0);
        else if(!rbs_skel.state || WORD_AT(RBS_SKEL_STATE)!=rbs_skel.state)
            rbs_log("no giant skeleton state this level: bot behaviour",a,WORD_AT(RBS_SKEL_STATE));
        else if(rbs_skel.record && rbs_skel.record!=rec) {
            if(rbs_skel.extras++<4u) rbs_log("only one giant skeleton per map: this one keeps the bot behaviour",a,WORD_AT(rec+8u));
        } else {
            id=WORD_AT(rec+8u);
            WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_SKEL_CLASS;
            rbs_skel.record=rec;
            rbs_note(rec,pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);
            rbs_log("giant skeleton gets the mansion boss AI (class 0x20000)",a,id);
        }
        original();
        return;
    }
    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_MODEL)) {
        original();
        return;
    }
    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy monster with bot behaviour",a,0);
    else if(!rbs_pak_mounted()) rbs_log("lv1_3.pak not mounted: giant trophy monster with bot behaviour",a,0);
    else {
        id=WORD_AT(rec+8u);
        WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;
        rbs_note(rec,pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);
        rbs_log("giant trophy monster gets the story monster AI (class 0x80)",a,id);
    }
    original();
}

/* WRAP sub_0014CC80 (story boss bar; chain INSIDE pc_mapmaker_bosses_hud). */
void pc_mapmaker_realbosses_hud(void (*original)(void))
{
    uint32_t player,focus,a,rec;
    unsigned k;
    if(!rbs.real[0].record) { original();return; }   /* cheap: no real boss started yet (filled from 0) */
    player=WORD_AT(RBS_PLAYER);
    if(!MAPMAKER_PTR_OK(player,RBS_FOCUS+4u) || !MAPMAKER_PTR_OK(focus=WORD_AT(player+RBS_FOCUS),0x100u) ||
       (RBS_H(focus+RBS_ENT_MODEL)!=RBS_MODEL && RBS_H(focus+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&
        RBS_H(focus+RBS_ENT_MODEL)!=RBS_WALK_MODEL) || !rbs_play()) { original();return; }
    a=WORD_AT(focus+RBS_ENT_AI);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR)) { original();return; }
    rec=WORD_AT(a+RBS_ACTOR_RECORD);
    for(k=0;k<RBS_REAL_MAX && rbs.real[k].record!=rec;k++) {}
    if(k==RBS_REAL_MAX) { original();return; }
    /* never the creature's own bar: Boss bar = the bosses area's; Sub-boss =
     * the meter; Boss Off = no health shown at all (user, 2026-10-31) */
    WORD_AT(player+RBS_FOCUS)=0;
    original();
    WORD_AT(player+RBS_FOCUS)=focus;
}

/* WRAP sub_0013A400 (cdecl actor, 0, 0x7F, 0x6E; MapMaker story overhead
 * meter, per frame per actor with a +0x268 record): no meter on a real boss. */
void pc_mapmaker_realbosses_meter(void (*original)(void))
{
    uint32_t a,ent,rec;
    unsigned k;
    if(!rbs.real[0].record || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=RBS_RET_METER) { original();return; }
    a=WORD_AT(g_esp+4u);
    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) { original();return; }
    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&
       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&
       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_WALK_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_WALK_MODEL) { original();return; }
    rec=WORD_AT(a+RBS_ACTOR_RECORD);
    for(k=0;k<RBS_REAL_MAX && rbs.real[k].record!=rec;k++) {}
    if(k==RBS_REAL_MAX || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play()) { original();return; }   /* Sub-boss: keep it */
    if(!rbs.meters++) rbs_log("overhead health meter hidden on a real story boss",a,ent);
    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */
}
