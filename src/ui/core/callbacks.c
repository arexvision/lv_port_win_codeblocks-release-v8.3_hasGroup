/*
 * 文件: src/app_ui/ui/core/callbacks.c
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "callbacks.h"
#include "data.h"
#include "ui_settings.h"
#include "../../config/build/ui_debug_flags.h"
#include <stdio.h>

#ifdef PC_SIMULATOR
#include "../../algo_sim/deco_core.h"
#include "../../hal_sim/sim_data.h"
#endif

#ifndef PC_SIMULATOR
#define WEAK_CALLBACK __attribute__((weak))
#else
#define WEAK_CALLBACK
#endif

static uint32_t light_color_to_rgb(light_color_t color)
{
    switch (color)
    {
    case LIGHT_COLOR_RED:   return 0xFF0000UL;
    case LIGHT_COLOR_GREEN: return 0x00FF00UL;
    case LIGHT_COLOR_BLUE:  return 0x0000FFUL;
    case LIGHT_COLOR_WHITE: return 0xFFFFFFUL;
    case LIGHT_COLOR_CUSTOM:
    default:                return g_light_rgb_state;
    }
}

static light_color_t light_rgb_to_color(uint32_t rgb)
{
    switch (rgb & 0x00FFFFFFUL)
    {
    case 0xFF0000UL: return LIGHT_COLOR_RED;
    case 0x00FF00UL: return LIGHT_COLOR_GREEN;
    case 0x0000FFUL: return LIGHT_COLOR_BLUE;
    case 0xFFFFFFUL: return LIGHT_COLOR_WHITE;
    default:         return LIGHT_COLOR_CUSTOM;
    }
}

/* 这些 weak 回调是 UI 层的默认落地实现。
 * 真机平台可以覆盖它们，把 UI 操作接到真实业务或硬件服务。 */
#ifdef PC_SIMULATOR
static uint8_t s_dive_mode = 0U;
static float s_air_ppo2 = 1.4f;
static uint8_t s_nitrox_o2_pct = 32U;
static float s_nitrox_ppo2 = 1.4f;
static uint8_t s_three_gas_o2_pct[3] = { 21U, 32U, 100U };
static float s_three_gas_ppo2[3] = { 1.4f, 1.4f, 1.4f };
static uint8_t s_three_gas_active[3] = { 1U, 1U, 1U };
static uint8_t s_oc_tech_o2_pct[5] = { 18U, 21U, 35U, 50U, 100U };
static uint8_t s_oc_tech_he_pct[5] = { 45U, 35U, 25U, 0U, 0U };
static float s_oc_tech_ppo2[5] = { 1.4f, 1.4f, 1.4f, 1.4f, 1.4f };
static uint8_t s_oc_tech_active[5] = { 1U, 1U, 1U, 1U, 1U };
static uint8_t s_units_mode = 0U;
static uint8_t s_temperature_unit = UI_TEMP_UNIT_DEFAULT;
static uint8_t s_bluetooth_enabled = 0U;
static uint16_t s_datetime_year = 2026U;
static uint8_t s_datetime_month = 1U;
static uint8_t s_datetime_day = 1U;
static uint8_t s_datetime_hour = 0U;
static uint8_t s_datetime_minute = 0U;
#endif

WEAK_CALLBACK
void bus_set_light_power(bool on)
{
    UI_CALLBACK_TRACE("[LIGHT] Power: %s\n", on ? "ON" : "OFF");
}

WEAK_CALLBACK
bool bus_get_light_power(void)
{
    return g_light_power_state;
}

WEAK_CALLBACK
void bus_toggle_light_power(void)
{
    /* 默认实现仅维护 UI 侧电源状态并回调底层设置接口。 */
    g_light_power_state = !g_light_power_state;
    bus_set_light_power(g_light_power_state);
}

WEAK_CALLBACK
void bus_set_light_mode(light_mode_t mode)
{
    UI_CALLBACK_TRACE("[LIGHT] Mode: %s\n", mode == LIGHT_MODE_BREATH ? "BREATH" : "ALWAYS");
}

