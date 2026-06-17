/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_menu.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_menu.h"
#include "ui_vm_dashboard.h"
#include "ui_vm_system_view.h"
#include "../../views/submenu_model.h"
#include "../data.h"
#include "../ui_settings.h"

#include <stdio.h>
#include <string.h>

static const char *vm_salinity_label(uint8_t value)
{
    switch (value)
    {
    case 0U:
        return "FRESH";
    case 1U:
        return "SALT";
    case 2U:
        return "EN13319";
    default:
        return "--";
    }
}

static const char *vm_safety_stop_label(uint8_t value)
{
    return ui_safety_stop_label(value);
}

static void vm_format_depth_compact(char *buf, size_t buf_size, float depth_m)
{
    (void)snprintf(buf, buf_size, "%.0f%s", (double)bus_get_depth_display(depth_m), bus_get_depth_unit_label());
}

static const char *vm_altitude_label(uint8_t value)
{
    switch (value)
    {
    case 0U:
        return "SEA";
    case 1U:
        return "ALT1";
    case 2U:
        return "ALT2";
    case 3U:
        return "ALT3";
    default:
        return "--";
    }
}

static const char *vm_units_label(uint8_t value)
{
    return (value == 0U) ? "METRIC" : "IMPERIAL";
}

static const char *vm_bluetooth_label(uint8_t value)
{
    return (value == 0U) ? "OFF" : "ON";
}

static const char *vm_dive_mode_label(uint8_t mode)
{
    switch (mode)
    {
    case 0U:
        return "AIR";
    case 1U:
        return "NITROX";
    case 2U:
        return "3 GAS";
    case 3U:
        return "OC Tech";
    default:
        return "--";
    }
}

static const char *vm_ai_state_label(uint8_t state)
{
    switch (state)
    {
    case 1U:
        return "PAIRING";
    case 2U:
        return "PAIRED";
    default:
        return "UNPAIRED";
    }
}

static const char *vm_gtr_label(uint8_t enabled)
{
    return (enabled == 0U) ? "OFF" : "ON";
}

void ui_vm_setup_menu_update(ui_vm_setup_menu_t *vm)
{
    uint8_t cons;
    uint8_t brt;
    compass_cal_ui_state_t cal_state;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    cons = bus_get_conservatism();
    brt = bus_get_brightness();
    cal_state = get_compass_calibration_ui_state();

    (void)snprintf(vm->conservatism_badge,
                   sizeof(vm->conservatism_badge),
                   "%s",
                   submenu_conservatism_badge(cons));
    (void)snprintf(vm->brightness_badge,
                   sizeof(vm->brightness_badge),
                   "%s",
                   submenu_brightness_badge(brt));

    vm->compass_cal_state = cal_state;
    vm->compass_cal_badge_idx = 0U;
    if (cal_state == COMPASS_CAL_RUNNING)
    {
        vm->compass_cal_badge_idx = 1U;
    }
    else if (cal_state == COMPASS_CAL_READY)
    {
        vm->compass_cal_badge_idx = 2U;
    }
    else
    {
    }
}

