#include "submenu_model.h"

#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#ifdef PC_SIMULATOR
#include "../../arex_algo_sim/buhlmann_debug.h"
#endif

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

static const char *s_info_titles[AREX_SUBMENU_INFO_COUNT] =
{
    "LAST DIVE", "DIVE PLAN", "TISSUE & TOX", "GAS & CALC", "SENSOR & DEVICE"
};

static char s_info_str[AREX_SUBMENU_INFO_COUNT][6][32];
static const char *s_info_dyn[AREX_SUBMENU_INFO_COUNT][7];
static char s_plan_str[16][48];
static const char *s_plan_dyn[16];
static char s_gas_switch_str[GAS_COUNT][20];
static const char *s_gas_switch_dyn[GAS_COUNT + 1];

static const char *s_setup_sub[AREX_SUBMENU_SETUP_COUNT][7] =
{
    { NULL },
    { "LOW (GF 40/95)", "MED (GF 40/85)", "HIGH (GF 30/70)", "CUSTOM (GF 50/70)", NULL },
    { "ECO", "MED", "HIGH", "MAX", "SUN", NULL },
    { "AUTO CAL: AUTO", "RESET AUTO CAL", NULL },
    { "LIGHT ON/OFF", "RED COLOR", "GREEN COLOR", "BLUE COLOR", "WHITE COLOR", NULL },
    { "VERSION: " SYSTEM_VERSION, "MODE SETUP", "DIVE SETUP", "AI SETUP", "ALERTS SETUP", "DISPLAY" },
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
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS", "OC Tech", NULL };
static const uint8_t s_safety_stop_values[] = { 0, 3, 4, 5 };
static const uint8_t s_last_deco_values[] = { 3, 6 };
static const uint8_t s_log_rate_values[]    = { 2, 5, 10, 30 };

static char s_compass_cal_status_str[24];
static const char *s_compass_cal_items[] = { s_compass_cal_status_str, "RESET AUTO CAL", NULL };

static char s_modppo2_str[20];
static char s_salinity_str[24];
static char s_safety_stop_str[24];
static char s_last_deco_str[24];
static char s_altitude_str[32];
static const char *s_nested_dive_setup[6];

static char s_ai_gtr_str[24];
static const char *s_nested_ai_setup[4];

static char s_alert_depth_str[28];
static char s_alert_time_str[28];
static char s_alert_ndl_str[28];
static const char *s_nested_alerts_setup[4];

static char s_display_units_str[28];
static char s_display_log_rate_str[24];
static char s_display_bluetooth_str[24];
static const char *s_nested_display_sys[6];

static char s_datetime_year_str[20];
static char s_datetime_month_str[20];
static char s_datetime_day_str[20];
static char s_datetime_hour_str[20];
static char s_datetime_minute_str[20];
static const char *s_nested_datetime[6];

static char s_nitrox_o2_str[24];
static const char *s_nested_nitrox[3];
static char s_three_gas_o2_str[3][24];
static char s_three_gas_count_str[24];
static const char *s_nested_three_gas[6];
static char s_oc_tech_gas_str[5][24];
static char s_oc_tech_edit_str[4][28];
static const char *s_nested_oc_tech[8];
static const char *s_nested_oc_tech_edit[5];

static uint8_t s_salinity_mode = 0;      /* 0=FRESH, 1=SALT, 2=EN13319 */
static uint8_t s_safety_stop_mode = 1;   /* 0=OFF, 1=3min, 2=4min, 3=5min */
static uint8_t s_last_deco_mode = 0;     /* 0=3m, 1=6m */
static uint8_t s_altitude_level = 0;     /* 0=AUTO, 1=SEA, 2=L1, 3=L2 */
static uint8_t s_dive_mode = 0;          /* 0=AIR, 1=NITROX, 2=3 GAS, 3=OC Tech */
static uint8_t s_nitrox_o2_pct = 32;
static uint8_t s_three_gas_o2_pct[3] = { 21, 32, 100 };
static uint8_t s_three_gas_count = 3;
static uint8_t s_oc_tech_o2_pct[5] = { 18, 21, 35, 50, 100 };
static uint8_t s_oc_tech_he_pct[5] = { 45, 35, 25, 0, 0 };
static uint8_t s_oc_tech_draft_o2_pct[5] = { 18, 21, 35, 50, 100 };
static uint8_t s_oc_tech_draft_he_pct[5] = { 45, 35, 25, 0, 0 };
static uint8_t s_oc_tech_edit_slot = 0;
static uint8_t s_ai_tank_state[2] = { 0, 0 }; /* 0=UNPAIRED, 1=PAIRING, 2=PAIRED */
static uint8_t s_gtr_enabled = 1;        /* 0=OFF, 1=ON */
static uint16_t s_depth_alarm_m = 40;
static uint16_t s_time_alarm_min = 60;
static const uint8_t s_ndl_alarm_min = 5;
static uint8_t s_units_mode = 0;         /* 0=METRIC, 1=IMPERIAL */
static uint8_t s_log_rate_s = 10;
static uint8_t s_bluetooth_enabled = 0;  /* 0=OFF, 1=ON */
static uint16_t s_datetime_year = 2026;
static uint8_t s_datetime_month = 5;
static uint8_t s_datetime_day = 20;
static uint8_t s_datetime_hour = 12;
static uint8_t s_datetime_minute = 0;

typedef enum
{
    AREX_PLAN_PAGE_DEPTH = 0,
    AREX_PLAN_PAGE_TIME,
    AREX_PLAN_PAGE_RMV,
    AREX_PLAN_PAGE_READY,
    AREX_PLAN_PAGE_RESULT,
    AREX_PLAN_PAGE_ERROR,
} arex_plan_page_t;

#define AREX_PLAN_ROWS_PER_PAGE 8U

static arex_plan_page_t s_plan_page = AREX_PLAN_PAGE_DEPTH;
static bool s_plan_defaults_loaded = false;
static float s_plan_depth_m = 30.0f;
static uint16_t s_plan_time_min = 20U;
static float s_plan_rmv_lpm = 14.0f;
static uint8_t s_plan_result_page = 0U;
#ifdef PC_SIMULATOR
static buhlmann_debug_plan_result_t s_plan_result;
#endif

enum
{
    AREX_DATETIME_FIELD_YEAR = 0,
    AREX_DATETIME_FIELD_MONTH,
    AREX_DATETIME_FIELD_DAY,
    AREX_DATETIME_FIELD_HOUR,
    AREX_DATETIME_FIELD_MINUTE,
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

static float gas_mod_for_o2(uint8_t o2_pct)
{
    if (o2_pct == 0U)
    {
        return 0.0f;
    }
    return ((g_sys_config.mod_ppo2 * 100.0f) / (float)o2_pct - 1.0f) * 10.0f;
}

static void format_gas_name(char *out, size_t out_size, uint8_t o2_pct, uint8_t he_pct)
{
    if (!out || out_size == 0U)
    {
        return;
    }
    if (o2_pct == 0U)
    {
        lv_snprintf(out, out_size, "OFF");
    }
    else if (he_pct > 0U)
    {
        lv_snprintf(out, out_size, "Trimix %u/%u", (unsigned)o2_pct, (unsigned)he_pct);
    }
    else if (o2_pct == 21U)
    {
        lv_snprintf(out, out_size, "AIR");
    }
    else if (o2_pct == 100U)
    {
        lv_snprintf(out, out_size, "O2 100%%");
    }
    else
    {
        lv_snprintf(out, out_size, "EAN%u", (unsigned)o2_pct);
    }
}

static uint16_t plan_round_u16(float value)
{
    if (value <= 0.0f)
    {
        return 0U;
    }
    if (value >= 65535.0f)
    {
        return 65535U;
    }
    return (uint16_t)(value + 0.5f);
}

static uint8_t plan_gf_low(void)
{
    return g_sensor_data.gf_low ? g_sensor_data.gf_low : 40U;
}

static uint8_t plan_gf_high(void)
{
    return g_sensor_data.gf_high ? g_sensor_data.gf_high : 85U;
}

static uint8_t plan_last_deco_depth(void)
{
    return (g_sys_config.last_deco_stop_m == 6U) ? 6U : 3U;
}

static uint8_t last_deco_mode_from_config(void)
{
    return (g_sys_config.last_deco_stop_m == 6U) ? 1U : 0U;
}

static uint8_t salinity_mode_from_config(void)
{
    return (g_sys_config.salinity_mode <= 2U) ? g_sys_config.salinity_mode : 0U;
}

static void plan_ensure_defaults(void)
{
    if (s_plan_defaults_loaded)
    {
        return;
    }

    s_plan_depth_m = (g_sensor_data.max_depth >= 3.0f) ? g_sensor_data.max_depth : 30.0f;
    s_plan_time_min = (g_sensor_data.dive_time_s > 0U)
                      ? (uint16_t)((g_sensor_data.dive_time_s + 59U) / 60U)
                      : 20U;
    if (s_plan_time_min < 1U)
    {
        s_plan_time_min = 1U;
    }
    s_plan_rmv_lpm = 14.0f;
    s_plan_defaults_loaded = true;
}

static void plan_format_gas_summary(char *out, size_t out_size)
{
    if (!out || out_size == 0U)
    {
        return;
    }

    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count > 3U)
    {
        gas_count = 3U;
    }
    if (gas_count == 0U)
    {
        lv_snprintf(out, out_size, "GAS: AIR");
        return;
    }

    size_t used = 0U;
    uint8_t valid_count = 0U;
    int written = lv_snprintf(out, out_size, "GAS:");
    if (written > 0)
    {
        used = (size_t)written;
    }
    for (uint8_t i = 0; i < gas_count && valid_count < 3U && used + 1U < out_size; i++)
    {
        uint8_t o2 = g_sensor_data.gas_slot_o2_pct[i];
        uint8_t he = g_sensor_data.gas_slot_he_pct[i];
        if (o2 == 0U || o2 > 100U || he > 100U || (uint16_t)o2 + (uint16_t)he > 100U)
        {
            continue;
        }
        written = lv_snprintf(out + used,
                              out_size - used,
                              " %u/%02u",
                              (unsigned)o2,
                              (unsigned)he);
        if (written <= 0)
        {
            break;
        }
        used += (size_t)written;
        valid_count++;
    }
    if (valid_count == 0U)
    {
        lv_snprintf(out, out_size, "GAS: AIR");
    }
}

static const char *plan_row_time_text(uint8_t row_index, char *buf, size_t buf_size)
{
#ifdef PC_SIMULATOR
    const buhlmann_debug_plan_row_t *row = &s_plan_result.entries[row_index];
    switch (row->type)
    {
    case BUHLMANN_DEBUG_PLAN_ROW_ASCENT:
        return "asc";
    case BUHLMANN_DEBUG_PLAN_ROW_DECO_STOP:
        lv_snprintf(buf, buf_size, "%u", (unsigned)row->time_min);
        return buf;
    case BUHLMANN_DEBUG_PLAN_ROW_BOTTOM:
    default:
        return "bot";
    }
#else
    (void)row_index;
    (void)buf;
    (void)buf_size;
    return "--";
#endif
}

static uint8_t plan_result_total_pages(void)
{
#ifdef PC_SIMULATOR
    if (s_plan_result.entry_count == 0U)
    {
        return 1U;
    }
    return (uint8_t)((s_plan_result.entry_count + AREX_PLAN_ROWS_PER_PAGE - 1U) /
                     AREX_PLAN_ROWS_PER_PAGE);
#else
    return 1U;
#endif
}

static void plan_build_action_items(uint8_t *out_count)
{
    uint8_t n = 0;
    plan_ensure_defaults();

    s_plan_dyn[n++] = "Exit";
    if (s_plan_page == AREX_PLAN_PAGE_READY)
    {
        s_plan_dyn[n++] = "Plan >";
    }
    else if (s_plan_page == AREX_PLAN_PAGE_RESULT)
    {
        uint8_t total_pages = plan_result_total_pages();
        if (s_plan_result_page + 1U < total_pages)
        {
            s_plan_dyn[n++] = "More >";
        }
        else
        {
            s_plan_dyn[n++] = "Next >";
        }
    }
    else
    {
        s_plan_dyn[n++] = "Next >";
    }
    *out_count = n;
}

static void format_oc_tech_list_item(char *out, size_t out_size, uint8_t slot)
{
    uint8_t o2 = s_oc_tech_o2_pct[slot];
    uint8_t he = s_oc_tech_he_pct[slot];

    if (!out || out_size == 0U)
    {
        return;
    }
    if ((uint16_t)o2 + (uint16_t)he > 100U)
    {
        he = (uint8_t)(100U - o2);
    }

    if (o2 == 0U)
    {
        lv_snprintf(out, out_size, "G%u: OFF", (unsigned)(slot + 1U));
    }
    else if (he > 0U)
    {
        lv_snprintf(out, out_size, "G%u: TX %u/%u", (unsigned)(slot + 1U), (unsigned)o2, (unsigned)he);
    }
    else if (o2 == 21U)
    {
        lv_snprintf(out, out_size, "G%u: AIR", (unsigned)(slot + 1U));
    }
    else if (o2 == 100U)
    {
        lv_snprintf(out, out_size, "G%u: O2 100%%", (unsigned)(slot + 1U));
    }
    else
    {
        lv_snprintf(out, out_size, "G%u: NX %u", (unsigned)(slot + 1U), (unsigned)o2);
    }
}

static bool oc_tech_slot_from_title(const char *title, uint8_t *out_slot)
{
    unsigned slot_no = 0;
    const char *clean_title = strip_title_prefix(title);

    if (!clean_title)
    {
        return false;
    }
    if (sscanf(clean_title, "G%u TRIMIX", &slot_no) != 1)
    {
        return false;
    }
    if (slot_no < 1U || slot_no > 5U)
    {
        return false;
    }
    if (out_slot)
    {
        *out_slot = (uint8_t)(slot_no - 1U);
    }
    return true;
}

static void begin_oc_tech_slot_edit(uint8_t slot)
{
    if (slot >= 5U)
    {
        slot = 0;
    }
    s_oc_tech_edit_slot = slot;
    s_oc_tech_draft_o2_pct[slot] = s_oc_tech_o2_pct[slot];
    s_oc_tech_draft_he_pct[slot] = s_oc_tech_he_pct[slot];
    if (s_oc_tech_draft_o2_pct[slot] < 8U)
    {
        s_oc_tech_draft_o2_pct[slot] = 21U;
    }
    if ((uint16_t)s_oc_tech_draft_o2_pct[slot] + (uint16_t)s_oc_tech_draft_he_pct[slot] > 100U)
    {
        s_oc_tech_draft_he_pct[slot] = (uint8_t)(100U - s_oc_tech_draft_o2_pct[slot]);
    }
}

static void save_oc_tech_slot(uint8_t slot)
{
    if (slot >= 5U)
    {
        return;
    }
    s_oc_tech_o2_pct[slot] = s_oc_tech_draft_o2_pct[slot];
    s_oc_tech_he_pct[slot] = s_oc_tech_draft_he_pct[slot];
    if (s_oc_tech_o2_pct[slot] < 8U)
    {
        s_oc_tech_o2_pct[slot] = 8U;
    }
    if ((uint16_t)s_oc_tech_o2_pct[slot] + (uint16_t)s_oc_tech_he_pct[slot] > 100U)
    {
        s_oc_tech_he_pct[slot] = (uint8_t)(100U - s_oc_tech_o2_pct[slot]);
    }
}

static void apply_air_mode_gases(void)
{
    arex_bus_set_gas_slot_count(1);
    arex_bus_set_gas_slot(0, "AIR", 21, 0, gas_mod_for_o2(21));
    arex_bus_set_gas(0, "AIR");
    arex_bus_set_gas_mix(21, 0);
    arex_bus_set_fio2(21.0f);
}

static void apply_nitrox_mode_gases(void)
{
    char name[16];
    format_gas_name(name, sizeof(name), s_nitrox_o2_pct, 0);
    arex_bus_set_gas_slot_count(1);
    arex_bus_set_gas_slot(0, name, s_nitrox_o2_pct, 0, gas_mod_for_o2(s_nitrox_o2_pct));
    arex_bus_set_gas(0, name);
    arex_bus_set_gas_mix(s_nitrox_o2_pct, 0);
    arex_bus_set_fio2((float)s_nitrox_o2_pct);
}

static void apply_three_gas_mode_gases(void)
{
    char name[3][16];
    uint8_t gas_count = s_three_gas_count;
    if (gas_count == 0U || gas_count > 3U)
    {
        gas_count = 3U;
    }

    for (uint8_t i = 0; i < 3U; i++)
    {
        format_gas_name(name[i], sizeof(name[i]), s_three_gas_o2_pct[i], 0);
        arex_bus_set_gas_slot(i,
                              name[i],
                              s_three_gas_o2_pct[i],
                              0,
                              gas_mod_for_o2(s_three_gas_o2_pct[i]));
    }
    arex_bus_set_gas_slot_count(gas_count);
    arex_bus_set_gas(0, name[0]);
    arex_bus_set_gas_mix(s_three_gas_o2_pct[0], 0);
    arex_bus_set_fio2((float)s_three_gas_o2_pct[0]);
}

static void apply_oc_tech_mode_gases(void)
{
    uint8_t active_count = 0;
    char name[16];

    for (uint8_t i = 0; i < 5U; i++)
    {
        uint8_t o2 = s_oc_tech_o2_pct[i];
        uint8_t he = s_oc_tech_he_pct[i];
        if (o2 == 0U)
        {
            continue;
        }
        if ((uint16_t)o2 + (uint16_t)he > 100U)
        {
            he = (uint8_t)(100U - o2);
        }

        format_gas_name(name, sizeof(name), o2, he);
        arex_bus_set_gas_slot(active_count, name, o2, he, gas_mod_for_o2(o2));
        active_count++;
    }

    for (uint8_t i = active_count; i < GAS_COUNT; i++)
    {
        arex_bus_set_gas_slot(i, "", 0, 0, 0.0f);
    }

    arex_bus_set_gas_slot_count(active_count);
    if (active_count > 0U)
    {
        const char *first_name = g_sensor_data.gas_slot_name[0][0] ? g_sensor_data.gas_slot_name[0] : "GAS 1";
        arex_bus_set_gas(0, first_name);
        arex_bus_set_gas_mix(g_sensor_data.gas_slot_o2_pct[0], g_sensor_data.gas_slot_he_pct[0]);
        arex_bus_set_fio2((float)g_sensor_data.gas_slot_o2_pct[0]);
    }
    else
    {
        arex_bus_set_gas(0, "--");
        arex_bus_set_gas_mix(0, 0);
        arex_bus_set_fio2(0.0f);
    }
}

static void apply_dive_mode_gases(uint8_t mode)
{
    switch (mode)
    {
    case 1:
        apply_nitrox_mode_gases();
        break;
    case 2:
        apply_three_gas_mode_gases();
        break;
    case 3:
        apply_oc_tech_mode_gases();
        break;
    default:
        apply_air_mode_gases();
        break;
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
    static const char *labels[] = { "FRESH", "SALT", "EN13319" };
    if (value >= (sizeof(labels) / sizeof(labels[0])))
    {
        value = 0;
    }
    return labels[value];
}

static const char *safety_stop_label(uint8_t value)
{
    if (value >= (sizeof(s_safety_stop_values) / sizeof(s_safety_stop_values[0])))
    {
        value = 1;
    }
    if (s_safety_stop_values[value] == 0)
    {
        return "OFF";
    }
    static char label[8];
    snprintf(label, sizeof(label), "%umin", s_safety_stop_values[value]);
    return label;
}

static const char *last_deco_label(uint8_t value)
{
    if (value >= (sizeof(s_last_deco_values) / sizeof(s_last_deco_values[0])))
    {
        value = 0;
    }
    static char label[8];
    snprintf(label, sizeof(label), "%um", s_last_deco_values[value]);
    return label;
}

static const char *altitude_label(uint8_t value)
{
    static const char *labels[] = { "AUTO", "SEA", "L1", "L2" };
    if (value >= (sizeof(labels) / sizeof(labels[0])))
    {
        value = 0;
    }
    return labels[value];
}

static const char *dive_mode_label(uint8_t value)
{
    if (value >= 4)
    {
        value = 0;
    }
    return s_nested_mode_setup[value];
}

static const char *ai_state_label(uint8_t value)
{
    static const char *labels[] = { "UNPAIRED", "PAIRING", "PAIRED" };
    if (value >= (sizeof(labels) / sizeof(labels[0])))
    {
        value = 0;
    }
    return labels[value];
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

static void format_duration(char *out, size_t out_size, uint32_t total_s)
{
    uint32_t h = total_s / 3600U;
    uint32_t m = (total_s % 3600U) / 60U;
    uint32_t s = total_s % 60U;

    if (!out || out_size == 0U)
    {
        return;
    }
    if (h > 0U)
    {
        snprintf(out, out_size, "%luh %02lum", (unsigned long)h, (unsigned long)m);
    }
    else
    {
        snprintf(out, out_size, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
    }
}

static uint8_t max_tissue_pct(void)
{
    uint8_t max_pct = 0;
    for (uint8_t i = 0; i < 16U; i++)
    {
        if (g_sensor_data.tissue_pct[i] > max_pct)
        {
            max_pct = g_sensor_data.tissue_pct[i];
        }
    }
    return max_pct;
}

static void format_pressure(char *out, size_t out_size, const char *label, float bar)
{
    if (!out || out_size == 0U)
    {
        return;
    }
    if (bar <= 0.0f)
    {
        snprintf(out, out_size, "%s: -- BAR", label);
    }
    else
    {
        snprintf(out, out_size, "%s: %.0f BAR", label, (double)bar);
    }
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
    {
        char dive_time[12];
        char surface_time[12];
        format_duration(dive_time, sizeof(dive_time), g_sensor_data.dive_time_s);
        format_duration(surface_time, sizeof(surface_time), g_sensor_data.surface_time_s);
        snprintf(s_info_str[0][0], sizeof(s_info_str[0][0]), "MAX DEPTH: %.1fm", (double)g_sensor_data.max_depth);
        snprintf(s_info_str[0][1], sizeof(s_info_str[0][1]), "AVG DEPTH: %.1fm", (double)g_sensor_data.avg_depth);
        snprintf(s_info_str[0][2], sizeof(s_info_str[0][2]), "DIVE TIME: %s", dive_time);
        snprintf(s_info_str[0][3], sizeof(s_info_str[0][3]), "SURFACE: %s", surface_time);
        s_info_dyn[0][n++] = s_info_str[0][0];
        s_info_dyn[0][n++] = s_info_str[0][1];
        s_info_dyn[0][n++] = s_info_str[0][2];
        s_info_dyn[0][n++] = s_info_str[0][3];
        break;
    }
    case 1:
        plan_build_action_items(&n);
        if (out_count)
        {
            *out_count = n;
        }
        return s_plan_dyn;
    case 2:
    {
        uint8_t gf_low = g_sensor_data.gf_low ? g_sensor_data.gf_low : 30U;
        uint8_t gf_high = g_sensor_data.gf_high ? g_sensor_data.gf_high : 70U;
        snprintf(s_info_str[2][0], sizeof(s_info_str[2][0]), "GF: %u/%u", (unsigned)gf_low, (unsigned)gf_high);
        snprintf(s_info_str[2][1], sizeof(s_info_str[2][1]), "GF99: %.0f%%", (double)g_sensor_data.gf99);
        snprintf(s_info_str[2][2], sizeof(s_info_str[2][2]), "SURF GF: %.0f%%", (double)g_sensor_data.surf_gf);
        snprintf(s_info_str[2][3], sizeof(s_info_str[2][3]), "TISSUE: %u%%", (unsigned)max_tissue_pct());
        snprintf(s_info_str[2][4], sizeof(s_info_str[2][4]), "CNS: %u%%", (unsigned)g_sensor_data.cns_pct);
        snprintf(s_info_str[2][5], sizeof(s_info_str[2][5]), "OTU: %u", (unsigned)g_sensor_data.otu);
        s_info_dyn[2][n++] = s_info_str[2][0];
        s_info_dyn[2][n++] = s_info_str[2][1];
        s_info_dyn[2][n++] = s_info_str[2][2];
        s_info_dyn[2][n++] = s_info_str[2][3];
        s_info_dyn[2][n++] = s_info_str[2][4];
        s_info_dyn[2][n++] = s_info_str[2][5];
        break;
    }
    case 3:
    {
        uint8_t active_idx = g_sensor_data.gas_active_idx;
        uint8_t gas_count = g_sensor_data.gas_slot_count;
        if (gas_count > GAS_COUNT)
        {
            gas_count = GAS_COUNT;
        }
        if (gas_count == 0U || active_idx >= gas_count)
        {
            active_idx = 0;
        }

        snprintf(s_info_str[3][0],
                 sizeof(s_info_str[3][0]),
                 "ACTIVE: G%u %s",
                 (unsigned)(active_idx + 1U),
                 g_sensor_data.gas_name[0] ? g_sensor_data.gas_name : "--");
        snprintf(s_info_str[3][1],
                 sizeof(s_info_str[3][1]),
                 "MIX: O2 %u%% HE %u%%",
                 (unsigned)g_sensor_data.gas_o2_pct,
                 (unsigned)g_sensor_data.gas_he_pct);
        snprintf(s_info_str[3][2],
                 sizeof(s_info_str[3][2]),
                 "MOD: %.0fm",
                 (double)((active_idx < GAS_COUNT && g_sensor_data.gas_slot_mod_m[active_idx] > 0.0f)
                          ? g_sensor_data.gas_slot_mod_m[active_idx]
                          : g_sensor_data.mod_m));
        snprintf(s_info_str[3][3],
                 sizeof(s_info_str[3][3]),
                 "PPO2: %.2f",
                 (double)((active_idx < GAS_COUNT) ? g_sensor_data.ppo2[active_idx] : 0.0f));
        snprintf(s_info_str[3][4], sizeof(s_info_str[3][4]), "DENS: %.1fg/L", (double)g_sensor_data.gas_density);
        s_info_dyn[3][n++] = s_info_str[3][0];
        s_info_dyn[3][n++] = s_info_str[3][1];
        s_info_dyn[3][n++] = s_info_str[3][2];
        s_info_dyn[3][n++] = s_info_str[3][3];
        s_info_dyn[3][n++] = s_info_str[3][4];
        break;
    }
    case 4:
    {
        float battery_pct = g_sensor_data.battery_pct;
        if (battery_pct < 0.0f)
        {
            battery_pct = 0.0f;
        }
        else if (battery_pct > 100.0f)
        {
            battery_pct = 100.0f;
        }
        format_pressure(s_info_str[4][0], sizeof(s_info_str[4][0]), "POD 1", g_sensor_data.pod1_bar);
        format_pressure(s_info_str[4][1], sizeof(s_info_str[4][1]), "POD 2", g_sensor_data.pod2_bar);
        snprintf(s_info_str[4][2], sizeof(s_info_str[4][2]), "BATTERY: %.0f%%", battery_pct);
        snprintf(s_info_str[4][3], sizeof(s_info_str[4][3]), "TEMP: %.1fC", (double)g_sensor_data.temperature_c);
        if (g_sensor_data.bat_temperature_valid && g_sensor_data.prj_temperature_valid)
        {
            snprintf(s_info_str[4][4],
                     sizeof(s_info_str[4][4]),
                     "BAT/PRJ: %.1f/%.1fC",
                     (double)g_sensor_data.bat_temperature_c,
                     (double)g_sensor_data.prj_temperature_c);
        }
        else if (g_sensor_data.bat_temperature_valid)
        {
            snprintf(s_info_str[4][4],
                     sizeof(s_info_str[4][4]),
                     "BAT TEMP: %.1fC",
                     (double)g_sensor_data.bat_temperature_c);
        }
        else if (g_sensor_data.prj_temperature_valid)
        {
            snprintf(s_info_str[4][4],
                     sizeof(s_info_str[4][4]),
                     "PRJ TEMP: %.1fC",
                     (double)g_sensor_data.prj_temperature_c);
        }
        else
        {
            snprintf(s_info_str[4][4], sizeof(s_info_str[4][4]), "BAT/PRJ: --/--C");
        }
        s_info_dyn[4][n++] = s_info_str[4][0];
        s_info_dyn[4][n++] = s_info_str[4][1];
        s_info_dyn[4][n++] = s_info_str[4][2];
        s_info_dyn[4][n++] = s_info_str[4][3];
        s_info_dyn[4][n++] = s_info_str[4][4];
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
    s_system_setup_dyn[0] = "VERSION: " SYSTEM_VERSION;
    s_system_setup_dyn[1] = s_system_mode_str;
    s_system_setup_dyn[2] = "DIVE SETUP";
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

static const char **build_gas_switch_items(uint8_t *out_count)
{
    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count > GAS_COUNT)
    {
        gas_count = GAS_COUNT;
    }
    if (gas_count == 0U)
    {
        s_gas_switch_dyn[0] = "NO ACTIVE GAS";
        s_gas_switch_dyn[1] = NULL;
        if (out_count)
        {
            *out_count = 1U;
        }
        return s_gas_switch_dyn;
    }

    for (uint8_t i = 0; i < gas_count; i++)
    {
        const char *slot_name = g_sensor_data.gas_slot_name[i][0]
                                ? g_sensor_data.gas_slot_name[i]
                                : GAS_NAMES[i];
        if (gas_count > 1U)
        {
            lv_snprintf(s_gas_switch_str[i],
                        sizeof(s_gas_switch_str[i]),
                        "GAS %u: %s",
                        (unsigned)(i + 1U),
                        slot_name);
        }
        else
        {
            lv_snprintf(s_gas_switch_str[i], sizeof(s_gas_switch_str[i]), "%s", slot_name);
        }
        s_gas_switch_dyn[i] = s_gas_switch_str[i];
    }
    s_gas_switch_dyn[gas_count] = NULL;
    if (out_count)
    {
        *out_count = gas_count;
    }
    return s_gas_switch_dyn;
}

static const char **build_nested_nitrox(uint8_t *out_count)
{
    snprintf(s_nitrox_o2_str, sizeof(s_nitrox_o2_str), "O2: %u%%", s_nitrox_o2_pct);
    s_nested_nitrox[0] = s_nitrox_o2_str;
    s_nested_nitrox[1] = "CONFIRM";
    s_nested_nitrox[2] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_nitrox, 3);
    }
    return s_nested_nitrox;
}

static const char **build_nested_three_gas(uint8_t *out_count)
{
    for (uint8_t i = 0; i < 3U; i++)
    {
        snprintf(s_three_gas_o2_str[i],
                 sizeof(s_three_gas_o2_str[i]),
                 "GAS %u: %u%%",
                 (unsigned)(i + 1U),
                 (unsigned)s_three_gas_o2_pct[i]);
        s_nested_three_gas[i] = s_three_gas_o2_str[i];
    }
    snprintf(s_three_gas_count_str,
             sizeof(s_three_gas_count_str),
             "ACTIVE GASES: %u",
             (unsigned)s_three_gas_count);
    s_nested_three_gas[3] = s_three_gas_count_str;
    s_nested_three_gas[4] = "CONFIRM";
    s_nested_three_gas[5] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_three_gas, 6);
    }
    return s_nested_three_gas;
}

static const char **build_nested_oc_tech(uint8_t *out_count)
{
    for (uint8_t i = 0; i < 5U; i++)
    {
        format_oc_tech_list_item(s_oc_tech_gas_str[i], sizeof(s_oc_tech_gas_str[i]), i);
        s_nested_oc_tech[i] = s_oc_tech_gas_str[i];
    }
    s_nested_oc_tech[5] = "CONFIRM & ACTIVATE";
    s_nested_oc_tech[6] = "< BACK";
    s_nested_oc_tech[7] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_oc_tech, 8);
    }
    return s_nested_oc_tech;
}

