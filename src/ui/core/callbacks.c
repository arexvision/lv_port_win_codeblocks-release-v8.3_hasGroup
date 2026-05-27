#include "callbacks.h"
#include "data.h"
#include <stdio.h>

#ifndef PC_SIMULATOR
#define WEAK_CALLBACK __attribute__((weak))
#else
#define WEAK_CALLBACK
#endif

WEAK_CALLBACK
void bus_set_light_power(bool on)
{
    printf("[LIGHT] Power: %s\n", on ? "ON" : "OFF");
}

WEAK_CALLBACK
bool bus_get_light_power(void)
{
    return g_light_power_state;
}

WEAK_CALLBACK
void bus_toggle_light_power(void)
{
    g_light_power_state = !g_light_power_state;
    bus_set_light_power(g_light_power_state);
}

WEAK_CALLBACK
void ui_on_light_color_set(const char *color, const char *level)
{
    printf("[LIGHT] Color: %s, Level: %s\n", color, level);
}

WEAK_CALLBACK
void set_brightness(uint8_t level)
{
    apply_software_brightness(level);
}

WEAK_CALLBACK
void ui_on_conservatism_set(uint8_t level)
{
    bus_set_conservatism(level);
}

WEAK_CALLBACK
void ui_on_salinity_set(uint8_t mode)
{
    bus_set_salinity_mode(mode);
}

WEAK_CALLBACK
void ui_on_safety_stop_depth_set(uint8_t depth_m)
{
    printf("[DIVE_SETUP] Safety stop depth: %um\n", depth_m);
}

WEAK_CALLBACK
void ui_on_safety_stop_time_set(uint8_t minutes)
{
    if (minutes == 0)
    {
        printf("[DIVE_SETUP] Safety stop: OFF\n");
    }
    else
    {
        printf("[DIVE_SETUP] Safety stop: %umin\n", minutes);
    }
}

WEAK_CALLBACK
void ui_on_last_deco_stop_set(uint8_t depth_m)
{
    bus_set_last_deco_stop(depth_m);
}

WEAK_CALLBACK
void ui_on_altitude_range_set(uint8_t level)
{
    static const char *labels[] =
    {
        "AUTO",
        "0-700m",
        "700-1500m",
        "1500-2400m",
        "2400-3700m",
    };
    if (level >= (sizeof(labels) / sizeof(labels[0])))
    {
        level = 0;
    }
    printf("[DIVE_SETUP] Altitude: %s\n", labels[level]);
}

WEAK_CALLBACK
void ui_on_dive_mode_set(uint8_t mode)
{
    static const char *labels[] = { "AIR", "NITROX", "3 GAS", "OC Tech" };
    if (mode >= (sizeof(labels) / sizeof(labels[0])))
    {
        mode = 0;
    }
    printf("[SYSTEM_SETUP] Dive mode: %s\n", labels[mode]);
}

WEAK_CALLBACK
void ui_on_ai_pair(uint8_t tank_index)
{
    printf("[SYSTEM_SETUP] Pair AI tank: T%u\n", (unsigned)(tank_index + 1));
}

WEAK_CALLBACK
void ui_on_ai_tank_state_set(uint8_t tank_index, uint8_t state)
{
    static const char *labels[] = { "UNPAIRED", "PAIRING", "PAIRED" };
    if (state >= (sizeof(labels) / sizeof(labels[0])))
    {
        state = 0;
    }
    printf("[SYSTEM_SETUP] AI T%u: %s\n", (unsigned)(tank_index + 1), labels[state]);
}

WEAK_CALLBACK
void ui_on_gtr_mode_set(bool enabled)
{
    printf("[SYSTEM_SETUP] GTR mode: %s\n", enabled ? "ON" : "OFF");
}

WEAK_CALLBACK
void ui_on_mod_ppo2_set(float ppo2)
{
    bus_set_mod_ppo2(ppo2);
    printf("[DIVE_SETUP] MOD PPO2: %.1f\n", (double)ppo2);
}

WEAK_CALLBACK
void ui_on_depth_alarm_set(uint16_t depth_m)
{
    printf("[ALERT_SETUP] Depth alarm: %um\n", depth_m);
}

WEAK_CALLBACK
void ui_on_time_alarm_set(uint16_t minutes)
{
    printf("[ALERT_SETUP] Time alarm: %umin\n", minutes);
}

WEAK_CALLBACK
void ui_on_ndl_alarm_set(uint16_t minutes)
{
    printf("[ALERT_SETUP] NDL alarm: %umin\n", minutes);
}

WEAK_CALLBACK
void ui_on_vibration_test(void)
{
    printf("[ALERT_SETUP] Test vibration\n");
}

WEAK_CALLBACK
void ui_on_units_set(uint8_t units)
{
    printf("[DISPLAY_SETUP] Units: %s\n", units == 1 ? "IMPERIAL" : "METRIC");
}

WEAK_CALLBACK
void ui_on_datetime_field_set(uint8_t field, uint16_t value)
{
    static const char *labels[] = { "YEAR", "MONTH", "DAY", "HOUR", "MINUTE" };
    if (field >= (sizeof(labels) / sizeof(labels[0])))
    {
        field = 0;
    }
    printf("[DISPLAY_SETUP] Date/time %s: %u\n", labels[field], value);
}

WEAK_CALLBACK
void ui_on_datetime_action(uint8_t action)
{
    static const char *labels[] = { "SYNC CURRENT TIME" };
    if (action >= (sizeof(labels) / sizeof(labels[0])))
    {
        action = 0;
    }
    printf("[DISPLAY_SETUP] Date/time action: %s\n", labels[action]);
}

WEAK_CALLBACK
void ui_on_log_rate_set(uint8_t seconds)
{
    printf("[DISPLAY_SETUP] Log rate: %us\n", seconds);
}

WEAK_CALLBACK
void ui_on_bluetooth_set(bool enabled)
{
    printf("[DISPLAY_SETUP] Bluetooth: %s\n", enabled ? "ON" : "OFF");
}

WEAK_CALLBACK
void ui_on_reset_defaults(void)
{
    printf("[DISPLAY_SETUP] Reset defaults\n");
}