void ui_vm_modal_gas_update(ui_vm_modal_gas_t *vm, uint8_t gas_cursor)
{
    uint8_t gas_count;
    uint8_t slot_idx;
    const char *gas_name;
    float mod_m;
    float depth_m;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->title, sizeof(vm->title), "%s", "CONFIRM GAS");

    gas_count = bus_get_gas_slot_count();
    if (gas_count == 0U)
    {
        (void)snprintf(vm->body, sizeof(vm->body), "%s", "NO ACTIVE GAS");
        (void)snprintf(vm->hint, sizeof(vm->hint), "%s", "[ ESC CANCEL ]");
        return;
    }

    slot_idx = (gas_cursor < gas_count) ? gas_cursor : 0U;
    gas_name = bus_get_gas_slot_name(slot_idx);
    mod_m = bus_get_gas_slot_mod_m(slot_idx);
    if (mod_m <= 0.0f)
    {
        mod_m = (float)GAS_MOD_M[slot_idx];
    }

    depth_m = bus_get_depth();
    (void)snprintf(vm->body,
                   sizeof(vm->body),
                   "%s\nMOD: %.0f%s",
                   (gas_name != NULL) ? gas_name : "--",
                   (double)bus_get_depth_display(mod_m),
                   bus_get_depth_unit_label());

    if (depth_m > mod_m)
    {
        vm->danger = 1U;
        (void)snprintf(vm->hint, sizeof(vm->hint), "%s", "[ FATAL: OVER MOD ]");
    }
    else
    {
        (void)snprintf(vm->hint,
                       sizeof(vm->hint),
                       "%s",
                       "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    }
    vm->valid = 1U;
}

void ui_vm_gas_switch_menu_update(ui_vm_gas_switch_menu_t *vm)
{
    uint8_t gas_count;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    gas_count = bus_get_gas_slot_count();
    if (gas_count > GAS_COUNT)
    {
        gas_count = GAS_COUNT;
    }

    if (gas_count == 0U)
    {
        (void)snprintf(vm->items[0], sizeof(vm->items[0]), "%s", "NO ACTIVE GAS");
        vm->count = 1U;
        return;
    }

    for (uint8_t i = 0U; i < gas_count; i++)
    {
        const char *slot_name = bus_get_gas_slot_name(i);

        if (gas_count > 1U)
        {
            (void)snprintf(vm->items[i],
                           sizeof(vm->items[i]),
                           "GAS %u: %s",
                           (unsigned)(i + 1U),
                           (slot_name != NULL) ? slot_name : "--");
        }
        else
        {
            (void)snprintf(vm->items[i],
                           sizeof(vm->items[i]),
                           "%s",
                           (slot_name != NULL) ? slot_name : "--");
        }
    }
    vm->count = gas_count;
}

void ui_vm_dive_setup_menu_update(ui_vm_dive_setup_menu_t *vm,
                                  uint8_t salinity_mode,
                                  uint8_t safety_stop_mode,
                                  uint8_t altitude_level)
{
    char safety_depth[12];
    char last_deco_depth[12];

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm_format_depth_compact(safety_depth, sizeof(safety_depth), 5.0f);
    vm_format_depth_compact(last_deco_depth, sizeof(last_deco_depth), (bus_get_last_deco_stop() == 6U) ? 6.0f : 3.0f);

    (void)snprintf(vm->items[0], sizeof(vm->items[0]), "SALINITY: %s", vm_salinity_label(salinity_mode));
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "MOD PO2: %.1f", (double)bus_get_mod_ppo2());
    if (safety_stop_mode == UI_SAFETY_STOP_OFF) (void)snprintf(vm->items[2], sizeof(vm->items[2]), "SAFETY STOP: %s", vm_safety_stop_label(safety_stop_mode));
    else (void)snprintf(vm->items[2], sizeof(vm->items[2]), "SAFETY STOP: %s @ %s", vm_safety_stop_label(safety_stop_mode), safety_depth);
    (void)snprintf(vm->items[3], sizeof(vm->items[3]), "LAST DECO: %s", last_deco_depth);
    (void)snprintf(vm->items[4], sizeof(vm->items[4]), "ALTITUDE: %s", vm_altitude_label(altitude_level));
    vm->count = 5U;
}

void ui_vm_dive_context_update(ui_vm_dive_context_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    vm->salinity_mode = bus_get_salinity_mode();
    if (vm->salinity_mode > 2U)
    {
        vm->salinity_mode = 0U;
    }

    vm->gf_low = bus_get_gf_low();
    vm->gf_high = bus_get_gf_high();

    vm->last_stop_depth_m = (bus_get_last_deco_stop() == 6U) ? 6U : 3U;
}

void ui_vm_alerts_menu_update(ui_vm_simple_menu_t *vm,
                              uint16_t depth_alarm_m,
                              uint16_t time_alarm_min,
                              uint8_t ndl_alarm_min)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->items[0],
                   sizeof(vm->items[0]),
                   "DEPTH ALARM: %.0f%s",
                   (double)bus_get_depth_display((float)depth_alarm_m),
                   bus_get_depth_unit_label());
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "TIME ALARM: %umin", (unsigned)time_alarm_min);
    (void)snprintf(vm->items[2], sizeof(vm->items[2]), "LOW NDL ALARM: %umin", (unsigned)ndl_alarm_min);
    vm->count = 3U;
}

void ui_vm_systems_setup_menu_update(ui_vm_simple_menu_t *vm, uint8_t dive_mode)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->items[0], sizeof(vm->items[0]), "%s", "VERSION: " SYSTEM_VERSION);
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "MODE SETUP: %s", vm_dive_mode_label(dive_mode));
    (void)snprintf(vm->items[2], sizeof(vm->items[2]), "%s", "DIVE SETUP");
    (void)snprintf(vm->items[3], sizeof(vm->items[3]), "%s", "AI SETUP");
    (void)snprintf(vm->items[4], sizeof(vm->items[4]), "%s", "ALERTS SETUP");
    (void)snprintf(vm->items[5], sizeof(vm->items[5]), "%s", "DISPLAY");
    vm->count = 6U;
}

