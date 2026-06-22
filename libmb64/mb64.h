/**
 * mb64.h - Public API for libmb64.
 *
 * Parses Mario Builder 64 (.mb64) level files and returns structured C data.
 * Public enum values mirror src/mb64/data.h and packed field shapes mirror
 * src/mb64/structs.h so external consumers use the same IDs as the game.
 *
 * Usage:
 *   mb64_level_t *lvl = mb64_load("level.mb64");
 *   if (!lvl) { ... error ... }
 *   // Access all five named fields:
 *   //   lvl->header       - file metadata (author, version, counts, etc.)
 *   //   lvl->tiles        - lvl->header.tile_count decoded tiles
 *   //   lvl->objects      - lvl->header.object_count decoded objects
 *   //   lvl->trajectories - MB64_TRAJ_COUNT waypoints (20 paths x 50 points)
 *   //   lvl->header.theme - theme index (also lvl->header.custom_theme)
 *   mb64_free(lvl);
 */

#ifndef MB64_H
#define MB64_H

#include <stdint.h>
#include <stddef.h>

#include "mb64_object_types.h"
#include "mb64_save_format.h"
#include "mb64_tile_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Total trajectory waypoints stored per level (20 paths x 50 points). */
#define MB64_TRAJ_COUNT (MB64_MAX_TRAJECTORIES * MB64_TRAJECTORY_LENGTH)
#define MB64_EXCLAMATION_BOX_TYPE_COUNT 7

enum {
    MB64_GAME_VANILLA = 0,
    MB64_GAME_BTCM = 1,
};

/* Custom theme */
typedef struct {
    uint8_t mats[10];
    uint8_t topmats[10];
    uint8_t topmats_enabled[10];
    uint8_t fence, pole, bars, water;
} mb64_custom_theme_t;

/* Level header */
typedef struct {
    char     file_header[11];              /* 10-byte ASCII magic + NUL terminator */
    uint8_t  version;
    char     author[32];                   /* 31-byte ASCII + NUL terminator */
    uint16_t piktcher[MB64_PIKTCHER_SIZE]; /* 64x64 RGB5A1 thumbnail (offset 42, 8192 bytes) */
    uint8_t costume;
    uint8_t seq[5];
    uint8_t envfx;
    uint8_t theme;             /* built-in theme index */
    uint8_t bg;
    uint8_t boundary_mat;
    uint8_t boundary;
    uint8_t boundary_height;
    uint8_t coinstar;
    uint8_t level_size;        /* MB64 size field */
    uint8_t waterlevel;
    uint8_t secret;
    uint8_t game;
    uint8_t toolbar[9];
    uint8_t toolbar_params[9];
    uint16_t tile_count;
    uint16_t object_count;
    mb64_custom_theme_t custom_theme;
} mb64_header_t;

/* Tile decoded from the packed u32 layout in src/mb64/structs.h. */
typedef struct {
    uint32_t raw;         /* original big-endian word */
    uint8_t  x, y, z;    /* grid coordinates (0-63) */
    uint8_t  type;        /* tile type (0-31) */
    uint8_t  mat;         /* material (0-15) */
    uint8_t  rot;         /* rotation (0-3) */
    uint8_t  waterlogged; /* 1 if waterlogged, 0 otherwise */
} mb64_tile_t;

/* Object */
typedef struct {
    uint8_t bparam;
    uint8_t x, y, z;
    uint8_t type;
    uint8_t rot;
    uint8_t imbue;
    /* pad byte skipped */
} mb64_obj_t;

typedef struct mb64_comptraj mb64_traj_t;

typedef enum {
    MB64_ENVFX_NONE = 0,
    MB64_ENVFX_ASHES,
    MB64_ENVFX_SNOW,
    MB64_ENVFX_RAIN,
    MB64_ENVFX_SANDSTORM,
    MB64_ENVFX_COUNT,
} mb64_envfx_t;

/* Parsed level - five first-class fields per scenario 7. */
typedef struct {
    mb64_header_t  header;                         /* metadata; theme at header.theme */
    mb64_tile_t   *tiles;                          /* header.tile_count entries, heap-alloc */
    mb64_obj_t    *objects;                        /* header.object_count entries, heap-alloc */
    mb64_traj_t    trajectories[MB64_TRAJ_COUNT];  /* flat 20 path x 50 waypoint table */
} mb64_level_t;

typedef enum {
    MB64_MESH_FACE_TOP = 0,
    MB64_MESH_FACE_BOTTOM = 1,
    MB64_MESH_FACE_POS_X = 2,
    MB64_MESH_FACE_NEG_X = 3,
    MB64_MESH_FACE_POS_Z = 4,
    MB64_MESH_FACE_NEG_Z = 5,
} mb64_mesh_face_dir_t;

typedef struct {
    int16_t v[4][3];      /* MB64 coordinates in sixteenths of one tile */
    int16_t tc[4][2];     /* Optional texture coordinates in native N64 units */
    uint8_t material;
    uint8_t resolved_material;
    uint8_t tile_type;
    uint8_t direction;
    uint8_t is_water;
    uint8_t vertex_count;
    uint8_t use_tc;
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t tile_z;
} mb64_mesh_face_t;

typedef struct {
    mb64_mesh_face_t *faces;
    uint32_t face_count;
    uint32_t solid_tile_count;
    uint32_t water_tile_count;
} mb64_mesh_t;

typedef struct {
    uint32_t face_count;
    uint32_t solid_tile_count;
    uint32_t water_tile_count;
    uint32_t duplicate_face_count;
} mb64_mesh_info_t;

typedef struct {
    int (*begin)(const mb64_mesh_info_t *info, void *user);
    int (*face)(const mb64_mesh_face_t *face, void *user);
    int (*end)(const mb64_mesh_info_t *info, void *user);
} mb64_mesh_visitor_t;

#define MB64_BOUNDARY_FLAG_INNER_FLOOR (1 << 0)
#define MB64_BOUNDARY_FLAG_OUTER_FLOOR (1 << 1)
#define MB64_BOUNDARY_FLAG_INNER_WALLS (1 << 2)
#define MB64_BOUNDARY_FLAG_OUTER_WALLS (1 << 3)
#define MB64_BOUNDARY_FLAG_CEILING     (1 << 4)
#define MB64_DEATH_PLANE_FACE_COUNT 4

typedef struct {
    int16_t v[4][3]; /* MB64 coordinates in sixteenths of one tile */
} mb64_boundary_face_t;

typedef struct {
    uint8_t animated;
    uint8_t tile_size_cmd;
    uint8_t interval;
    uint16_t step_s;
    uint16_t step_t;
} mb64_material_texture_animation_t;

typedef enum {
    MB64_RENDER_BINDING_MATERIAL = 0,
    MB64_RENDER_BINDING_FENCE,
    MB64_RENDER_BINDING_BARS,
    MB64_RENDER_BINDING_BARS_TOP,
    MB64_RENDER_BINDING_TTC_GRATE_TOP,
    MB64_RENDER_BINDING_WATER,
} mb64_render_binding_kind_t;

typedef enum {
    MB64_RENDER_CLASS_OPAQUE = 0,
    MB64_RENDER_CLASS_DECAL,
    MB64_RENDER_CLASS_CUTOUT,
    MB64_RENDER_CLASS_CUTOUT_NOCULL,
    MB64_RENDER_CLASS_TRANSPARENT,
    MB64_RENDER_CLASS_SCREEN,
} mb64_render_class_t;

typedef struct {
    uint8_t kind;
    uint8_t token;
    uint8_t material;
    uint8_t render_class;
    uint8_t cull_backfaces;
    mb64_material_texture_animation_t animation;
} mb64_render_binding_t;

