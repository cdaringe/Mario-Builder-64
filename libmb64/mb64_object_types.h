/**
 * mb64_object_types.h - Public MB64 object ids.
 *
 * These ids mirror Mario Builder 64's src/mb64/data.h object type table. Keep
 * them in libmb64 so consumers index by the same save-format ids instead of
 * maintaining private copies.
 */

#ifndef MB64_OBJECT_TYPES_H
#define MB64_OBJECT_TYPES_H

typedef enum {
    MB64_OBJECT_TYPE_SETTINGS = 0,
    MB64_OBJECT_TYPE_1,
    MB64_OBJECT_TYPE_STAR,
    MB64_OBJECT_TYPE_RED_COIN_STAR,
    MB64_OBJECT_TYPE_GOOMBA,
    MB64_OBJECT_TYPE_BIG_GOOMBA,
    MB64_OBJECT_TYPE_TINY_GOOMBA,
    MB64_OBJECT_TYPE_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_BIG_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_TINY_PIRANHA_PLANT,
    MB64_OBJECT_TYPE_KOOPA,
    MB64_OBJECT_TYPE_COIN,
    MB64_OBJECT_TYPE_GREEN_COIN,
    MB64_OBJECT_TYPE_RED_COIN,
    MB64_OBJECT_TYPE_BLUE_COIN,
    MB64_OBJECT_TYPE_BLUE_COIN_SWITCH,
    MB64_OBJECT_TYPE_NOTEBLOCK,
    MB64_OBJECT_TYPE_BOBOMB,
    MB64_OBJECT_TYPE_CHUCKYA,
    MB64_OBJECT_TYPE_BULLY,
    MB64_OBJECT_TYPE_CHILL_BULLY,
    MB64_OBJECT_TYPE_BULLET_BILL,
    MB64_OBJECT_TYPE_HEAVE_HO,
    MB64_OBJECT_TYPE_MOTOS,
    MB64_OBJECT_TYPE_TREE,
    MB64_OBJECT_TYPE_EXCL_BOX,
    MB64_OBJECT_TYPE_MARIO_SPAWN,
    MB64_OBJECT_TYPE_REX,
    MB64_OBJECT_TYPE_PODOBOO,
    MB64_OBJECT_TYPE_CRABLET,
    MB64_OBJECT_TYPE_HAMMER_BRO,
    MB64_OBJECT_TYPE_FIRE_BRO,
    MB64_OBJECT_TYPE_CHICKEN,
    MB64_OBJECT_TYPE_PHANTASM,
    MB64_OBJECT_TYPE_WARP_PIPE,
    MB64_OBJECT_TYPE_BADGE,
    MB64_OBJECT_TYPE_KING_BOBOMB,
    MB64_OBJECT_TYPE_KING_WHOMP,
    MB64_OBJECT_TYPE_BIG_BOO,
    MB64_OBJECT_TYPE_BIG_BULLY,
    MB64_OBJECT_TYPE_BIG_CHILL_BULLY,
    MB64_OBJECT_TYPE_WIGGLER,
    MB64_OBJECT_TYPE_BOWSER,
    MB64_OBJECT_TYPE_PLATFORM_TRACK,
    MB64_OBJECT_TYPE_PLATFORM_LOOPING,
    MB64_OBJECT_TYPE_BOWLING_BALL,
    MB64_OBJECT_TYPE_KOOPA_THE_QUICK,
    MB64_OBJECT_TYPE_PURPLE_SWITCH,
    MB64_OBJECT_TYPE_TIMED_BOX,
    MB64_OBJECT_TYPE_RECOVERY_HEART,
    MB64_OBJECT_TYPE_TEST_MARIO,
    MB64_OBJECT_TYPE_THWOMP,
    MB64_OBJECT_TYPE_WHOMP,
    MB64_OBJECT_TYPE_GRINDEL,
    MB64_OBJECT_TYPE_LAKITU,
    MB64_OBJECT_TYPE_FLY_GUY,
    MB64_OBJECT_TYPE_SNUFIT,
    MB64_OBJECT_TYPE_AMP,
    MB64_OBJECT_TYPE_BOO,
    MB64_OBJECT_TYPE_MR_I,
    MB64_OBJECT_TYPE_SCUTTLEBUG,
    MB64_OBJECT_TYPE_BOWSER_BOMB,
    MB64_OBJECT_TYPE_FIRE_SPINNER,
    MB64_OBJECT_TYPE_COIN_FORMATION,
    MB64_OBJECT_TYPE_RED_FLAME,
    MB64_OBJECT_TYPE_BLUE_FLAME,
    MB64_OBJECT_TYPE_FIRE_SPITTER,
    MB64_OBJECT_TYPE_FLAMETHROWER,
    MB64_OBJECT_TYPE_SPINDRIFT,
    MB64_OBJECT_TYPE_MR_BLIZZARD,
    MB64_OBJECT_TYPE_MONEYBAG,
    MB64_OBJECT_TYPE_SKEETER,
    MB64_OBJECT_TYPE_POKEY,
    MB64_OBJECT_TYPE_BBOX_SMALL,
    MB64_OBJECT_TYPE_BBOX_NORMAL,
    MB64_OBJECT_TYPE_BBOX_CRAZY,
    MB64_OBJECT_TYPE_DIAMOND,
    MB64_OBJECT_TYPE_SIGN,
    MB64_OBJECT_TYPE_BUDDY,
    MB64_OBJECT_TYPE_BUTTON,
    MB64_OBJECT_TYPE_ON_OFF_BLOCK,
    MB64_OBJECT_TYPE_WOODPLAT,
    MB64_OBJECT_TYPE_RFBOX,
    MB64_OBJECT_TYPE_CULL_PREVIEW,
    MB64_OBJECT_TYPE_SHOWRUNNER,
    MB64_OBJECT_TYPE_CROWBAR,
    MB64_OBJECT_TYPE_MASK,
    MB64_OBJECT_TYPE_TOAD,
    MB64_OBJECT_TYPE_TUXIE,
    MB64_OBJECT_TYPE_UKIKI,
    MB64_OBJECT_TYPE_MOLEMAN,
    MB64_OBJECT_TYPE_COBIE,
    MB64_OBJECT_TYPE_CONVEYOR,
    MB64_OBJECT_TYPE_TIMEDBLOCK,
    MB64_OBJECT_TYPE_TRIGGER,
    MB64_OBJECT_TYPE_TRIGGER_STAR,
    MB64_OBJECT_TYPE_COUNT,
} mb64_object_type_t;

