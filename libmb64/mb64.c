/**
 * mb64.c - libmb64 implementation.
 *
 * mb64_load and src/mb64/file.c:load_level read the same save stream: the
 * shared mb64_level_save_header, then packed tiles, then packed objects.
 * Shared disk-layout structs and version-upgrade helpers live in
 * mb64_save_format.h so both loaders keep the same compatibility rules.
 *
 * The implementations stay separate because load_level is the editor/runtime
 * loader: it reads through FatFs/libcart, mutates global editor state, creates
 * template levels when no save exists, initializes UI/toolbox state, and places
 * terrain into the live MB64 grid. mb64_load is a host-side parser API: it reads
 * a named file, returns heap-owned decoded data, logs parse stages, and never
 * touches game globals.
 *
 * Multi-byte scalar fields are stored big-endian.
 */

#include "mb64.h"
#include "mb64_log.h"
#include "mb64_save_format.h"
#include "../include/seq_ids.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>

/* src/mb64/file.c reads this header, then tiles, then objects. */
#define MB64_DISK_HEADER_SIZE sizeof(struct mb64_level_save_header)

#define TILE_SIZE  4   /* u32 packed */
#define OBJ_SIZE   8   /* bparam,x,y,z,type,rot,imbue,pad */

/* Mirrors src/mb64/data.c:seq_musicmenu_array. Keep music index semantics in
 * libmb64 so external loaders do not maintain partial duplicate mappings. */
