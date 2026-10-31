"""Mouse-wheel scope zoom (src/input/pc_scope_wheel.h).

While player 1 looks through a zoomable scope, a wheel notch bound to
Next/Previous weapon holds the zoom-in/zoom-out key briefly instead of
switching weapons; iron sights, plain aim and rebound wheels keep their
behaviour. The fixture runs the shared helper against a host buffer laid out
like the US guest player/camera; header mutations must fail, and both input
adapters must use the helper in their wheel, key and player-update paths.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]

MUTATIONS = {
    # Iron sights / plain aim (no scope flag 0x40) must keep weapon switching.
    'iron-sight-zooms': ('if ((*(const uint32_t *)(p + 0xc) & 0x60) != 0x60) return 0;',
                         'if ((*(const uint32_t *)(p + 0xc) & 0x20) != 0x20) return 0;'),
    'no-zoom-range': ('return isfinite(minimum) && isfinite(maximum) && minimum < maximum;', 'return 1;'),
    'any-local-player': ('if (*(const int32_t *)(p + 4) != 0) return -1;', ''),
    'zooms-and-switches': ('w->until = now + PC_SCOPE_WHEEL_MAX_MS;\n    return 0;',
                           'w->until = now + PC_SCOPE_WHEEL_MAX_MS;\n    return steps;'),
    'queued-switch': ('(key == PC_SCOPE_WHEEL_DOWN_KEY && w->route_down)) return 0;',
                      '(key == PC_SCOPE_WHEEL_DOWN_KEY && w->route_down)) return down;'),
    'hijacks-rebound': ('w->route_up = route[0] && !other[0];', 'w->route_up = route[0];'),
    'wheel-down-zooms-in': ('dir = steps > 0 ? 1 : -1;', 'dir = steps > 0 ? -1 : 1;'),
    'stale-scope': ('now - w->seen < PC_SCOPE_WHEEL_FRESH_MS', '1'),
    'hold-outlives-scope': ('if (!view) { w->dir = 0; w->until = 0; }', ''),
    'unbounded-hold': ('if (w->until > now + PC_SCOPE_WHEEL_MAX_MS) w->until = now + PC_SCOPE_WHEEL_MAX_MS;', ''),
}


def build_and_run(tmp_path, include_dir):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('scope_wheel.exe' if os.name == 'nt' else 'scope_wheel')
    sources = [str(ROOT / 'tests/input/scope_wheel.c')]
    includes = [str(include_dir), str(ROOT / 'src')]
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX',
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
def test_scope_wheel(tmp_path, mutation):
    include_dir = tmp_path / 'include'
    include_dir.mkdir()
    header = (ROOT / 'src/input/pc_scope_wheel.h').read_text()
    if mutation != 'none':
        old, new = MUTATIONS[mutation]
        assert header.count(old) == 1, mutation
        header = header.replace(old, new)
    (include_dir / 'pc_scope_wheel.h').write_text(header)
    for name in ('runtime/pc_guest_memory.h', 'interface/pc_settings.h'):
        shutil.copy(ROOT / 'src' / name, include_dir / Path(name).name)
    result = build_and_run(tmp_path, include_dir)
    if mutation == 'none':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: scope wheel zoom' in result.stdout
    else:
        assert result.returncode != 0 and 'FAIL' in result.stderr, (mutation, result.stdout, result.stderr)


def function(text, signature):
    match = re.search(re.escape(signature) + r'[^{;]*\{.*?\n\}', text, re.S)
    assert match, signature
    return match.group()


@pytest.mark.parametrize('adapter,key_down,wheel', [
    ('src/input/pc_controls.c', 'int pc_controls_key_down(int key)', 'static void wheel_input(int steps)'),
    ('src/native/input_sdl.c', 'int tsfp_native_key_down(int vk)', 'void tsfp_native_input_event('),
])
def test_adapters_share_scope_wheel(adapter, key_down, wheel):
    text = (ROOT / adapter).read_text()
    assert '#include "pc_scope_wheel.h"' in text
    # The player update publishes bindings and scope state; guest reads and
    # the bindings lock come before the adapter's own lock.
    body = function(text, 'void pc_controls_apply_mouse(')
    bound, view = body.find('pc_settings_bound_keys('), body.find('pc_scope_view(')
    lock = min(i for i in (body.find('AcquireSRWLockExclusive(&lock)'),
                           body.find('pthread_mutex_lock(&camera_lock)')) if i >= 0)
    state = body.find('pc_scope_wheel_state(')
    assert 0 <= bound < lock and 0 <= view < lock < state, adapter
    assert body.find('pc_scope_wheel_bindings(') > lock, adapter
    # Wheel notches pass through the redirect before reaching weapon keys.
    wheel_body = function(text, wheel)
    assert 'pc_scope_wheel_input(' in wheel_body, adapter
    # Every polled key is filtered: routed wheel keys up, zoom key held.
    assert 'pc_scope_wheel_key(' in function(text, key_down), adapter
    # Menu transitions drop any pending zoom hold.
    assert 'pc_scope_wheel_reset(&scope_wheel)' in function(text, 'void pc_controls_menu_mode('), adapter


def test_bindings_expose_every_slot():
    text = (ROOT / 'src/input/pc_bindings.inc').read_text()
    body = function(text, 'int pc_settings_bound_keys(')
    assert 'keys[i][j]=g_ctl[0].bind[i].src[j].arg' in body
    assert 'j<PC_BINDING_SLOTS' in body and 'break' not in body
    assert 'int pc_settings_bound_keys(int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS]);' in (
        ROOT / 'src/interface/pc_settings.h').read_text()
    # The redirect's action numbers are the shared binding order.
    labels = re.search(r'pc_action_labels\[PC_ACTION_COUNT\] = \{(.*?)\};', text, re.S).group(1)
    names = re.findall(r'"([^"]*)"', labels)
    assert names[10] == 'Weapon mode / zoom in' and names[11] == 'Grenade type / zoom out'
    assert names[12] == 'Previous weapon' and names[13] == 'Next weapon'
    defaults = re.search(r'static const int keys\[PC_ACTION_COUNT\]\[PC_BINDING_SLOTS\] = \{(.*?)\};', text, re.S).group(1)
    assert '{VK_LEFT,VK_F24}, {VK_RIGHT,VK_F23}' in defaults  # wheel down/up switch weapons
