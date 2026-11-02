"""MapMaker real story bosses: the giant trophy monster (deerbeast) as a Story AI.

Compiles src/interface/pc_mapmaker_realbosses.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE and its
disassembly are available, checks the stock facts the hooks rely on: the
picker's list end, row callback, highlight and row lookups; the synthetic row
id being past every stock bound; the character data of id 0x15; the anim pak
string, mount routine, mount cap and registration order; the AI class copy
and the 0x80 dispatch; the boss bar label site; and that the monster handler
cannot fire a level event (static call graph).
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_realbosses.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-list': ('WORD_AT(g_esp)==RBS_PICKER_END && rbs_editor()', '1 && rbs_editor()'),
    'row-in-play': ('static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)==102u; }',
                    'static int rbs_editor(void) { return rbs_on() && WORD_AT(RBS_SCENE)>=100u; }'),
    'row-disabled': ('(sub_00071630,0),0u);   /* enabled, as the stock rows */', '(sub_00071630,0),1u);   /* enabled, as the stock rows */'),
    'wrong-flags': ('#define RBS_PICK_FLAGS 0x20900u', '#define RBS_PICK_FLAGS 0x22900u'),
    'row-after-original': ('        guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);\n',
                           '        original();guest_call(sub_00079D60,6,text,RBS_ROW,RBS_PICK_FLAGS,0u,RBS_PICK_CB,0u);g_esp-=4u;\n'),
    'pick-keeps-variant': ('        WORD_AT(s+0x18u)=0xFFFFFFFFu;\n', ''),
    'pick-wrong-id': ('if(WORD_AT(s+0x14u)==RBS_ROW) id=RBS_ID;', 'if(WORD_AT(s+0x14u)==RBS_ROW) id=0xA7u;'),
    'preview-150': ('    if(rbs_editor()) WORD_AT(RBS_PREVIEW)=rbs_preview_row(WORD_AT(RBS_PREVIEW));', '    if(0) WORD_AT(RBS_PREVIEW)=0;'),
    'name-any-caller': ('(ret==RBS_RET_NAME && WORD_AT(g_esp+12u)==RBS_RET_NAME_OUTER)', '(ret==RBS_RET_NAME)'),
    'preview-gets-150': ('    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) row=rbs_preview_row(row);\n',
                         '    else if(ret==RBS_RET_PREVIEW1 || ret==RBS_RET_PREVIEW2) {}\n'),
    'row-of-any-id': ('((id=WORD_AT(g_esp+4u))!=RBS_ID && id!=RBS_SKEL_ID && id!=RBS_WALK_ID &&\n       rbs_extra_by_id(id)>=RBS_EXTRA_N)', '((id=WORD_AT(g_esp+4u)),0)'),
    'extra-rows-missing': ('        for(k=0;k<RBS_EXTRA_N;k++) if((text=rbs_extra_label(k))!=0) {', '        for(k=0;0;k++) if((text=rbs_extra_label(k))!=0) {'),
    'extra-pick': ('        else if((e=rbs_extra_by_row(WORD_AT(s+0x14u)))<RBS_EXTRA_N) id=rbs_extra[e].id;\n', ''),
    'extra-text': ('            g_eax=rbs.extra_label[k]?rbs.extra_label[k]:RBS_EMPTY_TEXT;', '            g_eax=RBS_EMPTY_TEXT;'),
    'extra-preview': ('    if(rbs_extra_by_row(row)<RBS_EXTRA_N) return rbs_arcade_row(rbs_extra[rbs_extra_by_row(row)].standin);\n', ''),
    'extra-row-text': ('        g_eax=rbs_extra[e].text;', '        g_eax=0x52420000u;'),
    'extra-wrong-id': ('{155u,0x12u,0x52420012u,0xA8u,"Castle Tank"}', '{155u,0x13u,0x52420012u,0xA8u,"Castle Tank"}'),
    'extra-crowe-standin': ('{154u,0x46u,0x52420046u,0x1Eu,"Split Crowe"}', '{154u,0x46u,0x52420046u,0xA8u,"Split Crowe"}'),
    'row-of-no-ret': ('    g_eax=row;\n    MAPMAKER_GUEST_RET();\n', '    g_eax=row;\n'),
    'row-text-any': ('((row=WORD_AT(g_esp+4u))!=RBS_ROW && row!=RBS_SKEL_ROW && row!=RBS_WALK_ROW)', '((row=WORD_AT(g_esp+4u))>RBS_WALK_ROW)'),
    'text-no-ret': ('        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n        MAPMAKER_GUEST_RET();\n',
                    '        g_eax=rbs.label?rbs.label:RBS_EMPTY_TEXT;\n'),
    'bar-label-anywhere': ('id==RBS_BAR_TEXT && WORD_AT(g_esp)==RBS_RET_BAR_TEXT && rbs.label', 'id==RBS_BAR_TEXT && rbs.label'),
    'bar-label-any-scene': ('rbs.label && rbs_play()) {\n        g_eax=rbs.label;', 'rbs.label && rbs_on()) {\n        g_eax=rbs.label;'),
    'mount-before-original': ('    int has;\n    uint32_t mounted;\n    original();\n    if(!rbs_play() || rbs.optout) return;',
                              '    int has;\n    uint32_t mounted;\n    if(!rbs_play() || rbs.optout) { original();return; }\n    guest_call(sub_0005A130,1,RBS_PAK);original();'),
    'mount-any-map': ('    if((has=rbs_map_has(RBS_ID))==0) return;\n', '    has=1;\n'),
    'mount-preset': ('    if(RBS_H(info+RBS_INFO_SET)!=RBS_SET_MAP) return 0;   /* a preset or profile set */\n', ''),
    'set-any-custom-index': ('!=RBS_SET_MAP) return 0;   /* a preset', '<0xCu) return 0;   /* a preset'),
    'set-skips-slots-11-20': ('    for(k=0;extra && k<RBS_B(extra+3u);k++) {', '    for(k=0;0 && extra && k<RBS_B(extra+3u);k++) {'),
    'set-skips-slot-1': ('    for(k=0;k<n;k++) {\n        id=RBS_H(info', '    for(k=1;k<n;k++) {\n        id=RBS_H(info'),
    'set-first-boss-only': ('        if(id==RBS_SKEL_ID && !*s) *s=k+1u;\n    }\n', '        if(id==RBS_SKEL_ID && !*s) *s=k+1u;\n        if(*t || *s) break;\n    }\n'),
    'set-extra-offset': ('        id=RBS_H(extra+4u+k*4u);', '        id=RBS_H(extra+2u+k*4u);'),
    'set-extra-any-header': ('!extra && RBS_H(d)==1u && RBS_B(d+2u)==RBS_INFO_SLOTS &&', '!extra &&'),
    'set-extra-only-trophy': ('        if(id==RBS_SKEL_ID && !*s) *s=RBS_INFO_SLOTS+k+1u;\n', ''),
    'set-info-only-trophy': ('        if(id==RBS_SKEL_ID && !*s) *s=k+1u;\n', ''),
    'mount-twice': ('    if(rbs_pak_mounted()) return;\n    if((mounted', '    if((mounted'),
    'mount-fills-table': ('#define RBS_MOUNT_KEEP 8u', '#define RBS_MOUNT_KEEP 0u'),
    'mount-ignores-optout': ('    if(!rbs_play() || rbs.optout) return;\n    rbs_set_report("anim pak");', '    if(!rbs_play()) return;\n    rbs_set_report("anim pak");'),
    'wrong-pak': ('#define RBS_PAK 0x0049D77Cu', '#define RBS_PAK 0x0047E624u'),
    'class-after-original': ('        WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;\n',
                             '        original();WORD_AT(rec)=RBS_CLASS_LO;WORD_AT(rec+4u)=RBS_CLASS_HI;g_esp-=4u;\n'),
    'class-swapped': ('#define RBS_CLASS_LO 0u ', '#define RBS_CLASS_LO 0x80u '),
    'class-without-anims': ('    else if(!rbs_pak_mounted()) rbs_log', '    else if(0) rbs_log'),
    'class-any-character': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBS_H(ent+RBS_ENT_MODEL)==RBS_MODEL)) {',
                            '    if(0) {'),
    'class-any-mode': ('    return rbs_on() && WORD_AT(RBS_SCENE)==100u && RBS_B(RBS_MODE)==10u;', '    return rbs_on() && WORD_AT(RBS_SCENE)==100u;'),
    'class-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant trophy'),
    'boss-flag-ignored': ('            if((f&0xF8u)!=0xB0u) return 0;', '            return 0;'),
    'boss-flag-loose': ('            if((f&0xF8u)!=0xB0u) return 0;', '            if((f&0xF0u)!=0xB0u) return 0;'),
    'sub-is-bar': ('return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);', 'return (uint8_t)RBS_BOSS_BAR;'),
    'sub-wrong-bit': ('return (uint8_t)((f&4u)?RBS_BOSS_SUB:RBS_BOSS_BAR);', 'return (uint8_t)((f&1u)?RBS_BOSS_SUB:RBS_BOSS_BAR);'),
    'boss-wrong-byte': ('#define RBS_SAI_BOSS 0x29u', '#define RBS_SAI_BOSS 0x21u'),
    'boss-without-area': ('pc_mapmaker_area_enabled("bosses","pc_mapmaker_bosses_hud")?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);\n        rbs_log("giant trophy',
                          '1?rbs_boss_flag(id-WORD_AT(RBS_SAI_BASE)-1u):0u);\n        rbs_log("giant trophy'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n', '    original();\n'),
    'hud-boss-off-stock': ('    WORD_AT(player+RBS_FOCUS)=0;\n    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n}',
                           '    if(!rbs.real[k].boss) { original();return; }\n    WORD_AT(player+RBS_FOCUS)=0;\n    original();\n    WORD_AT(player+RBS_FOCUS)=focus;\n}'),
    'hud-any-actor': ('    if(k==RBS_REAL_MAX) { original();return; }\n    /* never', '    /* never'),
    'hud-any-model': ('       (RBS_H(focus+RBS_ENT_MODEL)!=RBS_MODEL && RBS_H(focus+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&\n        RBS_H(focus+RBS_ENT_MODEL)!=RBS_WALK_MODEL) || !rbs_play()', '       !rbs_play()'),
    'meter-any-caller': ('WORD_AT(g_esp)!=RBS_RET_METER) { original();return; }', '0) { original();return; }'),
    'meter-no-ret': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n', ''),
    'meter-kept': ('    MAPMAKER_GUEST_RET();                            /* cdecl: the caller pops the four args */\n',
                   '    original();\n'),
    'meter-any-character': ('    if(RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_WALK_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_WALK_MODEL) { original();return; }\n', ''),
    'meter-model-only': ('RBS_H(a+RBS_ACTOR_CHAR)!=RBS_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_WALK_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_WALK_MODEL)',
                         'RBS_H(ent+RBS_ENT_MODEL)!=RBS_MODEL &&\n       RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL && RBS_H(ent+RBS_ENT_MODEL)!=RBS_WALK_MODEL)'),
    'meter-any-actor': ('    if(k==RBS_REAL_MAX || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', '    if(rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())'),
    'meter-any-mode': ('rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play()) { original();return; }', 'rbs.real[k].boss==RBS_BOSS_SUB || !rbs_on()) { original();return; }'),
    'meter-sub-hidden': (' || rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', ' || !rbs_play())'),
    'meter-bar-shown': ('rbs.real[k].boss==RBS_BOSS_SUB || !rbs_play())', 'rbs.real[k].boss || !rbs_play())'),
    'meter-wrong-return': ('#define RBS_RET_METER 0x0014D264u', '#define RBS_RET_METER 0x0014D25Fu'),
    # the giant skeleton (id 0x28)
    'skel-row-missing': ('        if((text=rbs_skel_label())!=0) {', '        if(0) {'),
    'skel-pick-id': ('else if(WORD_AT(s+0x14u)==RBS_SKEL_ROW) id=RBS_SKEL_ID;', 'else if(WORD_AT(s+0x14u)==RBS_SKEL_ROW) id=RBS_ID;'),
    'skel-preview': ('if(row==RBS_SKEL_ROW) return rbs_arcade_row(RBS_SKEL_STANDIN);', 'if(row==RBS_SKEL_ROW) return RBS_SKEL_ROW;'),
    'skel-wrong-pak': ('#define RBS_SKEL_PAK 0x0049D6FCu', '#define RBS_SKEL_PAK 0x0049D77Cu'),
    'skel-stale-pointer': ('    WORD_AT(RBS_SKEL_STATE)=0;WORD_AT(RBS_SKEL_PARTS)=0;', '   '),
    'skel-level-any-mode': ('    if(!rbs_play() || rbs.optout) return;\n    rbs_set_report("level init");',
                            '    if(!rbs_on() || rbs.optout) return;\n    rbs_set_report("level init");'),
    'skel-class': ('#define RBS_SKEL_CLASS 0x20000u', '#define RBS_SKEL_CLASS 0x80u'),
    'skel-without-pak': ('else if(!rbs_skel_pak_mounted()) rbs_log("lv1_6', 'else if(0) rbs_log("lv1_6'),
    'skel-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant skeleton', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: giant skeleton'),
    'skel-floor-wrong-ret': ('#define RBS_RET_FACE 0x0029D075u', '#define RBS_RET_FACE 0x0029E528u'),
    'skel-fire-always': ('WORD_AT(st+RBS_SKEL_FIRE)!=1u ||', '0 ||'),
    'skel-fire-any-scene': ('WORD_AT(st+RBS_SKEL_FIRE)!=1u ||\n       WORD_AT(RBS_SCENE)!=100u ||', 'WORD_AT(st+RBS_SKEL_FIRE)!=1u ||\n      '),
    'skel-fire-size': ('guest_call(sub_002A7F30,2,WORD_AT(view+0x208u),0x40200000u);', 'guest_call(sub_002A7F30,2,WORD_AT(view+0x208u),0x3FE00000u);'),
    'skel-throw-unguarded': ('        if(!rbs_sane(rbs_getf(shot)) || ', '        if(0 && !rbs_sane(rbs_getf(shot)) || '),
    'skel-message-original': ('    sub_00286D30();\n}', '    original();\n}'),
    'skel-message-only-death': ('    sub_00286D30();\n}', '    if(WORD_AT(g_esp+8u)!=RBS_MSG_DEATH) { original();return; }\n    sub_00286D30();\n}'),
    'skel-risen-anim': ('#define RBS_SKEL_START 0x19Cu', '#define RBS_SKEL_START 0x23Bu'),
    'skel-risen-no-ret': ('    g_eax=guest_call(sub_00059DB0,2,ent,RBS_SKEL_START);\n    MAPMAKER_GUEST_RET();', '    g_eax=guest_call(sub_00059DB0,2,ent,RBS_SKEL_START);'),
    'skel-throw-any-action': ('    if(act>=8u || WORD_AT(RBS_SKEL_ACTIONS+act*4u)!=RBS_SKEL_THROW) { original();return; }', '    if(act>=8u) { original();return; }'),
    'skel-throw-past-table': ('    if(act>=8u || WORD_AT(RBS_SKEL_ACTIONS', '    if(WORD_AT(RBS_SKEL_ACTIONS'),
    'skel-aim-no-spoof': ('        WORD_AT(RBS_SCENE)=1u;\n', ''),
    'skel-aim-no-restore': ('        WORD_AT(RBS_SCENE)=100u;\n', ''),
    'skel-fire-spoofed': ('    if(phase==0) {', '    if(phase<=1) {'),
    'skel-fire-args': ('0x34u,shot,shot+0xCu', '0x34u,shot+0xCu,shot'),
    'skel-fire-keeps-phase': ('        WORD_AT(st+RBS_SKEL_PHASE)=2u;\n', ''),
    'skel-fire-effect': ('guest_call(sub_0010EB90,2,WORD_AT(shot+0x18u),0u);', 'guest_call(sub_0010EB90,2,WORD_AT(shot+0x14u),0u);'),
    'skel-done-original': ('    MAPMAKER_GUEST_RET();                              /* done: scene 1 returns (0x00287FD7) */', '    original();'),
    'skel-event-any': ('WORD_AT(g_esp)!=RBS_RET_EVENT || WORD_AT(RBS_SCENE)', 'WORD_AT(RBS_SCENE)'),
    'skel-event-no-ret': ('WORD_AT(g_esp+8u));\n    MAPMAKER_GUEST_RET();\n}', 'WORD_AT(g_esp+8u));\n}'),
    'skel-meter-shown': (' &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_SKEL_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_SKEL_MODEL &&', ' &&'),
    # the Crow walker (id 0x64)
    'walk-row-missing': ('        if((text=rbs_walk_label())!=0) {', '        if(0) {'),
    'walk-pick-id': ('else if(WORD_AT(s+0x14u)==RBS_WALK_ROW) id=RBS_WALK_ID;', 'else if(WORD_AT(s+0x14u)==RBS_WALK_ROW) id=RBS_SKEL_ID;'),
    'walk-preview': ('if(row==RBS_WALK_ROW) return rbs_arcade_row(RBS_WALK_STANDIN);', 'if(row==RBS_WALK_ROW) return RBS_WALK_ROW;'),
    'walk-text-missing': ('    if(id==RBS_WALK_TEXT) {', '    if(0) {'),
    'walk-wrong-pak': ('#define RBS_WALK_PAK 0x0049D9A8u', '#define RBS_WALK_PAK 0x0049D6FCu'),
    'walk-mount-any-map': ('    if((has=rbs_map_has(RBS_WALK_ID))!=0) {\n        rbs_walk_label();', '    if((has=1)!=0) {\n        rbs_walk_label();'),
    'walk-set-info': ('        if(id==RBS_WALK_ID && !*w) *w=k+1u;\n', ''),
    'walk-set-extra': ('        if(id==RBS_WALK_ID && !*w) *w=RBS_INFO_SLOTS+k+1u;\n', ''),
    'walk-class': ('#define RBS_WALK_CLASS 0x100000u', '#define RBS_WALK_CLASS 0x20000u'),
    'walk-without-pak': ('else if(!rbs_walk_pak_mounted()) rbs_log("lv81', 'else if(0) rbs_log("lv81'),
    'walk-ignores-optout': ('    if(rbs.optout) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: Crow walker', '    if(0) rbs_log("TSFP_MAPMAKER_REAL_BOSS=0: Crow walker'),
    'walk-well-any-caller': ('WORD_AT(g_esp)!=RBS_RET_WELL || WORD_AT(g_esp+4u)!=RBS_WALK_TAG)', 'WORD_AT(g_esp+4u)!=RBS_WALK_TAG)'),
    'walk-event-intro-kept': ('((ret=WORD_AT(g_esp))==RBS_RET_WALK_INTRO || ret==RBS_RET_WALK_INTRO2 ||', '((ret=WORD_AT(g_esp))==RBS_RET_WALK_INTRO ||'),
    'walk-event-any-scene': ('       ret==RBS_RET_WALK_DONE) && WORD_AT(RBS_SCENE)==100u && rbs_on()) {', '       ret==RBS_RET_WALK_DONE) && rbs_on()) {'),
    'walk-focus-kept': ('WORD_AT(player+RBS_FOCUS)==ent) WORD_AT(player+RBS_FOCUS)=0;', 'WORD_AT(player+RBS_FOCUS)==ent) {}'),
    'walk-fx-missing': ('    guest_call(sub_0029CCB0,0);\n', ''),
    'walk-fx-any-caller': ('MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBS_RET_FX;', 'MAPMAKER_STACK_OK(4);'),
    'walk-meter-shown': (' &&\n       RBS_H(a+RBS_ACTOR_CHAR)!=RBS_WALK_ID && RBS_H(ent+RBS_ENT_MODEL)!=RBS_WALK_MODEL)', ')'),
    'walk-hud-own-bar': (' &&\n        RBS_H(focus+RBS_ENT_MODEL)!=RBS_WALK_MODEL)', ')'),
    'walk-never-woken': ('    WORD_AT(a+RBS_WALK_AI_NEXT)=RBS_WALK_ROAR;\n', ''),
    'walk-wake-skips-roar': ('#define RBS_WALK_ROAR 2u', '#define RBS_WALK_ROAR 4u'),
    'walk-wake-any-state': ('    if(s==RBS_WALK_WAIT && (int32_t)WORD_AT(a+RBS_WALK_AI_NEXT)<0) {', '    if((int32_t)WORD_AT(a+RBS_WALK_AI_NEXT)<0) {'),
    'walk-wake-before-setup': ('    if(RBS_B(st+RBS_WALK_STAGE)!=1u) return;\n', ''),
    'walk-wake-over-request': ('    if(s==RBS_WALK_WAIT && (int32_t)WORD_AT(a+RBS_WALK_AI_NEXT)<0) {', '    if(s==RBS_WALK_WAIT) {'),
    'walk-wake-other-entity': ('!MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBS_ENT_AI)!=a) return;\n    rbs_walk_track',
                               '!MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u)) return;\n    rbs_walk_track'),
    'walk-roar-never': ('    r=guest_call(sub_0005F460,4,ent,RBS_WALK_STUN,0u,0x3F800000u);', '    r=0;'),
    'walk-roar-wrong-anim': ('#define RBS_WALK_STUN 0x6F7u', '#define RBS_WALK_STUN 0x6F6u'),
    'walk-roar-slow': ('guest_call(sub_0005F460,4,ent,RBS_WALK_STUN,0u,0x3F800000u)', 'guest_call(sub_0005F460,4,ent,RBS_WALK_STUN,0u,0x3F000000u)'),
    'walk-roar-over-anim': (' || rbs_walk_own(WORD_AT(ctl+RBS_ANIM_CUR)) ||\n       rbs_walk_own(WORD_AT(ctl+RBS_ANIM_ALT))) return;', ' ||\n       rbs_walk_own(WORD_AT(ctl+RBS_ANIM_ALT))) return;'),
    'walk-roar-over-blend': (' || rbs_walk_own(WORD_AT(ctl+RBS_ANIM_CUR)) ||\n       rbs_walk_own(WORD_AT(ctl+RBS_ANIM_ALT))) return;', ' || rbs_walk_own(WORD_AT(ctl+RBS_ANIM_CUR))) return;'),
    'walk-roar-empty-only': (' || rbs_walk_own(WORD_AT(ctl+RBS_ANIM_CUR)) ||', ' || WORD_AT(ctl+RBS_ANIM_CUR) ||'),
    'walk-own-range-low': ('return anim>=RBS_WALK_ANIM_FIRST && anim<=RBS_WALK_ANIM_LAST; }', 'return anim>RBS_WALK_ANIM_FIRST && anim<=RBS_WALK_ANIM_LAST; }'),
    'walk-own-range-high': ('return anim>=RBS_WALK_ANIM_FIRST && anim<=RBS_WALK_ANIM_LAST; }', 'return anim>=RBS_WALK_ANIM_FIRST && anim<RBS_WALK_ANIM_LAST; }'),
    'walk-own-any': ('return anim>=RBS_WALK_ANIM_FIRST && anim<=RBS_WALK_ANIM_LAST; }', 'return anim!=0; }'),
    'walk-roar-any-state': ('    } else if(s!=RBS_WALK_ROAR) return;\n', '    }\n'),
    'walk-roar-only-state1': ('    } else if(s!=RBS_WALK_ROAR) return;\n', '    } else return;\n'),
    'walk-arm-never': ('    WORD_AT(a+RBS_WALK_AI_NEXT)=RBS_WALK_ARM;\n', ''),
    'walk-arm-wrong-state': ('#define RBS_WALK_ARM 0x12u', '#define RBS_WALK_ARM 0x4u'),
    'walk-arm-over-request': (' || (int32_t)WORD_AT(a+RBS_WALK_AI_NEXT)>=0 ||\n', ' ||\n'),
    'walk-arm-one-gun': ('       WORD_AT(st+RBS_WALK_GUN1) || WORD_AT(st+RBS_WALK_GUN2)) return;', '       (WORD_AT(st+RBS_WALK_GUN1) && WORD_AT(st+RBS_WALK_GUN2))) return;'),
    'walk-wake-sets-state': ('    WORD_AT(a+RBS_WALK_AI_NEXT)=RBS_WALK_ROAR;\n', '    WORD_AT(a+RBS_WALK_AI_STATE)=RBS_WALK_ROAR;\n'),
    # the giant skeleton (rewritten for several per map)
    'skel-mount-any-map': ('    if((has=rbs_map_has(RBS_SKEL_ID))!=0) {\n        rbs_skel_label();', '    if((has=1)!=0) {\n        rbs_skel_label();'),
    'skel-level-any-map': ('    if((has=rbs_map_has(RBS_SKEL_ID))!=0) {\n        n=rbs_count', '    if((has=1)!=0) {\n        n=rbs_count'),
    'skel-level-keeps': ('    memset(rbs_skels,0,sizeof rbs_skels);memset(&rbs_skelf,0,sizeof rbs_skelf);', '    memset(&rbs_skelf,0,sizeof rbs_skelf);'),
    'skel-level-trophy-gate': ('    if((has=rbs_map_has(RBS_SKEL_ID))!=0) {\n        n=rbs_count', '    if((has=rbs_map_has(RBS_ID))!=0) {\n        n=rbs_count'),
    'skel-two-bosses': ('    for(k=0;k<rbs_skelf.n;k++) if(!rbs_skels[k].record && rbs_skels[k].state) return &rbs_skels[k];', '    if(rbs_skelf.n && !rbs_skels[0].record) return &rbs_skels[0];'),
    'skel-without-state': ('        else if(!rbs_skelf.n || !rbs_skel_ours())\n', '        else if(0)\n'),
    'skel-princess-any': ("    if(!rbs_skelf.n || !MAPMAKER_STACK_OK(8) || !(k=rbs_skel_of(WORD_AT(g_esp+4u)))) { original();return; }\n    rbs_skel_floor(",
                          "    if(!rbs_skelf.n || !MAPMAKER_STACK_OK(8) || !(k=&rbs_skels[0])) { original();return; }\n    rbs_skel_floor("),
    'skel-floor-missing': ('    rbs_setf(ent+RBS_ENT_Y,k->floor);\n', ''),
    'skel-floor-any-caller': ('|| WORD_AT(g_esp)!=RBS_RET_FACE || !(k=rbs_skel_of', '|| !(k=rbs_skel_of'),
    'skel-floor-any-boss': ('!(k=rbs_skel_of(ent=WORD_AT(g_esp+4u))) || !k->floor_set) { original();return; }', '!(k=&rbs_skels[0]) || !(ent=WORD_AT(g_esp+4u)) || !k->floor_set) { original();return; }'),
    'skel-floor-refreshed': ('    if(k->floor_set || !rbs_sane(y=rbs_getf(ent+RBS_ENT_Y))) return;', '    if(!rbs_sane(y=rbs_getf(ent+RBS_ENT_Y))) return;'),
    'skel-floor-not-read': ('    rbs_skel_floor(k,WORD_AT(g_esp+4u));\n', ''),
    'skel-fire-missing': ('    for(k=0;k<rbs_skelf.n;k++) rbs_skel_fire(&rbs_skels[k]);\n', ''),
    'skel-fire-stale': ('    if(!k->record || !st || !rbs_skel_ours() || WORD_AT(st+RBS_SKEL_FIRE)', '    if(!k->record || !st || WORD_AT(st+RBS_SKEL_FIRE)'),
    'skel-princess-original': ('    rbs_skel_in(k,&s);\n    sub_0029CEF0();\n', '    rbs_skel_in(k,&s);\n    original();\n'),
    'skel-boss-any-scene': ('    for(k=0;k<rbs_skelf.n && (!rbs_skels[k].record || rbs_skels[k].record!=rec);k++) {}\n    if(k==rbs_skelf.n || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return 0;',
                            '    for(k=0;k<rbs_skelf.n && (!rbs_skels[k].record || rbs_skels[k].record!=rec);k++) {}\n    if(k==rbs_skelf.n || !rbs_on()) return 0;'),
    'skel-boss-stale-state': ('    if(!rbs_skelf.n || !rbs_skel_ours()) return 0;', '    if(!rbs_skelf.n) return 0;'),
    'skel-message-any': ('    if(!rbs_skelf.n || !MAPMAKER_STACK_OK(20) || !rbs_skel_of(ent=WORD_AT(g_esp+4u))) { original();return; }',
                         '    if(!rbs_skelf.n || !MAPMAKER_STACK_OK(20) || !(ent=WORD_AT(g_esp+4u))) { original();return; }'),
    'skel-event-any-scene': ('|| WORD_AT(RBS_SCENE)!=100u || !rbs_on()) { original();return; }\n    if(rbs_skelf.events', '|| !rbs_on()) { original();return; }\n    if(rbs_skelf.events'),
    # the Crow walker (rewritten for several per map)
    'walk-level-any-map': ('    if((has=rbs_map_has(RBS_WALK_ID))!=0) {\n        n=rbs_count', '    if((has=1)!=0) {\n        n=rbs_count'),
    'walk-level-keeps': ('    memset(rbs_walks,0,sizeof rbs_walks);memset(&rbs_walkf,0,sizeof rbs_walkf);', '    memset(&rbs_walkf,0,sizeof rbs_walkf);'),
    'walk-stale-pointer': ('            WORD_AT(RBS_WALK_STATE)=0;                 /* a stale pointer of an earlier level is not taken */\n', ''),
    'walk-without-state': ('        else if(!rbs_walkf.n || !rbs_walk_ours())\n', '        else if(0)\n'),
    'walk-two': ('    for(k=0;k<rbs_walkf.n;k++) if(!rbs_walks[k].record && rbs_walks[k].state) return &rbs_walks[k];', '    if(rbs_walkf.n && !rbs_walks[0].record) return &rbs_walks[0];'),
    'walk-not-noted': ('            w->record=rec;w->actor=a;\n            rbs_note(', '            w->record=rec;w->actor=a;\n            (void)('),
    'walk-well-replaces-found': ('    if(g_eax || !(w=rbs_walk_now()) || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;', '    if(!(w=rbs_walk_now()) || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;'),
    'walk-well-any-scene': ('    if(g_eax || !(w=rbs_walk_now()) || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;', '    if(g_eax || !(w=rbs_walk_now()) || !rbs_on()) return;'),
    'walk-well-null': ('    g_eax=*slot;\n', ''),
    'walk-well-dirty': ('    memset((void *)((uintptr_t)*slot+g_xbox_mem_offset),0,RBS_OBJ_SIZE);\n', ''),
    'walk-well-shared': ('    slot=&rbs.well[w-rbs_walks];', '    slot=&rbs.well[0];'),
    'walk-event-kept': ('rbs_walk_remove(w);\n        MAPMAKER_GUEST_RET();\n        return;', 'rbs_walk_remove(w);\n        original();\n        return;'),
    'walk-not-removed': ('        if(ret==RBS_RET_WALK_DONE && (w=rbs_walk_now())) rbs_walk_remove(w);\n', ''),
    'walk-remove-other': ('    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) || WORD_AT(a+RBS_ACTOR_RECORD)!=w->record ||', '    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR) ||'),
    'walk-remove-twice': ('    w->actor=0;\n    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR)', '    if(!MAPMAKER_PTR_OK(a,RBS_ACTOR)'),
    'walk-remove-first': ('    if(rbs_walkf.cur) return rbs_walkf.cur;\n', ''),
    'walk-fx-any-scene': ('    if(!w->state || !rbs_walk_ours() || WORD_AT(RBS_SCENE)!=100u || !rbs_on()) return;', '    if(!w->state || !rbs_walk_ours() || !rbs_on()) return;'),
    'walk-fx-stale': ('    if(!w->state || !rbs_walk_ours() || WORD_AT(RBS_SCENE)!=100u', '    if(!w->state || WORD_AT(RBS_SCENE)!=100u'),
    'walk-wake-not-called': ('    rbs_walk_wake(w);\n', ''),
    'walk-wake-other-record': ('       WORD_AT(a+RBS_ACTOR_RECORD)!=w->record || !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBS_ENT_AI)!=a) return;',
                               '       !MAPMAKER_PTR_OK(ent=WORD_AT(a),0x100u) || WORD_AT(ent+RBS_ENT_AI)!=a) return;'),
    'walk-roar-repeats': ('    w->roars++;\n    r=guest_call', '    r=guest_call'),
    'walk-arm-any-state': ('    if(w->armed || WORD_AT(a+RBS_WALK_AI_STATE)!=RBS_WALK_STANDOFF || ', '    if(w->armed || '),
    'walk-arm-stage2': ('    if(RBS_B(st+RBS_WALK_STAGE)!=1u) return;\n    rbs_walk_arm(w,a,ent,st);\n', '    rbs_walk_arm(w,a,ent,st);\n    if(RBS_B(st+RBS_WALK_STAGE)!=1u) return;\n'),
    'walk-arm-repeats': ('    w->armed++;\n', ''),
    'walk-track-stuck': ('    w->last=s;\n', ''),
    # several per map: instances, the swap
    'multi-count-one': ('    return n<1u?1u:n>RBS_INST_MAX?RBS_INST_MAX:n;', '    return 1u;'),
    'multi-cap-raised': ('#define RBS_INST_MAX 4u', '#define RBS_INST_MAX 8u'),
    'multi-count-wrong-field': ('#define RBS_SAI_SLOT 0x28u', '#define RBS_SAI_SLOT 0x29u'),
    'multi-count-any-slot': (' && (slot=RBS_B(c+8u+RBS_SAI_SLOT))<RBS_SET_MAX && ids[slot]==id) n++;', ' && (slot=RBS_B(c+8u+RBS_SAI_SLOT))<RBS_SET_MAX) n++;'),
    'multi-extra-slots-ignored': ('        if(ids && RBS_INFO_SLOTS+k<RBS_SET_MAX) ids[RBS_INFO_SLOTS+k]=(uint16_t)id;\n', ''),
    'multi-shared-base': ('            memcpy(rbs_g(RBS_FX),base,RBS_FX_SIZE);\n            WORD_AT(RBS_SKEL_STATE)=0;', '            WORD_AT(RBS_SKEL_STATE)=0;'),
    'late-walker-none': (' && !(w=rbs_walk_more())) {', ') {'),
    'late-skeleton-none': (' && !(k=rbs_skel_more())) {', ') {'),
    'late-live-statics': ('    memcpy(rbs_g(RBS_FX),rbs_fx_base,RBS_FX_SIZE);\n    WORD_AT(RBS_SKEL_STATE)=0u;', '    WORD_AT(RBS_SKEL_STATE)=0u;'),
    'late-statics-left': ('    memcpy(rbs_g(RBS_FX),cur,RBS_FX_SIZE);\n    WORD_AT(RBS_SKEL_STATE)=home;', '    WORD_AT(RBS_SKEL_STATE)=home;'),
    'late-home-lost': ('    WORD_AT(RBS_WALK_STATE)=home;\n', ''),
    'late-past-cap': ('    unsigned k=rbs_skelf.n;\n    if(k>=RBS_INST_MAX) return 0;', '    unsigned k=rbs_skelf.n;\n    if(k>=RBS_INST_MAX+1u) return 0;'),
    'multi-statics-left': ('    memcpy(rbs_g(RBS_FX),base,RBS_FX_SIZE);            /* the statics as they were */\n', ''),
    'multi-no-home': ('        WORD_AT(RBS_SKEL_STATE)=rbs_skelf.n?rbs_skels[0].state:0u;\n', ''),
    'multi-fx-not-restored': ('    if(s->swapped) WORD_AT(global)=s->ptr;\n    mapmaker_fx_leave(s->fx);\n', '    if(s->swapped) WORD_AT(global)=s->ptr;\n'),
    'multi-ptr-not-restored': ('    if(s->swapped) WORD_AT(global)=s->ptr;\n', ''),
    'multi-fx-not-swapped': ('    s->fx=mapmaker_fx_enter(fx);\n', '    s->fx=mapmaker_fx_holder;\n'),
    'multi-nested-swaps': ('rbs_in(RBS_SKEL_STATE,k->state,&k->fx,rbs_skelf.cur==k,s);', 'rbs_in(RBS_SKEL_STATE,k->state,&k->fx,0,s);'),
    # v0.4.0-mapmaker.28: the same instance again skipped the statics too
    'multi-same-skips-fx': ('    s->fx=mapmaker_fx_enter(fx);\n    s->swapped=0;\n    if(same) return;\n',
                            '    s->fx=mapmaker_fx_holder;\n    s->swapped=0;\n    if(same) return;\n    s->fx=mapmaker_fx_enter(fx);\n'),
    'multi-level-forget-none': ('    mapmaker_fx_forget(rbs_skels,sizeof rbs_skels);mapmaker_fx_forget(rbs_walks,sizeof rbs_walks);\n', ''),
    # the shared holder itself (pc_mapmaker_bossfx.inc)
    'fx-park-lost': ('    memcpy(prev?prev->b:mapmaker_fx_outside.b,live,MAPMAKER_FX_SIZE);\n', '', 'pc_mapmaker_bossfx.inc'),
    'fx-holder-kept': ('    mapmaker_fx_holder=c;\n', '', 'pc_mapmaker_bossfx.inc'),
    'fx-forget-none': ('    if(h && h>=lo && h<lo+bytes) mapmaker_fx_enter(0);\n', '', 'pc_mapmaker_bossfx.inc'),
    'multi-cur-kept': ('    rbs_skelf.cur=(RbsSkel *)s->prev;\n', ''),
    'multi-reuse-ignored': ('    for(k=0;k<rbs_skelf.n;k++) if(rbs_skels[k].record==rec) return &rbs_skels[k];\n', ''),
    'multi-skel-slot-unswapped': ('    rbs_skel_in(k,&s);\n    original();\n    rbs_skel_out(k,&s);\n}\n', '    original();\n}\n'),
    'multi-fire-unswapped': ('    rbs_skel_in(k,&s);                                 /* its own particle block and counts */\n', ''),
    'multi-beams-unswapped': ('    rbs_walk_in(w,&s);\n    guest_call(sub_0029CCB0,0);\n', '    guest_call(sub_0029CCB0,0);\n'),
    'multi-walk-unswapped': ('    if(!(w=rbs_walk_of(ent))) { original();return; }\n    rbs_walk_in(w,&s);\n', '    if(!(w=rbs_walk_of(ent))) { original();return; }\n'),
    'multi-walk-damage-record': ('    rbs_walk_call(original,WORD_AT(r));', '    rbs_walk_call(original,r);'),
    'multi-walk-fx-first-only': ('    for(k=0;k<rbs_walkf.n;k++) rbs_walk_frame(&rbs_walks[k]);', '    if(rbs_walkf.n) rbs_walk_frame(&rbs_walks[0]);'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new, *name = MUTATIONS[mutation]
        path = INC.parent / name[0] if name else INC
        text = path.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
        if name:                                       # the includer next to it, so the copy is the one found
            (tmp_path / INC.name).write_text(INC.read_text(encoding='utf-8'), encoding='utf-8')
    exe = tmp_path / ('realbosses.exe' if os.name == 'nt' else 'realbosses')
    source = ROOT / 'tests/mapmaker/mapmaker_realbosses.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_realbosses(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker realbosses ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only, and no Story AI chunk byte of its own (+0x21 and +0x29 belong to bosses)."""
    assert not (ROOT / 'config/guest-code-patches/realbosses.json').exists()
    text = INC.read_text(encoding='utf-8')
    writes = re.findall(r'RBS_B\([^)]*\)\s*=[^=]', text)
    assert not writes, writes


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def test_picker_sites():
    a = by_address()
    # sub_002C1EA0: item add sub_00079D60(text, row, flags, 0, 0x002C1C90, 0), then enable, row walk to 150
    assert ('imm', 0x002C1C90) in a[0x002C1F3E].ops
    assert ('imm', 0x20900) in a[0x002C1EC6].ops and ('imm', 0x2000) in a[0x002C1EC0].ops
    assert target(a[0x002C1F86]) == 0x00071630 and target(a[0x002C1F8C]) == 0x00071C70
    assert ('imm', 0x1C20) in a[0x002C1F98].ops                       # 150 rows of 0x30
    # the list end: call sub_000781A0 at 0x002C1FA0, returning to 0x002C1FA5, after the loop
    assert a[0x002C1FA0].call and target(a[0x002C1FA0]) == 0x000781A0 and ends(a, 0x002C1FA0) == 0x002C1FA5
    assert a[0x002C1FA0].function == 0x002C1EA0
    # sub_00071630 returns the last item; sub_00071C70(item, disabled) sets item +0x28 bit 0
    assert mem(a[0x00071630])[0][4] == 0x007BB1D8 and a[0x00071635].mnemonic == 'ret'
    assert mem(a[0x00071C74])[0][4] == 0x28 and ('imm', 0xFFFFFFFE) in a[0x00071C7B].ops
    # pick callback: menu +0x18C row; slot +0 id / +0x14 row / +0x18 variant; disabled rows refused
    assert mem(a[0x002C1CBF])[0][4] == 0x18C and mem(a[0x002C1CD3])[0][4] == 0x00521A1C
    assert mem(a[0x002C1D2B])[0][4] == 0x0075FD90 and mem(a[0x002C1D37])[0][4] == 0x0075FDA4
    assert mem(a[0x002C1D50])[0][4] == 0x0075FDA8
    assert mem(a[0x002C1C9F])[0][4] == 0x28
    # highlight: [0x0078AEA8] = highlighted row; the preview reads it as an Arcade row
    assert mem(a[0x002C1E25])[0][4] == 0x18C and mem(a[0x002C1E2B])[0][4] == 0x0078AEA8
    assert mem(a[0x002C2BA0])[0][4] == 0x0078AEA8 and mem(a[0x002C2BAB])[0][4] == 0x00521A1C


