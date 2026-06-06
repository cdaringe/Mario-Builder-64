#include "mb64.h"
#include "mb64_faceshapes.h"
#include "mb64_tile_types.h"

#include <stdlib.h>
#include <string.h>

#define MB64_GRID_SIZE 64
#define MB64_GRID_CENTRE 32
#define MB64_TILE_SUBUNITS 16
#define MB64_THEME_CUSTOM 10
#define MB64_MATERIAL_SLOT_COUNT 10
#define MB64_DEATH_PLANE_GRID_Y (-40)
#define PACK_TILESIZE(w, d) (((w) << 2) + (d))

#define MB64_BOUNDARY_INNER_FLOOR (1 << 0)
#define MB64_BOUNDARY_OUTER_FLOOR (1 << 1)
#define MB64_BOUNDARY_INNER_WALLS (1 << 2)
#define MB64_BOUNDARY_OUTER_WALLS (1 << 3)
#define MB64_BOUNDARY_CEILING     (1 << 4)

enum {
    MAT_OPAQUE = 0,
    MAT_DECAL,
    MAT_CUTOUT,
    MAT_CUTOUT_NOCULL,
    MAT_TRANSPARENT,
    MAT_SCREEN,
};

enum {
    CLASS_OPAQUE = 0,
    CLASS_HOLLOW_TRANSPARENT,
    CLASS_HOLLOW_CUTOUT,
    CLASS_TRANSPARENT,
    CLASS_CUTOUT,
};

typedef struct {
    uint8_t side;
    uint8_t top;
} mb64_material_def_t;

#include "mb64_theme_data.generated.inc.c"


static uint8_t s_solid_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];
static uint8_t s_water_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];
static const mb64_tile_t *s_tile_grid[MB64_GRID_SIZE][MB64_GRID_SIZE][MB64_GRID_SIZE];

static const int8_t s_fence_alt_uvs[4][2] = {
    {32, 16},
    {32,  0},
    { 0, 16},
    { 0,  0},
};

typedef struct {
    int8_t v[4][2];
} mb64_boundary_floor_quad_t;



static int solid_at(int x, int y, int z);

typedef struct {
    int8_t v[4][3];
    uint8_t direction;
    uint8_t faceshape;
    uint8_t vertex_count;
} mb64_shape_face_t;

typedef struct {
    const mb64_shape_face_t *faces;
    uint8_t face_count;
} mb64_shape_t;

static const mb64_shape_t *shape_for_tile(const mb64_tile_t *t);

#define Q(dir, faceshape, ...) { { __VA_ARGS__ }, dir, faceshape, 4 }
#define T(dir, faceshape, ...) { { __VA_ARGS__, {0,0,0} }, dir, faceshape, 3 }

#include "mb64_mesh_data.generated.inc.c"

static const uint8_t s_rotated_dirs[4][6] = {
    {0, 1, 2, 3, 4, 5},
    {0, 1, 5, 4, 2, 3},
    {0, 1, 3, 2, 5, 4},
    {0, 1, 4, 5, 3, 2},
};

static int tile_in_range(const mb64_tile_t *t) {
    return t != NULL &&
           t->x < MB64_GRID_SIZE &&
           t->y < MB64_GRID_SIZE &&
           t->z < MB64_GRID_SIZE;
}

static uint8_t mb64_material_type(uint8_t material) {
    if (material < (uint8_t)(sizeof(s_material_types) / sizeof(s_material_types[0]))) {
        return s_material_types[material];
    }
    return MAT_OPAQUE;
}

static int tile_is_solid(const mb64_tile_t *t) {
    return tile_in_range(t) &&
           t->type != 0 &&
           t->type != TILE_TYPE_CULL &&
           t->type != TILE_TYPE_WATER;
}

uint8_t mb64_tile_has_collision(const mb64_tile_t *tile) {
    return tile_is_solid(tile) && tile->type != TILE_TYPE_TROLL;
}