/* Runtime descendants spawned after the authored parent exists. NONE records
 * an intentional nonvisual helper; every other token names its render model. */
typedef enum {
    MB64_CHILD_MODEL_NONE = 0,
    MB64_CHILD_MODEL_BOWLING_BALL,
    MB64_CHILD_MODEL_SPINY_BALL,
    MB64_CHILD_MODEL_WIGGLER_BODY,
    MB64_CHILD_MODEL_MR_I_IRIS,
    MB64_CHILD_MODEL_TRANSPARENT_STAR,
    MB64_CHILD_MODEL_STAR,
    MB64_CHILD_MODEL_BULLET_BILL,
    MB64_CHILD_MODEL_SMOKE,
    MB64_CHILD_MODEL_MIST,
    MB64_CHILD_MODEL_EXPLOSION,
    MB64_CHILD_MODEL_IDLE_WATER_WAVE,
    MB64_CHILD_MODEL_WHITE_PARTICLE,
    MB64_CHILD_MODEL_PURPLE_MARBLE,
    MB64_CHILD_MODEL_NUMBER,
    MB64_CHILD_MODEL_SPARKLES_ANIMATION,
    MB64_CHILD_MODEL_SPARKLES,
    MB64_CHILD_MODEL_RED_FLAME_SHADOW,
    MB64_CHILD_MODEL_DIRT_ANIMATION,
} mb64_child_model_t;

