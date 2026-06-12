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

static const mb64_exclamation_box_config_t s_exclamation_box_config = {
    0x4000, /* scale_angle_start */
    0x1000, /* scale_angle_step */
    7,      /* explode_frame */
    300,    /* respawn_frames */
    30.0f,  /* hit_launch_vel_y */
    -8.0f,  /* hit_gravity */
    0.3f,   /* squash_scale_factor */
    0.5f,   /* stretch_scale_factor */
    1.0f,   /* stretch_scale_offset */
    26.0f,  /* graph_y_offset_factor */
    2.0f,   /* model_scale */
};

static const mb64_floor_switch_config_t s_floor_switch_config = {
    3,      /* scale_frames */
    400,    /* timer_frames */
    800,    /* double_time_timer_frames */
    40,     /* hidden_box_blink_frames */
    1.28f,  /* switch_scale */
    0.2f,   /* pressed_scale */
    127.5f, /* press_radius */
};

static const mb64_noteblock_config_t s_noteblock_config = {
    5000,  /* graph_angle_step */
    0x100, /* min_health */
    0.95f, /* velocity_decay */
    50.0f, /* bounce_graph_vel_y */
    95.0f, /* bounce_mario_vel_y */
};

static const mb64_onoff_config_t s_onoff_config = {
    0.1f,  /* pressed_scale_factor */
    0.1f,  /* scale_step_factor */
    0.11f, /* collision_min_scale_y */
};

static const mb64_badge_config_t s_badge_config = {
    0x70,  /* spin_accel */
    0.95f, /* shrink_factor */
    0.2f,  /* delete_scale */
};

static const mb64_green_coin_config_t s_green_coin_config = {
    3.0f,   /* damage_or_coin_value */
    100.0f, /* hitbox_radius */
    64.0f,  /* hitbox_height */
};

static const mb64_powerup_config_t s_powerup_config = {
    1u,       /* Crowbar power bit: 1 << 0 */
    2u,       /* Bullet Bill Mask power bit: 1 << 1 */
    3u,       /* sparkle every fourth frame */
    0x1A00,   /* pickup face pitch */
    0x400,    /* pickup yaw spin step */
    30 * 5,   /* hidden respawn delay */
    4000.0f,  /* MB64_DRAWDIST_LOW */
    6000.0f,  /* MB64_DRAWDIST_HIGH */
    80.0f,    /* hitbox radius */
    160.0f,   /* hitbox height */
    80.0f,    /* hitbox downward offset */
    -80.0f,   /* bhvBMask graph y offset */
};

static const mb64_phantasm_config_t s_phantasm_config = {
    3,        /* default health */
    2,        /* behavior param 2: Showrunner-dispensed health */
    5,        /* default loot coins */
    0,        /* Showrunner-dispensed loot coins */
    100,      /* idle timer before wandering */
    100,      /* wander timer before idle */
    60,       /* alert timer before attacking */
    40,       /* alert turn/back-up timer */
    125,      /* fireball throw window */
    160,      /* fireball attack end */
    25,       /* fireball interval */
    30,       /* attacked death check */
    50,       /* attacked recover */
    0x2000,   /* facing angle range */
    1000.0f,  /* attack distance when facing Mario */
    400.0f,   /* close attack distance */
    100.0f,   /* hitbox radius */
    100.0f,   /* hitbox height */
    100.0f,   /* hurtbox radius */
    80.0f,    /* hurtbox height */
    150.0f,   /* invincible hurtbox radius */
    120.0f,   /* invincible hurtbox height */
    3.0f,     /* damage */
    -4.0f,    /* default gravity */
    -0.5f,    /* alert gravity down */
    0.5f,     /* alert gravity up */
    300.0f,   /* desired height above Mario */
    10.0f,    /* wander speed */
    -20.0f,   /* alert back-up speed clamp */
    -20.0f,   /* alert back-up min */
    40.0f,    /* kick base speed */
    3.0f,     /* kick distance divisor */
    75.0f,    /* kick max speed */
    -3.0f,    /* kick velY */
    0.95f,    /* kick deceleration */
    1.0f,     /* vulnerable below this speed */
    -10.0f,   /* fireball back-up speed */
    30.0f,    /* fireball forward velocity */
    20.0f,    /* fireball vertical velocity */
    20.0f,    /* fireball y offset */
    300.0f,   /* floor drop guard */
    100.0f,   /* MB64_STAR_HEIGHT */
};

