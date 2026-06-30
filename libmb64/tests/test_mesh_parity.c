#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mb64.h"
#include "mb64_tile_types.h"

#define MB64_TEST_THEME_CUSTOM 10

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

static void expect_texcoord(const char *label,
                            const int16_t actual[2],
                            int16_t u,
                            int16_t v) {
    if (actual[0] != u || actual[1] != v) {
        fprintf(stderr, "%s: expected (%d,%d), got (%d,%d)\n",
                label, u, v, actual[0], actual[1]);
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
        expect_int("front uses explicit fence texture coordinates", mesh.faces[0].use_tc, 1);
        expect_int("back uses explicit fence texture coordinates", mesh.faces[1].use_tc, 1);
        for (int i = 0; i < 4; i++) {
            static const int16_t expected_tc[4][2] = {
                { 7152, -16 }, { 7152, 1008 }, { 9200, -16 }, { 9200, 1008 },
            };
            snprintf(label, sizeof(label), "rot %u front v%d", rot, i);
            expect_vertex(label, mesh.faces[0].v[i], front[i][0], front[i][1], front[i][2]);
            snprintf(label, sizeof(label), "rot %u back v%d", rot, i);
            expect_vertex(label, mesh.faces[1].v[i], back[i][0], back[i][1], back[i][2]);
            snprintf(label, sizeof(label), "rot %u front tc%d", rot, i);
            expect_texcoord(label, mesh.faces[0].tc[i], expected_tc[i][0], expected_tc[i][1]);
            snprintf(label, sizeof(label), "rot %u back tc%d", rot, i);
            expect_texcoord(label, mesh.faces[1].tc[i], expected_tc[i][0], expected_tc[i][1]);
        }
    }

    mb64_free_render_mesh(&mesh);
}

static void verify_adjacent_fence_uv_phase(void) {
    mb64_level_t level;
    mb64_tile_t tiles[2];
    mb64_mesh_t mesh = { 0 };

    set_level(&level, &tiles[0], 0);
    tiles[1] = tiles[0];
    tiles[1].x = (uint8_t)(tiles[0].x + 1);
    level.header.tile_count = 2;
    level.tiles = tiles;

    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "adjacent fence uv phase: mb64_build_render_mesh failed\n");
        g_failures++;
        return;
    }

    expect_int("adjacent fence face count", (int)mesh.face_count, 4);
    if (mesh.face_count >= 4) {
        expect_int("adjacent fence tile 0 face material", mesh.faces[0].resolved_material, MB64_RENDER_MATERIAL_FENCE);
        expect_int("adjacent fence tile 1 face material", mesh.faces[2].resolved_material, MB64_RENDER_MATERIAL_FENCE);
        expect_int("adjacent fence tile 0 explicit tc", mesh.faces[0].use_tc, 1);
        expect_int("adjacent fence tile 1 explicit tc", mesh.faces[2].use_tc, 1);
        expect_int("adjacent fence u phase", mesh.faces[2].tc[0][0] - mesh.faces[0].tc[0][0], 2048);
    }

    mb64_free_render_mesh(&mesh);
}

static void verify_bars_match_mb64_connection_rendering(void) {
    mb64_level_t level;
    mb64_tile_t tile;
    mb64_mesh_t mesh = { 0 };

    set_tile_level(&level, &tile, TILE_TYPE_BARS, 0);
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "isolated bars: mb64_build_render_mesh failed\n");
        g_failures++;
        return;
    }
    expect_int("isolated bars face count", (int)mesh.face_count, 6);
    if (mesh.face_count == 6) {
        expect_int("isolated bars side material", mesh.faces[0].resolved_material, MB64_RENDER_MATERIAL_BARS);
        expect_int("isolated bars top material", mesh.faces[4].resolved_material, MB64_RENDER_MATERIAL_BARS_TOP);
        expect_int("isolated bars bottom material", mesh.faces[5].resolved_material, MB64_RENDER_MATERIAL_BARS_TOP);
        expect_int("isolated bars side vertical uv span",
                   mesh.faces[0].tc[2][1] - mesh.faces[0].tc[0][1],
                   2048);
    }
    mb64_free_render_mesh(&mesh);

    mb64_tile_t tiles[2];
    set_tile_level(&level, &tiles[0], TILE_TYPE_BARS, 0);
    tiles[1] = tiles[0];
    tiles[1].z = (uint8_t)(tiles[0].z + 1);
    level.header.tile_count = 2;
    level.tiles = tiles;
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "connected bars: mb64_build_render_mesh failed\n");
        g_failures++;
        return;
    }
    expect_int("connected bars face count", (int)mesh.face_count, 18);
    if (mesh.face_count >= 4) {
        expect_int("connected bars first face direction", mesh.faces[0].direction, MB64_MESH_FACE_NEG_X);
        expect_int("connected bars second face direction", mesh.faces[1].direction, MB64_MESH_FACE_POS_X);
        expect_int("connected bars connected side material", mesh.faces[0].resolved_material, MB64_RENDER_MATERIAL_BARS);
    }
    mb64_free_render_mesh(&mesh);
}

static void verify_shared_surface_semantics(void) {
    expect_int("no cam collision base", mb64_surface_has_no_camera_collision(0x0076), 1);
    expect_int("no cam collision unused", mb64_surface_has_no_camera_collision(0x0077), 1);
    expect_int("no cam collision very slippery", mb64_surface_has_no_camera_collision(0x0078), 1);
    expect_int("no cam collision switch", mb64_surface_has_no_camera_collision(0x007A), 1);
    expect_int("no cam collision vanish walls", mb64_surface_has_no_camera_collision(0x007B), 1);
    expect_int("no cam collision ice", mb64_surface_has_no_camera_collision(0x002E), 1);
    expect_int("no cam collision crystal", mb64_surface_has_no_camera_collision(0x0080), 1);
    expect_int("no cam collision hangable", mb64_surface_has_no_camera_collision(0x0005), 1);
    expect_int("cam collides default", mb64_surface_has_no_camera_collision(0x0000), 0);
    expect_int("vanish walls are passable", mb64_surface_is_vanish_cap_passable(0x007B), 1);
    expect_int("hangable mesh is passable", mb64_surface_is_vanish_cap_passable(0x0005), 1);
    expect_int("crystal is passable", mb64_surface_is_vanish_cap_passable(0x0080), 1);
    expect_int("ice is passable", mb64_surface_is_vanish_cap_passable(0x002E), 1);
    expect_int("switch is not vanish passable", mb64_surface_is_vanish_cap_passable(0x007A), 0);
}

static void verify_star_count_helpers(void) {
    mb64_level_t level;
    mb64_obj_t objects[5];

    memset(&level, 0, sizeof(level));
    memset(objects, 0, sizeof(objects));
    level.header.coinstar = 1;
    level.header.object_count = 5;
    level.objects = objects;
    objects[0].type = MB64_OBJECT_TYPE_STAR;
    objects[1].type = MB64_OBJECT_TYPE_RED_COIN_STAR;
    objects[2].type = MB64_OBJECT_TYPE_TRIGGER_STAR;
    objects[3].type = MB64_OBJECT_TYPE_GOOMBA;
    objects[3].imbue = MB64_IMBUE_STAR;
    objects[4].type = MB64_OBJECT_TYPE_GOOMBA;

    expect_int("star object counts", mb64_object_counts_as_star(&objects[0]), 1);
    expect_int("non-star object ignored", mb64_object_counts_as_star(&objects[4]), 0);
    expect_int("level play star count includes coinstar", (int)mb64_level_play_star_count(&level), 5);
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

static int count_faces_for_tile(const mb64_mesh_t *mesh,
                                uint8_t x,
                                uint8_t y,
                                uint8_t z,
                                int direction) {
    int count = 0;
    for (uint32_t i = 0; i < mesh->face_count; i++) {
        const mb64_mesh_face_t *face = &mesh->faces[i];
        if (face->tile_x == x && face->tile_y == y && face->tile_z == z &&
            (direction < 0 || face->direction == direction)) {
            count++;
        }
    }
    return count;
}

static int count_water_faces_for_tile(const mb64_mesh_t *mesh,
                                      uint8_t x,
                                      uint8_t y,
                                      uint8_t z,
                                      int direction) {
    int count = 0;
    for (uint32_t i = 0; i < mesh->face_count; i++) {
        const mb64_mesh_face_t *face = &mesh->faces[i];
        if (face->is_water &&
            face->tile_x == x && face->tile_y == y && face->tile_z == z &&
            (direction < 0 || face->direction == direction)) {
            count++;
        }
    }
    return count;
}

static void expect_face_equal(const char *label,
                              const mb64_mesh_face_t *actual,
                              const mb64_mesh_face_t *expected) {
    expect_int(label, actual->material, expected->material);
    expect_int(label, actual->resolved_material, expected->resolved_material);
    expect_int(label, actual->tile_type, expected->tile_type);
    expect_int(label, actual->direction, expected->direction);
    expect_int(label, actual->is_water, expected->is_water);
    expect_int(label, actual->vertex_count, expected->vertex_count);
    expect_int(label, actual->use_tc, expected->use_tc);
    expect_int(label, actual->tile_x, expected->tile_x);
    expect_int(label, actual->tile_y, expected->tile_y);
    expect_int(label, actual->tile_z, expected->tile_z);
    for (uint8_t i = 0; i < actual->vertex_count; i++) {
        expect_vertex(label, actual->v[i], expected->v[i][0], expected->v[i][1], expected->v[i][2]);
        expect_texcoord(label, actual->tc[i], expected->tc[i][0], expected->tc[i][1]);
    }
}

typedef struct {
    const mb64_mesh_t *mesh;
    uint32_t index;
    uint8_t saw_begin;
    uint8_t saw_end;
    uint8_t saw_water;
} mesh_visitor_test_t;

static int render_mesh_visitor_begin(const mb64_mesh_info_t *info, void *user) {
    mesh_visitor_test_t *ctx = (mesh_visitor_test_t *) user;
    ctx->saw_begin = 1;
    expect_int("visitor face count", info->face_count, ctx->mesh->face_count);
    expect_int("visitor solid tile count", info->solid_tile_count, ctx->mesh->solid_tile_count);
    expect_int("visitor water tile count", info->water_tile_count, ctx->mesh->water_tile_count);
    expect_int("visitor duplicate face count", info->duplicate_face_count, 0);
    return 1;
}

static int render_mesh_visitor_face(const mb64_mesh_face_t *face, void *user) {
    mesh_visitor_test_t *ctx = (mesh_visitor_test_t *) user;
    if (!face->is_water && ctx->saw_water) {
        fprintf(stderr, "visitor emitted solid face after water face\n");
        g_failures++;
        return 0;
    }
    if (face->is_water) {
        ctx->saw_water = 1;
    }
    if (ctx->index >= ctx->mesh->face_count) {
        fprintf(stderr, "visitor emitted too many faces\n");
        g_failures++;
        return 0;
    }
    expect_face_equal("visitor face", face, &ctx->mesh->faces[ctx->index]);
    ctx->index++;
    return 1;
}

static int render_mesh_visitor_end(const mb64_mesh_info_t *info, void *user) {
    mesh_visitor_test_t *ctx = (mesh_visitor_test_t *) user;
    ctx->saw_end = 1;
    expect_int("visitor end face count", info->face_count, ctx->index);
    return 1;
}

static void verify_render_mesh_visitor_matches_snapshot(void) {
    mb64_level_t level;
    mb64_tile_t tiles[2];
    mb64_mesh_t mesh = { 0 };

    memset(&level, 0, sizeof(level));
    memset(tiles, 0, sizeof(tiles));
    level.header.theme = 0;
    level.header.tile_count = 2;
    level.tiles = tiles;

    tiles[0].x = 32;
    tiles[0].y = 2;
    tiles[0].z = 32;
    tiles[0].type = TILE_TYPE_BLOCK;
    tiles[0].mat = MB64_MAT_GRASS;

    tiles[1].x = 33;
    tiles[1].y = 2;
    tiles[1].z = 32;
    tiles[1].type = TILE_TYPE_WATER;
    tiles[1].mat = MB64_MAT_GRASS;

    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "visitor snapshot setup: mb64_build_render_mesh failed\n");
        g_failures++;
        return;
    }

    mesh_visitor_test_t ctx = { &mesh, 0, 0, 0, 0 };
    const mb64_mesh_visitor_t visitor = {
        render_mesh_visitor_begin,
        render_mesh_visitor_face,
        render_mesh_visitor_end,
    };
    if (!mb64_visit_render_mesh(&level, &visitor, &ctx)) {
        fprintf(stderr, "mb64_visit_render_mesh failed\n");
        g_failures++;
    }
    expect_int("visitor begin called", ctx.saw_begin, 1);
    expect_int("visitor end called", ctx.saw_end, 1);
    expect_int("visitor emitted all faces", ctx.index, mesh.face_count);
    expect_int("visitor saw water partition", ctx.saw_water, 1);

    mb64_free_render_mesh(&mesh);
}

