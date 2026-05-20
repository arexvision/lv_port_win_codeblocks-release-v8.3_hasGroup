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

static char s_system_mode_str[28];
static const char *s_system_setup_dyn[7];

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
static const char *s_nested_gtr_mode[]     = { "ON", "OFF", NULL };
static const char *s_nested_depth_alarm[]  = { "10m", "20m", "30m", "40m", "50m", "60m", "70m", "80m", "90m", "100m", NULL };
static const char *s_nested_time_alarm[]   = { "10min", "20min", "30min", "40min", "50min", "60min", "90min", "120min", NULL };
static const char *s_nested_ndl_alarm[]    = { "1min", "3min", "5min", "10min", "15min", NULL };
static const char *s_nested_units[]        = { "METRIC (m)", "IMPERIAL (ft)", NULL };
static const char *s_nested_log_rate[]     = { "2s", "4s", "6s", "8s", "10s", NULL };
static const char *s_nested_bluetooth[]    = { "ON", "OFF", NULL };
static const uint8_t s_depth_alarm_values[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
static const uint8_t s_time_alarm_values[]  = { 10, 20, 30, 40, 50, 60, 90, 120 };
static const uint8_t s_ndl_alarm_values[]   = { 1, 3, 5, 10, 15 };
static const uint8_t s_log_rate_values[]    = { 2, 4, 6, 8, 10 };

static char s_compass_cal_status_str[24];
static const char *s_compass_cal_items[] = { s_compass_cal_status_str, "RESET AUTO CAL", NULL };

static char s_modppo2_str[20];
static char s_salinity_str[24];
static char s_safety_stop_str[24];
static char s_altitude_str[32];
static const char *s_nested_dive_setup[5];

static char s_ai_gtr_str[24];
static const char *s_nested_ai_setup[4];

static char s_alert_depth_str[28];
static char s_alert_time_str[28];
static char s_alert_ndl_str[24];
static const char *s_nested_alerts_setup[5];

static char s_display_units_str[28];
static char s_display_log_rate_str[24];
static char s_display_bluetooth_str[24];
static const char *s_nested_display_sys[6];

static char s_datetime_year_str[20];
static char s_datetime_month_str[20];
static char s_datetime_day_str[20];
static char s_datetime_hour_str[20];
static char s_datetime_minute_str[20];
static char s_datetime_second_str[20];
static const char *s_nested_datetime[8];

static char s_year_item_str[12][8];
static const char *s_year_items[13];
static char s_month_item_str[12][4];
static const char *s_month_items[13];
static char s_day_item_str[31][4];
static const char *s_day_items[32];
static char s_hour_item_str[24][4];
static const char *s_hour_items[25];
static char s_minute_item_str[60][4];
static const char *s_minute_items[61];
static char s_second_item_str[60][4];
static const char *s_second_items[61];

static uint8_t s_salinity_mode = 0;      /* 0=FRESH WATER, 1=SEA WATER */
static uint8_t s_safety_stop_mode = 0;   /* 0=3m, 1=6m */
static uint8_t s_altitude_level = 0;     /* 0=AUTO, 1..4=altitude ranges */
static uint8_t s_dive_mode = 0;          /* 0=AIR, 1=NITROX, 2=3 GAS NX, 3=GAUGE */
static uint8_t s_gtr_enabled = 1;        /* 0=OFF, 1=ON */
static uint8_t s_depth_alarm_m = 40;
static uint8_t s_time_alarm_min = 60;
static uint8_t s_ndl_alarm_min = 5;
static uint8_t s_units_mode = 0;         /* 0=METRIC, 1=IMPERIAL */
static uint8_t s_log_rate_s = 10;
static uint8_t s_bluetooth_enabled = 0;  /* 0=OFF, 1=ON */
static uint16_t s_datetime_year = 2026;
static uint8_t s_datetime_month = 5;
static uint8_t s_datetime_day = 20;
static uint8_t s_datetime_hour = 12;
static uint8_t s_datetime_minute = 0;
static uint8_t s_datetime_second = 0;

enum
{
    AREX_DATETIME_FIELD_YEAR = 0,
    AREX_DATETIME_FIELD_MONTH,
    AREX_DATETIME_FIELD_DAY,
    AREX_DATETIME_FIELD_HOUR,
    AREX_DATETIME_FIELD_MINUTE,
    AREX_DATETIME_FIELD_SECOND,
};

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

static const char *dive_mode_label(uint8_t value)
{
    if (value >= 4)
    {
        value = 0;
    }
    return s_nested_mode_setup[value];
}

static const char *gtr_label(uint8_t enabled)
{
    return enabled ? "ON" : "OFF";
}

static const char *units_label(uint8_t value)
{
    return value == 1 ? "IMPERIAL" : "METRIC";
}

static const char *bluetooth_label(uint8_t enabled)
{
    return enabled ? "ON" : "OFF";
}

static uint8_t value_from_table(const uint8_t *values, uint8_t count, uint8_t index, uint8_t fallback)
{
    if (!values || index >= count)
    {
        return fallback;
    }
    return values[index];
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

static const char **build_systems_setup_items(uint8_t *out_count)
{
    snprintf(s_system_mode_str, sizeof(s_system_mode_str), "MODE SETUP: %s", dive_mode_label(s_dive_mode));
    s_system_setup_dyn[0] = "VERSION: " AREX_SYSTEM_VERSION;
    s_system_setup_dyn[1] = s_system_mode_str;
    s_system_setup_dyn[2] = "DIVE MENU";
    s_system_setup_dyn[3] = "AI SETUP";
    s_system_setup_dyn[4] = "ALERTS SETUP";
    s_system_setup_dyn[5] = "DISPLAY";
    s_system_setup_dyn[6] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_system_setup_dyn, 7);
    }
    return s_system_setup_dyn;
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
    if (strcmp(s_setup_titles[index], "SYSTEMS SETUP") == 0)
    {
        return build_systems_setup_items(out_count);
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

static const char **build_nested_ai_setup(uint8_t *out_count)
{
    snprintf(s_ai_gtr_str, sizeof(s_ai_gtr_str), "GTR MODE: %s", gtr_label(s_gtr_enabled));
    s_nested_ai_setup[0] = "PAIR T1";
    s_nested_ai_setup[1] = "PAIR T2";
    s_nested_ai_setup[2] = s_ai_gtr_str;
    s_nested_ai_setup[3] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_ai_setup, 4);
    }
    return s_nested_ai_setup;
}