void ui_vm_ai_menu_update(ui_vm_simple_menu_t *vm,
                          const uint8_t tank_state[2],
                          uint8_t gtr_enabled)
{
    uint8_t tank0 = 0U;
    uint8_t tank1 = 0U;

    if (vm == NULL)
    {
        return;
    }

    if (tank_state != NULL)
    {
        tank0 = tank_state[0];
        tank1 = tank_state[1];
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->items[0], sizeof(vm->items[0]), "T1 MAIN: %s", vm_ai_state_label(tank0));
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "T2 BUDDY: %s", vm_ai_state_label(tank1));
    (void)snprintf(vm->items[2], sizeof(vm->items[2]), "GTR MODE: %s", vm_gtr_label(gtr_enabled));
    vm->count = 3U;
}

void ui_vm_display_menu_update(ui_vm_simple_menu_t *vm,
                               uint8_t units_mode,
                               uint8_t temperature_unit,
                               uint8_t log_rate_s,
                               uint8_t bluetooth_enabled)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->items[0], sizeof(vm->items[0]), "UNITS: %s", vm_units_label(units_mode));
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "TEMP: %s", ui_temp_unit_label(temperature_unit));
    (void)snprintf(vm->items[2], sizeof(vm->items[2]), "%s", "Time/date");
    (void)snprintf(vm->items[3], sizeof(vm->items[3]), "LOG RATE: %us", (unsigned)log_rate_s);
    (void)snprintf(vm->items[4], sizeof(vm->items[4]), "BLUETOOTH: %s", vm_bluetooth_label(bluetooth_enabled));
    (void)snprintf(vm->items[5], sizeof(vm->items[5]), "%s", "RESET DEFAULTS");
    vm->count = 6U;
}

void ui_vm_datetime_menu_update(ui_vm_simple_menu_t *vm,
                                uint16_t year,
                                uint8_t month,
                                uint8_t day,
                                uint8_t hour,
                                uint8_t minute)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    (void)snprintf(vm->items[0], sizeof(vm->items[0]), "YEAR: %04u", (unsigned)year);
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "MONTH: %02u", (unsigned)month);
    (void)snprintf(vm->items[2], sizeof(vm->items[2]), "DAY: %02u", (unsigned)day);
    (void)snprintf(vm->items[3], sizeof(vm->items[3]), "HOUR: %02u", (unsigned)hour);
    (void)snprintf(vm->items[4], sizeof(vm->items[4]), "MINUTE: %02u", (unsigned)minute);
    vm->count = 5U;
}

void ui_vm_nitrox_menu_update(ui_vm_simple_menu_t *vm, uint8_t nitrox_o2_pct)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    if (nitrox_o2_pct == 21U)
    {
        (void)snprintf(vm->items[0], sizeof(vm->items[0]), "%s", "G1: AIR");
    }
    else
    {
        (void)snprintf(vm->items[0], sizeof(vm->items[0]), "G1: EAN%u", (unsigned)nitrox_o2_pct);
    }
    (void)snprintf(vm->items[1], sizeof(vm->items[1]), "%s", "CONFIRM & ACTIVATE");
    vm->count = 2U;
}

void ui_vm_three_gas_menu_update(ui_vm_simple_menu_t *vm,
                                 const uint8_t three_gas_o2_pct[3])
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    for (uint8_t i = 0U; i < 3U; i++)
    {
        const uint8_t value = (three_gas_o2_pct != NULL) ? three_gas_o2_pct[i] : 0U;
        if (value == 21U)
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: AIR", (unsigned)(i + 1U));
        }
        else if (value == 100U)
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: O2 100%%", (unsigned)(i + 1U));
        }
        else
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: EAN%u", (unsigned)(i + 1U), (unsigned)value);
        }
    }
    (void)snprintf(vm->items[3], sizeof(vm->items[3]), "%s", "CONFIRM & ACTIVATE");
    vm->count = 4U;
}