typedef struct {
    mb64_child_model_t model;
    unsigned char runtime_spawn;
} mb64_child_model_dependency_t;

static inline const char *mb64_child_model_name(mb64_child_model_t model) {
    switch (model) {
        case MB64_CHILD_MODEL_BOWLING_BALL: return "bowling_ball";
        case MB64_CHILD_MODEL_SPINY_BALL: return "spiny_ball";
        case MB64_CHILD_MODEL_WIGGLER_BODY: return "wiggler_body";
        case MB64_CHILD_MODEL_MR_I_IRIS: return "mr_i_iris";
        case MB64_CHILD_MODEL_TRANSPARENT_STAR: return "transparent_star";
        case MB64_CHILD_MODEL_STAR: return "star";
        case MB64_CHILD_MODEL_BULLET_BILL: return "bullet_bill";
        case MB64_CHILD_MODEL_SMOKE: return "smoke";
        case MB64_CHILD_MODEL_MIST: return "mist";
        case MB64_CHILD_MODEL_EXPLOSION: return "explosion";
        case MB64_CHILD_MODEL_IDLE_WATER_WAVE: return "idle_water_wave";
        case MB64_CHILD_MODEL_WHITE_PARTICLE: return "white_particle";
        case MB64_CHILD_MODEL_PURPLE_MARBLE: return "purple_marble";
        case MB64_CHILD_MODEL_NUMBER: return "number";
        case MB64_CHILD_MODEL_SPARKLES_ANIMATION: return "sparkles_animation";
        case MB64_CHILD_MODEL_SPARKLES: return "sparkles";
        case MB64_CHILD_MODEL_RED_FLAME_SHADOW: return "red_flame_shadow";
        case MB64_CHILD_MODEL_DIRT_ANIMATION: return "dirt_animation";
        case MB64_CHILD_MODEL_NONE:
        default: return "none";
    }
}

/* Editor preview display functions are distinct from the model spawned while
 * playing a level. Timed Box previews use MODEL_MAKER_TIMEDBOX, while runtime
 * objects retain the object-table breakable-box model. */
typedef enum {
    MB64_OBJECT_DISPLAY_DEFAULT = 0,
    MB64_OBJECT_DISPLAY_TIMED_BOX,
} mb64_object_display_t;

typedef struct {
    mb64_object_display_t runtime;
    mb64_object_display_t preview;
} mb64_object_display_spec_t;

static inline mb64_object_display_spec_t
mb64_object_display_spec_for_type(unsigned char type) {
    mb64_object_display_spec_t spec = {
        MB64_OBJECT_DISPLAY_DEFAULT,
        MB64_OBJECT_DISPLAY_DEFAULT,
    };
    if (type == MB64_OBJECT_TYPE_TIMED_BOX) {
        spec.preview = MB64_OBJECT_DISPLAY_TIMED_BOX;
    }
    return spec;
}

typedef enum {
    MB64_TIMED_BOX_ACTION_HIDDEN = 0,
    MB64_TIMED_BOX_ACTION_ACTIVE,
    MB64_TIMED_BOX_ACTION_BROKEN,
} mb64_timed_box_action_t;