static const char **build_nested_alerts_setup(uint8_t *out_count)
{
    snprintf(s_alert_depth_str, sizeof(s_alert_depth_str), "DEPTH ALARM: %um", s_depth_alarm_m);
    snprintf(s_alert_time_str, sizeof(s_alert_time_str), "TIME ALARM: %umin", s_time_alarm_min);
    snprintf(s_alert_ndl_str, sizeof(s_alert_ndl_str), "LOW NDL: %umin", s_ndl_alarm_min);
    s_nested_alerts_setup[0] = s_alert_depth_str;
    s_nested_alerts_setup[1] = s_alert_time_str;
    s_nested_alerts_setup[2] = s_alert_ndl_str;
    s_nested_alerts_setup[3] = "TEST VIBRATION";
    s_nested_alerts_setup[4] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_alerts_setup, 5);
    }
    return s_nested_alerts_setup;
}

static const char **build_nested_display_sys(uint8_t *out_count)
{
    snprintf(s_display_units_str, sizeof(s_display_units_str), "UNITS: %s", units_label(s_units_mode));
    snprintf(s_display_log_rate_str, sizeof(s_display_log_rate_str), "LOG RATE: %us", s_log_rate_s);
    snprintf(s_display_bluetooth_str, sizeof(s_display_bluetooth_str), "BLUETOOTH: %s", bluetooth_label(s_bluetooth_enabled));
    s_nested_display_sys[0] = s_display_units_str;
    s_nested_display_sys[1] = "DATE & CLOCK";
    s_nested_display_sys[2] = s_display_log_rate_str;
    s_nested_display_sys[3] = s_display_bluetooth_str;
    s_nested_display_sys[4] = "RESET DEFAULTS";
    s_nested_display_sys[5] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_display_sys, 6);
    }
    return s_nested_display_sys;
}