typedef struct {
    float thin_height;
    float fat_height;
    float stack_dist_epsilon;
    float wall_hitbox_radius;
    float gravity;
    float buoyancy;
    float water_probe_y;
    float water_surface_offset;
    float mario_weight_vel;
    float ground_pound_weight_vel;
    float water_drag;
    float water_float_base;
    float water_float_min;
    float water_float_max;
    int death_drop_offset;
    int steep_slope_degrees;
} mb64_woodplat_config_t;

typedef struct {
    float forward_vel;
    uint8_t activates_immediately;
    uint8_t returns_to_start;
    uint8_t does_not_disappear;
} mb64_looping_platform_config_t;

typedef struct {
    float wake_min_distance;
    float wake_max_distance;
    float shake_forward_speed;
    float launch_forward_speed;
    float floor_probe_offset_y;
    float rotate_min_distance;
    int wake_angle_threshold;
    int shake_start_frame;
    int launch_frame;
    int timeout_frame;
    int explosion_reset_frame;
    int rotate_step;
} mb64_bullet_bill_config_t;

typedef struct {
    float break_coin_radius;
    float shake_amplitude;
    float shake_center_offset;
    int init_timer;
    int shake_timer_limit;
    int clank_cooldown_timer;
} mb64_reinforced_box_config_t;

typedef struct {
    float first_flame_distance;
    float flame_spacing;
    float flame_y_offset;
    float flame_scale;
    int rotation_speed;
} mb64_fire_spinner_config_t;

enum {
    MB64_GOOMBA_SIZE_REGULAR = 0,
    MB64_GOOMBA_SIZE_HUGE = 1,
    MB64_GOOMBA_SIZE_TINY = 2,
};

enum {
    MB64_KOOPA_BP_UNSHELLED = 0,
    MB64_KOOPA_BP_NORMAL = 1,
    MB64_KOOPA_BP_KOOPA_THE_QUICK_BASE = 2,
    MB64_KOOPA_BP_TINY = 4,
};

typedef struct {
    int scale_angle_start;
    int scale_angle_step;
    int explode_frame;
    int respawn_frames;
    float hit_launch_vel_y;
    float hit_gravity;
    float squash_scale_factor;
    float stretch_scale_factor;
    float stretch_scale_offset;
    float graph_y_offset_factor;
    float model_scale;
} mb64_exclamation_box_config_t;

typedef enum {
    MB64_EXCLAMATION_BOX_ACT_INIT = 0,
    MB64_EXCLAMATION_BOX_ACT_OUTLINE = 1,
    MB64_EXCLAMATION_BOX_ACT_ACTIVE = 2,
    MB64_EXCLAMATION_BOX_ACT_SCALING = 3,
    MB64_EXCLAMATION_BOX_ACT_EXPLODE = 4,
    MB64_EXCLAMATION_BOX_ACT_WAIT_FOR_RESPAWN = 5,
} mb64_exclamation_box_action_t;

typedef enum {
    MB64_EXCLAMATION_BOX_ANIM_RED = 0,
    MB64_EXCLAMATION_BOX_ANIM_GREEN = 1,
    MB64_EXCLAMATION_BOX_ANIM_BLUE = 2,
    MB64_EXCLAMATION_BOX_ANIM_YELLOW = 3,
    MB64_EXCLAMATION_BOX_ANIM_GREEN_COIN = 4,
} mb64_exclamation_box_anim_t;

typedef enum {
    MB64_EXCLAMATION_BOX_MODEL_NONE = 0,
    MB64_EXCLAMATION_BOX_MODEL_WING_CAP,
    MB64_EXCLAMATION_BOX_MODEL_METAL_CAP,
    MB64_EXCLAMATION_BOX_MODEL_VANISH_CAP,
    MB64_EXCLAMATION_BOX_MODEL_KOOPA_SHELL,
    MB64_EXCLAMATION_BOX_MODEL_YELLOW_COIN,
    MB64_EXCLAMATION_BOX_MODEL_GREEN_COIN,
    MB64_EXCLAMATION_BOX_MODEL_ROCKET_BOOTS,
    MB64_EXCLAMATION_BOX_MODEL_VANETAL_CAP,
} mb64_exclamation_box_model_t;

typedef enum {
    MB64_EXCLAMATION_BOX_BEHAVIOR_NONE = 0,
    MB64_EXCLAMATION_BOX_BEHAVIOR_WING_CAP,
    MB64_EXCLAMATION_BOX_BEHAVIOR_METAL_CAP,
    MB64_EXCLAMATION_BOX_BEHAVIOR_VANISH_CAP,
    MB64_EXCLAMATION_BOX_BEHAVIOR_KOOPA_SHELL,
    MB64_EXCLAMATION_BOX_BEHAVIOR_SINGLE_COIN,
    MB64_EXCLAMATION_BOX_BEHAVIOR_THREE_COINS,
    MB64_EXCLAMATION_BOX_BEHAVIOR_TEN_COINS,
    MB64_EXCLAMATION_BOX_BEHAVIOR_GREEN_COIN,
} mb64_exclamation_box_behavior_t;

typedef struct {
    uint8_t beh_params;
    uint8_t model;
    uint8_t behavior;
    uint8_t anim_state;
    uint8_t do_respawn;
    uint8_t num_coins;
} mb64_exclamation_box_content_t;

typedef struct {
    int scale_frames;
    int timer_frames;
    int double_time_timer_frames;
    int hidden_box_blink_frames;
    float switch_scale;
    float pressed_scale;
    float press_radius;
} mb64_floor_switch_config_t;

typedef struct {
    int graph_angle_step;
    int min_health;
    float velocity_decay;
    float bounce_graph_vel_y;
    float bounce_mario_vel_y;
} mb64_noteblock_config_t;

typedef struct {
    float pressed_scale_factor;
    float scale_step_factor;
    float collision_min_scale_y;
} mb64_onoff_config_t;

enum {
    MB64_CONVEYOR_SHAPE_HALF = 0,
    MB64_CONVEYOR_SHAPE_FLAT = 1,
    MB64_CONVEYOR_SHAPE_SLOPE = 2,
    MB64_CONVEYOR_SHAPE_DOWNSLOPE = 3,
};

enum {
    MB64_CONVEYOR_STATE_ALWAYS = 0,
    MB64_CONVEYOR_STATE_RED = 1,
    MB64_CONVEYOR_STATE_BLUE = 2,
};

typedef struct {
    int spin_accel;
    float shrink_factor;
    float delete_scale;
} mb64_badge_config_t;

typedef struct {
    float damage_or_coin_value;
    float hitbox_radius;
    float hitbox_height;
} mb64_green_coin_config_t;

typedef struct {
    uint8_t crowbar_power_bit;
    uint8_t mask_power_bit;
    uint8_t sparkle_timer_mask;
    int face_pitch;
    int yaw_step;
    int respawn_frames;
    float sparkle_distance;
    float drawing_distance;
    float hitbox_radius;
    float hitbox_height;
    float hitbox_down_offset;
    float mask_graph_y_offset;
} mb64_powerup_config_t;

typedef enum {
    MB64_PHANTASM_ACT_INIT = 0,
    MB64_PHANTASM_ACT_IDLE = 1,
    MB64_PHANTASM_ACT_WANDER = 2,
    MB64_PHANTASM_ACT_ATTACKED = 3,
    MB64_PHANTASM_ACT_ALERT = 4,
    MB64_PHANTASM_ACT_KICK = 5,
    MB64_PHANTASM_ACT_FIREBALLS = 6,
    MB64_PHANTASM_ACT_DISPENSED = 7,
} mb64_phantasm_action_t;

