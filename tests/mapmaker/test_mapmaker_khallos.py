"""MapMaker Khallos: the mission 4 flying-chair boss in MapMaker story play.

Compiles src/interface/pc_mapmaker_khallos.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE, its disassembly and
the game paks are available, checks the stock facts the hooks rely on.
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import zlib
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, _private

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_khallos.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('return khl_on() && WORD_AT(KHL_SCENE)==100u && KHL_B(KHL_MODE)==10u; }', 'return khl_on() && WORD_AT(KHL_SCENE)==100u; }'),
    'any-scene': ('return khl_on() && WORD_AT(KHL_SCENE)==100u && KHL_B(KHL_MODE)==10u; }', 'return khl_on() && KHL_B(KHL_MODE)==10u; }'),
    'mine-any-scene': ('    if(!obj || !khl_lv.n || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return -1;', '    if(!obj || !khl_lv.n || !khl_on()) return -1;'),
    'mine-any-object': ('    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].obj==obj) return (int)k;', '    for(k=0;k<khl_lv.n;k++) return (int)k;'),
    'stale-resmap': ('#define KHL_RESMAP 0x008CA000u', '#define KHL_RESMAP 0x0078B860u'),
    'wrong-id': ('#define KHL_ID 0x33u', '#define KHL_ID 0x34u'),
    'mount-wrong-pak': ('#define KHL_PAK 0x0049E1D0u', '#define KHL_PAK 0x0049E1B4u'),
    'mount-loads-whole': ('guest_call(sub_000622D0,3,KHL_PAK,0u,1u)', 'guest_call(sub_000622D0,3,KHL_PAK,1u,1u)'),
    'mount-any-map': ('if(!khl_play() || khl.optout || (has=khl_map_has(KHL_ID))==0) return;', 'if(!khl_play() || khl.optout || (has=1)==0) return;'),
    'mount-ignores-optout': ('if(!khl_play() || khl.optout || (has=khl_map_has(KHL_ID))==0) return;', 'if(!khl_play() || (has=khl_map_has(KHL_ID))==0) return;'),
    'mount-twice': ('    if(khl_mounted(KHL_PAK)) return;\n    if((mounted', '    if((mounted'),
    'mount-fills-table': ('#define KHL_MOUNT_KEEP 8u', '#define KHL_MOUNT_KEEP 0u'),
    'mount-off-by-one': ('if((mounted=WORD_AT(KHL_MOUNTS))+KHL_MOUNT_KEEP>KHL_MOUNT_MAX) {\n        khl_log("pak mount table full; the chair', 'if((mounted=WORD_AT(KHL_MOUNTS))+KHL_MOUNT_KEEP>=KHL_MOUNT_MAX) {\n        khl_log("pak mount table full; the chair'),
    'mount-preset': ('    if(KHL_H(info+KHL_INFO_SET)!=KHL_SET_MAP) return 0;\n', ''),
    'mount-no-unreadable': ('WORD_AT(container)!=KHL_MAGIC) return -1;', 'WORD_AT(container)!=KHL_MAGIC) return 0;'),
    'mount-skips-extra': ('    for(k=0;extra && k<KHL_B(extra+3u);k++)', '    for(k=0;0 && extra && k<KHL_B(extra+3u);k++)'),
    'mount-ignores-count': ('    if(n>KHL_INFO_SLOTS) n=KHL_INFO_SLOTS;', '    n=KHL_INFO_SLOTS;'),
    'mount-resets-when-idle': ('    if(WORD_AT(KHL_MOUNT_SKIP)) { original();return; }    /* the original does nothing */\n', ''),
    'mount-keeps-old-chair': ('    memset(khl_ch,0,sizeof khl_ch);\n    khl_lv.cur=-1;', '    khl_lv.cur=-1;'),
    'anims-wrong-pak': ('#define KHL_ANIM_PAK 0x0049D9C8u', '#define KHL_ANIM_PAK 0x0049D8C8u'),
    'anims-without-chair': ('    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)) return;', '    if(!khl_play() || khl.optout || !khl_map_has(KHL_RIDER_ID)) return;'),
    'anims-needs-rider': ('    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)) return;', '    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID) || !khl_map_has(KHL_RIDER_ID)) return;'),
    'anims-twice': ('    if(khl_mounted(KHL_ANIM_PAK)) return;\n', ''),
    'anims-fills-table': ('khl_log("pak mount table full; no seated Khallos",mounted,KHL_MOUNT_MAX);\n        return;', 'khl_log("pak mount table full; no seated Khallos",mounted,KHL_MOUNT_MAX);'),
    'anims-any-scene': ('    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)', '    if(!khl_on() || khl.optout || !khl_map_has(KHL_ID)'),
    'anims-before-original': ('    uint32_t mounted;\n    original();\n    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)', '    uint32_t mounted;\n    if(!khl_play() || khl.optout || !khl_map_has(KHL_ID)'),
    'request-never': ('    guest_call(sub_000A5E40,1,KHL_MODEL);\n', ''),
    'request-wrong-model': ('    guest_call(sub_000A5E40,1,KHL_MODEL);', '    guest_call(sub_000A5E40,1,0x377u);'),
    'request-before-original': ('    original();\n    if(!khl_play() || khl.optout || khl_map_has(KHL_ID)==0) return;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init', '    if(!khl_play() || khl.optout || khl_map_has(KHL_ID)==0) { original();return; }\n    guest_call(sub_000A5E40,1,KHL_MODEL);original();g_esp-=4u;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init'),
    'request-any-map': ('    if(!khl_play() || khl.optout || khl_map_has(KHL_ID)==0) return;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init', '    if(!khl_play() || khl.optout) return;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init'),
    'request-any-scene': ('    if(!khl_play() || khl.optout || khl_map_has(KHL_ID)==0) return;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init', '    if(!khl_on() || khl.optout || khl_map_has(KHL_ID)==0) return;\n    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init'),
    'request-without-pak': ('    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted at level init', '    if(0) { khl_log("prop3.pak not mounted at level init'),
    'request-without-row': ('    if(!khl_resource(KHL_MODEL)) { khl_log("no resource row', '    if(0) { khl_log("no resource row'),
    'ready-ignored': ('    if(!khl_model_ready(e,&req)) {', '    if(0 && !khl_model_ready(e,&req)) {'),
    'ready-empty-request': ('    return r && MAPMAKER_PTR_OK(r,0x14u) && WORD_AT(r+KHL_REQ_DATA)!=0;', '    return r && MAPMAKER_PTR_OK(r,0x14u);'),
    'ready-no-lod': ('    return !(path=WORD_AT(e+KHL_RES_LOD)) || khl_path_ready(path,req);', '    (void)path;return 1;'),
    'ready-loaded-ignored': ('    if(WORD_AT(e+KHL_RES_LOADED)) return 1;             /* loaded before: no file read */\n', ''),
    'spawn-before-original': ('    a=WORD_AT(g_esp+4u);\n    original();\n', '    a=WORD_AT(g_esp+4u);\n'),
    'spawn-without-pak': ('    if(!khl_mounted(KHL_PAK)) { khl_log("prop3.pak not mounted: the chair', '    if(0) { khl_log("prop3.pak not mounted: the chair'),
    'spawn-ignores-init': ('|| WORD_AT(e+KHL_RES_INIT)!=KHL_INIT) {', ') {'),
    'spawn-wrong-model': ('obj=guest_call(sub_000A6F90,5,KHL_MODEL,', 'obj=guest_call(sub_000A6F90,5,0x377u,'),
    'spawn-args-swapped': ('KHL_MODEL,args,args+12u,0u,0u);', 'KHL_MODEL,args+12u,args,0u,0u);'),
    'spawn-no-yaw': ('khl_setf(args+16u,yaw);', 'khl_setf(args+16u,0.0f);'),
    'spawn-failed-kept': ('    if(!obj || !MAPMAKER_PTR_OK(obj,0x100u)) {', '    if(!MAPMAKER_PTR_OK(obj,0x100u) && 0) {'),
    'variant-kept': ("    if(WORD_AT(KHL_OBJ)!=obj) {                            /* the train's rising variant */", '    if(0) {'),
    'variant-not-removed': ('        guest_call(sub_000A9070,1,obj);\n        khl_out(k,keep);khl_detach(k);\n        khl_log("the chair took', '        khl_out(k,keep);khl_detach(k);\n        khl_log("the chair took'),
    'anchor-not-rebased': ('    khl_setf(KHL_POS,pos[0]);khl_setf(KHL_POS+4u,pos[1]+KHL_LIFT);khl_setf(KHL_POS+8u,pos[2]);\n', ''),
    'anchor-no-lift': ('khl_setf(KHL_POS+4u,pos[1]+KHL_LIFT);', 'khl_setf(KHL_POS+4u,pos[1]);'),
    'object-not-placed': ('    for(i=0;i<3u;i++) khl_setf(obj+KHL_ENT_POS+i*4u,khl_getf(KHL_POS+i*4u));\n', ''),
    'rider-any-character': ('    if(id==KHL_RIDER_ID) {', '    if(id==KHL_RIDER_ID || id==0x34u) {'),
    'rider-any-distance': ('        if(dx*dx+dz*dz<=bd) { bd=dx*dx+dz*dz;best=c; }', '        if(1) { bd=dx*dx+dz*dz;best=c; }'),
    'rider-first-not-nearest': ('        if(dx*dx+dz*dz<=bd) { bd=dx*dx+dz*dz;best=c; }', '        if(dx*dx+dz*dz<=bd && best==KHL_CANDS) { best=c; }'),
    'rider-x-only': ('dx=khl_lv.cand_x[c]-khl_ch[k].spawn[0];dz=khl_lv.cand_z[c]-khl_ch[k].spawn[2];', 'dx=khl_lv.cand_x[c]-khl_ch[k].spawn[0];dz=0.0f;'),
    'rider-without-anims': ('    if(!khl_mounted(KHL_ANIM_PAK)) { khl_log("lv11.pak not mounted: Khallos near', '    if(0) { khl_log("lv11.pak not mounted: Khallos near'),
    'rider-stale-entry': ('        if(khl_lv.cand_bound[c] || !khl_same_ai(khl_lv.cand[c],khl_lv.cand_ent[c],khl_lv.cand_rec[c])) continue;', '        if(khl_lv.cand_bound[c]) continue;'),
    'rider-two': ('    if(!khl_ch[k].obj || khl_ch[k].rider || khl_ch[k].removed) return;', '    if(!khl_ch[k].obj || khl_ch[k].removed) return;'),
    'rider-wrong-class': ('#define KHL_RIDER_CLASS 8u', '#define KHL_RIDER_CLASS 0x80u'),
    'rider-class-hi-kept': ('    WORD_AT(a+KHL_AI_CLASS_HI)=0;WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;\n    WORD_AT(a+KHL_AI_ASK_HI)', '    WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;\n    WORD_AT(a+KHL_AI_ASK_HI)'),
    'rider-no-state': ("    WORD_AT(a+KHL_AI_NEXT)=0;                             /* the handler's state 0: sit down */\n}", '}'),
    'rider-bind-no-class': ('    khl_rider_class(a);\n    khl_lv.cand_bound', '    khl_lv.cand_bound'),
    'rider-not-after-chair': ('            for(k=0;k<khl_lv.n;k++) khl_bind_rider(k);\n', ''),
    'rider-not-at-chair': ('khl_seat_own(k,a,ent);   /* made as Khallos (create) */\n    khl_bind_rider(k);\n}', 'khl_seat_own(k,a,ent);   /* made as Khallos (create) */\n}'),
    'rider-not-removed': ('    khl_unfocus(ent);\n    guest_call(sub_0015DDA0,1,ent);\n    khl_log("rider removed', '    khl_unfocus(ent);\n    khl_log("rider removed'),
    'rider-remove-unchecked': ('    if(!khl_same_ai(a,ent,khl_ch[k].rider_rec)) { khl_log("the rider is already gone",a,ent);return; }\n', ''),
    'remove-never': ('            guest_call(sub_0015DDA0,1,ent);\n            khl_log("chair placeholder AI removed', '            khl_log("chair placeholder AI removed'),
    'remove-every-frame': ('        a=khl_ch[k].place;ent=khl_ch[k].place_ent;khl_ch[k].place=0;', '        a=khl_ch[k].place;ent=khl_ch[k].place_ent;'),
    'remove-unchecked': ('        if(khl_same_ai(a,ent,khl_ch[k].place_rec)) {', '        if(1) {'),
    'remove-focus-kept': ('    if(MAPMAKER_PTR_OK(player,KHL_FOCUS+4u) && WORD_AT(player+KHL_FOCUS)==ent) WORD_AT(player+KHL_FOCUS)=0;', '    (void)player;(void)ent;'),
    'same-ai-no-record': ('    return MAPMAKER_PTR_OK(a,KHL_ACTOR) && WORD_AT(a+KHL_ACTOR_RECORD)==rec && WORD_AT(a)==ent &&', '    return MAPMAKER_PTR_OK(a,KHL_ACTOR) && (rec|1u) && WORD_AT(a)==ent &&'),
    'wake-stock-line': ('        WORD_AT(KHL_LINE)=KHL_FLT_LOW;\n', ''),
    'wake-always': ('        if(dx*dx+dz*dz<=KHL_WAKE*KHL_WAKE) return 1;', '        return 1;'),
    'wake-dead-player': ('|| !(khl_getf(ent+KHL_ENT_HEALTH)>0.0f)) continue;', ') continue;'),
    'wake-x-only': ('        if(dx*dx+dz*dz<=KHL_WAKE*KHL_WAKE) return 1;', '        if(dx*dx<=KHL_WAKE*KHL_WAKE) return 1;'),
    'wake-first-player-only': ('    for(k=0;k<n && MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u);k++) {\n        ent=', '    for(k=0;k<n && k<1u && MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u);k++) {\n        ent='),
    'wake-line-not-forced': ('    WORD_AT(KHL_LINE)=KHL_FLT_MAX;\n    khl_setf(KHL_GAP,px-tx);', '    khl_setf(KHL_GAP,px-tx);'),
    'line-not-restored': ('    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;\n    khl_rider_health', '    WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;\n    khl_rider_health'),
    'line-wait-not-restored': ('        original();\n        WORD_AT(KHL_LINE)=line;\n        khl_rider_health(k,obj);\n        khl_keep_class(k);\n        return;', '        original();\n        khl_rider_health(k,obj);\n        khl_keep_class(k);\n        return;'),
    'frame-none': ('    khl_shift_in(obj);\n    khl_lv.inside=1;', '    khl_lv.inside=1;'),
    'frame-not-back': ('    khl_lv.inside=0;\n    khl_shift_out(obj);', '    khl_lv.inside=0;'),
    'frame-no-lift': ('    khl_lv.d[1]=khl_ch[k].spawn[1]+KHL_LIFT-KHL_TRAIN_Y;', '    khl_lv.d[1]=khl_ch[k].spawn[1]-KHL_TRAIN_Y;'),
    'frame-no-y': ('    khl_lv.d[1]=khl_ch[k].spawn[1]+KHL_LIFT-KHL_TRAIN_Y;', '    khl_lv.d[1]=0.0f;'),
    'frame-z-spawn': ('    khl_lv.d[2]=khl_clamp(khl_getf(p+KHL_P_POS+8u),khl_ch[k].spawn[2]-KHL_ROAM,khl_ch[k].spawn[2]+KHL_ROAM);', '    khl_lv.d[2]=khl_ch[k].spawn[2];'),
    'frame-z-unclamped': ('    khl_lv.d[2]=khl_clamp(khl_getf(p+KHL_P_POS+8u),khl_ch[k].spawn[2]-KHL_ROAM,khl_ch[k].spawn[2]+KHL_ROAM);', '    khl_lv.d[2]=khl_getf(p+KHL_P_POS+8u);'),
    'gap-unclamped': ('    tx=khl_clamp(px-khl_getf(KHL_GAP),khl_ch[k].spawn[0]-KHL_ROAM,khl_ch[k].spawn[0]+KHL_ROAM);', '    tx=px-khl_getf(KHL_GAP);'),
    'gap-stock': ('    khl_setf(KHL_GAP,px-tx);\n', ''),
    'gap-not-restored': ('    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;\n    khl_rider_health', '    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_SPAN)=span;\n    khl_rider_health'),
    'roam-small': ('#define KHL_ROAM 12.0f', '#define KHL_ROAM 8.0f'),
    'clamp-swapped': ('static float khl_clamp(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }', 'static float khl_clamp(float v,float lo,float hi) { return v<lo?hi:v>hi?lo:v; }'),
    'steer-first-always': ('       khl_getf(base+KHL_P_LINE)>khl_getf(base+KHL_PLAYER_STRIDE+KHL_P_LINE)) return base+KHL_PLAYER_STRIDE;', '       0) return base+KHL_PLAYER_STRIDE;'),
    'steer-any-count': ('    if(KHL_B(KHL_MODE)==10u && WORD_AT(KHL_PLAYERS)>1u &&', '    if(KHL_B(KHL_MODE)==10u &&'),
    'steer-reversed': ('khl_getf(base+KHL_P_LINE)>khl_getf(base+KHL_PLAYER_STRIDE+KHL_P_LINE)', 'khl_getf(base+KHL_P_LINE)<khl_getf(base+KHL_PLAYER_STRIDE+KHL_P_LINE)'),
    'players-not-moved': ('            khl_setf(p,khl_getf(p)-khl_lv.d[1u+i]);\n', ''),
    'players-recomputed': ('        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pworld[k][i];\n}', '        for(i=0;i<2u;i++) khl_setf(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u,khl_getf(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)+khl_lv.d[1u+i]);\n}'),
    'players-first-only': ('    if(n>4u) n=4u;\n    if(!MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u)) n=0;', '    if(n>1u) n=1u;\n    if(!MAPMAKER_PTR_OK(base,KHL_PLAYER_STRIDE*4u)) n=0;'),
    'object-not-moved': ('        khl_setf(obj+KHL_ENT_POS+i*4u,khl_getf(obj+KHL_ENT_POS+i*4u)-khl_lv.d[i]);\n', ''),
    'live-not-moved': ('        khl_setf(KHL_POS+i*4u,khl_getf(KHL_POS+i*4u)-khl_lv.d[i]);\n', ''),
    'inside-kept': ('    original();\n    khl_lv.inside=0;\n    khl_shift_out(obj);', '    original();\n    khl_shift_out(obj);'),
    'world-pos-not-moved': ('    if(has) for(i=0;i<3u;i++) { keep[i]=WORD_AT(pos+i*4u);khl_setf(pos+i*4u,khl_getf(pos+i*4u)+khl_lv.d[i]); }', '    if(has) for(i=0;i<3u;i++) { keep[i]=WORD_AT(pos+i*4u); }'),
    'world-pos-not-back': ('    if(has) for(i=0;i<3u;i++) WORD_AT(pos+i*4u)=keep[i];\n', ''),
    'world-players-moved': ('    for(k=0;k<khl_lv.np;k++)\n        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pworld[k][i];\n    if(has)', '    if(has)'),
    'world-players-not-back': ('        for(i=0;i<2u;i++) WORD_AT(base+k*KHL_PLAYER_STRIDE+KHL_P_POS+4u+i*4u)=khl_lv.pmoved[k][i];\n', ''),
    'world-self-moved': ('        khl_setf(obj+KHL_ENT_POS+4u+i*4u,khl_getf(obj+KHL_ENT_POS+4u+i*4u)+khl_lv.d[1u+i]);\n', ''),
    'world-live-moved': ('        khl_setf(KHL_POS+4u+i*4u,khl_getf(KHL_POS+4u+i*4u)+khl_lv.d[1u+i]);\n', ''),
    'world-self-not-back': ('    for(i=0;i<2u;i++) { WORD_AT(KHL_POS+4u+i*4u)=khl_lv.self[i];WORD_AT(obj+KHL_ENT_POS+4u+i*4u)=khl_lv.self[2u+i]; }\n', ''),
    'world-inside-flag': ('    khl_lv.inside=0;\n    original();\n    khl_lv.inside=1;', '    original();'),
    'flame-unshifted': ('    khl_world_call(original,WORD_AT(g_esp+8u));\n}\nvoid pc_mapmaker_khallos_flash', '    original();\n}\nvoid pc_mapmaker_khallos_flash'),
    'flame-any-caller': ('|| WORD_AT(g_esp)!=KHL_RET_FLAME) { original();return; }', ') { original();return; }'),
    'flame-outside': ('    if(!khl_lv.inside || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_FLAME)', '    if(!MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_FLAME)'),
    'flash-wrong-arg': ('    khl_world_call(original,WORD_AT(g_esp+12u));', '    khl_world_call(original,WORD_AT(g_esp+8u));'),
    'flash-gun1-only': ('(WORD_AT(g_esp)!=KHL_RET_FLASH && WORD_AT(g_esp)!=KHL_RET_FLASH2)', '(WORD_AT(g_esp)!=KHL_RET_FLASH)'),
    'shot-unshifted': ('    if(!khl_lv.inside || !MAPMAKER_STACK_OK(12) || WORD_AT(g_esp)!=KHL_RET_SHOT) { original();return; }\n    khl_world_call(original,WORD_AT(g_esp+8u));', '    original();'),
    'shot-any-caller': ('|| WORD_AT(g_esp)!=KHL_RET_SHOT) { original();return; }', ') { original();return; }'),
    'sound-wrong-offset': ('#define KHL_SOUND_POS 0x1Cu', '#define KHL_SOUND_POS 0x18u'),
    'sound-unshifted': ('    khl_world_call(original,MAPMAKER_PTR_OK(rec,KHL_SOUND_POS+12u)?rec+KHL_SOUND_POS:0u);', '    (void)rec;original();'),
    'engine-unshifted': ('    khl_world_call(original,WORD_AT(g_esp+8u));\n    khl_ch[khl_lv.cur].engine++;', '    original();\n    khl_ch[khl_lv.cur].engine++;'),
    'engine-any-caller': ('|| WORD_AT(g_esp)!=KHL_RET_ENGINE) { original();return; }', ') { original();return; }'),
    'damage-stock': ('    khl_setf(r+KHL_REC_DAMAGE,h);                          /* the stock code leaves him at exactly 0 */\n', ''),
    'damage-not-restored': ('    WORD_AT(r+KHL_REC_DAMAGE)=bits;\n', ''),
    'damage-health-zero': ('    khl_setf(obj+KHL_ENT_HEALTH,h-dmg);\n', ''),
    'damage-no-stops': ('    for(i=0;i<3u;i++) guest_call(sub_0010EB90,2,WORD_AT(KHL_SMOKE+i*4u),0u);\n', ''),
    'damage-two-stops': ('    for(i=0;i<3u;i++) guest_call(sub_0010EB90', '    for(i=0;i<2u;i++) guest_call(sub_0010EB90'),
    'damage-focus-kept': ('    khl_unfocus(obj);\n    khl_log("Khallos\' chair shot down', '    khl_log("Khallos\' chair shot down'),
    'damage-inactive': ('    if(WORD_AT(KHL_ACTIVE)!=1u || obj!=khl_ch[k].obj ||', '    if(obj!=khl_ch[k].obj ||'),
    'damage-no-direction': ('|| !WORD_AT(r+KHL_REC_DIR) || !WORD_AT(r+KHL_REC_HIT) ||', '||'),
    'damage-dead-again': ('       !((h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) || ', '       ((h=khl_getf(obj+KHL_ENT_HEALTH))!=h) || '),
    'damage-every-hit': ('|| !(h-(dmg=khl_getf(r+KHL_REC_DAMAGE))<0.0f)) { original();return; }', '|| ((dmg=khl_getf(r+KHL_REC_DAMAGE))!=dmg)) { original();return; }'),
    'damage-any-chair': ('|| (k=khl_chair(WORD_AT(r)))<0 || khl_ch[k].removed) { original();return; }', '|| (k=khl_lv.n?0:-1)<0 || khl_ch[k].removed) { original();return; }'),
    'removal-never': ('    if(++khl_ch[k].dying>=KHL_GONE_FRAMES || khl_getf(KHL_POS+4u)>khl_ch[k].spawn[1]+KHL_GONE_UP) khl_remove(k,obj);', '    ++khl_ch[k].dying;'),
    'removal-no-height': ('    if(++khl_ch[k].dying>=KHL_GONE_FRAMES || khl_getf(KHL_POS+4u)>khl_ch[k].spawn[1]+KHL_GONE_UP) khl_remove(k,obj);', '    if(++khl_ch[k].dying>=KHL_GONE_FRAMES) khl_remove(k,obj);'),
    'removal-no-time': ('    if(++khl_ch[k].dying>=KHL_GONE_FRAMES || khl_getf(KHL_POS+4u)>khl_ch[k].spawn[1]+KHL_GONE_UP) khl_remove(k,obj);', '    if(++khl_ch[k].dying && khl_getf(KHL_POS+4u)>khl_ch[k].spawn[1]+KHL_GONE_UP) khl_remove(k,obj);'),
    'removal-alive': ('    if(!(khl_getf(obj+KHL_ENT_HEALTH)<0.0f)) return;\n', ''),
    'removal-early': ('#define KHL_GONE_UP 25.0f', '#define KHL_GONE_UP 20.0f'),
    'removal-not-destroyed': ('    guest_call(sub_000A9070,1,obj);\n    if(jet)', '    if(jet)'),
    'removal-jet-kept': ('    if(jet) { guest_call(sub_0010E2A0,1,jet);WORD_AT(KHL_JET)=0; }\n', ''),
    'removal-state-kept': ('    if(WORD_AT(KHL_OBJ)==obj) WORD_AT(KHL_OBJ)=0;\n', ''),
    'removal-runs-on': ('    if(khl_ch[k].removed) { MAPMAKER_GUEST_RET();return; }   /* removed: nothing more to run */\n', ''),
    'removal-focus-kept': ('    khl_unfocus(obj);\n    guest_call(sub_000A9070,1,obj);', '    guest_call(sub_000A9070,1,obj);'),
    'removal-keeps-rider': ('    khl_log("the dead chair flew off and was removed (chair, frames)",k,khl_ch[k].dying);\n    khl_drop_rider(k);', '    khl_log("the dead chair flew off and was removed (chair, frames)",k,khl_ch[k].dying);'),
    'event-kept': ('    if(khl_lv.events++<6u) khl_log("chair level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),ret);\n    MAPMAKER_GUEST_RET();', '    if(khl_lv.events++<6u) khl_log("chair level event dropped (MapMaker logic marker untouched)",WORD_AT(g_esp+4u),ret);\n    original();'),
    'event-any-caller': ('    if((ret!=KHL_RET_START && ret!=KHL_RET_HURT && ret!=KHL_RET_LOW && ret!=KHL_RET_DEAD) ||\n       WORD_AT(KHL_SCENE)!=100u', '    if(WORD_AT(KHL_SCENE)!=100u'),
    'event-no-start': ('(ret!=KHL_RET_START && ret!=KHL_RET_HURT', '(ret!=KHL_RET_HURT'),
    'event-no-hurt': ('ret!=KHL_RET_START && ret!=KHL_RET_HURT && ret!=KHL_RET_LOW', 'ret!=KHL_RET_START && ret!=KHL_RET_LOW'),
    'event-no-low': ('ret!=KHL_RET_HURT && ret!=KHL_RET_LOW && ret!=KHL_RET_DEAD', 'ret!=KHL_RET_HURT && ret!=KHL_RET_DEAD'),
    'event-no-dead': ('ret!=KHL_RET_LOW && ret!=KHL_RET_DEAD)', 'ret!=KHL_RET_LOW)'),
    'event-any-scene': ('       WORD_AT(KHL_SCENE)!=100u || !khl_on()) { original();return; }', '       !khl_on()) { original();return; }'),
    'text-any-caller': ('WORD_AT(g_esp+4u)!=KHL_BAR_TEXT || WORD_AT(g_esp)!=KHL_RET_BAR_TEXT ||', 'WORD_AT(g_esp+4u)!=KHL_BAR_TEXT ||'),
    'text-any-mode': ('|| !khl.label || !khl_play()) {', '|| !khl.label || !khl_on()) {'),
    'text-no-ret': ('    g_eax=khl.label;\n    MAPMAKER_GUEST_RET();', '    g_eax=khl.label;'),
    'mine-first-only': ('    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].obj==obj) return (int)k;', '    for(k=0;k<1u && k<khl_lv.n;k++) if(khl_ch[k].obj==obj) return (int)k;'),
    'mount-keeps-count': ('    memset(&khl_lv,0,sizeof khl_lv);                       /* forget the last chairs */\n', ''),
    'spawn-one-chair': ('    if(khl_lv.n>=KHL_MAX) {', '    if(khl_lv.n>=1u) {'),
    'spawn-unlimited': ('    if(khl_lv.n>=KHL_MAX) {', '    if(khl_lv.n>=KHL_MAX+1u) {'),
    'spawn-same-placeholder-twice': ('    if(k<khl_lv.n) { khl_log("this chair placeholder already has its chair",a,k);return; }\n', ''),
    'spawn-globals-not-scratch': ("    khl_in(k,keep);                                       /* the init writes the new chair's globals */\n", '    memcpy(keep,(void *)((uintptr_t)KHL_OBJ+g_xbox_mem_offset),4u*KHL_BLOCK_N);memcpy(keep+KHL_BLOCK_N,(void *)((uintptr_t)KHL_SMOKE+g_xbox_mem_offset),4u*KHL_SMOKE_N);\n'),
    'spawn-chair-not-kept': ('    khl_out(k,keep);\n    khl_ch[k].obj=obj;', '    memcpy((void *)((uintptr_t)KHL_OBJ+g_xbox_mem_offset),keep,4u*KHL_BLOCK_N);\n    khl_ch[k].obj=obj;'),
    'spawn-failed-not-back': ('        khl_out(k,keep);khl_detach(k);\n        khl_log("the chair was not created', '        khl_detach(k);\n        khl_log("the chair was not created'),
    'variant-not-back': ('        guest_call(sub_000A9070,1,obj);\n        khl_out(k,keep);', '        guest_call(sub_000A9070,1,obj);'),
    'rider-two-chairs': ('        if(khl_lv.cand_bound[c] || !khl_same_ai(khl_lv.cand[c],khl_lv.cand_ent[c],khl_lv.cand_rec[c])) continue;', '        if(!khl_same_ai(khl_lv.cand[c],khl_lv.cand_ent[c],khl_lv.cand_rec[c])) continue;'),
    'rider-not-swapped': ('    khl_swapped(original,(unsigned)k);\n    khl_idle((unsigned)k);\n}\n/* WRAP sub_00263150', '    original();\n    khl_idle((unsigned)k);\n}\n/* WRAP sub_00263150'),
    'rider-any-entity': ('    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].rider && khl_ch[k].rider_ent==ent) return (int)k;', '    for(k=0;k<khl_lv.n;k++) if(khl_ch[k].rider) return (int)k;'),
    'frame-first-chair-spawn': ('    khl_lv.d[1]=khl_ch[k].spawn[1]+KHL_LIFT-KHL_TRAIN_Y;', '    khl_lv.d[1]=khl_ch[0].spawn[1]+KHL_LIFT-KHL_TRAIN_Y;'),
    'gap-first-chair': ('    tx=khl_clamp(px-khl_getf(KHL_GAP),khl_ch[k].spawn[0]-KHL_ROAM,khl_ch[k].spawn[0]+KHL_ROAM);', '    tx=khl_clamp(px-khl_getf(KHL_GAP),khl_ch[0].spawn[0]-KHL_ROAM,khl_ch[0].spawn[0]+KHL_ROAM);'),
    'damage-not-swapped': ('    khl_in((unsigned)k,keep);\n    prev=khl_lv.cur;khl_lv.cur=k;\n    active=', '    prev=khl_lv.cur;khl_lv.cur=k;\n    active='),
    'damage-not-back': ('    khl_hit(original,(unsigned)k,r);\n    khl_lv.cur=prev;\n    khl_out((unsigned)k,keep);', '    khl_hit(original,(unsigned)k,r);\n    khl_lv.cur=prev;(void)keep;'),
    'update-not-swapped': ('    khl_in((unsigned)k,keep);\n    prev=khl_lv.cur;khl_lv.cur=k;\n    khl_frame(', '    prev=khl_lv.cur;khl_lv.cur=k;\n    khl_frame('),
    'update-not-back': ('    khl_frame(original,(unsigned)k,obj);\n    khl_lv.cur=prev;\n    khl_out((unsigned)k,keep);', '    khl_frame(original,(unsigned)k,obj);\n    khl_lv.cur=prev;(void)keep;'),
    'swap-no-smoke': ('#define KHL_SMOKE_N 3u', '#define KHL_SMOKE_N 1u'),
    'swap-short-block': ('#define KHL_BLOCK_N 0x1Cu', '#define KHL_BLOCK_N 0x16u'),
    'swap-smoke-not-kept': ('    for(i=0;i<KHL_SMOKE_N;i++) { khl_ch[k].smoke[i]=WORD_AT(KHL_SMOKE+i*4u);WORD_AT(KHL_SMOKE+i*4u)=keep[KHL_BLOCK_N+i]; }', '    for(i=0;i<KHL_SMOKE_N;i++) { WORD_AT(KHL_SMOKE+i*4u)=keep[KHL_BLOCK_N+i]; }'),
    'swap-block-not-kept': ('    for(i=0;i<KHL_BLOCK_N;i++) { khl_ch[k].block[i]=WORD_AT(KHL_OBJ+i*4u);WORD_AT(KHL_OBJ+i*4u)=keep[i]; }', '    for(i=0;i<KHL_BLOCK_N;i++) { WORD_AT(KHL_OBJ+i*4u)=keep[i]; }'),
    'swap-cur-kept': ('    original();\n    khl_lv.cur=prev;\n    khl_out(k,keep);', '    original();\n    khl_out(k,keep);'),
    'world-chair-zero': ('obj=khl_ch[khl_lv.cur].obj,k,i,keep[3]={0};', 'obj=khl_ch[0].obj,k,i,keep[3]={0};'),
    'removal-first-chair': ('    khl_ch[k].removed=1;\n    khl_log("the dead chair', '    khl_ch[0].removed=1;\n    khl_log("the dead chair'),
    'create-any-id': ('    if((id!=KHL_ID && id!=KHL_RIDER_ID) || khl_lv.creates>=KHL_CREATES || !khl_play()) return;', '    if(khl_lv.creates>=KHL_CREATES || !khl_play()) return;'),
    'create-before-original': ('    } else original();\n    if((id!=KHL_ID', '    } else { original();g_esp-=4u;original(); }\n    if((id!=KHL_ID'),
    'create-any-scene': ('khl_lv.creates>=KHL_CREATES || !khl_play()) return;', 'khl_lv.creates>=KHL_CREATES || !khl_on()) return;'),
    'seen-not-logged': ('    if(id==KHL_ID && khl_lv.seen++<KHL_CREATES) {', '    if(0 && id==KHL_ID && khl_lv.seen++<KHL_CREATES) {'),
    'pose-not-swapped': ('    khl_swapped(original,(unsigned)k);\n}\n\n/* WRAP sub_0014CC80', '    original();\n}\n\n/* WRAP sub_0014CC80'),
    'owner-not-loaded': ('        WORD_AT(khl_owners[i])=e=WORD_AT(g);\n        khl_reown(e,g,khl_owners[i]);', '        e=WORD_AT(g);(void)e;'),
    'owner-not-moved-in': ('        WORD_AT(khl_owners[i])=e=WORD_AT(g);\n        khl_reown(e,g,khl_owners[i]);', '        WORD_AT(khl_owners[i])=e=WORD_AT(g);'),
    'owner-not-saved': ('        WORD_AT(g)=e=WORD_AT(khl_owners[i]);\n        khl_reown(e,khl_owners[i],g);', '        e=WORD_AT(khl_owners[i]);\n        khl_reown(e,khl_owners[i],g);'),
    'owner-not-moved-out': ('        WORD_AT(g)=e=WORD_AT(khl_owners[i]);\n        khl_reown(e,khl_owners[i],g);', '        WORD_AT(g)=e=WORD_AT(khl_owners[i]);'),
    'owner-any': ('    if(e && MAPMAKER_PTR_OK(e,4u) && WORD_AT(e+KHL_EMIT_OWNER)==from) WORD_AT(e+KHL_EMIT_OWNER)=to;', '    if(e && MAPMAKER_PTR_OK(e,4u)) WORD_AT(e+KHL_EMIT_OWNER)=to;'),
    'owner-jet-only': ('={0x0078F054u,0x0078DF50u};', '={0x0078F054u,0x0078F054u};'),
    'owner-slots-shared': ('static uint32_t khl_gslot(unsigned k,unsigned i) { return khl.slots?khl.slots+(k*KHL_OWNERS+i)*4u:0u; }', 'static uint32_t khl_gslot(unsigned k,unsigned i) { (void)k;return khl.slots?khl.slots+i*4u:0u; }'),
    'owner-slots-none': ('        if(s && MAPMAKER_PTR_OK(s,KHL_MAX*KHL_OWNERS*4u)) khl.slots=s;', '        (void)s;'),
    'owner-slots-stale': ('    if(khl.slots) memset((void *)((uintptr_t)khl.slots+g_xbox_mem_offset),0,KHL_MAX*KHL_OWNERS*4u);\n', ''),
    'detach-missing': ('        khl_out(k,keep);khl_detach(k);\n        khl_log("the chair took', '        khl_out(k,keep);\n        khl_log("the chair took'),
    # the seated Khallos is the placeholder itself (v0.4.0-mapmaker.27: not visible)
    'request-no-rider-models': ('        if((m=khl_rider_model(i))<KHL_MODEL_MAX) guest_call(sub_000A5E40,1,m);', '        if((m=khl_rider_model(i))<KHL_MODEL_MAX) (void)m;'),
    'request-rider-models-always': ('    if(khl_map_has(KHL_RIDER_ID)==1) return;              /* the set\'s preload', '    if(0) return;              /* the set\'s preload'),
    'request-rider-no-lod': ('    for(i=0;i<KHL_CHAR_MODELS;i++)                         /* the seated', '    for(i=0;i<1u;i++)                         /* the seated'),
    'create-no-swap': ('{ keep[i]=WORD_AT(c+i*4u);WORD_AT(c+i*4u)=khl_rider_model(i); }', '{ keep[i]=WORD_AT(c+i*4u); }'),
    'create-swap-kept': ('        for(i=0;i<KHL_CHAR_MODELS;i++) WORD_AT(c+i*4u)=keep[i];\n', ''),
    'create-swap-model-only': ('        for(i=0;i<KHL_CHAR_MODELS;i++) { keep[i]=', '        for(i=0;i<1u;i++) { keep[i]='),
    'create-swap-wrong-id': ('        c=KHL_CHARS+KHL_ID*KHL_CHAR_STRIDE+KHL_CHAR_MODEL;', '        c=KHL_CHARS+0x34u*KHL_CHAR_STRIDE+KHL_CHAR_MODEL;'),
    'create-any-id-swapped': ('    if(id==KHL_ID && khl_play() && !khl.optout && khl_lv.n<KHL_MAX', '    if(khl_play() && !khl.optout && khl_lv.n<KHL_MAX'),
    'create-without-anims': ('khl_lv.n<KHL_MAX && khl_mounted(KHL_ANIM_PAK) && khl_chair_ready()', 'khl_lv.n<KHL_MAX && khl_chair_ready()'),
    'create-without-chair': ('khl_mounted(KHL_ANIM_PAK) && khl_chair_ready() &&\n', 'khl_mounted(KHL_ANIM_PAK) &&\n'),
    'create-unread-model': ('       khl_rider_ready()) {', '       1) {'),
    'create-unread-lod': ('        if((m=khl_rider_model(i))==0xFFFFFFFFu && i) continue;', '        if((m=khl_rider_model(i))==0xFFFFFFFFu || i) continue;'),
    'create-optout': ('    if(id==KHL_ID && khl_play() && !khl.optout && khl_lv.n<KHL_MAX', '    if(id==KHL_ID && khl_play() && khl_lv.n<KHL_MAX'),
    'create-fifth': ('    if(id==KHL_ID && khl_play() && !khl.optout && khl_lv.n<KHL_MAX', '    if(id==KHL_ID && khl_play() && !khl.optout'),
    'own-never': ('KHL_H(ent+KHL_ENT_MODEL)==m) khl_seat_own(k,a,ent);', 'KHL_H(ent+KHL_ENT_MODEL)==m+1u) khl_seat_own(k,a,ent);'),
    'own-any-model': ('KHL_H(ent+KHL_ENT_MODEL)==m) khl_seat_own(k,a,ent);', 'KHL_H(ent+KHL_ENT_MODEL)!=0xFFFFu) khl_seat_own(k,a,ent);'),
    'own-without-anims': ('    if(!khl_mounted(KHL_ANIM_PAK)) { khl_log("lv11.pak not mounted: the Khallos placeholder', '    if(0) { khl_log("lv11.pak not mounted: the Khallos placeholder'),
    'own-class-kept': ('    khl_rider_class(a);\n    khl_ch[k].rider=a;', '    khl_ch[k].rider=a;'),
    'own-removed': ('    if(khl_ch[k].place && !khl_ch[k].own) {', '    if(khl_ch[k].place) {'),
    'own-wrong-record': ('khl_ch[k].rider_rec=khl_ch[k].place_rec;khl_ch[k].own=1;', 'khl_ch[k].rider_rec=0;khl_ch[k].own=1;'),
    'rider-hits-ignored': ('        khl_setf(obj+KHL_ENT_HEALTH,left=h-(max-(cur>0.0f?cur:0.0f)));', '        left=h;'),
    'rider-hits-while-waiting': ('    if(WORD_AT(KHL_ACTIVE)==1u && (h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) {', '    if((h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) {'),
    'rider-hits-dead-chair': ('    if(WORD_AT(KHL_ACTIVE)==1u && (h=khl_getf(obj+KHL_ENT_HEALTH))>=0.0f) {', '    if(WORD_AT(KHL_ACTIVE)==1u && ((h=khl_getf(obj+KHL_ENT_HEALTH))==h)) {'),
    'rider-hits-overkill': ('left=h-(max-(cur>0.0f?cur:0.0f))', 'left=h-(max-cur)'),
    'rider-not-topped': ('    if(cur>0.0f) khl_setf(ent+KHL_ENT_HEALTH,max);\n', ''),
    'rider-revived': ('    if(cur>0.0f) khl_setf(ent+KHL_ENT_HEALTH,max);', '    if(1) khl_setf(ent+KHL_ENT_HEALTH,max);'),
    'rider-kill-no-down': ('        if(left<0.0f) khl_down(k,obj);\n', ''),
    'rider-health-waiting-never': ('        WORD_AT(KHL_LINE)=line;\n        khl_rider_health(k,obj);\n        khl_keep_class(k);\n        return;', '        WORD_AT(KHL_LINE)=line;\n        khl_keep_class(k);\n        return;'),
    'rider-health-active-never': ('    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;\n    khl_rider_health(k,obj);\n', '    WORD_AT(KHL_LINE)=line;WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;\n'),
    'hud-kept': ('    WORD_AT(player+KHL_FOCUS)=0;\n    original();\n    WORD_AT(player+KHL_FOCUS)=focus;', '    original();\n    WORD_AT(player+KHL_FOCUS)=focus;'),
    'hud-not-back': ('    WORD_AT(player+KHL_FOCUS)=0;\n    original();\n    WORD_AT(player+KHL_FOCUS)=focus;', '    WORD_AT(player+KHL_FOCUS)=0;\n    original();'),
    'hud-unseated': ('(k=khl_chair(focus=WORD_AT(player+KHL_FOCUS)))<0 || !khl_seated((unsigned)k)) { original();return; }', '(k=khl_chair(focus=WORD_AT(player+KHL_FOCUS)))<0) { original();return; }'),
    'hud-any-focus': ('(k=khl_chair(focus=WORD_AT(player+KHL_FOCUS)))<0 || !khl_seated((unsigned)k)) { original();return; }', '(k=0,focus=WORD_AT(player+KHL_FOCUS))<0 || !khl_seated((unsigned)k)) { original();return; }'),
    'seated-stale': ('static int khl_seated(unsigned k) { return khl_ch[k].rider && khl_same_ai(', 'static int khl_seated(unsigned k) { return khl_ch[k].rider || khl_same_ai('),
    'proxy-not-set': ('    bos_bar_proxy=khl_bar_entity;', '    (void)khl_bar_entity;'),
    'proxy-removed-chair': ('    return k>=0 && !khl_ch[k].removed?khl_ch[k].obj:0u;', '    return k>=0?khl_ch[k].obj:0u;'),
    'proxy-any-entity': ('    int k=khl_ridden(ent);\n    return k>=0', '    int k=khl_lv.n?0:-1;(void)ent;\n    return k>=0'),
    # v0.4.0-mapmaker.28: orientation, the seat, the sounds
    'yaw-clamped': ('    khl_setf(KHL_SPAN,KHL_SPAN_FREE);', '    (void)KHL_SPAN_FREE;'),
    'yaw-not-restored': ('WORD_AT(KHL_GAP)=gap;WORD_AT(KHL_SPAN)=span;', 'WORD_AT(KHL_GAP)=gap;(void)span;'),
    'seat-never': ('    khl_seat(k,obj);\n', ''),
    'seat-chair-0': ('    uint32_t ent=khl_ch[k].rider_ent,i;\n    float c,want[3];', '    uint32_t ent=khl_ch[0].rider_ent,i;\n    float c,want[3];'),
    'seat-unrotated': ('+c*khl_getf(obj+KHL_ENT_ROT+0x10u+i*4u)', '+(i==1u?c:0.0f)'),
    'seat-wrong-row': ('+c*khl_getf(obj+KHL_ENT_ROT+0x10u+i*4u)', '+c*khl_getf(obj+KHL_ENT_ROT+0x20u+i*4u)'),
    'seat-no-angles': ('    WORD_AT(ent+KHL_ENT_YAW)=WORD_AT(ent+KHL_ENT_YAW2)=WORD_AT(obj+KHL_ENT_YAW);\n', ''),
    'seat-no-tilt': ('    WORD_AT(ent+KHL_ENT_PITCH)=WORD_AT(obj+KHL_ENT_PITCH);WORD_AT(ent+KHL_ENT_ROLL)=WORD_AT(obj+KHL_ENT_ROLL);\n', ''),
    'seat-dead-rider': (' || !(khl_getf(ent+KHL_ENT_HEALTH)>0.0f)) return;\n    if(!khl_sane(c=', ') return;\n    if(!khl_sane(c='),
    'seat-stock-only': ('    if(!khl_sane(c=khl_getf(KHL_SEAT))) c=KHL_SEAT_STOCK;', '    c=KHL_SEAT_STOCK;'),
    'seat-no-fallback': ('    if(!khl_sane(c=khl_getf(KHL_SEAT))) c=KHL_SEAT_STOCK;', '    c=khl_getf(KHL_SEAT);if(c!=c) return;'),
    'seat-offset-unkept': ('khl_ch[k].off[i]=khl_getf(ent+KHL_ENT_POS+i*4u)-want[i];', ''),
    'seat-class-kept': ('    WORD_AT(a+KHL_AI_ASK_HI)=0;WORD_AT(a+KHL_AI_ASK)=KHL_RIDER_CLASS;WORD_AT(a+KHL_AI_NEXT)=0;\n}', '}'),
    'rider-state-kept': ('        WORD_AT(a+KHL_AI_NEXT)=0;                          /* states 2+', '        (void)0;                          /* states 2+'),
    'rider-state-sitting': ('((int32_t)next>1 ||', '((int32_t)next>0 ||'),
    'rider-state-seated': ('WORD_AT(a+KHL_AI_STATE)>1u))) {', 'WORD_AT(a+KHL_AI_STATE)>0u))) {'),
    'rider-uncounted': ('    khl_ch[k].calls++;\n', ''),
    'sound-load-never': ('    g_eax=KHL_SND_LOAD;\n', ''),
    'sound-load-any-id': ('(id!=KHL_SND_GUN && id!=KHL_SND_JET) || ', ''),
    'sound-load-gun-only': ('(id!=KHL_SND_GUN && id!=KHL_SND_JET) || ', '(id!=KHL_SND_GUN) || '),
    'sound-load-overrides': ('    if(g_eax || !khl_lv.sounds', '    if(!khl_lv.sounds'),
    'sound-load-any-map': ('    if(!khl_play() || khl.optout || (has=khl_map_has(KHL_ID))==0) return;\n    khl_lv.sounds=1;', '    khl_lv.sounds=1;\n    if(!khl_play() || khl.optout || (has=khl_map_has(KHL_ID))==0) return;'),
    'sound-load-any-scene': (' || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return;\n    g_eax=KHL_SND_LOAD;', ' || !khl_on()) return;\n    g_eax=KHL_SND_LOAD;'),
    'sound-load-when-off': (' || WORD_AT(KHL_SCENE)!=100u || !khl_on()) return;\n    g_eax=KHL_SND_LOAD;', ' || WORD_AT(KHL_SCENE)!=100u) return;\n    g_eax=KHL_SND_LOAD;'),
    'shots-all-failed': ('    if((int32_t)g_eax<0) khl_ch[khl_lv.cur].shot_fail++;', '    khl_ch[khl_lv.cur].shot_fail++;'),
    'engine-all-playing': ('(int32_t)WORD_AT(handle)<0) khl_ch[khl_lv.cur].engine_off++;', '(int32_t)WORD_AT(handle)<-1) khl_ch[khl_lv.cur].engine_off++;'),
    'status-every-frame': ('(++khl_ch[k].frames!=1u && khl_ch[k].frames%KHL_STATUS_EVERY)) return;', '(++khl_ch[k].frames!=1u && 0)) return;'),
    'rider-class-not-asked': ('    WORD_AT(a+KHL_AI_ASK_HI)=0;WORD_AT(a+KHL_AI_ASK)=KHL_RIDER_CLASS;\n    WORD_AT(a+KHL_AI_NEXT)', '    WORD_AT(a+KHL_AI_NEXT)'),
    'seat-class-written': ('    WORD_AT(a+KHL_AI_ASK_HI)=0;WORD_AT(a+KHL_AI_ASK)=KHL_RIDER_CLASS;WORD_AT(a+KHL_AI_NEXT)=0;\n}', '    WORD_AT(a+KHL_AI_CLASS_HI)=0;WORD_AT(a+KHL_AI_CLASS)=KHL_RIDER_CLASS;WORD_AT(a+KHL_AI_NEXT)=0;\n}'),
    'seat-class-flying-only': ('        khl_rider_health(k,obj);\n        khl_keep_class(k);\n        return;', '        khl_rider_health(k,obj);\n        return;'),
    'seat-class-dead-too': ('|| !(khl_getf(khl_ch[k].rider_ent+KHL_ENT_HEALTH)>0.0f)) return;\n    if(!WORD_AT(a+KHL_AI_ASK_HI)', ') return;\n    if(!WORD_AT(a+KHL_AI_ASK_HI)'),
    'seat-class-always': ('    if(!WORD_AT(a+KHL_AI_ASK_HI) && WORD_AT(a+KHL_AI_ASK)==KHL_RIDER_CLASS) return;\n', ''),
    'dead-logged-each-frame': ('    else if(!khl_ch[k].dead++) khl_log(', '    else if(++khl_ch[k].dead) khl_log('),
    'idle-never': ('    r=guest_call(sub_0005F460,4,ent,KHL_IDLE,0u,KHL_ONE);', '    r=0;'),
    'idle-wrong-anim': ('guest_call(sub_0005F460,4,ent,KHL_IDLE,0u,KHL_ONE)', 'guest_call(sub_0005F460,4,ent,0xBFAu,0u,KHL_ONE)'),
    'idle-every-call': ('    if(khl_ch[k].wait++%KHL_ANIM_RETRY) return;', '    khl_ch[k].wait++;'),
    'idle-while-playing': ('    if(khl_ch[k].anim==KHL_IDLE) { khl_ch[k].wait=0;return; }', '    if(0) { khl_ch[k].wait=0;return; }'),
    'idle-while-sitting-down': ('    if(!khl_seated(k) || WORD_AT(a+KHL_AI_STATE)!=1u || !MAPMAKER_PTR_OK(ent,0x200u)', '    if(!khl_seated(k) || !MAPMAKER_PTR_OK(ent,0x200u)'),
    'idle-unseated': ('    if(!khl_seated(k) || WORD_AT(a+KHL_AI_STATE)!=1u', '    if(WORD_AT(a+KHL_AI_STATE)!=1u'),
    'restart-not-reset': ('    if(khl_on()) khl_reset(1);\n', ''),
    'restart-after-original': ('    if(khl_on()) khl_reset(1);\n    original();', '    original();\n    if(khl_on()) khl_reset(1);'),
    'restart-when-off': ('    if(khl_on()) khl_reset(1);', '    khl_reset(1);'),
    'restart-forgets-sounds': ('    if(restart) { khl_lv.sounds=sounds;khl_lv.requested=requested; }\n', ''),
    'restart-keeps-log': ('    khl.logged=0;                                          /* and log it afresh */\n', ''),
    'restart-keeps-slots': ('    if(khl.slots) memset((void *)((uintptr_t)khl.slots+g_xbox_mem_offset),0,KHL_MAX*KHL_OWNERS*4u);\n    if(khl_play()) fprintf', '    if(khl_play()) fprintf'),
}




def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('khallos.exe' if os.name == 'nt' else 'khallos')
    source = ROOT / 'tests/mapmaker/mapmaker_khallos.c'
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
def test_mapmaker_khallos(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_KHALLOS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker khallos ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only, and the map container is only read."""
    assert not (ROOT / 'config/guest-code-patches/khallos.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'KHL_[BH]\([^)]*\)\s*=[^=]', text)


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


