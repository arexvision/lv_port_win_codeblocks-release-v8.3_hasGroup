#include "error_screen.h"

#include "lvgl/lvgl.h"
#include "ui/core/ui_defs.h"
#include "ui/fonts/fonts.h"

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

    lv_coord_t group_y = (lv_coord_t)(h / 2 - 64);
    lv_coord_t line_w = (lv_coord_t)(w > 360 ? 290 : w - 80);
    lv_color_t title_color = lv_color_hex(0x2F6F36);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_color(title, title_color, 0);
    lv_obj_set_style_text_font(title, FONT_MEDIUM, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(title, ERROR_SCREEN_TITLE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, (lv_coord_t)(group_y + 14));

    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, line_w, 2);
    lv_obj_set_style_bg_color(line, title_color, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, (lv_coord_t)(group_y + 62));

    lv_obj_t *message = lv_label_create(scr);
    lv_obj_set_width(message, (lv_coord_t)(w - 80));
    lv_obj_set_style_text_color(message, GREEN, 0);
    lv_obj_set_style_text_font(message, FONT_SMALL, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_label_set_text(message, ERROR_SCREEN_MESSAGE);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, (lv_coord_t)(group_y + 76));

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