typedef struct {
    int health_default;
    int health_boss_dispensed;
    int loot_coins_default;
    int loot_coins_boss_dispensed;
    int idle_timer;
    int wander_timer;
    int alert_timer;
    int alert_track_timer;
    int fireball_attack_timer;
    int fireball_end_timer;
    int fireball_interval;
    int attacked_death_check_timer;
    int attacked_recover_timer;
    int facing_angle_range;
    float facing_attack_distance;
    float close_attack_distance;
    float hitbox_radius;
    float hitbox_height;
    float hurtbox_radius;
    float hurtbox_height;
    float invincible_hurtbox_radius;
    float invincible_hurtbox_height;
    float damage_or_coin_value;
    float gravity_default;
    float gravity_alert_down;
    float gravity_alert_up;
    float mario_height_target_offset;
    float wander_speed;
    float alert_back_speed;
    float alert_back_speed_min;
    float kick_base_speed;
    float kick_distance_divisor;
    float kick_speed_max;
    float kick_vel_y;
    float kick_deceleration;
    float kick_recover_speed;
    float fireball_back_speed;
    float fireball_forward_vel;
    float fireball_vel_y;
    float fireball_y_offset;
    float ledge_drop_guard_height;
    float death_barrier_drop_height;
} mb64_phantasm_config_t;

typedef enum {
    MB64_SHOWRUNNER_ACT_INIT = 0,
    MB64_SHOWRUNNER_ACT_WAIT = 1,
    MB64_SHOWRUNNER_ACT_BACK_AWAY = 2,
    MB64_SHOWRUNNER_ACT_SPIKE_ATTACK = 3,
    MB64_SHOWRUNNER_ACT_TENNIS = 10,
    MB64_SHOWRUNNER_ACT_STUNNED = 11,
    MB64_SHOWRUNNER_ACT_DAMAGED = 12,
    MB64_SHOWRUNNER_ACT_BATTLE_END = 13,
    MB64_SHOWRUNNER_ACT_DROP_ITEMS = 14,
    MB64_SHOWRUNNER_ACT_DIE = 15,
    MB64_SHOWRUNNER_ACT_BALLERINA = 16,
    MB64_SHOWRUNNER_ACT_PHANTASM_RELEASE = 17,
} mb64_showrunner_action_t;

typedef enum {
    MB64_SHOWRUNNER_SPIKE_ACT_INIT = 0,
    MB64_SHOWRUNNER_SPIKE_ACT_RUMBLE = 1,
    MB64_SHOWRUNNER_SPIKE_ACT_RISE = 2,
    MB64_SHOWRUNNER_SPIKE_ACT_WAIT = 3,
} mb64_showrunner_spike_action_t;

typedef enum {
    MB64_SHOWRUNNER_TENNIS_ACT_INIT = 0,
    MB64_SHOWRUNNER_TENNIS_ACT_ACTIVE = 1,
} mb64_showrunner_tennis_action_t;

typedef enum {
    MB64_SHOWRUNNER_FLAME_ACT_EXIST = 0,
    MB64_SHOWRUNNER_FLAME_ACT_DELETE = 1,
} mb64_showrunner_flame_action_t;

typedef enum {
    MB64_SHOWRUNNER_ANIM_IDLE = 0,
    MB64_SHOWRUNNER_ANIM_SPIKE = 1,
    MB64_SHOWRUNNER_ANIM_BACK_AWAY = 2,
    MB64_SHOWRUNNER_ANIM_WIND_UP = 3,
    MB64_SHOWRUNNER_ANIM_BLOCK = 4,
    MB64_SHOWRUNNER_ANIM_SHOCKED = 5,
    MB64_SHOWRUNNER_ANIM_HURT = 6,
    MB64_SHOWRUNNER_ANIM_DEAD = 7,
    MB64_SHOWRUNNER_ANIM_CANE_SLAP = 8,
    MB64_SHOWRUNNER_ANIM_BALLERINA = 9,
    MB64_SHOWRUNNER_ANIM_FLOAT = 10,
    MB64_SHOWRUNNER_ANIM_FACEPALM = 11,
    MB64_SHOWRUNNER_ANIM_FACEPALM2 = 12,
} mb64_showrunner_anim_t;

typedef struct {
    int health;
    int spike_attacks_initial;
    int spike_attacks_after_phantasm_release;
    int spike_spawn_start_timer;
    int spike_spawn_end_timer;
    int spike_spawn_interval;
    int spike_attack_end_timer;
    int back_away_recover_timer;
    int tennis_spawn_timer;
    int tennis_turns_by_health[4];
    int stunned_recover_timer;
    int damaged_recover_timer;
    int battle_end_timer;
    int shrink_sound_timer;
    int ballerina_end_timer;
    int ballerina_spin_accel;
    int ballerina_spin_max;
    int ballerina_projectile_start_timer;
    int ballerina_projectile_interval;
    int ballerina_projectile_min_spin;
    float ballerina_projectile_y_range;
    float trigger_distance;
    float scale;
    float back_away_forward_vel;
    float back_away_friction;
    float back_away_end_speed;
    float spike_initial_offset;
    float spike_step_offset;
    float spike_close_distance;
    float spike_floor_probe_offset_y;
    float spike_home_offset_y;
    float tennis_forward_vel;
    float tennis_projectile_y_offset;
    float spike_rumble_offset_y;
    int spike_rumble_end_timer;
    float spike_rise_step;
    float spike_rise_height;
    int tennis_turn_rate;
    int tennis_initial_damage;
    float tennis_initial_forward_vel;
    float tennis_hitbox_radius;
    float tennis_hitbox_height;
    float tennis_hurtbox_radius;
    float tennis_hurtbox_height;
    float tennis_parent_hit_distance;
    float tennis_return_speed_bonus;
    int tennis_trail_initial_opacity;
    int tennis_trail_fade_step;
    int tennis_trail_delete_opacity;
    float hitbox_radius;
    float hitbox_height;
    float damage_or_coin_value;
    float ballerina_damage_or_coin_value;
    float shrink_step;
    float delete_scale;
    int loot_coins;
    float death_barrier_drop_height;
    int phantasm_release_flame_timer;
    int phantasm_release_flame_count;
    int phantasm_release_flame_angle_step;
    float phantasm_release_flame_y_offset;
    float phantasm_release_flame_forward_vel;
    float thwomp_flame_medium_speed;
    float thwomp_flame_fast_speed;
    int thwomp_flame_slow_lifetime;
    int thwomp_flame_medium_lifetime;
    int thwomp_flame_fast_lifetime;
    int thwomp_flame_shrink_timer;
    float thwomp_flame_scale;
    float cosmic_projectile_forward_vel;
    int cosmic_projectile_delete_timer;
    int cosmic_projectile_roll_step;
} mb64_showrunner_config_t;

typedef enum {
    MB64_MOTOS_ACT_WAIT = 0,
    MB64_MOTOS_ACT_PLAYER_SEARCH = 1,
    MB64_MOTOS_ACT_PLAYER_CARRY = 2,
    MB64_MOTOS_ACT_PLAYER_PITCH = 3,
    MB64_MOTOS_ACT_CARRY_RUN = 4,
    MB64_MOTOS_ACT_THROWN = 5,
    MB64_MOTOS_ACT_RECOVER = 6,
    MB64_MOTOS_ACT_DEATH = 7,
} mb64_motos_action_t;