WEAK_CALLBACK
light_mode_t bus_get_light_mode(void)
{
    return g_light_mode_state;
}

WEAK_CALLBACK
void bus_toggle_light_mode(void)
{
    g_light_mode_state = (g_light_mode_state == LIGHT_MODE_BREATH)
                         ? LIGHT_MODE_ALWAYS
                         : LIGHT_MODE_BREATH;
    bus_set_light_mode(g_light_mode_state);
}

const char *bus_get_light_color_label(void)
{
    switch (bus_get_light_color())
    {
    case LIGHT_COLOR_RED:   return "RED";
    case LIGHT_COLOR_GREEN: return "GREEN";
    case LIGHT_COLOR_BLUE:  return "BLUE";
    case LIGHT_COLOR_CUSTOM: return "CUSTOM";
    case LIGHT_COLOR_WHITE:
    default:                return "WHITE";
    }
}

const char *bus_get_light_level_label(void)
{
    switch (bus_get_light_level())
    {
    case LIGHT_LEVEL_10:  return "10%";
    case LIGHT_LEVEL_30:  return "30%";
    case LIGHT_LEVEL_50:  return "50%";
    case LIGHT_LEVEL_70:  return "70%";
    case LIGHT_LEVEL_100:
    default:              return "100%";
    }
}

WEAK_CALLBACK
void bus_set_light_color(light_color_t color)
{
    g_light_color_state = color;
    g_light_rgb_state = light_color_to_rgb(color);
    UI_CALLBACK_TRACE("[LIGHT] Color: %s\n", bus_get_light_color_label());
    ui_on_light_color_set(bus_get_light_color_label(), bus_get_light_level_label());
}

WEAK_CALLBACK
void bus_preview_light_color(light_color_t color)
{
    g_light_color_state = color;
    g_light_rgb_state = light_color_to_rgb(color);
    UI_CALLBACK_TRACE("[LIGHT] Preview Color: %s\n", bus_get_light_color_label());
    ui_on_light_color_preview(bus_get_light_color_label(), bus_get_light_level_label());
}

WEAK_CALLBACK
light_color_t bus_get_light_color(void)
{
    return g_light_color_state;
}

WEAK_CALLBACK
void bus_set_light_rgb(uint32_t rgb)
{
    g_light_rgb_state = rgb & 0x00FFFFFFUL;
    g_light_color_state = light_rgb_to_color(g_light_rgb_state);
    UI_CALLBACK_TRACE("[LIGHT] RGB Color: 0x%06X\n", (unsigned int)g_light_rgb_state);
    ui_on_light_rgb_set(g_light_rgb_state, bus_get_light_level_label());
}

WEAK_CALLBACK
void bus_preview_light_rgb(uint32_t rgb)
{
    g_light_rgb_state = rgb & 0x00FFFFFFUL;
    g_light_color_state = light_rgb_to_color(g_light_rgb_state);
    UI_CALLBACK_TRACE("[LIGHT] Preview RGB Color: 0x%06X\n", (unsigned int)g_light_rgb_state);
    ui_on_light_rgb_preview(g_light_rgb_state, bus_get_light_level_label());
}

WEAK_CALLBACK
uint32_t bus_get_light_rgb(void)
{
    return g_light_rgb_state & 0x00FFFFFFUL;
}

WEAK_CALLBACK
void bus_set_light_level(light_level_t level)
{
    g_light_level_state = level;
    UI_CALLBACK_TRACE("[LIGHT] Level: %s\n", bus_get_light_level_label());
    ui_on_light_rgb_set(bus_get_light_rgb(), bus_get_light_level_label());
}

WEAK_CALLBACK
light_level_t bus_get_light_level(void)
{
    return g_light_level_state;
}

WEAK_CALLBACK
void ui_on_light_color_set(const char *color, const char *level)
{
    UI_CALLBACK_TRACE("[LIGHT] Color: %s, Level: %s\n", color, level);
}

