#include "error_screen.h"

#include "lvgl/lvgl.h"
#include "ui/core/ui_defs.h"

#if ERROR_SCREEN_ENABLED

static void error_screen_create(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, w, h);
    lv_obj_set_style_bg_color(scr, BLACK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_color(title, GREEN, 0);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_label_set_text(title, ERROR_SCREEN_TITLE);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -22);

    lv_obj_t *message = lv_label_create(scr);
    lv_obj_set_width(message, (lv_coord_t)(w - 80));
    lv_obj_set_style_text_color(message, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(message, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_label_set_text(message, ERROR_SCREEN_MESSAGE);
    lv_obj_align(message, LV_ALIGN_CENTER, 0, 18);

    lv_scr_load(scr);
}

#endif /* ERROR_SCREEN_ENABLED */

bool error_screen_try_start(void)
{
#if ERROR_SCREEN_ENABLED
    error_screen_create();
    return true;
#else
    return false;
#endif
}