static const uint8_t s_music_sequence_by_index[] = {
    SEQ_LEVEL_GRASS,
    SEQ_LEVEL_SLIDE,
    SEQ_LEVEL_WATER,
    SEQ_LEVEL_WATER,
    SEQ_LEVEL_HOT,
    SEQ_LEVEL_SNOW,
    SEQ_LEVEL_SPOOKY,
    SEQ_LEVEL_UNDERGROUND,
    SEQ_LEVEL_UNDERGROUND,
    SEQ_LEVEL_KOOPA_ROAD_2,
    SEQ_VANILLA_BOSS,
    SEQ_LEVEL_BOSS_KOOPA,
    SEQ_LEVEL_BOSS_KOOPA_FINAL,
    SEQ_LEVEL_INSIDE_CASTLE2,
    SEQ_LEVEL_INSIDE_CASTLE,
    SEQ_REDHOT,
    SEQ_FARM,
    SEQ_JUNGLE,
    SEQ_PIRATE,
    SEQ_EVENT_CUTSCENE_ENDING,
    SEQ_BIG_HOUSE,
    SEQ_NSMB_CASTLE,
    SEQ_EVENT_BOSS,
    SEQ_LEVEL_KOOPA_ROAD,
    SEQ_COSMIC_SEED_BOSS,
    SEQ_SHOWRUNNER_BOSS,
    SEQ_COSMIC_SEED_LEVEL,
    SEQ_FINAL_BOSS,
    SEQ_SMS_BIANCO_HILLS,
    SEQ_SMS_SKY_AND_SEA,
    SEQ_SMS_SECRET_COURSE,
    SEQ_SMG_COMET_OBSERVATORY,
    SEQ_SMG_BUOY_BASE,
    SEQ_SMG_BATTLEROCK,
    SEQ_SMG_GHOSTLY_GALAXY,
    SEQ_SMG_PURPLE_COMET,
    SEQ_SMG2_HONEYBLOOM,
    SEQ_PIRANHA_CREEK,
    SEQ_NSMB_DESERT,
    SEQ_KOOPA_BEACH,
    SEQ_FRAPPE_SNOWLAND,
    SEQ_MK64_BOWSERS_CASTLE,
    SEQ_MK64_RAINBOW_ROAD,
    SEQ_MKDS_WALUIGI_PINBALL,
    SEQ_MK8_RAINBOW_ROAD,
    SEQ_SMRPG_MARIOS_PAD,
    SEQ_SMRPG_NIMBUS_LAND,
    SEQ_FOREST_MAZE,
    SEQ_SMRPG_SUNKEN_SHIP,
    SEQ_PM_DRY_DESERT,
    SEQ_PM_FOREVER_FOREST,
    SEQ_TTYD_PETAL_MEADOWS,
    SEQ_TTYD_EIGHT_KEY_DOMAIN,
    SEQ_TTYD_ROGUEPORT_SEWERS,
    SEQ_TTYD_XNAUT_FORTRESS,
    SEQ_SPM_FLIPSIDE,
    SEQ_SPM_LINELAND_ROAD,
    SEQ_SAMMER_KINGDOM,
    SEQ_SPM_FLORO_CAVERNS,
    SEQ_SPM_OVERTHERE_STAIR,
    SEQ_MP_YOSHIS_TROPICAL_ISLAND,
    SEQ_MP_RAINBOW_CASTLE,
    SEQ_MLPIT_BEHIND_YOSHI_VILLAGE,
    SEQ_PIT_GRITZY_DESERT,
    SEQ_BIS_BUMPSY_PLAINS,
    SEQ_BIS_DEEP_CASTLE,
    SEQ_YI_OVERWORLD,
    SEQ_YI_CRYSTAL_CAVES,
    SEQ_YS_TITLE,
    SEQ_OOT_KOKIRI_FOREST,
    SEQ_OOT_LOST_WOODS,
    SEQ_OOT_GERUDO_VALLEY,
    SEQ_STONE_TOWER_TEMPLE,
    SEQ_WW_OUTSET_ISLAND,
    SEQ_TP_LAKE_HYLIA,
    SEQ_TP_GERUDO_DESERT,
    SEQ_SS_SKYLOFT,
    SEQ_DK64_FRANTIC_FACTORY,
    SEQ_DK64_HIDEOUT_HELM,
    SEQ_DK_CREEPY_CASTLE,
    SEQ_DK64_GLOOMY_GALLEON,
    SEQ_DK64_FUNGI_FOREST,
    SEQ_DK64_CRYSTAL_CAVES,
    SEQ_DK64_ANGRY_AZTEC,
    SEQ_DKC2_SNOWBOUND_LAND,
    SEQ_BK_BUBBLEGLOOP_SWAMP,
    SEQ_BK_FREEZEEZY_PEAKS,
    SEQ_BK_GOBI_VALLEY,
    SEQ_K64_FACTORY_INSPECTION,
    SEQ_BM_GREEN_GARDEN,
    SEQ_BM_BLACK_FORTRESS,
    SEQ_SA_WINDY_HILL,
    SEQ_PKMN_SKY_TOWER,
    SEQ_TOUHOU_YOUKAI_MOUNTAIN,
    SEQ_FOREST_TEMPLE,
    SEQ_RAYMAN_BAND_LAND,
    SEQ_SMB1_OVERWORLD,
    SEQ_SMB_BOWSER_REMIX,
    SEQ_SMB2_OVERWORLD,
    SEQ_SMB3_OVERWORLD,
    SEQ_SMB3_CASTLE,
    SEQ_SMW_ATHLETIC,
    SEQ_SMW_CASTLE,
};

static const mb64_woodplat_config_t s_woodplat_config = {
    96.0f,   /* thin_height */
    256.0f,  /* fat_height */
    5.0f,    /* stack_dist_epsilon */
    127.8f,  /* wall_hitbox_radius */
    -4.0f,   /* gravity */
    0.0f,    /* buoyancy */
    80.0f,   /* water_probe_y */
    64.0f,   /* water_surface_offset */
    -1.0f,   /* mario_weight_vel */
    -2.0f,   /* ground_pound_weight_vel */
    0.9f,    /* water_drag */
    4.0f,    /* water_float_base */
    -2.0f,   /* water_float_min */
    2.0f,    /* water_float_max */
    384,     /* death_drop_offset */
    -20,     /* steep_slope_degrees */
};

