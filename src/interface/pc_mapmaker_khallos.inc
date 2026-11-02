/* US 45410066 MapMaker: Khallos in the flying chair, the jetpack boss of story
 * mission 4 (the train), in MapMaker story play (scene 100, story mode). Area
 * `khallos`; every hook is inert unless TSFP_MAPMAKER_LIMITS=1 (opt out of
 * this boss alone with TSFP_MAPMAKER_KHALLOS=0).
 * Docs: docs/mapmaker/mapmaker-khallos.md. Research basis:
 * logs/limits/features/real-bosses.md section 5 (main checkout, ignored).
 * CONFIRMED facts are checked by tests/mapmaker/test_mapmaker_khallos.py.
 *
 * The chair is a prop: model 0x376 TRAIN_KHALLOSJETPACK (resource type
 * 0x4000000, train/khallosjetpack, init index 30 -> sub_001F4E00 case 0x376),
 * a scene-100 resource row whose model file is only in the lv11 stream paks;
 * this area mounts pak/stream/lv11/prop3.pak (string 0x0049E1D0). Init
 * sub_001EEA30 (esi = object) zeroes the static state 0x0078F000..0x0078F06F
 * and sets: [0x0078F000] = object, live position [0x0078F004..0C] =
 * (-576.2, 7.5, 0), velocity [0x0078F010..18], active flag [0x0078F04C] = 0,
 * jet emitter [0x0078F054], health 60. Update sub_001EEBD0 (slot 3): waits
 * until the player's x [player+0xA8] passes -535 ([0x00500088]), then sets
 * the story focus, fires level event 0x29 and flies: thrust 9.8 +- 3 along
 * the chair's tilt against gravity 9.8, more lift below y 7, less above
 * y 10, a tilt controller steering at (player x [player+0xB4] - 11
 * ([0x00500094]), 10, 0), yaw clamped to 60..120 degrees; it integrates the
 * live position and copies it to the object (+0x64). It fires every 1-2.5 s
 * from two gun nodes at the player. Damage sub_001EF840 (only while active):
 * health -= damage, level events 0x12 (health <= 35) and 0x13 (<= 15); at
 * health < 0 the train's death: tag 'FTWL', three emitters stopped, focus
 * cleared, level event 4 and train particle effects at fixed train points
 * (definitions 0x0078E900/0x0078EF60 and emitter [0x0078E468], all loaded by
 * the train level setup sub_001F1260/sub_001F5A10). After that the update
 * rockets the dead chair away (thrust x4) for ever. Only 0x00500088 and
 * 0x00500094 among the train constants are read by nothing else.
 *
 * The seated Khallos is a separate story AI of class 8 (AI +0x14 = 8, +0x10
 * = 0: sub_0027D590 0x0027D5B4 -> sub_00263260 at 0x0027DA9B): it plays
 * `khallosidle` (0xBFB, named only by pak/stream/lv11/anim/lv11.pak, string
 * 0x0049D9C8) and copies the chair's live position plus its rotated seat
 * offset and its angles every frame.
 *
 * Several chairs (user, 2026-11-01: "All of these one-per-map limits need
 * fixed"): up to KHL_MAX (4) per map. The chair statics (the 0x70 bytes from
 * 0x0078F000 the init zeroes: object, live position, velocity, active flag,
 * jet emitter, gun toggle, fire timer, the rider's sway at +0x60..+0x6C) and
 * the death's emitters [0x0078DF50..58] are kept per chair and loaded around
 * every way into the chair code: the creation (the init writes them), the
 * update, the damage callback (sub_001EF840 reads [0x0078F000], not the hit
 * object; the dispatcher sub_000BB490 passes the object hit at record +0),
 * and the seated Khallos' handler sub_00263260; the previous values are put
 * back after (nests). Each chair has its own placeholder, rider, frame and
 * removal.
 * The jet [0x0078F054] and the first death emitter [0x0078DF50] are owner
 * slots: the emitter keeps the slot address (+0) and the particle system
 * writes 0 there when it frees it, at any time. Each chair owns two guest
 * slots its emitters point at; only while its code runs do they point at the
 * stock slots (v0.4.0-mapmaker.26 crashed with a single chair).
 *
 * The seated Khallos (v0.4.0-mapmaker.27: "khallos is not actually visible"):
 * no Khallos AI was placed, so no rider existed. Now the placeholder itself is
 * made with Khallos' models (character table 0x0053A3CC: id 2's +4/+8 copied
 * into id 0x33's for the sub_000DAC30 call only, both requested at level
 * init) and rides its own chair, unremoved. Hits on him go to the chair
 * (his health topped up each frame). Bars: the chair's stock bar
 * ("Khallos", focus) is hidden for a ridden chair; the bosses area draws one
 * bar per Boss-flagged rider with his Story AI's name and the chair's health
 * (bos_bar_proxy, the chair as the bar's entity).
 *
 * Placement (no new save bytes): a Story AI whose character is the
 * placeholder id KHL_ID 0x33 (the fifth copy of the story-only cutscene
 * soldier `soldiermodel_talk`, no Arcade row, used by no other port). A
 * Khallos Story AI (id 2) placed within KHL_RIDE metres of it rides the
 * chair (when the placeholder could not be made as Khallos). A stock or
 * older build plays both as ordinary bots.
 *
 * Play, per level:
 *  - mount (sub_00062790): after the original, forget the last level; when
 *    the map's own set holds KHL_ID (or cannot be read), mount prop3.pak with
 *    sub_000622D0(path, 0, 1) (kept open, read on demand) if the 64-slot
 *    table keeps 8 free.
 *  - anims (sub_001C8540, before sub_001CA2F0 registers the mounted anim
 *    paks): when the set holds KHL_ID, mount lv11.pak with
 *    sub_0005A130 (read whole, like the stock level anim paks).
 *  - request (sub_0015E7C0): request model 0x376 (sub_000A5E40) for the
 *    level's load pass; nothing else asks for it in a MapMaker map. Without
 *    Khallos (id 2) in the set, also his models (id 2's +4 and +8).
 *  - ai_start (sub_0015B830): after the original. For the first KHL_ID
 *    entry, when the pak is mounted, the relocated resource map (0x008CA000)
 *    has 0x376 with init sub_001F4E00 and the model is loaded or read:
 *    sub_000A6F90(0x376, &pos, &rot, 0, 0) at the placeholder. If the train's
 *    other variant ran (init path for player +0x1568 == 0xE/0xF, it never
 *    sets [0x0078F000]) the object is removed (sub_000A9070) and the
 *    placeholder stays a bot. Else the live position becomes the spawn +
 *    (0, KHL_LIFT, 0). Khallos entries (id 2) are noted; the nearest within
 *    KHL_RIDE of the chair's spawn becomes the rider (AI +0x10 = 0, +0x14 = 8,
 *    next state +0x9D0 = 0), only with lv11.pak mounted.
 *  - update (sub_001EEBD0, cdecl object): first frame removes the
 *    placeholder AI (sub_0015DDA0). Until active: the start line is forced
 *    to "not yet" (0x00500088 = -FLT_MAX) unless a living player is within
 *    KHL_WAKE metres (x/z), then to "now" (+FLT_MAX). Active: the call runs
 *    in a frame moved by d = (0, spawn y + KHL_LIFT - 7.5, z) where z is the
 *    steered player's z clamped to spawn z +- KHL_ROAM: the live position,
 *    the object position and every player's +0xB8/+0xBC are moved by -d and
 *    put back after (players bit for bit). So the train's height band 7..10
 *    and target z 0 land around the spawn. 0x00500094 is set so the x target
 *    is the stock "player x - 11" clamped to spawn x +- KHL_ROAM. Both
 *    constants are restored after the call. Calls out of the update that use
 *    world positions run unshifted (flame, flash, shot, sound, engine below).
 *    A dead chair (health < 0) is removed once it is KHL_GONE_UP above its
 *    spawn or after KHL_GONE_FRAMES frames: sub_000A9070(object) and
 *    sub_0010E2A0(jet emitter) as the train's rising variant sub_001EFBA0
 *    does, [0x0078F000] = 0, the rider removed; later calls are skipped.
 *  - damage (sub_001EF840, cdecl hit record): a hit that would take the
 *    active chair below 0 runs with the damage cut to the remaining health
 *    (exactly 0: no train death), then health = the real result, the three
 *    emitters are stopped (sub_0010EB90(e, 0), null safe) and the focus is
 *    cleared. Every other hit is the original.
 *  - event (sub_002274C0): the chair's events 0x29, 0x12, 0x13 and 4
 *    (returns 0x001EEC72, 0x001EF9A2, 0x001EF9C2, 0x001EFAEC) are dropped in
 *    scene 100: they would be the map's own logic markers.
 *  - create (sub_000DAC30, character creation) and every AI start of id
 *    0x33: logged (v0.4.0-mapmaker.25: the chair never appeared and no AI
 *    start was logged for its placeholder). An id 0x33 made while lv11.pak
 *    is mounted, a chair can be made and Khallos' models are read gets them.
 *  - ai_start: a placeholder whose entity model (+0x94) is Khallos' rides
 *    its own chair (class 8 as below).
 *  - hud (sub_0014CC80, chained INSIDE pc_mapmaker_bosses_hud): a focus on a
 *    seated chair is cleared for the stock call and put back after.
 *  - text (sub_0003D7B0): the bar for model 0x376 is labelled with text
 *    0xFF1 (return 0x0014CDA6), a level text id in scene 100: "Khallos".
 *  - flame (sub_0010E730, return 0x001EED7A, emitter position arg 2), flash
 *    (sub_000DFFA0, 0x001EF689/0x001EF6EA, muzzle position arg 3), shot
 *    (sub_000EA9C0, 0x001EF75B, projectile position arg 2), sound
 *    (sub_00045820, 0x001EF7B4, sound record arg 1, position +0x1C) and
 *    engine (sub_000468A0, 0x001EF824, position arg 2 = sub_000A5A60's static
 *    chair position): inside the moved update only, the frame is put back to
 *    world for the call and the position argument moved by +d, then all is
 *    restored bit for bit.
 *  - v0.4.0-mapmaker.28 ("didn't orient toward me", "floated in front of
 *    their chairs", "not much audio"): the active update runs with the yaw
 *    clamp half-width 0x005000B8 (30: yaw 90 +- 30, facing +x like the train)
 *    set to 360, so the chair turns to the player at any bearing; after it,
 *    the living rider is put on the seat (live position + [0x0049E4BC] x the
 *    chair's up row +0x284, its angles) and an AI class or handler state he
 *    cannot sit in is put back; sound_load (sub_00045670) loads the chair's
 *    level-11 gun 0x393 and engine loop 0x394 in a map holding it. Status
 *    lines per chair: yaw, player offset, seat offset, rider calls, sounds.
 *
 * Requires the includer's WORD_AT, guest_call, g_esp/g_eax,
 * g_xbox_mem_offset, xbox_HeapAlloc and pc_mapmaker_limits.inc
 * (mapmaker_limits, MAPMAKER_GUEST_RET, MAPMAKER_PTR_OK); uses
 * pc_mapmaker_bosses.inc's bos_bar_proxy when BOS_BAR_PROXY is defined (it is
 * included first). Hot hooks (hud, text,
 * flame, flash, shot, sound, engine) test fixed values or a flag first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPMAKER_STACK_OK
#define MAPMAKER_STACK_OK(n) (g_esp>=0x00010000u && g_esp<0x04000000u-(uint32_t)(n))
#endif

#define KHL_SCENE 0x0079094Cu
#define KHL_MODE 0x007DE455u             /* byte: 10 = story */
#define KHL_ID 0x33u                     /* placeholder: soldiermodel_talk (story-only copy) */
#define KHL_RIDER_ID 0x02u               /* Khallos (chrs/trainkhallos) */
#define KHL_MODEL 0x376u                 /* TRAIN_KHALLOSJETPACK */
#define KHL_INIT 0x001F4E00u             /* resource init index 30 */
#define KHL_RESMAP 0x008CA000u           /* model -> row, moved there by the rooms area */
#define KHL_RESMAP_MODELS 0xA00u
#define KHL_RES 0x00791D4Cu              /* resource table, 0x30 per entry */
#define KHL_RES_INIT 0x2Cu
#define KHL_RES_LOADED 0x4u
#define KHL_RES_PATH 0x10u
#define KHL_RES_LOD 0x14u
#define KHL_REQ_DATA 0x10u
#define KHL_PAK 0x0049E1D0u              /* "pak/stream/lv11/prop3.pak" */
#define KHL_ANIM_PAK 0x0049D9C8u         /* "pak/stream/lv11/anim/lv11.pak" */
#define KHL_MOUNTS 0x006D7880u
#define KHL_MOUNT_MAX 64u                /* paktable.json: 64 slots at 0x00907000 */
#define KHL_MOUNT_KEEP 8u
#define KHL_MOUNT_SKIP 0x006D7C4Cu       /* sub_00062790 does nothing when set */
#define KHL_CONTAINER 0x00784014u
#define KHL_MAGIC 0x00C77667u
#define KHL_TAG_INFO 0x00C77668u
#define KHL_TAG_END 0x00C7766Au
#define KHL_TAG_SETS 0x00C776F5u         /* pc_mapmaker_sets.inc: slots 11-20 */
#define KHL_SCAN 0x00080000u
#define KHL_INFO_SET 0x14u
#define KHL_INFO_COUNT 0x16u
#define KHL_INFO_MEMBERS 0x18u
#define KHL_INFO_SLOTS 10u
#define KHL_SET_MAX 20u
#define KHL_SET_MAP 0x11u
#define KHL_SETS_SIZE 0x30u
#define KHL_ACTOR 0xEA0u                 /* AI pool entry (the AI itself) */
#define KHL_ACTOR_CHAR 0x48u
#define KHL_ACTOR_RECORD 0xBF0u
#define KHL_AI_CLASS_HI 0x10u            /* sub_0027D590: eax = AI +0x10, ecx = AI +0x14 */
#define KHL_AI_CLASS 0x14u
#define KHL_AI_NEXT 0x9D0u               /* sub_00263260: requested state, -1 = none */
#define KHL_RIDER_CLASS 8u
#define KHL_ENT_AI 0x7Cu
#define KHL_ENT_POS 0x64u                /* x, y (up), z */
#define KHL_ENT_YAW 0x70u
#define KHL_ENT_HEALTH 0x1CCu
#define KHL_ENT_MAX 0x1D8u
#define KHL_ENT_MODEL 0x94u              /* word: the model the entity was made with */
#define KHL_CHARS 0x0053A3CCu            /* character table, 0x30 per id: +4 model, +8 second model (-1 none) */
#define KHL_CHAR_STRIDE 0x30u
#define KHL_CHAR_MODEL 4u
#define KHL_CHAR_MODELS 2u               /* +4 and +8 (sub_000DAC30 0x000DAC59, 0x000DB084) */
#define KHL_MODEL_MAX 0x9D0u             /* sub_000A5E40 ignores ids >= this */
#define KHL_PLAYER 0x007B0C7Cu
#define KHL_FOCUS 0xFF4u
#define KHL_PLAYERS 0x00790950u          /* count */
#define KHL_PLAYER_BASE 0x007B0C48u      /* players, 0x1584 each, entity +0x14 */
#define KHL_PLAYER_STRIDE 0x1584u
#define KHL_PLAYER_ENT 0x14u
#define KHL_P_LINE 0xA8u                 /* x for the start line and the player pick */
#define KHL_P_POS 0xB4u                  /* x, y, z steered at and aimed at */
#define KHL_OBJ 0x0078F000u              /* the chair */
#define KHL_POS 0x0078F004u              /* live position x, y, z */
#define KHL_ACTIVE 0x0078F04Cu
#define KHL_JET 0x0078F054u              /* jet emitter */
#define KHL_SMOKE 0x0078DF50u            /* three emitters the death stops */
#define KHL_LINE 0x00500088u             /* -535.0: start line (update only) */
#define KHL_GAP 0x00500094u              /* 11.0: x target = player x - gap (update only) */
#define KHL_SPAN 0x005000B8u             /* 30.0: yaw clamped to 90 +- this (update only, 0x001EEEDE) */
#define KHL_SPAN_FREE 360.0f             /* no clamp: atan2 yaws are within 90 +- 270 */
#define KHL_SEAT 0x0049E4BCu             /* -1.2: the rider's seat, along the chair's up row (sub_00263260) */
#define KHL_SEAT_STOCK (-1.2f)
#define KHL_ENT_ROT 0x274u               /* rotation cached by sub_000A14B0 in the update; row 1 (+0x10) up */
#define KHL_ENT_PITCH 0xC0u              /* angles the handler copies: +0x70 (also to +0xC4), +0xC0, +0xC8 */
#define KHL_ENT_YAW2 0xC4u
#define KHL_ENT_ROLL 0xC8u
#define KHL_AI_STATE 0x20u               /* sub_00263260 knows states 0 (sit down) and 1 (seated) only */
#define KHL_SND_GUN 0x393u               /* SFX_11_KHALLOS_GUN, sounddata flags 0x0B00000C: level 11 only */
#define KHL_SND_JET 0x394u               /* SFX_11_JETPACK_NEW, 0x0B00041C: level 11 only (engine loop) */
#define KHL_SND_LOAD 4u                  /* sub_00045670's "load it this level" */
#define KHL_STATUS 6u                    /* status lines per chair */
#define KHL_STATUS_EVERY 300u            /* frames between them */
#define KHL_FLT_MAX 0x7F7FFFFFu
#define KHL_FLT_LOW 0xFF7FFFFFu          /* -FLT_MAX */
#define KHL_TRAIN_Y 7.5f                 /* the train start height (0x0047D014) */
#define KHL_LIFT 1.0f                    /* chair start height over the spawn floor */
#define KHL_ROAM 12.0f                   /* steering box: spawn x/z +- this */
#define KHL_WAKE 30.0f                   /* metres (x/z) */
#define KHL_RIDE 5.0f                    /* rider: Khallos AI this near the spawn (x/z) */
#define KHL_GONE_UP 25.0f                /* dead chair removed this far above its spawn */
#define KHL_GONE_FRAMES 240u             /* or after this many frames */
#define KHL_REC_DAMAGE 0x20u             /* hit record: damage, hit point, direction */
#define KHL_REC_HIT 0x28u
#define KHL_REC_DIR 0x30u
#define KHL_CANDS 8u
#define KHL_MAX 4u                       /* chairs per map */
#define KHL_BLOCK_N 0x1Cu                /* words of chair statics from 0x0078F000 (sub_001EEA30 zeroes these) */
#define KHL_SMOKE_N 3u                   /* the death's emitters [0x0078DF50..58] */
#define KHL_OWNERS 2u                    /* slots the engine knows as emitter owners: the jet, the death's first */
#define KHL_EMIT_OWNER 0x0u              /* emitter +0: its owner slot, cleared by the engine on free (sub_0010E070) */
#define KHL_LOGS 64u                     /* log lines per level */
#define KHL_HITS 4u                      /* hit lines per chair */
#define KHL_CREATES 8u                   /* placeholder/Khallos creation and AI start lines per level */
#define KHL_RET_START 0x001EEC72u        /* event 0x29 (call 0x001EEC6D) */
#define KHL_RET_HURT 0x001EF9A2u         /* event 0x12 */
#define KHL_RET_LOW 0x001EF9C2u          /* event 0x13 */
#define KHL_RET_DEAD 0x001EFAECu         /* event 4 */
#define KHL_RET_FLAME 0x001EED7Au        /* sub_0010E730(jet, pos) */
#define KHL_RET_FLASH 0x001EF689u        /* sub_000DFFA0(2, 2.0, pos, dir, ..), gun 1 */
#define KHL_RET_FLASH2 0x001EF6EAu       /* gun 2 */
#define KHL_RET_SHOT 0x001EF75Bu         /* sub_000EA9C0(0x35, pos, dir, ..) */
#define KHL_RET_SOUND 0x001EF7B4u        /* sub_00045820(record): position +0x1C */
#define KHL_SOUND_POS 0x1Cu
#define KHL_RET_ENGINE 0x001EF824u       /* sub_000468A0(chair +0x22C, pos, ..) */
#define KHL_BAR_TEXT 0xFF1u
#define KHL_RET_BAR_TEXT 0x0014CDA6u
#define KHL_LABEL "Khallos"
#define KHL_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define KHL_H(a) (*(uint16_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

