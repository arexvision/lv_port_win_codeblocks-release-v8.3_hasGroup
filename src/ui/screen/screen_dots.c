/*
 * 文件: src/app_ui/ui/screen/screen_dots.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "screen_dots.h"
#include "../core/data.h"
#include "../core/ui_defs.h"
#include "../core/ui_state.h"
#include "page_registry.h"

typedef struct
{
    uint8_t valid;
    uint8_t show;
    uint8_t active;
} scroll_dot_state_t;

static scroll_dot_state_t s_dot_state_cache[DASH_PAGE_COUNT];
static uint32_t s_last_interaction_ms;
static uint8_t s_interaction_visible;

void screen_scroll_dots_reset_cache(void)
{
    for (uint8_t i = 0U; i < DASH_PAGE_COUNT; i++)
    {
        s_dot_state_cache[i].valid = 0U;
        s_dot_state_cache[i].show = 0U;
        s_dot_state_cache[i].active = 0U;
    }
}

void screen_scroll_dots_notify_interaction(void)
{
    s_last_interaction_ms = lv_tick_get();
    s_interaction_visible = 1U;
}

static bool screen_scroll_dots_interaction_window_active(void)
{
#if UI_SCROLL_DOTS_AUTO_HIDE_MS == 0U
    return true;
#else
    if (!s_interaction_visible)
    {
        return false;
    }
    if ((uint32_t)(lv_tick_get() - s_last_interaction_ms) <= UI_SCROLL_DOTS_AUTO_HIDE_MS)
    {
        return true;
    }
    s_interaction_visible = 0U;
    return false;
#endif
}

void screen_update_scroll_dots(uint8_t active_idx, bool visible)
{
    bool in_dash_or_edit = (ui_state_get_state() == UI_DASH || ui_state_get_state() == UI_MENU_ENTRY || ui_state_get_state() == UI_EDIT_GAS);
    bool dots_enabled = (ui_dots_position_get() != DOTS_NONE);
    bool dots_visible = visible && screen_scroll_dots_interaction_window_active();
    uint8_t visible_dash = page_visible_dash_count();

    for (uint8_t i = 0; i < DASH_PAGE_COUNT; i++)
    {
        if (!s_scroll_dots[i])
        {
            continue;
        }

        bool show = dots_visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
        bool active = (show && i == active_idx);
        if (s_dot_state_cache[i].valid &&
            s_dot_state_cache[i].show == (show ? 1U : 0U) &&
            s_dot_state_cache[i].active == (active ? 1U : 0U))
        {
            continue;
        }

        s_dot_state_cache[i].valid = 1U;
        s_dot_state_cache[i].show = show ? 1U : 0U;
        s_dot_state_cache[i].active = active ? 1U : 0U;

        if (!show)
        {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (active)
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
