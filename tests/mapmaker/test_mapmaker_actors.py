"""Compile the MapMaker actors area (64 characters in a local MapMaker story map) against fake guest memory."""
import json
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
ACTORS = ROOT / 'src/interface/pc_mapmaker_actors.inc'
TABLE = ROOT / 'config/guest-code-patches/actors.json'
MOVES = ROOT / 'config/guest-code-patches/actors-list.json'
OBJECTS = ROOT / 'config/guest-code-patches/objects.json'

MUTATIONS = {
    # spawner gate
    'gate-any-scene': ("return WORD_AT(MAPMAKER_ACTORS_SCENE)==100u && !WORD_AT(0x0076B108u);",
                       "return !WORD_AT(0x0076B108u);"),
    'gate-network-raised': ("return WORD_AT(MAPMAKER_ACTORS_SCENE)==100u && !WORD_AT(0x0076B108u);",
                            "return WORD_AT(MAPMAKER_ACTORS_SCENE)==100u;"),
    'gate-sign': ('-mapmaker_actors_delta());', '+mapmaker_actors_delta());'),
    'gate-no-original': ('    original();                                /* edx = record',
                         '    g_esp+=4u;                                 /* edx = record'),
    'gate-wide-in-arcade': ("    if(!mapmaker_actors_story() && limit>MAPMAKER_ACTORS_STOCK) limit=MAPMAKER_ACTORS_STOCK;\n", "\n"),
    'gate-unclamped': ('v>=1u && v<=MAPMAKER_ACTORS_MAX', 'v>=1u'),
    'gate-zero-allowed': ('v>=1u && v<=MAPMAKER_ACTORS_MAX', 'v<=MAPMAKER_ACTORS_MAX'),
    'gate-loose-parse': ('if(!*end && v>=1u', 'if(v>=1u'),
    'gate-default-32': ('mapmaker_actors.alive=MAPMAKER_ACTORS_MAX;', 'mapmaker_actors.alive=MAPMAKER_ACTORS_STOCK;'),
    # pool
    'pool-stock-size': ('bytes=mapmaker_actors_pool_bytes(MAPMAKER_ACTORS_MAX);', 'bytes=mapmaker_actors_pool_bytes(MAPMAKER_ACTORS_STOCK);'),
    'pool-not-cleared': ('        if(p && MAPMAKER_PTR_OK(p,bytes)) mapmaker_actors_zero(p,bytes);\n', '\n'),
    'pool-no-ret': ('        mapmaker_actors.pool_slots=p?MAPMAKER_ACTORS_MAX:0u;\n    }\n    MAPMAKER_GUEST_RET();',
                    '        mapmaker_actors.pool_slots=p?MAPMAKER_ACTORS_MAX:0u;\n    }'),
    'pool-any-scene': ('    if(scene!=100u) {\n        original();', '    if(scene==1234u) {\n        original();'),
    'pool-flag-ignored': ('WORD_AT(0x007356D0u)=(f&0x1000u)?1u:0u;', 'WORD_AT(0x007356D0u)=1u;'),
    'pool-early-alloc': ('    if(WORD_AT(0x007DE304u)) {                 /* the original', '    if(1) {                 /* the original'),
    'pool-not-recorded': ('        mapmaker_actors.pool_ptr=p;\n', '\n'),
    'pool-stock-101': ('mapmaker_actors.pool_slots=scene==101u?MAPMAKER_ACTORS_POOL_101:MAPMAKER_ACTORS_STOCK;',
                       'mapmaker_actors.pool_slots=MAPMAKER_ACTORS_STOCK;'),
    'pool-clear-stock': ('       mapmaker_actors.pool_slots<=MAPMAKER_ACTORS_STOCK) { original(); return; }',
                         '       1) { original(); return; }'),
    # slot guard
    'slot-wide-everywhere': ('ai_cap=wide?MAPMAKER_ACTORS_AI_CAP:MAPMAKER_ACTORS_STOCK_AI;', 'ai_cap=MAPMAKER_ACTORS_AI_CAP;'),
    'slot-stock-cap-wide': ('ai_cap=wide?MAPMAKER_ACTORS_AI_CAP:MAPMAKER_ACTORS_STOCK_AI;', 'ai_cap=MAPMAKER_ACTORS_STOCK_AI;'),
    'slot-pool-ignored': ('    cap=mapmaker_actors_pool_slots(pool);\n', '    cap=MAPMAKER_ACTORS_MAX;\n'),
    'slot-network-wide': ('    wide=story && mapmaker_actors_local100();', '    wide=story && WORD_AT(MAPMAKER_ACTORS_SCENE)==100u;'),
    'slot-pairs-ignored': ('    if(pairs<cap) cap=pairs;\n', '\n'),
    'slot-offsets-swapped': ('(story?0x10u:0x18u)', '(story?0x18u:0x10u)'),
    'slot-one-word': ('if(!(WORD_AT(a)|WORD_AT(a+4)))', 'if(!WORD_AT(a+4))'),
    'slot-used-cap': ('return used<ai_cap?', 'return used<=ai_cap?'),
    'slot-no-reuse': ('        if(!(WORD_AT(a)|WORD_AT(a+4))) return (int32_t)i;\n', '\n'),
    'slot-no-ret': ("        g_eax=0xFFFFFFFFu;                     /* the original's own refusal */\n        MAPMAKER_GUEST_RET();",
                    "        g_eax=0xFFFFFFFFu;                     /* the original's own refusal */"),
    'slot-zero-result': ("g_eax=0xFFFFFFFFu;                     /* the original's own refusal */",
                         "g_eax=0;                               /* the original's own refusal */"),
    'slot-off-by-one': ('(uint32_t)slot>=cap ||', '(uint32_t)slot>cap ||'),
    'slot-not-counted': ('if(mapmaker_actors.drops[kind]++<3u) {', 'if(mapmaker_actors.drops[kind]<3u) {'),
    'slot-refusal-logged': ('        if(slot>=0) mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_SLOT', '        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_SLOT'),
    # level-start AI
    'placed-unclamped': ('        WORD_AT(level+0x30u)=cap;', '        (void)cap;'),
    'placed-ignores-humans': ('cap=slots>humans?slots-humans:0u;', 'cap=slots;'),
    # engine object headroom (m45c crash: 106 Story AI ran the object pool past 1023)
    'objects-headroom-stock': ('#define MAPMAKER_ACTORS_OBJECT_HEADROOM 72u', '#define MAPMAKER_ACTORS_OBJECT_HEADROOM 5u'),
    'objects-headroom-off-by-one': ('free_objects<(int32_t)MAPMAKER_ACTORS_OBJECT_HEADROOM) {',
                                    'free_objects<=(int32_t)MAPMAKER_ACTORS_OBJECT_HEADROOM) {'),
    'objects-freed-ignored': ('live=(int32_t)WORD_AT(MAPMAKER_ACTORS_OBJ_COUNT)-(int32_t)WORD_AT(MAPMAKER_ACTORS_OBJ_FREE);',
                              'live=(int32_t)WORD_AT(MAPMAKER_ACTORS_OBJ_COUNT);'),
    'objects-past-pool-wraps': ('return live>=(int32_t)cap?0:(int32_t)cap-live;', 'return (int32_t)cap-live;'),
    'objects-network': ('    if(!mapmaker_actors_local100() || !cap) return -1;', '    if(!cap) return -1;'),
    'objects-no-pool-refuses': ('    if(!mapmaker_actors_local100() || !cap) return -1;', '    if(!mapmaker_actors_local100()) return -1;'),
    'objects-slot-unchecked': ('    if(free_objects>=0 && free_objects<(int32_t)MAPMAKER_ACTORS_OBJECT_HEADROOM) {',
                               '    if(0) {'),
    'objects-slot-not-counted': ('        mapmaker_actors_drop(MAPMAKER_ACTORS_DROP_OBJECTS,', '        (void)(MAPMAKER_ACTORS_DROP_OBJECTS,'),
    'objects-placed-unclamped': ('        if(room<cap) cap=room;', '        (void)room;'),
    'objects-placed-no-headroom': ('(uint32_t)free_objects-MAPMAKER_ACTORS_OBJECT_HEADROOM:0u;', '(uint32_t)free_objects:0u;'),
    # list
    'list-63': ('if(count<MAPMAKER_ACTORS_MAX) return 0;', 'if(count<MAPMAKER_ACTORS_MAX-1u) return 0;'),
    'list-unbounded': ('if(count<MAPMAKER_ACTORS_MAX) return 0;', 'if(1) return 0;'),
    # pick
    'pick-no-self-skip': ('|| WORD_AT(e+8u)==id) continue;', ') continue;'),
    'pick-humans-filtered': ('} else if(!(humans_too && i<humans)) {', '} else if(1) {'),
    'pick-field-swapped': ('MAPMAKER_ACTORS_PICK_GROUP_AI)?8u:0x28u))!=t)', 'MAPMAKER_ACTORS_PICK_GROUP_AI)?0x28u:8u))!=t)'),
    'pick-start-zero': ('MAPMAKER_ACTORS_PICK_TEAM?0u:humans;i<count;i++)', 'MAPMAKER_ACTORS_PICK_TEAM?0u:0u;i<count;i++)'),
    'pick-default-swapped': ('MAPMAKER_ACTORS_PICK_GROUP_AI)?0u:', 'MAPMAKER_ACTORS_PICK_GROUP_AI)?1u:'),
    'pick-esp-kept-low': ('    g_esp=buf+0x100u;\n', '\n'),
    'pick-always': ('if(count<=MAPMAKER_ACTORS_ARRAY || !MAPMAKER_STACK_OK(0x200)', 'if(!MAPMAKER_STACK_OK(0x200)'),
    'pick-type-unsigned': ('if((int32_t)MAPMAKER_ACTORS_S16(e+0x48u)!=(int32_t)t) continue;', 'if((uint32_t)(uint16_t)MAPMAKER_ACTORS_S16(e+0x48u)!=t-1u) continue;'),
    # enemies
    'enemies-33': ('if(n==MAPMAKER_ACTORS_ARRAY) break;', 'if(n==MAPMAKER_ACTORS_ARRAY+1u) break;'),
    'enemies-one-pass': ('    g_esp-=4u;WORD_AT(g_esp)=ret;                 /* same arguments, same return address */\n    original();\n',
                         '    g_eax=winner;(void)ret;\n'),
    'enemies-winner-dropped': ('{ WORD_AT(MAPMAKER_ACTORS_LIST)=saved[i];break; }', '{ break; }'),
    'enemies-not-restored': ('    for(i=0;i<count;i++) WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=saved[i];\n}',
                             '\n}'),
    'enemies-pass-b-overlap': ('WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=i<k?self:saved[i];', 'WORD_AT(MAPMAKER_ACTORS_LIST+i*4u)=saved[i];'),
    'enemies-early-ignored': ("    if((arg2 && WORD_AT(0x0074C388u)!=WORD_AT(self+8u)) || (MAPMAKER_ACTORS_B(0x007DE458u)&0x40u)) { original(); return; }\n", "\n"),
    # kill words
    'kill-not-undone': ('        MAPMAKER_ACTORS_S16(a)=(int16_t)(uint16_t)((uint16_t)MAPMAKER_ACTORS_S16(a)-(uint16_t)points);\n', '\n'),
    'kill-side-not-added': ('            MAPMAKER_ACTORS_S16(a)=(int16_t)(uint16_t)((uint16_t)MAPMAKER_ACTORS_S16(a)+(uint16_t)points);\n', '\n'),
    'kill-any-state': ('added=state==6u || state==7u;', 'added=1;'),
    'kill-low-ids': ('victim<MAPMAKER_ACTORS_SCORE_WORDS ||', 'victim<1u ||'),
    'kill-submode-run': ('    if(sub==0x12u || sub==0x13u) {', '    if(0) {'),
    'create-object-ignored': (' && WORD_AT(a)==g_eax)', ')'),
    'create-no-clear': ('        mapmaker_actors_zero(mapmaker_actors_side(slot,MAPMAKER_ACTORS_SCORE_WORDS),MAPMAKER_ACTORS_SCORE_WORDS*2u);', '        ;'),
    'side-not-cleared-pool': ('    mapmaker_actors_zero(MAPMAKER_ACTORS_SIDE,MAPMAKER_ACTORS_SIDE_BYTES);\n    if(scene!=100u) {', '    if(scene!=100u) {'),
    'side-not-cleared-clear': ('    mapmaker_actors_zero(MAPMAKER_ACTORS_SIDE,MAPMAKER_ACTORS_SIDE_BYTES);   /* as the pool', '    (void)0;   /* as the pool'),
    'score-side-ignored': ('    if(k<MAPMAKER_ACTORS_MAX && j<MAPMAKER_ACTORS_MAX) return MAPMAKER_ACTORS_S16(mapmaker_actors_side(k,j));',
                           '    if(0) return 0;'),
    'score-same-team': ('!=team) sum+=', '==team) sum+='),
    'score-no-network': ('    if(guest_call(sub_003533F0,0)) sum+=', '    if(0) sum+='),
    'score-own-skipped': ('if(flags&0x20u) sum-=mapmaker_actors_kills(e,k,k);', 'if(flags&0x20u) sum-=0;'),
    'deaths-original': ('    if(id<MAPMAKER_ACTORS_SCORE_WORDS || id>=MAPMAKER_ACTORS_MAX) { original(); return; }\n    n=',
                        '    if(1) { original(); return; }\n    n='),
    'deaths-all-slots': ('    if((int32_t)n>(int32_t)MAPMAKER_ACTORS_MAX) n=MAPMAKER_ACTORS_MAX;\n    for(j=0;(int32_t)j<(int32_t)n;j++) sum+=',
                         '    n=MAPMAKER_ACTORS_MAX;\n    for(j=0;(int32_t)j<(int32_t)n;j++) sum+='),
    # radar, patrol
    'blip-off-by-one': ('if(count>=MAPMAKER_ACTORS_BLIP_CAP) {', 'if(count>MAPMAKER_ACTORS_BLIP_CAP) {'),
    'blip-unbounded': ('if(count>=MAPMAKER_ACTORS_BLIP_CAP) {', 'if(0) {'),
    'patrol-not-filled': ('           0xFF,(MAPMAKER_ACTORS_MAX-MAPMAKER_ACTORS_STOCK+1u)*MAPMAKER_ACTORS_PATROL_ROW);',
                          '           0x00,(MAPMAKER_ACTORS_MAX-MAPMAKER_ACTORS_STOCK+1u)*MAPMAKER_ACTORS_PATROL_ROW);'),
    'patrol-overrun': ('(MAPMAKER_ACTORS_MAX-MAPMAKER_ACTORS_STOCK+1u)*', '(MAPMAKER_ACTORS_MAX-MAPMAKER_ACTORS_STOCK+2u)*'),
    'patrol-short-copy': ('           MAPMAKER_ACTORS_STOCK*MAPMAKER_ACTORS_PATROL_ROW);', '           (MAPMAKER_ACTORS_STOCK-1u)*MAPMAKER_ACTORS_PATROL_ROW);'),
    'effects-always-stock': ('    if(!mapmaker_actors_wide() && WORD_AT(0x0075F6E8u)', '    if(WORD_AT(0x0075F6E8u)'),
    'effects-never-stock': ('    if(!mapmaker_actors_wide() && WORD_AT(0x0075F6E8u)', '    if(0 && WORD_AT(0x0075F6E8u)'),
    'effects-off-by-one': ('WORD_AT(0x0075F6E8u)>=MAPMAKER_ACTORS_EFFECTS_STOCK', 'WORD_AT(0x0075F6E8u)>MAPMAKER_ACTORS_EFFECTS_STOCK'),
    'effects-wrong-result': ('        g_eax=WORD_AT(g_esp+4);MAPMAKER_GUEST_RET();return;\n    }\n    original();\n}',
                             '        g_eax=0;MAPMAKER_GUEST_RET();return;\n    }\n    original();\n}'),
    'patrol-not-refreshed':('    mapmaker_actors_patrol_init();             /* every level load', '    (void)0;             /* every level load'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ACTORS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / ACTORS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('actors.exe' if os.name == 'nt' else 'actors')
    source = ROOT / 'tests/mapmaker/mapmaker_actors.c'
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
def test_mapmaker_actors(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k != 'TSFP_MAPMAKER_ALIVE'}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker actors ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def _define(name):
    match = re.search(rf'#define {name} (0x[0-9A-Fa-f]+|\d+)u', ACTORS.read_text(encoding='utf-8'))
    assert match, name
    return int(match.group(1), 0)


def _le(value):
    return value.to_bytes(4, 'little').hex()


def test_table_matches_policy():
    """The byte rewrites, relocations and moved references agree with the C constants."""
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    entries = {e['id']: e for e in table['patches']}
    lo, hi = 0x00900000, 0x00920000                 # this area's sub-range
    gate = _define('MAPMAKER_ACTORS_GATE')
    assert lo <= gate and gate + 4 <= hi
    stock = _define('MAPMAKER_ACTORS_STOCK_ALIVE')
    for eid, address in (('actor-spawn-gate-flagged', '0x001640EC'), ('actor-spawn-gate-scene100', '0x001641A2')):
        e = entries[eid]
        assert e['kind'] == 'code' and e['function'] == '0x00164010' and e['address'] == address
        assert e['original_hex'] == '833d' + _le(0x0074C37C) + '%02x' % stock
        assert e['new_hex'] == '833d' + _le(gate) + '%02x' % stock
    n = _define('MAPMAKER_ACTORS_MAX')
    assert n == 64 and _define('MAPMAKER_ACTORS_STOCK') == 32
    cap = entries['actor-ai-slot-cap']
    assert cap['kind'] == 'operand' and int(cap['value'], 0) == _define('MAPMAKER_ACTORS_AI_CAP') == n - 1
    for eid in ('actor-pairs-alloc', 'actor-pairs-clear', 'actor-list-clear', 'actor-list-clear-level'):
        assert entries[eid]['original_hex'] == 'b9' + _le(32) and entries[eid]['new_hex'] == 'b9' + _le(n)
    assert entries['actor-pairs-column']['new_hex'] == 'ba' + _le(n)
    attach = entries['actor-attach-relocate']
    base = _define('MAPMAKER_ACTORS_ATTACH')
    assert int(attach['new'], 0) == base and int(attach['new_size'], 0) == n * 3 * 4
    assert int(attach['size'], 0) == 32 * 3 * 4 and base + n * 12 <= hi
    assert entries['actor-attach-clear']['new_hex'] == 'b9' + _le(n * 3)
    # Radar: the character loop runs to n slots of the 0x124-byte network table; blips move.
    radar = entries['actor-radar-bound']
    assert radar['original_hex'] == '81fb' + _le(0x0077AD94) and radar['operand'] == 1
    assert int(radar['value'], 0) == _define('MAPMAKER_ACTORS_RADAR_END') == 0x00777244 + n * 0x124
    blips = _define('MAPMAKER_ACTORS_BLIPS')
    for eid, raw in (('actor-blips-write', '05'), ('actor-blips-read', 'bf')):
        assert entries[eid]['original_hex'] == raw + _le(0x007A5DC0) and int(entries[eid]['value'], 0) == blips
    assert blips + 16 * _define('MAPMAKER_ACTORS_BLIP_CAP') <= hi
    # Patrol routes: every reference of 0x00520C38 reads the 65-row copy.
    patrol = _define('MAPMAKER_ACTORS_PATROL')
    sites = [e for e in table['patches'] if e['id'].startswith('actor-patrol-')]
    assert len(sites) == 5 and all(int(e['value'], 0) == patrol and e['function'] == '0x002B2750'
                                   and _le(0x00520C38) in e['original_hex'] for e in sites)
    assert _define('MAPMAKER_ACTORS_PATROL_STOCK') == 0x00520C38 and _define('MAPMAKER_ACTORS_PATROL_ROW') == 40
    side = _define('MAPMAKER_ACTORS_SIDE')
    assert base + n * 12 <= side and side + n * 32 * 2 <= patrol
    assert patrol + (n + 1) * 40 <= blips
    # Per-death effect lists: 64 entries each, bound 64, stock 30 kept by the hook.
    pads, objs = _define('MAPMAKER_ACTORS_EFFECT_PADS'), _define('MAPMAKER_ACTORS_EFFECTS')
    assert blips + 16 * _define('MAPMAKER_ACTORS_BLIP_CAP') <= pads - 4 and pads + n * 4 <= objs - 4
    assert objs + n * 4 <= hi
    reloc = entries['actor-effects-objects-relocate']
    assert int(reloc['old'], 0) == 0x0075F670 and int(reloc['size'], 0) == 30 * 4
    assert int(reloc['new'], 0) == objs and int(reloc['new_size'], 0) == n * 4
    for e in table['patches']:
        if e['id'].startswith('actor-effects-pads'):
            assert int(e['value'], 0) in (pads, pads - 4)
        if e['id'].startswith('actor-effects-objects-last'):
            assert int(e['value'], 0) == objs - 4
    assert int(entries['actor-effects-bound']['value'], 0) == n
    assert entries['actor-effects-bound']['original_hex'].endswith('%02x' % _define('MAPMAKER_ACTORS_EFFECTS_STOCK'))
    assert entries['actor-effects-clear']['new_hex'] == 'b9' + _le(n)
    # Perception: characters 0..n-1 then objects; objects.json sizes the slice.
    assert entries['actor-perception-objects']['new_hex'] == 'bf' + _le(n * 16)
    assert entries['actor-perception-objects2']['new_hex'] == 'c7442410' + _le(n * 16)
    assert entries['actor-perception-objects3']['new_hex'] == '8d75%02x' % n
    objects = {e['id']: e for e in json.loads(OBJECTS.read_text(encoding='utf-8'))['patches']}
    obj_cap = int(re.search(r'#define MAPMAKER_OBJECTS_LIST9_CAP (\d+)u',
                            (ROOT / 'src/interface/pc_mapmaker_objects.inc').read_text(encoding='utf-8')).group(1))
    per = n * 16 + 16 * obj_cap
    for pid in ('obj-perception-clear', 'obj-perception-stride', 'obj-perception-reset',
                'obj-perception-alloc', 'obj-perception-reset2'):
        assert objects[pid]['new_hex'].endswith(_le(per)), pid
    for e in table['patches']:
        if e['kind'] == 'code':
            assert len(e['original_hex']) == len(e['new_hex']) and e['original_hex'] != e['new_hex']


def test_moved_references():
    """Every moved list/AI-table reference keeps its offset in the new, larger table."""
    moves = json.loads(MOVES.read_text(encoding='utf-8'))['patches']
    lst, ai = _define('MAPMAKER_ACTORS_LIST'), _define('MAPMAKER_ACTORS_AITAB')
    n = _define('MAPMAKER_ACTORS_MAX')
    seen = set()
    counts = {'actor-list': 0, 'actor-aitab': 0}
    for e in moves:
        assert e['kind'] == 'operand' and e['address'] not in seen
        seen.add(e['address'])
        value = int(e['value'], 0)
        raw = bytes.fromhex(e['original_hex'])
        if e['id'].startswith('actor-list-'):
            old, new, size = 0x007A3580, lst, n * 4
            counts['actor-list'] += 1
        else:
            assert e['id'].startswith('actor-aitab-')
            old, new, size = 0x007A2D00, ai, n * 4
            counts['actor-aitab'] += 1
        # the old address is encoded in the instruction (disp32/imm32); find it and map it
        found = [int.from_bytes(raw[i:i + 4], 'little') for i in range(len(raw) - 3)]
        olds = [v for v in found if old - 4 <= v < old + 0x80]
        assert len(olds) == 1, e['id']
        assert value == new + (olds[0] - old) and new - 4 <= value < new + size, e['id']
    # guest-refs.py 0x007A357C 0x84: 181 distinct instructions (0x007A3600 is another variable);
    # guest-refs.py 0x007A2D00 0x78: 8 (0x007A2D24 belongs to the group table 0x007A2D80).
    assert counts == {'actor-list': 181, 'actor-aitab': 8}
    for a in (lst, ai, _define('MAPMAKER_ACTORS_ATTACH')):
        assert 0x00900004 <= a - 4 and a + n * 12 <= 0x00920000
    assert lst + n * 4 <= ai and ai + n * 4 <= _define('MAPMAKER_ACTORS_ATTACH')
