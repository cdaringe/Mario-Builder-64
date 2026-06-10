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

static void expect_float(const char *label, float actual, float expected) {
    float delta = actual - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    if (delta > 0.001f) {
        fprintf(stderr, "%s: expected %.3f, got %.3f\n", label, expected, actual);
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

static void set_tile_level(mb64_level_t *level, mb64_tile_t *tile, uint8_t type, uint8_t rot) {
    set_level(level, tile, rot);
    tile->type = type;
}

static uint8_t expected_rotated_direction(uint8_t direction, uint8_t rot) {
    static const uint8_t dirs[4][6] = {
        { MB64_MESH_FACE_TOP, MB64_MESH_FACE_BOTTOM, MB64_MESH_FACE_POS_X, MB64_MESH_FACE_NEG_X, MB64_MESH_FACE_POS_Z, MB64_MESH_FACE_NEG_Z },
        { MB64_MESH_FACE_TOP, MB64_MESH_FACE_BOTTOM, MB64_MESH_FACE_NEG_Z, MB64_MESH_FACE_POS_Z, MB64_MESH_FACE_POS_X, MB64_MESH_FACE_NEG_X },
        { MB64_MESH_FACE_TOP, MB64_MESH_FACE_BOTTOM, MB64_MESH_FACE_NEG_X, MB64_MESH_FACE_POS_X, MB64_MESH_FACE_NEG_Z, MB64_MESH_FACE_POS_Z },
        { MB64_MESH_FACE_TOP, MB64_MESH_FACE_BOTTOM, MB64_MESH_FACE_POS_Z, MB64_MESH_FACE_NEG_Z, MB64_MESH_FACE_NEG_X, MB64_MESH_FACE_POS_X },
    };
    if (direction >= 6) {
        return direction;
    }
    return dirs[rot & 3][direction];
}

static void expected_rotated_vertex(uint8_t rot, const int16_t in[3], int16_t out[3]) {
    switch (rot & 3) {
        case 1:
            out[0] = in[2];
            out[1] = in[1];
            out[2] = (int16_t)(16 - in[0]);
            break;
        case 2:
            out[0] = (int16_t)(16 - in[0]);
            out[1] = in[1];
            out[2] = (int16_t)(16 - in[2]);
            break;
        case 3:
            out[0] = (int16_t)(16 - in[2]);
            out[1] = in[1];
            out[2] = in[0];
            break;
        default:
            out[0] = in[0];
            out[1] = in[1];
            out[2] = in[2];
            break;
    }
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

static void verify_shaped_tile_rotation(uint8_t type, uint8_t rot) {
    mb64_level_t base_level;
    mb64_level_t rotated_level;
    mb64_tile_t base_tile;
    mb64_tile_t rotated_tile;
    mb64_mesh_t base_mesh = { 0 };
    mb64_mesh_t rotated_mesh = { 0 };
    char label[96];

    set_tile_level(&base_level, &base_tile, type, 0);
    set_tile_level(&rotated_level, &rotated_tile, type, rot);

    if (!mb64_build_render_mesh(&base_level, &base_mesh)) {
        fprintf(stderr, "tile %u rot 0: mb64_build_render_mesh failed\n", type);
        g_failures++;
        return;
    }
    if (!mb64_build_render_mesh(&rotated_level, &rotated_mesh)) {
        fprintf(stderr, "tile %u rot %u: mb64_build_render_mesh failed\n", type, rot);
        g_failures++;
        mb64_free_render_mesh(&base_mesh);
        return;
    }

    snprintf(label, sizeof(label), "tile %u rot %u face_count", type, rot);
    expect_int(label, (int)rotated_mesh.face_count, (int)base_mesh.face_count);
    snprintf(label, sizeof(label), "tile %u rot %u solid_tile_count", type, rot);
    expect_int(label, (int)rotated_mesh.solid_tile_count, 1);

    if (base_mesh.face_count == rotated_mesh.face_count) {
        for (uint32_t face_idx = 0; face_idx < base_mesh.face_count; face_idx++) {
            const mb64_mesh_face_t *base = &base_mesh.faces[face_idx];
            const mb64_mesh_face_t *actual = &rotated_mesh.faces[face_idx];
            snprintf(label, sizeof(label), "tile %u rot %u face %u tile_type", type, rot, face_idx);
            expect_int(label, actual->tile_type, type);
            snprintf(label, sizeof(label), "tile %u rot %u face %u vertex_count", type, rot, face_idx);
            expect_int(label, actual->vertex_count, base->vertex_count);
            snprintf(label, sizeof(label), "tile %u rot %u face %u direction", type, rot, face_idx);
            expect_int(label, actual->direction, expected_rotated_direction(base->direction, rot));
            for (uint8_t v = 0; v < base->vertex_count; v++) {
                int16_t expected[3];
                expected_rotated_vertex(rot, base->v[v], expected);
                snprintf(label, sizeof(label), "tile %u rot %u face %u v%u", type, rot, face_idx, v);
                expect_vertex(label, actual->v[v], expected[0], expected[1], expected[2]);
            }
        }
    }

    mb64_free_render_mesh(&base_mesh);
    mb64_free_render_mesh(&rotated_mesh);
}

static void verify_shaped_tile_rotations(void) {
    static const uint8_t shaped_types[] = {
        TILE_TYPE_SLOPE,
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
        TILE_TYPE_SSLOPE,
        TILE_TYPE_SSLAB,
    };

    for (size_t i = 0; i < sizeof(shaped_types) / sizeof(shaped_types[0]); i++) {
        for (uint8_t rot = 1; rot < 4; rot++) {
            verify_shaped_tile_rotation(shaped_types[i], rot);
        }
    }
}

static void verify_woodplat_helpers(void) {
    const uint8_t thin_fat_thin[] = { 0, 1, 0 };

    expect_float("woodplat thin height", mb64_woodplat_piece_height(0), 96.0f);
    expect_float("woodplat fat height", mb64_woodplat_piece_height(1), 256.0f);
    expect_float("woodplat stack height", mb64_woodplat_stack_height(thin_fat_thin, 3), 448.0f);
    expect_float("woodplat null stack height", mb64_woodplat_stack_height(NULL, 3), 0.0f);
    expect_int("woodplat fat stacks within epsilon", mb64_woodplat_should_stack(1, 4.99f), 1);
    expect_int("woodplat fat does not stack at epsilon", mb64_woodplat_should_stack(1, 5.0f), 0);
    expect_int("woodplat thin does not stack", mb64_woodplat_should_stack(0, 0.0f), 0);
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
    verify_shaped_tile_rotations();
    verify_woodplat_helpers();

    if (g_failures != 0) {
        fprintf(stderr, "mesh parity tests failed: %d\n", g_failures);
        return 1;
    }
    puts("[LIBMB64_TEST] mesh parity tests passed");
    return 0;
}
