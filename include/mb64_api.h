/**
 * Mario Builder 64 Public API Header
 * 
 * This header provides a complete interface for integrating Mario Builder 64
 * level loading and playback functionality into external projects like SM64CoopDX.
 * 
 * ALL necessary struct definitions, constants, and function declarations are
 * included here - integrators should NOT need to redefine any structures.
 * 
 * Usage in integrator projects:
 * 1. #include "mb64_api.h" 
 * 2. Link with libmb64-levels.a
 * 3. Call mb64_* functions for level loading/playback
 * 
 * Requirements:
 * - Host must provide N64/SM64 compatibility types (ultra64.h, PR/gbi.h, etc.)
 * - Host must define standard math constants (M_PI, MAX, MIN)
 * - Host must provide basic N64 types (OSThread, Gfx, Mtx, Vp, Vtx, Vec3f, etc.)
 */

#pragma once
#ifndef MB64_API_H
#define MB64_API_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Core Dependencies and Types
//=============================================================================

// Only define types if not already provided by the host
#ifndef ULTRA64_H
// Basic type definitions that host should provide
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef signed char    s8;
typedef signed short   s16;
typedef signed int     s32;
typedef signed long long s64;
typedef float          f32;
typedef double         f64;

// Vector types (host should provide these)
typedef s16 Vec3s[3];
typedef f32 Vec3f[3];
#endif

// Forward declarations for N64/SM64 types (host must define)
#ifndef MB64_FORWARD_DECLS_DEFINED
#define MB64_FORWARD_DECLS_DEFINED

// These match the host's definitions
typedef struct Gfx Gfx;
typedef struct Vtx Vtx;
typedef union BehaviorScript BehaviorScript;
typedef union LevelScript LevelScript;
typedef u16 ModelID16;
typedef u32 TerrainData;
typedef struct Animation Animation;

// Special handle for trajectory - different systems use different definitions
#ifndef Trajectory
typedef union Trajectory Trajectory;
#endif

// File info type
#ifndef FILINFO
typedef struct FILINFO FILINFO;
#endif

#endif // MB64_FORWARD_DECLS_DEFINED

//=============================================================================
// Constants and Limits
//=============================================================================

#define MB64_VERSION 1
#define MAX_FILE_NAME_SIZE 41
#define MAX_USERNAME_SIZE 31

#define MB64_TILE_POOL_SIZE 20000
#define MB64_GFX_SIZE 20000
#define MB64_VTX_SIZE 50000
#define MB64_MAX_OBJS 512
#define MB64_MAX_TRAJECTORIES 20
#define MB64_TRAJECTORY_LENGTH 50

#define TILE_SIZE 256
#define NUM_MATERIALS_PER_THEME 10

// Object type flags
#define OBJ_TYPE_BILLBOARD         (1 << 0)
#define OBJ_TYPE_TRAJECTORY        (1 << 1)
#define OBJ_TYPE_STAR             (1 << 2)
#define OBJ_TYPE_HAS_DIALOG       (1 << 3)
#define OBJ_TYPE_IMBUABLE         (1 << 4)
#define OBJ_TYPE_IMBUABLE_COINS   (1 << 5)
#define OBJ_TYPE_IMBUABLE_TRIGGER (1 << 6)

#define OBJ_OCCUPY_OUTER  (1 << 0)
#define OBJ_OCCUPY_INNER  (1 << 1)
#define OBJ_OCCUPY_FULL   (OBJ_OCCUPY_OUTER | OBJ_OCCUPY_INNER)

// Boundary flags
#define MB64_BOUNDARY_INNER_FLOOR (1 << 0)
#define MB64_BOUNDARY_OUTER_FLOOR (1 << 1)
#define MB64_BOUNDARY_INNER_WALLS (1 << 2)
#define MB64_BOUNDARY_OUTER_WALLS (1 << 3)
#define MB64_BOUNDARY_CEILING     (1 << 4)

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * Represents a single terrain polygon (triangle or quad)
 */
struct mb64_terrain_poly {
    s8 vtx[4][3];           // Vertex positions
    u8 faceDir;             // Face direction
    u8 faceshape;           // Face shape type
    u8 growthType;          // Growth/connection type
    s8 (*altuvs)[4][2];     // Alternative UV coordinates
};

