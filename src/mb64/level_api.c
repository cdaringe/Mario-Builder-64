// Mario Builder 64 Level API - Public interface for SM64CoopDX integration
// Provides complete level loading and playback functionality

#include "main.h"
#include <string.h>

// In external mode, main.c functions are provided by the host (SM64CoopDX)
// Forward declarations for functions that will be provided by main.c (external dependency)
extern void load_level(void);
extern void generate_objects_to_level(void);

// External data that will be provided by main.c (external dependency)
extern struct mb64_grid_obj mb64_grid_data[64][64][64];
extern struct mb64_tile mb64_tile_data[MB64_TILE_POOL_SIZE];
extern struct mb64_obj mb64_object_data[MB64_MAX_OBJS];
extern struct mb64_level_save_header mb64_save;
extern u16 mb64_tile_count;
extern u16 mb64_object_count;
extern u8 mb64_play_stars_max;
extern char mb64_file_name[MAX_FILE_NAME_SIZE];

// Level loading API - simple forwarding functions
// Host (SM64CoopDX) must provide main.c functionality

int mb64_load_level_data(const char* filename) {
    // Set the filename and call main.c's load_level() function
    strncpy(mb64_file_name, filename, MAX_FILE_NAME_SIZE - 1);
    mb64_file_name[MAX_FILE_NAME_SIZE - 1] = '\0';
    load_level();
    return (mb64_object_count > 0 || mb64_tile_count > 0) ? 0 : -1;
}

int mb64_generate_level_objects(void) {
    // Call main.c's generate_objects_to_level() function
    generate_objects_to_level();
    return 0;
}

// Data accessors - forward to existing globals from main.c
u16 mb64_get_object_count(void) { return mb64_object_count; }
u16 mb64_get_tile_count(void) { return mb64_tile_count; }
struct mb64_obj* mb64_get_object_data(void) { return mb64_object_data; }
struct mb64_tile* mb64_get_tile_data(void) { return mb64_tile_data; }
struct mb64_level_save_header* mb64_get_save_data(void) { return &mb64_save; }
struct mb64_grid_obj* mb64_get_grid_data(void) { return (struct mb64_grid_obj*)mb64_grid_data; }
u8 mb64_get_stars_max(void) { return mb64_play_stars_max; }
