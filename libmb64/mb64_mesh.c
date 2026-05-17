#include "mb64.h"

#include <stdlib.h>
#include <string.h>

#define MB64_GRID_SIZE 64
#define MB64_GRID_CENTRE 32
#define MB64_TILE_SUBUNITS 16
#define MB64_TILE_CULL 21
#define MB64_TILE_WATER 26
#define MB64_THEME_CUSTOM 10
#define MB64_MATERIAL_SLOT_COUNT 10
#define PACK_TILESIZE(w, d) (((w) << 2) + (d))

#define TILE_TYPE_SLOPE 2
#define TILE_TYPE_DSLOPE 3
#define TILE_TYPE_SLAB 4
#define TILE_TYPE_DSLAB 5
#define TILE_TYPE_CORNER 6
#define TILE_TYPE_DCORNER 7
#define TILE_TYPE_ICORNER 8
#define TILE_TYPE_DICORNER 9
#define TILE_TYPE_SCORNER 10
#define TILE_TYPE_DSCORNER 11
#define TILE_TYPE_ISCORNER 12
#define TILE_TYPE_DISCORNER 13
#define TILE_TYPE_UGENTLE 14
#define TILE_TYPE_DUGENTLE 15
#define TILE_TYPE_LGENTLE 16
#define TILE_TYPE_DLGENTLE 17
#define TILE_TYPE_BLOCK 18
#define TILE_TYPE_SSLOPE 19
#define TILE_TYPE_SSLAB 20
#define TILE_TYPE_TROLL 22
#define TILE_TYPE_FENCE 23
#define TILE_TYPE_POLE 24
#define TILE_TYPE_BARS 25

enum mb64_material_id {
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
    MB64_MAT_COBBLESTONE = 29,
    MB64_MAT_JRB_WALL = 30,
    MB64_MAT_RHR_STONE = 32,
    MB64_MAT_RHR_BASALT = 35,
    MB64_MAT_JRB_UNDERWATER = 38,
    MB64_MAT_SNOW_ROCK = 39,
    MB64_MAT_DESERT_STONE = 41,
    MB64_MAT_RHR_OBSIDIAN = 42,
    MB64_MAT_JRB_STONE = 43,
    MB64_MAT_BRICKS = 44,
    MB64_MAT_DESERT_BRICKS = 45,
    MB64_MAT_RHR_BRICK = 46,
    MB64_MAT_HMC_BRICK = 47,
    MB64_MAT_C_BRICK = 51,
    MB64_MAT_BBH_BRICKS = 52,
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
    MB64_MAT_RHR_TILES = 68,
    MB64_MAT_VP_TILES = 69,
    MB64_MAT_C_STONETOP = 71,
    MB64_MAT_SNOW_BRICK_TILES = 72,
    MB64_MAT_DESERT_BLOCK = 73,
    MB64_MAT_VP_BLOCK = 74,
    MB64_MAT_BBH_STONE = 75,
    MB64_MAT_BBH_STONE_PATTERN = 76,
    MB64_MAT_HMC_SLAB = 78,
    MB64_MAT_RHR_BLOCK = 79,
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
    MB64_MAT_LAVA = 116,
    MB64_MAT_VANILLA_LAVA = 117,
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
};

enum mb64_surface_id {
    MB64_SURFACE_DEFAULT = 0x0000,
    MB64_SURFACE_BURNING = 0x0001,
    MB64_SURFACE_DEEP_QUICKSAND = 0x0022,
    MB64_SURFACE_INSTANT_QUICKSAND = 0x0023,
};

typedef struct {
    uint8_t side;
    uint8_t top;
} mb64_material_def_t;

typedef struct {
    uint8_t fence;
    uint8_t pole;
    uint8_t bars;
    uint8_t water;
} mb64_theme_special_t;

enum mb64_water_id {
    MB64_WATER_DEFAULT = 0,
    MB64_WATER_GREEN = 1,
    MB64_WATER_RETRO = 2,
    MB64_WATER_MC = 3,
};