static const mb64_bullet_bill_config_t s_bullet_bill_config = {
    400.0f,  /* wake_min_distance */
    1500.0f, /* wake_max_distance */
    3.0f,    /* shake_forward_speed */
    30.0f,   /* launch_forward_speed */
    127.0f,  /* floor_probe_offset_y */
    300.0f,  /* rotate_min_distance */
    0x2000,  /* wake_angle_threshold */
    40,      /* shake_start_frame */
    50,      /* launch_frame */
    150,     /* timeout_frame */
    90,      /* explosion_reset_frame */
    0x100,   /* rotate_step */
};

static const mb64_reinforced_box_config_t s_reinforced_box_config = {
    46.0f, /* break_coin_radius */
    6.0f,  /* shake_amplitude */
    3.0f,  /* shake_center_offset */
    15,    /* init_timer */
    10,    /* shake_timer_limit */
    15,    /* clank_cooldown_timer */
};

static const mb64_badge_config_t s_badge_config = {
    0x70,  /* spin_accel */
    0.95f, /* shrink_factor */
    0.2f,  /* delete_scale */
};

static const mb64_chicken_config_t s_chicken_config = {
    1,       /* behavior_param_2 */
    0,       /* animation_index */
    1.0f,    /* scale */
    50.0f,   /* wall_hitbox_radius */
    0.0f,    /* SET_OBJ_PHYSICS_AIR gravity */
    0.0f,    /* SET_OBJ_PHYSICS_AIR bounciness */
    1000.0f, /* MB64_DRAG_DEFAULT */
    1000.0f, /* MB64_FRICTION_DEFAULT */
    0.0f,    /* SET_OBJ_PHYSICS_AIR buoyancy */
    4000.0f, /* MB64_DRAWDIST_LOW */
};

static const mb64_podoboo_config_t s_podoboo_config = {
    2.0f,    /* gravity */
    1.5f,    /* launch_accel */
    2500.0f, /* mario_activation_distance */
    40.0f,   /* flame_y_offset */
    35,      /* flame_warmup_frame */
    50,      /* launch_frame */
    0x7FFF,  /* landing_roll_angle */
    0x0FFF,  /* roll_step */
    3,       /* splash_flame_count */
};

static const mb64_pokey_config_t s_pokey_config = {
    5,        /* segment_count */
    0,        /* head_part_index */
    3.0f,     /* scale */
    120.0f,   /* body_step */
    480.0f,   /* head_start_y */
    6.0f,     /* sway_radius */
    0.1f,     /* expand_step */
    3.0f,     /* standard_action_scale */
    22.0f,    /* graph_y_offset_scale */
    4.0f,     /* gravity */
    500.0f,   /* unload_distance_margin */
    5.0f,     /* forward_speed */
    25000.0f, /* far_mario_distance */
    2000.0f,  /* random_wander_distance */
    200.0f,   /* shy_min_distance */
    10.0f,    /* shy_angle_scale */
    120.0f,   /* quicksand_part_death_depth */
    30,       /* blink_min_frames */
    60,       /* blink_max_frames */
    4,        /* blink_random_frames */
    100,      /* regrow_frame */
    0x2000,   /* random_turn_step */
    30,       /* random_timer_min */
    50,       /* random_timer_range */
    0x200,    /* turn_step */
    -78,      /* steep_slope_degrees */
    20,       /* death_delay_base */
    2,        /* death_delay_shift */
    254,      /* quicksand_depth_to_die */
    384,      /* star_drop_height */
};

static float mb64_clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

uint8_t mb64_music_sequence_from_index(uint8_t music_index) {
    if (music_index >= sizeof(s_music_sequence_by_index) / sizeof(s_music_sequence_by_index[0])) {
        return 0;
    }
    return s_music_sequence_by_index[music_index];
}