static const char **build_nested_oc_tech_edit(uint8_t slot, uint8_t *out_count)
{
    if (slot >= 5U)
    {
        slot = s_oc_tech_edit_slot;
    }
    if (slot >= 5U)
    {
        slot = 0;
    }

    snprintf(s_oc_tech_edit_str[0],
             sizeof(s_oc_tech_edit_str[0]),
             "O2 PERCENT: %u%%",
             (unsigned)s_oc_tech_draft_o2_pct[slot]);
    snprintf(s_oc_tech_edit_str[1],
             sizeof(s_oc_tech_edit_str[1]),
             "HE PERCENT: %u%%",
             (unsigned)s_oc_tech_draft_he_pct[slot]);
    s_nested_oc_tech_edit[0] = s_oc_tech_edit_str[0];
    s_nested_oc_tech_edit[1] = s_oc_tech_edit_str[1];
    s_nested_oc_tech_edit[2] = "SAVE GAS CONFIG";
    s_nested_oc_tech_edit[3] = "< BACK";
    s_nested_oc_tech_edit[4] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_oc_tech_edit, 5);
    }
    return s_nested_oc_tech_edit;
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
    if (strcmp(s_setup_titles[index], "GAS SWITCH") == 0)
    {
        return build_gas_switch_items(out_count);
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
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", (double)g_sys_config.mod_ppo2);
    snprintf(s_salinity_str,
             sizeof(s_salinity_str),
             "SALINITY: %s",
             salinity_label(salinity_mode_from_config()));
    snprintf(s_safety_stop_str, sizeof(s_safety_stop_str), "SAFETY STOP: %s", safety_stop_label(s_safety_stop_mode));
    snprintf(s_last_deco_str,
             sizeof(s_last_deco_str),
             "LAST DECO: %s",
             last_deco_label(last_deco_mode_from_config()));
    snprintf(s_altitude_str, sizeof(s_altitude_str), "ALTITUDE: %s", altitude_label(s_altitude_level));
    s_nested_dive_setup[0] = s_salinity_str;
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = s_safety_stop_str;
    s_nested_dive_setup[3] = s_last_deco_str;
    s_nested_dive_setup[4] = s_altitude_str;
    s_nested_dive_setup[5] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_dive_setup, 6);
    }
    return s_nested_dive_setup;
}