def calls_to(fn):
    return {i.address for i in decoded() if i.call and target(i) == fn}


def refs(value):
    return {i.function for i in decoded() if any(v == value for _, v in i.values())}


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


def f32(rd, va):
    return struct.unpack('<f', rd(va, 4))[0]


def anim_table(rd, va, n):
    out = {}
    for k in range(n):
        e = rd(va + k * 0x18, 0x18)
        name = rd(struct.unpack_from('<I', e, 0)[0], 40).split(b'\0')[0].decode()
        out[name] = struct.unpack_from('<H', e, 0x10)[0]
    return out


UPDATE, DAMAGE, INIT, STATE_INIT, RISING = 0x001EEBD0, 0x001EF840, 0x001F4E00, 0x001EEA30, 0x001EFBA0


def test_strings_init_and_placeholder():
    rd = xbe_reader()
    assert rd(0x0049E1D0, 26) == b'pak/stream/lv11/prop3.pak\0'
    assert rd(0x0049D9C8, 30) == b'pak/stream/lv11/anim/lv11.pak\0'
    assert struct.unpack('<I', rd(0x004FCA60 + 30 * 4, 4))[0] == INIT          # init index 30
    # placeholder 0x33: the fifth copy of soldiermodel_talk (0x619), no Arcade row
    models = [struct.unpack('<I', rd(0x0053A3CC + i * 0x30 + 4, 4))[0] for i in range(0xB0)]
    assert [i for i in range(0xB0) if models[i] == 0x619] == [0x2F, 0x30, 0x31, 0x32, 0x33, 0x4B]
    arcade = {struct.unpack('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)}
    assert 0x33 not in arcade and 0x2 in arcade
    assert models[0x2] == 0x582                                                # Khallos: trainkhallos
    # no other boss port uses 0x33 as its placeholder
    for other in ('pc_mapmaker_tank.inc', 'pc_mapmaker_robot.inc', 'pc_mapmaker_realbosses.inc'):
        p = ROOT / 'src/interface' / other
        if p.exists():
            assert not re.search(r'_ID 0x33u', p.read_text(encoding='utf-8')), other


def test_init_case_and_variants():
    rd = xbe_reader()
    a = by_address()
    # sub_001F4E00 switches on model - 0x2ED: 0x376 -> 0x001F5281
    assert ('imm', 0xFFFFFD13) in a[0x001F4E0C].ops and ('imm', 0xA1) in a[0x001F4E11].ops
    idx = rd(0x001F596C + 0x376 - 0x2ED, 1)[0]
    assert struct.unpack('<I', rd(0x001F58B4 + 4 * idx, 4))[0] == 0x001F5281
    # player +0x1568 == 0xE/0xF: the rising variant (update sub_001EFBA0)
    assert mem(a[0x001F528A])[0][4] == 0x1568 and ('imm', 0xE) in a[0x001F5290].ops and ('imm', 0xF) in a[0x001F5295].ops
    assert target(a[0x001F5293]) == 0x001F52E2 and ('imm', RISING) in a[0x001F5352].ops
    # the fight: state init, update slot 3, damage callback
    assert target(a[0x001F52BB]) == STATE_INIT and calls_to(STATE_INIT) == {0x001F52BB}
    assert ('imm', UPDATE) in a[0x001F52C0].ops and ('imm', 3) in a[0x001F52C5].ops and target(a[0x001F52CC]) == 0x000A5CE0
    assert ('imm', DAMAGE) in a[0x001F52D1].ops and target(a[0x001F52D7]) == 0x0009E720
    # the chair pointer is set by the fight's state init and by the rising variant's update only
    writers = {i.function for i in decoded() if i.mnemonic == 'mov' and i.text.startswith('dword ptr [0x78f000], e')}
    assert writers == {STATE_INIT, RISING}


def test_chair_state():
    rd = xbe_reader()
    a = by_address()
    # sub_001EEA30 (esi = chair): 0x1C words from 0x0078F000 zeroed, the chair, the train anchor, health 60
    assert ('imm', 0x1C) in a[0x001EEA90].ops and ('imm', 0x0078F000) in a[0x001EEA95].ops
    assert mem(a[0x001EEA9C])[0][4] == 0x0078F04C and 'esi' in a[0x001EEACE].text and mem(a[0x001EEACE])[0][4] == 0x0078F000
    assert mem(a[0x001EEA88])[0][4] == 0x0049E328 and mem(a[0x001EEAA1])[0][4] == 0x0078F004
    assert f32(rd, 0x0049E328) == pytest.approx(-576.2) and f32(rd, 0x0047D014) == 7.5
    assert mem(a[0x001EEAA9])[0][4] == 0x0047D014 and mem(a[0x001EEAB1])[0][4] == 0x0078F008
    assert f32(rd, 0x00500080) == 60.0 and mem(a[0x001EEBA7])[0][4] == 0x1CC and mem(a[0x001EEB97])[0][4] == 0x1D8
    assert ('imm', 0x0078F054) in a[0x001EEB26].ops                         # the jet emitter


def test_update_constants_and_frame():
    rd = xbe_reader()
    a = by_address()
    # start line: player +0xA8 > -535 returns (0x001EF827); active flag, focus, event 0x29
    assert mem(a[0x001EEC47])[0][4] == 0xA8 and mem(a[0x001EEC50])[0][4] == 0x00500088 and f32(rd, 0x00500088) == -535.0
    assert a[0x001EEC57].mnemonic == 'ja' and target(a[0x001EEC57]) == 0x001EF827 and a[0x001EF830].mnemonic == 'ret'
    assert mem(a[0x001EEC2E])[0][4] == 0x0078F04C and mem(a[0x001EEC67])[0][4] == 0xFF4
    assert ('imm', 0x29) in a[0x001EEC65].ops and target(a[0x001EEC6D]) == 0x002274C0 and ends(a, 0x001EEC6D) == 0x001EEC72
    # the steered player: the second when story, two players and the first one's +0xA8 is larger
    assert mem(a[0x001EEBD9])[0][4] == 0x007DE455 and mem(a[0x001EEC13])[0][4] == 0x00790950
    assert mem(a[0x001EEC1B])[0][4] == 0xA8 and mem(a[0x001EEC23])[0][4] == 0x162C and a[0x001EEC2A].mnemonic == 'jbe'
    # steering: x = player +0xB4 - 11, height 10 (band 7..10), z 0; yaw clamped 90 +- 30
    assert mem(a[0x001EEEB2])[0][4] == 0xB4 and mem(a[0x001EEEC1])[0][4] == 0x00500094 and f32(rd, 0x00500094) == 11.0
    assert mem(a[0x001EEE04])[0][4] == 0x0047862C and f32(rd, 0x0047862C) == 10.0
    assert mem(a[0x001EEE61])[0][4] == 0x0047D204 and f32(rd, 0x0047D204) == 7.0
    assert a[0x001EEEFC].mnemonic == 'movaps' and mem(a[0x001EEEFF])[0][4] == 0x0078F00C   # 0 - z
    assert f32(rd, 0x00478628) == 90.0 and f32(rd, 0x005000B8) == 30.0
    # the two train constants this area changes are read by the update alone
    assert refs(0x00500088) == {UPDATE} and refs(0x00500094) == {UPDATE}
    # the live position is integrated and copied to the object
    assert mem(a[0x001EF250])[0][4] == 0x0078F004 and mem(a[0x001EF258])[0][4] == 0x64
    assert mem(a[0x001EF265])[0][4] == 0x68 and mem(a[0x001EF272])[0][4] == 0x6C
    # aim: player +0xB4/+0xB8/+0xBC
    assert {mem(a[x])[0][4] for x in (0x001EF57E, 0x001EF5A4, 0x001EF595)} == {0xB4, 0xB8, 0xBC}


def test_update_calls_out():
    """Every call out of the update; those placing things in the world, at their return addresses."""
    a = by_address()
    callees = {target(i) for i in decoded() if i.function == UPDATE and i.call}
    assert callees == {0x002274C0, 0x000A14B0, 0x00152090, 0x0010E730, 0x0010E760, 0x00153B40, 0x0010EB10,
                       0x003C8501, 0x00151650, 0x000DFFA0, 0x000EA9C0, 0x00045390, 0x00045820, 0x000A5A60, 0x000468A0}
    world = {0x001EED75: (0x0010E730, 0x001EED7A), 0x001EF684: (0x000DFFA0, 0x001EF689), 0x001EF6E5: (0x000DFFA0, 0x001EF6EA),
             0x001EF756: (0x000EA9C0, 0x001EF75B), 0x001EF7AF: (0x00045820, 0x001EF7B4), 0x001EF81F: (0x000468A0, 0x001EF824)}
    for site, (fn, ret) in world.items():
        assert target(a[site]) == fn and ends(a, site) == ret, hex(site)
    fns = {f for f, _ in world.values()}
    assert {i.address for i in decoded() if i.function == UPDATE and i.call and target(i) in fns} == set(world)
    # the muzzle flash gets the gun position as arg 3 (edx), the shot as arg 2 (edx)
    assert 'edx' in a[0x001EF67C].text and ('imm', 2) in a[0x001EF682].ops
    assert 'edx' in a[0x001EF753].text and ('imm', 0x35) in a[0x001EF754].ops
    # the gunshot sound record: chair position stored at [esp + 0xb8], record at [esp + 0x9c] (+0x1C)
    assert mem(a[0x001EF770])[0][4] == 0xB8 and mem(a[0x001EF77E])[0][4] == 0x9C
    # sub_000A5A60 returns its static position = object +0x64 + object +0x1A8; it goes to sub_000468A0 as arg 2
    assert mem(a[0x000A5A6C])[0][4] == 0x64 and ('imm', 0x00715F98) in a[0x000A5AA3].ops
    assert target(a[0x001EF80F]) == 0x000A5A60 and 'eax' in a[0x001EF817].text
    # positions derive from the live position plus rotated offsets: sub_00152090 is a 3x3 rotation
    disps = {op[4] for i in decoded() if i.function == 0x00152090 for op in mem(i)}
    assert max(d for d in disps if d < 0x100) == 0x28
    # sub_000A14B0 caches the rotation (+0x274) from the angles; the update asks for no translation
    assert mem(a[0x000A155D])[0][4] == 0x274 and target(a[0x000A1569]) == 0x00151650
    assert [a[x].mnemonic for x in (0x001EEC75, 0x001EEC77)] == ['push', 'push'] and target(a[0x001EEC7F]) == 0x000A14B0


def test_damage_and_train_death():
    rd = xbe_reader()
    a = by_address()
    # sub_001EF840(record): only while active, with a hit point (+0x28) and direction (+0x30)
    assert mem(a[0x001EF844])[0][4] == 0x0078F000 and mem(a[0x001EF880])[0][4] == 0x0078F04C
    assert mem(a[0x001EF86A])[0][4] == 0x30 and mem(a[0x001EF875])[0][4] == 0x28
    assert mem(a[0x001EF866])[0][4] == 0x50                                  # the record: 0x48 frame + esi
    # health -= damage (+0x20); events 0x12 at <= 35, 0x13 at <= 15; the death needs 0 > health
    assert mem(a[0x001EF987])[0][4] == 0x20 and mem(a[0x001EF98F])[0][4] == 0x1CC
    assert mem(a[0x001EF96F])[0][4] == 0x0047DAC4 and f32(rd, 0x0047DAC4) == 35.0 and f32(rd, 0x004788FC) == 15.0
    assert ('imm', 0x12) in a[0x001EF99B].ops and ends(a, 0x001EF99D) == 0x001EF9A2
    assert ('imm', 0x13) in a[0x001EF9BB].ops and ends(a, 0x001EF9BD) == 0x001EF9C2
    assert a[0x001EF9C8].mnemonic == 'comiss' and 'xmm5' in a[0x001EF9C8].text and a[0x001EF9CF].mnemonic == 'jbe'
    # the train's death: 'FTWL', the three emitters (null safe), focus, event 4, train particles
    assert ('imm', 0x4654574C) in a[0x001EFA5B].ops
    assert [mem(a[x])[0][4] for x in (0x001EFAAB, 0x001EFAB8, 0x001EFAC6)] == [0x0078DF50, 0x0078DF54, 0x0078DF58]
    assert a[0x0010EB94].mnemonic == 'test' and a[0x0010EB96].mnemonic == 'je'
    assert ('imm', 4) in a[0x001EFADB].ops and ends(a, 0x001EFAE7) == 0x001EFAEC
    assert refs(0x0078E900) == {DAMAGE, 0x001F1260} and refs(0x0078EF60) == {DAMAGE, 0x001F1260}
    assert refs(0x0078E468) == {DAMAGE, 0x001F5A10}
    # the death effects stand at fixed train points
    assert f32(rd, 0x0049E338) == -593.0 and f32(rd, 0x0049E354) == pytest.approx(-622.84)
    # the chair's level events are these four
    fire = {i.address for i in decoded() if i.function in (UPDATE, DAMAGE, STATE_INIT) and i.call and target(i) == 0x002274C0}
    assert fire == {0x001EEC6D, 0x001EF99D, 0x001EF9BD, 0x001EFAE7}


def test_removal_path():
    a = by_address()
    # the rising variant removes its own chair from inside its update
    assert target(a[0x001EFDE2]) == 0x000A9070 and mem(a[0x001EFDE7])[0][4] == 0x0078F054 and target(a[0x001EFDED]) == 0x0010E2A0
    assert mem(a[0x001EFDF4])[0][4] == 0x0078F000
    # sub_000A9070(object) = sub_0009E330(object, 1, 0) + sub_000A7090(object)
    assert target(a[0x000A907A]) == 0x0009E330 and target(a[0x000A9080]) == 0x000A7090


def test_rider():
    rd = xbe_reader()
    a = by_address()
    # class 8 (AI +0x14 = 8, +0x10 = 0) -> sub_00263260
    assert mem(a[0x0027D594])[0][4] == 0x14 and mem(a[0x0027D59A])[0][4] == 0x10
    assert a[0x0027D5AF].mnemonic == 'jne' and ('imm', 8) in a[0x0027D5B1].ops and target(a[0x0027D5B4]) == 0x0027DA8A
    assert target(a[0x0027DA9B]) == 0x00263260 and calls_to(0x00263260) == {0x0027DA9B}
    # it sits in the chair: live position + rotated seat offset (chair +0x274), the chair's angles, khallosidle
    assert mem(a[0x002632FB])[0][4] == 0x0078F000 and mem(a[0x0026331C])[0][4] == 0x0078F004
    assert ('imm', 0x274) in a[0x0026334E].ops and ('imm', 0xBFB) in a[0x002635C1].ops
    assert mem(a[0x00263279])[0][4] == 0x9D0 and mem(a[0x002632E8])[0][4] == 0x20
    # khallosidle (0xBFB) is named only by lv11.pak, registered when that pak is mounted
    assert ('imm', 0x0049D9C8) in a[0x001CA8BA].ops and target(a[0x001CA8BF]) == 0x00062260
    assert ('imm', 0x21) in a[0x001CA8CC].ops and ('imm', 0x00484410) in a[0x001CA8CE].ops and target(a[0x001CA8D8]) == 0x0005B980
    lv11 = anim_table(rd, 0x00484410, 0x21)
    assert lv11['khallosidle'] == 0xBFB
    # its ids collide with no global anim pak's
    tables, pushes, pak = [], [], None
    for i in sorted((i for i in decoded() if i.function == 0x001CA2F0), key=lambda i: i.address):
        if i.mnemonic == 'push':
            pushes.append(next((v for k, v in i.values() if k == 'imm'), None))
        elif i.call and target(i) == 0x00062260:
            pak = rd(pushes[-1], 48).split(b'\0')[0].decode() if pushes and pushes[-1] else None
            pushes = []
        elif i.call and target(i) == 0x0005B980:
            if len(pushes) >= 3 and pushes[-3] is not None and pushes[-2] is not None:
                tables.append((pak, set(anim_table(rd, pushes[-2], pushes[-3]).values())))
            pushes = []
    globals_ = [ids for p, ids in tables if p and 'stream' not in p]
    assert len(globals_) > 30 and not set(lv11.values()) & set().union(*globals_)


def test_rider_made_from_the_character_table():
    """The placeholder made as Khallos: sub_000DAC30 takes its models only
    from the character table (+4, +8), which the set preload requests the
    same way; the stock bar follows the focus by its model word +0x94."""
    rd = xbe_reader()
    a = by_address()

    def word(cid, off):
        return struct.unpack('<I', rd(0x0053A3CC + cid * 0x30 + off, 4))[0]
    assert word(2, 4) == 0x582 and word(0x33, 4) == 0x619                       # trainkhallos, soldiermodel_talk
    assert word(2, 8) == 0xFFFFFFFF or word(2, 8) < 0x9D0
    assert mem(a[0x000DAC59])[0][4] == 0x0053A3D0 and mem(a[0x000DB084])[0][4] == 0x0053A3D4
    assert ('imm', 0xC) in a[0x000DB0B1].ops                                    # +8 only (edi 8 .. < 0xC)
    assert refs(0x0053A3D4) == {0x000D7780, 0x000DAC30}
    assert mem(a[0x000D77A3])[0][4] == 0x0053A3D0 and target(a[0x000D77AA]) == 0x000A5E40
    assert mem(a[0x000D77B4])[0][4] == 0x0053A3D4 and target(a[0x000D77C8]) == 0x000A5E40
    assert ('imm', 1) in a[0x000D77D4].ops
    # sub_0014CC80: focus [[0x007B0C7C]+0xFF4], its model word +0x94
    assert mem(a[0x0014CCC3])[0][4] == 0xFF4 and mem(a[0x0014CCD6])[0][4] == 0x94


def test_bar_text_site():
    a = by_address()
    assert ('imm', 0xA) in a[0x0014CCB3].ops                                  # story mode only
    assert ('imm', 0x376) in a[0x0014CCEF].ops and target(a[0x0014CCFA]) == 0x0014CD95
    assert ('imm', 0xFF1) in a[0x0014CD9C].ops and target(a[0x0014CDA1]) == 0x0003D7B0 and ends(a, 0x0014CDA1) == 0x0014CDA6
    assert mem(a[0x001EEC67])[0][4] == 0xFF4                                 # the focus the bar follows


def test_resource_map_relocated():
    import json
    patches = json.loads((ROOT / 'config/guest-code-patches/rooms.json').read_text(encoding='utf-8'))['patches']
    moved = {int(p['address'], 16): p for p in patches if '0x78b860' in p.get('reason', '')}
    for site in (0x002427C8, 0x00242A34, 0x000A6F94, 0x000A6EC4, 0x000A688F):
        assert int(moved[site]['value'], 16) == 0x008CA000, hex(site)
    text = INC.read_text(encoding='utf-8')
    assert '#define KHL_RESMAP 0x008CA000u' in text and '#define KHL_RESMAP_MODELS 0xA00u' in text


# ---- game data (skipped without the paks) ------------------------------------

GAME = ROOT / 'private/game/pak'
if not GAME.exists() and len(ROOT.parents) > 2:
    GAME = ROOT.parents[2] / 'private/game/pak'


def _index(path):
    with open(path, 'rb') as f:
        head = f.read(12)
        offset, length = struct.unpack_from('<II', head, 4)
        f.seek(offset)
        table = f.read(length)
    return {c: (pos, n) for c, pos, n, _ in struct.iter_unpack('<4I', table)}


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def test_models_and_paks():
    """0x376 is a scene-100 resource row; its model is in lv11 prop3/4/6/7, not in the paks a map mounts."""
    if not (GAME / 'arcade/l_100.pak').exists():
        pytest.skip('game paks not available locally')
    idx = _index(GAME / 'arcade/l_100.pak')
    with open(GAME / 'arcade/l_100.pak', 'rb') as f:
        pos, n = idx[0xD9E2667A]                       # prop/table/prop_all_xbas.raw
        f.seek(pos)
        table = f.read(n)
    start = struct.unpack_from('<I', table, 4)[0] + 8
    props = {}
    for o in range(start, len(table) - 0x27, 0x28):
        mid, typ, _, path, _, _, _, _, flags, init = struct.unpack_from('<iIiiifiiIi', table, o)
        props[mid] = (typ, table[path + 8:table.index(b'\0', path + 8)].decode('latin1'), init)
    assert props[0x376] == (0x4000000, 'train/khallosjetpack', 30)
    assert props[0x619][1] == 'chrs/soldiermodel_talk' and props[0x582][1] == 'chrs/trainkhallos'
    model = 'assets/ts/models/{0}/xbox/output/{1}.xbr'
    names = ['stream/lv11/prop%d.pak' % k for k in (1, 2, 3, 4, 6, 7)] + ['chr.pak', 'gun.pak', 'arcade/l_100.pak']
    paks = {k: _index(GAME / k) for k in names}
    p = props[0x376][1]
    holders = [k for k, v in paks.items() if _crc(model.format(p, p.split('/')[-1])) in v]
    assert holders == ['stream/lv11/prop3.pak', 'stream/lv11/prop4.pak', 'stream/lv11/prop6.pak', 'stream/lv11/prop7.pak']
    for m in (0x619, 0x582):
        p = props[m][1]
        assert _crc(model.format(p, p.split('/')[-1])) in paks['chr.pak']
    assert (GAME / 'stream/lv11/anim/lv11.pak').stat().st_size == 300416


def test_multi_chair_globals():
    """What several chairs rely on: the chair statics are touched only by chair
    code and the seated Khallos; the damage callback is called with the object
    hit at record +0; the death's emitters come from the init."""
    a = by_address()
    block = range(0x0078F000, 0x0078F070, 4)
    users = {}
    for i in decoded():
        for _, v in i.values():
            if v in block:
                users.setdefault(i.function, set()).add(v)
    assert set(users) == {STATE_INIT, UPDATE, DAMAGE, RISING, INIT, 0x00263260, 0x00263150}
    assert max(max(v) for v in users.values()) == 0x0078F06C          # 0x1C words cover them all
    assert {0x0078F060, 0x0078F064, 0x0078F068, 0x0078F06C} <= users[0x00263260]   # the rider's sway
    assert users[0x00263150] <= {0x0078F068, 0x0078F06C}                             # read by its pose callback
    assert {i.function for i in decoded() if i.mnemonic == 'push' and ('imm', 0x00263150) in i.ops} == {0x00263260}
    assert mem(a[0x00263162])[0][4] == 0xA0 and ('imm', 0x98) in a[0x00263150].ops      # entity arg 1
    # the damage dispatcher: callback = [[record] + 0x19C], called with the record
    assert ('imm', 0x19C) not in a[0x0009E728].ops and mem(a[0x0009E728])[0][4] == 0x19C
    assert 'edx' in a[0x000BB614].text and mem(a[0x000BB614])[0][1] == 'ebx' and mem(a[0x000BB614])[0][4] == 0
    assert mem(a[0x000BB616])[0][4] == 0x19C and a[0x000BB638].mnemonic == 'call' and 'eax' in a[0x000BB638].text
    assert a[0x000BB61E].mnemonic == 'push' and 'ebx' in a[0x000BB61E].text
    # the death's three emitters: the init hands 0x0078DF50 to the emitter maker
    assert ('imm', 0x0078DF50) in a[0x001EEB7A].ops and target(a[0x001EEB7F]) == 0x00378530
    # the character maker a placeholder goes through (logged): sub_000DAC30(id, .., pool slot)
    assert calls_to(0x000DAC30) >= {0x0015DFD5}


def test_emitter_owner_slots():
    """The chair's jet and death emitters remember the address of their owner
    slot, and the particle system clears that slot when it frees them."""
    a = by_address()
    # sub_00111AE0: new emitter (esi) +0 = owner slot (arg 1), *slot = emitter
    assert mem(a[0x00111DC4])[0][4] == 0x28 and a[0x00111DC8].mnemonic == 'test'
    assert a[0x00111DCC].mnemonic == 'mov' and mem(a[0x00111DCC])[0][1] == 'esi' and mem(a[0x00111DCC])[0][4] == 0
    assert mem(a[0x00111DD4])[0][1] == 'eax' and mem(a[0x00111DD4])[0][4] == 0
    # sub_0010E070 (free): if emitter +0 then *(+0) = 0; reached from the particle system and sub_0010E2A0
    assert mem(a[0x0010E076])[0][1] == 'esi' and mem(a[0x0010E076])[0][4] == 0
    assert a[0x0010E07F].mnemonic == 'je' and mem(a[0x0010E081])[0][1] == 'eax' and 'ecx' in a[0x0010E081].text
    assert a[0x0010E078].mnemonic == 'xor' and 'ecx, ecx' in a[0x0010E078].text
    assert calls_to(0x0010E070) == {0x0010E14A, 0x0010E1A8, 0x0010E2A9, 0x0010E52C, 0x00111FA4}
    # the chair's init passes its jet slot and its first death emitter slot as owners
    assert ('imm', 0x0078F054) in a[0x001EEB26].ops and target(a[0x001EEB33]) == 0x00378530
    assert ('imm', 0x0078DF50) in a[0x001EEB7A].ops and target(a[0x001EEB7F]) == 0x00378530
    assert target(a[0x00378591]) == 0x00111AE0


def test_yaw_clamp_and_seat():
    """.28 "didn't orient toward me": the yaw target is the player's bearing,
    clamped to 90 +- [0x005000B8] (30, read by the update alone); the wrap
    handles any target. "Floated in front": the seat is live position + -1.2 x
    the chair's up row (+0x284), only in handler states 0 and 1."""
    rd = xbe_reader()
    a = by_address()
    assert refs(0x005000B8) == {UPDATE} and mem(a[0x001EEEDE])[0][4] == 0x005000B8 and f32(rd, 0x005000B8) == 30.0
    assert mem(a[0x001EEEAB])[0][4] == 0xB4 and mem(a[0x001EEEC9])[0][4] == 0xBC and a[0x001EEEED].mnemonic == 'fpatan'
    assert mem(a[0x001EEF41])[0][4] == 0x00478638 and f32(rd, 0x00478638) == pytest.approx(57.29578, rel=1e-5)
    assert a[0x001EEF36].mnemonic == 'addss' and a[0x001EEF3A].mnemonic == 'subss'          # hi 90 + 30, lo 90 - 30
    assert f32(rd, 0x00475C8C) == -180.0 and f32(rd, 0x00475C88) == 360.0 and f32(rd, 0x00475CB4) == pytest.approx(1 / 360)
    assert mem(a[0x001EF014])[0][4] == 0x70 and mem(a[0x001EF023])[0][4] == 0x00475C8C   # yaw - target, wrapped
    # the seat: (0, [0x0049E4BC], 0) through the chair's +0x274, added to the live position
    assert mem(a[0x00263308])[0][4] == 0x0049E4BC and f32(rd, 0x0049E4BC) == pytest.approx(-1.2)
    assert mem(a[0x00263355])[0][4] == 0x1C and target(a[0x0026335B]) == 0x00152090        # y of the local vector
    assert [mem(a[x])[0][4] for x in (0x001520F5, 0x001520A7, 0x001520B0)] == [0x10, 0x14, 0x18]   # y picks row 1
    assert [mem(a[x])[0][4] for x in (0x002633D1, 0x002633DE, 0x002633E6, 0x002633F0, 0x00263412)] == [0x70, 0xC4, 0x70, 0xC0, 0xC8]
    # handler states: 0 (sit down, then 1) and 1 (seated); anything else does nothing
    assert target(a[0x002632EE]) == 0x00263597 and a[0x002632F4].mnemonic == 'dec' and target(a[0x002632F5]) == 0x002636F1
    assert mem(a[0x002636BA])[0][4] == 0x9D0 and 'ebx' in a[0x002636BA].text


def test_rider_class_anim_and_restart():
    """.31: chair 0's rider never reached his handler ("rider calls 0") and the
    riders looked unseated; a restart made no chairs. sub_0015C700 makes the
    class asked for (+0xAA8/+0xAAC) the class before every think; state 0
    plays khallosidle once whatever sub_0005F460 answers (current anim at
    entity +4 -> +0xD4); the restart is sub_001CDD60, without the pak mount."""
    rd = xbe_reader()
    a = by_address()
    # the asked-for class becomes the class (story: +0x10/+0x14), then the think
    assert mem(a[0x0015C724])[0][4] == 0xAA8 and mem(a[0x0015C72C])[0][4] == 0xAAC
    assert mem(a[0x0015C788])[0][4] == 0xAA8 and mem(a[0x0015C78E])[0][4] == 0xAAC
    assert mem(a[0x0015C794])[0][4] == 0x10 and mem(a[0x0015C797])[0][4] == 0x14
    assert target(a[0x0015F078]) == 0x0015C700 and target(a[0x0015F07E]) == 0x0027FE30
    assert target(a[0x0027FE71]) == 0x0027D590                                          # class 8 -> the handler's dispatch
    assert mem(a[0x000175B8])[0][4] == 0xAA8 and mem(a[0x000175BE])[0][4] == 0xAAC      # sub_00017570: ask for a class
    # khallosidle: state 0 tests its slot, plays it (entity, 0xBFB, 0, 1.0) and goes on to state 1 regardless
    assert mem(a[0x002635A4])[0][4] == 0x007E03C0 + 0xBFB * 2 and ('imm', 0x3F800000) in a[0x002635BA].ops
    assert ('imm', 0xBFB) in a[0x002635C1].ops and target(a[0x002635D9]) == 0x0005F460
    assert a[0x002635DE].mnemonic == 'movss' and mem(a[0x002636AF])[0][4] == 0x20     # result unused; state 1
    assert mem(a[0x0005F46D])[0][4] == 0x007E03C0 and mem(a[0x0005F489])[0][4] == 0x007E3954
    assert target(a[0x0005F4EB]) == 0x001C8C20 and mem(a[0x001C8C5E])[0][4] == 0xD4     # refused over the one playing
    assert mem(a[0x0005BDA1])[0][4] == 0xD4                                             # sub_0005BC20: the current anim
    # restart: "START restart" runs the teardown and sub_001CDD60; "from scratch" sub_001CDB80 (the mount)
    assert rd(0x0047817C, 8) == b'restart\0' and rd(0x0047816C, 13) == b'from scratch\0'
    assert mem(a[0x0003D460])[0][4] == 0x006B7AA8 and mem(a[0x0003D4B1])[0][4] == 0x006B7AA8
    assert target(a[0x0003D4DC]) == 0x001CDAF0 and target(a[0x0003D4E1]) == 0x001CDD60
    assert calls_to(0x001CDD60) == {0x0003D4E1} and calls_to(0x001CDB80) == {0x0003D590}
    assert target(a[0x001CDD74]) == 0x0015E4F0 and target(a[0x001CDB85]) == 0x00062790
    text = INC.read_text(encoding='utf-8')
    assert '#define KHL_AI_ASK_HI 0xAA8u' in text and '#define KHL_IDLE 0xBFBu' in text and '#define KHL_ANIM_DATA 0x007E3954u' in text


def test_chair_sounds():
    """.28 "didn't really have much audio": the chair plays 0x393 and loops
    0x394 (handle at chair +0x22C); sub_00045820 needs the sound loaded
    (sub_00027F80), and sub_00045670 loads a flag-4 sound only in the level of
    its top byte. It is called only by the level's sound load, after the pak
    mounts."""
    rd = xbe_reader()
    a = by_address()
    assert ('imm', 0x393) in a[0x001EF79B].ops and ('imm', 0x394) in a[0x001EF809].ops and ('imm', 0x22C) in a[0x001EF818].ops
    assert target(a[0x00045838]) == 0x00027F80 and mem(a[0x00027F8E])[0][4] == 0xC and mem(a[0x00027F84])[0][4] == 0x004CA9B4
    assert mem(a[0x00045671])[0][4] == 0x0079094C and ('imm', 4) in a[0x000456B1].ops and ('imm', 0x18) in a[0x000456B8].ops
    assert ('imm', 4) in a[0x000456CA].ops                                               # "load it"
    assert calls_to(0x00045670) == {0x0002787A, 0x000278AC} and a[0x000278B4].mnemonic == 'test'
    assert a[0x000454F0].mnemonic == 'jmp' and target(a[0x000454F0]) == 0x00027700 and calls_to(0x000454F0) == {0x0003D5A8}
    assert target(a[0x0003D590]) == 0x001CDB80 and target(a[0x001CDB85]) == 0x00062790 and calls_to(0x00062790) == {0x001CDB85}
    assert ('imm', 0x004761F8) in a[0x00027743].ops and rd(0x004761F8, 22) == b'host0:sound/sounddata\0'
    if not (GAME / 'sounds_e.pak').exists():
        pytest.skip('game paks not available locally')
    pos, n = _index(GAME / 'sounds_e.pak')[_crc('sound/sounddata')]
    with open(GAME / 'sounds_e.pak', 'rb') as f:
        f.seek(pos)
        d = f.read(n)
    p = 2
    for _ in range(d[0] | d[1] << 8):                                                    # bank names
        p += 1 + d[p]
    sounds = []
    for _ in range(0x1BD8C // 0x14):                                                     # name, bank, word, flags, word
        L = d[p]
        sounds.append((d[p + 1:p + 1 + L].decode('latin1'), struct.unpack_from('<I', d, p + 1 + L + 4)[0]))
        p += L + 11
    assert p == len(d)
    assert sounds[0x393] == ('SFX_11_KHALLOS_GUN', 0x0B00000C) and sounds[0x394] == ('SFX_11_JETPACK_NEW', 0x0B00041C)
    text = INC.read_text(encoding='utf-8')
    assert '#define KHL_SND_GUN 0x393u' in text and '#define KHL_SND_JET 0x394u' in text and '#define KHL_SPAN 0x005000B8u' in text