const mb64_woodplat_config_t *mb64_woodplat_config(void) {
    return &s_woodplat_config;
}

float mb64_woodplat_piece_height(uint8_t bparam) {
    return bparam == 1 ? s_woodplat_config.fat_height : s_woodplat_config.thin_height;
}

float mb64_woodplat_stack_height(const uint8_t *bparams, size_t count) {
    float stack_height = 0.0f;

    if (bparams == NULL) {
        return 0.0f;
    }

    for (size_t i = 0; i < count; i++) {
        stack_height += mb64_woodplat_piece_height(bparams[i]);
    }

    return stack_height;
}

uint8_t mb64_woodplat_should_stack(uint8_t bparam, float nearest_distance) {
    return bparam == 1 && nearest_distance < s_woodplat_config.stack_dist_epsilon;
}

float mb64_woodplat_water_float_accel(float water_level, float platform_y) {
    return s_woodplat_config.water_float_base +
        mb64_clampf((water_level - s_woodplat_config.water_surface_offset - platform_y) * 0.1f,
                    s_woodplat_config.water_float_min,
                    s_woodplat_config.water_float_max);
}

float mb64_woodplat_water_velocity(float current_vel_y, float water_level, float platform_y,
                                   uint8_t mario_on_platform, uint8_t ground_pound_landing) {
    float vel_y = current_vel_y * s_woodplat_config.water_drag;
    vel_y += mb64_woodplat_water_float_accel(water_level, platform_y);
    if (mario_on_platform) {
        vel_y += s_woodplat_config.mario_weight_vel;
        if (ground_pound_landing) {
            vel_y += s_woodplat_config.ground_pound_weight_vel;
        }
    }
    return vel_y;
}

int mb64_woodplat_death_drop_offset(void) {
    return s_woodplat_config.death_drop_offset;
}

uint8_t mb64_woodplat_should_use_simple_wall_checks(uint8_t floor_is_conveyor,
                                                    uint8_t floor_object_has_vertical_push,
                                                    uint8_t on_ground) {
    return !(floor_is_conveyor && floor_object_has_vertical_push && on_ground);
}

uint8_t mb64_woodplat_should_die_on_death_barrier(uint8_t has_floor, uint8_t floor_is_death_plane,
                                                  float platform_y, float floor_y) {
    if (!has_floor) {
        return 1;
    }
    return floor_is_death_plane && platform_y < floor_y + 100.0f;
}

const mb64_bullet_bill_config_t *mb64_bullet_bill_config(void) {
    return &s_bullet_bill_config;
}

uint8_t mb64_bullet_bill_should_wake(int angle_diff, float distance) {
    return angle_diff < s_bullet_bill_config.wake_angle_threshold &&
        s_bullet_bill_config.wake_min_distance < distance &&
        distance < s_bullet_bill_config.wake_max_distance;
}

float mb64_bullet_bill_forward_velocity(int timer, float launch_speed) {
    if (timer < s_bullet_bill_config.shake_start_frame) {
        return s_bullet_bill_config.shake_forward_speed;
    }
    if (timer < s_bullet_bill_config.launch_frame) {
        return (timer & 1) ? s_bullet_bill_config.shake_forward_speed : -s_bullet_bill_config.shake_forward_speed;
    }
    return launch_speed;
}

uint8_t mb64_bullet_bill_should_launch(int timer) {
    return timer == s_bullet_bill_config.launch_frame;
}

uint8_t mb64_bullet_bill_should_floor_probe(int timer) {
    return timer >= s_bullet_bill_config.launch_frame + 1;
}

uint8_t mb64_bullet_bill_should_rotate_toward_player(float distance) {
    return distance > s_bullet_bill_config.rotate_min_distance;
}

uint8_t mb64_bullet_bill_should_timeout(int timer) {
    return timer > s_bullet_bill_config.timeout_frame;
}

uint8_t mb64_bullet_bill_should_reset_after_explosion(int timer) {
    return timer > s_bullet_bill_config.explosion_reset_frame;
}