def test_row_lookup_sites():
    a = by_address()
    # sub_002BE300 returns 0 for rows outside 0..149 (so 150 is free); sub_002BE600 returns -1 when not found
    assert ('imm', 0x96) in a[0x002BE30C].ops and a[0x002BE311].mnemonic == 'jae'
    assert '0x2be3d0' in a[0x002BE311].text and a[0x002BE3D0].mnemonic == 'xor'
    assert ('imm', 0xFFFFFFFF) in a[0x002BE63A].ops
    for site, ret, fn in ((0x002BE65A, 0x002BE65F, 0x002BE650), (0x002C0547, 0x002C054C, 0x002C0490),
                          (0x002C1800, 0x002C1805, 0x002C1570), (0x002C180F, 0x002C1814, 0x002C1570)):
        assert a[site].call and target(a[site]) == 0x002BE600 and ends(a, site) == ret and a[site].function == fn
    # sub_002BE650: no push between entry and the call, so its own return is at esp+0xC
    assert [a[x].mnemonic for x in (0x002BE650, 0x002BE654, 0x002BE658, 0x002BE659)] == ['mov', 'mov', 'push', 'push']
    assert target(a[0x003033BB]) == 0x002BE650 and ends(a, 0x003033BB) == 0x003033C0
    assert a[0x003033BB].function == 0x00303350
    # the reload stores the row at slot +0x14; the slot draw names the slot by that row
    assert mem(a[0x002C054C])[0][4] == 0x14
    assert mem(a[0x002C08FB])[0][4] == 0x0075FDA4 and target(a[0x002C0902]) == 0x002BE300
    # sub_0003D7B0(0) is the empty string 0x004766D8 (stock name of a row-less id)
    assert ('imm', 0x004766D8) in a[0x0003D7D1].ops
    # the preview lookups keep [0x0078AEA8] unless the row is found
    assert mem(a[0x002C1817])[0][4] == 0x0078AEA8