typedef enum {
    MB64_MOTOS_ANIM_BASE = 0,
    MB64_MOTOS_ANIM_CARRY = 1,
    MB64_MOTOS_ANIM_CARRY_RUN = 2,
    MB64_MOTOS_ANIM_CARRY_START = 3,
    MB64_MOTOS_ANIM_DOWN_RECOVER = 4,
    MB64_MOTOS_ANIM_DOWN_STOP = 5,
    MB64_MOTOS_ANIM_PITCH = 6,
    MB64_MOTOS_ANIM_SAFE_DOWN = 7,
    MB64_MOTOS_ANIM_WAIT = 8,
    MB64_MOTOS_ANIM_WALK = 9,
    MB64_MOTOS_ANIM_END = 10,
} mb64_motos_anim_t;

typedef struct {
    float scale;
    float hand_relative_x;
    float hand_relative_y;
    float anchor_throw_forward_vel;
    float anchor_throw_vel_y;
    int anchor_throw_status_arg;
    float wait_search_distance;
    float search_drop_distance;
    float search_forward_vel;
    int search_turn_speed;
    int throw_timer;
    int escape_actions;
    float carry_run_forward_vel;
    int pitch_throw_frame;
    int recover_wait_timer;
    float placed_forward_vel;
    float placed_vel_y;
    float blue_coin_forward_vel;
    float blue_coin_vel_y;
    float blue_coin_y_offset;
    float death_barrier_drop_height;
    int quicksand_depth_to_die;
} mb64_motos_config_t;

typedef struct {
    uint8_t behavior_param_2;
    int animation_index;
    float scale;
    float wall_hitbox_radius;
    float gravity;
    float bounciness;
    float drag_strength;
    float friction;
    float buoyancy;
    float draw_distance;
} mb64_chicken_config_t;

typedef struct {
    uint8_t damage_or_coin_value;
    uint8_t health;
    uint8_t num_loot_coins;
    int16_t down_offset;
    int16_t radius;
    int16_t height;
    int16_t hurtbox_radius;
    int16_t hurtbox_height;
} mb64_object_hitbox_t;

enum {
    MB64_CRABLET_ACT_SPAWN = 0,
    MB64_CRABLET_ACT_PATROL = 1,
    MB64_CRABLET_ACT_TURN = 2,
    MB64_CRABLET_ACT_KNOCKBACK_START = 3,
    MB64_CRABLET_ACT_KNOCKBACK_AIR = 4,
    MB64_CRABLET_ACT_RECOVER = 5,
};

enum {
    MB64_CRABLET_ATTACK_READY = 0,
    MB64_CRABLET_ATTACK_ACTIVE = 1,
};

typedef struct {
    int animation_index;
    float scale;
    float wall_hitbox_radius;
    float gravity;
    float bounciness;
    float drag_strength;
    float friction;
    float buoyancy;
    float walk_forward_vel;
    float attack_forward_vel;
    float attack_jump_vel_y;
    float knockback_forward_vel;
    float knockback_vel_y;
    float recover_forward_vel;
    float attack_distance;
    float attack_angle_threshold;
    float head_grab_distance;
    float head_grab_y_offset;
    float carried_forward_offset;
    float carried_y_offset;
    int attack_timer_limit;
    int recover_timer_limit;
    int hurt_quicksand_depth_step;
    int death_drop_height;
} mb64_crablet_config_t;

typedef struct {
    uint8_t behavior_param_2;
    int anim_idle;
    int anim_throw;
    int anim_jump;
    float scale;
    float wall_hitbox_radius;
    float gravity;
    float bounciness;
    float drag_strength;
    float friction;
    float buoyancy;
    float mario_min_y_offset;
    float activation_distance;
    float projectile_y_offset;
    float fireball_vel_y;
    float fireball_forward_vel;
    float fireball_move_forward_vel;
    float jump_vel_y;
    int throw_start_frame;
    int hold_frame_limit;
    int repeat_frame_limit;
    int landing_rearm_max_timer;
    int fireball_timeout_frame;
} mb64_fire_bro_config_t;

typedef struct {
    int anim_idle;
    int anim_throw;
    int anim_jump;
    float scale;
    float wall_hitbox_radius;
    float gravity;
    float bounciness;
    float drag_strength;
    float friction;
    float buoyancy;
    float mario_min_y_offset;
    float activation_distance;
    float projectile_y_offset;
    float projectile_quicksand_y_scale;
    float hammer_vel_y_min;
    float hammer_vel_y_max;
    float hammer_forward_vel_min;
    float hammer_forward_vel_max;
    float jump_vel_y;
    float hammer_initial_y_offset;
    int throw_start_frame;
    int hold_frame_limit;
    int repeat_frame_limit;
    int landing_rearm_max_timer;
    int quicksand_depth_step;
    int hammer_ready_frame;
    int hammer_pitch_start;
    int hammer_pitch_step;
    int hammer_timeout_frame;
} mb64_hammer_bro_config_t;

typedef struct {
    int health;
    int animation_index;
    float scale;
    float graph_y_offset;
    float draw_distance;
    float wall_hitbox_radius;
    float gravity;
    float bounciness;
    float drag_strength;
    float friction;
    float buoyancy;
} mb64_rex_config_t;

typedef struct {
    int animation_index;
    int bobomb_buddy_role;
    int forced_anim_state;
    float graph_y_offset;
    float hitbox_radius;
    float hitbox_height;
    float draw_distance;
} mb64_npc_config_t;

typedef struct {
    float gravity;
    float launch_accel;
    float mario_activation_distance;
    float flame_y_offset;
    int flame_warmup_frame;
    int launch_frame;
    int landing_roll_angle;
    int roll_step;
    int splash_flame_count;
} mb64_podoboo_config_t;

typedef struct {
    uint8_t segment_count;
    uint8_t head_part_index;
    float scale;
    float body_step;
    float head_start_y;
    float sway_radius;
    float expand_step;
    float standard_action_scale;
    float graph_y_offset_scale;
    float gravity;
    float unload_distance_margin;
    float forward_speed;
    float far_mario_distance;
    float random_wander_distance;
    float shy_min_distance;
    float shy_angle_scale;
    float quicksand_part_death_depth;
    int blink_min_frames;
    int blink_max_frames;
    int blink_random_frames;
    int regrow_frame;
    int random_turn_step;
    int random_timer_min;
    int random_timer_range;
    int turn_step;
    int steep_slope_degrees;
    int death_delay_base;
    int death_delay_shift;
    int quicksand_depth_to_die;
    int star_drop_height;
} mb64_pokey_config_t;

typedef enum {
    MB64_BULLET_BILL_ACT_IDLE_RESET = 0,
    MB64_BULLET_BILL_ACT_WAIT_FOR_PLAYER = 1,
    MB64_BULLET_BILL_ACT_FIRE = 2,
    MB64_BULLET_BILL_ACT_RESET_AFTER_TIMEOUT = 3,
    MB64_BULLET_BILL_ACT_EXPLODE = 4,
} mb64_bullet_bill_action_t;

typedef enum {
    MB64_PODOBOO_ACT_INIT = 0,
    MB64_PODOBOO_ACT_FALL_INTO_LAVA = 1,
    MB64_PODOBOO_ACT_IDLE_IN_LAVA = 2,
    MB64_PODOBOO_ACT_JUMP = 3,
} mb64_podoboo_action_t;

typedef enum {
    MB64_POKEY_ACT_UNINITIALIZED = 0,
    MB64_POKEY_ACT_WANDER = 1,
    MB64_POKEY_ACT_UNLOAD_PARTS = 2,
} mb64_pokey_action_t;

#define MB64_RENDER_MATERIAL_FENCE    240
#define MB64_RENDER_MATERIAL_BARS     241
#define MB64_RENDER_MATERIAL_BARS_TOP 242
#define MB64_RENDER_MATERIAL_TTC_GRATE_TOP 243

