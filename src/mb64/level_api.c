// Mario Builder 64 Level API - Minimal level loading functions for SM64CoopDX integration
// Extracted from main.c to provide level loading/playback without heavy dependencies

#include "main.h"
#include <string.h>

// External data structures from main.c that would be provided by host
extern struct mb64_grid_obj mb64_grid_data[64][64][64];
extern struct mb64_tile mb64_tile_data[MB64_TILE_POOL_SIZE];
extern struct mb64_obj mb64_object_data[MB64_MAX_OBJS];
extern struct mb64_level_save_header mb64_save;
extern u16 mb64_tile_count;
extern u16 mb64_object_count;
extern u8 mb64_play_stars_max;

// File I/O functions - host must provide
#include "libcart/ff/ff.h"
extern FIL mb64_file;
extern FILINFO mb64_file_info;
extern char mb64_file_name[MAX_FILE_NAME_SIZE];

// Minimal level loading function
// Host must provide: bzero, file I/O, mb64_save structure
int mb64_load_level_data(const char* filename __attribute__((unused))) {
    bzero(&mb64_save, sizeof(mb64_save));
    bzero(&mb64_grid_data, sizeof(mb64_grid_data));

    TCHAR path[256];
    create_level_file_path(path, filename, NULL);
    FRESULT code = f_stat(path, &mb64_file_info);

    if (code != FR_OK) {
        return -1; // File not found
    }

    UINT bytes_read;
    f_open(&mb64_file, path, FA_READ);

    // Read header
    f_read(&mb64_file, &mb64_save, sizeof(mb64_save), &bytes_read);

    // Read tiles
    f_read(&mb64_file, &mb64_tile_data, sizeof(mb64_tile_data[0]) * mb64_save.tile_count, &bytes_read);

    // Read objects
    f_read(&mb64_file, &mb64_object_data, sizeof(mb64_object_data[0]) * mb64_save.object_count, &bytes_read);

    f_close(&mb64_file);

    mb64_tile_count = mb64_save.tile_count;
    mb64_object_count = mb64_save.object_count;

    return 0; // Success
}

// Minimal object generation function
// Host must provide: spawn_object, gMarioObject, object behavior constants
int mb64_generate_level_objects(void) {
    struct Object *obj;
    u32 i;
    mb64_play_stars_max = 0;

    for(i = 0; i < mb64_object_count; i++) {
        struct mb64_object_info *info = &mb64_object_type_list[mb64_object_data[i].type];
        s32 param = mb64_object_data[i].bparam;

        obj = spawn_object(gMarioObject, info->model_id, info->behavior);
        obj->oPosX = GRID_TO_POS(mb64_object_data[i].x);
        obj->oPosY = GRID_TO_POS(mb64_object_data[i].y) - TILE_SIZE/2 + info->y_offset;
        obj->oPosZ = GRID_TO_POS(mb64_object_data[i].z);
        obj->oFaceAngleYaw = mb64_object_data[i].rot * 0x4000;
        obj->oMoveAngleYaw = mb64_object_data[i].rot * 0x4000;
        obj->oBehParams2ndByte = param;
        obj->oBehParams = (param << 16);
        obj->oImbue = mb64_object_data[i].imbue;

        // Assign star IDs
        if ((info->flags & OBJ_TYPE_STAR) || (mb64_object_data[i].imbue == IMBUE_STAR)) {
            if (mb64_play_stars_max < 63) {
                obj->oBehParams = ((mb64_play_stars_max << 24) | (obj->oBehParams2ndByte << 16));
                mb64_play_stars_max++;
            }
        }
    }

    return 0; // Success
}

// Data accessor functions - these should always work
u16 mb64_get_object_count(void) {
    return mb64_object_count;
}

u16 mb64_get_tile_count(void) {
    return mb64_tile_count;
}

struct mb64_obj* mb64_get_object_data(void) {
    return mb64_object_data;
}

struct mb64_tile* mb64_get_tile_data(void) {
    return mb64_tile_data;
}

struct mb64_level_save_header* mb64_get_save_data(void) {
    return &mb64_save;
}
