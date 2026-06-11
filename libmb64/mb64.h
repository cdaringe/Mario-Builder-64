/**
 * mb64.h - Public API for libmb64.
 *
 * Parses Mario Builder 64 (.mb64) level files and returns structured C data.
 * Public enum values mirror src/mb64/data.h and packed field shapes mirror
 * src/mb64/structs.h so external consumers use the same IDs as the game.
 *
 * Usage:
 *   mb64_level_t *lvl = mb64_load("level.mb64");
 *   if (!lvl) { ... error ... }
 *   // Access all five named fields:
 *   //   lvl->header       - file metadata (author, version, counts, etc.)
 *   //   lvl->tiles        - lvl->header.tile_count decoded tiles
 *   //   lvl->objects      - lvl->header.object_count decoded objects
 *   //   lvl->trajectories - MB64_TRAJ_COUNT waypoints (20 paths x 50 points)
 *   //   lvl->header.theme - theme index (also lvl->header.custom_theme)
 *   mb64_free(lvl);
 */

#ifndef MB64_H
#define MB64_H

#include <stdint.h>
#include <stddef.h>

#include "mb64_object_types.h"
#include "mb64_save_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Total trajectory waypoints stored per level (20 paths x 50 points). */
#define MB64_TRAJ_COUNT (MB64_MAX_TRAJECTORIES * MB64_TRAJECTORY_LENGTH)

/* Custom theme */
typedef struct {
    uint8_t mats[10];
    uint8_t topmats[10];
    uint8_t topmats_enabled[10];
    uint8_t fence, pole, bars, water;
} mb64_custom_theme_t;

/* Level header */
typedef struct {
    char     file_header[11];              /* 10-byte ASCII magic + NUL terminator */
    uint8_t  version;
    char     author[32];                   /* 31-byte ASCII + NUL terminator */
    uint16_t piktcher[MB64_PIKTCHER_SIZE]; /* 64x64 RGB5A1 thumbnail (offset 42, 8192 bytes) */
    uint8_t costume;
    uint8_t seq[5];
    uint8_t envfx;
    uint8_t theme;             /* built-in theme index */
    uint8_t bg;
    uint8_t boundary_mat;
    uint8_t boundary;
    uint8_t boundary_height;
    uint8_t coinstar;
    uint8_t level_size;        /* MB64 size field */
    uint8_t waterlevel;
    uint8_t secret;
    uint8_t game;
    uint8_t toolbar[9];
    uint8_t toolbar_params[9];
    uint16_t tile_count;
    uint16_t object_count;
    mb64_custom_theme_t custom_theme;
} mb64_header_t;

/* Tile decoded from the packed u32 layout in src/mb64/structs.h. */
typedef struct {
    uint32_t raw;         /* original big-endian word */
    uint8_t  x, y, z;    /* grid coordinates (0-63) */
    uint8_t  type;        /* tile type (0-31) */
    uint8_t  mat;         /* material (0-15) */
    uint8_t  rot;         /* rotation (0-3) */
    uint8_t  waterlogged; /* 1 if waterlogged, 0 otherwise */
} mb64_tile_t;

/* Object */
typedef struct {
    uint8_t bparam;
    uint8_t x, y, z;
    uint8_t type;
    uint8_t rot;
    uint8_t imbue;
    /* pad byte skipped */
} mb64_obj_t;

typedef struct mb64_comptraj mb64_traj_t;

typedef enum {
    MB64_ENVFX_NONE = 0,
    MB64_ENVFX_ASHES,
    MB64_ENVFX_SNOW,
    MB64_ENVFX_RAIN,
    MB64_ENVFX_SANDSTORM,
    MB64_ENVFX_COUNT,
} mb64_envfx_t;

/* Parsed level - five first-class fields per scenario 7. */
typedef struct {
    mb64_header_t  header;                         /* metadata; theme at header.theme */
    mb64_tile_t   *tiles;                          /* header.tile_count entries, heap-alloc */
    mb64_obj_t    *objects;                        /* header.object_count entries, heap-alloc */
    mb64_traj_t    trajectories[MB64_TRAJ_COUNT];  /* flat 20 path x 50 waypoint table */
} mb64_level_t;

