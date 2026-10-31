/* US 45410066 MapMaker editor grid settings block (src/interface/pc_mapmaker_grid.inc).
 *
 * Four floats in the guest extension block that the editor code reads through
 * operand redirects (config/guest-code-patches/grid.json), so one switch
 * (TSFP_MAPMAKER_LIMITS=1) changes them at run time. Stock values are copied
 * from the image constants they replace, so the default is bit-exact. Shared
 * with src/interface/pc_mapmaker_pointer.inc (mouse panning clamp). */
#ifndef PC_MAPMAKER_GRID_H
#define PC_MAPMAKER_GRID_H

#define MAPMAKER_GRID_CONFIG 0x00903600u
#define MAPMAKER_GRID_CFG_FLOORS (MAPMAKER_GRID_CONFIG+0x0u)   /* floor menu bars: 5.0 (stock [0x004760CC]) / 7.0 */
#define MAPMAKER_GRID_CFG_EXTENT (MAPMAKER_GRID_CONFIG+0x4u)   /* grid cells per axis: 40.0 ([0x004788B8]) / 80.0 */
#define MAPMAKER_GRID_CFG_ZOOM (MAPMAKER_GRID_CONFIG+0x8u)     /* widest view in cells: 60.0 ([0x004760AC]) / 100.0 */
#define MAPMAKER_GRID_CFG_ZOOM_INV (MAPMAKER_GRID_CONFIG+0xCu) /* 1/widest: [0x00478394] / 0.01 */
#define MAPMAKER_GRID_CONFIG_SIZE 0x10u

#endif
