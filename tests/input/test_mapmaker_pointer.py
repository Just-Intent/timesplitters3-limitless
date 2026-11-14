"""Compile the MapMaker editor pointer against fake guest memory on any host."""
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
ADAPTER = ROOT / 'src/interface/pc_mapmaker_pointer.inc'
POLICY = ROOT / 'src/interface/pc_mapmaker_pointer.h'
HOSTS = ('src/input/pc_controls.c', 'src/native/input_sdl.c')

# Each mutation breaks one contract the fixture must detect.
MUTATIONS = {
    'cannot-disable': (ADAPTER, "mapmaker_pointer.enabled=!(v && v[0]=='0' && !v[1]);", 'mapmaker_pointer.enabled=1;'),
    'default-off': (ADAPTER, "mapmaker_pointer.enabled=!(v && v[0]=='0' && !v[1]);", "mapmaker_pointer.enabled=v && v[0]=='1' && !v[1];"),
    'any-scene': (ADAPTER, '(int32_t)WORD_AT(0x0079094Cu)==MAPMAKER_POINTER_SCENE &&', ''),
    'child-menu': (ADAPTER, ' && !WORD_AT(menu+0x170);', ';'),
    'still-mouse-overrides': (ADAPTER, 'if(moved || pressed) mapmaker_pointer_place(x,y);', 'mapmaker_pointer_place(x,y);'),
    'no-max-clamp': (ADAPTER, 'if(px>FLOAT_AT(b+0x50)) {', 'if(0) {'),
    'wrong-player': (ADAPTER, 'b=MAPMAKER_POINTER_BLOCK+player*MAPMAKER_POINTER_STRIDE;', 'b=MAPMAKER_POINTER_BLOCK;'),
    'held-click-leaks': (POLICY, 'if (active != s->active) s->blocked = left_down;', ''),
    'row-carries-to-editor': (POLICY, '(s->row && !active && left_down)', '(s->row && left_down)'),
    'row-latch-sticks': (POLICY, 'if (!left_down) s->blocked = s->row = 0;', 'if (!left_down) s->blocked = 0;'),
    'no-scroll-clamp': (ADAPTER, 'if(target>last) target=last;', ''),
    'arrow-no-repeat-delay': (ADAPTER, 'if(now<mapmaker_pointer.grab_next) return;', ''),
    'thumb-ignores-grab': (ADAPTER, '(y-mapmaker_pointer.grab-s.track0)/(s.track1-s.track0)*s.content);', '(y-s.track0)/(s.track1-s.track0)*s.content);'),
    'scroll-any-scene': (ADAPTER, 'if(!mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu)) return 0;\n    if(!mapmaker_scroll_geometry', 'if(!mapmaker_pointer_enabled()) return 0;\n    if(!mapmaker_scroll_geometry'),
    'parent-any-menu': (ADAPTER, ' || WORD_AT(parent+0x170)!=menu) return 0;', ') return 0;'),
    'parent-disabled-rows': (ADAPTER, "if((type==4 || type==7 || type==8) && !(flags&5) &&\n           x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&\n           y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) {\n            if(WORD_AT(parent", "if((type==4 || type==7 || type==8) &&\n           x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&\n           y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) {\n            if(WORD_AT(parent"),
    'panel-any-menu': (ADAPTER, 'if(WORD_AT(menu+4)==0x0052E9D8u || WORD_AT(menu+4)==0x0052E9E8u) {', 'if(1) {'),
    'panel-no-a': (ADAPTER, '                pc_controls_mapmaker_row_press();\n                return item;\n            }\n    }', '                return item;\n            }\n    }'),
    'wheel-scroll-down-is-up': (ADAPTER, 'FLOAT_AT(menu+0x88)-(float)notches*3.f', 'FLOAT_AT(menu+0x88)+(float)notches*3.f'),
    'wheel-zoom-in-menus': (ADAPTER, 'if(mapmaker_pointer_context(menu)) { pc_controls_mapmaker_zoom(notches);return 1; }', 'pc_controls_mapmaker_zoom(notches);'),
    'zoom-no-cap': (POLICY, 'if (z->until > now + 1000u) z->until = now + 1000u;', ''),
    'zoom-no-reset': (POLICY, 'if (dir != z->dir || now >= z->until) { z->dir = dir; z->until = now; }', 'z->dir = dir; if (now >= z->until) z->until = now;'),
    'pan-any-state': (ADAPTER, '(WORD_AT(editor+0x25c)&0x0C044088u) ||', ''),
    'edge-pan-unclamped-left': (ADAPTER, 'fmaxf(-1.f,fminf(1.f,ex))*MAPMAKER_EDGE_SPEED', 'fminf(1.f,ex)*MAPMAKER_EDGE_SPEED'),
    'pan-no-clamp': (ADAPTER, 'c=left+w*.5f; if(c<0) left-=c; else if(c>MAPMAKER_GRID) left-=c-MAPMAKER_GRID;', ''),
    'pan-in-menus': (ADAPTER, 'if(root) mapmaker_pointer_navigate(', 'if(1) mapmaker_pointer_navigate('),
    'rotate-lost': (ADAPTER, 'else WORD_AT(menu+0xc4)|=0x20u; /* as the shared hook', 'else (void)0; /* as the shared hook'),
    'drag-rotates': (ADAPTER, 'if(mapmaker_pointer.right_travel<MAPMAKER_RIGHT_CLICK && guest_writable(menu,0x23c))', 'if(guest_writable(menu,0x23c))'),
    'right-never-clears': (ADAPTER, 'if(mapmaker_pointer_right_clears()) pc_controls_mapmaker_pulse(PC_MAPMAKER_PULSE_Y);\n            else ', ''),
    'right-clears-held': (ADAPTER, '!WORD_AT(editor+0x2f7c) && ', ''),
    'lighting-back-late': (ADAPTER, 'if(d!=0x0052DFF0u && d!=', 'if(d!='),
    'tick-back-any-list': (ADAPTER, '    if(d!=0x0052DFF0u && d!=0x0052E000u && d!=0x0052E020u && d!=0x0052E030u && d!=0x0052E040u) return 0;\n', ''),
    'tick-back-on-screen': (ADAPTER, ' || mapmaker_pointer_context(menu)) return 0;\n    d=', ') return 0;\n    d='),
    'tick-back-any-scene': (ADAPTER, 'if(!mapmaker_pointer_enabled() || !mapmaker_pointer_scene(menu) || mapmaker_pointer_context(menu)) return 0;\n    d=', 'if(!mapmaker_pointer_enabled() || mapmaker_pointer_context(menu)) return 0;\n    d='),
    'tick-back-no-bit': (ADAPTER, '    WORD_AT(menu+0xc4)|=0x20u;\n    return 1;\n}', '    return 1;\n}'),
    'row-any-scene': (ADAPTER, '(int32_t)WORD_AT(0x0079094Cu)!=MAPMAKER_POINTER_SCENE) return 0;', '0) return 0;'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        path, old, new = MUTATIONS[mutation]
        text = path.read_text(encoding='utf-8')
        assert old in text
        (tmp_path / path.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('pointer.exe' if os.name == 'nt' else 'pointer')
    source = ROOT / 'tests/input/mapmaker_pointer.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-o', str(exe), '-lm']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_pointer(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k != 'TSFP_MAPMAKER_POINTER'}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker pointer ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_pointer_wiring():
    guest = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
    assert '#include "pc_mapmaker_pointer.inc"' in guest
    # After the original stick update, before any early return of the menu hook.
    hook = guest.split('void pc_display_menu_mouse_input(void (*original)(void))', 1)[1]
    assert hook.index('original();') < hook.index('mapmaker_pointer_input(menu);') < hook.index('return;')
    # Editor rows get a real A press instead of the menu-engine confirm.
    confirm = guest.split('void pc_display_menu_mouse(void (*original)(void))', 1)[1]
    gate = confirm.index('if(!mapmaker_pointer_row_click()) {')
    assert gate < confirm.index('menu_mouse_press(menu_mouse_dispatch,0x80u);') < confirm.index('WORD_AT(menu+0xc4)|=0x100010u;')
    assert 'if(!hit && (actions&2) && !mapmaker_pointer_parent_press(menu,x,y))\n        mapmaker_pointer_scroll_press(menu,x,y);' in confirm
    # Both wheel samples (dispatch and layout fallback) go to the editor first.
    assert guest.count('.wheel && mapmaker_pointer_wheel(menu,') == 2
    # The editor screen keeps the right button (pan, rotate, clear).
    assert '(actions&4) && !(mapmaker_pointer_enabled() && mapmaker_pointer_context(menu))' in confirm
    # Lighting list Back: after the input read, before the tick; the late hook skips it.
    back = 'if((s->actions&4) && mapmaker_pointer_tick_back(menu)) s->actions&=~4u;'
    assert hook.index('s->sampled=1;') < hook.index(back) < hook.index('if(!(s->actions&2))')
    for host in HOSTS:
        text = (ROOT / host).read_text(encoding='utf-8')
        assert 'void pc_controls_mapmaker_pointer(int active)' in text
        assert 'pc_mapmaker_row_press(&mapmaker_click);' in text
        assert 'void pc_controls_mapmaker_editor(int active)' in text
        assert 'pc_mapmaker_zoom_add(&mapmaker_zoom,' in text
        assert 'void pc_controls_mapmaker_pulse(int button)' in text
        assert 'pc_mapmaker_zoom_key(&mapmaker_zoom,' in text
        click = text.split('int pc_controls_terminal_click(void)', 1)[1].split('return down;', 1)[0]
        assert 'pc_mapmaker_click(&mapmaker_click,' in click
    # SDL: the editor policy runs for every polled key (Windows: inline).
    native = (ROOT / 'src/native/input_sdl.c').read_text(encoding='utf-8')
    key = native.split('int pc_controls_key_down(int vk)', 1)[1].split('\n}', 1)[0]
    assert 'return mapmaker_key(vk,down,menu);' in key
