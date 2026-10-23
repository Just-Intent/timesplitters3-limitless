"""Verified guest-code patches (the runtime's tools/recomp/disasm.py, added by
patches/*-guest-code-patches.patch).

A patch that applied to the wrong bytes, or quietly did nothing, would leave a
limit at its stock value while the code around it assumes the new one, so
every way an entry can be wrong must stop the lift.
"""

import hashlib
import json
import os
import sys

import pytest

RUNTIME = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "external", "ts3FPrecomp")
if not os.path.isfile(os.path.join(RUNTIME, "tools", "recomp", "disasm.py")):
    pytest.skip("runtime checkout is not prepared", allow_module_level=True)
sys.path.insert(0, RUNTIME)
pytest.importorskip("capstone")

from tools.recomp import config, disasm  # noqa: E402
if not hasattr(disasm, "apply_guest_patches"):
    pytest.skip("runtime checkout lacks the guest-code patch", allow_module_level=True)
from tools.recomp.disasm import GuestPatchError  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402


class guest_patches:
    load = staticmethod(disasm.load_guest_patches)
    apply = staticmethod(disasm.apply_guest_patches)

BASE = 0x00010000
#   cmp esi, 0x32          83 fe 32
#   cmp eax, 0x1f4000      3d 00 40 1f 00
#   jl  +0                 7c 00
#   mov eax, [0x00762cb8]  a1 b8 2c 76 00
#   ret                    c3
IMAGE = bytes.fromhex("83fe32" "3d00401f00" "7c00" "a1b82c7600" "c3")
FUNC = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(IMAGE),
               "_addr": BASE, "size": len(IMAGE)}}


def _install():
    config._install(
        [config.Section(".text", BASE, len(IMAGE), 0x0000, len(IMAGE), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="guest-patch-test")


def _table(tmp_path, *entries, sha=None):
    path = tmp_path / "patches.json"
    path.write_text(json.dumps({
        "title_id": "00000000",
        "xbe_sha256": sha or hashlib.sha256(IMAGE).hexdigest(),
        "patches": list(entries)}), encoding="utf-8")
    return guest_patches.load(str(path))


def _entry(eid, address, original, **kw):
    e = {"id": eid, "function": f"0x{BASE:08X}", "address": f"0x{address:08X}",
         "original_hex": original, "reason": "test"}
    e.update(kw)
    return e


def _lift(data, overrides):
    return FunctionTranslator(data, FUNC, operand_overrides=overrides) \
        .translate_function(BASE, FUNC[BASE])


def test_code_and_operand_patches_reach_the_lift(tmp_path):
    _install()
    table = _table(
        tmp_path,
        _entry("mem", BASE + 3, "3d00401f00", kind="code", new_hex="3d00007d00"),
        _entry("sub", BASE, "83fe32", kind="operand", operand=1, value="0x190"),
        _entry("disp", BASE + 10, "a1b82c7600", kind="operand", operand=1,
               value="0x008A0000"))
    data, overrides, functions = guest_patches.apply(table, IMAGE, FUNC)
    assert functions == {BASE}
    assert data[3:8] == bytes.fromhex("3d00007d00") and data[:3] == IMAGE[:3]
    assert IMAGE[3:8] == bytes.fromhex("3d00401f00")   # input left alone
    c = _lift(data, overrides)
    assert "cmp eax, 0x7D0000" in c and "0x1F4000" not in c, c
    assert "cmp esi, 0x190" in c, c
    assert "MEM32(0x8A0000)" in c and "0x762CB8" not in c, c
    plain = _lift(IMAGE, {})
    assert "cmp esi, 0x32" in plain and "MEM32(0x762CB8)" in plain, plain


@pytest.mark.parametrize("entry, message", [
    (_entry("bytes", BASE + 3, "3d00401f01", kind="code", new_hex="3d00007d00"),
     "expected"),
    (_entry("inside", BASE + 4, "00401f00", kind="code", new_hex="00007d00"),
     "instruction"),
    (_entry("mnemonic", BASE + 3, "3d00401f00", kind="code", new_hex="0500401f00"),
     "boundaries"),
    (_entry("branch", BASE + 8, "7c00", kind="code", new_hex="7c01"),
     "boundaries"),
    (_entry("jump-op", BASE + 8, "7c00", kind="operand", operand=0, value=9),
     "branches"),
    (_entry("width", BASE, "83fe32", kind="operand", operand=0, value=1),
     "register"),
    (_entry("outside", BASE + len(IMAGE), "c3", kind="code", new_hex="90"),
     "outside"),
])
def test_bad_entries_stop_the_lift(tmp_path, entry, message):
    _install()
    with pytest.raises(GuestPatchError, match=message):
        guest_patches.apply(_table(tmp_path, entry), IMAGE, FUNC)


def test_imm_must_fit_an_8_bit_operation(tmp_path):
    image = bytes.fromhex("80f932" "c3")          # cmp cl, 0x32
    func = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(image),
                   "_addr": BASE, "size": len(image)}}
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="guest-patch-test")
    table = _table(tmp_path, _entry("byte", BASE, "80f932", kind="operand",
                                    operand=1, value=0x100),
                   sha=hashlib.sha256(image).hexdigest())
    with pytest.raises(GuestPatchError, match="8 bits"):
        guest_patches.apply(table, image, func)


def test_wrong_xbe_and_bad_tables_are_refused(tmp_path):
    _install()
    table = _table(tmp_path, _entry("x", BASE, "83fe32", kind="operand",
                                    operand=1, value=1), sha="00" * 32)
    with pytest.raises(GuestPatchError, match="written for XBE"):
        guest_patches.apply(table, IMAGE, FUNC)
    with pytest.raises(GuestPatchError, match="overlaps"):
        _table(tmp_path,
               _entry("a", BASE, "83fe32", kind="operand", operand=1, value=1),
               _entry("b", BASE + 1, "fe32", kind="code", new_hex="fe33"))
    with pytest.raises(GuestPatchError, match="duplicate"):
        _table(tmp_path,
               _entry("a", BASE, "83fe32", kind="operand", operand=1, value=1),
               _entry("a", BASE + 3, "3d00401f00", kind="code", new_hex="3d00007d00"))
    with pytest.raises(GuestPatchError, match="same length"):
        _table(tmp_path, _entry("a", BASE + 3, "3d00401f00", kind="code", new_hex="3d00"))