static void verify_water_render_predicates(void) {
    mb64_level_t level;
    mb64_tile_t tiles[2];
    mb64_mesh_t mesh = { 0 };

    memset(&level, 0, sizeof(level));
    memset(tiles, 0, sizeof(tiles));
    level.header.theme = MB64_TEST_THEME_CUSTOM;
    level.header.custom_theme.mats[MB64_THEME_MATERIAL_SLOT_GRASS] = MB64_MAT_GRASS;
    level.header.tile_count = 1;
    level.tiles = tiles;

    tiles[0].x = 32;
    tiles[0].y = 2;
    tiles[0].z = 32;
    tiles[0].type = TILE_TYPE_BLOCK;
    tiles[0].mat = MB64_THEME_MATERIAL_SLOT_GRASS;
    tiles[0].waterlogged = 1;
    expect_int("opaque waterlogged block does not render water",
               mb64_tile_renders_water(&level, &tiles[0]), 0);
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "opaque waterlogged block mesh failed\n");
        g_failures++;
        return;
    }
    expect_int("opaque waterlogged block water tiles", mesh.water_tile_count, 0);
    expect_int("opaque waterlogged block water faces",
               count_water_faces_for_tile(&mesh, 32, 2, 32, -1), 0);
    mb64_free_render_mesh(&mesh);

    level.header.custom_theme.mats[MB64_THEME_MATERIAL_SLOT_GRASS] = MB64_MAT_MC_OAK_LEAVES;
    expect_int("cutout waterlogged block renders water",
               mb64_tile_renders_water(&level, &tiles[0]), 1);

    memset(&mesh, 0, sizeof(mesh));
    level.header.custom_theme.mats[MB64_THEME_MATERIAL_SLOT_GRASS] = MB64_MAT_GRASS;
    tiles[0].type = TILE_TYPE_TROLL;
    expect_int("opaque waterlogged troll does not render water",
               mb64_tile_renders_water(&level, &tiles[0]), 0);

    memset(tiles, 0, sizeof(tiles));
    level.header.tile_count = 2;
    tiles[0].x = 32;
    tiles[0].y = 2;
    tiles[0].z = 32;
    tiles[0].type = TILE_TYPE_WATER;
    tiles[0].mat = MB64_THEME_MATERIAL_SLOT_GRASS;
    tiles[1].x = 33;
    tiles[1].y = 2;
    tiles[1].z = 32;
    tiles[1].type = TILE_TYPE_BLOCK;
    tiles[1].mat = MB64_THEME_MATERIAL_SLOT_GRASS;
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "water side culling mesh failed\n");
        g_failures++;
        return;
    }
    expect_int("water next to opaque block has no blocked side",
               count_water_faces_for_tile(&mesh, 32, 2, 32, MB64_MESH_FACE_POS_X), 0);
    expect_int("water next to opaque block still has water faces",
               count_water_faces_for_tile(&mesh, 32, 2, 32, -1) > 0, 1);
    mb64_free_render_mesh(&mesh);
}

static void verify_stacked_water_query_uses_top_surface(void) {
    mb64_level_t level;
    mb64_tile_t tiles[3];
    int surfaceGridY = -1;
    int fullblock = -1;

    memset(&level, 0, sizeof(level));
    memset(tiles, 0, sizeof(tiles));
    level.header.tile_count = 3;
    level.tiles = tiles;

    for (int i = 0; i < 3; i++) {
        tiles[i].x = 32;
        tiles[i].y = (uint8_t)(2 + i);
        tiles[i].z = 32;
        tiles[i].type = TILE_TYPE_WATER;
        tiles[i].waterlogged = 1;
    }

    expect_int("stacked water query from bottom resolves",
               mb64_find_water_query_surface(&level, 32, 2, 32, &surfaceGridY, &fullblock),
               1);
    expect_int("stacked water bottom query uses top surface", surfaceGridY, 4);
    expect_int("stacked water query from above resolves",
               mb64_find_water_query_surface(&level, 32, 5, 32, &surfaceGridY, &fullblock),
               1);
    expect_int("stacked water above query uses top surface", surfaceGridY, 4);
}

static void verify_render_binding_descriptors(void) {
    mb64_level_t level;
    mb64_mesh_face_t face;
    mb64_render_binding_t binding;

    memset(&level, 0, sizeof(level));
    memset(&face, 0, sizeof(face));
    level.header.theme = 0;

    face.resolved_material = MB64_RENDER_MATERIAL_FENCE;
    expect_int("fence binding resolves", mb64_render_binding_for_face(&level, &face, &binding), 1);
    expect_int("fence binding kind", binding.kind, MB64_RENDER_BINDING_FENCE);
    expect_int("fence binding token", binding.token, MB64_FENCE_NORMAL);
    expect_int("fence binding class", binding.render_class, MB64_RENDER_CLASS_CUTOUT);
    expect_int("fence binding culls", binding.cull_backfaces, 1);

    face.resolved_material = MB64_RENDER_MATERIAL_BARS_TOP;
    expect_int("bars top binding resolves", mb64_render_binding_for_face(&level, &face, &binding), 1);
    expect_int("bars top binding kind", binding.kind, MB64_RENDER_BINDING_BARS_TOP);
    expect_int("bars top binding token", binding.token, MB64_BAR_GENERIC);
    expect_int("bars top binding class", binding.render_class, MB64_RENDER_CLASS_CUTOUT_NOCULL);
    expect_int("bars top binding culls", binding.cull_backfaces, 0);

    face.resolved_material = MB64_MAT_MC_GLASS;
    expect_int("glass binding resolves", mb64_render_binding_for_face(&level, &face, &binding), 1);
    expect_int("glass binding kind", binding.kind, MB64_RENDER_BINDING_MATERIAL);
    expect_int("glass binding token", binding.token, MB64_MAT_MC_GLASS);
    expect_int("glass binding class", binding.render_class, MB64_RENDER_CLASS_CUTOUT_NOCULL);
    expect_int("glass binding culls", binding.cull_backfaces, 0);

    memset(&face, 0, sizeof(face));
    face.is_water = 1;
    expect_int("water binding resolves", mb64_render_binding_for_face(&level, &face, &binding), 1);
    expect_int("water binding kind", binding.kind, MB64_RENDER_BINDING_WATER);
    expect_int("water binding token", binding.token, MB64_WATER_DEFAULT);
    expect_int("water binding class", binding.render_class, MB64_RENDER_CLASS_TRANSPARENT);
    expect_int("water binding animates", binding.animation.animated, 1);
}

static void verify_face_surface_descriptors(void) {
    mb64_mesh_face_t face;

    memset(&face, 0, sizeof(face));
    face.tile_type = TILE_TYPE_FENCE;
    face.resolved_material = MB64_MAT_GRASS;
    expect_int("fence face surface", mb64_surface_for_mesh_face(&face), 0x0076);

    memset(&face, 0, sizeof(face));
    face.tile_type = TILE_TYPE_BARS;
    face.resolved_material = MB64_RENDER_MATERIAL_BARS;
    expect_int("bars face surface", mb64_surface_for_mesh_face(&face), 0x007B);

    memset(&face, 0, sizeof(face));
    face.tile_type = TILE_TYPE_BLOCK;
    face.resolved_material = MB64_MAT_ICE;
    expect_int("material face surface",
               mb64_surface_for_mesh_face(&face),
               mb64_surface_for_material(MB64_MAT_ICE));
}

