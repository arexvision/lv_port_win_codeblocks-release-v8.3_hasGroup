#include "arex_submenu_model.h"

#include "arex_data.h"
#include "arex_ui_engine.h"
#include "arex_ui_state.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

static const char *s_info_titles[AREX_SUBMENU_INFO_COUNT] =
{
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

static char s_info_str[AREX_SUBMENU_INFO_COUNT][5][32];
static const char *s_info_dyn[AREX_SUBMENU_INFO_COUNT][6];

static const char *s_setup_sub[AREX_SUBMENU_SETUP_COUNT][7] =
{
    { "AIR", "NX 32", "TX 18/45", "O2 100%", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", "GF 50/70", NULL },
    { "LOW", "ECO", "MED", "HIGH", "MAX", "SUN", NULL },
    { "AUTO CAL: AUTO", "RESET AUTO CAL", NULL },
    { "LIGHT ON/OFF", "RED COLOR", "GREEN COLOR", "BLUE COLOR", "WHITE COLOR", NULL },
    { "VERSION: " AREX_SYSTEM_VERSION, "MODE SETUP", "DIVE MENU", "AI SETUP", "ALERTS SETUP", "DISPLAY" },
};

static const char *s_setup_titles[AREX_SUBMENU_SETUP_COUNT] =
{
    "GAS SWITCH", "CONSERVATISM", "BRIGHTNESS", "COMPASS CAL", "LIGHT CONTROL", "SYSTEMS SETUP"
};

static const char *s_nested_red[]    = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_green[]  = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_blue[]   = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_white[]  = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_salinity[]    = { "FRESH WATER", "SEA WATER", NULL };
static const char *s_nested_safety_stop[] = { "3m", "6m", NULL };
static const char *s_nested_altitude[] =
{
    "AUTO",
    "0-700m",
    "700-1500m",
    "1500-2400m",
    "2400-3700m",
    NULL
};
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS NX", "GAUGE", NULL };
static const char *s_nested_ai_setup[]     = { "PAIR T1", "PAIR T2", "GTR MODE: ON", NULL };
static const char *s_nested_alerts_setup[] = { "DEPTH ALARM: 40m", "TIME ALARM: 60m", "LOW NDL: 5m", "TEST VIBRATION", NULL };
static const char *s_nested_display_sys[]  = { "UNITS: METRIC", "DATE & CLOCK", "LOG RATE: 10s", "BLUETOOTH: OFF", "RESET DEFAULTS", NULL };

static char s_compass_cal_status_str[24];
static const char *s_compass_cal_items[] = { s_compass_cal_status_str, "RESET AUTO CAL", NULL };

static char s_modppo2_str[20];
static char s_salinity_str[24];
static char s_safety_stop_str[24];
static char s_altitude_str[32];
static const char *s_nested_dive_setup[5];

static uint8_t s_salinity_mode = 0;      /* 0=FRESH WATER, 1=SEA WATER */
static uint8_t s_safety_stop_mode = 0;   /* 0=3m, 1=6m */
static uint8_t s_altitude_level = 0;     /* 0=AUTO, 1..4=altitude ranges */

static uint8_t count_items(const char **items, uint8_t max_count)
{
    uint8_t count = 0;
    if (!items)
    {
        return 0;
    }
    while (count < max_count && items[count])
    {
        count++;
    }
    return count;
}

static const char *strip_title_prefix(const char *title)
{
    if (title && title[0] == '>' && title[1] == ' ')
    {
        return title + 2;
    }
    return title;
}

static void normalize_menu_key(const char *text, char *out, uint8_t out_size)
{
    if (!out || out_size == 0)
    {
        return;
    }
    out[0] = '\0';
    if (!text)
    {
        return;
    }

    lv_snprintf(out, out_size, "%s", strip_title_prefix(text));
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == ' ')
    {
        out[--len] = '\0';
    }
    if (len > 0 && out[len - 1] == '>')
    {
        out[--len] = '\0';
    }
    while (len > 0 && out[len - 1] == ' ')
    {
        out[--len] = '\0';
    }
}

static const char *compass_cal_status_text(void)
{
    arex_compass_cal_ui_state_t st = arex_get_compass_calibration_ui_state();
    if (st == AREX_COMPASS_CAL_RUNNING) return "LEARN";
    if (st == AREX_COMPASS_CAL_READY) return "OK";
    return "AUTO";
}

uint8_t arex_submenu_safety_stop_depth_m(uint8_t value)
{
    return value == 1 ? 6 : 3;
}

static const char *salinity_label(uint8_t value)
{
    return value == 1 ? "SEA WATER" : "FRESH WATER";
}

static const char *safety_stop_label(uint8_t value)
{
    return value == 1 ? "6m" : "3m";
}

static const char *altitude_label(uint8_t value)
{
    if (value >= 5)
    {
        value = 0;
    }
    return s_nested_altitude[value];
}

const char *arex_submenu_info_title(uint8_t index)
{
    if (index >= AREX_SUBMENU_INFO_COUNT)
    {
        return NULL;
    }
    return s_info_titles[index];
}

const char **arex_submenu_build_info_items(uint8_t index, uint8_t *out_count)
{
    if (out_count)
    {
        *out_count = 0;
    }
    if (index >= AREX_SUBMENU_INFO_COUNT)
    {
        return NULL;
    }

    uint8_t n = 0;
    switch (index)
    {
    case 0:
        snprintf(s_info_str[0][0], sizeof(s_info_str[0][0]), "MAX DEPTH: %dm", (int)g_sensor_data.depth);
        snprintf(s_info_str[0][1], sizeof(s_info_str[0][1]), "DIVE TIME: %dm", (int)(g_sensor_data.dive_time_s / 60));
        s_info_dyn[0][n++] = s_info_str[0][0];
        s_info_dyn[0][n++] = s_info_str[0][1];
        s_info_dyn[0][n++] = "SURFACE INT: 2h 10m";
        break;
    case 1:
        s_info_dyn[1][n++] = "VIEW PROFILE";
        s_info_dyn[1][n++] = "RECALCULATE";
        break;
    case 2:
        snprintf(s_info_str[2][0], sizeof(s_info_str[2][0]), "GF: %d/%d", 30, 70);
        snprintf(s_info_str[2][1], sizeof(s_info_str[2][1]), "CNS: %d%%", g_sensor_data.cns_pct);
        snprintf(s_info_str[2][2], sizeof(s_info_str[2][2]), "OTU: %d", g_sensor_data.otu);
        s_info_dyn[2][n++] = "VIEW BAR GRAPH";
        s_info_dyn[2][n++] = s_info_str[2][0];
        s_info_dyn[2][n++] = s_info_str[2][1];
        s_info_dyn[2][n++] = s_info_str[2][2];
        break;
    case 3:
        snprintf(s_info_str[3][0], sizeof(s_info_str[3][0]), "GAS 1: %s", g_sensor_data.gas_name);
        s_info_dyn[3][n++] = s_info_str[3][0];
        s_info_dyn[3][n++] = "ALGO: ZHL-16C";
        break;
    case 4:
    {
        if (g_sensor_data.pod1_bar <= 0.0f)
            snprintf(s_info_str[4][0], sizeof(s_info_str[4][0]), "POD 1: -- BAR");
        else
            snprintf(s_info_str[4][0], sizeof(s_info_str[4][0]), "POD 1: %.0f BAR", g_sensor_data.pod1_bar);

        if (g_sensor_data.pod2_bar <= 0.0f)
            snprintf(s_info_str[4][1], sizeof(s_info_str[4][1]), "POD 2: -- BAR");
        else
            snprintf(s_info_str[4][1], sizeof(s_info_str[4][1]), "POD 2: %.0f BAR", g_sensor_data.pod2_bar);

        float battery_pct = g_sensor_data.battery_pct;
        if (battery_pct < 0.0f)
        {
            battery_pct = 0.0f;
        }
        else if (battery_pct > 100.0f)
        {
            battery_pct = 100.0f;
        }
        snprintf(s_info_str[4][2], sizeof(s_info_str[4][2]), "BATTERY: %.0f%%", battery_pct);
        snprintf(s_info_str[4][3], sizeof(s_info_str[4][3]), "TEMP: 24C");
        s_info_dyn[4][n++] = s_info_str[4][0];
        s_info_dyn[4][n++] = s_info_str[4][1];
        s_info_dyn[4][n++] = s_info_str[4][2];
        s_info_dyn[4][n++] = s_info_str[4][3];
        break;
    }
    default:
        break;
    }

    s_info_dyn[index][n] = NULL;
    if (out_count)
    {
        *out_count = n;
    }
    return s_info_dyn[index];
}

const char *arex_submenu_setup_title(uint8_t index)
{
    if (index >= AREX_SUBMENU_SETUP_COUNT)
    {
        return NULL;
    }
    return s_setup_titles[index];
}

int8_t arex_submenu_setup_index_for_title(const char *title)
{
    const char *clean_title = strip_title_prefix(title);
    if (!clean_title)
    {
        return -1;
    }

    for (uint8_t i = 0; i < AREX_SUBMENU_SETUP_COUNT; i++)
    {
        if (strcmp(clean_title, s_setup_titles[i]) == 0)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

const char **arex_submenu_build_compass_cal_items(uint8_t *out_count)
{
    lv_snprintf(s_compass_cal_status_str,
                sizeof(s_compass_cal_status_str),
                "AUTO CAL: %s",
                compass_cal_status_text());
    if (out_count)
    {
        *out_count = count_items(s_compass_cal_items, 2);
    }
    return s_compass_cal_items;
}

const char **arex_submenu_build_setup_items(uint8_t index, uint8_t *out_count)
{
    if (out_count)
    {
        *out_count = 0;
    }
    if (index >= AREX_SUBMENU_SETUP_COUNT)
    {
        return NULL;
    }
    if (strcmp(s_setup_titles[index], "COMPASS CAL") == 0)
    {
        return arex_submenu_build_compass_cal_items(out_count);
    }

    const char **items = s_setup_sub[index];
    if (out_count)
    {
        *out_count = count_items(items, 7);
    }
    return items;
}

static const char **build_nested_dive_setup(uint8_t *out_count)
{
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", 1.4f);
    snprintf(s_salinity_str, sizeof(s_salinity_str), "SALINITY: %s", salinity_label(s_salinity_mode));
    snprintf(s_safety_stop_str, sizeof(s_safety_stop_str), "SAFETY STOP: %s", safety_stop_label(s_safety_stop_mode));
    snprintf(s_altitude_str, sizeof(s_altitude_str), "ALTITUDE: %s", altitude_label(s_altitude_level));
    s_nested_dive_setup[0] = s_salinity_str;
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = s_safety_stop_str;
    s_nested_dive_setup[3] = s_altitude_str;
    s_nested_dive_setup[4] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_dive_setup, 5);
    }
    return s_nested_dive_setup;
}

const char **arex_submenu_nested_items_for(const char *title, uint8_t *out_count)
{
    char clean_title_buf[40];
    normalize_menu_key(title, clean_title_buf, sizeof(clean_title_buf));
    const char *clean_title = clean_title_buf;
    const char **items = NULL;
    uint8_t max_count = 8;

    if (out_count)
    {
        *out_count = 0;
    }
    if (clean_title[0] == '\0')
    {
        return NULL;
    }

    if      (strcmp(clean_title, "MODE SETUP") == 0) items = s_nested_mode_setup;
    else if (strcmp(clean_title, "DIVE MENU") == 0) return build_nested_dive_setup(out_count);
    else if (strcmp(clean_title, "SALINITY") == 0) items = s_nested_salinity;
    else if (strcmp(clean_title, "SAFETY STOP") == 0) items = s_nested_safety_stop;
    else if (strcmp(clean_title, "ALTITUDE") == 0) items = s_nested_altitude;
    else if (strcmp(clean_title, "AI SETUP") == 0) items = s_nested_ai_setup;
    else if (strcmp(clean_title, "ALERTS SETUP") == 0) items = s_nested_alerts_setup;
    else if (strcmp(clean_title, "DISPLAY") == 0) items = s_nested_display_sys;
    else if (strcmp(clean_title, "RED") == 0) items = s_nested_red;
    else if (strcmp(clean_title, "GREEN") == 0) items = s_nested_green;
    else if (strcmp(clean_title, "BLUE") == 0) items = s_nested_blue;
    else if (strcmp(clean_title, "WHITE") == 0) items = s_nested_white;

    if (items && out_count)
    {
        *out_count = count_items(items, max_count);
    }
    return items;
}

const char **arex_submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count)
{
    char key[40];
    uint8_t count = 0;
    const char **items = NULL;

    if (out_count)
    {
        *out_count = 0;
    }
    if (out_title && out_title_size > 0)
    {
        out_title[0] = '\0';
    }
    if (!item_text)
    {
        return NULL;
    }

    const char *clean_current_title = strip_title_prefix(current_title);
    if (clean_current_title && strcmp(clean_current_title, "SYSTEMS SETUP") == 0)
    {
        static const char *system_child_titles[] =
        {
            NULL,
            "MODE SETUP",
            "DIVE MENU",
            "AI SETUP",
            "ALERTS SETUP",
            "DISPLAY",
        };
        if (item_index < (sizeof(system_child_titles) / sizeof(system_child_titles[0])) &&
            system_child_titles[item_index])
        {
            lv_snprintf(key, sizeof(key), "%s", system_child_titles[item_index]);
        }
        else
        {
            key[0] = '\0';
        }
    }
    else if (clean_current_title && strcmp(clean_current_title, "LIGHT CONTROL") == 0)
    {
        static const char *light_child_titles[] =
        {
            NULL,
            "RED",
            "GREEN",
            "BLUE",
            "WHITE",
        };
        if (item_index < (sizeof(light_child_titles) / sizeof(light_child_titles[0])) &&
            light_child_titles[item_index])
        {
            lv_snprintf(key, sizeof(key), "%s", light_child_titles[item_index]);
        }
        else
        {
            key[0] = '\0';
        }
    }
    else
    {
        normalize_menu_key(item_text, key, sizeof(key));
        if (strcmp(clean_current_title ? clean_current_title : "", "DIVE MENU") == 0)
        {
            static const char *dive_child_titles[] =
            {
                "SALINITY",
                NULL,
                "SAFETY STOP",
                "ALTITUDE",
            };
            if (item_index < (sizeof(dive_child_titles) / sizeof(dive_child_titles[0])) &&
                dive_child_titles[item_index])
            {
                lv_snprintf(key, sizeof(key), "%s", dive_child_titles[item_index]);
            }
        }
    }

    items = arex_submenu_nested_items_for(key, &count);
    if (!items || count == 0)
    {
        return NULL;
    }

    if (out_title && out_title_size > 0)
    {
        lv_snprintf(out_title, out_title_size, "%s", key);
    }
    if (out_count)
    {
        *out_count = count;
    }
    return items;
}

