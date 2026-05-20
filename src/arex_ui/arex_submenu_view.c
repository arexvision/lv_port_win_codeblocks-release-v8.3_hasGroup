#include "arex_submenu_view.h"

#include "arex_callbacks.h"
#include "arex_screen.h"
#include "arex_submenu_model.h"
#include "arex_ui_state.h"
#include "fonts/arex_fonts.h"

#include <string.h>

static lv_obj_t *s_submenu_layer = NULL;
static lv_obj_t *s_submenu_title = NULL;
static lv_obj_t *s_submenu_list = NULL;
static lv_obj_t *s_light_status_lbl = NULL;
static uint16_t s_submenu_width = 0;
static uint16_t s_submenu_height = 0;
static arex_submenu_setting_confirm_t s_pending_setting;

void arex_submenu_view_reset(void)
{
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_list = NULL;
    s_light_status_lbl = NULL;
    s_submenu_width = 0;
    s_submenu_height = 0;
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
}

lv_obj_t *arex_submenu_view_get_list(void)
{
    return s_submenu_list;
}

void arex_submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
{
    s_submenu_width = width;
    s_submenu_height = height;

    s_submenu_layer = lv_obj_create(parent);
    lv_obj_set_size(s_submenu_layer, s_submenu_width, s_submenu_height);
    lv_obj_set_pos(s_submenu_layer, s_submenu_width, 0);
    lv_obj_set_style_bg_color(s_submenu_layer, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_set_style_text_color(s_submenu_title, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(s_submenu_title, 16, 8);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    lv_obj_t *title_line = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(title_line, s_submenu_width - 32, 2);
    lv_obj_set_pos(title_line, 16, AREX_CARD_TITLE_H - 2);
    lv_obj_set_style_bg_color(title_line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_line, 0, 0);
    lv_obj_set_style_pad_all(title_line, 0, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, s_submenu_width - 15, s_submenu_height - AREX_CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, AREX_CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_clear_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    if (!s_submenu_layer) return;
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_submenu_width > 0 ? s_submenu_width : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, slide_w, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void submenu_slide_out(void)
{
    if (!s_submenu_layer) return;
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_submenu_width > 0 ? s_submenu_width : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, slide_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    if (!s_submenu_title || !s_submenu_list) return;

    lv_label_set_text(s_submenu_title, title);
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;  /* 重置 LIGHT 状态标签 */

    /* right_w 从缓存读取，fallback = safe_zone_w - left_anchor_w - panel_gap */
    uint16_t right_w = (s_submenu_width > 0)
                       ? s_submenu_width
                       : (g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - g_sys_config.panel_gap_u * AREX_BASE_U);
    uint16_t sub_w = right_w;
    int item_h = (int)(g_sys_config.h_menu_item * AREX_BASE_U);  /* 5U=50px */
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)(g_sys_config.gap_menu * AREX_BASE_U);   /* 1U=10px */
    int current_y = 0;

    for (uint8_t i = 0; i < count; i++)
    {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* LIGHT CONTROL 特殊布局: LIGHT 左侧，ON/OFF 右侧 */
        if (strcmp(title, "LIGHT CONTROL") == 0 && i == 0)
        {
            /* "LIGHT" 标签在左侧 */
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, AREX_GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, "LIGHT");

            /* "ON"/"OFF" 标签在右侧 */
            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, g_light_power_state ? "ON" : "OFF");

            /* 保存状态标签引用，用于点击时更新 */
            s_light_status_lbl = lbl_status;
            current_y += item_h + gap_y;
            continue;
        }

        /* 普通菜单项 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, items[i]);

        current_y += item_h + gap_y;
    }
    arex_screen_set_submenu_selection(0);
}

void arex_screen_set_submenu_selection(uint8_t idx)
{
    if (!s_submenu_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
        if (i == idx)
        {
            lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_GREEN, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
        }
        else
        {
            lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
            if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_DARK, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
            /* LIGHT CONTROL 特殊处理：第二列（ON/OFF）恢复状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
        }
    }
}

/* INFO sub-menu */
void arex_screen_open_info_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    const char *title = arex_submenu_info_title(item_idx);
    const char **items = arex_submenu_build_info_items(item_idx, &count);
    if (!title || !items || count == 0) return;

    submenu_populate(title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

static bool refresh_compass_cal_submenu(void)
{
    if (!s_submenu_list || !s_submenu_title)
    {
        return false;
    }

    const char *raw_title = lv_label_get_text(s_submenu_title);
    const char *title = raw_title;
    if (title && title[0] == '>' && title[1] == ' ')
    {
        title += 2;
    }
    if (!title || strcmp(title, "COMPASS CAL") != 0)
    {
        return false;
    }

    uint8_t count = 0;
    const char **items = arex_submenu_build_compass_cal_items(&count);
    if (!items || count == 0)
    {
        return false;
    }
    submenu_populate("COMPASS CAL", items, count);
    g_ui.sub_item_count = count;
    if (g_ui.sub_menu_idx >= count)
    {
        g_ui.sub_menu_idx = count - 1;
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
    return true;
}

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    const char *title = arex_submenu_setup_title(item_idx);
    const char **items = arex_submenu_build_setup_items(item_idx, &count);
    if (!title || !items || count == 0) return;

    submenu_populate(title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

void arex_screen_refresh_compass_cal_submenu_if_open(void)
{
    (void)refresh_compass_cal_submenu();
}

static void submenu_history_push(void)
{
    if (!s_submenu_title) return;
    if (g_ui.sub_history_depth >= AREX_SUB_HISTORY_MAX) return;
    arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
    const char *cur_title = lv_label_get_text(s_submenu_title);
    lv_snprintf(h->title, sizeof(h->title), "%s", cur_title ? cur_title : "");
    h->idx = g_ui.sub_menu_idx;
    g_ui.sub_history_depth++;
}

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    if (!title || !items) return;
    submenu_history_push();
    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.state = UI_SUB_MENU;
}

static void dispatch_submenu_setting_callback(const arex_submenu_setting_confirm_t *setting)
{
    if (!setting)
    {
        return;
    }

    switch (setting->kind)
    {
    case AREX_SUBMENU_SETTING_DIVE_MODE:
        arex_ui_on_dive_mode_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_SALINITY:
        arex_ui_on_salinity_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_SAFETY_STOP:
        if (setting->value < 4)
        {
            static const uint8_t minutes[] = { 0, 3, 4, 5 };
            arex_ui_on_safety_stop_time_set(minutes[setting->value]);
        }
        break;
    case AREX_SUBMENU_SETTING_ALTITUDE:
        arex_ui_on_altitude_range_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_AI_PAIR:
        arex_ui_on_ai_pair((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_AI_TANK_STATE:
        arex_ui_on_ai_tank_state_set(setting->arg, (uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_GTR_MODE:
        arex_ui_on_gtr_mode_set(setting->value != 0);
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        arex_ui_on_depth_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        arex_ui_on_time_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_NDL_ALARM:
        arex_ui_on_ndl_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_VIBRATION_TEST:
        arex_ui_on_vibration_test();
        break;
    case AREX_SUBMENU_SETTING_UNITS:
        arex_ui_on_units_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_DATETIME_FIELD:
    {
        uint16_t field_value = setting->value;
        if (setting->arg == 0)
        {
            field_value = (uint16_t)(2024 + setting->value);
        }
        arex_ui_on_datetime_field_set(setting->arg, field_value);
        break;
    }
    case AREX_SUBMENU_SETTING_DATETIME_ACTION:
        arex_ui_on_datetime_action((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_LOG_RATE:
        arex_ui_on_log_rate_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_BLUETOOTH:
        arex_ui_on_bluetooth_set(setting->value != 0);
        break;
    case AREX_SUBMENU_SETTING_RESET_DEFAULTS:
        arex_ui_on_reset_defaults();
        break;
    default:
        break;
    }
}

static void refresh_current_submenu_page(const char *cur_title, uint8_t keep_idx)
{
    uint8_t count = 0;
    const char **items = arex_submenu_nested_items_for(cur_title, &count);
    if (!items || count == 0)
    {
        return;
    }

    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", cur_title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1);
    }
    g_ui.sub_menu_idx = keep_idx;
    arex_screen_set_submenu_selection(keep_idx);
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (!s_submenu_list || !s_submenu_title) return;
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    const char *raw_title = lv_label_get_text(s_submenu_title);
    char cur_title[40] = {0};
    if (raw_title)
    {
        const char *p = raw_title;
        if (p[0] == '>' && p[1] == ' ') p += 2;
        lv_snprintf(cur_title, sizeof(cur_title), "%s", p);
    }

    if (strcmp(text, "< BACK") == 0)
    {
        arex_screen_close_submenu();
        return;
    }

    // HOTFIX: Block action for Info detail rows.
    if (arex_submenu_is_readonly_info_title(cur_title))
    {
        return;
    }

    {
        arex_submenu_edit_spec_t edit_spec;
        if (arex_submenu_edit_spec_from_selection(cur_title, item_idx, text, &edit_spec))
        {
            arex_screen_begin_edit_value(item_idx, &edit_spec);
            return;
        }
    }

    {
        arex_submenu_setting_confirm_t direct_setting;
        if (arex_submenu_direct_setting_from_selection(cur_title, item_idx, text, &direct_setting))
        {
            arex_submenu_apply_setting(direct_setting.kind,
                                       direct_setting.arg,
                                       direct_setting.value);
            dispatch_submenu_setting_callback(&direct_setting);
            if (direct_setting.kind == AREX_SUBMENU_SETTING_DATETIME_FIELD)
            {
                arex_screen_close_submenu();
                return;
            }
            refresh_current_submenu_page(cur_title, item_idx);
            return;
        }
    }

    if (arex_submenu_setting_from_selection(cur_title, item_idx, text, &s_pending_setting))
    {
        arex_screen_show_modal_setup_confirm(s_pending_setting.body);
        g_ui.state = UI_MODAL_SETUP_CONFIRM;
        return;
    }

    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && item_idx == 0)
    {
        g_light_power_state = !g_light_power_state;
        arex_bus_set_light_power(g_light_power_state);

        /* 同步当前子菜单显示 */
        if (s_light_status_lbl)
        {
            lv_label_set_text(s_light_status_lbl, g_light_power_state ? "ON" : "OFF");
        }
        /* 保持选中项停留在 ON/OFF 行 */
        arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
        return;
    }

    {
        char child_title[40] = {0};
        uint8_t ncnt = 0;
        const char **nitems = arex_submenu_child_items_for(cur_title,
                                                           item_idx,
                                                           text,
                                                           child_title,
                                                           sizeof(child_title),
                                                           &ncnt);
        if (nitems && ncnt > 0)
        {
            arex_screen_open_nested_submenu(child_title, nitems, ncnt);
            return;
        }
    }

    if (strcmp(cur_title, "GAS SWITCH") == 0)
    {
        const char *gas_name = text;
        if (strncmp(text, "SELECT ", 7) == 0) gas_name = text + 7;
        extern const char *AREX_GAS_NAMES[4];
        for (uint8_t i = 0; i < 4; i++)
        {
            if (strcmp(AREX_GAS_NAMES[i], gas_name) == 0)
            {
                // HOTFIX: Route gas switch to safety modal.
                g_ui.gas_cursor = i;
                g_ui.gas_modal_from_submenu = true;  // HOTFIX: Route GAS modal exit based on context.
                arex_screen_show_modal_gas();
                g_ui.state = UI_MODAL_GAS;
                return;
            }
        }
        return;
    }

    if (strcmp(cur_title, "CONSERVATISM") == 0)
    {
        if (strcmp(text, "< BACK") != 0)
        {
            /* 解析 conservatism 等级：LOW/MED/HIGH/GF 50/70 */
            uint8_t level = 1;  /* 默认 MED */
            if (strncmp(text, "LOW", 3) == 0) level = 0;
            else if (strncmp(text, "MED", 3) == 0) level = 1;
            else if (strncmp(text, "HIGH", 4) == 0) level = 2;
            else if (strncmp(text, "GF 50/70", 8) == 0) level = 3;

            /* 通知业务层应用保守度设置 */
            arex_ui_on_conservatism_set(level);

            arex_screen_refresh_setup_menu();
        }
        arex_screen_update_setup_badge(1, text);
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "COMPASS CAL") == 0)
    {
        if (strncmp(text, "AUTO CAL:", 9) == 0)
        {
            arex_request_compass_calibration_start();
            arex_set_compass_calibration_ui_state(AREX_COMPASS_CAL_RUNNING);
            arex_screen_refresh_setup_menu();
            refresh_compass_cal_submenu();
            return;
        }
        if (strcmp(text, "RESET AUTO CAL") == 0)
        {
            arex_request_compass_calibration_reset();
            arex_set_compass_calibration_ui_state(AREX_COMPASS_CAL_IDLE);
            arex_screen_refresh_setup_menu();
            refresh_compass_cal_submenu();
            return;
        }
        return;
    }

    if (strcmp(cur_title, "BRIGHTNESS") == 0)
    {
        if (strcmp(text, "< BACK") != 0)
        {
            /* 更新亮度配置并刷新 badge */
            if (strcmp(text, "LOW") == 0)
            {
                g_sys_config.brightness = 0;
            }
            else if (strcmp(text, "ECO") == 0)
            {
                g_sys_config.brightness = 1;
            }
            else if (strcmp(text, "MED") == 0)
            {
                g_sys_config.brightness = 2;
            }
            else if (strcmp(text, "HIGH") == 0)
            {
                g_sys_config.brightness = 3;
            }
            else if (strcmp(text, "MAX") == 0)
            {
                g_sys_config.brightness = 4;
            }
            else if (strcmp(text, "SUN") == 0)
            {
                g_sys_config.brightness = 5;
            }
            /* 通过业务回调应用亮度 */
            arex_set_brightness(g_sys_config.brightness);
        }
        arex_screen_update_setup_badge(2, text);
        arex_screen_close_submenu();
        return;
    }

    /* 颜色子菜单：RED/GREEN/BLUE/WHITE */
    if (strcmp(cur_title, "RED") == 0 || strcmp(cur_title, "GREEN") == 0 ||
            strcmp(cur_title, "BLUE") == 0 || strcmp(cur_title, "WHITE") == 0)
    {
        /* 非返回项表示选择了亮度档位 */
        if (strcmp(text, "< BACK") != 0)
        {
            /* 通知业务层处理灯光颜色亮度 */
            arex_ui_on_light_color_set(cur_title, text);

        }
        /* 完成后关闭颜色子菜单 */
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE MENU") == 0 || strcmp(cur_title, "DIVE SETUP") == 0)
    {
        arex_screen_show_modal_act(text);
        return;
    }

    if (strcmp(cur_title, "ALERTS SETUP") == 0 && item_idx == 2)
    {
        return;
    }

    arex_screen_show_modal_act(text);
}

void arex_screen_close_submenu(void)
{
    if (!s_submenu_layer || !s_submenu_title || !s_submenu_list)
    {
        g_ui.sub_history_depth = 0;
        g_ui.sub_item_count = 0;
        g_ui.sub_menu_idx = 0;
        g_ui.edit_ctx.active = false;
        g_ui.gas_modal_from_submenu = false;
        g_ui.state = g_ui.sub_parent;
        return;
    }

    if (g_ui.sub_history_depth > 0)
    {
        g_ui.sub_history_depth--;
        arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
        const char *prev_title = h->title;
        if (prev_title[0] == '>' && prev_title[1] == ' ') prev_title += 2;

        bool found = false;
        int8_t setup_idx = arex_submenu_setup_index_for_title(prev_title);
        if (setup_idx >= 0)
        {
            uint8_t cnt = 0;
            const char **items = arex_submenu_build_setup_items((uint8_t)setup_idx, &cnt);
            const char *title = arex_submenu_setup_title((uint8_t)setup_idx);
            if (items && title && cnt > 0)
            {
                submenu_populate(title, items, cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                if (g_ui.sub_menu_idx >= cnt)
                {
                    g_ui.sub_menu_idx = cnt - 1;
                }
                arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
                found = true;
            }
        }
        if (!found)
        {
            uint8_t ncnt = 0;
            const char **nitems = arex_submenu_nested_items_for(prev_title, &ncnt);
            if (nitems && ncnt > 0)
            {
                char full_title[40];
                lv_snprintf(full_title, sizeof(full_title), "> %s", prev_title);
                submenu_populate(full_title, nitems, ncnt);
                g_ui.sub_item_count = ncnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        g_ui.state = UI_SUB_MENU;
        return;
    }
    submenu_slide_out();
    g_ui.state = g_ui.sub_parent;
}

void arex_screen_confirm_submenu_setting(void)
{
    if (s_pending_setting.kind == AREX_SUBMENU_SETTING_NONE)
    {
        arex_screen_hide_modal();
        g_ui.state = UI_SUB_MENU;
        return;
    }

    arex_submenu_apply_setting(s_pending_setting.kind, s_pending_setting.arg, s_pending_setting.value);
    dispatch_submenu_setting_callback(&s_pending_setting);

    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
    arex_screen_hide_modal();
    arex_screen_close_submenu();
}

void arex_screen_cancel_submenu_setting(void)
{
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
    arex_screen_hide_modal();
    g_ui.state = UI_SUB_MENU;
}