static inline const mb64_child_model_dependency_t *
mb64_object_child_model_dependency(unsigned char type, unsigned char index) {
    static const mb64_child_model_dependency_t bowling_ball[] = {
        { MB64_CHILD_MODEL_BOWLING_BALL, 1 },
    };
    static const mb64_child_model_dependency_t lakitu[] = {
        { MB64_CHILD_MODEL_SPINY_BALL, 1 },
    };
    static const mb64_child_model_dependency_t wiggler[] = {
        { MB64_CHILD_MODEL_WIGGLER_BODY, 0 },
    };
    static const mb64_child_model_dependency_t mr_i[] = {
        { MB64_CHILD_MODEL_MR_I_IRIS, 1 },
        { MB64_CHILD_MODEL_PURPLE_MARBLE, 1 },
    };
    static const mb64_child_model_dependency_t red_coin_star[] = {
        { MB64_CHILD_MODEL_TRANSPARENT_STAR, 1 },
        { MB64_CHILD_MODEL_STAR, 1 },
        { MB64_CHILD_MODEL_NUMBER, 1 },
    };
    static const mb64_child_model_dependency_t bullet_bill[] = {
        { MB64_CHILD_MODEL_BULLET_BILL, 1 },
        { MB64_CHILD_MODEL_SMOKE, 1 },
        { MB64_CHILD_MODEL_MIST, 1 },
        { MB64_CHILD_MODEL_EXPLOSION, 1 },
    };
    static const mb64_child_model_dependency_t skeeter[] = {
        { MB64_CHILD_MODEL_IDLE_WATER_WAVE, 1 },
    };
    static const mb64_child_model_dependency_t mr_blizzard[] = {
        { MB64_CHILD_MODEL_WHITE_PARTICLE, 1 },
    };
    static const mb64_child_model_dependency_t number[] = {
        { MB64_CHILD_MODEL_NUMBER, 1 },
    };
    static const mb64_child_model_dependency_t flamethrower[] = {
        { MB64_CHILD_MODEL_MIST, 1 },
    };
    static const mb64_child_model_dependency_t fire_spitter[] = {
        { MB64_CHILD_MODEL_RED_FLAME_SHADOW, 1 },
    };
    static const mb64_child_model_dependency_t wooden_platform[] = {
        { MB64_CHILD_MODEL_NONE, 0 },
    };
    const mb64_child_model_dependency_t *dependencies = 0;
    unsigned char count = 0;
    switch (type) {
        case MB64_OBJECT_TYPE_SNUFIT:
            dependencies = bowling_ball;
            count = sizeof(bowling_ball) / sizeof(bowling_ball[0]);
            break;
        case MB64_OBJECT_TYPE_BOWLING_BALL:
            dependencies = bowling_ball;
            count = sizeof(bowling_ball) / sizeof(bowling_ball[0]);
            break;
        case MB64_OBJECT_TYPE_LAKITU:
            dependencies = lakitu;
            count = sizeof(lakitu) / sizeof(lakitu[0]);
            break;
        case MB64_OBJECT_TYPE_WIGGLER:
            dependencies = wiggler;
            count = sizeof(wiggler) / sizeof(wiggler[0]);
            break;
        case MB64_OBJECT_TYPE_MR_I:
            dependencies = mr_i;
            count = sizeof(mr_i) / sizeof(mr_i[0]);
            break;
        case MB64_OBJECT_TYPE_RED_COIN_STAR:
            dependencies = red_coin_star;
            count = sizeof(red_coin_star) / sizeof(red_coin_star[0]);
            break;
        case MB64_OBJECT_TYPE_BULLET_BILL:
            dependencies = bullet_bill;
            count = sizeof(bullet_bill) / sizeof(bullet_bill[0]);
            break;
        case MB64_OBJECT_TYPE_SKEETER:
            dependencies = skeeter;
            count = sizeof(skeeter) / sizeof(skeeter[0]);
            break;
        case MB64_OBJECT_TYPE_MR_BLIZZARD:
            dependencies = mr_blizzard;
            count = sizeof(mr_blizzard) / sizeof(mr_blizzard[0]);
            break;
        case MB64_OBJECT_TYPE_STAR:
        case MB64_OBJECT_TYPE_BLUE_COIN_SWITCH:
            dependencies = number;
            count = sizeof(number) / sizeof(number[0]);
            break;
        case MB64_OBJECT_TYPE_FLAMETHROWER:
            dependencies = flamethrower;
            count = sizeof(flamethrower) / sizeof(flamethrower[0]);
            break;
        case MB64_OBJECT_TYPE_FIRE_SPITTER:
            dependencies = fire_spitter;
            count = sizeof(fire_spitter) / sizeof(fire_spitter[0]);
            break;
        case MB64_OBJECT_TYPE_WOODPLAT:
            dependencies = wooden_platform;
            count = sizeof(wooden_platform) / sizeof(wooden_platform[0]);
            break;
        default:
            break;
    }
    return index < count ? &dependencies[index] : 0;
}