bool arex_submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         arex_submenu_setting_confirm_t *out_setting)
{
    const char *clean_title = strip_title_prefix(current_title);
    if (!clean_title || !item_text || !out_setting)
    {
        return false;
    }

    memset(out_setting, 0, sizeof(*out_setting));

    if (strcmp(clean_title, "SALINITY") == 0 && item_index < 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_SALINITY;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "SALINITY\n%s", salinity_label(item_index));
        return true;
    }

    if (strcmp(clean_title, "SAFETY STOP") == 0 && item_index < 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_SAFETY_STOP;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "SAFETY STOP\n%s", safety_stop_label(item_index));
        return true;
    }

    if (strcmp(clean_title, "ALTITUDE") == 0 && item_index < 5)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_ALTITUDE;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "ALTITUDE\n%s", altitude_label(item_index));
        return true;
    }

    return false;
}

void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_SALINITY:
        s_salinity_mode = (value > 1) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_SAFETY_STOP:
        s_safety_stop_mode = (value > 1) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_ALTITUDE:
        s_altitude_level = (value > 4) ? 0 : value;
        break;
    default:
        break;
    }
}

bool arex_submenu_is_readonly_info_title(const char *title)
{
    const char *clean_title = strip_title_prefix(title);
    if (!clean_title)
    {
        return false;
    }

    return strcmp(clean_title, "LAST DIVE") == 0 ||
           strcmp(clean_title, "TISSUE & TOX") == 0 ||
           strcmp(clean_title, "GAS & CALC") == 0 ||
           strcmp(clean_title, "SENSOR & DEVICE") == 0;
}