WEAK_CALLBACK
void ui_on_light_color_preview(const char *color, const char *level)
{
    UI_CALLBACK_TRACE("[LIGHT] Preview Color: %s, Level: %s\n", color, level);
}

WEAK_CALLBACK
void ui_on_light_rgb_set(uint32_t rgb, const char *level)
{
    UI_CALLBACK_TRACE("[LIGHT] RGB Color: 0x%06X, Level: %s\n", (unsigned int)(rgb & 0x00FFFFFFUL), level);
}

WEAK_CALLBACK
void ui_on_light_rgb_preview(uint32_t rgb, const char *level)
{
    UI_CALLBACK_TRACE("[LIGHT] Preview RGB Color: 0x%06X, Level: %s\n", (unsigned int)(rgb & 0x00FFFFFFUL), level);
}

WEAK_CALLBACK
void set_brightness(uint8_t level)
{
    /* 默认亮度策略仍走软件遮罩路径，平台侧可覆盖成真实硬件亮度。 */
    apply_software_brightness(level);
}

WEAK_CALLBACK
void ui_on_conservatism_set(uint8_t level)
{
    /* 保守度直接映射到 data bus。 */
    bus_set_conservatism(level);
}

WEAK_CALLBACK
void ui_on_salinity_set(uint8_t mode)
{
    bus_set_salinity_mode(mode);
}

WEAK_CALLBACK
void ui_on_safety_stop_mode_set(uint8_t mode)
{
    bus_set_safety_stop_mode(mode);
    UI_CALLBACK_TRACE("[DIVE_SETUP] Safety stop: %s\n", ui_safety_stop_label(mode));
}

WEAK_CALLBACK
void ui_on_surface_confirm_min_set(uint8_t minutes)
{
    bus_set_surface_confirm_min(minutes);
    UI_CALLBACK_TRACE("[DIVE_SETUP] Dive end time: %umin\n", (unsigned)minutes);
}

WEAK_CALLBACK
void ui_on_dive_start_depth_set(float depth_m)
{
    bus_set_dive_start_depth_m(depth_m);
    UI_CALLBACK_TRACE("[DIVE_SETUP] Dive start depth: %.1fm\n", (double)depth_m);
}

WEAK_CALLBACK
void ui_on_depth_comp_enabled_set(bool enabled)
{
    bus_set_depth_comp_enabled(enabled);
    UI_CALLBACK_TRACE("[DIVE_SETUP] Depth compensation: %s\n", enabled ? "ON" : "OFF");
}

WEAK_CALLBACK
void ui_on_depth_comp_value_set(float depth_m)
{
    bus_set_depth_comp_m(depth_m);
    UI_CALLBACK_TRACE("[DIVE_SETUP] Depth compensation value: %.1fm\n", (double)depth_m);
}

WEAK_CALLBACK
void ui_on_last_deco_stop_set(uint8_t depth_m)
{
    /* 最后减压停留深度由 data 层统一持有。 */
    bus_set_last_deco_stop(depth_m);
}

WEAK_CALLBACK
void ui_on_dive_mode_set(uint8_t mode)
{
    static const char *labels[] = { "AIR", "NITROX", "3 GAS", "OC Tech" };
    if (mode >= (sizeof(labels) / sizeof(labels[0])))
    {
        mode = 0;
    }
#ifdef PC_SIMULATOR
    s_dive_mode = mode;
#endif
    UI_CALLBACK_TRACE("[SYSTEM_SETUP] Dive mode: %s\n", labels[mode]);
}

WEAK_CALLBACK
void ui_on_gas_profile_commit(void)
{
    /* 默认实现只保留 UI data.c 的显示态更新。
     * 真机平台可覆盖此 hook，把 gas profile 同步到真实算法运行态。 */
}