/* Loot and imbue effects are transitive descendants of any authored object
 * that can release them, so they are cataloged once instead of copied into
 * every eligible object type. */
static inline const mb64_child_model_dependency_t *
mb64_object_runtime_effect_model_dependency(unsigned char index) {
    static const mb64_child_model_dependency_t dependencies[] = {
        { MB64_CHILD_MODEL_SPARKLES, 1 },
        { MB64_CHILD_MODEL_SPARKLES_ANIMATION, 1 },
        { MB64_CHILD_MODEL_MIST, 1 },
        { MB64_CHILD_MODEL_TRANSPARENT_STAR, 1 },
        { MB64_CHILD_MODEL_SMOKE, 1 },
        { MB64_CHILD_MODEL_DIRT_ANIMATION, 1 },
    };
    const unsigned char count = sizeof(dependencies) / sizeof(dependencies[0]);
    return index < count ? &dependencies[index] : 0;
}

#define MB64_OBJECT_TYPE_SPAWN MB64_OBJECT_TYPE_MARIO_SPAWN
#define MB64_OBJECT_TYPE_TIMED_BLOCK MB64_OBJECT_TYPE_TIMEDBLOCK

/*
 * Runtime networking policy exported for ports that instantiate MB64 objects
 * in a multiplayer engine. Behavior-specific fields remain owned by the
 * behavior; these flags describe the common state and child lifecycle that a
 * port must preserve for periodic updates and late-join snapshots.
 */
typedef enum {
    MB64_OBJECT_NETWORK_SKIP = 0,
    MB64_OBJECT_NETWORK_STATIC,
    MB64_OBJECT_NETWORK_COLLECTIBLE,
    MB64_OBJECT_NETWORK_EVENT,
    MB64_OBJECT_NETWORK_CONTINUOUS,
    MB64_OBJECT_NETWORK_CONTROLLER,
} mb64_object_network_policy_t;

enum {
    MB64_OBJECT_NETWORK_FIELD_VISUAL      = 1u << 0,
    MB64_OBJECT_NETWORK_FIELD_ORIENTATION = 1u << 1,
    MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS  = 1u << 2,
    MB64_OBJECT_NETWORK_FIELD_HEALTH      = 1u << 3,
};

enum {
    MB64_OBJECT_NETWORK_CHILDREN_NONE          = 0,
    MB64_OBJECT_NETWORK_CHILDREN_INITIAL       = 1u << 0,
    MB64_OBJECT_NETWORK_CHILDREN_RUNTIME_SPAWN = 1u << 1,
    MB64_OBJECT_NETWORK_CHILDREN_RIDABLE       = 1u << 2,
    MB64_OBJECT_NETWORK_CHILDREN_DERIVED       = 1u << 3,
};

typedef struct {
    mb64_object_network_policy_t policy;
    unsigned char common_fields;
    unsigned char child_policy;
    unsigned char collection_identity;
    unsigned char late_join_snapshot;
} mb64_object_network_descriptor_t;

