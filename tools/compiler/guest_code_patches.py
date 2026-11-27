"""Verified guest-code patches applied before lifting.

Used by translation_batch (--guest-patches, table in config/guest-code-patches)
and translation_decode (operand overrides). Formerly a runtime patch to
tools/recomp/disasm.py; now part of the project translator.

A title project can change constants in the guest code it recompiles -- a
limit compared against, an allocation size -- without touching the user's
XBE and without hand edits to generated C. The table is JSON:

    {"title_id": "45410066",
     "xbe_sha256": "<sha256 of the unmodified XBE>",
     "patches": [
       {"id": "...", "kind": "code", "function": "0x002FA720",
        "address": "0x002FAEF2", "original_hex": "3d00401f00",
        "new_hex": "3d00007d00", "reason": "..."},
       {"id": "...", "kind": "operand", "function": "0x002FA720",
        "address": "0x002FAF2E", "original_hex": "83fe32",
        "operand": 1, "value": "0x190", "reason": "..."}]}

`code` rewrites bytes in the in-memory copy the lifter decodes. It must keep
every instruction boundary, mnemonic and branch/call target of the function,
so the disassembly stage's outputs (function bounds, xrefs) stay valid.

`operand` keeps the bytes and changes one decoded operand's value (an
immediate, or a memory displacement). The lifter emits C from the decoded
operand model, so `cmp esi, imm8` can be given a value an imm8 cannot hold.
The value must fit the operation width; branches, calls and `ret n` are
refused.

`relocate` moves a zero-initialised table to a larger one in the title's
reserved guest block (the table's top-level "extension": {"va", "size"}):

    {"id": "...", "kind": "relocate", "old": "0x00716148", "size": "0x78",
     "new": "0x008A0000", "new_size": "0x200",
     "sites": {"0x0009E663": "offset", "0x0009E2B4": "end", ...},
     "data_ok": []}

Every immediate or displacement in lifted code with a value in
[old, old + size] must be listed in `sites`, and every listed site must be
one: "offset" maps old + k to new + k, "end" maps old + size to new +
new_size, and "keep" leaves a reference to old + size that is another
variable, not the table end. Initialised data words in that range must be listed in `data_ok`
(confirmed not to be pointers). So a reference that was missed stops the
lift instead of reaching the old table.

Anything that does not verify stops the lift naming the entry: a patch that
silently failed to apply would leave a limit at its stock value while the
policy around it assumes the new one.
"""

import translation_layout as _layout


_REG_WIDTH = {}
for _names, _w in (("al cl dl bl ah ch dh bh", 1),
                   ("ax cx dx bx sp bp si di", 2),
                   ("eax ecx edx ebx esp ebp esi edi", 4)):
    for _n in _names.split():
        _REG_WIDTH[_n] = _w


class GuestPatchError(Exception):
    pass


def _int(value, what, entry_id):
    try:
        return int(value, 0) if isinstance(value, str) else int(value)
    except (TypeError, ValueError):
        raise GuestPatchError(f"guest patch {entry_id}: bad {what} {value!r}")