extern void sub_00062260(void),sub_000622D0(void),sub_0005A130(void),sub_000A5E40(void),sub_000631C0(void);
extern void sub_000A6F90(void),sub_0015DDA0(void),sub_000A9070(void),sub_0010E2A0(void),sub_0010EB90(void);
extern uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t alignment);

/* Kept for the session: settings, label and the spawn argument buffer. */
static struct { int read, optout; uint32_t label, args, slots; unsigned logged; } khl;
/* sub_00111AE0 keeps the owner slot's address in a new emitter (+0) and the
 * emitter in the slot; sub_0010E070 (an emitter freed or recycled, at any
 * time) writes 0 through that address. Each chair therefore owns guest slots
 * (khl.slots) for its jet and death emitters: its emitters point there, and
 * only while its code runs do they point at the stock slots. */
static const uint32_t khl_owners[KHL_OWNERS]={0x0078F054u,0x0078DF50u};
/* This level's chairs: each its own copy of the chair statics and emitters,
 * its placeholder, rider and spawn (cleared by every level's pak mount). */
static struct {
    uint32_t obj, place, place_ent, place_rec;
    uint32_t rider, rider_ent, rider_rec;
    uint32_t block[KHL_BLOCK_N], smoke[KHL_SMOKE_N];
    float spawn[3];
    int removed, own;                      /* own: the placeholder itself rides */
    unsigned dying, hits;
    float off[3];                          /* the rider's distance from his seat before it was put back */
    unsigned frames, status, calls, fixes, shots, shot_fail, engine, engine_off;
} khl_ch[KHL_MAX];
static struct {
    uint32_t cand[KHL_CANDS], cand_ent[KHL_CANDS], cand_rec[KHL_CANDS];
    float cand_x[KHL_CANDS], cand_z[KHL_CANDS];
    int cand_bound[KHL_CANDS];
    unsigned ncand, n, extras, events, seen, creates, np, bars, sounds_logged;
    int requested, inside, cur, sounds;    /* sounds: load the chair's level-11 sounds this level */
    float d[3];                            /* the moved frame of the update running now */
    uint32_t pworld[4][2], pmoved[4][2];   /* players' +0xB8/+0xBC: world and moved bits */
    uint32_t self[4];                      /* live y/z and object y/z (moved) while unshifted */
} khl_lv;