WEAK_CALLBACK
float ui_calculate_gas_mod(uint8_t o2_pct, uint8_t he_pct, float max_ppo2)
{
#ifdef PC_SIMULATOR
    return deco_core_calculate_gas_mod(o2_pct, he_pct, max_ppo2);
#else
    (void)o2_pct;
    (void)he_pct;
    (void)max_ppo2;
    return 0.0f;
#endif
}

WEAK_CALLBACK
void ui_on_air_ppo2_set(float ppo2)
{
#ifdef PC_SIMULATOR
    s_air_ppo2 = ppo2;
#else
    (void)ppo2;
#endif
}

WEAK_CALLBACK
void ui_on_nitrox_o2_set(uint8_t o2_pct)
{
#ifdef PC_SIMULATOR
    s_nitrox_o2_pct = o2_pct;
#else
    (void)o2_pct;
#endif
}

WEAK_CALLBACK
void ui_on_nitrox_ppo2_set(float ppo2)
{
#ifdef PC_SIMULATOR
    s_nitrox_ppo2 = ppo2;
#else
    (void)ppo2;
#endif
}

WEAK_CALLBACK
void ui_on_three_gas_o2_set(uint8_t slot, uint8_t o2_pct)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_three_gas_o2_pct) / sizeof(s_three_gas_o2_pct[0])))
    {
        s_three_gas_o2_pct[slot] = o2_pct;
    }
#else
    (void)slot;
    (void)o2_pct;
#endif
}

WEAK_CALLBACK
void ui_on_three_gas_ppo2_set(uint8_t slot, float ppo2)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_three_gas_ppo2) / sizeof(s_three_gas_ppo2[0])))
    {
        s_three_gas_ppo2[slot] = ppo2;
    }
#else
    (void)slot;
    (void)ppo2;
#endif
}

WEAK_CALLBACK
void ui_on_three_gas_active_set(uint8_t slot, bool active)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_three_gas_active) / sizeof(s_three_gas_active[0])))
    {
        s_three_gas_active[slot] = active ? 1U : 0U;
    }
#else
    (void)slot;
    (void)active;
#endif
}

WEAK_CALLBACK
void ui_on_oc_tech_gas_set(uint8_t slot, uint8_t o2_pct, uint8_t he_pct)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_oc_tech_o2_pct) / sizeof(s_oc_tech_o2_pct[0])))
    {
        s_oc_tech_o2_pct[slot] = o2_pct;
        s_oc_tech_he_pct[slot] = he_pct;
    }
#else
    (void)slot;
    (void)o2_pct;
    (void)he_pct;
#endif
}

WEAK_CALLBACK
void ui_on_oc_tech_ppo2_set(uint8_t slot, float ppo2)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_oc_tech_ppo2) / sizeof(s_oc_tech_ppo2[0])))
    {
        s_oc_tech_ppo2[slot] = ppo2;
    }
#else
    (void)slot;
    (void)ppo2;
#endif
}

WEAK_CALLBACK
void ui_on_oc_tech_active_set(uint8_t slot, bool active)
{
#ifdef PC_SIMULATOR
    if (slot < (sizeof(s_oc_tech_active) / sizeof(s_oc_tech_active[0])))
    {
        s_oc_tech_active[slot] = active ? 1U : 0U;
    }
#else
    (void)slot;
    (void)active;
#endif
}

WEAK_CALLBACK
void ui_on_ai_pair(uint8_t tank_index)
{
    UI_CALLBACK_TRACE("[SYSTEM_SETUP] Pair AI tank: T%u\n", (unsigned)(tank_index + 1));
}

WEAK_CALLBACK
void ui_on_ai_tank_state_set(uint8_t tank_index, uint8_t state)
{
    static const char *labels[] = { "UNPAIRED", "PAIRING", "PAIRED" };
    if (state >= (sizeof(labels) / sizeof(labels[0])))
    {
        state = 0;
    }
    UI_CALLBACK_TRACE("[SYSTEM_SETUP] AI T%u: %s\n", (unsigned)(tank_index + 1), labels[state]);
}

