#ifndef MB64_SAVE_FORMAT_H
#define MB64_SAVE_FORMAT_H

#ifndef MB64_FORMAT_U8
#include <stdint.h>
#define MB64_FORMAT_U8 uint8_t
#define MB64_FORMAT_S8 int8_t
#define MB64_FORMAT_U16 uint16_t
#define MB64_FORMAT_U64 uint64_t
#endif

#ifndef NUM_MATERIALS_PER_THEME
#define NUM_MATERIALS_PER_THEME 10
#endif

#ifndef MB64_MAX_TRAJECTORIES
#define MB64_MAX_TRAJECTORIES 20
#endif

#ifndef MB64_TRAJECTORY_LENGTH
#define MB64_TRAJECTORY_LENGTH 50
#endif

#ifndef MB64_PIKTCHER_WIDTH
#define MB64_PIKTCHER_WIDTH 64
#endif

#ifndef MB64_PIKTCHER_HEIGHT
#define MB64_PIKTCHER_HEIGHT 64
#endif

#ifndef MB64_PIKTCHER_SIZE
#define MB64_PIKTCHER_SIZE (MB64_PIKTCHER_WIDTH * MB64_PIKTCHER_HEIGHT)
#endif

struct mb64_custom_theme {
    MB64_FORMAT_U8 mats[NUM_MATERIALS_PER_THEME];
    MB64_FORMAT_U8 topmats[NUM_MATERIALS_PER_THEME];
    MB64_FORMAT_U8 topmatsEnabled[NUM_MATERIALS_PER_THEME];
    MB64_FORMAT_U8 fence;
    MB64_FORMAT_U8 pole;
    MB64_FORMAT_U8 bars;
    MB64_FORMAT_U8 water;
};

struct mb64_comptraj {
    MB64_FORMAT_S8 t;
    MB64_FORMAT_U8 x;
    MB64_FORMAT_U8 y;
    MB64_FORMAT_U8 z;
};

struct mb64_level_save_header {
    char file_header[10];
    MB64_FORMAT_U8 version;
    char author[31];
    MB64_FORMAT_U16 piktcher[MB64_PIKTCHER_HEIGHT][MB64_PIKTCHER_WIDTH];

    MB64_FORMAT_U8 costume;
    MB64_FORMAT_U8 seq[5];
    MB64_FORMAT_U8 envfx;
    MB64_FORMAT_U8 theme;
    MB64_FORMAT_U8 bg;
    MB64_FORMAT_U8 boundary_mat;
    MB64_FORMAT_U8 boundary;
    MB64_FORMAT_U8 boundary_height;
    MB64_FORMAT_U8 coinstar;
    MB64_FORMAT_U8 size;
    MB64_FORMAT_U8 waterlevel;
    MB64_FORMAT_U8 secret;
    MB64_FORMAT_U8 game;

    MB64_FORMAT_U8 toolbar[9];
    MB64_FORMAT_U8 toolbar_params[9];
    MB64_FORMAT_U16 tile_count;
    MB64_FORMAT_U16 object_count;

    struct mb64_custom_theme custom_theme;
    struct mb64_comptraj trajectories[MB64_MAX_TRAJECTORIES][MB64_TRAJECTORY_LENGTH];

    MB64_FORMAT_U64 pad;
};

#endif