const mb64_reinforced_box_config_t *mb64_reinforced_box_config(void) {
    return &s_reinforced_box_config;
}

uint8_t mb64_reinforced_box_should_clank(int timer) {
    return timer > s_reinforced_box_config.clank_cooldown_timer;
}

uint8_t mb64_reinforced_box_should_shake(int timer) {
    return timer < s_reinforced_box_config.shake_timer_limit;
}

float mb64_reinforced_box_shake_offset(float random_unit) {
    return random_unit * s_reinforced_box_config.shake_amplitude -
        s_reinforced_box_config.shake_center_offset;
}

const mb64_badge_config_t *mb64_badge_config(void) {
    return &s_badge_config;
}

uint8_t mb64_badge_is_equipped(uint32_t equipped_badges, uint8_t badge_id) {
    if (badge_id >= 32) {
        return 0;
    }
    return (equipped_badges & (1u << badge_id)) != 0;
}

uint8_t mb64_badge_should_collect(uint8_t equipped, uint8_t overlaps_mario, uint8_t mario_levelup_dance) {
    return equipped || (overlaps_mario && !mario_levelup_dance);
}

float mb64_badge_next_collect_scale(float current_scale) {
    return current_scale * s_badge_config.shrink_factor;
}

uint8_t mb64_badge_should_delete(float current_scale) {
    return current_scale < s_badge_config.delete_scale;
}

const mb64_chicken_config_t *mb64_chicken_config(void) {
    return &s_chicken_config;
}

const mb64_podoboo_config_t *mb64_podoboo_config(void) {
    return &s_podoboo_config;
}

float mb64_podoboo_launch_velocity(float rest_y, float peak_y) {
    float y = rest_y;
    float velocity = 0.0f;
    while (y < peak_y) {
        velocity += s_podoboo_config.launch_accel;
        y += velocity;
    }
    return velocity;
}

uint8_t mb64_podoboo_should_reset_idle_timer(float distance_to_mario) {
    return distance_to_mario > s_podoboo_config.mario_activation_distance;
}

uint8_t mb64_podoboo_should_spawn_warmup_flame(int timer) {
    return timer > s_podoboo_config.flame_warmup_frame;
}

uint8_t mb64_podoboo_should_launch(int timer) {
    return timer > s_podoboo_config.launch_frame;
}

const mb64_pokey_config_t *mb64_pokey_config(void) {
    return &s_pokey_config;
}

uint32_t mb64_pokey_alive_flags(uint8_t segment_count) {
    if (segment_count >= 32) {
        return UINT32_MAX;
    }
    return ((uint32_t)1 << segment_count) - 1u;
}

float mb64_pokey_part_spawn_y(uint8_t part_index) {
    return s_pokey_config.head_start_y - (float)part_index * s_pokey_config.body_step;
}

int mb64_pokey_part_offset_angle(uint8_t part_index, int timer) {
    return part_index * 0x4000 + timer * 0x800;
}

float mb64_pokey_part_base_height(float parent_y,
                                  uint8_t alive_parts,
                                  uint8_t part_index,
                                  float bottom_size,
                                  float quicksand_depth) {
    return parent_y
        + (s_pokey_config.body_step * (float)(alive_parts - part_index) - (s_pokey_config.body_step * 2.0f))
        + s_pokey_config.body_step * bottom_size
        - quicksand_depth;
}

float mb64_pokey_part_graph_y_offset(float scale_y) {
    return scale_y * s_pokey_config.graph_y_offset_scale;
}

int mb64_pokey_part_death_delay(uint8_t part_index) {
    return (part_index << s_pokey_config.death_delay_shift) + s_pokey_config.death_delay_base;
}

uint8_t mb64_pokey_should_shift_part(uint8_t part_index, uint32_t alive_flags) {
    return part_index > 1 && !(alive_flags & ((uint32_t)1 << (part_index - 1)));
}

