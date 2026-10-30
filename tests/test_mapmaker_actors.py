"""Compile the MapMaker actors area (spawner alive gate, AI slot guard) against fake guest memory."""
import json
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
ACTORS = ROOT / 'src/pc_mapmaker_actors.inc'
TABLE = ROOT / 'config/guest-code-patches/actors.json'

MUTATIONS = {
    'gate-any-scene': ("if(WORD_AT(0x0079094Cu)!=100u || WORD_AT(0x0076B108u)) return 0;",
                       "if(WORD_AT(0x0076B108u)) return 0;"),
    'gate-network-raised': ("if(WORD_AT(0x0079094Cu)!=100u || WORD_AT(0x0076B108u)) return 0;",
                            "if(WORD_AT(0x0079094Cu)!=100u) return 0;"),
    'gate-sign': ('-mapmaker_actors_delta());', '+mapmaker_actors_delta());'),
    'gate-no-original': ('    original();                                /* edx = record',
                         '    g_esp+=4u;                                 /* edx = record'),
    'gate-stock-limit': ('#define MAPMAKER_ACTORS_ALIVE 32u', '#define MAPMAKER_ACTORS_ALIVE 20u'),
    'gate-unclamped': ('v>=1u && v<=MAPMAKER_ACTORS_ALIVE', 'v>=1u'),
    'gate-zero-allowed': ('v>=1u && v<=MAPMAKER_ACTORS_ALIVE', 'v<=MAPMAKER_ACTORS_ALIVE'),
    'gate-loose-parse': ('if(!*end && v>=1u', 'if(v>=1u'),
    'slot-any-scene': ("    if(WORD_AT(0x0079094Cu)!=100u) { original(); return; }\n    pool=",
                       "    pool="),
    'slot-pool-31': ('#define MAPMAKER_ACTORS_POOL 32u', '#define MAPMAKER_ACTORS_POOL 31u'),
    'slot-pairs-ignored': ('    if(pairs<cap) cap=pairs;\n', '\n'),
    'slot-pairs-story-everywhere': ('pairs=story?MAPMAKER_ACTORS_PAIRS_STORY:humans+MAPMAKER_ACTORS_PAIRS_EXTRA;',
                                    'pairs=MAPMAKER_ACTORS_PAIRS_STORY;'),
    'slot-offsets-swapped': ('(story?0x10u:0x18u)', '(story?0x18u:0x10u)'),
    'slot-one-word': ('if(!(WORD_AT(a)|WORD_AT(a+4)))', 'if(!WORD_AT(a+4))'),
    'slot-used-cap': ('return used<MAPMAKER_ACTORS_AI_SLOTS?', 'return used<=MAPMAKER_ACTORS_AI_SLOTS?'),
    'slot-no-reuse': ('        if(!(WORD_AT(a)|WORD_AT(a+4))) return (int32_t)i;\n', '\n'),
    'slot-no-ret': ('        g_eax=0xFFFFFFFFu;                     /* the original\'s own refusal */\n        MAPMAKER_GUEST_RET();',
                    '        g_eax=0xFFFFFFFFu;                     /* the original\'s own refusal */'),
    'slot-zero-result': ("g_eax=0xFFFFFFFFu;                     /* the original's own refusal */",
                         "g_eax=0;                               /* the original's own refusal */"),
    'slot-off-by-one': ('(uint32_t)slot>=cap ||', '(uint32_t)slot>cap ||'),
    'slot-not-counted': ('if(mapmaker_actors.drops++<3u) {', 'if(mapmaker_actors.drops<3u) {'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = ACTORS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / ACTORS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('actors.exe' if os.name == 'nt' else 'actors')
    source = ROOT / 'tests/mapmaker_actors.c'
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


def test_table_matches_policy():
    """Both spawner comparisons read the gate the hook writes, inside this area's sub-range."""
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    entries = {e['id']: e for e in table['patches']}
    gate = _define('MAPMAKER_ACTORS_GATE')
    assert 0x00900000 <= gate and gate + 4 <= 0x00920000
    stock = _define('MAPMAKER_ACTORS_STOCK_ALIVE')
    for eid, address in (('actor-spawn-gate-flagged', '0x001640EC'), ('actor-spawn-gate-scene100', '0x001641A2')):
        e = entries[eid]
        assert e['kind'] == 'code' and e['function'] == '0x00164010' and e['address'] == address
        # cmp dword ptr [disp32], imm8: only the displacement changes; the imm8 stays the stock 20.
        assert e['original_hex'] == '833d' + (0x0074C37C).to_bytes(4, 'little').hex() + '%02x' % stock
        assert e['new_hex'] == '833d' + gate.to_bytes(4, 'little').hex() + '%02x' % stock
    assert len(table['patches']) == 2
    # The raised limit never exceeds the actor pool, and the AI slots fit in it with two humans.
    assert _define('MAPMAKER_ACTORS_ALIVE') <= _define('MAPMAKER_ACTORS_POOL')
    assert 2 + _define('MAPMAKER_ACTORS_AI_SLOTS') <= _define('MAPMAKER_ACTORS_POOL')
