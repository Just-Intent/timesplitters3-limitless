"""Compile the MapMaker objects area (turrets, object lists, pools, teleporters, tags) against fake guest memory."""
import json
import os
from pathlib import Path
import re
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
OBJECTS = ROOT / 'src/interface/pc_mapmaker_objects.inc'
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
    'pool-stock': ('#define MAPMAKER_OBJECTS_POOL 1023u', '#define MAPMAKER_OBJECTS_POOL 550u'),
    'pool-round3': ('#define MAPMAKER_OBJECTS_POOL 1023u', '#define MAPMAKER_OBJECTS_POOL 1000u'),
    'pool-story': ('return mapmaker_limits.pools && (mapmaker_objects_scene(100u) || mapmaker_objects_scene(102u));',
                   'return mapmaker_limits.pools;'),
    'pool-play-only': ('(mapmaker_objects_scene(100u) || mapmaker_objects_scene(102u))', 'mapmaker_objects_scene(100u)'),
    'pool-any-callback': ('if(callback && callback!=0x00242BA0u) { original(); return; }', ''),
    'pool-no-callback-call': ('    if(callback) guest_call(sub_00242BA0,1,0u);\n', '\n'),
    'pool-bucket-stock': ('MAPMAKER_OBJECTS_BUCKET_BYTES,1u);', '0x5340u,1u);'),
    'pool-no-tail': ('    guest_call(sub_000A5AF0,0);', '   '),
    'pool-tables-kept': ('    mapmaker_objects_clear(0x008CE080u,0x40Du*4u);\n', '\n'),
    'pool-tables-stale': ('    mapmaker_objects_clear(0x008CC000u,0x40Du*4u);\n', '    mapmaker_objects_clear(0x007B54E0u,0xD6u*4u);\n'),
    'pool-tables-short': ('    mapmaker_objects_clear(0x008D0100u,0x40Du*4u);\n', '    mapmaker_objects_clear(0x008D0100u,0x20Du*4u);\n'),
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
    'tele-self-link': ('            if(a==b) {', '            if(0) {'),
    'tele-self-ends-chain': ('            if(a==b) {', '            if(a==b) { n=0;'),
    'tele-registers-leak': ('    restore_registers(&r);\n    MAPMAKER_GUEST_RET();\n}\n\n/* ---- logic tags',
                            '    MAPMAKER_GUEST_RET();\n}\n\n/* ---- logic tags'),
    'tele-any-scene': ('    if(!mapmaker_objects_scene(100u)) { original(); return; }\n    save_registers(&r);\n    mapmaker_limits_read();',
                       '    save_registers(&r);\n    mapmaker_limits_read();'),
    'tele-stock-pool-assumed': ('links=mapmaker_limits.pools?MAPMAKER_POOL_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;',
                                'links=MAPMAKER_POOL_LINKS;'),
    # The editor limit is not the pool: past it the teleporters still get links.
    'tele-editor-limit-as-pool': ('links=mapmaker_limits.pools?MAPMAKER_POOL_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;',
                                  'links=mapmaker_limits.pools?MAPMAKER_LIMIT_LINKS:MAPMAKER_OBJECTS_STOCK_LINKS;'),
    'tag-not-guarded': ('mapmaker_objects_tags_full()) {', '0) {'),
    'tag-stock-cap': ('#define MAPMAKER_OBJECTS_TAGS_CAP 1000u', '#define MAPMAKER_OBJECTS_TAGS_CAP 250u'),
    'tag-no-serial': ('        WORD_AT(0x00519750u)++;\n', '\n'),
    'tag-sentinel': (' && tag!=0x3F3F3F3Fu', ''),
    # The placed-pickup table: the `cmp ah,n` test needs a multiple of 256, above the 500 items.
    'pickups-stock': ('#define MAPMAKER_OBJECTS_PICKUPS 768u', '#define MAPMAKER_OBJECTS_PICKUPS 120u'),
    'pickups-below-items': ('#define MAPMAKER_OBJECTS_PICKUPS 768u', '#define MAPMAKER_OBJECTS_PICKUPS 256u'),
    # Prop blocks: two per pool slot, within the 8 KB table.
    'propblocks-stock': ('#define MAPMAKER_OBJECTS_PROP_BLOCKS 2048u', '#define MAPMAKER_OBJECTS_PROP_BLOCKS 60u'),
    'propblocks-pool-only': ('#define MAPMAKER_OBJECTS_PROP_BLOCKS 2048u', '#define MAPMAKER_OBJECTS_PROP_BLOCKS 1024u'),
    'propblocks-past-table': ('#define MAPMAKER_OBJECTS_PROP_BLOCKS 2048u', '#define MAPMAKER_OBJECTS_PROP_BLOCKS 4096u'),
    # Physics bodies (sub_0016C430): one kind-1 body per pool slot in MapMaker play.
    'physics-stock': ('#define MAPMAKER_OBJECTS_PHYSICS 1024u', '#define MAPMAKER_OBJECTS_PHYSICS 100u'),
    'physics-below-pool': ('#define MAPMAKER_OBJECTS_PHYSICS 1024u', '#define MAPMAKER_OBJECTS_PHYSICS 1000u'),
    'physics-any-scene': ('if(!mapmaker_limits.pools || !mapmaker_objects_scene(100u)) { original(); return; }',
                          'if(!mapmaker_limits.pools) { original(); return; }'),
    'physics-pools-off-ignored': ('if(!mapmaker_limits.pools || !mapmaker_objects_scene(100u))', 'if(!mapmaker_objects_scene(100u))'),
    'physics-count-not-stored': ('    MAPMAKER_OBJECTS_HALF_AT(0x0074C49Cu)=(uint16_t)n;\n', '\n'),
    'physics-count-dword': ('    MAPMAKER_OBJECTS_HALF_AT(0x0074C49Cu)=(uint16_t)n;', '    WORD_AT(0x0074C49Cu)=n;'),
    'physics-bodies-short': ('(n+0x60u)<<9,1u);', 'n<<9,1u);'),
    'physics-handles-short': ('WORD_AT(0x0074C480u)=guest_call(sub_0003E0B0,3,0x40u,(n+0x60u)*4u,1u);',
                              'WORD_AT(0x0074C480u)=guest_call(sub_0003E0B0,3,0x40u,n*4u,1u);'),
    'physics-free-list-short': ('(n+0x60u)*2u,1u);', 'n*2u,1u);'),
    'physics-records-short': ('(n+0x60u)<<5,1u);', 'n<<5,1u);'),
    'physics-wrong-heap': ('WORD_AT(0x0074C494u)=guest_call(sub_0003E0B0,3,0x40u,(n+0x60u)*4u,1u);',
                           'WORD_AT(0x0074C494u)=guest_call(sub_0003E0B0,3,0x40u,(n+0x60u)*4u,0u);'),
    'physics-no-reset': ('    guest_call(sub_00165760,0);', '   '),
    'physics-floats-short': ('a<0x0074C450u;a+=4u', 'a<0x0074C44Cu;a+=4u'),
    'physics-floats-low': ('for(a=0x0074C414u;', 'for(a=0x0074C410u;'),
    'physics-lists-kept': ('    WORD_AT(0x007A20CCu)=0;WORD_AT(0x007A20C8u)=0;\n', '\n'),
    # The prop break log: kind 1 full is the explosion refusal; 5 free slots for debris.
    'break-log-kind1-off-by-one': ('int explode=used<bodies,', 'int explode=used<=bodies,'),
    'break-log-debris-margin': ('debris=live+1+4<slots;', 'debris=live+1<slots;'),
    'break-log-any-scene': ('    if(mapmaker_objects_scene(100u) && MAPMAKER_STACK_OK(8)) {\n        int32_t used',
                            '    if(MAPMAKER_STACK_OK(8)) {\n        int32_t used'),
    'break-no-original': ('    original();\n}\n\n/* ---- teleporters', '}\n\n/* ---- teleporters'),
    # Object create (m45c): never append past the pool; reuse a freed slot; MapMaker scenes only.
    'full-off-by-one': ('    if(count<cap) return 1;', '    if(count<=cap) return 1;'),
    'full-trusts-free-count': ("    if(!WORD_AT(MAPMAKER_OBJECTS_FREED) || !base) return 0;\n    for(i=0;i<cap;i++)",
                               "    if(!WORD_AT(MAPMAKER_OBJECTS_FREED) || !base) return 0;\n    return 1;\n    for(i=0;i<cap;i++)"),
    'full-no-reuse': ('&0x80u) return 1;\n    return 0;', '&0x80u) return 0;\n    return 0;'),
    'full-not-zero': ('        g_eax=0;\n        MAPMAKER_GUEST_RET();\n        return;\n    }\n    original();\n}\n\n/* ---- physics',
                      '        MAPMAKER_GUEST_RET();\n        return;\n    }\n    original();\n}\n\n/* ---- physics'),
    'full-runs-original': ('        g_eax=0;\n        MAPMAKER_GUEST_RET();\n        return;\n    }\n    original();\n}\n\n/* ---- physics',
                           '        g_eax=0;\n    }\n    original();\n}\n\n/* ---- physics'),
    'full-any-scene': ('    if(mapmaker_objects_pool_on() && !mapmaker_objects_slot_free()) {',
                       '    if(!mapmaker_objects_slot_free()) {'),
    'full-not-counted': ('if(mapmaker_objects_full++<8u ||', 'if(mapmaker_objects_full<8u ||'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = OBJECTS.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / OBJECTS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('objects.exe' if os.name == 'nt' else 'objects')
    source = ROOT / 'tests/mapmaker/mapmaker_objects.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    if mutation != 'fixed' and result.returncode != 0:
        return None                      # the pool/editor-limit size check refused it at compile time
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_objects(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    if exe is None:
        return
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
    assert spares[1] <= extension[1]
    for a, b in relocated:                     # spares and relocated tables never overlap
        assert b <= spares[0] or spares[1] <= a, (hex(a), hex(b))
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
    # The perception array holds 64 characters (actors area) then one entry per slot-9 list entry.
    per = 0x400 + 0x10 * _define('MAPMAKER_OBJECTS_LIST9_CAP')
    for pid in ('obj-perception-clear', 'obj-perception-stride', 'obj-perception-alloc'):
        assert entries[pid]['new_hex'].endswith(per.to_bytes(4, 'little').hex())
    # Placed pickups: table bytes, clear dwords and the `cmp ah,n` full test agree (768 = 3 << 8).
    pickups, record = _define('MAPMAKER_OBJECTS_PICKUPS'), _define('MAPMAKER_OBJECTS_PICKUP_BYTES')
    assert pickups % 256 == 0 and pickups < 0x8000 and record == 0xD8
    assert entries['obj-pickups-alloc']['original_hex'] == '68' + (120 * record).to_bytes(4, 'little').hex()
    assert entries['obj-pickups-alloc']['new_hex'] == '68' + (pickups * record).to_bytes(4, 'little').hex()
    assert entries['obj-pickups-clear']['new_hex'] == 'b9' + (pickups * record // 4).to_bytes(4, 'little').hex()
    assert entries['obj-pickups-cap']['original_hex'] == '83f878'
    assert entries['obj-pickups-cap']['new_hex'] == '80fc%02x' % (pickups >> 8)
    # Prop blocks: the pointer table and its clear hold the same count, at least
    # two per pool slot (placed props plus creations during play), stock 60.
    blocks = _define('MAPMAKER_OBJECTS_PROP_BLOCKS')
    assert blocks >= 2 * _define('MAPMAKER_OBJECTS_POOL')
    assert entries['obj-propblocks-alloc']['original_hex'] == '68' + (60 * 4).to_bytes(4, 'little').hex()
    assert entries['obj-propblocks-alloc']['new_hex'] == '68' + (blocks * 4).to_bytes(4, 'little').hex()
    assert entries['obj-propblocks-clear']['original_hex'] == 'b9' + (60).to_bytes(4, 'little').hex()
    assert entries['obj-propblocks-clear']['new_hex'] == 'b9' + blocks.to_bytes(4, 'little').hex()
    for e in table['patches']:
        if e['kind'] == 'code':
            assert len(e['original_hex']) == len(e['new_hex']) and e['original_hex'] != e['new_hex']