typedef enum {
    MB64_MESH_FACE_TOP = 0,
    MB64_MESH_FACE_BOTTOM = 1,
    MB64_MESH_FACE_POS_X = 2,
    MB64_MESH_FACE_NEG_X = 3,
    MB64_MESH_FACE_POS_Z = 4,
    MB64_MESH_FACE_NEG_Z = 5,
} mb64_mesh_face_dir_t;

typedef struct {
    int16_t v[4][3];      /* MB64 coordinates in sixteenths of one tile */
    int16_t tc[4][2];     /* Optional texture coordinates in native N64 units */
    uint8_t material;
    uint8_t resolved_material;
    uint8_t tile_type;
    uint8_t direction;
    uint8_t is_water;
    uint8_t vertex_count;
    uint8_t use_tc;
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t tile_z;
} mb64_mesh_face_t;

typedef struct {
    mb64_mesh_face_t *faces;
    uint32_t face_count;
    uint32_t solid_tile_count;
    uint32_t water_tile_count;
} mb64_mesh_t;

#define MB64_BOUNDARY_FLAG_INNER_FLOOR (1 << 0)
#define MB64_BOUNDARY_FLAG_OUTER_FLOOR (1 << 1)
#define MB64_BOUNDARY_FLAG_INNER_WALLS (1 << 2)
#define MB64_BOUNDARY_FLAG_OUTER_WALLS (1 << 3)
#define MB64_BOUNDARY_FLAG_CEILING     (1 << 4)
#define MB64_DEATH_PLANE_FACE_COUNT 4

typedef struct {
    int16_t v[4][3]; /* MB64 coordinates in sixteenths of one tile */
} mb64_boundary_face_t;

typedef struct {
    uint8_t animated;
    uint8_t tile_size_cmd;
    uint8_t interval;
    uint16_t step_s;
    uint16_t step_t;
} mb64_material_texture_animation_t;

typedef struct {
    float thin_height;
    float fat_height;
    float stack_dist_epsilon;
    float wall_hitbox_radius;
    float gravity;
    float buoyancy;
    float water_probe_y;
    float water_surface_offset;
    float mario_weight_vel;
    float ground_pound_weight_vel;
    float water_drag;
    float water_float_base;
    float water_float_min;
    float water_float_max;
    int death_drop_offset;
    int steep_slope_degrees;
} mb64_woodplat_config_t;

typedef struct {
    float wake_min_distance;
    float wake_max_distance;
    float shake_forward_speed;
    float launch_forward_speed;
    float floor_probe_offset_y;
    float rotate_min_distance;
    int wake_angle_threshold;
    int shake_start_frame;
    int launch_frame;
    int timeout_frame;
    int explosion_reset_frame;
    int rotate_step;
} mb64_bullet_bill_config_t;

typedef struct {
    float break_coin_radius;
    float shake_amplitude;
    float shake_center_offset;
    int init_timer;
    int shake_timer_limit;
    int clank_cooldown_timer;
} mb64_reinforced_box_config_t;

typedef struct {
    int spin_accel;
    float shrink_factor;
    float delete_scale;
} mb64_badge_config_t;

typedef struct {
    float gravity;
    float launch_accel;
    float mario_activation_distance;
    float flame_y_offset;
    int flame_warmup_frame;
    int launch_frame;
    int landing_roll_angle;
    int roll_step;
    int splash_flame_count;
} mb64_podoboo_config_t;

typedef struct {
    uint8_t segment_count;
    uint8_t head_part_index;
    float scale;
    float body_step;
    float head_start_y;
    float sway_radius;
    float expand_step;
    float standard_action_scale;
    float graph_y_offset_scale;
    float gravity;
    float unload_distance_margin;
    float forward_speed;
    float far_mario_distance;
    float random_wander_distance;
    float shy_min_distance;
    float shy_angle_scale;
    float quicksand_part_death_depth;
    int blink_min_frames;
    int blink_max_frames;
    int blink_random_frames;
    int regrow_frame;
    int random_turn_step;
    int random_timer_min;
    int random_timer_range;
    int turn_step;
    int steep_slope_degrees;
    int death_delay_base;
    int death_delay_shift;
    int quicksand_depth_to_die;
    int star_drop_height;
} mb64_pokey_config_t;

