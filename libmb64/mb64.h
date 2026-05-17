/**
 * mb64.h — Public API for libmb64.
 *
 * Parses Mario Builder 64 (.mb64) level files (big-endian format per
 * assets/kaitai_mb64.yaml) and returns structured C data.
 *
 * Usage:
 *   mb64_level_t *lvl = mb64_load("level.mb64");
 *   if (!lvl) { ... error ... }
 *   // Access all five named fields:
 *   //   lvl->header       — file metadata (author, version, counts, etc.)
 *   //   lvl->tiles        — lvl->header.tile_count decoded tiles
 *   //   lvl->objects      — lvl->header.object_count decoded objects
 *   //   lvl->trajectories — MB64_TRAJ_COUNT waypoints (20 paths × 50 points)
 *   //   lvl->header.theme — theme index (also lvl->header.custom_theme)
 *   mb64_free(lvl);
 */

#ifndef MB64_H
#define MB64_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Total trajectory waypoints stored per level (20 paths × 50 points). */
#define MB64_TRAJ_COUNT 1000

/** Pixel count for the level thumbnail texture (64×64 RGB5A1 pixels). */
#define MB64_PIKTCHER_SIZE 4096

/* ── Trajectory waypoint (comptraj in kaitai) ─────────────────────── */
typedef struct {
    int8_t  t;   /* waypoint type/direction; ≤0 typically marks unused slot */
    uint8_t x, y, z;
} mb64_traj_t;

/* ── Custom theme ─────────────────────────────────────────────────── */
typedef struct {
    uint8_t mats[10];
    uint8_t topmats[10];
    uint8_t topmats_enabled[10];
    uint8_t fence, pole, bars, water;
} mb64_custom_theme_t;

/* ── Level header (level_save_header in kaitai) ──────────────────── */
typedef struct {
    char     file_header[11];              /* 10-byte ASCII magic + NUL terminator */
    uint8_t  version;
    char     author[32];                   /* 31-byte ASCII + NUL terminator */
    uint16_t piktcher[MB64_PIKTCHER_SIZE]; /* 64×64 RGB5A1 thumbnail (offset 42, 8192 bytes) */
    uint8_t costume;
    uint8_t seq[5];
    uint8_t envfx;
    uint8_t theme;             /* built-in theme index */
    uint8_t bg;
    uint8_t boundary_mat;
    uint8_t boundary;
    uint8_t boundary_height;
    uint8_t coinstar;
    uint8_t level_size;        /* 'size' field in kaitai */
    uint8_t waterlevel;
    uint8_t secret;
    uint8_t game;
    uint8_t toolbar[9];
    uint8_t toolbar_params[9];
    uint16_t tile_count;
    uint16_t object_count;
    mb64_custom_theme_t custom_theme;
} mb64_header_t;

/* ── Tile (decoded from packed u32) ───────────────────────────────── */
typedef struct {
    uint32_t raw;         /* original big-endian word */
    uint8_t  x, y, z;    /* grid coordinates (0-63) */
    uint8_t  type;        /* tile type (0-31) */
    uint8_t  mat;         /* material (0-15) */
    uint8_t  rot;         /* rotation (0-3) */
    uint8_t  waterlogged; /* 1 if waterlogged, 0 otherwise */
} mb64_tile_t;

/* ── Object ───────────────────────────────────────────────────────── */
typedef struct {
    uint8_t bparam;
    uint8_t x, y, z;
    uint8_t type;
    uint8_t rot;
    uint8_t imbue;
    /* pad byte skipped */
} mb64_obj_t;

/* ── Parsed level — five first-class fields per scenario 7 ───────── */
typedef struct {
    mb64_header_t  header;                         /* metadata; theme at header.theme */
    mb64_tile_t   *tiles;                          /* header.tile_count entries, heap-alloc */
    mb64_obj_t    *objects;                        /* header.object_count entries, heap-alloc */
    mb64_traj_t    trajectories[MB64_TRAJ_COUNT];  /* 1000 waypoints, always present */
} mb64_level_t;

typedef enum {
    MB64_MESH_FACE_TOP = 0,
    MB64_MESH_FACE_BOTTOM = 1,
    MB64_MESH_FACE_NEG_X = 2,
    MB64_MESH_FACE_POS_X = 3,
    MB64_MESH_FACE_NEG_Z = 4,
    MB64_MESH_FACE_POS_Z = 5,
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
} mb64_mesh_face_t;

typedef struct {
    mb64_mesh_face_t *faces;
    uint32_t face_count;
    uint32_t solid_tile_count;
    uint32_t water_tile_count;
} mb64_mesh_t;