static void khl_log(const char *what,uint32_t a,uint32_t b)
{
    if(khl.logged>=KHL_LOGS) return;
    khl.logged++;
    fprintf(stderr,"[mapmaker] khallos: %s (0x%X, 0x%X)\n",what,(unsigned)a,(unsigned)b);
    fflush(stderr);
}
static int khl_on(void)
{
    mapmaker_limits_read();
    if(!khl.read) {
        const char *v=getenv("TSFP_MAPMAKER_KHALLOS");
        khl.read=1;
        khl.optout=v && v[0]=='0' && !v[1];
    }
    return mapmaker_limits.limits;
}
static int khl_play(void) { return khl_on() && WORD_AT(KHL_SCENE)==100u && KHL_B(KHL_MODE)==10u; }
/* The chair index of this object (one of this level's chairs, scene 100), or -1. */
static int khl_chair(uint32_t obj)
{
    unsigned k;
    if(!obj || !khl_lv.n || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return -1;
    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].obj==obj) return (int)k;
    return -1;
}
/* The chair this entity rides, or -1. */
static int khl_ridden(uint32_t ent)
{
    unsigned k;
    if(!ent || !khl_lv.n || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return -1;
    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].rider && khl_ch[k].rider_ent==ent) return (int)k;
    return -1;
}

static float khl_getf(uint32_t a) { float f;memcpy(&f,(const void *)((uintptr_t)a+g_xbox_mem_offset),4);return f; }
static void khl_setf(uint32_t a,float f) { memcpy((void *)((uintptr_t)a+g_xbox_mem_offset),&f,4); }
static int khl_sane(float f) { return f==f && f>-100000.0f && f<100000.0f; }
static float khl_clamp(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }

/* The chair globals (statics block and the death's emitters) of chair k in,
 * the previous ones kept; khl_out stores chair k's and puts them back. */
static uint32_t khl_gslot(unsigned k,unsigned i) { return khl.slots?khl.slots+(k*KHL_OWNERS+i)*4u:0u; }
/* Moves an emitter's owner from slot `from` to slot `to` when it is still `from`. */
static void khl_reown(uint32_t e,uint32_t from,uint32_t to)
{
    if(e && MAPMAKER_PTR_OK(e,4u) && WORD_AT(e+KHL_EMIT_OWNER)==from) WORD_AT(e+KHL_EMIT_OWNER)=to;
}
static void khl_in(unsigned k,uint32_t *keep)
{
    uint32_t i,g,e;
    for(i=0;i<KHL_BLOCK_N;i++) { keep[i]=WORD_AT(KHL_OBJ+i*4u);WORD_AT(KHL_OBJ+i*4u)=khl_ch[k].block[i]; }
    for(i=0;i<KHL_SMOKE_N;i++) { keep[KHL_BLOCK_N+i]=WORD_AT(KHL_SMOKE+i*4u);WORD_AT(KHL_SMOKE+i*4u)=khl_ch[k].smoke[i]; }
    for(i=0;i<KHL_OWNERS;i++) {                           /* the chair's own slots are authoritative */
        if(!(g=khl_gslot(k,i))) continue;
        WORD_AT(khl_owners[i])=e=WORD_AT(g);
        khl_reown(e,g,khl_owners[i]);
    }
}
static void khl_out(unsigned k,const uint32_t *keep)
{
    uint32_t i,g,e;
    for(i=0;i<KHL_OWNERS;i++) {
        if(!(g=khl_gslot(k,i))) continue;
        WORD_AT(g)=e=WORD_AT(khl_owners[i]);
        khl_reown(e,khl_owners[i],g);
    }
    for(i=0;i<KHL_BLOCK_N;i++) { khl_ch[k].block[i]=WORD_AT(KHL_OBJ+i*4u);WORD_AT(KHL_OBJ+i*4u)=keep[i]; }
    for(i=0;i<KHL_SMOKE_N;i++) { khl_ch[k].smoke[i]=WORD_AT(KHL_SMOKE+i*4u);WORD_AT(KHL_SMOKE+i*4u)=keep[KHL_BLOCK_N+i]; }
}
/* The original with chair k's globals (nests). */
/* Chair slot k will not be used (a failed creation): its emitters lose their owner. */
static void khl_detach(unsigned k)
{
    uint32_t i,g,e;
    for(i=0;i<KHL_OWNERS;i++) {
        if(!(g=khl_gslot(k,i))) continue;
        e=WORD_AT(g);WORD_AT(g)=0;
        khl_reown(e,g,0u);
    }
}
static void khl_swapped(void (*original)(void),unsigned k)
{
    uint32_t keep[KHL_BLOCK_N+KHL_SMOKE_N];
    int prev=khl_lv.cur;
    khl_in(k,keep);
    khl_lv.cur=(int)k;
    original();
    khl_lv.cur=prev;
    khl_out(k,keep);
}

