#ifndef TSFP_PC_DEBUG_PANEL_H
#define TSFP_PC_DEBUG_PANEL_H
/* MapMaker debug panel (show_mapmaker_panel, Shift+F9): value-only readings
 * and their formatting, shared by every host. No locks, clocks or guest
 * memory here; pc_debug.c owns the store and the refresh, the game-thread
 * reader is src/interface/pc_mapmaker_panel.inc, the renderer counters come
 * from the HLE overlay frame (patches/diagnostics/2640-mapmaker-debug-panel.patch).
 * Every number is a counter that something already maintains; the panel
 * only takes differences between two readings 250 ms apart. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PC_DEBUG_PANEL_LINES 8u
#define PC_DEBUG_PANEL_LINE 160u
#define PC_DEBUG_PANEL_REFRESH_MS 250u
#define PC_DEBUG_PANEL_LOG_MS 1000u

/* Renderer counters, read once per presented guest frame. Draw and texture
 * counts are cumulative from start. The swap timing fields are the HLE's
 * five-second report window: they restart (swap_frames drops) after each
 * "[HLE-D3D8] swap timing" line. frames is pc_debug's own count of readings. */
typedef struct PcDebugRender {
    unsigned long long draws_up, draws_indexed_up, draws_vb, draws_indexed_vb, texture_binds;
    unsigned long long frames;
    unsigned long long swap_frames;
    long long gate_ticks, swap_ticks, rest_ticks, present_ticks, ticks_per_second;
} PcDebugRender;

/* Game-thread reading of fixed guest counters and host-side MapMaker
 * counters (pc_mapmaker_panel.inc). A cap of 0 means "not known here". */
typedef struct PcDebugMapmaker {
    int valid;                         /* set by pc_debug on publish */
    int32_t scene;
    int mapmaker;                      /* scene 100 (MapMaker play) */
    unsigned characters, characters_cap, ai, ai_cap;
    int objects_known;
    unsigned objects, objects_cap;
    unsigned pads, pads_cap, cover, cover_cap, links, links_cap, groups, groups_cap;
    unsigned rooms, rooms_cap;
    int visible_rooms;                 /* -1: not counted */
    int camera_room;                   /* -1: unknown */
    int player;
    float x, y, z;
    int heap_known;
    int32_t level_free;
    unsigned nav_used, nav_size, nav_fallbacks;
    unsigned long long heap_overruns, room_queries;
    unsigned long long at_ms;          /* host time of the reading (pc_debug) */
} PcDebugMapmaker;

typedef struct PcDebugPanelRates {
    int render;                        /* draw counts known */
    double draws, up, indexed_up, vb, indexed_vb, binds;   /* per frame */
    int timing;                        /* frame split known */
    double work_ms, present_ms, swap_ms, wait_ms;
    int queries;
    double queries_per_second;
} PcDebugPanelRates;

/* Per-frame averages between two renderer readings and two MapMaker
 * readings (any may be NULL). A restarted swap window uses the new window
 * alone; any counter that went backwards drops that part of the result. */
static inline void pc_debug_panel_rates(const PcDebugRender *a, const PcDebugRender *b,
                                        const PcDebugMapmaker *ma, const PcDebugMapmaker *mb,
                                        PcDebugPanelRates *out)
{
    memset(out, 0, sizeof *out);
    if (a && b && b->frames > a->frames &&
        b->draws_up >= a->draws_up && b->draws_indexed_up >= a->draws_indexed_up &&
        b->draws_vb >= a->draws_vb && b->draws_indexed_vb >= a->draws_indexed_vb &&
        b->texture_binds >= a->texture_binds) {
        double n = (double)(b->frames - a->frames);
        out->render = 1;
        out->up = (double)(b->draws_up - a->draws_up) / n;
        out->indexed_up = (double)(b->draws_indexed_up - a->draws_indexed_up) / n;
        out->vb = (double)(b->draws_vb - a->draws_vb) / n;
        out->indexed_vb = (double)(b->draws_indexed_vb - a->draws_indexed_vb) / n;
        out->draws = out->up + out->indexed_up + out->vb + out->indexed_vb;
        out->binds = (double)(b->texture_binds - a->texture_binds) / n;
    }
    if (a && b && b->ticks_per_second > 0) {
        unsigned long long frames = 0;
        long long gate = 0, swap = 0, rest = 0, present = 0;
        if (b->swap_frames > a->swap_frames) {
            frames = b->swap_frames - a->swap_frames;
            gate = b->gate_ticks - a->gate_ticks; swap = b->swap_ticks - a->swap_ticks;
            rest = b->rest_ticks - a->rest_ticks; present = b->present_ticks - a->present_ticks;
        } else if (b->swap_frames < a->swap_frames && b->swap_frames) {
            frames = b->swap_frames;   /* the window restarted in between */
            gate = b->gate_ticks; swap = b->swap_ticks; rest = b->rest_ticks; present = b->present_ticks;
        }
        if (frames && gate >= 0 && swap >= 0 && rest >= 0 && present >= 0) {
            double ms = 1000.0 / (double)b->ticks_per_second / (double)frames;
            /* The host present runs after the timed Swap, so the next frame's
             * "rest" contains it: work is the rest without the present. */
            out->timing = 1;
            out->present_ms = (double)present * ms;
            out->work_ms = rest > present ? (double)(rest - present) * ms : 0.0;
            out->swap_ms = (double)swap * ms;
            out->wait_ms = (double)gate * ms;
        }
    }
    if (ma && mb && ma->valid && mb->valid && mb->at_ms > ma->at_ms && mb->room_queries >= ma->room_queries) {
        out->queries = 1;
        out->queries_per_second = (double)(mb->room_queries - ma->room_queries) * 1000.0 /
                                  (double)(mb->at_ms - ma->at_ms);
    }
}