static void verify_shaped_corner_under_block_keeps_shell_faces(void) {
    mb64_level_t level;
    mb64_tile_t tiles[2];
    mb64_mesh_t mesh = { 0 };

    memset(&level, 0, sizeof(level));
    memset(tiles, 0, sizeof(tiles));
    level.header.theme = 0;
    level.header.tile_count = 2;
    level.tiles = tiles;

    tiles[0].x = 32;
    tiles[0].y = 2;
    tiles[0].z = 32;
    tiles[0].type = TILE_TYPE_DCORNER;
    tiles[0].mat = MB64_MAT_GRASS;

    tiles[1] = tiles[0];
    tiles[1].y = (uint8_t)(tiles[0].y + 1);
    tiles[1].type = TILE_TYPE_BLOCK;

    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "shaped corner under block: mb64_build_render_mesh failed\n");
        g_failures++;
        return;
    }

    expect_int("shaped corner under block visible shell faces",
               count_faces_for_tile(&mesh, tiles[0].x, tiles[0].y, tiles[0].z, -1),
               4);
    expect_int("shaped corner under block top occluded",
               count_faces_for_tile(&mesh, tiles[0].x, tiles[0].y, tiles[0].z, MB64_MESH_FACE_TOP),
               0);
    expect_int("shaped corner under block keeps bottom faces",
               count_faces_for_tile(&mesh, tiles[0].x, tiles[0].y, tiles[0].z, MB64_MESH_FACE_BOTTOM),
               2);

    mb64_free_render_mesh(&mesh);
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
    expect_int("woodplat fat uses water physics", mb64_woodplat_should_use_water_physics(1), 1);
    expect_int("woodplat thin stays static", mb64_woodplat_should_use_water_physics(0), 0);
    expect_int("woodplat simple walls normally enabled",
               mb64_woodplat_should_use_simple_wall_checks(0, 0, 1), 1);
    expect_int("woodplat simple walls stay enabled on flat conveyor",
               mb64_woodplat_should_use_simple_wall_checks(1, 0, 1), 1);
    expect_int("woodplat simple walls disabled on upward conveyor while grounded",
               mb64_woodplat_should_use_simple_wall_checks(1, 1, 1), 0);
    expect_int("woodplat simple walls enabled on upward conveyor while airborne",
               mb64_woodplat_should_use_simple_wall_checks(1, 1, 0), 1);
    expect_float("thwomp floor probe offset", mb64_thwomp_floor_probe_offset_y(), 30.0f);
    expect_int("thwomp unknown floor does not die",
               mb64_thwomp_should_die_on_death_barrier(0, 0, 0.0f, 0.0f), 0);
    expect_int("thwomp death plane kills near floor",
               mb64_thwomp_should_die_on_death_barrier(1, 1, 99.0f, 0.0f), 1);
    expect_int("thwomp death plane does not kill when high",
               mb64_thwomp_should_die_on_death_barrier(1, 1, 100.0f, 0.0f), 0);
}

static void verify_looping_platform_helpers(void) {
    const mb64_looping_platform_config_t *config = mb64_looping_platform_config();

    expect_float("looping platform speed", config->forward_vel, 15.0f);
    expect_int("looping platform immediate activation", config->activates_immediately, 1);
    expect_int("looping platform returns to start", config->returns_to_start, 1);
    expect_int("looping platform does not disappear", config->does_not_disappear, 1);
}

static void verify_reinforced_box_helpers(void) {
    const mb64_reinforced_box_config_t *config = mb64_reinforced_box_config();
    const mb64_object_hitbox_t *hitbox = mb64_reinforced_box_hitbox();

    expect_float("rfbox break coin radius", config->break_coin_radius, 46.0f);
    expect_int("rfbox hitbox radius", hitbox->radius, 150);
    expect_int("rfbox hitbox height", hitbox->height, 200);
    expect_int("rfbox hitbox health", hitbox->health, 1);
    expect_int("rfbox init timer", config->init_timer, 15);
    expect_int("rfbox no clank at cooldown", mb64_reinforced_box_should_clank(15), 0);
    expect_int("rfbox clank after cooldown", mb64_reinforced_box_should_clank(16), 1);
    expect_int("rfbox shakes before limit", mb64_reinforced_box_should_shake(9), 1);
    expect_int("rfbox no shake at limit", mb64_reinforced_box_should_shake(10), 0);
    expect_float("rfbox negative shake edge", mb64_reinforced_box_shake_offset(0.0f), -3.0f);
    expect_float("rfbox positive shake edge", mb64_reinforced_box_shake_offset(1.0f), 3.0f);
}

static void verify_breakable_box_helpers(void) {
    const mb64_breakable_box_config_t *config = mb64_breakable_box_config();
    const mb64_object_hitbox_t *hitbox = mb64_breakable_box_hitbox();

    expect_float("breakable box break coin radius", config->break_coin_radius, 46.0f);
    expect_int("breakable box cork anim state", config->cork_anim_state, 1);
    expect_int("breakable box initial loot coins", config->initial_loot_coins, 0);
    expect_float("breakable box imbue drop y offset", config->imbue_drop_y_offset, 150.0f);
    expect_int("breakable box hitbox radius", hitbox->radius, 192);
    expect_int("breakable box hitbox height", hitbox->height, 256);
    expect_int("breakable box hurtbox radius", hitbox->hurtbox_radius, 192);
    expect_int("breakable box hurtbox height", hitbox->hurtbox_height, 256);
}

static void verify_fire_spinner_helpers(void) {
    const mb64_fire_spinner_config_t *config = mb64_fire_spinner_config();

    expect_float("fire spinner first flame distance", config->first_flame_distance, 200.0f);
    expect_float("fire spinner flame spacing", config->flame_spacing, 150.0f);
    expect_float("fire spinner flame y offset", config->flame_y_offset, 100.0f);
    expect_float("fire spinner flame scale", config->flame_scale, 6.0f);
    expect_int("fire spinner rotation speed", config->rotation_speed, -0x100);
    expect_int("fire spinner bparam zero flames", mb64_fire_spinner_flames_per_arm(0), 2);
    expect_int("fire spinner bparam five flames", mb64_fire_spinner_flames_per_arm(5), 7);
}

static void verify_goomba_helpers(void) {
    expect_int("goomba regular size param",
               mb64_goomba_size_param_for_type(MB64_OBJECT_TYPE_GOOMBA),
               MB64_GOOMBA_SIZE_REGULAR);
    expect_int("goomba big size param",
               mb64_goomba_size_param_for_type(MB64_OBJECT_TYPE_BIG_GOOMBA),
               MB64_GOOMBA_SIZE_HUGE);
    expect_int("goomba tiny size param",
               mb64_goomba_size_param_for_type(MB64_OBJECT_TYPE_TINY_GOOMBA),
               MB64_GOOMBA_SIZE_TINY);
}

static void verify_koopa_helpers(void) {
    expect_int("koopa normal behavior param",
               mb64_koopa_behavior_param_for_type(MB64_OBJECT_TYPE_KOOPA, 0),
               MB64_KOOPA_BP_NORMAL);
    expect_int("koopa the quick path zero stays authored",
               mb64_koopa_behavior_param_for_type(MB64_OBJECT_TYPE_KOOPA_THE_QUICK, 0),
               0);
    expect_int("koopa the quick path one stays authored",
               mb64_koopa_behavior_param_for_type(MB64_OBJECT_TYPE_KOOPA_THE_QUICK, 1),
               1);
}

static void expect_bully_variant(const char *name, uint8_t object_type, uint8_t subtype,
                                 uint8_t size_param, uint8_t is_chill, uint8_t is_big) {
    const mb64_bully_variant_t *variant = mb64_bully_variant_for_type(object_type);

    expect_int(name, variant != NULL, 1);
    if (variant == NULL) {
        return;
    }
    expect_int("bully object type", variant->object_type, object_type);
    expect_int("bully subtype", variant->subtype, subtype);
    expect_int("bully size param", variant->size_param, size_param);
    expect_int("bully chill flag", variant->is_chill, is_chill);
    expect_int("bully big flag", variant->is_big, is_big);
}

static void verify_bully_helpers(void) {
    expect_bully_variant("small bully variant",
                         MB64_OBJECT_TYPE_BULLY,
                         MB64_BULLY_SUBTYPE_GENERIC,
                         MB64_BULLY_SIZE_SMALL,
                         0,
                         0);
    expect_bully_variant("small chill bully variant",
                         MB64_OBJECT_TYPE_CHILL_BULLY,
                         MB64_BULLY_SUBTYPE_CHILL,
                         MB64_BULLY_SIZE_SMALL,
                         1,
                         0);
    expect_bully_variant("big bully variant",
                         MB64_OBJECT_TYPE_BIG_BULLY,
                         MB64_BULLY_SUBTYPE_GENERIC,
                         MB64_BULLY_SIZE_BIG,
                         0,
                         1);
    expect_bully_variant("big chill bully variant",
                         MB64_OBJECT_TYPE_BIG_CHILL_BULLY,
                         MB64_BULLY_SUBTYPE_CHILL,
                         MB64_BULLY_SIZE_BIG,
                         1,
                         1);
    expect_int("goomba is not bully variant",
               mb64_bully_variant_for_type(MB64_OBJECT_TYPE_GOOMBA) == NULL,
               1);
    expect_int("bully predicate accepts variant",
               mb64_object_type_is_bully_variant(MB64_OBJECT_TYPE_BIG_CHILL_BULLY),
               1);
    expect_int("bully predicate rejects non variant",
               mb64_object_type_is_bully_variant(MB64_OBJECT_TYPE_GOOMBA),
               0);
}

static void verify_bullet_bill_helpers(void) {
    const mb64_bullet_bill_config_t *config = mb64_bullet_bill_config();
    const mb64_object_hitbox_t *hitbox = mb64_bullet_bill_hitbox();

    expect_float("bullet bill wake min", config->wake_min_distance, 400.0f);
    expect_int("bullet bill hitbox radius", hitbox->radius, 200);
    expect_int("bullet bill hitbox down offset", hitbox->down_offset, 250);
    expect_int("bullet bill hitbox damage", hitbox->damage_or_coin_value, 2);
    expect_int("bullet bill no wake too close", mb64_bullet_bill_should_wake(0x1000, 399.0f), 0);
    expect_int("bullet bill wakes in range", mb64_bullet_bill_should_wake(0x1000, 401.0f), 1);
}

