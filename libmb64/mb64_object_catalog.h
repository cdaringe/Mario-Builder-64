/** Portable metadata exported from Mario Builder 64's object type table. */
#ifndef MB64_OBJECT_CATALOG_H
#define MB64_OBJECT_CATALOG_H

#include <stdint.h>

#include "mb64_object_types.h"

enum mb64_object_flag {
    MB64_OBJECT_FLAG_BILLBOARD = 1 << 0,
    MB64_OBJECT_FLAG_TRAJECTORY = 1 << 1,
    MB64_OBJECT_FLAG_STAR = 1 << 2,
    MB64_OBJECT_FLAG_HAS_DIALOG = 1 << 3,
    MB64_OBJECT_FLAG_IMBUABLE = 1 << 4,
    MB64_OBJECT_FLAG_IMBUABLE_COINS = 1 << 5,
    MB64_OBJECT_FLAG_IMBUABLE_TRIGGER = 1 << 6,
};

enum mb64_object_occupancy {
    MB64_OBJECT_OCCUPY_OUTER = 1 << 0,
    MB64_OBJECT_OCCUPY_INNER = 1 << 1,
    MB64_OBJECT_OCCUPY_FULL = MB64_OBJECT_OCCUPY_OUTER | MB64_OBJECT_OCCUPY_INNER,
};

typedef struct {
    mb64_object_type_t type;
    const char *name;
    const char *behavior_token;
    const char *model_token;
    const char *animation_token;
    const char *display_token;
    const char *sound_token;
    float y_offset;
    float scale;
    uint8_t flags;
    uint8_t occupancy;
    uint8_t coin_count;
    uint8_t extra_object_count;
} mb64_object_spec_t;

const mb64_object_spec_t *mb64_object_catalog_get(unsigned int type);
unsigned int mb64_object_catalog_count(void);

#endif