uint8_t mb64_pokey_should_expand_bottom(float bottom_size,
                                        uint8_t part_index,
                                        uint8_t alive_parts) {
    return bottom_size < 1.0f && (uint8_t)(part_index + 1) == alive_parts;
}

uint8_t mb64_pokey_should_spawn_parts(float distance_to_mario, float drawing_distance) {
    return distance_to_mario < drawing_distance;
}

uint8_t mb64_pokey_should_unload(float distance_to_mario, float drawing_distance) {
    return distance_to_mario > drawing_distance + s_pokey_config.unload_distance_margin;
}

uint8_t mb64_pokey_should_regrow(uint8_t alive_parts, int timer, uint8_t floor_is_instant_quicksand) {
    return alive_parts < s_pokey_config.segment_count
        && timer > s_pokey_config.regrow_frame
        && !floor_is_instant_quicksand;
}

int mb64_pokey_target_angle_offset(float distance_to_mario, int angle_to_mario, int move_angle_yaw) {
    int target_angle_offset = (int)(0x4000 - (distance_to_mario - s_pokey_config.shy_min_distance) * s_pokey_config.shy_angle_scale);
    if (target_angle_offset < 0) {
        target_angle_offset = 0;
    } else if (target_angle_offset > 0x4000) {
        target_angle_offset = 0x4000;
    }

    if ((int16_t)(angle_to_mario - move_angle_yaw) > 0) {
        target_angle_offset = -target_angle_offset;
    }

    return target_angle_offset;
}

uint8_t mb64_pokey_should_die_in_quicksand(uint8_t part_index,
                                           uint8_t alive_parts,
                                           float quicksand_depth) {
    return part_index == (uint8_t)(alive_parts - 1)
        && quicksand_depth > s_pokey_config.quicksand_part_death_depth;
}

_Static_assert(offsetof(struct mb64_level_save_header, version) == 10,
               "MB64 disk header version offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, author) == 11,
               "MB64 disk header author offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, piktcher) == 42,
               "MB64 disk header piktcher offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, tile_count) == 8270,
               "MB64 disk header tile_count offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, object_count) == 8272,
               "MB64 disk header object_count offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, custom_theme) == 8274,
               "MB64 disk header custom_theme offset changed");
_Static_assert(offsetof(struct mb64_level_save_header, trajectories) == 8308,
               "MB64 disk header trajectories offset changed");
_Static_assert(MB64_DISK_HEADER_SIZE == 12320,
               "MB64 disk header size changed");

/* Big-endian read helpers */

