#ifndef AREX_SUBMENU_VIEW_H
#define AREX_SUBMENU_VIEW_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void arex_submenu_view_reset(void);
void arex_submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height);
lv_obj_t *arex_submenu_view_get_list(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_SUBMENU_VIEW_H */
