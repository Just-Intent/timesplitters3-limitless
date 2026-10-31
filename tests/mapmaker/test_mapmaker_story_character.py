"""MapMaker Story character choice: gate byte hooks and the guest-code table.

Compiles src/interface/pc_mapmaker_story_character.inc against fake guest memory (with
mutations that must be caught), checks config/guest-code-patches/story-character.json
against the C constants, and, when the private XBE is available, that every
`cmp byte [0x0078ADAE],2` in game code is either patched to the gate or
reviewed as unrelated to the Story character select.
"""
import json
import os
import re
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded, patched, reviewed, unclassified

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_story_character.inc'
TABLE = ROOT / 'config/guest-code-patches/story-character.json'
SUBMODE = 0x0078ADAE
GATE = 0x00903500
# patch id -> (containing function, site, wrapper hook)
SITES = {
    'story-char-roster': (0x002C6C80, 0x002C6D1F, 'pc_mapmaker_story_char_roster'),
    'story-char-preview': (0x002C6680, 0x002C672E, 'pc_mapmaker_story_char_preview'),
    'story-char-confirm': (0x002D4D00, 0x002D5414, 'pc_mapmaker_story_char_confirm'),
    'story-char-config': (0x002D2C70, 0x002D359D, 'pc_mapmaker_story_char_config'),
}

