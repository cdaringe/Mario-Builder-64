#include "mb64_object_catalog.h"

#include "mb64_object_catalog.generated.inc.c"

const mb64_object_spec_t *mb64_object_catalog_get(unsigned int type) {
    if (type >= MB64_OBJECT_TYPE_COUNT) {
        return 0;
    }
    return &sMb64ObjectCatalog[type];
}

unsigned int mb64_object_catalog_count(void) {
    return MB64_OBJECT_TYPE_COUNT;
}
