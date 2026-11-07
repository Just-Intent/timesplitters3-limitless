"""MapMaker real story boss: the far-future giant robot (story mission 10) as a Story AI.

Compiles src/interface/pc_mapmaker_robot.inc against fake guest memory (with
mutations that must be caught) and, when the private XBE and its disassembly
are available, checks the stock facts the hooks rely on: the character data
of id 0x42, the lv14 anim pak and its names, the class 0x80000 dispatch and
message handler, the scene-14 state allocation, the robot's scene reads,
level event, music change and pad lookups, the scene-14 explosion draw, the
soft-restart reset and the boss bar case.
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
INC = ROOT / 'src/interface/pc_mapmaker_robot.inc'

MUTATIONS = {
    'always-on': ('    return mapmaker_limits.limits;\n}', '    return 1;\n}'),
    'any-mode': ('    return rbt_on() && WORD_AT(RBT_SCENE)==100u && RBT_B(RBT_MODE)==10u;', '    return rbt_on() && WORD_AT(RBT_SCENE)==100u;'),
    'any-scene': ('    return rbt_on() && WORD_AT(RBT_SCENE)==100u && RBT_B(RBT_MODE)==10u;', '    return rbt_on() && RBT_B(RBT_MODE)==10u;'),
    'wrong-pak': ('#define RBT_PAK 0x0049DC78u', '#define RBT_PAK 0x0049DB98u'),
    'wrong-id': ('#define RBT_ID 0x42u ', '#define RBT_ID 0xA8u '),
    'mount-any-map': ('    if(!rbt_play() || rbt.optout || (has=rbt_map_has())==0) return;\n    if(rbt_pak', '    if(!rbt_play() || rbt.optout || ((has=rbt_map_has()),0)) return;\n    if(rbt_pak'),
    'mount-ignores-optout': ('    if(!rbt_play() || rbt.optout || (has=rbt_map_has())==0) return;\n    if(rbt_pak', '    if(!rbt_play() || (has=rbt_map_has())==0) return;\n    if(rbt_pak'),
    'mount-twice': ('    if(rbt_pak_mounted()) { rbt_log("lv14.pak already mounted",0,(uint32_t)has);return; }\n', ''),
    'mount-fills-table': ('#define RBT_MOUNT_KEEP 8u', '#define RBT_MOUNT_KEEP 0u'),
    'mount-before-original': ('    int has;\n    uint32_t mounted;\n    original();\n', '    int has;\n    uint32_t mounted;\n    guest_call(sub_0005A130,1,RBT_PAK);original();\n'),
    'set-preset': ('    if(RBT_H(info+RBT_INFO_SET)!=RBT_SET_MAP) return 0;   /* a preset or profile set */\n', ''),
    'set-skips-slot-1': ('    for(k=0;k<n;k++) if(RBT_H(info', '    for(k=1;k<n;k++) if(RBT_H(info'),
    'set-skips-slots-11-20': ('    for(k=0;extra && k<RBT_B(extra+3u);k++)', '    for(k=0;0 && extra && k<RBT_B(extra+3u);k++)'),
    'set-extra-offset': ('if(RBT_H(extra+4u+k*4u)==RBT_ID) return 1;', 'if(RBT_H(extra+2u+k*4u)==RBT_ID) return 1;'),
    'set-unreadable-skipped': ('    if(!container || !MAPMAKER_PTR_OK(container,RBT_SCAN) || WORD_AT(container)!=RBT_MAGIC) return -1;', '    if(!container || !MAPMAKER_PTR_OK(container,RBT_SCAN) || WORD_AT(container)!=RBT_MAGIC) return 0;'),
    'level-keeps-parts': ('    if(play) WORD_AT(RBT_PARTS)=0;', '    if(0) WORD_AT(RBT_PARTS)=0;'),
    'level-clears-parts-anywhere': ('    if(play) WORD_AT(RBT_PARTS)=0;', '    WORD_AT(RBT_PARTS)=0;'),
    'level-keeps-robot': ('    memset(&rbt_lvl,0,sizeof rbt_lvl);                /* a new level', '    rbt_lvl.cur=0;                /* a new level'),
    'level-any-map': ('    if(!play || (has=rbt_map_has())==0) return;\n    if(!WORD_AT(RBT_PARTS))', '    if(!play || ((has=rbt_map_has()),0)) return;\n    if(!WORD_AT(RBT_PARTS))'),
    'level-stale-state': ('        WORD_AT(RBT_STATE)=0;                          /* stale pointer of an earlier level */\n', ''),
    'level-second-parts': ('    if(!WORD_AT(RBT_PARTS)) guest_call(sub_002A62C0,0);', '    guest_call(sub_002A62C0,0);'),
    'level-no-parts': ('    if(!WORD_AT(RBT_PARTS)) guest_call(sub_002A62C0,0);', '    if(0) guest_call(sub_002A62C0,0);'),
    'level-unchecked': ('        if(!st || !MAPMAKER_PTR_OK(st,RBT_STATE_SIZE)) break;', '        if(!st) break;'),
    'class-value': ('#define RBT_CLASS 0x80000u', '#define RBT_CLASS 0x100000u'),
    'class-after-original': ('        WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;\n', '        original();WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;g_esp-=4u;\n'),
    'class-keeps-lo': ('        WORD_AT(rec)=0;WORD_AT(rec+4u)=RBT_CLASS;', '        WORD_AT(rec+4u)=RBT_CLASS;'),
    'class-any-character': ('    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBT_H(ent+RBT_ENT_MODEL)==RBT_MODEL)) { original();return; }\n', ''),
    'class-char-only': ('    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID && !(MAPMAKER_PTR_OK(ent,0x100u) && RBT_H(ent+RBT_ENT_MODEL)==RBT_MODEL))', '    if(RBT_H(a+RBT_ACTOR_CHAR)!=RBT_ID)'),
    'class-without-pak': ('    else if(!rbt_pak_mounted()) rbt_log', '    else if(0) rbt_log'),
    'class-ignores-optout': ('    if(rbt.optout) rbt_log("TSFP_MAPMAKER_REAL_BOSS=0', '    if(0) rbt_log("TSFP_MAPMAKER_REAL_BOSS=0'),
    'class-no-actor': ('        rbt_inst[k].record=rec;rbt_inst[k].actor=a;rbt_inst[k].removed=0;', '        rbt_inst[k].record=rec;rbt_inst[k].removed=0;'),
    'nav-any-caller': ('    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || WORD_AT', '    if(WORD_AT'),
    'nav-missing-site': ('0x00293C64u,0x00293C9Eu,0x00293CD0u};', '0x00293C64u,0x00293C9Eu,0x00293CE5u};'),
    'nav-any-scene': ('    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || WORD_AT(RBT_SCENE)!=100u || !rbt_on())', '    if(k==sizeof rbt_nav_rets/sizeof rbt_nav_rets[0] || !rbt_on())'),
    'nav-found': ('    g_eax=0xFFFFFFFFu;\n    MAPMAKER_GUEST_RET();\n}', '    g_eax=0u;\n    MAPMAKER_GUEST_RET();\n}'),
    'nav-no-ret': ('    g_eax=0xFFFFFFFFu;\n    MAPMAKER_GUEST_RET();\n}', '    g_eax=0xFFFFFFFFu;\n}'),
    'music-kept': ('    if(!rbt_lvl.music++) rbt_log("giant robot death: music change dropped (the map keeps its track)",WORD_AT(g_esp+4u),0);\n    MAPMAKER_GUEST_RET();', '    original();'),
    'music-any-caller': ('|| WORD_AT(g_esp)!=RBT_RET_MUSIC ', ''),
    'music-any-scene': ('WORD_AT(g_esp)!=RBT_RET_MUSIC || WORD_AT(RBT_SCENE)!=100u', 'WORD_AT(g_esp)!=RBT_RET_MUSIC'),
    'event-kept': ('    if(rbt_lvl.cur>=0) rbt_remove((unsigned)rbt_lvl.cur);\n    MAPMAKER_GUEST_RET();', '    if(rbt_lvl.cur>=0) rbt_remove((unsigned)rbt_lvl.cur);\n    original();'),
    'event-any-caller': ('|| WORD_AT(g_esp)!=RBT_RET_EVENT ', ''),
    'event-any-scene': ('WORD_AT(g_esp)!=RBT_RET_EVENT || WORD_AT(RBT_SCENE)!=100u', 'WORD_AT(g_esp)!=RBT_RET_EVENT'),
    'event-not-removed': ('    if(rbt_lvl.cur>=0) rbt_remove((unsigned)rbt_lvl.cur);\n', ''),
    'remove-twice': ('    if(rbt_inst[k].removed) return;\n', ''),
    'remove-focus-kept': ('WORD_AT(player+RBT_FOCUS)==ent) WORD_AT(player+RBT_FOCUS)=0;', 'WORD_AT(player+RBT_FOCUS)==ent) {}'),
    'remove-focus-any': ('    if(MAPMAKER_PTR_OK(player,RBT_FOCUS+4u) && WORD_AT(player+RBT_FOCUS)==ent) WORD_AT', '    if(MAPMAKER_PTR_OK(player,RBT_FOCUS+4u)) WORD_AT'),
    'fx-missing': ('    guest_call(sub_002A7F30,2,WORD_AT(cam+RBT_CAMERA_VIEW),RBT_FX_SCALE);\n', ''),
    'fx-no-mode': ('    guest_call(sub_00019D70,1,1u);\n', ''),
    'fx-scale': ('#define RBT_FX_SCALE 0x40200000u', '#define RBT_FX_SCALE 0x3F800000u'),
    'fx-view': ('WORD_AT(cam+RBT_CAMERA_VIEW),RBT_FX_SCALE', 'cam,RBT_FX_SCALE'),
    'fx-any-caller': ('MAPMAKER_STACK_OK(4) && WORD_AT(g_esp)==RBT_RET_FX;', 'MAPMAKER_STACK_OK(4);'),
    'fx-alive': (' && WORD_AT(rbt_inst[k].state+RBT_DYING)==1u) return 1;', ') return 1;'),
    'fx-after-removal': ('        if(!rbt_inst[k].removed && MAPMAKER_PTR_OK(rbt_inst[k].state', '        if(MAPMAKER_PTR_OK(rbt_inst[k].state'),
    'fx-split-screen': ('    if(RBT_B(RBT_MODE)==10u && (int32_t)WORD_AT(RBT_PLAYERS)>1) return;', ''),
    'reset-missing': ('    guest_call(sub_00292550,0);\n', ''),
    'reset-any-scene': ('    if(!rbt_lvl.blocks || WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return;', '    if(!rbt_lvl.blocks || !rbt_on()) return;'),
    'reset-keeps-removed': ('        rbt_inst[k].removed=0;\n        rbt_walk[k].hold_ok=0;', '        rbt_walk[k].hold_ok=0;'),
    'hud-own-bar': ('    WORD_AT(player+RBT_FOCUS)=0;                     /* never the stock bar', '    (void)0;                     /* never the stock bar'),
    'hud-keeps-cleared': ('    original();\n    WORD_AT(player+RBT_FOCUS)=focus;\n}', '    original();\n}'),
    'hud-any-robot': ('    if(k==rbt_lvl.n) { original();return; }\n    WORD_AT(player+RBT_FOCUS)=0;', '    WORD_AT(player+RBT_FOCUS)=0;'),
    'hud-any-model': ('       RBT_H(focus+RBT_ENT_MODEL)!=RBT_MODEL || !MAPMAKER_PTR_OK', '       !MAPMAKER_PTR_OK'),
    'hud-any-scene': ('RBT_ACTOR) || !rbt_play()) { original();return; }', 'RBT_ACTOR) || !rbt_on()) { original();return; }'),
    'level-one-block': ('    for(k=0;k<RBT_MAX;k++) {\n        WORD_AT(RBT_STATE)=0;', '    for(k=0;k<1u;k++) {\n        WORD_AT(RBT_STATE)=0;'),
    'level-outside-last': ('    WORD_AT(RBT_STATE)=rbt_inst[0].state;              /* outside any robot', '    (void)0;              /* outside any robot'),
    'level-parts-unchecked': ('    if(!pt || !MAPMAKER_PTR_OK(pt,RBT_PARTS_SIZE)) { rbt_log', '    if(0) { rbt_log'),
    'level-keeps-instances': ('    memset(rbt_inst,0,sizeof rbt_inst);\n', ''),
    'class-beyond-blocks': ('    else if(k==rbt_lvl.n && rbt_lvl.n>=rbt_lvl.blocks) {', '    else if(k==rbt_lvl.n && rbt_lvl.n>=RBT_MAX) {'),
    'class-one-robot': ('    else if(k==rbt_lvl.n && rbt_lvl.n>=rbt_lvl.blocks) {', '    else if(k==rbt_lvl.n && rbt_lvl.n>=1u) {'),
    'class-restart-new-slot': ('    for(k=0;k<rbt_lvl.n && rbt_inst[k].record!=rec;k++) {}\n    if(rbt.optout)', '    k=rbt_lvl.n;\n    if(rbt.optout)'),
    'class-stale-statics': ('        memset(rbt_inst[k].statics,0,sizeof rbt_inst[k].statics);\n        rbt_walk[k].hold_ok=0;                         /* (re)spawned', '        rbt_walk[k].hold_ok=0;                         /* (re)spawned'),
    'swap-none': ('    rbt_swapped(original,ent);\n}\n/* WRAP sub_00294520', '    original();\n}\n/* WRAP sub_00294520'),
    'swap-no-message': ('    rbt_swapped(original,WORD_AT(g_esp+4u));\n}\n/* WRAP sub_002926B0', '    original();\n}\n/* WRAP sub_002926B0'),
    'swap-no-callback': ('    rbt_swapped(original,WORD_AT(g_esp+4u));\n}\n/* WRAP sub_002936C0', '    original();\n}\n/* WRAP sub_002936C0'),
    'swap-no-damage': ('    rbt_swapped(original,WORD_AT(r));', '    original();'),
    'swap-damage-record': ('    rbt_swapped(original,WORD_AT(r));', '    rbt_swapped(original,r);'),
    'swap-no-state': ('    WORD_AT(RBT_STATE)=rbt_inst[k].state;\n    prev=', '    prev='),
    'swap-state-kept': ('    WORD_AT(RBT_STATE)=keep[0];\n    rbt_lvl.swaps++;', '    rbt_lvl.swaps++;'),
    'swap-no-statics': ('keep[1u+i]=WORD_AT(RBT_STATICS+i*4u);WORD_AT(RBT_STATICS+i*4u)=rbt_inst[k].statics[i]; }', 'keep[1u+i]=WORD_AT(RBT_STATICS+i*4u); }'),
    'swap-statics-lost': ('rbt_inst[k].statics[i]=WORD_AT(RBT_STATICS+i*4u);WORD_AT(RBT_STATICS+i*4u)=keep[1u+i]; }', 'WORD_AT(RBT_STATICS+i*4u)=keep[1u+i]; }'),
    'swap-statics-leak': ('rbt_inst[k].statics[i]=WORD_AT(RBT_STATICS+i*4u);WORD_AT(RBT_STATICS+i*4u)=keep[1u+i]; }', 'rbt_inst[k].statics[i]=WORD_AT(RBT_STATICS+i*4u); }'),
    'swap-two-statics': ('#define RBT_STATICS_N 3u', '#define RBT_STATICS_N 2u'),
    'swap-wrong-statics': ('#define RBT_STATICS 0x0075F5ECu', '#define RBT_STATICS 0x0075F5E8u'),
    'swap-cur-kept': ('    rbt_lvl.cur=prev;\n', ''),
    'swap-no-cur': ('    prev=rbt_lvl.cur;rbt_lvl.cur=k;', '    prev=rbt_lvl.cur;'),
    'find-any-actor': ('if(rbt_inst[k].actor==a && rbt_inst[k].record==rec) return (int)k;', 'if(rbt_inst[k].record==rec) return (int)k;'),
    'find-any-record': ('if(rbt_inst[k].actor==a && rbt_inst[k].record==rec) return (int)k;', 'if(rbt_inst[k].actor==a) return (int)k;'),
    'find-any-scene': ('       WORD_AT(RBT_SCENE)!=100u || !rbt_on()) return -1;', '       !rbt_on()) return -1;'),
    'find-first-only': ('    for(k=0;k<rbt_lvl.n;k++) if(rbt_inst[k].actor==a', '    for(k=0;k<1u && k<rbt_lvl.n;k++) if(rbt_inst[k].actor==a'),
    'event-removes-first': ('    if(rbt_lvl.cur>=0) rbt_remove((unsigned)rbt_lvl.cur);', '    rbt_remove(0u);'),
    'remove-unchecked': ('    if(!MAPMAKER_PTR_OK(a,RBT_ACTOR) || WORD_AT(a+RBT_ACTOR_RECORD)!=rbt_inst[k].record ||', '    if(!MAPMAKER_PTR_OK(a,RBT_ACTOR) ||'),
    'fx-first-only': ('    for(k=0;k<rbt_lvl.n;k++)\n        if(!rbt_inst[k].removed', '    for(k=0;k<1u && k<rbt_lvl.n;k++)\n        if(!rbt_inst[k].removed'),
    'fx-any-scene': ('    if(!here || WORD_AT(RBT_SCENE)!=100u || !rbt_on() || !rbt_any_dying()) return;', '    if(!here || !rbt_on() || !rbt_any_dying()) return;'),
    'reset-first-only': ('    for(k=0;k<rbt_lvl.blocks;k++) {\n        WORD_AT(RBT_STATE)=rbt_inst[k].state;', '    for(k=0;k<1u;k++) {\n        WORD_AT(RBT_STATE)=rbt_inst[k].state;'),
    'reset-one-state': ('        WORD_AT(RBT_STATE)=rbt_inst[k].state;\n        guest_call(sub_00292550,0);', '        guest_call(sub_00292550,0);'),
    'reset-keeps-statics': ('        memset(rbt_inst[k].statics,0,sizeof rbt_inst[k].statics);\n        rbt_inst[k].removed=0;', '        rbt_inst[k].removed=0;'),
    'reset-pointer-kept': ('    WORD_AT(RBT_STATE)=keep;\n    if(rbt_lvl.resets', '    if(rbt_lvl.resets'),
    'hud-first-only': ('    for(k=0;k<rbt_lvl.n && rbt_inst[k].record!=rec;k++) {}\n    if(k==rbt_lvl.n) { original();return; }', '    for(k=0;k<1u && k<rbt_lvl.n && rbt_inst[k].record!=rec;k++) {}\n    if(k==rbt_lvl.n || rbt_inst[k].record!=rec) { original();return; }'),
    # v0.4.0-mapmaker.40: through openings lower than the robot
    'goal-any-caller': ('WORD_AT(g_esp)!=RBT_RET_GOAL) { original();return; }', '0) { original();return; }'),
    'goal-any-entity': ('    if((k=rbt_find(ent))<0 || !rbt_nav_build()', '    if((k=0)<0 || !rbt_nav_build()'),
    'goal-kept': ('    if(to==goal) { original();return; }\n', '    if(1) { original();return; }\n'),
    'goal-actor-kept': ('WORD_AT(sp+8u)=to;WORD_AT(a+RBT_AI_GOAL)=to;', 'WORD_AT(sp+8u)=to;'),
    'goal-slot-kept': ('    original();\n    WORD_AT(sp+8u)=goal;\n', '    original();\n'),
    'goal-door-ignored': ('            if(rbt_nav.open[e]<need) { if(!dist) rbt_walk[k].blocked++;continue; }', '            (void)need;'),
    'goal-no-slack': ('#define RBT_FIT_SLACK 0.25f', '#define RBT_FIT_SLACK 0.0f'),
    'goal-height-fixed': ('    return rbt_sane(h) && h>=1.0f && h<=40.0f?h:RBT_HEIGHT;', '    return RBT_HEIGHT;'),
    'goal-height-box-centre': ('rbt_getf(ent+RBT_ENT_BOX_Y)+rbt_getf(ent+RBT_ENT_BOX_H);', 'rbt_getf(ent+RBT_ENT_BOX_Y);'),
    'goal-farthest-portal': ('if(best<0.0f || d<best-0.01f || (d<=best+0.01f && p.lo<best_lo))', 'if(best<0.0f || d>best+0.01f || (d>=best-0.01f && p.lo<best_lo))'),
    'goal-portal-any-rooms': ("        if(!((p.r0==ra && p.r1==rb) || (p.r0==rb && p.r1==ra))) continue;\n        d=", '        d='),
    'goal-no-substitute': ('        goal=rbt_walk[k].goal_sub;\n', ''),
    'goal-substitute-farthest': ('if(rbt_sane(d) && (bd<0.0f || d<bd)) { bd=d;best=p; }', 'if(rbt_sane(d) && (bd<0.0f || d>bd)) { bd=d;best=p; }'),
    'goal-no-steer': ('    if(!rbt_walk[k].blocked || goal==at) return goal;', '    return goal;'),
    'goal-steer-whole-leg': ('rbt_walk[k].dist[rbt_nav.adj[e]]+1u==rbt_walk[k].dist[at]) return rbt_nav.adj[e];\n    return at;', 'rbt_walk[k].dist[rbt_nav.adj[e]]+1u==rbt_walk[k].dist[at]) return goal;\n    return at;'),
    'goal-steer-any-link': ('        if(rbt_nav.open[e]>=rbt_walk[k].height-RBT_FIT_SLACK && rbt_walk[k].dist[rbt_nav.adj[e]]+1u==rbt_walk[k].dist[at]) return', '        if(rbt_walk[k].dist[rbt_nav.adj[e]]+1u==rbt_walk[k].dist[at]) return'),
    'goal-no-tell': ('        rbt_tell(k);\n', ''),
    'goal-no-rebuild': ('    if(!rbt_walk[k].built || (rbt_walk[k].from!=at && !rbt_walk[k].reach[at])) {', '    if(!rbt_walk[k].built) {'),
    'goal-nav-kept': ('    rbt_nav_free();\n    memset(rbt_walk,0,sizeof rbt_walk);\n', '    memset(rbt_walk,0,sizeof rbt_walk);\n'),
    # v0.4.0-mapmaker.44: stacked one-floor portals and the hold
    'stack-none': ('    while(pmax<=RBT_PORTAL_MAX && n<=RBT_STACK_MAX) {', '    while(0 && n<=RBT_STACK_MAX) {'),
    'stack-slab-ignored': ('            if(rbt_joined(c.r0,a) && rbt_joined(c.r1,b)) {', '            if(1) {'),
    'stack-any-plane': (' || !rbt_coplanar(&p,&c)) continue;', ') continue;'),
    'stack-gap-jumped': ('            if(c.lo>top+RBT_STACK_GAP || c.lo<top-RBT_STACK_GAP || !rbt_coplanar', '            if(!rbt_coplanar'),
    'stack-floor-closed': ('    if(p.hi-p.lo<RBT_WALL_MIN) return RBT_OPEN;\n', ''),
    'stack-joined-other': ('    if(x==y) return 1;\n', ''),
    'stack-joined-by-door': (' && q.hi-q.lo<RBT_WALL_MIN && ((q.r0==x', ' && ((q.r0==x'),
    'stack-plane-span': ('    return over>=0.1f && over>=0.5f*', '    return over>=-1.0e9f || over>=0.5f*'),
    'stack-highest-portal': ('(d<=best+0.01f && p.lo<best_lo)', '(d<=best+0.01f && p.lo>best_lo)'),
    'hold-none': ('    if((k=rbt_find(ent))>=0) rbt_hold((unsigned)k,ent);\n', ''),
    'hold-after-ai': ('    if((k=rbt_find(ent))>=0) rbt_hold((unsigned)k,ent);\n    rbt_swapped(original,ent);', '    rbt_swapped(original,ent);\n    if((k=rbt_find(ent))>=0) rbt_hold((unsigned)k,ent);'),
    'hold-room-kept': ('            WORD_AT(ent+RBT_ENT_ROOM)=rbt_walk[k].hold_room;\n', ''),
    'hold-pos-kept': ('            memcpy((void *)((uintptr_t)(ent+RBT_ENT_POS)+g_xbox_mem_offset),rbt_walk[k].hold,sizeof rbt_walk[k].hold);\n', ''),
    'hold-teleport': ('        if(d<=RBT_HOLD_JUMP*RBT_HOLD_JUMP &&\n', '        if(\n'),
    'hold-removed': ('    if(rbt_inst[k].removed || !rbt_room_ok(room)) return;', '    if(!rbt_room_ok(room)) return;'),
    'hold-no-room-check': ('    if(rbt_inst[k].removed || !rbt_room_ok(room)) return;', '    if(rbt_inst[k].removed) return;'),
    'hold-restart-kept': ('        rbt_walk[k].hold_ok=0;                         /* back at its spawn point */\n', ''),
    'hold-respawn-kept': ('        rbt_walk[k].hold_ok=0;                         /* (re)spawned: no previous position */\n', ''),
    'hold-follows': ('            return;\n        }\n    }\n    rbt_walk[k].hold_ok=1;', '        }\n    }\n    rbt_walk[k].hold_ok=1;'),
    'hold-no-slack': ('<rbt_walk[k].height-RBT_FIT_SLACK) {\n            memcpy', '<rbt_walk[k].height+1.0f) {\n            memcpy'),
    'goal-unused-links': ('        if(!(WORD_AT(l)&RBT_LINK_USED) || (a=WORD_AT(l+4u))>=np || (b=WORD_AT(l+8u))>=np || a==b) continue;\n        o=', '        if((a=WORD_AT(l+4u))>=np || (b=WORD_AT(l+8u))>=np || a==b) continue;\n        o='),
}



def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('robot.exe' if os.name == 'nt' else 'robot')
    source = ROOT / 'tests/mapmaker/mapmaker_robot.c'
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
def test_mapmaker_robot(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_REAL_BOSS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker robot ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table_and_no_save_byte():
    """Wrappers only; the map container is only read."""
    assert not (ROOT / 'config/guest-code-patches/robot.json').exists()
    text = INC.read_text(encoding='utf-8')
    assert not re.findall(r'RBT_[BH]\([^)]*\)\s*=[^=]', text)


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


def calls(f):
    return {i.address for i in decoded() if i.call and target(i) == f}


def direct_graph():
    graph = {}
    for i in decoded():
        if i.call:
            imm = [op[1] for op in i.ops if op[0] == 'imm']
            if imm:
                graph.setdefault(i.function, set()).add(imm[0])
    return graph


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


HANDLER, MESSAGE = 0x00293730, 0x00294520
CALLBACKS = (0x000BC470, 0x002926B0, 0x002936C0)
ROBOT = (HANDLER, MESSAGE, 0x002926B0, 0x002936C0, 0x00292BF0, 0x00292FA0, 0x00293100, 0x00293660)


def test_robot_data():
    rd = xbe_reader()
    voice, model, lod, flags = struct.unpack_from('<4I', rd(0x0053A3CC + 0x42 * 0x30, 16), 0)
    assert (model, lod) == (0x5F2, 0xFFFFFFFF) and flags & 0x2000000
    assert voice == struct.unpack_from('<I', rd(0x0053A3CC + 0xA8 * 0x30, 4))[0]   # the Arcade giant robot's
    ids = [struct.unpack_from('<I', rd(0x00521A18 + i * 0x30 + 4, 4))[0] for i in range(150)]
    assert 0x42 not in ids and ids.index(0xA8) == 90                  # no Arcade row; the stand-in has one
    assert rd(0x002BE29C + 0x42 - 0x0A, 1)[0] == 1                   # sub_002BE270(0x42) == 0
    assert rd(0x0049DC78, 30) == b'pak/stream/lv14/anim/lv14.pak\0'
    lv14 = anim_table(rd, 0x0047E938, 0x29)
    assert (lv14['giantrob_startthroughdoor'], lv14['giantrob_walkcycle'], lv14['giantrob_death']) == (0x807, 0x808, 0x809)
    others = (anim_table(rd, 0x0048C1B0, 0x10), anim_table(rd, 0x0048C738, 0x12), anim_table(rd, 0x00484F68, 0x14))
    assert not set(lv14.values()) & set().union(*(set(t.values()) for t in others))   # all may be mounted
    # sub_0025F720: only scene 14 reaches the robot case; scene 100 is past the table
    jt = [struct.unpack_from('<I', rd(0x0025F7C0 + 4 * k, 4))[0] for k in range(6)]
    assert [s for s in range(1, 0x52) if jt[rd(0x0025F7D8 + s - 1, 1)[0]] == 0x0025F760] == [14]
    # HUD: model 0x5F2 -> the case at 0x0014D06F (no scene test), label 0x163D
    idx = rd(0x0014D174 + 0x5F2 - 0x5CB, 1)[0]
    assert struct.unpack_from('<I', rd(0x0014D158 + 4 * idx, 4))[0] == 0x0014D06F
    # handler state table: 7 states, 0 = intro, 1 = door entry, 6 = death
    st = struct.unpack('<7I', rd(0x002944FC, 28))
    assert st[0] == 0x00293847 and st[1] == 0x002938B4 and st[6] == 0x00293FB3


def test_robot_registration_and_dispatch():
    a = by_address()
    assert ('imm', 0x0049DC78) in a[0x001CA316].ops and a[0x001CA316].function == 0x001CA2F0
    assert target(a[0x001CA31B]) == 0x00062260 and ('imm', 0x0047E938) in a[0x001CA32A].ops
    assert ('imm', 0x29) in a[0x001CA328].ops and target(a[0x001CA334]) == 0x0005B980
    # class 0x80000 (AI +0x10 == 0): sub_00293730(entity) and the message handler sub_00294520
    assert ('imm', 0x80000) in a[0x0027DDF5].ops and a[0x0027DDF5].function == 0x0027D590
    assert calls(HANDLER) == {0x0027DE18} and a[0x0027DE18].function == 0x0027D590
    assert ('imm', 0x80000) in a[0x0027D402].ops and calls(MESSAGE) == {0x0027D447}
    # the handler reads the entity at [esp+0x88] (0x74 frame, 4 pushes) and the state unchecked
    assert ('imm', 0x74) in a[0x00293730].ops and mem(a[0x00293737])[0][4] == 0x88
    assert mem(a[0x002937DC])[0][4] == 0x0078B1DC and a[0x002937E2].mnemonic == 'mov'
    # state 0: callbacks, health 55, story focus; state 1: 0x807 until its end
    assert ('imm', 0x000BC470) in a[0x00293858].ops and ('imm', 0x002926B0) in a[0x002938EE].ops
    assert ('imm', 0x002936C0) in a[0x00293923].ops
    assert mem(a[0x0029388C])[0][4] == 0x004785CC and mem(a[0x002938A9])[0][4] == 0xFF4
    assert ('imm', 0x807) in a[0x002938B4].ops and target(a[0x002938DE]) == 0x00059DD0


def test_robot_level_state():
    a = by_address()
    assert target(a[0x0025F760]) == 0x0028D6D0 and target(a[0x0025F765]) == 0x00292630
    assert target(a[0x0025F76A]) == 0x002A62C0
    assert calls(0x00292630) == {0x0025F765} and calls(0x0025F720) == {0x0015E818}
    # sub_00292630: [0x0078B1DC] = sub_0003DFA0(0x4C, 1), then the field init sub_00292550
    assert ('imm', 1) in a[0x00292630].ops and ('imm', 0x4C) in a[0x00292632].ops
    assert target(a[0x00292634]) == 0x0003DFA0 and mem(a[0x0029263C])[0][4] == 0x0078B1DC
    assert a[0x00292641].mnemonic == 'jmp' and target(a[0x00292641]) == 0x00292550
    assert ('imm', 0x7D00) in a[0x002A62C2].ops and mem(a[0x002A62CF])[0][4] == 0x0078B0D0
    # sub_00292550 only writes the block (no scene, no call); +0x48 starts at 0
    assert not scene_readers({0x00292550}) and not any(i.call for i in decoded() if i.function == 0x00292550)
    assert mem(a[0x00292622])[0][4] == 0x48
    # no robot function reads the split Crowe's block or the walker/skeleton states
    for g in (0x0075F5E8, 0x0078B1E4, 0x0078B1E8, 0x0078B1EC):
        assert not [i for i in decoded() if i.function in ROBOT and any(op[0] == 'mem' and not op[1] and op[4] == g for op in i.ops)]


def test_robot_scene_events_and_tags():
    a = by_address()
    graph = direct_graph()
    # the fight reads no scene; its only direct callee that does is the level event
    assert not scene_readers(set(ROBOT))
    assert scene_readers(graph[HANDLER]) == {0x002274C0}
    for f in (MESSAGE, *CALLBACKS):
        assert not scene_readers(graph.get(f, set()) | {f})
    seen = reach(graph, [HANDLER, MESSAGE, *CALLBACKS])
    assert len(seen) > 100
    sites = {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x002274C0}
    assert sites == {0x002944E0} and ends(a, 0x002944E0) == 0x002944E5
    assert ('imm', 2) in a[0x002944DE].ops and ('imm', 0x3F000000) in a[0x002944D9].ops
    assert mem(a[0x002944EE])[0][4] == 0xFF4                        # the focus is cleared after it
    # the music change right before it: sub_000462B0(0) = [0x004D1850] = track
    assert target(a[0x002944D4]) == 0x000462B0 and ends(a, 0x002944D4) == 0x002944D9
    assert mem(a[0x000462B4])[0][4] == 0x004D1850
    # it runs when the death animation has ended (sub_00059DD0 on entity +4), every frame
    assert target(a[0x002944C7]) == 0x00059DD0 and '0x2944f4' in a[0x002944D1].text
    # no tagged-object lookup is reachable; no height is forced; no overhead meter record
    assert not {i.address for i in decoded() if i.call and i.function in seen and target(i) == 0x0023FF20}
    for i in decoded():
        if i.function in ROBOT and i.mnemonic in ('mov', 'movss', 'fstp') and i.ops and i.ops[0][0] == 'mem':
            assert not (i.ops[0][4] == 0x68 and i.ops[0][1] not in ('esp', 'ebp')), hex(i.address)
    assert not {i.function for i in decoded() if i.call and target(i) == 0x0012C800} & set(ROBOT)


def test_robot_death_message():
    a = by_address()
    # message 2: unless already state 6, +0x48 = 1, death anim 0x809, AI +0x9D0 = 6, -1 (no generic death)
    assert mem(a[0x0029454C])[0][4] == 0x20 and ('imm', 6) in a[0x00294550].ops
    assert mem(a[0x00294697])[0][4] == 0x48 and mem(a[0x00294687])[0][4] == 0x0078B1DC
    assert ('imm', 0x809) in a[0x002946F9].ops and target(a[0x002946FF]) == 0x0005F460
    assert mem(a[0x0029470F])[0][4] == 0x9D0
    assert a[0x00294728].mnemonic == 'or' and ('imm', 0xFFFFFFFF) in a[0x00294728].ops


def test_robot_pad_lookups():
    """The walk's hangar pad ids: seven existence tests, two index fetches after a found id."""
    a = by_address()
    sites = {i.address for i in decoded() if i.call and i.function == HANDLER and target(i) == 0x0009CC40}
    tests = {0x00293944, 0x00293BC4, 0x00293C0B, 0x00293C26, 0x00293C5F, 0x00293C99, 0x00293CCB}
    assert sites == tests | {0x00293981, 0x00293CE0}
    assert sorted(ends(a, x) for x in tests) == [0x00293949, 0x00293BC9, 0x00293C10, 0x00293C2B, 0x00293C64, 0x00293C9E, 0x00293CD0]
    for x in tests:                                                  # add esp, 4; test eax, eax; jl/jge
        r = ends(a, x)
        assert a[r].mnemonic == 'add' and a[ends(a, r)].mnemonic == 'test'
        assert a[ends(a, ends(a, r))].mnemonic in ('jl', 'jge')
    # the fetches: 0x0029397C only after the 0x00293944 test found it; 0x00293CE0 from the push blocks
    branches = {i.address: target(i) for i in decoded() if i.function == HANDLER and i.branch}
    assert [s for s, t in branches.items() if t == 0x0029397C] == [0x0029394E] and a[0x0029394E].mnemonic == 'jge'
    assert sorted(s for s, t in branches.items() if t == 0x00293CE0) == [0x00293BF2, 0x00293C1C, 0x00293C37, 0x00293C83, 0x00293CC4]
    assert branches[0x00293BCE] == 0x00293BED and branches[0x00293C69] == 0x00293C7E and branches[0x00293CA3] == 0x00293CBF
    assert a[0x00293C15].mnemonic == 'jl' and a[0x00293C30].mnemonic == 'jl' and a[0x00293CD5].mnemonic == 'jl'
    # sub_0009CC40: a binary search of the pad table [0x00715F2C] (+0 id, stride 0x20)
    assert mem(a[0x0009CC6B])[0][4] == 0x00715F2C and mem(a[0x0009CC5A])[0][4] == 0x007B6664
    # missing id -> the player's own pad (+0x678 of the first player's actor)
    assert mem(a[0x00293C76])[0][4] == 0x678 and mem(a[0x00293C6B])[0][4] == 0x007B0C48