static const mb64_material_def_t s_theme_materials[][MB64_MATERIAL_SLOT_COUNT] = {
    {{MB64_MAT_DIRT, MB64_MAT_GRASS}, {MB64_MAT_BRICKS, MB64_MAT_BRICKS}, {MB64_MAT_COBBLESTONE, MB64_MAT_STONE}, {MB64_MAT_TILESBRICKS, MB64_MAT_TILES}, {MB64_MAT_ROOF, MB64_MAT_ROOF}, {MB64_MAT_WOOD, MB64_MAT_WOOD}, {MB64_MAT_SANDDIRT, MB64_MAT_SAND}, {MB64_MAT_SNOWDIRT, MB64_MAT_SNOW}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND}},
    {{MB64_MAT_SANDDIRT, MB64_MAT_SAND}, {MB64_MAT_DESERT_BRICKS, MB64_MAT_DESERT_BRICKS}, {MB64_MAT_DESERT_STONE, MB64_MAT_DESERT_STONE}, {MB64_MAT_DESERT_TILES, MB64_MAT_DESERT_TILES}, {MB64_MAT_DESERT_BLOCK, MB64_MAT_DESERT_BLOCK}, {MB64_MAT_DESERT_SLOWSAND, MB64_MAT_DESERT_SLOWSAND}, {MB64_MAT_DESERT_BRICKS, MB64_MAT_DESERT_TILES2}, {MB64_MAT_DIRT, MB64_MAT_GRASS}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND}},
    {{MB64_MAT_RHR_STONE, MB64_MAT_RHR_OBSIDIAN}, {MB64_MAT_RHR_BRICK, MB64_MAT_RHR_OBSIDIAN}, {MB64_MAT_RHR_BASALT, MB64_MAT_RHR_BASALT}, {MB64_MAT_RHR_TILES, MB64_MAT_RHR_TILES}, {MB64_MAT_RHR_BLOCK, MB64_MAT_RHR_BLOCK}, {MB64_MAT_RHR_WOOD, MB64_MAT_RHR_WOOD}, {MB64_MAT_RHR_PILLAR, MB64_MAT_RHR_TILES}, {MB64_MAT_RHR_MESH, MB64_MAT_RHR_MESH}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_SERVER_ACID, MB64_MAT_SERVER_ACID}},
    {{MB64_MAT_HMC_DIRT, MB64_MAT_HMC_GRASS}, {MB64_MAT_HMC_BRICK, MB64_MAT_HMC_MAZEFLOOR}, {MB64_MAT_HMC_STONE, MB64_MAT_HMC_STONE}, {MB64_MAT_HMC_SLAB, MB64_MAT_HMC_TILES}, {MB64_MAT_HMC_BRICK, MB64_MAT_HMC_GRASS}, {MB64_MAT_HMC_LAKEGRASS, MB64_MAT_HMC_GRASS}, {MB64_MAT_HMC_LIGHT, MB64_MAT_HMC_LIGHT}, {MB64_MAT_HMC_MESH, MB64_MAT_HMC_MESH}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND}},
    {{MB64_MAT_C_WOOD, MB64_MAT_C_TILES}, {MB64_MAT_C_BRICK, MB64_MAT_C_TILES}, {MB64_MAT_C_STONESIDE, MB64_MAT_C_STONETOP}, {MB64_MAT_C_WOOD, MB64_MAT_C_CARPET}, {MB64_MAT_C_ROOF, MB64_MAT_C_ROOF}, {MB64_MAT_C_WALL, MB64_MAT_C_WALL}, {MB64_MAT_C_PILLAR, MB64_MAT_C_STONETOP}, {MB64_MAT_C_BASEMENTWALL, MB64_MAT_C_BASEMENTWALL}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_C_OUTSIDEBRICK, MB64_MAT_C_OUTSIDEBRICK}},
    {{MB64_MAT_VP_BLOCK, MB64_MAT_VP_BLOCK}, {MB64_MAT_VP_TILES, MB64_MAT_VP_TILES}, {MB64_MAT_DIRT, MB64_MAT_GRASS}, {MB64_MAT_VP_TILES, MB64_MAT_VP_BLUETILES}, {MB64_MAT_VP_RUSTYBLOCK, MB64_MAT_VP_RUSTYBLOCK}, {MB64_MAT_VP_SCREEN, MB64_MAT_VP_SCREEN}, {MB64_MAT_VP_CAUTION, MB64_MAT_VP_CAUTION}, {MB64_MAT_VP_BLOCK, MB64_MAT_SNOW}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_VP_VOID, MB64_MAT_VP_VOID}},
    {{MB64_MAT_SNOWDIRT, MB64_MAT_SNOW}, {MB64_MAT_SNOW_BRICKS, MB64_MAT_SNOW_BRICK_TILES}, {MB64_MAT_SNOW_ROCK, MB64_MAT_SNOW_ROCK}, {MB64_MAT_SNOW_TILE_SIDE, MB64_MAT_SNOW_TILES}, {MB64_MAT_SNOW_ROOF, MB64_MAT_SNOW_ROOF}, {MB64_MAT_WOOD, MB64_MAT_WOOD}, {MB64_MAT_CRYSTAL, MB64_MAT_CRYSTAL}, {MB64_MAT_ICE, MB64_MAT_ICE}, {MB64_MAT_BURNING_ICE, MB64_MAT_BURNING_ICE}, {MB64_MAT_LAVA, MB64_MAT_LAVA}},
    {{MB64_MAT_BBH_BRICKS, MB64_MAT_BBH_STONE}, {MB64_MAT_BBH_HAUNTED_PLANKS, MB64_MAT_BBH_HAUNTED_PLANKS}, {MB64_MAT_BBH_STONE_PATTERN, MB64_MAT_BBH_WOOD_FLOOR}, {MB64_MAT_BBH_BRICKS, MB64_MAT_BBH_METAL}, {MB64_MAT_BBH_ROOF, MB64_MAT_BBH_ROOF}, {MB64_MAT_BBH_WOOD_WALL, MB64_MAT_BBH_WOOD_WALL}, {MB64_MAT_BBH_STONE, MB64_MAT_BBH_STONE}, {MB64_MAT_BBH_PILLAR, MB64_MAT_BBH_STONE}, {MB64_MAT_LAVA, MB64_MAT_LAVA}, {MB64_MAT_BBH_WINDOW, MB64_MAT_BBH_WINDOW}},
    {{MB64_MAT_JRB_STONE, MB64_MAT_JRB_SAND}, {MB64_MAT_JRB_BRICKS, MB64_MAT_JRB_BRICKS}, {MB64_MAT_JRB_UNDERWATER, MB64_MAT_JRB_UNDERWATER}, {MB64_MAT_JRB_TILESIDE, MB64_MAT_JRB_TILETOP}, {MB64_MAT_JRB_SHIPSIDE, MB64_MAT_JRB_SHIPTOP}, {MB64_MAT_JRB_METAL, MB64_MAT_JRB_WOOD}, {MB64_MAT_JRB_METALSIDE, MB64_MAT_JRB_METAL}, {MB64_MAT_HMC_MESH, MB64_MAT_HMC_MESH}, {MB64_MAT_JRB_WALL, MB64_MAT_JRB_WALL}, {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND}},
    {{MB64_MAT_RETRO_GROUND, MB64_MAT_RETRO_GROUND}, {MB64_MAT_RETRO_BRICKS, MB64_MAT_RETRO_BRICKS}, {MB64_MAT_RETRO_TREEPLAT, MB64_MAT_RETRO_TREETOP}, {MB64_MAT_RETRO_BLOCK, MB64_MAT_RETRO_BLOCK}, {MB64_MAT_RETRO_BLUEGROUND, MB64_MAT_RETRO_BLUEGROUND}, {MB64_MAT_RETRO_BLUEBRICKS, MB64_MAT_RETRO_BLUEBRICKS}, {MB64_MAT_RETRO_BLUEBLOCK, MB64_MAT_RETRO_BLUEBLOCK}, {MB64_MAT_RETRO_WHITEBRICK, MB64_MAT_RETRO_WHITEBRICK}, {MB64_MAT_RETRO_LAVA, MB64_MAT_RETRO_LAVA}, {MB64_MAT_RETRO_UNDERWATERGROUND, MB64_MAT_RETRO_UNDERWATERGROUND}},
    {{0}},
    {{MB64_MAT_MC_DIRT, MB64_MAT_MC_GRASS}, {MB64_MAT_MC_COBBLESTONE, MB64_MAT_MC_COBBLESTONE}, {MB64_MAT_MC_STONE, MB64_MAT_MC_STONE}, {MB64_MAT_MC_OAK_LOG_SIDE, MB64_MAT_MC_OAK_LOG_TOP}, {MB64_MAT_MC_OAK_LEAVES, MB64_MAT_MC_OAK_LEAVES}, {MB64_MAT_MC_WOOD_PLANKS, MB64_MAT_MC_WOOD_PLANKS}, {MB64_MAT_MC_SAND, MB64_MAT_MC_SAND}, {MB64_MAT_MC_BRICKS, MB64_MAT_MC_BRICKS}, {MB64_MAT_MC_FLOWING_LAVA, MB64_MAT_MC_LAVA}, {MB64_MAT_MC_GLASS, MB64_MAT_MC_GLASS}},
};