/* The map's own custom set (slots 1-10 in the map-info chunk, 11-20 in the
 * sets area's chunk). 1: holds id; 0: does not (or a preset set); -1: unreadable. */
static int khl_map_has(uint32_t id)
{
    uint32_t container=WORD_AT(KHL_CONTAINER),c,end,steps,info=0,isz=0,extra=0,n,k;
    if(!container || !MAPMAKER_PTR_OK(container,KHL_SCAN) || WORD_AT(container)!=KHL_MAGIC) return -1;
    c=container+8u;end=container+KHL_SCAN;
    for(steps=0;steps<0x40000u && end-c>=8u;steps++) {
        uint32_t tag=WORD_AT(c),sz=WORD_AT(c+4u),d=c+8u;
        if(tag==KHL_TAG_END || sz>end-c-8u) break;
        if(tag==KHL_TAG_INFO && sz>=KHL_INFO_MEMBERS && !info) { info=d;isz=sz; }
        else if(tag==KHL_TAG_SETS && sz>=KHL_SETS_SIZE && !extra && KHL_H(d)==1u && KHL_B(d+2u)==KHL_INFO_SLOTS &&
                KHL_B(d+3u)>=1u && KHL_B(d+3u)<=KHL_SET_MAX-KHL_INFO_SLOTS) extra=d;
        c+=8u+sz;
    }
    if(!info) return -1;
    if(KHL_H(info+KHL_INFO_SET)!=KHL_SET_MAP) return 0;
    n=KHL_B(info+KHL_INFO_COUNT);
    if(n>KHL_INFO_SLOTS) n=KHL_INFO_SLOTS;
    if(KHL_INFO_MEMBERS+n*4u>isz) n=(isz-KHL_INFO_MEMBERS)/4u;
    for(k=0;k<n;k++) if(KHL_H(info+KHL_INFO_MEMBERS+k*4u)==id) return 1;
    for(k=0;extra && k<KHL_B(extra+3u);k++) if(KHL_H(extra+4u+k*4u)==id) return 1;
    return 0;
}
static int khl_mounted(uint32_t path) { return (int32_t)guest_call(sub_00062260,1,path)!=-1; }
static uint32_t khl_resource(uint32_t model)
{
    int16_t i;
    uint32_t t=WORD_AT(KHL_RES),e;
    if(model>=KHL_RESMAP_MODELS) return 0;
    i=(int16_t)KHL_H(KHL_RESMAP+model*2u);
    if(i<0 || !t) return 0;
    e=t+(uint32_t)i*0x30u;
    return MAPMAKER_PTR_OK(e,0x30u)?e:0;
}
/* sub_00096410 takes a request's data without a null test: a model path is
 * ready when its request (sub_000631C0) holds the read file. */
static int khl_path_ready(uint32_t path,uint32_t *req)
{
    uint32_t r;
    *req=0;
    if(!path || !MAPMAKER_PTR_OK(path,4u)) return 0;
    r=guest_call(sub_000631C0,1,path);
    *req=r;
    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+KHL_REQ_DATA)!=0;
}
static int khl_model_ready(uint32_t e,uint32_t *req)
{
    uint32_t path;
    *req=0;
    if(WORD_AT(e+KHL_RES_LOADED)) return 1;             /* loaded before: no file read */
    if(!khl_path_ready(WORD_AT(e+KHL_RES_PATH),req)) return 0;
    return !(path=WORD_AT(e+KHL_RES_LOD)) || khl_path_ready(path,req);
}
/* Khallos' (id 2) model i (0: +4, 1: +8; 0xFFFFFFFF none). */
static uint32_t khl_rider_model(unsigned i) { return WORD_AT(KHL_CHARS+KHL_RIDER_ID*KHL_CHAR_STRIDE+KHL_CHAR_MODEL+i*4u); }
/* Both of Khallos' models loaded or read this level. */
static int khl_rider_ready(void)
{
    uint32_t m,e,req;
    unsigned i;
    for(i=0;i<KHL_CHAR_MODELS;i++) {
        if((m=khl_rider_model(i))==0xFFFFFFFFu && i) continue;
        if(m>=KHL_MODEL_MAX || !(e=khl_resource(m)) || !khl_model_ready(e,&req)) return 0;
    }
    return 1;
}
/* A chair can be made now (what ai_start checks, without its log lines). */
static int khl_chair_ready(void)
{
    uint32_t e,req;
    return khl_mounted(KHL_PAK) && (e=khl_resource(KHL_MODEL))!=0 && WORD_AT(e+KHL_RES_INIT)==KHL_INIT && khl_model_ready(e,&req);
}
static uint32_t khl_string(uint32_t *slot,const char *text,size_t n)
{
    if(!*slot) {
        uint32_t p=xbox_HeapAlloc((uint32_t)n,4u);
        if(p && MAPMAKER_PTR_OK(p,n)) {
            memcpy((void *)((uintptr_t)p+g_xbox_mem_offset),text,n);
            *slot=p;
        } else khl_log("no memory for the label",(uint32_t)n,0);
    }
    return *slot;
}
static void khl_unfocus(uint32_t ent)
{
    uint32_t player=WORD_AT(KHL_PLAYER);
    if(MAPMAKER_PTR_OK(player,KHL_FOCUS+4u) && WORD_AT(player+KHL_FOCUS)==ent) WORD_AT(player+KHL_FOCUS)=0;
}
/* A pool entry still holding the AI it held: record, entity and back link. */
static int khl_same_ai(uint32_t a,uint32_t ent,uint32_t rec)
{
    return MAPMAKER_PTR_OK(a,KHL_ACTOR) && WORD_AT(a+KHL_ACTOR_RECORD)==rec && WORD_AT(a)==ent &&
           MAPMAKER_PTR_OK(ent,0x100u) && WORD_AT(ent+KHL_ENT_AI)==a;
}

/* Chair k's rider: the nearest unbound noted Khallos within KHL_RIDE of its spawn. */
static void khl_bind_rider(unsigned k)
{
    unsigned c,best=KHL_CANDS;
    float bd=KHL_RIDE*KHL_RIDE,dx,dz;
    uint32_t a;
    if(!khl_ch[k].obj || khl_ch[k].rider || khl_ch[k].removed) return;
    for(c=0;c<khl_lv.ncand;c++) {
        if(khl_lv.cand_bound[c] || !khl_same_ai(khl_lv.cand[c],khl_lv.cand_ent[c],khl_lv.cand_rec[c])) continue;
        dx=khl_lv.cand_x[c]-khl_ch[k].spawn[0];dz=khl_lv.cand_z[c]-khl_ch[k].spawn[2];
        if(dx*dx+dz*dz<=bd) { bd=dx*dx+dz*dz;best=c; }
    }
    if(best==KHL_CANDS) return;
    a=khl_lv.cand[best];
    if(!khl_mounted(KHL_ANIM_PAK)) { khl_log("lv11.pak not mounted: Khallos near the chair stays a bot",a,k);return; }
    WORD_AT(a+KHL_AI_CLASS_HI)=0;WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;
    WORD_AT(a+KHL_AI_NEXT)=0;                             /* the handler's state 0: sit down */
    khl_lv.cand_bound[best]=1;
    khl_ch[k].rider=a;khl_ch[k].rider_ent=khl_lv.cand_ent[best];khl_ch[k].rider_rec=khl_lv.cand_rec[best];
    khl_log("Khallos rides the chair (rider, chair)",a,k);
}
static void khl_drop_rider(unsigned k)
{
    uint32_t a=khl_ch[k].rider,ent=khl_ch[k].rider_ent;
    if(!a) return;
    khl_ch[k].rider=0;
    if(!khl_same_ai(a,ent,khl_ch[k].rider_rec)) { khl_log("the rider is already gone",a,ent);return; }
    khl_unfocus(ent);
    guest_call(sub_0015DDA0,1,ent);
    khl_log("rider removed with the chair",a,ent);
}
/* Chair k's rider is still its AI. */
static int khl_seated(unsigned k) { return khl_ch[k].rider && khl_same_ai(khl_ch[k].rider,khl_ch[k].rider_ent,khl_ch[k].rider_rec); }
/* Chair k's placeholder, made with Khallos' models, rides its own chair. */
static void khl_seat_own(unsigned k,uint32_t a,uint32_t ent)
{
    if(!khl_mounted(KHL_ANIM_PAK)) { khl_log("lv11.pak not mounted: the Khallos placeholder stays a bot next to his chair",a,k);return; }
    WORD_AT(a+KHL_AI_CLASS_HI)=0;WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;
    WORD_AT(a+KHL_AI_NEXT)=0;                             /* the handler's state 0: sit down */
    khl_ch[k].rider=a;khl_ch[k].rider_ent=ent;khl_ch[k].rider_rec=khl_ch[k].place_rec;khl_ch[k].own=1;
    khl_log("the placeholder is Khallos and rides his own chair (rider, chair)",a,k);
}
/* Chair k (its globals in) at health < 0 without the train's death. */
static void khl_down(unsigned k,uint32_t obj)
{
    uint32_t i;
    for(i=0;i<3u;i++) guest_call(sub_0010EB90,2,WORD_AT(KHL_SMOKE+i*4u),0u);
    khl_unfocus(obj);
    khl_log("Khallos' chair shot down (chair, chair)",obj,k);
}
/* Chair k (its globals in): hits on its rider go to the chair while it
 * fights; his own health is put back to its maximum. */
