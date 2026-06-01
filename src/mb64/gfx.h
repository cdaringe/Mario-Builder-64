#pragma once

#include "game/game_init.h"
#include "game/geo_misc.h"
#include "../../libmb64/mb64_faceshapes.h"

enum mb64_growth_types {
    MB64_GROWTH_NONE,
    MB64_GROWTH_FULL,
    MB64_GROWTH_NORMAL_SIDE,
    MB64_GROWTH_HALF_SIDE, // vertical slabs - either side
    MB64_GROWTH_UNDERSLOPE_CORNER, // special check
    MB64_GROWTH_DIAGONAL_SIDE,
    MB64_GROWTH_VSLAB_SIDE, // vertical slabs - middle face
    MB64_GROWTH_UNCONDITIONAL,

    // Anything beyond this is a slope decal type
    // & 1 - left or right side
    // & 2 - gentle or steep
    MB64_GROWTH_EXTRADECAL_START = 0x10,
    MB64_GROWTH_SLOPE_SIDE_L = MB64_GROWTH_EXTRADECAL_START,
    MB64_GROWTH_SLOPE_SIDE_R,
    MB64_GROWTH_GENTLE_SIDE_L,
    MB64_GROWTH_GENTLE_SIDE_R,
};

enum ProcessTileRenderModes {
    PROCESS_TILE_NORMAL,
    PROCESS_TILE_TRANSPARENT,
    PROCESS_TILE_BOTH,
    PROCESS_TILE_VPLEX,
};

enum mb64_mat_types {
    // Opaque types (for culling)
    MAT_OPAQUE,
    MAT_DECAL, // only used for VP screen when used as a block type
    // Transparent types
    MAT_CUTOUT,
    MAT_CUTOUT_NOCULL,
    MAT_TRANSPARENT,
    // Used for override when processing vplex screens
    MAT_SCREEN,
};

// Returns full tile definition (struct mb64_tilemat_def)
#define TILE_MATDEF(matid) (mb64_theme_table[mb64_lopt_theme].mats[matid])
// Returns main material (struct mb64_material)
#define MATERIAL(matid) (mb64_mat_table[TILE_MATDEF(matid).mat])

// Returns TRUE if given material has a unique top texture
#define HAS_TOPMAT(matid) (TILE_MATDEF(matid).topmat != TILE_MATDEF(matid).mat)
// Returns top material's topmat struct (struct mb64_material)
#define TOPMAT(matid) (mb64_mat_table[TILE_MATDEF(matid).topmat])

#define fullblock_can_be_waterlogged(mat) ((MATERIAL(mat).type == MAT_CUTOUT) || (TOPMAT(mat).type == MAT_CUTOUT))

#define FENCE_TEX() (mb64_fence_texs[mb64_theme_table[mb64_lopt_theme].fence])
#define POLE_TEX()  (mb64_mat_table[mb64_theme_table[mb64_lopt_theme].pole].gfx)
#define BARS_TEX() (mb64_bar_texs[mb64_theme_table[mb64_lopt_theme].bars][0])
#define BARS_TOPTEX() (mb64_bar_texs[mb64_theme_table[mb64_lopt_theme].bars][1])
#define WATER_TEX() (mb64_water_texs[mb64_theme_table[mb64_lopt_theme].water])

#define retroland_filter_on() if ((mb64_lopt_theme == MB64_THEME_RETRO) || (mb64_lopt_theme == MB64_THEME_MC)) { gDPSetTextureFilter(&mb64_curr_gfx[mb64_gfx_index++], G_TF_POINT); if (!gIsGliden) {mb64_uv_offset = 0;} }
#define retroland_filter_off() if ((mb64_lopt_theme == MB64_THEME_RETRO) || (mb64_lopt_theme == MB64_THEME_MC)) { gDPSetTextureFilter(&mb64_curr_gfx[mb64_gfx_index++], G_TF_BILERP); mb64_uv_offset = (mb64_lopt_theme == MB64_THEME_MC ? -32 : -16); }

extern u32 mb64_gfx_total;
extern u32 mb64_vtx_total;

extern Vtx *mb64_curr_vtx;
extern Gfx *mb64_curr_gfx;
extern u16 mb64_gfx_index;

extern u8 mb64_use_alt_uvs;
extern s8 mb64_uv_offset;
extern u8 mb64_render_flip_normals;
extern u8 mb64_render_vertical;
extern u8 mb64_render_culling_off;
extern u8 mb64_growth_render_type;
extern u8 mb64_curr_mat_has_topside;
extern u8 mb64_curr_poly_vert_count;

// also used for collision
u32 get_faceshape(s8 pos[3], u32 dir);
void mb64_transform_vtx_with_rot(s8 v[][3], s8 oldv[][3], u32 rot);
void check_bar_connections(s8 pos[3], u8 connections[5]);
void render_bars_side(s8 pos[3], u8 connections[5]);
void render_bars_top(s8 pos[3], u8 connections[5]);
u32 is_water_fullblock(s8 pos[3]);
void process_tile(s8 pos[3], struct mb64_terrain *terrain, u32 rot);

// used for boundaries
void render_boundary_quad(struct mb64_boundary_quad *quad, s16 y, s16 yHeight, u32 fade);
void set_render_mode(u32 tileType, u32 disableZ);
u32 do_process(u8 *targetMatType, u32 processTileRenderMode);
Gfx *get_sidetex(s32 matid);

void draw_dotted_line(s16 pos1[3], s16 pos2[3]);

void display_cached_tris(void);
void generate_terrain_gfx(void);
Gfx *mb64_append(s32 callContext, UNUSED struct GraphNode *node, UNUSED Mat4 mtx);
void custom_theme_draw_block(f32 xpos, f32 ypos, s32 index);
