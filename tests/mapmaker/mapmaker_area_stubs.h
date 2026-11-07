/* Link stubs for fixtures that compile the whole guest include chain
 * (pc_display_menu.c / pc_menu_mouse_guest.inc), e.g. menu_mouse.c and
 * fov_settings.c: game functions and runtime helpers that the MapMaker
 * capacity areas (src/pc_mapmaker_<area>.inc) call. Each area's own fixture
 * tests their behaviour. */
#ifndef TSFP_MAPMAKER_AREA_STUBS_H
#define TSFP_MAPMAKER_AREA_STUBS_H
#include <stdint.h>
#include <stdio.h>
void sub_000827B0(void) {} void sub_000827F0(void) {} void sub_0035E960(void) {}
void sub_00025280(void) {} void sub_00336600(void) {}
void sub_00061D80(void) {}  /* disco: editor stream unload */
uint32_t xbox_ContiguousAlloc(uint32_t size, uint32_t alignment) { (void)size; (void)alignment; return 0; }
/* memory, probe (host allocation counters) */
int xbox_ContiguousFree(uint32_t address) { (void)address; return 0; }
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0; }
void pc_capture_allocation_usage(uint32_t out[6]) { for (unsigned i = 0; i < 6; i++) out[i] = 0; }
/* objects */
void sub_0009A2C0(void) {}
void sub_0009A5B0(void) {}
void sub_0009CC40(void) {}
void sub_0009CCA0(void) {}
void sub_000A5AF0(void) {}
void sub_000AEAC0(void) {}
void sub_000B05B0(void) {}
void sub_000B3BE0(void) {}
void sub_000C3540(void) {}
void sub_001806B0(void) {}
void sub_00242BA0(void) {}
void sub_00248970(void) {}
void sub_002FDF50(void) {}
void sub_00333700(void) {}
/* actors */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_001CD040(void) {}
#endif
void sub_001638F0(void) {}
void sub_003533F0(void) {}
/* editor, rooms */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_0003D7B0(void) {}
#endif
void sub_0007A270(void) {}
void sub_000BD090(void) {}  /* roof: fall kill */
void sub_00087310(void) {}  /* roof: room query */
void sub_00157440(void) {}
void sub_00157970(void) {}
void sub_0001A010(void) {}  /* rooms: word draw buckets (sub_00086C40 reimplemented) */
void sub_000A79B0(void) {}
void sub_003442B0(void) {}
void sub_00086AE0(void) {}
void sub_00344350(void) {}
void sub_00087720(void) {}  /* rooms: view room list (sub_000897D0) */
void sub_00300670(void) {}
void sub_0031BB60(void) {}
void sub_003C85D4(void) {}
/* restable: meter font, text measure, backdrop and outlined text */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines sub_0001EFB0 and sub_0003FA10 */
void sub_0001EFB0(void) {}
void sub_0003FA10(void) {}
#endif
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it (upstream sync) */
void sub_00041E30(void) {}
#endif
/* aitemplates */
void sub_0031BE70(void) {}
void sub_00079D60(void) {}
/* bosses */
void sub_00139740(void) {}
void sub_00071800(void) {}
void sub_0013A400(void) {}
/* realbosses */
void sub_0005A130(void) {}
void sub_00062260(void) {}
void sub_00071630(void) {}
void sub_00071C70(void) {}
void sub_00287E00(void) {}
void sub_0029CEF0(void) {}
void sub_00286D30(void) {}
void sub_00059DB0(void) {}
void sub_000EA9C0(void) {}
void sub_0010EB90(void) {}
void sub_002976F0(void) {}  /* realbosses: the Crow walker */
void sub_0015DDA0(void) {}
void sub_0029CCB0(void) {}
void sub_002A7F30(void) {}  /* realbosses: skeleton fire */
void sub_0005F460(void) {}  /* realbosses: the Crow walker's roar */
void sub_00287860(void) {} void sub_00297690(void) {} void sub_002BD480(void) {}  /* realbosses: the stock skeleton/walker resets at a restart, the walker grid */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it */
void sub_00019D70(void) {}
#endif
/* sky (sub_00078560 becomes a wrapper dispatcher) */
void sub_00078560(void) {}
void sub_0007D4F0(void) {}
/* sets (pc_mapmaker_sets.inc: slot screen, Story AI choice, preload) */
void sub_00071970(void) {}
void sub_00074670(void) {}
void sub_002BE650(void) {}
void sub_002BE300(void) {}  /* sets: slot label, row -> text id */
void sub_002BFFD0(void) {}
void sub_000D7780(void) {}
void sub_002C0310(void) {}
/* sets: the 20-row slot pane builder replays the stock builder's calls */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it (upstream sync) */
void sub_00071BE0(void) {}
#endif
void sub_002C1830(void) {} void sub_00078320(void) {}
void sub_000792B0(void) {} void sub_00078380(void) {} void sub_00077140(void) {}
void sub_00077360(void) {}
/* matchbots (pc_mapmaker_matchbots.inc: profile, slot portrait, availability) */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it */
void sub_00069640(void) {}
#endif
void sub_002BE600(void) {}
void sub_002BE270(void) {}
/* weaponset (pc_mapmaker_weaponset.inc) */
void sub_00064E70(void) {}
void sub_0006B2D0(void) {}
void sub_000716D0(void) {}
void sub_00074460(void) {}
void sub_000783D0(void) {}
void sub_00078590(void) {}
void sub_00079DE0(void) {}
void sub_0007D4A0(void) {}
void sub_000BE860(void) {}
void sub_00303350(void) {}
/* weaponset: the 20-row slot pane builder replays the stock builder's calls */
void sub_00074A00(void) {} void sub_00077110(void) {} void sub_000772E0(void) {}
void sub_00077320(void) {} void sub_000780E0(void) {} void sub_000781A0(void) {}
void sub_002BF830(void) {}
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_00075B70(void) {} void sub_00075E70(void) {} void sub_00079710(void) {}
#endif
/* tileflags (pc_mapmaker_tileflags.inc: right-click tile roof menu) */
void sub_002FB230(void) {}
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines these */
void sub_00071B10(void) {} void sub_00071C50(void) {}
#endif
/* robot, crowe, tank (pc_mapmaker_robot.inc, pc_mapmaker_crowe.inc, pc_mapmaker_tank.inc) */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it (upstream sync) */
void sub_000A6F90(void) {}
#endif
void sub_000EFBD0(void) {} void sub_0028D6D0(void) {} void sub_0028D710(void) {}
void sub_00292550(void) {} void sub_00292630(void) {} void sub_002A62C0(void) {} void sub_0028D0C0(void) {}
void sub_000A5E40(void) {} void sub_000631C0(void) {}  /* tank: model request at level init */
void sub_00256040(void) {}  /* tank: the castle's explosive-on-the-stunned-tank damage */
void sub_00096330(void) {}  /* mothership: texture registration */
void sub_00096220(void) {}  /* storyprops: load-pass texture registration */
void sub_000B3B20(void) {}  /* vehicles: gun template preload */
void sub_002F7920(void) {}  /* sets: spawn mode mask of an editor item */
void sub_0003E0B0(void) {} void sub_00165760(void) {}  /* objects: physics body pools */
void sub_0033E560(void) {}  /* aitemplates: the AI death event */
void sub_000C3C50(void) {}  /* crowe: the floor finder */
void sub_00045A70(void) {}  /* imported music: music off test */
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines sub_000717D0 */
void sub_000717D0(void) {}  /* imported music: jukebox row */
#endif
int recomp_audio_output_submit(uint32_t slot,const uint8_t *pcm,uint32_t bytes,uint32_t rate,uint32_t channels,
                               uint32_t bits,int32_t volume) { (void)slot;(void)pcm;(void)bytes;(void)rate;(void)channels;(void)bits;(void)volume;return 0; }