static void khl_rider_health(unsigned k,uint32_t obj)
{
    uint32_t ent=khl_ch[k].rider_ent;
    float max,cur,h,left;
    if(!khl_seated(k)) return;
    max=khl_getf(ent+KHL_ENT_MAX);cur=khl_getf(ent+KHL_ENT_HEALTH);
    if(!(max>0.0f) || !(cur<max)) return;
    if(WORD_AT(KHL_ACTIVE)==1u && (h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) {
        khl_setf(obj+KHL_ENT_HEALTH,left=h-(max-(cur>0.0f?cur:0.0f)));
        if(khl_ch[k].hits<KHL_HITS) {
            khl_ch[k].hits++;
            fprintf(stderr,"[mapmaker] khallos: chair %u: its rider was hit, the chair takes it: health %.1f -> %.1f\n",k,(double)h,(double)left);
            fflush(stderr);
        }
        if(left<0.0f) khl_down(k,obj);
    }
    if(cur>0.0f) khl_setf(ent+KHL_ENT_HEALTH,max);
    else khl_log("the rider was killed by one hit (rider, chair)",khl_ch[k].rider,k);
}

#ifdef BOS_BAR_PROXY
/* pc_mapmaker_bosses.inc's bar entity for a Boss AI: the chair he rides. */
static uint32_t khl_bar_entity(uint32_t ent)
{
    int k=khl_ridden(ent);
    return k>=0 && !khl_ch[k].removed?khl_ch[k].obj:0u;
}
#endif

/* WRAP sub_00062790 (the level's pak mounts, no args). */
void pc_mapmaker_khallos_mount(void (*original)(void))
{
    int has;
    uint32_t mounted;
#ifdef BOS_BAR_PROXY
    bos_bar_proxy=khl_bar_entity;                          /* the bosses area's bar: a rider shows his chair */
#endif
    if(WORD_AT(KHL_MOUNT_SKIP)) { original();return; }    /* the original does nothing */
    original();
    memset(&khl_lv,0,sizeof khl_lv);                       /* a new level: forget the last chairs */
    memset(khl_ch,0,sizeof khl_ch);
    khl_lv.cur=-1;
    khl.logged=0;                                          /* and log it afresh */
    if(!khl.slots) {                                       /* the chairs' emitter owner slots, once */
        uint32_t s=xbox_HeapAlloc(KHL_MAX*KHL_OWNERS*4u,4u);
        if(s && MAPMAKER_PTR_OK(s,KHL_MAX*KHL_OWNERS*4u)) khl.slots=s;
    }
    if(khl.slots) memset((void *)((uintptr_t)khl.slots+g_xbox_mem_offset),0,KHL_MAX*KHL_OWNERS*4u);
    if(!khl_play() || khl.optout || (has=khl_map_has(KHL_ID))==0) return;
    khl_lv.sounds=1;                                       /* sub_000454F0 loads the level's sounds after this */
    if(khl_mounted(KHL_PAK)) return;
    if((mounted=WORD_AT(KHL_MOUNTS))+KHL_MOUNT_KEEP>KHL_MOUNT_MAX) {
        khl_log("pak mount table full; the chair placeholder stays a bot",mounted,KHL_MOUNT_MAX);
        return;
    }
    if((int32_t)guest_call(sub_000622D0,3,KHL_PAK,0u,1u)<0) khl_log("prop3.pak did not mount; the chair placeholder stays a bot",mounted,(uint32_t)has);
    else khl_log(has<0?"mounted prop3.pak (map container unreadable)":"mounted prop3.pak for Khallos' chair",mounted+1u,0);
}

/* WRAP sub_001C8540 (the level's anim paks, before their names are registered). */
void pc_mapmaker_khallos_anims(void (*original)(void))
{
    uint32_t mounted;
    original();
    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)) return;
    if(khl_mounted(KHL_ANIM_PAK)) return;
    if((mounted=WORD_AT(KHL_MOUNTS))+KHL_MOUNT_KEEP>KHL_MOUNT_MAX) {
        khl_log("pak mount table full; no seated Khallos",mounted,KHL_MOUNT_MAX);
        return;
    }
    if(!guest_call(sub_0005A130,1,KHL_ANIM_PAK)) khl_log("lv11.pak did not mount; no seated Khallos",mounted,0);
    else khl_log("mounted lv11.pak for the seated Khallos",mounted+1u,0);
}

/* WRAP sub_0015E7C0 (story level init, no args): request the chair model. */
void pc_mapmaker_khallos_request(void (*original)(void))
{
    uint32_t m;
    unsigned i;
    original();
    if(!khl_play() || khl.optout || khl_map_has(KHL_ID)==0) return;
    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init: chair model not requested",0,0);return; }
    if(!khl_resource(KHL_MODEL)) { khl_log("no resource row: chair model not requested",KHL_MODEL,0);return; }
    guest_call(sub_000A5E40,1,KHL_MODEL);
    khl_lv.requested=1;
    khl_log("requested the chair model",KHL_MODEL,0);
    if(khl_map_has(KHL_RIDER_ID)==1) return;              /* the set's preload (sub_000D7780) requests his */
    for(i=0;i<KHL_CHAR_MODELS;i++)                         /* the seated Khallos' models, as sub_000D7780 asks */
        if((m=khl_rider_model(i))<KHL_MODEL_MAX) guest_call(sub_000A5E40,1,m);
    khl_log("requested Khallos' models for the seated Khallos",khl_rider_model(0),khl_rider_model(1));
}

/* WRAP sub_000DAC30 (cdecl character id, ..., pool slot: makes a character
 * object, 0 on failure). Log only: each placeholder or Khallos character made
 * in MapMaker story play and what came back (v0.4.0-mapmaker.25: the chair
 * never appeared and no AI start was logged for its placeholder). */
void pc_mapmaker_khallos_create(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(16)?WORD_AT(g_esp+4u):0u,slot=id?WORD_AT(g_esp+12u):0u,keep[KHL_CHAR_MODELS],c;
    unsigned i;
    if(id==KHL_ID && khl_play() && !khl.optout && khl_lv.n<KHL_MAX && khl_mounted(KHL_ANIM_PAK) && khl_chair_ready() &&
       khl_rider_ready()) {                                /* the placeholder is made as Khallos for this call only */
        c=KHL_CHARS+KHL_ID*KHL_CHAR_STRIDE+KHL_CHAR_MODEL;
        for(i=0;i<KHL_CHAR_MODELS;i++) { keep[i]=WORD_AT(c+i*4u);WORD_AT(c+i*4u)=khl_rider_model(i); }
        original();
        for(i=0;i<KHL_CHAR_MODELS;i++) WORD_AT(c+i*4u)=keep[i];
        khl_log("chair placeholder made with Khallos' models (pool slot, object)",slot,g_eax);
    } else original();
    if((id!=KHL_ID && id!=KHL_RIDER_ID) || khl_lv.creates>=KHL_CREATES || !khl_play()) return;
    khl_lv.creates++;
    khl_log(g_eax?(id==KHL_ID?"chair placeholder character made (pool slot, object)":"Khallos character made (pool slot, object)"):
                  (id==KHL_ID?"chair placeholder character NOT made (pool slot, 0)":"Khallos character NOT made (pool slot, 0)"),slot,g_eax);
}

/* WRAP sub_0015B830 (cdecl AI pool entry): each placeholder becomes a chair
 * (up to KHL_MAX), a Khallos next to one its rider. */
