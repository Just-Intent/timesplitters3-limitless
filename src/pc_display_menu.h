#ifndef TSFP_PC_DISPLAY_MENU_H
#define TSFP_PC_DISPLAY_MENU_H
#include <stdint.h>
#define PC_DISPLAY_MENU_CALLBACK 0x00012008u
void pc_display_menu_finish(void (*original)(void));
void pc_display_menu_button(void (*original)(void));
void pc_network_bots_menu(void (*original)(void));
void pc_profile_accept(void (*original)(void));
uint32_t pc_network_bots_revive(unsigned slot);
void pc_network_bots_entity_refresh(uint32_t entity);
#include "pc_network_bots_state.h"
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *state);
void pc_network_bots_pose_weapons(uint32_t entity);
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot);
void pc_network_bots_request_hit(uint32_t context);
int pc_network_bots_kill_notice(unsigned victim,unsigned killer);
void pc_display_menu_mouse(void (*original)(void));
void pc_display_menu_mouse_mode(void);
void pc_display_menu_mouse_input(void (*original)(void));
void pc_display_menu_mouse_footer(void (*original)(void));
void pc_display_menu_mouse_dispatch(void (*original)(void));
void pc_display_menu_audio_draw(void (*original)(void),int compact);
void pc_display_menu_sensitivity_draw(void (*original)(void));
void pc_display_menu_slider_bar(void (*original)(void));
void pc_display_mapmaker_meter(void (*original)(void));
/* MapMaker level memory estimate: TSFP_MAPMAKER_MEMORY_SCALE divides it. */
void pc_display_mapmaker_memory(void (*original)(void));
/* MapMaker capacity stage A: AI navigation pools (scene 100) and editor limits. */
void pc_display_mapmaker_pools(void (*original)(void));
void pc_display_mapmaker_limits(void (*original)(void));
/* MapMaker capacity area hooks (begin) */
void pc_mapmaker_pads_group_node(void (*original)(void));
void pc_mapmaker_pads_pad_node(void (*original)(void));
void pc_mapmaker_pads_cover_topup(void (*original)(void));
void pc_mapmaker_container_frontend_init(void (*original)(void));
void pc_mapmaker_container_editor_init(void (*original)(void));
void pc_mapmaker_container_undo_ring(void (*original)(void));
void pc_mapmaker_container_load(void (*original)(void));
void pc_mapmaker_container_save_async(void (*original)(void));
void pc_mapmaker_container_save_sign(void (*original)(void));
void pc_mapmaker_container_save_sync(void (*original)(void));
void pc_mapmaker_container_crc(void (*original)(void));
void pc_mapmaker_container_crc_check(void (*original)(void));
void pc_mapmaker_container_compile(void (*original)(void));
void pc_mapmaker_container_compiled_size(void (*original)(void));
void pc_mapmaker_container_find(void (*original)(void));
void pc_mapmaker_container_net_send(void (*original)(void));
/* MapMaker capacity area hooks (end) */
void pc_display_menu_choice_arrow(void (*original)(void),unsigned direction);
void pc_display_menu_character_draw(void (*original)(void));
void pc_display_menu_choice_draw(void (*original)(void));
void pc_display_menu_team_draw(void (*original)(void));
void pc_display_menu_team_tick(void (*original)(void));
void pc_display_camera(void (*original)(void));
void pc_display_camera_feed(void (*original)(void));
void pc_display_ui(void (*original)(void),uint32_t tag);
void pc_display_scope(void (*original)(void),uint32_t tag);
int pc_display_scope_mask(void (*original)(void));
void pc_display_aim_primitive(void (*original)(void),unsigned kind);
void pc_display_aim_panel(void (*original)(void),uint32_t tag);
void pc_display_effect_scope(void (*original)(void));
void pc_display_weapon_effect(void (*original)(void),unsigned kind);
int pc_display_transition_overlay(void (*original)(void));
void pc_display_flash_overlay(void (*original)(void));
void pc_display_footer_background(void (*original)(void));
void pc_display_title_matte(void (*original)(void));
void pc_display_menu_backdrop(void (*original)(void));
void pc_display_main_menu_font(void (*original)(void),int building);
void pc_display_radar_begin(void);
void pc_display_health(void (*original)(void));
void pc_display_health_side(void);
void pc_display_tutorial(void (*original)(void));
void pc_display_tutorial_wrap(void (*original)(void));
void pc_display_tutorial_text(void (*original)(void));
void pc_display_perspective_projection(void (*original)(void));
void (*pc_display_menu_lookup(uint32_t address))(void);
#endif