static const mb64_theme_special_t s_theme_specials[] = {
    {0, MB64_MAT_STONE, 0, 0},
    {2, MB64_MAT_DESERT_TILES2, 7, 1},
    {4, MB64_MAT_RHR_PILLAR, 4, 0},
    {5, MB64_MAT_HMC_LAKEGRASS, 5, 1},
    {6, MB64_MAT_C_STONESIDE, 7, 0},
    {7, MB64_MAT_VP_CAUTION, 7, 0},
    {11, MB64_MAT_SNOW_TILE_SIDE, 0, 0},
    {8, MB64_MAT_BBH_BRICKS, 8, 0},
    {9, MB64_MAT_VP_CAUTION, 5, 0},
    {12, MB64_MAT_RETRO_BRICKS, 12, 2},
    {0, 0, 0, 0},
    {13, MB64_MAT_MC_OAK_LOG_SIDE, 10, 3},
};

static uint8_t s_solid_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];
static uint8_t s_water_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];

static const int8_t s_fence_alt_uvs[4][2] = {
    {32, 16},
    {32,  0},
    { 0, 16},
    { 0,  0},
};

static int solid_at(int x, int y, int z);

typedef struct {
    int8_t v[4][3];
    uint8_t direction;
    uint8_t vertex_count;
} mb64_shape_face_t;

typedef struct {
    const mb64_shape_face_t *faces;
    uint8_t face_count;
} mb64_shape_t;

#define Q(dir, ...) { { __VA_ARGS__ }, dir, 4 }
#define T(dir, ...) { { __VA_ARGS__, {0,0,0} }, dir, 3 }

static const uint8_t s_rotated_dirs[4][6] = {
    {0, 1, 2, 3, 4, 5},
    {0, 1, 5, 4, 2, 3},
    {0, 1, 3, 2, 5, 4},
    {0, 1, 4, 5, 3, 2},
};

