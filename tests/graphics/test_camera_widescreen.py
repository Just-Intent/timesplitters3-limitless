"""Mounted-gun / switch-camera view on a wide display (src/interface/pc_display_ui.inc):
the HUD mode 10 viewfinder (US sub_0014BFF0) and the remote-view static
(sub_000F8DC0) fill the window at 4:3 proportions; 4:3, narrower, split and
partial views keep stock placement. Fixture with mutations, wiring checks and
decoder evidence for the cause (skipped without the private XBE)."""
import hashlib
from pathlib import Path
import struct

import pytest

from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / 'src/interface/pc_display_ui.inc'
XBE = ROOT / 'private/game/default.xbe'

MUTATIONS = {
    'no-scope': ('    viewfinder_on=1;\n    original();\n    viewfinder_on=old;', '    original();'),
    'no-span': ('if(span)anchor=v<=320.f?0.f:640.f;', ''),
    'no-fit': ('x[i]=(int32_t)floorf(anchor+(v-anchor)*k+.5f);', 'x[i]=x[i];'),
    'no-arg-restore': ('    WORD_AT(frame+4u)=saved[0];WORD_AT(frame+12u)=saved[1];\n    return 1;', '    return 1;'),
    'no-split-guard': ('guest_call(sub_001CDAE0,0)==1 && guest_writable(view,0x24) &&\n        WORD_AT(view)==0 && (WORD_AT(view+4u)==639',
                       'guest_writable(view,0x24) &&\n        WORD_AT(view)==0 && (WORD_AT(view+4u)==639'),
    'no-reticle-range': ('caller>=VIEWFINDER_FIRST && ', ''),
    'no-rand': ('    WORD_AT(0x004E7950u)=lo;WORD_AT(0x004E7954u)=hi;WORD_AT(0x007497C8u)=count;\n', ''),
    'no-width-restore': ('    WORD_AT(record+0x24u)=size;\n', ''),
    'no-tile': ('WORD_AT(record+0x24u)=(size&0xFFFFu)|(uint32_t)tile<<16;', ''),
}


def production(mutation):
    text = UI.read_text(encoding='utf-8')
    place = text[text.index('static void ui_place('):text.index('/* US sub_000C8120 composes')]
    block = text[text.index('/* Mounted-gun / switch-camera view'):text.index('/* Verified US full-screen fade')]
    body = place + block
    layout = None
    if mutation in MUTATIONS:
        old, new = MUTATIONS[mutation]
        if old in body:
            body = body.replace(old, new)
        else:
            layout = (old, new)
    return body, layout


@pytest.mark.parametrize('mutation', ['none', *MUTATIONS])
def test_camera_widescreen(tmp_path, mutation):
    body, layout = production(mutation)
    fixture = (ROOT / 'tests/graphics/camera_widescreen.c').read_text(encoding='utf-8')
    if layout:
        header = (ROOT / 'src/interface/pc_ui_layout.h').read_text(encoding='utf-8')
        assert header.count(layout[0]) == 1, mutation
        (tmp_path / 'interface').mkdir()
        (tmp_path / 'interface/pc_ui_layout.h').write_text(header.replace(*layout), encoding='utf-8')
        fixture = fixture.replace('#include "interface/pc_ui_layout.h"', '#include "' +
                                  (tmp_path / 'interface/pc_ui_layout.h').as_posix() + '"')
    compile_and_run(tmp_path, fixture.replace('/* PRODUCTION */', body), fails=mutation != 'none')