void pc_mapmaker_khallos_ai_start(void (*original)(void))
{
    uint32_t a,ent,e,obj,args,i,id,req,m,keep[KHL_BLOCK_N+KHL_SMOKE_N];
    unsigned k;
    float pos[3],yaw;
    if(!MAPMAKER_STACK_OK(8) || !khl_play()) { original();return; }
    a=WORD_AT(g_esp+4u);
    original();
    if(!MAPMAKER_PTR_OK(a,KHL_ACTOR)) return;
    id=KHL_H(a+KHL_ACTOR_CHAR);
    if(id==KHL_ID && khl_lv.seen++<KHL_CREATES) {
        khl_log("chair placeholder AI starts (pool entry, entity)",a,WORD_AT(a));
        if(MAPMAKER_PTR_OK(WORD_AT(a),0x100u)) khl_log("chair placeholder model (model, Khallos' model)",KHL_H(WORD_AT(a)+KHL_ENT_MODEL),khl_rider_model(0));
    }
    if(khl.optout) { if(id==KHL_ID) khl_log("TSFP_MAPMAKER_KHALLOS=0: the chair placeholder stays a bot",a,0);return; }
    if(!MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) { if(id==KHL_ID) khl_log("chair placeholder without an entity: stays a bot",a,ent);return; }
    if(id==KHL_RIDER_ID) {
        if(khl_lv.ncand<KHL_CANDS) {
            i=khl_lv.ncand++;
            khl_lv.cand[i]=a;khl_lv.cand_ent[i]=ent;khl_lv.cand_rec[i]=WORD_AT(a+KHL_ACTOR_RECORD);
            khl_lv.cand_x[i]=khl_getf(ent+KHL_ENT_POS);khl_lv.cand_z[i]=khl_getf(ent+KHL_ENT_POS+8u);
            for(k=0;k<khl_lv.n;k++) khl_bind_rider(k);
        }
        return;
    }
    if(id!=KHL_ID) return;
    for(k=0;k<khl_lv.n && !(khl_ch[k].place_rec==WORD_AT(a+KHL_ACTOR_RECORD) && khl_ch[k].place==a);k++) {}
    if(k<khl_lv.n) { khl_log("this chair placeholder already has its chair",a,k);return; }
    if(khl_lv.n>=KHL_MAX) {
        if(khl_lv.extras++<4u) khl_log("more chairs than KHL_MAX (4) per map: this placeholder stays a bot",a,khl_lv.n);
        return;
    }
    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted: the chair placeholder stays a bot",a,0);return; }
    if(!(e=khl_resource(KHL_MODEL)) || WORD_AT(e+KHL_RES_INIT)!=KHL_INIT) {
        khl_log("chair model missing from the resource table: the placeholder stays a bot",e,e?WORD_AT(e+KHL_RES_INIT):0);
        return;
    }
    if(!khl_model_ready(e,&req)) {
        khl_log(khl_lv.requested?"the chair model was requested but not read: the placeholder stays a bot":
                                 "chair model not requested this level: the placeholder stays a bot",e,req);
        return;
    }
    for(i=0;i<3u;i++) if(!khl_sane(pos[i]=khl_getf(ent+KHL_ENT_POS+i*4u))) { khl_log("placeholder position unusable",a,i);return; }
    if(!khl_sane(yaw=khl_getf(ent+KHL_ENT_YAW))) yaw=0.0f;
    if(!khl.args) {
        uint32_t p=xbox_HeapAlloc(32u,16u);
        if(!p || !MAPMAKER_PTR_OK(p,32u)) { khl_log("no memory for the spawn arguments",32u,0);return; }
        khl.args=p;
    }
    args=khl.args;
    for(i=0;i<3u;i++) khl_setf(args+i*4u,pos[i]);
    khl_setf(args+12u,0.0f);khl_setf(args+16u,yaw);khl_setf(args+20u,0.0f);
    k=khl_lv.n;
    memset(khl_ch[k].block,0,sizeof khl_ch[k].block);memset(khl_ch[k].smoke,0,sizeof khl_ch[k].smoke);
    khl_in(k,keep);                                       /* the init writes the new chair's globals */
    obj=guest_call(sub_000A6F90,5,KHL_MODEL,args,args+12u,0u,0u);
    if(!obj || !MAPMAKER_PTR_OK(obj,0x100u)) {
        khl_out(k,keep);khl_detach(k);
        khl_log("the chair was not created (model not loaded?): placeholder stays a bot",a,obj);
        return;
    }
    if(WORD_AT(KHL_OBJ)!=obj) {                            /* the train's rising variant */
        guest_call(sub_000A9070,1,obj);
        khl_out(k,keep);khl_detach(k);
        khl_log("the chair took the train's other variant (player +0x1568): removed, placeholder stays a bot",obj,khl_ch[k].block[0]);
        return;
    }
    khl_setf(KHL_POS,pos[0]);khl_setf(KHL_POS+4u,pos[1]+KHL_LIFT);khl_setf(KHL_POS+8u,pos[2]);
    for(i=0;i<3u;i++) khl_setf(obj+KHL_ENT_POS+i*4u,khl_getf(KHL_POS+i*4u));
    khl_out(k,keep);
    khl_ch[k].obj=obj;khl_ch[k].place=a;khl_ch[k].place_ent=ent;khl_ch[k].place_rec=WORD_AT(a+KHL_ACTOR_RECORD);
    for(i=0;i<3u;i++) khl_ch[k].spawn[i]=pos[i];
    khl_lv.n=k+1u;
    khl_string(&khl.label,KHL_LABEL,sizeof KHL_LABEL);     /* off the text hook's path */
    khl_log("Khallos' chair created at the placeholder (placeholder, chair)",a,obj);
    fprintf(stderr,"[mapmaker] khallos: chair %u spawn (%.2f, %.2f, %.2f), anchor rebased to (%.2f, %.2f, %.2f)\n",k,
            (double)pos[0],(double)pos[1],(double)pos[2],(double)pos[0],(double)(pos[1]+KHL_LIFT),(double)pos[2]);
    fflush(stderr);
    if((m=khl_rider_model(0))!=0 && m<KHL_MODEL_MAX && KHL_H(ent+KHL_ENT_MODEL)==m) khl_seat_own(k,a,ent);   /* made as Khallos (create) */
    khl_bind_rider(k);
}

/* 1 when a living player stands within KHL_WAKE metres (x/z) of the chair. */
static int khl_near(uint32_t obj)
{
    uint32_t n=WORD_AT(KHL_PLAYERS),base=WORD_AT(KHL_PLAYER_BASE),k,ent;
    float x=khl_getf(obj+KHL_ENT_POS),z=khl_getf(obj+KHL_ENT_POS+8u),dx,dz;
    if(n>4u) n=4u;
    for(k=0;k<n && MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u);k++) {
        ent=WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_PLAYER_ENT);
        if(!MAPMAKER_PTR_OK(ent,0x200u) || !(khl_getf(ent+KHL_ENT_HEALTH)>0.0f)) continue;
        dx=khl_getf(ent+KHL_ENT_POS)-x;dz=khl_getf(ent+KHL_ENT_POS+8u)-z;
        if(dx*dx+dz*dz<=KHL_WAKE*KHL_WAKE) return 1;
    }
    return 0;
}
/* The player the update steers at: the second when story, two or more
 * players and the first's +0xA8 is larger (0x001EEBFF..0x001EEC2C). */
static uint32_t khl_steered(void)
{
    uint32_t base=WORD_AT(KHL_PLAYER_BASE);
    if(!MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*2u)) return 0;
    if(KHL_B(KHL_MODE)==10u && WORD_AT(KHL_PLAYERS)>1u &&
       khl_getf(base+KHL_P_LINE)>khl_getf(base+KHL_PLAYER_STRIDE+KHL_P_LINE)) return base+KHL_PLAYER_STRIDE;
    return base;
}
/* The update's frame: moved by -d (live position, object, players' y/z). */
static void khl_shift_in(uint32_t obj)
{
    uint32_t base=WORD_AT(KHL_PLAYER_BASE),n=WORD_AT(KHL_PLAYERS),k,i,p;
    if(n>4u) n=4u;
    if(!MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u)) n=0;
    khl_lv.np=n;
    for(k=0;k<n;k++)
        for(i=0;i<2u;i++) {
            p=base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u;
            khl_lv.pworld[k][i]=WORD_AT(p);
            khl_setf(p,khl_getf(p)-khl_lv.d[1u+i]);
            khl_lv.pmoved[k][i]=WORD_AT(p);
        }
    for(i=1;i<3u;i++) {
        khl_setf(KHL_POS+i*4u,khl_getf(KHL_POS+i*4u)-khl_lv.d[i]);
        khl_setf(obj+KHL_ENT_POS+i*4u,khl_getf(obj+KHL_ENT_POS+i*4u)-khl_lv.d[i]);
    }
}
static void khl_shift_out(uint32_t obj)
{
    uint32_t base=WORD_AT(KHL_PLAYER_BASE),k,i;
    for(i=1;i<3u;i++) {
        khl_setf(KHL_POS+i*4u,khl_getf(KHL_POS+i*4u)+khl_lv.d[i]);
        khl_setf(obj+KHL_ENT_POS+i*4u,khl_getf(obj+KHL_ENT_POS+i*4u)+khl_lv.d[i]);
    }
    for(k=0;k<khl_lv.np;k++)
        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pworld[k][i];
}
/* A call out of the moved update that uses world positions: the frame put
 * back for it (pos, a guest vector or 0, moved by +d), then restored bit for bit. */
static void khl_world_call(void (*original)(void),uint32_t pos)
{
    uint32_t base=WORD_AT(KHL_PLAYER_BASE),obj=khl_ch[khl_lv.cur].obj,k,i,keep[3]={0};
    int has=pos && MAPMAKER_PTR_OK(pos,12u);
    for(i=0;i<2u;i++) {
        khl_lv.self[i]=WORD_AT(KHL_POS+4u+i*4u);khl_lv.self[2u+i]=WORD_AT(obj+KHL_ENT_POS+4u+i*4u);
        khl_setf(KHL_POS+4u+i*4u,khl_getf(KHL_POS+4u+i*4u)+khl_lv.d[1u+i]);
        khl_setf(obj+KHL_ENT_POS+4u+i*4u,khl_getf(obj+KHL_ENT_POS+4u+i*4u)+khl_lv.d[1u+i]);
    }
    for(k=0;k<khl_lv.np;k++)
        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pworld[k][i];
    if(has) for(i=0;i<3u;i++) { keep[i]=WORD_AT(pos+i*4u);khl_setf(pos+i*4u,khl_getf(pos+i*4u)+khl_lv.d[i]); }
    khl_lv.inside=0;
    original();
    khl_lv.inside=1;
    if(has) for(i=0;i<3u;i++) WORD_AT(pos+i*4u)=keep[i];
    for(k=0;k<khl_lv.np;k++)
        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pmoved[k][i];
    for(i=0;i<2u;i++) { WORD_AT(KHL_POS+4u+i*4u)=khl_lv.self[i];WORD_AT(obj+KHL_ENT_POS+4u+i*4u)=khl_lv.self[2u+i]; }
}
/* Chair k (its globals in) is gone: removed as the train's rising variant
 * removes its own chair, its rider with it. */