static const mb64_shape_face_t s_shape_full[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(2, {16,16,16}, {16,0,16}, {16,16,0}, {16,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    Q(4, {0,16,16}, {0,0,16}, {16,16,16}, {16,0,16}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
};

static const mb64_shape_face_t s_shape_slope[] = {
    Q(0, {16,0,16}, {16,16,0}, {0,0,16}, {0,16,0}),
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    T(2, {16,0,0}, {16,16,0}, {16,0,16}),
    T(3, {0,16,0}, {0,0,0}, {0,0,16}),
};

static const mb64_shape_face_t s_shape_dslope[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(1, {16,16,16}, {0,16,16}, {16,0,0}, {0,0,0}),
    T(2, {16,0,0}, {16,16,0}, {16,16,16}),
    T(3, {0,16,0}, {0,0,0}, {0,16,16}),
};

static const mb64_shape_face_t s_shape_bottom_slab[] = {
    Q(0, {16,8,16}, {16,8,0}, {0,8,16}, {0,8,0}),
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(2, {16,8,16}, {16,0,16}, {16,8,0}, {16,0,0}),
    Q(3, {0,8,0}, {0,0,0}, {0,8,16}, {0,0,16}),
    Q(4, {0,8,16}, {0,0,16}, {16,8,16}, {16,0,16}),
    Q(5, {16,8,0}, {16,0,0}, {0,8,0}, {0,0,0}),
};

static const mb64_shape_face_t s_shape_top_slab[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(1, {16,8,16}, {0,8,16}, {16,8,0}, {0,8,0}),
    Q(2, {16,16,16}, {16,8,16}, {16,16,0}, {16,8,0}),
    Q(3, {0,16,0}, {0,8,0}, {0,16,16}, {0,8,16}),
    Q(4, {0,16,16}, {0,8,16}, {16,16,16}, {16,8,16}),
    Q(5, {16,16,0}, {16,8,0}, {0,16,0}, {0,8,0}),
};

static const mb64_shape_face_t s_shape_corner[] = {
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    T(0, {0,0,16}, {16,0,16}, {0,16,0}),
    T(0, {0,16,0}, {16,0,16}, {16,0,0}),
    T(3, {0,16,0}, {0,0,0}, {0,0,16}),
    T(5, {0,0,0}, {0,16,0}, {16,0,0}),
};

static const mb64_shape_face_t s_shape_dcorner[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    T(1, {16,16,16}, {0,16,16}, {0,0,0}),
    T(1, {0,0,0}, {16,16,0}, {16,16,16}),
    T(3, {0,0,0}, {0,16,16}, {0,16,0}),
    T(5, {0,0,0}, {0,16,0}, {16,16,0}),
};

static const mb64_shape_face_t s_shape_icorner[] = {
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    T(0, {0,16,16}, {16,0,16}, {0,16,0}),
    T(0, {0,16,0}, {16,0,16}, {16,16,0}),
    T(2, {16,0,0}, {16,16,0}, {16,0,16}),
    T(4, {0,16,16}, {0,0,16}, {16,0,16}),
};

static const mb64_shape_face_t s_shape_dicorner[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    T(2, {16,0,0}, {16,16,0}, {16,16,16}),
    T(4, {0,16,16}, {0,0,16}, {16,16,16}),
    T(1, {16,16,16}, {0,0,16}, {0,0,0}),
    T(1, {16,16,16}, {0,0,0}, {16,0,0}),
};

static const mb64_shape_face_t s_shape_scorner[] = {
    T(0, {0,0,16}, {16,0,0}, {0,16,0}),
    T(1, {16,0,0}, {0,0,16}, {0,0,0}),
    T(3, {0,16,0}, {0,0,0}, {0,0,16}),
    T(5, {0,0,0}, {0,16,0}, {16,0,0}),
};

static const mb64_shape_face_t s_shape_dscorner[] = {
    T(1, {16,16,0}, {0,16,16}, {0,0,0}),
    T(0, {0,16,16}, {16,16,0}, {0,16,0}),
    T(3, {0,0,0}, {0,16,16}, {0,16,0}),
    T(5, {0,0,0}, {0,16,0}, {16,16,0}),
};

static const mb64_shape_face_t s_shape_iscorner[] = {
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    T(0, {0,16,16}, {16,16,0}, {0,16,0}),
    T(0, {0,16,16}, {16,0,16}, {16,16,0}),
    T(2, {16,0,0}, {16,16,0}, {16,0,16}),
    T(4, {0,16,16}, {0,0,16}, {16,0,16}),
};

static const mb64_shape_face_t s_shape_discorner[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    T(2, {16,0,0}, {16,16,0}, {16,16,16}),
    T(4, {0,16,16}, {0,0,16}, {16,16,16}),
    T(1, {16,0,0}, {0,0,16}, {0,0,0}),
    T(1, {0,0,16}, {16,0,0}, {16,16,16}),
};

static const mb64_shape_face_t s_shape_ugentle[] = {
    Q(0, {16,8,16}, {16,16,0}, {0,8,16}, {0,16,0}),
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(4, {0,8,16}, {0,0,16}, {16,8,16}, {16,0,16}),
    Q(2, {16,8,16}, {16,0,16}, {16,8,0}, {16,0,0}),
    Q(3, {0,8,0}, {0,0,0}, {0,8,16}, {0,0,16}),
    T(2, {16,8,0}, {16,16,0}, {16,8,16}),
    T(3, {0,16,0}, {0,8,0}, {0,8,16}),
};

static const mb64_shape_face_t s_shape_dugentle[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(1, {16,0,0}, {16,8,16}, {0,0,0}, {0,8,16}),
    Q(4, {0,16,16}, {0,8,16}, {16,16,16}, {16,8,16}),
    Q(2, {16,16,16}, {16,8,16}, {16,16,0}, {16,8,0}),
    Q(3, {0,16,0}, {0,8,0}, {0,16,16}, {0,8,16}),
    T(2, {16,0,0}, {16,8,0}, {16,8,16}),
    T(3, {0,8,0}, {0,0,0}, {0,8,16}),
};

static const mb64_shape_face_t s_shape_lgentle[] = {
    Q(0, {16,0,16}, {16,8,0}, {0,0,16}, {0,8,0}),
    Q(1, {16,0,16}, {0,0,16}, {16,0,0}, {0,0,0}),
    Q(5, {16,8,0}, {16,0,0}, {0,8,0}, {0,0,0}),
    T(2, {16,0,0}, {16,8,0}, {16,0,16}),
    T(3, {0,8,0}, {0,0,0}, {0,0,16}),
};

static const mb64_shape_face_t s_shape_dlgentle[] = {
    Q(0, {16,16,16}, {16,16,0}, {0,16,16}, {0,16,0}),
    Q(5, {16,16,0}, {16,8,0}, {0,16,0}, {0,8,0}),
    Q(1, {16,8,0}, {16,16,16}, {0,8,0}, {0,16,16}),
    T(2, {16,8,0}, {16,16,0}, {16,16,16}),
    T(3, {0,16,0}, {0,8,0}, {0,16,16}),
};

static const mb64_shape_face_t s_shape_vslab[] = {
    Q(0, {16,16,8}, {16,16,0}, {0,16,8}, {0,16,0}),
    Q(1, {16,0,8}, {0,0,8}, {16,0,0}, {0,0,0}),
    Q(2, {16,16,8}, {16,0,8}, {16,16,0}, {16,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,8}, {0,0,8}),
    Q(4, {0,16,8}, {0,0,8}, {16,16,8}, {16,0,8}),
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
};

static const mb64_shape_face_t s_shape_sslope[] = {
    Q(5, {16,16,0}, {16,0,0}, {0,16,0}, {0,0,0}),
    Q(3, {0,16,0}, {0,0,0}, {0,16,16}, {0,0,16}),
    Q(4, {16,16,0}, {0,16,16}, {16,0,0}, {0,0,16}),
    T(0, {0,16,16}, {16,16,0}, {0,16,0}),
    T(1, {16,0,0}, {0,0,16}, {0,0,0}),
};

static const mb64_shape_face_t s_shape_fence[] = {
    Q(5, {0,8,0}, {0,0,0}, {16,8,0}, {16,0,0}),
    Q(4, {16,8,0}, {16,0,0}, {0,8,0}, {0,0,0}),
};

static const mb64_shape_face_t s_shape_pole[] = {
    Q(5, {8,16,9}, {8,0,9}, {9,16,8}, {9,0,8}),
    Q(3, {9,16,8}, {9,0,8}, {8,16,7}, {8,0,7}),
    Q(4, {8,16,7}, {8,0,7}, {7,16,8}, {7,0,8}),
    Q(2, {7,16,8}, {7,0,8}, {8,16,9}, {8,0,9}),
    Q(0, {8,16,9}, {9,16,8}, {7,16,8}, {8,16,7}),
    Q(1, {8,0,9}, {7,0,8}, {9,0,8}, {8,0,7}),
};

static const mb64_shape_face_t s_shape_bars[] = {
    Q(5, {9,16,9}, {7,16,9}, {9,0,9}, {7,0,9}),
    Q(4, {7,16,7}, {9,16,7}, {7,0,7}, {9,0,7}),
    Q(2, {7,16,9}, {7,0,9}, {7,16,7}, {7,0,7}),
    Q(3, {9,16,7}, {9,0,7}, {9,16,9}, {9,0,9}),
    Q(0, {7,16,9}, {9,16,9}, {7,16,7}, {9,16,7}),
    Q(1, {9,0,9}, {7,0,9}, {9,0,7}, {7,0,7}),
};

static const mb64_shape_t s_shapes[32] = {
    [TILE_TYPE_SLOPE] = {s_shape_slope, sizeof(s_shape_slope) / sizeof(s_shape_slope[0])},
    [TILE_TYPE_DSLOPE] = {s_shape_dslope, sizeof(s_shape_dslope) / sizeof(s_shape_dslope[0])},
    [TILE_TYPE_SLAB] = {s_shape_bottom_slab, sizeof(s_shape_bottom_slab) / sizeof(s_shape_bottom_slab[0])},
    [TILE_TYPE_DSLAB] = {s_shape_top_slab, sizeof(s_shape_top_slab) / sizeof(s_shape_top_slab[0])},
    [TILE_TYPE_CORNER] = {s_shape_corner, sizeof(s_shape_corner) / sizeof(s_shape_corner[0])},
    [TILE_TYPE_DCORNER] = {s_shape_dcorner, sizeof(s_shape_dcorner) / sizeof(s_shape_dcorner[0])},
    [TILE_TYPE_ICORNER] = {s_shape_icorner, sizeof(s_shape_icorner) / sizeof(s_shape_icorner[0])},
    [TILE_TYPE_DICORNER] = {s_shape_dicorner, sizeof(s_shape_dicorner) / sizeof(s_shape_dicorner[0])},
    [TILE_TYPE_SCORNER] = {s_shape_scorner, sizeof(s_shape_scorner) / sizeof(s_shape_scorner[0])},
    [TILE_TYPE_DSCORNER] = {s_shape_dscorner, sizeof(s_shape_dscorner) / sizeof(s_shape_dscorner[0])},
    [TILE_TYPE_ISCORNER] = {s_shape_iscorner, sizeof(s_shape_iscorner) / sizeof(s_shape_iscorner[0])},
    [TILE_TYPE_DISCORNER] = {s_shape_discorner, sizeof(s_shape_discorner) / sizeof(s_shape_discorner[0])},
    [TILE_TYPE_UGENTLE] = {s_shape_ugentle, sizeof(s_shape_ugentle) / sizeof(s_shape_ugentle[0])},
    [TILE_TYPE_DUGENTLE] = {s_shape_dugentle, sizeof(s_shape_dugentle) / sizeof(s_shape_dugentle[0])},
    [TILE_TYPE_LGENTLE] = {s_shape_lgentle, sizeof(s_shape_lgentle) / sizeof(s_shape_lgentle[0])},
    [TILE_TYPE_DLGENTLE] = {s_shape_dlgentle, sizeof(s_shape_dlgentle) / sizeof(s_shape_dlgentle[0])},
    [TILE_TYPE_BLOCK] = {s_shape_full, sizeof(s_shape_full) / sizeof(s_shape_full[0])},
    [TILE_TYPE_SSLOPE] = {s_shape_sslope, sizeof(s_shape_sslope) / sizeof(s_shape_sslope[0])},
    [TILE_TYPE_SSLAB] = {s_shape_vslab, sizeof(s_shape_vslab) / sizeof(s_shape_vslab[0])},
    [TILE_TYPE_TROLL] = {s_shape_full, sizeof(s_shape_full) / sizeof(s_shape_full[0])},
    [TILE_TYPE_FENCE] = {s_shape_fence, sizeof(s_shape_fence) / sizeof(s_shape_fence[0])},
    [TILE_TYPE_POLE] = {s_shape_pole, sizeof(s_shape_pole) / sizeof(s_shape_pole[0])},
    [TILE_TYPE_BARS] = {s_shape_bars, sizeof(s_shape_bars) / sizeof(s_shape_bars[0])},
};

static int tile_in_range(const mb64_tile_t *t) {
    return t != NULL &&
           t->x < MB64_GRID_SIZE &&
           t->y < MB64_GRID_SIZE &&
           t->z < MB64_GRID_SIZE;
}

static int tile_is_solid(const mb64_tile_t *t) {
    return tile_in_range(t) &&
           t->type != 0 &&
           t->type != MB64_TILE_CULL &&
           t->type != MB64_TILE_WATER;
}

uint8_t mb64_tile_has_collision(const mb64_tile_t *tile) {
    return tile_is_solid(tile) && tile->type != TILE_TYPE_TROLL;
}

static int tile_is_water(const mb64_tile_t *t) {
    return tile_in_range(t) &&
           (t->type == MB64_TILE_WATER || t->waterlogged);
}

static int solid_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return 0;
    }
    return s_solid_grid[z][y][x] != 0;
}

static int water_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return 0;
    }
    return s_water_grid[z][y][x] != 0;
}

