/**
 * Mario Builder 64 Level Disk API
 *
 * Provides level loading functionality from standard computer disk filesystem.
 * This complements the existing cartridge-based loading with support for
 * host computer file I/O, enabling integration with SM64CoopDX and other
 * projects that need to load .mb64 files from standard disk.
 */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef BSWAP16
#define BSWAP16(x) ((((x) & 0xFF) << 8) | (((x) & 0xFF00) >> 8))
#endif

#ifndef BSWAP32
#define BSWAP32(x) ((((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF000000) >> 24))
#endif

// Forward declarations for functions from level_api.c
extern int mb64_generate_level_objects(void);
extern void mb64_perform_file_upgrade(struct mb64_level_save_header *save, void *tile_data, void *obj_data);

// External data structures that host provides or we share
extern struct mb64_grid_obj mb64_grid_data[64][64][64];
extern struct mb64_tile mb64_tile_data[MB64_TILE_POOL_SIZE];
extern struct mb64_obj mb64_object_data[MB64_MAX_OBJS];
extern struct mb64_level_save_header mb64_save;
extern u16 mb64_tile_count;
extern u16 mb64_object_count;
extern u8 mb64_play_stars_max;

/**
 * Check if the loaded MB64 file has different endianness (e.g. from different platform)
 * Similar to save_file_need_bswap() in SM64CoopDX
 */