def xbe_reader():
    path = _private() / 'game/default.xbe'
    if not path.is_file():
        pytest.skip('needs the private XBE')
    d = path.read_bytes()
    base = struct.unpack_from('<I', d, 0x104)[0]
    n = struct.unpack_from('<I', d, 0x11C)[0]
    sh = struct.unpack_from('<I', d, 0x120)[0] - base
    secs = [struct.unpack_from('<5I', d, sh + i * 56)[1:] for i in range(n)]

    def rd(va, size):
        for sva, vs, ra, rs in secs:
            if sva <= va < sva + vs:
                off = va - sva
                b = d[ra + off:ra + min(off + size, rs)]
                return b + bytes(size - len(b))
        raise KeyError(hex(va))
    return rd


def test_character_data():
    rd = xbe_reader()
    entry = rd(0x0053A3CC + 0x15 * 0x30, 0x30)
    voice, model, lod = struct.unpack_from('<3I', entry, 0)
    assert (model, lod) == (0x5BE, 0x5BF)
    assert voice == struct.unpack_from('<I', rd(0x0053A3CC + 0xA7 * 0x30, 4))[0]   # trophy monster's voices
    ids = {struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    assert 0x15 not in ids and 0xA7 in ids                          # no Arcade row; the stand-in has one
    # sub_002BE270(0x15) == 0: byte table 0x002BE29C (ids 0x0A..0x63) -> jump table 0x002BE294 entry 1
    assert rd(0x002BE29C + 0x15 - 0x0A, 1)[0] == 1
    assert struct.unpack('<2I', rd(0x002BE294, 8)) == (0x002BE28A, 0x002BE290)
    assert rd(0x0049D77C, 30) == b'pak/stream/lv1/anim/lv1_3.pak\0'


def test_mount_and_registration():
    a = by_address()
    # sub_0005C420: sub_001C8540 (mount) at 0x0005C501, then sub_001CA2F0 (register names) at 0x0005C558
    assert target(a[0x0005C501]) == 0x001C8540 and target(a[0x0005C558]) == 0x001CA2F0
    callers = {i.address for i in decoded() if i.call and target(i) == 0x001C8540}
    assert callers == {0x0005C501}
    # sub_001CA2F0 registers lv1_3's names when it is mounted (sub_00062260 != -1)
    assert ('imm', 0x0049D77C) in a[0x001CADA3].ops and target(a[0x001CADA8]) == 0x00062260
    assert a[0x001CADA3].function == 0x001CA2F0 and target(a[0x001CADBC + 5]) == 0x0005B980
    # sub_0005A130(path) = sub_000622D0(path, 1, 1) -> 1 on success; the table holds 30 paks
    assert [op for op in a[0x0005A134].ops] == [('imm', 1)] and target(a[0x0005A139]) == 0x000622D0
    assert mem(a[0x000620E0])[0][4] == 0x006D7880 and ('imm', 0x1E) in a[0x000620F9].ops
    # the bulk unmount clears the whole table (sub_00062720, from the scene loop sub_0003D440)
    assert mem(a[0x0006277E])[0][4] == 0x006D7880
    assert any(i.call and target(i) == 0x00062720 and i.function == 0x0003D440 for i in decoded())
    # scene 0x64 never gets lv1 by the stock list: sub_001C8540 builds lv%d paths only from that scene list
    assert ('imm', 0x0047E624) in a[0x001C897B].ops


def test_class_copy_and_dispatch():
    a = by_address()
    # sub_0015B830: story mode only, record +0/+4 -> AI +0x10/+0x14; record at AI +0xBF0
    assert mem(a[0x0015B841])[0][4] == 0xBF0
    assert mem(a[0x0015B99A])[0][4] == 0x007DE455 and ('imm', 10) in a[0x0015B99A].ops
    assert mem(a[0x0015B9A5])[0][4] == 0x10 and mem(a[0x0015B9AB])[0][4] == 0x14
    assert {i.function for i in decoded() if i.call and target(i) == 0x0015B830} == {0x0015F8C0}
    # MapMaker's own class (0x11000000/0) written by sub_00240C50
    assert ('imm', 0x11000000) in a[0x002421A7].ops and a[0x002421A7].function == 0x00240C50
    # dispatcher: AI +0x14 == 0x80 (with +0x10 == 0) -> sub_00280FA0
    assert mem(a[0x0027D594])[0][4] == 0x14 and mem(a[0x0027D59A])[0][4] == 0x10
    assert ('imm', 0x80) in a[0x0027DB94].ops and target(a[0x0027DBE2]) == 0x00280FA0
    # the handler indexes the character table by AI +0x48 and sets the story focus
    assert mem(a[0x002810F6])[0][4] == 0x48 and mem(a[0x00281110])[0][4] == 0x0053A3EC
    assert mem(a[0x002811B5])[0][4] == 0x007B0C7C


def test_boss_bar_label_site():
    a = by_address()
    assert ('imm', 0xDCC) in a[0x0014CECB].ops and target(a[0x0014CED0]) == 0x0003D7B0
    assert ends(a, 0x0014CED0) == 0x0014CED5 and a[0x0014CED0].function == 0x0014CC80
    assert mem(a[0x0014CEDE])[0][4] == 0xFF4
    # the model flag test: skipped in scene 0x64, applied in the editor 0x66
    assert ('imm', 0x64) in a[0x000A5E80].ops and '0xa5ea2' in a[0x000A5E83].text
    assert ('imm', 0x66) in a[0x000A5E93].ops and ('imm', 0x2000000) in a[0x000A5E9B].ops


def test_monster_cannot_fire_level_events():
    """sub_00280FA0 and its damage callback never reach sub_002274C0 (direct calls)."""
    graph = {}
    for i in decoded():
        if i.call:
            t = target(i)
            if t is not None:
                graph.setdefault(i.function, set()).add(t)
    a = by_address()
    assert ('imm', 0x000BC4D0) in a[0x0028115B].ops                 # damage callback
    seen, todo = set(), [0x00280FA0, 0x000BC4D0]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(graph.get(f, ()))
    assert len(seen) > 100                                          # the walk really ran
    assert 0x002274C0 not in seen
    # control: a boss that does fire events reaches it (the train jetpack, 0x001EEC6D)
    assert 0x002274C0 in graph.get(0x001EEBD0, set())


def test_overhead_meter_sites():
    """The floating meter: MapMaker story play only, actors with a +0x268 record, one caller."""
    a = by_address()
    # sub_0014D1F0: scene 100 and story mode, else the actor loop is skipped (story levels, mission 5)
    assert mem(a[0x0014D21C])[0][4] == 0x0079094C and ('imm', 0x64) in a[0x0014D21C].ops
    assert a[0x0014D223].mnemonic == 'jne' and '0x14d277' in a[0x0014D223].text
    assert mem(a[0x0014D225])[0][4] == 0x007DE455 and ('imm', 0xA) in a[0x0014D225].ops
    assert a[0x0014D22C].mnemonic == 'jne' and '0x14d277' in a[0x0014D22C].text
    # loop: actors 0x007A3580[[0x00790950] .. [0x0074C37C]); entity +0x268 record, not the player's entity
    assert mem(a[0x0014D22E])[0][4] == 0x00790950 and mem(a[0x0014D234])[0][4] == 0x0074C37C
    assert mem(a[0x0014D240])[0][4] == 0x007A3580 and mem(a[0x0014D249])[0][4] == 0x268
    assert mem(a[0x0014D253])[0][4] == 0x14
    assert ('imm', 0x6E) in a[0x0014D258].ops and ('imm', 0x7F) in a[0x0014D25A].ops
    assert a[0x0014D25F].call and target(a[0x0014D25F]) == 0x0013A400 and ends(a, 0x0014D25F) == 0x0014D264
    assert a[0x0014D25F].function == 0x0014D1F0 and ('imm', 0x10) in a[0x0014D26A].ops   # cdecl
    callers = {i.address for i in decoded() if i.call and target(i) == 0x0013A400}
    assert callers == {0x0014D25F}
    assert {i.function for i in decoded() if i.call and target(i) == 0x0014D1F0} == {0x001CEBC0}
    # sub_0013A400 reads the entity from the actor, health +0x1CC, ends in a plain ret
    assert a[0x0013A41E].mnemonic == 'mov' and mem(a[0x0013AAA5])[0][4] == 0x1CC
    assert target(a[0x0013AAEF]) == 0x0001AC50 and a[0x0013AB00].mnemonic == 'ret' and not a[0x0013AB00].ops
    # the list holds entity +0x7C (the actor pool entry)
    assert mem(a[0x0015BCA4])[0][4] == 0x7C and mem(a[0x0015BCAC])[0][4] == 0x007A3580
    # the deerbeast handler creates the record; MapMaker's own AI setup never does
    assert target(a[0x002811A3]) == 0x0012C800 and mem(a[0x002811AA])[0][4] == 0x268
    makers = {i.function for i in decoded() if i.call and target(i) == 0x0012C800}
    assert 0x00280FA0 in makers and 0x000D2FD0 in makers
    assert not makers & {0x00240C50, 0x0015B830, 0x0015F8C0}


# ---- the giant skeleton (mansion boss, id 0x28) -----------------------------

def direct_graph():
    """Direct calls only (an immediate target); indirect calls listed apart."""
    graph, indirect = {}, {}
    for i in decoded():
        if i.call:
            imm = [op[1] for op in i.ops if op[0] == 'imm']
            if imm:
                graph.setdefault(i.function, set()).add(imm[0])
            else:
                indirect.setdefault(i.function, []).append(i.address)
    return graph, indirect


def reach(graph, roots):
    seen, todo = set(), list(roots)
    while todo:
        f = todo.pop()
        if f not in seen:
            seen.add(f)
            todo.extend(graph.get(f, ()))
    return seen


def scene_readers(functions):
    return {i.function for i in decoded() if i.function in functions
            and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] == 0x0079094C for op in i.ops)}


