"""Rooms past 255, stage 6: word draw buckets.

C fixture (tests/mapmaker/mapmaker_rooms_buckets.c) with mutations: the hooks of
sub_00086C40, sub_00086480/sub_000865E0 and sub_000897D0 against models of the
stock byte-table code; plus rooms.json's reader patches decoded with the lifter's
decoder and tied to the C constants.
"""
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
ROOMS = ROOT / 'src/interface/pc_mapmaker_rooms.inc'
TABLE = ROOT / 'config/guest-code-patches/rooms.json'
FIXTURE = ROOT / 'tests/mapmaker/mapmaker_rooms_buckets.c'

MUTATIONS = {
    'byte-room-ids': ('c->room[c->start[d]+normal[d]++]=(uint16_t)r;', 'c->room[c->start[d]+normal[d]++]=(uint8_t)r;'),
    'segment-256': ('#define MAPMAKER_ROOMS_BUCKET_SEGMENT 255u', '#define MAPMAKER_ROOMS_BUCKET_SEGMENT 256u'),
    'segment-no-return-address': ('        if(done) { g_esp-=4u;WORD_AT(g_esp)=ret; }  /* the same call again */\n', ''),
    'segment-first-only': ('    } while(done<n);', '    } while(0);'),
    'sorted-11': ('#define MAPMAKER_ROOMS_SORTED 10u', '#define MAPMAKER_ROOMS_SORTED 11u'),
    'sorted-not-appended': ('    if(any_sorted)                                   /*', '    if(0)                                   /*'),
    'sorted-from-deepest-only': ('for(k=0;k<=deepest && k<', 'for(k=deepest;k<=deepest && k<'),
    'replaced-keeps-normal': ('        if(d<0 || replaced[d]) continue;', '        if(d<0) continue;'),
    'deepest-not-restored': ('    WORD_AT(WORD_AT(MAPMAKER_ROOMS_VIEW_CURRENT)+0xD28u)=(uint32_t)deepest;\n', ''),
    'deepest-not-capped': ('    if((int32_t)WORD_AT(view+0xD28u)>=(int32_t)MAPMAKER_ROOMS_DEPTHS) WORD_AT(view+0xD28u)=MAPMAKER_ROOMS_DEPTHS-1u;\n    if(!WORD_AT(view+0x214u))',
                           '    if(!WORD_AT(view+0x214u))'),
    'draw-arg-extended': ('guest_call(sub_00086AE0,1,(uint32_t)deepest);', 'guest_call(sub_00086AE0,1,WORD_AT(view+0xD28u));'),
    'global-stock-id': ('guest_call(sub_000A79B0,1,MAPMAKER_ROOMS_ID_GLOBAL);', 'guest_call(sub_000A79B0,1,0xD5u);'),
    'viewport-order': ('WORD_AT(vp),WORD_AT(vp+0x14u),WORD_AT(vp+4u),WORD_AT(vp+0x18u)', 'WORD_AT(vp),WORD_AT(vp+4u),WORD_AT(vp+0x14u),WORD_AT(vp+0x18u)'),
    'scissor-flag-kept': ('    if(!WORD_AT(view+0x214u)) WORD_AT(0x006B7ACCu)=0;\n', ''),
    'sorted-flag-ignored': ('+r*0xB4u+0xB2u)&8u) {', '+r*0xB4u+0xB2u)&0u) {'),
    'reader-outside-stale': ('        if(b<MAPMAKER_ROOMS_DEPTHS) *(uint8_t *)mapmaker_rooms_host(MAPMAKER_ROOMS_BUCKET_COUNTS+b)=0;\n', ''),
    'list-ascending-depth': ('        for(b=deepest;b>=0;b--)\n', '        for(b=0;b<=deepest;b++)\n'),
    'list-x-limit': ('if((int32_t)x0>=right || x1<=x0) continue;', 'if((int32_t)x0>right || x1<=x0) continue;'),
    'list-empty-rect': ('if((int32_t)x0>=right || x1<=x0) continue;', 'if((int32_t)x0>=right || x1<x0) continue;'),
    'list-no-cap': ('if(found || count>=(int16_t)MAPMAKER_ROOMS_VIEW_LIST_CAP || count<0) continue;', 'if(found || count<0) continue;'),
    'list-duplicates': ('if(found || count>=(int16_t)MAPMAKER_ROOMS_VIEW_LIST_CAP', 'if(count>=(int16_t)MAPMAKER_ROOMS_VIEW_LIST_CAP'),
    'list-unbounded': ('                if(at>=MAPMAKER_ROOMS_VIEW_LIST_WORDS) continue;\n', ''),
    'list-first-call': ('    if(!WORD_AT(0x0070877Cu)) { WORD_AT(0x0070877Cu)=1;MAPMAKER_GUEST_RET();return; }\n', ''),
    'list-flag-ignored': ('if(!(*(uint8_t *)mapmaker_rooms_host(view+0x21Cu)&0x20u)) continue;', ';'),
    'list-no-e0a': ('        mapmaker_rooms_set_u16(view+0xE0Au,mapmaker_rooms_u16(view+0xE08u));\n', ''),
    'list-global-neighbours': ('        if(room==MAPMAKER_ROOMS_ID_GLOBAL) continue;\n', ''),
    'list-byte-ids': ('mapmaker_rooms_set_u16(list+2u*at,(uint16_t)r);', 'mapmaker_rooms_set_u16(list+2u*at,(uint8_t)r);'),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    text = ROOMS.read_text(encoding='utf-8')
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        assert text.count(old) == 1, mutation
        text = text.replace(old, new)
    (tmp_path / ROOMS.name).write_text(text, encoding='utf-8')
    exe = tmp_path / ('buckets.exe' if os.name == 'nt' else 'buckets')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996',
                '/I' + str(tmp_path), str(FIXTURE), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                '-I' + str(tmp_path), str(FIXTURE), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe, result.stdout + result.stderr


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_rooms_buckets(tmp_path, mutation):
    exe, _ = build(tmp_path, mutation)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker rooms buckets ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_hooks_compile_without_warnings(tmp_path):
    _, output = build(tmp_path, 'fixed')
    lines = [l for l in output.splitlines() if 'pc_mapmaker_rooms.inc' in l and 'warning' in l]
    assert not lines, lines


def define(name):
    for line in ROOMS.read_text(encoding='utf-8').splitlines():
        if line.startswith(f'#define {name} '):
            return int(line.split()[2].rstrip('u'), 0)
    raise AssertionError(name)


def table():
    return json.loads(TABLE.read_text(encoding='utf-8'))['patches']


def decode(hex_bytes, va):
    sys.path.insert(0, str(ROOT / 'tools/compiler'))
    from translation_decode import Disassembler
    raw = bytes.fromhex(hex_bytes)
    insns = Disassembler().disassemble_function(raw, va, va + len(raw))
    assert len(insns) == 1 and insns[0].size == len(raw)
    return insns[0]


READERS = {'0x00086480': ('0x000864CB', '0x000864D1', '0x000864F4'),
           '0x000865E0': ('0x00086621', '0x00086627', '0x00086644')}


def bucket_problems(patches):
    """Stage 6 table contract; [] when right."""
    out = []

    def need(cond, why):
        if not cond:
            out.append(why)
    ops = {p['address']: p for p in patches if p['kind'] != 'relocate'}
    relocs = {p['id']: p for p in patches if p['kind'] == 'relocate'}
    rows = relocs.get('rooms-reloc-t5-0x7042b8')
    need(rows is not None, 'bucket row relocation')
    if rows is None:
        return out
    staging = int(rows['new'], 16) + (0x007042B8 - int(rows['old'], 16))
    need(staging == define('MAPMAKER_ROOMS_BUCKET_ROW'), 'staging row = the readers\' row base')
    need(int(rows['new_size'], 16) >= 2 * define('MAPMAKER_ROOMS_BUCKET_SEGMENT'), 'staging row holds a segment of words')
    for fn, (stride, base, load) in READERS.items():
        s = ops.get(stride)
        need(s is not None and s['kind'] == 'operand' and int(s['value'], 16) == 0, f'{fn} row stride 0')
        need(rows['sites'].get(base) == 'offset', f'{fn} row base relocated')
        w = ops.get(load)
        if w is None or w['kind'] != 'code' or w['function'] != fn:
            out.append(f'{fn} word load')
            continue
        old, new = decode(w['original_hex'], int(load, 16)), decode(w['new_hex'], int(load, 16))
        need(old.op_str == 'esi, byte ptr [edx + eax]', f'{fn} stock load')
        need(new.mnemonic == 'movzx' and new.op_str == 'esi, word ptr [edx + eax*2]', f'{fn} word load decode')
    # the C constants are the table's
    v = lambda a: int(ops[a]['value'], 16)
    need(v('0x00086DDF') == v('0x0008996B') == define('MAPMAKER_ROOMS_ID_GLOBAL'), 'global id')
    need(v('0x000899CA') == define('MAPMAKER_ROOMS_VIEW_LIST_CAP') < define('MAPMAKER_ROOMS_VIEW_LIST_WORDS'), 'list cap')
    need(v('0x000899A8') == v('0x00089925') == define('MAPMAKER_ROOMS_VIEW_LIST'), 'list offset')
    need(v('0x00089873') == v('0x00086C97') == define('MAPMAKER_ROOMS_VIEW_RECTS') + 12, 'rect offset')
    need(define('MAPMAKER_ROOMS_VIEW_LIST') + 2 * define('MAPMAKER_ROOMS_VIEW_LIST_WORDS') <= 0x6000 + 0xFFC, 'list in shadow')
    return out


def test_bucket_table():
    assert not bucket_problems(table())


def _mutate(address, **fields):
    return [dict(p, **fields) if p.get('address') == address or p['id'] == address else p for p in table()]


TABLE_MUTATIONS = {
    'stride-256': lambda: _mutate('0x000864CB', value='0x100'),
    'stride-other-reader': lambda: _mutate('0x00086621', value='0x200'),
    'byte-load': lambda: _mutate('0x00086644', new_hex='0fb63c02'),
    'load-scale-1': lambda: _mutate('0x000864F4', new_hex='0fb73402'),
    'staging-moved': lambda: _mutate('rooms-reloc-t5-0x7042b8', new='0x008C2000'),
    'list-cap-raised': lambda: _mutate('0x000899CA', value='0x10D'),
}


@pytest.mark.parametrize('mutation', sorted(TABLE_MUTATIONS))
def test_bucket_table_mutations_fail(mutation):
    assert bucket_problems(TABLE_MUTATIONS[mutation]()), mutation