def test_camera_widescreen_wiring():
    """The rectangle, textured-quad and text wrappers route viewfinder callers;
    the camera-gun dimming rectangle is a full-canvas pass; both new wrappers
    are declared."""
    text = UI.read_text(encoding='utf-8')
    overlay = text.split('int pc_display_transition_overlay(void (*original)(void))', 1)[1]
    assert overlay.index('if(viewfinder_primitive(original))return 1;') < overlay.index('switch(caller)')
    assert 'case 0x00137E6Fu:' in overlay.split('default:return 0;', 1)[0]
    effect = text.split('void pc_display_weapon_effect(void (*original)(void),unsigned kind)', 1)[1]
    assert effect.index('if(kind==0 && viewfinder_primitive(original))return;') < effect.index('if(kind==0) {')
    tutorial = (ROOT / 'src/interface/pc_tutorial_ui.inc').read_text(encoding='utf-8')
    entry = tutorial.split('void pc_display_tutorial_text(void (*original)(void))', 1)[1]
    assert entry.index('if(viewfinder_label(original)) return;') < entry.index('original();')
    header = (ROOT / 'src/interface/pc_display_menu.h').read_text(encoding='utf-8')
    assert 'void pc_display_viewfinder(void (*original)(void));' in header
    assert 'void pc_display_view_static(void (*original)(void));' in header
    menu = (ROOT / 'src/interface/pc_display_menu.c').read_text(encoding='utf-8')
    assert menu.index('#include "pc_display_ui.inc"') < menu.index('#include "pc_tutorial_ui.inc"')


def _va(image, va, n):
    base = struct.unpack_from('<I', image, 0x104)[0]
    count, headers = struct.unpack_from('<II', image, 0x11C)
    for i in range(count):
        _, vaddr, vsize, raw, rsize = struct.unpack_from('<IIIII', image, headers - base + i * 56)
        if vaddr <= va < vaddr + rsize:
            return image[raw + va - vaddr: raw + va - vaddr + n]
    raise AssertionError(hex(va))


def _call(image, at):
    data = _va(image, at, 5)
    assert data[0] == 0xE8, hex(at)
    return (at + 5 + struct.unpack('<i', data[1:])[0]) & 0xFFFFFFFF


def test_original_viewfinder_and_static():
    """US 45410066: HUD mode 10 pairs sub_0012E100 with sub_0014BFF0; the frame
    is 3% x 15% of the view; every remapped return address follows a call to
    the wrapped primitive; the static draw is sub_000F8DC0 from sub_000FE990."""
    if not XBE.is_file():
        pytest.skip('Local supported US XBE required; never included in CI')
    image = XBE.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    assert struct.unpack('<II', _va(image, 0x004E6900 + 10 * 0x1C, 8)) == (0x0012E100, 0x0014BFF0)
    assert struct.unpack('<f', _va(image, 0x00475CF8, 4))[0] == pytest.approx(0.03)
    assert struct.unpack('<f', _va(image, 0x004765E0, 4))[0] == pytest.approx(0.15)
    # Frame margins: mulss xmm0,[0x475CF8] (width) and [0x4765E0] (height).
    assert _va(image, 0x0014C2B4, 8) == bytes.fromhex('f30f5905f85c4700')
    assert _va(image, 0x0014C2DD, 8) == bytes.fromhex('f30f5905e0654700')
    for ret in (0x0014C39F, 0x0014C3D7, 0x0014C3F1, 0x0014C40A, 0x0014C422,
                0x0014C437, 0x0014C468, 0x0014C484, 0x0014C753):
        assert _call(image, ret - 5) == 0x0001EFB0, hex(ret)
    for ret in (0x0014C516, 0x0014C537, 0x0014C562, 0x0014C581):
        assert _call(image, ret - 5) == 0x0001F090, hex(ret)
    assert _call(image, 0x0014C127) == 0x0001F090  # aim reticle, before the frame
    assert 0x0014C12C < 0x0014C281
    assert _call(image, 0x0014C5C7) == 0x00041DC0
    # Camera-gun dimming in sub_00137E30, and the static tiles.
    assert _call(image, 0x00137E6A) == 0x0001EFB0
    assert _call(image, 0x000FEB4E) == 0x000F8DC0
    assert _call(image, 0x000F8E96) == 0x0001F090
    # Texture record tile width/height (+0x26/+0x2A) feed both the loop and UVs.
    assert _va(image, 0x000F8DD7, 9) == bytes.fromhex('0fb7582a570fb77826')
    assert _va(image, 0x0001B7B9, 4) == bytes.fromhex('0fb74826')
    # sub_00153B40: state * 0x343FD + 0x269EC3, call counter at 0x007497C8.
    assert _va(image, 0x00153B40, 6) == bytes.fromhex('8b0dc8977400')
    assert _va(image, 0x00153B4E, 5) == bytes.fromhex('68fd430300')
    assert _va(image, 0x00153B66, 5) == bytes.fromhex('05c39e2600')