static void khl_remove(unsigned k,uint32_t obj)
{
    uint32_t jet=WORD_AT(KHL_JET);
    khl_unfocus(obj);
    guest_call(sub_000A9070,1,obj);
    if(jet) { guest_call(sub_0010E2A0,1,jet);WORD_AT(KHL_JET)=0; }
    if(WORD_AT(KHL_OBJ)==obj) WORD_AT(KHL_OBJ)=0;
    khl_ch[k].removed=1;
    khl_log("the dead chair flew off and was removed (chair, frames)",k,khl_ch[k].dying);
    khl_drop_rider(k);
}

/* Chair k (its globals in, after its active update): its living rider is put
 * on the seat as sub_00263260 puts him (live position + seat x the chair's up
 * row, the chair's angles), whether or not his handler ran this frame
 * (v0.4.0-mapmaker.28: "two of them kind of floated in front of their
 * chairs"). How far off he was is kept for the log; an AI class or state the
 * handler cannot sit in is put back. */
static void khl_seat(unsigned k,uint32_t obj)
{
    uint32_t a=khl_ch[k].rider,ent=khl_ch[k].rider_ent,i;
    float c,want[3];
    if(khl_ch[k].removed || !khl_seated(k) || WORD_AT(KHL_OBJ)!=obj || !MAPMAKER_PTR_OK(obj,KHL_ENT_ROT+0x30u) ||
       !MAPMAKER_PTR_OK(ent,0x200u) || !(khl_getf(ent+KHL_ENT_HEALTH)>0.0f)) return;
    if(!khl_sane(c=khl_getf(KHL_SEAT))) c=KHL_SEAT_STOCK;
    for(i=0;i<3u;i++) if(!khl_sane(want[i]=khl_getf(KHL_POS+i*4u)+c*khl_getf(obj+KHL_ENT_ROT+0x10u+i*4u))) return;
    for(i=0;i<3u;i++) { khl_ch[k].off[i]=khl_getf(ent+KHL_ENT_POS+i*4u)-want[i];khl_setf(ent+KHL_ENT_POS+i*4u,want[i]); }
    WORD_AT(ent+KHL_ENT_YAW)=WORD_AT(ent+KHL_ENT_YAW2)=WORD_AT(obj+KHL_ENT_YAW);
    WORD_AT(ent+KHL_ENT_PITCH)=WORD_AT(obj+KHL_ENT_PITCH);WORD_AT(ent+KHL_ENT_ROLL)=WORD_AT(obj+KHL_ENT_ROLL);
    if(WORD_AT(a+KHL_AI_CLASS_HI) || WORD_AT(a+KHL_AI_CLASS)!=KHL_RIDER_CLASS) {
        if(khl_ch[k].fixes++<4u) fprintf(stderr,"[mapmaker] khallos: chair %u: rider's AI class %X/%X put back to 0/8\n",k,
                                        (unsigned)WORD_AT(a+KHL_AI_CLASS_HI),(unsigned)WORD_AT(a+KHL_AI_CLASS));
        WORD_AT(a+KHL_AI_CLASS_HI)=0;WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;WORD_AT(a+KHL_AI_NEXT)=0;
    }
}
/* Chair k's status line (its globals in, world frame): yaw, the player it
 * turns to, the rider's seat offset, his handler calls and the sounds. */
static void khl_status(unsigned k,uint32_t obj,uint32_t p)
{
    uint32_t a=khl_ch[k].rider;
    if(khl_ch[k].status>=KHL_STATUS || (++khl_ch[k].frames!=1u && khl_ch[k].frames%KHL_STATUS_EVERY)) return;
    khl_ch[k].status++;
    fprintf(stderr,"[mapmaker] khallos: chair %u frame %u: yaw %.0f, player at dx %.1f dz %.1f; seat offset (%.2f, %.2f, %.2f), "
            "rider calls %u state %d class %X/%X; sounds: gun %u (%u failed), engine %u (%u not playing)\n",k,khl_ch[k].frames,
            (double)khl_getf(obj+KHL_ENT_YAW),p?(double)(khl_getf(p+KHL_P_POS)-khl_getf(KHL_POS)):0.0,
            p?(double)(khl_getf(p+KHL_P_POS+8u)-khl_getf(KHL_POS+8u)):0.0,
            (double)khl_ch[k].off[0],(double)khl_ch[k].off[1],(double)khl_ch[k].off[2],khl_ch[k].calls,
            khl_seated(k)?(int)WORD_AT(a+KHL_AI_STATE):-1,khl_seated(k)?(unsigned)WORD_AT(a+KHL_AI_CLASS_HI):0u,
            khl_seated(k)?(unsigned)WORD_AT(a+KHL_AI_CLASS):0u,khl_ch[k].shots,khl_ch[k].shot_fail,khl_ch[k].engine,khl_ch[k].engine_off);
    fflush(stderr);
}

/* Chair k's frame, its globals in. */
static void khl_frame(void (*original)(void),unsigned k,uint32_t obj)
{
    uint32_t p,line,gap,span,a,ent;
    float px,tx;
    if(khl_ch[k].place && !khl_ch[k].own) {                /* first frame: the placeholder leaves */
        a=khl_ch[k].place;ent=khl_ch[k].place_ent;khl_ch[k].place=0;
        if(khl_same_ai(a,ent,khl_ch[k].place_rec)) {
            khl_unfocus(ent);
            guest_call(sub_0015DDA0,1,ent);
            khl_log("chair placeholder AI removed (placeholder, chair)",a,k);
        } else khl_log("chair placeholder AI already gone",a,ent);
    }
    line=WORD_AT(KHL_LINE);gap=WORD_AT(KHL_GAP);
    if(!WORD_AT(KHL_ACTIVE) && !khl_near(obj)) {           /* waits: the start line never passed */
        WORD_AT(KHL_LINE)=KHL_FLT_LOW;
        original();
        WORD_AT(KHL_LINE)=line;
        khl_rider_health(k,obj);
        return;
    }
    if(!WORD_AT(KHL_ACTIVE)) khl_log("Khallos' fight started: a player came near (chair, chair)",obj,k);
    if(!(p=khl_steered())) { WORD_AT(KHL_LINE)=KHL_FLT_MAX;original();WORD_AT(KHL_LINE)=line;return; }
    px=khl_getf(p+KHL_P_POS);
    tx=khl_clamp(px-khl_getf(KHL_GAP),khl_ch[k].spawn[0]-KHL_ROAM,khl_ch[k].spawn[0]+KHL_ROAM);
    khl_lv.d[0]=0.0f;
    khl_lv.d[1]=khl_ch[k].spawn[1]+KHL_LIFT-KHL_TRAIN_Y;
    khl_lv.d[2]=khl_clamp(khl_getf(p+KHL_P_POS+8u),khl_ch[k].spawn[2]-KHL_ROAM,khl_ch[k].spawn[2]+KHL_ROAM);
    WORD_AT(KHL_LINE)=KHL_FLT_MAX;
    khl_setf(KHL_GAP,px-tx);
    span=WORD_AT(KHL_SPAN);                               /* the train's yaw 90 +- 30 (facing +x) lifted: */
    khl_setf(KHL_SPAN,KHL_SPAN_FREE);                     /* the chair turns to the player wherever he is */
    khl_shift_in(obj);
    khl_lv.inside=1;
    original();
    khl_lv.inside=0;
    khl_shift_out(obj);
    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;
    khl_rider_health(k,obj);
    khl_seat(k,obj);
    khl_status(k,obj,p);
    if(!(khl_getf(obj+KHL_ENT_HEALTH)<0.0f)) return;
    if(++khl_ch[k].dying>=KHL_GONE_FRAMES || khl_getf(KHL_POS+4u)>khl_ch[k].spawn[1]+KHL_GONE_UP) khl_remove(k,obj);
}

/* WRAP sub_001EEBD0 (cdecl chair; its per-frame callback). */
void pc_mapmaker_khallos_update(void (*original)(void))
{
    uint32_t obj,keep[KHL_BLOCK_N+KHL_SMOKE_N];
    int k,prev;
    if(!MAPMAKER_STACK_OK(8) || (k=khl_chair(obj=WORD_AT(g_esp+4u)))<0) { original();return; }
    if(khl_ch[k].removed) { MAPMAKER_GUEST_RET();return; }   /* removed: nothing more to run */
    khl_in((unsigned)k,keep);
    prev=khl_lv.cur;khl_lv.cur=k;
    khl_frame(original,(unsigned)k,obj);
    khl_lv.cur=prev;
    khl_out((unsigned)k,keep);
}