typedef struct {
    uint8_t animated;
    uint8_t tile_size_cmd;
    uint8_t interval;
    uint16_t step_s;
    uint16_t step_t;
} mb64_material_texture_animation_t;

#define MB64_RENDER_MATERIAL_FENCE    240
#define MB64_RENDER_MATERIAL_BARS     241
#define MB64_RENDER_MATERIAL_BARS_TOP 242
#define MB64_RENDER_MATERIAL_TTC_GRATE_TOP 243

#define MB64_OBJECT_FLAG_BILLBOARD (1u << 0)
#define MB64_OBJECT_FLAG_TRAJECTORY (1u << 1)
#define MB64_OBJECT_FLAG_STAR (1u << 2)
#define MB64_OBJECT_FLAG_HAS_DIALOG (1u << 3)
#define MB64_OBJECT_FLAG_IMBUABLE (1u << 4)
#define MB64_OBJECT_FLAG_IMBUABLE_COINS (1u << 5)
#define MB64_OBJECT_FLAG_IMBUABLE_TRIGGER (1u << 6)

#define MB64_OBJECT_OCCUPY_OUTER (1u << 0)
#define MB64_OBJECT_OCCUPY_INNER (1u << 1)
#define MB64_OBJECT_OCCUPY_FULL (MB64_OBJECT_OCCUPY_OUTER | MB64_OBJECT_OCCUPY_INNER)

typedef enum {
    MB64_OBJECT_TYPE_SETTINGS = 0,
    MB64_OBJECT_TYPE_1,
    MB64_OBJECT_TYPE_STAR,
    MB64_OBJECT_TYPE_RED_COIN_STAR,
    MB64_OBJECT_TYPE_GOOMBA,
    MB64_OBJECT_TYPE_BIG_GOOMBA,
    MB64_OBJECT_TYPE_TINY_GOOMBA,
    MB64_OBJECT_TYPE_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_BIG_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_TINY_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_KOOPA,
    MB64_OBJECT_TYPE_COIN,
    MB64_OBJECT_TYPE_GREEN_COIN,
    MB64_OBJECT_TYPE_RED_COIN,
    MB64_OBJECT_TYPE_BLUE_COIN,
    MB64_OBJECT_TYPE_BLUE_COIN_SWITCH,
    MB64_OBJECT_TYPE_NOTEBLOCK,
    MB64_OBJECT_TYPE_BOBOMB,
    MB64_OBJECT_TYPE_CHUCKYA,
    MB64_OBJECT_TYPE_BULLY,
    MB64_OBJECT_TYPE_CHILL_BULLY,
    MB64_OBJECT_TYPE_BULLET_BILL,
    MB64_OBJECT_TYPE_HEAVE_HO,
    MB64_OBJECT_TYPE_MOTOS,
    MB64_OBJECT_TYPE_TREE,
    MB64_OBJECT_TYPE_EXCL_BOX,
    MB64_OBJECT_TYPE_MARIO_SPAWN,
    MB64_OBJECT_TYPE_REX,
    MB64_OBJECT_TYPE_PODOBOO,
    MB64_OBJECT_TYPE_CRABLET,
    MB64_OBJECT_TYPE_HAMMER_BRO,
    MB64_OBJECT_TYPE_FIRE_BRO,
    MB64_OBJECT_TYPE_CHICKEN,
    MB64_OBJECT_TYPE_PHANTASM,
    MB64_OBJECT_TYPE_WARP_PIPE,
    MB64_OBJECT_TYPE_BADGE,
    MB64_OBJECT_TYPE_KING_BOBOMB,
    MB64_OBJECT_TYPE_KING_WHOMP,
    MB64_OBJECT_TYPE_BIG_BOO,
    MB64_OBJECT_TYPE_BIG_BULLY,
    MB64_OBJECT_TYPE_BIG_CHILL_BULLY,
    MB64_OBJECT_TYPE_WIGGLER,
    MB64_OBJECT_TYPE_BOWSER,
    MB64_OBJECT_TYPE_PLATFORM_TRACK,
    MB64_OBJECT_TYPE_PLATFORM_LOOPING,
    MB64_OBJECT_TYPE_BOWLING_BALL,
    MB64_OBJECT_TYPE_KOOPA_THE_QUICK,
    MB64_OBJECT_TYPE_PURPLE_SWITCH,
    MB64_OBJECT_TYPE_TIMED_BOX,
    MB64_OBJECT_TYPE_RECOVERY_HEART,
    MB64_OBJECT_TYPE_TEST_MARIO,
    MB64_OBJECT_TYPE_THWOMP,
    MB64_OBJECT_TYPE_WHOMP,
    MB64_OBJECT_TYPE_GRINDEL,
    MB64_OBJECT_TYPE_LAKITU,
    MB64_OBJECT_TYPE_FLY_GUY,
    MB64_OBJECT_TYPE_SNUFIT,
    MB64_OBJECT_TYPE_AMP,
    MB64_OBJECT_TYPE_BOO,
    MB64_OBJECT_TYPE_MR_I,
    MB64_OBJECT_TYPE_SCUTTLEBUG,
    MB64_OBJECT_TYPE_BOWSER_BOMB,
    MB64_OBJECT_TYPE_FIRE_SPINNER,
    MB64_OBJECT_TYPE_COIN_FORMATION,
    MB64_OBJECT_TYPE_RED_FLAME,
    MB64_OBJECT_TYPE_BLUE_FLAME,
    MB64_OBJECT_TYPE_FIRE_SPITTER,
    MB64_OBJECT_TYPE_FLAMETHROWER,
    MB64_OBJECT_TYPE_SPINDRIFT,
    MB64_OBJECT_TYPE_MR_BLIZZARD,
    MB64_OBJECT_TYPE_MONEYBAG,
    MB64_OBJECT_TYPE_SKEETER,
    MB64_OBJECT_TYPE_POKEY,
    MB64_OBJECT_TYPE_BBOX_SMALL,
    MB64_OBJECT_TYPE_BBOX_NORMAL,
    MB64_OBJECT_TYPE_BBOX_CRAZY,
    MB64_OBJECT_TYPE_DIAMOND,
    MB64_OBJECT_TYPE_SIGN,
    MB64_OBJECT_TYPE_BUDDY,
    MB64_OBJECT_TYPE_BUTTON,
    MB64_OBJECT_TYPE_ON_OFF_BLOCK,
    MB64_OBJECT_TYPE_WOODPLAT,
    MB64_OBJECT_TYPE_RFBOX,
    MB64_OBJECT_TYPE_CULL_PREVIEW,
    MB64_OBJECT_TYPE_SHOWRUNNER,
    MB64_OBJECT_TYPE_CROWBAR,
    MB64_OBJECT_TYPE_MASK,
    MB64_OBJECT_TYPE_TOAD,
    MB64_OBJECT_TYPE_TUXIE,
    MB64_OBJECT_TYPE_UKIKI,
    MB64_OBJECT_TYPE_MOLEMAN,
    MB64_OBJECT_TYPE_COBIE,
    MB64_OBJECT_TYPE_CONVEYOR,
    MB64_OBJECT_TYPE_TIMEDBLOCK,
    MB64_OBJECT_TYPE_TRIGGER,
    MB64_OBJECT_TYPE_TRIGGER_STAR,
    MB64_OBJECT_TYPE_COUNT,
} mb64_object_type_t;

