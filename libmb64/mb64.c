/**
 * mb64.c — libmb64 implementation.
 *
 * Parses Mario Builder 64 (.mb64) binary files (big-endian) per the format
 * described in assets/kaitai_mb64.yaml.
 *
 * Byte offsets (derived from kaitai spec, all fields big-endian):
 *
 *   Offset  Size   Field
 *   ------  ----   -----
 *        0    10   file_header (ASCII)
 *       10     1   version (u1)
 *       11    31   author (ASCII)
 *       42  8192   piktcher (64*64 u2, stored in header.piktcher[MB64_PIKTCHER_SIZE])
 *     8234     1   costume
 *     8235     5   seq[5]
 *     8240     1   envfx
 *     8241     1   theme
 *     8242     1   bg
 *     8243     1   boundary_mat
 *     8244     1   boundary
 *     8245     1   boundary_height
 *     8246     1   coinstar
 *     8247     1   size
 *     8248     1   waterlevel
 *     8249     1   secret
 *     8250     1   game
 *     8251     9   toolbar[9]
 *     8260     9   toolbar_params[9]
 *     8269     1   magic_byte
 *     8270     2   tile_count (u2 BE)
 *     8272     2   object_count (u2 BE)
 *     8274    34   custom_theme (10+10+10+1+1+1+1)
 *     8308  4000   trajectories (20*50 * 4 bytes each)
 *    12308     8   pad (u8)
 *    12316     4   magic_bytes (u4)
 *    12320     -   tiles (tile_count * 4 bytes each)
 *    12320+T   -   objects (object_count * 8 bytes each)
 */

#include "mb64.h"
#include "mb64_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ── Byte offsets ─────────────────────────────────────────────────── */

#define OFF_FILE_HEADER    0
#define OFF_VERSION       10
#define OFF_AUTHOR        11
#define OFF_PIKTCHER      42
#define OFF_COSTUME     8234
#define OFF_SEQ         8235
#define OFF_ENVFX       8240
#define OFF_THEME       8241
#define OFF_BG          8242
#define OFF_BOUNDARY_MAT 8243
#define OFF_BOUNDARY    8244
#define OFF_BOUNDARY_H  8245
#define OFF_COINSTAR    8246
#define OFF_LEVEL_SIZE  8247
#define OFF_WATERLEVEL  8248
#define OFF_SECRET      8249
#define OFF_GAME        8250
#define OFF_TOOLBAR     8251
#define OFF_TOOLBAR_PARAMS 8260
#define OFF_MAGIC_BYTE  8269
#define OFF_TILE_COUNT  8270
#define OFF_OBJECT_COUNT 8272
#define OFF_CUSTOM_THEME 8274
#define OFF_TRAJECTORIES 8308
#define OFF_PAD         12308
#define OFF_MAGIC_BYTES 12316
#define OFF_TILES       12320

#define TILE_SIZE  4   /* u32 packed */
#define OBJ_SIZE   8   /* bparam,x,y,z,type,rot,imbue,pad */
#define TRAJ_SIZE  4   /* t,x,y,z */
#define TRAJ_COUNT (20 * 50)

/* ── Big-endian read helpers ──────────────────────────────────────── */