typedef enum {
    MB64_MAT_GRASS = 0,
    MB64_MAT_GRASS_OLD = 1,
    MB64_MAT_CARTOON_GRASS = 2,
    MB64_MAT_DARK_GRASS = 3,
    MB64_MAT_HMC_GRASS = 4,
    MB64_MAT_ORANGE_GRASS = 5,
    MB64_MAT_RED_GRASS = 6,
    MB64_MAT_PURPLE_GRASS = 7,
    MB64_MAT_SAND = 8,
    MB64_MAT_JRB_SAND = 9,
    MB64_MAT_SNOW = 10,
    MB64_MAT_SNOW_OLD = 11,
    MB64_MAT_DIRT = 12,
    MB64_MAT_SANDDIRT = 13,
    MB64_MAT_LIGHTDIRT = 14,
    MB64_MAT_HMC_DIRT = 15,
    MB64_MAT_ROCKY_DIRT = 16,
    MB64_MAT_DIRT_OLD = 17,
    MB64_MAT_WAVY_DIRT = 18,
    MB64_MAT_WAVY_DIRT_BLUE = 19,
    MB64_MAT_SNOWDIRT = 20,
    MB64_MAT_PURPLE_DIRT = 21,
    MB64_MAT_HMC_LAKEGRASS = 22,
    MB64_MAT_STONE = 23,
    MB64_MAT_HMC_STONE = 24,
    MB64_MAT_HMC_MAZEFLOOR = 25,
    MB64_MAT_CCM_ROCK = 26,
    MB64_MAT_TTM_FLOOR = 27,
    MB64_MAT_TTM_ROCK = 28,
    MB64_MAT_COBBLESTONE = 29,
    MB64_MAT_JRB_WALL = 30,
    MB64_MAT_GABBRO = 31,
    MB64_MAT_RHR_STONE = 32,
    MB64_MAT_LAVA_ROCKS = 33,
    MB64_MAT_VOLCANO_WALL = 34,
    MB64_MAT_RHR_BASALT = 35,
    MB64_MAT_OBSIDIAN = 36,
    MB64_MAT_CASTLE_STONE = 37,
    MB64_MAT_JRB_UNDERWATER = 38,
    MB64_MAT_SNOW_ROCK = 39,
    MB64_MAT_ICY_ROCK = 40,
    MB64_MAT_DESERT_STONE = 41,
    MB64_MAT_RHR_OBSIDIAN = 42,
    MB64_MAT_JRB_STONE = 43,
    MB64_MAT_BRICKS = 44,
    MB64_MAT_DESERT_BRICKS = 45,
    MB64_MAT_RHR_BRICK = 46,
    MB64_MAT_HMC_BRICK = 47,
    MB64_MAT_LIGHTBROWN_BRICK = 48,
    MB64_MAT_WDW_BRICK = 49,
    MB64_MAT_TTM_BRICK = 50,
    MB64_MAT_C_BRICK = 51,
    MB64_MAT_BBH_BRICKS = 52,
    MB64_MAT_ROOF_BRICKS = 53,
    MB64_MAT_C_OUTSIDEBRICK = 54,
    MB64_MAT_SNOW_BRICKS = 55,
    MB64_MAT_JRB_BRICKS = 56,
    MB64_MAT_SNOW_TILE_SIDE = 57,
    MB64_MAT_TILESBRICKS = 58,
    MB64_MAT_TILES = 59,
    MB64_MAT_C_TILES = 60,
    MB64_MAT_DESERT_TILES = 61,
    MB64_MAT_VP_BLUETILES = 62,
    MB64_MAT_SNOW_TILES = 63,
    MB64_MAT_JRB_TILETOP = 64,
    MB64_MAT_JRB_TILESIDE = 65,
    MB64_MAT_HMC_TILES = 66,
    MB64_MAT_GRANITE_TILES = 67,
    MB64_MAT_RHR_TILES = 68,
    MB64_MAT_VP_TILES = 69,
    MB64_MAT_DIAMOND_PATTERN = 70,
    MB64_MAT_C_STONETOP = 71,
    MB64_MAT_SNOW_BRICK_TILES = 72,
    MB64_MAT_DESERT_BLOCK = 73,
    MB64_MAT_VP_BLOCK = 74,
    MB64_MAT_BBH_STONE = 75,
    MB64_MAT_BBH_STONE_PATTERN = 76,
    MB64_MAT_PATTERNED_BLOCK = 77,
    MB64_MAT_HMC_SLAB = 78,
    MB64_MAT_RHR_BLOCK = 79,
    MB64_MAT_GRANITE_BLOCK = 80,
    MB64_MAT_C_STONESIDE = 81,
    MB64_MAT_C_PILLAR = 82,
    MB64_MAT_BBH_PILLAR = 83,
    MB64_MAT_RHR_PILLAR = 84,
    MB64_MAT_WOOD = 85,
    MB64_MAT_BBH_WOOD_FLOOR = 86,
    MB64_MAT_BBH_WOOD_WALL = 87,
    MB64_MAT_C_WOOD = 88,
    MB64_MAT_JRB_WOOD = 89,
    MB64_MAT_JRB_SHIPSIDE = 90,
    MB64_MAT_JRB_SHIPTOP = 91,
    MB64_MAT_BBH_HAUNTED_PLANKS = 92,
    MB64_MAT_BBH_ROOF = 93,
    MB64_MAT_SOLID_WOOD = 94,
    MB64_MAT_RHR_WOOD = 95,
    MB64_MAT_BBH_METAL = 96,
    MB64_MAT_JRB_METALSIDE = 97,
    MB64_MAT_JRB_METAL = 98,
    MB64_MAT_C_BASEMENTWALL = 99,
    MB64_MAT_DESERT_TILES2 = 100,
    MB64_MAT_VP_RUSTYBLOCK = 101,
    MB64_MAT_C_CARPET = 102,
    MB64_MAT_C_WALL = 103,
    MB64_MAT_ROOF = 104,
    MB64_MAT_C_ROOF = 105,
    MB64_MAT_SNOW_ROOF = 106,
    MB64_MAT_BBH_WINDOW = 107,
    MB64_MAT_HMC_LIGHT = 108,
    MB64_MAT_VP_CAUTION = 109,
    MB64_MAT_RR_BLOCKS = 110,
    MB64_MAT_STUDDED_TILE = 111,
    MB64_MAT_TTC_BLOCK = 112,
    MB64_MAT_TTC_SIDE = 113,
    MB64_MAT_TTC_WALL = 114,
    MB64_MAT_FLOWERS = 115,
    MB64_MAT_LAVA = 116,
    MB64_MAT_VANILLA_LAVA = 117,
    MB64_MAT_LAVA_OLD = 117,
    MB64_MAT_SERVER_ACID = 118,
    MB64_MAT_BURNING_ICE = 119,
    MB64_MAT_QUICKSAND = 120,
    MB64_MAT_DESERT_SLOWSAND = 121,
    MB64_MAT_VOID = 122,
    MB64_MAT_VP_VOID = 122,
    MB64_MAT_RHR_MESH = 123,
    MB64_MAT_VP_MESH = 124,
    MB64_MAT_HMC_MESH = 125,
    MB64_MAT_BBH_MESH = 126,
    MB64_MAT_PINK_MESH = 127,
    MB64_MAT_TTC_MESH = 128,
    MB64_MAT_ICE = 129,
    MB64_MAT_CRYSTAL = 130,
    MB64_MAT_VP_SCREEN = 131,
    MB64_MAT_RETRO_GROUND = 132,
    MB64_MAT_RETRO_BRICKS = 133,
    MB64_MAT_RETRO_TREETOP = 134,
    MB64_MAT_RETRO_TREEPLAT = 135,
    MB64_MAT_RETRO_BLOCK = 136,
    MB64_MAT_RETRO_BLUEGROUND = 137,
    MB64_MAT_RETRO_BLUEBRICKS = 138,
    MB64_MAT_RETRO_BLUEBLOCK = 139,
    MB64_MAT_RETRO_WHITEBRICK = 140,
    MB64_MAT_RETRO_LAVA = 141,
    MB64_MAT_RETRO_UNDERWATERGROUND = 142,
    MB64_MAT_MC_DIRT = 143,
    MB64_MAT_MC_GRASS = 144,
    MB64_MAT_MC_COBBLESTONE = 145,
    MB64_MAT_MC_STONE = 146,
    MB64_MAT_MC_OAK_LOG_TOP = 147,
    MB64_MAT_MC_OAK_LOG_SIDE = 148,
    MB64_MAT_MC_OAK_LEAVES = 149,
    MB64_MAT_MC_WOOD_PLANKS = 150,
    MB64_MAT_MC_SAND = 151,
    MB64_MAT_MC_BRICKS = 152,
    MB64_MAT_MC_LAVA = 153,
    MB64_MAT_MC_FLOWING_LAVA = 154,
    MB64_MAT_MC_GLASS = 155,
} mb64_material_id_t;