WEAK_CALLBACK
void ui_on_gtr_mode_set(bool enabled)
{
    UI_CALLBACK_TRACE("[SYSTEM_SETUP] GTR mode: %s\n", enabled ? "ON" : "OFF");
}

WEAK_CALLBACK
void ui_on_mod_ppo2_set(float ppo2)
{
    bus_set_mod_ppo2(ppo2);
    UI_CALLBACK_TRACE("[DIVE_SETUP] MOD PPO2: %.1f\n", (double)ppo2);
}

WEAK_CALLBACK
void ui_on_depth_alarm_set(uint16_t depth_m)
{
    bus_set_depth_alarm_m(depth_m);
    UI_CALLBACK_TRACE("[ALERT_SETUP] Depth alarm: %um\n", depth_m);
}

WEAK_CALLBACK
void ui_on_time_alarm_set(uint16_t minutes)
{
    bus_set_time_alarm_min(minutes);
    UI_CALLBACK_TRACE("[ALERT_SETUP] Time alarm: %umin\n", minutes);
}

WEAK_CALLBACK
void ui_on_ndl_alarm_set(uint16_t minutes)
{
    bus_set_ndl_alarm_min(minutes);
    UI_CALLBACK_TRACE("[ALERT_SETUP] NDL alarm: %umin\n", minutes);
}

WEAK_CALLBACK
void ui_on_vibration_test(void)
{
    UI_CALLBACK_TRACE("[ALERT_SETUP] Test vibration\n");
}

WEAK_CALLBACK
void ui_on_units_set(uint8_t units)
{
#ifdef PC_SIMULATOR
    s_units_mode = (units == UI_UNITS_IMPERIAL) ? UI_UNITS_IMPERIAL : UI_UNITS_METRIC;
#endif
    bus_set_units_mode(units);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Units: %s\n", units == UI_UNITS_IMPERIAL ? "IMPERIAL" : "METRIC");
}

WEAK_CALLBACK
void ui_on_temperature_unit_set(uint8_t unit)
{
#ifdef PC_SIMULATOR
    s_temperature_unit = (unit == UI_TEMP_UNIT_F) ? UI_TEMP_UNIT_F : UI_TEMP_UNIT_C;
#endif
    bus_set_temperature_unit(unit);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Temp unit: %s\n", ui_temp_unit_label(unit));
}

WEAK_CALLBACK
void ui_on_datetime_field_set(uint8_t field, uint16_t value)
{
    static const char *labels[] = { "YEAR", "MONTH", "DAY", "HOUR", "MINUTE" };
#ifdef PC_SIMULATOR
    switch (field)
    {
    case 0: s_datetime_year = value; break;
    case 1: s_datetime_month = (uint8_t)value; break;
    case 2: s_datetime_day = (uint8_t)value; break;
    case 3: s_datetime_hour = (uint8_t)value; break;
    case 4: s_datetime_minute = (uint8_t)value; break;
    default: break;
    }
#endif
    if (field == 3U || field == 4U)
    {
        uint8_t hour = (field == 3U) ? (uint8_t)value : (uint8_t)bus_get_sys_time_h();
        uint8_t minute = (field == 4U) ? (uint8_t)value : (uint8_t)bus_get_sys_time_m();
        bus_set_sys_time(hour, minute, (uint8_t)bus_get_sys_time_s());
    }
    if (field >= (sizeof(labels) / sizeof(labels[0])))
    {
        field = 0;
    }
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Date/time %s: %u\n", labels[field], value);
}

WEAK_CALLBACK
void ui_on_datetime_action(uint8_t action)
{
    static const char *labels[] = { "SYNC CURRENT TIME" };
    if (action >= (sizeof(labels) / sizeof(labels[0])))
    {
        action = 0;
    }
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Date/time action: %s\n", labels[action]);
}

WEAK_CALLBACK
void ui_on_time_24h_set(bool enabled)
{
    bus_set_time_24h_enabled(enabled);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Time format: %s\n", enabled ? "24-hour" : "12-hour");
}

