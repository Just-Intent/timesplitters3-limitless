"""Mouse aiming for mounted guns and vehicle gunner seats (src/pc_mounted.h).

The fixture runs the shared helper against a host buffer laid out like the
US guest structures. Mutations re-create the old behaviour (mouse look only
moves player yaw/pitch, which mode 0x400 sub_000D2470 overwrites) and must
fail; both input adapters must route mounted modes before ordinary look.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]

MUTATIONS = {
    # The Scotland truck gun: vehicle occupants fell through to player look.
    'no-vehicle-route': ('if(mode!=0x80 && mode!=0x400)return -1;', 'if(mode!=0x80)return -1;'),
    'no-scripted-route': ('if(mode!=0x80 && mode!=0x400)return -1;', 'if(mode!=0x400)return -1;'),
    'no-operator-check': ('*(const uint32_t*)(data+0xd8)!=owner)return 0;', '0)return 0;'),
    'no-limits': ('return (float)fmax(minimum,fmin(maximum,angle));', 'return (float)angle;'),
    'replays-motion': ('dx=input->dx;dy=input->dy;input->dx=input->dy=0;', 'dx=input->dx;dy=input->dy;'),
    'pitch-sign': ('pc_mounted_limit(pitch+dy*scale*(invert?-1:1)', 'pc_mounted_limit(pitch-dy*scale*(invert?-1:1)'),
}


def build_and_run(tmp_path, include_dir):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('mounted.exe' if os.name == 'nt' else 'mounted')
    sources = [str(ROOT / 'tests/mounted_mouse.c'), str(ROOT / 'src/pc_input.c')]
    includes = [str(include_dir), str(ROOT / 'src')]
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                   *['/I' + i for i in includes], *sources, '/Fe:' + str(exe)]
    else:
        command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                   *['-I' + i for i in includes], *sources, '-lm', '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'), reason='Host guest-memory guards require Windows or macOS')
@pytest.mark.parametrize('mutation', ['none', *MUTATIONS])
def test_mounted_mouse(tmp_path, mutation):
    include_dir = tmp_path / 'include'
    include_dir.mkdir()
    header = (ROOT / 'src/pc_mounted.h').read_text()
    if mutation != 'none':
        old, new = MUTATIONS[mutation]
        assert header.count(old) == 1, mutation
        header = header.replace(old, new)
    (include_dir / 'pc_mounted.h').write_text(header)
    shutil.copy(ROOT / 'src/pc_guest_memory.h', include_dir / 'pc_guest_memory.h')
    result = build_and_run(tmp_path, include_dir)
    if mutation == 'none':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: mounted mouse routing' in result.stdout
    else:
        assert result.returncode != 0 and 'FAIL' in result.stderr, (mutation, result.stdout, result.stderr)


@pytest.mark.parametrize('adapter', ['src/pc_controls.c', 'src/native/input_sdl.c'])
def test_adapters_route_mounted_guns_first(adapter):
    text = (ROOT / adapter).read_text()
    assert '#include "pc_mounted.h"' in text
    body = re.search(r'void pc_controls_apply_mouse\([^\n]*\).*?\n\}', text, re.S).group()
    mounted, look = body.find('pc_mounted_mouse('), body.find('pc_input_look(')
    camera = body.find('pc_camera_fields(')
    assert 0 <= mounted < camera < look, adapter
    # A consumed mounted batch (>= 0) must return before ordinary look.
    assert re.search(r'pc_mounted_mouse\([^;]*?\)\s*>=\s*0\s*\|\|', body, re.S), adapter
