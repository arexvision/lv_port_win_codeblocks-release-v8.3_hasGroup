#ifndef AREX_MODAL_VIEW_H
#define AREX_MODAL_VIEW_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void arex_modal_view_reset(void);
void arex_modal_view_create(lv_obj_t *parent, uint16_t width, uint16_t height);

void arex_screen_show_modal_act(const char *action_text);
void arex_screen_show_modal_gas(void);
void arex_screen_show_modal_compass(void);
void arex_screen_pulse_modal(void);
void arex_screen_hide_modal(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_MODAL_VIEW_H */