/* Chair k's hit (its globals in): no train death in a MapMaker map. */
static void khl_hit(void (*original)(void),unsigned k,uint32_t r)
{
    uint32_t obj=WORD_AT(KHL_OBJ),bits;
    float h,dmg;
    if(WORD_AT(KHL_ACTIVE)!=1u || obj!=khl_ch[k].obj || !MAPMAKER_PTR_OK(r,KHL_REC_DIR+4u) || !WORD_AT(r+KHL_REC_DIR) || !WORD_AT(r+KHL_REC_HIT) ||
       !((h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) || !(h-(dmg=khl_getf(r+KHL_REC_DAMAGE))<0.0f)) { original();return; }
    bits=WORD_AT(r+KHL_REC_DAMAGE);
    khl_setf(r+KHL_REC_DAMAGE,h);                          /* the stock code leaves him at exactly 0 */
    original();
    WORD_AT(r+KHL_REC_DAMAGE)=bits;
    khl_setf(obj+KHL_ENT_HEALTH,h-dmg);
    khl_down(k,obj);
}
/* WRAP sub_001EF840 (cdecl hit record, +0 the object hit: sub_000BB490). */
void pc_mapmaker_khallos_damage(void (*original)(void))
{
    uint32_t r,active,keep[KHL_BLOCK_N+KHL_SMOKE_N];
    int k,prev;
    float h;
    if(!MAPMAKER_STACK_OK(8) || !MAPMAKER_PTR_OK(r=WORD_AT(g_esp+4u),4u) || (k=khl_chair(WORD_AT(r)))<0 || khl_ch[k].removed) { original();return; }
    h=khl_getf(khl_ch[k].obj+KHL_ENT_HEALTH);
    khl_in((unsigned)k,keep);
    prev=khl_lv.cur;khl_lv.cur=k;
    active=WORD_AT(KHL_ACTIVE);
    khl_hit(original,(unsigned)k,r);
    khl_lv.cur=prev;
    khl_out((unsigned)k,keep);
    if(khl_ch[k].hits<KHL_HITS) {                          /* v0.4.0-mapmaker.27: "3 khallos' didn't seem to take damage" */
        khl_ch[k].hits++;
        fprintf(stderr,"[mapmaker] khallos: chair %u hit (active %u, damage %.1f): health %.1f -> %.1f\n",(unsigned)k,(unsigned)active,
                MAPMAKER_PTR_OK(r,KHL_REC_DAMAGE+4u)?(double)khl_getf(r+KHL_REC_DAMAGE):0.0,(double)h,(double)khl_getf(khl_ch[k].obj+KHL_ENT_HEALTH));
        fflush(stderr);
    }
}

/* WRAP sub_00263260 (cdecl entity; the seated Khallos, class 8): with the
 * globals of the chair he rides. */
void pc_mapmaker_khallos_rider(void (*original)(void))
{
    uint32_t a,next;
    int k;
    if(!khl_lv.n || !MAPMAKER_STACK_OK(8) || (k=khl_ridden(WORD_AT(g_esp+4u)))<0) { original();return; }
    khl_ch[k].calls++;
    a=khl_ch[k].rider;
    next=khl_seated((unsigned)k)?WORD_AT(a+KHL_AI_NEXT):0xFFFFFFFFu;
    if(khl_seated((unsigned)k) && ((int32_t)next>1 || ((int32_t)next<0 && WORD_AT(a+KHL_AI_STATE)>1u))) {
        if(khl_ch[k].fixes++<4u) fprintf(stderr,"[mapmaker] khallos: chair %u: rider state %d (next %d) is none of the seat's: sits down again\n",
                                        (unsigned)k,(int)WORD_AT(a+KHL_AI_STATE),(int)next);
        WORD_AT(a+KHL_AI_NEXT)=0;                          /* states 2+ do nothing in the handler: he would hang in the air */
    }
    khl_swapped(original,(unsigned)k);
}
/* WRAP sub_00263150 (cdecl entity; the slot-2 callback the seated Khallos'
 * handler installs, reads the chair sway [0x0078F068/6C]): same. */
void pc_mapmaker_khallos_rider_pose(void (*original)(void))
{
    int k;
    if(!khl_lv.n || !MAPMAKER_STACK_OK(8) || (k=khl_ridden(WORD_AT(g_esp+4u)))<0) { original();return; }
    khl_swapped(original,(unsigned)k);
}

/* WRAP sub_0014CC80 (story boss bar; chain INSIDE pc_mapmaker_bosses_hud):
 * a seated chair never gets its stock bar ("Khallos", the focus): the bosses
 * area draws its rider's Boss bar (his name, the chair's health), or none. */
void pc_mapmaker_khallos_hud(void (*original)(void))
{
    uint32_t player,focus;
    int k;
    if(!khl_lv.n) { original();return; }                   /* cheap: no chair this level */
    player=WORD_AT(KHL_PLAYER);
    if(!MAPMAKER_PTR_OK(player,KHL_FOCUS+4u) || (k=khl_chair(focus=WORD_AT(player+KHL_FOCUS)))<0 || !khl_seated((unsigned)k)) { original();return; }
    if(!khl_lv.bars++) khl_log("stock chair bar hidden: the bosses area shows its rider's (chair, rider)",focus,khl_ch[k].rider);
    WORD_AT(player+KHL_FOCUS)=0;
    original();
    WORD_AT(player+KHL_FOCUS)=focus;
}

/* WRAP sub_002274C0 (cdecl event, arg): the chair's four events. */
void pc_mapmaker_khallos_event(void (*original)(void))
{
    uint32_t ret;
    if(!MAPMAKER_STACK_OK(12)) { original();return; }
    ret=WORD_AT(g_esp);
    if((ret!=KHL_RET_START && ret!=KHL_RET_HURT && ret!=KHL_RET_LOW && ret!=KHL_RET_DEAD) ||
       WORD_AT(KHL_SCENE)!=100u || !khl_on()) { original();return; }
    if(khl_lv.events++<6u) khl_log("chair level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),ret);
    MAPMAKER_GUEST_RET();
}

/* WRAP sub_0003D7B0 (cdecl text id -> string), hot. */
void pc_mapmaker_khallos_text(void (*original)(void))
{
    if(!MAPMAKER_STACK_OK(8) || WORD_AT(g_esp+4u)!=KHL_BAR_TEXT || WORD_AT(g_esp)!=KHL_RET_BAR_TEXT || !khl.label || !khl_play()) {
        original();return;
    }
    g_eax=khl.label;
    MAPMAKER_GUEST_RET();
}

/* The calls out of the moved update that place things in the world. */
void pc_mapmaker_khallos_flame(void (*original)(void))
{
    if(!khl_lv.inside || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_FLAME) { original();return; }
    khl_world_call(original,WORD_AT(g_esp+8u));
}
void pc_mapmaker_khallos_flash(void (*original)(void))
{
    if(!khl_lv.inside || !MAPMAKER_STACK_OK(16) || (WORD_AT(g_esp)!=KHL_RET_FLASH && WORD_AT(g_esp)!=KHL_RET_FLASH2)) { original();return; }
    khl_world_call(original,WORD_AT(g_esp+12u));
}
void pc_mapmaker_khallos_shot(void (*original)(void))
{
    if(!khl_lv.inside || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_SHOT) { original();return; }
    khl_world_call(original,WORD_AT(g_esp+8u));
}
void pc_mapmaker_khallos_sound(void (*original)(void))
{
    uint32_t rec;
    if(!khl_lv.inside || !MAPMAKER_STACK_OK(8) || WORD_AT(g_esp)!=KHL_RET_SOUND) { original();return; }
    rec=WORD_AT(g_esp+4u);
    khl_world_call(original,MAPMAKER_PTR_OK(rec,KHL_SOUND_POS+12u)?rec+KHL_SOUND_POS:0u);
    khl_ch[khl_lv.cur].shots++;                            /* -1: the sound is not loaded (sub_00027F80) */
    if((int32_t)g_eax<0) khl_ch[khl_lv.cur].shot_fail++;
}
void pc_mapmaker_khallos_engine(void (*original)(void))
{
    uint32_t handle;
    if(!khl_lv.inside || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_ENGINE) { original();return; }
    handle=WORD_AT(g_esp+4u);                              /* chair +0x22C: this chair's own loop */
    khl_world_call(original,WORD_AT(g_esp+8u));
    khl_ch[khl_lv.cur].engine++;
    if(!MAPMAKER_PTR_OK(handle,4u) || (int32_t)WORD_AT(handle)<0) khl_ch[khl_lv.cur].engine_off++;
}

/* WRAP sub_00045670 (cdecl sound id -> nonzero when this level loads it; called
 * only by sub_00027700, the level's sound load, after the pak mounts). The
 * chair's gun and engine loop are level-11 sounds (sounddata flag 4, level
 * byte 11), never loaded in scene 100: v0.4.0-mapmaker.28 "didn't really have
 * much audio". Loaded when the map's set holds the chair. */
void pc_mapmaker_khallos_sound_load(void (*original)(void))
{
    uint32_t id=MAPMAKER_STACK_OK(8)?WORD_AT(g_esp+4u):0xFFFFFFFFu;
    original();
    if(g_eax || !khl_lv.sounds || (id!=KHL_SND_GUN && id!=KHL_SND_JET) || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return;
    g_eax=KHL_SND_LOAD;
    if(khl_lv.sounds_logged++<2u) khl_log("the chair's level-11 sound is loaded in this map (sound, load)",id,KHL_SND_LOAD);
}