static inline const mb64_object_network_descriptor_t *
mb64_object_network_descriptor_for_type(unsigned char type) {
    static const mb64_object_network_descriptor_t skip = {
        MB64_OBJECT_NETWORK_SKIP, 0, MB64_OBJECT_NETWORK_CHILDREN_NONE, 0, 0,
    };
    static const mb64_object_network_descriptor_t static_object = {
        MB64_OBJECT_NETWORK_STATIC, 0, MB64_OBJECT_NETWORK_CHILDREN_NONE, 0, 1,
    };
    static const mb64_object_network_descriptor_t collectible = {
        MB64_OBJECT_NETWORK_COLLECTIBLE,
        MB64_OBJECT_NETWORK_FIELD_VISUAL,
        MB64_OBJECT_NETWORK_CHILDREN_NONE, 1, 1,
    };
    static const mb64_object_network_descriptor_t event = {
        MB64_OBJECT_NETWORK_EVENT,
        MB64_OBJECT_NETWORK_FIELD_VISUAL | MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_NONE, 0, 1,
    };
    static const mb64_object_network_descriptor_t continuous = {
        MB64_OBJECT_NETWORK_CONTINUOUS,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_NONE, 0, 1,
    };
    static const mb64_object_network_descriptor_t controller = {
        MB64_OBJECT_NETWORK_CONTROLLER,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_RUNTIME_SPAWN, 0, 1,
    };
    static const mb64_object_network_descriptor_t controller_derived = {
        MB64_OBJECT_NETWORK_CONTROLLER,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_RUNTIME_SPAWN |
            MB64_OBJECT_NETWORK_CHILDREN_DERIVED,
        0, 1,
    };
    static const mb64_object_network_descriptor_t derived = {
        MB64_OBJECT_NETWORK_CONTINUOUS,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_DERIVED, 0, 1,
    };
    static const mb64_object_network_descriptor_t initial_children = {
        MB64_OBJECT_NETWORK_CONTROLLER,
        MB64_OBJECT_NETWORK_FIELD_VISUAL,
        MB64_OBJECT_NETWORK_CHILDREN_INITIAL, 0, 1,
    };
    static const mb64_object_network_descriptor_t initial_controller = {
        MB64_OBJECT_NETWORK_CONTROLLER,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_INITIAL, 0, 1,
    };
    static const mb64_object_network_descriptor_t koopa = {
        MB64_OBJECT_NETWORK_CONTINUOUS,
        MB64_OBJECT_NETWORK_FIELD_VISUAL |
            MB64_OBJECT_NETWORK_FIELD_ORIENTATION |
            MB64_OBJECT_NETWORK_FIELD_MOVE_FLAGS |
            MB64_OBJECT_NETWORK_FIELD_HEALTH,
        MB64_OBJECT_NETWORK_CHILDREN_RUNTIME_SPAWN |
            MB64_OBJECT_NETWORK_CHILDREN_DERIVED |
            MB64_OBJECT_NETWORK_CHILDREN_RIDABLE,
        0, 1,
    };

    switch (type) {
        case MB64_OBJECT_TYPE_SETTINGS:
        case MB64_OBJECT_TYPE_1:
        case MB64_OBJECT_TYPE_MARIO_SPAWN:
        case MB64_OBJECT_TYPE_TEST_MARIO:
        case MB64_OBJECT_TYPE_CULL_PREVIEW:
            return &skip;

        case MB64_OBJECT_TYPE_TREE:
        case MB64_OBJECT_TYPE_WARP_PIPE:
        case MB64_OBJECT_TYPE_RED_FLAME:
        case MB64_OBJECT_TYPE_BLUE_FLAME:
        case MB64_OBJECT_TYPE_SIGN:
        case MB64_OBJECT_TYPE_BUDDY:
        case MB64_OBJECT_TYPE_TOAD:
        case MB64_OBJECT_TYPE_TUXIE:
        case MB64_OBJECT_TYPE_UKIKI:
        case MB64_OBJECT_TYPE_MOLEMAN:
        case MB64_OBJECT_TYPE_COBIE:
            return &static_object;

        case MB64_OBJECT_TYPE_STAR:
        case MB64_OBJECT_TYPE_RED_COIN_STAR:
        case MB64_OBJECT_TYPE_COIN:
        case MB64_OBJECT_TYPE_GREEN_COIN:
        case MB64_OBJECT_TYPE_RED_COIN:
        case MB64_OBJECT_TYPE_BLUE_COIN:
        case MB64_OBJECT_TYPE_RECOVERY_HEART:
        case MB64_OBJECT_TYPE_BADGE:
        case MB64_OBJECT_TYPE_CROWBAR:
        case MB64_OBJECT_TYPE_MASK:
        case MB64_OBJECT_TYPE_TRIGGER_STAR:
            return &collectible;

        case MB64_OBJECT_TYPE_BLUE_COIN_SWITCH:
        case MB64_OBJECT_TYPE_NOTEBLOCK:
        case MB64_OBJECT_TYPE_PURPLE_SWITCH:
        case MB64_OBJECT_TYPE_TIMED_BOX:
        case MB64_OBJECT_TYPE_BBOX_SMALL:
        case MB64_OBJECT_TYPE_BBOX_NORMAL:
        case MB64_OBJECT_TYPE_BBOX_CRAZY:
        case MB64_OBJECT_TYPE_DIAMOND:
        case MB64_OBJECT_TYPE_BUTTON:
        case MB64_OBJECT_TYPE_ON_OFF_BLOCK:
        case MB64_OBJECT_TYPE_RFBOX:
        case MB64_OBJECT_TYPE_TIMEDBLOCK:
        case MB64_OBJECT_TYPE_TRIGGER:
            return &event;

        case MB64_OBJECT_TYPE_COIN_FORMATION:
            return &initial_children;

        case MB64_OBJECT_TYPE_KOOPA:
        case MB64_OBJECT_TYPE_KOOPA_THE_QUICK:
            return &koopa;

        case MB64_OBJECT_TYPE_BULLET_BILL:
            return &initial_controller;

        case MB64_OBJECT_TYPE_FIRE_SPINNER:
        case MB64_OBJECT_TYPE_WOODPLAT:
        case MB64_OBJECT_TYPE_MR_BLIZZARD:
        case MB64_OBJECT_TYPE_WIGGLER:
            return &derived;

        case MB64_OBJECT_TYPE_PIRANHA_PLANT:
        case MB64_OBJECT_TYPE_BIG_PIRANHA_PLANT:
        case MB64_OBJECT_TYPE_TINY_PIRANHA_PLANT:
        case MB64_OBJECT_TYPE_EXCL_BOX:
        case MB64_OBJECT_TYPE_BOWSER:
        case MB64_OBJECT_TYPE_BOWLING_BALL:
        case MB64_OBJECT_TYPE_FIRE_SPITTER:
        case MB64_OBJECT_TYPE_FLAMETHROWER:
        case MB64_OBJECT_TYPE_SHOWRUNNER:
        case MB64_OBJECT_TYPE_HAMMER_BRO:
        case MB64_OBJECT_TYPE_FIRE_BRO:
        case MB64_OBJECT_TYPE_PHANTASM:
        case MB64_OBJECT_TYPE_FLY_GUY:
        case MB64_OBJECT_TYPE_SNUFIT:
        case MB64_OBJECT_TYPE_MONEYBAG:
            return &controller;

        case MB64_OBJECT_TYPE_LAKITU:
        case MB64_OBJECT_TYPE_MR_I:
        case MB64_OBJECT_TYPE_POKEY:
        case MB64_OBJECT_TYPE_MOTOS:
            return &controller_derived;

        case MB64_OBJECT_TYPE_GOOMBA:
        case MB64_OBJECT_TYPE_BIG_GOOMBA:
        case MB64_OBJECT_TYPE_TINY_GOOMBA:
        case MB64_OBJECT_TYPE_BOBOMB:
        case MB64_OBJECT_TYPE_CHUCKYA:
        case MB64_OBJECT_TYPE_BULLY:
        case MB64_OBJECT_TYPE_CHILL_BULLY:
        case MB64_OBJECT_TYPE_HEAVE_HO:
        case MB64_OBJECT_TYPE_REX:
        case MB64_OBJECT_TYPE_PODOBOO:
        case MB64_OBJECT_TYPE_CRABLET:
        case MB64_OBJECT_TYPE_CHICKEN:
        case MB64_OBJECT_TYPE_KING_BOBOMB:
        case MB64_OBJECT_TYPE_KING_WHOMP:
        case MB64_OBJECT_TYPE_BIG_BOO:
        case MB64_OBJECT_TYPE_BIG_BULLY:
        case MB64_OBJECT_TYPE_BIG_CHILL_BULLY:
        case MB64_OBJECT_TYPE_PLATFORM_TRACK:
        case MB64_OBJECT_TYPE_PLATFORM_LOOPING:
        case MB64_OBJECT_TYPE_THWOMP:
        case MB64_OBJECT_TYPE_WHOMP:
        case MB64_OBJECT_TYPE_GRINDEL:
        case MB64_OBJECT_TYPE_AMP:
        case MB64_OBJECT_TYPE_BOO:
        case MB64_OBJECT_TYPE_SCUTTLEBUG:
        case MB64_OBJECT_TYPE_BOWSER_BOMB:
        case MB64_OBJECT_TYPE_SPINDRIFT:
        case MB64_OBJECT_TYPE_SKEETER:
        case MB64_OBJECT_TYPE_CONVEYOR:
            return &continuous;

        default:
            return 0;
    }
}