def load_guest_patches(path):
    """Load one table, or a folder of tables (merged; each file is one area)."""
    import json
    import os
    if os.path.isdir(path):
        merged, header, files = {"patches": []}, {}, 0
        for name in sorted(os.listdir(path)):
            if not name.endswith(".json"):
                continue
            files += 1
            with open(os.path.join(path, name), "r", encoding="utf-8") as fh:
                part = json.load(fh)
            for key in ("title_id", "xbe_sha256", "extension"):
                if part.get(key) is None:
                    continue
                if key in header and header[key] != part[key]:
                    raise GuestPatchError(f"{name}: {key} differs from the other tables")
                header[key] = part[key]
            merged["patches"].extend(part.get("patches") or [])
        if not files:
            raise GuestPatchError(f"{path}: no tables")
        merged.update(header)
        table = merged
    else:
        with open(path, "r", encoding="utf-8") as fh:
            table = json.load(fh)
    if not isinstance(table, dict) or not isinstance(table.get("patches"), list):
        raise GuestPatchError(f"{path}: expected an object with a 'patches' list")
    extension = None
    if table.get("extension") is not None:
        ext = table["extension"]
        extension = (_int(ext.get("va"), "extension va", "extension"),
                     _int(ext.get("size"), "extension size", "extension"))
    seen_ids, spans = set(), []
    entries, relocations = [], []
    for raw in table["patches"]:
        eid = raw.get("id")
        if not eid or eid in seen_ids:
            raise GuestPatchError(f"{path}: missing or duplicate patch id {eid!r}")
        seen_ids.add(eid)
        kind = raw.get("kind")
        if kind == "relocate":
            relocations.append(_load_relocation(raw, eid, extension, relocations))
            continue
        if kind not in ("code", "operand"):
            raise GuestPatchError(f"guest patch {eid}: unknown kind {kind!r}")
        e = {"id": eid, "kind": kind,
             "address": _int(raw.get("address"), "address", eid),
             "function": _int(raw.get("function"), "function", eid),
             "original": bytes.fromhex(raw.get("original_hex", "")),
             "reason": raw.get("reason", "")}
        if not e["original"]:
            raise GuestPatchError(f"guest patch {eid}: original_hex is required")
        if kind == "code":
            e["new"] = bytes.fromhex(raw.get("new_hex", ""))
            if len(e["new"]) != len(e["original"]) or e["new"] == e["original"]:
                raise GuestPatchError(
                    f"guest patch {eid}: new_hex must differ and have the same length")
        else:
            e["operand"] = _int(raw.get("operand"), "operand", eid)
            e["value"] = _int(raw.get("value"), "value", eid) & 0xFFFFFFFF
        span = (e["address"], e["address"] + len(e["original"]))
        for other in spans:
            if span[0] < other[1] and other[0] < span[1]:
                raise GuestPatchError(f"guest patch {eid}: overlaps another entry")
        spans.append(span)
        entries.append(e)
    return {"title_id": table.get("title_id"), "xbe_sha256": table.get("xbe_sha256"),
            "patches": entries, "relocations": relocations, "extension": extension}


def _load_relocation(raw, eid, extension, others):
    r = {"id": eid, "kind": "relocate",
         "old": _int(raw.get("old"), "old", eid), "size": _int(raw.get("size"), "size", eid),
         "new": _int(raw.get("new"), "new", eid),
         "new_size": _int(raw.get("new_size"), "new_size", eid),
         "sites": {}, "data_ok": set()}
    if r["size"] <= 0 or r["new_size"] < r["size"]:
        raise GuestPatchError(f"guest patch {eid}: new_size must be at least size")
    if extension is None or not (extension[0] <= r["new"] and
                                 r["new"] + r["new_size"] <= extension[0] + extension[1]):
        raise GuestPatchError(f"guest patch {eid}: new table is outside the reserved extension")
    for o in others:
        if r["new"] < o["new"] + o["new_size"] and o["new"] < r["new"] + r["new_size"]:
            raise GuestPatchError(f"guest patch {eid}: new table overlaps {o['id']}")
        if r["old"] <= o["old"] + o["size"] and o["old"] <= r["old"] + r["size"]:
            raise GuestPatchError(f"guest patch {eid}: old table overlaps {o['id']}")
    for addr, how in (raw.get("sites") or {}).items():
        if how not in ("offset", "end", "keep"):
            raise GuestPatchError(f"guest patch {eid}: site {addr} must be offset, end or keep")
        r["sites"][_int(addr, "site", eid)] = how
    r["data_ok"] = {_int(a, "data_ok", eid) for a in raw.get("data_ok") or ()}
    return r


def _function_bytes(data, func):
    config = _config()
    start, end = func["_addr"], func.get("end")
    off = config.va_to_file_offset(start)
    if not end or end <= start or off is None:
        return None
    return data[off:off + (end - start)]


def _decode(dis, data, func):
    raw = _function_bytes(data, func)
    if raw is None:
        return []
    return dis.disassemble_function(raw, func["_addr"], func["end"])