typedef enum {
    MB64_BULLET_BILL_ACT_IDLE_RESET = 0,
    MB64_BULLET_BILL_ACT_WAIT_FOR_PLAYER = 1,
    MB64_BULLET_BILL_ACT_FIRE = 2,
    MB64_BULLET_BILL_ACT_RESET_AFTER_TIMEOUT = 3,
    MB64_BULLET_BILL_ACT_EXPLODE = 4,
} mb64_bullet_bill_action_t;

typedef enum {
    MB64_PODOBOO_ACT_INIT = 0,
    MB64_PODOBOO_ACT_FALL_INTO_LAVA = 1,
    MB64_PODOBOO_ACT_IDLE_IN_LAVA = 2,
    MB64_PODOBOO_ACT_JUMP = 3,
} mb64_podoboo_action_t;

typedef enum {
    MB64_POKEY_ACT_UNINITIALIZED = 0,
    MB64_POKEY_ACT_WANDER = 1,
    MB64_POKEY_ACT_UNLOAD_PARTS = 2,
} mb64_pokey_action_t;

#define MB64_RENDER_MATERIAL_FENCE    240
#define MB64_RENDER_MATERIAL_BARS     241
#define MB64_RENDER_MATERIAL_BARS_TOP 242
#define MB64_RENDER_MATERIAL_TTC_GRATE_TOP 243