uint8_t mb64_resolve_tile_material(const mb64_level_t *level,
                                   const mb64_tile_t *tile,
                                   uint8_t top_face) {
    uint8_t slot = tile->mat % MB64_MATERIAL_SLOT_COUNT;
    if (level->header.theme == MB64_THEME_CUSTOM) {
        if (top_face && level->header.custom_theme.topmats_enabled[slot]) {
            return level->header.custom_theme.topmats[slot];
        }
        return level->header.custom_theme.mats[slot];
    }

    uint8_t theme = level->header.theme;
    if (theme >= (uint8_t)(sizeof(s_theme_materials) / sizeof(s_theme_materials[0]))) {
        theme = 0;
    }
    const mb64_material_def_t *def = &s_theme_materials[theme][slot];
    return top_face ? def->top : def->side;
}

static const mb64_theme_special_t *theme_specials(const mb64_level_t *level) {
    static const mb64_theme_special_t fallback = {0, MB64_MAT_STONE, 0, 0};
    if (level == NULL) {
        return &fallback;
    }
    if (level->header.theme == MB64_THEME_CUSTOM) {
        static mb64_theme_special_t custom;
        custom.fence = level->header.custom_theme.fence;
        custom.pole = level->header.custom_theme.pole;
        custom.bars = level->header.custom_theme.bars;
        custom.water = level->header.custom_theme.water;
        return &custom;
    }
    uint8_t theme = level->header.theme;
    if (theme >= (uint8_t)(sizeof(s_theme_specials) / sizeof(s_theme_specials[0]))) {
        theme = 0;
    }
    return &s_theme_specials[theme];
}