def test_skeleton_data():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x28 * 0x30, 12), 0)
    assert (model, lod) == (0x5E9, 0xFFFFFFFF)
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x28 not in ids and ids.index(0xA6) == 144                # no Arcade row; the stand-in has one
    assert rd(0x002BE29C + 0x28 - 0x0A, 1)[0] == 1                   # sub_002BE270(0x28) == 0, as 0x15
    assert rd(0x0049D6FC, 30) == b'pak/stream/lv1/anim/lv1_6.pak\0'
    assert struct.unpack('<8I', rd(0x005208B8, 32)) == (8, 0, 6, 3, 7, 4, 2, 5)   # action types; 6 = throw

    def table(va, n):
        out = {}
        for k in range(n):
            e = rd(va + k * 0x18, 0x18)
            name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
            out[name] = struct.unpack_from('<H', e, 0x10)[0]
        return out
    lv1_6, lv1_3 = table(0x0048C738, 0x12), table(0x0048C1B0, 0x10)
    assert lv1_6['princess_start'] == 0x19C and lv1_6['princess_raise_react_end'] == 0x1A6
    assert not set(lv1_6.values()) & set(lv1_3.values())             # both may be mounted
    # sub_0025F720: scenes 1 and 17 reach the sub_00287E00 case; scene 100 is past the table (ja)
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert jt[rd(0x0025F7D8 + 0, 1)[0]] == 0x0025F7AD and jt[rd(0x0025F7D8 + 16, 1)[0]] == 0x0025F7AD
    # HUD: model 0x5E9 -> the case at 0x0014CF05 (scene 1 test)
    idx = rd(0x0014D174 + 0x5E9 - 0x5CB, 1)[0]
    assert struct.unpack_from('<I', rd(0x0014D158 + 4 * idx, 4))[0] == 0x0014CF05


