"""Physics bodies and the prop break, classified over all game code.

v0.4.0-mapmaker.39/.40 playtest (maps near the 1023-object cap): shooting an
exploding barrel no longer crashed, but nothing exploded and nobody was hurt.
Cause (docs/agent-context/limitless-caps.md "Physics bodies"): the prop break
sub_000AE4F0 asks sub_0016C020(1) for a kind-1 rigid body before the
explosion; when the pool is full it only fades the prop out. sub_0016C430 sizes
that pool from a word count P at [0x0074C49C] (scene 100: 100, or 50 outside
mode 0xA), and every other reader takes P from that word, so
pc_mapmaker_objects_physics can run the same allocations with P = 1024 in
MapMaker play.

Decoded with the lifter's decoder over the original XBE (mapmaker_decoded.py).
"""
from mapmaker_decoded import ROOT, decoded

OBJECTS = ROOT / 'src/interface/pc_mapmaker_objects.inc'
COUNT = 0x0074C49C                    # word P
KIND1 = 0x0074C45E                    # word: next kind-1 index (0x58 .. P + 0x58)
SIZER, CHECK, BREAK, RESET = 0x0016C430, 0x0016C020, 0x000AE4F0, 0x00165760
ALLOC, ALLOC_ALIGNED = 0x0003DFA0, 0x0003E0B0
# The sizing function's allocations in order (pc_mapmaker_objects_physics repeats them).
SIZER_CALLS = [ALLOC_ALIGNED] * 6 + [ALLOC] * 3 + [RESET]


def _function(insns, start):
    return [i for i in insns if i.function == start]


def _global(op, address):
    return op[0] == 'mem' and not op[1] and not op[2] and op[4] == address


def _writes(insns, address):
    return [i for i in insns if i.ops and _global(i.ops[0], address)
            and i.mnemonic in ('mov', 'inc', 'dec', 'add', 'sub')]


def _calls(body):
    return [v for i in body if i.call for k, v in i.values() if k == 'imm']


def test_count_is_written_only_by_the_sizer():
    """Every other use of P reads the word, so a larger P sizes every loop and bound."""
    insns = decoded()
    assert [i.address for i in _writes(insns, COUNT)] == [0x0016C4A2]
    readers = {i.function for i in insns if any(_global(op, COUNT) for op in i.ops)}
    assert readers == {SIZER, RESET, CHECK, 0x0016C170, 0x00167E70}, sorted(map(hex, readers))


def test_sizer_allocations_match_the_wrapper():
    insns = decoded()
    body = _function(insns, SIZER)
    assert _calls(body) == SIZER_CALLS
    sizes = {v for i in body for k, v in i.values() if k == 'imm'}
    assert {0x2E80, 0xBA, 0x6D60, 0x60, 0x40, 0x28, 0x32, 0x64} <= sizes
    text = OBJECTS.read_text(encoding='utf-8')
    for size in ('0x2E80u', '0xBAu', '0x6D60u', '(n+0x60u)<<9', '(n+0x60u)<<5', '(n+0x60u)*2u'):
        assert size in text, size
    assert text.count('mapmaker_objects_take(') >= 6 and text.count(',0x40u);') >= 6   # 64-aligned bodies via mapmaker_objects_take (nav block or Level heap, .61)
    callers = [i.address for i in insns if i.call and ('imm', SIZER) in i.ops]
    assert callers == [0x001CDC0C]                        # level init, after the object pools


def test_kind1_is_full_at_p_plus_0x58_and_never_steals():
    insns = decoded()
    body = _function(insns, CHECK)
    assert any(i.mnemonic == 'add' and ('imm', 0x58) in i.ops for i in body)
    assert any(_global(op, KIND1) for i in body for op in i.ops)
    # kind-1 index moves only in the allocator, the free and the reset
    # reset to 0x58, allocate (inc), free (dec): nothing else hands out kind-1 bodies
    assert {i.address for i in _writes(insns, KIND1)} == {0x00165771, 0x0016C21F, 0x0016C401}


def test_prop_break_asks_for_a_kind1_body_before_the_explosion():
    insns = decoded()
    body = _function(insns, BREAK)
    calls = [i for i in body if i.call]
    check = next(n for n, i in enumerate(calls) if ('imm', CHECK) in i.ops)
    explosion = next(n for n, i in enumerate(calls) if ('imm', 0x000EFBD0) in i.ops)
    assert check < explosion
    assert calls[check].address == 0x000AE55B
    # the refusal path stores state 9 and schedules sub_000AC560 (fade, then destroy)
    assert any(i.mnemonic == 'push' and ('imm', 0x000AC560) in i.ops for i in body)
    assert any(i.mnemonic == 'mov' and ('imm', 9) in i.ops for i in body)
    breakers = sorted(i.address for i in insns if i.call and ('imm', BREAK) in i.ops)
    assert breakers == [0x000AE7F2, 0x000AE8A4, 0x001EE132]


def test_detects_a_second_count_writer():
    """Mutation: a store to P outside the sizer is caught."""
    insns = decoded()
    k = next(n for n, i in enumerate(insns) if i.function == 0x00167E70 and any(_global(op, COUNT) for op in i.ops))
    i = insns[k]
    mutated = list(insns)
    mutated[k] = type(i)(i.address, i.function, 'mov', i.text, i.hex, False, False, (i.ops[1], i.ops[0]))
    assert len(_writes(mutated, COUNT)) == 2


def test_detects_a_wrapper_allocation_drift():
    """Mutation: a wrapper without one aligned allocation no longer matches the sizer."""
    text = OBJECTS.read_text(encoding='utf-8').replace(
        '    WORD_AT(0x0074C48Cu)=guest_call(sub_0003E0B0,3,0x40u,(n+0x60u)<<5,1u);\n', '')
    assert text.count('guest_call(sub_0003E0B0,3,0x40u,') != SIZER_CALLS.count(ALLOC_ALIGNED)