WEAK_CALLBACK
void ui_on_date_format_set(uint8_t format)
{
    bus_set_date_format(format);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Date format: %s\n", format == 0U ? "MM.DD.YY" : "DD.MM.YY");
}


WEAK_CALLBACK
void ui_on_log_rate_set(uint8_t seconds)
{
    bus_set_log_rate(seconds);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Log rate: %us\n", seconds);
}

WEAK_CALLBACK
void ui_on_bluetooth_set(bool enabled)
{
#ifdef PC_SIMULATOR
    s_bluetooth_enabled = enabled ? 1U : 0U;
#endif
    UI_CALLBACK_TRACE("[DEVICE_CONTROL] Bluetooth: %s\n", enabled ? "ON" : "OFF");
}

WEAK_CALLBACK
void ui_on_reset_defaults(void)
{
#ifdef PC_SIMULATOR
    s_dive_mode = 0U;
    s_air_ppo2 = 1.4f;
    s_nitrox_o2_pct = 32U;
    s_nitrox_ppo2 = 1.4f;
    s_three_gas_o2_pct[0] = 21U;
    s_three_gas_o2_pct[1] = 32U;
    s_three_gas_o2_pct[2] = 100U;
    s_three_gas_ppo2[0] = 1.4f;
    s_three_gas_ppo2[1] = 1.4f;
    s_three_gas_ppo2[2] = 1.4f;
    s_three_gas_active[0] = 1U;
    s_three_gas_active[1] = 1U;
    s_three_gas_active[2] = 1U;
    s_oc_tech_o2_pct[0] = 18U;
    s_oc_tech_o2_pct[1] = 21U;
    s_oc_tech_o2_pct[2] = 35U;
    s_oc_tech_o2_pct[3] = 50U;
    s_oc_tech_o2_pct[4] = 100U;
    s_oc_tech_he_pct[0] = 45U;
    s_oc_tech_he_pct[1] = 35U;
    s_oc_tech_he_pct[2] = 25U;
    s_oc_tech_he_pct[3] = 0U;
    s_oc_tech_he_pct[4] = 0U;
    for (uint8_t i = 0U; i < 5U; i++) s_oc_tech_ppo2[i] = 1.4f;
    for (uint8_t i = 0U; i < 5U; i++) s_oc_tech_active[i] = 1U;
    s_units_mode = UI_UNITS_DEFAULT;
    s_temperature_unit = UI_TEMP_UNIT_DEFAULT;
    s_bluetooth_enabled = 0U;
    s_datetime_year = 2026U;
    s_datetime_month = 1U;
    s_datetime_day = 1U;
    s_datetime_hour = 0U;
    s_datetime_minute = 0U;
#endif
    bus_set_safety_stop_mode(UI_SAFETY_STOP_DEFAULT);
    bus_set_surface_confirm_min(UI_SURFACE_CONFIRM_DEFAULT_MIN);
    bus_set_dive_start_depth_m(UI_DIVE_START_DEPTH_DEFAULT_M);
    bus_set_depth_comp_enabled(UI_DEPTH_COMP_DEFAULT_ENABLED != 0U);
    bus_set_depth_comp_m(UI_DEPTH_COMP_DEFAULT_M);
    bus_set_log_rate(UI_LOG_RATE_DEFAULT_S);
    bus_set_time_24h_enabled(true);
    bus_set_units_mode(UI_UNITS_DEFAULT);
    bus_set_date_format(1U);
    bus_set_temperature_unit(UI_TEMP_UNIT_DEFAULT);
    bus_set_sys_time(0U, 0U, 0U);
    bus_set_depth_alarm_m(40U);
    bus_set_time_alarm_min(60U);
    bus_set_ndl_alarm_min(5U);
    UI_CALLBACK_TRACE("[DISPLAY_SETUP] Reset defaults\n");
}