static void verify_exclamation_box_helpers(void) {
    const mb64_exclamation_box_config_t *config = mb64_exclamation_box_config();
    const mb64_exclamation_box_content_t *vanilla_metal =
        mb64_exclamation_box_content(MB64_GAME_VANILLA, 1);
    const mb64_exclamation_box_content_t *vanilla_ten =
        mb64_exclamation_box_content(MB64_GAME_VANILLA, 6);
    const mb64_exclamation_box_content_t *btcm_green =
        mb64_exclamation_box_content(MB64_GAME_BTCM, 3);

    expect_int("exclamation box content count", MB64_EXCLAMATION_BOX_TYPE_COUNT, 7);
    expect_int("exclamation box init action", MB64_EXCLAMATION_BOX_ACT_INIT, 0);
    expect_int("exclamation box active action", MB64_EXCLAMATION_BOX_ACT_ACTIVE, 2);
    expect_int("exclamation box scale angle start", config->scale_angle_start, 0x4000);
    expect_int("exclamation box scale angle step", config->scale_angle_step, 0x1000);
    expect_int("exclamation box no explode before frame", mb64_exclamation_box_should_explode(6), 0);
    expect_int("exclamation box explode frame", mb64_exclamation_box_should_explode(7), 1);
    expect_int("exclamation box no respawn at frame", mb64_exclamation_box_should_respawn(300), 0);
    expect_int("exclamation box respawn after frame", mb64_exclamation_box_should_respawn(301), 1);
    expect_float("exclamation box launch vel", config->hit_launch_vel_y, 30.0f);
    expect_float("exclamation box gravity", config->hit_gravity, -8.0f);
    expect_float("exclamation box squash factor", config->squash_scale_factor, 0.3f);
    expect_float("exclamation box stretch factor", config->stretch_scale_factor, 0.5f);
    expect_float("exclamation box stretch offset", config->stretch_scale_offset, 1.0f);
    expect_float("exclamation box graph y offset factor", config->graph_y_offset_factor, 26.0f);
    expect_float("exclamation box model scale", config->model_scale, 2.0f);
    expect_int("exclamation box invalid content", mb64_exclamation_box_content(MB64_GAME_VANILLA, 7) == NULL, 1);
    expect_int("vanilla metal box beh param", vanilla_metal->beh_params, 6);
    expect_int("vanilla metal box model", vanilla_metal->model, MB64_EXCLAMATION_BOX_MODEL_METAL_CAP);
    expect_int("vanilla metal box behavior", vanilla_metal->behavior, MB64_EXCLAMATION_BOX_BEHAVIOR_METAL_CAP);
    expect_int("vanilla metal box anim", vanilla_metal->anim_state, MB64_EXCLAMATION_BOX_ANIM_GREEN);
    expect_int("vanilla metal box respawns", vanilla_metal->do_respawn, 1);
    expect_int("vanilla ten coin count", vanilla_ten->num_coins, 10);
    expect_int("vanilla ten coin behavior", vanilla_ten->behavior, MB64_EXCLAMATION_BOX_BEHAVIOR_TEN_COINS);
    expect_int("btcm green box model", btcm_green->model, MB64_EXCLAMATION_BOX_MODEL_GREEN_COIN);
    expect_int("btcm green box behavior", btcm_green->behavior, MB64_EXCLAMATION_BOX_BEHAVIOR_GREEN_COIN);
    expect_int("btcm green box anim", btcm_green->anim_state, MB64_EXCLAMATION_BOX_ANIM_GREEN_COIN);
    expect_int("btcm green coin count", btcm_green->num_coins, 3);
}

static void verify_floor_switch_helpers(void) {
    const mb64_floor_switch_config_t *config = mb64_floor_switch_config();

    expect_int("floor switch scale frames", config->scale_frames, 3);
    expect_int("floor switch timer", config->timer_frames, 400);
    expect_int("floor switch double time timer", config->double_time_timer_frames, 800);
    expect_int("floor switch blink frames", config->hidden_box_blink_frames, 40);
    expect_float("floor switch scale", config->switch_scale, 1.28f);
    expect_float("floor switch pressed scale", config->pressed_scale, 0.2f);
    expect_float("floor switch press radius", config->press_radius, 127.5f);
    expect_int("floor switch default hidden timer", mb64_floor_switch_hidden_box_timer(0), 400);
    expect_int("floor switch double time hidden timer", mb64_floor_switch_hidden_box_timer(1), 800);
    expect_int("floor switch default fast tick threshold", mb64_floor_switch_fast_tick_threshold(0), 360);
    expect_int("floor switch double time fast tick threshold", mb64_floor_switch_fast_tick_threshold(1), 760);
    expect_int("floor switch press inside radius", mb64_floor_switch_should_press(127.4f), 1);
    expect_int("floor switch no press at radius", mb64_floor_switch_should_press(127.5f), 0);
    expect_int("floor switch no scale done before frame", mb64_floor_switch_scale_done(2), 0);
    expect_int("floor switch scale done at frame", mb64_floor_switch_scale_done(3), 1);
    expect_int("floor switch no timeout at frame", mb64_floor_switch_should_timeout(400, 0), 0);
    expect_int("floor switch timeout after frame", mb64_floor_switch_should_timeout(401, 0), 1);
    expect_int("floor switch no double timeout at frame", mb64_floor_switch_should_timeout(800, 1), 0);
    expect_int("floor switch double timeout after frame", mb64_floor_switch_should_timeout(801, 1), 1);
    expect_int("hidden box blinks on odd final frames", mb64_hidden_box_should_blink(39), 1);
    expect_int("hidden box no blink on even final frames", mb64_hidden_box_should_blink(38), 0);
    expect_int("hidden box no blink before final window", mb64_hidden_box_should_blink(40), 0);
    expect_int("hidden box no blink when inactive", mb64_hidden_box_should_blink(0), 0);
    expect_int("timed block solid when timer inactive", mb64_timed_block_is_solid(0), 1);
    expect_int("timed block not solid when timer active", mb64_timed_block_is_solid(400), 0);
    expect_int("timed block shows on model when timer inactive", mb64_timed_block_show_on_model(0), 1);
    expect_int("timed block shows off model before blink window", mb64_timed_block_show_on_model(40), 0);
    expect_int("timed block blinks on model on odd final frames", mb64_timed_block_show_on_model(39), 1);
    expect_int("timed block blinks off model on even final frames", mb64_timed_block_show_on_model(38), 0);
}

static void verify_conveyor_helpers(void) {
    expect_int("conveyor half shape", mb64_conveyor_shape(0), MB64_CONVEYOR_SHAPE_HALF);
    expect_int("conveyor flat shape", mb64_conveyor_shape(1), MB64_CONVEYOR_SHAPE_FLAT);
    expect_int("conveyor slope shape", mb64_conveyor_shape(2), MB64_CONVEYOR_SHAPE_SLOPE);
    expect_int("conveyor downslope shape", mb64_conveyor_shape(3), MB64_CONVEYOR_SHAPE_DOWNSLOPE);
    expect_int("conveyor red state", mb64_conveyor_state(5), MB64_CONVEYOR_STATE_RED);
    expect_int("conveyor blue state", mb64_conveyor_state(10), MB64_CONVEYOR_STATE_BLUE);
    expect_int("conveyor inactive slope stays slope", mb64_conveyor_effective_shape(2, 0), MB64_CONVEYOR_SHAPE_SLOPE);
    expect_int("conveyor active slope flips down", mb64_conveyor_effective_shape(6, 1), MB64_CONVEYOR_SHAPE_DOWNSLOPE);
    expect_int("conveyor inactive downslope flips up", mb64_conveyor_effective_shape(7, 0), MB64_CONVEYOR_SHAPE_DOWNSLOPE);
    expect_int("conveyor active downslope flips up", mb64_conveyor_effective_shape(7, 1), MB64_CONVEYOR_SHAPE_SLOPE);
    expect_int("conveyor effective bparam preserves state bits", mb64_conveyor_effective_bparam(6, 1), 7);
    expect_int("conveyor half has no vertical push", mb64_conveyor_has_vertical_push(0, 0), 0);
    expect_int("conveyor slope has vertical push", mb64_conveyor_has_vertical_push(2, 0), 1);
    expect_int("conveyor switched slope has vertical push", mb64_conveyor_has_vertical_push(6, 1), 1);
    expect_int("conveyor initial slope push", mb64_conveyor_initial_vertical_push(2), 1);
    expect_int("conveyor initial downslope push", mb64_conveyor_initial_vertical_push(3), -1);
    expect_int("conveyor no flip when always active", mb64_conveyor_should_flip_state(0, 0), 0);
    expect_int("conveyor no flip when state matches", mb64_conveyor_should_flip_state(1, 0), 0);
    expect_int("conveyor flips when state changes", mb64_conveyor_should_flip_state(2, 0), 1);
}

static void verify_noteblock_helpers(void) {
    const mb64_noteblock_config_t *config = mb64_noteblock_config();

    expect_int("noteblock graph angle step", config->graph_angle_step, 5000);
    expect_int("noteblock min health", config->min_health, 0x100);
    expect_float("noteblock velocity decay", config->velocity_decay, 0.95f);
    expect_float("noteblock graph bounce velocity", config->bounce_graph_vel_y, 50.0f);
    expect_float("noteblock mario bounce velocity", config->bounce_mario_vel_y, 95.0f);
    expect_float("noteblock model scale", config->model_scale, MB64_NOTEBLOCK_MODEL_SCALE);
    expect_float("noteblock collision half height",
                 config->collision_half_height,
                 MB64_NOTEBLOCK_COLLISION_HALF_HEIGHT);
    expect_float("noteblock collision top offset", config->collision_top_y_offset, 127.0f);
    expect_int("noteblock graph angle", mb64_noteblock_graph_angle(2), 10000);
    expect_float("noteblock velocity decay helper", mb64_noteblock_next_velocity(50.0f), 47.5f);
    expect_int("noteblock bounces on valid platform", mb64_noteblock_should_bounce(0, 0, 0x101, 1), 1);
    expect_int("noteblock no bounce when intangible", mb64_noteblock_should_bounce(1, 0, 0x101, 1), 0);
    expect_int("noteblock no bounce when swimming", mb64_noteblock_should_bounce(0, 1, 0x101, 1), 0);
    expect_int("noteblock no bounce at min health", mb64_noteblock_should_bounce(0, 0, 0x100, 1), 0);
    expect_int("noteblock no bounce off platform", mb64_noteblock_should_bounce(0, 0, 0x101, 0), 0);
}

