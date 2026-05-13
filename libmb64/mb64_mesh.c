#include "mb64.h"

#include <stdlib.h>
#include <string.h>

#define MB64_GRID_SIZE 64
#define MB64_GRID_CENTRE 32
#define MB64_TILE_SUBUNITS 16
#define MB64_TILE_CULL 21
#define MB64_TILE_WATER 26
#define MB64_THEME_CUSTOM 9
#define MB64_MATERIAL_SLOT_COUNT 10

enum mb64_material_id {
    MB64_MAT_GRASS = 0,
    MB64_MAT_SAND = 8,
    MB64_MAT_DIRT = 12,
    MB64_MAT_SANDDIRT = 13,
    MB64_MAT_STONE = 23,
    MB64_MAT_COBBLESTONE = 29,
    MB64_MAT_DESERT_STONE = 41,
    MB64_MAT_BRICKS = 44,
    MB64_MAT_DESERT_BRICKS = 45,
    MB64_MAT_TILESBRICKS = 58,
    MB64_MAT_TILES = 59,
    MB64_MAT_DESERT_TILES = 61,
    MB64_MAT_DESERT_BLOCK = 73,
    MB64_MAT_WOOD = 85,
    MB64_MAT_DESERT_TILES2 = 100,
    MB64_MAT_ROOF = 102,
    MB64_MAT_SNOW = 10,
    MB64_MAT_SNOWDIRT = 20,
    MB64_MAT_LAVA = 116,
    MB64_MAT_QUICKSAND = 120,
    MB64_MAT_DESERT_SLOWSAND = 121,
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

static const mb64_material_def_t s_theme_materials[][MB64_MATERIAL_SLOT_COUNT] = {
    {
        {MB64_MAT_DIRT, MB64_MAT_GRASS},
        {MB64_MAT_BRICKS, MB64_MAT_BRICKS},
        {MB64_MAT_COBBLESTONE, MB64_MAT_STONE},
        {MB64_MAT_TILESBRICKS, MB64_MAT_TILES},
        {MB64_MAT_ROOF, MB64_MAT_ROOF},
        {MB64_MAT_WOOD, MB64_MAT_WOOD},
        {MB64_MAT_SANDDIRT, MB64_MAT_SAND},
        {MB64_MAT_SNOWDIRT, MB64_MAT_SNOW},
        {MB64_MAT_LAVA, MB64_MAT_LAVA},
        {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND},
    },
    {
        {MB64_MAT_SANDDIRT, MB64_MAT_SAND},
        {MB64_MAT_DESERT_BRICKS, MB64_MAT_DESERT_BRICKS},
        {MB64_MAT_DESERT_STONE, MB64_MAT_DESERT_STONE},
        {MB64_MAT_DESERT_TILES, MB64_MAT_DESERT_TILES},
        {MB64_MAT_DESERT_BLOCK, MB64_MAT_DESERT_BLOCK},
        {MB64_MAT_DESERT_SLOWSAND, MB64_MAT_DESERT_SLOWSAND},
        {MB64_MAT_DESERT_BRICKS, MB64_MAT_DESERT_TILES2},
        {MB64_MAT_DIRT, MB64_MAT_GRASS},
        {MB64_MAT_LAVA, MB64_MAT_LAVA},
        {MB64_MAT_QUICKSAND, MB64_MAT_QUICKSAND},
    },
};

static uint8_t s_solid_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];
static uint8_t s_water_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];

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
    face->resolved_material = mb64_resolve_tile_material(level, t, direction == MB64_MESH_FACE_TOP);
    face->tile_type = t->type;
    face->direction = direction;
    face->is_water = is_water;
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

    return out == face_count;
}

void mb64_free_render_mesh(mb64_mesh_t *mesh) {
    if (mesh == NULL) { return; }
    free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}
