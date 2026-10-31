"""Compile the MapMaker Story AI templates policy (src/pc_mapmaker_aitemplates.inc)
against fake guest memory, with mutation cases: duplicate / linked copy rows and
actions, propagation of shared settings, delete, the saved link (chunk +0x32)
and "any linked copy" logic in play."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'src/pc_mapmaker_aitemplates.inc'

MUTATIONS = {
    'always-on': ('static int ait_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }',
                  'static int ait_on(void) { mapmaker_limits_read(); return 1; }'),
    'rows-any-caller': ('if(r!=AIT_ROWS_AT || cb!=AIT_CLOSE ||', 'if(cb!=AIT_CLOSE ||'),
    'rows-map-path': ('if(r!=AIT_ROWS_AT || cb!=AIT_CLOSE ||', 'if(r!=AIT_ROWS_AT ||'),
    'rows-cb-offset': ('cb=WORD_AT(g_esp+0x24u); }', 'cb=WORD_AT(g_esp+0x20u); }'),
    'rows-any-scene': ("WORD_AT(AIT_SCENE)!=102u || !(e=ait_editor())) return;", "!(e=ait_editor())) return;"),
    'rows-type5': ('    if(s>=count || ait_type(e,s)==5u) return;\n    if(ait_link', '    if(s>=count) return;\n    if(ait_link'),
    'close-type5': ("s<ait_count(e) && ait_type(e,s)!=5u && !ait_action", "s<ait_count(e) && !ait_action"),
    'refused-closes': ('        MAPMAKER_GUEST_RET();                     /* sub_0031CAC0: ret */\n        return;',
                       '        original();\n        return;'),
    'refused-no-ret': ('        MAPMAKER_GUEST_RET();                     /* sub_0031CAC0: ret */\n', ''),
    'limit-off-by-one': ('if(count+need>AIT_CAP) {', 'if(count+need>AIT_CAP+1u) {'),
    'first-link-one-slot': ('need=row==AIT_ROW_LINKED && root<0?2u:1u;', 'need=1u;'),
    'no-room-check': ('    if(!guest_call(sub_00300670,1,AIT_CHUNK_BYTES*need)) {', '    if(0) {'),
    'duplicate-no-points': ('    for(o=AIT_POINT0;o<AIT_STRIDE;o+=4u) WORD_AT(b+o)=WORD_AT(a+o);', ''),
    'duplicate-copies-link': ('for(o=0x28u;o<0x60u;o+=4u) if(o!=AIT_LINK)', 'for(o=0x28u;o<0x60u;o+=4u)'),
    'template-keeps-points': ('        WORD_AT(ait_entry(e,t)+AIT_POINTS)=0;\n', ''),
    'template-no-name': ('        ait_template_name(e,s,(uint32_t)t);\n', ''),
    'source-not-linked': ('        WORD_AT(ait_entry(e,s)+AIT_LINK)=(uint32_t)t+1u;\n', ''),
    'copy-link-off': ('    WORD_AT(ait_entry(e,c)+AIT_LINK)=(uint32_t)root+1u;', '    WORD_AT(ait_entry(e,c)+AIT_LINK)=(uint32_t)root;'),
    'template-copy-no-points': ('if(ait_link(e,k,count)==root && WORD_AT(ait_entry(e,k)+AIT_POINTS)) { from=k;break; }', '(void)k;'),
    'chained-links': ('    if(WORD_AT(ait_entry(e,l-1u)+AIT_LINK) || ait_type(e,l-1u)==5u) return -1;\n', ''),
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
    'delete-no-renumber': ('        else if(l>i+1u) WORD_AT(ait_entry(e,k)+AIT_LINK)=l-1u;', ''),
    'delete-keeps-orphans': ('        if(l==i+1u) WORD_AT(ait_entry(e,k)+AIT_LINK)=0;\n        else if',
                             '        if(0) (void)0;\n        else if'),
    'save-no-link': ('    ait_put16(data+AIT_CHUNK_LINK,t>=0?(uint32_t)t+1u:0u);', '    (void)t;(void)data;'),
    'save-offset': ('#define AIT_CHUNK_LINK 0x32u', '#define AIT_CHUNK_LINK 0x30u'),
    'load-keeps-stale': ('        WORD_AT(entry+AIT_LINK)=value;', '        if(value) WORD_AT(entry+AIT_LINK)=value;'),
    'load-no-base': ('value=base+l;', 'value=l;'),
    'load-chained': ('l-1u!=k && !x.link[l-1u] &&', 'l-1u!=k &&'),
    'load-no-index-check': (' && WORD_AT(entry+0x24u)==x.index[k]) value=', ') value='),
    'level-chained': ('|| !present[l-1u] || link_of[l-1u]) {', '|| !present[l-1u]) {'),
    'play-any-scene': ("static int ait_play(void) { return ait_level.active && WORD_AT(AIT_SCENE)==100u && mapmaker_limits.limits; }",
                       "static int ait_play(void) { return ait_level.active && mapmaker_limits.limits; }"),
    'event-no-restore': ('    guest_call(original,1,actor);\n    WORD_AT(record+8u)=id;', '    guest_call(original,1,actor);'),
    'event-no-redeliver': ('    guest_call(original,1,actor);\n    WORD_AT(record+8u)=id;', '    WORD_AT(record+8u)=id;'),
    'remove-no-copies': ('        if(ait_level.template_of[k]==(int)t) guest_call(original,1,ait_play_id(k));', '        (void)k;'),
    'gate-own-only': ('    g_eax=guest_call(original,1,ait_play_id((uint32_t)ait_level.template_of[i]));', '    (void)i;'),
    'gate-lost-eax': ('    original();\n    r=g_eax;\n    if(!r ||', '    original();\n    r=1u;\n    if(!r ||'),
    'alert-no-stop': ('!r && !(state!=7u && WORD_AT(trigger+0x18u)==7u);k++)', '!r;k++)'),
    'alert-no-restore': ('        r=guest_call(original,1,trigger);\n        WORD_AT(trigger+0x14u)=id;', '        r=guest_call(original,1,trigger);'),
    'alert-lost-result': ('    }\n    g_eax=r;\n}', '    }\n}'),
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
    source = ROOT / 'tests/mapmaker_aitemplates.c'
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
