"""MapMaker Arcade matches with up to 20 bots / 24 characters.

Compiles src/interface/pc_mapmaker_matchbots.inc against fake guest memory
(with mutations that must be caught) and, when the private XBE is available,
checks the stock facts the hooks rely on: the match blocks and their whole
copies, the roster loop and its row fields, the Bots screen's update/store
and builder, the hooked functions' returns, and the capacity facts that set
the 24-character limit (stats table 26, awards frame 26 rows).
"""
import json
import os
from pathlib import Path
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'src/interface/pc_mapmaker_matchbots.inc'

MUTATIONS = {
    'live-loses-stock': ('MB_B(MB_MIRROR+a*64u+k)=k<16u?MB_B(MB_LIVE+MB_ARRAYS+a*16u+k):',
                         'MB_B(MB_MIRROR+a*64u+k)=k<15u?MB_B(MB_LIVE+MB_ARRAYS+a*16u+k):'),
    'live-any-match': ('&& humans==mb.humans && bots==mb.bots &&', '&&'),
    'live-network': ('    wide=mb.extra_valid && !WORD_AT(MB_NET) &&', '    wide=mb.extra_valid &&'),
    'live-any-map': ('MB_B(MB_LIVE+0x54u)==0x64u && humans', '1 && humans'),
    'live-no-clamp': ('        MB_B(MB_LIVE+2u)=(uint8_t)(16u-humans);\n', ''),
    'restart-skips': ('    for(a=0;a<4u;a++) for(k=0;k<16u;k++) MB_B(MB_LIVE+MB_ARRAYS+a*16u+k)=MB_B(MB_MIRROR+a*64u+k);\n', ''),
    'start-no-unswap': ('    mb_unswap();\n    original();\n    mb_extras();', '    original();\n    mb_extras();'),
    'start-before-roster': ('    mb_unswap();\n    original();\n    mb_extras();', '    mb_unswap();\n    mb_extras();\n    original();'),
    'team-divisor': ("team=(uint8_t)(team_field%(MB_B(MB_SETUP+0x56u)==4u?4:2)+1);", "team=(uint8_t)(team_field%2+1);"),
    'team-always': ('    if(WORD_AT(MB_SETUP+0x58u)&1u) team=', '    if(1) team='),
    'skill-formula': ('diff=(uint8_t)((15-(int32_t)WORD_AT(', 'diff=(uint8_t)((16-(int32_t)WORD_AT('),
    'skill-type5': ('&& MB_B(MB_SETUP+0x55u)!=5u)\n        diff=', ')\n        diff='),
    'override-ignored': ('            if(over!=-1) on=over;                             /* as [esi] / [esi-4] at 0x002D36F0 */\n', ''),
    'off-rows-used': ('            if(id<0 || !on) continue;', '            if(id<0) continue;'),
    # the 24-character cap equals 4 players + the 20-bot cap, so it cannot fire alone (kept as a guard)
    'bot-cap': ('        if(n>=MB_ACTORS || bots>=MB_BOTS)', '        if(n>=MB_ACTORS)'),
    'count-not-raised': ('    MB_B(MB_SETUP+2u)=(uint8_t)bots;\n', ''),
    'block-past-16': ('        if(n<16u) MB_B(MB_SETUP+MB_ARRAYS+a*16u+n)=v[a];', '        if(n<17u) MB_B(MB_SETUP+MB_ARRAYS+a*16u+n)=v[a];'),
    'extras-stale': ('    memset(mb.extra,0,sizeof mb.extra);\n', ''),
    'extras-network': ('    if(WORD_AT(MB_NET) || mode==0xBu', '    if(mode==0xBu'),
    'extras-net-mode': ('    if(WORD_AT(MB_NET) || mode==0xBu || mode==0xCu || mode==0xDu) return;', '    if(WORD_AT(MB_NET)) return;'),
    'extras-story': ('    if(MB_B(MB_SETUP+0x55u)==10u || MB_B(MB_SETUP+0x56u)==0x13u) return;', '    if(MB_B(MB_SETUP+0x56u)==0x13u) return;'),
    'extras-monkeys': ('    if(MB_B(MB_SETUP+0x55u)==10u || MB_B(MB_SETUP+0x56u)==0x13u) return;', '    if(MB_B(MB_SETUP+0x55u)==10u) return;'),
    'extras-preset': ('        if((int32_t)WORD_AT(SETS_INDEX)<0xC || !(e=sets_editor())) return;', '        if(!(e=sets_editor())) return;'),
    'extras-any-map': ("        if(!(mode==9u || (mode==5u && MB_B(MB_SETUP+0x54u)==0x64u)) || !mb.rows_valid) return;",
                       "        if(!mb.rows_valid) return;"),
    'extras-switch': ('    if(!mb_on()) return;\n    humans=', '    humans='),
    'extras-any-scene': ('    } else return;\n    if(!mb_on()) return;', '    }\n    if(!mb_on()) return;'),
    'editor-team-row': ('team=(int32_t)WORD_AT(row+4u);diff=(int32_t)WORD_AT(row+8u);', 'team=0;diff=(int32_t)WORD_AT(row+8u);'),
    'flip-no-store': ('            guest_call(sub_002BFFD0,0);                       /* keep rows 1-10\'s last change */\n', ''),
    'flip-no-portraits': ('            mb_portraits();\n', ''),
    'flip-keeps-input': ('            WORD_AT(pane+0xC4u)&=~0x000F000Fu;WORD_AT(pane+0xC8u)&=~0xFu;\n', ''),
    'flip-no-cursor': ('            sets_select(pane,mb.page?0u:MB_ROWS-1u);\n', ''),
    'flip-during-reload': (' && mb.rows_valid && WORD_AT(MB_LOADED)==WORD_AT(MB_INDEX)) {', ' && mb.rows_valid) {'),
    'no-title': ('    if(MAPMAKER_PTR_OK(menu,0x208u)) { keep=sets.page;sets.page=mb.page;sets_title(menu);sets.page=keep; }\n', ''),
    'store-reaches-profile': ('    if(mb.page && WORD_AT(MB_SCENE)!=102u) {', '    if(0) {'),
    'store-ignores-index': ('        if(WORD_AT(MB_INDEX)==mb.page_index) {', '        if(1) {'),
    'open-no-unswap': ('    mb_unswap();\n    mb.opened=1;', '    mb.opened=1;'),
    'init-no-map': ("    if(index==0x11u && WORD_AT(MB_MODE)==9u && (data=", "    if(0 && (data="),
    'init-locked-variant': ('            if(!guest_call(sub_002BE270,1,(uint32_t)id)) variant=-1;     /* as the fill sub_002C0310 */\n', ''),
    'reopen-keeps-rows': ('index!=mb.rows_index || (opened && index==0x11u && WORD_AT(MB_MODE)==9u))', 'index!=mb.rows_index)'),
    'screen-editor': ('    if((scene!=100u && scene!=101u) || WORD_AT(MB_NET)) return 0;', '    if(WORD_AT(MB_NET)) return 0;'),
    'screen-network': ('    if((scene!=100u && scene!=101u) || WORD_AT(MB_NET)) return 0;', '    if(scene!=100u && scene!=101u) return 0;'),
    'screen-switch': ('    return mb_on();\n}', '    return 1;\n}'),
    'screen-any-map': ('    if(!(mode==9u || (mode==5u && MB_B(MB_SETUP+0x54u)==0x64u))) return 0;\n', ''),
    'screen-skips-original': ('    original();                                               /* pops the return address */\n',
                              '    g_esp+=4u;\n'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = INC.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / INC.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('matchbots.exe' if os.name == 'nt' else 'matchbots')
    source = ROOT / 'tests/mapmaker/mapmaker_matchbots.c'
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', '/wd4706', '/wd4505',
                '/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), '-I' + str(ROOT / 'src/interface'), '-I' + str(ROOT / 'src'),
                str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_matchbots(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if k not in ('TSFP_MAPMAKER_POOLS', 'TSFP_MAPMAKER_LIMITS')}
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=60)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker matchbots ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_table_shape():
    """The table: 26 mirror redirects, 3 pair-table sizes, the rank copy end, one relocation."""
    table = json.loads((ROOT / 'config/guest-code-patches/matchbots.json').read_text(encoding='utf-8'))
    kinds = {}
    for e in table['patches']:
        kinds.setdefault(e['id'].split('-')[1], []).append(e)
    assert sorted(kinds) == ['character', 'difficulty', 'pairs', 'rank', 'team', 'variant']
    targets = {'character': 0x00904000, 'team': 0x00904040, 'difficulty': 0x00904080, 'variant': 0x009040C0}
    assert sum(len(kinds[k]) for k in targets) == 26
    for k, new in targets.items():
        assert all(int(e['value'], 16) == new for e in kinds[k])
    assert {e['address'] for e in kinds['pairs']} == {'0x0015E0F5', '0x0015E76C', '0x0015E7DF'}
    assert all(e['value'] == '0x14' for e in kinds['pairs'])
    rank = [e for e in kinds['rank'] if e['kind'] == 'relocate']
    assert len(rank) == 1 and rank[0]['new'] == '0x00904100' and rank[0]['new_size'] == '0x100'
    assert rank[0]['sites']['0x00064CAB'] == 'keep'
    end = [e for e in kinds['rank'] if e['kind'] == 'operand']
    assert len(end) == 1 and end[0]['address'] == '0x00064CAB' and end[0]['value'] == '0x00904140'
    text = INC.read_text(encoding='utf-8')
    assert '#define MB_MIRROR 0x00904000u' in text and '#define MB_ACTORS 24u' in text


# ---- stock facts (skipped without the private XBE) --------------------------

def by_address():
    return {i.address: i for i in decoded()}


def has(i, value):
    return any(v == value for _, v in i.values())


def test_match_blocks_are_copied_whole():
    a = by_address()
    # setup -> pending (sub_000658D0: esi = argument), pending -> live (sub_000670C0, sub_00067280 x2),
    # live -> setup (sub_002EF9F0), setup <-> backup 0x0075F800 (sub_002CA880 / sub_002CA8B0): 0x28 dwords
    assert has(a[0x000658DD], 0x007DE340) and has(a[0x000658D6], 0x28) and a[0x000658E2].mnemonic.startswith('rep')
    for mov_esi, mov_edi in ((0x0006715D, 0x00067162), (0x000672CC, 0x000672D1), (0x000672E9, 0x000672EE)):
        assert has(a[mov_esi], 0x007DE340) and has(a[mov_edi], 0x007DE400)
    assert has(a[0x002EFA01], 0x007DE400) and has(a[0x002EFA06], 0x0078AD58) and has(a[0x002EF9FC], 0x28)
    assert a[0x0006715D].function == 0x000670C0
    assert a[0x002EFA01].function == 0x002EF9F0
    # every other immediate use of the live/pending block addresses is one of these copies or a network copy
    users = {i.function for i in decoded() if any(k == 'imm' and v in (0x007DE400, 0x007DE340) for k, v in i.values())}
    assert users == {0x000658D0, 0x000670C0, 0x00067280, 0x002EF9F0, 0x003550A0, 0x003563C0}


def test_roster_loop():
    a = by_address()
    # 10 rows from 0x0075FDA0 (row +0x10), stride 0x1C, end 0x0075FEB8; ebx starts at the local players
    assert has(a[0x002D36EA], 0x0075FDA0) and has(a[0x002D381C], 0x0075FEB8) and has(a[0x002D3818], 0x1C)
    assert has(a[0x002D36E2], 0x005236C4) and a[0x002D3817].mnemonic == 'inc'
    # per-actor bytes of the setup block, index ebx
    for site, disp in ((0x002D37C3, 0x0078AD6C), (0x002D37B7, 0x0078AD7C), (0x002D3811, 0x0078AD8C), (0x002D37C9, 0x0078AD9C)):
        assert has(a[site], disp), hex(site)
    # team = row +4 % (sub-mode 4 ? 4 : 2) + 1 in a team game (flags +0x58 bit 0)
    assert has(a[0x002D3794], 0x0078ADB0) and has(a[0x002D37A8], 4) and a[0x002D37B3].mnemonic == 'idiv'
    # difficulty: (15 - skill [id*0x30 + 0x0053A3DC]) / 3 if profile +0x10 and type != 5, else row +8
    assert has(a[0x002D37D4], 0x10) and has(a[0x002D37DB], 5) and has(a[0x002D37F1], 0x0053A3DC) and has(a[0x002D37F7], 0xF)
    assert has(a[0x002D380E], 0xFFFFFFF8)
    # bot count = ebx - players
    assert has(a[0x002D3828], 0x005236C4) and has(a[0x002D382F], 0x0078AD5A)
    # network modes 0xB/0xC/0xD skip the roster
    assert {v for x in (0x002D36C7, 0x002D36D0, 0x002D36D9) for k, v in a[x].values() if k == 'imm'} == {0xB, 0xC, 0xD}


def test_screen_and_store():
    a = by_address()
    # the update tail-jumps to the store; the store writes the profile rows (scene != 102)
    assert a[0x002C05C8].branch and '0x2bffd0' in a[0x002C05C8].text
    assert has(a[0x002C04FB], 0x0078AEAC) and has(a[0x002C0501], 0x0078A068) and has(a[0x002C0562], 0x0078AEAC)
    assert has(a[0x002BFFD5], 0x66) and has(a[0x002BFFF1], 0x0075FDA8)
    # the builder resets the loaded index
    assert has(a[0x002C2C92], 0x0078AEAC)
    # the reload's portrait per row: sub_002BE600(row +0, row +0x18) -> row +0x14
    assert '0x2be600' in a[0x002C0547].text and has(a[0x002C054C], 0x14)
    # the fill checks the character is available before using its variant (sub_002BE270)
    assert '0x2be270' in a[0x002C033C].text


def test_capacity_facts():
    a = by_address()
    # arcade stats table: 26 records (0x1A) outside story mode
    assert has(a[0x00238DE4], 0x1A) and has(a[0x00238DE7], 0x9CC) and has(a[0x00238DF6], 0x0075CCC0)
    # the awards function's frame holds 26 rows of 0xEC
    assert has(a[0x0023A040], 0x19A4) and has(a[0x0023A834], 0xEC)
    # pair table outside story mode: humans + 10
    for site in (0x0015E0F5, 0x0015E76C, 0x0015E7DF):
        assert a[site].mnemonic == 'add' and has(a[site], 0xA)
    # the rank table's clear/copy loop and the team scores after it
    assert has(a[0x00064C96], 0x007DE4C0) and has(a[0x00064CAB], 0x007DE500) and has(a[0x0006416C], 0x007DE500)
    # MapMaker row of the map table allows 14/14/16/16 actors
    assert has(a[0x002C0441], 0x004D239C)


def test_hooked_functions_return_plainly():
    a = by_address()
    ends = {0x002C2C20: 0x002C2C9D, 0x002BFFD0: 0x002BFFEB, 0x002D2C70: 0x002D383B, 0x002EF9F0: 0x002EFA3D}
    for fn, ret in ends.items():
        assert a[ret].mnemonic == 'ret' and a[ret].function == fn and not a[ret].ops, hex(fn)
    # sub_000670C0 ends in a tail jump to sub_00042510 (plain ret)
    assert a[0x00067262].branch and '0x42510' in a[0x00067262].text
    assert a[0x00042586].mnemonic == 'ret' and not a[0x00042586].ops
