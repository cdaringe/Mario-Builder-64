#include <assert.h>
#include <string.h>

#include "mb64_object_catalog.h"

int main(void) {
    unsigned int authored_reward_count = 0;
    assert(mb64_object_catalog_count() == MB64_OBJECT_TYPE_COUNT);
    for (unsigned int type = 0; type < mb64_object_catalog_count(); type++) {
        const mb64_object_spec_t *spec = mb64_object_catalog_get(type);
        assert(spec != 0);
        assert((unsigned int) spec->type == type);
        assert(spec->name != 0);
        assert(spec->behavior_token != 0);
        assert(spec->model_token != 0);
        assert(spec->display_token != 0);
        assert(spec->sound_token != 0);
        const int imbuable = (spec->flags & MB64_OBJECT_FLAG_IMBUABLE) != 0;
        assert(spec->reward_policy ==
               (imbuable ? MB64_OBJECT_REWARD_AUTHORED_IMBUE
                         : MB64_OBJECT_REWARD_ENGINE_DEFAULT));
        authored_reward_count += spec->reward_policy == MB64_OBJECT_REWARD_AUTHORED_IMBUE;
    }

    const mb64_object_spec_t *star = mb64_object_catalog_get(MB64_OBJECT_TYPE_STAR);
    assert(star->y_offset == 128.0f);
    assert(strcmp(star->model_token, "MODEL_STAR") == 0);
    assert((star->flags & MB64_OBJECT_FLAG_STAR) != 0);

    const mb64_object_spec_t *coin = mb64_object_catalog_get(MB64_OBJECT_TYPE_COIN);
    assert(coin->coin_count == 1);
    assert((coin->flags & MB64_OBJECT_FLAG_BILLBOARD) != 0);

    const mb64_object_spec_t *moneybag = mb64_object_catalog_get(MB64_OBJECT_TYPE_MONEYBAG);
    assert(strcmp(moneybag->behavior_token, "bhvMoneybagHidden") == 0);
    assert(moneybag->extra_object_count == 1);

    const mb64_object_spec_t *showrunner = mb64_object_catalog_get(MB64_OBJECT_TYPE_SHOWRUNNER);
    assert(showrunner->coin_count == 50);
    assert(showrunner->extra_object_count == 39);

    assert(authored_reward_count > 20);
    const mb64_object_spec_t *bigBoo = mb64_object_catalog_get(MB64_OBJECT_TYPE_BIG_BOO);
    assert(bigBoo->reward_policy == MB64_OBJECT_REWARD_AUTHORED_IMBUE);
    assert(bigBoo->reward_y_offset == 128.0f);

    const mb64_object_spec_t *koopaQuick = mb64_object_catalog_get(MB64_OBJECT_TYPE_KOOPA_THE_QUICK);
    assert(koopaQuick->reward_policy == MB64_OBJECT_REWARD_ENGINE_DEFAULT);

    assert(mb64_object_catalog_get(MB64_OBJECT_TYPE_COUNT) == 0);
    return 0;
}
