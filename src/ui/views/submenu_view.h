#ifndef SUBMENU_VIEW_H
#define SUBMENU_VIEW_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void submenu_view_reset(void);
void submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height);
lv_obj_t *submenu_view_get_list(void);

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_VIEW_H */