static const char **build_nested_datetime(uint8_t *out_count)
{
    snprintf(s_datetime_year_str, sizeof(s_datetime_year_str), "YEAR: %04u", s_datetime_year);
    snprintf(s_datetime_month_str, sizeof(s_datetime_month_str), "MONTH: %02u", s_datetime_month);
    snprintf(s_datetime_day_str, sizeof(s_datetime_day_str), "DAY: %02u", s_datetime_day);
    snprintf(s_datetime_hour_str, sizeof(s_datetime_hour_str), "HOUR: %02u", s_datetime_hour);
    snprintf(s_datetime_minute_str, sizeof(s_datetime_minute_str), "MINUTE: %02u", s_datetime_minute);
    snprintf(s_datetime_second_str, sizeof(s_datetime_second_str), "SECOND: %02u", s_datetime_second);
    s_nested_datetime[0] = s_datetime_year_str;
    s_nested_datetime[1] = s_datetime_month_str;
    s_nested_datetime[2] = s_datetime_day_str;
    s_nested_datetime[3] = s_datetime_hour_str;
    s_nested_datetime[4] = s_datetime_minute_str;
    s_nested_datetime[5] = s_datetime_second_str;
    s_nested_datetime[6] = "SYNC CURRENT TIME";
    s_nested_datetime[7] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_datetime, 8);
    }
    return s_nested_datetime;
}

static const char **build_year_items(uint8_t *out_count)
{
    for (uint8_t i = 0; i < 12; i++)
    {
        snprintf(s_year_item_str[i], sizeof(s_year_item_str[i]), "%04u", (unsigned)(2024 + i));
        s_year_items[i] = s_year_item_str[i];
    }
    s_year_items[12] = NULL;
    if (out_count) *out_count = 12;
    return s_year_items;
}

static const char **build_two_digit_items(char items_str[][4],
                                          const char **items,
                                          uint8_t count,
                                          uint8_t start,
                                          uint8_t *out_count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        snprintf(items_str[i], 4, "%02u", (unsigned)(start + i));
        items[i] = items_str[i];
    }
    items[count] = NULL;
    if (out_count)
    {
        *out_count = count;
    }
    return items;
}

