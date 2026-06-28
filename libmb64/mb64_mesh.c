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
#define MB64_SURFACE_DEFAULT 0
#define MB64_SURFACE_NO_CAM_COLLISION 0x0076
#define MB64_SURFACE_VANISH_CAP_WALLS 0x007B

#define MB64_BOUNDARY_INNER_FLOOR (1 << 0)
#define MB64_BOUNDARY_OUTER_FLOOR (1 << 1)
#define MB64_BOUNDARY_INNER_WALLS (1 << 2)
#define MB64_BOUNDARY_OUTER_WALLS (1 << 3)
#define MB64_BOUNDARY_CEILING     (1 << 4)

#define BAR_CONNECTED_SIDE(flags)   ((flags) & 1)
#define BAR_CONNECTED_TOP(flags)    ((flags) & 2)
#define BAR_CONNECTED_BOTTOM(flags) ((flags) & 4)

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

enum {
    MB64_GROWTH_NONE,
    MB64_GROWTH_FULL,
    MB64_GROWTH_NORMAL_SIDE,
    MB64_GROWTH_HALF_SIDE,
    MB64_GROWTH_UNDERSLOPE_CORNER,
    MB64_GROWTH_DIAGONAL_SIDE,
    MB64_GROWTH_VSLAB_SIDE,
    MB64_GROWTH_UNCONDITIONAL,

    MB64_GROWTH_EXTRADECAL_START = 0x10,
    MB64_GROWTH_SLOPE_SIDE_L = MB64_GROWTH_EXTRADECAL_START,
    MB64_GROWTH_SLOPE_SIDE_R,
    MB64_GROWTH_GENTLE_SIDE_L,
    MB64_GROWTH_GENTLE_SIDE_R,
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

typedef struct {
    int8_t v[4][3];
} mb64_boundary_wall_quad_t;



static int solid_at(int x, int y, int z);
static uint8_t rotate_direction(uint8_t direction, uint8_t rot);
static uint8_t boundary_flags(const mb64_level_t *level);

typedef struct {
    int8_t v[4][3];
    uint8_t direction;
    uint8_t faceshape;
    uint8_t vertex_count;
} mb64_shape_face_geometry_t;

typedef struct {
    uint8_t has_alt_uvs;
    int8_t alt_uvs[4][2];
} mb64_shape_face_uv_t;

typedef struct {
    mb64_shape_face_geometry_t geometry;
    uint8_t growth_type;
    mb64_shape_face_uv_t uv;
} mb64_shape_face_t;

typedef struct {
    const mb64_shape_face_t *faces;
    uint8_t face_count;
} mb64_shape_t;

static const mb64_shape_t *shape_for_tile(const mb64_tile_t *t, int collision_mesh);
static void emit_base_shape_face(mb64_mesh_t *mesh, uint32_t *idx,
                                 const mb64_level_t *level,
                                 const mb64_tile_t *t,
                                 const mb64_shape_face_geometry_t *src,
                                 const mb64_shape_face_uv_t *uv,
                                 int collision_mesh);

#define Q(dir, faceshape, growth, has_alt, alt, ...) { { { __VA_ARGS__ }, dir, faceshape, 4 }, growth, { has_alt, alt } }
#define T(dir, faceshape, growth, has_alt, alt, ...) { { { __VA_ARGS__, {0,0,0} }, dir, faceshape, 3 }, growth, { has_alt, alt } }

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

static uint8_t mb64_material_vertical(uint8_t material) {
    if (material < (sizeof(s_material_verticals) / sizeof(s_material_verticals[0]))) {
        return s_material_verticals[material];
    }
    return 0;
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

uint8_t mb64_tile_has_terrain_collision(const mb64_tile_t *tile) {
    return mb64_tile_has_collision(tile) && tile->type != TILE_TYPE_POLE;
}

uint8_t mb64_mesh_face_has_terrain_collision(const mb64_mesh_face_t *face) {
    if (face == NULL || face->is_water) {
        return 0;
    }
    return face->tile_type != TILE_TYPE_TROLL && face->tile_type != TILE_TYPE_POLE;
}

static int fullblock_can_be_waterlogged(const mb64_level_t *level,
                                        const mb64_tile_t *t) {
    if (level == NULL || t == NULL) {
        return 0;
    }
    return mb64_material_type(mb64_resolve_tile_material(level, t, 0)) == MAT_CUTOUT ||
           mb64_material_type(mb64_resolve_tile_material(level, t, 1)) == MAT_CUTOUT;
}

uint8_t mb64_tile_renders_water(const mb64_level_t *level, const mb64_tile_t *t) {
    if (!tile_in_range(t)) {
        return 0;
    }
    if (t->type == TILE_TYPE_WATER) {
        return 1;
    }
    if (!t->waterlogged) {
        return 0;
    }
    if ((t->type == TILE_TYPE_BLOCK || t->type == TILE_TYPE_TROLL) &&
        !fullblock_can_be_waterlogged(level, t)) {
        return 0;
    }
    return 1;
}

uint8_t mb64_level_grid_size(const mb64_level_t *level) {
    if (level == NULL) {
        return MB64_GRID_SIZE;
    }
    switch (level->header.level_size) {
        case 0: return 32;
        case 1: return 48;
        case 2: return 64;
        default: return MB64_GRID_SIZE;
    }
}

uint8_t mb64_level_grid_min(const mb64_level_t *level) {
    return (uint8_t)((MB64_GRID_SIZE - mb64_level_grid_size(level)) / 2);
}

mb64_level_bounds_t mb64_level_playable_bounds(const mb64_level_t *level) {
    const uint8_t gridMin = mb64_level_grid_min(level);
    const uint8_t gridSize = mb64_level_grid_size(level);
    mb64_level_bounds_t bounds = {
        (int16_t)(((int16_t)gridMin - MB64_GRID_CENTRE) * MB64_TILE_SUBUNITS),
        (int16_t)(((int16_t)gridMin + (int16_t)gridSize - MB64_GRID_CENTRE) * MB64_TILE_SUBUNITS),
    };
    if ((boundary_flags(level) & MB64_BOUNDARY_OUTER_FLOOR) == 0) {
        bounds.min = (int16_t)(bounds.min - 8 * MB64_TILE_SUBUNITS);
        bounds.max = (int16_t)(bounds.max + 8 * MB64_TILE_SUBUNITS);
    }
    return bounds;
}

static int solid_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return 0;
    }
    return s_solid_grid[z][y][x] != 0;
}

static int coords_in_level_range(const mb64_level_t *level, int x, int y, int z) {
    const int grid_min = mb64_level_grid_min(level);
    const int grid_max = grid_min + mb64_level_grid_size(level) - 1;
    return x >= grid_min && x <= grid_max &&
           y >= 0 && y < MB64_GRID_SIZE &&
           z >= grid_min && z <= grid_max;
}