/**
 * Boundary quad definition for level boundaries
 */
struct mb64_boundary_quad {
    s8 vtx[4][3];   // Vertex positions
    s8 u[2];        // U texture coordinates
    s8 v[2];        // V texture coordinates  
    u8 uYScale;     // Scale U by Y instead of width
    u8 vYScale;     // Scale V by Y instead of width
    u8 flipUvs;     // Flip UV coordinates
};

/**
 * Terrain geometry definition (collection of quads and triangles)
 */
struct mb64_terrain {
    u8 numQuads;                        // Number of quad polygons
    u8 numTris;                         // Number of triangle polygons
    struct mb64_terrain_poly * quads;   // Array of quad polygons
    struct mb64_terrain_poly * tris;    // Array of triangle polygons
};

/**
 * Compact tile representation in the grid
 * Uses bitfields for memory efficiency
 */
struct mb64_tile {
    u32 x:6, y:6, z:6, type:5, mat:4, rot:2, waterlogged:1;
};

/**
 * Compact object representation
 */
struct mb64_obj {
    u8 bparam;  // Behavior parameter
    u8 x;       // X position in grid
    u8 y;       // Y position in grid
    u8 z;       // Z position in grid
    u8 type;    // Object type ID
    u8 rot;     // Rotation
    u8 imbue;   // Imbue type (power-ups, etc.)
    u8 pad;     // Padding
};

/**
 * Grid cell object representation (more compact)
 */
struct mb64_grid_obj {
    u16 type:5, mat:4, rot:2, waterlogged:1;
};

/**
 * Material types for rendering
 */
enum mb64_mat_types {
    MAT_OPAQUE,         // Opaque materials (for culling)
    MAT_DECAL,          // Decal material (VP screen when used as block)
    MAT_CUTOUT,         // Alpha cutout material
    MAT_CUTOUT_NOCULL,  // Alpha cutout without culling
    MAT_TRANSPARENT,    // Transparent material
    MAT_SCREEN,         // Screen material override
};

/**
 * Material definition with graphics and collision
 */
struct mb64_material {
    Gfx *gfx;       // Graphics data
    u8 type;        // Material type (from mb64_mat_types)
    u8 vertical;    // Vertical orientation flag
    TerrainData col;// Collision data
    char *name;     // Material name (for Custom Theme menu)
};

/**
 * Top material with optional side decal
 */
struct mb64_topmaterial {
    u8 mat;         // Material ID
    Gfx *decaltex;  // Decal texture
};

/**
 * Full tile material definition
 */
struct mb64_tilemat_def {
    u8 mat;     // Main material ID
    u8 topmat;  // Top material ID  
    char *name; // Material name
};

/**
 * Theme definition with materials and special elements
 */
struct mb64_theme {
    struct mb64_tilemat_def mats[NUM_MATERIALS_PER_THEME]; // Theme materials
    u8 fence;   // Fence material ID
    u8 pole;    // Pole material ID
    u8 bars;    // Bars material ID
    u8 water;   // Water material ID
};

/**
 * Terrain information for UI/menu display
 */
struct mb64_terrain_info {
    char *name;                     // Display name
    Gfx *button;                    // Button graphics
    struct mb64_terrain *terrain;   // Terrain geometry
};

/**
 * Level template definition
 */
struct mb64_template {
    u8 music[2];        // Music tracks [vanilla, btcm]
    u32 envfx:3;        // Environment effects
    u32 bg:4;           // Background type
    u32 theme:4;        // Theme ID
    u32 boundaryMat:4;  // Boundary material
    u32 boundaryHeight:6; // Boundary height
    u32 boundary:3;     // Boundary type
    u32 water:6;        // Water level
    u32 spawnHeight:6;  // Spawn height
    u32 platform:1;     // Spawn platform flag
    u32 platformmat:4;  // Platform material
};

/**
 * Custom theme configuration
 */
