#ifndef MODAL_VIEW_H
#define MODAL_VIEW_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void modal_view_reset(void);
void modal_view_create(lv_obj_t *parent, uint16_t width, uint16_t height);

void screen_show_modal_act(const char *action_text);
void screen_show_modal_setup_confirm(const char *body);
void screen_show_modal_gas(void);
void screen_show_modal_compass(void);
void screen_pulse_modal(void);
void screen_hide_modal(void);

#ifdef __cplusplus
}
#endif

#endif /* MODAL_VIEW_H */
