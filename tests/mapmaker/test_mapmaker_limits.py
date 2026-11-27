"""Compile MapMaker capacity stage A (AI pools and editor limits) against fake guest memory."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
LIMITS = ROOT / 'src/interface/pc_mapmaker_limits.inc'
PATCH = ROOT / 'patches/graphics/interface/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-mapmaker-meter-stagea.patch'

MUTATIONS = {
    'limits-without-pools': ('mapmaker_limits.limits=mapmaker_limits.pools && l &&', 'mapmaker_limits.limits=l &&'),
    'pools-off-by-default': ("mapmaker_limits.pools=!(p && p[0]=='0' && !p[1]);", 'mapmaker_limits.pools=p!=NULL;'),
    'pools-any-scene': (' || WORD_AT(0x0079094Cu)!=100u) {', ') {'),
    'pools-not-cleared': ('memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,bytes);', '(void)0;'),
    'cover-raised': ('#define MAPMAKER_LIMIT_COVER 14080u', '#define MAPMAKER_LIMIT_COVER 14336u'),
    'cover-pool-stock': ('#define MAPMAKER_POOL_COVER 14592u', '#define MAPMAKER_POOL_COVER 1280u'),
    'cover-layout-b': ('#define MAPMAKER_LIMIT_COVER 14080u', '#define MAPMAKER_LIMIT_COVER 9984u'),
    'pools-ignore-nav': ('uint32_t p=mapmaker_pool_source?mapmaker_pool_source(bytes):0u;', 'uint32_t p=0u;'),
    'pools-no-fallback': ('    if(!p) p=guest_call(sub_0003DFA0,2,bytes,1u);', '    if(0) p=guest_call(sub_0003DFA0,2,bytes,1u);'),
    'link-pool-editor-limit': ('mapmaker_pool(MAPMAKER_POOL_LINKS*20u)', 'mapmaker_pool(MAPMAKER_LIMIT_LINKS*20u)'),
    # Links: the pool stays within int16 A* indices; template links plus half
    # the side connection slots (one link per facing slot pair) fit 32,256.
    'links-pool-stock-raise': ('#define MAPMAKER_POOL_LINKS 32512u', '#define MAPMAKER_POOL_LINKS 16336u'),
    'links-past-int16': ('#define MAPMAKER_POOL_LINKS 32512u', '#define MAPMAKER_POOL_LINKS 32768u'),
    'links-no-stitch': ('    return p->cost[2]+h->cost[2]+(p->slots+h->slots+1u)/2u;', '    return p->cost[2]+h->cost[2];'),
    'links-flat-16': ('    return p->cost[2]+h->cost[2]+(p->slots+h->slots+1u)/2u;',
                      '    return p->cost[2]+h->cost[2]+16u*(p->pieces+h->pieces);'),
    'links-both-sides': ('(p->slots+h->slots+1u)/2u', '(p->slots+h->slots)'),
    'links-round-down': ('(p->slots+h->slots+1u)/2u', '(p->slots+h->slots)/2u'),
    'links-held-slots-ignored': ('(p->slots+h->slots+1u)/2u', '(p->slots+1u)/2u'),
    'links-updown-slots': ("if((e&0xFFu)>=1u && (e&0xFFu)<=4u) slots+=slot;", "if((e&0xFFu)>=1u && (e&0xFFu)<=6u) slots+=slot;"),
    'links-catalogue-offset': ('n=WORD_AT(row+0x14u);', 'n=WORD_AT(row+0x44u);'),
    'links-slots-unclamped': ('return slots<MAPMAKER_LINK_SLOTS_MAX?slots:MAPMAKER_LINK_SLOTS_MAX;', 'return slots;'),
    'links-count-unchecked': ('if(n>40u || !MAPMAKER_PTR_OK', 'if(!MAPMAKER_PTR_OK'),
    'links-no-slack': ('#define MAPMAKER_LIMIT_LINKS (MAPMAKER_POOL_LINKS-MAPMAKER_LINK_SLACK)',
                       '#define MAPMAKER_LIMIT_LINKS MAPMAKER_POOL_LINKS'),
    'links-held-ignored': ('if(mapmaker_link_total(p,h)>MAPMAKER_LIMIT_LINKS)',
                           'if(mapmaker_link_cost(p)>MAPMAKER_LIMIT_LINKS)'),
    'no-held-pieces': ('p->pieces+h->pieces>MAPMAKER_LIMIT_PIECES', 'p->pieces>MAPMAKER_LIMIT_PIECES'),
    # Engine objects: pieces + items + 64 characters + 64 effects within the 2047-slot pool (slot << 21).
    'objects-unchecked': ('    if(mapmaker_objects_needed(p->pieces+h->pieces,sub)>MAPMAKER_LIMIT_OBJECTS) over|=MAPMAKER_CAP_OBJECTS;\n', ''),
    'objects-no-reserve': ('    return pieces+sub+MAPMAKER_OBJECTS_RESERVE;', '    return pieces+sub;'),
    # The reserve keeps 64 slots for explosions and debris on top of the characters.
    'objects-characters-only': ('#define MAPMAKER_OBJECTS_RESERVE (MAPMAKER_OBJECTS_CHARACTERS+MAPMAKER_OBJECTS_EFFECTS)',
                                '#define MAPMAKER_OBJECTS_RESERVE (MAPMAKER_OBJECTS_CHARACTERS)'),
    'objects-effects-32': ('#define MAPMAKER_OBJECTS_EFFECTS 64u ', '#define MAPMAKER_OBJECTS_EFFECTS 32u '),
    'objects-placed-only': ('mapmaker_objects_needed(p->pieces+h->pieces,sub)', 'mapmaker_objects_needed(p->pieces,sub)'),
    'objects-stock-pool': ('#define MAPMAKER_LIMIT_OBJECTS 2047u', '#define MAPMAKER_LIMIT_OBJECTS 1000u'),
    'objects-slot22-pool': ('#define MAPMAKER_LIMIT_OBJECTS 2047u', '#define MAPMAKER_LIMIT_OBJECTS 1023u'),
    # Pad groups 2047 (lazy distances above 1024): 1023 pieces at 1.5 groups each fit.
    'groups-round3': ('#define MAPMAKER_LIMIT_GROUPS 2047\n', '#define MAPMAKER_LIMIT_GROUPS 1024\n'),
    'groups-unbounded': ('#define MAPMAKER_LIMIT_GROUPS 2047\n', '#define MAPMAKER_LIMIT_GROUPS 4096\n'),
    'groups-off-by-one': ('if(groups>MAPMAKER_LIMIT_GROUPS)', 'if(groups>=MAPMAKER_LIMIT_GROUPS)'),
    # Regular pads: 16,128 in the editor, 16,384 in the pool (Layout C, ids past 8000).
    'pads-round3': ('#define MAPMAKER_LIMIT_PADS 16128u', '#define MAPMAKER_LIMIT_PADS 7680u'),
    'pads-pool-stock': ('#define MAPMAKER_POOL_PADS 16384u', '#define MAPMAKER_POOL_PADS 8000u'),
    'pads-past-ids': ('#define MAPMAKER_POOL_PADS 16384u', '#define MAPMAKER_POOL_PADS 16385u'),
    'sub-stock-round2': ('#define MAPMAKER_LIMIT_SUB 500u', '#define MAPMAKER_LIMIT_SUB 200u'),
    'sub-past-pool': ('#define MAPMAKER_LIMIT_SUB 500u', '#define MAPMAKER_LIMIT_SUB 501u'),
    'keycards-round2': ('    {0x00524CECu,2u,16u},', '    {0x00524CECu,2u,8u},'),
    'movers-round2': ('    {0x00524E3Cu,20u,160u},', '    {0x00524E3Cu,20u,80u},'),
    'markers-round2': ('    {0x00524EE4u,20u,160u},', '    {0x00524EE4u,20u,80u},'),
    # Pieces 1023 = rooms (rooms.json real rooms 1..0x3FF); 511 was the cap before, 255 before that.
    'pieces-stock': ('#define MAPMAKER_LIMIT_PIECES 1023u', '#define MAPMAKER_LIMIT_PIECES 200u'),
    'pieces-241': ('#define MAPMAKER_LIMIT_PIECES 1023u', '#define MAPMAKER_LIMIT_PIECES 241u'),
    'pieces-255': ('#define MAPMAKER_LIMIT_PIECES 1023u', '#define MAPMAKER_LIMIT_PIECES 255u'),
    'pieces-511': ('#define MAPMAKER_LIMIT_PIECES 1023u', '#define MAPMAKER_LIMIT_PIECES 511u'),
    'pieces-past-rooms': ('#define MAPMAKER_LIMIT_PIECES 1023u', '#define MAPMAKER_LIMIT_PIECES 1024u'),
    'memory-unchecked': ('    if(memory>MAPMAKER_LIMIT_MEMORY) over|=MAPMAKER_CAP_MEMORY;\n', ''),
    # The refusal reason (stderr): logged, and only when the caps passed change.
    'reason-not-logged': ('        mapmaker_limits_log(over,&p,&h,total_mem,groups,sub);\n', ''),
    'reason-every-frame': ('    if(over==last) return;\n', ''),
    'reason-wrong-count': ('(unsigned)(p->pieces+h->pieces),MAPMAKER_LIMIT_PIECES)', '(unsigned)p->pieces,MAPMAKER_LIMIT_PIECES)'),
    'categories-always': ('    if(!mapmaker_limits.limits) return;\n    mapmaker_categories_apply();',
                          '    mapmaker_categories_apply();\n    if(!mapmaker_limits.limits) return;'),
    'categories-overwrite': (' && WORD_AT(a)==mapmaker_categories[i].stock)', ')'),
    # Turrets: 3 x 24 = 72 of the 80 gun/owner slots (config/guest-code-patches/objects.json).
    'turrets-past-pool': ('    {0x00524E6Cu,8u,24u},', '    {0x00524E6Cu,8u,32u},'),
    'turrets-stock': ('    {0x00524E9Cu,8u,24u},', '    {0x00524E9Cu,8u,8u},'),
    # Teleporters need the sub_0009CD00 wrapper in pc_mapmaker_objects.inc.
    'teleporters-missing': ('    {0x00524D64u,2u,8u},\n', ''),
    'teleporters-past-wrapper': ('    {0x00524D34u,2u,8u},', '    {0x00524D34u,2u,16u},'),
    'cover-props-past-x2': ('{0x00524EFCu,66u,132u}', '{0x00524EFCu,66u,264u}'),
    'flags-clobbered': ('WORD_AT(editor+0x25c)=refuse?(flags|0x400000u):(flags&~0x400000u);',
                        'WORD_AT(editor+0x25c)=refuse?0x400000u:0u;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = LIMITS.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / LIMITS.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('limits.exe' if os.name == 'nt' else 'limits')
    source = ROOT / 'tests/mapmaker/mapmaker_limits.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    if mutation != 'fixed' and result.returncode != 0:
        return None                      # a size check refused it at compile time
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_limits(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    if exe is None:
        return
    env = {k: v for k, v in os.environ.items()
           if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS', 'TSFP_MAPMAKER_MEMORY_SCALE')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker limits ok' in result.stdout
        assert reasons_ok(result.stderr), result.stderr
    else:
        assert result.returncode != 0 or not reasons_ok(result.stderr), f'{mutation} was not detected'


def reasons_ok(stderr):
    """The refusal names the cap reached, logged once per change of the caps passed."""
    lines = [line for line in stderr.splitlines() if 'placement refused' in line]
    memory = ('[MAPMAKER] placement refused: memory estimate 2052407 of 2048000 at scale 8 '
              '(TSFP_MAPMAKER_ARENA_MB raises it);')
    # memory: refused two frames running, logged once
    pieces = '[MAPMAKER] placement refused: pieces 1024 of 1023;'
    both = ('[MAPMAKER] placement refused: pieces 1920 of 1023; '
            'engine objects 2048 of 2047 (pieces + items + 128 reserved);')
    return pieces in lines and both in lines and lines.count(memory) == 1


def test_stage_a_patch():
    patch = PATCH.read_text(encoding='utf-8')
    for name, hook in (('sub_0009A140', 'pc_display_mapmaker_pools'), ('sub_002FA720', 'pc_display_mapmaker_limits')):
        assert f'extern void {name}_gen(void);' in patch and f'{hook}({name}_gen);' in patch
    assert not any(line.startswith('-') for line in patch.split('@@', 2)[2].splitlines())
    raise_patch = next(ROOT.glob('patches/**/*-mapmaker-meter-raise.patch'))
    assert raise_patch.name < PATCH.name
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_limits.inc"' in guest
    assert guest.index('#include "pc_mapmaker_budget.inc"') < guest.index('#include "pc_mapmaker_limits.inc"')
    for stub in ('pc_display_mapmaker_pools', 'pc_display_mapmaker_limits'):
        assert f'void {stub}(void (*original)(void)) {{ original(); }}' in guest
