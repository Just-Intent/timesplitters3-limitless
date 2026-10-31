"""MapMaker Story AI bosses and the creature probe (src/interface/pc_mapmaker_bosses.inc).

Compiles the policy against fake guest memory with mutation cases (the Boss
row, cycling, the per-entry table through delete/reset/compile/load, chunk
byte +0x29, health multiplier, nearest-boss bar through sub_00139740, the
read-only probe), and, with the private XBE, checks the guest sites it relies
on in the lifter's decode: chunk byte +0x29 is never touched by the stock
writer, loader or play readers; the story boss bar sub_0014CC80 and its draw
helper sub_00139740 work as the hook assumes.
"""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_bosses.inc'

MUTATIONS = {
    'always-on': ('static int bos_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }',
                  'static int bos_on(void) { mapmaker_limits_read(); return 1; }'),
    'row-any-return': ('if(r!=BOS_ROWS_AT || cb!=BOS_CLOSE', 'if(cb!=BOS_CLOSE'),
    'row-type5': ('    if(s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) return;',
                  '    if(s>=bos_count(e)) return;'),
    'click-closes': ('    MAPMAKER_GUEST_RET();                         /* sub_0031CAC0: ret; the screen stays open */',
                     '    original();'),
    'click-no-ret': ('    MAPMAKER_GUEST_RET();                         /* sub_0031CAC0: ret; the screen stays open */', ''),
    'cycle-skips-x8': ('level=(bos_get(e,s)+1u)%(BOS_LEVELS+1u);', 'level=(bos_get(e,s)+1u)%BOS_LEVELS;'),
    'label-not-in-place': ('    uint32_t text=bos_text(0);\n    if(text) bos_put_text',
                           '    uint32_t text=xbox_HeapAlloc(BOS_NAME,4u);\n    if(text) bos_put_text'),
    'no-colour-check': ('    if(bos.colour[i]!=bos_colour_of(e,i)) {', '    if(0) {'),
    'delete-no-shift': ('    memmove(bos.level+i,bos.level+i+1u,before-i-1u);', ''),
    'delete-keeps-tail': ('    bos.level[before-1u]=0;', ''),
    'reset-keeps': ('    original();\n    memset(bos.level,0,sizeof bos.level);', '    original();'),
    'wrong-byte': ('#define BOS_CHUNK_FLAG 0x29u', '#define BOS_CHUNK_FLAG 0x28u'),
    'wrong-mark': ('#define BOS_FLAG_MARK 0xB0u', '#define BOS_FLAG_MARK 0x80u'),
    'accept-any-flag': ('    if((flag&BOS_FLAG_MASK)!=BOS_FLAG_MARK) {', '    if(0) {'),
    'load-no-index-check': ('if(level && WORD_AT(bos_entry(e,base+k)+BOS_ENTRY_INDEX)!=x.index[k]) {', 'if(0) {'),
    'compile-off-too': ('    if(!bos_on() || !(e=bos_editor())) return;\n    bos_walk(container,bos_count(e),bos_compile_chunk,&e);',
                        '    if(!(e=bos_editor())) return;\n    bos_walk(container,bos_count(e),bos_compile_chunk,&e);'),
    'multiplier-linear': ('return level?1u<<(level-1u):1u;', 'return level?level:1u;'),
    'scale-every-frame': ('    if(bos_play.scaled[k].record==rec && bos_play.scaled[k].max_bits==bits) return;   /* already ours */', ''),
    'no-rescale': ('    if(bos_play.scaled[k].record==rec && bos_play.scaled[k].max_bits==bits) return;   /* already ours */',
                   '    if(bos_play.scaled[k].record==rec) return;'),
    'cur-not-scaled': ('    if(cur<=max*1.0001f) cur*=m;', ''),
    'range-2d': ('d=dx*dx+dy*dy+dz*dz;', 'd=dx*dx+dy*dy;'),
    'farthest': ('(best && d>=best_d)', '(best && d<=best_d)'),
    'dead-shown': ("        if(!(bos_getf(ent+BOS_ENT_HEALTH)>0.0f)) continue;\n", ''),
    'humans-included': ('for(i=humans<count?humans:count;i<count;i++)', 'for(i=0;i<count;i++)'),
    'no-entity-check': (' || WORD_AT(ent+BOS_ENT_AI)!=a || ', ' || '),
    'over-stock-bar': ("    if(!ent || (WORD_AT(hud+BOS_HUD_FLAGS)&1u) ||", '    if(!ent ||'),
    'any-mode': (" || BOS_B(BOS_MODE)!=10u) return;", ') return;'),
    'any-hud': (' || hud!=BOS_HUD+p*BOS_HUD_STRIDE) return;', ') return;'),
    'no-restore': ('    drawn=WORD_AT(buf+0x40u);\n    restore_registers(&r);', '    drawn=WORD_AT(buf+0x40u);\n    g_esp=r.sp;'),
    'no-flag': ('    if(drawn) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;', ''),
    'flag-always': ('    if(drawn) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;', '    WORD_AT(hud+BOS_HUD_FLAGS)|=1u;'),
    'no-name': ('bos_play.name[index][0]?bos_play.name[index]:"Boss"', '"Boss"'),
    'range-ignored': ('        if(f>=1.0f && f<=100000.0f) r=f;', ''),
    'range-unclamped': ('        if(f>=1.0f && f<=100000.0f) r=f;', '        r=f;'),
    'level-keeps-active': ('    memset(&bos_play,0,sizeof bos_play);\n    if(!bos_on()) return;',
                           '    if(!bos_on()) return;\n    memset(&bos_play,0,sizeof bos_play);'),
    'probe-no-dedupe': ('        if(k<bos_probe.level_new) continue;', ''),
    'probe-no-level': ('    if(scene!=bos_probe.scene || pool!=bos_probe.pool) bos_probe_level(scene,pool);',
                       '    if(scene!=bos_probe.scene) bos_probe_level(scene,pool);'),
    'probe-writes': ('            model=bos_get16(ent+BOS_ENT_MODEL);', '            model=bos_get16(ent+BOS_ENT_MODEL);WORD_AT(ent+0x1F0u)^=1u;'),
    'probe-always': ("        bos_probe.on=v && v[0]=='1' && !v[1];", '        bos_probe.on=1;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('bosses.exe' if os.name == 'nt' else 'bosses')
    source = ROOT / 'tests/mapmaker/mapmaker_bosses.c'
    include = ['/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src')]
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', *include, str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                *['-I' + i[2:] for i in include], str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_bosses(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker bosses ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table():
    """No guest code changes: the save byte is one the stock writer leaves zero."""
    assert not (ROOT / 'config/guest-code-patches/bosses.json').exists()


# ---- guest sites in the lifter's decode (skipped without the private XBE) ----

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


# The Story AI chunk (tag 0x00C7766C) case of every other chunk walker in game
# code (the only `cmp eax, 0xC7766C` sites besides the writer's store) plus the
# helpers given the chunk pointer, and the editor's writer / loader ranges
# (docs/mapmaker/mapmaker-aitemplates.md).
CHUNK_READERS = (
    (0x0023F511, 0x0023F546),   # sub_0023F390: reads +0x2A only
    (0x00240757, 0x00240783),   # sub_00240590: sub_0033EC30(data), counts
    (0x00241DB9, 0x00242386),   # sub_00240C50 (spawn records): data = ebp
    (0x00337E2D, 0x00337E68),   # sub_00337D80: rewrites the name +0x00..+0x1F
    (0x0033EC30, 0x0033EC79),   # sub_0033EC30(data): reads +0x20
)
WRITER = (0x003013E1, 0x003015E5)
LOADER = (0x00302428, 0x0030267F)


def test_sai_chunk_byte_0x29_unused():
    insns = decoded()
    writer = [i for i in insns if WRITER[0] <= i.address < WRITER[1]]
    loader = [i for i in insns if LOADER[0] <= i.address < LOADER[1]]
    assert writer and loader
    for i in writer:                               # data = ebx + esi
        for op in mem(i):
            if {op[1], op[2]} == {'ebx', 'esi'}:
                assert op[4] != 0x29, hex(i.address)
                if op[4] == 0x28:                  # the toggle byte, not a word over +0x29
                    assert op[5] == 1, hex(i.address)
    for i in loader:                               # data = ebx
        for op in mem(i):
            if op[1] == 'ebx' and not op[2]:
                assert op[4] != 0x29 and not (op[4] == 0x28 and op[5] > 1), hex(i.address)
    tags = sorted(i.address for i in insns if i.mnemonic == 'cmp' and ('imm', 0x00C7766C) in i.ops)
    assert tags == [0x0023F511, 0x00240757, 0x00241DB9, 0x00337E2D]
    for start, end in CHUNK_READERS:
        body = [i for i in insns if start <= i.address < end]
        assert body, hex(start)
        for i in body:
            if i.mnemonic == 'lea':
                continue
            for op in mem(i):
                if op[1] != 'esp':
                    assert op[4] != 0x29 and not (op[4] == 0x28 and op[5] > 1), hex(i.address)
    a = by_address()
    assert mem(a[0x00301459])[0][4] == 0x28 and mem(a[0x00301459])[0][5] == 1
    assert mem(a[0x0030146A])[0][4] == 0x2A and mem(a[0x0030146A])[0][5] == 2
    assert mem(a[0x0030143F])[0][4] == 0x22 and mem(a[0x0030143F])[0][5] == 2    # writer: loop index


def test_edit_screen_rows_site():
    a = by_address()
    call = a[0x0031D07E]
    assert call.call and call.function == 0x0031CB90 and '0x784a0' in call.text
    read = a[0x0031D083]
    assert read.mnemonic == 'mov' and mem(read)[0][1] == 'esp' and mem(read)[0][4] == 0x20


def test_story_boss_bar_sites():
    a = by_address()
    call = a[0x0014F256]                           # HUD frame -> boss bar
    assert call.call and call.function == 0x0014F0B0 and '0x14cc80' in call.text
    assert a[0x0014F0CC].mnemonic == 'imul' and ('imm', 0x177C) in a[0x0014F0CC].ops
    assert a[0x0014F0D2].mnemonic == 'add' and ('imm', 0x007A6160) in a[0x0014F0D2].ops
    assert a[0x0014F0DF].mnemonic == 'mov' and a[0x0014F0DF].text.replace(' ', '') == 'ebx,eax'
    # sub_0014CC80: story mode only, flag bit 0 of HUD block +8 set / cleared
    assert a[0x0014CCB3].mnemonic == 'cmp' and ('imm', 0x0A) in a[0x0014CCB3].ops
    sets, clears = a[0x0014D0D5], a[0x0014D0DD]
    assert sets.mnemonic == 'or' and mem(sets)[0][1] == 'ebx' and mem(sets)[0][4] == 8
    assert clears.mnemonic == 'and' and mem(clears)[0][1] == 'ebx' and mem(clears)[0][4] == 8
    assert a[0x0014CCC3].mnemonic == 'mov' and mem(a[0x0014CCC3])[0][4] == 0xFF4
    assert a[0x0014CCD6].mnemonic == 'movzx' and mem(a[0x0014CCD6])[0][4] == 0x94
    # sub_00139740: edi health fraction, esi smoothing, int *drawn = 1
    for site, disp in ((0x001398B8, 0x1CC), (0x001398C0, 0x1D8)):
        op = mem(a[site])[0]
        assert a[site].function == 0x00139740 and op[1] == 'edi' and op[4] == disp
    assert mem(a[0x0013994B])[0][1] == 'esi' and mem(a[0x0013994B])[0][4] == 0x171C
    out = a[0x00139ABC]
    assert mem(out)[0][1] == 'esp' and mem(out)[0][4] == 0x4C       # arg 3 after 0x18 + 0x28 + 4
    assert a[0x00139AC3].mnemonic == 'mov' and mem(a[0x00139AC3])[0][1] == 'ecx'
    assert a[0x00139ACC].mnemonic == 'ret' and a[0x00139ACC].text.strip() == ''
    # the stock caller: text, 0, &local after the call
    assert a[0x0014D0B1].call and '0x139740' in a[0x0014D0B1].text
    assert a[0x0014D0B6].mnemonic == 'add' and ('imm', 0x0C) in a[0x0014D0B6].ops


def test_character_and_health_sites():
    a = by_address()
    # sub_000DAC30: entity +0x7C = pool entry, entry +0 = entity, entry +0x48 = id
    assert mem(a[0x000DAE15])[0][4] == 0x7C and mem(a[0x000DAE18])[0][1] == 'esi' and mem(a[0x000DAE18])[0][4] == 0
    assert mem(a[0x000DAE1D])[0][4] == 0x48
    # health pair at AI start (sub_0015F8C0) and positions (sub_0027BA40)
    assert a[0x0015FAB4].function == 0x0015F8C0 and mem(a[0x0015FAB4])[0][4] == 0x1D8
    assert mem(a[0x0015FABA])[0][4] == 0x1CC
    for site, disp in ((0x0027BBE5, 0x68), (0x0027BBEF, 0x6C), (0x0027BBF9, 0x64)):
        assert mem(a[site])[0][4] == disp
