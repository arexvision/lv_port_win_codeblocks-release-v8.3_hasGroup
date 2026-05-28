/*
 * 文件: src/app_ui/ui/views/submenu_model.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "submenu_model.h"

#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../core/callbacks.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "../core/vm/ui_vm_info.h"
#include "../core/vm/ui_vm_menu.h"
#include "../core/vm/ui_vm_dashboard_types.h"
#include "../core/vm/ui_vm_info_types.h"
#include "../core/vm/ui_vm_menu_types.h"
#include "submenu_dive_plan_state.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

static const char *s_info_titles[SUBMENU_INFO_COUNT] =
{
    "LAST DIVE", "DIVE PLAN", "TISSUE & TOX", "GAS & CALC", "SENSOR & DEVICE"
};

/* 这些静态缓存用于承接 VM 动态文本，保证返回给 view 层的是稳定字符串指针。 */
static char s_info_str[SUBMENU_INFO_COUNT][6][32];
static const char *s_info_dyn[SUBMENU_INFO_COUNT][7];
static const char *s_plan_dyn[16];
static char s_gas_switch_str[GAS_COUNT][20];
static const char *s_gas_switch_dyn[GAS_COUNT + 1];
static const char *s_brightness_dyn[BRIGHTNESS_COUNT + 1];
static const char *s_conservatism_dyn[CONSERVATISM_COUNT + 1];

static const setting_option_t s_conservatism_options[CONSERVATISM_COUNT] =
{
    { CONSERVATISM_LOW,    "LOW (GF 40/95)",    "LOW" },
    { CONSERVATISM_MED,    "MED (GF 40/85)",    "MED" },
    { CONSERVATISM_HIGH,   "HIGH (GF 30/70)",   "HIGH" },
    { CONSERVATISM_CUSTOM, "CUSTOM (GF 50/70)", "CUSTOM" },
};

static const brightness_option_t s_brightness_options[BRIGHTNESS_COUNT] =
{
    { BRIGHTNESS_LOW,  "LOW",  "LOW",  190 },
    { BRIGHTNESS_MED,  "MED",  "MED",  212 },
    { BRIGHTNESS_HIGH, "HIGH", "HIGH", 232 },
    { BRIGHTNESS_MAX,  "MAX",  "MAX",  255 },
};

static const char *s_setup_sub[SUBMENU_SETUP_COUNT][7] =
{
    { NULL },
    { NULL },
    { NULL },
    { "AUTO CAL: AUTO", "RESET AUTO CAL", NULL },
    { "LIGHT ON/OFF", "RED COLOR", "GREEN COLOR", "BLUE COLOR", "WHITE COLOR", NULL },
    { "VERSION: " SYSTEM_VERSION, "MODE SETUP", "DIVE SETUP", "AI SETUP", "ALERTS SETUP", "DISPLAY" },
};

static const char *s_setup_titles[SUBMENU_SETUP_COUNT] =
{
    "GAS SWITCH", "CONSERVATISM", "BRIGHTNESS", "COMPASS CAL", "LIGHT CONTROL", "SYSTEMS SETUP"
};

static char s_menu_vm_str[8][40];
static const char *s_menu_vm_dyn[9];

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

static const char *s_nested_ai_setup[4];
static const char *s_nested_alerts_setup[4];
static const char *s_nested_display_sys[6];
static const char *s_nested_datetime[6];
static const char *s_nested_nitrox[3];
static const char *s_nested_three_gas[6];
static const char *s_nested_oc_tech[8];
static char s_oc_tech_edit_str[4][28];
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

typedef struct
{
    char name[16];
    uint8_t o2_pct;
    uint8_t he_pct;
    float mod_m;
    uint8_t valid;
} submenu_gas_profile_slot_t;

enum
{
    DATETIME_FIELD_YEAR = 0,
    DATETIME_FIELD_MONTH,
    DATETIME_FIELD_DAY,
    DATETIME_FIELD_HOUR,
    DATETIME_FIELD_MINUTE,
};

static uint8_t count_items(const char **items, uint8_t max_count)
{
    /* 统计以 NULL 结尾的字符串数组长度。 */
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
    /* 去掉标题里的装饰前缀，便于后续做关键字匹配。 */
    if (title && title[0] == '>' && title[1] == ' ')
    {
        return title + 2;
    }
    return title;
}

static void normalize_menu_key(const char *text, char *out, uint8_t out_size)
{
    /* 把菜单文本规整成可比较的 key，避免因为空格和箭头符号匹配失败。 */
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
    /* 根据当前 PPO2 设定和氧浓度计算 MOD。 */
    ui_vm_edit_spec_t vm_edit;

    if (o2_pct == 0U)
    {
        return 0.0f;
    }

    ui_vm_edit_mod_ppo2_update(&vm_edit);
    return ((vm_edit.value * 100.0f) / (float)o2_pct - 1.0f) * 10.0f;
}

