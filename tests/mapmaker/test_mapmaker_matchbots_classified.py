"""Decoder classification for more Arcade bots per MapMaker match.

1. Every displacement into the live block's per-actor arrays
   0x007DE414..0x007DE453 is moved by matchbots.json to the 64-entry mirror
   of the same array; none uses an absolute (index-free) form, and the
   pending (0x007DE354..), backup (0x0075F814..) and setup (0x0078AD6C..)
   arrays are only reached by the whole-block copies and the roster.
2. In every function that reads the bot count (0x007DE402 / 0x0078AD5A),
   the per-actor arrays or the rank table, each count-like constant (10, 14,
   15, 16, 17, 26) is handled or reviewed below (compares against game-mode
   bytes are classified automatically). A new unclassified site fails.
"""
import json
from pathlib import Path

from mapmaker_decoded import decoded, reviewed, unclassified

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / 'config/guest-code-patches/matchbots.json'
ARRAYS = {0x007DE414: 0x00904000, 0x007DE424: 0x00904040, 0x007DE434: 0x00904080, 0x007DE444: 0x009040C0}
MODE_BYTES = {0x007DE455, 0x007DE456, 0x0078ADAD, 0x0078ADAE, 0x0079094C, 0x0076B1A4, 0x0078AD2C}
VALUES = {0xA, 0xE, 0xF, 0x10, 0x11, 0x1A}

HANDLED = reviewed("""
0x002D37F7 roster difficulty (15 - skill) / 3; pc_mapmaker_matchbots mb_bot uses the same formula
""")

REVIEWED = reviewed("""
0x00068A3F sub_00068830 network team setup: bl = 16 (team id "none"), System Link stays stock
0x00068D3B sub_00068830 network team setup: bl = 16 (team id "none"), System Link stays stock
0x000D7593 sub_000D7570 team 16 (no team) written to the actor, not a count
0x002D2CBA sub_002D2C70 switch over the game mode (values 14/15)
0x002D2CCC sub_002D2C70 switch over the game mode (values 14/15)
0x002D3098 sub_002D2C70 clears a 17-dword stack array (team presence), not a bot count
0x002D315C sub_002D2C70 team-list loop over 17 team ids (the +4 team list), not the bots
0x002D3169 sub_002D2C70 a mode with 16 humans (System Link style block), no bot rows
0x002D3330 sub_002D2C70 sub-mode value 14 in bl
0x002D339A sub_002D2C70 sub-mode value 10 in bl
0x002D349A sub_002D2C70 clears a 16-dword stack array (team presence), not a bot count
0x002D3550 sub_002D2C70 team-list loop over 16 team ids, not the bots
0x002D3641 sub_002D2C70 game mode value 10 in eax
0x002EE11D sub_002EDDC0 field offset 0x10 of a menu record, not a count
0x002F64F1 sub_002F6320 award / message id 15, not a count
0x002F6540 sub_002F6320 award / message id 16, not a count
0x00357B42 sub_00357B00 System Link score message: 16 rank bytes (System Link keeps 16 characters)
0x00359C57 sub_00359AA0 System Link: 16 network slots
0x0035AEDD sub_0035AD90 System Link: 16 network slots
0x0035D369 sub_0035D150 System Link: 16 network slots
0x0035D38E sub_0035D150 System Link: 16 network slots
0x0035D43E sub_0035D150 System Link: 16 network slots
0x0035D490 sub_0035D150 System Link: 16 network slots
0x0035D4D7 sub_0035D150 System Link: 16 network slots
0x0035D5D9 sub_0035D150 System Link: 16 network slots
0x0035D5F7 sub_0035D150 System Link: 16 network slots
0x000D36CC sub_000D2FD0 game type value 10 in bl
0x000D541C sub_000D50E0 game type value 10 in al
0x0006898A sub_00068830 game type value 10 in al
0x0015E756 sub_0015E4F0 game type value 10 in bl
""")


def test_every_live_array_reference_is_moved():
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    moved = {int(e['address'], 16): (e['operand'], int(e['value'], 16)) for e in table['patches']
             if e['kind'] == 'operand' and e['id'].split('-')[1] in ('character', 'team', 'difficulty', 'variant')}
    found = {}
    for i in decoded():
        for n, op in enumerate(i.ops):
            if op[0] != 'mem':
                continue
            if 0x007DE414 <= op[4] < 0x007DE454:
                assert op[4] in ARRAYS, (hex(i.address), i.text)
                assert op[1] or op[2], (hex(i.address), 'absolute form', i.text)
                found[i.address] = (n, ARRAYS[op[4]])
            # the other copies' arrays: only whole-block copies (no displacement into them) ...
            assert not (0x007DE354 <= op[4] < 0x007DE394), (hex(i.address), i.text)
            assert not (0x0075F814 <= op[4] < 0x0075F854), (hex(i.address), i.text)
            # ... and the setup block's arrays only in the match start
            if 0x0078AD6C <= op[4] < 0x0078ADAC:
                assert i.function == 0x002D2C70, (hex(i.address), i.text)
    assert found == moved
    assert len(found) == 26


def test_every_count_constant_is_classified():
    insns = decoded()
    fs = set()
    for i in insns:
        for op in i.ops:
            if op[0] == 'mem' and (op[4] in (0x007DE402, 0x0078AD5A) or 0x007DE414 <= op[4] < 0x007DE454
                                   or 0x007DE4C0 <= op[4] < 0x007DE500):
                fs.add(i.function)
    found = {}
    for i in insns:
        if i.function not in fs or i.mnemonic not in ('cmp', 'mov', 'lea', 'sub') or i.frame():
            continue
        if ('reg', 'esp') in i.ops:                           # frame set-up
            continue
        if not any(k == 'imm' and v in VALUES for k, v in i.values()):
            continue
        if i.mnemonic == 'cmp' and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in MODE_BYTES for op in i.ops):
            continue
        if i.mnemonic == 'mov' and any(op[0] == 'mem' and not op[1] and not op[2] and op[4] in MODE_BYTES for op in i.ops):
            continue
        found[i.address] = i.text
    assert unclassified(found, HANDLED, REVIEWED) == {}
    stale = sorted(hex(a) for a in set(HANDLED) | set(REVIEWED) if a not in found)
    assert not stale, f'classified sites no longer found: {stale}'
    assert not set(HANDLED) & set(REVIEWED)


def test_rank_table_moves_whole():
    """matchbots-rank lists every reference; the team scores after it stay."""
    table = json.loads(TABLE.read_text(encoding='utf-8'))
    rank = next(e for e in table['patches'] if e['id'] == 'matchbots-rank')
    found = {}
    for i in decoded():
        if i.branch or i.call:
            continue
        for k, v in i.values():
            if 0x007DE4C0 <= v <= 0x007DE500:
                found[f'0x{i.address:08X}'] = 'keep' if v == 0x007DE500 else 'offset'
    assert found == rank['sites']
    keeps = sorted(a for a, how in found.items() if how == 'keep')
    assert keeps == ['0x0006416C', '0x00064190', '0x000641B5', '0x000644C0', '0x000648A4', '0x00064CAB']