def test_skeleton_state_and_dispatch():
    a = by_address()
    # registration of lv1_6's names when mounted
    assert ('imm', 0x0049D6FC) in a[0x001CAE61].ops and target(a[0x001CAE66]) == 0x00062260
    assert a[0x001CAE61].function == 0x001CA2F0 and ('imm', 0x0048C738) in a[0x001CAE75].ops
    assert target(a[0x001CAE7F]) == 0x0005B980
    # class 0x20000 -> sub_0029FE00(entity); scene 1 -> sub_0029CEF0, else sub_0029E0A0
    assert ('imm', 0x20000) in a[0x0027DD54].ops and a[0x0027DD54].function == 0x0027D590
    assert a[0x0027DDD3].call and target(a[0x0027DDD3]) == 0x0029FE00 and ends(a, 0x0027DDD3) == 0x0027DDD8
    assert mem(a[0x0029FE00])[0][4] == 0x0079094C and ('imm', 1) in a[0x0029FE00].ops
    assert a[0x0029FE09].mnemonic == 'jmp' and target(a[0x0029FE09]) == 0x0029CEF0
    assert a[0x0029FE0E].mnemonic == 'jmp' and target(a[0x0029FE0E]) == 0x0029E0A0
    assert {i.address for i in decoded() if i.call and target(i) == 0x0029FE00} == {0x0027DDD3}
    # the handler takes the entity at [esp+4] (0x80 after its 0x78 frame and one push)
    assert a[0x0029CEF0].mnemonic == 'sub' and ('imm', 0x78) in a[0x0029CEF0].ops
    assert mem(a[0x0029CEF4])[0][4] == 0x80 and mem(a[0x0029CEFD])[0][4] == 0x7C
    # level init: sub_0015E7C0 calls sub_0025F720 in story mode only; it is the only caller
    assert {i.address for i in decoded() if i.call and target(i) == 0x0025F720} == {0x0015E818}
    assert mem(a[0x0015E80F])[0][4] == 0x007DE455 and ('imm', 0xA) in a[0x0015E80F].ops
    assert target(a[0x0025F7AD]) == 0x00287E00 and mem(a[0x0025F72F])[0][4] == 0x0079094C
    assert ('imm', 0x50) in a[0x0025F735].ops and a[0x0025F738].mnemonic == 'ja'
    # sub_00287E00: level-heap blocks 0x4C7C -> [0x0078B1EC], 0x7D00 -> [0x0078B0D0], then the init
    assert ('imm', 1) in a[0x00287E00].ops and ('imm', 0x4C7C) in a[0x00287E02].ops
    assert target(a[0x00287E07]) == 0x0003DFA0 and mem(a[0x00287E0F])[0][4] == 0x0078B1EC
    assert target(a[0x00287E14]) == 0x002A62C0 and target(a[0x00287E19]) == 0x00287860
    assert ('imm', 0x7D00) in a[0x002A62C2].ops and mem(a[0x002A62CF])[0][4] == 0x0078B0D0
    assert {i.address for i in decoded() if i.call and target(i) == 0x00287E00} == {0x0025F7AD}
    # sub_00287860 only writes fields of the block (reads no scene, calls only the RNG and rounding)
    assert scene_readers({0x00287860}) == set()