typedef enum {
    MB64_FENCE_NORMAL = 0,
    MB64_FENCE_WOOD2,
    MB64_FENCE_DESERT,
    MB64_FENCE_BARBED,
    MB64_FENCE_RHR,
    MB64_FENCE_HMC,
    MB64_FENCE_CASTLE,
    MB64_FENCE_VIRTUAPLEX,
    MB64_FENCE_BBH,
    MB64_FENCE_JRB,
    MB64_FENCE_SNOW2,
    MB64_FENCE_SNOW,
    MB64_FENCE_RETRO,
    MB64_FENCE_MC,
} mb64_fence_id_t;

typedef enum {
    MB64_BAR_GENERIC = 0,
    MB64_BAR_RHR,
    MB64_BAR_VP,
    MB64_BAR_HMC,
    MB64_BAR_BBH,
    MB64_BAR_LLL,
    MB64_BAR_TTC,
    MB64_BAR_DESERT,
    MB64_BAR_BOB,
    MB64_BAR_RETRO,
    MB64_BAR_MC,
} mb64_bar_id_t;

typedef enum {
    MB64_WATER_DEFAULT = 0,
    MB64_WATER_GREEN,
    MB64_WATER_RETRO,
    MB64_WATER_MC,
} mb64_water_id_t;

typedef struct {
    uint8_t fence;
    uint8_t pole;
    uint8_t bars;
    uint8_t water;
} mb64_theme_special_t;

/**
 * mb64_load() - Parse an .mb64 file.
 *
 * Reads the file at @path, validates the binary structure, and returns a
 * heap-allocated mb64_level_t with fully decoded header, tiles, and objects.
 *
 * Logs progress via MB64_LOG at each processing stage:
 *   [MB64_PARSE]     header fields
 *   [MB64_GEOM_GEN]  tile decode summary
 *   [MB64_OBJ_SPAWN] object decode summary
 *   [MB64_COLLISION] solid-tile collision stats
 *
 * Returns: pointer on success, NULL on any error (logs reason before returning).
 */
mb64_level_t *mb64_load(const char *path);

/**
 * mb64_free() - Release all memory owned by a parsed level.
 *
 * Safe to call with NULL.
 */
void mb64_free(mb64_level_t *level);

/**
 * mb64_music_sequence_from_index() - Convert MB64 music menu index to sequence.
 *
 * MB64 saves level, race, and boss music as indices into its music selector
 * table. This returns the SM64 sequence id from MB64's seq_musicmenu_array, or
 * 0 when the index is outside the authored table.
 */
uint8_t mb64_music_sequence_from_index(uint8_t music_index);

int mb64_build_render_mesh(const mb64_level_t *level, mb64_mesh_t *mesh);
int mb64_build_collision_mesh(const mb64_level_t *level, mb64_mesh_t *mesh);
int mb64_visit_render_mesh(const mb64_level_t *level,
                           const mb64_mesh_visitor_t *visitor,
                           void *user);
int mb64_visit_collision_mesh(const mb64_level_t *level,
                              const mb64_mesh_visitor_t *visitor,
                              void *user);
void mb64_free_render_mesh(mb64_mesh_t *mesh);
uint8_t mb64_tile_has_collision(const mb64_tile_t *tile);
uint8_t mb64_tile_has_terrain_collision(const mb64_tile_t *tile);
uint8_t mb64_mesh_face_has_terrain_collision(const mb64_mesh_face_t *face);
uint8_t mb64_level_grid_size(const mb64_level_t *level);
uint8_t mb64_level_grid_min(const mb64_level_t *level);
uint8_t mb64_tile_occludes_face(const mb64_level_t *level,
                                const mb64_tile_t *cur,
                                const mb64_tile_t *other,
                                uint8_t direction);
uint8_t mb64_boundary_flags_for_level(const mb64_level_t *level);
uint32_t mb64_build_death_plane_faces(const mb64_level_t *level,
                                      mb64_boundary_face_t out[MB64_DEATH_PLANE_FACE_COUNT]);
uint8_t mb64_resolve_tile_material(const mb64_level_t *level,
                                   const mb64_tile_t *tile,
                                   uint8_t top_face);
int16_t mb64_surface_for_material(uint8_t material);
int16_t mb64_surface_for_tile(const mb64_level_t *level, const mb64_tile_t *tile);
int16_t mb64_surface_for_mesh_face(const mb64_mesh_face_t *face);
mb64_material_texture_animation_t mb64_texture_animation_for_material(uint8_t material);
mb64_material_texture_animation_t mb64_texture_animation_for_water(const mb64_level_t *level);
int mb64_render_binding_for_face(const mb64_level_t *level,
                                 const mb64_mesh_face_t *face,
                                 mb64_render_binding_t *out);

/**
 * mb64_find_water_column_top() - MB64's Y-aware stacked-water lookup.
 *
 * @grid_x, @grid_y, and @grid_z are MB64 grid coordinates. The query mirrors
 * src/mb64/collision.c: if the query cell is water, scan upward to the top of
 * the contiguous water column; otherwise scan downward to the nearest water
 * column below. Returns 1 and writes the top water grid Y on success, or 0 when
 * no local water column exists.
 */
int mb64_find_water_column_top(const mb64_level_t *level,
                               int grid_x,
                               int grid_y,
                               int grid_z,
                               int *out_top_grid_y);

/**
 * mb64_find_water_surface() - MB64's water lookup plus surface shape.
 *
 * Mirrors mb64_get_water_level() from the MB64 runtime: first find the local
 * Y-aware water column, then report whether that water surface is a full block
 * surface or the usual shallow water face. Consumers keep their own world-unit
 * conversion but must use this to avoid reimplementing MB64 shape/material
 * rules.
 */
int mb64_find_water_surface(const mb64_level_t *level,
                            int grid_x,
                            int grid_y,
                            int grid_z,
                            int *out_top_grid_y,
                            int *out_fullblock);

/**
 * mb64_find_water_query_surface() - Gameplay-safe water lookup.
 *
 * Use this for Mario/object water checks with a real query Y. If the query
 * cell itself contains water, the returned surface belongs to that cell rather
 * than the top of a taller contiguous stack. This prevents side-entry into a
 * lower water block from snapping gameplay to an upper stacked water surface.
 * If the query cell is not water, this falls back to mb64_find_water_surface().
 */