static void verify_onoff_helpers(void) {
    const mb64_onoff_config_t *config = mb64_onoff_config();

    expect_float("onoff pressed scale factor", config->pressed_scale_factor, 0.1f);
    expect_float("onoff scale step factor", config->scale_step_factor, 0.1f);
    expect_float("onoff collision min scale", config->collision_min_scale_y, 0.11f);
    expect_float("onoff pressed scale", mb64_onoff_button_pressed_scale(2.0f), 0.2f);
    expect_float("onoff scale step", mb64_onoff_button_scale_step(2.0f), 0.2f);
    expect_float("onoff collision min", mb64_onoff_button_collision_min_scale(2.0f), 0.22f);
    expect_int("onoff red initial anim", mb64_onoff_button_initial_anim_state(0), 0);
    expect_int("onoff blue initial anim", mb64_onoff_button_initial_anim_state(1), 1);
    expect_int("onoff red pressed when off", mb64_onoff_button_is_pressed(0, 0), 1);
    expect_int("onoff red up when on", mb64_onoff_button_is_pressed(0, 1), 0);
    expect_int("onoff blue pressed when on", mb64_onoff_button_is_pressed(1, 1), 1);
    expect_int("onoff red should rise when on", mb64_onoff_button_should_rise(0, 1), 1);
    expect_int("onoff blue should rise when off", mb64_onoff_button_should_rise(1, 0), 1);
    expect_int("onoff state from red", mb64_onoff_state_from_bparam(0), 0);
    expect_int("onoff state from blue", mb64_onoff_state_from_bparam(1), 1);
    expect_int("onoff red block active when off", mb64_onoff_block_is_active(0, 0), 1);
    expect_int("onoff blue block active when on", mb64_onoff_block_is_active(1, 1), 1);
}

static void verify_badge_helpers(void) {
    const mb64_badge_config_t *config = mb64_badge_config();

    expect_int("badge spin accel", config->spin_accel, 0x70);
    expect_float("badge shrink factor", config->shrink_factor, 0.95f);
    expect_float("badge delete scale", config->delete_scale, 0.2f);
    expect_int("badge equipped bit set", mb64_badge_is_equipped(1u << 4, 4), 1);
    expect_int("badge equipped bit clear", mb64_badge_is_equipped(1u << 4, 3), 0);
    expect_int("badge rejects out of range id", mb64_badge_is_equipped(0xffffffffu, 32), 0);
    expect_int("badge collects if already equipped", mb64_badge_should_collect(1, 0, 0), 1);
    expect_int("badge collects on touch", mb64_badge_should_collect(0, 1, 0), 1);
    expect_int("badge waits during levelup", mb64_badge_should_collect(0, 1, 1), 0);
    expect_float("badge next scale", mb64_badge_next_collect_scale(1.0f), 0.95f);
    expect_int("badge delete under threshold", mb64_badge_should_delete(0.19f), 1);
    expect_int("badge keep at threshold", mb64_badge_should_delete(0.2f), 0);
}

static void verify_green_coin_helpers(void) {
    const mb64_green_coin_config_t *config = mb64_green_coin_config();

    expect_float("green coin value", config->damage_or_coin_value, 3.0f);
    expect_float("green coin hitbox radius", config->hitbox_radius, 100.0f);
    expect_float("green coin hitbox height", config->hitbox_height, 64.0f);
}

static void verify_powerup_helpers(void) {
    const mb64_powerup_config_t *config = mb64_powerup_config();

    expect_int("powerup crowbar bit", config->crowbar_power_bit, 1);
    expect_int("powerup mask bit", config->mask_power_bit, 2);
    expect_int("powerup sparkle mask", config->sparkle_timer_mask, 3);
    expect_int("powerup face pitch", config->face_pitch, 0x1A00);
    expect_int("powerup yaw step", config->yaw_step, 0x400);
    expect_int("powerup respawn frames", config->respawn_frames, 150);
    expect_float("powerup sparkle distance", config->sparkle_distance, 4000.0f);
    expect_float("powerup drawing distance", config->drawing_distance, 6000.0f);
    expect_float("powerup hitbox radius", config->hitbox_radius, 80.0f);
    expect_float("powerup hitbox height", config->hitbox_height, 160.0f);
    expect_float("powerup hitbox offset", config->hitbox_down_offset, 80.0f);
    expect_float("powerup mask graph offset", config->mask_graph_y_offset, -80.0f);
    expect_int("powerup bparam crowbar", mb64_powerup_bit_for_bparam(0), 1);
    expect_int("powerup bparam mask", mb64_powerup_bit_for_bparam(1), 2);
    expect_int("powerup sparkle near on cadence", mb64_powerup_should_sparkle(3999.0f, 4), 1);
    expect_int("powerup no sparkle far", mb64_powerup_should_sparkle(4000.0f, 4), 0);
    expect_int("powerup no sparkle off cadence", mb64_powerup_should_sparkle(3999.0f, 5), 0);
    expect_int("powerup no respawn at frame", mb64_powerup_should_respawn(150), 0);
    expect_int("powerup respawn after frame", mb64_powerup_should_respawn(151), 1);
}

static void verify_phantasm_helpers(void) {
    const mb64_phantasm_config_t *config = mb64_phantasm_config();

    expect_int("phantasm default health", mb64_phantasm_initial_health(0), 3);
    expect_int("phantasm dispensed health", mb64_phantasm_initial_health(2), 2);
    expect_int("phantasm default coins", mb64_phantasm_initial_loot_coins(0), 5);
    expect_int("phantasm dispensed coins", mb64_phantasm_initial_loot_coins(2), 0);
    expect_int("phantasm facing range", config->facing_angle_range, 0x2000);
    expect_float("phantasm facing attack distance", config->facing_attack_distance, 1000.0f);
    expect_float("phantasm close attack distance", config->close_attack_distance, 400.0f);
    expect_int("phantasm no wander at timer", mb64_phantasm_should_wander(100), 0);
    expect_int("phantasm wander after timer", mb64_phantasm_should_wander(101), 1);
    expect_int("phantasm attack facing near", mb64_phantasm_should_attack(1, 999.0f), 1);
    expect_int("phantasm no attack facing far", mb64_phantasm_should_attack(1, 1000.0f), 0);
    expect_int("phantasm attack close", mb64_phantasm_should_attack(0, 399.0f), 1);
    expect_int("phantasm fireball interval", mb64_phantasm_should_throw_fireball(25), 1);
    expect_int("phantasm fireball outside window", mb64_phantasm_should_throw_fireball(150), 0);
    expect_int("phantasm end fireballs", mb64_phantasm_should_end_fireball_attack(161), 1);
    expect_int("phantasm death check", mb64_phantasm_should_die_after_hit(30, 0), 1);
    expect_int("phantasm recover", mb64_phantasm_should_recover_after_hit(51), 1);
    expect_float("phantasm kick capped", mb64_phantasm_kick_forward_vel(1000.0f), 75.0f);
    expect_int("phantasm ledge guard", mb64_phantasm_should_prevent_ledge_drop(1000.0f, 699.0f), 1);
}

static void verify_motos_helpers(void) {
    const mb64_motos_config_t *config = mb64_motos_config();

    expect_float("motos scale", config->scale, 2.0f);
    expect_float("motos hand relative x", config->hand_relative_x, -70.0f);
    expect_float("motos hand relative y", config->hand_relative_y, -30.0f);
    expect_float("motos search speed", config->search_forward_vel, 5.0f);
    expect_int("motos turn speed", config->search_turn_speed, 800);
    expect_int("motos no search at boundary", mb64_motos_should_search(1000.0f), 0);
    expect_int("motos search below boundary", mb64_motos_should_search(999.0f), 1);
    expect_int("motos no stop at boundary", mb64_motos_should_stop_searching(1500.0f), 0);
    expect_int("motos stop above boundary", mb64_motos_should_stop_searching(1501.0f), 1);
    expect_int("motos no timed throw at boundary", mb64_motos_should_throw(45, 0), 0);
    expect_int("motos timed throw after boundary", mb64_motos_should_throw(46, 0), 1);
    expect_int("motos edge throw", mb64_motos_should_throw(0, 1), 1);
    expect_int("motos no escape at boundary", mb64_motos_escape_succeeds(20), 0);
    expect_int("motos escape after boundary", mb64_motos_escape_succeeds(21), 1);
    expect_int("motos recover wait", mb64_motos_should_leave_recover_wait(36), 1);
}

static void verify_chicken_helpers(void) {
    const mb64_chicken_config_t *config = mb64_chicken_config();

    expect_int("chicken bparam2", config->behavior_param_2, 1);
    expect_int("chicken animation", config->animation_index, 0);
    expect_float("chicken scale", config->scale, 1.0f);
    expect_float("chicken wall hitbox", config->wall_hitbox_radius, 50.0f);
    expect_float("chicken gravity", config->gravity, 0.0f);
    expect_float("chicken bounciness", config->bounciness, 0.0f);
    expect_float("chicken drag", config->drag_strength, 1000.0f);
    expect_float("chicken friction", config->friction, 1000.0f);
    expect_float("chicken buoyancy", config->buoyancy, 0.0f);
    expect_float("chicken draw distance", config->draw_distance, 4000.0f);
}

