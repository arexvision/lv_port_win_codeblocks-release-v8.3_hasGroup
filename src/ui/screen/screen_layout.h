#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

#include "screen_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void clear_widget_arrays(void);
void left_anchor_rebuild(uint8_t comp_count);
void left_anchor_create(void);
void safe_zone_reposition(void);
void right_panel_create(void);
lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y);
void wall_create(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_LAYOUT_H */
