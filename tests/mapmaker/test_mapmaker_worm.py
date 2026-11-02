"""MapMaker real story boss: the mansion worm of story mission 5 as a Story AI.

Compiles src/interface/pc_mapmaker_worm.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE and its disassembly
are available, checks the stock facts the hooks rely on: the character data,
the anim pak and its names, the scene descriptor and its level callbacks, the
worm block, the init's mansion steps, the head binding and lookup, the
character update slot, the shifted constants and their readers, the route
pads, the hit callback, the draw and its scene gate, and that no level marker
or tag lookup is reachable from the update.
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
INC = ROOT / 'src/interface/pc_mapmaker_worm.inc'

MUTATIONS = {
    'shared-ids-trophy': ('(int32_t)guest_call(sub_00062260,1,WRM_PAK_TROPHY)!=-1 || ', ''),
    'shared-ids-skeleton': (' || (int32_t)guest_call(sub_00062260,1,WRM_PAK_SKELETON)!=-1)) {', ')) {'),
    'shared-ids-always': ('    if(!pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register") &&\n', '    if(1 &&\n'),
    'shared-ids-never': ('    if(!pc_mapmaker_area_enabled("worm","pc_mapmaker_worm_register") &&\n', '    if(0 &&\n'),
    'register-off': ('    if(ans_register()) return;\n    original();', '    original();'),
    'register-skipped': ('    if(ans_register()) return;\n    original();', '    if(ans_register()) return;'),
    'shift-burst-line-missing': ('    {0x0047D78Cu,0x41D80000u,2},', ''),
    'shared-ids-wrong-pak': ('#define WRM_PAK_SKELETON 0x0049D6FCu', '#define WRM_PAK_SKELETON 0x0049D77Cu'),
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('WORD_AT(WRM_SCENE)==100u && WRM_B(WRM_MODE)==10u; }', 'WORD_AT(WRM_SCENE)==100u; }'),
    'any-scene': ('wrm_on() && WORD_AT(WRM_SCENE)==100u && WRM_B(WRM_MODE)==10u; }', 'wrm_on() && WRM_B(WRM_MODE)==10u; }'),
    'set-any-index': ('    if(WRM_H(info+0x14u)!=WRM_SET_MAP) return 0;\n', ''),
    'optout-ignored': ('    if(!wrm_play() || wrm.optout || (has=wrm_set_has())==0) return;\n    WORD_AT(WRM_STATE)=0;',
                       '    if(!wrm_play() || (has=wrm_set_has())==0) return;\n    WORD_AT(WRM_STATE)=0;'),
    'mount-wrong-pak': ('#define WRM_PAK 0x0049D7BCu', '#define WRM_PAK 0x0049DC78u'),
    'mount-full-ignored': ('    if((mounted=WORD_AT(WRM_MOUNTS))+WRM_MOUNT_KEEP>WRM_MOUNT_MAX)', '    if(0)'),
    'mount-when-mounted': ('(has=wrm_set_has())==0 || wrm_pak_mounted()) return;', '(has=wrm_set_has())==0) return;'),
    'level-no-alloc-check': ('    if(!st || !MAPMAKER_PTR_OK(st,WRM_STATE_SIZE)) {\n        wrm_log("no level memory for a mansion worm', '    if(0) {\n        wrm_log("no level memory for a mansion worm'),
    'level-no-scratch-check': ('    if(!scratch || !MAPMAKER_PTR_OK(scratch,WRM_SCRATCH)) {', '    if(0) {'),
    'scratch-not-zeroed': ('    wrm_zero(scratch,WRM_SCRATCH);\n', ''),
    'scratch-no-object': ('    WORD_AT(scratch+WRM_FAKE_TABLE+WRM_OBJECT*4u)=scratch+WRM_FAKE_OBJECT;\n', ''),
    'init-real-objects': ('    WORD_AT(WRM_OBJECTS)=wrm_scratch+WRM_FAKE_TABLE;  /* object', '    (void)0;  /* object'),
    'init-converts-pads': ('    WORD_AT(WRM_ROUTE_DONE)=0;                         /* the mansion', '    (void)0;                         /* the mansion'),
    'init-objects-kept': ('    WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_ROUTE_DONE)=route_done;', '    WORD_AT(WRM_ROUTE_DONE)=route_done;'),
    'init-flag-kept': ('    WORD_AT(WRM_OBJECTS)=objects;WORD_AT(WRM_ROUTE_DONE)=route_done;', '    WORD_AT(WRM_OBJECTS)=objects;'),
    'no-effect': ('    wrm_effect=guest_call(sub_00378400,2,WRM_EFFECT_FN,5u);', '    wrm_effect=0;'),
    'park-none': ('        WORD_AT(w+WRM_W_STATE)=WRM_GONE;\n', ''),
    'park-from-0': ('    for(i=1;i<WRM_WORMS;i++) {                         /* one map worm', '    for(i=0;i<WRM_WORMS;i++) {                         /* one map worm'),
    'park-emitters-kept': ('        if(WORD_AT(w+WRM_W_EMITTER)) guest_call(sub_0010E2A0,1,WORD_AT(w+WRM_W_EMITTER));\n', ''),
    'ai-extra-wins': ('    else if(k==wrm_n && wrm_n>=WRM_MAX) {', '    else if(0) {'),
    'ai-any-character': ("    if(WRM_H(a+WRM_ACTOR_CHAR)!=WRM_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && WRM_H(ent+WRM_ENT_MODEL)==WRM_MODEL)) { original();return; }\n", ''),
    'ai-class-changed': ('        wrm_inst[k].record=rec;wrm_inst[k].actor=a;', '        wrm_inst[k].record=rec;wrm_inst[k].actor=a;WORD_AT(rec)=0;'),
    'head-any-entity': ('        if(wrm_inst[k].record && wrm_inst[k].record==rec && wrm_inst[k].state && !wrm_inst[k].again) return (int)k;', '        if(wrm_inst[k].state && !wrm_inst[k].again) return (int)k;'),
    'head-any-scene': ('    return k>=0 && WORD_AT(WRM_SCENE)==100u && wrm_on()?k:-1;', '    return k>=0 && wrm_on()?k:-1;'),
    'tick-thinks': ('WRM_W_STATE));\n    wrm_leave(prev);\n    MAPMAKER_GUEST_RET();', 'WRM_W_STATE));\n    wrm_leave(prev);\n    original();'),
    'tick-no-update': ('    guest_call(sub_001DAAB0,0);\n', ''),
    'tick-setup-every-time': ('    if(!wrm_lv.ready && !wrm_lv.failed) wrm_setup(ent);', '    if(!wrm_lv.failed) wrm_setup(ent);'),
    'tick-failed-runs': ('    if(wrm_lv.failed) { wrm_leave(prev);original();return; }\n', ''),
    'tick-slot-lost': ('!WORD_AT(ent+WRM_SLOT3)) WORD_AT(ent+WRM_SLOT3)=WRM_CHAR_UPDATE;', '!WORD_AT(ent+WRM_SLOT3)) {}'),
    'tick-slot-after-death': ('    if(WORD_AT(WRM_HEADS)==ent) {\n        if(!wrm_lv.head)', '    if(1) {\n        if(!wrm_lv.head)'),
    'room-binding-kept': ('        wrm_room(ent);\n', ''),
    'room-not-saved': ('wrm_lv.room=r;   /* not bound yet', '(void)r;   /* not bound yet'),
    'room-saved-after-bind': ('    if(WORD_AT(WRM_HEADS)!=ent && wrm_room_ok(r=', '    if(wrm_room_ok(r='),
    'room-any-state': ('    for(i=0;WORD_AT(w+WRM_W_STATE)>=WRM_SURFACED && i<n', '    for(i=0;i<n'),
    'room-any-distance': ('        if(wrm_sane(d) && d<best) { best=d;room=r; }', '        if(wrm_sane(d)) { best=d;room=r; }'),
    'room-empty-player': ('        if((int32_t)WORD_AT(pl+4u)<0 || !MAPMAKER_PTR_OK(pe,0x40u)', '        if(!MAPMAKER_PTR_OK(pe,0x40u)'),
    'room-invalid-player': ('!wrm_room_ok(r=WORD_AT(pe+WRM_ENT_ROOM))) continue;', '!(r=WORD_AT(pe+WRM_ENT_ROOM))) continue;'),
    'room-only-34': ('WORD_AT(ent+WRM_ENT_ROOM)=room;WORD_AT(ent+WRM_ENT_ROOM+4u)=room;', 'WORD_AT(ent+WRM_ENT_ROOM)=room;'),
    'gone-stock': ('    if(k==wrm_n) { original();return; }\n    prev=wrm_enter((int)k);', '    if(1) { original();return; }\n    prev=wrm_enter((int)k);'),
    'gone-unswapped': ('    prev=wrm_enter((int)k);\n    WORD_AT(wrm_lv.state+WRM_W_STATE)=WRM_GONE;', '    prev=wrm_cur;wrm_i=&wrm_inst[k];\n    WORD_AT(wrm_lv.state+WRM_W_STATE)=WRM_GONE;'),
    'gone-state-kept': ('    WORD_AT(wrm_lv.state+WRM_W_STATE)=WRM_GONE;\n    for(j=0', '    for(j=0'),
    'gone-heads-kept': ('    for(j=0;j<WRM_WORMS;j++) if(WORD_AT(WRM_HEADS+j*4u)==ent) WORD_AT(WRM_HEADS+j*4u)=0;\n', ''),
    'gone-head-kept': ('    wrm_lv.head=0;\n', ''),
    'level-counters-kept': ('     * logged neither its update nor its draw) */\n    wrm.logged=wrm.extras=wrm.ticks=wrm.hits=wrm.draws=wrm.scripts=wrm.surfacings=wrm.gones=0;\n',
                            '     * logged neither its update nor its draw) */\n'),
    'restart-counters-kept': ('    wrm_cur=-1;wrm_i=&wrm_none;\n    wrm.logged=wrm.extras=wrm.ticks=wrm.hits=wrm.draws=wrm.scripts=wrm.surfacings=wrm.gones=0;\n',
                              '    wrm_cur=-1;wrm_i=&wrm_none;\n'),
    'shift-none': ('    wrm_shift(1,0);\n    guest_call(sub_001DAAB0,0);', '    guest_call(sub_001DAAB0,0);'),
    'shift-not-restored': ('    guest_call(sub_001DAAB0,0);\n    wrm_shift(0,0);', '    guest_call(sub_001DAAB0,0);'),
    'shift-ignores-stock-bits': ('            if(wrm_lv.saved[k]!=wrm_consts[k].bits) continue;\n', ''),
    'shift-ground-x': ('    {0x0049DF08u,0xC0198937u,1},', '    {0x0049DF08u,0xC0198937u,0},'),
    'shift-box-z-as-x': ('    {0x0047D69Cu,0xC0C00000u,2},', '    {0x0047D69Cu,0xC0C00000u,0},'),
    'shift-wake-missing': ('    {0x0049DE3Cu,0xC29E0000u,0},   /* -79.0: wake region x */\n', ''),
    'shift-point-z-as-x': ('    {0x0049DDD4u,0x41873333u,2},', '    {0x0049DDD4u,0x41873333u,0},'),
    'route-not-swapped': ('WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.pad[k]; }', '(void)0; }'),
    'route-not-restored': ('            for(k=0;k<WRM_ROUTE_N;k++) WORD_AT(WRM_ROUTE+k*4u)=wrm_lv.route[k];\n', ''),
    'offset-box-z': ('wrm_lv.dz=sz-WRM_BOX_Z;', 'wrm_lv.dz=sz;'),
    'offset-ground': ('wrm_lv.dy=sy-WRM_GROUND;', 'wrm_lv.dy=sy;'),
    'place-no-trail': ('    for(k=0;k<2u+WRM_W_TRAILS;k++) {', '    for(k=0;k<2u;k++) {'),
    'place-trail-long': ('    for(k=0;k<2u+WRM_W_TRAILS;k++) {', '    for(k=0;k<3u+WRM_W_TRAILS;k++) {'),
    'place-no-enable': ('    WORD_AT(WRM_ENABLE)=1u;\n', ''),
    'setup-nan': ('    if(!wrm_sane(sx) || !wrm_sane(sy) || !wrm_sane(sz)) {', '    if(!wrm_sane(sy)) {'),
    'route-no-box': ('fabsf(dx)>WRM_BOX_HX || fabsf(dz)>WRM_BOX_HZ ||', ''),
    'route-any-floor': ('               fabsf(wrm_getf(p+0x10u)-sy)>WRM_PAD_DY) continue;', '               0) continue;'),
    'route-invalid-pads': ('!(WORD_AT(p+4u)&1u)) continue;', '0) continue;'),
    'route-no-gap': ('ex*ex+ez*ez<WRM_PAD_GAP*WRM_PAD_GAP) break;', '0) break;'),
    'route-farthest': ('if(bd<0.0f || d<bd) { bd=d;best=i; }', 'if(bd<0.0f || d>bd) { bd=d;best=i; }'),
    'route-unsorted': ('        for(j=k;j>0 && ang[j-1]>ang[j];j--) {', '        for(j=k;j>0 && 0;j--) {'),
    'route-refused-kept': ('            if(rejects>=16u) break;\n            reject[rejects++]=best;\n            continue;', '            break;'),
    'route-no-own-pad': ('        wrm_lv.pad[0]=(uint32_t)own;\n        found=1;', '        return 0;'),
    'route-no-repeat': ('    for(k=found;k<WRM_ROUTE_N;k++) wrm_lv.pad[k]=wrm_lv.pad[k%found];\n', ''),
    'tag-any-site': ('|| WORD_AT(g_esp)!=WRM_RET_TAG ||', '||'),
    'tag-any-worm': ('    g_eax=tag==WRM_TAG && wrm_head(ent)?ent:0u;', '    g_eax=wrm_head(ent)?ent:0u;'),
    'hit-no-script': ('    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;WORD_AT(WRM_OBJECTS)', '    WORD_AT(WRM_OBJECTS)'),
    'hit-no-objects': ('WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;WORD_AT(WRM_OBJECTS)=wrm_lv.scratch+WRM_FAKE_TABLE;', 'WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;'),
    'hit-no-effect': ('    WORD_AT(WRM_EFFECT)=wrm_lv.effect;WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;', '    WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;'),
    'hit-no-tag': ('    WORD_AT(WRM_EFFECT)=wrm_lv.effect;WORD_AT(ent+WRM_ENT_TAG)=WRM_TAG;', '    WORD_AT(WRM_EFFECT)=wrm_lv.effect;'),
    'hit-not-restored': ('    WORD_AT(WRM_SCRIPT)=script;WORD_AT(WRM_OBJECTS)=objects;', '    WORD_AT(WRM_OBJECTS)=objects;'),
    'hit-tag-kept': ('WORD_AT(WRM_EFFECT)=effect;WORD_AT(ent+WRM_ENT_TAG)=tag;', 'WORD_AT(WRM_EFFECT)=effect;'),
    'hit-counter-kept': ('    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;\n    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;', '    WORD_AT(WRM_SCRIPT)=wrm_lv.scratch;'),
    'hit-counter-after': ('    WORD_AT(wrm_lv.scratch+WRM_KILLS)=0;\n    if(WORD_AT(wrm_lv.state+WRM_W_STATE)!=st', '    if(WORD_AT(wrm_lv.state+WRM_W_STATE)!=st'),
    'hit-any-entity': ('       (k=wrm_worm(ent=WORD_AT(info)))<0 || !wrm_inst[k].ready) { original();return; }', '       (k=0,ent=WORD_AT(info),!wrm_inst[k].ready)) { original();return; }'),
    'fx-any-call': ('    int here=wrm_n && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_FX,prev;', '    int here=wrm_n && MAPMAKER_STACK_OK(4),prev;'),
    'fx-not-ready': ('        if(!wrm_inst[k].ready) continue;\n', ''),
    'fx-no-draw': ('    guest_call(sub_001DA990,0);\n', ''),
    'fx-no-shift': ('    wrm_shift(1,1);\n', ''),
    'fx-wall-kept': ('            if(wrm_lv.wall==WRM_WALL_BITS) WORD_AT(WRM_WALL)=WRM_FLT_MAX;', '            (void)0;'),
    'fx-wall-not-restored': ('        if(draw) WORD_AT(WRM_WALL)=wrm_lv.wall;', '        if(draw) {}'),
    'fx-draws-props': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_PROPS) { MAPMAKER_GUEST_RET();return; }\n', ''),
    'fx-draws-extra': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_EXTRA) { MAPMAKER_GUEST_RET();return; }\n', ''),
    'props-always-skipped': ('    if(wrm_lv.drawing && MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==WRM_RET_PROPS)', '    if(MAPMAKER_STACK_OK(4))'),
    'script-kept': ('    if(wrm_lv.ready && MAPMAKER_STACK_OK(8) && WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', '    if(0) {'),
    'script-any-site': ('&& WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', '&& WORD_AT(WRM_SCENE)==100u) {'),
    'script-any-scene': ('WORD_AT(g_esp)==WRM_RET_SCRIPT && WORD_AT(WRM_SCENE)==100u) {', 'WORD_AT(g_esp)==WRM_RET_SCRIPT) {'),
    'height-burst-unshifted': ('    if(WORD_AT(w+WRM_W_ROUTE_FROM)==2u) wrm_setf(head+WRM_ENT_X+4u,wrm_getf(head+WRM_ENT_X+4u)+wrm_lv.dy);', '    (void)0;'),
    'height-always-shifted': ('    if(WORD_AT(w+WRM_W_ROUTE_FROM)==2u) wrm_setf(', '    if(1) wrm_setf('),
    'height-buried-shifted': ('WORD_AT(w+WRM_W_STATE)<WRM_SURFACED) { wrm_lv.surfaced=0;return; }', '0) { wrm_lv.surfaced=0;return; }'),
    'height-not-called': ('            wrm_shift(0,1);\n            wrm_height();', '            wrm_shift(0,1);'),
    'multi-no-swap': ('    prev=wrm_enter(k);                                 /* this worm', '    prev=wrm_cur;wrm_i=&wrm_inst[k];                                 /* this worm'),
    'multi-tick-not-left': ('    if(!wrm.ticks++) wrm_log("mansion worm runs its story update (worm, state)",(uint32_t)k,WORD_AT(wrm_lv.state+WRM_W_STATE));\n    wrm_leave(prev);', '    if(!wrm.ticks++) wrm_log("mansion worm runs its story update (worm, state)",(uint32_t)k,WORD_AT(wrm_lv.state+WRM_W_STATE));'),
    'multi-failed-not-left': ('    if(wrm_lv.failed) { wrm_leave(prev);original();return; }', '    if(wrm_lv.failed) { original();return; }'),
    'multi-leave-no-store': ('    if(wrm_cur==prev) return;\n    wrm_store(wrm_cur);', '    if(wrm_cur==prev) return;'),
    'multi-enter-no-store': ('    if(prev==k) return prev;\n    wrm_store(prev);', '    if(prev==k) return prev;'),
    'multi-window-no-heads': ('    0x0078FAE0u,0x0078FAE4u,0x0078FAE8u,0x0078FAECu,0x0078FAF0u,\n', ''),
    'multi-window-no-block': ('    WRM_STATE,0x0075B4B8u,WRM_ENABLE,0x004FDC20u,', '    0x0075B4B8u,WRM_ENABLE,0x004FDC20u,'),
    'multi-alloc-keeps-globals': ('    for(j=0;j<WRM_NWIN;j++) WORD_AT(wrm_win[j])=live[j];\n    wrm_inst[k].state=st;', '    wrm_inst[k].state=st;'),
    'multi-alloc-not-captured': ('    for(j=0;j<WRM_NWIN;j++) wrm_inst[k].win[j]=WORD_AT(wrm_win[j]);\n', ''),
    'multi-no-extra-alloc': ('    } else if(k==wrm_n && !wrm_alloc(k)) {', '    } else if(k==wrm_n) {'),
    'multi-count-kept': ('        if(k==wrm_n) wrm_n++;\n', ''),
    'multi-cap-high': ('#define WRM_MAX 4u', '#define WRM_MAX 5u'),
    'multi-draw-unswapped': ('        prev=wrm_enter((int)k);\n        if(wrm_state_ok()) {', '        prev=wrm_cur;wrm_i=&wrm_inst[k];\n        if(wrm_state_ok()) {'),
    'multi-hit-unswapped': ('    prev=wrm_enter(k);\n    wrm_hit(ent,original);', '    prev=wrm_cur;\n    wrm_hit(ent,original);'),
    'multi3-state-log-index': ('height %.2f)\\n",k,', 'height %.2f)\\n",0,'),
    'multi3-state-log-level': ('(unsigned)wrm_lv.last_state,(unsigned)st,wrm.serial,', '(unsigned)wrm_lv.last_state,(unsigned)st,0u,'),
    'multi3-setup-log-index': ('z %.1f..%.1f\\n",(int)(wrm_i-wrm_inst),wrm.serial,', 'z %.1f..%.1f\\n",0,wrm.serial,'),
    'multi3-setup-wake-unshifted': ('(double)(WRM_WAKE_X0+wrm_lv.dx)', '(double)WRM_WAKE_X0'),
    'multi3-serial-kept': ('    wrm.serial++;\n', ''),
    'multi3-bind-log-index': ('ent,(uint32_t)(wrm_i-wrm_inst));', 'ent,0u);'),
    'multi3-surfaced-log-index': ('                (int)(wrm_i-wrm_inst),(unsigned)WORD_AT(w+WRM_W_STATE),', '                0,(unsigned)WORD_AT(w+WRM_W_STATE),'),
    'multi3-shift-first-worm': ('off=wrm_consts[k].axis==0?wrm_lv.dx:', 'off=wrm_consts[k].axis==0?wrm_inst[0].dx:'),
    'multi3-room-first-worm': ('        wrm_room(ent);\n', '        wrm_room(wrm_inst[0].head?wrm_inst[0].head:ent);\n'),
    'multi3-fx-first-worm': ('    for(k=0;k<wrm_n;k++) {                             /* each map worm', '    for(k=0;k<1u;k++) {                             /* each map worm'),
    'level-not-forgotten':('    memset(wrm_inst,0,sizeof wrm_inst);               /* a new level', '    (void)0;               /* a new level'),
    # the pause menu's restart (sub_001CDD60: no level hook)
    'restart-never': ('    if(wrm_on()) wrm_restart_reset();\n', ''),
    'restart-after-original': ('    if(wrm_on()) wrm_restart_reset();\n    original();', '    original();\n    if(wrm_on()) wrm_restart_reset();'),
    'restart-when-off': ('    if(wrm_on()) wrm_restart_reset();', '    wrm_restart_reset();'),
    'restart-not-forgotten': ('        memset(&wrm_inst[k],0,sizeof wrm_inst[k]);        /* the run before', '        (void)0;        /* the run before'),
    'restart-not-again': ('        wrm_inst[k].again=1;\n', ''),
    'restart-drops-block': ('wrm_inst[k].state=st;wrm_inst[k].scratch=scratch;', 'wrm_inst[k].state=0;wrm_inst[k].scratch=scratch;'),
    'restart-drops-record': ('wrm_inst[k].effect=effect;wrm_inst[k].record=rec;', 'wrm_inst[k].effect=effect;'),
    'restart-globals-left': ('    if(wrm_cur>=0) wrm_leave(-1);\n    for(k=0;k<wrm_n && k<WRM_MAX;k++) {\n        uint32_t st', '    for(k=0;k<wrm_n && k<WRM_MAX;k++) {\n        uint32_t st'),
    'restart-new-block': ('    if(reuse && MAPMAKER_PTR_OK(reuse,WRM_STATE_SIZE)) { wrm_zero(reuse,WRM_STATE_SIZE);WORD_AT(WRM_STATE)=reuse; }\n    else guest_call',
                          '    if(0) { wrm_zero(reuse,WRM_STATE_SIZE);WORD_AT(WRM_STATE)=reuse; }\n    else guest_call'),
    'restart-not-set-up': ('    } else if(k<wrm_n && wrm_inst[k].again && !wrm_alloc(k)) {', '    } else if(0) {'),
    'restart-ticks-early': ('wrm_inst[k].state && !wrm_inst[k].again) return (int)k;', 'wrm_inst[k].state) return (int)k;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('worm.exe' if os.name == 'nt' else 'worm')
    source = ROOT / 'tests/mapmaker/mapmaker_worm.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


def multi3_log_problems(stderr):
    """The 3-worm case's log (mapmaker_worm.c multi3): each worm's set-up line
    with its own index, level and wake region, its own bind, and the state
    lines of the worms that woke (1 and 2) under their own index. The .28
    playtest log was misread as one worm's updates; these lines tell worms and
    levels apart."""
    part = stderr.split('--- multi3\n', 1)[-1].split('--- multi3 end', 1)[0]
    if '--- multi3 end' not in stderr:
        return ['multi3 did not finish']
    problems = []
    setups = re.findall(r'worm: worm (\d) \(level (\d+)\): mansion garden moved by .*? route pads; '
                        r'buried until a player is in x (\S+)\.\.(\S+), z (\S+)\.\.(\S+)\n', part)
    if [s[0] for s in setups] != ['0', '1', '2'] or len({s[1] for s in setups}) != 1:
        return [f'set-up lines {setups}']
    level = setups[0][1]
    if level in ('0', '1'):                       # the fixture loads many worm levels before this one
        problems.append(f'level {level}: not counted')
    for k, (_, _, x0, x1, z0, z1) in enumerate(setups):
        sx = 10.0 + 30.0 * k
        want = (sx - 12.55, sx + 10.75, -20.0 - 24.65, -20.0 + 23.55)
        if any(abs(float(a) - b) > 0.06 for a, b in zip((x0, x1, z0, z1), want)):
            problems.append(f'worm {k} wake region {(x0, x1, z0, z1)} != {want}')
    for k in range(3):
        if f'mansion worm head bound (head, worm) (0x{0x00480000 + k * 0x400:X}, 0x{k:X})' not in part:
            problems.append(f'worm {k} bind line')
        if f'worm: worm {k} state 0 -> 1 (level {level},' not in part:
            problems.append(f'worm {k} first state line')
    for k in (1, 2):
        if f'worm: worm {k} state 1 -> 9 (level {level},' not in part or f'worm: worm {k} surfaced' not in part:
            problems.append(f'worm {k} woke but its lines are missing')
    if 'worm: worm 0 state 1 -> 9' in part or 'worm: worm 0 surfaced' in part:
        problems.append('worm 0 logged as woken')
    return problems


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_worm(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker worm ok' in result.stdout
        assert multi3_log_problems(result.stderr) == []
    else:
        assert result.returncode != 0 or multi3_log_problems(result.stderr), f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    text = INC.read_text(encoding='utf-8')
    assert 'guest-code-patches' not in text and 'pc_mapmaker_container_write' not in text


# ---- stock facts (need the private XBE and its disassembly) ----------------

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [o for o in i.ops if o[0] == 'mem']


def ends(a, site):
    return site + len(bytes.fromhex(a[site].hex))


def target(i):
    m = re.search(r'0x([0-9a-f]+)', i.text)
    return int(m.group(1), 16) if m else None


def calls_to(f):
    return {i.address for i in decoded() if i.call and target(i) == f}


def functions_of(f):
    return {i.function for i in decoded() if (i.call or i.mnemonic == 'jmp') and target(i) == f}


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


def anim_table(rd, va, n):
    out = {}
    for k in range(n):
        e = rd(va + k * 0x18, 0x18)
        name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
        out[name] = struct.unpack_from('<H', e, 0x10)[0]
    return out


def word(rd, va):
    return struct.unpack('<I', rd(va, 4))[0]


def test_character_and_anims():
    rd = xbe_reader()
    voice, model, lod = struct.unpack_from('<3I', rd(0x0053A3CC + 0x2C * 0x30, 12), 0)
    assert (voice, model, lod) == (0x005397C8, 0x5EA, 0xFFFFFFFF)
    ids = [word(rd, 0x00521A18 + i * 0x30 + 4) for i in range(150)]
    assert 0x2C not in ids                                           # no Arcade row
    assert rd(0x0049D7BC, 30) == b'pak/stream/lv1/anim/lv1_2.pak\0'
    lv1_2 = anim_table(rd, 0x0048BEC8, 0x1E)
    worm = {k: v for k, v in lv1_2.items() if k.startswith('wrm')}
    assert worm == {'wrmburst': 0x194, 'wrmidle': 0x195, 'wrmflaming': 0x196, 'wrmattack': 0x197,
                    'wrmburrow': 0x198, 'wrmhitreact': 0x199, 'wrmtantrum': 0x19A, 'wrmdeath': 0x19B}
    others = {}
    for va, n in ((0x00484F68, 0x14), (0x0048C738, 0x12), (0x0048C1B0, 0x10), (0x0047E938, 0x29)):   # lv81, lv1_6, lv1_3, lv14
        others.update({v: k for k, v in anim_table(rd, va, n).items()})
    for name, v in lv1_2.items():                                    # a shared id is the same anim
        assert v not in others or others[v] == name
    # the shifted constants and their stock bits
    text = INC.read_text(encoding='utf-8')
    for va, b in ((0x0049DF08, 0xC0198937), (0x0049DF10, 0xC29A6666), (0x0049DE40, 0xC25ECCCD), (0x0049DF0C, 0x421E0000),
                  (0x0047D69C, 0xC0C00000), (0x0049DE3C, 0xC29E0000), (0x0049DE34, 0xC0FCCCCD), (0x0049DE38, 0x42213333),
                  (0x0049DDD8, 0xC2794189), (0x0049DDD4, 0x41873333)):
        assert word(rd, va) == b and f'{{0x{va:08X}u,0x{b:08X}u,' in text
    assert word(rd, 0x0049DE08) == 0xC257999A and 'WRM_WALL_BITS 0xC257999Au' in text
    # the six mansion route pad ids and the "not converted" flag
    assert struct.unpack('<7I', rd(0x004FDC24, 28))[:6] == (0x4FF, 0x514, 0x515, 0x51D, 0x516, 0x500)
    assert word(rd, 0x004FDC58) == 1


def test_anim_registration_and_mount():
    a = by_address()
    assert ('imm', 0x0049D7BC) in a[0x001CAD57].ops and a[0x001CAD57].function == 0x001CA2F0
    assert target(a[0x001CAD5C]) == 0x00062260 and ('imm', 0x0048BEC8) in a[0x001CAD6B].ops
    assert ('imm', 0x1E) in a[0x001CAD69].ops and target(a[0x001CAD75]) == 0x0005B980
    assert target(a[0x0005C501]) == 0x001C8540 and target(a[0x0005C558]) == 0x001CA2F0


def test_scene_descriptor_and_level():
    rd = xbe_reader()
    a = by_address()
    # data/mansion2.raw: init0 (script block), level init, level update
    assert rd(word(rd, 0x004FDFDC), 18) == b'data/mansion2.raw\0'
    assert (word(rd, 0x004FDFC4), word(rd, 0x004FDFC8), word(rd, 0x004FDFD0)) == (0x001D7830, 0x001DDB50, 0x001DDC90)
    assert mem(a[0x001D783D])[0][4] == 0x0075B0C4 and ('imm', 0x50) in a[0x001D7833].ops
    # the worm block: 0x1590 bytes, allocated only for scene 1
    assert ('imm', 0x1590) in a[0x001D1E82].ops and mem(a[0x001D1E91])[0][4] == 0x0075B4B4
    assert calls_to(0x001D1E80) == set() and functions_of(0x001D1E80) == {0x0036C740}
    assert ('imm', 1) in a[0x0036C740].ops
    # level init -> worm init; level update -> worm update; frame draws it in scene 1 only
    assert target(a[0x001DDBBA]) == 0x001D2350 and calls_to(0x001D2350) == {0x001DDBBA}
    assert target(a[0x001DDDC0]) == 0x001DAAB0 and calls_to(0x001DAAB0) == {0x001DDDC0}
    assert calls_to(0x001DA990) == {0x001CEE2C} and ('imm', 1) in a[0x001CEE1D].ops
    assert target(a[0x001CEE87]) == 0x00126560 and ends(a, 0x001CEE87) == 0x001CEE8C


def test_worm_init():
    a = by_address()
    # five worms of 0x450, worm 2 in state 8, the others 1; the trail 8 x 0x60 from +0x3C
    assert ('imm', 0x450) in a[0x001D249A].ops and ('imm', 5) in a[0x001D260E].ops
    assert ('imm', 2) in a[0x001D24B6].ops and ('imm', 8) in a[0x001D2525].ops and ('imm', 1) in a[0x001D24FA].ops
    assert mem(a[0x001D24FA])[0][4] == 0x24 and mem(a[0x001D2525])[0][4] == 0x24
    assert mem(a[0x001D254F])[0][4] == 0x40 and ('imm', 8) in a[0x001D2552].ops and ('imm', 0x60) in a[0x001D25DE].ops
    assert mem(a[0x001D2559])[0][4] in (0xFFFFFFFC, -4)
    assert target(a[0x001D25FF]) == 0x00111AE0 and ('imm', 0x43C) in a[0x001D25F3].ops
    # the enable flag off, the pad conversion once ([0x004FDC58]), object 0x18 without a null check
    assert mem(a[0x001D261E])[0][4] == 0x0075B4BC and mem(a[0x001D2617])[0][4] == 0x004FDC58
    assert ('imm', 0x004FDC24) in a[0x001D2631].ops and target(a[0x001D2639]) == 0x0009CC40
    assert ('imm', 0x18) in a[0x001D2654].ops and target(a[0x001D2656]) == 0x000877B0
    assert mem(a[0x001D265B])[0][4] == 0x28 and target(a[0x001D2679]) == 0x0009E720
    assert mem(a[0x000877B4])[0][4] == 0x007B9784
    assert mem(a[0x0009E728])[0][4] == 0x19C                         # sub_0009E720: the hit callback slot
    # the enable is the level script's: sub_001D5E10 has no direct caller
    assert mem(a[0x001D5E10])[0][4] == 0x0075B4BC and calls_to(0x001D5E10) == set()


def test_head_binding_and_tick_slot():
    a = by_address()
    # sub_001D9AF0: the AI tagged 'WRM1' + worm, slots 3 and 4 off, hit callback, 5 lower, 12.1 health
    assert ('imm', 0x57524D31) in a[0x001D9AF0].ops or mem(a[0x001D9AF0])[0][4] == 0x57524D31
    assert target(a[0x001D9AF8]) == 0x0015BA10 and ends(a, 0x001D9AF8) == 0x001D9AFD
    assert ('imm', 3) in a[0x001D9B08].ops and ('imm', 4) in a[0x001D9B12].ops
    assert ('imm', 0x001D1F30) in a[0x001D9B42].ops and target(a[0x001D9B57]) == 0x0009E720
    assert mem(a[0x001D9B64])[0][4] == 0x0078FAE0
    # sub_000A5CE0: slot n at +0x154 + n*4
    assert mem(a[0x000A5CFB])[0][4] == 0x154
    # the character update in slot 3 of every character
    assert ('imm', 0x000D8E30) in a[0x000D9210].ops and ('imm', 3) in a[0x000D9215].ops and ('imm', 8) in a[0x000D9217].ops
    assert target(a[0x000D9219]) == 0x0009E2D0
    # sub_0015BA10: the AI pool by spawn record +8
    assert mem(a[0x0015BA58])[0][4] == 0xBD8 and mem(a[0x0015BA5E])[0][4] == 8
    # the head lookup is sub_001D9AF0's only sub_0015BA10 call, from the update's loop
    assert calls_to(0x001D9AF0) == {0x001DABFC}


def test_update_constants_route_and_reachability():
    a = by_address()
    readers = {}
    for i in decoded():
        for k, v in i.values():
            if k == 'disp':
                readers.setdefault(v, set()).add(i.function)
    worm = {0x001DAAB0, 0x001DACC5, 0x001DC0B8, 0x001DC0BB, 0x001DC0FC}     # the update and its tail aliases
    for va in (0x0049DF08, 0x0049DF10, 0x0049DF0C):
        assert readers[va] <= worm
    assert readers[0x0049DE40] <= worm | {0x001D5D70}
    for va in (0x0049DE34, 0x0049DE38, 0x0049DE3C):
        assert readers[va] == {0x001D5D70}
    assert readers[0x0049DDD8] <= worm | {0x001D2350, 0x001D9B80} and readers[0x0049DDD4] <= worm | {0x001D2350, 0x001D9B80}
    assert readers[0x0049DE08] == {0x001D2800, 0x001D9B80}
    # the box clamp reads -6.0 (shared) at 0x001DBB6D; the wake test is called from the update
    assert mem(a[0x001DBB6D])[0][4] == 0x0047D69C and target(a[0x001DB108]) == 0x001D5D70
    # the route: only the update reads the six pads
    assert readers[0x004FDC24] <= worm | {0x001D2350}
    # nothing the update reaches reads -6.0, fires a level marker or looks up a tag (direct calls)
    graph = {}
    for i in decoded():
        if (i.call or i.mnemonic == 'jmp') and target(i) is not None:
            graph.setdefault(i.function, set()).add(target(i))

    def reach(stop):
        seen, todo = set(), [0x001DAAB0]
        while todo:
            f = todo.pop()
            if f in seen or f in stop:
                continue
            seen.add(f)
            todo.extend(t for t in graph.get(f, ()) if t not in seen)
        return seen
    seen = reach(set())
    others = readers[0x0047D69C] - worm
    assert others and not others & seen
    others = readers[0x0047D78C] - worm
    assert others and not others & seen
    assert 0x0015BA10 in seen and 0x000877B0 not in seen
    # the two ways to markers and tag lookups: the dropped 'SFCC' trigger and the generic explosion (a read)
    assert ('imm', 0x53464343) in a[0x001DC455].ops and target(a[0x001DC45A]) == 0x00234CD0
    assert ends(a, 0x001DC45A) == 0x001DC45F
    assert {x for x in calls_to(0x00234CD0) if 0x001DAAB0 <= x < 0x001DD400} == {0x001DC45A}
    assert len(calls_to(0x000BB6C0)) > 50
    assert not {0x002274C0, 0x002273C0, 0x002275F0, 0x0023FF20} & reach({0x00234CD0, 0x000BB6C0})
    assert not {0x002274C0, 0x002275F0, 0x0023FF20} & reach({0x00234CD0})


def test_head_room_and_slot12():
    """The binding puts the head in room 0x18 (the garden) and sets slot 12 to sub_001D5E20; both damage
    rays start from the head's +0x34 room; sub_001D5E20 writes state 4 through head +0x60 and the live
    block; sub_000A7090 (entity removal) calls slot 12; rooms are 1..[0x007B975C]."""
    a = by_address()
    assert ('imm', 0x18) in a[0x001D9B3D].ops and mem(a[0x001D9B48])[0][4] == 0x34 and mem(a[0x001D9B4B])[0][4] == 0x38
    assert ('imm', 0x001D5E20) in a[0x001D9B1A].ops and ('imm', 0xC) in a[0x001D9B1F].ops and target(a[0x001D9B22]) == 0x000A5CE0
    for load, ray in ((0x001DC3E8, 0x001DC3F4), (0x001DCCC1, 0x001DCCCD)):
        assert mem(a[load])[0][4] == 0x34 and mem(a[load])[0][1] == 'edi' and target(a[ray]) == 0x00088C60
    assert mem(a[0x001D5E24])[0][4] == 0x60 and mem(a[0x001D5E27])[0][4] == 0x0075B4B4 and ('imm', 0x450) in a[0x001D5E2D].ops
    assert calls_to(0x001D5E20) == set()
    assert mem(a[0x000A7095])[0][4] == 0x184 and a[0x000A70A5].call
    assert mem(a[0x000C3B46])[0][4] == 0x007B975C and mem(a[0x000C3B52])[0][4] == 0x007B9784
    assert mem(a[0x001DAD0B])[0][4] == 0x00790950 and mem(a[0x001DAD1C])[0][4] == 0x007B0C48
    assert ('imm', 0x1584) in a[0x001DAE7B].ops


def test_hit_callback():
    a = by_address()
    # the head's tag at +0x60, the kill counter in the script block, the 5-kill branch
    assert mem(a[0x001D1FC1])[0][4] == 0x60
    assert mem(a[0x001D1F9B])[0][4] == 0x0075B0C4 and mem(a[0x001D2013])[0][4] == 0x3C
    assert ('imm', 5) in a[0x001D2019].ops and target(a[0x001D204F]) == 0x002274C0
    assert target(a[0x001D2023]) == 0x000877B0 and target(a[0x001D2116]) == 0x002274C0
    assert mem(a[0x001D207C])[0][4] == 0x0078F590
    # the mansion effect type: sub_00378400(sub_001D6290, 5)
    assert ('imm', 0x001D6290) in a[0x001D64A3].ops and ('imm', 5) in a[0x001D64A1].ops
    assert target(a[0x001D64AD]) == 0x00378400 and mem(a[0x001D64D7])[0][4] == 0x0078F590


def test_draw():
    a = by_address()
    # sub_001DA990: the mansion parts first, then the worms (enable flag), heads placed by sub_001D9B80
    assert target(a[0x001DA993]) == 0x001D9370 and ends(a, 0x001DA993) == 0x001DA998
    assert target(a[0x001DA9A4]) == 0x001D7200 and ends(a, 0x001DA9A4) == 0x001DA9A9
    assert mem(a[0x001DA9BA])[0][4] == 0x0075B4BC
    assert target(a[0x001DAA4B]) == 0x001D9B80 and target(a[0x001DAA3B]) == 0x001D2800
    assert mem(a[0x001DA917])[0][4] == 0x64 and mem(a[0x001DA928])[0][4] == 0x6C
    # the mansion wall clip: camera x against -53.9
    assert mem(a[0x001D294A])[0][4] == 0x0049DE08 and mem(a[0x001D2942])[0][4] == 0x27C



def test_lv1_paks_share_anim_ids():
    """lv1_2 shares two Jo-Beth names (and ids) with lv1_3 (trophy monster) and lv1_6 (skeleton);
    the worm's own entries (21..28 and 5) are unique among every boss pak's registration table."""
    rd = xbe_reader()

    def names(va, n):
        return {anim_table_entry(rd, va, k) for k in range(n)}
    lv1_2 = [anim_table_entry(rd, 0x0048BEC8, k) for k in range(0x1E)]
    trophy, skel = names(0x0048C1B0, 0x10), names(0x0048C738, 0x12)
    assert {n for n in lv1_2 if n in trophy} == {('jblookout', 0x193)}
    assert {n for n in lv1_2 if n in skel} == {('jbgrossend', 0x1AE)} and not trophy & skel
    kept = lv1_2[21:29] + [lv1_2[5]]
    assert [n for n, _ in kept] == ['wrmburst', 'wrmidle', 'wrmflaming', 'wrmattack', 'wrmburrow', 'wrmhitreact',
                                    'wrmtantrum', 'wrmdeath', 'barney_worm']
    others = set()
    for va, n in ((0x00484F68, 0x14), (0x0048C738, 0x12), (0x0048C1B0, 0x10), (0x0047E938, 0x29)):  # lv81, lv1_6, lv1_3, lv14
        others |= names(va, n)
    assert not {i for _, i in kept} & {i for _, i in others} and not {n for n, _ in kept} & {n for n, _ in others}


def anim_table_entry(rd, va, k):
    e = rd(va + k * 0x18, 0x18)
    return rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode(), struct.unpack_from('<H', e, 0x10)[0]


def test_registration_sites_and_fixup():
    """sub_0005B980(prefix, table, count) registers entry by entry; sub_00057F30 fixes the anim data up in place;
    the worm's state 2 burst (+0x44C 2, barney_worm 0x149) is behind the z line 27.0 (0x0047D78C)."""
    a = by_address()
    assert ('imm', 0x1E) in a[0x001CAD69].ops and target(a[0x001CAD75]) == 0x0005B980 and ends(a, 0x001CAD75) == 0x001CAD7A
    assert ('imm', 0x0048BEC8) in a[0x001C95F2].ops and target(a[0x001C95FC]) == 0x0005B980
    assert mem(a[0x0005B993])[0][4] == 0x8C and mem(a[0x0005B99B])[0][4] == 0x94 and mem(a[0x0005B987])[0][4] == 0x90
    assert ('imm', 0x18) in a[0x0005B9E9].ops and target(a[0x0005B9C9]) == 0x00057F30
    assert mem(a[0x0005805F])[0][4] == 0x1C and mem(a[0x00058070])[0][4] == 0x28          # offsets become pointers
    # barney_worm on the head in state 9 when +0x44C == 2; +0x44C = the state (2) south of z 27.0
    assert ('imm', 0x149) in a[0x001DBF46].ops and target(a[0x001DBF64]) == 0x0005F460
    assert mem(a[0x001DB887])[0][4] == 0x0047D78C and mem(a[0x001DB88F])[0][4] == 8
    assert mem(a[0x001DB8A4])[0][4] == 0x44C
    readers = {i.function for i in decoded() if any(k == 'disp' and v == 0x0047D78C for k, v in i.values())}
    assert 0x001DACC5 in readers and len(readers - {0x001DAAB0, 0x001DACC5}) == 5   # five readers outside the worm


