/* MapMaker debug panel: the game-thread reading (src/interface/pc_mapmaker_panel.inc).
 * Read-only, 250 ms gated, caps by scene/mode/pools, pointer guards. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char memory[0x00800000];
static ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
/* What the areas included before the panel provide (pc_mapmaker_limits.inc,
 * pc_mapmaker_memory.inc, pc_mapmaker_roomquery.inc). */
#define MAPMAKER_POOL_PADS 16384u
#define MAPMAKER_POOL_COVER 14592u
#define MAPMAKER_POOL_LINKS 32512u
#define MAPMAKER_LIMIT_GROUPS 2047
static struct { int initialized, pools, limits; } mapmaker_limits;
static unsigned limits_reads;
static void mapmaker_limits_read(void) { limits_reads++; }
static struct { int over[2]; unsigned lines; unsigned long long overrun_allocs; int32_t min_free[2]; } mapmaker_memory;
static struct { int initialized, off, tried, reported; uint32_t base, size, used, peak, resets, fallbacks; } mapmaker_nav;
static unsigned long long mapmaker_rooms_walk_calls;

#include "pc_debug_panel.h"
struct PcDebugMapmaker;
static int due = 1;
static unsigned published;
static PcDebugMapmaker last;
int pc_debug_mapmaker_due(void) { return due; }
void pc_debug_mapmaker_publish(const PcDebugMapmaker *m) { published++; last = *m; }

#include "pc_mapmaker_panel.inc"

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
#define B(a) memory[(uint32_t)(a)]
static void put_s16(uint32_t a, int16_t v) { memcpy(&memory[a], &v, 2); }

enum { VIEW = 0x00100000u, RECORDS = 0x00120000u, PLAYER = 0x00140000u, ENT = 0x00141000u };

static void level(void)
{
    memset(memory, 0, sizeof memory);
    WORD_AT(0x0079094Cu) = 100u;            /* scene */
    B(0x007DE455u) = 10u;                   /* Story */
    WORD_AT(0x0074C37Cu) = 40u; WORD_AT(0x007A2958u) = 39u;
    WORD_AT(0x0072F5E4u) = 2047u; WORD_AT(0x007B4FACu) = 1600u; WORD_AT(0x007B65B8u) = 100u;
    WORD_AT(0x007B6664u) = 9000u; WORD_AT(0x007B65C0u) = 3000u; WORD_AT(0x007B6660u) = 20000u; WORD_AT(0x007B666Cu) = 800u;
    WORD_AT(0x007B975Cu) = 3u; WORD_AT(0x00708788u) = RECORDS; WORD_AT(0x007B0CC0u) = VIEW;
    WORD_AT(VIEW + 0x2A0u) = 2u;
    /* room 1 visible, room 2 culled (negative), room 3 empty rectangle */
    WORD_AT(VIEW + 0x1000u + 12u) = 0u; put_s16(VIEW + 0x1000u + 12u + 4u, 10); put_s16(VIEW + 0x1000u + 12u + 8u, 20);
    WORD_AT(VIEW + 0x1000u + 24u) = 0xFFFFFFFFu; put_s16(VIEW + 0x1000u + 24u + 4u, 10); put_s16(VIEW + 0x1000u + 24u + 8u, 20);
    WORD_AT(VIEW + 0x1000u + 36u) = 0u; put_s16(VIEW + 0x1000u + 36u + 4u, 20); put_s16(VIEW + 0x1000u + 36u + 8u, 20);
    WORD_AT(0x007B0C7Cu) = PLAYER; WORD_AT(PLAYER + 0x14u) = ENT;
    FLOAT_AT(ENT + 0x64u) = 1.5f; FLOAT_AT(ENT + 0x68u) = -2.f; FLOAT_AT(ENT + 0x6Cu) = 300.25f;
    WORD_AT(0x006B8384u) = (uint32_t)-4096;
    mapmaker_limits.pools = 1;
    mapmaker_nav.used = 5000000u; mapmaker_nav.size = 16777216u; mapmaker_nav.fallbacks = 3u;
    mapmaker_memory.overrun_allocs = 9u;
    mapmaker_rooms_walk_calls = 123456u;
}

