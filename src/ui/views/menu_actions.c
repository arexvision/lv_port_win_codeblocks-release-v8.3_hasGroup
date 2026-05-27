#include "menu_actions.h"

#include "../core/callbacks.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "../screen/screen.h"
#include "menu_runtime.h"
#include "submenu_model.h"
#include <stdio.h>
#include <string.h>

static submenu_setting_confirm_t s_pending_setting;

/* 清空 action 输出，避免 view 误执行上一次残留动作。 */
static void action_clear(menu_action_t *action)
{
    if (action)
    {
        memset(action, 0, sizeof(*action));
        action->type = MENU_ACTION_NONE;
    }
}

static void dispatch_setting_callback(const submenu_setting_confirm_t *setting)
{
    /* submenu_model 仍保存一部分旧设置状态。
     * apply_setting 更新模型内部值；这里负责通知真正业务回调。
     */
    if (!setting)
    {
        return;
    }

    switch (setting->kind)
    {
    case SUBMENU_SETTING_DIVE_MODE:
        ui_on_dive_mode_set((uint8_t)setting->value);
        screen_refresh_gas_menu();
        screen_refresh_left_panel();
        break;
    case SUBMENU_SETTING_SALINITY:
        ui_on_salinity_set((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_SAFETY_STOP:
        if (setting->value < 4)
        {
            static const uint8_t minutes[] = { 0, 3, 4, 5 };
            ui_on_safety_stop_time_set(minutes[setting->value]);
        }
        break;
    case SUBMENU_SETTING_LAST_DECO:
        ui_on_last_deco_stop_set(setting->value == 1 ? 6 : 3);
        break;
    case SUBMENU_SETTING_ALTITUDE:
        ui_on_altitude_range_set((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_AI_PAIR:
        ui_on_ai_pair((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_AI_TANK_STATE:
        ui_on_ai_tank_state_set(setting->arg, (uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_GTR_MODE:
        ui_on_gtr_mode_set(setting->value != 0);
        break;
    case SUBMENU_SETTING_DEPTH_ALARM:
        ui_on_depth_alarm_set(setting->value);
        break;
    case SUBMENU_SETTING_TIME_ALARM:
        ui_on_time_alarm_set(setting->value);
        break;
    case SUBMENU_SETTING_NDL_ALARM:
        ui_on_ndl_alarm_set(setting->value);
        break;
    case SUBMENU_SETTING_VIBRATION_TEST:
        ui_on_vibration_test();
        break;
    case SUBMENU_SETTING_UNITS:
        ui_on_units_set((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_DATETIME_FIELD:
    {
        uint16_t field_value = setting->value;
        if (setting->arg == 0)
        {
            field_value = (uint16_t)(2024 + setting->value);
        }
        ui_on_datetime_field_set(setting->arg, field_value);
        break;
    }
    case SUBMENU_SETTING_DATETIME_ACTION:
        ui_on_datetime_action((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_LOG_RATE:
        ui_on_log_rate_set((uint8_t)setting->value);
        break;
    case SUBMENU_SETTING_BLUETOOTH:
        ui_on_bluetooth_set(setting->value != 0);
        break;
    case SUBMENU_SETTING_RESET_DEFAULTS:
        ui_on_reset_defaults();
        break;
    default:
        break;
    }
}

static uint8_t next_log_rate_s(void)
{
    static const uint8_t values[] = { 2, 5, 10, 30 };
    uint8_t current = submenu_log_rate_s();

    for (uint8_t i = 0; i < (sizeof(values) / sizeof(values[0])); i++)
    {
        if (values[i] == current)
        {
            uint8_t next = (uint8_t)((i + 1U) % (sizeof(values) / sizeof(values[0])));
            return values[next];
        }
    }
    return values[0];
}

static void setting_prepare(submenu_setting_confirm_t *setting,
                            submenu_setting_kind_t kind,
                            uint8_t arg,
                            uint16_t value)
{
    memset(setting, 0, sizeof(*setting));
    setting->kind = kind;
    setting->arg = arg;
    setting->value = value;
}

static bool direct_setting_for_id(menu_item_id_t id,
                                  submenu_setting_confirm_t *out_setting)
{
    if (!out_setting)
    {
        return false;
    }

    switch (id)
    {
    case MENU_ITEM_DIVE_SALINITY:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_SALINITY,
                        0U,
                        (uint16_t)((g_sys_config.salinity_mode + 1U) % 3U));
        return true;
    case MENU_ITEM_DIVE_SAFETY_STOP:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_SAFETY_STOP,
                        0U,
                        (uint16_t)((submenu_safety_stop_mode() + 1U) % 4U));
        return true;
    case MENU_ITEM_DIVE_LAST_DECO:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_LAST_DECO,
                        0U,
                        (g_sys_config.last_deco_stop_m == 6U) ? 0U : 1U);
        return true;
    case MENU_ITEM_DIVE_ALTITUDE:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_ALTITUDE,
                        0U,
                        (uint16_t)((submenu_altitude_level() + 1U) % 4U));
        return true;
    case MENU_ITEM_AI_TANK_0:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_AI_TANK_STATE,
                        0U,
                        (uint16_t)((submenu_ai_tank_state(0U) + 1U) % 3U));
        return true;
    case MENU_ITEM_AI_TANK_1:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_AI_TANK_STATE,
                        1U,
                        (uint16_t)((submenu_ai_tank_state(1U) + 1U) % 3U));
        return true;
    case MENU_ITEM_AI_GTR:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_GTR_MODE,
                        0U,
                        submenu_gtr_enabled() ? 0U : 1U);
        return true;
    case MENU_ITEM_DISPLAY_UNITS:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_UNITS,
                        0U,
                        submenu_units_mode() == 0U ? 1U : 0U);
        return true;
    case MENU_ITEM_DISPLAY_LOG_RATE:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_LOG_RATE,
                        0U,
                        next_log_rate_s());
        return true;
    case MENU_ITEM_DISPLAY_BLUETOOTH:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_BLUETOOTH,
                        0U,
                        submenu_bluetooth_enabled() ? 0U : 1U);
        return true;
    case MENU_ITEM_THREE_GAS_COUNT:
    {
        uint8_t count = submenu_three_gas_count();
        setting_prepare(out_setting,
                        SUBMENU_SETTING_3GAS_COUNT,
                        0U,
                        count >= 3U ? 1U : (uint16_t)(count + 1U));
        return true;
    }
    case MENU_ITEM_OC_TECH_EDIT_SAVE:
        setting_prepare(out_setting,
                        SUBMENU_SETTING_OC_TECH_SAVE,
                        submenu_oc_tech_edit_slot(),
                        0U);
        return true;
    default:
        return false;
    }
}

static bool confirm_setting_for_id(menu_item_id_t id,
                                   submenu_setting_confirm_t *out_setting)
{
    if (!out_setting)
    {
        return false;
    }

    switch (id)
    {
    case MENU_ITEM_MODE_AIR:
        setting_prepare(out_setting, SUBMENU_SETTING_DIVE_MODE, 0U, 0U);
        snprintf(out_setting->body, sizeof(out_setting->body), "DIVE MODE\nAIR");
        return true;
    case MENU_ITEM_NITROX_CONFIRM:
        setting_prepare(out_setting, SUBMENU_SETTING_DIVE_MODE, 0U, 1U);
        snprintf(out_setting->body,
                 sizeof(out_setting->body),
                 "DIVE MODE\nNITROX %u%%",
                 (unsigned)submenu_nitrox_o2_pct());
        return true;
    case MENU_ITEM_THREE_GAS_CONFIRM:
        setting_prepare(out_setting, SUBMENU_SETTING_DIVE_MODE, 0U, 2U);
        snprintf(out_setting->body,
                 sizeof(out_setting->body),
                 "DIVE MODE\n3 GAS / %u ACTIVE",
                 (unsigned)submenu_three_gas_count());
        return true;
    case MENU_ITEM_OC_TECH_CONFIRM:
        setting_prepare(out_setting, SUBMENU_SETTING_DIVE_MODE, 0U, 3U);
        snprintf(out_setting->body, sizeof(out_setting->body), "DIVE MODE\nOC Tech ACTIVE");
        return true;
    case MENU_ITEM_DISPLAY_RESET:
        setting_prepare(out_setting, SUBMENU_SETTING_RESET_DEFAULTS, 0U, 0U);
        snprintf(out_setting->body, sizeof(out_setting->body), "RESET DEFAULTS\nDISPLAY SETUP");
        return true;
    default:
        return false;
    }
}

static bool handle_conservatism(menu_item_id_t id, menu_action_t *action)
{
    uint8_t index;
    const setting_option_t *option;

    if (id < MENU_ITEM_CONSERVATISM_LOW || id > MENU_ITEM_CONSERVATISM_CUSTOM)
    {
        return false;
    }

    index = (uint8_t)(id - MENU_ITEM_CONSERVATISM_LOW);
    option = submenu_conservatism_option(index);
    ui_on_conservatism_set(option->value);
    screen_refresh_setup_menu();
    screen_update_setup_badge(1, option->badge_label);
    action->type = MENU_ACTION_CLOSE;
    return true;
}

static bool handle_brightness(menu_item_id_t id, menu_action_t *action)
{
    uint8_t index;
    const brightness_option_t *option;

    if (id < MENU_ITEM_BRIGHTNESS_ECO || id > MENU_ITEM_BRIGHTNESS_SUN)
    {
        return false;
    }

    index = (uint8_t)(id - MENU_ITEM_BRIGHTNESS_ECO);
    option = submenu_brightness_option(index);
    bus_set_brightness(option->value);
    set_brightness(option->value);
    screen_update_setup_badge(2, option->badge_label);
    action->type = MENU_ACTION_CLOSE;
    return true;
}

static bool handle_gas_switch(menu_item_id_t id, menu_action_t *action)
{
    if (id < MENU_ITEM_GAS_SLOT_0 || id > MENU_ITEM_GAS_SLOT_4)
    {
        return false;
    }
    g_ui.gas_cursor = (uint8_t)(id - MENU_ITEM_GAS_SLOT_0);
    g_ui.gas_modal_from_submenu = true;
    action->type = MENU_ACTION_SHOW_GAS_MODAL;
    return true;
}

static bool handle_compass(menu_item_id_t id, menu_action_t *action)
{
    if (id == MENU_ITEM_COMPASS_CAL_START)
    {
        request_compass_calibration_start();
        set_compass_calibration_ui_state(COMPASS_CAL_RUNNING);
        screen_refresh_setup_menu();
        action->type = MENU_ACTION_REFRESH;
        return true;
    }
    if (id == MENU_ITEM_COMPASS_CAL_RESET)
    {
        request_compass_calibration_reset();
        set_compass_calibration_ui_state(COMPASS_CAL_IDLE);
        screen_refresh_setup_menu();
        action->type = MENU_ACTION_REFRESH;
        return true;
    }
    return false;
}

static const char *light_level_label(menu_item_id_t id)
{
    switch (id)
    {
    case MENU_ITEM_LIGHT_LEVEL_10:  return "10%";
    case MENU_ITEM_LIGHT_LEVEL_30:  return "30%";
    case MENU_ITEM_LIGHT_LEVEL_50:  return "50%";
    case MENU_ITEM_LIGHT_LEVEL_70:  return "70%";
    case MENU_ITEM_LIGHT_LEVEL_100: return "100%";
    default:                       return "";
    }
}

static bool handle_light(menu_item_id_t id, const menu_row_t *row, menu_action_t *action)
{
    (void)row;
    if (id == MENU_ITEM_LIGHT_POWER)
    {
        g_light_power_state = !g_light_power_state;
        bus_set_light_power(g_light_power_state);
        action->type = MENU_ACTION_REFRESH;
        return true;
    }

    if (id >= MENU_ITEM_LIGHT_LEVEL_10 && id <= MENU_ITEM_LIGHT_LEVEL_100)
    {
        ui_on_light_color_set(menu_defs_light_color_name(menu_runtime_current_id()),
                              light_level_label(id));
        action->type = MENU_ACTION_CLOSE;
        return true;
    }
    return false;
}

static void edit_spec_prepare(submenu_edit_spec_t *spec,
                              submenu_setting_kind_t kind,
                              uint8_t arg,
                              float value,
                              float min,
                              float max,
                              float step,
                              uint8_t decimals,
                              const char *label)
{
    memset(spec, 0, sizeof(*spec));
    spec->kind = kind;
    spec->arg = arg;
    spec->value = value;
    spec->min = min;
    spec->max = max;
    spec->step = step;
    spec->decimals = decimals;
    snprintf(spec->label, sizeof(spec->label), "%s", label);
}

static bool edit_spec_for_id(menu_item_id_t id, submenu_edit_spec_t *out_spec)
{
    if (!out_spec)
    {
        return false;
    }

    switch (id)
    {
    case MENU_ITEM_DIVE_MOD_PPO2:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_MOD_PPO2, 0U,
                          g_sys_config.mod_ppo2, 1.0f, 1.6f, 0.1f, 1U, "MOD PO2:");
        return true;
    case MENU_ITEM_NITROX_O2:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_NITROX_O2, 0U,
                          (float)submenu_nitrox_o2_pct(), 21.0f, 40.0f, 1.0f, 0U, "O2:");
        return true;
    case MENU_ITEM_THREE_GAS_O2_0:
    case MENU_ITEM_THREE_GAS_O2_1:
    case MENU_ITEM_THREE_GAS_O2_2:
    {
        uint8_t gas_index = (uint8_t)(id - MENU_ITEM_THREE_GAS_O2_0);
        char label[20];
        snprintf(label, sizeof(label), "GAS %u:", (unsigned)(gas_index + 1U));
        edit_spec_prepare(out_spec, SUBMENU_SETTING_3GAS_O2, gas_index,
                          (float)submenu_three_gas_o2_pct(gas_index),
                          21.0f, 100.0f, 1.0f, 0U, label);
        return true;
    }
    case MENU_ITEM_OC_TECH_EDIT_O2:
    case MENU_ITEM_OC_TECH_EDIT_HE:
    {
        uint8_t slot = submenu_oc_tech_edit_slot();
        uint8_t o2 = submenu_oc_tech_draft_o2_pct(slot);
        uint8_t he = submenu_oc_tech_draft_he_pct(slot);
        bool edit_he = (id == MENU_ITEM_OC_TECH_EDIT_HE);
        float min = edit_he ? 0.0f : 8.0f;
        float max = edit_he ? (float)(100U - o2) : (float)(100U - he);
        if (max < min)
        {
            max = min;
        }
        edit_spec_prepare(out_spec, SUBMENU_SETTING_OC_TECH_GAS,
                          (uint8_t)(slot * 2U + (edit_he ? 1U : 0U)),
                          edit_he ? (float)he : (float)o2,
                          min, max, 1.0f, 0U,
                          edit_he ? "HE PERCENT:" : "O2 PERCENT:");
        return true;
    }
    case MENU_ITEM_ALERT_DEPTH:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DEPTH_ALARM, 0U,
                          (float)submenu_depth_alarm_m(), 10.0f, 150.0f, 10.0f, 0U, "DEPTH:");
        return true;
    case MENU_ITEM_ALERT_TIME:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_TIME_ALARM, 0U,
                          (float)submenu_time_alarm_min(), 10.0f, 300.0f, 10.0f, 0U, "TIME:");
        return true;
    case MENU_ITEM_DATE_YEAR:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DATETIME_FIELD, 0U,
                          (float)submenu_datetime_year(), 2000.0f, 2099.0f, 1.0f, 0U, "YEAR:");
        return true;
    case MENU_ITEM_DATE_MONTH:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DATETIME_FIELD, 1U,
                          (float)submenu_datetime_month(), 1.0f, 12.0f, 1.0f, 0U, "MONTH:");
        return true;
    case MENU_ITEM_DATE_DAY:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DATETIME_FIELD, 2U,
                          (float)submenu_datetime_day(), 1.0f, 31.0f, 1.0f, 0U, "DAY:");
        return true;
    case MENU_ITEM_DATE_HOUR:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DATETIME_FIELD, 3U,
                          (float)submenu_datetime_hour(), 0.0f, 23.0f, 1.0f, 0U, "HOUR:");
        return true;
    case MENU_ITEM_DATE_MINUTE:
        edit_spec_prepare(out_spec, SUBMENU_SETTING_DATETIME_FIELD, 4U,
                          (float)submenu_datetime_minute(), 0.0f, 59.0f, 1.0f, 0U, "MINUTE:");
        return true;
    default:
        return false;
    }
}