void ui_vm_oc_tech_menu_update(ui_vm_simple_menu_t *vm,
                               const uint8_t o2_pct[5],
                               const uint8_t he_pct[5])
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    for (uint8_t i = 0U; i < GAS_COUNT; i++)
    {
        uint8_t o2 = (o2_pct != NULL) ? o2_pct[i] : 0U;
        uint8_t he = (he_pct != NULL) ? he_pct[i] : 0U;

        if (((uint16_t)o2 + (uint16_t)he) > 100U)
        {
            he = (o2 < 100U) ? (uint8_t)(100U - o2) : 0U;
        }

        if (o2 == 0U)
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: OFF", (unsigned)(i + 1U));
        }
        else if (he > 0U)
        {
            (void)snprintf(vm->items[i],
                           sizeof(vm->items[i]),
                           "G%u: TX %u/%u",
                           (unsigned)(i + 1U),
                           (unsigned)o2,
                           (unsigned)he);
        }
        else if (o2 == 21U)
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: AIR", (unsigned)(i + 1U));
        }
        else if (o2 == 100U)
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: O2 100%%", (unsigned)(i + 1U));
        }
        else
        {
            (void)snprintf(vm->items[i], sizeof(vm->items[i]), "G%u: NX %u", (unsigned)(i + 1U), (unsigned)o2);
        }
    }
    (void)snprintf(vm->items[GAS_COUNT], sizeof(vm->items[GAS_COUNT]), "%s", "CONFIRM & ACTIVATE");
    vm->count = GAS_COUNT + 1U;
}

void ui_vm_edit_mod_ppo2_update(ui_vm_edit_spec_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = bus_get_mod_ppo2();
    vm->min = 1.0f;
    vm->max = 1.6f;
    vm->step = 0.1f;
    vm->decimals = 1U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "MOD PO2:");
}

void ui_vm_edit_nitrox_o2_update(ui_vm_edit_spec_t *vm, uint8_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->min = 21.0f;
    vm->max = 40.0f;
    vm->step = 1.0f;
    vm->decimals = 0U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "O2:");
}

void ui_vm_edit_three_gas_o2_update(ui_vm_edit_spec_t *vm, uint8_t item_index, uint8_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->min = 21.0f;
    vm->max = 100.0f;
    vm->step = 1.0f;
    vm->decimals = 0U;
    (void)snprintf(vm->label, sizeof(vm->label), "GAS %u:", (unsigned)(item_index + 1U));
}

void ui_vm_edit_oc_tech_gas_update(ui_vm_edit_spec_t *vm,
                                   uint8_t slot,
                                   uint8_t item_index,
                                   uint8_t o2,
                                   uint8_t he)
{
    (void)slot;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->step = 1.0f;
    vm->decimals = 0U;

    if (item_index == 1U)
    {
        vm->value = (float)he;
        vm->min = 0.0f;
        vm->max = (float)(100U - o2);
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "HE PERCENT:");
    }
    else
    {
        vm->value = (float)o2;
        vm->min = 8.0f;
        vm->max = (float)(100U - he);
        if (vm->max < vm->min)
        {
            vm->max = vm->min;
        }
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "O2 PERCENT:");
    }
}

void ui_vm_edit_gas_ppo2_update(ui_vm_edit_spec_t *vm, float value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = value;
    vm->min = 1.0f;
    vm->max = 1.6f;
    vm->step = 0.1f;
    vm->decimals = 1U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "PO2:");
}

void ui_vm_edit_depth_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->min = 10.0f;
    vm->max = 150.0f;
    vm->step = 10.0f;
    vm->decimals = 0U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "DEPTH:");
}

void ui_vm_edit_time_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->min = 10.0f;
    vm->max = 300.0f;
    vm->step = 10.0f;
    vm->decimals = 0U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "TIME:");
}

void ui_vm_edit_ndl_alarm_update(ui_vm_edit_spec_t *vm, uint16_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->min = 0.0f;
    vm->max = 80.0f;
    vm->step = 1.0f;
    vm->decimals = 0U;
    (void)snprintf(vm->label, sizeof(vm->label), "%s", "NDL:");
}

void ui_vm_edit_datetime_update(ui_vm_edit_spec_t *vm, uint8_t field, uint16_t value)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->value = (float)value;
    vm->step = 1.0f;
    vm->decimals = 0U;

    switch (field)
    {
    case 0U:
        vm->min = 2000.0f;
        vm->max = 2099.0f;
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "YEAR:");
        break;
    case 1U:
        vm->min = 1.0f;
        vm->max = 12.0f;
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "MONTH:");
        break;
    case 2U:
        vm->min = 1.0f;
        vm->max = 31.0f;
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "DAY:");
        break;
    case 3U:
        vm->min = 0.0f;
        vm->max = 23.0f;
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "HOUR:");
        break;
    case 4U:
        vm->min = 0.0f;
        vm->max = 59.0f;
        (void)snprintf(vm->label, sizeof(vm->label), "%s", "MINUTE:");
        break;
    default:
        break;
    }
}
