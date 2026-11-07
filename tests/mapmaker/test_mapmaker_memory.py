"""Compile the MapMaker memory hooks (heap guard, arena switch) against fake
guest memory, with mutations the fixture must detect."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_memory.inc'

MUTATIONS = {
    'guard-silent': ('    if(mapmaker_memory.lines<MAPMAKER_MEMORY_LOG_LINES) {', '    if(0) {'),
    'guard-every-call': ('    if(mapmaker_memory.over[pool]) return;              /* still overrun: counted */\n', ''),
    'guard-never-rearms': ('    if(after>=0) { mapmaker_memory.over[pool]=0;return; }', '    if(after>=0) return;'),
    'guard-unbounded': ('mapmaker_memory.lines<MAPMAKER_MEMORY_LOG_LINES', 'mapmaker_memory.lines<1000u'),
    'guard-one-pool': ('    return heap<=1u?0:heap==2u?1:-1;', '    return heap<=2u?0:-1;'),
    'guard-wrong-heap-arg': ('    mapmaker_memory_guard(original,2u);', '    mapmaker_memory_guard(original,1u);'),
    'guard-skips-original': ('    original();\n    mapmaker_memory_check(', '    mapmaker_memory_check('),
    'arena-default-on': ('    if(!t || !*t) return 0;', '    if(!t || !*t) return 8;'),
    'arena-unbounded': ('v>MAPMAKER_ARENA_MAX_MB) {', 'v>1000u) {'),
    'arena-no-trial': ('} else if((trial=xbox_ContiguousAlloc(size,0x1000u))==0) {', '} else if((trial=0x80001000u)==0) {'),
    'arena-trial-kept': ('            xbox_ContiguousFree(trial);\n', '            (void)trial;\n'),
    'arena-after-original': ('            WORD_AT(MAPMAKER_ARENA_SIZE)=size;raised=size;', '            raised=size;'),
    'arena-ignores-current': ('        if(stock!=MAPMAKER_ARENA_STOCK) {', '        if(0) {'),
    # Nav block (AI navigation data of a MapMaker map in the runtime heap).
    'nav-before-reset': (' || !mapmaker_nav.resets ||', ' ||'),
    'nav-any-scene': (' || WORD_AT(MAPMAKER_MEMORY_SCENE)!=MAPMAKER_NAV_SCENE || !bytes) return 0;', ' || !bytes) return 0;'),
    'nav-not-zeroed': ('    memset((void *)((uintptr_t)p+g_xbox_mem_offset),0,need);\n', '\n'),
    'nav-never-emptied': ('    mapmaker_nav.used=0;mapmaker_nav.reported=0;\n', '    mapmaker_nav.reported=0;\n'),
    'nav-unbounded': (' ||\n       need>mapmaker_nav.size-mapmaker_nav.used-skip) {', ') {'),
    'nav-unaligned': ('    mapmaker_nav.used+=skip;\n', '\n'),
    'nav-objects-not-registered': ('    mapmaker_objects_source=mapmaker_nav_objects;', '    (void)mapmaker_nav_objects;'),
    'nav-matrix-any-caller': ('    if(caller!=MAPMAKER_NAV_MATRIX_RET || heap!=1u ||', '    if(heap!=1u ||'),
    'nav-matrix-no-ret': ('    MAPMAKER_GUEST_RET();                      /* the original\'s ret', '    (void)0;                      /* the original\'s ret'),
    'nav-retries-heap': ('    if(!mapmaker_nav.tried) {', '    if(!mapmaker_nav.base) {'),
    'nav-aligned-redirected': ('    if(heap_arg==1u && mapmaker_nav_matrix(', '    if(mapmaker_nav_matrix('),
    'nav-report-every-alloc': ('    if(!mapmaker_nav.reported) { mapmaker_nav.reported=1;mapmaker_nav_report(); }',
                               '    mapmaker_nav_report();'),
    'nav-no-off-switch': ("    mapmaker_nav.off=v && v[0]=='0' && !v[1];", '    mapmaker_nav.off=0;(void)v;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('memory.exe' if os.name == 'nt' else 'memory')
    source = ROOT / 'tests/mapmaker/mapmaker_memory.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_memory(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe), str(tmp_path / 'memory.log')], env=env,
                            capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker memory ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_arena_constants_match_the_disassembly():
    """The arena size dword, its stock value and its two readers (decoder: the
    only references to 0x004C0450 are the loads at 0x00023CDA and 0x0003C34E)."""
    text = INC.read_text(encoding='utf-8')
    assert '#define MAPMAKER_ARENA_SIZE 0x004C0450u' in text
    assert '#define MAPMAKER_ARENA_STOCK 0x02CEC000u' in text
    assert '0x00023CDA' in text and '0x0003C34E' in text


def test_arena_dword_has_exactly_two_readers():
    """Decoder scan of the whole image for references to the arena dword."""
    from mapmaker_decoded import decoded
    refs = sorted(i.address for i in decoded()
                  if any(op[0] == 'mem' and not op[1] and not op[2] and op[4] == 0x004C0450 for op in i.ops))
    assert refs == [0x00023CDA, 0x0003C34E]