static int water_at(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return 0;
    }
    return s_water_grid[z][y][x] != 0;
}

static const mb64_tile_t *find_level_tile(const mb64_level_t *level, int x, int y, int z) {
    if (level == NULL || level->tiles == NULL ||
        x < 0 || y < 0 || z < 0 ||
        x >= MB64_GRID_SIZE || y >= MB64_GRID_SIZE || z >= MB64_GRID_SIZE) {
        return NULL;
    }

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *tile = &level->tiles[i];
        if (tile->x == x && tile->y == y && tile->z == z) {
            return tile;
        }
    }
    return NULL;
}

static int level_water_at(const mb64_level_t *level, int x, int y, int z) {
    return mb64_tile_renders_water(level, find_level_tile(level, x, y, z));
}

static int mb64_level_faceshape_at(const mb64_level_t *level,
                                   int x,
                                   int y,
                                   int z,
                                   uint8_t direction) {
    const mb64_tile_t *tile = find_level_tile(level, x, y, z);
    const mb64_shape_t *shape = shape_for_tile(tile, 0);
    if (shape == NULL) {
        return tile_is_solid(tile) ? MB64_FACESHAPE_FULL : MB64_FACESHAPE_EMPTY;
    }

    uint8_t local_dir = (uint8_t)(rotate_direction(direction, (uint8_t)((4 - (tile->rot & 3)) & 3)) ^ 1);
    for (uint8_t i = 0; i < shape->face_count; i++) {
        if (shape->faces[i].geometry.direction == local_dir) {
            return shape->faces[i].geometry.faceshape;
        }
    }
    return MB64_FACESHAPE_EMPTY;
}

static int mb64_water_surface_is_fullblock(const mb64_level_t *level,
                                           int grid_x,
                                           int grid_y,
                                           int grid_z) {
    const mb64_tile_t *tile = find_level_tile(level, grid_x, grid_y, grid_z);
    const mb64_tile_t *above = find_level_tile(level, grid_x, grid_y + 1, grid_z);

    if (tile == NULL) {
        return 0;
    }
    if (mb64_tile_renders_water(level, above)) {
        return 1;
    }
    if (above == NULL || mb64_level_faceshape_at(level, grid_x, grid_y + 1, grid_z,
                                                MB64_MESH_FACE_TOP) != MB64_FACESHAPE_FULL) {
        return 0;
    }
    if (tile->type == TILE_TYPE_TROLL && above->type == TILE_TYPE_TROLL) {
        return 0;
    }
    return mb64_tile_occludes_face(level, tile, above, MB64_MESH_FACE_TOP) ? 1 : 0;
}

int mb64_find_water_column_top(const mb64_level_t *level,
                               int grid_x,
                               int grid_y,
                               int grid_z,
                               int *out_top_grid_y) {
    if (out_top_grid_y != NULL) {
        *out_top_grid_y = -1;
    }
    if (level == NULL || level->tiles == NULL ||
        grid_x < 0 || grid_x >= MB64_GRID_SIZE ||
        grid_z < 0 || grid_z >= MB64_GRID_SIZE) {
        return 0;
    }

    if (grid_y >= MB64_GRID_SIZE) {
        grid_y = MB64_GRID_SIZE - 1;
    }
    if (grid_y < 0) {
        return 0;
    }

    if (level_water_at(level, grid_x, grid_y, grid_z)) {
        grid_y++;
        while (grid_y < MB64_GRID_SIZE && level_water_at(level, grid_x, grid_y, grid_z)) {
            grid_y++;
        }
        grid_y--;
    } else {
        grid_y--;
        while (grid_y > -1 && !level_water_at(level, grid_x, grid_y, grid_z)) {
            grid_y--;
        }
        if (grid_y == -1) {
            return 0;
        }
    }

    if (out_top_grid_y != NULL) {
        *out_top_grid_y = grid_y;
    }
    return 1;
}

int mb64_find_water_surface(const mb64_level_t *level,
                            int grid_x,
                            int grid_y,
                            int grid_z,
                            int *out_top_grid_y,
                            int *out_fullblock) {
    int top_grid_y = -1;
    if (out_top_grid_y != NULL) {
        *out_top_grid_y = -1;
    }
    if (out_fullblock != NULL) {
        *out_fullblock = 0;
    }
    if (!mb64_find_water_column_top(level, grid_x, grid_y, grid_z, &top_grid_y)) {
        return 0;
    }
    if (out_top_grid_y != NULL) {
        *out_top_grid_y = top_grid_y;
    }
    if (out_fullblock != NULL) {
        *out_fullblock = mb64_water_surface_is_fullblock(level, grid_x, top_grid_y, grid_z);
    }
    return 1;
}