int main(void)
{
    static unsigned char before[sizeof memory];
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)memory;
    level();
    memcpy(before, memory, sizeof memory);
    pc_mapmaker_panel_guest_tick();
    CHECK(!memcmp(before, memory, sizeof memory));        /* read-only */
    CHECK(published == 1 && limits_reads == 1);
    CHECK(last.scene == 100 && last.mapmaker == 1);
    CHECK(last.characters == 40 && last.characters_cap == 64 && last.ai == 39 && last.ai_cap == 63);
    CHECK(last.objects_known && last.objects == 1500 && last.objects_cap == 2047);
    CHECK(last.pads == 9000 && last.pads_cap == 16384 && last.cover == 3000 && last.cover_cap == 14592);
    CHECK(last.links == 20000 && last.links_cap == 32512 && last.groups == 800 && last.groups_cap == 2047);
    CHECK(last.rooms == 3 && last.rooms_cap == 1023 && last.visible_rooms == 1 && last.camera_room == 2);
    CHECK(last.player && last.x == 1.5f && last.y == -2.f && last.z == 300.25f);
    CHECK(last.heap_known && last.level_free == -4096 && last.nav_used == 5000000u && last.nav_size == 16777216u);
    CHECK(last.nav_fallbacks == 3 && last.heap_overruns == 9 && last.room_queries == 123456u);

    /* Not due: nothing read or published. */
    due = 0; pc_mapmaker_panel_guest_tick(); CHECK(published == 1 && limits_reads == 1); due = 1;

    /* Pools off: counts without the pool caps. */
    mapmaker_limits.pools = 0; pc_mapmaker_panel_guest_tick();
    CHECK(last.pads == 9000 && !last.pads_cap && !last.cover_cap && !last.links_cap && !last.groups_cap);
    mapmaker_limits.pools = 1;

    /* Arcade, or a System Link session: the stock AI table. */
    B(0x007DE455u) = 4u; pc_mapmaker_panel_guest_tick(); CHECK(last.ai_cap == 30);
    B(0x007DE455u) = 10u; WORD_AT(0x0076B108u) = 1u; pc_mapmaker_panel_guest_tick(); CHECK(last.ai_cap == 30);
    WORD_AT(0x0076B108u) = 0u;

    /* Another scene: no MapMaker caps, no room scan, camera room still read. */
    WORD_AT(0x0079094Cu) = 7u; pc_mapmaker_panel_guest_tick();
    CHECK(!last.mapmaker && last.scene == 7 && !last.rooms_cap && last.visible_rooms == -1 && last.camera_room == 2);
    CHECK(!last.pads_cap && last.ai_cap == 30);
    WORD_AT(0x0079094Cu) = 100u;

    /* Guards: no view, no player, room count past the cap, freed > count. */
    WORD_AT(0x007B0CC0u) = 0u; WORD_AT(0x007B0C7Cu) = 0u; WORD_AT(0x007B65B8u) = 1601u;
    pc_mapmaker_panel_guest_tick();
    CHECK(last.camera_room == -1 && last.visible_rooms == -1 && !last.player && !last.objects_known);
    WORD_AT(0x007B0CC0u) = VIEW; WORD_AT(0x007B0C7Cu) = PLAYER; WORD_AT(PLAYER + 0x14u) = 0x00000010u;
    WORD_AT(0x007B975Cu) = 1024u; WORD_AT(0x0072F5E4u) = 0u;
    pc_mapmaker_panel_guest_tick();
    CHECK(!last.player && last.visible_rooms == -1 && last.rooms == 1024 && !last.objects_known);

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    puts("mapmaker panel: ok");
    return 0;
}
