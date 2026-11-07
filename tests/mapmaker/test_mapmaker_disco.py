"""MapMaker Disco tileset variant (src/interface/pc_mapmaker_disco.inc, the marker in
src/interface/pc_mapmaker_sky.inc, the defaults in src/interface/pc_mapmaker_skymusic.inc):
fixture with mutations, the decoder/XBE proof that info flag bit 31 is free and the
call sites the hooks rely on, and a pak audit of the local game data (skipped without
the private XBE or game paks; names, CRCs and byte comparisons only)."""
import json
import os
import struct
import subprocess
import zlib
from pathlib import Path

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
DISCO = ROOT / 'src/interface/pc_mapmaker_disco.inc'
SMX = ROOT / 'src/interface/pc_mapmaker_skymusic.inc'
SKY = ROOT / 'src/interface/pc_mapmaker_sky.inc'
DOC = ROOT / 'docs/mapmaker/mapmaker-disco-variant.md'

MUTATIONS = {
    # marker
    'marker-other-bit': ('#define MAPMAKER_VARIANT_FLAG 0x80000000u', '#define MAPMAKER_VARIANT_FLAG 0x40000000u'),
    'marker-any-base': ('(WORD_AT(info+0x08u)>>16)==MAPMAKER_VARIANT_BASE;', '1;'),
    'compile-any-base': (' && (WORD_AT(info+0x08u)>>16)==MAPMAKER_VARIANT_BASE)\n            flags|=MAPMAKER_VARIANT_FLAG;', ')\n            flags|=MAPMAKER_VARIANT_FLAG;'),
    'compile-stale-bit-kept': ('&~(SMX_MASK|MAPMAKER_VARIANT_FLAG);', '&~SMX_MASK;'),
    'compile-without-switch': ('    if(smx_on()) {\n        flags|=sky', '    {\n        flags|=sky'),
    'load-not-restored': ('    if(mapmaker_variant_disco(info)) WORD_AT(ed+MAPMAKER_VARIANT_ED_TILESET)=MAPMAKER_VARIANT_ROW;   /* the Disco row */\n', ''),
    # play
    'play-no-mount': ('    if(info) dsc_level_start(info);', '    (void)0;'),
    'play-mount-whole': ('#define DSC_PAK "pak/arcademem/l_56_arm.pak"', '#define DSC_PAK "pak/arcademem/l_56_ar.pak"'),
    'play-every-time': ('    if(dsc.level_tried || smx_scene()', '    if(smx_scene()'),
    'play-any-map': (' || !mapmaker_variant_disco(info)) return;\n    dsc.level_tried=1;', ') return;\n    dsc.level_tried=1;'),
    'play-ready-without-ids': ('    dsc.play_ready=n!=0;', '    dsc.play_ready=1;'),
    'level-not-reset': ('    dsc.level_tried=0;dsc.play_ready=0;dsc.frame_logged=0;', '    dsc.frame_logged=0;'),
    'ids-not-checked': ('        if(dsc_slot_has((uint32_t)slot,dsc_alias[k].to)) {', '        if(1) {'),
    'crc-wrong': ('c=c&1u?(c>>1)^0xEDB88320u:c>>1;', 'c=c&1u?(c>>1)^0x04C11DB7u:c>>1;'),
    'index-wrong-field': ('#define DSC_SLOT_INDEX 0x10u', '#define DSC_SLOT_INDEX 0x0Cu'),
    'alias-wrong-pair': ('{0x294554u,0x7B0662u,', '{0x294554u,0x20E656u,'),
    'alias-keeps-mask': ('{0xE871CCu,0x7DDE70u,', '{0x5A89BCu,0x7DDE70u,'),
    'rewrite-one-word': ('WORD_AT(e)=WORD_AT(e+4u)=dsc.live[j];', 'WORD_AT(e)=dsc.live[j];'),
    'rewrite-registered': ('        if(WORD_AT(e+4u)!=id) continue;          /* registered (row in word 0) or embedded */\n', ''),
    'rewrite-past-end': ('        if((id=WORD_AT(e))==0xFFFFFFFFu) break;', '        if((id=WORD_AT(e))==0xFFFFFFFFu) continue;'),
    'rewrite-any-scene': ('    if(s==102u) return dsc.editor_ready && WORD_AT(DSC_CURRENT)==MAPMAKER_VARIANT_BASE;\n    return 0;',
                          '    return dsc.play_ready || dsc.editor_ready;'),
    'rewrite-any-tileset-editor': (' && WORD_AT(DSC_CURRENT)==MAPMAKER_VARIANT_BASE;', ';'),
    # defaults
    'sky-default-missing': ('        smx_set16(info+SMX_INFO_SKY,(uint32_t)MAPMAKER_VARIANT_SKY);\n', ''),
    'sky-default-kept': ('        smx_set16(info+SMX_INFO_SKY,(uint32_t)old);\n        smx_log("map start: Disco', '        smx_log("map start: Disco'),
    'sky-default-over-own': (' && (old=smx_get16(info+SMX_INFO_SKY))<0 &&', ' && ((old=smx_get16(info+SMX_INFO_SKY))<0 || 1) &&'),
    'clouds-sky-default': ('    if(mapmaker_variant_disco(info)) return MAPMAKER_VARIANT_SKY;\n', ''),
    'music-default-missing': ('        g_eax=MAPMAKER_VARIANT_MUSIC;          /* the Disco variant\'s default track */\n', ''),
    'music-default-over-own': ('SMX_RET_MUSIC_PLAY && g_eax==0xFFFFFFFFu &&\n              (info=smx_play_info())!=0 && mapmaker_variant_disco(info)) {',
                               'SMX_RET_MUSIC_PLAY &&\n              (info=smx_play_info())!=0 && mapmaker_variant_disco(info)) {'),
    'music-default-wrong': ('#define MAPMAKER_VARIANT_MUSIC 0xBAu', '#define MAPMAKER_VARIANT_MUSIC 0x1Au'),
    # editor
    'close-row-to-original': ('    if(disco) WORD_AT(ed+DSC_ED_TILESET)=MAPMAKER_VARIANT_BASE;\n    before', '    before'),
    'close-row-lost': ('    if(disco) WORD_AT(ed+DSC_ED_TILESET)=MAPMAKER_VARIANT_ROW;\n', ''),
    'close-no-reload': ('        WORD_AT(ed+DSC_ED_FLAGS)|=DSC_ED_RELOAD;\n', ''),
    'close-reload-always': (' && disco!=dsc.stream_variant) {', ') {'),
    'stream-no-unload': ('        r=guest_call(sub_00061D80,1,name);', '        r=0;'),
    'stream-busy-forced': ('        if(r==DSC_STREAM_BUSY) dsc_log', '        if(0) dsc_log'),
    'stream-not-marked': ('        else { WORD_AT(DSC_STREAM_NOW)=0xFFFFFFFFu;forced=1;current=0; }', '        else { forced=1;current=0; }'),
    'stream-prev-kept': ('    if(forced) WORD_AT(DSC_STREAM_PREV)=name;   /* as the stock switch leaves it */\n', ''),
    'stream-reload-always': (' && t==MAPMAKER_VARIANT_BASE && want!=dsc.stream_variant) {', ' && t==MAPMAKER_VARIANT_BASE) {'),
    'stream-any-scene': (' || smx_scene()!=102u || (t=WORD_AT(g_esp+4u))>=DSC_TILESETS) { original();return; }',
                         ' || (t=WORD_AT(g_esp+4u))>=DSC_TILESETS) { original();return; }'),
    'stream-variant-not-kept': ('    if(!current && g_eax) dsc.stream_variant=want;', '    if(0) dsc.stream_variant=want;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    old, new = MUTATIONS.get(mutation, ('', ''))
    hits = 0
    # All three are copied: skymusic includes disco from its own directory first.
    for src in (DISCO, SMX, SKY):
        text = src.read_text(encoding='utf-8')
        if old and old in text:
            hits += 1
            text = text.replace(old, new, 1)
        (tmp_path / src.name).write_text(text, encoding='utf-8')
    assert hits == (0 if mutation == 'fixed' else 1), mutation
    exe = tmp_path / ('disco.exe' if os.name == 'nt' else 'disco')
    source = ROOT / 'tests/mapmaker/mapmaker_disco.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_disco(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker disco ok' in result.stdout
        assert '[mapmaker] disco: variant active (Virtual + Disco): l_56_arm.pak mounted, 5 of 5 texture ids aliased' in result.stderr
        assert '[mapmaker] disco: 5 texture list rows rewritten in 1 model lists this level' in result.stderr
        assert 'textures/7b0662.xbt missing from l_56_arm.pak; Virtual 294554 kept' in result.stderr
        assert 'pak/arcademem/l_56_arm.pak not mounted; plain Virtual textures kept' in result.stderr
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_flag_bits_disjoint():
    """Bit 31 is outside the stock bits (0..17) and every fork field of +0x244."""
    sky, smx = SKY.read_text(encoding='utf-8'), SMX.read_text(encoding='utf-8')
    assert '#define MAPMAKER_VARIANT_FLAG 0x80000000u' in sky
    fork = 0x00040000 | 0x00F80000 | 0x1F000000 | 0x60000000     # clouds, sky, music, weather
    assert '0x00040000u' in sky and '0x00F80000u' in sky and '0x1F000000u' in smx and '0x60000000u' in smx
    stock = 0x3FFFF
    assert not 0x80000000 & (fork | stock) and (fork | stock | 0x80000000) == 0xFFFFFFFF
    # The validator uses the same bit and base.
    val = (ROOT / 'src/interface/pc_mapmaker_validate.inc').read_text(encoding='utf-8')
    assert '#define MAPMAKER_VALIDATE_VARIANT 0x80000000u' in val and '#define MAPMAKER_VALIDATE_VARIANT_BASE 5u' in val
    assert '#define MAPMAKER_VARIANT_BASE 5u' in sky and '#define MAPMAKER_VARIANT_ROW 6u' in sky
    assert '#define TS_DISCO_ROW 6u' in (ROOT / 'src/interface/pc_mapmaker_tilesets.inc').read_text(encoding='utf-8')


def test_variant_bit_free_decoded():
    """No stock code reads info flag bit 31: every access to the info chunk's +0x244
    (functions using tag 0xC77668, plus the loader's chunk switch) tests bits 0..17
    only, and the rebuild writes the dword from those bits (lifter's decoder)."""
    from mapmaker_decoded import decoded
    insns = decoded()
    nxt = {}
    for a, b in zip(insns, insns[1:]):
        nxt[a.address] = b
    info_fns = {i.function for i in insns if any(op[0] == 'imm' and op[1] == 0x00C77668 for op in i.ops)}
    flags = {i.address: i for i in insns if i.function in info_fns
             and any(op[0] == 'mem' and 0x244 <= op[4] <= 0x247 and op[1] != 'esp' for op in i.ops)}
    readers = {0x0023F42F, 0x002BE769} | {0x002FDA4A + 10 * k for k in range(7)} | {0x002FDA92 + 12 * k for k in range(7)}
    assert set(flags) == readers, sorted(map(hex, flags))
    assert flags[0x0023F42F].text.endswith('0x20000')                      # bit 17
    assert nxt[0x002BE769].text == 'esp, 8' and nxt[nxt[0x002BE769].address].text == 'ch, ch'   # bit 15
    masks = []
    for a in sorted(readers - {0x0023F42F, 0x002BE769}):
        op = nxt[a]
        assert op.mnemonic == 'and' and op.text.startswith('eax, '), hex(a)
        masks.append(int(op.text.split(', ')[1], 16))
    assert masks == [1 << b for b in range(1, 15)]
    # The editor loader's chunk switch (not in the tag list: 0xFF388998 = -0xC77668).
    load = {i.address: i for i in insns if i.function == 0x00301A90
            and any(op[0] == 'mem' and 0x244 <= op[4] <= 0x247 and op[1] != 'esp' for op in i.ops)}
    assert set(load) == {0x00301CCF, 0x00301CF6, 0x00301D06, 0x00301D1A, 0x00301D38, 0x00301D46}, sorted(map(hex, load))
    assert load[0x00301D06].text.endswith(', 1') and load[0x00301D38].text.endswith('+ 0x246]')
    assert nxt[0x00301D1A].text == 'ah, ah' and nxt[nxt[0x00301D46].address].text == 'eax, 0x11'
    # the loop 1..sub_00311910() tests 1 << cl; sub_00311910 returns 15
    at = {i.address: i for i in insns}
    assert at[0x00311910].text == 'eax, 0xf' and at[0x00301CF4].mnemonic == 'shl'
    # the nonzero test at 0x00301CCF only gates the editor fields set from those bits
    assert any(i.address == 0x00301CD5 and i.mnemonic == 'je' for i in insns)
    # Rebuild (ebx = info - 0x10): +0x254 starts at 0 and gets bits 0..17 only.
    rebuild = sorted((i.address, i.mnemonic, i.text) for i in insns if i.function == 0x003006C0
                     and any(op[0] == 'mem' and 0x254 <= op[4] <= 0x257 for op in i.ops))
    assert rebuild == [(0x003007B9, 'mov', 'dword ptr [ebx + 0x254], 0'),
                       (0x00300806, 'mov', 'edi, dword ptr [ebx + 0x254]'),
                       (0x00300815, 'mov', 'dword ptr [ebx + 0x254], edi'),
                       (0x00300830, 'or', 'dword ptr [ebx + 0x254], 1'),
                       (0x00300846, 'or', 'dword ptr [ebx + 0x254], 0x8000'),
                       (0x00300860, 'or', 'dword ptr [ebx + 0x254], 0x10000'),
                       (0x0030087A, 'or', 'dword ptr [ebx + 0x254], 0x20000')]
    assert at[0x0030073A].text == '0x311910' and at[0x00300768].text == 'ax, word ptr [0x784020]'


def test_decoded_sites():
    """The hooks' only callers and the editor +0x204 accesses (lifter's decoder)."""
    from mapmaker_decoded import decoded
    insns = decoded()

    def calls(target):
        return {i.address for i in insns if i.call and any(op[0] == 'imm' and op[1] == target for op in i.ops)}
    # every first pass over a model's texture list (0x00085C3F: non-MapMaker levels only)
    assert calls(0x0008E3E0) == {0x00085C3F, 0x00089E3F, 0x0009628F, 0x00096354, 0x00098005}
    assert calls(0x000637E0) == {0x00085C8A, 0x000962D9, 0x000963C9}
    assert 0x0009628F < 0x000962D9 and 0x00096354 < 0x000963C9          # rewrite before the queue
    assert calls(0x00096220) == {0x0006355D}
    assert calls(0x002FC0A0) == {0x00313311}
    assert calls(0x00338180) == {0x002FC265, 0x00311FD5}
    assert calls(0x00061D80) == {0x00061E77, 0x00061EAF}
    # editor +0x204 (integer, editor code): the editor's own accesses are the defaults,
    # the reset (write 3, then compare), the loader and the menu close; the rest are
    # other objects (menu items, text widgets) at the same offset.
    acc = {i.address for i in insns if 0x002F8000 <= i.address < 0x00340000
           and any(op[0] == 'mem' and op[4] == 0x204 and op[1] not in ('esp', 'ebp') for op in i.ops)}
    editor = {0x002FC0A5, 0x002FE32B, 0x002FEC5F, 0x002FEC6B, 0x00301CB8}
    others = {0x003122A9, 0x00313459, 0x0032B779, 0x0032B787, 0x0032BF4D, 0x0032BF93, 0x0032BFB7,
              0x0032C006, 0x00335FE5, 0x003360B3, 0x00337EFD}
    assert acc == editor | others, sorted(map(hex, acc - editor - others))
    at = {i.address: i for i in insns}
    for a in editor:
        prev = [i for i in insns if i.function == at[a].function and a - 0x20 <= i.address < a]
        assert any('0x7844a8' in i.text for i in prev), hex(a)            # based on [0x007844A8]
    # The Map Settings tileset list binds editor +0x204 (0x0031355E add eax, 0x204).
    assert at[0x0031355E].text == 'eax, 0x204' and at[0x00313564].call


def test_stock_code_bytes():
    """XBE bytes of the editor stream switch and the mount table the hooks use."""
    from test_mapmaker_sky import _xbe
    read = _xbe()
    # sub_002FC0A0: +0x204 vs [0x00784020]; copy and +0x238 |= 0x12
    assert read(0x002FC0A0, 0x21) == bytes.fromhex(
        'a1a8447800' '8b8804020000' '3b0d20407800' '740d' '890d20407800' '838838020000' '12' 'c3')
    # sub_00338180: stream set [0x0052EC88 + t*0x18] -> sub_00061E90
    assert read(0x00338180, 0x17) == bytes.fromhex('8b442404' '8d0c40' '8b14cd88ec5200' '89542404' 'e9f99cd2ff')
    # sub_00061E90: nothing when the set is current ([0x007DE314] via sub_00066560); unload with sub_00061D80
    assert read(0x00061E91, 5) == bytes.fromhex('e8ca460000') and read(0x00066560, 6) == bytes.fromhex('a114e37d00c3')
    assert read(0x00061EAF, 5) == bytes.fromhex('e8ccfeffff') and read(0x00061EB7, 3) == bytes.fromhex('83f8fb')
    # sub_00066580: previous = current, current = argument
    assert read(0x00066580, 0x15) == bytes.fromhex('a114e37d00' '8b4c2404' 'a310e37d00' '890d14e37d00' 'c3')
    # mount slot: +8 entry count, +0x10 index (stock table 0x006D7900, moved by paktable.json)
    assert read(0x0006238D, 6) == bytes.fromhex('8b8210796d00') and read(0x00062399, 6) == bytes.fromhex('8bb208796d00')
    assert read(0x00062226, 3) == bytes.fromhex('894e08')
    table = json.loads((ROOT / 'config/guest-code-patches/paktable.json').read_text(encoding='utf-8'))
    values = {e['address']: e.get('value') for e in table['patches'] if 'address' in e}
    assert values['0x0006238D'] == '0x00907010' and values['0x00062399'] == '0x00907008'
    text = DISCO.read_text(encoding='utf-8')
    assert '#define DSC_PAKTABLE 0x00907000u' in text and '#define DSC_SLOT 0x1Cu' in text
    # texture names are "textures/%06x.xbt" (the CRC the index holds)
    assert read(0x0047D3C8, 18) == b'textures/%06x.xbt\0'


def _private():
    private = ROOT / 'private'
    if not (private / 'game/pak').is_dir() and len(ROOT.parents) > 2:
        private = ROOT.parents[2] / 'private'
    return private


GAME = _private() / 'game/pak'
VIRTUAL = 'stream/lv100/tileset/tileset6.pak'
DISCO_PAK = 'arcademem/l_56_arm.pak'
ALIAS = {0xE871CC: 0x7DDE70, 0x25AE6E: 0x0D9139, 0x294554: 0x7B0662, 0x79B187: 0x3C2652, 0x666443: 0xF05B7C}
KEPT = (0xB2A030, 0x5A89BC, 0xD7E9C7, 0x939772)


def _crc(name):
    return zlib.crc32(name.encode()) & 0xFFFFFFFF


def _tex(i):
    return _crc('textures/%06x.xbt' % i)


def _pak(rel):
    data = (GAME / rel).read_bytes()
    assert data[:4] == b'P5CK', rel
    off, n = struct.unpack_from('<II', data, 4)
    return data, {c: (p, s, z) for c, p, s, z in struct.iter_unpack('<4I', data[off:off + n])}


def _lists(data, idx):
    """Model texture lists: {crc: [(w0, w1, w2, w3), ...]}."""
    out = {}
    for c, (pos, size, comp) in idx.items():
        if comp or size < 16:
            continue
        start = struct.unpack_from('<I', data, pos)[0]
        if not 12 <= start <= size - 4:
            continue
        rows = []
        for p in range(pos + start, pos + size - 15, 16):
            row = struct.unpack_from('<4I', data, p)
            if row[0] == 0xFFFFFFFF:
                out[c] = rows
                break
            if row[0] != row[1] or row[0] >> 24:
                break
            rows.append(row)
    return out


def _same(a, b):
    (da, ia), (db, ib) = a, b
    common = set(ia) & set(ib)
    diff = [c for c in common if da[ia[c][0]:ia[c][0] + ia[c][1]] != db[ib[c][0]:ib[c][0] + ib[c][1]]]
    return len(common), diff


def test_disco_pak_audit():
    """The Disco ids are textures of l_56_arm.pak; the aliased Virtual ids are listed
    by the tile models (both words, flags 0) and present in no other pak MapMaker
    reads; l_56_arm.pak shares only byte-identical names with them (skipped without
    game data)."""
    if not (GAME / DISCO_PAK).exists() or not (GAME / VIRTUAL).exists():
        pytest.skip('game paks not available locally')
    disco, virtual = _pak(DISCO_PAK), _pak(VIRTUAL)
    for v, d in ALIAS.items():
        pos, size, comp = disco[1][_tex(d)]
        w, h, w2, h2, _, kind, sentinel = struct.unpack_from('<7I', disco[0], pos)
        assert not comp and w == w2 and h == h2 and sentinel == 0xFFFFFFFF and kind == 0, '%06x' % d   # DXT1, opaque
        assert _tex(v) in virtual[1] and _tex(v) not in disco[1]
    lists = _lists(*virtual)
    tiles = [_crc(f'assets/ts/models/tileset6/tile6_{m}/xbox/output/tile6_{m}.xbr') for m in range(1, 30)]
    assert all(t in lists for t in tiles)
    used = {row[0] for t in tiles for row in lists[t]}
    assert used == set(ALIAS) | set(KEPT), sorted('%06x' % u for u in used)
    assert all(row[2] == 0 and row[3] == 0 for rows in lists.values() for row in rows)
    others = ['arcade/l_100.pak', 'arcade/l_102.pak', 'gun.pak', 'chr.pak', 'arcademem/l_100m.pak',
              'arcademem/l_102m.pak', 'arcade/l_56_ar.pak']
    for rel in others:
        data, idx = _pak(rel)
        assert not any(row[0] in ALIAS for rows in _lists(data, idx).values() for row in rows), rel
        assert not any(_tex(v) in idx for v in ALIAS), rel
    expected = {'arcademem/l_100m.pak': 162, 'gun.pak': 27, 'arcademem/l_102m.pak': 19,
                'arcade/l_100.pak': 0, 'arcade/l_102.pak': 0, 'chr.pak': 0, VIRTUAL: 0}
    for rel, n in expected.items():
        common, diff = _same(disco, _pak(rel) if rel != VIRTUAL else virtual)
        assert (common, diff) == (n, []), rel


def test_abstract_textures_audit():
    """Abstract's tile models list textures found only in arcademem/l_102m.pak, which
    scene 100 does not mount by itself (it mounts l_100m.pak), so the Abstract play
    hook mounts it; its names shared with the play paks are byte-identical (skipped
    without game data)."""
    if not (GAME / 'arcademem/l_102m.pak').exists():
        pytest.skip('game paks not available locally')
    editor = _pak('arcade/l_102.pak')
    lists = _lists(*editor)
    tiles = [_crc(f'assets/ts/models/tileset1/tile1_{m}/xbox/output/tile1_{m}.xbr') for m in range(1, 30)]
    ids = {row[0] for t in tiles for row in lists[t]}
    mem = _pak('arcademem/l_102m.pak')
    play = {rel: _pak(rel) for rel in ('arcademem/l_100m.pak', 'arcade/l_100.pak', 'arcade/l_102.pak',
                                       'stream/lv100/tileset/tileset1.pak', 'gun.pak', 'chr.pak')}
    assert len(ids) == 87 and all(_tex(i) in mem[1] for i in ids)
    assert not any(_tex(i) in p[1] for i in ids for p in play.values())
    for rel, pak in play.items():
        assert _same(mem, pak)[1] == [], rel
    from test_mapmaker_sky import _xbe
    read = _xbe()
    # scenes 100..102 mount their own memory pak: sub_00062610(scene, 1) "pak/%s%s/l_%d%s.pak"
    assert read(0x0006289B, 4) == bytes.fromhex('6a0157e8') and read(0x0047874C, 20) == b'pak/%s%s/l_%d%s.pak\0'


def test_wrappers_documented():
    doc = DOC.read_text(encoding='utf-8')
    text = DISCO.read_text(encoding='utf-8')
    for target, name in (('0008E3E0', 'textures'), ('002FC0A0', 'menu_close'), ('00338180', 'stream')):
        assert f'WRAP sub_{target} -> pc_mapmaker_disco_{name}' in doc, name
        assert f'void pc_mapmaker_disco_{name}(void (*original)(void))' in text, name
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    stubs = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    for name in ('textures', 'menu_close', 'stream'):
        assert f'void pc_mapmaker_disco_{name}(void (*original)(void));' in header
        assert f'void pc_mapmaker_disco_{name}(void (*original)(void)) {{ original(); }}' in stubs
    assert '#include "pc_mapmaker_disco.inc"' in SMX.read_text(encoding='utf-8')
    workflow = (ROOT / '.github/workflows/runtime-patches.yml').read_text(encoding='utf-8')
    assert 'tests/mapmaker/test_mapmaker_disco.py' in workflow