static uint8_t mb64_resolve_face_material(const mb64_level_t *level,
                                          const mb64_tile_t *tile,
                                          uint8_t direction) {
    if (tile == NULL) {
        return 0;
    }
    switch (tile->type) {
        case TILE_TYPE_FENCE:
            return MB64_RENDER_MATERIAL_FENCE;
        case TILE_TYPE_POLE:
            return theme_specials(level)->pole;
        case TILE_TYPE_BARS:
            return direction == MB64_MESH_FACE_TOP || direction == MB64_MESH_FACE_BOTTOM
                ? MB64_RENDER_MATERIAL_BARS_TOP
                : MB64_RENDER_MATERIAL_BARS;
        default:
            {
                const uint8_t resolved =
                    mb64_resolve_tile_material(level, tile, direction == MB64_MESH_FACE_TOP);
                if (resolved == MB64_MAT_TTC_MESH &&
                    (direction == MB64_MESH_FACE_TOP || direction == MB64_MESH_FACE_BOTTOM)) {
                    return MB64_RENDER_MATERIAL_TTC_GRATE_TOP;
                }
                return resolved;
            }
    }
}

int16_t mb64_surface_for_material(uint8_t material) {
    switch (material) {
        case MB64_MAT_LAVA:
            return MB64_SURFACE_BURNING;
        case MB64_MAT_QUICKSAND:
            return MB64_SURFACE_INSTANT_QUICKSAND;
        case MB64_MAT_DESERT_SLOWSAND:
            return MB64_SURFACE_DEEP_QUICKSAND;
        default:
            return MB64_SURFACE_DEFAULT;
    }
}

int16_t mb64_surface_for_tile(const mb64_level_t *level, const mb64_tile_t *tile) {
    if (level == NULL || tile == NULL) {
        return MB64_SURFACE_DEFAULT;
    }
    return mb64_surface_for_material(mb64_resolve_tile_material(level, tile, 1));
}

mb64_material_texture_animation_t mb64_texture_animation_for_material(uint8_t material) {
    switch (material) {
        case MB64_MAT_LAVA:
        case MB64_MAT_VANILLA_LAVA:
        case MB64_MAT_SERVER_ACID:
        case MB64_MAT_BURNING_ICE:
        case MB64_MAT_QUICKSAND:
        case MB64_MAT_VOID:
            return (mb64_material_texture_animation_t) {
                1, 15, 1, PACK_TILESIZE(0, 1), PACK_TILESIZE(0, 1)
            };
        default:
            return (mb64_material_texture_animation_t) { 0, 0, 0, 0, 0 };
    }
}

mb64_material_texture_animation_t mb64_texture_animation_for_water(const mb64_level_t *level) {
    const uint8_t water = theme_specials(level)->water;
    switch (water) {
        case MB64_WATER_DEFAULT:
        case MB64_WATER_GREEN:
            return (mb64_material_texture_animation_t) {
                1, 16, 1, PACK_TILESIZE(0, 1), PACK_TILESIZE(0, 1)
            };
        case MB64_WATER_RETRO:
            return (mb64_material_texture_animation_t) {
                1, 16, 10, PACK_TILESIZE(0, 5), PACK_TILESIZE(0, 5)
            };
        default:
            return (mb64_material_texture_animation_t) { 0, 0, 0, 0, 0 };
    }
}

static void tile_bounds(const mb64_tile_t *t,
                        int16_t *x0, int16_t *x1,
                        int16_t *y0, int16_t *y1,
                        int16_t *z0, int16_t *z1) {
    *x0 = (int16_t)(((int)t->x - MB64_GRID_CENTRE) * MB64_TILE_SUBUNITS);
    *x1 = (int16_t)(*x0 + MB64_TILE_SUBUNITS);
    *y0 = (int16_t)((int)t->y * MB64_TILE_SUBUNITS);
    *y1 = (int16_t)(*y0 + MB64_TILE_SUBUNITS);
    *z0 = (int16_t)(((int)t->z - MB64_GRID_CENTRE) * MB64_TILE_SUBUNITS);
    *z1 = (int16_t)(*z0 + MB64_TILE_SUBUNITS);
}

static void emit_face(mb64_mesh_t *mesh, uint32_t *idx,
                      const mb64_level_t *level,
                      const mb64_tile_t *t, uint8_t direction,
                      uint8_t is_water, const int16_t p[4][3]) {
    mb64_mesh_face_t *face = &mesh->faces[(*idx)++];
    memcpy(face->v, p, sizeof(face->v));
    face->material = t->mat;
    face->resolved_material = mb64_resolve_face_material(level, t, direction);
    face->tile_type = t->type;
    face->direction = direction;
    face->is_water = is_water;
    face->vertex_count = 4;
    face->use_tc = 0;
}

static uint8_t rotate_direction(uint8_t direction, uint8_t rot) {
    if (direction >= 6) {
        return direction;
    }
    return s_rotated_dirs[rot & 3][direction];
}

static uint8_t face_uv_axes(uint8_t direction, uint8_t *u_axis, uint8_t *v_axis) {
    switch (direction) {
        case 3: /* MB64_DIRECTION_NEG_X */
            *u_axis = 2;
            *v_axis = 1;
            return 1;
        case 2: /* MB64_DIRECTION_POS_X */
            *u_axis = 2;
            *v_axis = 1;
            return 0;
        case MB64_MESH_FACE_BOTTOM:
        case MB64_MESH_FACE_TOP:
            *u_axis = 2;
            *v_axis = 0;
            return 0;
        case 5: /* MB64_DIRECTION_NEG_Z */
            *u_axis = 0;
            *v_axis = 1;
            return 0;
        case 4: /* MB64_DIRECTION_POS_Z */
        default:
            *u_axis = 0;
            *v_axis = 1;
            return 1;
    }
}