struct mb64_custom_theme {
    u8 mats[NUM_MATERIALS_PER_THEME];        // Material IDs
    u8 topmats[NUM_MATERIALS_PER_THEME];     // Top material IDs
    u8 topmatsEnabled[NUM_MATERIALS_PER_THEME]; // Top material enable flags
    u8 fence;   // Fence material
    u8 pole;    // Pole material
    u8 bars;    // Bars material
    u8 water;   // Water material
};

/**
 * Compressed trajectory point
 */
struct mb64_comptraj {
    s8 t;   // Time/progress
    u8 x;   // X position
    u8 y;   // Y position
    u8 z;   // Z position
};

/**
 * Object information for editor/spawning
 */
struct mb64_object_info {
    char *name;                         // Object display name
    Gfx *btn;                          // Button graphics
    const BehaviorScript *behavior;     // Behavior script
    f32 y_offset;                      // Y offset for placement
    u16 model_id;                      // 3D model ID
    u8 flags;                          // Object flags (OBJ_TYPE_*)
    u8 occupy;                         // Occupation flags (OBJ_OCCUPY_*)
    u8 numCoins;                       // Number of coins if imbuable
    u8 numExtraObjects;                // Extra objects spawned
    f32 scale;                         // Object scale
    const struct Animation *const *anim; // Animation data
    void (*disp_func)(s32);            // Display function
    u32 soundBits;                     // Sound effect bits
};

/**
 * Imbue model data (power-ups, collectibles)
 */
struct imbue_model {
    s16 model;      // Model ID
    u8 billboarded:1; // Billboard flag
    u8 doShrink:1;    // Shrink animation flag
    u8 doMove:1;      // Movement flag
    f32 scale;      // Model scale
    s16 spin;       // Spin rate
};

/**
 * Imbue data definition
 */
struct ImbueData {
    u32 coins;  // Coin value
    u32 model;  // Model ID
    u32 color;  // Color value
};

/**
 * Exclamation box contents definition
 */
struct ExclamationBoxContents {
    u8 behParams;                   // Behavior parameters
    ModelID16 model;                // Model ID
    const BehaviorScript *behavior; // Behavior script
    u8 animState;                   // Animation state
    u8 doRespawn;                   // Respawn flag
    u8 numCoins;                    // Coin count
};

/**
 * Main level save header structure
 * 
 * IMPORTANT: The first members (file_header, version, author, piktcher)
 * must always remain the same across versions for compatibility.
 */
struct mb64_level_save_header {
    char file_header[10];           // File magic/identifier
    u8 version;                     // Format version
    char author[MAX_USERNAME_SIZE]; // Level author name
    u16 piktcher[64][64];          // Level thumbnail image

    // Level configuration options
    u8 costume;         // Mario costume
    u8 seq[5];          // Music sequence settings
    u8 envfx;           // Environment effects
    u8 theme;           // Visual theme
    u8 bg;              // Background type
    u8 boundary_mat;    // Boundary material
    u8 boundary;        // Boundary type
    u8 boundary_height; // Boundary height
    u8 coinstar;        // Coin star settings
    u8 size;            // Level size
    u8 waterlevel;      // Water level
    u8 secret;          // Secret settings
    u8 game;            // Game mode

    u8 toolbar[9];       // Toolbar configuration
    u8 toolbar_params[9]; // Toolbar parameters
    u16 tile_count;      // Number of tiles in level
    u16 object_count;    // Number of objects in level

    struct mb64_custom_theme custom_theme; // Custom theme data
    
    // Trajectory data for moving platforms/objects
    struct mb64_comptraj trajectories[MB64_MAX_TRAJECTORIES][MB64_TRAJECTORY_LENGTH];

    u64 pad; // Padding for alignment
};

//=============================================================================
// Enumerations
//=============================================================================

/**
 * Tile types for terrain generation
 */
enum {
    TILE_TYPE_EMPTY,
    // Flippable tiles
    TILE_TYPE_SLOPE = 2,
    TILE_TYPE_DSLOPE,
    TILE_TYPE_SLAB,
    TILE_TYPE_DSLAB,
    TILE_TYPE_CORNER,
    TILE_TYPE_DCORNER,
    TILE_TYPE_ICORNER,
    TILE_TYPE_DICORNER,
    TILE_TYPE_SCORNER,
    TILE_TYPE_DSCORNER,
    TILE_TYPE_ISCORNER,
    TILE_TYPE_DISCORNER,
    TILE_TYPE_UGENTLE,
    TILE_TYPE_DUGENTLE,
    TILE_TYPE_LGENTLE,
    TILE_TYPE_DLGENTLE,

