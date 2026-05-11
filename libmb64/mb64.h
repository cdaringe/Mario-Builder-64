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
    uint8_t material;
    uint8_t tile_type;
    uint8_t direction;
    uint8_t is_water;
} mb64_mesh_face_t;

typedef struct {
    mb64_mesh_face_t *faces;
    uint32_t face_count;
    uint32_t solid_tile_count;
    uint32_t water_tile_count;
} mb64_mesh_t;

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

#ifdef __cplusplus
}
#endif

#endif /* MB64_H */