int mb64_find_water_query_surface(const mb64_level_t *level,
                                  int grid_x,
                                  int grid_y,
                                  int grid_z,
                                  int *out_surface_grid_y,
                                  int *out_fullblock);

/**
 * mb64_theme_specials_for_level() - Return MB64 special material ids.
 *
 * Regular tile materials resolve through mb64_resolve_tile_material(); fences,
 * poles, bars, and water are stored as separate theme-special ids in MB64. This
 * returns those ids using the same built-in/custom-theme lookup that libmb64
 * uses during mesh generation.
 */
const mb64_theme_special_t *mb64_theme_specials_for_level(const mb64_level_t *level);

/**
 * mb64_water_vertex_color() - Return animated water vertex color.
 *
 * The renderer supplies @wave in the 0..15 range for the current frame. libmb64
 * owns the MB64 palette constants and writes RGBA values into @rgba.
 */
void mb64_water_vertex_color(const mb64_level_t *level, uint8_t wave, uint8_t rgba[4]);

/**
 * mb64_woodplat_config() - Return MB64 Wooden Platform behavior constants.
 *
 * These values mirror bhvWoodPlat in Mario Builder 64. Consumers still own
 * engine-specific movement/collision calls, but should source the portable
 * behavior constants here rather than duplicating magic numbers.
 */
const mb64_woodplat_config_t *mb64_woodplat_config(void);

float mb64_woodplat_piece_height(uint8_t bparam);
float mb64_woodplat_stack_height(const uint8_t *bparams, size_t count);
uint8_t mb64_woodplat_should_stack(uint8_t bparam, float nearest_distance);
float mb64_woodplat_water_float_accel(float water_level, float platform_y);
float mb64_woodplat_water_velocity(float current_vel_y, float water_level, float platform_y,
                                   uint8_t mario_on_platform, uint8_t ground_pound_landing);
int mb64_woodplat_death_drop_offset(void);
uint8_t mb64_woodplat_should_use_simple_wall_checks(uint8_t floor_is_conveyor,
                                                    uint8_t floor_object_has_vertical_push,
                                                    uint8_t on_ground);
uint8_t mb64_woodplat_should_die_on_death_barrier(uint8_t has_floor, uint8_t floor_is_death_plane,
                                                  float platform_y, float floor_y);