typedef enum {
    MB64_TREE_TYPE_BUBBLY = 0,
    MB64_TREE_TYPE_PALM,
    MB64_TREE_TYPE_SPIKY,
    MB64_TREE_TYPE_SNOWY,
    MB64_TREE_TYPE_FARM,
    MB64_TREE_TYPE_DEAD,
    MB64_TREE_TYPE_COUNT,
} mb64_tree_type_t;

typedef enum {
    MB64_ONOFF_COLOR_RED = 0,
    MB64_ONOFF_COLOR_BLUE,
    MB64_ONOFF_COLOR_COUNT,
} mb64_onoff_color_t;

typedef enum {
    MB64_WOODPLAT_THIN = 0,
    MB64_WOODPLAT_FULL,
    MB64_WOODPLAT_COUNT,
} mb64_woodplat_type_t;

typedef enum {
    MB64_POWERUP_TYPE_CROWBAR = 0,
    MB64_POWERUP_TYPE_BULLET_MASK,
    MB64_POWERUP_TYPE_COUNT,
} mb64_powerup_type_t;

typedef enum {
    MB64_FLOOR_SWITCH_TIMER_PERMANENT = 0,
    MB64_FLOOR_SWITCH_TIMER_PRESSURE,
} mb64_floor_switch_timer_t;

