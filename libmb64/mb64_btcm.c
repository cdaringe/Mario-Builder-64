#include "mb64_btcm.h"

static const mb64_btcm_config_t s_btcm_config = {
    8,      /* numMaxHP */
    8,      /* numMaxFP */
    2295,   /* 255 + 255 * numMaxHP */
    0x40,   /* normal hurt-counter health tick */
    0x80,   /* Brittle burden health tick */
    201,    /* source updates when regentime > 200 */
    451,    /* source updates when mario_decay > 450 */
    226,    /* Brittle updates when mario_decay > 225 */
};

/* Native render_hud_power_meter uses 16x16 V/X/Z glyphs at these positions. */
static const mb64_btcm_hud_spec_t s_btcm_hud_spec = {
    16, /* custom HP/BP/empty glyph size */
    8,  /* overlapping glyph stride */
    22, /* GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE */
    30, /* native print_text health baseline */
    20, /* native print_text mana baseline */
};

const mb64_btcm_config_t *mb64_btcm_config(void) {
    return &s_btcm_config;
}

const mb64_btcm_hud_spec_t *mb64_btcm_hud_spec(void) {
    return &s_btcm_hud_spec;
}

uint8_t mb64_btcm_badge_is_equipped(uint32_t badges, mb64_btcm_badge_t badge) {
    return badge < MB64_BTCM_BADGE_COUNT && (badges & (1u << badge)) != 0;
}

uint8_t mb64_btcm_health_segments(uint16_t health) {
    unsigned int segments = health > 0 ? health >> 8 : 0;
    return segments > s_btcm_config.max_health ? s_btcm_config.max_health : (uint8_t)segments;
}

uint8_t mb64_btcm_clamp_mana(unsigned int mana) {
    return mana > s_btcm_config.max_mana ? s_btcm_config.max_mana : (uint8_t)mana;
}

mb64_btcm_hurt_result_t mb64_btcm_apply_hurt_tick(
    uint16_t health, uint8_t mana, uint8_t hurt_counter, uint32_t badges) {
    mb64_btcm_hurt_result_t result = { health, mb64_btcm_clamp_mana(mana), hurt_counter };
    if (result.hurt_counter == 0) {
        return result;
    }

    if (mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_DEFENSE) && result.mana > 0) {
        if (result.hurt_counter > 3) {
            result.mana--;
            result.hurt_counter -= 3;
        }
    } else {
        const uint16_t tick = mb64_btcm_badge_is_equipped(
            badges, MB64_BTCM_BADGE_BRITTLE)
            ? s_btcm_config.brittle_health_tick
            : s_btcm_config.health_tick;
        result.health = result.health > tick ? result.health - tick : 0;
    }
    result.hurt_counter--;
    return result;
}

float mb64_btcm_fins_breaststroke_acceleration(uint16_t action_timer,
                                               uint32_t badges) {
    if (!mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_FINS)) {
        return 0.0f;
    }
    if (action_timer < 6) {
        return 10.0f;
    }
    return action_timer >= 9 ? 30.0f : 0.0f;
}

uint16_t mb64_btcm_fins_swim_strength(uint32_t badges, uint16_t fallback) {
    return mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_FINS)
        ? 1000 : fallback;
}

float mb64_btcm_fins_flutter_speed(uint32_t badges, float fallback) {
    return mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_FINS)
        ? 200.0f : fallback;
}

uint8_t mb64_btcm_squish_hurt_delta(uint32_t badges, uint8_t cap_on) {
    if (mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_SQUISH)) {
        return 0;
    }
    return cap_on ? 12 : 18;
}

uint8_t mb64_btcm_coin_magnet_enabled(uint32_t badges) {
    return mb64_btcm_badge_is_equipped(badges, MB64_BTCM_BADGE_MAGNET);
}
