/* MapMaker debug panel: rates, format and snapshot (pc_debug_panel.h) on
 * every host; with TEST_STORE (Windows) also the store in pc_debug.c:
 * refresh cadence, option gating, toggle and its rate-limited stderr line. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef TEST_STORE
#include "pc_debug.c"

const char *recomp_config_lookup(const char *env, const char *key) { (void)env; (void)key; return NULL; }
void pc_crash_frame(void) {}
void pc_debug_config_read(PcDebugOptions *o) { memset(o, 0, sizeof *o); }
int pc_debug_config_save(const PcDebugOptions *o, char *e, size_t n) { (void)o; (void)e; (void)n; return 1; }
#else
#include "pc_debug_panel.h"
#endif

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
#define CLOSE_TO(a, b) (fabs((double)(a) - (double)(b)) < 1e-6)

static int has(char lines[][PC_DEBUG_PANEL_LINE], unsigned n, const char *text)
{
    for (unsigned i = 0; i < n; i++) if (strstr(lines[i], text)) return 1;
    return 0;
}

static PcDebugMapmaker reading(void)
{
    PcDebugMapmaker m;
    memset(&m, 0, sizeof m);
    m.valid = 1; m.scene = 100; m.mapmaker = 1;
    m.characters = 40; m.characters_cap = 64; m.ai = 39; m.ai_cap = 63;
    m.objects_known = 1; m.objects = 1500; m.objects_cap = 2047;
    m.pads = 9000; m.pads_cap = 16384; m.cover = 3000; m.cover_cap = 14592;
    m.links = 20000; m.links_cap = 32512; m.groups = 800; m.groups_cap = 2047;
    m.rooms = 676; m.rooms_cap = 1023; m.visible_rooms = 650; m.camera_room = 12;
    m.player = 1; m.x = 12.5f; m.y = 5.f; m.z = -67.25f;
    m.heap_known = 1; m.level_free = 1048576 * 3 / 2;
    m.nav_used = 1048576 * 5; m.nav_size = 1048576 * 16; m.nav_fallbacks = 2; m.heap_overruns = 7;
    m.room_queries = 1000; m.at_ms = 1000;
    return m;
}

static void test_rates(void)
{
    PcDebugRender a, b;
    PcDebugPanelRates r;
    PcDebugMapmaker ma = reading(), mb = reading();
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.frames = 10; b.frames = 20;
    a.draws_up = 100; b.draws_up = 200;            /* 10/frame */
    a.draws_indexed_up = 0; b.draws_indexed_up = 9000;
    a.draws_vb = 5; b.draws_vb = 45;
    a.draws_indexed_vb = 50; b.draws_indexed_vb = 3250;
    a.texture_binds = 1000; b.texture_binds = 22000;
    a.ticks_per_second = b.ticks_per_second = 1000000;
    a.swap_frames = 100; b.swap_frames = 110;
    a.gate_ticks = 1000; b.gate_ticks = 1000 + 50000;       /* 5 ms wait */
    a.swap_ticks = 0; b.swap_ticks = 3000;                  /* 0.3 ms swap */
    a.rest_ticks = 0; b.rest_ticks = 113000;                /* 11.3 ms rest */
    a.present_ticks = 0; b.present_ticks = 11000;           /* 1.1 ms present */
    mb.room_queries = 1500; mb.at_ms = 1250;
    pc_debug_panel_rates(&a, &b, &ma, &mb, &r);
    CHECK(r.render && CLOSE_TO(r.up, 10) && CLOSE_TO(r.indexed_up, 900) && CLOSE_TO(r.vb, 4) && CLOSE_TO(r.indexed_vb, 320));
    CHECK(CLOSE_TO(r.draws, 1234) && CLOSE_TO(r.binds, 2100));
    CHECK(r.timing && CLOSE_TO(r.wait_ms, 5) && CLOSE_TO(r.swap_ms, .3) && CLOSE_TO(r.present_ms, 1.1) && CLOSE_TO(r.work_ms, 10.2));
    CHECK(r.queries && CLOSE_TO(r.queries_per_second, 2000));
    /* The HLE's five-second window restarted: the new window alone. */
    b.swap_frames = 4; b.gate_ticks = 4000; b.swap_ticks = 400; b.rest_ticks = 40000; b.present_ticks = 4000;
    pc_debug_panel_rates(&a, &b, NULL, NULL, &r);
    CHECK(r.timing && CLOSE_TO(r.wait_ms, 1) && CLOSE_TO(r.work_ms, 9) && CLOSE_TO(r.present_ms, 1) && !r.queries);
    /* Restarted with no frame yet, or no swap at all: no split. */
    b.swap_frames = 0; pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.timing);
    b.swap_frames = a.swap_frames; pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.timing);
    /* A present longer than the rest never gives negative work. */
    b.swap_frames = 110; b.gate_ticks = a.gate_ticks; b.rest_ticks = 1000; b.present_ticks = 5000;
    pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(r.timing && r.work_ms == 0);
    /* A counter going backwards drops that part; no frames, no draws. */
    b.gate_ticks = a.gate_ticks - 1; pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.timing && r.render);
    b.draws_vb = 1; pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.render);
    b.draws_vb = 45; b.frames = a.frames; pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.render);
    b.ticks_per_second = 0; b.frames = 20; b.gate_ticks = 9000000;
    pc_debug_panel_rates(&a, &b, NULL, NULL, &r); CHECK(!r.timing && r.render);
    pc_debug_panel_rates(NULL, &b, NULL, NULL, &r); CHECK(!r.render && !r.timing);
    /* Queries need two valid readings, later in time, monotonic. */
    mb.at_ms = ma.at_ms; pc_debug_panel_rates(NULL, NULL, &ma, &mb, &r); CHECK(!r.queries);
    mb.at_ms = 2000; mb.room_queries = 10; pc_debug_panel_rates(NULL, NULL, &ma, &mb, &r); CHECK(!r.queries);
    mb.room_queries = 2000; ma.valid = 0; pc_debug_panel_rates(NULL, NULL, &ma, &mb, &r); CHECK(!r.queries);
}