    TILE_END_OF_FLIPPABLE,
    TILE_TYPE_BLOCK = TILE_END_OF_FLIPPABLE,
    TILE_TYPE_SSLOPE,
    TILE_TYPE_SSLAB,
    TILE_TYPE_CULL,
    TILE_TYPE_TROLL,
    TILE_TYPE_FENCE,
    TILE_TYPE_POLE,
    TILE_TYPE_BARS,
    TILE_TYPE_WATER,
};

/**
 * MB64 operational modes
 */
enum {
    MB64_MODE_PLAY,         // Playing a level
    MB64_MODE_MAKE,         // Level editor mode  
    MB64_MODE_UNINITIALIZED // Not initialized
};

/**
 * Level action types
 */
enum {
    MB64_LA_PLAY_LEVELS,    // Playing levels
    MB64_LA_BUILD,          // Building/editing
    MB64_LA_TEST_LEVEL,     // Testing a level
};

/**
 * Visual themes available
 */
enum mb64_themes {
    MB64_THEME_GENERIC,     // Generic/default theme
    MB64_THEME_SSL,         // Shifting Sand Land
    MB64_THEME_RHR,         // Rainbow Ride  
    MB64_THEME_HMC,         // Hazy Maze Cave
    MB64_THEME_CASTLE,      // Princess's Castle
    MB64_THEME_VIRTUAPLEX,  // Virtuaplex
    MB64_THEME_SNOW,        // Snow theme
    MB64_THEME_BBH,         // Big Boo's Haunt
    MB64_THEME_JRB,         // Jolly Roger Bay
    MB64_THEME_RETRO,       // Retro/classic theme
    MB64_THEME_CUSTOM,      // Custom user theme
    MB64_THEME_MC,          // Minecraft theme
};

//=============================================================================
// Public API Functions
//=============================================================================

/**
 * Level Loading Functions
 */

/**
 * Load a Mario Builder 64 level from file
 * @param filename Path to the .mb64 level file
 * @return 0 on success, -1 on failure
 */
int mb64_load_level_data(const char* filename);

/**
 * Generate objects and geometry from loaded level data
 * Call this after mb64_load_level_data() to spawn level content
 * @return 0 on success, negative on failure
 */
int mb64_generate_level_objects(void);

/**
 * Data Access Functions
 */

/**
 * Get the number of objects in the current level
 * @return Number of objects
 */
u16 mb64_get_object_count(void);

/**
 * Get the number of tiles in the current level  
 * @return Number of tiles
 */
u16 mb64_get_tile_count(void);

/**
 * Get pointer to object data array
 * @return Pointer to mb64_obj array with mb64_get_object_count() elements
 */
struct mb64_obj* mb64_get_object_data(void);

/**
 * Get pointer to tile data array
 * @return Pointer to mb64_tile array with mb64_get_tile_count() elements
 */
struct mb64_tile* mb64_get_tile_data(void);

/**
 * Get pointer to level save header data
 * @return Pointer to mb64_level_save_header structure
 */
struct mb64_level_save_header* mb64_get_save_data(void);

/**
 * Get pointer to 3D grid data
 * @return Pointer to 64x64x64 grid of mb64_grid_obj
 */
struct mb64_grid_obj* mb64_get_grid_data(void);

/**
 * Get maximum number of stars in current level
 * @return Maximum stars available
 */
u8 mb64_get_stars_max(void);

//=============================================================================
// Utility Macros
//=============================================================================

/**
 * Convert grid coordinates to world position
 */
#define GRID_TO_POS(gridx) ((gridx) * TILE_SIZE - (32 * TILE_SIZE) + TILE_SIZE/2)

/**
 * Convert world position to grid coordinates
 */
#define POS_TO_GRID(pos) (((pos) + (32 * TILE_SIZE) - TILE_SIZE/2) / TILE_SIZE)

#ifdef __cplusplus
}
#endif

#endif // MB64_API_H