static void verify_fire_bro_helpers(void) {
    const mb64_fire_bro_config_t *config = mb64_fire_bro_config();
    const mb64_object_hitbox_t *hitbox = mb64_fire_bro_hitbox();

    expect_int("fire bro bparam2", config->behavior_param_2, 1);
    expect_int("fire bro idle anim", config->anim_idle, 0);
    expect_int("fire bro throw anim", config->anim_throw, 2);
    expect_int("fire bro jump anim", config->anim_jump, 3);
    expect_float("fire bro scale", config->scale, 1.0f);
    expect_float("fire bro wall hitbox", config->wall_hitbox_radius, 80.0f);
    expect_float("fire bro gravity", config->gravity, -4.0f);
    expect_float("fire bro projectile y offset", config->projectile_y_offset, 20.0f);
    expect_float("fire bro projectile vel y", config->fireball_vel_y, 20.0f);
    expect_float("fire bro projectile fvel", config->fireball_forward_vel, 30.0f);
    expect_int("fire bro hitbox damage", hitbox->damage_or_coin_value, 2);
    expect_int("fire bro hitbox health", hitbox->health, 1);
    expect_int("fire bro loot coins", hitbox->num_loot_coins, 6);
    expect_int("fire bro hitbox radius", hitbox->radius, 80);
    expect_int("fire bro hitbox height", hitbox->height, 140);
    expect_int("fire bro hurtbox radius", hitbox->hurtbox_radius, 90);
    expect_int("fire bro hurtbox height", hitbox->hurtbox_height, 130);
    expect_int("fire bro cannot throw far below", mb64_fire_bro_can_throw(0.0f, 200.0f), 0);
    expect_int("fire bro can throw above cutoff", mb64_fire_bro_can_throw(0.1f, 200.0f), 1);
    expect_int("fire bro no throw far", mb64_fire_bro_should_start_throw(1500.1f, 41), 0);
    expect_int("fire bro no throw at frame", mb64_fire_bro_should_start_throw(1500.0f, 40), 0);
    expect_int("fire bro throw after frame", mb64_fire_bro_should_start_throw(1500.0f, 41), 1);
    expect_int("fire bro hold frame", mb64_fire_bro_should_leave_hold(15), 0);
    expect_int("fire bro leave hold", mb64_fire_bro_should_leave_hold(16), 1);
    expect_int("fire bro repeat frame", mb64_fire_bro_should_repeat_or_jump(25), 0);
    expect_int("fire bro repeat after", mb64_fire_bro_should_repeat_or_jump(26), 1);
    expect_int("fireball timeout frame", mb64_fire_bro_should_delete_fireball(300, 0), 0);
    expect_int("fireball timeout after", mb64_fire_bro_should_delete_fireball(301, 0), 1);
    expect_int("fireball wall delete", mb64_fire_bro_should_delete_fireball(1, 1), 1);
    expect_int("fireball no bounce", mb64_fire_bro_should_bounce_fireball(0), 0);
    expect_int("fireball ground bounce", mb64_fire_bro_should_bounce_fireball(1), 1);
}

static void verify_hammer_bro_helpers(void) {
    const mb64_hammer_bro_config_t *config = mb64_hammer_bro_config();
    const mb64_object_hitbox_t *hammer_hitbox = mb64_hammer_projectile_hitbox();

    expect_int("hammer bro idle anim", config->anim_idle, 0);
    expect_int("hammer bro throw anim", config->anim_throw, 2);
    expect_int("hammer bro jump anim", config->anim_jump, 3);
    expect_float("hammer bro scale", config->scale, 1.0f);
    expect_float("hammer bro wall hitbox", config->wall_hitbox_radius, 80.0f);
    expect_float("hammer bro gravity", config->gravity, -4.0f);
    expect_float("hammer bro projectile y offset", config->projectile_y_offset, 20.0f);
    expect_float("hammer bro projectile quicksand scale", config->projectile_quicksand_y_scale, 2.0f);
    expect_float("hammer bro hammer y min", config->hammer_vel_y_min, 30.0f);
    expect_float("hammer bro hammer y max", config->hammer_vel_y_max, 50.0f);
    expect_float("hammer bro hammer fvel min", config->hammer_forward_vel_min, 20.0f);
    expect_float("hammer bro hammer fvel max", config->hammer_forward_vel_max, 50.0f);
    expect_int("hammer projectile hitbox damage", hammer_hitbox->damage_or_coin_value, 2);
    expect_int("hammer projectile loot coins", hammer_hitbox->num_loot_coins, 7);
    expect_int("hammer projectile down offset", hammer_hitbox->down_offset, 40);
    expect_int("hammer projectile radius", hammer_hitbox->radius, 40);
    expect_int("hammer projectile height", hammer_hitbox->height, 80);
    expect_int("hammer bro cannot throw far below", mb64_hammer_bro_can_throw(0.0f, 200.0f), 0);
    expect_int("hammer bro can throw above cutoff", mb64_hammer_bro_can_throw(0.1f, 200.0f), 1);
    expect_int("hammer bro no throw far", mb64_hammer_bro_should_start_throw(1500.1f, 41), 0);
    expect_int("hammer bro no throw at frame", mb64_hammer_bro_should_start_throw(1500.0f, 40), 0);
    expect_int("hammer bro throw after frame", mb64_hammer_bro_should_start_throw(1500.0f, 41), 1);
    expect_int("hammer bro hold frame", mb64_hammer_bro_should_leave_hold(15), 0);
    expect_int("hammer bro leave hold", mb64_hammer_bro_should_leave_hold(16), 1);
    expect_int("hammer bro repeat frame", mb64_hammer_bro_should_repeat_or_jump(25), 0);
    expect_int("hammer bro repeat after", mb64_hammer_bro_should_repeat_or_jump(26), 1);
    expect_int("hammer random min", mb64_hammer_bro_random_range(0, 30, 50), 30);
    expect_int("hammer random max", mb64_hammer_bro_random_range(20, 30, 50), 50);
    expect_int("hammer quicksand clamp", mb64_hammer_bro_hurt_quicksand_depth(10), 0);
    expect_int("hammer quicksand step", mb64_hammer_bro_hurt_quicksand_depth(30), 15);
    expect_float("hammer projectile quicksand offset", mb64_hammer_bro_projectile_y_offset(7.0f), 6.0f);
    expect_int("hammer not armed before frame", mb64_hammer_should_arm_hitbox(19), 0);
    expect_int("hammer armed at frame", mb64_hammer_should_arm_hitbox(20), 1);
    expect_int("hammer timeout frame", mb64_hammer_should_delete(300, 0, 0, 0), 0);
    expect_int("hammer timeout after", mb64_hammer_should_delete(301, 0, 0, 0), 1);
    expect_int("hammer ground delete", mb64_hammer_should_delete(1, 1, 0, 0), 1);
    expect_int("hammer wall delete", mb64_hammer_should_delete(1, 1u << 9, 0, 0), 1);
}

static void verify_crablet_helpers(void) {
    const mb64_crablet_config_t *config = mb64_crablet_config();
    const mb64_object_hitbox_t *hitbox = mb64_crablet_hitbox();

    expect_int("crablet anim", config->animation_index, 0);
    expect_float("crablet scale", config->scale, 1.0f);
    expect_float("crablet wall hitbox", config->wall_hitbox_radius, 130.0f);
    expect_float("crablet walk speed", config->walk_forward_vel, 5.0f);
    expect_float("crablet attack speed", config->attack_forward_vel, 30.0f);
    expect_float("crablet attack jump", config->attack_jump_vel_y, 50.0f);
    expect_int("crablet spawn action", MB64_CRABLET_ACT_SPAWN, 0);
    expect_int("crablet patrol action", MB64_CRABLET_ACT_PATROL, 1);
    expect_int("crablet attack active state", MB64_CRABLET_ATTACK_ACTIVE, 1);
    expect_int("crablet hitbox damage", hitbox->damage_or_coin_value, 1);
    expect_int("crablet hitbox health", hitbox->health, 1);
    expect_int("crablet loot coins", hitbox->num_loot_coins, 3);
    expect_int("crablet hitbox radius", hitbox->radius, 130);
    expect_int("crablet hitbox height", hitbox->height, 70);
    expect_int("crablet hurtbox radius", hitbox->hurtbox_radius, 90);
    expect_int("crablet hurtbox height", hitbox->hurtbox_height, 60);
    expect_int("crablet no attack angle", mb64_crablet_should_attack(0x2000, 999.0f), 0);
    expect_int("crablet no attack distance", mb64_crablet_should_attack(0x1fff, 1000.0f), 0);
    expect_int("crablet attack", mb64_crablet_should_attack(0x1fff, 999.0f), 1);
    expect_int("crablet attack timer frame", mb64_crablet_should_end_attack(50), 0);
    expect_int("crablet attack timer after", mb64_crablet_should_end_attack(51), 1);
    expect_int("crablet recover timer frame", mb64_crablet_should_finish_recovery(30), 0);
    expect_int("crablet recover timer after", mb64_crablet_should_finish_recovery(31), 1);
    expect_int("crablet no grab if held", mb64_crablet_should_grab_head(100.0f, 100.0f, 0.0f, 1), 0);
    expect_int("crablet no grab low", mb64_crablet_should_grab_head(100.0f, 30.0f, 0.0f, 0), 0);
    expect_int("crablet grab", mb64_crablet_should_grab_head(199.0f, 31.0f, 0.0f, 0), 1);
    expect_int("crablet quicksand clamp low", mb64_crablet_hurt_quicksand_depth(10), 0);
    expect_int("crablet quicksand step", mb64_crablet_hurt_quicksand_depth(40), 25);
}

static void verify_rex_helpers(void) {
    const mb64_rex_config_t *config = mb64_rex_config();

    expect_int("rex health", config->health, 1);
    expect_int("rex animation", config->animation_index, 0);
    expect_float("rex scale", config->scale, 1.5f);
    expect_float("rex graph y offset", config->graph_y_offset, -30.0f);
    expect_float("rex draw distance", config->draw_distance, 4000.0f);
    expect_float("rex wall hitbox", config->wall_hitbox_radius, 40.0f);
    expect_float("rex gravity", config->gravity, -4.0f);
    expect_float("rex bounciness", config->bounciness, -0.5f);
    expect_float("rex drag", config->drag_strength, 10.0f);
    expect_float("rex friction", config->friction, 10.0f);
    expect_float("rex buoyancy", config->buoyancy, 2.0f);
}