#define MB64_OBJECT_TYPE_SPAWN MB64_OBJECT_TYPE_MARIO_SPAWN
#define MB64_OBJECT_TYPE_TIMED_BLOCK MB64_OBJECT_TYPE_TIMEDBLOCK

typedef struct {
    uint8_t type;
    const char *type_token;
    const char *name;
    const char *model_token;
    const char *behavior_token;
    const char *display_func_token;
    int16_t y_offset;
    uint8_t flags;
    uint8_t occupy;
    uint8_t num_coins;
    uint8_t num_extra_objects;
    float scale;
    const char *sound_token;
} mb64_object_spec_t;

/**
 * mb64_load() — Parse an .mb64 file.
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
 * mb64_free() — Release all memory owned by a parsed level.
 *
 * Safe to call with NULL.
 */
void mb64_free(mb64_level_t *level);

int mb64_build_render_mesh(const mb64_level_t *level, mb64_mesh_t *mesh);
void mb64_free_render_mesh(mb64_mesh_t *mesh);
uint8_t mb64_tile_has_collision(const mb64_tile_t *tile);
uint8_t mb64_resolve_tile_material(const mb64_level_t *level,
                                   const mb64_tile_t *tile,
                                   uint8_t top_face);
int16_t mb64_surface_for_material(uint8_t material);
int16_t mb64_surface_for_tile(const mb64_level_t *level, const mb64_tile_t *tile);
mb64_material_texture_animation_t mb64_texture_animation_for_material(uint8_t material);
mb64_material_texture_animation_t mb64_texture_animation_for_water(const mb64_level_t *level);
const mb64_object_spec_t *mb64_object_spec_for_type(uint8_t type);
size_t mb64_object_spec_count(void);

#ifdef __cplusplus
}
#endif

#endif /* MB64_H */