static void format_gas_name(char *out, size_t out_size, uint8_t o2_pct, uint8_t he_pct)
{
    /* 统一气体名称格式，避免各个菜单分支各自拼字符串。 */
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

static void plan_build_action_items(uint8_t *out_count)
{
    /* 潜水计划页底部动作行会随当前页面状态变化。 */
    uint8_t n = 0;
    submenu_dive_plan_snapshot_t snapshot;

    submenu_dive_plan_get_snapshot(&snapshot);
    s_plan_dyn[n++] = "Exit";
    if (snapshot.page == DIVE_PLAN_PAGE_READY)
    {
        s_plan_dyn[n++] = "Plan >";
    }
    else if (snapshot.page == DIVE_PLAN_PAGE_RESULT)
    {
        if (snapshot.result_page_index + 1U < snapshot.result_total_pages)
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
    s_plan_dyn[n] = NULL;
    *out_count = n;
}

static const char **copy_simple_menu_items(const ui_vm_simple_menu_t *vm, uint8_t *out_count)
{
    uint8_t count = 0U;

    if (vm == NULL)
    {
        if (out_count != NULL)
        {
            *out_count = 0U;
        }
        return NULL;
    }

    count = vm->count;
    if (count > 8U)
    {
        count = 8U;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        lv_snprintf(s_menu_vm_str[i], sizeof(s_menu_vm_str[i]), "%s", vm->items[i]);
        s_menu_vm_dyn[i] = s_menu_vm_str[i];
    }
    s_menu_vm_dyn[count] = NULL;

    if (out_count != NULL)
    {
        *out_count = count;
    }
    return s_menu_vm_dyn;
}

static const char **copy_menu_lines(const char items[][32], uint8_t count, uint8_t *out_count)
{
    uint8_t copy_count = count;

    if (copy_count > 8U)
    {
        copy_count = 8U;
    }

    for (uint8_t i = 0U; i < copy_count; i++)
    {
        lv_snprintf(s_menu_vm_str[i], sizeof(s_menu_vm_str[i]), "%s", items[i]);
        s_menu_vm_dyn[i] = s_menu_vm_str[i];
    }
    s_menu_vm_dyn[copy_count] = NULL;

    if (out_count != NULL)
    {
        *out_count = copy_count;
    }

    return s_menu_vm_dyn;
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

static void submenu_gas_profile_reset(submenu_gas_profile_slot_t *slots, uint8_t slot_count)
{
    if (slots == NULL)
    {
        return;
    }

    for (uint8_t i = 0U; i < slot_count; i++)
    {
        (void)memset(&slots[i], 0, sizeof(slots[i]));
    }
}

static void submenu_gas_profile_set(submenu_gas_profile_slot_t *slots,
                                    uint8_t slot_count,
                                    uint8_t index,
                                    uint8_t o2_pct,
                                    uint8_t he_pct)
{
    if ((slots == NULL) || (index >= slot_count))
    {
        return;
    }

    slots[index].o2_pct = o2_pct;
    slots[index].he_pct = he_pct;
    slots[index].mod_m = gas_mod_for_o2(o2_pct);
    slots[index].valid = (o2_pct > 0U) ? 1U : 0U;
    format_gas_name(slots[index].name, sizeof(slots[index].name), o2_pct, he_pct);
}

static void submenu_commit_gas_profile(const submenu_gas_profile_slot_t *slots, uint8_t active_count)
{
    uint8_t commit_count = active_count;

    if (commit_count > GAS_COUNT)
    {
        commit_count = GAS_COUNT;
    }

    for (uint8_t i = 0U; i < GAS_COUNT; i++)
    {
        if ((slots != NULL) && (i < commit_count) && (slots[i].valid != 0U))
        {
            bus_set_gas_slot(i,
                             slots[i].name,
                             slots[i].o2_pct,
                             slots[i].he_pct,
                             slots[i].mod_m);
        }
        else
        {
            bus_set_gas_slot(i, "", 0U, 0U, 0.0f);
        }
    }

    bus_set_gas_slot_count(commit_count);
    if ((slots != NULL) && (commit_count > 0U) && (slots[0].valid != 0U))
    {
        bus_set_gas(0, slots[0].name);
        bus_set_gas_mix(slots[0].o2_pct, slots[0].he_pct);
        bus_set_fio2((float)slots[0].o2_pct);
    }
    else
    {
        bus_set_gas(0, "--");
        bus_set_gas_mix(0U, 0U);
        bus_set_fio2(0.0f);
    }
}

static void apply_air_mode_gases(void)
{
    submenu_gas_profile_slot_t slots[GAS_COUNT];

    submenu_gas_profile_reset(slots, GAS_COUNT);
    submenu_gas_profile_set(slots, GAS_COUNT, 0U, 21U, 0U);
    submenu_commit_gas_profile(slots, 1U);
}

static void apply_nitrox_mode_gases(void)
{
    submenu_gas_profile_slot_t slots[GAS_COUNT];

    submenu_gas_profile_reset(slots, GAS_COUNT);
    submenu_gas_profile_set(slots, GAS_COUNT, 0U, s_nitrox_o2_pct, 0U);
    submenu_commit_gas_profile(slots, 1U);
}

static void apply_three_gas_mode_gases(void)
{
    submenu_gas_profile_slot_t slots[GAS_COUNT];
    uint8_t gas_count = s_three_gas_count;

    if (gas_count == 0U || gas_count > 3U)
    {
        gas_count = 3U;
    }

    submenu_gas_profile_reset(slots, GAS_COUNT);
    for (uint8_t i = 0U; i < gas_count; i++)
    {
        submenu_gas_profile_set(slots, GAS_COUNT, i, s_three_gas_o2_pct[i], 0U);
    }
    submenu_commit_gas_profile(slots, gas_count);
}

static void apply_oc_tech_mode_gases(void)
{
    submenu_gas_profile_slot_t slots[GAS_COUNT];
    uint8_t active_count = 0;

    submenu_gas_profile_reset(slots, GAS_COUNT);

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

        submenu_gas_profile_set(slots, GAS_COUNT, active_count, o2, he);
        active_count++;
    }

    submenu_commit_gas_profile(slots, active_count);
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

static void submenu_commit_dive_mode(uint16_t value)
{
    s_dive_mode = (value > 3U) ? 0U : (uint8_t)value;
    apply_dive_mode_gases(s_dive_mode);
}

static void submenu_commit_datetime_field(uint8_t field, uint16_t value)
{
    switch (field)
    {
    case DATETIME_FIELD_YEAR:
        s_datetime_year = (value < 2000U || value > 2099U) ? 2026U : value;
        break;
    case DATETIME_FIELD_MONTH:
        s_datetime_month = (value < 1U || value > 12U) ? 1U : (uint8_t)value;
        break;
    case DATETIME_FIELD_DAY:
        s_datetime_day = (value < 1U || value > 31U) ? 1U : (uint8_t)value;
        break;
    case DATETIME_FIELD_HOUR:
        s_datetime_hour = (value > 23U) ? 0U : (uint8_t)value;
        break;
    case DATETIME_FIELD_MINUTE:
        s_datetime_minute = (value > 59U) ? 0U : (uint8_t)value;
        break;
    default:
        break;
    }
}

static void submenu_commit_setting_value(submenu_setting_kind_t kind, uint8_t arg, uint16_t value)
{
    switch (kind)
    {
    case SUBMENU_SETTING_DIVE_MODE:
        submenu_commit_dive_mode(value);
        break;
    case SUBMENU_SETTING_3GAS_COUNT:
        s_three_gas_count = (value < 1 || value > 3) ? 3 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_OC_TECH_SAVE:
        save_oc_tech_slot(arg);
        break;
    case SUBMENU_SETTING_SALINITY:
        s_salinity_mode = (value > 2) ? 0 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_SAFETY_STOP:
        s_safety_stop_mode = (value > 3) ? 1 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_LAST_DECO:
        s_last_deco_mode = (value > 1) ? 0 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_ALTITUDE:
        s_altitude_level = (value > 3) ? 0 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_AI_TANK_STATE:
        if (arg < 2U)
        {
            s_ai_tank_state[arg] = (value > 2) ? 0 : (uint8_t)value;
        }
        break;
    case SUBMENU_SETTING_GTR_MODE:
        s_gtr_enabled = value ? 1 : 0;
        break;
    case SUBMENU_SETTING_DEPTH_ALARM:
        s_depth_alarm_m = value;
        break;
    case SUBMENU_SETTING_TIME_ALARM:
        s_time_alarm_min = value;
        break;
    case SUBMENU_SETTING_UNITS:
        s_units_mode = (value > 1) ? 0 : (uint8_t)value;
        break;
    case SUBMENU_SETTING_DATETIME_FIELD:
        submenu_commit_datetime_field(arg, value);
        break;
    case SUBMENU_SETTING_LOG_RATE:
        s_log_rate_s = (uint8_t)value;
        break;
    case SUBMENU_SETTING_BLUETOOTH:
        s_bluetooth_enabled = value ? 1 : 0;
        break;
    case SUBMENU_SETTING_RESET_DEFAULTS:
        s_units_mode = 0;
        s_log_rate_s = 10;
        s_bluetooth_enabled = 0;
        break;
    default:
        break;
    }
}

static void submenu_commit_edit_value(submenu_setting_kind_t kind, uint8_t arg, float value)
{
    switch (kind)
    {
    case SUBMENU_SETTING_PLAN_DEPTH:
        submenu_dive_plan_set_depth_m(value);
        break;
    case SUBMENU_SETTING_PLAN_TIME:
        submenu_dive_plan_set_time_min(value);
        break;
    case SUBMENU_SETTING_PLAN_RMV:
        submenu_dive_plan_set_rmv_lpm(value);
        break;
    case SUBMENU_SETTING_MOD_PPO2:
        ui_on_mod_ppo2_set(value);
        break;
    case SUBMENU_SETTING_NITROX_O2:
        s_nitrox_o2_pct = (uint8_t)(value + 0.5f);
        break;
    case SUBMENU_SETTING_3GAS_O2:
        if (arg < 3U)
        {
            s_three_gas_o2_pct[arg] = (uint8_t)(value + 0.5f);
        }
        break;
    case SUBMENU_SETTING_OC_TECH_GAS:
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
    case SUBMENU_SETTING_DEPTH_ALARM:
        s_depth_alarm_m = (uint16_t)(value + 0.5f);
        break;
    case SUBMENU_SETTING_TIME_ALARM:
        s_time_alarm_min = (uint16_t)(value + 0.5f);
        break;
    case SUBMENU_SETTING_DATETIME_FIELD:
        submenu_commit_datetime_field(arg, (uint16_t)(value + 0.5f));
        break;
    default:
        break;
    }
}

static const char *compass_cal_status_text(void)
{
    compass_cal_ui_state_t st = get_compass_calibration_ui_state();
    if (st == COMPASS_CAL_RUNNING) return "LEARN";
    if (st == COMPASS_CAL_READY) return "OK";
    return "AUTO";
}

uint8_t submenu_safety_stop_depth_m(uint8_t value)
{
    /* 安全停留深度只允许在两个固定档位之间切换。 */
    return value == 1 ? 6 : 3;
}

const char *submenu_info_title(uint8_t index)
{
    /* INFO 标题按索引直接读取。 */
    if (index >= SUBMENU_INFO_COUNT)
    {
        return NULL;
    }
    return s_info_titles[index];
}

const char **submenu_build_info_items(uint8_t index, uint8_t *out_count)
{
    /* INFO 页条目由 VM 文本数组生成，这里只负责打包成可遍历列表。 */
    ui_vm_info_page_t vm;

    if (out_count)
    {
        *out_count = 0;
    }
    if (index >= SUBMENU_INFO_COUNT)
    {
        return NULL;
    }

    uint8_t n = 0;
    switch (index)
    {
    case 0:
    {
        ui_vm_info_page_update(&vm, index);
        for (uint8_t i = 0U; i < vm.count; i++)
        {
            snprintf(s_info_str[0][i], sizeof(s_info_str[0][i]), "%s", vm.lines[i]);
            s_info_dyn[0][n++] = s_info_str[0][i];
        }
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
        ui_vm_info_page_update(&vm, index);
        for (uint8_t i = 0U; i < vm.count; i++)
        {
            snprintf(s_info_str[2][i], sizeof(s_info_str[2][i]), "%s", vm.lines[i]);
            s_info_dyn[2][n++] = s_info_str[2][i];
        }
        break;
    }
    case 3:
    {
        ui_vm_info_page_update(&vm, index);
        for (uint8_t i = 0U; i < vm.count; i++)
        {
            snprintf(s_info_str[3][i], sizeof(s_info_str[3][i]), "%s", vm.lines[i]);
            s_info_dyn[3][n++] = s_info_str[3][i];
        }
        break;
    }
    case 4:
    {
        ui_vm_info_page_update(&vm, index);
        for (uint8_t i = 0U; i < vm.count; i++)
        {
            snprintf(s_info_str[4][i], sizeof(s_info_str[4][i]), "%s", vm.lines[i]);
            s_info_dyn[4][n++] = s_info_str[4][i];
        }
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

const char *submenu_setup_title(uint8_t index)
{
    /* SETUP 标题同样按固定索引读取。 */
    if (index >= SUBMENU_SETUP_COUNT)
    {
        return NULL;
    }
    return s_setup_titles[index];
}

const setting_option_t *submenu_conservatism_option(uint8_t index)
{
    /* 保守度选项表是静态枚举。 */
    return &s_conservatism_options[index];
}

const char *submenu_conservatism_badge(uint8_t level)
{
    return s_conservatism_options[level].badge_label;
}

const brightness_option_t *submenu_brightness_option(uint8_t index)
{
    /* 亮度选项表也是静态枚举。 */
    return &s_brightness_options[index];
}

const char *submenu_brightness_badge(uint8_t level)
{
    return s_brightness_options[level].badge_label;
}

uint8_t submenu_brightness_visible_opa(uint8_t level)
{
    return s_brightness_options[level].visible_opa;
}

int8_t submenu_setup_index_for_title(const char *title)
{
    const char *clean_title = strip_title_prefix(title);
    if (!clean_title)
    {
        return -1;
    }

    for (uint8_t i = 0; i < SUBMENU_SETUP_COUNT; i++)
    {
        if (strcmp(clean_title, s_setup_titles[i]) == 0)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

const char **submenu_build_compass_cal_items(uint8_t *out_count)
{
    /* 罗盘校准菜单只显示当前状态和复位入口。 */
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
    ui_vm_simple_menu_t vm;

    ui_vm_systems_setup_menu_update(&vm, s_dive_mode);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_gas_switch_items(uint8_t *out_count)
{
    ui_vm_gas_switch_menu_t vm;

    ui_vm_gas_switch_menu_update(&vm);

    for (uint8_t i = 0U; i < vm.count; i++)
    {
        lv_snprintf(s_gas_switch_str[i], sizeof(s_gas_switch_str[i]), "%s", vm.items[i]);
        s_gas_switch_dyn[i] = s_gas_switch_str[i];
    }
    s_gas_switch_dyn[vm.count] = NULL;
    if (out_count)
    {
        *out_count = vm.count;
    }
    return s_gas_switch_dyn;
}

static const char **build_nested_nitrox(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_nitrox_menu_update(&vm, s_nitrox_o2_pct);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_nested_three_gas(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_three_gas_menu_update(&vm, s_three_gas_o2_pct, s_three_gas_count);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_nested_oc_tech(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_oc_tech_menu_update(&vm, s_oc_tech_o2_pct, s_oc_tech_he_pct);
    return copy_simple_menu_items(&vm, out_count);
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

const char **submenu_build_setup_items(uint8_t index, uint8_t *out_count)
{
    if (out_count)
    {
        *out_count = 0;
    }
    if (index >= SUBMENU_SETUP_COUNT)
    {
        return NULL;
    }
    if (strcmp(s_setup_titles[index], "GAS SWITCH") == 0)
    {
        return build_gas_switch_items(out_count);
    }
    if (strcmp(s_setup_titles[index], "CONSERVATISM") == 0)
    {
        for (uint8_t i = 0; i < CONSERVATISM_COUNT; i++)
        {
            s_conservatism_dyn[i] = s_conservatism_options[i].menu_label;
        }
        s_conservatism_dyn[CONSERVATISM_COUNT] = NULL;
        if (out_count)
        {
            *out_count = CONSERVATISM_COUNT;
        }
        return s_conservatism_dyn;
    }
    if (strcmp(s_setup_titles[index], "COMPASS CAL") == 0)
    {
        return submenu_build_compass_cal_items(out_count);
    }
    if (strcmp(s_setup_titles[index], "BRIGHTNESS") == 0)
    {
        for (uint8_t i = 0; i < BRIGHTNESS_COUNT; i++)
        {
            s_brightness_dyn[i] = s_brightness_options[i].menu_label;
        }
        s_brightness_dyn[BRIGHTNESS_COUNT] = NULL;
        if (out_count)
        {
            *out_count = BRIGHTNESS_COUNT;
        }
        return s_brightness_dyn;
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
    ui_vm_dive_context_t ctx_vm;
    ui_vm_dive_setup_menu_t vm;

    ui_vm_dive_context_update(&ctx_vm);
    ui_vm_dive_setup_menu_update(&vm,
                                 ctx_vm.salinity_mode,
                                 s_safety_stop_mode,
                                 s_altitude_level);
    return copy_menu_lines(vm.items, vm.count, out_count);
}

static const char **build_nested_ai_setup(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_ai_menu_update(&vm, s_ai_tank_state, s_gtr_enabled);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_nested_alerts_setup(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_alerts_menu_update(&vm, s_depth_alarm_m, s_time_alarm_min, s_ndl_alarm_min);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_nested_display_sys(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_display_menu_update(&vm, s_units_mode, s_log_rate_s, s_bluetooth_enabled);
    return copy_simple_menu_items(&vm, out_count);
}

static const char **build_nested_datetime(uint8_t *out_count)
{
    ui_vm_simple_menu_t vm;

    ui_vm_datetime_menu_update(&vm,
                               s_datetime_year,
                               s_datetime_month,
                               s_datetime_day,
                               s_datetime_hour,
                               s_datetime_minute);
    return copy_simple_menu_items(&vm, out_count);
}

const char **submenu_nested_items_for(const char *title, uint8_t *out_count)
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

const char **submenu_child_items_for(const char *current_title,
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

    items = submenu_nested_items_for(key, &count);
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

bool submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         submenu_setting_confirm_t *out_setting)
{
    /* 通过当前标题和条目文本，推导出需要确认的设置动作。 */
    const char *clean_title = strip_title_prefix(current_title);
    if (!clean_title || !item_text || !out_setting)
    {
        return false;
    }

    memset(out_setting, 0, sizeof(*out_setting));

    if (strcmp(clean_title, "MODE SETUP") == 0 && item_index == 0)
    {
        out_setting->kind = SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nAIR");
        return true;
    }

    if (strcmp(clean_title, "NITROX") == 0 && item_index == 1)
    {
        out_setting->kind = SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 1;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nNITROX %u%%", (unsigned)s_nitrox_o2_pct);
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index == 4)
    {
        out_setting->kind = SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 2;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\n3 GAS / %u ACTIVE", (unsigned)s_three_gas_count);
        return true;
    }

    if (strcmp(clean_title, "OC Tech") == 0 && item_index == 5)
    {
        out_setting->kind = SUBMENU_SETTING_DIVE_MODE;
        out_setting->value = 3;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "DIVE MODE\nOC Tech ACTIVE");
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 4)
    {
        out_setting->kind = SUBMENU_SETTING_RESET_DEFAULTS;
        out_setting->value = 0;
        lv_snprintf(out_setting->body, sizeof(out_setting->body),
                    "RESET DEFAULTS\nDISPLAY SETUP");
        return true;
    }

    return false;
}

bool submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                submenu_setting_confirm_t *out_setting)
{
    /* 直接生效设置不需要确认弹窗。 */
    const char *clean_title = strip_title_prefix(current_title);
    (void)item_text;
    if (!clean_title || !out_setting)
    {
        return false;
    }

    memset(out_setting, 0, sizeof(*out_setting));

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 0)
    {
        ui_vm_dive_context_t vm;
        uint8_t current_salinity;

        ui_vm_dive_context_update(&vm);
        current_salinity = vm.salinity_mode;
        uint8_t next = (uint8_t)((current_salinity + 1U) % 3U);
        out_setting->kind = SUBMENU_SETTING_SALINITY;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 2)
    {
        uint8_t next = (uint8_t)((s_safety_stop_mode + 1) %
                       (sizeof(s_safety_stop_values) / sizeof(s_safety_stop_values[0])));
        out_setting->kind = SUBMENU_SETTING_SAFETY_STOP;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 3)
    {
        ui_vm_dive_context_t vm;
        uint8_t current_last_deco_mode;

        ui_vm_dive_context_update(&vm);
        current_last_deco_mode = (vm.last_stop_depth_m == 6U) ? 1U : 0U;
        uint8_t next = (uint8_t)((current_last_deco_mode + 1U) %
                                 (sizeof(s_last_deco_values) / sizeof(s_last_deco_values[0])));
        out_setting->kind = SUBMENU_SETTING_LAST_DECO;
        out_setting->value = next;
        return true;
    }

    if ((strcmp(clean_title, "DIVE SETUP") == 0 || strcmp(clean_title, "DIVE MENU") == 0) && item_index == 4)
    {
        uint8_t next = (uint8_t)((s_altitude_level + 1) % 4);
        out_setting->kind = SUBMENU_SETTING_ALTITUDE;
        out_setting->value = next;
        return true;
    }

    if (strcmp(clean_title, "AI SETUP") == 0 && item_index < 2)
    {
        uint8_t next = (uint8_t)((s_ai_tank_state[item_index] + 1) % 3);
        out_setting->kind = SUBMENU_SETTING_AI_TANK_STATE;
        out_setting->arg = item_index;
        out_setting->value = next;
        return true;
    }

    if (strcmp(clean_title, "AI SETUP") == 0 && item_index == 2)
    {
        out_setting->kind = SUBMENU_SETTING_GTR_MODE;
        out_setting->value = s_gtr_enabled ? 0 : 1;
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 0)
    {
        out_setting->kind = SUBMENU_SETTING_UNITS;
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
        out_setting->kind = SUBMENU_SETTING_LOG_RATE;
        out_setting->value = s_log_rate_values[next_index];
        return true;
    }

    if (strcmp(clean_title, "DISPLAY") == 0 && item_index == 3)
    {
        out_setting->kind = SUBMENU_SETTING_BLUETOOTH;
        out_setting->value = s_bluetooth_enabled ? 0 : 1;
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index == 3)
    {
        out_setting->kind = SUBMENU_SETTING_3GAS_COUNT;
        out_setting->value = (s_three_gas_count >= 3U) ? 1U : (uint16_t)(s_three_gas_count + 1U);
        return true;
    }

    if (oc_tech_slot_from_title(clean_title, &s_oc_tech_edit_slot) && item_index == 2)
    {
        out_setting->kind = SUBMENU_SETTING_OC_TECH_SAVE;
        out_setting->arg = s_oc_tech_edit_slot;
        return true;
    }

    return false;
}

bool submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           submenu_edit_spec_t *out_spec)
{
    /* 行内编辑项在这里生成范围、步长和默认值。 */
    const char *clean_title = strip_title_prefix(current_title);
    ui_vm_edit_spec_t vm_edit;
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
        ui_vm_edit_mod_ppo2_update(&vm_edit);
        out_spec->kind = SUBMENU_SETTING_MOD_PPO2;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (strcmp(clean_title, "NITROX") == 0 && item_index == 0)
    {
        ui_vm_edit_nitrox_o2_update(&vm_edit, s_nitrox_o2_pct);
        out_spec->kind = SUBMENU_SETTING_NITROX_O2;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (strcmp(clean_title, "3 GAS") == 0 && item_index < 3)
    {
        ui_vm_edit_three_gas_o2_update(&vm_edit, item_index, s_three_gas_o2_pct[item_index]);
        out_spec->kind = SUBMENU_SETTING_3GAS_O2;
        out_spec->arg = item_index;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (oc_tech_slot_from_title(clean_title, &s_oc_tech_edit_slot) && item_index < 2)
    {
        uint8_t slot = s_oc_tech_edit_slot;
        bool edit_he = (item_index == 1U);
        uint8_t o2 = s_oc_tech_draft_o2_pct[slot];
        uint8_t he = s_oc_tech_draft_he_pct[slot];

        ui_vm_edit_oc_tech_gas_update(&vm_edit, slot, item_index, o2, he);
        out_spec->kind = SUBMENU_SETTING_OC_TECH_GAS;
        out_spec->arg = (uint8_t)(slot * 2U + item_index);
        (void)edit_he;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (strcmp(clean_title, "ALERTS SETUP") == 0 && item_index == 0)
    {
        ui_vm_edit_depth_alarm_update(&vm_edit, s_depth_alarm_m);
        out_spec->kind = SUBMENU_SETTING_DEPTH_ALARM;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (strcmp(clean_title, "ALERTS SETUP") == 0 && item_index == 1)
    {
        ui_vm_edit_time_alarm_update(&vm_edit, s_time_alarm_min);
        out_spec->kind = SUBMENU_SETTING_TIME_ALARM;
        out_spec->value = vm_edit.value;
        out_spec->min = vm_edit.min;
        out_spec->max = vm_edit.max;
        out_spec->step = vm_edit.step;
        out_spec->decimals = vm_edit.decimals;
        lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
        return true;
    }

    if (strcmp(clean_title, "DATE & CLOCK") == 0)
    {
        out_spec->kind = SUBMENU_SETTING_DATETIME_FIELD;
        out_spec->decimals = 0;
        out_spec->step = 1.0f;

        switch (item_index)
        {
        case 0:
            ui_vm_edit_datetime_update(&vm_edit, 0U, s_datetime_year);
            out_spec->arg = DATETIME_FIELD_YEAR;
            out_spec->value = vm_edit.value;
            out_spec->min = vm_edit.min;
            out_spec->max = vm_edit.max;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
            return true;
        case 1:
            ui_vm_edit_datetime_update(&vm_edit, 1U, s_datetime_month);
            out_spec->arg = DATETIME_FIELD_MONTH;
            out_spec->value = vm_edit.value;
            out_spec->min = vm_edit.min;
            out_spec->max = vm_edit.max;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
            return true;
        case 2:
            ui_vm_edit_datetime_update(&vm_edit, 2U, s_datetime_day);
            out_spec->arg = DATETIME_FIELD_DAY;
            out_spec->value = vm_edit.value;
            out_spec->min = vm_edit.min;
            out_spec->max = vm_edit.max;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
            return true;
        case 3:
            ui_vm_edit_datetime_update(&vm_edit, 3U, s_datetime_hour);
            out_spec->arg = DATETIME_FIELD_HOUR;
            out_spec->value = vm_edit.value;
            out_spec->min = vm_edit.min;
            out_spec->max = vm_edit.max;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
            return true;
        case 4:
            ui_vm_edit_datetime_update(&vm_edit, 4U, s_datetime_minute);
            out_spec->arg = DATETIME_FIELD_MINUTE;
            out_spec->value = vm_edit.value;
            out_spec->min = vm_edit.min;
            out_spec->max = vm_edit.max;
            lv_snprintf(out_spec->label, sizeof(out_spec->label), "%s", vm_edit.label);
            return true;
        default:
            break;
        }
    }

    return false;
}

static const char *submenu_title_for_menu_id(menu_id_t menu_id)
{
    switch (menu_id)
    {
    case MENU_MODE_SETUP:   return "MODE SETUP";
    case MENU_NITROX:       return "NITROX";
    case MENU_THREE_GAS:    return "3 GAS";
    case MENU_OC_TECH:      return "OC Tech";
    case MENU_DIVE_SETUP:   return "DIVE SETUP";
    case MENU_AI_SETUP:     return "AI SETUP";
    case MENU_ALERTS_SETUP: return "ALERTS SETUP";
    case MENU_DISPLAY:      return "DISPLAY";
    case MENU_DATE_CLOCK:   return "DATE & CLOCK";
    default:                return menu_defs_title(menu_id);
    }
}

static int8_t oc_tech_slot_from_item_id(menu_item_id_t item_id)
{
    if (item_id < MENU_ITEM_OC_TECH_SLOT_0 || item_id > MENU_ITEM_OC_TECH_SLOT_4)
    {
        return -1;
    }
    return (int8_t)(item_id - MENU_ITEM_OC_TECH_SLOT_0);
}

static int8_t date_field_from_item_id(menu_item_id_t item_id)
{
    switch (item_id)
    {
    case MENU_ITEM_DATE_YEAR:   return 0;
    case MENU_ITEM_DATE_MONTH:  return 1;
    case MENU_ITEM_DATE_DAY:    return 2;
    case MENU_ITEM_DATE_HOUR:   return 3;
    case MENU_ITEM_DATE_MINUTE: return 4;
    default:                    return -1;
    }
}

bool submenu_setting_from_ids(menu_id_t current_menu,
                              menu_item_id_t item_id,
                              submenu_setting_confirm_t *out_setting)
{
    /* 通过稳定 ID 做设置映射，避免依赖字符串文本。 */
    const char *title = submenu_title_for_menu_id(current_menu);
    uint8_t item_index = 0U;
    const char *item_text = "";

    switch (item_id)
    {
    case MENU_ITEM_MODE_AIR:
        item_index = 0U;
        item_text = "AIR";
        break;
    case MENU_ITEM_NITROX_CONFIRM:
        item_index = 1U;
        item_text = "CONFIRM";
        break;
    case MENU_ITEM_THREE_GAS_CONFIRM:
        item_index = 4U;
        item_text = "CONFIRM";
        break;
    case MENU_ITEM_OC_TECH_CONFIRM:
        item_index = 5U;
        item_text = "CONFIRM & ACTIVATE";
        break;
    case MENU_ITEM_DISPLAY_RESET:
        item_index = 4U;
        item_text = "RESET DEFAULTS";
        break;
    default:
        return false;
    }

    return submenu_setting_from_selection(title, item_index, item_text, out_setting);
}

bool submenu_direct_setting_from_ids(menu_id_t current_menu,
                                     menu_item_id_t item_id,
                                     submenu_setting_confirm_t *out_setting)
{
    /* 直接生效项的 ID 映射入口。 */
    const char *title = submenu_title_for_menu_id(current_menu);
    uint8_t item_index = 0U;

    switch (item_id)
    {
    case MENU_ITEM_DIVE_SALINITY:    item_index = 0U; break;
    case MENU_ITEM_DIVE_SAFETY_STOP: item_index = 2U; break;
    case MENU_ITEM_DIVE_LAST_DECO:   item_index = 3U; break;
    case MENU_ITEM_DIVE_ALTITUDE:    item_index = 4U; break;
    case MENU_ITEM_AI_TANK_0:        item_index = 0U; break;
    case MENU_ITEM_AI_TANK_1:        item_index = 1U; break;
    case MENU_ITEM_AI_GTR:           item_index = 2U; break;
    case MENU_ITEM_DISPLAY_UNITS:    item_index = 0U; break;
    case MENU_ITEM_DISPLAY_LOG_RATE: item_index = 2U; break;
    case MENU_ITEM_DISPLAY_BLUETOOTH:item_index = 3U; break;
    case MENU_ITEM_THREE_GAS_COUNT:  item_index = 3U; break;
    case MENU_ITEM_OC_TECH_EDIT_SAVE:item_index = 2U; break;
    default:
        return false;
    }

    return submenu_direct_setting_from_selection(title, item_index, NULL, out_setting);
}

bool submenu_edit_spec_from_ids(menu_id_t current_menu,
                                menu_item_id_t item_id,
                                submenu_edit_spec_t *out_spec)
{
    /* 通过 menu_id + item_id 生成编辑规格。 */
    const char *title = submenu_title_for_menu_id(current_menu);
    uint8_t item_index = 0U;

    switch (item_id)
    {
    case MENU_ITEM_DIVE_MOD_PPO2:
        item_index = 1U;
        break;
    case MENU_ITEM_NITROX_O2:
        item_index = 0U;
        break;
    case MENU_ITEM_THREE_GAS_O2_0:
        item_index = 0U;
        break;
    case MENU_ITEM_THREE_GAS_O2_1:
        item_index = 1U;
        break;
    case MENU_ITEM_THREE_GAS_O2_2:
        item_index = 2U;
        break;
    case MENU_ITEM_OC_TECH_EDIT_O2:
        item_index = 0U;
        break;
    case MENU_ITEM_OC_TECH_EDIT_HE:
        item_index = 1U;
        break;
    case MENU_ITEM_ALERT_DEPTH:
        item_index = 0U;
        break;
    case MENU_ITEM_ALERT_TIME:
        item_index = 1U;
        break;
    default:
    {
        int8_t field = date_field_from_item_id(item_id);
        if (field < 0)
        {
            return false;
        }
        item_index = (uint8_t)field;
        break;
    }
    }

    return submenu_edit_spec_from_selection(title, item_index, NULL, out_spec);
}

void submenu_prepare_oc_tech_child(menu_item_id_t item_id,
                                   char *out_title,
                                   uint8_t out_title_size)
{
    /* OC Tech 的子菜单标题需要带槽位编号。 */
    int8_t slot = oc_tech_slot_from_item_id(item_id);

    if (out_title != NULL && out_title_size > 0U)
    {
        out_title[0] = '\0';
    }
    if (slot < 0)
    {
        return;
    }

    begin_oc_tech_slot_edit((uint8_t)slot);
    if (out_title != NULL && out_title_size > 0U)
    {
        lv_snprintf(out_title, out_title_size, "G%u TRIMIX", (unsigned)(slot + 1U));
    }
}

void submenu_apply_setting(submenu_setting_kind_t kind, uint8_t arg, uint16_t value)
{
    /* 所有设置结果最终统一写回业务层。 */
    submenu_commit_setting_value(kind, arg, value);
}

void submenu_apply_edit_value(submenu_setting_kind_t kind, uint8_t arg, float value)
{
    /* 编辑态提交的浮点值最终也走统一设置提交路径。 */
    submenu_commit_edit_value(kind, arg, value);
}

bool submenu_is_readonly_info_title(const char *title)
{
    /* 只读 INFO 页在菜单系统里不允许进入编辑。 */
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