static void test_format(void)
{
    char lines[PC_DEBUG_PANEL_LINES][PC_DEBUG_PANEL_LINE], snap[1400];
    PcDebugPanelRates r;
    PcDebugMapmaker m = reading();
    unsigned n;
    memset(&r, 0, sizeof r);
    r.render = 1; r.draws = 1234; r.up = 10; r.indexed_up = 900; r.vb = 4; r.indexed_vb = 320; r.binds = 2100;
    r.timing = 1; r.work_ms = 10.2; r.present_ms = 1.1; r.swap_ms = .3; r.wait_ms = 5;
    r.queries = 1; r.queries_per_second = 2000;
    n = pc_debug_panel_format(&r, &m, 60, 16.67, lines, PC_DEBUG_PANEL_LINES);
    CHECK(n == 8 && strstr(lines[0], "Shift+F9"));
    CHECK(has(lines, n, "Frame 16.7 ms (60 fps): work 10.2, present 1.1, swap 0.3, wait 5.0 ms"));
    CHECK(has(lines, n, "Draws 1234/frame (UP 10, indexed UP 900, VB 4, indexed VB 320), texture binds 2100"));
    CHECK(has(lines, n, "Scene 100 (MapMaker): characters 40/64, AI 39/63, engine objects 1500/2047"));
    CHECK(has(lines, n, "Pads 9000/16384, cover 3000/14592, links 20000/32512, groups 800/2047"));
    CHECK(has(lines, n, "Rooms 676/1023, visible 650, camera room 12, room queries 2000/s"));
    CHECK(has(lines, n, "Level heap free 1.50 MB, nav block 5.00/16.00 MB, 2 fallbacks, 7 overrun allocs"));
    CHECK(has(lines, n, "Player 12.5, 5.0, -67.2") || has(lines, n, "Player 12.5, 5.0, -67.3"));
    for (unsigned i = 0; i < n; i++) CHECK(strlen(lines[i]) < 120);
    pc_debug_panel_snapshot(lines, n, snap, sizeof snap);
    CHECK(!strncmp(snap, "[PC-DEBUG] MapMaker panel: Frame 16.7 ms", 40) && !strstr(snap, "Shift+F9"));
    CHECK(strstr(snap, " | Draws 1234/frame") && strstr(snap, " | Player 12.5") && !strchr(snap, '\n'));
    pc_debug_panel_snapshot(lines, n, snap, 50); CHECK(strlen(snap) == 49);
    /* Unknowns read as "--", caps of 0 drop the "/cap". */
    memset(&r, 0, sizeof r);
    m.scene = 1; m.mapmaker = 0; m.pads_cap = m.cover_cap = m.links_cap = m.groups_cap = m.rooms_cap = 0;
    m.objects_known = 0; m.visible_rooms = -1; m.camera_room = -1; m.player = 0; m.heap_known = 0;
    n = pc_debug_panel_format(&r, &m, 0, 0, lines, PC_DEBUG_PANEL_LINES);
    CHECK(has(lines, n, "Frame -- ms") && has(lines, n, "Draws --") && has(lines, n, "engine objects --"));
    CHECK(has(lines, n, "Scene 1: ") && has(lines, n, "Pads 9000, cover 3000, links 20000, groups 800"));
    CHECK(has(lines, n, "Rooms 676, visible --, camera room --, room queries --"));
    CHECK(has(lines, n, "Level heap --") && has(lines, n, "Player --"));
    n = pc_debug_panel_format(&r, &m, 59.9, 16.7, lines, PC_DEBUG_PANEL_LINES);
    CHECK(has(lines, n, "split unavailable"));
    n = pc_debug_panel_format(&r, NULL, 0, 0, lines, PC_DEBUG_PANEL_LINES);
    CHECK(n == 4 && has(lines, n, "waiting for the game"));
    m.valid = 0; n = pc_debug_panel_format(&r, &m, 0, 0, lines, 2); CHECK(n == 2 && !has(lines, n, "waiting"));
}