typedef enum {
    MB64_MAT_GRASS = 0,
    MB64_MAT_GRASS_OLD = 1,
    MB64_MAT_CARTOON_GRASS = 2,
    MB64_MAT_DARK_GRASS = 3,
    MB64_MAT_HMC_GRASS = 4,
    MB64_MAT_ORANGE_GRASS = 5,
    MB64_MAT_RED_GRASS = 6,
    MB64_MAT_PURPLE_GRASS = 7,
    MB64_MAT_SAND = 8,
    MB64_MAT_JRB_SAND = 9,
    MB64_MAT_SNOW = 10,
    MB64_MAT_SNOW_OLD = 11,
    MB64_MAT_DIRT = 12,
    MB64_MAT_SANDDIRT = 13,
    MB64_MAT_LIGHTDIRT = 14,
    MB64_MAT_HMC_DIRT = 15,
    MB64_MAT_ROCKY_DIRT = 16,
    MB64_MAT_DIRT_OLD = 17,
    MB64_MAT_WAVY_DIRT = 18,
    MB64_MAT_WAVY_DIRT_BLUE = 19,
    MB64_MAT_SNOWDIRT = 20,
    MB64_MAT_PURPLE_DIRT = 21,
    MB64_MAT_HMC_LAKEGRASS = 22,
    MB64_MAT_STONE = 23,
    MB64_MAT_HMC_STONE = 24,
    MB64_MAT_HMC_MAZEFLOOR = 25,
    MB64_MAT_CCM_ROCK = 26,
    MB64_MAT_TTM_FLOOR = 27,
    MB64_MAT_TTM_ROCK = 28,
    MB64_MAT_COBBLESTONE = 29,
    MB64_MAT_JRB_WALL = 30,
    MB64_MAT_GABBRO = 31,
    MB64_MAT_RHR_STONE = 32,
    MB64_MAT_LAVA_ROCKS = 33,
    MB64_MAT_VOLCANO_WALL = 34,
    MB64_MAT_RHR_BASALT = 35,
    MB64_MAT_OBSIDIAN = 36,
    MB64_MAT_CASTLE_STONE = 37,
    MB64_MAT_JRB_UNDERWATER = 38,
    MB64_MAT_SNOW_ROCK = 39,
    MB64_MAT_ICY_ROCK = 40,
    MB64_MAT_DESERT_STONE = 41,
    MB64_MAT_RHR_OBSIDIAN = 42,
    MB64_MAT_JRB_STONE = 43,
    MB64_MAT_BRICKS = 44,
    MB64_MAT_DESERT_BRICKS = 45,
    MB64_MAT_RHR_BRICK = 46,
    MB64_MAT_HMC_BRICK = 47,
    MB64_MAT_LIGHTBROWN_BRICK = 48,
    MB64_MAT_WDW_BRICK = 49,
    MB64_MAT_TTM_BRICK = 50,
    MB64_MAT_C_BRICK = 51,
    MB64_MAT_BBH_BRICKS = 52,
    MB64_MAT_ROOF_BRICKS = 53,
    MB64_MAT_C_OUTSIDEBRICK = 54,
    MB64_MAT_SNOW_BRICKS = 55,
    MB64_MAT_JRB_BRICKS = 56,
    MB64_MAT_SNOW_TILE_SIDE = 57,
    MB64_MAT_TILESBRICKS = 58,
    MB64_MAT_TILES = 59,
    MB64_MAT_C_TILES = 60,
    MB64_MAT_DESERT_TILES = 61,
    MB64_MAT_VP_BLUETILES = 62,
    MB64_MAT_SNOW_TILES = 63,
    MB64_MAT_JRB_TILETOP = 64,
    MB64_MAT_JRB_TILESIDE = 65,
    MB64_MAT_HMC_TILES = 66,
    MB64_MAT_GRANITE_TILES = 67,
    MB64_MAT_RHR_TILES = 68,
    MB64_MAT_VP_TILES = 69,
    MB64_MAT_DIAMOND_PATTERN = 70,
    MB64_MAT_C_STONETOP = 71,
    MB64_MAT_SNOW_BRICK_TILES = 72,
    MB64_MAT_DESERT_BLOCK = 73,
    MB64_MAT_VP_BLOCK = 74,
    MB64_MAT_BBH_STONE = 75,
    MB64_MAT_BBH_STONE_PATTERN = 76,
    MB64_MAT_PATTERNED_BLOCK = 77,
    MB64_MAT_HMC_SLAB = 78,
    MB64_MAT_RHR_BLOCK = 79,
    MB64_MAT_GRANITE_BLOCK = 80,
    MB64_MAT_C_STONESIDE = 81,
    MB64_MAT_C_PILLAR = 82,
    MB64_MAT_BBH_PILLAR = 83,
    MB64_MAT_RHR_PILLAR = 84,
    MB64_MAT_WOOD = 85,
    MB64_MAT_BBH_WOOD_FLOOR = 86,
    MB64_MAT_BBH_WOOD_WALL = 87,
    MB64_MAT_C_WOOD = 88,
    MB64_MAT_JRB_WOOD = 89,
    MB64_MAT_JRB_SHIPSIDE = 90,
    MB64_MAT_JRB_SHIPTOP = 91,
    MB64_MAT_BBH_HAUNTED_PLANKS = 92,
    MB64_MAT_BBH_ROOF = 93,
    MB64_MAT_SOLID_WOOD = 94,
    MB64_MAT_RHR_WOOD = 95,
    MB64_MAT_BBH_METAL = 96,
    MB64_MAT_JRB_METALSIDE = 97,
    MB64_MAT_JRB_METAL = 98,
    MB64_MAT_C_BASEMENTWALL = 99,
    MB64_MAT_DESERT_TILES2 = 100,
    MB64_MAT_VP_RUSTYBLOCK = 101,
    MB64_MAT_C_CARPET = 102,
    MB64_MAT_C_WALL = 103,
    MB64_MAT_ROOF = 104,
    MB64_MAT_C_ROOF = 105,
    MB64_MAT_SNOW_ROOF = 106,
    MB64_MAT_BBH_WINDOW = 107,
    MB64_MAT_HMC_LIGHT = 108,
    MB64_MAT_VP_CAUTION = 109,
    MB64_MAT_RR_BLOCKS = 110,
    MB64_MAT_STUDDED_TILE = 111,
    MB64_MAT_TTC_BLOCK = 112,
    MB64_MAT_TTC_SIDE = 113,
    MB64_MAT_TTC_WALL = 114,
    MB64_MAT_FLOWERS = 115,
    MB64_MAT_LAVA = 116,
    MB64_MAT_VANILLA_LAVA = 117,
    MB64_MAT_LAVA_OLD = 117,
    MB64_MAT_SERVER_ACID = 118,
    MB64_MAT_BURNING_ICE = 119,
    MB64_MAT_QUICKSAND = 120,
    MB64_MAT_DESERT_SLOWSAND = 121,
    MB64_MAT_VOID = 122,
    MB64_MAT_VP_VOID = 122,
    MB64_MAT_RHR_MESH = 123,
    MB64_MAT_VP_MESH = 124,
    MB64_MAT_HMC_MESH = 125,
    MB64_MAT_BBH_MESH = 126,
    MB64_MAT_PINK_MESH = 127,
    MB64_MAT_TTC_MESH = 128,
    MB64_MAT_ICE = 129,
    MB64_MAT_CRYSTAL = 130,
    MB64_MAT_VP_SCREEN = 131,
    MB64_MAT_RETRO_GROUND = 132,
    MB64_MAT_RETRO_BRICKS = 133,
    MB64_MAT_RETRO_TREETOP = 134,
    MB64_MAT_RETRO_TREEPLAT = 135,
    MB64_MAT_RETRO_BLOCK = 136,
    MB64_MAT_RETRO_BLUEGROUND = 137,
    MB64_MAT_RETRO_BLUEBRICKS = 138,
    MB64_MAT_RETRO_BLUEBLOCK = 139,
    MB64_MAT_RETRO_WHITEBRICK = 140,
    MB64_MAT_RETRO_LAVA = 141,
    MB64_MAT_RETRO_UNDERWATERGROUND = 142,
    MB64_MAT_MC_DIRT = 143,
    MB64_MAT_MC_GRASS = 144,
    MB64_MAT_MC_COBBLESTONE = 145,
    MB64_MAT_MC_STONE = 146,
    MB64_MAT_MC_OAK_LOG_TOP = 147,
    MB64_MAT_MC_OAK_LOG_SIDE = 148,
    MB64_MAT_MC_OAK_LEAVES = 149,
    MB64_MAT_MC_WOOD_PLANKS = 150,
    MB64_MAT_MC_SAND = 151,
    MB64_MAT_MC_BRICKS = 152,
    MB64_MAT_MC_LAVA = 153,
    MB64_MAT_MC_FLOWING_LAVA = 154,
    MB64_MAT_MC_GLASS = 155,
} mb64_material_id_t;

