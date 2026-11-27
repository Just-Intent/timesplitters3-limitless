/* US 45410066 MapMaker Story: choose any unlocked character (not only Cortez).
 * Research: logs/limits/features/story-character.md; docs/mapmaker/mapmaker-story-character.md.
 *
 * Stock: a MapMaker map played in Story (mode [0x0078AD2C] == 9, sub-mode
 * byte [0x0078ADAE] == 2) shows the character select screen with one Cortez
 * portrait per player. Four front-end branches test that sub-mode:
 *   roster  sub_002C6C80 @0x002C6D1F: one fixed entry (id 0xC) per player;
 *   preview sub_002C6680 @0x002C672E: cursor on Cortez;
 *   confirm sub_002D4D00 @0x002D5414: choice not stored in the profile;
 *   config  sub_002D2C70 @0x002D359D: character 0xC for every local player.
 * The play code (scene 100) takes the character and variant from the game
 * config (+0x14/+0x44) for every mode, so nothing there needs changing.
 *
 * config/guest-code-patches/story-character.json makes those four
 * `cmp byte [0x0078ADAE],2` read the gate byte MAPMAKER_STORY_GATE instead.
 * Each function above is wrapped and sets the gate on entry:
 *   TSFP_MAPMAKER_LIMITS off: gate = [0x0078ADAE] (stock: Cortez only);
 *   TSFP_MAPMAKER_LIMITS=1:   gate = 0, so MapMaker Story takes the Arcade
 *   path: the unlocked roster (sub_001CD980, player 0's profile), the cursor
 *   on the profile's last choice, the choice stored in the profile and the
 *   chosen character and variant written to the config for each player.
 * Every site tests mode 9 first, so the gate only matters for a MapMaker map
 * played locally; campaign story levels, Arcade Custom (mode 5), challenges
 * (6/7) and System Link MapMaker (11..13) are unchanged. The wrappers are
 * cold front-end functions; they touch two fixed image/extension addresses.
 *
 * Requires the includer's WORD_AT/g_xbox_mem_offset and pc_mapmaker_limits.inc
 * (mapmaker_limits, mapmaker_limits_read). */

#define MAPMAKER_STORY_GATE 0x00903500u    /* story-character.json new_hex 803d0035900002 */
#define MAPMAKER_STORY_SUBMODE 0x0078ADAEu /* front-end sub-mode byte, 2 = Story */
#define MAPMAKER_STORY_B(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))

static void mapmaker_story_gate(void)
{
    mapmaker_limits_read();
    MAPMAKER_STORY_B(MAPMAKER_STORY_GATE)=mapmaker_limits.limits?0u:MAPMAKER_STORY_B(MAPMAKER_STORY_SUBMODE);
}

void pc_mapmaker_story_char_roster(void (*original)(void))  { mapmaker_story_gate();original(); }
void pc_mapmaker_story_char_preview(void (*original)(void)) { mapmaker_story_gate();original(); }
void pc_mapmaker_story_char_confirm(void (*original)(void)) { mapmaker_story_gate();original(); }
void pc_mapmaker_story_char_config(void (*original)(void))  { mapmaker_story_gate();original(); }