typedef enum {
    MB64_COIN_FORMATION_SHAPE_HORIZONTAL_LINE = 0,
    MB64_COIN_FORMATION_SHAPE_VERTICAL_LINE,
    MB64_COIN_FORMATION_SHAPE_HORIZONTAL_RING,
    MB64_COIN_FORMATION_SHAPE_VERTICAL_RING,
    MB64_COIN_FORMATION_SHAPE_ARROW,
    MB64_COIN_FORMATION_SHAPE_RESERVOIR,
    MB64_COIN_FORMATION_SHAPE_MASK = 0x07,
} mb64_coin_formation_shape_t;

typedef enum {
    MB64_IMBUE_NONE = 0,
    MB64_IMBUE_STAR,
    MB64_IMBUE_THREE_COINS,
    MB64_IMBUE_ONE_COIN,
    MB64_IMBUE_GREEN_COIN,
    MB64_IMBUE_BLUE_COIN,
    MB64_IMBUE_RED_SWITCH,
    MB64_IMBUE_BLUE_SWITCH,
    MB64_IMBUE_RED_COIN,
    MB64_IMBUE_TRIGGER,
    MB64_IMBUE_CROWBAR,
    MB64_IMBUE_BULLET_MASK,
    MB64_IMBUE_BADGE_BASE,
} mb64_imbue_type_t;

#endif