WEAK_CALLBACK
void ui_on_tissue_reset(void)
{
#ifdef PC_SIMULATOR
    dive_lifecycle_phase_t phase = bus_get_dive_lifecycle_phase();
    if (phase != DIVE_LIFECYCLE_SURFACE_CONFIRMED)
    {
        UI_CALLBACK_TRACE("[DIVE_SETUP] Tissue reset rejected while diving\n");
        return;
    }
    deco_core_reset();
#endif
    UI_CALLBACK_TRACE("[DIVE_SETUP] Tissue reset\n");
}

WEAK_CALLBACK
void ui_on_end_dive_confirm(void)
{
    dive_lifecycle_phase_t phase = bus_get_dive_lifecycle_phase();
    if (phase != DIVE_LIFECYCLE_SURFACING_PENDING)
    {
        UI_CALLBACK_TRACE("[MENU_HUB] End dive rejected outside surfacing pending\n");
        return;
    }
#ifdef PC_SIMULATOR
    sim_data_end_dive_now();
#else
    bus_set_dive_lifecycle_phase(DIVE_LIFECYCLE_SURFACE_CONFIRMED);
#endif
    UI_CALLBACK_TRACE("[MENU_HUB] End dive confirmed\n");
}

WEAK_CALLBACK
void ui_on_turn_off(void)
{
    UI_CALLBACK_TRACE("[DEVICE_CONTROL] Turn off\n");
}

WEAK_CALLBACK
bool ui_get_persisted_settings_snapshot(ui_persisted_settings_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL)
    {
        return false;
    }

    out_snapshot->salinity_mode = bus_get_salinity_mode();
    out_snapshot->safety_stop_mode = bus_get_safety_stop_mode();
    out_snapshot->surface_confirm_min = bus_get_surface_confirm_min();
    out_snapshot->last_deco_stop_m = bus_get_last_deco_stop();
    out_snapshot->depth_comp_enabled = bus_get_depth_comp_enabled() ? 1U : 0U;
    out_snapshot->depth_comp_m = bus_get_depth_comp_m();
    out_snapshot->depth_alarm_m = bus_get_depth_alarm_m();
    out_snapshot->time_alarm_min = bus_get_time_alarm_min();
    out_snapshot->ndl_alarm_min = bus_get_ndl_alarm_min();
#ifdef PC_SIMULATOR
    out_snapshot->units_mode = bus_get_units_mode();
    out_snapshot->temperature_unit = s_temperature_unit;
    out_snapshot->log_rate_s = bus_get_log_rate();
    out_snapshot->time_24h_enabled = bus_get_time_24h_enabled() ? 1U : 0U;
    out_snapshot->date_format = bus_get_date_format();
    out_snapshot->bluetooth_enabled = s_bluetooth_enabled;
    out_snapshot->dive_mode = s_dive_mode;
    out_snapshot->air_ppo2 = s_air_ppo2;
    out_snapshot->nitrox_o2_pct = s_nitrox_o2_pct;
    out_snapshot->nitrox_ppo2 = s_nitrox_ppo2;
    out_snapshot->three_gas_o2_pct[0] = s_three_gas_o2_pct[0];
    out_snapshot->three_gas_o2_pct[1] = s_three_gas_o2_pct[1];
    out_snapshot->three_gas_o2_pct[2] = s_three_gas_o2_pct[2];
    out_snapshot->three_gas_ppo2[0] = s_three_gas_ppo2[0];
    out_snapshot->three_gas_ppo2[1] = s_three_gas_ppo2[1];
    out_snapshot->three_gas_ppo2[2] = s_three_gas_ppo2[2];
    out_snapshot->three_gas_active[0] = s_three_gas_active[0];
    out_snapshot->three_gas_active[1] = s_three_gas_active[1];
    out_snapshot->three_gas_active[2] = s_three_gas_active[2];
    out_snapshot->oc_tech_o2_pct[0] = s_oc_tech_o2_pct[0];
    out_snapshot->oc_tech_o2_pct[1] = s_oc_tech_o2_pct[1];
    out_snapshot->oc_tech_o2_pct[2] = s_oc_tech_o2_pct[2];
    out_snapshot->oc_tech_o2_pct[3] = s_oc_tech_o2_pct[3];
    out_snapshot->oc_tech_o2_pct[4] = s_oc_tech_o2_pct[4];
    out_snapshot->oc_tech_he_pct[0] = s_oc_tech_he_pct[0];
    out_snapshot->oc_tech_he_pct[1] = s_oc_tech_he_pct[1];
    out_snapshot->oc_tech_he_pct[2] = s_oc_tech_he_pct[2];
    out_snapshot->oc_tech_he_pct[3] = s_oc_tech_he_pct[3];
    out_snapshot->oc_tech_he_pct[4] = s_oc_tech_he_pct[4];
    for (uint8_t i = 0U; i < 5U; i++) out_snapshot->oc_tech_ppo2[i] = s_oc_tech_ppo2[i];
    for (uint8_t i = 0U; i < 5U; i++) out_snapshot->oc_tech_active[i] = s_oc_tech_active[i];
    out_snapshot->datetime_year = s_datetime_year;
    out_snapshot->datetime_month = s_datetime_month;
    out_snapshot->datetime_day = s_datetime_day;
    out_snapshot->datetime_hour = (uint8_t)bus_get_sys_time_h();
    out_snapshot->datetime_minute = (uint8_t)bus_get_sys_time_m();
