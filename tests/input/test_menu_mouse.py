"""Compile the production pointer policy and guest adapter on either host."""


import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('mutation', ['fixed', 'missing-source', 'hover-conflict', 'missing-slider-drag', 'missing-setting-drag', 'late-map-confirm', 'late-briefing-confirm', 'footer-only-character-confirm', 'late-lighting-back'])
def test_menu_mouse(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    source = ROOT / 'tests/input/menu_mouse.c'
    exe = tmp_path / ('menu.exe' if os.name == 'nt' else 'menu')
    if mutation == 'missing-slider-drag':
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        audio = (ROOT / 'src/interface/pc_menu_sliders.inc').read_text(encoding='utf-8')
        hook = 'if(WORD_AT(volume)!=value)'
        assert hook in audio
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(adapter, encoding='utf-8')
        (tmp_path / 'pc_menu_sliders.inc').write_text(audio.replace(hook, 'if(0 && WORD_AT(volume)!=value)'), encoding='utf-8')
    if mutation == 'missing-setting-drag':
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        sliders = (ROOT / 'src/interface/pc_menu_sliders.inc').read_text(encoding='utf-8')
        hook = 'menu_slider_set(track,(unsigned)(track-menu_slider_tracks),x);'
        assert hook in sliders
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(adapter, encoding='utf-8')
        (tmp_path / 'pc_menu_sliders.inc').write_text(sliders.replace(hook, 'if(track-menu_slider_tracks<2) '+hook), encoding='utf-8')
    if mutation == 'missing-source':
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        injection = 'menu_mouse_press(menu_mouse_dispatch,0x80u);'
        assert injection in adapter
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(adapter.replace(injection, '(void)0;'), encoding='utf-8')
    if mutation == 'hover-conflict':
        policy = (ROOT / 'src/interface/pc_menu_mouse.h').read_text(encoding='utf-8')
        guard = 'if(moved && !s->scrolling)'
        assert guard in policy
        (tmp_path / 'pc_menu_mouse.h').write_text(policy.replace(guard, 'if(moved)'), encoding='utf-8')
        # Quoted includes resolve alongside the adapter before -I directories.
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(
            (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8'), encoding='utf-8')
    if mutation == 'late-map-confirm':
        # Without the input-hook click the browser tick never sees Confirm.
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        hook = 'else if(!menu_map_browser_click(s,menu,x,y)) menu_briefing_click(s,menu);'
        assert hook in adapter
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(
            adapter.replace(hook, 'else menu_briefing_click(s,menu);'), encoding='utf-8')
    if mutation == 'late-briefing-confirm':
        # Without the briefing click a flag-4 page ignores a left click.
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        hook = 'if(!menu_map_browser_click(s,menu,x,y)) menu_briefing_click(s,menu);'
        assert hook in adapter
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(
            adapter.replace(hook, 'if(!menu_map_browser_click(s,menu,x,y)) (void)0;'), encoding='utf-8')
    if mutation == 'footer-only-character-confirm':
        # The old rule: only the footer Select confirmed a previewed character.
        characters = (ROOT / 'src/interface/pc_menu_characters.inc').read_text(encoding='utf-8')
        rule = 'menu_mouse_press(s,footer==1?0x120u:0x80u);'
        assert rule in characters
        (tmp_path / 'pc_menu_characters.inc').write_text(characters.replace(
            rule, 'if(footer==0 || footer==1) menu_mouse_press(s,footer?0x120u:0x80u);'), encoding='utf-8')
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(
            (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8'), encoding='utf-8')
    if mutation == 'late-lighting-back':
        # Without the input-hook Back the lighting list's tick never sees a right click.
        adapter = (ROOT / 'src/interface/pc_menu_mouse_guest.inc').read_text(encoding='utf-8')
        hook = 'if((s->actions&4) && mapmaker_pointer_tick_back(menu)) s->actions&=~4u;'
        assert hook in adapter
        (tmp_path / 'pc_menu_mouse_guest.inc').write_text(adapter.replace(hook, '(void)mapmaker_pointer_tick_back;'), encoding='utf-8')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src'), str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src'), str(source), '-lm', '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if mutation == 'missing-source':
        assert result.returncode != 0
        assert '&0x80' in result.stderr
        return
    if mutation == 'hover-conflict':
        assert result.returncode != 0
        assert 'selections==selected_before' in result.stderr
        return
    if mutation == 'missing-slider-drag':
        assert result.returncode != 0
        assert 'WORD_AT(0x5236bc)==4' in result.stderr
        return
    if mutation == 'missing-setting-drag':
        assert result.returncode != 0
        assert 'FLOAT_AT(value_address)==.25f' in result.stderr
        return
    if mutation == 'late-lighting-back':
        assert result.returncode != 0
        assert 'early_action==0x20' in result.stderr  # Back reaches the tick
        return
    if mutation == 'late-map-confirm':
        assert result.returncode != 0
        assert '!WORD_AT(m+0xc4)' in result.stderr  # late row confirm only
        return
    if mutation == 'late-briefing-confirm':
        assert result.returncode != 0
        assert 'early_source&0x80u' in result.stderr  # the briefing click confirms early
        return
    if mutation == 'footer-only-character-confirm':
        assert result.returncode != 0
        assert 'early_source&0x80u' in result.stderr  # a click off the footer must confirm
        return
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS:' in result.stdout