#ifdef TEST_STORE
static void test_store(void)
{
    char lines[PC_DEBUG_PANEL_LINES][PC_DEBUG_PANEL_LINE];
    PcDebugOptions o;
    PcDebugRender render;
    PcDebugMapmaker m = reading();
    unsigned n;
    memset(&o, 0, sizeof o); pc_debug_set(&o);
    memset(&render, 0, sizeof render);
    CHECK(pc_debug_panel_text(lines, PC_DEBUG_PANEL_LINES) == 0);       /* option off */
    CHECK(pc_debug_mapmaker_due());
    pc_debug_mapmaker_publish(&m);
    CHECK(!pc_debug_mapmaker_due());                                   /* 250 ms cadence */
    pc_debug_toggle_mapmaker(); pc_debug_get(&o); CHECK(o.show_mapmaker_panel && !o.show_fps);
    n = pc_debug_panel_text(lines, PC_DEBUG_PANEL_LINES);
    CHECK(n == 8 && has(lines, n, "characters 40/64") && has(lines, n, "Draws --"));
    pc_debug_toggle_fps(); pc_debug_get(&o); CHECK(o.show_mapmaker_panel && o.show_fps);
    /* Renderer readings: one per frame, refreshed at most every 250 ms. */
    for (unsigned i = 0; i < 4; i++) { render.draws_up += 100; pc_debug_render(&render); }
    n = pc_debug_panel_text(lines, PC_DEBUG_PANEL_LINES);
    CHECK(has(lines, n, "Draws --"));                                  /* not refreshed yet */
    Sleep(300);
    render.draws_up += 100; pc_debug_render(&render);
    n = pc_debug_panel_text(lines, PC_DEBUG_PANEL_LINES);
    CHECK(has(lines, n, "Draws 100/frame (UP 100,"));
    CHECK(pc_debug_panel_text(lines, 3) == 3);
    pc_debug_toggle_mapmaker(); pc_debug_get(&o); CHECK(!o.show_mapmaker_panel);
    CHECK(pc_debug_panel_text(lines, PC_DEBUG_PANEL_LINES) == 0);
}
#endif

int main(void)
{
    test_rates();
    test_format();
#ifdef TEST_STORE
    test_store();
#endif
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    puts("debug panel: ok");
    return 0;
}