def test_heights():
    """The surfaced head: at the worm's height, or after a state-2 burst at the fixed point (x, 0, z);
    the mission 5 garden floor is the worms' height -2.4 (most common height of the level geometry
    in the garden region)."""
    a = by_address()
    assert mem(a[0x001DA94C])[0][4] == 0x0049DDD8 and mem(a[0x001DA954])[0][4] == 0x0049DDD4
    assert a[0x001DA961].mnemonic == 'xorps' and mem(a[0x001DA964])[0][4] == 0x68          # y 0
    assert mem(a[0x001DA8E6])[0][4] == 0x44C and ('imm', 2) in a[0x001DA8EF].ops
    assert mem(a[0x001DA937])[0][4] == 0x68                                                # else the worm's height
    import collections
    import zlib
    pak = _private() / 'game/pak/story/l_1_st.pak'
    if not pak.is_file():
        pytest.skip('needs the mission 5 pak')
    raw = pak.read_bytes()
    off, ln = struct.unpack_from('<II', raw, 4)
    idx = {c: (p, n) for c, p, n, _ in struct.iter_unpack('<4I', raw[off:off + ln])}
    assert zlib.crc32(b'data/mansion2.raw') & 0xFFFFFFFF in idx
    best = collections.Counter()
    for p, n in idx.values():
        if n < 0x100000:
            continue
        d = raw[p:p + n]
        for i in range(0, len(d) - 12, 4):
            x, y, z = struct.unpack_from('<3f', d, i)
            if -80 < x < -54 and -8 < z < 41 and -8 < y < 8 and abs(y) > 1e-6:
                best[round(y, 1)] += 1
    assert best.most_common(1)[0][0] == -2.4