static inline uint16_t u16be(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* Host stdio file buffering for libmb64; src/mb64/file.c owns FatFs loading. */

static uint8_t *read_file_into_buffer(const char *path, size_t *out_size) {
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

static uint8_t *unwrap_json_buffer_payload(uint8_t *data, size_t file_size, size_t *out_size) {
    if (file_size == 0 || data[0] != '{') return data;

    const char *text = (const char *)data;
    const char *level_data = strstr(text, "\"levelData\"");
    const char *buffer_type = strstr(text, "\"type\":\"Buffer\"");
    const char *array_start = strstr(text, "\"data\":[");
    if (!level_data || !buffer_type || !array_start) return data;

    array_start = strchr(array_start, '[');
    if (!array_start) return data;
    array_start++;

    size_t cap = file_size / 2;
    if (cap < 256) cap = 256;
    uint8_t *decoded = malloc(cap);
    if (!decoded) return data;

    size_t count = 0;
    const char *p = array_start;
    while (*p && *p != ']') {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']') break;
        if (!isdigit((unsigned char)*p)) {
            free(decoded);
            return data;
        }

        char *end = NULL;
        unsigned long value = strtoul(p, &end, 10);
        if (end == p || value > 255) {
            free(decoded);
            return data;
        }
        if (count == cap) {
            size_t next_cap = cap * 2;
            uint8_t *next = realloc(decoded, next_cap);
            if (!next) {
                free(decoded);
                return data;
            }
            decoded = next;
            cap = next_cap;
        }
        decoded[count++] = (uint8_t)value;
        p = end;

        while (*p && isspace((unsigned char)*p)) p++;
        if (*p && *p != ',' && *p != ']') {
            free(decoded);
            return data;
        }
    }

    if (*p != ']' || count == 0) {
        free(decoded);
        return data;
    }

    free(data);
    *out_size = count;
    MB64_LOG("MB64_PARSE", "stage=json_buffer_unwrap decoded_size=%zu", count);
    return decoded;
}

/* Public API */

mb64_level_t *mb64_load(const char *path) {
    if (!path) {
        MB64_LOG("MB64_PARSE", "error=null_path");
        return NULL;
    }

    MB64_LOG("MB64_PARSE", "stage=start path=%s", path);

    /* Read file */
    size_t file_size = 0;
    uint8_t *data = read_file_into_buffer(path, &file_size);
    if (!data) {
        MB64_LOG("MB64_PARSE", "error=cannot_read path=%s errno=%s", path, strerror(errno));
        return NULL;
    }
    MB64_LOG("MB64_PARSE", "stage=file_read file_size=%zu", file_size);
    data = unwrap_json_buffer_payload(data, file_size, &file_size);

    /* Validate minimum size */
    if (file_size < (size_t)(MB64_DISK_HEADER_SIZE + TILE_SIZE)) {
        MB64_LOG("MB64_PARSE", "error=file_too_small file_size=%zu min=%d",
                 file_size, (int)(MB64_DISK_HEADER_SIZE + TILE_SIZE));
        free(data);
        return NULL;
    }

    /* Allocate output struct */
    mb64_level_t *lvl = calloc(1, sizeof(mb64_level_t));
    if (!lvl) {
        MB64_LOG("MB64_PARSE", "error=oom");
        free(data);
        return NULL;
    }

    /* Parse header */
    mb64_header_t *hdr = &lvl->header;
    const struct mb64_level_save_header *disk =
        (const struct mb64_level_save_header *)data;

    memcpy(hdr->file_header, disk->file_header, 10);
    hdr->file_header[10] = '\0';
    hdr->version = disk->version;

    memcpy(hdr->author, disk->author, 31);
    hdr->author[31] = '\0';
    /* Trim embedded NULs in author string */
    for (int i = 0; i < 31; i++) {
        if (hdr->author[i] == '\0') break;
    }

    /* piktcher: 64x64 RGB5A1 thumbnail, big-endian u16 array */
    for (int y = 0; y < MB64_PIKTCHER_HEIGHT; y++) {
        for (int x = 0; x < MB64_PIKTCHER_WIDTH; x++) {
            hdr->piktcher[y * MB64_PIKTCHER_WIDTH + x] =
                u16be((const uint8_t *)&disk->piktcher[y][x]);
        }
    }

    hdr->costume       = disk->costume;
    memcpy(hdr->seq, disk->seq, 5);
    hdr->envfx         = disk->envfx;
    hdr->theme         = disk->theme;
    hdr->bg            = disk->bg;
    hdr->boundary_mat  = disk->boundary_mat;
    hdr->boundary      = disk->boundary;
    hdr->boundary_height = disk->boundary_height;
    hdr->coinstar      = disk->coinstar;
    hdr->level_size    = disk->size;
    hdr->waterlevel    = disk->waterlevel;
    hdr->secret        = disk->secret;
    hdr->game          = disk->game;
    memcpy(hdr->toolbar, disk->toolbar, 9);
    memcpy(hdr->toolbar_params, disk->toolbar_params, 9);
    hdr->tile_count   = u16be((const uint8_t *)&disk->tile_count);
    hdr->object_count = u16be((const uint8_t *)&disk->object_count);

    /* custom_theme: mats(10) topmats(10) topmats_enabled(10) fence pole bars water */
    memcpy(hdr->custom_theme.mats, disk->custom_theme.mats, 10);
    memcpy(hdr->custom_theme.topmats, disk->custom_theme.topmats, 10);
    memcpy(hdr->custom_theme.topmats_enabled,
           disk->custom_theme.topmatsEnabled, 10);
    hdr->custom_theme.fence = disk->custom_theme.fence;
    hdr->custom_theme.pole  = disk->custom_theme.pole;
    hdr->custom_theme.bars  = disk->custom_theme.bars;
    hdr->custom_theme.water = disk->custom_theme.water;

    memcpy(lvl->trajectories, disk->trajectories, sizeof(lvl->trajectories));

    MB64_LOG("MB64_PARSE",
             "magic=%.10s version=%u author=%s tile_count=%u object_count=%u "
             "theme=%u boundary=%u boundary_height=%u file_size=%zu",
             hdr->file_header, hdr->version, hdr->author,
             hdr->tile_count, hdr->object_count,
             hdr->theme, hdr->boundary, hdr->boundary_height, file_size);

    /* Validate declared sizes fit in file */
    /* File sizes use the packed on-disk sizes; struct sizes for malloc. */
    size_t tiles_file    = (size_t)hdr->tile_count   * TILE_SIZE;
    size_t objects_file  = (size_t)hdr->object_count * OBJ_SIZE;
    size_t tiles_bytes   = (size_t)hdr->tile_count   * sizeof(mb64_tile_t);
    size_t objects_bytes = (size_t)hdr->object_count * sizeof(mb64_obj_t);
    size_t expected      = (size_t)MB64_DISK_HEADER_SIZE + tiles_file + objects_file;
    if (file_size < expected) {
        MB64_LOG("MB64_PARSE", "error=truncated expected=%zu actual=%zu",
                 expected, file_size);
        free(data);
        free(lvl);
        return NULL;
    }

    /* Decode tiles (GEOM_GEN stage) */
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

        const uint8_t *tb = data + MB64_DISK_HEADER_SIZE;
        for (uint16_t i = 0; i < hdr->tile_count; i++) {
            uint32_t raw = u32be(tb + (size_t)i * TILE_SIZE);
            mb64_tile_t *t = &lvl->tiles[i];
            t->raw        = raw;
            t->x          = (uint8_t)((raw >> 26) & 0x3F);
            t->y          = (uint8_t)((raw >> 20) & 0x3F);
            t->z          = (uint8_t)((raw >> 14) & 0x3F);
            t->type       = mb64_save_upgrade_tile_type(hdr->version, (uint8_t)((raw >>  9) & 0x1F));
            t->mat        = (uint8_t)((raw >>  5) & 0x0F);
            t->rot        = (uint8_t)((raw >>  3) & 0x03);
            t->waterlogged= (uint8_t)((raw >>  2) & 0x01);

            if (t->type < 32) type_hist[t->type]++;
            if (t->waterlogged) wl_count++;
        }
        for (int t = 0; t < 32; t++) if (type_hist[t] > 0) unique_types++;
    }

    MB64_LOG("MB64_GEOM_GEN",
             "tiles_decoded=%u unique_types=%u waterlogged=%u",
             hdr->tile_count, unique_types, wl_count);

    /* Decode objects (OBJ_SPAWN stage) */
    uint32_t obj_type_hist[256] = {0};
    uint32_t obj_unique         = 0;

    if (hdr->object_count > 0) {
        lvl->objects = malloc(objects_bytes);
        if (!lvl->objects) {
            MB64_LOG("MB64_OBJ_SPAWN", "error=oom object_count=%u", hdr->object_count);
            free(lvl->tiles); free(data); free(lvl);
            return NULL;
        }

        const uint8_t *ob = data + MB64_DISK_HEADER_SIZE + tiles_file;
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

    /* Collision summary */
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
