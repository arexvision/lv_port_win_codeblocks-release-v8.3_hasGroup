#ifndef AREX_CARD_COMPASS_H
#define AREX_CARD_COMPASS_H

#include <stdbool.h>

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void card_compass_create(lv_obj_t *parent);
void card_compass_update(void);
void card_compass_refresh_heading(bool force_refresh);

#ifdef __cplusplus
}
#endif

#endif /* AREX_CARD_COMPASS_H */