static u8 mb64_need_bswap(const struct mb64_level_save_header *header) {
    // Check if counts are unreasonable when interpreted as current endianness
    // If tile_count or object_count are way too high, likely wrong endianness
    if (header->tile_count > MB64_TILE_POOL_SIZE || header->object_count > MB64_MAX_OBJS) {
        // Try swapping and see if it makes more sense
        u16 swapped_tiles = BSWAP16(header->tile_count);
        u16 swapped_objects = BSWAP16(header->object_count);

        if (swapped_tiles <= MB64_TILE_POOL_SIZE && swapped_objects <= MB64_MAX_OBJS) {
            printf("[MB64] Detected wrong endianness - tile_count=%u->%u, object_count=%u->%u\n",
                   header->tile_count, swapped_tiles, header->object_count, swapped_objects);
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * Byteswap all multibyte fields in mb64_level_save_header
 * Similar to bswap_savefile() in SM64CoopDX
 */
static inline void bswap_mb64_header(struct mb64_level_save_header *header) {
    header->tile_count = BSWAP16(header->tile_count);
    header->object_count = BSWAP16(header->object_count);
    header->version = BSWAP16(header->version);
    header->theme = BSWAP16(header->theme);
    header->bg = BSWAP16(header->bg);
    header->costume = BSWAP16(header->costume);
    header->waterlevel = BSWAP16(header->waterlevel);
    // Leave strings (file_header, author, level_name) alone - they don't need swapping
}

/**
 * Byteswap all multibyte fields in mb64_tile array
 */
/**
 * Byteswap all multibyte fields in mb64_tile array
 * The mb64_tile struct uses bitfields packed into a single u32, so we swap the whole word
 */
static inline void bswap_mb64_tiles(struct mb64_tile *tiles, u16 count) {
    for (u16 i = 0; i < count; i++) {
        // Cast tile to u32 pointer and swap the entire 32-bit word containing all bitfields
        u32 *tile_word = (u32*)&tiles[i];
        *tile_word = BSWAP32(*tile_word);
    }
}

/**
 * Byteswap all multibyte fields in mb64_obj array
 */
static inline void bswap_mb64_objects(struct mb64_obj *objects, u16 count) {
    // All fields in mb64_obj are u8 (single byte), so no byte swapping needed
    (void)objects; // Suppress unused parameter warning
    (void)count;
}

/**
 * Apply endianness conversion to all loaded MB64 data if needed
 * This is the main endianness fixer function
 */
static void mb64_fix_endianness_if_needed(void) {
    if (mb64_need_bswap(&mb64_save)) {
        printf("[MB64] Applying endianness conversion...\n");

        // Fix header first
        bswap_mb64_header(&mb64_save);

        // Fix tiles (use the now-corrected count)
        if (mb64_save.tile_count > 0) {
            bswap_mb64_tiles(mb64_tile_data, mb64_save.tile_count);
        }

        // Fix objects (use the now-corrected count)
        if (mb64_save.object_count > 0) {
            bswap_mb64_objects(mb64_object_data, mb64_save.object_count);
        }

        printf("[MB64] Endianness conversion complete - tile_count=%u, object_count=%u\n",
               mb64_save.tile_count, mb64_save.object_count);
    } else {
        printf("[MB64] No endianness conversion needed\n");
    }
}

/**
 * Internal helper: Clear all level data
 */
static void mb64_clear_level_data(void) {
    bzero(&mb64_save, sizeof(mb64_save));
    bzero(&mb64_grid_data, sizeof(mb64_grid_data));
    bzero(&mb64_tile_data, sizeof(mb64_tile_data));
    bzero(&mb64_object_data, sizeof(mb64_object_data));

    mb64_tile_count = 0;
    mb64_object_count = 0;
    mb64_play_stars_max = 0;
}

/**
 * Check if a file is a valid Mario Builder 64 level file
 * @param filepath Path to file to check
 * @return 1 if valid .mb64 file, 0 if not, -1 on error
 */
int mb64_is_valid_level_file(const char* filepath) {
    if (!filepath) {
        return -1;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }

    struct mb64_level_save_header header;
    size_t bytes_read = fread(&header, sizeof(header), 1, file);
    fclose(file);

    if (bytes_read != 1) {
        return 0;
    }

    // Check if we need to swap endianness for validation
    if (mb64_need_bswap(&header)) {
        bswap_mb64_header(&header);
    }

    // Basic validation - check reasonable bounds after potential endianness fix
    if (header.tile_count > MB64_TILE_POOL_SIZE ||
        header.object_count > MB64_MAX_OBJS) {
        return 0;
    }

    return 1; // Looks like a valid level file
}

/**
 * Load a Mario Builder 64 level from standard disk file
 * Uses standard C file I/O instead of libcart FatFS
 */
int mb64_load_level_from_disk(const char* filepath) {
    printf("[MB64] mb64_load_level_from_disk: Loading level from %s\n", filepath);
    if (!filepath) {
        return -2;
    }

    // Clear existing data
    mb64_clear_level_data();

    // Open file using standard C I/O
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return -3; // File not found or cannot open
    }

    // Get file size for validation
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("[MB64] File size: %ld bytes\n", file_size);

    // Read header
    size_t bytes_read = fread(&mb64_save, 1, sizeof(mb64_save), file);
    if (bytes_read != sizeof(mb64_save)) {
        printf("[MB64] Failed to read header - only read %zu bytes, expected %zu\n",
               bytes_read, sizeof(mb64_save));
        fclose(file);
        return -4; // Failed to read header
    }

    printf("[MB64] Header read successfully - raw tile_count: %u, object_count: %u\n",
           mb64_save.tile_count, mb64_save.object_count);

    // Read tiles if header says there are any (before endianness conversion)
    u16 raw_tile_count = mb64_save.tile_count;
    u16 raw_object_count = mb64_save.object_count;

    // Try both interpretations to read the right amount of data
    u16 tiles_to_read = (raw_tile_count <= MB64_TILE_POOL_SIZE) ? raw_tile_count : BSWAP16(raw_tile_count);
    u16 objects_to_read = (raw_object_count <= MB64_MAX_OBJS) ? raw_object_count : BSWAP16(raw_object_count);

    if (tiles_to_read > 0 && tiles_to_read <= MB64_TILE_POOL_SIZE) {
        size_t tiles_size = sizeof(mb64_tile_data[0]) * tiles_to_read;
        bytes_read = fread(&mb64_tile_data, 1, tiles_size, file);
        if (bytes_read != tiles_size) {
            printf("[MB64] Failed to read tiles - read %zu bytes, expected %zu\n",
                   bytes_read, tiles_size);
            fclose(file);
            return -6; // Failed to read tiles
        }
        printf("[MB64] Successfully read %u tiles (%zu bytes)\n", tiles_to_read, tiles_size);
    }

    // Read objects if header says there are any
    if (objects_to_read > 0 && objects_to_read <= MB64_MAX_OBJS) {
        size_t objects_size = sizeof(mb64_object_data[0]) * objects_to_read;
        bytes_read = fread(&mb64_object_data, 1, objects_size, file);
        if (bytes_read != objects_size) {
            printf("[MB64] Failed to read objects - read %zu bytes, expected %zu\n",
                   bytes_read, objects_size);
            fclose(file);
            return -7; // Failed to read objects
        }
        printf("[MB64] Successfully read %u objects (%zu bytes)\n", objects_to_read, objects_size);
    }

    fclose(file);

    // CRITICAL: Fix endianness BEFORE any validation or processing
    mb64_fix_endianness_if_needed();

    // Now validate with corrected values
    printf("[MB64] After endianness check:\n");
    printf("[MB64] File header: '%.10s'\n", mb64_save.file_header);
    printf("[MB64] Author: '%.30s'\n", mb64_save.author);
    printf("[MB64] tile_count: %u, object_count: %u\n", mb64_save.tile_count, mb64_save.object_count);

    // Final validation with corrected values
    if (mb64_save.tile_count > MB64_TILE_POOL_SIZE ||
        mb64_save.object_count > MB64_MAX_OBJS) {
        printf("[MB64] ERROR: Invalid counts after endianness correction - tile_count=%u > %d or object_count=%u > %d\n",
               mb64_save.tile_count, MB64_TILE_POOL_SIZE, mb64_save.object_count, MB64_MAX_OBJS);
        return -5; // Invalid data
    }

    // Apply compatibility upgrades if needed
    mb64_perform_file_upgrade(&mb64_save, &mb64_tile_data, &mb64_object_data);

    // Update counts
    mb64_tile_count = mb64_save.tile_count;
    mb64_object_count = mb64_save.object_count;

    printf("[MB64] Level loaded successfully: %u tiles, %u objects\n",
           mb64_tile_count, mb64_object_count);

    return 0; // Success
}

/**
 * One-call level loading: load level data and generate objects in one step
 */
int mb64_load_level_complete(const char* filepath) {
    int result = mb64_load_level_from_disk(filepath);
    if (result != 0) {
        return result;
    }

    // Generate level objects after successful load
    result = mb64_generate_level_objects();
    if (result != 0) {
        printf("[MB64] Failed to generate level objects: %d\n", result);
        return -9; // Failed to generate objects
    }

    printf("[MB64] Level loading and object generation complete\n");
    return 0;
}

/**
 * Load level from memory buffer (also applies endianness fixing)
 */
int mb64_load_level_from_buffer(const void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(mb64_save)) {
        return -2;
    }

    // Clear existing data
    mb64_clear_level_data();

    // Copy header
    memcpy(&mb64_save, buffer, sizeof(mb64_save));
    const u8* buf_ptr = (const u8*)buffer + sizeof(mb64_save);
    size_t remaining = buffer_size - sizeof(mb64_save);

    // Determine how much data to read (before endianness conversion)
    u16 raw_tile_count = mb64_save.tile_count;
    u16 raw_object_count = mb64_save.object_count;

    u16 tiles_to_read = (raw_tile_count <= MB64_TILE_POOL_SIZE) ? raw_tile_count : BSWAP16(raw_tile_count);
    u16 objects_to_read = (raw_object_count <= MB64_MAX_OBJS) ? raw_object_count : BSWAP16(raw_object_count);

    // Copy tiles
    if (tiles_to_read > 0) {
        size_t tiles_size = sizeof(mb64_tile_data[0]) * tiles_to_read;
        if (remaining < tiles_size) return -6;
        memcpy(&mb64_tile_data, buf_ptr, tiles_size);
        buf_ptr += tiles_size;
        remaining -= tiles_size;
    }

    // Copy objects
    if (objects_to_read > 0) {
        size_t objects_size = sizeof(mb64_object_data[0]) * objects_to_read;
        if (remaining < objects_size) return -7;
        memcpy(&mb64_object_data, buf_ptr, objects_size);
    }

    // CRITICAL: Fix endianness after loading from buffer
    mb64_fix_endianness_if_needed();

    if (mb64_save.tile_count > MB64_TILE_POOL_SIZE ||
        mb64_save.object_count > MB64_MAX_OBJS) {
        return -5;
    }

    // Apply compatibility upgrades
    mb64_perform_file_upgrade(&mb64_save, &mb64_tile_data, &mb64_object_data);

    // Update counts
    mb64_tile_count = mb64_save.tile_count;
    mb64_object_count = mb64_save.object_count;

    return 0;
}