int recomp_audio_output_position(uint32_t slot,uint64_t *played,uint32_t *queued)
{ (void)slot;if(played)*played=0;if(queued)*queued=0;return 0; }
int recomp_audio_output_set_volume(uint32_t slot,int32_t volume) { (void)slot;(void)volume;return 0; }
void recomp_audio_output_reset_voice(uint32_t slot) { (void)slot; }
/* imported music: the MP3 decoder adapter (pc_music_codec.c) is not linked here */
int pc_music_codec_available(void) { return 0; }
struct PcMusicCodec *pc_music_codec_open(FILE *f,uint32_t *r,uint32_t *c) { (void)r;(void)c;if(f) fclose(f);return NULL; }
long pc_music_codec_read(struct PcMusicCodec *d,int16_t *o,unsigned n) { (void)d;(void)o;(void)n;return -1; }
int pc_music_codec_rewind(struct PcMusicCodec *d) { (void)d;return 0; }
void pc_music_codec_close(struct PcMusicCodec *d) { (void)d; }
int recomp_audio_output_take_discontinuity(uint32_t slot) { (void)slot;return 0; }
/* khallos, worm (pc_mapmaker_khallos.inc, pc_mapmaker_worm.inc) */
void sub_0009A420(void) {}
#ifndef TSFP_FIXTURE_DEFINES_UNLOCK_STUBS /* fov_settings.c defines it (upstream sync) */
void sub_000A9070(void) {}
#endif
void sub_0010E2A0(void) {} void sub_001D1E80(void) {}
void sub_0010E730(void) {}  /* khallos: the jet emitter's position */
void sub_001D2350(void) {} void sub_001DA990(void) {} void sub_001DAAB0(void) {} void sub_00378400(void) {} void sub_0005B980(void) {}
/* tilesets (pc_mapmaker_tilesets.inc: pak mount) */
void sub_000622D0(void) {}
#endif
