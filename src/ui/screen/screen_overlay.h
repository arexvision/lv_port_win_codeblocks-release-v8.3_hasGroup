#ifndef SCREEN_OVERLAY_H
#define SCREEN_OVERLAY_H

#include "screen_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void apply_software_brightness(uint8_t level);
void set_software_brightness_enabled(bool enabled);
lv_obj_t *get_safe_zone(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_OVERLAY_H */
