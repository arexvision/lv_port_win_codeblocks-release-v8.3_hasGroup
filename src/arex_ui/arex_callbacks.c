#include "arex_screen.h"
#include "arex_data.h"
#include <stdio.h>

#ifndef PC_SIMULATOR
#define AREX_WEAK_CALLBACK __attribute__((weak))
#else
#define AREX_WEAK_CALLBACK
#endif

AREX_WEAK_CALLBACK
void arex_bus_set_light_power(bool on)
{
    printf("[LIGHT] Power: %s\n", on ? "ON" : "OFF");
}

AREX_WEAK_CALLBACK
void arex_ui_on_light_color_set(const char *color, const char *level)
{
    printf("[LIGHT] Color: %s, Level: %s\n", color, level);
}

AREX_WEAK_CALLBACK
void arex_set_brightness(uint8_t level)
{
    arex_apply_software_brightness(level);
}

AREX_WEAK_CALLBACK
void arex_ui_on_conservatism_set(uint8_t level)
{
    static const uint8_t gf_table[][2] =
    {
        { 40, 85 },
        { 30, 70 },
        { 20, 65 },
        { 50, 70 },
    };

    if (level >= (sizeof(gf_table) / sizeof(gf_table[0])))
    {
        level = 1;
    }

    g_sys_config.conservatism = level;
    arex_bus_set_gf_setting(gf_table[level][0], gf_table[level][1]);
}
