"""Compile the MapMaker objects area (turrets, object lists, pools, teleporters, tags) against fake guest memory."""
import json
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
OBJECTS = ROOT / 'src/pc_mapmaker_objects.inc'
TABLE = ROOT / 'config/guest-code-patches/objects.json'

MUTATIONS = {
    'create-any-caller': ("WORD_AT(g_esp)==0x0006E818u && ", ''),
    'create-gun-only': ('return !WORD_AT(MAPMAKER_OBJECTS_GUN_LIST+4) || !WORD_AT(MAPMAKER_OBJECTS_OWNER_LIST+4);',
                        'return !WORD_AT(MAPMAKER_OBJECTS_GUN_LIST+4);'),
    'create-not-zero': ('g_eax=0;      /* sub_0006D7A0', '(void)0;      /* sub_0006D7A0'),
    'spare-not-cleared': ('memset((void *)((uintptr_t)node+g_xbox_mem_offset),0,size);', '(void)0;'),
    'spare-not-rotated': ('mapmaker_objects_spares_used++;\n', '\n'),
    'spare-no-count': ('    WORD_AT(list+8)++;\n', '\n'),
    'list9-off-by-one': ('(int32_t)WORD_AT(0x007B625Cu)>=(int32_t)MAPMAKER_OBJECTS_LIST9_CAP', '(int32_t)WORD_AT(0x007B625Cu)>(int32_t)MAPMAKER_OBJECTS_LIST9_CAP'),
    'register-no-fields': ('        WORD_AT(obj+0x90)=WORD_AT(0x007DE318u);\n', '\n'),
    'register-slot0-stock': ('cap=MAPMAKER_OBJECTS_LIST0_CAP;', 'cap=112u;'),
    'register-removal-guarded': ('if(flag && (int32_t)WORD_AT(count)', 'if((int32_t)WORD_AT(count)'),
    'register-listed-ignored': (' &&\n       !mapmaker_objects_listed(base,cap,obj)', ''),
    'pool-stock': ('#define MAPMAKER_OBJECTS_POOL 1000u', '#define MAPMAKER_OBJECTS_POOL 550u'),
    'pool-story': ('return mapmaker_limits.pools && (mapmaker_objects_scene(100u) || mapmaker_objects_scene(102u));',
                   'return mapmaker_limits.pools;'),
    'pool-play-only': ('(mapmaker_objects_scene(100u) || mapmaker_objects_scene(102u))', 'mapmaker_objects_scene(100u)'),
    'pool-any-callback': ('if(callback && callback!=0x00242BA0u) { original(); return; }', ''),
    'pool-no-callback-call': ('    if(callback) guest_call(sub_00242BA0,1,0u);\n', '\n'),
    'pool-bucket-stock': ('MAPMAKER_OBJECTS_BUCKET_BYTES,1u);', '0x5340u,1u);'),
    'pool-no-tail': ('    guest_call(sub_000A5AF0,0);', '   '),
    'pool-tables-kept': ('    mapmaker_objects_clear(0x007B5840u,0xD6u*4u);\n', '\n'),
    'twin-not-cleared': ('    mapmaker_objects_clear(p,bytes);\n', '\n'),
    'twin-flag-inverted': ('WORD_AT(0x004D9700u)=WORD_AT(0x006F73B0u)==0;', 'WORD_AT(0x004D9700u)=WORD_AT(0x006F73B0u)!=0;'),
    'twin-story': ('    if(!mapmaker_objects_pool_on()) { original(); return; }\n    save_registers(&r);\n    WORD_AT(0x0070AAD8u)',
                   '    save_registers(&r);\n    WORD_AT(0x0070AAD8u)'),
    'tele-stock-arrays': ('#define MAPMAKER_OBJECTS_TELEPORTER_CAP 1200u', '#define MAPMAKER_OBJECTS_TELEPORTER_CAP 10u'),
    'tele-all-pairs': ('            break;\n        }\n    }\n    restore_registers(&r);', '        }\n    }\n    restore_registers(&r);'),
    'tele-no-link-guard': (' && WORD_AT(0x007B6660u)<links', ''),
    'tele-cover-raw': ('if((int32_t)pad>0x1F40) {', 'if(0) {'),
    'tele-cover-inclusive': ('if((int32_t)pad>0x1F40) {', 'if((int32_t)pad>=0x1F40) {'),
    'tele-wrong-range': ('id>=0x0A && id<=0x0E', 'id>=0x0A && id<=0x11'),
    'tele-direction': ('g_ebx=a;g_edi=b;', 'g_ebx=b;g_edi=a;'),
    'tele-registers-leak': ('    restore_registers(&r);\n    MAPMAKER_GUEST_RET();\n}\n\n/* ---- logic tags',
                            '    MAPMAKER_GUEST_RET();\n}\n\n/* ---- logic tags'),
    'tele-any-scene': ('    if(!mapmaker_objects_scene(100u)) { original(); return; }\n    save_registers(&r);\n    mapmaker_limits_read();',
                       '    save_registers(&r);\n    mapmaker_limits_read();'),
    'tele-stock-pool-assumed': ('links=mapmaker_limits.pools?MAPMAKER_LIMIT_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;',
                                'links=MAPMAKER_LIMIT_LINKS;'),
    'tag-not-guarded': ('mapmaker_objects_tags_full()) {', '0) {'),
    'tag-stock-cap': ('#define MAPMAKER_OBJECTS_TAGS_CAP 1000u', '#define MAPMAKER_OBJECTS_TAGS_CAP 250u'),
    'tag-no-serial': ('        WORD_AT(0x00519750u)++;\n', '\n'),
    'tag-sentinel': (' && tag!=0x3F3F3F3Fu', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = OBJECTS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / OBJECTS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('objects.exe' if os.name == 'nt' else 'objects')
    source = ROOT / 'tests/mapmaker_objects.c'
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
def test_mapmaker_objects(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MEMORY_SCALE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker objects ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def _define(name):
    match = re.search(rf'#define {name} (0x[0-9A-Fa-f]+|\d+)u', OBJECTS.read_text(encoding='utf-8'))
    assert match, name
    return int(match.group(1), 0)


def test_table_matches_policy():
    """The C guards use the capacities and addresses the patch table gives the code."""
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    entries = {e['id']: e for e in table['patches']}
    extension = (0x008A0000, 0x008C0000)            # this area's sub-range
    relocated = []
    for e in table['patches']:
        if e['kind'] == 'relocate':
            new, size = int(e['new'], 16), int(e['new_size'], 16)
            assert extension[0] <= new and new + size <= extension[1], e['id']
            relocated.append((new, new + size))
    relocated.sort()
    for (a, b), (c, d) in zip(relocated, relocated[1:]):
        assert b <= c, 'relocated tables overlap'
    for name, rid, entry in (('LIST9', 'obj-list9-relocate', 4), ('LIST8', 'obj-list8-relocate', 4),
                             ('LIST0', 'obj-list0-relocate', 4), ('TAGS', 'obj-tag-entries-relocate', 16)):
        e = entries[rid]
        assert int(e['new'], 16) == _define(f'MAPMAKER_OBJECTS_{name}')
        assert int(e['new_size'], 16) == entry * _define(f'MAPMAKER_OBJECTS_{name}_CAP')
    spares = (_define('MAPMAKER_OBJECTS_SPARE_GUNS'), _define('MAPMAKER_OBJECTS_SPARE_OWNERS') + 8 * 0x130)
    assert relocated[-1][1] <= spares[0] and spares[1] <= extension[1]
    assert _define('MAPMAKER_OBJECTS_SPARE_GUNS') + 8 * 0x1E0 <= _define('MAPMAKER_OBJECTS_SPARE_OWNERS')
    n = _define('MAPMAKER_OBJECTS_TURRETS')
    assert entries['obj-turret-gun-nodes']['new_hex'] == '6a%02x' % n
    assert entries['obj-turret-sync-slots']['new_hex'] == '83fb%02x' % n
    assert 9 + 3 * n <= 0x104 and n % 4 == 0 and n < 128
    assert entries['obj-turret-gun-bytes']['new_hex'] == '68' + (n * 0x1DC).to_bytes(4, 'little').hex()
    assert entries['obj-bucket-bytes']['new_hex'] == '68' + _define('MAPMAKER_OBJECTS_BUCKET_BYTES').to_bytes(4, 'little').hex()
    assert entries['obj-bucket-bound']['new_hex'] == '3d' + (_define('MAPMAKER_OBJECTS_BUCKET_BYTES') // 4 - 3).to_bytes(4, 'little').hex()
    assert entries['obj-tag-entries-cap']['new_hex'] == '81fa' + _define('MAPMAKER_OBJECTS_TAGS_CAP').to_bytes(4, 'little').hex()
    assert entries['obj-list0-clear']['new_hex'] == 'b9' + _define('MAPMAKER_OBJECTS_LIST0_CAP').to_bytes(4, 'little').hex()
    assert _define('MAPMAKER_OBJECTS_POOL') < 1024        # slot = handle >> 22
    # The perception array holds 32 characters then one entry per slot-9 list entry.
    per = 0x200 + 0x10 * _define('MAPMAKER_OBJECTS_LIST9_CAP')
    for pid in ('obj-perception-clear', 'obj-perception-stride', 'obj-perception-alloc'):
        assert entries[pid]['new_hex'].endswith(per.to_bytes(4, 'little').hex())
    for e in table['patches']:
        if e['kind'] == 'code':
            assert len(e['original_hex']) == len(e['new_hex']) and e['original_hex'] != e['new_hex']