def test_skeleton_scene_sites():
    a = by_address()
    graph, indirect = direct_graph()
    # direct callees of the fight that read the scene: exactly the three hooked/handled ones
    assert scene_readers(graph[0x0029CEF0]) == {0x002274C0, 0x002875F0, 0x00287E20}
    # sub_002875F0: scene 1 -> sub_00059DB0(entity, 0x19C); 17 -> 0x23B; else 0
    assert mem(a[0x002875F0])[0][4] == 0x0079094C
    assert ('imm', 0x19C) in a[0x00287617].ops and target(a[0x0028761D]) == 0x00059DB0
    assert ('imm', 0x23B) in a[0x00287604].ops and a[0x002875FD].mnemonic == 'xor'
    assert target(a[0x0029D49F]) == 0x002875F0
    # sub_00287E20: one scene read, its first instruction; the throw phases
    reads = [i.address for i in decoded() if i.function == 0x00287E20
             and any(op[0] == 'mem' and op[4] == 0x0079094C for op in i.ops)]
    assert reads == [0x00287E20]
    assert mem(a[0x00287E34])[0][4] == 0x1C and mem(a[0x00287E38])[0][4] == 0x7C      # entity, actor
    assert mem(a[0x00287E43])[0][4] == 0x9DC and mem(a[0x00287E49])[0][4] == 0x005208B8
    assert ('imm', 6) in a[0x00287E49].ops
    assert mem(a[0x00287E3B])[0][4] == 0x413C and mem(a[0x00287EA0])[0][4] == 0x4160
    assert ('imm', 1) in a[0x00287FD4].ops and '0x287e7b' in a[0x00287FD7].text
    pushes = [a[x] for x in (0x00287FE0, 0x00287FE5, 0x00287FE7, 0x00287FEF, 0x00287FF9)]
    assert [p.ops[0] for p in pushes] == [('imm', 0x3DCCCCCD), ('imm', 0xC), ('imm', 0xE), ('imm', 0x3E99999A), ('imm', 0x34)]
    assert mem(a[0x00287FDD])[0][4] == 0x34 and mem(a[0x00287FF4])[0][4] == 0xC
    assert target(a[0x00287FFB]) == 0x000EA9C0 and ('imm', 0x38) in a[0x00288011].ops
    assert mem(a[0x00288000])[0][4] == 0x18 and target(a[0x00288006]) == 0x0010EB90
    assert mem(a[0x00288016])[0][4] == 0x4160 and ('imm', 2) in a[0x00288016].ops
    # the aim path's callees read no scene; their one indirect call (a tree walk,
    # sub_001571C0 0x00157204) calls the callback sub_00087310 passes (0x00087324)
    aim = reach(graph, [0x00059E70, 0x0010E730, 0x0010EB90])
    assert {x for f in aim for x in indirect.get(f, ())} == {0x00157204} and 0x00087310 in aim
    assert ('imm', 0x00087260) in a[0x00087324].ops and target(a[0x0008733A]) == 0x00157440
    aim = reach(graph, aim | {0x00087260})
    assert not scene_readers(aim) and {x for f in aim for x in indirect.get(f, ())} == {0x00157204}
    assert target(a[0x0029D050]) == 0x00287E20 and a[0x0029D050].function == 0x0029CEF0
    # the action choice sub_00286F30 plays the throw (0x19F) and resets the phase to 0
    assert ('imm', 0x19F) in a[0x0028753C].ops and mem(a[0x00287542])[0][4] == 0x4160


def test_skeleton_events_and_bar():
    a = by_address()
    graph, indirect = direct_graph()
    # callbacks the fight installs: damage sub_000BC470, slot 2 sub_00286430, sub_00289690, emitter sub_00288BD0
    assert ('imm', 0x000BC470) in a[0x0029D41E].ops and ('imm', 0x00286430) in a[0x0029D43B].ops
    assert ('imm', 0x00289690) in a[0x0029D4BF].ops and ('imm', 0x00288BD0) in a[0x0029D3D0].ops
    seen = reach(graph, [0x0029CEF0, 0x000BC470, 0x00286430, 0x00289690, 0x00288BD0])
    assert len(seen) > 100
    sites = {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x002274C0}
    assert sites == {0x0029E063}                                     # the death, event 3
    assert ('imm', 3) in a[0x0029E057].ops and ends(a, 0x0029E063) == 0x0029E068
    assert mem(a[0x0029E059])[0][4] == 0xFF4                         # the focus is cleared first
    # the fight's own focus and damage record (state 0)
    assert mem(a[0x0029D0FB])[0][4] == 0xFF4 and target(a[0x0029D101]) == 0x0012C800
    assert mem(a[0x0029D108])[0][4] == 0x268 and mem(a[0x0029D0E5])[0][4] == 0x1CC
    # HUD case 0x5E9: scene 1 -> label 0x163C, the bar sub_00139740; else entity +0x1A0 == 1 meter
    assert mem(a[0x0014CF05])[0][4] == 0x0079094C and ('imm', 0x163C) in a[0x0014CF14].ops
    assert '0x14d07b' in a[0x0014CF19].text and target(a[0x0014D07B]) == 0x0003D7B0
    assert target(a[0x0014D0B1]) == 0x00139740 and mem(a[0x0014CF1E])[0][4] == 0x1A0
    assert target(a[0x0014CC9D]) == 0x002E4B80 and mem(a[0x0014D0D5])[0][4] == 8
    # the dust particles are drawn by the frame only in scenes 1 and 17
    assert {i.address for i in decoded() if i.call and target(i) == 0x0029CE00} == {0x001CEE31, 0x001CEE42}
    assert ('imm', 1) in a[0x001CEE1D].ops and ('imm', 0x11) in a[0x001CEE38].ops


def test_custom_set_sites():
    """The map's members are played only for set index 0x11; map-info holds 10 of them."""
    a = by_address()
    # sub_00240C50: index word +0x14 >= 0xC and - 0xC == 5 -> sub_0033EBF0(play set, +0x16)
    assert mem(a[0x00240DAD])[0][4] == 0x14 and ('imm', 0xC) in a[0x00240DAD].ops
    assert ('imm', 5) in a[0x00240DBB].ops and a[0x00240DBE].mnemonic == 'jne'
    assert target(a[0x00240DCE]) == 0x0033EBF0 and ends(a, 0x00240DCE) == 0x00240DD3
    # sub_0023F390: the same test chooses the custom preload sub_0023F320 (ebx = +0x16)
    assert ('imm', 0xC) in a[0x0023F462].ops and ('imm', 5) in a[0x0023F46A].ops
    assert target(a[0x0023F472]) == 0x0023F320 and ends(a, 0x0023F472) == 0x0023F477
    # sub_003006C0 (compile): count byte 10 at data +0x16, members data +0x18 (container +0x10 = data)
    assert mem(a[0x00300884])[0][4] == 0x26 and ('imm', 0xA) in a[0x00300884].ops
    assert mem(a[0x003007B5])[0][4] == 0x24 and mem(a[0x003007AE])[0][4] == 0x00784024