int mb64_find_water_query_surface(const mb64_level_t *level,
                                  int grid_x,
                                  int grid_y,
                                  int grid_z,
                                  int *out_surface_grid_y,
                                  int *out_fullblock) {
    if (out_surface_grid_y != NULL) {
        *out_surface_grid_y = -1;
    }
    if (out_fullblock != NULL) {
        *out_fullblock = 0;
    }

    if (level == NULL || level->tiles == NULL ||
        grid_x < 0 || grid_x >= MB64_GRID_SIZE ||
        grid_y < 0 || grid_y >= MB64_GRID_SIZE ||
        grid_z < 0 || grid_z >= MB64_GRID_SIZE) {
        return 0;
    }

    return mb64_find_water_surface(level, grid_x, grid_y, grid_z,
                                   out_surface_grid_y, out_fullblock);
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

uint8_t mb64_tile_occludes_face(const mb64_level_t *level,
                                const mb64_tile_t *cur,
                                const mb64_tile_t *other,
                                uint8_t direction) {
    if (!tile_is_solid(other)) {
        return 0;
    }
    return !mb64_cutout_skip_culling_check(level, cur, other, direction);
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
    const int16_t levelGridSize = mb64_level_grid_size(level);
    const int16_t gridSize =
        (flags & (MB64_BOUNDARY_OUTER_FLOOR | MB64_BOUNDARY_CEILING))
            ? levelGridSize
            : levelGridSize + 16;
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

static uint32_t boundary_wall_face_count(const mb64_level_t *level) {
    uint8_t flags = boundary_flags(level);
    if (flags & (MB64_BOUNDARY_INNER_WALLS | MB64_BOUNDARY_OUTER_WALLS)) {
        return (uint32_t)(sizeof(s_boundary_walls) / sizeof(s_boundary_walls[0]));
    }
    return 0;
}

static int16_t boundary_tc_from_subunits(int16_t value) {
    return (int16_t)((int32_t)value * 64);
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

    const int16_t gridScale = (int16_t)(mb64_level_grid_size(level) / 4);
    for (uint8_t i = 0; i < 4; i++) {
        face->v[i][0] = (int16_t)(quad->v[i][0] * gridScale);
        face->v[i][1] = y;
        face->v[i][2] = (int16_t)(quad->v[i][1] * gridScale);
        face->tc[i][0] = boundary_tc_from_subunits(face->v[i][0]);
        face->tc[i][1] = boundary_tc_from_subunits(face->v[i][2]);
    }
}

static uint8_t boundary_wall_direction(const mb64_boundary_wall_quad_t *quad, uint8_t reverse) {
    int8_t x = quad->v[0][0];
    int8_t z = quad->v[0][2];
    uint8_t sameX = 1;
    uint8_t sameZ = 1;
    for (uint8_t i = 1; i < 4; i++) {
        if (quad->v[i][0] != x) { sameX = 0; }
        if (quad->v[i][2] != z) { sameZ = 0; }
    }
    if (sameX) {
        if (x >= 0) { return reverse ? MB64_MESH_FACE_POS_X : MB64_MESH_FACE_NEG_X; }
        return reverse ? MB64_MESH_FACE_NEG_X : MB64_MESH_FACE_POS_X;
    }
    if (sameZ) {
        if (z >= 0) { return reverse ? MB64_MESH_FACE_POS_Z : MB64_MESH_FACE_NEG_Z; }
        return reverse ? MB64_MESH_FACE_NEG_Z : MB64_MESH_FACE_POS_Z;
    }
    return MB64_MESH_FACE_POS_X;
}

static void emit_boundary_wall_face(mb64_mesh_t *mesh, uint32_t *idx,
                                    const mb64_level_t *level,
                                    const mb64_boundary_wall_quad_t *quad,
                                    int16_t y_bottom, int16_t y_top, uint8_t reverse) {
    mb64_mesh_face_t *face = &mesh->faces[(*idx)++];
    mb64_tile_t material_tile;
    memset(&material_tile, 0, sizeof(material_tile));
    material_tile.mat = level->header.boundary_mat;

    face->material = material_tile.mat;
    face->resolved_material = mb64_resolve_tile_material(level, &material_tile, 0);
    face->tile_type = 0;
    face->direction = boundary_wall_direction(quad, reverse);
    face->is_water = 0;
    face->vertex_count = 4;
    face->use_tc = 1;
    face->tile_x = UINT8_MAX;
    face->tile_y = UINT8_MAX;
    face->tile_z = UINT8_MAX;

    const int16_t gridScale = (int16_t)(mb64_level_grid_size(level) / 4);
    const int16_t yHeight = (int16_t)(y_top - y_bottom);
    for (uint8_t i = 0; i < 4; i++) {
        const uint8_t src = reverse ? (uint8_t)((i == 1) ? 2 : (i == 2) ? 1 : i) : i;
        face->v[i][0] = (int16_t)(quad->v[src][0] * gridScale);
        face->v[i][1] = (int16_t)((quad->v[src][1] * yHeight + y_bottom) * MB64_TILE_SUBUNITS);
        face->v[i][2] = (int16_t)(quad->v[src][2] * gridScale);
        face->tc[i][0] = boundary_tc_from_subunits(
            face->direction == MB64_MESH_FACE_POS_X ||
            face->direction == MB64_MESH_FACE_NEG_X ? face->v[i][2] : face->v[i][0]);
        face->tc[i][1] = boundary_tc_from_subunits(face->v[i][1]);
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

static void emit_boundary_wall_faces(mb64_mesh_t *mesh, uint32_t *idx,
                                     const mb64_level_t *level) {
    const uint8_t flags = boundary_flags(level);
    if (flags & MB64_BOUNDARY_INNER_WALLS) {
        const int16_t bottomY = (flags & MB64_BOUNDARY_INNER_FLOOR) ? -32 : -40;
        const int16_t topY = (int16_t)level->header.boundary_height - 32;
        for (uint32_t i = 0; i < (uint32_t)(sizeof(s_boundary_walls) / sizeof(s_boundary_walls[0])); i++) {
            emit_boundary_wall_face(mesh, idx, level, &s_boundary_walls[i], bottomY, topY, 0);
        }
    } else if (flags & MB64_BOUNDARY_OUTER_WALLS) {
        for (uint32_t i = 0; i < (uint32_t)(sizeof(s_boundary_walls) / sizeof(s_boundary_walls[0])); i++) {
            emit_boundary_wall_face(mesh, idx, level, &s_boundary_walls[i], -42, -32, 1);
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
        return MB64_SURFACE_DEFAULT;
    }
    return mb64_surface_for_material(mb64_resolve_tile_material(level, tile, 1));
}

int16_t mb64_surface_for_mesh_face(const mb64_mesh_face_t *face) {
    if (face == NULL) {
        return MB64_SURFACE_DEFAULT;
    }
    switch (face->tile_type) {
        case TILE_TYPE_FENCE:
            return MB64_SURFACE_NO_CAM_COLLISION;
        case TILE_TYPE_BARS:
            return MB64_SURFACE_VANISH_CAP_WALLS;
        default:
            return mb64_surface_for_material(face->resolved_material);
    }
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

static uint8_t render_class_for_material_type(uint8_t type) {
    switch (type) {
        case MAT_DECAL:
            return MB64_RENDER_CLASS_DECAL;
        case MAT_CUTOUT:
            return MB64_RENDER_CLASS_CUTOUT;
        case MAT_CUTOUT_NOCULL:
            return MB64_RENDER_CLASS_CUTOUT_NOCULL;
        case MAT_TRANSPARENT:
            return MB64_RENDER_CLASS_TRANSPARENT;
        case MAT_SCREEN:
            return MB64_RENDER_CLASS_SCREEN;
        case MAT_OPAQUE:
        default:
            return MB64_RENDER_CLASS_OPAQUE;
    }
}

int mb64_render_binding_for_face(const mb64_level_t *level,
                                 const mb64_mesh_face_t *face,
                                 mb64_render_binding_t *out) {
    if (level == NULL || face == NULL || out == NULL) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->material = face->resolved_material;

    if (face->is_water) {
        out->kind = MB64_RENDER_BINDING_WATER;
        out->token = mb64_theme_specials_for_level(level)->water;
        out->render_class = MB64_RENDER_CLASS_TRANSPARENT;
        out->animation = mb64_texture_animation_for_water(level);
        return 1;
    }

    switch (face->resolved_material) {
        case MB64_RENDER_MATERIAL_FENCE:
            out->kind = MB64_RENDER_BINDING_FENCE;
            out->token = mb64_theme_specials_for_level(level)->fence;
            out->render_class = MB64_RENDER_CLASS_CUTOUT;
            out->cull_backfaces = 1;
            break;
        case MB64_RENDER_MATERIAL_BARS:
            out->kind = MB64_RENDER_BINDING_BARS;
            out->token = mb64_theme_specials_for_level(level)->bars;
            out->render_class = MB64_RENDER_CLASS_CUTOUT;
            out->cull_backfaces = 1;
            break;
        case MB64_RENDER_MATERIAL_BARS_TOP:
            out->kind = MB64_RENDER_BINDING_BARS_TOP;
            out->token = mb64_theme_specials_for_level(level)->bars;
            out->render_class = MB64_RENDER_CLASS_CUTOUT_NOCULL;
            break;
        case MB64_RENDER_MATERIAL_TTC_GRATE_TOP:
            out->kind = MB64_RENDER_BINDING_TTC_GRATE_TOP;
            out->render_class = MB64_RENDER_CLASS_OPAQUE;
            break;
        default:
            out->kind = MB64_RENDER_BINDING_MATERIAL;
            out->token = face->resolved_material;
            out->render_class = render_class_for_material_type(mb64_material_type(face->resolved_material));
            break;
    }

    out->animation = mb64_texture_animation_for_material(face->resolved_material);
    return 1;
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
                                            uint8_t direction,
                                            uint8_t faceshape,
                                            uint8_t material,
                                            const int8_t alt_uvs[4][2],
                                            uint8_t use_alt_uvs);

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
        assign_tile_texture_coordinates(face, t, local, direction,
                                        MB64_FACESHAPE_FULL, face->resolved_material,
                                        NULL, 0);
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
                                            uint8_t direction,
                                            uint8_t faceshape,
                                            uint8_t material,
                                            const int8_t alt_uvs[4][2],
                                            uint8_t use_alt_uvs) {
    uint8_t uv_direction = direction;
    if (mb64_material_vertical(material) && faceshape > MB64_FACESHAPE_EMPTY) {
        uv_direction = (uint8_t)((faceshape - MB64_FACESHAPE_EMPTY) + 1);
    }
    uint8_t u_axis;
    uint8_t v_axis;
    uint8_t flip_u = face_uv_axes(uv_direction, &u_axis, &v_axis);
    int32_t u_pos = flip_u ? 64 - tile_axis_value(tile, u_axis)
                           : tile_axis_value(tile, u_axis);
    int32_t v_pos = tile_axis_value(tile, v_axis);
    u_pos = mb64_uv_wrap_offset(u_pos);
    v_pos = mb64_uv_wrap_offset(v_pos);

    for (uint8_t i = 0; i < face->vertex_count; i++) {
        int16_t u;
        int16_t v;
        if (use_alt_uvs && alt_uvs != NULL) {
            u = (int16_t)(16 - alt_uvs[i][0]);
            v = (int16_t)(16 - alt_uvs[i][1]);
        } else {
            u = local[i][u_axis];
            v = (int16_t)(16 - local[i][v_axis]);
            if (!flip_u) {
                u = (int16_t)(16 - u);
            }
        }
        u = (int16_t)(u - u_pos * 16);
        v = (int16_t)(v - v_pos * 16);
        if (material == MB64_RENDER_MATERIAL_BARS) {
            /* MB64 bar side materials use N64 tile shift 15 on S/T, which
             * doubles texture frequency. Apply the same scale to generated
             * mesh UVs so non-MB64 renderers do not depend on display-list
             * tile-shift behavior to get the authored bar cell count. */
            u = (int16_t)(u * 2);
            v = (int16_t)(v * 2);
        }
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

static uint8_t faceshape_at(int x, int y, int z, uint8_t direction, int collision_mesh) {
    const mb64_tile_t *tile = tile_at(x, y, z);
    const mb64_shape_t *shape = shape_for_tile(tile, collision_mesh);
    if (shape == NULL) {
        return solid_at(x, y, z) ? MB64_FACESHAPE_FULL : MB64_FACESHAPE_EMPTY;
    }
    uint8_t local_dir = (uint8_t)(rotate_direction(direction, (uint8_t)((4 - (tile->rot & 3)) & 3)) ^ 1);
    for (uint8_t i = 0; i < shape->face_count; i++) {
        if (shape->faces[i].geometry.direction == local_dir) {
            return shape->faces[i].geometry.faceshape;
        }
    }
    return MB64_FACESHAPE_EMPTY;
}

static int shape_face_is_occluded(const mb64_level_t *level,
                                  const mb64_tile_t *t,
                                  uint8_t direction,
                                  uint8_t faceshape,
                                  int collision_mesh) {
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
    uint8_t other = faceshape_at(ax, ay, az, direction, collision_mesh);
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
                                 int nz,
                                 int collision_mesh) {
    if (!solid_at(nx, ny, nz)) {
        return 0;
    }
    if (!mb64_tile_occludes_face(level, t, tile_at(nx, ny, nz), direction)) {
        return 0;
    }
    return faceshape_at(nx, ny, nz, direction, collision_mesh) == MB64_FACESHAPE_FULL;
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

static const mb64_shape_t *shape_for_tile(const mb64_tile_t *t, int collision_mesh) {
    if (t == NULL || t->type >= (sizeof(s_shapes) / sizeof(s_shapes[0]))) {
        return NULL;
    }
    if (collision_mesh &&
        t->type < (sizeof(s_collision_shapes) / sizeof(s_collision_shapes[0])) &&
        s_collision_shapes[t->type].faces != NULL) {
        return &s_collision_shapes[t->type];
    }
    if (s_shapes[t->type].faces == NULL) {
        return NULL;
    }
    return &s_shapes[t->type];
}

static int tile_uses_shaped_mesh(const mb64_tile_t *t, int collision_mesh) {
    return shape_for_tile(t, collision_mesh) != NULL &&
           t->type != TILE_TYPE_BLOCK &&
           t->type != TILE_TYPE_TROLL;
}

static void emit_base_shape_face(mb64_mesh_t *mesh, uint32_t *idx,
                                 const mb64_level_t *level,
                                 const mb64_tile_t *t,
                                 const mb64_shape_face_geometry_t *src,
                                 const mb64_shape_face_uv_t *uv,
                                 int collision_mesh) {
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
    if (shape_face_is_occluded(level, t, direction, src->faceshape, collision_mesh)) {
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
        assign_tile_texture_coordinates(face, t, local, direction,
                                        src->faceshape, face->resolved_material,
                                        uv->alt_uvs, uv->has_alt_uvs);
    }
}

static void bar_direction_offset(uint8_t direction, int *dx, int *dy, int *dz) {
    *dx = 0;
    *dy = 0;
    *dz = 0;
    switch (direction) {
        case MB64_MESH_FACE_TOP: *dy = 1; break;
        case MB64_MESH_FACE_BOTTOM: *dy = -1; break;
        case MB64_MESH_FACE_POS_X: *dx = 1; break;
        case MB64_MESH_FACE_NEG_X: *dx = -1; break;
        case MB64_MESH_FACE_POS_Z: *dz = 1; break;
        case MB64_MESH_FACE_NEG_Z: *dz = -1; break;
        default: break;
    }
}

static int water_block_side_is_solid(const mb64_level_t *level,
                                     const mb64_tile_t *adjacent,
                                     const mb64_tile_t *waterlogged,
                                     uint8_t direction) {
    const uint8_t adjacent_class =
        mb64_resolve_tile_side_class(level, adjacent, direction ^ 1);
    if (adjacent_class == CLASS_CUTOUT) {
        return 0;
    }
    if (adjacent_class != CLASS_HOLLOW_CUTOUT) {
        return 1;
    }

    return mb64_resolve_tile_side_class(level, waterlogged, MB64_MESH_FACE_BOTTOM) !=
           CLASS_HOLLOW_CUTOUT;
}

static uint8_t water_face_render_type(const mb64_level_t *level,
                                      const mb64_tile_t *t,
                                      uint8_t direction,
                                      uint8_t is_fullblock,
                                      int collision_mesh) {
    int dx = 0;
    int dy = 0;
    int dz = 0;
    bar_direction_offset(direction, &dx, &dy, &dz);
    if (level == NULL || t == NULL || (dx == 0 && dy == 0 && dz == 0)) {
        return 0;
    }
    if ((boundary_flags(level) & MB64_BOUNDARY_CEILING) &&
        level->header.boundary_height > 0 &&
        t->y == (uint8_t)(level->header.boundary_height - 1) &&
        direction == MB64_MESH_FACE_TOP) {
        mb64_tile_t boundary_tile;
        memset(&boundary_tile, 0, sizeof(boundary_tile));
        boundary_tile.mat = level->header.boundary_mat;
        if (mb64_material_type(mb64_resolve_tile_material(level, &boundary_tile, 0)) != MAT_CUTOUT) {
            return 0;
        }
    }

    const int nx = (int)t->x + dx;
    const int ny = (int)t->y + dy;
    const int nz = (int)t->z + dz;
    if (!coords_in_level_range(level, nx, ny, nz)) {
        const uint8_t type = is_fullblock ? 2 : 1;
        if (direction == MB64_MESH_FACE_TOP) {
            return 1;
        }
        if (direction == MB64_MESH_FACE_BOTTOM) {
            return (boundary_flags(level) & MB64_BOUNDARY_INNER_FLOOR) ? 0 : type;
        }
        return ((boundary_flags(level) & MB64_BOUNDARY_INNER_WALLS) &&
                t->y < level->header.boundary_height) ? 0 : type;
    }

    if (water_at(nx, ny, nz)) {
        return 0;
    }

    const mb64_tile_t *adj = find_level_tile(level, nx, ny, nz);
    if (adj != NULL && adj->type == TILE_TYPE_CULL) {
        return 0;
    }
    if (adj != NULL &&
        faceshape_at(nx, ny, nz, direction, collision_mesh) == MB64_FACESHAPE_FULL &&
        water_block_side_is_solid(level, adj, t, direction)) {
        return 0;
    }
    if (faceshape_at(t->x, t->y, t->z, direction ^ 1, collision_mesh) == MB64_FACESHAPE_FULL &&
        water_block_side_is_solid(level, t, adj, direction ^ 1)) {
        if (!is_fullblock && direction == MB64_MESH_FACE_TOP) {
            return 1;
        }
        return 0;
    }

    if (adj != NULL && adj->waterlogged) {
        if (direction != MB64_MESH_FACE_TOP && direction != MB64_MESH_FACE_BOTTOM &&
            is_fullblock &&
            !mb64_water_surface_is_fullblock(level, nx, ny, nz)) {
            return 3;
        }
        return 0;
    }

    return is_fullblock ? 2 : 1;
}

static int water_side_should_render(const mb64_level_t *level,
                                    const mb64_tile_t *t,
                                    uint8_t direction,
                                    int collision_mesh) {
    const uint8_t is_fullblock = (uint8_t)mb64_water_surface_is_fullblock(
        level, t != NULL ? t->x : 0, t != NULL ? t->y : 0, t != NULL ? t->z : 0);
    return water_face_render_type(level, t, direction, is_fullblock, collision_mesh) != 0;
}

static void check_bar_side_connections(const mb64_level_t *level,
                                       const mb64_tile_t *t,
                                       uint8_t connections[5],
                                       int collision_mesh) {
    for (uint8_t rot = 0; rot < 4; rot++) {
        int dx, dy, dz;
        const uint8_t dir = rotate_direction(MB64_MESH_FACE_POS_Z, rot);
        bar_direction_offset(dir, &dx, &dy, &dz);
        const int ax = (int)t->x + dx;
        const int ay = (int)t->y + dy;
        const int az = (int)t->z + dz;
        if (!coords_in_level_range(level, ax, ay, az)) {
            if ((level->header.boundary & MB64_BOUNDARY_INNER_WALLS) &&
                ay < level->header.boundary_height) {
                connections[rot] = 1;
            }
            continue;
        }
        const mb64_tile_t *adj = tile_at(ax, ay, az);
        if (adj != NULL && adj->type == TILE_TYPE_BARS) {
            connections[rot] = 1;
            continue;
        }
        if (faceshape_at(ax, ay, az, dir, collision_mesh) == MB64_FACESHAPE_FULL ||
            (adj != NULL && adj->type == TILE_TYPE_CULL)) {
            connections[rot] = 1;
        }
    }
}

static void check_bar_connections(const mb64_level_t *level,
                                  const mb64_tile_t *t,
                                  uint8_t connections[5],
                                  int collision_mesh) {
    memset(connections, 0, 5);
    check_bar_side_connections(level, t, connections, collision_mesh);
    connections[4] = 0;
    for (uint8_t updown = 0; updown < 2; updown++) {
        int dx, dy, dz;
        uint8_t adjacent_connections[5] = {0};
        bar_direction_offset(updown == 0 ? MB64_MESH_FACE_TOP : MB64_MESH_FACE_BOTTOM,
                             &dx, &dy, &dz);
        const int ax = (int)t->x + dx;
        const int ay = (int)t->y + dy;
        const int az = (int)t->z + dz;
        if (!coords_in_level_range(level, ax, ay, az)) {
            continue;
        }
        const mb64_tile_t *adj = tile_at(ax, ay, az);
        const uint8_t faceshape = faceshape_at(ax, ay, az, updown, collision_mesh);
        if (faceshape == MB64_FACESHAPE_FULL ||
            (adj != NULL && adj->type == TILE_TYPE_CULL)) {
            for (uint8_t rot = 0; rot < 4; rot++) {
                connections[rot] |= (uint8_t)(1 << (updown + 1));
            }
            connections[4] |= (uint8_t)(1 << (updown + 1));
        } else if (faceshape == MB64_FACESHAPE_TOPHALF && adj != NULL) {
            connections[(adj->rot + 2) & 3] |= (uint8_t)(1 << (updown + 1));
        } else if (adj != NULL && adj->type == TILE_TYPE_BARS) {
            check_bar_side_connections(level, adj, adjacent_connections, collision_mesh);
            for (uint8_t rot = 0; rot < 4; rot++) {
                connections[rot] |= (uint8_t)(adjacent_connections[rot] << (updown + 1));
            }
            connections[4] |= (uint8_t)(1 << (updown + 1));
        }
    }
    if (t->y == 0 && (level->header.boundary & MB64_BOUNDARY_INNER_FLOOR)) {
        for (uint8_t rot = 0; rot < 5; rot++) {
            connections[rot] |= 4;
        }
    }
    if ((level->header.boundary & MB64_BOUNDARY_CEILING) &&
        t->y == (uint8_t)(level->header.boundary_height - 1)) {
        for (uint8_t rot = 0; rot < 5; rot++) {
            connections[rot] |= 2;
        }
    }
}

static uint8_t bars_emitted_face_count(const mb64_level_t *level,
                                       const mb64_tile_t *t,
                                       int collision_mesh) {
    uint8_t connections[5];
    uint8_t count = 0;
    check_bar_connections(level, t, connections, collision_mesh);
    for (uint8_t rot = 0; rot < 4; rot++) {
        const uint8_t left_rot = (rot + 3) & 3;
        const uint8_t right_rot = (rot + 1) & 3;
        if (BAR_CONNECTED_SIDE(connections[rot])) {
            count += 2;
        }
        if (!BAR_CONNECTED_SIDE(connections[rot]) ||
            (BAR_CONNECTED_SIDE(connections[left_rot]) &&
             BAR_CONNECTED_SIDE(connections[right_rot]))) {
            count++;
        }
    }
    for (uint8_t rot = 0; rot < 4; rot++) {
        if (BAR_CONNECTED_SIDE(connections[rot])) {
            if (!BAR_CONNECTED_TOP(connections[rot])) {
                count++;
            }
            if (!BAR_CONNECTED_BOTTOM(connections[rot])) {
                count++;
            }
        }
    }
    if (!BAR_CONNECTED_TOP(connections[4])) {
        count++;
    }
    if (!BAR_CONNECTED_BOTTOM(connections[4])) {
        count++;
    }
    return count;
}

static void emit_bars_faces(mb64_mesh_t *mesh,
                            uint32_t *idx,
                            const mb64_level_t *level,
                            const mb64_tile_t *t,
                            const mb64_shape_t *shape,
                            int collision_mesh) {
    uint8_t connections[5];
    check_bar_connections(level, t, connections, collision_mesh);
    for (uint8_t rot = 0; rot < 4; rot++) {
        mb64_tile_t rotated = *t;
        const uint8_t left_rot = (rot + 3) & 3;
        const uint8_t right_rot = (rot + 1) & 3;
        rotated.rot = rot;
        if (BAR_CONNECTED_SIDE(connections[rot])) {
            emit_base_shape_face(mesh, idx, level, &rotated,
                                 &shape->faces[1].geometry, &shape->faces[1].uv,
                                 collision_mesh);
            emit_base_shape_face(mesh, idx, level, &rotated,
                                 &shape->faces[2].geometry, &shape->faces[2].uv,
                                 collision_mesh);
        }
        if (!BAR_CONNECTED_SIDE(connections[rot]) ||
            (BAR_CONNECTED_SIDE(connections[left_rot]) &&
             BAR_CONNECTED_SIDE(connections[right_rot]))) {
            emit_base_shape_face(mesh, idx, level, &rotated,
                                 &shape->faces[0].geometry, &shape->faces[0].uv,
                                 collision_mesh);
        }
    }
    for (uint8_t rot = 0; rot < 4; rot++) {
        if (BAR_CONNECTED_SIDE(connections[rot])) {
            mb64_tile_t rotated = *t;
            rotated.rot = rot;
            if (!BAR_CONNECTED_TOP(connections[rot])) {
                emit_base_shape_face(mesh, idx, level, &rotated,
                                     &shape->faces[3].geometry, &shape->faces[3].uv,
                                     collision_mesh);
            }
            if (!BAR_CONNECTED_BOTTOM(connections[rot])) {
                emit_base_shape_face(mesh, idx, level, &rotated,
                                     &shape->faces[4].geometry, &shape->faces[4].uv,
                                     collision_mesh);
            }
        }
    }
    if (!BAR_CONNECTED_TOP(connections[4])) {
        emit_base_shape_face(mesh, idx, level, t,
                             &shape->faces[5].geometry, &shape->faces[5].uv,
                             collision_mesh);
    }
    if (!BAR_CONNECTED_BOTTOM(connections[4])) {
        emit_base_shape_face(mesh, idx, level, t,
                             &shape->faces[6].geometry, &shape->faces[6].uv,
                             collision_mesh);
    }
}

static int mb64_build_mesh(const mb64_level_t *level, mb64_mesh_t *mesh, int collision_mesh) {
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
        const int water = mb64_tile_renders_water(level, t);
        if ((collision_mesh ? mb64_tile_has_terrain_collision(t) : tile_is_solid(t))) {
            s_solid_grid[t->z][t->y][t->x] = 1;
            s_tile_grid[t->z][t->y][t->x] = t;
            mesh->solid_tile_count++;
        }
        if (water) {
            s_water_grid[t->z][t->y][t->x] = 1;
            mesh->water_tile_count++;
        }
    }

    uint32_t face_count = 0;
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!mb64_tile_renders_water(level, t) || water_at(t->x, t->y - 1, t->z)) { continue; }
        int y1 = t->y + 1;
        while (water_at(t->x, y1, t->z)) { y1++; }
        const mb64_tile_t *top_water = find_level_tile(level, t->x, y1 - 1, t->z);
        if (water_face_render_type(level, top_water != NULL ? top_water : t,
                                   MB64_MESH_FACE_TOP,
                                   (uint8_t)mb64_water_surface_is_fullblock(level, t->x, y1 - 1, t->z),
                                   collision_mesh) != 0) {
            face_count++;
        }
        if (water_side_should_render(level, t, MB64_MESH_FACE_NEG_X, collision_mesh)) { face_count++; }
        if (water_side_should_render(level, t, MB64_MESH_FACE_POS_X, collision_mesh)) { face_count++; }
        if (water_side_should_render(level, t, MB64_MESH_FACE_NEG_Z, collision_mesh)) { face_count++; }
        if (water_side_should_render(level, t, MB64_MESH_FACE_POS_Z, collision_mesh)) { face_count++; }
    }
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!(collision_mesh ? mb64_tile_has_terrain_collision(t) : tile_is_solid(t))) { continue; }
        const mb64_shape_t *shape = shape_for_tile(t, collision_mesh);
        if (tile_uses_shaped_mesh(t, collision_mesh) && shape != NULL) {
            face_count += t->type == TILE_TYPE_BARS
                ? bars_emitted_face_count(level, t, collision_mesh)
                : shape->face_count;
            continue;
        }
        int tx = t->x, ty = t->y, tz = t->z;
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_TOP, tx, ty + 1, tz, collision_mesh)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_BOTTOM, tx, ty - 1, tz, collision_mesh)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_X, tx - 1, ty, tz, collision_mesh)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_X, tx + 1, ty, tz, collision_mesh)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_Z, tx, ty, tz - 1, collision_mesh)) { face_count++; }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_Z, tx, ty, tz + 1, collision_mesh)) { face_count++; }
    }
    face_count += boundary_floor_face_count(level);
    face_count += boundary_wall_face_count(level);

    if (face_count == 0) { return 0; }
    mesh->faces = calloc(face_count, sizeof(*mesh->faces));
    if (mesh->faces == NULL) {
        memset(mesh, 0, sizeof(*mesh));
        return 0;
    }
    mesh->face_count = face_count;

    uint32_t out = 0;
    emit_boundary_floor_faces(mesh, &out, level);
    emit_boundary_wall_faces(mesh, &out, level);
    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!(collision_mesh ? mb64_tile_has_terrain_collision(t) : tile_is_solid(t))) { continue; }
        const mb64_shape_t *shape = shape_for_tile(t, collision_mesh);
        if (tile_uses_shaped_mesh(t, collision_mesh) && shape != NULL) {
            if (t->type == TILE_TYPE_BARS) {
                emit_bars_faces(mesh, &out, level, t, shape, collision_mesh);
            } else {
                for (uint8_t j = 0; j < shape->face_count; j++) {
                    emit_base_shape_face(mesh, &out, level, t,
                                         &shape->faces[j].geometry, &shape->faces[j].uv,
                                         collision_mesh);
                }
            }
            continue;
        }

        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        int tx = t->x, ty = t->y, tz = t->z;

        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_TOP, tx, ty + 1, tz, collision_mesh)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y1,z0},{x1,y1,z1},{x1,y1,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_TOP, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_BOTTOM, tx, ty - 1, tz, collision_mesh)) {
            const int16_t p[4][3] = {{x0,y0,z0},{x0,y0,z1},{x1,y0,z0},{x1,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_BOTTOM, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_X, tx - 1, ty, tz, collision_mesh)) {
            const int16_t p[4][3] = {{x0,y1,z0},{x0,y0,z0},{x0,y1,z1},{x0,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_X, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_X, tx + 1, ty, tz, collision_mesh)) {
            const int16_t p[4][3] = {{x1,y1,z1},{x1,y0,z1},{x1,y1,z0},{x1,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_X, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_NEG_Z, tx, ty, tz - 1, collision_mesh)) {
            const int16_t p[4][3] = {{x1,y1,z0},{x1,y0,z0},{x0,y1,z0},{x0,y0,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_Z, 0, p);
        }
        if (!full_face_is_occluded(level, t, MB64_MESH_FACE_POS_Z, tx, ty, tz + 1, collision_mesh)) {
            const int16_t p[4][3] = {{x0,y1,z1},{x0,y0,z1},{x1,y1,z1},{x1,y0,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_Z, 0, p);
        }
    }

    for (uint32_t i = 0; i < level->header.tile_count; i++) {
        const mb64_tile_t *t = &level->tiles[i];
        if (!mb64_tile_renders_water(level, t)) { continue; }
        if (water_at(t->x, t->y - 1, t->z)) { continue; }
        int16_t x0, x1, y0, y1, z0, z1;
        tile_bounds(t, &x0, &x1, &y0, &y1, &z0, &z1);
        while (water_at(t->x, (y1 / MB64_TILE_SUBUNITS), t->z)) {
            y1 = (int16_t)(y1 + MB64_TILE_SUBUNITS);
        }
        int16_t water_top_y = (int16_t)(y1 - 2);
        int16_t water_side_top_y = water_top_y;
        int16_t water_side_bottom_y = (int16_t)(y0 - 2);
        const mb64_tile_t *top_water = find_level_tile(level, t->x, (y1 / MB64_TILE_SUBUNITS) - 1, t->z);
        if (water_face_render_type(level, top_water != NULL ? top_water : t,
                                   MB64_MESH_FACE_TOP,
                                   (uint8_t)mb64_water_surface_is_fullblock(level, t->x, (y1 / MB64_TILE_SUBUNITS) - 1, t->z),
                                   collision_mesh) != 0) {
            const int16_t top[4][3] = {{x0,water_top_y,z1},{x0,water_top_y,z0},{x1,water_top_y,z1},{x1,water_top_y,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_TOP, 1, top);
        }
        if (water_side_should_render(level, t, MB64_MESH_FACE_NEG_X, collision_mesh)) {
            const int16_t p[4][3] = {{x0,water_side_top_y,z0},{x0,water_side_bottom_y,z0},{x0,water_side_top_y,z1},{x0,water_side_bottom_y,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_X, 1, p);
        }
        if (water_side_should_render(level, t, MB64_MESH_FACE_POS_X, collision_mesh)) {
            const int16_t p[4][3] = {{x1,water_side_top_y,z1},{x1,water_side_bottom_y,z1},{x1,water_side_top_y,z0},{x1,water_side_bottom_y,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_X, 1, p);
        }
        if (water_side_should_render(level, t, MB64_MESH_FACE_NEG_Z, collision_mesh)) {
            const int16_t p[4][3] = {{x1,water_side_top_y,z0},{x1,water_side_bottom_y,z0},{x0,water_side_top_y,z0},{x0,water_side_bottom_y,z0}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_NEG_Z, 1, p);
        }
        if (water_side_should_render(level, t, MB64_MESH_FACE_POS_Z, collision_mesh)) {
            const int16_t p[4][3] = {{x0,water_side_top_y,z1},{x0,water_side_bottom_y,z1},{x1,water_side_top_y,z1},{x1,water_side_bottom_y,z1}};
            emit_face(mesh, &out, level, t, MB64_MESH_FACE_POS_Z, 1, p);
        }
    }

    mesh->face_count = out;
    return out > 0;
}

int mb64_build_render_mesh(const mb64_level_t *level, mb64_mesh_t *mesh) {
    return mb64_build_mesh(level, mesh, 0);
}

int mb64_build_collision_mesh(const mb64_level_t *level, mb64_mesh_t *mesh) {
    return mb64_build_mesh(level, mesh, 1);
}

typedef struct {
    uint8_t direction;
    uint8_t is_water;
    uint8_t vertex_count;
    uint16_t resolved_material;
    int16_t vertices[4][3];
} mb64_face_duplicate_key_t;

typedef struct {
    mb64_face_duplicate_key_t key;
    uint32_t count;
    uint8_t occupied;
} mb64_face_duplicate_entry_t;

static int mesh_vertex_compare(const int16_t a[3], const int16_t b[3]) {
    for (uint8_t i = 0; i < 3; i++) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

static void mesh_duplicate_key_from_face(mb64_face_duplicate_key_t *key,
                                         const mb64_mesh_face_t *face) {
    memset(key, 0, sizeof(*key));
    key->direction = face->direction;
    key->is_water = face->is_water;
    key->vertex_count = face->vertex_count;
    key->resolved_material = face->resolved_material;
    for (uint8_t i = 0; i < face->vertex_count && i < 4; i++) {
        key->vertices[i][0] = face->v[i][0];
        key->vertices[i][1] = face->v[i][1];
        key->vertices[i][2] = face->v[i][2];
    }

    for (uint8_t i = 1; i < key->vertex_count && i < 4; i++) {
        int16_t vertex[3] = {
            key->vertices[i][0],
            key->vertices[i][1],
            key->vertices[i][2],
        };
        uint8_t j = i;
        while (j > 0 && mesh_vertex_compare(vertex, key->vertices[j - 1]) < 0) {
            key->vertices[j][0] = key->vertices[j - 1][0];
            key->vertices[j][1] = key->vertices[j - 1][1];
            key->vertices[j][2] = key->vertices[j - 1][2];
            j--;
        }
        key->vertices[j][0] = vertex[0];
        key->vertices[j][1] = vertex[1];
        key->vertices[j][2] = vertex[2];
    }
}

static uint32_t mesh_duplicate_hash_key(const mb64_face_duplicate_key_t *key) {
    uint32_t hash = 2166136261u;
#define MB64_HASH_U32(value) do { \
        uint32_t mb64_hash_value = (uint32_t)(value); \
        for (uint8_t mb64_hash_i = 0; mb64_hash_i < 4; mb64_hash_i++) { \
            hash ^= (mb64_hash_value >> (mb64_hash_i * 8)) & 0xffu; \
            hash *= 16777619u; \
        } \
    } while (0)

    MB64_HASH_U32(key->direction);
    MB64_HASH_U32(key->is_water);
    MB64_HASH_U32(key->vertex_count);
    MB64_HASH_U32(key->resolved_material);
    for (uint8_t i = 0; i < key->vertex_count && i < 4; i++) {
        MB64_HASH_U32((uint16_t)key->vertices[i][0]);
        MB64_HASH_U32((uint16_t)key->vertices[i][1]);
        MB64_HASH_U32((uint16_t)key->vertices[i][2]);
    }

#undef MB64_HASH_U32
    return hash;
}

static int mesh_duplicate_keys_equal(const mb64_face_duplicate_key_t *a,
                                     const mb64_face_duplicate_key_t *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static uint32_t count_duplicate_mesh_faces(const mb64_mesh_t *mesh) {
    uint32_t duplicates = 0;
    if (mesh == NULL || mesh->faces == NULL) {
        return 0;
    }

    size_t capacity = 1;
    while (capacity < ((size_t)mesh->face_count * 2u)) {
        capacity <<= 1;
    }
    mb64_face_duplicate_entry_t *entries =
        (mb64_face_duplicate_entry_t *)calloc(capacity, sizeof(*entries));
    if (entries == NULL) {
        return 0;
    }

    for (uint32_t i = 0; i < mesh->face_count; i++) {
        mb64_face_duplicate_key_t key;
        mesh_duplicate_key_from_face(&key, &mesh->faces[i]);
        size_t slot = (size_t)mesh_duplicate_hash_key(&key) & (capacity - 1u);
        while (entries[slot].occupied) {
            if (mesh_duplicate_keys_equal(&entries[slot].key, &key)) {
                duplicates += entries[slot].count;
                entries[slot].count++;
                break;
            }
            slot = (slot + 1u) & (capacity - 1u);
        }
        if (!entries[slot].occupied) {
            entries[slot].key = key;
            entries[slot].count = 1;
            entries[slot].occupied = 1;
        }
    }

    free(entries);
    return duplicates;
}

static int mb64_visit_mesh(const mb64_level_t *level,
                           const mb64_mesh_visitor_t *visitor,
                           void *user,
                           int collision_mesh) {
    mb64_mesh_t mesh;
    if (!mb64_build_mesh(level, &mesh, collision_mesh)) {
        return 0;
    }

    mb64_mesh_info_t info;
    info.face_count = mesh.face_count;
    info.solid_tile_count = mesh.solid_tile_count;
    info.water_tile_count = mesh.water_tile_count;
    info.duplicate_face_count = count_duplicate_mesh_faces(&mesh);

    int ok = 1;
    if (visitor != NULL && visitor->begin != NULL) {
        ok = visitor->begin(&info, user);
    }
    if (ok && visitor != NULL && visitor->face != NULL) {
        for (uint32_t i = 0; i < mesh.face_count; i++) {
            if (!visitor->face(&mesh.faces[i], user)) {
                ok = 0;
                break;
            }
        }
    }
    if (ok && visitor != NULL && visitor->end != NULL) {
        ok = visitor->end(&info, user);
    }

    mb64_free_render_mesh(&mesh);
    return ok;
}

int mb64_visit_render_mesh(const mb64_level_t *level,
                           const mb64_mesh_visitor_t *visitor,
                           void *user) {
    return mb64_visit_mesh(level, visitor, user, 0);
}

int mb64_visit_collision_mesh(const mb64_level_t *level,
                              const mb64_mesh_visitor_t *visitor,
                              void *user) {
    return mb64_visit_mesh(level, visitor, user, 1);
}

void mb64_free_render_mesh(mb64_mesh_t *mesh) {
    if (mesh == NULL) { return; }
    free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}
