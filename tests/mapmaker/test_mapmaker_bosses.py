"""MapMaker Story AI bosses and the creature probe (src/interface/pc_mapmaker_bosses.inc).

Compiles the policy against fake guest memory with mutation cases (the Boss
row, cycling, the per-entry table through delete/reset/compile/load, chunk
byte +0x29, health multiplier, nearest-boss bar through sub_00139740, the
read-only probe), and, with the private XBE, checks the guest sites it relies
on in the lifter's decode: chunk byte +0x29 is never touched by the stock
writer, loader or play readers; the story boss bar sub_0014CC80 and its draw
helper sub_00139740 work as the hook assumes.
"""
import os
from pathlib import Path
import struct
import subprocess
import pytest

from native_compiler import native_compiler
from mapmaker_decoded import decoded

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/interface/pc_mapmaker_bosses.inc'

MUTATIONS = {
    'always-on': ('static int bos_on(void) { mapmaker_limits_read(); return mapmaker_limits.limits; }',
                  'static int bos_on(void) { mapmaker_limits_read(); return 1; }'),
    'row-any-return': ('if(r!=BOS_ROWS_AT || cb!=BOS_CLOSE', 'if(cb!=BOS_CLOSE'),
    'row-type5': ('    if(s>=bos_count(e) || WORD_AT(bos_entry(e,s)+BOS_TYPE)==5u) return;',
                  '    if(s>=bos_count(e)) return;'),
    'click-closes': ('    MAPMAKER_GUEST_RET();                         /* sub_0031CAC0: ret; the screen stays open */',
                     '    original();'),
    'click-no-ret': ('    MAPMAKER_GUEST_RET();                         /* sub_0031CAC0: ret; the screen stays open */', ''),
    'cycle-skips-x8': ('level=(level+1u)%(BOS_LEVELS+1u);', 'level=(level+1u)%BOS_LEVELS;'),
    'label-not-in-place': ('    uint32_t text=bos_text(0);\n    if(text) bos_put_text',
                           '    uint32_t text=xbox_HeapAlloc(BOS_NAME,4u);\n    if(text) bos_put_text'),
    'no-colour-check': ('    if(bos.colour[i]!=bos_colour_of(e,i)) {', '    if(0) {'),
    'delete-no-shift': ('    memmove(bos.level+i,bos.level+i+1u,before-i-1u);', ''),
    'delete-keeps-tail': ('    bos.level[before-1u]=0;', ''),
    'reset-keeps': ('    original();\n    memset(bos.level,0,sizeof bos.level);', '    original();'),
    'wrong-byte': ('#define BOS_CHUNK_FLAG 0x29u', '#define BOS_CHUNK_FLAG 0x28u'),
    'wrong-mark': ('#define BOS_FLAG_MARK 0xB0u', '#define BOS_FLAG_MARK 0x80u'),
    'accept-any-flag': ('    if((flag&BOS_FLAG_MASK)!=BOS_FLAG_MARK) {', '    if(0) {'),
    'load-no-index-check': ('if(level && !same) {', 'if(0) {'),
    'compile-off-too': ('    if(!bos_on() || !(e=bos_editor())) return;\n    bos_walk(container,bos_count(e),bos_compile_chunk,&e);',
                        '    if(!(e=bos_editor())) return;\n    bos_walk(container,bos_count(e),bos_compile_chunk,&e);'),
    'multiplier-linear': ('return level?1u<<(level-1u):1u;', 'return level?level:1u;'),
    'scale-every-frame': ('    if(bos_play.scaled[k].record==rec && bos_play.scaled[k].max_bits==bits) return;   /* already ours */', ''),
    'no-rescale': ('    if(bos_play.scaled[k].record==rec && bos_play.scaled[k].max_bits==bits) return;   /* already ours */',
                   '    if(bos_play.scaled[k].record==rec) return;'),
    'cur-not-scaled': ('    if(cur<=max*1.0001f) cur*=m;', ''),
    'range-2d': ('d=dx*dx+dy*dy+dz*dz;', 'd=dx*dx+dy*dy;'),
    'farthest': ('k>0 && dist[k-1u]>d;', 'k>0 && dist[k-1u]<d;'),
    'dead-shown': ('if(!bos_play.level[s] || bos_play.sub[s] || !(bos_getf(ent+BOS_ENT_HEALTH)>0.0f)) continue;',
                   'if(!bos_play.level[s] || bos_play.sub[s]) continue;'),
    'humans-included': ('for(i=humans<count?humans:count;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),p;',
                        'for(i=0;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),p;'),
    'proxy-ignored': ('        if(bos_bar_proxy && (p=bos_bar_proxy(ent))!=0 && MAPMAKER_PTR_OK(p,BOS_ENT_SIZE)) ent=p;', '        (void)p;'),
    'proxy-after-scale': ('        if(bos_bar_proxy && (p=bos_bar_proxy(ent))!=0 && MAPMAKER_PTR_OK(p,BOS_ENT_SIZE)) ent=p;   /* the bar\'s entity */\n        bos_scale(a,ent,rec,bos_factor(s));',
                          '        bos_scale(a,ent,rec,bos_factor(s));\n        if(bos_bar_proxy && (p=bos_bar_proxy(ent))!=0 && MAPMAKER_PTR_OK(p,BOS_ENT_SIZE)) ent=p;'),
    'meter-humans-included': ('for(i=humans<count?humans:count;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),inst;',
                              'for(i=0;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),inst;'),
    'no-entity-check': (' || WORD_AT(ent+BOS_ENT_AI)!=a || ', ' || '),
    'over-stock-bar': ('slot=(WORD_AT(hud+BOS_HUD_FLAGS)&1u)?1u:0u;', 'slot=0;'),
    'any-mode': (" || BOS_B(BOS_MODE)!=10u) return;", ') return;'),
    'any-hud': (' || hud!=BOS_HUD+p*BOS_HUD_STRIDE) return;', ') return;'),
    'no-restore': ('        drawn=WORD_AT(buf+0x40u);\n        restore_registers(&r);',
                   '        drawn=WORD_AT(buf+0x40u);\n        g_esp=r.sp;'),
    'no-flag': ('    if(any) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;', ''),
    'flag-always': ('    if(any) WORD_AT(hud+BOS_HUD_FLAGS)|=1u;', '    WORD_AT(hud+BOS_HUD_FLAGS)|=1u;'),
    'no-name': ('bos_play.name[index[k]][0]?bos_play.name[index[k]]:"Boss"', '"Boss"'),
    # several bars
    'one-bar': ('for(k=0;k<n && slot<cap;k++)', 'for(k=0;k<n && k<1u;k++)'),
    'no-stack': ('WORD_AT(BOS_MARGIN)=margin+(uint32_t)(step*(int)slot);', 'WORD_AT(BOS_MARGIN)=margin;'),
    'margin-not-restored': ('    WORD_AT(BOS_MARGIN)=margin;\n    bos_smooth_io', '    bos_smooth_io'),
    'step-unscaled': ('return (int)(BOS_BAR_STEP*scale+0.5f);', 'return (int)(BOS_BAR_STEP+0.5f);'),
    'scale-unclamped': ('    if(!(scale>=0.25f && scale<=4.0f)) scale=1.0f;', ''),
    'shared-smoothing': ('        bos_smooth_io(hud,bos_play.smooth[s].state,1);', ''),
    'smoothing-not-saved': ('        bos_smooth_io(hud,bos_play.smooth[s].state,0);', ''),
    'stock-smoothing-lost': ("    bos_smooth_io(hud,stock,1);", ''),
    # the drain (fade +0x172C) advances per boss copy
    'no-fade': ('        bos_smooth_fade(s,hud);\n', ''),
    'fade-per-draw': ('fade-=dt*bos_getf(BOS_FADE_RATE);', 'fade-=bos_getf(BOS_FADE_RATE);'),
    'fade-clock-not-kept': ('    bos_play.smooth[s].clock=now;bos_play.smooth[s].timed=1u;', '    bos_play.smooth[s].timed=1u;'),
    'fade-no-floor': ('        if(fade<0.0f) fade=0.0f;\n', ''),
    'fade-wrong-field': ('memcpy(&bos_play.smooth[s].state[2],&fade,4);', 'memcpy(&bos_play.smooth[s].state[1],&fade,4);'),
    'no-split-cap': ('cap=WORD_AT(BOS_HUMANS)>1u?BOS_BARS_SPLIT:BOS_BARS_MAX;', 'cap=BOS_BARS_MAX;'),
    'slot-on-undrawn': ('if(drawn) { any=1;slot++; }', 'any|=drawn!=0;slot++;'),
    'one-label': ('uint32_t text=bos_text(1u+k),buf,drawn;', 'uint32_t text=bos_text(1u),buf,drawn;'),
    # stronger health
    'health-no-rows': ('            guest_call(sub_00071800,2,text,BOS_HEALTH_TOUGH+1u+k);', ''),
    'health-values': ('guest_call(sub_00071800,2,text,BOS_HEALTH_TOUGH+1u+k);', 'guest_call(sub_00071800,2,text,BOS_HEALTH_TOUGH+k);'),
    'health-any-return': ('if(r==BOS_HEALTH_ROW_AT && bos_on()', 'if(bos_on()'),
    'health-any-field': (' && field==bos_entry(e,s)+BOS_HEALTH)', ')'),
    'health-any-scene': ('bos_on() && WORD_AT(BOS_SCENE)==102u && (e=bos_editor())!=0', 'bos_on() && (e=bos_editor())!=0'),
    'compile-stock-unsafe': ('            BOS_B(data+BOS_CHUNK_HEALTH)=(uint8_t)BOS_HEALTH_TOUGH;   /* the stock table has 5 entries */', ''),
    'compile-no-extra': ('    BOS_B(data+BOS_CHUNK_EXTRA)=(uint8_t)(extra?BOS_EXTRA_MARK|extra:0u);', '    (void)extra;'),
    'compile-garbage-extra': ('if(health<=BOS_HEALTH_TOUGH+BOS_HEALTH_EXTRA) extra=health-BOS_HEALTH_TOUGH;',
                              'if(1) extra=health-BOS_HEALTH_TOUGH;'),
    'wrong-extra-byte': ('#define BOS_CHUNK_EXTRA 0x21u', '#define BOS_CHUNK_EXTRA 0x31u'),
    'load-no-extra': ('if(same && WORD_AT(entry+BOS_HEALTH)==BOS_HEALTH_TOUGH) WORD_AT(entry+BOS_HEALTH)=BOS_HEALTH_TOUGH+extra;',
                      'if(same && WORD_AT(entry+BOS_HEALTH)==BOS_HEALTH_TOUGH) (void)extra;'),
    'load-extra-no-index-check': ('if(same && WORD_AT(entry+BOS_HEALTH)==BOS_HEALTH_TOUGH)', 'if(WORD_AT(entry+BOS_HEALTH)==BOS_HEALTH_TOUGH)'),
    'extra-any-health': (' || health!=BOS_HEALTH_TOUGH) {', ') {'),
    'extra-accept-any': ('if((flag&BOS_EXTRA_MASK)!=BOS_EXTRA_MARK ||', 'if(0 ||'),
    'extra-range': (' || (flag&7u)>BOS_HEALTH_EXTRA', ''),
    'extra-not-multiplied': ('return bos_multiplier(bos_play.level[i])<<bos_play.extra[i];', 'return bos_multiplier(bos_play.level[i]);'),
    'extra-additive': ('<<bos_play.extra[i];', '+bos_play.extra[i];'),
    'extra-gets-bar': ('if(!bos_play.level[s] || bos_play.sub[s] || !(', 'if(bos_play.sub[s] || !('),
    'extra-not-active': ('    if(!level && !extra) return;', '    if(!level) return;'),
    'range-ignored': ('        if(f>=1.0f && f<=100000.0f) r=f;', ''),
    'range-unclamped': ('        if(f>=1.0f && f<=100000.0f) r=f;', '        r=f;'),
    'level-keeps-active': ('    memset(&bos_play,0,sizeof bos_play);\n    if(!bos_on()) return;',
                           '    if(!bos_on()) return;\n    memset(&bos_play,0,sizeof bos_play);'),
    'probe-no-dedupe': ('        if(k<bos_probe.level_new) continue;', ''),
    'probe-no-level': ('    if(scene!=bos_probe.scene || pool!=bos_probe.pool) bos_probe_level(scene,pool);',
                       '    if(scene!=bos_probe.scene) bos_probe_level(scene,pool);'),
    'probe-writes': ('            model=bos_get16(ent+BOS_ENT_MODEL);', '            model=bos_get16(ent+BOS_ENT_MODEL);WORD_AT(ent+0x1F0u)^=1u;'),
    'probe-always': ("        bos_probe.on=v && v[0]=='1' && !v[1];", '        bos_probe.on=1;'),
    # health display: Boss bar / Sub-boss bar (+0x29 bit 2)
    'display-row-always': ('    if(text && level && (text=bos_display_label', '    if(text && (text=bos_display_label'),
    'display-row-missing': ('        guest_call(sub_00079D60,6,text,BOS_ROW_DISPLAY,0u,0u,BOS_CLOSE,0u);', '        (void)text;'),
    'display-click-closes': ('if(row!=BOS_ROW && row!=BOS_ROW_DISPLAY) {', 'if(row!=BOS_ROW) {'),
    # a duplicate / linked copy inherits the Boss level and Health display
    'copy-not-inherited': ('        if(level && bos_count(e)==before+1u', '        if(0 && bos_count(e)==before+1u'),
    'copy-no-sub': ('bos_set(e,before,level,sub);', 'bos_set(e,before,level,0);'),
    'copy-wrong-index': ('bos_set(e,before,level,sub);', 'bos_set(e,s+1u,level,sub);'),
    'copy-any-entry': (' && bos_is_copy(e,s,before)) bos_set', ') bos_set'),
    'copy-short-compare': ('#define BOS_COPY_FIRST 0x28u', '#define BOS_COPY_FIRST 0x2Cu'),
    'display-label-stale': ('    bos_label(level);\n    bos_display_label(level,bos_get_sub(e,s));\n', '    bos_label(level);\n'),
    'display-cycles-level': ('    if(row==BOS_ROW) level=', '    if(1) level='),
    'delete-no-sub-shift': ('    memmove(bos.sub+i,bos.sub+i+1u,before-i-1u);\n', ''),
    'sub-not-saved': ('(bos_get_sub(e,i)?BOS_FLAG_SUB:0u)', '0u'),
    'sub-wrong-bit': ('#define BOS_FLAG_SUB 0x04u', '#define BOS_FLAG_SUB 0x08u'),
    'old-mask': ('#define BOS_FLAG_MASK 0xF8u', '#define BOS_FLAG_MASK 0xFCu'),
    'loose-mask': ('#define BOS_FLAG_MASK 0xF8u', '#define BOS_FLAG_MASK 0xF0u'),
    'sub-not-loaded': ('bos_set(e,base+k,level,bos_decode_sub(x.flag[k]));', 'bos_set(e,base+k,level,0);'),
    'sub-not-in-play': ('    bos_play.sub[i]=(uint8_t)(level && sub);', '    bos_play.sub[i]=0;'),
    'sub-gets-bar': ('if(!bos_play.level[s] || bos_play.sub[s] || !(', 'if(!bos_play.level[s] || !('),
    'meter-no-original': ('    restore_registers(&r);\n    original();\n}', '    restore_registers(&r);\n    MAPMAKER_GUEST_RET();\n}'),
    'meter-after-original': ('    save_registers(&r);\n    for(i=humans<count?humans:count;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),inst;',
                             '    original();g_esp-=4u;save_registers(&r);\n    for(i=humans<count?humans:count;i<count;i++) {\n        uint32_t a=WORD_AT(BOS_LIST+i*4u),ent=0,rec=0,s=bos_actor(a,&ent,&rec),inst;'),
    'meter-with-record': (' || WORD_AT(ent+BOS_ENT_RECORD)) continue;', ') continue;'),
    'meter-dead': ('        if(!(bos_getf(ent+BOS_ENT_HEALTH)>0.0f) || !(bos_getf(ent+BOS_ENT_MAX)>0.0f)) continue;\n', ''),
    'meter-player': ('        if(ent==pe || !MAPMAKER_PTR_OK(ent,', '        if(!MAPMAKER_PTR_OK(ent,'),
    'meter-any-scene': ('WORD_AT(BOS_SCENE)!=100u || BOS_B(BOS_MODE)!=10u\n       || !MAPMAKER_STACK_OK(0x400)', '!MAPMAKER_STACK_OK(0x400)'),
    'meter-args': ('guest_call(sub_0013A400,4,a,0u,0x7Fu,0x6Eu);', 'guest_call(sub_0013A400,4,ent,0u,0x7Fu,0x6Eu);'),
    'meter-bar-bosses': (' || !bos_play.sub[s]) continue;', ') continue;'),
    'meter-no-model-check': ('        if(!MAPMAKER_PTR_OK(inst,0x90u) || !MAPMAKER_PTR_OK(WORD_AT(inst+0x8Cu),0x1Cu) || !MAPMAKER_PTR_OK(WORD_AT(inst+0x10u),0x40u)) continue;\n',
                             '        (void)inst;\n'),
    'meter-negative-player': (' || (int32_t)WORD_AT(player+4u)<0) { original();return; }', ') { original();return; }'),
    'meter-no-restore': ('        restore_registers(&r);\n        bos_play.meters++;\n    }\n    restore_registers(&r);', '        bos_play.meters++;\n    }'),
    'meter-no-count': ('    if(level && sub) bos_play.subs++;\n', ''),
}