static bool handle_dive_plan(uint8_t row_index, const menu_row_t *row, menu_action_t *action)
{
    bool close_submenu = false;
    uint8_t keep_idx = row_index;
    bool exit_action;

    if (!menu_runtime_is_dive_plan())
    {
        return false;
    }
    exit_action = (row && row->id == MENU_ITEM_DIVE_PLAN_EXIT);
    if (!submenu_dive_plan_handle_action(exit_action,
                                         &close_submenu,
                                         &keep_idx))
    {
        action->type = MENU_ACTION_NONE;
        return true;
    }
    action->keep_index = keep_idx;
    action->type = close_submenu ? MENU_ACTION_CLOSE : MENU_ACTION_REFRESH;
    return true;
}

bool menu_actions_handle_select(uint8_t row_index,
                                const menu_row_t *row,
                                menu_action_t *out_action)
{
    submenu_setting_confirm_t setting;
    menu_id_t child;

    action_clear(out_action);
    if (!row || !out_action)
    {
        return false;
    }

    if (row->id == MENU_ITEM_BACK)
    {
        out_action->type = MENU_ACTION_BACK;
        return true;
    }

    if (handle_dive_plan(row_index, row, out_action))
    {
        return true;
    }

    if (row->type == MENU_ROW_READONLY || menu_defs_is_readonly_menu(menu_runtime_current_id()))
    {
        return true;
    }

    child = menu_defs_child_menu_for_item(row->id);
    if (child != MENU_NONE)
    {
        /* 行 ID 能映射到子菜单，就返回 OPEN_CHILD。
         * 真正创建/动画仍由 submenu_view.c 完成。
         */
        out_action->type = MENU_ACTION_OPEN_CHILD;
        out_action->child_menu = child;
        return true;
    }

    if (handle_gas_switch(row->id, out_action) ||
        handle_conservatism(row->id, out_action) ||
        handle_brightness(row->id, out_action) ||
        handle_compass(row->id, out_action) ||
        handle_light(row->id, row, out_action))
    {
        /* 已经被新 ID 分发处理的设置项，到这里就结束。
         * 这也是后续迁移更多设置项的推荐模式。
         */
        return true;
    }

    if (direct_setting_for_id(row->id, &setting))
    {
        submenu_apply_setting(setting.kind, setting.arg, setting.value);
        dispatch_setting_callback(&setting);
        out_action->type = (setting.kind == SUBMENU_SETTING_OC_TECH_SAVE)
                           ? MENU_ACTION_CLOSE : MENU_ACTION_REFRESH;
        return true;
    }

    if (confirm_setting_for_id(row->id, &setting))
    {
        s_pending_setting = setting;
        out_action->type = MENU_ACTION_SHOW_CONFIRM;
        out_action->modal_text = s_pending_setting.body;
        return true;
    }

    if (edit_spec_for_id(row->id, &out_action->edit_spec))
    {
        out_action->type = MENU_ACTION_BEGIN_EDIT;
        return true;
    }

    if (menu_runtime_current_id() == MENU_ALERTS_SETUP && row->id == MENU_ITEM_ALERT_NDL)
    {
        return true;
    }

    out_action->type = MENU_ACTION_SHOW_TEXT_MODAL;
    out_action->modal_text = row->label;
    return true;
}

void menu_actions_clear_pending(void)
{
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
}

bool menu_actions_confirm_pending(bool *out_close_parent_too,
                                  bool *out_return_dash)
{
    if (out_close_parent_too)
    {
        *out_close_parent_too = false;
    }
    if (out_return_dash)
    {
        *out_return_dash = false;
    }
    if (s_pending_setting.kind == SUBMENU_SETTING_NONE)
    {
        return false;
    }

    if (out_close_parent_too)
    {
        *out_close_parent_too =
            (s_pending_setting.kind == SUBMENU_SETTING_DIVE_MODE &&
             s_pending_setting.value != 0);
    }
    if (out_return_dash)
    {
        *out_return_dash =
            (s_pending_setting.kind == SUBMENU_SETTING_DIVE_MODE &&
             s_pending_setting.value == 3);
    }

    submenu_apply_setting(s_pending_setting.kind,
                          s_pending_setting.arg,
                          s_pending_setting.value);
    dispatch_setting_callback(&s_pending_setting);
    menu_actions_clear_pending();
    return true;
}