static inline uint16_t u16be(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* ── File I/O ─────────────────────────────────────────────────────── */

static uint8_t *slurp(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* ── Public API ───────────────────────────────────────────────────── */

mb64_level_t *mb64_load(const char *path) {
    if (!path) {
        MB64_LOG("MB64_PARSE", "error=null_path");
        return NULL;
    }

    MB64_LOG("MB64_PARSE", "stage=start path=%s", path);

    /* ── Read file ─────────────────────────────────────────────── */
    size_t file_size = 0;
    uint8_t *data = slurp(path, &file_size);
    if (!data) {
        MB64_LOG("MB64_PARSE", "error=cannot_read path=%s errno=%s", path, strerror(errno));
        return NULL;
    }
    MB64_LOG("MB64_PARSE", "stage=file_read file_size=%zu", file_size);

    /* ── Validate minimum size ─────────────────────────────────── */
    if (file_size < (size_t)(OFF_TILES + TILE_SIZE)) {
        MB64_LOG("MB64_PARSE", "error=file_too_small file_size=%zu min=%d",
                 file_size, OFF_TILES + TILE_SIZE);
        free(data);
        return NULL;
    }

    /* ── Allocate output struct ────────────────────────────────── */
    mb64_level_t *lvl = calloc(1, sizeof(mb64_level_t));
    if (!lvl) {
        MB64_LOG("MB64_PARSE", "error=oom");
        free(data);
        return NULL;
    }

    /* ── Parse header ──────────────────────────────────────────── */
    mb64_header_t *hdr = &lvl->header;

    memcpy(hdr->file_header, data + OFF_FILE_HEADER, 10);
    hdr->file_header[10] = '\0';
    hdr->version = data[OFF_VERSION];

    memcpy(hdr->author, data + OFF_AUTHOR, 31);
    hdr->author[31] = '\0';
    /* Trim embedded NULs in author string */
    for (int i = 0; i < 31; i++) {
        if (hdr->author[i] == '\0') break;
    }

    /* piktcher: 64×64 RGB5A1 thumbnail, big-endian u16 array */
    {
        const uint8_t *pp = data + OFF_PIKTCHER;
        for (int i = 0; i < MB64_PIKTCHER_SIZE; i++) {
            hdr->piktcher[i] = u16be(pp + i * 2);
        }
    }

    hdr->costume       = data[OFF_COSTUME];
    memcpy(hdr->seq,    data + OFF_SEQ, 5);
    hdr->envfx         = data[OFF_ENVFX];
    hdr->theme         = data[OFF_THEME];
    hdr->bg            = data[OFF_BG];
    hdr->boundary_mat  = data[OFF_BOUNDARY_MAT];
    hdr->boundary      = data[OFF_BOUNDARY];
    hdr->boundary_height = data[OFF_BOUNDARY_H];
    hdr->coinstar      = data[OFF_COINSTAR];
    hdr->level_size    = data[OFF_LEVEL_SIZE];
    hdr->waterlevel    = data[OFF_WATERLEVEL];
    hdr->secret        = data[OFF_SECRET];
    hdr->game          = data[OFF_GAME];
    memcpy(hdr->toolbar,        data + OFF_TOOLBAR,        9);
    memcpy(hdr->toolbar_params, data + OFF_TOOLBAR_PARAMS, 9);
    hdr->tile_count   = u16be(data + OFF_TILE_COUNT);
    hdr->object_count = u16be(data + OFF_OBJECT_COUNT);

    /* custom_theme: mats(10) topmats(10) topmats_enabled(10) fence pole bars water */
    {
        const uint8_t *ct = data + OFF_CUSTOM_THEME;
        memcpy(hdr->custom_theme.mats,            ct,      10);
        memcpy(hdr->custom_theme.topmats,         ct + 10, 10);
        memcpy(hdr->custom_theme.topmats_enabled, ct + 20, 10);
        hdr->custom_theme.fence = ct[30];
        hdr->custom_theme.pole  = ct[31];
        hdr->custom_theme.bars  = ct[32];
        hdr->custom_theme.water = ct[33];
    }

    /* trajectories: TRAJ_COUNT * 4 bytes (t:s1, x:u1, y:u1, z:u1)
     * Stored in lvl->trajectories (first-class field, not inside header). */
    {
        const uint8_t *tp = data + OFF_TRAJECTORIES;
        for (int i = 0; i < TRAJ_COUNT; i++, tp += TRAJ_SIZE) {
            lvl->trajectories[i].t = (int8_t)tp[0];
            lvl->trajectories[i].x = tp[1];
            lvl->trajectories[i].y = tp[2];
            lvl->trajectories[i].z = tp[3];
        }
    }

    MB64_LOG("MB64_PARSE",
             "magic=%.10s version=%u author=%s tile_count=%u object_count=%u "
             "theme=%u boundary=%u boundary_height=%u file_size=%zu",
             hdr->file_header, hdr->version, hdr->author,
             hdr->tile_count, hdr->object_count,
             hdr->theme, hdr->boundary, hdr->boundary_height, file_size);

    /* ── Validate declared sizes fit in file ───────────────────── */
    /* File sizes use the packed on-disk sizes; struct sizes for malloc. */
    size_t tiles_file    = (size_t)hdr->tile_count   * TILE_SIZE;
    size_t objects_file  = (size_t)hdr->object_count * OBJ_SIZE;
    size_t tiles_bytes   = (size_t)hdr->tile_count   * sizeof(mb64_tile_t);
    size_t objects_bytes = (size_t)hdr->object_count * sizeof(mb64_obj_t);
    size_t expected      = (size_t)OFF_TILES + tiles_file + objects_file;
    if (file_size < expected) {
        MB64_LOG("MB64_PARSE", "error=truncated expected=%zu actual=%zu",
                 expected, file_size);
        free(data);
        free(lvl);
        return NULL;
    }

    /* ── Decode tiles (GEOM_GEN stage) ────────────────────────── */
    uint32_t type_hist[32]  = {0};
    uint32_t wl_count       = 0;
    uint32_t unique_types   = 0;

    if (hdr->tile_count > 0) {
        lvl->tiles = malloc(tiles_bytes);
        if (!lvl->tiles) {
            MB64_LOG("MB64_GEOM_GEN", "error=oom tile_count=%u", hdr->tile_count);
            free(data); free(lvl);
            return NULL;
        }

        const uint8_t *tb = data + OFF_TILES;
        for (uint16_t i = 0; i < hdr->tile_count; i++) {
            uint32_t raw = u32be(tb + (size_t)i * TILE_SIZE);
            mb64_tile_t *t = &lvl->tiles[i];
            t->raw        = raw;
            t->x          = (uint8_t)((raw >> 26) & 0x3F);
            t->y          = (uint8_t)((raw >> 20) & 0x3F);
            t->z          = (uint8_t)((raw >> 14) & 0x3F);
            t->type       = (uint8_t)((raw >>  9) & 0x1F);
            t->mat        = (uint8_t)((raw >>  5) & 0x0F);
            t->rot        = (uint8_t)((raw >>  3) & 0x03);
            t->waterlogged= (uint8_t)((raw >>  2) & 0x01);

            if (hdr->version < 1 && t->type >= 12) {
                t->type = (uint8_t)(t->type + 2);
            }

            if (t->type < 32) type_hist[t->type]++;
            if (t->waterlogged) wl_count++;
        }
        for (int t = 0; t < 32; t++) if (type_hist[t] > 0) unique_types++;
    }

    MB64_LOG("MB64_GEOM_GEN",
             "tiles_decoded=%u unique_types=%u waterlogged=%u",
             hdr->tile_count, unique_types, wl_count);

    /* ── Decode objects (OBJ_SPAWN stage) ─────────────────────── */
    uint32_t obj_type_hist[256] = {0};
    uint32_t obj_unique         = 0;

    if (hdr->object_count > 0) {
        lvl->objects = malloc(objects_bytes);
        if (!lvl->objects) {
            MB64_LOG("MB64_OBJ_SPAWN", "error=oom object_count=%u", hdr->object_count);
            free(lvl->tiles); free(data); free(lvl);
            return NULL;
        }

        const uint8_t *ob = data + OFF_TILES + tiles_file;
        for (uint16_t i = 0; i < hdr->object_count; i++) {
            const uint8_t *o = ob + (size_t)i * OBJ_SIZE;
            mb64_obj_t *obj = &lvl->objects[i];
            obj->bparam = o[0];
            obj->x      = o[1];
            obj->y      = o[2];
            obj->z      = o[3];
            obj->type   = o[4];
            obj->rot    = o[5];
            obj->imbue  = o[6];
            /* o[7] is pad, discarded */

            obj_type_hist[obj->type]++;
        }
        for (int t = 0; t < 256; t++) if (obj_type_hist[t] > 0) obj_unique++;
    }

    MB64_LOG("MB64_OBJ_SPAWN",
             "objects_decoded=%u unique_types=%u",
             hdr->object_count, obj_unique);

    /* ── Collision summary ─────────────────────────────────────── */
    uint32_t solid_tiles = (hdr->tile_count > 0)
                           ? hdr->tile_count - type_hist[0]
                           : 0;
    MB64_LOG("MB64_COLLISION",
             "collision_tiles=%u collision_tris=%u water_planes=%u "
             "boundary=%u boundary_height=%u theme=%u",
             solid_tiles, solid_tiles * 2, wl_count,
             hdr->boundary, hdr->boundary_height, hdr->theme);

    MB64_LOG("MB64_PARSE", "stage=complete status=SUCCESS "
             "tile_count=%u object_count=%u",
             hdr->tile_count, hdr->object_count);

    free(data);
    return lvl;
}

void mb64_free(mb64_level_t *level) {
    if (!level) return;
    free(level->tiles);
    free(level->objects);
    free(level);
}