static uint8_t tile_axis_value(const mb64_tile_t *tile, uint8_t axis) {
    switch (axis) {
        case 0: return tile->x;
        case 1: return tile->y;
        default: return tile->z;
    }
}

static int16_t mb64_uv_wrap_offset(int32_t value) {
    int32_t wrapped = value % 48;
    if (wrapped < 0) {
        wrapped += 48;
    }
    return (int16_t)(wrapped - 24);
}

static void assign_fence_texture_coordinates(mb64_mesh_face_t *face,
                                             const mb64_tile_t *tile,
                                             uint8_t direction) {
    uint8_t u_axis;
    uint8_t v_axis;
    uint8_t flip_u = face_uv_axes(direction, &u_axis, &v_axis);
    int32_t u_pos = flip_u ? 64 - tile_axis_value(tile, u_axis)
                           : tile_axis_value(tile, u_axis);
    (void)v_axis; /* MB64 clamps fence V coordinates for growth render type 3. */
    u_pos = mb64_uv_wrap_offset(u_pos * 2);

    for (uint8_t i = 0; i < face->vertex_count; i++) {
        int16_t u = (int16_t)(16 - s_fence_alt_uvs[i][0]);
        int16_t v = (int16_t)(16 - s_fence_alt_uvs[i][1]);
        u = (int16_t)(u - u_pos * 16);
        face->tc[i][0] = (int16_t)(u * 64 - 16);
        face->tc[i][1] = (int16_t)(v * 64 - 16);
    }
    face->use_tc = 1;
}

static int shape_face_is_occluded(const mb64_tile_t *t, uint8_t direction) {
    switch (t->type) {
        case TILE_TYPE_SCORNER:
        case TILE_TYPE_DSCORNER:
        case TILE_TYPE_ISCORNER:
        case TILE_TYPE_DISCORNER:
        case TILE_TYPE_UGENTLE:
        case TILE_TYPE_DUGENTLE:
        case TILE_TYPE_LGENTLE:
        case TILE_TYPE_DLGENTLE:
        case TILE_TYPE_SSLAB:
            /*
             * MB64's renderer culls shaped tiles by face shape, not just by
             * occupied neighbor cells. These partial/diagonal faces are the
             * cases where cell-only culling drops visible surfaces and exposes
             * interiors through angled blocks. Keep the exact shaped faces
             * emitted here until libmb64 exports the full MB64 faceshape culler.
             */
            return 0;
    }

    int dx = 0;
    int dy = 0;
    int dz = 0;
    switch (direction) {
        case MB64_MESH_FACE_TOP:
            dy = 1;
            break;
        case MB64_MESH_FACE_BOTTOM:
            dy = -1;
            break;
        case 2: /* MB64_DIRECTION_POS_X */
            dx = 1;
            break;
        case 3: /* MB64_DIRECTION_NEG_X */
            dx = -1;
            break;
        case 4: /* MB64_DIRECTION_POS_Z */
            dz = 1;
            break;
        case 5: /* MB64_DIRECTION_NEG_Z */
            dz = -1;
            break;
        default:
            return 0;
    }
    return solid_at((int)t->x + dx, (int)t->y + dy, (int)t->z + dz);
}

static void rotate_vertex(uint8_t rot, const int8_t in[3], int16_t out[3]) {
    int16_t x = in[0];
    int16_t y = in[1];
    int16_t z = in[2];
    switch (rot & 3) {
        case 1:
            out[0] = z;
            out[1] = y;
            out[2] = (int16_t)(16 - x);
            break;
        case 2:
            out[0] = (int16_t)(16 - x);
            out[1] = y;
            out[2] = (int16_t)(16 - z);
            break;
        case 3:
            out[0] = (int16_t)(16 - z);
            out[1] = y;
            out[2] = x;
            break;
        default:
            out[0] = x;
            out[1] = y;
            out[2] = z;
            break;
    }
}

static const mb64_shape_t *shape_for_tile(const mb64_tile_t *t) {
    if (t == NULL || t->type >= (sizeof(s_shapes) / sizeof(s_shapes[0])) ||
        s_shapes[t->type].faces == NULL) {
        return NULL;
    }
    return &s_shapes[t->type];
}

static int tile_uses_shaped_mesh(const mb64_tile_t *t) {
    return shape_for_tile(t) != NULL && t->type != TILE_TYPE_BLOCK && t->type != TILE_TYPE_TROLL;
}

static void emit_shape_face(mb64_mesh_t *mesh, uint32_t *idx,
                            const mb64_level_t *level,
                            const mb64_tile_t *t,
                            const mb64_shape_face_t *src) {
    int16_t p[4][3];
    int16_t x0, x1, y0, y1, z0, z1;
    (void)x1; (void)y1; (void)z1;
    tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
    for (uint8_t i = 0; i < 4; i++) {
        int16_t v[3];
        rotate_vertex(t->rot, src->v[i], v);
        p[i][0] = (int16_t)(x0 + v[0]);
        p[i][1] = (int16_t)(y0 + v[1]);
        p[i][2] = (int16_t)(z0 + v[2]);
    }

    uint8_t direction = rotate_direction(src->direction, t->rot);
    if (shape_face_is_occluded(t, direction)) {
        return;
    }
    mb64_mesh_face_t *face = &mesh->faces[(*idx)++];
    memcpy(face->v, p, sizeof(face->v));
    face->material = t->mat;
    face->resolved_material = mb64_resolve_face_material(level, t, direction);
    face->tile_type = t->type;
    face->direction = direction;
    face->is_water = 0;
    face->vertex_count = src->vertex_count;
    if (t->type == TILE_TYPE_FENCE) {
        assign_fence_texture_coordinates(face, t, direction);
    } else {
        face->use_tc = 0;
    }
}

