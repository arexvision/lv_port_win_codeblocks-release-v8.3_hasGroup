/*
 * 文件: src/app_ui/ui/screen/screen_edit.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "screen_edit.h"
#include "screen_internal.h"
#include "../core/callbacks.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "../core/ui_runtime.h"
#include "../core/ui_vm.h"
#include "../views/submenu_model.h"
#include "../views/submenu_view.h"
#include <stdio.h>

lv_timer_t *s_edit_flash_timer = NULL;
lv_obj_t   *s_edit_flash_badge = NULL;
lv_obj_t   *s_edit_flash_val_lbl = NULL;
bool        s_edit_flash_on = false;

static bool edit_kind_is_depth(submenu_setting_kind_t kind)
{
    return kind == SUBMENU_SETTING_PLAN_DEPTH || kind == SUBMENU_SETTING_DEPTH_ALARM || kind == SUBMENU_SETTING_DIVE_START_DEPTH;
}

static bool edit_kind_refreshes_submenu(submenu_setting_kind_t kind)
{
    return kind == SUBMENU_SETTING_OC_TECH_GAS || kind == SUBMENU_SETTING_GAS_EDIT_PPO2;
}

static void edit_flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_edit_flash_on = !s_edit_flash_on;
    if (s_edit_flash_val_lbl)
    {
        lv_color_t fg = s_edit_flash_on ? GREEN : DARK;
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
    }
}

static void edit_flash_start(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_on = false;
    edit_flash_timer_cb(NULL);
    if (s_edit_flash_val_lbl)
    {
        lv_obj_set_style_bg_color(s_edit_flash_val_lbl, GREEN, 0);
        lv_obj_set_style_bg_opa(s_edit_flash_val_lbl, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, BLACK, 0);
    }
}

void edit_flash_stop(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_badge = NULL;
    s_edit_flash_val_lbl = NULL;
}

static void format_edit_value_text(char *buf, size_t buf_size, submenu_setting_kind_t kind, uint8_t arg, float value, uint8_t decimals)
{
    if (kind == SUBMENU_SETTING_DATETIME_FIELD)
    {
        unsigned int v = (unsigned int)(value + 0.5f);
        snprintf(buf, buf_size, (arg == 0) ? "%04u" : "%02u", v);
    }
    else if (kind == SUBMENU_SETTING_DIVE_START_DEPTH)
    {
        snprintf(buf, buf_size, "%.1f%s", (double)bus_get_depth_display(value), bus_get_depth_unit_label());
    }
    else if (edit_kind_is_depth(kind))
    {
        snprintf(buf, buf_size, "%.0f%s", (double)bus_get_depth_display(value), bus_get_depth_unit_label());
    }
    else if (decimals == 0)
    {
        snprintf(buf, buf_size, "%.0f", (double)value);
    }
    else
    {
        snprintf(buf, buf_size, "%.1f", (double)value);
    }
}

static void format_edit_committed_text(char *buf, size_t buf_size, submenu_setting_kind_t kind, uint8_t arg, float value)
{
    switch (kind)
    {
    case SUBMENU_SETTING_PLAN_DEPTH: snprintf(buf, buf_size, "DEPTH: %.0f%s", (double)bus_get_depth_display(value), bus_get_depth_unit_label()); break;
    case SUBMENU_SETTING_PLAN_TIME: snprintf(buf, buf_size, "TIME: %.0fmin", (double)value); break;
    case SUBMENU_SETTING_PLAN_RMV: snprintf(buf, buf_size, "RMV: %.0fL/min", (double)value); break;
    case SUBMENU_SETTING_MOD_PPO2: snprintf(buf, buf_size, "MOD PO2: %.1f", (double)value); break;
    case SUBMENU_SETTING_SURFACE_CONFIRM: snprintf(buf, buf_size, "DIVE END TIME: %.0fmin", (double)value); break;
    case SUBMENU_SETTING_DIVE_START_DEPTH: snprintf(buf, buf_size, "DIVE START DEPTH: %.1f%s", (double)bus_get_depth_display(value), bus_get_depth_unit_label()); break;
    case SUBMENU_SETTING_GAS_EDIT_PPO2: snprintf(buf, buf_size, "PO2: %.1f", (double)value); break;
    case SUBMENU_SETTING_NITROX_O2: snprintf(buf, buf_size, "O2: %.0f%%", (double)value); break;
    case SUBMENU_SETTING_3GAS_O2: snprintf(buf, buf_size, "GAS %u: %.0f%%", (unsigned)(arg + 1U), (double)value); break;
    case SUBMENU_SETTING_OC_TECH_GAS: snprintf(buf, buf_size, "%s PERCENT: %.0f%%", (arg % 2U) ? "HE" : "O2", (double)value); break;
    case SUBMENU_SETTING_DEPTH_ALARM: snprintf(buf, buf_size, "DEPTH ALARM: %.0f%s", (double)bus_get_depth_display(value), bus_get_depth_unit_label()); break;
    case SUBMENU_SETTING_TIME_ALARM: snprintf(buf, buf_size, "TIME ALARM: %.0fmin", (double)value); break;
    case SUBMENU_SETTING_NDL_ALARM: snprintf(buf, buf_size, "LOW NDL ALARM: %.0fmin", (double)value); break;
    case SUBMENU_SETTING_DATETIME_FIELD:
        switch (arg)
        {
        case 0: snprintf(buf, buf_size, "YEAR: %04u", (unsigned)(value + 0.5f)); break;
        case 1: snprintf(buf, buf_size, "MONTH: %02u", (unsigned)(value + 0.5f)); break;
        case 2: snprintf(buf, buf_size, "DAY: %02u", (unsigned)(value + 0.5f)); break;
        case 3: snprintf(buf, buf_size, "HOUR: %02u", (unsigned)(value + 0.5f)); break;
        case 4: snprintf(buf, buf_size, "MINUTE: %02u", (unsigned)(value + 0.5f)); break;
        default: snprintf(buf, buf_size, "%.0f", (double)value); break;
        }
        break;
    default:
        snprintf(buf, buf_size, "%.1f", (double)value);
        break;
    }
}

static void dispatch_edit_setting_callback(submenu_setting_kind_t kind, uint8_t arg, float value)
{
    switch (kind)
    {
    case SUBMENU_SETTING_MOD_PPO2: ui_on_mod_ppo2_set(value); break;
    case SUBMENU_SETTING_SURFACE_CONFIRM: ui_on_surface_confirm_min_set((uint8_t)(value + 0.5f)); break;
    case SUBMENU_SETTING_DIVE_START_DEPTH: ui_on_dive_start_depth_set(value); break;
    case SUBMENU_SETTING_DEPTH_ALARM: ui_on_depth_alarm_set((uint16_t)(value + 0.5f)); break;
    case SUBMENU_SETTING_TIME_ALARM: ui_on_time_alarm_set((uint16_t)(value + 0.5f)); break;
    case SUBMENU_SETTING_NDL_ALARM: ui_on_ndl_alarm_set((uint16_t)(value + 0.5f)); break;
    case SUBMENU_SETTING_DATETIME_FIELD: ui_on_datetime_field_set(arg, (uint16_t)(value + 0.5f)); break;
    default: break;
    }
}

static void edit_value_cleanup(lv_obj_t *item)
{
    if (!item) return;
    edit_flash_stop();
    lv_obj_set_style_border_color(item, DARK, 0);
    uint32_t cnt = lv_obj_get_child_cnt(item);
    while (cnt > 1)
    {
        lv_obj_del(lv_obj_get_child(item, 1));
        cnt = lv_obj_get_child_cnt(item);
    }
    lv_obj_set_layout(item, 0);
    lv_obj_set_style_pad_left(item, 0, 0);
    lv_obj_set_style_pad_right(item, 0, 0);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
    }
    lv_obj_update_layout(item);
}

void screen_refresh_edit_value(void)
{
    if (!ui_state_get_edit_active() || !s_edit_flash_val_lbl || !submenu_view_get_list()) return;
    static float last_drawn = -9999.f;
    float cur = ui_state_get_edit_value();
    if (cur == last_drawn) return;
    last_drawn = cur;
    char buf[16];
    format_edit_value_text(buf, sizeof(buf), ui_state_get_edit_setting_kind(), ui_state_get_edit_setting_arg(), cur, ui_state_get_edit_decimals());
    lv_label_set_text(s_edit_flash_val_lbl, buf);
}

void screen_begin_edit_value(uint8_t item_idx, const submenu_edit_spec_t *spec)
{
    lv_obj_t *submenu_list = submenu_view_get_list();
    if (!submenu_list || !spec) return;
    ui_state_set_edit_value(spec->value);
    ui_state_set_edit_original(spec->value);
    ui_state_set_edit_min(spec->min);
    ui_state_set_edit_max(spec->max);
    ui_state_set_edit_step(spec->step);
    ui_state_set_edit_setting_kind(spec->kind);
    ui_state_set_edit_setting_arg(spec->arg);
    ui_state_set_edit_decimals(spec->decimals);
    ui_state_set_edit_item_index(item_idx);
    ui_state_set_edit_active(true);
    ui_state_set_edit_label(spec->label);
    ui_state_set_state(UI_EDIT_VALUE);
    lv_obj_t *item = lv_obj_get_child(submenu_list, item_idx);
    if (!item)
    {
        ui_state_set_edit_active(false);
        ui_state_set_state(UI_SUB_MENU);
        return;
    }
    lv_obj_set_style_bg_color(item, BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, GREEN, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(item, 12, 0);
    lv_obj_set_style_pad_right(item, 12, 0);
    lv_obj_set_style_pad_column(item, 8, 0);
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl)
    {
        lv_label_set_text(prefix_lbl, ui_state_get_edit_label());
        lv_obj_set_style_text_color(prefix_lbl, GREEN, 0);
        lv_obj_set_size(prefix_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(prefix_lbl, LV_OPA_TRANSP, 0);
    }
    lv_obj_t *old_badge = lv_obj_get_child(item, 1);
    if (old_badge) lv_obj_set_style_text_color(old_badge, GREEN, 0);
    lv_obj_t *val_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(val_lbl, BLACK, 0);
    lv_obj_set_style_text_font(val_lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_bg_color(val_lbl, GREEN, 0);
    lv_obj_set_style_bg_opa(val_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(val_lbl, 8, 0);
    lv_obj_set_style_pad_right(val_lbl, 8, 0);
    lv_obj_set_style_pad_top(val_lbl, 1, 0);
    lv_obj_set_style_pad_bottom(val_lbl, 1, 0);
    lv_obj_set_size(val_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_CENTER, 0);
    char buf[16];
    format_edit_value_text(buf, sizeof(buf), spec->kind, spec->arg, spec->value, spec->decimals);
    lv_label_set_text(val_lbl, buf);
    s_edit_flash_badge = val_lbl;
    lv_obj_t *arrow_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(arrow_lbl, GREEN, 0);
    lv_obj_set_style_text_font(arrow_lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_bg_opa(arrow_lbl, LV_OPA_TRANSP, 0);
    lv_label_set_text(arrow_lbl, "▲▼");
    s_edit_flash_val_lbl = val_lbl;
    edit_flash_start();
}

void screen_commit_edit_value(void)
{
    submenu_setting_kind_t kind = ui_state_get_edit_setting_kind();
    uint8_t arg = ui_state_get_edit_setting_arg();
    float value = ui_state_get_edit_value();
    lv_obj_t *submenu_list = submenu_view_get_list();
    if (!submenu_list)
    {
        edit_flash_stop();
        ui_state_set_edit_active(false);
        return;
    }
    lv_obj_t *item = lv_obj_get_child(submenu_list, ui_state_get_edit_item_index());
    if (!item)
    {
        edit_flash_stop();
        ui_state_set_edit_active(false);
        return;
    }
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        char buf[32];
        format_edit_committed_text(buf, sizeof(buf), kind, arg, value);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, GREEN, 0);
    }
    submenu_apply_edit_value(kind, arg, value);
    dispatch_edit_setting_callback(kind, arg, value);
    ui_state_set_edit_active(false);
    if (edit_kind_refreshes_submenu(kind))
    {
        screen_refresh_current_submenu();
    }
    else
    {
        screen_set_submenu_selection(ui_state_get_sub_menu_idx());
    }
}

void screen_cancel_edit_value(void)
{
    lv_obj_t *submenu_list = submenu_view_get_list();
    if (!submenu_list)
    {
        edit_flash_stop();
        return;
    }
    lv_obj_t *item = lv_obj_get_child(submenu_list, ui_state_get_edit_item_index());
    if (!item)
    {
        edit_flash_stop();
        return;
    }
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        char buf[32];
        format_edit_committed_text(buf, sizeof(buf), ui_state_get_edit_setting_kind(), ui_state_get_edit_setting_arg(), ui_state_get_edit_original());
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, GREEN, 0);
    }
    screen_set_submenu_selection(ui_state_get_sub_menu_idx());
}