typedef enum {
    MB64_FENCE_NORMAL = 0,
    MB64_FENCE_WOOD2,
    MB64_FENCE_DESERT,
    MB64_FENCE_BARBED,
    MB64_FENCE_RHR,
    MB64_FENCE_HMC,
    MB64_FENCE_CASTLE,
    MB64_FENCE_VIRTUAPLEX,
    MB64_FENCE_BBH,
    MB64_FENCE_JRB,
    MB64_FENCE_SNOW2,
    MB64_FENCE_SNOW,
    MB64_FENCE_RETRO,
    MB64_FENCE_MC,
} mb64_fence_id_t;

typedef enum {
    MB64_BAR_GENERIC = 0,
    MB64_BAR_RHR,
    MB64_BAR_VP,
    MB64_BAR_HMC,
    MB64_BAR_BBH,
    MB64_BAR_LLL,
    MB64_BAR_TTC,
    MB64_BAR_DESERT,
    MB64_BAR_BOB,
    MB64_BAR_RETRO,
    MB64_BAR_MC,
} mb64_bar_id_t;

typedef enum {
    MB64_WATER_DEFAULT = 0,
    MB64_WATER_GREEN,
    MB64_WATER_RETRO,
    MB64_WATER_MC,
} mb64_water_id_t;

typedef struct {
    uint8_t fence;
    uint8_t pole;
    uint8_t bars;
    uint8_t water;
} mb64_theme_special_t;

