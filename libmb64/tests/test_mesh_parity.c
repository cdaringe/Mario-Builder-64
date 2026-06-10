#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mb64.h"
#include "mb64_tile_types.h"

static int g_failures = 0;

static void expect_int(const char *label, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", label, expected, actual);
        g_failures++;
    }
}

static void expect_vertex(const char *label,
                          const int16_t actual[3],
                          int16_t x,
                          int16_t y,
                          int16_t z) {
    if (actual[0] != x || actual[1] != y || actual[2] != z) {
        fprintf(stderr, "%s: expected (%d,%d,%d), got (%d,%d,%d)\n",
                label, x, y, z, actual[0], actual[1], actual[2]);
        g_failures++;
    }
}

static void set_level(mb64_level_t *level, mb64_tile_t *tile, uint8_t rot) {
    memset(level, 0, sizeof(*level));
    memset(tile, 0, sizeof(*tile));
    level->header.theme = 0;
    level->header.tile_count = 1;
    level->tiles = tile;
    tile->x = 32;
    tile->y = 2;
    tile->z = 32;
    tile->type = TILE_TYPE_FENCE;
    tile->mat = MB64_MAT_GRASS;
    tile->rot = rot;
}

static void verify_fence_rotation(uint8_t rot,
                                  uint8_t front_dir,
                                  uint8_t back_dir,
                                  const int16_t front[4][3],
                                  const int16_t back[4][3]) {
    mb64_level_t level;
    mb64_tile_t tile;
    mb64_mesh_t mesh = { 0 };
    char label[64];

    set_level(&level, &tile, rot);
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "rot %u: mb64_build_render_mesh failed\n", rot);
        g_failures++;
        return;
    }

    snprintf(label, sizeof(label), "rot %u face_count", rot);
    expect_int(label, (int)mesh.face_count, 2);
    snprintf(label, sizeof(label), "rot %u solid_tile_count", rot);
    expect_int(label, (int)mesh.solid_tile_count, 1);

    if (mesh.face_count == 2) {
        expect_int("front vertex_count", mesh.faces[0].vertex_count, 4);
        expect_int("back vertex_count", mesh.faces[1].vertex_count, 4);
        expect_int("front tile type", mesh.faces[0].tile_type, TILE_TYPE_FENCE);
        expect_int("back tile type", mesh.faces[1].tile_type, TILE_TYPE_FENCE);
        expect_int("front material", mesh.faces[0].resolved_material, MB64_RENDER_MATERIAL_FENCE);
        expect_int("back material", mesh.faces[1].resolved_material, MB64_RENDER_MATERIAL_FENCE);
        expect_int("front direction", mesh.faces[0].direction, front_dir);
        expect_int("back direction", mesh.faces[1].direction, back_dir);
        for (int i = 0; i < 4; i++) {
            snprintf(label, sizeof(label), "rot %u front v%d", rot, i);
            expect_vertex(label, mesh.faces[0].v[i], front[i][0], front[i][1], front[i][2]);
            snprintf(label, sizeof(label), "rot %u back v%d", rot, i);
            expect_vertex(label, mesh.faces[1].v[i], back[i][0], back[i][1], back[i][2]);
        }
    }

    mb64_free_render_mesh(&mesh);
}

int main(void) {
    static const int16_t front0[4][3] = {
        { 0, 40, 0 }, { 0, 32, 0 }, { 16, 40, 0 }, { 16, 32, 0 },
    };
    static const int16_t back0[4][3] = {
        { 16, 40, 0 }, { 16, 32, 0 }, { 0, 40, 0 }, { 0, 32, 0 },
    };
    static const int16_t front1[4][3] = {
        { 0, 40, 16 }, { 0, 32, 16 }, { 0, 40, 0 }, { 0, 32, 0 },
    };
    static const int16_t back1[4][3] = {
        { 0, 40, 0 }, { 0, 32, 0 }, { 0, 40, 16 }, { 0, 32, 16 },
    };
    static const int16_t front2[4][3] = {
        { 16, 40, 16 }, { 16, 32, 16 }, { 0, 40, 16 }, { 0, 32, 16 },
    };
    static const int16_t back2[4][3] = {
        { 0, 40, 16 }, { 0, 32, 16 }, { 16, 40, 16 }, { 16, 32, 16 },
    };
    static const int16_t front3[4][3] = {
        { 16, 40, 0 }, { 16, 32, 0 }, { 16, 40, 16 }, { 16, 32, 16 },
    };
    static const int16_t back3[4][3] = {
        { 16, 40, 16 }, { 16, 32, 16 }, { 16, 40, 0 }, { 16, 32, 0 },
    };

    verify_fence_rotation(0, MB64_MESH_FACE_POS_Z, MB64_MESH_FACE_NEG_Z, front0, back0);
    verify_fence_rotation(1, MB64_MESH_FACE_POS_X, MB64_MESH_FACE_NEG_X, front1, back1);
    verify_fence_rotation(2, MB64_MESH_FACE_NEG_Z, MB64_MESH_FACE_POS_Z, front2, back2);
    verify_fence_rotation(3, MB64_MESH_FACE_NEG_X, MB64_MESH_FACE_POS_X, front3, back3);

    if (g_failures != 0) {
        fprintf(stderr, "mesh parity tests failed: %d\n", g_failures);
        return 1;
    }
    puts("[LIBMB64_TEST] mesh parity tests passed");
    return 0;
}