const char **arex_submenu_nested_items_for(const char *title, uint8_t *out_count)
{
    char clean_title_buf[40];
    normalize_menu_key(title, clean_title_buf, sizeof(clean_title_buf));
    const char *clean_title = clean_title_buf;
    const char **items = NULL;
    uint8_t max_count = 64;

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
    else if (strcmp(clean_title, "AI SETUP") == 0) return build_nested_ai_setup(out_count);
    else if (strcmp(clean_title, "GTR MODE") == 0) items = s_nested_gtr_mode;
    else if (strcmp(clean_title, "ALERTS SETUP") == 0) return build_nested_alerts_setup(out_count);
    else if (strcmp(clean_title, "DEPTH ALARM") == 0) items = s_nested_depth_alarm;
    else if (strcmp(clean_title, "TIME ALARM") == 0) items = s_nested_time_alarm;
    else if (strcmp(clean_title, "LOW NDL") == 0) items = s_nested_ndl_alarm;
    else if (strcmp(clean_title, "DISPLAY") == 0) return build_nested_display_sys(out_count);
    else if (strcmp(clean_title, "UNITS") == 0) items = s_nested_units;
    else if (strcmp(clean_title, "DATE & CLOCK") == 0) return build_nested_datetime(out_count);
    else if (strcmp(clean_title, "YEAR") == 0) return build_year_items(out_count);
    else if (strcmp(clean_title, "MONTH") == 0) return build_two_digit_items(s_month_item_str, s_month_items, 12, 1, out_count);
    else if (strcmp(clean_title, "DAY") == 0) return build_two_digit_items(s_day_item_str, s_day_items, 31, 1, out_count);
    else if (strcmp(clean_title, "HOUR") == 0) return build_two_digit_items(s_hour_item_str, s_hour_items, 24, 0, out_count);
    else if (strcmp(clean_title, "MINUTE") == 0) return build_two_digit_items(s_minute_item_str, s_minute_items, 60, 0, out_count);
    else if (strcmp(clean_title, "SECOND") == 0) return build_two_digit_items(s_second_item_str, s_second_items, 60, 0, out_count);
    else if (strcmp(clean_title, "LOG RATE") == 0) items = s_nested_log_rate;
    else if (strcmp(clean_title, "BLUETOOTH") == 0) items = s_nested_bluetooth;
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
        else if (strcmp(clean_current_title ? clean_current_title : "", "AI SETUP") == 0)
        {
            if (item_index == 2)
            {
                lv_snprintf(key, sizeof(key), "%s", "GTR MODE");
            }
            else
            {
                key[0] = '\0';
            }
        }
        else if (strcmp(clean_current_title ? clean_current_title : "", "ALERTS SETUP") == 0)
        {
            static const char *alerts_child_titles[] =
            {
                "DEPTH ALARM",
                "TIME ALARM",
                "LOW NDL",
                NULL,
            };
            if (item_index < (sizeof(alerts_child_titles) / sizeof(alerts_child_titles[0])) &&
                alerts_child_titles[item_index])
            {
                lv_snprintf(key, sizeof(key), "%s", alerts_child_titles[item_index]);
            }
            else
            {
                key[0] = '\0';
            }
        }
        else if (strcmp(clean_current_title ? clean_current_title : "", "DISPLAY") == 0)
        {
            static const char *display_child_titles[] =
            {
                "UNITS",
                "DATE & CLOCK",
                "LOG RATE",
                "BLUETOOTH",
                NULL,
            };
            if (item_index < (sizeof(display_child_titles) / sizeof(display_child_titles[0])) &&
                display_child_titles[item_index])
            {
                lv_snprintf(key, sizeof(key), "%s", display_child_titles[item_index]);
            }
            else
            {
                key[0] = '\0';
            }
        }
        else if (strcmp(clean_current_title ? clean_current_title : "", "DATE & CLOCK") == 0)
        {
            static const char *datetime_child_titles[] =
            {
                "YEAR",
                "MONTH",
                "DAY",
                "HOUR",
                "MINUTE",
                "SECOND",
                NULL,
            };
            if (item_index < (sizeof(datetime_child_titles) / sizeof(datetime_child_titles[0])) &&
                datetime_child_titles[item_index])
            {
                lv_snprintf(key, sizeof(key), "%s", datetime_child_titles[item_index]);
            }
            else
            {
                key[0] = '\0';
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

    if (strcmp(clean_title, "MODE SETUP") == 0 && item_index < 4)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\n%s", dive_mode_label(item_index));
        return true;
    }

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

    if (strcmp(clean_title, "AI SETUP") == 0 && item_index < 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_AI_PAIR;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "PAIR WIRELESS TANK\nT%u", (unsigned)(item_index + 1));
        return true;
    }

    if (strcmp(clean_title, "GTR MODE") == 0 && item_index < 2)
    {
        uint8_t enabled = (item_index == 0) ? 1 : 0;
        out_setting->kind = AREX_SUBMENU_SETTING_GTR_MODE;
        out_setting->value = enabled;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "GTR MODE\n%s", gtr_label(enabled));
        return true;
    }

    if (strcmp(clean_title, "ALERTS SETUP") == 0 && item_index == 3)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_VIBRATION_TEST;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "TEST VIBRATION\nRUN NOW");
        return true;
    }

    if (strcmp(clean_title, "DEPTH ALARM") == 0 &&
        item_index < (sizeof(s_depth_alarm_values) / sizeof(s_depth_alarm_values[0])))
    {
        uint8_t depth_m = value_from_table(s_depth_alarm_values,
                                           sizeof(s_depth_alarm_values) / sizeof(s_depth_alarm_values[0]),
                                           item_index,
                                           s_depth_alarm_m);
        out_setting->kind = AREX_SUBMENU_SETTING_DEPTH_ALARM;
        out_setting->value = depth_m;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DEPTH ALARM\n%um", depth_m);
        return true;
    }

    if (strcmp(clean_title, "TIME ALARM") == 0 &&
        item_index < (sizeof(s_time_alarm_values) / sizeof(s_time_alarm_values[0])))
    {
        uint8_t minutes = value_from_table(s_time_alarm_values,
                                           sizeof(s_time_alarm_values) / sizeof(s_time_alarm_values[0]),
                                           item_index,
                                           s_time_alarm_min);
        out_setting->kind = AREX_SUBMENU_SETTING_TIME_ALARM;
        out_setting->value = minutes;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "TIME ALARM\n%umin", minutes);
        return true;
    }

    if (strcmp(clean_title, "LOW NDL") == 0 &&
        item_index < (sizeof(s_ndl_alarm_values) / sizeof(s_ndl_alarm_values[0])))
    {
        uint8_t minutes = value_from_table(s_ndl_alarm_values,
                                           sizeof(s_ndl_alarm_values) / sizeof(s_ndl_alarm_values[0]),
                                           item_index,
                                           s_ndl_alarm_min);
        out_setting->kind = AREX_SUBMENU_SETTING_NDL_ALARM;
        out_setting->value = minutes;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "LOW NDL ALARM\n%umin", minutes);
        return true;
    }

    if (strcmp(clean_title, "UNITS") == 0 && item_index < 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_UNITS;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "UNITS\n%s", units_label(item_index));
        return true;
    }

    if (strcmp(clean_title, "YEAR") == 0 && item_index < 12)
    {
        uint16_t year = (uint16_t)(2024 + item_index);
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_YEAR;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "YEAR\n%04u", year);
        return true;
    }

    if (strcmp(clean_title, "MONTH") == 0 && item_index < 12)
    {
        uint8_t month = (uint8_t)(item_index + 1);
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_MONTH;
        out_setting->value = month;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "MONTH\n%02u", month);
        return true;
    }

    if (strcmp(clean_title, "DAY") == 0 && item_index < 31)
    {
        uint8_t day = (uint8_t)(item_index + 1);
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_DAY;
        out_setting->value = day;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DAY\n%02u", day);
        return true;
    }

    if (strcmp(clean_title, "HOUR") == 0 && item_index < 24)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_HOUR;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "HOUR\n%02u", item_index);
        return true;
    }

    if (strcmp(clean_title, "MINUTE") == 0 && item_index < 60)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_MINUTE;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "MINUTE\n%02u", item_index);
        return true;
    }

    if (strcmp(clean_title, "SECOND") == 0 && item_index < 60)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_setting->arg = AREX_DATETIME_FIELD_SECOND;
        out_setting->value = item_index;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "SECOND\n%02u", item_index);
        return true;
    }

    if (strcmp(clean_title, "DATE & CLOCK") == 0 && item_index == 6)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DATETIME_ACTION;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DATE & CLOCK\n%s", item_text);
        return true;
    }

    if (strcmp(clean_title, "LOG RATE") == 0 &&
        item_index < (sizeof(s_log_rate_values) / sizeof(s_log_rate_values[0])))
    {
        uint8_t seconds = value_from_table(s_log_rate_values,
                                           sizeof(s_log_rate_values) / sizeof(s_log_rate_values[0]),
                                           item_index,
                                           s_log_rate_s);
        out_setting->kind = AREX_SUBMENU_SETTING_LOG_RATE;
        out_setting->value = seconds;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "LOG RATE\n%us", seconds);
        return true;
    }

    if (strcmp(clean_title, "BLUETOOTH") == 0 && item_index < 2)
    {
        uint8_t enabled = (item_index == 0) ? 1 : 0;
        out_setting->kind = AREX_SUBMENU_SETTING_BLUETOOTH;
        out_setting->value = enabled;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "BLUETOOTH\n%s", bluetooth_label(enabled));
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 4)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_RESET_DEFAULTS;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "RESET DEFAULTS\nDISPLAY SETUP");
        return true;
    }

    return false;
}