/**
 * mb64_load() - Parse an .mb64 file.
 *
 * Reads the file at @path, validates the binary structure, and returns a
 * heap-allocated mb64_level_t with fully decoded header, tiles, and objects.
 *
 * Logs progress via MB64_LOG at each processing stage:
 *   [MB64_PARSE]     header fields
 *   [MB64_GEOM_GEN]  tile decode summary
 *   [MB64_OBJ_SPAWN] object decode summary
 *   [MB64_COLLISION] solid-tile collision stats
 *
 * Returns: pointer on success, NULL on any error (logs reason before returning).
 */
mb64_level_t *mb64_load(const char *path);

/**
 * mb64_free() - Release all memory owned by a parsed level.
 *
 * Safe to call with NULL.
 */
void mb64_free(mb64_level_t *level);

/**
 * mb64_music_sequence_from_index() - Convert MB64 music menu index to sequence.
 *
 * MB64 saves level, race, and boss music as indices into its music selector
 * table. This returns the SM64 sequence id from MB64's seq_musicmenu_array, or
 * 0 when the index is outside the authored table.
 */
uint8_t mb64_music_sequence_from_index(uint8_t music_index);

int mb64_build_render_mesh(const mb64_level_t *level, mb64_mesh_t *mesh);
int mb64_build_collision_mesh(const mb64_level_t *level, mb64_mesh_t *mesh);
void mb64_free_render_mesh(mb64_mesh_t *mesh);
uint8_t mb64_tile_has_collision(const mb64_tile_t *tile);
uint8_t mb64_tile_occludes_face(const mb64_level_t *level,
                                const mb64_tile_t *cur,
                                const mb64_tile_t *other,
                                uint8_t direction);
uint8_t mb64_boundary_flags_for_level(const mb64_level_t *level);
uint32_t mb64_build_death_plane_faces(const mb64_level_t *level,
                                      mb64_boundary_face_t out[MB64_DEATH_PLANE_FACE_COUNT]);
uint8_t mb64_resolve_tile_material(const mb64_level_t *level,
                                   const mb64_tile_t *tile,
                                   uint8_t top_face);
int16_t mb64_surface_for_material(uint8_t material);
int16_t mb64_surface_for_tile(const mb64_level_t *level, const mb64_tile_t *tile);
mb64_material_texture_animation_t mb64_texture_animation_for_material(uint8_t material);
mb64_material_texture_animation_t mb64_texture_animation_for_water(const mb64_level_t *level);

/**
 * mb64_find_water_column_top() - MB64's Y-aware stacked-water lookup.
 *
 * @grid_x, @grid_y, and @grid_z are MB64 grid coordinates. The query mirrors
 * src/mb64/collision.c: if the query cell is water, scan upward to the top of
 * the contiguous water column; otherwise scan downward to the nearest water
 * column below. Returns 1 and writes the top water grid Y on success, or 0 when
 * no local water column exists.
 */
int mb64_find_water_column_top(const mb64_level_t *level,
                               int grid_x,
                               int grid_y,
                               int grid_z,
                               int *out_top_grid_y);

/**
 * mb64_theme_specials_for_level() - Return MB64 special material ids.
 *
 * Regular tile materials resolve through mb64_resolve_tile_material(); fences,
 * poles, bars, and water are stored as separate theme-special ids in MB64. This
 * returns those ids using the same built-in/custom-theme lookup that libmb64
 * uses during mesh generation.
 */
const mb64_theme_special_t *mb64_theme_specials_for_level(const mb64_level_t *level);

/**
 * mb64_water_vertex_color() - Return animated water vertex color.
 *
 * The renderer supplies @wave in the 0..15 range for the current frame. libmb64
 * owns the MB64 palette constants and writes RGBA values into @rgba.
 */
void mb64_water_vertex_color(const mb64_level_t *level, uint8_t wave, uint8_t rgba[4]);

/**
 * mb64_woodplat_config() - Return MB64 Wooden Platform behavior constants.
 *
 * These values mirror bhvWoodPlat in Mario Builder 64. Consumers still own
 * engine-specific movement/collision calls, but should source the portable
 * behavior constants here rather than duplicating magic numbers.
 */
const mb64_woodplat_config_t *mb64_woodplat_config(void);