def _shape(insns):
    return [(i.address, i.size, i.mnemonic, i.call_target, i.jump_target) for i in insns]


def _config():
    # Read the module each time: configure_from_xbe() rebinds its sections.
    return _layout


def _width(insn, index):
    """Operation width in bytes for an operand override, from the other operand."""
    for i, op in enumerate(insn.operands):
        if i == index:
            continue
        if op.type == "reg" and op.reg in _REG_WIDTH:
            return _REG_WIDTH[op.reg]
        if op.type == "mem" and op.mem_size:
            return op.mem_size
    op = insn.operands[index]
    return op.mem_size if op.type == "mem" and op.mem_size else 4


def apply_guest_patches(table, xbe_data, func_db):
    """Verify every entry; return (patched bytes, operand overrides, functions)."""
    import hashlib
    digest = hashlib.sha256(xbe_data).hexdigest()
    if (table.get("xbe_sha256") or "").lower() != digest:
        raise GuestPatchError(
            f"guest patches were written for XBE {table.get('xbe_sha256')}, "
            f"not {digest}")
    data = bytearray(xbe_data)
    overrides, functions = {}, set()
    from translation_decode import Disassembler
    dis = Disassembler()
    by_function = {}
    for e in table["patches"]:
        func = func_db.get(e["function"])
        eid = e["id"]
        if func is None or not func.get("end"):
            raise GuestPatchError(f"guest patch {eid}: 0x{e['function']:08X} is not a known function")
        if not (func["_addr"] <= e["address"] < func["end"]
                and e["address"] + len(e["original"]) <= func["end"]):
            raise GuestPatchError(f"guest patch {eid}: address is outside its function")
        off = _config().va_to_file_offset(e["address"])
        if off is None or bytes(xbe_data[off:off + len(e["original"])]) != e["original"]:
            found = bytes(xbe_data[off:off + len(e["original"])]).hex() if off is not None else None
            raise GuestPatchError(
                f"guest patch {eid}: bytes at 0x{e['address']:08X} are {found}, "
                f"expected {e['original'].hex()}")
        by_function.setdefault(e["function"], []).append(e)
        functions.add(e["function"])

    for fn, entries in by_function.items():
        func = func_db[fn]
        before = _decode(dis, xbe_data, func)
        starts = {i.address: i for i in before}
        for e in entries:
            end = e["address"] + len(e["original"])
            insn = starts.get(e["address"])
            if insn is None:
                raise GuestPatchError(f"guest patch {e['id']}: does not start on an instruction")
            covered = e["address"]
            while covered < end:
                if covered not in starts:
                    raise GuestPatchError(f"guest patch {e['id']}: ends inside an instruction")
                covered += starts[covered].size
            if covered != end:
                raise GuestPatchError(f"guest patch {e['id']}: ends inside an instruction")
            if e["kind"] == "code":
                off = _config().va_to_file_offset(e["address"])
                data[off:off + len(e["new"])] = e["new"]
            else:
                if insn.size != len(e["original"]):
                    raise GuestPatchError(f"guest patch {e['id']}: an operand entry covers one instruction")
                if insn.is_branch or insn.is_call or insn.is_ret:
                    raise GuestPatchError(f"guest patch {e['id']}: branches, calls and ret are not patchable")
                if not (0 <= e["operand"] < len(insn.operands)):
                    raise GuestPatchError(f"guest patch {e['id']}: no operand {e['operand']}")
                op = insn.operands[e["operand"]]
                if op.type == "imm":
                    bits = 8 * _width(insn, e["operand"])
                    if bits < 32 and e["value"] >= (1 << bits):
                        raise GuestPatchError(
                            f"guest patch {e['id']}: 0x{e['value']:X} does not fit {bits} bits")
                elif op.type != "mem":
                    raise GuestPatchError(f"guest patch {e['id']}: operand {e['operand']} is a register")
                overrides.setdefault(e["address"], []).append((e["operand"], e["value"]))
        after = _decode(dis, bytes(data), func)
        if _shape(after) != _shape(before):
            raise GuestPatchError(
                f"guest patches in 0x{fn:08X} change its instruction boundaries, "
                f"mnemonics or branch targets")
    if table.get("relocations"):
        _apply_relocations(table["relocations"], bytes(data), func_db, dis, overrides, functions)
    return bytes(data), overrides, functions