static inline void pc_debug_panel_count(char *out, size_t bytes, unsigned value, unsigned cap)
{
    if (cap) snprintf(out, bytes, "%u/%u", value, cap);
    else snprintf(out, bytes, "%u", value);
}

/* The panel's lines; returns how many were written (at most max). */
static inline unsigned pc_debug_panel_format(const PcDebugPanelRates *r, const PcDebugMapmaker *m,
                                             double fps, double frame_ms,
                                             char lines[][PC_DEBUG_PANEL_LINE], unsigned max)
{
    char t[PC_DEBUG_PANEL_LINES][PC_DEBUG_PANEL_LINE];
    char a[24], b[24], c[24], d[24];
    unsigned n = 0, i;
    snprintf(t[n++], PC_DEBUG_PANEL_LINE, "MapMaker debug (Shift+F9)");
    if (fps > 0 && r->timing)
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Frame %.1f ms (%.0f fps): work %.1f, present %.1f, swap %.1f, wait %.1f ms",
                 frame_ms, fps, r->work_ms, r->present_ms, r->swap_ms, r->wait_ms);
    else if (fps > 0)
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Frame %.1f ms (%.0f fps): split unavailable", frame_ms, fps);
    else snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Frame -- ms");
    if (r->render)
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Draws %.0f/frame (UP %.0f, indexed UP %.0f, VB %.0f, indexed VB %.0f), texture binds %.0f",
                 r->draws, r->up, r->indexed_up, r->vb, r->indexed_vb, r->binds);
    else snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Draws --");
    if (!m || !m->valid) {
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "MapMaker counts: waiting for the game");
    } else {
        pc_debug_panel_count(a, sizeof a, m->characters, m->characters_cap);
        pc_debug_panel_count(b, sizeof b, m->ai, m->ai_cap);
        if (m->objects_known) pc_debug_panel_count(c, sizeof c, m->objects, m->objects_cap);
        else snprintf(c, sizeof c, "--");
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Scene %d%s: characters %s, AI %s, engine objects %s",
                 (int)m->scene, m->mapmaker ? " (MapMaker)" : "", a, b, c);
        pc_debug_panel_count(a, sizeof a, m->pads, m->pads_cap);
        pc_debug_panel_count(b, sizeof b, m->cover, m->cover_cap);
        pc_debug_panel_count(c, sizeof c, m->links, m->links_cap);
        pc_debug_panel_count(d, sizeof d, m->groups, m->groups_cap);
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Pads %s, cover %s, links %s, groups %s", a, b, c, d);
        pc_debug_panel_count(a, sizeof a, m->rooms, m->rooms_cap);
        if (m->visible_rooms >= 0) snprintf(b, sizeof b, "%d", m->visible_rooms); else snprintf(b, sizeof b, "--");
        if (m->camera_room >= 0) snprintf(c, sizeof c, "%d", m->camera_room); else snprintf(c, sizeof c, "--");
        if (r->queries) snprintf(d, sizeof d, "%.0f/s", r->queries_per_second); else snprintf(d, sizeof d, "--");
        snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Rooms %s, visible %s, camera room %s, room queries %s", a, b, c, d);
        if (m->heap_known)
            snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Level heap free %.2f MB, nav block %.2f/%.2f MB, %u fallbacks, %llu overrun allocs",
                     (double)m->level_free / 1048576.0, (double)m->nav_used / 1048576.0,
                     (double)m->nav_size / 1048576.0, m->nav_fallbacks, m->heap_overruns);
        else snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Level heap --");
        if (m->player) snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Player %.1f, %.1f, %.1f", m->x, m->y, m->z);
        else snprintf(t[n++], PC_DEBUG_PANEL_LINE, "Player --");
    }
    if (n > max) n = max;
    for (i = 0; i < n; i++) memcpy(lines[i], t[i], PC_DEBUG_PANEL_LINE);
    return n;
}

/* One stderr line from the panel's lines (the title line is left out). */
static inline void pc_debug_panel_snapshot(char lines[][PC_DEBUG_PANEL_LINE], unsigned count,
                                           char *out, size_t bytes)
{
    size_t used;
    unsigned i;
    if (!bytes) return;
    used = (size_t)snprintf(out, bytes, "[PC-DEBUG] MapMaker panel:");
    for (i = 1; i < count && used < bytes; i++)
        used += (size_t)snprintf(out + used, bytes - used, "%s %s", i > 1 ? " |" : "", lines[i]);
}
#endif