def test_robot_explosion_draw():
    a = by_address()
    # the frame draws the robot's particles only in scene 14 (sub_00292650, its only caller)
    assert calls(0x00292650) == {0x001CEE58} and ('imm', 0xE) in a[0x001CEE49].ops
    assert ('imm', 0x4D454348) in a[0x00292650].ops and target(a[0x00292655]) == 0x0023FF20   # 'MECH', null-checked
    assert a[0x0029265F].mnemonic == 'je' and mem(a[0x0029266A])[0][4] == 0x48
    assert mem(a[0x00292676])[0][4] == 0x007DE455 and mem(a[0x0029267F])[0][4] == 0x00790950
    assert target(a[0x00292688]) == 0x00019D70 and mem(a[0x00292692])[0][4] == 0x208
    assert ('imm', 0x40200000) in a[0x00292698].ops and target(a[0x0029269E]) == 0x002A7F30
    assert ('imm', 0xC) in a[0x002926A3].ops                       # cdecl: 1 + 2 args
    assert calls(0x00126560) == {0x001CEE87} and ends(a, 0x001CEE87) == 0x001CEE8C
    # sub_002A7F30 only draws: its stores are to its own frame
    for i in decoded():
        if i.function == 0x002A7F30 and i.mnemonic in ('mov', 'movss', 'fstp') and i.ops and i.ops[0][0] == 'mem':
            assert i.ops[0][1] == 'esp', hex(i.address)


