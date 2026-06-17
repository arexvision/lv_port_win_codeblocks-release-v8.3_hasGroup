/*
 * 文件: src/app_ui/ui/views/menu_actions.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "menu_actions.h"

#include "../core/callbacks.h"
#include "../core/data.h"
#include "../core/ui_state.h"
#include "../screen/screen.h"
#include "menu_runtime.h"
#include "submenu_model.h"
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
    /* 这里是“菜单层 -> 业务层”的桥。
     * 前面的 view/runtime/actions 只处理菜单语义，
     * 到这里才真正调用 ui_on_* 回调，把修改落到系统配置、算法或外设控制。 */
    if (!setting)
    {
        return;
    }

    switch (setting->kind)
    {
    case SUBMENU_SETTING_DIVE_MODE:
        ui_on_dive_mode_set((uint8_t)setting->value);
        screen_refresh_setup_menu();
        screen_refresh_gas_menu();
        screen_refresh_left_panel();
        break;
    case SUBMENU_SETTING_SALINITY:
        ui_on_salinity_set((uint8_t)setting->value);
        submenu_reapply_current_gas_profile();
        break;
    case SUBMENU_SETTING_SAFETY_STOP:
        ui_on_safety_stop_mode_set((uint8_t)setting->value);
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
    case SUBMENU_SETTING_TEMP_UNIT:
        ui_on_temperature_unit_set((uint8_t)setting->value);
        screen_refresh_left_panel();
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
    case SUBMENU_SETTING_TIME_24H:
        ui_on_time_24h_set(setting->value != 0U);
        screen_refresh_setup_menu();
        break;
    case SUBMENU_SETTING_DATE_FORMAT:
        ui_on_date_format_set((uint8_t)setting->value);
        screen_refresh_setup_menu();
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

static bool direct_setting_for_row(uint8_t row_index,
                                   const menu_row_t *row,
                                   submenu_setting_confirm_t *out_setting)
{
    /* 优先按稳定 ID 查直达设置，兼容旧逻辑时再退回标题/文本匹配。 */
    if (row != NULL && submenu_direct_setting_from_ids(menu_runtime_current_id(),
                                                       row->id,
                                                       out_setting))
    {
        return true;
    }
    return submenu_direct_setting_from_selection(menu_runtime_current_title(),
                                                 row_index,
                                                 row ? row->label : NULL,
                                                 out_setting);
}

static bool confirm_setting_for_row(uint8_t row_index,
                                    const menu_row_t *row,
                                    submenu_setting_confirm_t *out_setting)
{
    /* 需要二次确认的设置在这里生成确认上下文。 */
    if (row != NULL && submenu_setting_from_ids(menu_runtime_current_id(),
                                                row->id,
                                                out_setting))
    {
        return true;
    }
    return submenu_setting_from_selection(menu_runtime_current_title(),
                                          row_index,
                                          row ? row->label : NULL,
                                          out_setting);
}

static bool edit_spec_for_row(uint8_t row_index,
                              const menu_row_t *row,
                              submenu_edit_spec_t *out_spec)
{
    /* 行内编辑类条目需要先拿到编辑规格，再交给 screen 层进入编辑态。 */
    if (row != NULL && submenu_edit_spec_from_ids(menu_runtime_current_id(),
                                                  row->id,
                                                  out_spec))
    {
        return true;
    }
    return submenu_edit_spec_from_selection(menu_runtime_current_title(),
                                            row_index,
                                            row ? row->label : NULL,
                                            out_spec);
}

static bool handle_conservatism(menu_item_id_t id, menu_action_t *action)
{
    /* 保守度是直接生效型菜单，选中即落配置并关闭子菜单。 */
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
    /* 亮度选择同样是一步生效，不需要额外确认页。 */
    uint8_t index;
    const brightness_option_t *option;

    if (id < MENU_ITEM_BRIGHTNESS_LOW || id > MENU_ITEM_BRIGHTNESS_MAX)
    {
        return false;
    }

    index = (uint8_t)(id - MENU_ITEM_BRIGHTNESS_LOW);
    option = submenu_brightness_option(index);
    bus_set_brightness(option->value);
    set_brightness(option->value);
    screen_update_setup_badge(2, option->badge_label);
    action->type = MENU_ACTION_CLOSE;
    return true;
}

static bool handle_gas_switch(menu_item_id_t id, menu_action_t *action)
{
    /* 气体切换先记录光标和来源上下文，再弹出确认模态框。 */
    if (id < MENU_ITEM_GAS_SLOT_0 || id > MENU_ITEM_GAS_SLOT_4)
    {
        return false;
    }
    ui_state_set_gas_cursor((uint8_t)(id - MENU_ITEM_GAS_SLOT_0));
    ui_state_set_gas_modal_from_submenu(true);
    action->type = MENU_ACTION_SHOW_GAS_MODAL;
    return true;
}

static bool handle_compass(menu_item_id_t id, menu_action_t *action)
{
    /* 罗盘校准菜单本质上是向传感器任务投递命令。 */
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

static bool handle_light(menu_item_id_t id, const menu_row_t *row, menu_action_t *action)
{
    /* 灯光菜单既支持总开关，也支持颜色/亮度类子项。 */
    (void)row;
    if (id == MENU_ITEM_LIGHT_POWER)
    {
        bus_toggle_light_power();
        action->type = MENU_ACTION_REFRESH;
        return true;
    }
    if (id == MENU_ITEM_LIGHT_MODE)
    {
        bus_toggle_light_mode();
        action->type = MENU_ACTION_REFRESH;
        return true;
    }

    if (id >= MENU_ITEM_LIGHT_LEVEL_10 && id <= MENU_ITEM_LIGHT_LEVEL_100)
    {
        ui_on_light_color_set(menu_defs_light_color_name(menu_runtime_current_id()),
                              (id == MENU_ITEM_LIGHT_LEVEL_10)  ? "10%" :
                              (id == MENU_ITEM_LIGHT_LEVEL_30)  ? "30%" :
                              (id == MENU_ITEM_LIGHT_LEVEL_50)  ? "50%" :
                              (id == MENU_ITEM_LIGHT_LEVEL_70)  ? "70%" : "100%");
        action->type = MENU_ACTION_CLOSE;
        return true;
    }
    return false;
}

static bool handle_dive_plan(uint8_t row_index, const menu_row_t *row, menu_action_t *action)
{
    /* 潜水计划是菜单系统里的特殊页，内部交互不完全遵循普通列表点击规则。 */
    /* 它本质上更像“向导页面”，而不是简单菜单：
     * 同一行在不同 page 阶段可能代表 NEXT / PLAN / MORE / EXIT。 */
    bool close_submenu = false;
    uint8_t keep_idx = row_index;

    if (!menu_runtime_is_dive_plan())
    {
        return false;
    }
    if (!submenu_dive_plan_handle_action(row ? row->id : MENU_ITEM_NONE,
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
    /* 菜单点击入口：先判断是直接动作、确认动作还是需要下钻。 */
    /* 整个动作分发优先级大致是：
     * BACK -> 特殊页(DIVE PLAN) -> 只读拦截 -> 子菜单跳转 ->
     * 新 ID 直达动作 -> 行内编辑 -> 直接生效设置 -> 二次确认设置 -> 默认文本弹窗
     * 这个顺序体现了“先处理结构性导航，再处理业务动作”的原则。 */
    submenu_setting_confirm_t setting;
    menu_id_t child;

    action_clear(out_action);
    if (!row || !out_action)
    {
        return false;
    }
    out_action->keep_index = row_index;

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
        /* 注意这里不直接切状态，保持 action 层只产生命令，不直接操作界面。 */
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

    if (edit_spec_for_row(row_index, row, &out_action->edit_spec))
    {
        out_action->type = MENU_ACTION_BEGIN_EDIT;
        return true;
    }

    if (direct_setting_for_row(row_index, row, &setting))
    {
        submenu_apply_setting(setting.kind, setting.arg, setting.value);
        dispatch_setting_callback(&setting);
        if (setting.kind == SUBMENU_SETTING_DATE_FORMAT)
        {
            out_action->type = MENU_ACTION_BACK;
        }
        else if (setting.kind == SUBMENU_SETTING_OC_TECH_SAVE)
        {
            out_action->type = MENU_ACTION_CLOSE;
        }
        else
        {
            out_action->type = MENU_ACTION_REFRESH;
        }
        return true;
    }

    if (confirm_setting_for_row(row_index, row, &setting))
    {
        s_pending_setting = setting;
        out_action->type = MENU_ACTION_SHOW_CONFIRM;
        out_action->modal_text = s_pending_setting.body;
        return true;
    }

    out_action->type = MENU_ACTION_SHOW_TEXT_MODAL;
    out_action->modal_text = row->label;
    return true;
}

void menu_actions_clear_pending(void)
{
    /* 清空待确认动作，避免跨页面残留。 */
    /* pending setting 属于短生命周期上下文，一旦页面关闭或确认结束必须清掉，
     * 否则确认框可能把上一页的设置错误地应用到当前页。 */
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
}

bool menu_actions_confirm_pending(bool *out_close_parent_too,
                                  bool *out_return_dash)
{
    /* 用户在确认弹窗里按下确认后，统一在这里落地。 */
    /* 二次确认的意义在于把“用户当前高亮了哪个选项”和“真正提交哪个设置”
     * 解耦开：先缓存，再由 modal 确认后一次性提交。 */
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
        *out_return_dash = false;
    }

    submenu_apply_setting(s_pending_setting.kind,
                          s_pending_setting.arg,
                          s_pending_setting.value);
    dispatch_setting_callback(&s_pending_setting);
    menu_actions_clear_pending();
    return true;
}