static const mb64_showrunner_config_t s_showrunner_config = {
    3,        /* battle health */
    2,        /* first spike volley count */
    1,        /* post-phantasm-release spike volley count */
    20,       /* spike spawn window start */
    90,       /* spike spawn window end */
    5,        /* spike spawn interval */
    110,      /* spike attack end timer */
    1,        /* back-away friction starts after this timer */
    60,       /* tennis projectile spawn timer */
    {0, 8, 5, 3}, /* tennis_turns[] indexed by health */
    160,      /* stunned recovery timer */
    60,       /* damaged recovery timer */
    60,       /* battle-end timer */
    20,       /* shrink sound timer */
    400,      /* ballerina attack end timer */
    0x20,     /* ballerina spin acceleration */
    0x2000,   /* ballerina spin max */
    30,       /* ballerina projectile starts after this timer */
    3,        /* ballerina projectile spawn interval */
    0x1000,   /* ballerina projectile min spin speed */
    500.0f,   /* ballerina projectile random Y range */
    1500.0f,  /* MB64_BOSS_TRIGGER_DIST */
    1.0f,     /* scale */
    -70.0f,   /* back-away velocity */
    0.94f,    /* back-away friction */
    -5.0f,    /* back-away finished once faster than this */
    400.0f,   /* first spike target offset */
    350.0f,   /* spike chain target step */
    300.0f,   /* close spike lock distance */
    500.0f,   /* spike floor probe offset */
    400.0f,   /* spike home offset below floor */
    10.0f,    /* tennis movement speed */
    500.0f,   /* tennis projectile y offset */
    400.0f,   /* spike rumble particle Y offset */
    10,       /* spike rumble end timer */
    20.0f,    /* spike rise step */
    400.0f,   /* spike rise height */
    200,      /* tennis turn rate */
    1,        /* tennis initial damage/volley count */
    20.0f,    /* tennis initial forward velocity */
    100.0f,   /* tennis hitbox radius */
    100.0f,   /* tennis hitbox height */
    80.0f,    /* tennis hurtbox radius */
    80.0f,    /* tennis hurtbox height */
    400.0f,   /* tennis return contact distance */
    10.0f,    /* tennis speed increase after parent return */
    255,      /* tennis trail initial opacity */
    10,       /* tennis trail fade step */
    12,       /* tennis trail delete opacity */
    300.0f,   /* hitbox radius */
    800.0f,   /* hitbox height */
    0.0f,     /* normal damage */
    3.0f,     /* ballerina damage */
    0.01f,    /* death shrink step */
    0.1f,     /* delete below this scale */
    50,       /* loot coins */
    384.0f,   /* MB64_STAR_HEIGHT */
    45,       /* phantasm-release flame spawn timer */
    32,       /* phantasm-release flame count */
    0x800,    /* phantasm-release flame angle step */
    40.0f,    /* phantasm-release flame Y offset */
    30.0f,    /* phantasm-release flame forward velocity */
    4.0f,     /* thwomp flame medium lifetime speed threshold */
    16.0f,    /* thwomp flame fast lifetime speed threshold */
    200,      /* thwomp flame slow lifetime */
    100,      /* thwomp flame medium lifetime */
    35,       /* thwomp flame fast lifetime */
    30,       /* thwomp flame shrink timer */
    7.0f,     /* thwomp flame scale */
    35.0f,    /* cosmic projectile forward velocity */
    110,      /* cosmic projectile delete timer */
    0x400,    /* cosmic projectile roll step */
};

