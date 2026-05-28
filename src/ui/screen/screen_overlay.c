/*
 * 文件: src/app_ui/ui/screen/screen_overlay.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

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