def test_robot_soft_restart_reset():
    a = by_address()
    # sub_0015E4F0 (story mode) tail-jumps to sub_0025F6A0, which resets the robot only in scene 14
    assert a[0x0015E7B1].mnemonic == 'jmp' and target(a[0x0015E7B1]) == 0x0025F6A0
    assert ('imm', 0xE) in a[0x0025F6C8].ops and target(a[0x0025F6D2]) == 0x00292550
    assert calls(0x00292550) == {0x0025F6D2}
    # the restart path of the scene loop (sub_001CDD60) runs after a level init, apart from it
    assert target(a[0x001CDD74]) == 0x0015E4F0 and calls(0x0015E4F0) == {0x001CDD74}
    assert calls(0x001CDD60) == {0x0003D4E1} and calls(0x001CDB80) == {0x0003D590}
    assert a[0x0003D4BB].mnemonic == 'je' and '0x3d4f0' in a[0x0003D4BB].text


def test_robot_bar_label_site():
    a = by_address()
    assert ('imm', 0x163D) in a[0x0014D076].ops and target(a[0x0014D07B]) == 0x0003D7B0
    assert mem(a[0x0014D086])[0][4] == 0xFF4 and target(a[0x0014D0B1]) == 0x00139740


def test_robot_per_instance_globals():
    """The facts the multi-robot swap relies on: which globals are the robot's
    own, every way into the code that reads them, and a fresh block per call."""
    a = by_address()

    def users(value):
        return {i.function for i in decoded() if any(v == value for _, v in i.values())}
    robot = {0x00292550, 0x00292630, 0x00292650, 0x002926B0, 0x00292BF0, 0x00293100, 0x00293660,
             0x002936C0, HANDLER, MESSAGE}
    # the state pointer: robot code only
    assert users(0x0078B1DC) == robot
    # the three statics: the shot counter in sub_00293100, the two timers in the handler
    assert users(0x0075F5EC) == {0x00293100}
    assert users(0x0075F5F0) == {HANDLER}
    assert users(0x0075F5F4) == {HANDLER, 0x002B26F0}       # also a table base elsewhere (index * 4)
    assert all('*4' in i.text.replace(' ', '') for i in decoded()
               if i.function == 0x002B26F0 and any(v == 0x0075F5F4 for _, v in i.values()))
    # the robot never touches the split Crowe's block pointer next to them
    assert not users(0x0075F5E8) & robot
    # every state reader is reached only through the four wrapped entries (or the
    # scene-14 reset / draw and the allocation)
    callers = {f: {i.function for i in decoded() if (i.call or i.mnemonic == 'jmp') and target(i) == f}
               for f in robot}
    assert callers[HANDLER] == {0x0027D590} and calls(HANDLER) == {0x0027DE18}
    assert callers[MESSAGE] == {0x0027CEF0} and calls(MESSAGE) == {0x0027D447}
    assert callers[0x00292BF0] == {HANDLER} and callers[0x00293660] == {HANDLER} and callers[0x00293100] == {0x00293660}
    assert callers[0x00292550] == {0x0025F6A0, 0x00292630} and callers[0x00292650] == {0x001CEBC0}
    assert not callers[0x002926B0] and not callers[0x002936C0]               # callbacks: pushed, not called
    pushed = {f: {i.address for i in decoded() if i.mnemonic == 'push' and ('imm', f) in i.ops} for f in (0x002926B0, 0x002936C0)}
    assert pushed == {0x002926B0: {0x002938EE}, 0x002936C0: {0x00293923}}
    assert target(a[0x002938FA]) == 0x000A5CE0 and target(a[0x00293929]) == 0x0009E720
    # entity arguments: handler, message handler and slot callback take it first; the damage record holds it at +0
    assert mem(a[0x00294545])[0][4] == 0x40 and ('imm', 0x34) in a[0x00294524].ops       # 0x34 frame + 2 pushes
    assert mem(a[0x002926BF])[0][4] == 0xE8 and ('imm', 0xE0) in a[0x002926B0].ops       # 0xE0 frame + 1 push
    assert mem(a[0x002936C0])[0][4] == 4 and mem(a[0x002936C4])[0] [1] == 'eax'
    # sub_00292630: a fresh 0x4C block from the level heap on every call, then sub_00292550
    assert ('imm', 0x4C) in a[0x00292632].ops and target(a[0x00292634]) == 0x0003DFA0
    assert mem(a[0x0029263C])[0][4] == 0x0078B1DC and a[0x00292641].mnemonic == 'jmp' and target(a[0x00292641]) == 0x00292550
    # the intro re-arms the shared particle pool (sub_002A65A0), one reason it stays shared
    assert target(a[0x0029387F]) == 0x002A62F0 and target(a[0x00293887]) == 0x002A65A0