static const mb64_motos_config_t s_motos_config = {
    2.0f,     /* object scale */
    -70.0f,   /* hand relative X */
    -30.0f,   /* hand relative Y */
    50.0f,    /* anchor throw forward velocity */
    30.0f,    /* anchor throw vertical velocity */
    64,       /* anchor throw status arg */
    1000.0f,  /* distance to begin player search */
    1500.0f,  /* distance to stop player search */
    5.0f,     /* search forward speed */
    800,      /* search yaw turn speed */
    45,       /* throw timer */
    20,       /* escape actions before dropped */
    15.0f,    /* carry-run speed */
    14,       /* pitch throw animation frame */
    35,       /* recover wait timer */
    15.0f,    /* thrown/placed forward velocity */
    35.0f,    /* thrown/placed vertical velocity */
    10.0f,    /* blue coin forward velocity */
    100.0f,   /* blue coin vertical velocity */
    310.0f,   /* blue coin Y offset */
    100.0f,   /* MB64_STAR_HEIGHT */
    150,      /* quicksand depth to die */
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

static const mb64_crablet_config_t s_crablet_config = {
    0,        /* crab_anims_anims walk animation */
    1.0f,     /* object table scale */
    130.0f,   /* sScuttlebugHitbox radius */
    -4.0f,    /* SET_OBJ_PHYSICS_DEFAULT gravity */
    -0.5f,    /* SET_OBJ_PHYSICS_DEFAULT bounciness */
    10.0f,    /* SET_OBJ_PHYSICS_DEFAULT drag strength */
    10.0f,    /* SET_OBJ_PHYSICS_DEFAULT friction */
    2.0f,     /* SET_OBJ_PHYSICS_DEFAULT buoyancy */
    5.0f,     /* patrol forward velocity */
    30.0f,    /* attack forward velocity */
    50.0f,    /* attack jump velocity */
    -10.0f,   /* hurt knockback forward velocity */
    30.0f,    /* hurt knockback vertical velocity */
    2.0f,     /* recover forward velocity */
    1000.0f,  /* attack distance */
    0x2000,   /* attack angle threshold */
    200.0f,   /* head grab distance */
    30.0f,    /* crablet must be above Mario by this offset */
    60.0f,    /* carried x/z offset from Mario */
    100.0f,   /* carried y offset from Mario */
    50,       /* attack timer */
    30,       /* recover timer */
    15,       /* quicksand stun depth reduction */
    384,      /* MB64_STAR_HEIGHT */
};

static const mb64_fire_bro_config_t s_fire_bro_config = {
    1,       /* behavior_param_2: Fire Bro texture/projectile variant */
    0,       /* idle/land animation */
    2,       /* throw animation */
    3,       /* jump animation */
    1.0f,    /* object table scale */
    80.0f,   /* sHammerBroHitbox radius */
    -4.0f,   /* SET_OBJ_PHYSICS_DEFAULT gravity */
    -0.5f,   /* SET_OBJ_PHYSICS_DEFAULT bounciness */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT drag strength */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT friction */
    2.0f,    /* SET_OBJ_PHYSICS_DEFAULT buoyancy */
    -200.0f, /* do not throw if Mario is far below */
    1500.0f, /* reset attack timer when Mario is outside this distance */
    20.0f,   /* projectile y spawn offset */
    20.0f,   /* fireball initial vertical velocity */
    30.0f,   /* fireball initial forward velocity */
    35.0f,   /* fireball loop forward velocity */
    60.0f,   /* jump vertical velocity */
    40,      /* start throw after this timer */
    15,      /* leave hold pose after this timer */
    25,      /* repeat/jump after this timer */
    30,      /* random rearm timer max after landing */
    300,     /* fireball timeout */
};

static const mb64_hammer_bro_config_t s_hammer_bro_config = {
    0,       /* idle/land animation */
    2,       /* throw animation */
    3,       /* jump animation */
    1.0f,    /* object table scale */
    80.0f,   /* sHammerBroHitbox radius */
    -4.0f,   /* SET_OBJ_PHYSICS_DEFAULT gravity */
    -0.5f,   /* SET_OBJ_PHYSICS_DEFAULT bounciness */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT drag strength */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT friction */
    2.0f,    /* SET_OBJ_PHYSICS_DEFAULT buoyancy */
    -200.0f, /* do not throw if Mario is far below */
    1500.0f, /* reset attack timer when Mario is outside this distance */
    20.0f,   /* projectile y spawn offset */
    2.0f,    /* quicksand depth lowers projectile spawn by this multiplier */
    30.0f,   /* hammer initial vertical velocity min */
    50.0f,   /* hammer initial vertical velocity max */
    20.0f,   /* hammer initial forward velocity min */
    50.0f,   /* hammer initial forward velocity max */
    60.0f,   /* jump vertical velocity */
    210.0f,  /* hammer first-frame y offset */
    40,      /* start throw after this timer */
    15,      /* leave hold pose after this timer */
    25,      /* repeat/jump after this timer */
    30,      /* random rearm timer max after landing */
    15,      /* quicksand stun depth reduction */
    20,      /* hammer arms hitbox after this timer */
    -0x2000, /* hammer pitch before hitbox arms */
    0x2000,  /* hammer pitch spin step */
    300,     /* hammer timeout */
};

static const mb64_rex_config_t s_rex_config = {
    1,       /* health */
    0,       /* animation_index */
    1.5f,    /* object table scale */
    -30.0f,  /* graph_y_offset */
    4000.0f, /* MB64_DRAWDIST_LOW */
    40.0f,   /* SET_OBJ_PHYSICS_DEFAULT wall hitbox radius */
    -4.0f,   /* SET_OBJ_PHYSICS_DEFAULT gravity */
    -0.5f,   /* SET_OBJ_PHYSICS_DEFAULT bounciness */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT drag strength */
    10.0f,   /* SET_OBJ_PHYSICS_DEFAULT friction */
    2.0f,    /* SET_OBJ_PHYSICS_DEFAULT buoyancy */
};

static const mb64_npc_config_t s_moleman_config = {
    0,       /* animation_index */
    0,       /* oBobombBuddyRole */
    -1,      /* no forced anim state */
    65.0f,   /* graph_y_offset */
    100.0f,  /* hitbox_radius */
    60.0f,   /* hitbox_height */
    4000.0f, /* MB64_DRAWDIST_LOW */
};

static const mb64_npc_config_t s_cobie_config = {
    0,       /* animation_index */
    0,       /* oBobombBuddyRole */
    0,       /* force oAnimState every frame */
    0.0f,    /* graph_y_offset */
    130.0f,  /* hitbox_radius */
    60.0f,   /* hitbox_height */
    4000.0f, /* MB64_DRAWDIST_LOW */
};

static const mb64_npc_config_t s_toad_config = {
    6,       /* TOAD_ANIM_WEST_WAVING_BOTH_ARMS */
    0,       /* oBobombBuddyRole */
    -1,      /* no forced anim state */
    0.0f,    /* graph_y_offset */
    100.0f,  /* hitbox_radius */
    60.0f,   /* hitbox_height */
    6000.0f, /* MB64_DRAWDIST_MEDIUM */
};

static const mb64_npc_config_t s_tuxie_config = {
    0,       /* PENGUIN_ANIM_WALK */
    0,       /* oBobombBuddyRole */
    -1,      /* no forced anim state */
    0.0f,    /* graph_y_offset */
    100.0f,  /* hitbox_radius */
    60.0f,   /* hitbox_height */
    6000.0f, /* MB64_DRAWDIST_MEDIUM */
};

static const mb64_npc_config_t s_ukiki_config = {
    4,       /* UKIKI_ANIM_SCREECH */
    0,       /* oBobombBuddyRole */
    -1,      /* no forced anim state */
    0.0f,    /* graph_y_offset */
    100.0f,  /* hitbox_radius */
    60.0f,   /* hitbox_height */
    6000.0f, /* MB64_DRAWDIST_MEDIUM */
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

const mb64_exclamation_box_config_t *mb64_exclamation_box_config(void) {
    return &s_exclamation_box_config;
}

uint8_t mb64_exclamation_box_should_explode(int timer) {
    return timer == s_exclamation_box_config.explode_frame;
}

uint8_t mb64_exclamation_box_should_respawn(int timer) {
    return timer > s_exclamation_box_config.respawn_frames;
}

const mb64_floor_switch_config_t *mb64_floor_switch_config(void) {
    return &s_floor_switch_config;
}

int mb64_floor_switch_hidden_box_timer(uint8_t double_time_equipped) {
    return double_time_equipped ? s_floor_switch_config.double_time_timer_frames :
        s_floor_switch_config.timer_frames;
}

int mb64_floor_switch_fast_tick_threshold(uint8_t double_time_equipped) {
    return mb64_floor_switch_hidden_box_timer(double_time_equipped) -
        s_floor_switch_config.hidden_box_blink_frames;
}

uint8_t mb64_floor_switch_should_press(float lateral_distance) {
    return lateral_distance < s_floor_switch_config.press_radius;
}

uint8_t mb64_floor_switch_scale_done(int timer) {
    return timer == s_floor_switch_config.scale_frames;
}

uint8_t mb64_floor_switch_should_timeout(int timer, uint8_t double_time_equipped) {
    return timer > mb64_floor_switch_hidden_box_timer(double_time_equipped);
}

uint8_t mb64_hidden_box_should_blink(int hidden_box_timer) {
    return hidden_box_timer > 0 &&
        hidden_box_timer < s_floor_switch_config.hidden_box_blink_frames &&
        (hidden_box_timer & 1);
}

uint8_t mb64_conveyor_shape(uint8_t bparam) {
    return bparam & 0x3;
}

uint8_t mb64_conveyor_state(uint8_t bparam) {
    return (bparam >> 2) & 0x3;
}

uint8_t mb64_conveyor_effective_shape(uint8_t bparam, uint8_t play_onoff) {
    const uint8_t shape = mb64_conveyor_shape(bparam);
    const uint8_t state = mb64_conveyor_state(bparam);

    if (state == MB64_CONVEYOR_STATE_ALWAYS || shape < MB64_CONVEYOR_SHAPE_SLOPE) {
        return shape;
    }

    if ((shape == MB64_CONVEYOR_SHAPE_SLOPE && play_onoff) ||
        (shape == MB64_CONVEYOR_SHAPE_DOWNSLOPE && !play_onoff)) {
        return MB64_CONVEYOR_SHAPE_DOWNSLOPE;
    }

    return MB64_CONVEYOR_SHAPE_SLOPE;
}

uint8_t mb64_conveyor_effective_bparam(uint8_t bparam, uint8_t play_onoff) {
    return (bparam & (uint8_t) ~0x3) | mb64_conveyor_effective_shape(bparam, play_onoff);
}

uint8_t mb64_conveyor_has_vertical_push(uint8_t bparam, uint8_t play_onoff) {
    if (mb64_conveyor_state(bparam) == MB64_CONVEYOR_STATE_ALWAYS) {
        return mb64_conveyor_shape(bparam) >= MB64_CONVEYOR_SHAPE_SLOPE;
    }

    return mb64_conveyor_effective_shape(bparam, play_onoff) >= MB64_CONVEYOR_SHAPE_SLOPE;
}

int8_t mb64_conveyor_initial_vertical_push(uint8_t bparam) {
    const uint8_t shape = mb64_conveyor_shape(bparam);
    if (shape == MB64_CONVEYOR_SHAPE_SLOPE) {
        return 1;
    }
    if (shape == MB64_CONVEYOR_SHAPE_DOWNSLOPE) {
        return -1;
    }
    return 0;
}

uint8_t mb64_conveyor_should_flip_state(uint8_t anim_state, uint8_t play_onoff) {
    return anim_state > MB64_CONVEYOR_STATE_ALWAYS && anim_state != (uint8_t) (play_onoff + 1);
}

const mb64_noteblock_config_t *mb64_noteblock_config(void) {
    return &s_noteblock_config;
}

int mb64_noteblock_graph_angle(int timer) {
    return timer * s_noteblock_config.graph_angle_step;
}

float mb64_noteblock_next_velocity(float vel_y) {
    return vel_y * s_noteblock_config.velocity_decay;
}

uint8_t mb64_noteblock_should_bounce(uint8_t intangible, uint8_t swimming, int health, uint8_t mario_on_platform) {
    return !intangible && !swimming && health > s_noteblock_config.min_health && mario_on_platform;
}

const mb64_onoff_config_t *mb64_onoff_config(void) {
    return &s_onoff_config;
}

float mb64_onoff_button_pressed_scale(float base_scale) {
    return base_scale * s_onoff_config.pressed_scale_factor;
}

float mb64_onoff_button_scale_step(float base_scale) {
    return base_scale * s_onoff_config.scale_step_factor;
}

float mb64_onoff_button_collision_min_scale(float base_scale) {
    return base_scale * s_onoff_config.collision_min_scale_y;
}

uint8_t mb64_onoff_button_initial_anim_state(uint8_t bparam) {
    return bparam != 0;
}

uint8_t mb64_onoff_button_is_pressed(uint8_t anim_state, uint8_t play_onoff) {
    return (anim_state == 0 && !play_onoff) || (anim_state != 0 && play_onoff);
}

uint8_t mb64_onoff_button_should_rise(uint8_t bparam, uint8_t play_onoff) {
    return (bparam == 0 && play_onoff) || (bparam != 0 && !play_onoff);
}

uint8_t mb64_onoff_state_from_bparam(uint8_t bparam) {
    return bparam != 0;
}

uint8_t mb64_onoff_block_is_active(uint8_t bparam, uint8_t play_onoff) {
    return bparam != 0 ? play_onoff : !play_onoff;
}

const mb64_badge_config_t *mb64_badge_config(void) {
    return &s_badge_config;
}

const mb64_green_coin_config_t *mb64_green_coin_config(void) {
    return &s_green_coin_config;
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

const mb64_powerup_config_t *mb64_powerup_config(void) {
    return &s_powerup_config;
}

uint8_t mb64_powerup_bit_for_bparam(uint8_t behavior_param_2) {
    return behavior_param_2 == 0 ?
        s_powerup_config.crowbar_power_bit :
        s_powerup_config.mask_power_bit;
}

uint8_t mb64_powerup_should_sparkle(float distance_to_mario, int global_timer) {
    return distance_to_mario < s_powerup_config.sparkle_distance &&
        (global_timer & s_powerup_config.sparkle_timer_mask) == 0;
}

uint8_t mb64_powerup_should_respawn(int timer) {
    return timer > s_powerup_config.respawn_frames;
}

const mb64_phantasm_config_t *mb64_phantasm_config(void) {
    return &s_phantasm_config;
}

int mb64_phantasm_initial_health(uint8_t behavior_param_2) {
    return behavior_param_2 == 2 ?
        s_phantasm_config.health_boss_dispensed :
        s_phantasm_config.health_default;
}

int mb64_phantasm_initial_loot_coins(uint8_t behavior_param_2) {
    return behavior_param_2 == 2 ?
        s_phantasm_config.loot_coins_boss_dispensed :
        s_phantasm_config.loot_coins_default;
}

uint8_t mb64_phantasm_should_wander(int timer) {
    return timer > s_phantasm_config.idle_timer;
}

uint8_t mb64_phantasm_should_attack(uint8_t facing_mario, float distance_to_mario) {
    return (facing_mario && distance_to_mario < s_phantasm_config.facing_attack_distance) ||
        distance_to_mario < s_phantasm_config.close_attack_distance;
}

uint8_t mb64_phantasm_should_throw_fireball(int timer) {
    return timer <= s_phantasm_config.fireball_attack_timer &&
        timer % s_phantasm_config.fireball_interval == 0;
}

uint8_t mb64_phantasm_should_end_fireball_attack(int timer) {
    return timer > s_phantasm_config.fireball_end_timer;
}

uint8_t mb64_phantasm_should_die_after_hit(int timer, int health) {
    return timer == s_phantasm_config.attacked_death_check_timer && health < 1;
}

uint8_t mb64_phantasm_should_recover_after_hit(int timer) {
    return timer > s_phantasm_config.attacked_recover_timer;
}

float mb64_phantasm_kick_forward_vel(float distance_to_mario) {
    float speed = (distance_to_mario / s_phantasm_config.kick_distance_divisor) +
        s_phantasm_config.kick_base_speed;
    return speed > s_phantasm_config.kick_speed_max ?
        s_phantasm_config.kick_speed_max :
        speed;
}

uint8_t mb64_phantasm_should_prevent_ledge_drop(float old_floor_y, float current_floor_y) {
    return current_floor_y < old_floor_y - s_phantasm_config.ledge_drop_guard_height;
}

const mb64_showrunner_config_t *mb64_showrunner_config(void) {
    return &s_showrunner_config;
}

uint8_t mb64_showrunner_should_trigger(float distance_to_mario) {
    return distance_to_mario < s_showrunner_config.trigger_distance;
}

uint8_t mb64_showrunner_back_away_finished(float forward_vel) {
    return forward_vel > s_showrunner_config.back_away_end_speed;
}

uint8_t mb64_showrunner_should_spawn_spike(int timer) {
    return timer >= s_showrunner_config.spike_spawn_start_timer &&
        timer < s_showrunner_config.spike_spawn_end_timer &&
        timer % s_showrunner_config.spike_spawn_interval == 0;
}

uint8_t mb64_showrunner_spike_attack_finished(int timer) {
    return timer > s_showrunner_config.spike_attack_end_timer;
}

uint8_t mb64_showrunner_should_start_tennis_projectile(int timer) {
    return timer == s_showrunner_config.tennis_spawn_timer;
}

uint8_t mb64_showrunner_stun_from_tennis(int health, int tennis_damage) {
    if (health < 0 || health >= 4) {
        return 0;
    }
    return tennis_damage == s_showrunner_config.tennis_turns_by_health[health];
}

uint8_t mb64_showrunner_should_recover_from_stun(int timer) {
    return timer > s_showrunner_config.stunned_recover_timer;
}

uint8_t mb64_showrunner_should_leave_damaged(int timer) {
    return timer > s_showrunner_config.damaged_recover_timer;
}

uint8_t mb64_showrunner_should_drop_items(int timer) {
    return timer > s_showrunner_config.battle_end_timer;
}

uint8_t mb64_showrunner_should_shrink(int timer) {
    return timer > s_showrunner_config.shrink_sound_timer;
}

uint8_t mb64_showrunner_should_delete(float scale) {
    return scale < s_showrunner_config.delete_scale;
}

uint8_t mb64_showrunner_should_spawn_ballerina_projectile(int timer, int subaction, int angle_vel_yaw) {
    return subaction == 0 &&
        timer > s_showrunner_config.ballerina_projectile_start_timer &&
        timer % s_showrunner_config.ballerina_projectile_interval == 0 &&
        angle_vel_yaw > s_showrunner_config.ballerina_projectile_min_spin;
}

uint8_t mb64_showrunner_should_spawn_phantasm_release_flames(int timer) {
    return timer == s_showrunner_config.phantasm_release_flame_timer;
}

uint8_t mb64_showrunner_spike_should_lock_to_mario(uint8_t already_close, float distance_to_mario) {
    return !already_close && distance_to_mario < s_showrunner_config.spike_close_distance;
}

uint8_t mb64_showrunner_spike_should_leave_rumble(int timer) {
    return timer > s_showrunner_config.spike_rumble_end_timer;
}

uint8_t mb64_showrunner_spike_should_finish_rising(float pos_y, float home_y) {
    return pos_y > home_y + s_showrunner_config.spike_rise_height;
}

uint8_t mb64_showrunner_spike_should_retract(int parent_action) {
    return parent_action != MB64_SHOWRUNNER_ACT_SPIKE_ATTACK &&
        parent_action != MB64_SHOWRUNNER_ACT_BALLERINA;
}

uint8_t mb64_showrunner_spike_should_delete(float pos_y, float home_y) {
    return pos_y < home_y;
}

uint8_t mb64_showrunner_tennis_should_return_to_parent(uint8_t returning_to_parent) {
    return returning_to_parent != 0;
}

uint8_t mb64_showrunner_tennis_should_reset_parent(float distance_to_parent) {
    return distance_to_parent < s_showrunner_config.tennis_parent_hit_distance;
}

uint8_t mb64_showrunner_tennis_should_stun_parent(int parent_health, int tennis_damage) {
    return mb64_showrunner_stun_from_tennis(parent_health, tennis_damage);
}

uint8_t mb64_showrunner_tennis_trail_should_delete(int opacity) {
    return opacity <= s_showrunner_config.tennis_trail_delete_opacity;
}

uint8_t mb64_showrunner_thwomp_flame_should_leave_exist(int timer, float forward_vel) {
    int expire_timer = s_showrunner_config.thwomp_flame_slow_lifetime;
    if (forward_vel > s_showrunner_config.thwomp_flame_medium_speed) {
        expire_timer = s_showrunner_config.thwomp_flame_medium_lifetime;
    }
    if (forward_vel > s_showrunner_config.thwomp_flame_fast_speed) {
        expire_timer = s_showrunner_config.thwomp_flame_fast_lifetime;
    }
    return timer > expire_timer;
}

uint8_t mb64_showrunner_thwomp_flame_should_delete(int timer) {
    return timer > s_showrunner_config.thwomp_flame_shrink_timer;
}

float mb64_showrunner_thwomp_flame_scale(int timer) {
    return s_showrunner_config.thwomp_flame_scale -
        (s_showrunner_config.thwomp_flame_scale *
         ((float) timer / (float) s_showrunner_config.thwomp_flame_shrink_timer));
}

uint8_t mb64_showrunner_cosmic_projectile_should_delete(int timer, uint8_t hit_wall) {
    return timer > s_showrunner_config.cosmic_projectile_delete_timer || hit_wall;
}

const mb64_motos_config_t *mb64_motos_config(void) {
    return &s_motos_config;
}

uint8_t mb64_motos_should_search(float distance_to_mario) {
    return distance_to_mario < s_motos_config.wait_search_distance;
}

uint8_t mb64_motos_should_stop_searching(float distance_to_mario) {
    return distance_to_mario > s_motos_config.search_drop_distance;
}

uint8_t mb64_motos_should_throw(int timer, uint8_t hit_edge) {
    return timer > s_motos_config.throw_timer || hit_edge;
}

uint8_t mb64_motos_escape_succeeds(int escape_actions) {
    return escape_actions > s_motos_config.escape_actions;
}

uint8_t mb64_motos_should_leave_recover_wait(int timer) {
    return timer > s_motos_config.recover_wait_timer;
}

const mb64_chicken_config_t *mb64_chicken_config(void) {
    return &s_chicken_config;
}

const mb64_crablet_config_t *mb64_crablet_config(void) {
    return &s_crablet_config;
}

uint8_t mb64_crablet_should_attack(int angle_diff, float distance_to_mario) {
    return angle_diff < s_crablet_config.attack_angle_threshold &&
        distance_to_mario < s_crablet_config.attack_distance;
}

uint8_t mb64_crablet_should_end_attack(int timer) {
    return timer > s_crablet_config.attack_timer_limit;
}

uint8_t mb64_crablet_should_finish_recovery(int timer) {
    return timer > s_crablet_config.recover_timer_limit;
}

uint8_t mb64_crablet_should_grab_head(float distance_to_mario, float crablet_y, float mario_y, uint8_t already_grabbed) {
    return !already_grabbed &&
        distance_to_mario < s_crablet_config.head_grab_distance &&
        crablet_y - s_crablet_config.head_grab_y_offset > mario_y;
}

int mb64_crablet_hurt_quicksand_depth(int quicksand_depth) {
    int next_depth = quicksand_depth - s_crablet_config.hurt_quicksand_depth_step;
    if (next_depth < 0) {
        return 0;
    }
    if (next_depth > 255) {
        return 255;
    }
    return next_depth;
}

const mb64_fire_bro_config_t *mb64_fire_bro_config(void) {
    return &s_fire_bro_config;
}

uint8_t mb64_fire_bro_can_throw(float mario_y, float bro_y) {
    return mario_y > bro_y + s_fire_bro_config.mario_min_y_offset;
}

uint8_t mb64_fire_bro_should_start_throw(float distance_to_mario, int timer) {
    return distance_to_mario <= s_fire_bro_config.activation_distance &&
        timer > s_fire_bro_config.throw_start_frame;
}

uint8_t mb64_fire_bro_should_leave_hold(int timer) {
    return timer > s_fire_bro_config.hold_frame_limit;
}

uint8_t mb64_fire_bro_should_repeat_or_jump(int timer) {
    return timer > s_fire_bro_config.repeat_frame_limit;
}

uint8_t mb64_fire_bro_should_delete_fireball(int timer, uint8_t hit_wall) {
    return timer > s_fire_bro_config.fireball_timeout_frame || hit_wall;
}

uint8_t mb64_fire_bro_should_bounce_fireball(uint32_t move_flags) {
    return (move_flags & 3u) != 0;
}

const mb64_hammer_bro_config_t *mb64_hammer_bro_config(void) {
    return &s_hammer_bro_config;
}

uint8_t mb64_hammer_bro_can_throw(float mario_y, float bro_y) {
    return mario_y > bro_y + s_hammer_bro_config.mario_min_y_offset;
}

uint8_t mb64_hammer_bro_should_start_throw(float distance_to_mario, int timer) {
    return distance_to_mario <= s_hammer_bro_config.activation_distance &&
        timer > s_hammer_bro_config.throw_start_frame;
}

uint8_t mb64_hammer_bro_should_leave_hold(int timer) {
    return timer > s_hammer_bro_config.hold_frame_limit;
}

uint8_t mb64_hammer_bro_should_repeat_or_jump(int timer) {
    return timer > s_hammer_bro_config.repeat_frame_limit;
}

uint16_t mb64_hammer_bro_random_range(uint16_t random, uint16_t min, uint16_t max) {
    if (max <= min) {
        return min;
    }
    return (uint16_t)(min + (random % (uint16_t)(max - min + 1)));
}

int mb64_hammer_bro_hurt_quicksand_depth(int quicksand_depth) {
    int next_depth = quicksand_depth - s_hammer_bro_config.quicksand_depth_step;
    if (next_depth < 0) {
        return 0;
    }
    if (next_depth > 255) {
        return 255;
    }
    return next_depth;
}

float mb64_hammer_bro_projectile_y_offset(float quicksand_depth) {
    return s_hammer_bro_config.projectile_y_offset -
        quicksand_depth * s_hammer_bro_config.projectile_quicksand_y_scale;
}

uint8_t mb64_hammer_should_arm_hitbox(int timer) {
    return timer >= s_hammer_bro_config.hammer_ready_frame;
}

uint8_t mb64_hammer_should_delete(int timer, uint32_t move_flags, uint8_t attacked, uint8_t interacted) {
    return attacked || interacted ||
        (move_flags & 3u) != 0 ||
        (move_flags & (1u << 9)) != 0 ||
        timer > s_hammer_bro_config.hammer_timeout_frame;
}

const mb64_rex_config_t *mb64_rex_config(void) {
    return &s_rex_config;
}

const mb64_npc_config_t *mb64_moleman_config(void) {
    return &s_moleman_config;
}

const mb64_npc_config_t *mb64_cobie_config(void) {
    return &s_cobie_config;
}

const mb64_npc_config_t *mb64_toad_config(void) {
    return &s_toad_config;
}

const mb64_npc_config_t *mb64_tuxie_config(void) {
    return &s_tuxie_config;
}

const mb64_npc_config_t *mb64_ukiki_config(void) {
    return &s_ukiki_config;
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