int mb64_build_render_mesh(const mb64_level_t *level, mb64_mesh_t *mesh) {
    if (mesh == NULL) { return 0; }
    memset(mesh, 0, sizeof(*mesh));
    if (level == NULL || level->tiles == NULL || level->header.tile_count == 0) {
        return 0;
    }

    memset(s_solid_grid, 0, sizeof(s_solid_grid));
    memset(s_water_grid, 0, sizeof(s_water_grid));

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (tile_is_solid(t)) {
            s_solid_grid[t->z][t->y][t->x] = 1;
            mesh->solid_tile_count++;
        } else if (tile_is_water(t)) {
            s_water_grid[t->z][t->y][t->x] = 1;
            mesh->water_tile_count++;
        }
    }

    uint32_t face_count = 0;
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!tile_is_water(t) || water_at(t->x, t->y - 1, t->z)) { continue; }
        int y1 = t->y + 1;
        while (water_at(t->x, y1, t->z)) { y1++; }
        face_count++; /* top of the merged water column */
        if (!water_at(t->x - 1, t->y, t->z)) { face_count++; }
        if (!water_at(t->x + 1, t->y, t->z)) { face_count++; }
        if (!water_at(t->x, t->y, t->z - 1)) { face_count++; }
        if (!water_at(t->x, t->y, t->z + 1)) { face_count++; }
    }
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!tile_is_solid(t)) { continue; }
        const mb64_shape_t *shape = shape_for_tile(t);
        if (tile_uses_shaped_mesh(t) && shape != NULL) {
            face_count += shape->face_count;
            continue;
        }
        int tx = t->x, ty = t->y, tz = t->z;
        if (!solid_at(tx, ty + 1, tz)) { face_count++; }
        if (!solid_at(tx, ty - 1, tz)) { face_count++; }
        if (!solid_at(tx - 1, ty, tz)) { face_count++; }
        if (!solid_at(tx + 1, ty, tz)) { face_count++; }
        if (!solid_at(tx, ty, tz - 1)) { face_count++; }
        if (!solid_at(tx, ty, tz + 1)) { face_count++; }
    }

    if (face_count == 0) { return 0; }
    mesh->faces = calloc(face_count, sizeof(*mesh->faces));
    if (mesh->faces == NULL) {
        memset(mesh, 0, sizeof(*mesh));
        return 0;
    }
    mesh->face_count = face_count;

    uint32_t out = 0;
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!tile_is_solid(t)) { continue; }
        const mb64_shape_t *shape = shape_for_tile(t);
        if (tile_uses_shaped_mesh(t) && shape != NULL) {
            for (uint8_t j = 0; j < shape->face_count; j++) {
                emit_shape_face(mesh, &out, level, t, &shape->faces[j]);
            }
            continue;
        }

        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        int tx = t->x, ty = t->y, tz = t->z;

        if (!solid_at(tx, ty + 1, tz)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y1,z0},{x1,y1,z1},{x1,y1,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_TOP, 0, p);
        }
        if (!solid_at(tx, ty - 1, tz)) {
            const int16_t p[4][3] = {{x0,y0,z0},{x0,y0,z1},{x1,y0,z0},{x1,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_BOTTOM, 0, p);
        }
        if (!solid_at(tx - 1, ty, tz)) {
            const int16_t p[4][3] = {{x0,y1,z0},{x0,y0,z0},{x0,y1,z1},{x0,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_X, 0, p);
        }
        if (!solid_at(tx + 1, ty, tz)) {
            const int16_t p[4][3] = {{x1,y1,z1},{x1,y0,z1},{x1,y1,z0},{x1,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_X, 0, p);
        }
        if (!solid_at(tx, ty, tz - 1)) {
            const int16_t p[4][3] = {{x1,y1,z0},{x1,y0,z0},{x0,y1,z0},{x0,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_Z, 0, p);
        }
        if (!solid_at(tx, ty, tz + 1)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y0,z1},{x1,y1,z1},{x1,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_Z, 0, p);
        }
    }

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!tile_is_water(t)) { continue; }
        if (water_at(t->x, t->y - 1, t->z)) { continue; }
        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        while (water_at(t->x, (y1 / MB64_TILE_SUBUNITS), t->z)) {
            y1 = (int16_t)(y1 + MB64_TILE_SUBUNITS);
        }
        int16_t water_top_y = (int16_t)(y1 - 2);
        int16_t water_side_top_y = water_top_y;
        int16_t water_side_bottom_y = (int16_t)(y0 - 2);
        const int16_t top[4][3] = {{x0,water_top_y,z1},{x0,water_top_y,z0},{x1,water_top_y,z1},{x1,water_top_y,z0}};
        emit_face(mesh, &out, level, t, MB64_MESH_FACE_TOP, 1, top);
        if (!water_at(t->x - 1, t->y, t->z)) {
            const int16_t p[4][3] = {{x0,water_side_top_y,z0},{x0,water_side_bottom_y,z0},{x0,water_side_top_y,z1},{x0,water_side_bottom_y,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_X, 1, p);
        }
        if (!water_at(t->x + 1, t->y, t->z)) {
            const int16_t p[4][3] = {{x1,water_side_top_y,z1},{x1,water_side_bottom_y,z1},{x1,water_side_top_y,z0},{x1,water_side_bottom_y,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_X, 1, p);
        }
        if (!water_at(t->x, t->y, t->z - 1)) {
            const int16_t p[4][3] = {{x1,water_side_top_y,z0},{x1,water_side_bottom_y,z0},{x0,water_side_top_y,z0},{x0,water_side_bottom_y,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_Z, 1, p);
        }
        if (!water_at(t->x, t->y, t->z + 1)) {
            const int16_t p[4][3] = {{x0,water_side_top_y,z1},{x0,water_side_bottom_y,z1},{x1,water_side_top_y,z1},{x1,water_side_bottom_y,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_Z, 1, p);
        }
    }

    mesh->face_count = out;
    return out > 0;
}

void mb64_free_render_mesh(mb64_mesh_t *mesh) {
    if (mesh == NULL) { return; }
    free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}