def test_multi_instance_globals():
    """The globals the worm code writes outside the geometry tables and the draw scratch are the swapped
    window: the block, its init flag, the enable flag, the active flag and the five heads."""
    a = by_address()
    graph, writes = {}, {}
    for i in decoded():
        if i.call or (i.branch and i.mnemonic == 'jmp'):
            t = target(i)
            if t is not None:
                graph.setdefault(i.function, set()).add(t)
        if i.mnemonic in ('mov', 'movss', 'inc', 'dec', 'add', 'or', 'and', 'fstp') and i.ops and i.ops[0][0] == 'mem':
            disp = i.ops[0][4]
            if 0x004FD000 <= disp < 0x00547000 or 0x00700000 <= disp < 0x00800000:
                writes.setdefault(i.function, set()).add(disp)
    seen, todo = set(), [0x001DAAB0, 0x001DA990, 0x001D1F30, 0x001D2350, 0x001D1E80]
    while todo:
        f = todo.pop()
        if f not in seen:
            seen.add(f)
            todo.extend(graph.get(f, ()))
    worm = {f for f in seen if 0x001D1000 <= f < 0x001DE000}
    written = set().union(*(writes.get(f, set()) for f in worm))
    tables = {v for v in written if 0x0078F300 <= v < 0x00790800 and not 0x0078FAE0 <= v <= 0x0078FAF0}   # geometry, scratch
    assert {v for f in worm - {0x001D1BD0, 0x001D2800} for v in writes.get(f, set())} & tables <= set()
    kept = written - tables
    assert kept == {0x0075B4B4, 0x0075B4B8, 0x0075B4BC, 0x004FDC20, 0x004FDC58,
                    0x0078FAE0, 0x0078FAE4, 0x0078FAE8, 0x0078FAEC, 0x0078FAF0}, sorted(hex(v) for v in kept)
    text = INC.read_text(encoding='utf-8')
    assert 'WRM_STATE,0x0075B4B8u,WRM_ENABLE,0x004FDC20u,' in text          # 0x004FDC58: kept 0 during each init
    assert '0x0078FAE0u,0x0078FAE4u,0x0078FAE8u,0x0078FAECu,0x0078FAF0u,' in text


