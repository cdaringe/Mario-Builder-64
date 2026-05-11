#include "mb64.h"

#include <stdlib.h>
#include <string.h>

#define MB64_GRID_SIZE 64
#define MB64_GRID_CENTRE 32
#define MB64_TILE_SUBUNITS 16
#define MB64_TILE_CULL 21
#define MB64_TILE_WATER 26

static uint8_t s_solid_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];

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
                      const mb64_tile_t *t, uint8_t direction,
                      uint8_t is_water, const int16_t p[4][3]) {
    mb64_mesh_face_t *face = &mesh->faces[(*idx)++];
    memcpy(face->v, p, sizeof(face->v));
    face->material = t->mat;
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

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (tile_is_solid(t)) {
            s_solid_grid[t->z][t->y][t->x] = 1;
            mesh->solid_tile_count++;
        } else if (tile_is_water(t)) {
            mesh->water_tile_count++;
        }
    }

    uint32_t face_count = mesh->water_tile_count;
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
            emit_face(mesh, &out, t, MB64_MESH_FACE_TOP, 0, p);
        }
        if (!solid_at(tx, ty - 1, tz)) {
            const int16_t p[4][3] = {{x0,y0,z0},{x0,y0,z1},{x1,y0,z0},{x1,y0,z1}};
            emit_face(mesh, &out, t, MB64_MESH_FACE_BOTTOM, 0, p);
        }
        if (!solid_at(tx - 1, ty, tz)) {
            const int16_t p[4][3] = {{x0,y1,z0},{x0,y0,z0},{x0,y1,z1},{x0,y0,z1}};
            emit_face(mesh, &out, t, MB64_MESH_FACE_NEG_X, 0, p);
        }
        if (!solid_at(tx + 1, ty, tz)) {
            const int16_t p[4][3] = {{x1,y1,z1},{x1,y0,z1},{x1,y1,z0},{x1,y0,z0}};
            emit_face(mesh, &out, t, MB64_MESH_FACE_POS_X, 0, p);
        }
        if (!solid_at(tx, ty, tz - 1)) {
            const int16_t p[4][3] = {{x1,y1,z0},{x1,y0,z0},{x0,y1,z0},{x0,y0,z0}};
            emit_face(mesh, &out, t, MB64_MESH_FACE_NEG_Z, 0, p);
        }
        if (!solid_at(tx, ty, tz + 1)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y0,z1},{x1,y1,z1},{x1,y0,z1}};
            emit_face(mesh, &out, t, MB64_MESH_FACE_POS_Z, 0, p);
        }
    }

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!tile_is_water(t)) { continue; }
        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        (void)y1;
        int16_t water_y = (int16_t)(y0 + 14);
        const int16_t p[4][3] = {{x0,water_y,z1},{x0,water_y,z0},{x1,water_y,z1},{x1,water_y,z0}};
        emit_face(mesh, &out, t, MB64_MESH_FACE_TOP, 1, p);
    }

    return out == face_count;
}

void mb64_free_render_mesh(mb64_mesh_t *mesh) {
    if (mesh == NULL) { return; }
    free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}