static void verify_npc_helpers(void) {
    const mb64_npc_config_t *moleman = mb64_moleman_config();
    const mb64_npc_config_t *cobie = mb64_cobie_config();
    const mb64_npc_config_t *toad = mb64_toad_config();
    const mb64_npc_config_t *tuxie = mb64_tuxie_config();
    const mb64_npc_config_t *ukiki = mb64_ukiki_config();

    expect_int("moleman animation", moleman->animation_index, 0);
    expect_int("moleman role", moleman->bobomb_buddy_role, 0);
    expect_int("moleman no forced anim", moleman->forced_anim_state, -1);
    expect_float("moleman graph y offset", moleman->graph_y_offset, 65.0f);
    expect_float("moleman hitbox radius", moleman->hitbox_radius, 100.0f);
    expect_float("moleman hitbox height", moleman->hitbox_height, 60.0f);
    expect_float("moleman draw distance", moleman->draw_distance, 4000.0f);

    expect_int("cobie animation", cobie->animation_index, 0);
    expect_int("cobie role", cobie->bobomb_buddy_role, 0);
    expect_int("cobie forced anim", cobie->forced_anim_state, 0);
    expect_float("cobie graph y offset", cobie->graph_y_offset, 0.0f);
    expect_float("cobie hitbox radius", cobie->hitbox_radius, 130.0f);
    expect_float("cobie hitbox height", cobie->hitbox_height, 60.0f);
    expect_float("cobie draw distance", cobie->draw_distance, 4000.0f);

    expect_int("toad animation", toad->animation_index, 6);
    expect_int("toad role", toad->bobomb_buddy_role, 0);
    expect_int("toad no forced anim", toad->forced_anim_state, -1);
    expect_float("toad hitbox radius", toad->hitbox_radius, 100.0f);
    expect_float("toad hitbox height", toad->hitbox_height, 60.0f);
    expect_float("toad draw distance", toad->draw_distance, 6000.0f);

    expect_int("tuxie animation", tuxie->animation_index, 0);
    expect_int("tuxie role", tuxie->bobomb_buddy_role, 0);
    expect_float("tuxie hitbox radius", tuxie->hitbox_radius, 100.0f);
    expect_float("tuxie hitbox height", tuxie->hitbox_height, 60.0f);
    expect_float("tuxie draw distance", tuxie->draw_distance, 6000.0f);

    expect_int("ukiki animation", ukiki->animation_index, 4);
    expect_int("ukiki role", ukiki->bobomb_buddy_role, 0);
    expect_float("ukiki hitbox radius", ukiki->hitbox_radius, 100.0f);
    expect_float("ukiki hitbox height", ukiki->hitbox_height, 60.0f);
    expect_float("ukiki draw distance", ukiki->draw_distance, 6000.0f);
}

static void verify_podoboo_helpers(void) {
    const mb64_podoboo_config_t *config = mb64_podoboo_config();

    expect_float("podoboo gravity", config->gravity, 2.0f);
    expect_float("podoboo launch accel", config->launch_accel, 1.5f);
    expect_int("podoboo landing roll", config->landing_roll_angle, 0x7FFF);
    expect_int("podoboo roll step", config->roll_step, 0x0FFF);
    expect_int("podoboo idle reset far", mb64_podoboo_should_reset_idle_timer(2500.1f), 1);
    expect_int("podoboo idle no reset near", mb64_podoboo_should_reset_idle_timer(2500.0f), 0);
    expect_int("podoboo no warmup flame at frame", mb64_podoboo_should_spawn_warmup_flame(35), 0);
    expect_int("podoboo warmup flame after frame", mb64_podoboo_should_spawn_warmup_flame(36), 1);
    expect_int("podoboo no launch at frame", mb64_podoboo_should_launch(50), 0);
    expect_int("podoboo launch after frame", mb64_podoboo_should_launch(51), 1);
    expect_float("podoboo launch velocity", mb64_podoboo_launch_velocity(0.0f, 150.0f), 21.0f);
}

static void verify_pokey_helpers(void) {
    const mb64_pokey_config_t *config = mb64_pokey_config();
    const mb64_object_hitbox_t *hitbox = mb64_pokey_body_part_hitbox();

    expect_int("pokey segment count", config->segment_count, 5);
    expect_int("pokey body hitbox radius", hitbox->radius, 40);
    expect_int("pokey body hurtbox radius", hitbox->hurtbox_radius, 42);
    expect_int("pokey body hitbox damage", hitbox->damage_or_coin_value, 2);
    expect_int("pokey head index", config->head_part_index, 0);
    expect_float("pokey scale", config->scale, 3.0f);
    expect_float("pokey body step", config->body_step, 120.0f);
    expect_float("pokey head spawn y", mb64_pokey_part_spawn_y(0), 480.0f);
    expect_float("pokey bottom spawn y", mb64_pokey_part_spawn_y(4), 0.0f);
    expect_int("pokey alive flags", (int)mb64_pokey_alive_flags(5), 31);
    expect_int("pokey offset angle", mb64_pokey_part_offset_angle(2, 3), 0x8000 + 0x1800);
    expect_float("pokey base height subtracts quicksand", mb64_pokey_part_base_height(1000.0f, 5, 2, 1.0f, 30.0f), 1210.0f);
    expect_float("pokey graph y offset", mb64_pokey_part_graph_y_offset(3.0f), 66.0f);
    expect_int("pokey death delay", mb64_pokey_part_death_delay(3), 32);
    expect_int("pokey shift part", mb64_pokey_should_shift_part(3, 0x13), 1);
    expect_int("pokey no shift head", mb64_pokey_should_shift_part(0, 0), 0);
    expect_int("pokey expand bottom", mb64_pokey_should_expand_bottom(0.5f, 2, 3), 1);
    expect_int("pokey no expand middle", mb64_pokey_should_expand_bottom(0.5f, 1, 3), 0);
    expect_int("pokey spawn near", mb64_pokey_should_spawn_parts(999.0f, 1000.0f), 1);
    expect_int("pokey no spawn at draw distance", mb64_pokey_should_spawn_parts(1000.0f, 1000.0f), 0);
    expect_int("pokey unload beyond margin", mb64_pokey_should_unload(1500.1f, 1000.0f), 1);
    expect_int("pokey no unload at margin", mb64_pokey_should_unload(1500.0f, 1000.0f), 0);
    expect_int("pokey regrow after timer", mb64_pokey_should_regrow(4, 101, 0), 1);
    expect_int("pokey no regrow on quicksand", mb64_pokey_should_regrow(4, 101, 1), 0);
    expect_int("pokey target angle left", mb64_pokey_target_angle_offset(200.0f, 0x100, 0), -0x4000);
    expect_int("pokey target angle far", mb64_pokey_target_angle_offset(3000.0f, 0, 0), 0);
    expect_int("pokey quicksand top part dies", mb64_pokey_should_die_in_quicksand(4, 5, 120.1f), 1);
    expect_int("pokey quicksand middle part lives", mb64_pokey_should_die_in_quicksand(3, 5, 200.0f), 0);
}

static void verify_showrunner_helpers(void) {
    const mb64_showrunner_config_t *config = mb64_showrunner_config();

    expect_int("showrunner health", config->health, 3);
    expect_int("showrunner initial spike volleys", config->spike_attacks_initial, 2);
    expect_int("showrunner ballerina timer", config->ballerina_end_timer, 400);
    expect_int("showrunner ballerina spin max", config->ballerina_spin_max, 0x2000);
    expect_int("showrunner no trigger at boundary", mb64_showrunner_should_trigger(1500.0f), 0);
    expect_int("showrunner trigger below boundary", mb64_showrunner_should_trigger(1499.0f), 1);
    expect_int("showrunner back away boundary", mb64_showrunner_back_away_finished(-5.0f), 0);
    expect_int("showrunner back away done", mb64_showrunner_back_away_finished(-4.9f), 1);
    expect_int("showrunner no spike before window", mb64_showrunner_should_spawn_spike(15), 0);
    expect_int("showrunner spike cadence", mb64_showrunner_should_spawn_spike(20), 1);
    expect_int("showrunner no spike after window", mb64_showrunner_should_spawn_spike(90), 0);
    expect_int("showrunner spike attack done", mb64_showrunner_spike_attack_finished(111), 1);
    expect_int("showrunner tennis projectile", mb64_showrunner_should_start_tennis_projectile(60), 1);
    expect_int("showrunner tennis stun health 2", mb64_showrunner_stun_from_tennis(2, 5), 1);
    expect_int("showrunner tennis no stun invalid health", mb64_showrunner_stun_from_tennis(4, 1), 0);
    expect_int("showrunner stunned recover", mb64_showrunner_should_recover_from_stun(161), 1);
    expect_int("showrunner damaged recover", mb64_showrunner_should_leave_damaged(61), 1);
    expect_int("showrunner drop items", mb64_showrunner_should_drop_items(61), 1);
    expect_int("showrunner shrink", mb64_showrunner_should_shrink(21), 1);
    expect_int("showrunner no delete at boundary", mb64_showrunner_should_delete(0.1f), 0);
    expect_int("showrunner delete below boundary", mb64_showrunner_should_delete(0.09f), 1);
    expect_int("showrunner spike lock", mb64_showrunner_spike_should_lock_to_mario(0, 299.0f), 1);
    expect_int("showrunner spike no relock", mb64_showrunner_spike_should_lock_to_mario(1, 100.0f), 0);
    expect_int("showrunner spike rumble boundary", mb64_showrunner_spike_should_leave_rumble(10), 0);
    expect_int("showrunner spike rumble done", mb64_showrunner_spike_should_leave_rumble(11), 1);
    expect_int("showrunner spike rise done", mb64_showrunner_spike_should_finish_rising(401.0f, 0.0f), 1);
    expect_int("showrunner spike kept during ballerina", mb64_showrunner_spike_should_retract(MB64_SHOWRUNNER_ACT_BALLERINA), 0);
    expect_int("showrunner spike retract during tennis", mb64_showrunner_spike_should_retract(MB64_SHOWRUNNER_ACT_TENNIS), 1);
    expect_int("showrunner spike delete below home", mb64_showrunner_spike_should_delete(-1.0f, 0.0f), 1);
    expect_int("showrunner tennis outbound", mb64_showrunner_tennis_should_return_to_parent(0), 0);
    expect_int("showrunner tennis return", mb64_showrunner_tennis_should_return_to_parent(1), 1);
    expect_int("showrunner flame exist action", MB64_SHOWRUNNER_FLAME_ACT_EXIST, 0);
    expect_int("showrunner flame delete action", MB64_SHOWRUNNER_FLAME_ACT_DELETE, 1);
    expect_int("showrunner tennis parent hit", mb64_showrunner_tennis_should_reset_parent(399.0f), 1);
    expect_int("showrunner tennis parent no hit", mb64_showrunner_tennis_should_reset_parent(400.0f), 0);
    expect_int("showrunner tennis stun helper", mb64_showrunner_tennis_should_stun_parent(2, 5), 1);
    expect_int("showrunner tennis trail delete", mb64_showrunner_tennis_trail_should_delete(12), 1);
    expect_int("showrunner ballerina projectile gated", mb64_showrunner_should_spawn_ballerina_projectile(33, 0, 0x1001), 1);
    expect_int("showrunner ballerina projectile spin threshold", mb64_showrunner_should_spawn_ballerina_projectile(33, 0, 0x1000), 0);
    expect_int("showrunner ballerina projectile interval", mb64_showrunner_should_spawn_ballerina_projectile(34, 0, 0x1001), 0);
    expect_int("showrunner ballerina projectile subaction", mb64_showrunner_should_spawn_ballerina_projectile(33, 1, 0x1001), 0);
    expect_int("showrunner phantasm release flame timer", mb64_showrunner_should_spawn_phantasm_release_flames(45), 1);
    expect_int("showrunner no early phantasm release flames", mb64_showrunner_should_spawn_phantasm_release_flames(44), 0);
    expect_int("showrunner flame count", config->phantasm_release_flame_count, 32);
    expect_int("showrunner flame angle step", config->phantasm_release_flame_angle_step, 0x800);
    expect_int("showrunner slow flame lifetime", mb64_showrunner_thwomp_flame_should_leave_exist(200, 0.0f), 0);
    expect_int("showrunner slow flame expires", mb64_showrunner_thwomp_flame_should_leave_exist(201, 0.0f), 1);
    expect_int("showrunner fast flame expires", mb64_showrunner_thwomp_flame_should_leave_exist(36, 30.0f), 1);
    expect_int("showrunner flame delete", mb64_showrunner_thwomp_flame_should_delete(31), 1);
    expect_float("showrunner flame half scale", mb64_showrunner_thwomp_flame_scale(15), 3.5f);
    expect_int("showrunner cosmic projectile timer delete", mb64_showrunner_cosmic_projectile_should_delete(111, 0), 1);
    expect_int("showrunner cosmic projectile wall delete", mb64_showrunner_cosmic_projectile_should_delete(1, 1), 1);
    expect_float("showrunner cosmic projectile speed", config->cosmic_projectile_forward_vel, 35.0f);
}