float mb64_woodplat_piece_height(uint8_t bparam);
float mb64_woodplat_stack_height(const uint8_t *bparams, size_t count);
uint8_t mb64_woodplat_should_stack(uint8_t bparam, float nearest_distance);
float mb64_woodplat_water_float_accel(float water_level, float platform_y);
float mb64_woodplat_water_velocity(float current_vel_y, float water_level, float platform_y,
                                   uint8_t mario_on_platform, uint8_t ground_pound_landing);
int mb64_woodplat_death_drop_offset(void);
uint8_t mb64_woodplat_should_use_simple_wall_checks(uint8_t floor_is_conveyor,
                                                    uint8_t floor_object_has_vertical_push,
                                                    uint8_t on_ground);
uint8_t mb64_woodplat_should_die_on_death_barrier(uint8_t has_floor, uint8_t floor_is_death_plane,
                                                  float platform_y, float floor_y);
const mb64_bullet_bill_config_t *mb64_bullet_bill_config(void);
uint8_t mb64_bullet_bill_should_wake(int angle_diff, float distance);
float mb64_bullet_bill_forward_velocity(int timer, float launch_speed);
uint8_t mb64_bullet_bill_should_launch(int timer);
uint8_t mb64_bullet_bill_should_floor_probe(int timer);
uint8_t mb64_bullet_bill_should_rotate_toward_player(float distance);
uint8_t mb64_bullet_bill_should_timeout(int timer);
uint8_t mb64_bullet_bill_should_reset_after_explosion(int timer);
const mb64_reinforced_box_config_t *mb64_reinforced_box_config(void);
uint8_t mb64_reinforced_box_should_clank(int timer);
uint8_t mb64_reinforced_box_should_shake(int timer);
float mb64_reinforced_box_shake_offset(float random_unit);
const mb64_badge_config_t *mb64_badge_config(void);
uint8_t mb64_badge_is_equipped(uint32_t equipped_badges, uint8_t badge_id);
uint8_t mb64_badge_should_collect(uint8_t equipped, uint8_t overlaps_mario, uint8_t mario_levelup_dance);
float mb64_badge_next_collect_scale(float current_scale);
uint8_t mb64_badge_should_delete(float current_scale);
const mb64_podoboo_config_t *mb64_podoboo_config(void);
float mb64_podoboo_launch_velocity(float rest_y, float peak_y);
uint8_t mb64_podoboo_should_reset_idle_timer(float distance_to_mario);
uint8_t mb64_podoboo_should_spawn_warmup_flame(int timer);
uint8_t mb64_podoboo_should_launch(int timer);
const mb64_pokey_config_t *mb64_pokey_config(void);
uint32_t mb64_pokey_alive_flags(uint8_t segment_count);
float mb64_pokey_part_spawn_y(uint8_t part_index);
int mb64_pokey_part_offset_angle(uint8_t part_index, int timer);
float mb64_pokey_part_base_height(float parent_y,
                                  uint8_t alive_parts,
                                  uint8_t part_index,
                                  float bottom_size,
                                  float quicksand_depth);
float mb64_pokey_part_graph_y_offset(float scale_y);
int mb64_pokey_part_death_delay(uint8_t part_index);
uint8_t mb64_pokey_should_shift_part(uint8_t part_index, uint32_t alive_flags);
uint8_t mb64_pokey_should_expand_bottom(float bottom_size,
                                        uint8_t part_index,
                                        uint8_t alive_parts);
uint8_t mb64_pokey_should_spawn_parts(float distance_to_mario, float drawing_distance);
uint8_t mb64_pokey_should_unload(float distance_to_mario, float drawing_distance);
uint8_t mb64_pokey_should_regrow(uint8_t alive_parts, int timer, uint8_t floor_is_instant_quicksand);
int mb64_pokey_target_angle_offset(float distance_to_mario, int angle_to_mario, int move_angle_yaw);
uint8_t mb64_pokey_should_die_in_quicksand(uint8_t part_index,
                                           uint8_t alive_parts,
                                           float quicksand_depth);

#ifdef __cplusplus
}
#endif

#endif /* MB64_H */