def _apply_relocations(relocations, data, func_db, dis, overrides, functions):
    """Find every reference to each relocated table and redirect it."""
    config = _config()
    for r in relocations:
        lo, hi = r["old"], r["old"] + r["size"]
        for va in range(lo, hi, 4):
            off = config.va_to_file_offset(va)
            if off is not None and any(data[off:off + 4]):
                raise GuestPatchError(
                    f"guest patch {r['id']}: 0x{va:08X} is initialised; only "
                    f"zero-initialised tables can be relocated")
    found = {}
    for func in func_db.values():
        start, end = func.get("_addr"), func.get("end")
        if start is None or not end or not config.is_code_address(start):
            continue
        for insn in _decode(dis, data, func):
            if insn.is_branch or insn.is_call:
                continue
            for index, op in enumerate(insn.operands):
                if op.type == "imm":
                    value = op.imm
                elif op.type == "mem" and isinstance(op.mem_disp, int) and op.mem_disp > 0:
                    value = op.mem_disp & 0xFFFFFFFF
                else:
                    continue
                for r in relocations:
                    if r["old"] <= value <= r["old"] + r["size"]:
                        if insn.address in found:
                            raise GuestPatchError(
                                f"guest patch {r['id']}: 0x{insn.address:08X} has two "
                                f"relocated operands")
                        found[insn.address] = (r, index, value, func["_addr"])
    for r in relocations:
        mine = {a: v for a, v in found.items() if v[0] is r}
        missing = sorted(set(mine) - set(r["sites"]))
        extra = sorted(set(r["sites"]) - set(mine))
        if missing or extra:
            raise GuestPatchError(
                f"guest patch {r['id']}: references not listed: "
                + (", ".join(f"0x{a:08X}" for a in missing) or "none")
                + "; listed sites that are not references: "
                + (", ".join(f"0x{a:08X}" for a in extra) or "none"))
        for address, (_, index, value, fn) in sorted(mine.items()):
            how = r["sites"][address]
            if how == "keep":
                if value != r["old"] + r["size"]:
                    raise GuestPatchError(
                        f"guest patch {r['id']}: only a reference to the word after the table can be kept")
                continue
            if how == "end":
                if value != r["old"] + r["size"]:
                    raise GuestPatchError(
                        f"guest patch {r['id']}: 0x{address:08X} is not the table end")
                new_value = r["new"] + r["new_size"]
            else:
                if value >= r["old"] + r["size"]:
                    raise GuestPatchError(
                        f"guest patch {r['id']}: 0x{address:08X} is the table end, not an offset")
                new_value = r["new"] + (value - r["old"])
            if address in overrides:
                raise GuestPatchError(
                    f"guest patch {r['id']}: 0x{address:08X} already has an operand patch")
            overrides[address] = [(index, new_value)]
            functions.add(fn)
    for section in config._SECTIONS:
        if section.is_code:
            continue
        for va in range(section.va, section.va + min(section.raw_size, section.va_size) - 3, 4):
            off = section.raw_addr + (va - section.va)
            value = int.from_bytes(data[off:off + 4], "little")
            for r in relocations:
                if r["old"] <= value <= r["old"] + r["size"] and va not in r["data_ok"]:
                    raise GuestPatchError(
                        f"guest patch {r['id']}: data word at 0x{va:08X} holds 0x{value:08X}; "
                        f"list it in data_ok if it is not a pointer to the table")


def override_operands(insn, overrides):
    """Apply operand overrides to one decoded instruction (in place)."""
    for index, value in overrides.get(insn.address, ()):
        op = insn.operands[index]
        if op.type == "imm":
            op.imm = value
        else:
            op.mem_disp = value
            if not op.mem_base and not op.mem_index:
                insn.memory_refs = [value]
    insn.imm_values = [op.imm for op in insn.operands if op.type == "imm"]