#else
    out_snapshot->units_mode = bus_get_units_mode();
    out_snapshot->temperature_unit = bus_get_temperature_unit();
    out_snapshot->log_rate_s = bus_get_log_rate();
    out_snapshot->time_24h_enabled = bus_get_time_24h_enabled() ? 1U : 0U;
    out_snapshot->date_format = bus_get_date_format();
    out_snapshot->bluetooth_enabled = 0U;
    out_snapshot->dive_mode = 0U;
    out_snapshot->air_ppo2 = 1.4f;
    out_snapshot->nitrox_o2_pct = 32U;
    out_snapshot->nitrox_ppo2 = 1.4f;
    out_snapshot->three_gas_o2_pct[0] = 21U;
    out_snapshot->three_gas_o2_pct[1] = 32U;
    out_snapshot->three_gas_o2_pct[2] = 100U;
    out_snapshot->three_gas_ppo2[0] = 1.4f;
    out_snapshot->three_gas_ppo2[1] = 1.4f;
    out_snapshot->three_gas_ppo2[2] = 1.4f;
    out_snapshot->three_gas_active[0] = 1U;
    out_snapshot->three_gas_active[1] = 1U;
    out_snapshot->three_gas_active[2] = 1U;
    out_snapshot->oc_tech_o2_pct[0] = 18U;
    out_snapshot->oc_tech_o2_pct[1] = 21U;
    out_snapshot->oc_tech_o2_pct[2] = 35U;
    out_snapshot->oc_tech_o2_pct[3] = 50U;
    out_snapshot->oc_tech_o2_pct[4] = 100U;
    out_snapshot->oc_tech_he_pct[0] = 45U;
    out_snapshot->oc_tech_he_pct[1] = 35U;
    out_snapshot->oc_tech_he_pct[2] = 25U;
    out_snapshot->oc_tech_he_pct[3] = 0U;
    out_snapshot->oc_tech_he_pct[4] = 0U;
    for (uint8_t i = 0U; i < 5U; i++) out_snapshot->oc_tech_ppo2[i] = 1.4f;
    for (uint8_t i = 0U; i < 5U; i++) out_snapshot->oc_tech_active[i] = 1U;
    out_snapshot->datetime_year = 2026U;
    out_snapshot->datetime_month = 1U;
    out_snapshot->datetime_day = 1U;
    out_snapshot->datetime_hour = (uint8_t)bus_get_sys_time_h();
    out_snapshot->datetime_minute = (uint8_t)bus_get_sys_time_m();
#endif
    out_snapshot->dive_start_depth_m = bus_get_dive_start_depth_m();
    return true;
}

WEAK_CALLBACK
uint32_t ui_get_dive_plan_config_signature(void)
{
    return 0U;
}