static const char **build_nested_ai_setup(uint8_t *out_count)
{
    snprintf(s_ai_gtr_str, sizeof(s_ai_gtr_str), "GTR MODE: %s", gtr_label(s_gtr_enabled));
    static char t1_str[28];
    static char t2_str[28];
    snprintf(t1_str, sizeof(t1_str), "T1 MAIN: %s", ai_state_label(s_ai_tank_state[0]));
    snprintf(t2_str, sizeof(t2_str), "T2 BUDDY: %s", ai_state_label(s_ai_tank_state[1]));
    s_nested_ai_setup[0] = t1_str;
    s_nested_ai_setup[1] = t2_str;
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
    snprintf(s_alert_ndl_str, sizeof(s_alert_ndl_str), "LOW NDL ALARM: %umin", s_ndl_alarm_min);
    s_nested_alerts_setup[0] = s_alert_depth_str;
    s_nested_alerts_setup[1] = s_alert_time_str;
    s_nested_alerts_setup[2] = s_alert_ndl_str;
    s_nested_alerts_setup[3] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_alerts_setup, 4);
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
    s_nested_datetime[0] = s_datetime_year_str;
    s_nested_datetime[1] = s_datetime_month_str;
    s_nested_datetime[2] = s_datetime_day_str;
    s_nested_datetime[3] = s_datetime_hour_str;
    s_nested_datetime[4] = s_datetime_minute_str;
    s_nested_datetime[5] = NULL;
    if (out_count)
    {
        *out_count = count_items(s_nested_datetime, 6);
    }
    return s_nested_datetime;
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
    else if (strcmp(clean_title, "NITROX") == 0) return build_nested_nitrox(out_count);
    else if (strcmp(clean_title, "3 GAS") == 0) return build_nested_three_gas(out_count);
    else if (strcmp(clean_title, "OC Tech") == 0) return build_nested_oc_tech(out_count);
    else if (oc_tech_slot_from_title(clean_title, &s_oc_tech_edit_slot)) return build_nested_oc_tech_edit(s_oc_tech_edit_slot, out_count);
    else if (strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) return build_nested_dive_setup(out_count);
    else if (strcmp(clean_title, "AI SETUP") == 0) return build_nested_ai_setup(out_count);
    else if (strcmp(clean_title, "ALERTS SETUP") == 0) return build_nested_alerts_setup(out_count);
    else if (strcmp(clean_title, "DISPLAY") == 0) return build_nested_display_sys(out_count);
    else if (strcmp(clean_title, "DATE & CLOCK") == 0) return build_nested_datetime(out_count);
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
            "DIVE SETUP",
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
    else if (clean_current_title && strcmp(clean_current_title, "MODE SETUP") == 0)
    {
        if (item_index == 1)
        {
            lv_snprintf(key, sizeof(key), "%s", "NITROX");
        }
        else if (item_index == 2)
        {
            lv_snprintf(key, sizeof(key), "%s", "3 GAS");
        }
        else if (item_index == 3)
        {
            lv_snprintf(key, sizeof(key), "%s", "OC Tech");
        }
        else
        {
            key[0] = '\0';
        }
    }
    else if (clean_current_title && strcmp(clean_current_title, "OC Tech") == 0)
    {
        if (item_index < 5U)
        {
            begin_oc_tech_slot_edit(item_index);
            lv_snprintf(key, sizeof(key), "G%u TRIMIX", (unsigned)(item_index + 1U));
            items = build_nested_oc_tech_edit(item_index, &count);
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
        key[0] = '\0';
    }
    else
    {
        normalize_menu_key(item_text, key, sizeof(key));
        if (strcmp(clean_current_title ? clean_current_title : "", "DISPLAY") == 0)
        {
            if (item_index == 1)
            {
                lv_snprintf(key, sizeof(key), "%s", "DATE & CLOCK");
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

    if (strcmp(clean_title, "MODE SETUP") == 0 && item_index == 0)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nAIR");
        return true;
    }

    if (strcmp(clean_title, "NITROX") == 0 && item_index == 1)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 1;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nNITROX %u%%", (unsigned)s_nitrox_o2_pct);
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index == 4)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 2;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\n3 GAS / %u ACTIVE", (unsigned)s_three_gas_count);
        return true;
    }

    if (strcmp(clean_title, "OC Tech") == 0 && item_index == 5)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 3;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nOC Tech ACTIVE");
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

bool arex_submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                arex_submenu_setting_confirm_t *out_setting)
{
    const char *clean_title = strip_title_prefix(current_title);
    (void)item_text;
    if (!clean_title || !out_setting)
    {
        return false;
    }

    memset(out_setting, 0, sizeof(*out_setting));

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 0)
    {
        uint8_t next = (uint8_t)((salinity_mode_from_config() + 1U) % 3U);
        out_setting->kind = AREX_SUBMENU_SETTING_SALINITY;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 2)
    {
        uint8_t next = (uint8_t)((s_safety_stop_mode + 1) %
                       (sizeof(s_safety_stop_values) / sizeof(s_safety_stop_values[0])));
        out_setting->kind = AREX_SUBMENU_SETTING_SAFETY_STOP;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 3)
    {
        uint8_t next = (uint8_t)((last_deco_mode_from_config() + 1U) %
                       (sizeof(s_last_deco_values) / sizeof(s_last_deco_values[0])));
        out_setting->kind = AREX_SUBMENU_SETTING_LAST_DECO;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 4)
    {
        uint8_t next = (uint8_t)((s_altitude_level + 1) % 4);
        out_setting->kind = AREX_SUBMENU_SETTING_ALTITUDE;
        out_setting->value = next;
        return true;
    }

    if (strcmp(clean_title, "AI SETUP") == 0 && item_index < 2)
    {
        uint8_t next = (uint8_t)((s_ai_tank_state[item_index] + 1) % 3);
        out_setting->kind = AREX_SUBMENU_SETTING_AI_TANK_STATE;
        out_setting->arg = item_index;
        out_setting->value = next;
        return true;
    }

    if (strcmp(clean_title, "AI SETUP") == 0 && item_index == 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_GTR_MODE;
        out_setting->value = s_gtr_enabled ? 0 : 1;
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 0)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_UNITS;
        out_setting->value = (s_units_mode == 0) ? 1 : 0;
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 2)
    {
        uint8_t next_index = 0;
        for (uint8_t i = 0; i < (sizeof(s_log_rate_values) / sizeof(s_log_rate_values[0])); i++)
        {
            if (s_log_rate_values[i] == s_log_rate_s)
            {
                next_index = (uint8_t)((i + 1) % (sizeof(s_log_rate_values) / sizeof(s_log_rate_values[0])));
                break;
            }
        }
        out_setting->kind = AREX_SUBMENU_SETTING_LOG_RATE;
        out_setting->value = s_log_rate_values[next_index];
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 3)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_BLUETOOTH;
        out_setting->value = s_bluetooth_enabled ? 0 : 1;
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index == 3)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_3GAS_COUNT;
        out_setting->value = (s_three_gas_count >= 3U) ? 1U : (uint16_t)(s_three_gas_count + 1U);
        return true;
    }

    if (oc_tech_slot_from_title(clean_title, &s_oc_tech_edit_slot) && item_index == 2)
    {
        out_setting->kind = AREX_SUBMENU_SETTING_OC_TECH_SAVE;
        out_setting->arg = s_oc_tech_edit_slot;
        return true;
    }

    return false;
}

