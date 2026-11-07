"""Compile the MapMaker Story AI templates policy (src/interface/pc_mapmaker_aitemplates.inc)
against fake guest memory, with mutation cases: duplicate / leader-group rows
and actions, numbered copy names, the group-logic toggle, propagation of shared
settings, promotion on unlink/delete, the saved link (chunk +0x32, including
the old template format) and "whole group" logic in play (all members
killed / spotted, including ported boss-class AIs)."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_aitemplates.inc'

MUTATIONS = {
    'always-on': ('static int ait_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }',
                  'static int ait_on(void) { mapmaker_limits_read(); return 1; }'),
    'rows-any-caller': ('if(r!=AIT_ROWS_AT || cb!=AIT_CLOSE ||', 'if(cb!=AIT_CLOSE ||'),
    'rows-map-path': ('if(r!=AIT_ROWS_AT || cb!=AIT_CLOSE ||', 'if(r!=AIT_ROWS_AT ||'),
    'rows-cb-offset': ('cb=WORD_AT(g_esp+0x24u); }', 'cb=WORD_AT(g_esp+0x20u); }'),
    'rows-any-scene': ("WORD_AT(AIT_SCENE)!=102u || !(e=ait_editor())) return;", "!(e=ait_editor())) return;"),
    'rows-type5': ('    if(s>=count || ait_type(e,s)==5u) return;\n    if(ait_link', '    if(s>=count) return;\n    if(ait_link'),
    'rows-no-leader': ('    } else if(ait_has_copies(e,s,count)) {', '    } else if(0) {'),
    'rows-toggle-label': ('&AIT_GROUP?4u:3u,AIT_ROW_LOGIC);', '&AIT_GROUP?3u:4u,AIT_ROW_LOGIC);'),
    'close-type5': ("s<ait_count(e) && ait_type(e,s)!=5u && !ait_action", "s<ait_count(e) && !ait_action"),
    'close-no-toggle': (' || row==AIT_ROW_LOGIC) &&', ') &&'),
    'refused-closes': ('        MAPMAKER_GUEST_RET();                     /* sub_0031CAC0: ret */\n        return;',
                       '        original();\n        return;'),
    'refused-no-ret': ('        MAPMAKER_GUEST_RET();                     /* sub_0031CAC0: ret */\n', ''),
    'limit-off-by-one': ('if(count+1u>AIT_CAP) {', 'if(count+1u>AIT_CAP+1u) {'),
    'no-room-check': ('    if(!guest_call(sub_00300670,1,AIT_CHUNK_BYTES)) {', '    if(0) {'),
    'duplicate-keeps-points': ('{ ait_copy_all(e,s,(uint32_t)c);ait_unplace(e,(uint32_t)c); }', '{ ait_copy_all(e,s,(uint32_t)c); }'),
    'copy-keeps-points': ('    ait_copy_all(e,s,(uint32_t)c);\n    ait_unplace(e,(uint32_t)c);', '    ait_copy_all(e,s,(uint32_t)c);'),
    'unplace-count-only': ('    for(o=AIT_POINT0;o<AIT_STRIDE;o+=4u) WORD_AT(b+o)=0;', ''),
    'unplace-points-only': ('    WORD_AT(b+AIT_POINTS)=0;\n', ''),
    'menu-item-any-scene': ('    if(g_eax || WORD_AT(AIT_SCENE)!=102u) return;\n    if(!ait_menu_full++)', '    if(g_eax) return;\n    if(!ait_menu_full++)'),
    'menu-item-counts-success': ('    if(g_eax || WORD_AT(AIT_SCENE)!=102u) return;\n    if(!ait_menu_full++)', '    if(WORD_AT(AIT_SCENE)!=102u) return;\n    if(!ait_menu_full++)'),
    'menu-item-no-original': ('    original();\n    if(g_eax || WORD_AT(AIT_SCENE)', '    if(g_eax || WORD_AT(AIT_SCENE)'),
    'unplace-short': ('    for(o=AIT_POINT0;o<AIT_STRIDE;o+=4u) WORD_AT(b+o)=0;', '    for(o=AIT_POINT0;o<AIT_STRIDE-0x10u;o+=4u) WORD_AT(b+o)=0;'),
    'duplicate-copies-link': ('for(o=0x28u;o<0x60u;o+=4u) if(o!=AIT_LINK)', 'for(o=0x28u;o<0x60u;o+=4u)'),
    'copy-link-off': ('    WORD_AT(ait_entry(e,c)+AIT_LINK)=(uint32_t)root+1u;', '    WORD_AT(ait_entry(e,c)+AIT_LINK)=(uint32_t)root;'),
    'copy-no-name': ('    ait_copy_name(e,(uint32_t)root,(uint32_t)c,ait_count(e));\n', ''),
    'name-no-free-scan': ('        used[ait_number_of(name,base,bl)]=1;', '        (void)name;'),
    'name-no-strip': ('            if(ait_number_of(name,base,u)>1u) { bl=u;base[bl]=0;break; }', '            (void)u;'),
    'name-no-truncate': ('    if(bl>AIT_BASE_MAX) { bl=AIT_BASE_MAX;base[bl]=0; }', ''),
    'chained-links': ('    if(ait_raw(e,l-1u) || ait_type(e,l-1u)==5u) return -1;\n', ''),
    'link-flag-unmasked': ('static uint32_t ait_raw(uint32_t e,uint32_t i) { return WORD_AT(ait_entry(e,i)+AIT_LINK)&AIT_LINK_MASK; }',
                           'static uint32_t ait_raw(uint32_t e,uint32_t i) { return WORD_AT(ait_entry(e,i)+AIT_LINK); }'),
    'toggle-on-copy': ('        if(root==(int)s) WORD_AT(ait_entry(e,s)+AIT_LINK)^=AIT_GROUP;', '        WORD_AT(ait_entry(e,s)+AIT_LINK)^=AIT_GROUP;'),
    'tidy-off': ('        if((WORD_AT(a)&AIT_GROUP) && (ait_raw(e,k) || !ait_has_copies(e,k,count))) WORD_AT(a)&=~AIT_GROUP;', '        (void)a;'),
    'unlink-leader-no-promote': ('        if(root==(int)s) ait_promote(e,s,count);\n        else if(root>=0)', '        if(root>=0)'),
    'promote-keeps-flag': ('        if(next<0) { next=(int)k;WORD_AT(ait_entry(e,k)+AIT_LINK)=0; }',
                           '        if(next<0) { next=(int)k;WORD_AT(ait_entry(e,k)+AIT_LINK)=WORD_AT(ait_entry(e,leader)+AIT_LINK)&AIT_GROUP; }'),
    'promote-no-relink': ('        else WORD_AT(ait_entry(e,k)+AIT_LINK)=(uint32_t)next+1u;', '        else (void)0;'),
    'sync-facing': ('static const uint32_t ait_shared[]={0x28u,0x2Cu,0x30u,0x34u,0x38u,0x3Cu,0x44u,0x48u,0x4Cu};',
                    'static const uint32_t ait_shared[]={0x28u,0x2Cu,0x30u,0x34u,0x38u,0x3Cu,0x44u,0x48u,0x4Cu,0x54u};'),
    'sync-no-weapon': ('static const uint32_t ait_shared[]={0x28u,0x2Cu,0x30u,0x34u,0x38u,0x3Cu,0x44u,0x48u,0x4Cu};',
                       'static const uint32_t ait_shared[]={0x28u,0x2Cu,0x30u,0x34u,0x38u,0x3Cu,0x44u,0x4Cu};'),
    'sync-no-clamp': ('    if(WORD_AT(b+AIT_POINTS)>1u && type!=1u && type!=3u) WORD_AT(b+AIT_POINTS)=1u;', '    (void)type;'),
    'sync-always': ('        if(WORD_AT(ait_entry(e,s)+ait_shared[i])!=ait.shared[i]) {', '        if(1) {'),
    'flush-forgets': ('    if(keep) ait_snapshot(e,s,count);', ''),
    'flush-no-identity': ('    if(ait.editor!=e || s>=count || WORD_AT(ait_entry(e,s)+0x60u)!=ait.colour) return;',
                          '    if(s>=count) return;'),
    'compile-no-flush': ('    ait_flush(e,1);\n    original();', '    original();'),
    'delete-no-promote': ('    if(i<count && ait_root(e,i,count)==(int)i) ait_promote(e,i,count);\n', ''),
    'delete-no-renumber': ('        else if(l>i+1u) WORD_AT(a)=l-1u;', ''),
    'delete-keeps-orphans': ('        if(l==i+1u) WORD_AT(a)=0;\n        else if', '        if(0) (void)0;\n        else if'),
    'delete-no-tidy': ('        else if(l>i+1u) WORD_AT(a)=l-1u;\n    }\n    ait_tidy(e,count);',
                       '        else if(l>i+1u) WORD_AT(a)=l-1u;\n    }'),
    'save-no-link': ('    if(t>=0) v=AIT_SAVE_NEW|((uint32_t)t+1u);', '    if(t>=0) v=0;'),
    'save-no-marker': ('    if(t>=0) v=AIT_SAVE_NEW|((uint32_t)t+1u);', '    if(t>=0) v=(uint32_t)t+1u;'),
    'save-no-group': ('(WORD_AT(ait_entry(x->e,i)+AIT_LINK)&AIT_GROUP?AIT_SAVE_GROUP:0u);', '0u;'),
    'save-offset': ('#define AIT_CHUNK_LINK 0x32u', '#define AIT_CHUNK_LINK 0x30u'),
    'load-keeps-stale': ('        WORD_AT(entry+AIT_LINK)=value;', '        if(value) WORD_AT(entry+AIT_LINK)=value;'),
    'load-no-base': ('value=base+l;', 'value=l;'),
    'load-chained': ('l-1u!=k && !(x.word[l-1u]&AIT_SAVE_LINK) &&', 'l-1u!=k &&'),
    'load-no-index-check': (' &&\n              WORD_AT(ait_entry(e,base+k)+0x24u)==x.index[k];', ';'),
    'load-group-copy': ('        ok[k]=l && (w&AIT_SAVE_GROUP)==0u && l-1u<n', '        ok[k]=l && l-1u<n'),
    'load-no-group': ('(AIT_SAVE_NEW|AIT_SAVE_GROUP))==(AIT_SAVE_NEW|AIT_SAVE_GROUP)) value=AIT_GROUP;', '0u)) value=0;'),
    'load-no-tidy': ('        WORD_AT(entry+AIT_LINK)=value;\n    }\n    ait_tidy(e,count);', '        WORD_AT(entry+AIT_LINK)=value;\n    }'),
    'migrate-keeps-template': ('            if(lead[l-1u]!=(int16_t)k) value=base+(uint32_t)lead[l-1u]+1u;', '            if(1) value=base+l;'),
    'migrate-leader-linked': ('            if(lead[l-1u]!=(int16_t)k) value=', '            if(1) value='),
    'level-chained': ('|| !present[t] || (word_of[t]&AIT_SAVE_LINK)) {', '|| !present[t]) {'),
    'level-group-always': ('        if(w&AIT_SAVE_NEW) ait_level.group[t]=', '        if(0) ait_level.group[t]='),
    'level-old-no-group': ('        else ait_level.group[t]=1;                /* old template: the group of its copies */', '        else (void)0;'),
    'play-any-scene': ("static int ait_play(void) { return ait_level.active && WORD_AT(AIT_SCENE)==100u && mapmaker_limits.limits; }",
                       "static int ait_play(void) { return ait_level.active && mapmaker_limits.limits; }"),
    'play-leader-no-group': ('(t=ait_level.template_of[i])<0 || !ait_level.group[t]) return -1;', '(t=ait_level.template_of[i])<0) return -1;'),
    'event-no-restore': ('    guest_call(original,1,actor);\n    WORD_AT(record+8u)=id;', '    guest_call(original,1,actor);'),
    'event-no-redeliver': ('    guest_call(original,1,actor);\n    WORD_AT(record+8u)=id;', '    WORD_AT(record+8u)=id;'),
    'remove-no-copies': ('        if(ait_level.template_of[k]==(int)t) guest_call(original,1,ait_play_id(k));', '        (void)k;'),
    'remove-no-group': ('>=AIT_LEVEL_SLOTS || !ait_level.group[t]) return;\n    for', '>=AIT_LEVEL_SLOTS) return;\n    for'),
    'gate-own-only': ('    g_eax=guest_call(original,1,ait_play_id((uint32_t)t));', '    (void)t;'),
    'gate-lost-eax': ('    original();\n    r=g_eax;\n    if(!r ||', '    original();\n    r=1u;\n    if(!r ||'),
    # whole-group killed / spotted (all members) and boss-class AIs
    'kill-any-member': ('    if(ait_run.fired[t] || !ait_all((uint32_t)t,1,0)) return;', '    if(ait_run.fired[t]) return;'),
    'kill-leader-own': ('    else MAPMAKER_GUEST_RET();                   /* the leader', '    else original();                   /* the leader'),
    'kill-twice': ('    ait_run.fired[t]=1;\n    WORD_AT(record+8u)', '    WORD_AT(record+8u)'),
    'event-no-killed': ('    ait_run.killed[i]=1;\n', ''),
    'removed-no-event': ('            guest_call(sub_0033E560,1,ent);\n            for', '            for'),
    'removed-any-class': ('ait_play_index(WORD_AT(rec+8u))<AIT_LEVEL_SLOTS && ait_boss_record(rec)) {', 'ait_play_index(WORD_AT(rec+8u))<AIT_LEVEL_SLOTS) {'),
    'removed-full-health': ('        else if(ait_getf(ent+AIT_ENT_HEALTH)<ait_getf(ent+AIT_ENT_MAX)) {', '        else {'),
    'removed-no-dedupe': ('        if(k<AIT_SEEN) ait_run.seen[k]=0;             /* its event came already */\n        else if', '        if(0) (void)0;\n        else if'),
    'removed-no-original': ('    }\n    original();\n}\n\n/* sub_00163780', '    }\n}\n\n/* sub_00163780'),
    'removed-any-scene': ('    if(ait_play() && ait_actor(ent,&a,&rec) && WORD_AT(a)==ent &&', '    if(ait_actor(ent,&a,&rec) && WORD_AT(a)==ent &&'),
    'placed-ignored': ('    return !ait_run.any_placed || ait_run.placed[k];', '    return 1;'),
    'spot-any-member': ('        if(sight) { if(ait_run.spotted[k]) spotted++; else if(!ait_run.killed[k]) return 0; }',
                        '        if(sight) { if(ait_run.spotted[k]) return 1; }'),
    'spot-killed-blocks': ('spotted++; else if(!ait_run.killed[k]) return 0; }', 'spotted++; else return 0; }'),
    'spot-none-needed': ('    return members && (!sight || spotted);', '    return members!=0;'),
    'spot-boss-never': ('    if(!ait_boss_record(rec)) return 0;\n    hp=', '    return 0;\n    hp='),
    'spot-no-hurt': ('    if(hp<max) return 1;', '    (void)hp;(void)max;'),
    'spot-no-range': ('    return ait_player_near(ent);\n}', '    return 0&&ait_player_near(ent);\n}'),
    'spot-range-wide': ('#define AIT_SPOT_RANGE 30.0f', '#define AIT_SPOT_RANGE 3000.0f'),
    'spot-mode-ignored': ('(WORD_AT(AIT_MODE)&0xFFu)==10u?WORD_AT(a+0x10u):WORD_AT(a+0x18u)', 'WORD_AT(a+0x18u)'),
    'spot-no-latch': ('        if(!ait_run.spotted[k] && ait_spots(a,ent,rec)) {', '        ait_run.spotted[k]=0;\n        if(ait_spots(a,ent,rec)) {'),
    'alert-boss-stock': ('    if(!group && !boss) { original();return; }', '    if(!group) { original();return; }'),
    'alert-group-stock': ('    if(!group && !boss) { original();return; }', '    if(!boss) { original();return; }'),
    'alert-no-delay': ('    if(state!=7u) { WORD_AT(trigger+0x18u)=7u;WORD_AT(trigger+0x1Cu)=now+WORD_AT(AIT_DELAY);return; }\n', ''),
    'alert-no-wait': ('    if(state==7u && (int32_t)WORD_AT(trigger+0x1Cu)>=(int32_t)now) return;\n', ''),
    'alert-no-ret': ('    MAPMAKER_GUEST_RET();\n    g_eax=0;\n    state=', '    g_eax=0;\n    state='),
    'id-off-by-one': ('static uint32_t ait_play_id(uint32_t index) { return WORD_AT(AIT_SAI_BASE)+index+1u; }',
                      'static uint32_t ait_play_id(uint32_t index) { return WORD_AT(AIT_SAI_BASE)+index; }'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('aitemplates.exe' if os.name == 'nt' else 'aitemplates')
    source = ROOT / 'tests/mapmaker/mapmaker_aitemplates.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_aitemplates(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker aitemplates ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_new_guest_code_table():
    """The feature changes no guest code: the save format reuses bytes the
    stock writer leaves zero, so no config/guest-code-patches entry exists."""
    assert not (ROOT / 'config/guest-code-patches/aitemplates.json').exists()