def test_restart_skips_the_level_hook():
    """The pause menu's restart (sub_001CDD60, wrapped:
    pc_mapmaker_worm_restart) never reaches the story level init sub_0025F720
    where this area sets up a level; the worm block is level memory
    (sub_001D1E80: sub_0003E0B0(0x20, 0x1590, 1)) and the stock block init
    sub_001D2350 allocates nothing, so a restart can set the same block up again."""
    a = by_address()
    assert calls_to(0x001CDD60) == {0x0003D4E1} and calls_to(0x0025F720) == {0x0015E818}
    assert calls_to(0x0015E7C0) == {0x001CDC20} and target(a[0x001CDD74]) == 0x0015E4F0
    restart = [i for i in decoded() if 0x001CDD60 <= i.address < 0x001CDE27 and (i.call or i.mnemonic == 'jmp')]
    assert restart and not {target(i) for i in restart} & {0x001CDB80, 0x0015E7C0, 0x0025F720}
    assert target(a[0x0015E7B1]) == 0x0025F6A0                      # sub_0015E4F0 ends in the soft restart
    assert ('imm', 0x1590) in a[0x001D1E82].ops and target(a[0x001D1E89]) == 0x0003E0B0
    assert mem(a[0x001D1E91])[0][4] == 0x0075B4B4
    init = [i for i in decoded() if 0x001D2350 <= i.address < 0x001D2687 and i.call]
    assert init and not {target(i) for i in init} & {0x0003DFA0, 0x0003E0B0, 0x001D1E80}