bool arex_submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           arex_submenu_edit_spec_t *out_spec)
{
    const char *clean_title = strip_title_prefix(current_title);
    (void)item_text;
    if (!clean_title || !out_spec)
    {
        return false;
    }
    memset(out_spec, 0, sizeof(*out_spec));

    if (strcmp(clean_title, "DIVE PLAN") == 0)
    {
        (void)item_index;
        return false;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 1)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_MOD_PPO2;
        out_spec->value = g_sys_config.mod_ppo2;
        out_spec->min = 1.0f;
        out_spec->max = 1.6f;
        out_spec->step = 0.1f;
        out_spec->decimals = 1;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "MOD PO2:");
        return true;
    }

    if (strcmp(clean_title, "NITROX") == 0 && item_index == 0)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_NITROX_O2;
        out_spec->value = (float)s_nitrox_o2_pct;
        out_spec->min = 21.0f;
        out_spec->max = 40.0f;
        out_spec->step = 1.0f;
        out_spec->decimals = 0;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "O2:");
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index < 3)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_3GAS_O2;
        out_spec->arg = item_index;
        out_spec->value = (float)s_three_gas_o2_pct[item_index];
        out_spec->min = 21.0f;
        out_spec->max = 100.0f;
        out_spec->step = 1.0f;
        out_spec->decimals = 0;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "GAS %u:", (unsigned)(item_index + 1U));
        return true;
    }

    if (oc_tech_slot_from_title(clean_title, &s_oc_tech_edit_slot) && item_index < 2)
    {
        uint8_t slot = s_oc_tech_edit_slot;
        bool edit_he = (item_index == 1U);
        uint8_t o2 = s_oc_tech_draft_o2_pct[slot];
        uint8_t he = s_oc_tech_draft_he_pct[slot];

        out_spec->kind = AREX_SUBMENU_SETTING_OC_TECH_GAS;
        out_spec->arg = (uint8_t)(slot * 2U + item_index);
        out_spec->step = 1.0f;
        out_spec->decimals = 0;
        if (edit_he)
        {
            out_spec->value = (float)he;
            out_spec->min = 0.0f;
            out_spec->max = (float)(100U - o2);
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "HE PERCENT:");
        }
        else
        {
            out_spec->value = (float)o2;
            out_spec->min = 8.0f;
            out_spec->max = (float)(100U - he);
            if (out_spec->max < out_spec->min)
            {
                out_spec->max = out_spec->min;
            }
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "O2 PERCENT:");
        }
        return true;
    }

    if (strcmp(clean_title, "ALERTS SETUP") == 0 && item_index == 0)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_DEPTH_ALARM;
        out_spec->value = (float)s_depth_alarm_m;
        out_spec->min = 10.0f;
        out_spec->max = 150.0f;
        out_spec->step = 10.0f;
        out_spec->decimals = 0;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "DEPTH:");
        return true;
    }

    if (strcmp(clean_title, "ALERTS SETUP") == 0 && item_index == 1)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_TIME_ALARM;
        out_spec->value = (float)s_time_alarm_min;
        out_spec->min = 10.0f;
        out_spec->max = 300.0f;
        out_spec->step = 10.0f;
        out_spec->decimals = 0;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "TIME:");
        return true;
    }

    if (strcmp(clean_title, "DATE & CLOCK") == 0)
    {
        out_spec->kind = AREX_SUBMENU_SETTING_DATETIME_FIELD;
        out_spec->decimals = 0;
        out_spec->step = 1.0f;

        switch (item_index)
        {
        case 0:
            out_spec->arg = AREX_DATETIME_FIELD_YEAR;
            out_spec->value = (float)s_datetime_year;
            out_spec->min = 2000.0f;
            out_spec->max = 2099.0f;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "YEAR:");
            return true;
        case 1:
            out_spec->arg = AREX_DATETIME_FIELD_MONTH;
            out_spec->value = (float)s_datetime_month;
            out_spec->min = 1.0f;
            out_spec->max = 12.0f;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "MONTH:");
            return true;
        case 2:
            out_spec->arg = AREX_DATETIME_FIELD_DAY;
            out_spec->value = (float)s_datetime_day;
            out_spec->min = 1.0f;
            out_spec->max = 31.0f;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "DAY:");
            return true;
        case 3:
            out_spec->arg = AREX_DATETIME_FIELD_HOUR;
            out_spec->value = (float)s_datetime_hour;
            out_spec->min = 0.0f;
            out_spec->max = 23.0f;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "HOUR:");
            return true;
        case 4:
            out_spec->arg = AREX_DATETIME_FIELD_MINUTE;
            out_spec->value = (float)s_datetime_minute;
            out_spec->min = 0.0f;
            out_spec->max = 59.0f;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "MINUTE:");
            return true;
        default:
            break;
        }
    }

    return false;
}