MUTATIONS = {
    'always-on': ('mapmaker_limits.limits?0u:', '1?0u:'),
    'always-stock': ('mapmaker_limits.limits?0u:', '0?0u:'),
    'inverted': ('mapmaker_limits.limits?0u:MAPMAKER_STORY_B(MAPMAKER_STORY_SUBMODE)',
                 '!mapmaker_limits.limits?0u:MAPMAKER_STORY_B(MAPMAKER_STORY_SUBMODE)'),
    'on-forces-story': ('mapmaker_limits.limits?0u:', 'mapmaker_limits.limits?2u:'),
    'wrong-gate': ('#define MAPMAKER_STORY_GATE 0x00903500u', '#define MAPMAKER_STORY_GATE 0x00903501u'),
    'wrong-submode': ('#define MAPMAKER_STORY_SUBMODE 0x0078ADAEu', '#define MAPMAKER_STORY_SUBMODE 0x0078ADADu'),
    'writes-submode': ('MAPMAKER_STORY_B(MAPMAKER_STORY_GATE)=mapmaker_limits',
                       'MAPMAKER_STORY_B(MAPMAKER_STORY_SUBMODE)=MAPMAKER_STORY_B(MAPMAKER_STORY_GATE)=mapmaker_limits'),
    'gate-after-original': ('pc_mapmaker_story_char_config(void (*original)(void))  { mapmaker_story_gate();original(); }',
                            'pc_mapmaker_story_char_config(void (*original)(void))  { original();mapmaker_story_gate(); }'),
    'roster-no-gate': ('pc_mapmaker_story_char_roster(void (*original)(void))  { mapmaker_story_gate();original(); }',
                       'pc_mapmaker_story_char_roster(void (*original)(void))  { original(); }'),
    'preview-no-gate': ('pc_mapmaker_story_char_preview(void (*original)(void)) { mapmaker_story_gate();original(); }',
                        'pc_mapmaker_story_char_preview(void (*original)(void)) { original(); }'),
    'confirm-twice': ('pc_mapmaker_story_char_confirm(void (*original)(void)) { mapmaker_story_gate();original(); }',
                      'pc_mapmaker_story_char_confirm(void (*original)(void)) { mapmaker_story_gate();original();original(); }'),
    'replaces-original': ('pc_mapmaker_story_char_config(void (*original)(void))  { mapmaker_story_gate();original(); }',
                          'pc_mapmaker_story_char_config(void (*original)(void))  { (void)original;mapmaker_story_gate();g_esp+=4u; }'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('story.exe' if os.name == 'nt' else 'story')
    source = ROOT / 'tests/mapmaker/mapmaker_story_character.c'
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
def test_mapmaker_story_character(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker story character ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


# ---- the guest-code table --------------------------------------------------

STOCK = bytes([0x80, 0x3D]) + SUBMODE.to_bytes(4, 'little') + bytes([2])


def validate(table):
    entries = table['patches']
    assert {e['id'] for e in entries} == set(SITES)
    for e in entries:
        function, site, hook = SITES[e['id']]
        assert e['kind'] == 'code'
        assert int(e['function'], 16) == function and int(e['address'], 16) == site, e['id']
        assert bytes.fromhex(e['original_hex']) == STOCK, e['id']
        assert bytes.fromhex(e['new_hex']) == bytes([0x80, 0x3D]) + GATE.to_bytes(4, 'little') + bytes([2]), e['id']
        assert hook in e['reason'], e['id']
    # outside the actors' extension data (0x00900000..0x009034FF), inside the block
    assert 0x00903500 <= GATE < 0x00920000


def test_table():
    validate(json.loads(TABLE.read_text(encoding='utf-8')))


@pytest.mark.parametrize('mutation', ['wrong-address', 'wrong-gate', 'wrong-imm', 'missing', 'extra-site'])
def test_table_mutations(mutation):
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    e = table['patches'][0]
    if mutation == 'wrong-address':
        e['address'] = '0x002C6D20'
    elif mutation == 'wrong-gate':
        e['new_hex'] = '803d0135900002'
    elif mutation == 'wrong-imm':
        e['new_hex'] = '803d0035900000'
    elif mutation == 'missing':
        table['patches'].pop()
    else:
        table['patches'].append(dict(e, id='story-char-extra', address='0x002D31D8'))
    with pytest.raises(AssertionError):
        validate(table)


def test_c_matches_table():
    text = INC.read_text(encoding='utf-8')
    assert int(re.search(r'#define MAPMAKER_STORY_GATE (0x[0-9A-Fa-f]+)u', text).group(1), 16) == GATE
    assert int(re.search(r'#define MAPMAKER_STORY_SUBMODE (0x[0-9A-Fa-f]+)u', text).group(1), 16) == SUBMODE
    for _, _, hook in SITES.values():
        assert re.search(rf'void {hook}\(void \(\*original\)\(void\)\)\s*\{{ mapmaker_story_gate\(\);original\(\); \}}', text), hook
    # the actors' extension data ends below the gate
    actors = (ROOT / 'src/interface/pc_mapmaker_actors.inc').read_text(encoding='utf-8')
    effects = int(re.search(r'#define MAPMAKER_ACTORS_EFFECTS (0x[0-9A-Fa-f]+)u', actors).group(1), 16)
    assert effects + 64 * 4 <= GATE


# ---- every Story sub-mode compare in game code -----------------------------

REVIEWED = reviewed("""
002D31D8 sub_002D2C70: any-mode Story test after the campaign/arcade sub-mode copy (skips the 0x18000 flag set and handicap/profile flags), not the character choice
002D326C sub_002D2C70: same any-mode Story test for the profile flag read (sub_00069640)
002D64E3 sub_002D6490: Back from the character select returns to the game-type menu 0x523890 in Story (else 0x523800); navigation only
""")


def test_every_story_submode_compare_classified():
    insns = decoded()
    found = {}
    for i in insns:
        if i.mnemonic != 'cmp':
            continue
        ops = i.ops
        if (len(ops) == 2 and ops[0][0] == 'mem' and not ops[0][1] and not ops[0][2]
                and ops[0][4] == SUBMODE and ops[1] == ('imm', 2)):
            found[i.address] = f'{i.mnemonic} {i.text}'
    sites = patched('story-character')
    assert sites == {s for _, s, _ in SITES.values()}
    assert sites <= set(found)
    assert unclassified(found, sites, REVIEWED) == {}
    assert set(REVIEWED) <= set(found)
    # each patched site is in its wrapped function, right after a mode-9 test
    by_address = {i.address: i for i in insns}
    for function, site, _ in SITES.values():
        assert by_address[site].function == function
