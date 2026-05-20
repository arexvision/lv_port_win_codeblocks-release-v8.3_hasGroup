#ifndef AREX_WIDGET_VIEW_H
#define AREX_WIDGET_VIEW_H

#include "arex_ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

void arex_reset_widget_render_state(void);

lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              arex_widget_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              arex_font_id_t cfg_font_id);

void arex_widget_refresh_sys(uint32_t dirty_mask);
void arex_widget_refresh_ascent_icons(float rate);

#ifdef __cplusplus
}
#endif

#endif /* AREX_WIDGET_VIEW_H */