const mb64_looping_platform_config_t *mb64_looping_platform_config(void);
const mb64_bullet_bill_config_t *mb64_bullet_bill_config(void);
const mb64_object_hitbox_t *mb64_bullet_bill_hitbox(void);
uint8_t mb64_bullet_bill_should_wake(int angle_diff, float distance);
float mb64_bullet_bill_forward_velocity(int timer, float launch_speed);
uint8_t mb64_bullet_bill_should_launch(int timer);
uint8_t mb64_bullet_bill_should_floor_probe(int timer);
uint8_t mb64_bullet_bill_should_rotate_toward_player(float distance);
uint8_t mb64_bullet_bill_should_timeout(int timer);
uint8_t mb64_bullet_bill_should_reset_after_explosion(int timer);
const mb64_reinforced_box_config_t *mb64_reinforced_box_config(void);
const mb64_object_hitbox_t *mb64_reinforced_box_hitbox(void);
uint8_t mb64_reinforced_box_should_clank(int timer);
uint8_t mb64_reinforced_box_should_shake(int timer);
float mb64_reinforced_box_shake_offset(float random_unit);
const mb64_fire_spinner_config_t *mb64_fire_spinner_config(void);
uint8_t mb64_fire_spinner_flames_per_arm(uint8_t behavior_param_2);
uint8_t mb64_goomba_size_param_for_type(uint8_t object_type);
uint8_t mb64_koopa_behavior_param_for_type(uint8_t object_type, uint8_t authored_param);
const mb64_exclamation_box_config_t *mb64_exclamation_box_config(void);
const mb64_exclamation_box_content_t *mb64_exclamation_box_content(uint8_t game, uint8_t bparam);
uint8_t mb64_exclamation_box_should_explode(int timer);
uint8_t mb64_exclamation_box_should_respawn(int timer);
const mb64_floor_switch_config_t *mb64_floor_switch_config(void);
int mb64_floor_switch_hidden_box_timer(uint8_t double_time_equipped);
int mb64_floor_switch_fast_tick_threshold(uint8_t double_time_equipped);
uint8_t mb64_floor_switch_should_press(float lateral_distance);
uint8_t mb64_floor_switch_scale_done(int timer);
uint8_t mb64_floor_switch_should_timeout(int timer, uint8_t double_time_equipped);
uint8_t mb64_hidden_box_should_blink(int hidden_box_timer);
uint8_t mb64_timed_block_is_solid(int hidden_box_timer);
uint8_t mb64_timed_block_show_on_model(int hidden_box_timer);
uint8_t mb64_conveyor_shape(uint8_t bparam);
uint8_t mb64_conveyor_state(uint8_t bparam);
uint8_t mb64_conveyor_effective_shape(uint8_t bparam, uint8_t play_onoff);
uint8_t mb64_conveyor_effective_bparam(uint8_t bparam, uint8_t play_onoff);
uint8_t mb64_conveyor_has_vertical_push(uint8_t bparam, uint8_t play_onoff);
int8_t mb64_conveyor_initial_vertical_push(uint8_t bparam);
uint8_t mb64_conveyor_should_flip_state(uint8_t anim_state, uint8_t play_onoff);
const mb64_noteblock_config_t *mb64_noteblock_config(void);
int mb64_noteblock_graph_angle(int timer);
float mb64_noteblock_next_velocity(float vel_y);
uint8_t mb64_noteblock_should_bounce(uint8_t intangible, uint8_t swimming, int health, uint8_t mario_on_platform);
const mb64_onoff_config_t *mb64_onoff_config(void);
float mb64_onoff_button_pressed_scale(float base_scale);
float mb64_onoff_button_scale_step(float base_scale);
float mb64_onoff_button_collision_min_scale(float base_scale);
uint8_t mb64_onoff_button_initial_anim_state(uint8_t bparam);
uint8_t mb64_onoff_button_is_pressed(uint8_t anim_state, uint8_t play_onoff);
uint8_t mb64_onoff_button_should_rise(uint8_t bparam, uint8_t play_onoff);
uint8_t mb64_onoff_state_from_bparam(uint8_t bparam);
uint8_t mb64_onoff_block_is_active(uint8_t bparam, uint8_t play_onoff);
const mb64_badge_config_t *mb64_badge_config(void);
const mb64_green_coin_config_t *mb64_green_coin_config(void);
uint8_t mb64_badge_is_equipped(uint32_t equipped_badges, uint8_t badge_id);
uint8_t mb64_badge_should_collect(uint8_t equipped, uint8_t overlaps_mario, uint8_t mario_levelup_dance);
float mb64_badge_next_collect_scale(float current_scale);
uint8_t mb64_badge_should_delete(float current_scale);
const mb64_powerup_config_t *mb64_powerup_config(void);
uint8_t mb64_powerup_bit_for_bparam(uint8_t behavior_param_2);
uint8_t mb64_powerup_should_sparkle(float distance_to_mario, int global_timer);
uint8_t mb64_powerup_should_respawn(int timer);
const mb64_phantasm_config_t *mb64_phantasm_config(void);
int mb64_phantasm_initial_health(uint8_t behavior_param_2);
int mb64_phantasm_initial_loot_coins(uint8_t behavior_param_2);
uint8_t mb64_phantasm_should_wander(int timer);
uint8_t mb64_phantasm_should_attack(uint8_t facing_mario, float distance_to_mario);
uint8_t mb64_phantasm_should_throw_fireball(int timer);
uint8_t mb64_phantasm_should_end_fireball_attack(int timer);
uint8_t mb64_phantasm_should_die_after_hit(int timer, int health);
uint8_t mb64_phantasm_should_recover_after_hit(int timer);
float mb64_phantasm_kick_forward_vel(float distance_to_mario);
uint8_t mb64_phantasm_should_prevent_ledge_drop(float old_floor_y, float current_floor_y);
const mb64_showrunner_config_t *mb64_showrunner_config(void);
uint8_t mb64_showrunner_should_trigger(float distance_to_mario);
uint8_t mb64_showrunner_back_away_finished(float forward_vel);
uint8_t mb64_showrunner_should_spawn_spike(int timer);
uint8_t mb64_showrunner_spike_attack_finished(int timer);
uint8_t mb64_showrunner_should_start_tennis_projectile(int timer);
uint8_t mb64_showrunner_stun_from_tennis(int health, int tennis_damage);
uint8_t mb64_showrunner_should_recover_from_stun(int timer);
uint8_t mb64_showrunner_should_leave_damaged(int timer);
uint8_t mb64_showrunner_should_drop_items(int timer);
uint8_t mb64_showrunner_should_shrink(int timer);
uint8_t mb64_showrunner_should_delete(float scale);
uint8_t mb64_showrunner_should_spawn_ballerina_projectile(int timer, int subaction, int angle_vel_yaw);
uint8_t mb64_showrunner_should_spawn_phantasm_release_flames(int timer);
uint8_t mb64_showrunner_spike_should_lock_to_mario(uint8_t already_close, float distance_to_mario);
uint8_t mb64_showrunner_spike_should_leave_rumble(int timer);
uint8_t mb64_showrunner_spike_should_finish_rising(float pos_y, float home_y);
uint8_t mb64_showrunner_spike_should_retract(int parent_action);
uint8_t mb64_showrunner_spike_should_delete(float pos_y, float home_y);
uint8_t mb64_showrunner_tennis_should_return_to_parent(uint8_t returning_to_parent);
uint8_t mb64_showrunner_tennis_should_reset_parent(float distance_to_parent);
uint8_t mb64_showrunner_tennis_should_stun_parent(int parent_health, int tennis_damage);
uint8_t mb64_showrunner_tennis_trail_should_delete(int opacity);
uint8_t mb64_showrunner_thwomp_flame_should_leave_exist(int timer, float forward_vel);
uint8_t mb64_showrunner_thwomp_flame_should_delete(int timer);
float mb64_showrunner_thwomp_flame_scale(int timer);
uint8_t mb64_showrunner_cosmic_projectile_should_delete(int timer, uint8_t hit_wall);
const mb64_motos_config_t *mb64_motos_config(void);
uint8_t mb64_motos_should_search(float distance_to_mario);
uint8_t mb64_motos_should_stop_searching(float distance_to_mario);
uint8_t mb64_motos_should_throw(int timer, uint8_t hit_edge);
uint8_t mb64_motos_escape_succeeds(int escape_actions);
uint8_t mb64_motos_should_leave_recover_wait(int timer);
const mb64_chicken_config_t *mb64_chicken_config(void);
const mb64_crablet_config_t *mb64_crablet_config(void);
const mb64_object_hitbox_t *mb64_crablet_hitbox(void);
uint8_t mb64_crablet_should_attack(int angle_diff, float distance_to_mario);
uint8_t mb64_crablet_should_end_attack(int timer);
uint8_t mb64_crablet_should_finish_recovery(int timer);
uint8_t mb64_crablet_should_grab_head(float distance_to_mario, float crablet_y, float mario_y, uint8_t already_grabbed);
int mb64_crablet_hurt_quicksand_depth(int quicksand_depth);
const mb64_fire_bro_config_t *mb64_fire_bro_config(void);
const mb64_object_hitbox_t *mb64_fire_bro_hitbox(void);
uint8_t mb64_fire_bro_can_throw(float mario_y, float bro_y);
uint8_t mb64_fire_bro_should_start_throw(float distance_to_mario, int timer);
uint8_t mb64_fire_bro_should_leave_hold(int timer);
uint8_t mb64_fire_bro_should_repeat_or_jump(int timer);
uint8_t mb64_fire_bro_should_delete_fireball(int timer, uint8_t hit_wall);
uint8_t mb64_fire_bro_should_bounce_fireball(uint32_t move_flags);
const mb64_hammer_bro_config_t *mb64_hammer_bro_config(void);
const mb64_object_hitbox_t *mb64_hammer_projectile_hitbox(void);
uint8_t mb64_hammer_bro_can_throw(float mario_y, float bro_y);
uint8_t mb64_hammer_bro_should_start_throw(float distance_to_mario, int timer);
uint8_t mb64_hammer_bro_should_leave_hold(int timer);
uint8_t mb64_hammer_bro_should_repeat_or_jump(int timer);
uint16_t mb64_hammer_bro_random_range(uint16_t random, uint16_t min, uint16_t max);
int mb64_hammer_bro_hurt_quicksand_depth(int quicksand_depth);
float mb64_hammer_bro_projectile_y_offset(float quicksand_depth);
uint8_t mb64_hammer_should_arm_hitbox(int timer);
uint8_t mb64_hammer_should_delete(int timer, uint32_t move_flags, uint8_t attacked, uint8_t interacted);
const mb64_rex_config_t *mb64_rex_config(void);
const mb64_npc_config_t *mb64_moleman_config(void);
const mb64_npc_config_t *mb64_cobie_config(void);
const mb64_npc_config_t *mb64_toad_config(void);
const mb64_npc_config_t *mb64_tuxie_config(void);
const mb64_npc_config_t *mb64_ukiki_config(void);
const mb64_podoboo_config_t *mb64_podoboo_config(void);
float mb64_podoboo_launch_velocity(float rest_y, float peak_y);
uint8_t mb64_podoboo_should_reset_idle_timer(float distance_to_mario);
uint8_t mb64_podoboo_should_spawn_warmup_flame(int timer);
uint8_t mb64_podoboo_should_launch(int timer);
const mb64_pokey_config_t *mb64_pokey_config(void);
const mb64_object_hitbox_t *mb64_pokey_body_part_hitbox(void);
uint32_t mb64_pokey_alive_flags(uint8_t segment_count);
float mb64_pokey_part_spawn_y(uint8_t part_index);
int mb64_pokey_part_offset_angle(uint8_t part_index, int timer);
float mb64_pokey_part_base_height(float parent_y,
                                  uint8_t alive_parts,
                                  uint8_t part_index,
                                  float bottom_size,
                                  float quicksand_depth);
float mb64_pokey_part_graph_y_offset(float scale_y);
int mb64_pokey_part_death_delay(uint8_t part_index);
uint8_t mb64_pokey_should_shift_part(uint8_t part_index, uint32_t alive_flags);
uint8_t mb64_pokey_should_expand_bottom(float bottom_size,
                                        uint8_t part_index,
                                        uint8_t alive_parts);
uint8_t mb64_pokey_should_spawn_parts(float distance_to_mario, float drawing_distance);
uint8_t mb64_pokey_should_unload(float distance_to_mario, float drawing_distance);
uint8_t mb64_pokey_should_regrow(uint8_t alive_parts, int timer, uint8_t floor_is_instant_quicksand);
int mb64_pokey_target_angle_offset(float distance_to_mario, int angle_to_mario, int move_angle_yaw);
uint8_t mb64_pokey_should_die_in_quicksand(uint8_t part_index,
                                           uint8_t alive_parts,
                                           float quicksand_depth);

#ifdef __cplusplus
}
#endif

#endif /* MB64_H */