def test_walk_goal_and_pad_graph():
    """v0.4.0-mapmaker.40: the handler's walk goal call and the pad, link,
    portal and box fields the robot's own reachable pad set reads."""
    a = by_address()
    # sub_00293730: push [actor+0x684]; push entity; call sub_0015D500 (return 0x00293D27)
    assert mem(a[0x00293D1A])[0][4] == 0x684 and target(a[0x00293D22]) == 0x0015D500
    assert ends(a, 0x00293D22) == 0x00293D27
    # sub_0015D500: the goal compare on actor +0x684
    assert mem(a[0x0015D580])[0][4] == 0x684
    # the handler's arrival test: +0x678 (the pad it is at) against +0x684
    assert mem(a[0x00293B21])[0][4] == 0x678 and mem(a[0x00293B1B])[0][4] == 0x684
    # links (sub_0009A5B0): 0x14 each, +0 flags 0x10010, +4/+8 pads; pool [0x00715F30], count [0x007B6660]
    assert ('imm', 0x10010) in a[0x0009A603].ops and mem(a[0x0009A609])[0][4] == 4 and mem(a[0x0009A60C])[0][4] == 8
    assert mem(a[0x0009A5E7])[0][4] == 0x00715F30 and mem(a[0x0009A5D8])[0][4] == 0x007B6660
    # pads: room word +0xA from the floor finder (0x0009AFDF), the pad table [0x00715F2C]
    assert mem(a[0x0009AFDF])[0][4] == 0xA and mem(a[0x0009AFBF])[0][4] == 0x00715F2C
    # portals: +0/+4 rooms (sub_00087F80), +0x14 vertex count, +0x18 vertices of 0xC
    assert mem(a[0x00088089])[0][4] == 0 and mem(a[0x00088093])[0][4] == 4 and mem(a[0x00088075])[0][4] == 0x0070878C
    assert mem(a[0x0008C630])[0][4] == 0x14 and a[0x0008C63E].mnemonic == 'lea' and mem(a[0x0008C63E])[0][4] == 0x18
    assert ('imm', 0xC) in a[0x0008BE36].ops
    # the entity box: centre +0x1A8.., half extents +0x1C0..
    assert mem(a[0x0006B632])[0][4] == 0x1AC and mem(a[0x0006B65F])[0][4] == 0x1C4
    inc = INC.read_text(encoding='utf-8')
    for k in ('RBT_RET_GOAL 0x00293D27u', 'RBT_PADS 0x00715F2Cu', 'RBT_LINKS 0x00715F30u', 'RBT_LINK_COUNT 0x007B6660u',
              'RBT_PAD_COUNT 0x007B6664u', 'RBT_ROOM_RECS 0x00708788u', 'RBT_PORTALS 0x0070878Cu', 'RBT_AI_GOAL 0x684u',
              'RBT_AI_PAD 0x678u', 'RBT_ENT_BOX_Y 0x1ACu', 'RBT_ENT_BOX_H 0x1C4u'):
        assert '#define ' + k in inc, k