static void verify_level_size_boundary_helpers(void) {
    mb64_level_t level;
    mb64_mesh_t mesh;
    mb64_tile_t tile;
    memset(&level, 0, sizeof(level));
    memset(&tile, 0, sizeof(tile));
    level.header.tile_count = 1;
    level.tiles = &tile;
    tile.x = 32;
    tile.y = 0;
    tile.z = 32;
    tile.type = TILE_TYPE_CULL;
    level.header.boundary = 1;

    level.header.level_size = 0;
    expect_int("small level grid size", mb64_level_grid_size(&level), 32);
    expect_int("small level grid min", mb64_level_grid_min(&level), 16);
    mb64_level_bounds_t bounds = mb64_level_playable_bounds(&level);
    expect_int("small playable min", bounds.min, -256);
    expect_int("small playable max", bounds.max, 256);
    level.header.level_size = 1;
    expect_int("medium level grid size", mb64_level_grid_size(&level), 48);
    expect_int("medium level grid min", mb64_level_grid_min(&level), 8);
    bounds = mb64_level_playable_bounds(&level);
    expect_int("medium playable min", bounds.min, -384);
    expect_int("medium playable max", bounds.max, 384);
    level.header.level_size = 2;
    expect_int("large level grid size", mb64_level_grid_size(&level), 64);
    expect_int("large level grid min", mb64_level_grid_min(&level), 0);
    bounds = mb64_level_playable_bounds(&level);
    expect_int("large playable min", bounds.min, -512);
    expect_int("large playable max", bounds.max, 512);

    level.header.boundary = 0;
    bounds = mb64_level_playable_bounds(&level);
    expect_int("large no-boundary playable min", bounds.min, -640);
    expect_int("large no-boundary playable max", bounds.max, 640);

    memset(&mesh, 0, sizeof(mesh));
    level.header.level_size = 1;
    level.header.boundary = 1;
    level.header.boundary_mat = 0;
    if (!mb64_build_render_mesh(&level, &mesh)) {
        fprintf(stderr, "medium boundary render mesh failed\n");
        g_failures++;
        return;
    }
    expect_int("medium boundary floor face count", (int)mesh.face_count, 16);
    expect_vertex("medium inner boundary extent", mesh.faces[0].v[0], 384, 0, 384);
    expect_int("medium inner boundary tc u", mesh.faces[0].tc[0][0], 24576);
    expect_int("medium inner boundary tc v", mesh.faces[0].tc[0][1], 24576);
    expect_vertex("medium outer boundary extent", mesh.faces[4].v[0], 576, 0, 384);
    mb64_free_render_mesh(&mesh);

    memset(&mesh, 0, sizeof(mesh));
    level.header.boundary = 3;
    level.header.boundary_height = 40;
    if (!mb64_build_collision_mesh(&level, &mesh)) {
        fprintf(stderr, "medium boundary wall collision mesh failed\n");
        g_failures++;
        return;
    }
    if (mesh.face_count < 13) {
        fprintf(stderr, "medium boundary wall face count too small: %u\n", mesh.face_count);
        g_failures++;
    } else {
        expect_vertex("medium wall boundary extent", mesh.faces[12].v[0], 384, 128, 0);
        expect_int("medium wall boundary direction", mesh.faces[12].direction, MB64_MESH_FACE_NEG_X);
        expect_int("medium wall boundary tc horizontal", mesh.faces[12].tc[1][0], -24576);
        expect_int("medium wall boundary tc vertical", mesh.faces[12].tc[0][1], 8192);
    }
    mb64_free_render_mesh(&mesh);
}

int main(void) {
    static const int16_t front0[4][3] = {
        { 0, -472, 0 }, { 0, -480, 0 }, { 16, -472, 0 }, { 16, -480, 0 },
    };
    static const int16_t back0[4][3] = {
        { 16, -472, 0 }, { 16, -480, 0 }, { 0, -472, 0 }, { 0, -480, 0 },
    };
    static const int16_t front1[4][3] = {
        { 0, -472, 16 }, { 0, -480, 16 }, { 0, -472, 0 }, { 0, -480, 0 },
    };
    static const int16_t back1[4][3] = {
        { 0, -472, 0 }, { 0, -480, 0 }, { 0, -472, 16 }, { 0, -480, 16 },
    };
    static const int16_t front2[4][3] = {
        { 16, -472, 16 }, { 16, -480, 16 }, { 0, -472, 16 }, { 0, -480, 16 },
    };
    static const int16_t back2[4][3] = {
        { 0, -472, 16 }, { 0, -480, 16 }, { 16, -472, 16 }, { 16, -480, 16 },
    };
    static const int16_t front3[4][3] = {
        { 16, -472, 0 }, { 16, -480, 0 }, { 16, -472, 16 }, { 16, -480, 16 },
    };
    static const int16_t back3[4][3] = {
        { 16, -472, 16 }, { 16, -480, 16 }, { 16, -472, 0 }, { 16, -480, 0 },
    };

    verify_fence_rotation(0, MB64_MESH_FACE_POS_Z, MB64_MESH_FACE_NEG_Z, front0, back0);
    verify_fence_rotation(1, MB64_MESH_FACE_POS_X, MB64_MESH_FACE_NEG_X, front1, back1);
    verify_fence_rotation(2, MB64_MESH_FACE_NEG_Z, MB64_MESH_FACE_POS_Z, front2, back2);
    verify_fence_rotation(3, MB64_MESH_FACE_NEG_X, MB64_MESH_FACE_POS_X, front3, back3);
    verify_adjacent_fence_uv_phase();
    verify_bars_match_mb64_connection_rendering();
    verify_shared_surface_semantics();
    verify_star_count_helpers();
    verify_shaped_tile_rotations();
    verify_render_mesh_visitor_matches_snapshot();
    verify_water_render_predicates();
    verify_stacked_water_query_uses_top_surface();
    verify_render_binding_descriptors();
    verify_face_surface_descriptors();
    verify_shaped_corner_under_block_keeps_shell_faces();
    verify_woodplat_helpers();
    verify_looping_platform_helpers();
    verify_breakable_box_helpers();
    verify_reinforced_box_helpers();
    verify_fire_spinner_helpers();
    verify_goomba_helpers();
    verify_koopa_helpers();
    verify_bully_helpers();
    verify_bullet_bill_helpers();
    verify_exclamation_box_helpers();
    verify_floor_switch_helpers();
    verify_conveyor_helpers();
    verify_noteblock_helpers();
    verify_onoff_helpers();
    verify_badge_helpers();
    verify_green_coin_helpers();
    verify_powerup_helpers();
    verify_phantasm_helpers();
    verify_showrunner_helpers();
    verify_motos_helpers();
    verify_chicken_helpers();
    verify_crablet_helpers();
    verify_fire_bro_helpers();
    verify_hammer_bro_helpers();
    verify_rex_helpers();
    verify_npc_helpers();
    verify_podoboo_helpers();
    verify_pokey_helpers();
    verify_level_size_boundary_helpers();

    if (g_failures != 0) {
        fprintf(stderr, "mesh parity tests failed: %d\n", g_failures);
        return 1;
    }
    puts("[LIBMB64_TEST] mesh parity tests passed");
    return 0;
}
