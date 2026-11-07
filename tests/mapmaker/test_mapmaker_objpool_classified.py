"""The engine object pool's slot allocator, classified over all game code.

v0.4.0-mapmaker.45 playtest (logs/m45c: BigMap, 395 pieces, about 106 Story
AI): about a second into play sub_0009FF10 faulted on an object whose room
count was 0x40 and whose room id was a pointer. The dump shows object slot 2
(pool [0x0072F5E8] + 2 x 0x2E8) overwritten from +0x30 by a model whose twin
index is 1026: the twin pool (0x200 bytes a slot) ends where the object pool
starts, and the slot allocator sub_0009EE00 hands out slot = count
[0x007B4FAC] with no bound once no freed slot exists. So more than 1023
objects were live. Fix (docs/agent-context/limitless-caps.md "Engine object
pool end"): pc_mapmaker_objects_create (WRAP sub_000A6880) refuses a create
past the pool, and the actors area keeps object headroom before a new AI.

These checks pin the decode facts that fix relies on (mapmaker_decoded.py).
"""
import re

from mapmaker_decoded import ROOT, decoded

OBJECTS = ROOT / 'src/interface/pc_mapmaker_objects.inc'
ACTORS = ROOT / 'src/interface/pc_mapmaker_actors.inc'
LIMITS = ROOT / 'src/interface/pc_mapmaker_limits.inc'
ALLOCATOR, CREATE, RESET = 0x0009EE00, 0x000A6880, 0x000A5AF0
COUNT, FREED, CAP, POOL = 0x007B4FAC, 0x007B65B8, 0x0072F5E4, 0x0072F5E8
TWIN_SIZER = 0x0008DBF0


def _function(insns, start):
    return [i for i in insns if i.function == start]


def _global(op, address):
    return op[0] == 'mem' and not op[1] and not op[2] and op[4] == address


def _writes(insns, address):
    return [i for i in insns if i.ops and _global(i.ops[0], address)
            and i.mnemonic in ('mov', 'inc', 'dec', 'add', 'sub')]


def test_only_the_allocator_raises_the_count():
    """The count is set by the allocator's two appends and the pool reset only."""
    insns = decoded()
    assert {i.function for i in _writes(insns, COUNT)} == {ALLOCATOR, RESET}
    assert sorted(i.address for i in _writes(insns, COUNT) if i.function == ALLOCATOR) == [0x0009EE28, 0x0009EEBC]


def test_allocator_has_one_caller_and_no_bound():
    insns = decoded()
    callers = {i.function for i in insns if i.call and ('imm', ALLOCATOR) in i.ops}
    assert callers == {CREATE}
    body = _function(insns, ALLOCATOR)
    # The pool size is read once, after the slot is taken (the +0x88 search),
    # and never compared with the count: nothing stops an append at 1023.
    caps = [i for i in body if any(_global(op, CAP) for op in i.ops)]
    assert [i.address for i in caps] == [0x0009EE7E] and caps[0].mnemonic == 'mov'
    assert not any(i.mnemonic == 'cmp' and any(_global(op, CAP) for op in i.ops) for i in body)
    # a freed slot is one with +0x2B bit 0x80 (sub_000A7090 sets +0x28 bit 31)
    assert any(i.mnemonic == 'test' and ('imm', 0x80) in i.ops and
               any(op[0] == 'mem' and op[4] == 0x2B for op in i.ops) for i in body)


def test_create_returns_zero_itself_and_its_wrapper_does_the_same():
    """sub_000A6880 frees the slot and returns 0 when the model fails (0x000A695C);
    sub_000A6EC0 and the character create sub_000DAC30 test that result."""
    insns = decoded()
    body = {i.address: i for i in _function(insns, CREATE)}
    assert body[0x000A695C].mnemonic == 'xor'
    a6ec0 = {i.address: i for i in _function(insns, 0x000A6EC0)}
    assert a6ec0[0x000A6EF2].mnemonic == 'test'
    dac30 = {i.address: i for i in _function(insns, 0x000DAC30)}
    assert dac30[0x000DAD78].mnemonic == 'test'
    text = OBJECTS.read_text(encoding='utf-8')
    hook = text[text.index('void pc_mapmaker_objects_create('):]
    hook = hook[:hook.index('\n}\n')]
    assert 'g_eax=0;' in hook and 'MAPMAKER_GUEST_RET();' in hook
    assert 'mapmaker_objects_pool_on()' in hook        # MapMaker scenes only


def test_twin_pool_is_sized_before_the_object_pool_with_the_same_slots():
    """Twin slot n lies at [0x0070AADC] + n x 0x200; past the twin pool it is the object pool."""
    insns = decoded()
    body = _function(insns, TWIN_SIZER)
    assert any(i.mnemonic == 'shl' and ('imm', 9) in i.ops for i in body)
    text = OBJECTS.read_text(encoding='utf-8')
    assert 'bytes=((MAPMAKER_OBJECTS_POOL<<9)+15u)&~15u;' in text
    assert 'WORD_AT(0x0070AAD8u)=MAPMAKER_OBJECTS_POOL;' in text


def test_ai_headroom_exceeds_the_effects_reserve():
    headroom = int(re.search(r'#define MAPMAKER_ACTORS_OBJECT_HEADROOM (\d+)u', ACTORS.read_text(encoding='utf-8')).group(1))
    effects = int(re.search(r'#define MAPMAKER_OBJECTS_EFFECTS (\d+)u', LIMITS.read_text(encoding='utf-8')).group(1))
    assert headroom > effects
    actors = ACTORS.read_text(encoding='utf-8')
    for address in ('0x0072F5E4u', '0x007B4FACu', '0x007B65B8u'):
        assert address in actors