def test_skeleton_death_message_sites():
    """Death = message 2; only the mansion handler (scene 1) acts on it, the catacombs one swallows it."""
    a = by_address()
    # sub_0027CEF0, class 0x20000: sub_0028D0A0(entity, msg, a, b) at 0x0027D3A7
    assert target(a[0x0027D3A7]) == 0x0028D0A0 and ends(a, 0x0027D3A7) == 0x0027D3AC
    # sub_0028D0A0: scene 1 -> jmp sub_00286D30, else jmp sub_0028C560
    assert mem(a[0x0028D0A0])[0][4] == 0x0079094C and ('imm', 1) in a[0x0028D0A0].ops
    assert a[0x0028D0A9].mnemonic == 'jmp' and target(a[0x0028D0A9]) == 0x00286D30
    assert a[0x0028D0AE].mnemonic == 'jmp' and target(a[0x0028D0AE]) == 0x0028C560
    # catacombs: messages 1 and 2 -> or eax, -1 (consumed, nothing done)
    assert a[0x0028C615].mnemonic == 'or' and ('imm', 0xFFFFFFFF) in a[0x0028C615].ops
    # mansion: message 2 -> AI +0x9D0 = 6 with the death animation 0x1A6
    assert ('imm', 6) in a[0x00286D64].ops and mem(a[0x00286D7A])[0][4] == 0x9D0
    assert ('imm', 0x1A6) in a[0x00286D74].ops
    # sub_0015DB20 (the AI death, from the damage code 0x000BB059): message 2, -1 skips the generic death
    assert target(a[0x000BB059]) == 0x0015DB20
    assert ('imm', 2) in a[0x0015DB34].ops and target(a[0x0015DB3B]) == 0x0015C310
    assert ('imm', 0xFFFFFFFF) in a[0x0015DB43].ops
    # state 6 ends with level event 3 and the AI removal sub_0015DDA0
    assert target(a[0x0029E063]) == 0x002274C0 and target(a[0x0029E069]) == 0x0015DDA0
    # the deerbeast's handler sub_00281620 has no scene test (it dies by the generic path)
    assert not any(op[0] == 'mem' and op[4] == 0x0079094C for x in range(0x00281620, 0x002816C8)
                   if x in a for op in a[x].ops)

# ---- the Crow walker (mission 13, id 0x64) ----------------------------------

def test_walker_data():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x64 * 0x30, 12), 0)
    assert (voice, model, lod) == (0x00539948, 0x637, 0xFFFFFFFF)
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x64 not in ids and ids.index(0xA8) == 90                 # no Arcade row; the stand-in has one
    assert 0x64 - 0x0A > 0x59                                        # sub_002BE270(0x64) == 0 (0x002BE277)
    assert rd(0x0049D9A8, 30) == b'pak/stream/lv81/anim/lv81.pak\0'

    def table(va, n):
        out = {}
        for k in range(n):
            e = rd(va + k * 0x18, 0x18)
            name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
            out[name] = struct.unpack_from('<H', e, 0x10)[0]
        return out
    lv81, lv1_6, lv1_3 = table(0x00484F68, 0x14), table(0x0048C738, 0x12), table(0x0048C1B0, 0x10)
    assert lv81['crowewalkerdeath'] == 0x6F9 and lv81['crowewalkerwalk'] == 0x6EF
    assert not set(lv81.values()) & (set(lv1_6.values()) | set(lv1_3.values()))   # all three may be mounted
    # sub_0025F720: scene 81 reaches the sub_002976F0 case
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert jt[rd(0x0025F7D8 + 80, 1)[0]] == 0x0025F79A


def test_walker_sites():
    a = by_address()
    graph, indirect = direct_graph()
    calls = lambda f: {i.address for i in decoded() if i.call and target(i) == f}
    # anim names registered when lv81.pak is mounted
    assert ('imm', 0x0049D9A8) in a[0x001CA92C].ops and a[0x001CA92C].function == 0x001CA2F0
    assert target(a[0x001CA931]) == 0x00062260 and ('imm', 0x00484F68) in a[0x001CA940].ops
    assert target(a[0x001CA94A]) == 0x0005B980
    # class 0x100000: sub_002998E0(entity), its only caller; message handler sub_0029CBF0
    assert ('imm', 0x100000) in a[0x0027DD2A].ops and a[0x0027DD2A].function == 0x0027D590
    assert calls(0x002998E0) == {0x0027DE5A} and a[0x0027DE5A].function == 0x0027D590
    assert ('imm', 0x100000) in a[0x0027D40E].ops and calls(0x0029CBF0) == {0x0027D426}
    assert ('imm', 0x6F9) in a[0x0029CC29].ops and not scene_readers({0x0029CBF0})   # death in any scene
    # level state: sub_002976F0 only from sub_0025F720; 0x64DC -> [0x0078B1E8], then sub_00297690
    assert calls(0x002976F0) == {0x0025F79A}
    assert target(a[0x00297710]) == 0x002A62C0 and ('imm', 0x64DC) in a[0x00297717].ops
    assert target(a[0x0029771C]) == 0x0003DFA0 and mem(a[0x00297721])[0][4] == 0x0078B1E8
    assert a[0x0029773D].mnemonic == 'jmp' and target(a[0x0029773D]) == 0x00297690
    # the fight reads no scene itself; its only direct callee that does is the level event
    assert not scene_readers({0x002998E0}) and not indirect.get(0x002998E0)
    assert scene_readers(graph[0x002998E0]) == {0x002274C0}
    seen = reach(graph, [0x002998E0, 0x0029CBF0])
    sites = {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x002274C0}
    assert sites == {0x0029AEB9, 0x0029B11B, 0x0029CB7B}
    assert [ends(a, x) for x in sorted(sites)] == [0x0029AEBE, 0x0029B120, 0x0029CB80]
    assert ('imm', 2) in a[0x0029CB79].ops and ('imm', 1) in a[0x0029AEB7].ops
    # 'WELL': looked up, then written without a null test (the death end too)
    assert ('imm', 0x57454C4C) in a[0x0029B327].ops and target(a[0x0029B32C]) == 0x0023FF20
    assert ends(a, 0x0029B32C) == 0x0029B331 and mem(a[0x0029B337])[0][4] == 0x5BC
    assert mem(a[0x0029B343])[0][4] == 0x5BC and mem(a[0x0029B349])[0][4] == 0x28
    assert mem(a[0x0029CB34])[0][4] == 0x5BC and mem(a[0x0029CB3A])[0][4] == 0x28
    # beams: drawn by the frame only in scene 81, right after sub_00126560 (its only caller)
    assert calls(0x00126560) == {0x001CEE87} and ends(a, 0x001CEE87) == 0x001CEE8C
    assert mem(a[0x001CEE8C])[0][4] == 0x0079094C and ('imm', 0x51) in a[0x001CEE8C].ops
    assert calls(0x0029CCB0) == {0x001CEE95} and a[0x001CEE95].function == 0x001CEBC0
    assert mem(a[0x0029CCB0])[0][4] == 0x0078B1E8                    # null-safe: tests the state first


def test_skeleton_floor_site():
    """The mansion fight pins the boss to height 0 each frame, right before its facing step."""
    a = by_address()
    assert a[0x0029D067].mnemonic == 'xorps' and a[0x0029D06B].function == 0x0029CEF0
    assert mem(a[0x0029D06B])[0][4] == 0x68 and 'xmm6' in a[0x0029D06B].text
    assert a[0x0029D070].call and target(a[0x0029D070]) == 0x002895A0 and ends(a, 0x0029D070) == 0x0029D075
    assert {i.address for i in decoded() if i.call and target(i) == 0x002895A0} == {0x0029D070, 0x0029E523}
    assert a[0x0029E523].function == 0x0029E0A0 and ends(a, 0x0029E523) == 0x0029E528


def test_walker_intro_waits_for_level_trigger():
    """State 1 of the walker fight waits for mission 13's level trigger SPC2/SPC1;
    the fight applies a requested state (AI +0x9D0) first thing each frame."""
    a = by_address()
    rd = xbe_reader()
    # entity -> AI (ebp); a pending request +0x9D0 becomes the state +0x20, then -1
    assert mem(a[0x002998EF])[0][4] == 0x7C and mem(a[0x002998FA])[0][4] == 0x9D0
    assert a[0x00299905].mnemonic == 'jl' and mem(a[0x00299907])[0][4] == 0x20
    assert mem(a[0x0029991F])[0][4] == 0x20 and mem(a[0x00299922])[0][4] == 0x9D0
    # steering, walking and firing (sub_00298270 ..) only in states above 2
    assert mem(a[0x0029A9B2])[0][4] == 0x20 and ('imm', 2) in a[0x0029A9B2].ops and a[0x0029A9B6].mnemonic == 'jle'
    assert target(a[0x0029AA00]) == 0x00298270 and target(a[0x0029AA05]) == 0x00297750
    # the switch on AI +0x20 (21 states)
    assert mem(a[0x0029B243])[0][4] == 0x20 and ('imm', 0x14) in a[0x0029B246].ops
    assert a[0x0029B24F].mnemonic == 'jmp' and '0x29cb90' in a[0x0029B24F].text.lower()
    jt = [struct.unpack_from('<I', rd(0x0029CB90 + 4 * k, 4))[0] for k in range(21)]
    assert jt[1] == 0x0029B5A6 and jt[2] == 0x0029B62E and jt[4] == 0x0029B696
    # first frame (state 0, stage 0): state 1, stage 1
    assert ('imm', 1) in a[0x0029B4EC].ops and mem(a[0x0029B597])[0][4] == 0x98 and ('imm', 1) in a[0x0029B597].ops
    # state 1: anim done, then trigger 'SPC2' -> 0x6FB or 'SPC1' -> 0x6FC (both state 2), else return
    assert target(a[0x0029B5AA]) == 0x00059DD0
    assert ('imm', 0x53504332) in a[0x0029B5BA].ops and target(a[0x0029B5BF]) == 0x00234190
    assert ('imm', 0x53504331) in a[0x0029B5C4].ops and target(a[0x0029B5CB]) == 0x00234190
    assert target(a[0x0029B5D3]) == 0x00234270 and target(a[0x0029B5FF]) == 0x00234270
    assert ('imm', 0x6FB) in a[0x0029B5E6].ops and ('imm', 0x6FC) in a[0x0029B616].ops
    assert ('imm', 2) in a[0x0029B5F4].ops and ('imm', 2) in a[0x0029B624].ops
    assert target(a[0x0029B609]) == 0x0029CB83
    # the trigger table: level logic items [0x0078CD90] (count [0x0075CCA8]) by tag +0xAC; +0x34 tested
    assert mem(a[0x00234190])[0][4] == 0x0075CCA8 and mem(a[0x0023419E])[0][4] == 0x0078CD90
    assert mem(a[0x002341B2])[0][4] == 0xAC and mem(a[0x00234278])[0][4] == 0x34
    # nothing else uses these tags
    for tag, site in ((0x53504332, 0x0029B5BA), (0x53504331, 0x0029B5C4)):
        assert {i.address for i in decoded() if ('imm', tag) in i.ops} == {site}
    # state 2: anim done, then crowewalkerstunscript 0x6F7, then state 4
    assert target(a[0x0029B632]) == 0x00059DD0 and ('imm', 0x6F7) in a[0x0029B65E].ops
    assert target(a[0x0029B664]) == 0x0005F460 and ('imm', 4) in a[0x0029CAF5].ops
    # stock state changes write the request word as well (0x0029CAFA..)
    assert mem(a[0x0029CB19])[0][4] == 0x20 and mem(a[0x0029CB1C])[0][4] == 0x9D0