static int tile_is_water(const mb64_tile_t *t) {
    return tile_in_range(t) &&
           (t->type == TILE_TYPE_WATER || t->waterlogged);
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

const mb64_theme_special_t *mb64_theme_specials_for_level(const mb64_level_t *level) {
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
            return mb64_theme_specials_for_level(level)->pole;
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

static uint8_t mb64_resolve_tile_side_class(const mb64_level_t *level,
                                            const mb64_tile_t *tile,
                                            uint8_t direction) {
    if (tile == NULL) {
        return CLASS_CUTOUT;
    }
    if (tile->type == TILE_TYPE_FENCE || tile->type == TILE_TYPE_BARS) {
        return CLASS_CUTOUT;
    }
    if (tile->type == TILE_TYPE_WATER) {
        return CLASS_TRANSPARENT;
    }

    uint8_t material_type = mb64_material_type(mb64_resolve_tile_material(
        level,
        tile,
        direction == MB64_MESH_FACE_TOP
    ));
    if (direction != MB64_MESH_FACE_TOP) {
        material_type = mb64_material_type(mb64_resolve_tile_material(level, tile, 0));
        if (material_type < MAT_CUTOUT) {
            const uint8_t top_type = mb64_material_type(mb64_resolve_tile_material(level, tile, 1));
            if (top_type == MAT_CUTOUT) {
                return CLASS_HOLLOW_CUTOUT;
            }
            if (top_type > MAT_CUTOUT) {
                return CLASS_HOLLOW_TRANSPARENT;
            }
        }
    }
    if (material_type < MAT_CUTOUT) {
        return CLASS_OPAQUE;
    }
    if (material_type == MAT_CUTOUT) {
        return CLASS_CUTOUT;
    }
    return CLASS_TRANSPARENT;
}

static int mb64_cutout_skip_culling_check(const mb64_level_t *level,
                                          const mb64_tile_t *cur,
                                          const mb64_tile_t *other,
                                          uint8_t direction) {
    if (cur == NULL || other == NULL) {
        return 0;
    }
    const uint8_t cur_material = mb64_resolve_face_material(level, cur, direction);
    const uint8_t other_material = mb64_resolve_face_material(level, other, direction ^ 1);
    if (cur_material == other_material && cur->type == other->type) {
        return 0;
    }

    const uint8_t cur_class = mb64_resolve_tile_side_class(level, cur, direction);
    const uint8_t other_class = mb64_resolve_tile_side_class(level, other, direction ^ 1);
    if (cur_class == other_class) {
        return cur_class == CLASS_TRANSPARENT;
    }
    if (cur_class == CLASS_HOLLOW_CUTOUT || cur_class == CLASS_HOLLOW_TRANSPARENT) {
        if (direction == MB64_MESH_FACE_BOTTOM &&
            mb64_resolve_tile_side_class(level, other, MB64_MESH_FACE_BOTTOM) == cur_class) {
            return 0;
        }
    }
    return cur_class < other_class;
}

static uint8_t boundary_flags(const mb64_level_t *level) {
    if (level == NULL) {
        return 0;
    }
    uint8_t boundary = level->header.boundary;
    if (boundary >= (uint8_t)(sizeof(s_boundary_table) / sizeof(s_boundary_table[0]))) {
        return 0;
    }
    uint8_t flags = s_boundary_table[boundary];
    if ((flags & MB64_BOUNDARY_INNER_FLOOR) &&
        level->header.boundary_height == 0) {
        flags &= ~(MB64_BOUNDARY_CEILING | MB64_BOUNDARY_INNER_WALLS);
    }
    if ((flags & MB64_BOUNDARY_INNER_FLOOR) &&
        !(flags & (MB64_BOUNDARY_INNER_WALLS | MB64_BOUNDARY_OUTER_WALLS))) {
        flags |= MB64_BOUNDARY_OUTER_FLOOR;
    }
    return flags;
}

uint8_t mb64_boundary_flags_for_level(const mb64_level_t *level) {
    return boundary_flags(level);
}

uint32_t mb64_build_death_plane_faces(const mb64_level_t *level,
                                      mb64_boundary_face_t out[MB64_DEATH_PLANE_FACE_COUNT]) {
    if (out == NULL) {
        return 0;
    }
    const uint8_t flags = boundary_flags(level);
    const int16_t gridSize =
        (flags & (MB64_BOUNDARY_OUTER_FLOOR | MB64_BOUNDARY_CEILING))
            ? MB64_GRID_SIZE
            : MB64_GRID_SIZE + 16;
    const int16_t extent = (int16_t)(gridSize * MB64_TILE_SUBUNITS / 2);
    const int16_t y = (int16_t)(MB64_DEATH_PLANE_GRID_Y * MB64_TILE_SUBUNITS);
    const mb64_boundary_face_t faces[MB64_DEATH_PLANE_FACE_COUNT] = {
        {{{ extent, y, extent }, { extent, y, 0 }, { 0, y, extent }, { 0, y, 0 }}},
        {{{ 0, y, extent }, { 0, y, 0 }, { (int16_t)-extent, y, extent }, { (int16_t)-extent, y, 0 }}},
        {{{ extent, y, 0 }, { extent, y, (int16_t)-extent }, { 0, y, 0 }, { 0, y, (int16_t)-extent }}},
        {{{ 0, y, 0 }, { 0, y, (int16_t)-extent }, { (int16_t)-extent, y, 0 }, { (int16_t)-extent, y, (int16_t)-extent }}},
    };
    memcpy(out, faces, sizeof(faces));
    return MB64_DEATH_PLANE_FACE_COUNT;
}

static uint32_t boundary_floor_face_count(const mb64_level_t *level) {
    uint8_t flags = boundary_flags(level);
    uint32_t count = 0;
    if (flags & MB64_BOUNDARY_INNER_FLOOR) {
        count += (uint32_t)(sizeof(s_boundary_inner_floor) / sizeof(s_boundary_inner_floor[0]));
    }
    if (flags & MB64_BOUNDARY_OUTER_FLOOR) {
        count += (uint32_t)(sizeof(s_boundary_outer_floor) / sizeof(s_boundary_outer_floor[0]));
    }
    if (flags & MB64_BOUNDARY_CEILING) {
        count += (uint32_t)(sizeof(s_boundary_inner_floor) / sizeof(s_boundary_inner_floor[0]));
    }
    return count;
}

static void emit_boundary_floor_face(mb64_mesh_t *mesh, uint32_t *idx,
                                     const mb64_level_t *level,
                                     const mb64_boundary_floor_quad_t *quad,
                                     int16_t y, uint8_t direction) {
    mb64_mesh_face_t *face = &mesh->faces[(*idx)++];
    mb64_tile_t material_tile;
    memset(&material_tile, 0, sizeof(material_tile));
    material_tile.mat = level->header.boundary_mat;

    face->material = material_tile.mat;
    face->resolved_material = mb64_resolve_tile_material(
        level,
        &material_tile,
        direction == MB64_MESH_FACE_TOP
    );
    face->tile_type = 0;
    face->direction = direction;
    face->is_water = 0;
    face->vertex_count = 4;
    face->use_tc = 1;
    face->tile_x = UINT8_MAX;
    face->tile_y = UINT8_MAX;
    face->tile_z = UINT8_MAX;

    for (uint8_t i = 0; i < 4; i++) {
        face->v[i][0] = (int16_t)(quad->v[i][0] * MB64_TILE_SUBUNITS);
        face->v[i][1] = y;
        face->v[i][2] = (int16_t)(quad->v[i][1] * MB64_TILE_SUBUNITS);
        face->tc[i][0] = (int16_t)(quad->v[i][0] * 32);
        face->tc[i][1] = (int16_t)(quad->v[i][1] * 32);
    }
}

static void emit_boundary_floor_faces(mb64_mesh_t *mesh, uint32_t *idx,
                                      const mb64_level_t *level) {
    const uint8_t flags = boundary_flags(level);
    if (flags & MB64_BOUNDARY_INNER_FLOOR) {
        for (uint32_t i = 0; i < (uint32_t)(sizeof(s_boundary_inner_floor) / sizeof(s_boundary_inner_floor[0])); i++) {
            emit_boundary_floor_face(mesh, idx, level, &s_boundary_inner_floor[i], 0, MB64_MESH_FACE_TOP);
        }
    }
    if (flags & MB64_BOUNDARY_OUTER_FLOOR) {
        for (uint32_t i = 0; i < (uint32_t)(sizeof(s_boundary_outer_floor) / sizeof(s_boundary_outer_floor[0])); i++) {
            emit_boundary_floor_face(mesh, idx, level, &s_boundary_outer_floor[i], 0, MB64_MESH_FACE_TOP);
        }
    }
    if (flags & MB64_BOUNDARY_CEILING) {
        int16_t y = (int16_t)(level->header.boundary_height * MB64_TILE_SUBUNITS);
        for (uint32_t i = 0; i < (uint32_t)(sizeof(s_boundary_inner_floor) / sizeof(s_boundary_inner_floor[0])); i++) {
            emit_boundary_floor_face(mesh, idx, level, &s_boundary_inner_floor[i], y, MB64_MESH_FACE_BOTTOM);
        }
    }
}

int16_t mb64_surface_for_material(uint8_t material) {
    if (material < (sizeof(s_material_surfaces) / sizeof(s_material_surfaces[0]))) {
        return s_material_surfaces[material];
    }
    return 0;
}

int16_t mb64_surface_for_tile(const mb64_level_t *level, const mb64_tile_t *tile) {
    if (level == NULL || tile == NULL) {
        return 0;
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
    const uint8_t water = mb64_theme_specials_for_level(level)->water;
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

void mb64_water_vertex_color(const mb64_level_t *level, uint8_t wave, uint8_t rgba[4]) {
    (void)level;
    if (rgba == NULL) {
        return;
    }
    rgba[0] = (uint8_t)(54 + wave);
    rgba[1] = (uint8_t)(132 + wave * 2);
    rgba[2] = (uint8_t)(128 + wave * 2);
    rgba[3] = (uint8_t)(136 + wave);
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

static void assign_tile_texture_coordinates(mb64_mesh_face_t *face,
                                            const mb64_tile_t *tile,
                                            const int16_t local[4][3],
                                            uint8_t direction);

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
    face->tile_x = t->x;
    face->tile_y = t->y;
    face->tile_z = t->z;
    if (!is_water) {
        int16_t local[4][3];
        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        (void)x1; (void)y1; (void)z1;
        for (uint8_t i = 0; i < 4; i++) {
            local[i][0] = (int16_t)(p[i][0] - x0);
            local[i][1] = (int16_t)(p[i][1] - y0);
            local[i][2] = (int16_t)(p[i][2] - z0);
        }
        assign_tile_texture_coordinates(face, t, local, direction);
    }
}

static uint8_t rotate_direction(uint8_t direction, uint8_t rot) {
    if (direction >= 6) {
        return direction;
    }
    return s_rotated_dirs[rot & 3][direction];
}

static uint8_t face_uv_axes(uint8_t direction, uint8_t *u_axis, uint8_t *v_axis) {
    switch (direction) {
        case MB64_MESH_FACE_NEG_X:
            *u_axis = 2;
            *v_axis = 1;
            return 1;
        case MB64_MESH_FACE_POS_X:
            *u_axis = 2;
            *v_axis = 1;
            return 0;
        case MB64_MESH_FACE_BOTTOM:
        case MB64_MESH_FACE_TOP:
            *u_axis = 2;
            *v_axis = 0;
            return 0;
        case MB64_MESH_FACE_NEG_Z:
            *u_axis = 0;
            *v_axis = 1;
            return 0;
        case MB64_MESH_FACE_POS_Z:
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

static void assign_tile_texture_coordinates(mb64_mesh_face_t *face,
                                            const mb64_tile_t *tile,
                                            const int16_t local[4][3],
                                            uint8_t direction) {
    uint8_t u_axis;
    uint8_t v_axis;
    uint8_t flip_u = face_uv_axes(direction, &u_axis, &v_axis);
    int32_t u_pos = flip_u ? 64 - tile_axis_value(tile, u_axis)
                           : tile_axis_value(tile, u_axis);
    int32_t v_pos = tile_axis_value(tile, v_axis);
    u_pos = mb64_uv_wrap_offset(u_pos);
    v_pos = mb64_uv_wrap_offset(v_pos);

    for (uint8_t i = 0; i < face->vertex_count; i++) {
        int16_t u = local[i][u_axis];
        int16_t v = (int16_t)(16 - local[i][v_axis]);
        if (!flip_u) {
            u = (int16_t)(16 - u);
        }
        u = (int16_t)(u - u_pos * 16);
        v = (int16_t)(v - v_pos * 16);
        face->tc[i][0] = (int16_t)(u * 64 - 16);
        face->tc[i][1] = (int16_t)(v * 64 - 16);
    }
    face->use_tc = 1;
}

static const mb64_tile_t *tile_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return NULL;
    }
    return s_tile_grid[z][y][x];
}

static uint8_t faceshape_at(int x, int y, int z, uint8_t direction) {
    const mb64_tile_t *tile = tile_at(x, y, z);
    const mb64_shape_t *shape = shape_for_tile(tile);
    if (shape == NULL) {
        return MB64_FACESHAPE_EMPTY;
    }
    uint8_t local_dir = (uint8_t)(rotate_direction(direction, (uint8_t)((4 - (tile->rot & 3)) & 3)) ^ 1);
    for (uint8_t i = 0; i < shape->face_count; i++) {
        if (shape->faces[i].direction == local_dir) {
            return shape->faces[i].faceshape;
        }
    }
    return MB64_FACESHAPE_EMPTY;
}

static int shape_face_is_occluded(const mb64_level_t *level,
                                  const mb64_tile_t *t,
                                  uint8_t direction,
                                  uint8_t faceshape) {
    if ((faceshape & MB64_FACESHAPE_EMPTY) != 0) {
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
        case MB64_MESH_FACE_POS_X:
            dx = 1;
            break;
        case MB64_MESH_FACE_NEG_X:
            dx = -1;
            break;
        case MB64_MESH_FACE_POS_Z:
            dz = 1;
            break;
        case MB64_MESH_FACE_NEG_Z:
            dz = -1;
            break;
        default:
            return 0;
    }
    int ax = (int)t->x + dx;
    int ay = (int)t->y + dy;
    int az = (int)t->z + dz;
    if (!solid_at(ax, ay, az)) {
        return 0;
    }
    const mb64_tile_t *adj = tile_at(ax, ay, az);
    if (mb64_cutout_skip_culling_check(level, t, adj, direction)) {
        return 0;
    }
    uint8_t other = faceshape_at(ax, ay, az, direction);
    if ((other & MB64_FACESHAPE_EMPTY) != 0) {
        return 0;
    }
    if (other == MB64_FACESHAPE_FULL) {
        return 1;
    }
    if (faceshape == MB64_FACESHAPE_FULL) {
        return 0;
    }
    if (faceshape == MB64_FACESHAPE_TOPTRI || faceshape == MB64_FACESHAPE_TOPHALF) {
        return other == faceshape && adj != NULL && ((adj->rot & 3) == (t->rot & 3));
    }
    if (faceshape == MB64_FACESHAPE_BOTTOMSLAB ||
        faceshape == MB64_FACESHAPE_TOPSLAB ||
        faceshape == MB64_FACESHAPE_POLETOP) {
        if (other == faceshape) {
            return 1;
        }
    }
    if (faceshape == (uint8_t)(other ^ 1)) {
        return 1;
    }
    if (((faceshape & 0x10) && (other & 0x10)) ||
        ((faceshape & 0x20) && (other & 0x20))) {
        return faceshape > other;
    }
    return 0;
}

static int full_face_is_occluded(const mb64_level_t *level,
                                 const mb64_tile_t *t,
                                 uint8_t direction,
                                 int nx,
                                 int ny,
                                 int nz) {
    if (!solid_at(nx, ny, nz)) {
        return 0;
    }
    return !mb64_cutout_skip_culling_check(level, t, tile_at(nx, ny, nz), direction);
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
    int16_t local[4][3];
    int16_t x0, x1, y0, y1, z0, z1;
    (void)x1; (void)y1; (void)z1;
    tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
    for (uint8_t i = 0; i < 4; i++) {
        rotate_vertex(t->rot, src->v[i], local[i]);
        p[i][0] = (int16_t)(x0 + local[i][0]);
        p[i][1] = (int16_t)(y0 + local[i][1]);
        p[i][2] = (int16_t)(z0 + local[i][2]);
    }

    uint8_t direction = rotate_direction(src->direction, t->rot);
    if (shape_face_is_occluded(level, t, direction, src->faceshape)) {
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
    face->use_tc = 0;
    face->tile_x = t->x;
    face->tile_y = t->y;
    face->tile_z = t->z;
    if (t->type == TILE_TYPE_FENCE) {
        assign_fence_texture_coordinates(face, t, direction);
    } else {
        assign_tile_texture_coordinates(face, t, local, direction);
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
    memset(s_tile_grid, 0, sizeof(s_tile_grid));

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (tile_is_solid(t)) {
            s_solid_grid[t->z][t->y][t->x] = 1;
            s_tile_grid[t->z][t->y][t->x] = t;
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
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_TOP, tx, ty + 1, tz)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_BOTTOM, tx, ty - 1, tz)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_X, tx - 1, ty, tz)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_X, tx + 1, ty, tz)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_Z, tx, ty, tz - 1)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_Z, tx, ty, tz + 1)) { face_count++; }
    }
    face_count += boundary_floor_face_count(level);

    if (face_count == 0) { return 0; }
    mesh->faces = calloc(face_count, sizeof(*mesh->faces));
    if (mesh->faces == NULL) {
        memset(mesh, 0, sizeof(*mesh));
        return 0;
    }
    mesh->face_count = face_count;

    uint32_t out = 0;
    emit_boundary_floor_faces(mesh, &out, level);
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

        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_TOP, tx, ty + 1, tz)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y1,z0},{x1,y1,z1},{x1,y1,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_TOP, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_BOTTOM, tx, ty - 1, tz)) {
            const int16_t p[4][3] = {{x0,y0,z0},{x0,y0,z1},{x1,y0,z0},{x1,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_BOTTOM, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_X, tx - 1, ty, tz)) {
            const int16_t p[4][3] = {{x0,y1,z0},{x0,y0,z0},{x0,y1,z1},{x0,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_X, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_X, tx + 1, ty, tz)) {
            const int16_t p[4][3] = {{x1,y1,z1},{x1,y0,z1},{x1,y1,z0},{x1,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_X, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_Z, tx, ty, tz - 1)) {
            const int16_t p[4][3] = {{x1,y1,z0},{x1,y0,z0},{x0,y1,z0},{x0,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_Z, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_Z, tx, ty, tz + 1)) {
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
