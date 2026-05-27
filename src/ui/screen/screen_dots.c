#include "screen_dots.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "page_registry.h"

#include <stdio.h>

void screen_update_scroll_dots(uint8_t active_idx, bool visible)
{
    bool in_dash_or_edit = (ui_state_get_state() == UI_DASH || ui_state_get_state() == UI_EDIT_GAS);
    bool dots_enabled = (ui_dots_position_get() != DOTS_NONE);
    uint8_t visible_dash = page_visible_dash_count();

    printf("[DOTS] update: active=%u, visible=%d, state=%d, dots_pos=%d, visible_dash=%u, dot_cont_children=%d\r\n",
           active_idx, visible, ui_state_get_state(), ui_dots_position_get(), visible_dash,
           s_dot_cont ? lv_obj_get_child_cnt(s_dot_cont) : -1);

    for (uint8_t i = 0; i < DASH_PAGE_COUNT; i++)
    {
        if (!s_scroll_dots[i])
        {
            if (i < visible_dash)
            {
                printf("[DOTS] WARN: s_scroll_dots[%u] is NULL but visible_dash=%u!\r\n", i, visible_dash);
            }
            continue;
        }

        bool show = visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
        if (!show)
        {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i == active_idx)
        {
            lv_obj_set_style_bg_color(s_scroll_dots[i], GREEN, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 8, 0);
            lv_obj_set_style_shadow_color(s_scroll_dots[i], GREEN, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 255, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(s_scroll_dots[i], DARK, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 0, 0);
        }
    }
}