def test_walker_roar_needs_an_animation():
    """State 2 waits for the current animation to finish before its roar; a new
    animation controller has none and is never "finished"."""
    a = by_address()
    # state 2: sub_00059DD0(entity +4) == 0 -> return; 0x6F7 current -> state 4; else play 0x6F7
    assert mem(a[0x0029B62E])[0][4] == 4 and target(a[0x0029B632]) == 0x00059DD0 and target(a[0x0029B63C]) == 0x0029CB83
    assert ('imm', 0x6F7) in a[0x0029B642].ops and target(a[0x0029B648]) == 0x00059D20 and target(a[0x0029B652]) == 0x0029CAF5
    assert ('imm', 0x3F800000) in a[0x0029B658].ops and ('imm', 0x6F7) in a[0x0029B65E].ops and target(a[0x0029B664]) == 0x0005F460
    # sub_00059DD0: +0x12C set ? +0x17C : +0x124; sub_00059D20: +0xD4 or +0x12C equals the id
    assert mem(a[0x00059DD8])[0][4] == 0x12C and mem(a[0x00059DE2])[0][4] == 0x124 and mem(a[0x00059DE9])[0][4] == 0x17C
    assert mem(a[0x00059D2B])[0][4] == 0xD4 and mem(a[0x00059D33])[0][4] == 0x12C
    # a new controller (sub_00097C00, ebx = 0): no animation, not finished, no blend
    assert a[0x00097C14].mnemonic == 'xor' and a[0x00097C14].text == 'ebx, ebx'
    assert not [i for i in decoded() if 0x00097C14 < i.address < 0x00097CD4 and i.text.startswith('ebx') and i.mnemonic != 'cmp']
    assert mem(a[0x00097C9D])[0][4] == 0xD4 and mem(a[0x00097CC8])[0][4] == 0x124 and mem(a[0x00097CD4])[0][4] == 0x12C
    for x in (0x00097C9D, 0x00097CC8, 0x00097CD4):
        assert a[x].text.rstrip().endswith('ebx')
    # playing one (sub_0005BC20) sets the current animation and clears "finished"
    assert mem(a[0x0005BDA1])[0][4] == 0xD4 and mem(a[0x0005BE2B])[0][4] == 0x124
    # the play call accepts any animation when the controller has none (sub_001C8C20 -> 0x001C8D5D)
    assert mem(a[0x001C8C98])[0][4] == 0xD4 and a[0x001C8CA0].mnemonic == 'je' and target(a[0x001C8CA0]) == 0x001C8D09
    assert a[0x001C8D30].mnemonic == 'je' and target(a[0x001C8D30]) == 0x001C8D5D
    # 0x6F7 is the lv81 name crowewalkerstunscript, not a looping one (flags 0x10082)
    rd = xbe_reader()
    e = rd(0x00484F68 + 14 * 0x18, 0x18)
    assert struct.unpack_from('<H', e, 0x10)[0] == 0x6F7 and rd(struct.unpack_from('<I', e, 0)[0], 22) == b'crowewalkerstunscript\0'
    assert struct.unpack_from('<I', e, 8)[0] & 1 == 0
    # creation (sub_000DAC30) sets the skeleton's bind pose as the first "animation" with
    # sub_000966E0 (+0xD4 = id, finished +0x124 = 0): 0xFE for the walker skeleton
    assert a[0x000DB1C9].mnemonic == 'push' and ('imm', 0xFE) in a[0x000DB1C9].ops
    assert target(a[0x000DB1CF]) == 0x000966E0 and a[0x000DB1CF].function == 0x000DAC30
    assert mem(a[0x000966F1])[0][4] == 0xD4 and mem(a[0x00096770])[0][4] == 0x124
    # the walker's own animations are lv81's 0x6EE..0x6FC (20 names from 0x6B9)
    ids = [struct.unpack_from('<H', rd(0x00484F68 + k * 0x18 + 0x10, 2))[0] for k in range(0x14)]
    assert min(i for i in ids if i >= 0x6EE) == 0x6EE and max(ids) == 0x6FC and 0xFE not in ids


def test_walker_guns_gate_the_fight():
    """State 4 fights only with both guns active; they start off and only state 0x12
    (after a stun at the stage-1 threshold, back hits only) turns them on."""
    a = by_address()
    rd = xbe_reader()
    jt = [struct.unpack('<I', rd(0x0029CB90 + 4 * k, 4))[0] for k in range(21)]
    # state 4: either gun off -> 0x0029BC6F -> 0x0029BE76: state = edi = 0x13
    assert mem(a[0x0029B696])[0][4] == 0x4CC and target(a[0x0029B69E]) == 0x0029BC6F
    assert mem(a[0x0029B6A4])[0][4] == 0x52C and target(a[0x0029B6AC]) == 0x0029BC6F
    assert target(a[0x0029BC79]) == 0x0029BE76 and mem(a[0x0029BE76])[0][4] == 0x20
    assert a[0x0029B1EC].text == 'edi, 0x13'
    # state 0x13 only waits for both guns == 1, then state 4
    assert jt[0x13] == 0x0029C933 and mem(a[0x0029C940])[0][4] == 0x4CC and mem(a[0x0029C953])[0][4] == 0x52C
    # state 0x12: after the animation ends, sub_00294B90, then state 4
    assert jt[0x12] == 0x0029CA34 and target(a[0x0029CACD]) == 0x00059DD0 and target(a[0x0029CAF0]) == 0x00294B90
    assert ('imm', 4) in a[0x0029CAF5].ops
    assert {i.address for i in decoded() if i.call and target(i) == 0x00294B90} == {0x0029CAF0}
    # sub_00294B90: both guns = 1 (ecx)
    assert a[0x00294C1C].text == 'ecx, 1'
    assert mem(a[0x00294C9D])[0][4] == 0x4CC and mem(a[0x00294CC3])[0][4] == 0x52C
    assert a[0x00294C9D].text.endswith('ecx') and a[0x00294CC3].text.endswith('ecx')
    assert not [i for i in decoded() if 0x00294C1C < i.address < 0x00294CC3 and i.text.startswith('ecx')]
    # the level init never sets them
    for f in (0x002976F0, 0x00297690, 0x002971D0, 0x00296000, 0x00296BD0):
        assert not [i for i in decoded() if i.function == f and any(op[0] == 'mem' and op[4] in (0x4CC, 0x52C) for op in i.ops)]
    # the damage code: stage-1 health at +0x584 -> request 0x12, stun 0x6F6, stage 2
    assert mem(a[0x00298AB5])[0][4] == 0x584 and a[0x00298ACD].text == 'eax, 0x12' and mem(a[0x00298AD6])[0][4] == 0x9D0
    assert ('imm', 0x6F6) in a[0x00298AE8].ops and mem(a[0x00298B01])[0][4] == 0x98 and ('imm', 2) in a[0x00298B01].ops
    # stage-1 damage needs sub_00295150: the hit against the facing (+0xA8) below -0.75 = in the back
    assert target(a[0x00298A48]) == 0x00295150 and mem(a[0x002951FD])[0][4] == 0x0047DE84
    assert struct.unpack('<f', rd(0x0047DE84, 4))[0] == -0.75
    # turning: state 9/0x12 apply +0x590 as 59.5 degrees to the yaw (ebx = entity +0x70) and +0xC4
    assert struct.unpack('<f', rd(0x0049F440, 4))[0] == 59.5 and mem(a[0x0029CA51])[0][4] == 0x0049F440
    assert mem(a[0x0029A992])[0][4] == 0x70 and mem(a[0x0029CA70])[0][4] == 0xC4


def test_several_per_map_entries():
    """Every function that reads a skeleton's or walker's state pointer is reached
    only from the entries the area swaps an instance in for (or from code the area
    never runs for them); the particle/effect statics are 0x0078B080..0x0078B1DB."""
    graph, _ = direct_graph()
    starts = {i.function for i in decoded()}
    for i in decoded():                                                     # tail jumps (sub_00296120 -> sub_00295530)
        if i.mnemonic == 'jmp' and target(i) in starts and target(i) != i.function:
            graph.setdefault(i.function, set()).add(target(i))
    users = {}
    for i in decoded():
        for kind, v in i.values():
            users.setdefault(v, set()).add(i.function)
    # the statics the particle and effect code uses, and the boss pointers after them
    fx = set()
    for f in (0x002A62C0, 0x002A62E0, 0x002A62F0, 0x002A65A0, 0x002A65F0, 0x002A6650, 0x002A7090,
              0x002A7B90, 0x002A7D60, 0x002A7F30, 0x002A8300, 0x002A8370, 0x002A8380, 0x002A85C0, 0x002A8760):
        fx |= {v for i in decoded() if i.function == f for kind, v in i.values() if 0x0078B000 <= v < 0x0078B200}
    assert min(fx) == 0x0078B080 and max(fx) == 0x0078B1D8                 # RBS_FX, RBS_FX_SIZE 0x15C
    assert 0x0078B1E8 not in fx and 0x0078B1EC not in fx
    # the walker: fight, message, damage, slots 2/3, beams; init/restart only otherwise
    walk = reach(graph, [0x002998E0, 0x0029CBF0, 0x00298BA0, 0x00298F60, 0x00299140, 0x0029CCB0])
    assert users[0x0078B1E8] - walk == {0x002976F0, 0x00297690, 0x00296BD0}
    # its callbacks: damage 0x00298BA0, slot 2 0x00299140, slot 3 0x00298F60
    a = {i.address: i for i in decoded()}
    assert ('imm', 0x00299140) in a[0x0029B2AA].ops and ('imm', 0x00298BA0) in a[0x0029B2C3].ops
    assert ('imm', 0x00298F60) in a[0x0029B2D1].ops and ('imm', 3) in a[0x0029B2D6].ops
    # the skeleton: the mansion fight and its slot-2 callback; its particle callback 0x00288BD0 has no entity
    skel = reach(graph, [0x0029CEF0, 0x00286430, 0x00288BD0])
    rest = users[0x0078B1EC] - skel
    # the catacombs fight (never run for a MapMaker skeleton: _princess takes the
    # mansion variant) with the callbacks it registers (immediates in its code)
    catacombs, todo = set(), [0x0029E0A0, 0x0028C560]
    while todo:
        new = reach(graph, todo) - catacombs
        catacombs |= new
        todo = [v for i in decoded() if i.function in new for kind, v in i.values()
                if kind == 'imm' and v in starts and v not in catacombs]
    assert rest - catacombs - {0x00287E00, 0x00287860, 0x0029CE00} == set()
    assert {0x0028C2F0, 0x00288E20, 0x00289720} <= rest & catacombs               # e.g. the gas bottle's damage callback
    assert ('imm', 0x00286430) in a[0x0029D43B].ops and ('imm', 2) in a[0x0029D440].ops
    assert ('imm', 0x00288BD0) in a[0x0029D3D0].ops and target(a[0x0029D3D5]) == 0x00378400
    # sub_00378400 keeps one entry per callback (0x00378410), so all skeletons share it
    assert mem(a[0x00378410])[0][4] == 4 and a[0x00378413].mnemonic == 'je'
    # the message handler and the damage callback read neither pointer nor the statics
    for f in (0x00286D30, 0x00289690):
        r = reach(graph, [f])
        assert not any(r & users.get(v, set()) for v in fx | {0x0078B1EC, 0x0078B1E8})
    # damage records: +0 the hit entity (0x002896E0 skeleton, 0x00298A5E walker)
    assert mem(a[0x002896E0])[0][4] == 0 and mem(a[0x00298A5E])[0][4] == 0