void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t arg, uint8_t value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_DIVE_MODE:
        s_dive_mode = (value > 3) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_SALINITY:
        s_salinity_mode = (value > 1) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_SAFETY_STOP:
        s_safety_stop_mode = (value > 1) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_ALTITUDE:
        s_altitude_level = (value > 4) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_GTR_MODE:
        s_gtr_enabled = value ? 1 : 0;
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        s_depth_alarm_m = value;
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        s_time_alarm_min = value;
        break;
    case AREX_SUBMENU_SETTING_NDL_ALARM:
        s_ndl_alarm_min = value;
        break;
    case AREX_SUBMENU_SETTING_UNITS:
        s_units_mode = (value > 1) ? 0 : value;
        break;
    case AREX_SUBMENU_SETTING_DATETIME_FIELD:
        switch (arg)
        {
        case AREX_DATETIME_FIELD_YEAR:
            s_datetime_year = (uint16_t)(2024 + value);
            break;
        case AREX_DATETIME_FIELD_MONTH:
            s_datetime_month = (value < 1 || value > 12) ? 1 : value;
            break;
        case AREX_DATETIME_FIELD_DAY:
            s_datetime_day = (value < 1 || value > 31) ? 1 : value;
            break;
        case AREX_DATETIME_FIELD_HOUR:
            s_datetime_hour = (value > 23) ? 0 : value;
            break;
        case AREX_DATETIME_FIELD_MINUTE:
            s_datetime_minute = (value > 59) ? 0 : value;
            break;
        case AREX_DATETIME_FIELD_SECOND:
            s_datetime_second = (value > 59) ? 0 : value;
            break;
        default:
            break;
        }
        break;
    case AREX_SUBMENU_SETTING_LOG_RATE:
        s_log_rate_s = value;
        break;
    case AREX_SUBMENU_SETTING_BLUETOOTH:
        s_bluetooth_enabled = value ? 1 : 0;
        break;
    case AREX_SUBMENU_SETTING_RESET_DEFAULTS:
        s_units_mode = 0;
        s_log_rate_s = 10;
        s_bluetooth_enabled = 0;
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