void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t arg, uint16_t value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_DIVE_MODE:
        s_dive_mode = (value > 3) ? 0 : (uint8_t)value;
        apply_dive_mode_gases(s_dive_mode);
        break;
    case AREX_SUBMENU_SETTING_3GAS_COUNT:
        s_three_gas_count = (value < 1 || value > 3) ? 3 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_OC_TECH_SAVE:
        save_oc_tech_slot(arg);
        break;
    case AREX_SUBMENU_SETTING_SALINITY:
        s_salinity_mode = (value > 2) ? 0 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_SAFETY_STOP:
        s_safety_stop_mode = (value > 3) ? 1 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_LAST_DECO:
        s_last_deco_mode = (value > 1) ? 0 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_ALTITUDE:
        s_altitude_level = (value > 3) ? 0 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_AI_TANK_STATE:
        if (arg < 2)
        {
            s_ai_tank_state[arg] = (value > 2) ? 0 : (uint8_t)value;
        }
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
    case AREX_SUBMENU_SETTING_UNITS:
        s_units_mode = (value > 1) ? 0 : (uint8_t)value;
        break;
    case AREX_SUBMENU_SETTING_DATETIME_FIELD:
        switch (arg)
        {
        case AREX_DATETIME_FIELD_YEAR:
            s_datetime_year = (value < 2000 || value > 2099) ? 2026 : value;
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
        default:
            break;
        }
        break;
    case AREX_SUBMENU_SETTING_LOG_RATE:
        s_log_rate_s = (uint8_t)value;
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

