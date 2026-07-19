/** Portable Beyond the Cursed Mirror gameplay semantics. */
#ifndef MB64_BTCM_H
#define MB64_BTCM_H

#include <stdint.h>

typedef enum {
    MB64_BTCM_BADGE_LAVA = 0,
    MB64_BTCM_BADGE_FALL,
    MB64_BTCM_BADGE_DEFENSE,
    MB64_BTCM_BADGE_DAMAGE,
    MB64_BTCM_BADGE_GILLS,
    MB64_BTCM_BADGE_FINS,
    MB64_BTCM_BADGE_HP,
    MB64_BTCM_BADGE_MANA,
    MB64_BTCM_BADGE_GREED,
    MB64_BTCM_BADGE_TIME,
    MB64_BTCM_BADGE_MAGNET,
    MB64_BTCM_BADGE_BURN,
    MB64_BTCM_BADGE_SQUISH,
    MB64_BTCM_BADGE_FEATHER,
    MB64_BTCM_BADGE_WEIGHT,
    MB64_BTCM_BADGE_STICKY,
    MB64_BTCM_BADGE_FEET,
    MB64_BTCM_BADGE_HEAL,
    MB64_BTCM_BADGE_BOTTOMLESS,
    MB64_BTCM_BADGE_SLOWFALL,
    MB64_BTCM_BADGE_BRITTLE,
    MB64_BTCM_BADGE_WITHER,
    MB64_BTCM_BADGE_HARDCORE,
    MB64_BTCM_BADGE_COUNT,
} mb64_btcm_badge_t;

typedef struct {
    uint8_t max_health;
    uint8_t max_mana;
    uint16_t initial_health;
    uint16_t health_tick;
    uint16_t brittle_health_tick;
    uint16_t regen_interval_frames;
    uint16_t wither_interval_frames;
    uint16_t brittle_wither_interval_frames;
} mb64_btcm_config_t;

typedef struct {
    uint8_t glyph_size;
    uint8_t segment_spacing;
    uint8_t left_edge;
    uint8_t health_baseline;
    uint8_t mana_baseline;
} mb64_btcm_hud_spec_t;

typedef struct {
    uint16_t health;
    uint8_t mana;
    uint8_t hurt_counter;
} mb64_btcm_hurt_result_t;

const mb64_btcm_config_t *mb64_btcm_config(void);
const mb64_btcm_hud_spec_t *mb64_btcm_hud_spec(void);
uint8_t mb64_btcm_badge_is_equipped(uint32_t badges, mb64_btcm_badge_t badge);
uint8_t mb64_btcm_health_segments(uint16_t health);
uint8_t mb64_btcm_clamp_mana(unsigned int mana);
mb64_btcm_hurt_result_t mb64_btcm_apply_hurt_tick(
    uint16_t health, uint8_t mana, uint8_t hurt_counter, uint32_t badges);
float mb64_btcm_fins_breaststroke_acceleration(uint16_t action_timer,
                                               uint32_t badges);
uint16_t mb64_btcm_fins_swim_strength(uint32_t badges, uint16_t fallback);
float mb64_btcm_fins_flutter_speed(uint32_t badges, float fallback);
uint8_t mb64_btcm_squish_hurt_delta(uint32_t badges, uint8_t cap_on);
uint8_t mb64_btcm_coin_magnet_enabled(uint32_t badges);

#endif
