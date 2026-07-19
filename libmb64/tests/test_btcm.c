#include <assert.h>

#include "mb64_btcm.h"

int main(void) {
    const mb64_btcm_config_t *config = mb64_btcm_config();
    assert(config->max_health == 8);
    assert(config->max_mana == 8);
    assert(config->wither_interval_frames == 451);
    assert(config->brittle_wither_interval_frames == 226);
    assert(mb64_btcm_health_segments(config->initial_health) == 8);
    assert(mb64_btcm_health_segments(0x480) == 4);
    assert(mb64_btcm_clamp_mana(99) == 8);
    assert(mb64_btcm_badge_is_equipped(1u << MB64_BTCM_BADGE_DEFENSE,
                                       MB64_BTCM_BADGE_DEFENSE));

    mb64_btcm_hurt_result_t normal = mb64_btcm_apply_hurt_tick(0x880, 8, 8, 0);
    assert(normal.health == 0x840);
    assert(normal.mana == 8);
    assert(normal.hurt_counter == 7);

    mb64_btcm_hurt_result_t defense = mb64_btcm_apply_hurt_tick(
        0x880, 8, 8, 1u << MB64_BTCM_BADGE_DEFENSE);
    assert(defense.health == 0x880);
    assert(defense.mana == 7);
    assert(defense.hurt_counter == 4);

    mb64_btcm_hurt_result_t brittle = mb64_btcm_apply_hurt_tick(
        0x880, 8, 8, 1u << MB64_BTCM_BADGE_BRITTLE);
    assert(brittle.health == 0x800);
    assert(brittle.mana == 8);
    assert(brittle.hurt_counter == 7);

    const uint32_t holland_badges =
        (1u << MB64_BTCM_BADGE_FINS) |
        (1u << MB64_BTCM_BADGE_MAGNET) |
        (1u << MB64_BTCM_BADGE_SQUISH);
    assert(mb64_btcm_fins_breaststroke_acceleration(5, holland_badges) == 10.0f);
    assert(mb64_btcm_fins_breaststroke_acceleration(6, holland_badges) == 0.0f);
    assert(mb64_btcm_fins_breaststroke_acceleration(8, holland_badges) == 0.0f);
    assert(mb64_btcm_fins_breaststroke_acceleration(9, holland_badges) == 30.0f);
    assert(mb64_btcm_fins_swim_strength(holland_badges, 160) == 1000);
    assert(mb64_btcm_fins_flutter_speed(holland_badges, 12.0f) == 200.0f);
    assert(mb64_btcm_squish_hurt_delta(holland_badges, 1) == 0);
    assert(mb64_btcm_squish_hurt_delta(0, 1) == 12);
    assert(mb64_btcm_squish_hurt_delta(0, 0) == 18);
    assert(mb64_btcm_coin_magnet_enabled(holland_badges));
    return 0;
}
