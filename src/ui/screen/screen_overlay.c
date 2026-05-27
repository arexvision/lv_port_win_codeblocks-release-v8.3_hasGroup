#include "screen_overlay.h"
#include "../views/submenu_model.h"

#include <stdio.h>

void apply_software_brightness(uint8_t level)
{
    lv_opa_t opa = (lv_opa_t)submenu_brightness_visible_opa(level);
    lv_opa_t overlay_opa = (lv_opa_t)(255 - opa);

    if (s_scr == NULL)
    {
        return;
    }

    if (s_brightness_overlay == NULL)
    {
        s_brightness_overlay = lv_obj_create(s_scr);
        lv_obj_remove_style_all(s_brightness_overlay);
        lv_obj_set_size(s_brightness_overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(s_brightness_overlay, 0, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    if (!s_software_brightness_enabled || overlay_opa == 0)
    {
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_set_style_bg_opa(s_brightness_overlay, overlay_opa, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    printf("[BRIGHTNESS] Level: %d (OPA: %d overlay=%d)\n", level, opa, overlay_opa);
}

void set_software_brightness_enabled(bool enabled)
{
    s_software_brightness_enabled = enabled;

    if (s_brightness_overlay == NULL)
    {
        return;
    }

    if (enabled)
    {
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *get_safe_zone(void)
{
    return s_safe_zone;
}