def test_root_motion_move_before_handler():
    """v0.4.0-mapmaker.44: the robot is moved by its animation's root motion in
    the dispatcher just before its handler, not by its pads: sub_0015F0D0
    reads the model's root delta, stores the position and sets the room +0x34
    again from it; the attack states keep the walk cycle playing."""
    a = by_address()
    # sub_0027D590, class 0x80000: sub_0015F0D0(entity) at 0x0027DE12, then the handler at 0x0027DE18
    assert target(a[0x0027DE12]) == 0x0015F0D0 and target(a[0x0027DE18]) == 0x00293730
    # sub_0015F0D0: root delta [entity+4]+0xFC.., position stores, the room set again (sub_0015AB80)
    assert mem(a[0x0015F134])[0][4] == 4 and mem(a[0x0015F137])[0][4] == 0xFC
    assert [mem(a[s])[0][4] for s in (0x0015F861, 0x0015F86C, 0x0015F871)] == [0x64, 0x68, 0x6C]
    assert target(a[0x0015F850]) == 0x000D9230 and target(a[0x0015F889]) == 0x0015AB80
    assert mem(a[0x0015ACC6])[0][4] == 0x34 and mem(a[0x0015AE36])[0][4] == 0x34
    # the walk cycle 0x808 in the walk and attack states (sub_0005F460)
    for push, site in ((0x00293A8C, 0x00293A95), (0x00293D94, 0x00293D9A), (0x00293EB7, 0x00293EC2), (0x00293FA3, 0x00293FA9)):
        assert ('imm', 0x808) in a[push].ops and target(a[site]) == 0x0005F460
    inc = INC.read_text(encoding='utf-8')
    for k in ('RBT_ENT_POS 0x64u', 'RBT_ENT_ROOM 0x34u'):
        assert '#define ' + k in inc, k