void arex_submenu_apply_edit_value(arex_submenu_setting_kind_t kind, uint8_t arg, float value)
{
    switch (kind)
    {
    case AREX_SUBMENU_SETTING_PLAN_DEPTH:
        s_plan_depth_m = (float)plan_round_u16(value);
        if (s_plan_depth_m < 3.0f) s_plan_depth_m = 3.0f;
        if (s_plan_depth_m > 120.0f) s_plan_depth_m = 120.0f;
        s_plan_page = AREX_PLAN_PAGE_READY;
        break;
    case AREX_SUBMENU_SETTING_PLAN_TIME:
        s_plan_time_min = plan_round_u16(value);
        if (s_plan_time_min < 1U) s_plan_time_min = 1U;
        if (s_plan_time_min > 300U) s_plan_time_min = 300U;
        s_plan_page = AREX_PLAN_PAGE_READY;
        break;
    case AREX_SUBMENU_SETTING_PLAN_RMV:
        s_plan_rmv_lpm = (float)plan_round_u16(value);
        if (s_plan_rmv_lpm < 5.0f) s_plan_rmv_lpm = 5.0f;
        if (s_plan_rmv_lpm > 50.0f) s_plan_rmv_lpm = 50.0f;
        s_plan_page = AREX_PLAN_PAGE_READY;
        break;
    case AREX_SUBMENU_SETTING_MOD_PPO2:
        g_sys_config.mod_ppo2 = value;
        break;
    case AREX_SUBMENU_SETTING_NITROX_O2:
        s_nitrox_o2_pct = (uint8_t)(value + 0.5f);
        break;
    case AREX_SUBMENU_SETTING_3GAS_O2:
        if (arg < 3U)
        {
            s_three_gas_o2_pct[arg] = (uint8_t)(value + 0.5f);
        }
        break;
    case AREX_SUBMENU_SETTING_OC_TECH_GAS:
        if (arg < 10U)
        {
            uint8_t slot = (uint8_t)(arg / 2U);
            uint8_t val = (uint8_t)(value + 0.5f);
            if ((arg % 2U) == 0U)
            {
                uint8_t max_o2 = (uint8_t)(100U - s_oc_tech_draft_he_pct[slot]);
                if (max_o2 < 8U)
                {
                    max_o2 = 8U;
                    s_oc_tech_draft_he_pct[slot] = 92U;
                }
                if (val < 8U)
                {
                    val = 8U;
                }
                if (val > max_o2)
                {
                    val = max_o2;
                }
                s_oc_tech_draft_o2_pct[slot] = val;
                if ((uint16_t)s_oc_tech_draft_o2_pct[slot] + (uint16_t)s_oc_tech_draft_he_pct[slot] > 100U)
                {
                    s_oc_tech_draft_he_pct[slot] = (uint8_t)(100U - s_oc_tech_draft_o2_pct[slot]);
                }
            }
            else
            {
                if (s_oc_tech_draft_o2_pct[slot] < 8U)
                {
                    s_oc_tech_draft_o2_pct[slot] = 8U;
                }
                if (val > (uint8_t)(100U - s_oc_tech_draft_o2_pct[slot]))
                {
                    val = (uint8_t)(100U - s_oc_tech_draft_o2_pct[slot]);
                }
                s_oc_tech_draft_he_pct[slot] = val;
            }
        }
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        s_depth_alarm_m = (uint16_t)(value + 0.5f);
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        s_time_alarm_min = (uint16_t)(value + 0.5f);
        break;
    case AREX_SUBMENU_SETTING_DATETIME_FIELD:
    {
        uint16_t int_value = (uint16_t)(value + 0.5f);
        switch (arg)
        {
        case AREX_DATETIME_FIELD_YEAR:
            s_datetime_year = (int_value < 2000 || int_value > 2099) ? 2026 : int_value;
            break;
        case AREX_DATETIME_FIELD_MONTH:
            s_datetime_month = (int_value < 1 || int_value > 12) ? 1 : (uint8_t)int_value;
            break;
        case AREX_DATETIME_FIELD_DAY:
            s_datetime_day = (int_value < 1 || int_value > 31) ? 1 : (uint8_t)int_value;
            break;
        case AREX_DATETIME_FIELD_HOUR:
            s_datetime_hour = (int_value > 23) ? 0 : (uint8_t)int_value;
            break;
        case AREX_DATETIME_FIELD_MINUTE:
            s_datetime_minute = (int_value > 59) ? 0 : (uint8_t)int_value;
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

arex_dive_plan_page_t arex_submenu_dive_plan_page(void)
{
    return (arex_dive_plan_page_t)s_plan_page;
}

void arex_submenu_dive_plan_get_inputs(float *out_depth_m,
                                       uint16_t *out_time_min,
                                       float *out_rmv_lpm)
{
    plan_ensure_defaults();
    if (out_depth_m) *out_depth_m = s_plan_depth_m;
    if (out_time_min) *out_time_min = s_plan_time_min;
    if (out_rmv_lpm) *out_rmv_lpm = s_plan_rmv_lpm;
}

uint8_t arex_submenu_dive_plan_gf_low(void)
{
    return plan_gf_low();
}

uint8_t arex_submenu_dive_plan_gf_high(void)
{
    return plan_gf_high();
}

uint8_t arex_submenu_dive_plan_last_stop_m(void)
{
    return plan_last_deco_depth();
}

uint8_t arex_submenu_dive_plan_header_gas_o2(void)
{
    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count > GAS_COUNT) gas_count = GAS_COUNT;
    for (uint8_t i = 1U; i < gas_count; i++)
    {
        uint8_t o2 = g_sensor_data.gas_slot_o2_pct[i];
        uint8_t he = g_sensor_data.gas_slot_he_pct[i];
        if (o2 > 0U && o2 <= 100U && he <= 100U && (uint16_t)o2 + (uint16_t)he <= 100U)
        {
            return o2;
        }
    }
    return 0U;
}

void arex_submenu_dive_plan_gas_summary(char *out, size_t out_size)
{
    plan_format_gas_summary(out, out_size);
}

void arex_submenu_dive_plan_reset(void)
{
    s_plan_page = AREX_PLAN_PAGE_DEPTH;
    s_plan_defaults_loaded = false;
    s_plan_result_page = 0U;
#ifdef PC_SIMULATOR
    memset(&s_plan_result, 0, sizeof(s_plan_result));
#endif
}

bool arex_submenu_dive_plan_handle_rotate(int8_t dir)
{
    plan_ensure_defaults();
    switch (s_plan_page)
    {
    case AREX_PLAN_PAGE_DEPTH:
        s_plan_depth_m += (float)dir;
        if (s_plan_depth_m < 3.0f) s_plan_depth_m = 3.0f;
        if (s_plan_depth_m > 120.0f) s_plan_depth_m = 120.0f;
        return true;
    case AREX_PLAN_PAGE_TIME:
    {
        int next = (int)s_plan_time_min + (int)dir;
        if (next < 1) next = 1;
        if (next > 300) next = 300;
        s_plan_time_min = (uint16_t)next;
        return true;
    }
    case AREX_PLAN_PAGE_RMV:
        s_plan_rmv_lpm += (float)dir;
        if (s_plan_rmv_lpm < 5.0f) s_plan_rmv_lpm = 5.0f;
        if (s_plan_rmv_lpm > 50.0f) s_plan_rmv_lpm = 50.0f;
        return true;
    default:
        return false;
    }
}

bool arex_submenu_dive_plan_is_result_page(void)
{
    return s_plan_page == AREX_PLAN_PAGE_RESULT;
}

uint8_t arex_submenu_dive_plan_result_page_index(void)
{
    return s_plan_result_page;
}

uint8_t arex_submenu_dive_plan_result_total_pages(void)
{
    return plan_result_total_pages();
}

uint8_t arex_submenu_dive_plan_result_entry_count(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.entry_count;
#else
    return 0U;
#endif
}

bool arex_submenu_dive_plan_result_row(uint8_t row_index, arex_dive_plan_row_t *out_row)
{
    if (!out_row)
    {
        return false;
    }
#ifdef PC_SIMULATOR
    if (row_index >= s_plan_result.entry_count)
    {
        return false;
    }
    const buhlmann_debug_plan_row_t *src = &s_plan_result.entries[row_index];
    out_row->type = (arex_dive_plan_row_type_t)src->type;
    out_row->depth_m = src->depth_m;
    out_row->time_min = src->time_min;
    out_row->run_min = src->run_min;
    out_row->o2_pct = src->o2_pct;
    out_row->he_pct = src->he_pct;
    out_row->gas_l = src->gas_l;
    return true;
#else
    memset(out_row, 0, sizeof(*out_row));
    return false;
#endif
}

uint16_t arex_submenu_dive_plan_total_runtime_min(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.total_runtime_min;
#else
    return 0U;
#endif
}

uint16_t arex_submenu_dive_plan_total_deco_min(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.total_deco_min;
#else
    return 0U;
#endif
}

uint16_t arex_submenu_dive_plan_total_gas_l(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.total_gas_l;
#else
    return 0U;
#endif
}

uint16_t arex_submenu_dive_plan_cns_pct(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.cns_pct;
#else
    return 0U;
#endif
}

uint16_t arex_submenu_dive_plan_otu(void)
{
#ifdef PC_SIMULATOR
    return s_plan_result.otu;
#else
    return 0U;
#endif
}

bool arex_submenu_dive_plan_handle_action(uint8_t item_index,
                                          const char *item_text,
                                          bool *out_close_submenu,
                                          uint8_t *out_keep_index)
{
    (void)item_index;
    if (out_close_submenu)
    {
        *out_close_submenu = false;
    }
    if (out_keep_index)
    {
        *out_keep_index = 0U;
    }
    if (!item_text)
    {
        return false;
    }

    if (strcmp(item_text, "Exit") == 0)
    {
        if (out_close_submenu)
        {
            *out_close_submenu = true;
        }
        return true;
    }

    if (strcmp(item_text, "Next >") == 0)
    {
        switch (s_plan_page)
        {
        case AREX_PLAN_PAGE_DEPTH:
            s_plan_page = AREX_PLAN_PAGE_TIME;
            break;
        case AREX_PLAN_PAGE_TIME:
            s_plan_page = AREX_PLAN_PAGE_RMV;
            break;
        case AREX_PLAN_PAGE_RMV:
            s_plan_page = AREX_PLAN_PAGE_READY;
            break;
        case AREX_PLAN_PAGE_RESULT:
        case AREX_PLAN_PAGE_ERROR:
            s_plan_page = AREX_PLAN_PAGE_READY;
            break;
        default:
            break;
        }
        if (out_keep_index) *out_keep_index = 1U;
        return true;
    }

    if (strcmp(item_text, "More >") == 0)
    {
        uint8_t total_pages = plan_result_total_pages();
        if (s_plan_result_page + 1U < total_pages)
        {
            s_plan_result_page++;
        }
        if (out_keep_index) *out_keep_index = 1U;
        return true;
    }

    if (strcmp(item_text, "Plan >") == 0)
    {
        plan_ensure_defaults();
        s_plan_result_page = 0U;
#ifdef PC_SIMULATOR
        memset(&s_plan_result, 0, sizeof(s_plan_result));
        if (buhlmann_debug_plan_calculate(s_plan_depth_m,
                                          s_plan_time_min,
                                          s_plan_rmv_lpm,
                                          &s_plan_result))
        {
            s_plan_page = AREX_PLAN_PAGE_RESULT;
        }
        else
        {
            s_plan_page = AREX_PLAN_PAGE_ERROR;
        }
#else
        s_plan_page = AREX_PLAN_PAGE_ERROR;
#endif
        if (out_keep_index) *out_keep_index = 1U;
        return true;
    }

    return false;
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