def build(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    if mutation != 'fixed':
        old, new = MUTATIONS[mutation]
        text = SOURCE.read_text(encoding='utf-8')
        assert text.count(old) == 1, mutation
        (tmp_path / SOURCE.name).write_text(text.replace(old, new), encoding='utf-8')
    exe = tmp_path / ('bosses.exe' if os.name == 'nt' else 'bosses')
    source = ROOT / 'tests/mapmaker/mapmaker_bosses.c'
    include = ['/I' + str(tmp_path), '/I' + str(ROOT / 'src/interface'), '/I' + str(ROOT / 'src')]
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/wd4996', *include, str(source), '/Fe:' + str(exe)]
    else:
        args = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-D_POSIX_C_SOURCE=200809L',
                *['-I' + i[2:] for i in include], str(source), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize('mutation', ['fixed', *MUTATIONS])
def test_mapmaker_bosses(tmp_path, mutation):
    exe = build(tmp_path, mutation)
    env = {k: v for k, v in os.environ.items() if not k.startswith('TSFP_MAPMAKER')}
    result = subprocess.run([str(exe)], cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    if mutation == 'fixed':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'mapmaker bosses ok' in result.stdout
    else:
        assert result.returncode != 0, f'{mutation} was not detected'


def test_no_guest_code_table():
    """No guest code changes: the save byte is one the stock writer leaves zero."""
    assert not (ROOT / 'config/guest-code-patches/bosses.json').exists()


# ---- guest sites in the lifter's decode (skipped without the private XBE) ----

def by_address():
    return {i.address: i for i in decoded()}


def mem(i):
    return [op for op in i.ops if op[0] == 'mem']


# The Story AI chunk (tag 0x00C7766C) case of every other chunk walker in game
# code (the only `cmp eax, 0xC7766C` sites besides the writer's store) plus the
# helpers given the chunk pointer, and the editor's writer / loader ranges
# (docs/mapmaker/mapmaker-aitemplates.md).
CHUNK_READERS = (
    (0x0023F511, 0x0023F546),   # sub_0023F390: reads +0x2A only
    (0x00240757, 0x00240783),   # sub_00240590: sub_0033EC30(data), counts
    (0x00241DB9, 0x00242386),   # sub_00240C50 (spawn records): data = ebp
    (0x00337E2D, 0x00337E68),   # sub_00337D80: rewrites the name +0x00..+0x1F
    (0x0033EC30, 0x0033EC79),   # sub_0033EC30(data): reads +0x20
)
WRITER = (0x003013E1, 0x003015E5)
LOADER = (0x00302428, 0x0030267F)


def test_sai_chunk_byte_0x29_unused():
    insns = decoded()
    writer = [i for i in insns if WRITER[0] <= i.address < WRITER[1]]
    loader = [i for i in insns if LOADER[0] <= i.address < LOADER[1]]
    assert writer and loader
    for i in writer:                               # data = ebx + esi
        for op in mem(i):
            if {op[1], op[2]} == {'ebx', 'esi'}:
                assert op[4] != 0x29, hex(i.address)
                if op[4] == 0x28:                  # the toggle byte, not a word over +0x29
                    assert op[5] == 1, hex(i.address)
    for i in loader:                               # data = ebx
        for op in mem(i):
            if op[1] == 'ebx' and not op[2]:
                assert op[4] != 0x29 and not (op[4] == 0x28 and op[5] > 1), hex(i.address)
    tags = sorted(i.address for i in insns if i.mnemonic == 'cmp' and ('imm', 0x00C7766C) in i.ops)
    assert tags == [0x0023F511, 0x00240757, 0x00241DB9, 0x00337E2D]
    for start, end in CHUNK_READERS:
        body = [i for i in insns if start <= i.address < end]
        assert body, hex(start)
        for i in body:
            if i.mnemonic == 'lea':
                continue
            for op in mem(i):
                if op[1] != 'esp':
                    assert op[4] != 0x29 and not (op[4] == 0x28 and op[5] > 1), hex(i.address)
    a = by_address()
    assert mem(a[0x00301459])[0][4] == 0x28 and mem(a[0x00301459])[0][5] == 1
    assert mem(a[0x0030146A])[0][4] == 0x2A and mem(a[0x0030146A])[0][5] == 2
    assert mem(a[0x0030143F])[0][4] == 0x22 and mem(a[0x0030143F])[0][5] == 2    # writer: loop index


def test_overhead_meter_draw_contract():
    """Sub-boss meters: sub_0013A400 is a pure draw that never reads the
    damage-stage record (+0x268) and sits above a model node; making records
    with sub_0012C800 is a 17-slot bump allocator; sub_0014D1F0 calls nothing
    before its meter loop, so drawing before the original is the same context."""
    insns = decoded()
    a = by_address()
    meter = [i for i in insns if i.function == 0x0013A400]
    assert meter and not any(op[4] == 0x268 for i in meter for op in mem(i))
    assert a[0x0013AB00].mnemonic == 'ret' and not a[0x0013AB00].ops
    # height: node matrix [[entity+4]+0x10] + [[[entity+4]+0x8C]+0x18]*64, +0x34 (y), + entity +0x68 + [0x475CC8]
    assert mem(a[0x0013A688])[0][4] == 0x8C and mem(a[0x0013A68E])[0][4] == 0x18 and mem(a[0x0013A691])[0][4] == 0x10
    assert mem(a[0x0013A69F])[0][4] == 0x34 and mem(a[0x0013A6A5])[0][4] == 0x68
    assert mem(a[0x0013A694])[0][4] == 0x00475CC8
    assert ('imm', 0xC68) in a[0x0013A429].ops and '0x11f780' in a[0x0013A42F].text   # actor +0xC68 fade (read only)
    # sub_0012C800: [0x007490A0] < 0x11 slots of 0x144 at 0x00745938; reset only at 0x0014853D
    assert ('imm', 0x11) in a[0x0012C806].ops and ('imm', 0x144) in a[0x0012C80D].ops
    writes = sorted(i.address for i in insns if i.mnemonic == 'mov' and mem(i) and mem(i)[0][4] == 0x007490A0
                    and i.text.split(',')[0].count('[') == 1)
    assert writes == [0x0012C819, 0x0014853D]
    # sub_0014D1F0: no call before the meter loop's call at 0x0014D25F
    calls = sorted(i.address for i in insns if i.function == 0x0014D1F0 and i.call)
    assert calls[0] == 0x0014D25F


def touches(op, byte):
    """A memory operand covering chunk byte `byte` (disp..disp+size-1)."""
    return op[4] <= byte < op[4] + max(op[5], 1)


def test_sai_chunk_byte_0x21_unused():
    """Chunk +0x21 (stronger health): the writer stores byte +0x20 (type) and
    word +0x22, never +0x21; no loader or play reader touches it."""
    insns = decoded()
    writer = [i for i in insns if WRITER[0] <= i.address < WRITER[1]]
    loader = [i for i in insns if LOADER[0] <= i.address < LOADER[1]]
    for i in writer:
        for op in mem(i):
            if {op[1], op[2]} == {'ebx', 'esi'}:
                assert not touches(op, 0x21), hex(i.address)
    for i in loader:
        for op in mem(i):
            if op[1] == 'ebx' and not op[2]:
                assert not touches(op, 0x21), hex(i.address)
    for start, end in CHUNK_READERS:
        for i in (i for i in insns if start <= i.address < end):
            if i.mnemonic == 'lea':
                continue
            for op in mem(i):
                # sub_00240C50's chunk data is ebp; its eax/ecx/edx stores fill
                # the spawn tables (e.g. 0x00241E8D dword [eax+0x20])
                if op[1] != 'esp' and (start != 0x00241DB9 or op[1] == 'ebp'):
                    assert not touches(op, 0x21), hex(i.address)
    a = by_address()
    assert mem(a[0x00301460])[0][4] == 0x20 and mem(a[0x00301460])[0][5] == 1    # type byte
    assert mem(a[0x003014A2])[0][4] == 0x30 and mem(a[0x003014A2])[0][5] == 1    # health byte
    assert mem(a[0x0030149F])[0][1] == 'edi' and mem(a[0x0030149F])[0][4] == 0x4C


def test_health_row_and_tables():
    """Stock Health row (text 0xC0D, options 0xC0E..0xC12 = values 0..4) bound
    to entry +0x4C at 0x0031CFD2; play maps chunk +0x30 through 0x0052E060 to
    record +0x4E and the scene-100 health table 0x004DACF4."""
    a = by_address()
    assert ('imm', 0xC0D) in a[0x0031CF30].ops
    for site, text, value in ((0x0031CF5B, 0xC0E, 0), (0x0031CF73, 0xC0F, 1), (0x0031CF8B, 0xC10, 2),
                              (0x0031CFA3, 0xC11, 3), (0x0031CFBB, 0xC12, 4)):
        assert ('imm', text) in a[site].ops, hex(site)
    assert a[0x0031CFB9].mnemonic == 'push' and ('imm', 4) in a[0x0031CFB9].ops
    lea = a[0x0031CFCE]
    assert lea.mnemonic == 'lea' and mem(lea)[0][1] == 'esi' and mem(lea)[0][4] == 0x4C
    call = a[0x0031CFD2]
    assert call.call and call.function == 0x0031CB90 and '0x78590' in call.text
    assert 0x0031CFD7 in a and a[0x0031CFD7].mnemonic == 'add'
    # editor loader: chunk +0x30 byte -> entry +0x4C (0x288 + 0x4C), unclamped
    assert mem(a[0x0030260A])[0][4] == 0x30 and mem(a[0x0030260A])[0][5] == 1
    assert mem(a[0x00302614])[0][4] == 0x2D4
    # play: movzx [ebp+0x30] -> sub_0031BA70 -> record +0x4E
    assert mem(a[0x00242356])[0][1] == 'ebp' and mem(a[0x00242356])[0][4] == 0x30
    assert a[0x0024235B].call and '0x31ba70' in a[0x0024235B].text
    assert mem(a[0x00242360])[0][4] == 0x4E
    assert mem(a[0x0031BA74])[0][4] == 0x0052E060
    assert a[0x000B9A46].mnemonic == 'movzx' and mem(a[0x000B9A46])[0][4] == 0x4E
    assert ('imm', 0x64) in a[0x000B9A30].ops                     # scene 100 only
    assert mem(a[0x000B9A4A])[0][4] == 0x004DACF4
    read = _xbe()
    assert struct.unpack('<5i', read(0x0052E060, 20)) == (14, 10, 6, 3, 0)
    table = struct.unpack('<15f', read(0x004DACF4, 60))
    assert table[0] == 20.0 and table[3] == 11.0 and table[6] == 4.5 and table[10] == 2.75 and table[14] == 1.5


def test_bar_position_sites():
    """sub_00139740 places the box at the viewport bottom minus the int HUD
    margin [0x007A614C], height 45 x [0x004E68F8]; smoothing +0x171C/+0x1724/+0x172C."""
    a = by_address()
    assert mem(a[0x00139873])[0][4] == 0x18                      # viewport bottom
    assert mem(a[0x00139876])[0][4] == 0x007A614C and mem(a[0x00139876])[0][5] == 4
    assert a[0x00139884].mnemonic == 'sub'
    assert mem(a[0x0013987C])[0][4] == 0x004E68F8
    assert mem(a[0x0013988C])[0][4] == 0x00478900
    disps = {op[4] for i in decoded() if i.function == 0x00139740 for op in mem(i) if op[1] == 'esi'}
    assert disps == {0x171C, 0x1724, 0x172C}
    read = _xbe()
    assert struct.unpack('<f', read(0x00478900, 4))[0] == 45.0
    assert [round(struct.unpack('<f', read(v, 4))[0], 3) for v in (0x0047DA00, 0x00475CD0, 0x00475C78)] == [0.933, 0.8, 1.0]


def _xbe():
    path = ROOT / 'private/game/default.xbe'
    if not path.exists() and ROOT.parent.name == 'worktrees':
        path = ROOT.parents[2] / 'private/game/default.xbe'
    if not path.exists():
        pytest.skip('US XBE not available locally')
    data = path.read_bytes()
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, headers = struct.unpack_from('<II', data, 0x11C)
    sections = [struct.unpack_from('<IIIII', data, headers - base + i * 0x38) for i in range(count)]

    def read(va, n):
        for _, start, vsize, raw, rsize in sections:
            if start <= va < start + vsize:
                off = va - start
                chunk = data[raw + off:raw + min(off + n, rsize)]
                return chunk + bytes(n - len(chunk))
        raise KeyError(hex(va))
    return read


def test_edit_screen_rows_site():
    a = by_address()
    call = a[0x0031D07E]
    assert call.call and call.function == 0x0031CB90 and '0x784a0' in call.text
    read = a[0x0031D083]
    assert read.mnemonic == 'mov' and mem(read)[0][1] == 'esp' and mem(read)[0][4] == 0x20


def test_story_boss_bar_sites():
    a = by_address()
    call = a[0x0014F256]                           # HUD frame -> boss bar
    assert call.call and call.function == 0x0014F0B0 and '0x14cc80' in call.text
    assert a[0x0014F0CC].mnemonic == 'imul' and ('imm', 0x177C) in a[0x0014F0CC].ops
    assert a[0x0014F0D2].mnemonic == 'add' and ('imm', 0x007A6160) in a[0x0014F0D2].ops
    assert a[0x0014F0DF].mnemonic == 'mov' and a[0x0014F0DF].text.replace(' ', '') == 'ebx,eax'
    # sub_0014CC80: story mode only, flag bit 0 of HUD block +8 set / cleared
    assert a[0x0014CCB3].mnemonic == 'cmp' and ('imm', 0x0A) in a[0x0014CCB3].ops
    sets, clears = a[0x0014D0D5], a[0x0014D0DD]
    assert sets.mnemonic == 'or' and mem(sets)[0][1] == 'ebx' and mem(sets)[0][4] == 8
    assert clears.mnemonic == 'and' and mem(clears)[0][1] == 'ebx' and mem(clears)[0][4] == 8
    assert a[0x0014CCC3].mnemonic == 'mov' and mem(a[0x0014CCC3])[0][4] == 0xFF4
    assert a[0x0014CCD6].mnemonic == 'movzx' and mem(a[0x0014CCD6])[0][4] == 0x94
    # sub_00139740: edi health fraction, esi smoothing, int *drawn = 1
    for site, disp in ((0x001398B8, 0x1CC), (0x001398C0, 0x1D8)):
        op = mem(a[site])[0]
        assert a[site].function == 0x00139740 and op[1] == 'edi' and op[4] == disp
    assert mem(a[0x0013994B])[0][1] == 'esi' and mem(a[0x0013994B])[0][4] == 0x171C
    out = a[0x00139ABC]
    assert mem(out)[0][1] == 'esp' and mem(out)[0][4] == 0x4C       # arg 3 after 0x18 + 0x28 + 4
    assert a[0x00139AC3].mnemonic == 'mov' and mem(a[0x00139AC3])[0][1] == 'ecx'
    assert a[0x00139ACC].mnemonic == 'ret' and a[0x00139ACC].text.strip() == ''
    # the stock caller: text, 0, &local after the call
    assert a[0x0014D0B1].call and '0x139740' in a[0x0014D0B1].text
    assert a[0x0014D0B6].mnemonic == 'add' and ('imm', 0x0C) in a[0x0014D0B6].ops


def test_character_and_health_sites():
    a = by_address()
    # sub_000DAC30: entity +0x7C = pool entry, entry +0 = entity, entry +0x48 = id
    assert mem(a[0x000DAE15])[0][4] == 0x7C and mem(a[0x000DAE18])[0][1] == 'esi' and mem(a[0x000DAE18])[0][4] == 0
    assert mem(a[0x000DAE1D])[0][4] == 0x48
    # health pair at AI start (sub_0015F8C0) and positions (sub_0027BA40)
    assert a[0x0015FAB4].function == 0x0015F8C0 and mem(a[0x0015FAB4])[0][4] == 0x1D8
    assert mem(a[0x0015FABA])[0][4] == 0x1CC
    for site, disp in ((0x0027BBE5, 0x68), (0x0027BBEF, 0x6C), (0x0027BBF9, 0x64)):
        assert mem(a[site])[0][4] == disp
