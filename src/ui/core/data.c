/*
 * 文件: src/app_ui/ui/core/data.c
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "data.h"
#include "callbacks.h"
#include "ui_settings.h"
#include "../../config/build/ui_build_flags.h"
#include "../../config/build/ui_debug_flags.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include "stdio.h"

#define LOGBOOK_SAMPLE_BUFFER_BYTES ((uint32_t)(sizeof(dive_pt_t) * MAX_DIVE_LOG))

#ifdef PC_SIMULATOR
#include <stdlib.h>
#include "lvgl.h"
#include "../../algo_sim/deco_core.h"
#include "rtthread.h"
#else
#if defined(__has_include)
#if __has_include("mem_section.h")
#include "mem_section.h"
#else
#define L2_RET_BSS_SECT_BEGIN(name)
#define L2_RET_BSS_SECT_END
#define L2_RET_BSS_SECT(name)
#endif
#else
#include "mem_section.h"
#endif
#endif

static dive_pt_t s_dive_log[MAX_DIVE_LOG] __attribute__((section(".psram_bss")));
static uint16_t s_dive_log_count;
static bool s_dive_log_sample_valid;
static float s_dive_log_last_sample_time_s;
static deco_stop_t s_deco_stops[MAX_DECO_STOPS];
static uint16_t s_deco_stop_count;
#ifdef PC_SIMULATOR
static logbook_entry_t s_logbook_entries[MAX_LOGBOOK_ENTRIES];
static dive_pt_t s_logbook_samples[MAX_LOGBOOK_ENTRIES][MAX_DIVE_LOG];
static uint16_t s_logbook_sample_counts[MAX_LOGBOOK_ENTRIES];
static uint16_t s_logbook_count;
#endif
static logbook_entry_t s_last_dive_snapshot;
static uint16_t s_last_dive_snapshot_source_count;

#ifndef PC_SIMULATOR
L2_RET_BSS_SECT_BEGIN(logbook_backend_heap)
static uint8_t s_logbook_backend_heap_pool[LOGBOOK_SAMPLE_BUFFER_BYTES + 448U] L2_RET_BSS_SECT(logbook_backend_heap);
L2_RET_BSS_SECT_END
static struct rt_memheap s_logbook_backend_heap;
static bool s_logbook_backend_heap_ready = false;
#endif

typedef enum
{
    LAYOUT_ARCHIVE_SIDE = 0,
    LAYOUT_ARCHIVE_TOP,
    LAYOUT_ARCHIVE_BOTTOM,
    LAYOUT_ARCHIVE_COUNT
} layout_archive_id_t;

static sys_config_t s_layout_archives[LAYOUT_ARCHIVE_COUNT] __attribute__((section(".psram_bss")));
static bool s_layout_archive_valid[LAYOUT_ARCHIVE_COUNT];
#if UI_DIRTY_THROTTLE_ENABLED
static dirty_mask_t s_dirty_throttle_bypass_once;
#endif

static layout_archive_id_t layout_archive_id_for(theme_t theme, order_t order)
{
    if (theme == THEME_CLASSIC)
    {
        return (order == ORDER_NORMAL) ? LAYOUT_ARCHIVE_TOP : LAYOUT_ARCHIVE_BOTTOM;
    }
    return LAYOUT_ARCHIVE_SIDE;
}

static void bus_mark_dirty(dirty_mask_t mask)
{
    if (mask == DIRTY_NONE)
    {
        return;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    g_sensor_data.dirty_mask |= mask;
    rt_hw_interrupt_enable(level);
}

#if UI_DIRTY_THROTTLE_ENABLED
static bool bus_dirty_due(uint32_t now_ms, uint32_t *last_ms, uint32_t interval_ms)
{
    if (last_ms == NULL)
    {
        return true;
    }

    if (*last_ms == 0U || (now_ms - *last_ms) >= interval_ms)
    {
        *last_ms = now_ms;
        return true;
    }

    return false;
}

static dirty_mask_t bus_throttle_dirty_mask(dirty_mask_t mask)
{
    static uint32_t s_sensor_last_ms = 0U;
    static uint32_t s_compass_last_ms = 0U;
    static uint32_t s_system_last_ms = 0U;
    static uint32_t s_gas_last_ms = 0U;
    const uint32_t now_ms = rt_tick_get_millisecond();
    dirty_mask_t deferred = DIRTY_NONE;
    dirty_mask_t bypass = DIRTY_NONE;

    rt_base_t level = rt_hw_interrupt_disable();
    bypass = s_dirty_throttle_bypass_once & mask;
    s_dirty_throttle_bypass_once &= ~bypass;
    rt_hw_interrupt_enable(level);

    mask &= ~bypass;

    /*
     * 这里只对高频显示域做合帧。强一致性的布局、告警、日志、减压/气体等
     * dirty 原样放行，避免用户操作或潜水关键状态被节流。
     */
    if ((mask & DIRTY_SENSOR) != 0U &&
        !bus_dirty_due(now_ms, &s_sensor_last_ms, UI_DIRTY_SENSOR_MIN_INTERVAL_MS))
    {
        deferred |= DIRTY_SENSOR;
        mask &= ~DIRTY_SENSOR;
    }

    if ((mask & DIRTY_COMPASS) != 0U &&
        !bus_dirty_due(now_ms, &s_compass_last_ms, UI_DIRTY_COMPASS_MIN_INTERVAL_MS))
    {
        deferred |= DIRTY_COMPASS;
        mask &= ~DIRTY_COMPASS;
    }

    if ((mask & DIRTY_SYSTEM) != 0U &&
        !bus_dirty_due(now_ms, &s_system_last_ms, UI_DIRTY_SYSTEM_MIN_INTERVAL_MS))
    {
        deferred |= DIRTY_SYSTEM;
        mask &= ~DIRTY_SYSTEM;
    }

    if ((mask & DIRTY_GAS_SUPPLY) != 0U &&
        !bus_dirty_due(now_ms, &s_gas_last_ms, UI_DIRTY_GAS_MIN_INTERVAL_MS))
    {
        deferred |= DIRTY_GAS_SUPPLY;
        mask &= ~DIRTY_GAS_SUPPLY;
    }

    if (deferred != DIRTY_NONE)
    {
        bus_mark_dirty(deferred);
    }

    return mask | bypass;
}
#endif

static void layout_copy_fields(sys_config_t *dst, const sys_config_t *src)
{
    if (dst == NULL || src == NULL) return;

    dst->safe_zone_w = src->safe_zone_w;
    dst->safe_zone_h = src->safe_zone_h;
    dst->offset_x = src->offset_x;
    dst->offset_y = src->offset_y;
    dst->theme_mode = src->theme_mode;
    dst->layout_order = src->layout_order;
    dst->dots_position = src->dots_position;
    dst->compass_style = src->compass_style;
    dst->flash_speed = src->flash_speed;
    dst->mask_enabled = src->mask_enabled;
    dst->split_outward = src->split_outward;
    dst->sep_style = src->sep_style;
    dst->sep_thick = src->sep_thick;
    dst->sep_alpha = src->sep_alpha;
    dst->h_depth = src->h_depth;
    dst->h_ndl = src->h_ndl;
    dst->h_pod = src->h_pod;
    dst->h_batt = src->h_batt;
    dst->h_gas = src->h_gas;
    dst->h_time = src->h_time;
    dst->gap_u = src->gap_u;
    dst->panel_gap_u = src->panel_gap_u;
    dst->title_h_u = src->title_h_u;
    dst->h_menu_item = src->h_menu_item;
    dst->gap_menu = src->gap_menu;
    dst->h_tissues_chart = src->h_tissues_chart;
    dst->left_widget_count = src->left_widget_count;
    (void)memcpy(dst->left_widgets, src->left_widgets, sizeof(dst->left_widgets));
    dst->custom_card_count = src->custom_card_count;
    (void)memcpy(dst->custom_cards, src->custom_cards, sizeof(dst->custom_cards));
    (void)memcpy(dst->custom_card_slot, src->custom_card_slot, sizeof(dst->custom_card_slot));
    (void)memcpy(dst->card_order, src->card_order, sizeof(dst->card_order));
}

static void layout_archive_save_current(void)
{
    layout_archive_id_t id = layout_archive_id_for(ui_theme_mode_get(), ui_layout_order_get());
    layout_copy_fields(&s_layout_archives[id], &g_sys_config);
    s_layout_archive_valid[id] = true;
}

static void layout_set_default_card_order(sys_config_t *cfg)
{
    (void)memset(cfg->card_order, PAGE_ID_UNUSED, sizeof(cfg->card_order));
    cfg->card_order[PAGE_POS_INFO] = PAGE_ID_INFO;
    cfg->card_order[PAGE_POS_1] = PAGE_ID_BLANK;
    cfg->card_order[PAGE_POS_2] = PAGE_ID_COMPASS;
    cfg->card_order[PAGE_POS_3] = PAGE_ID_DECO;
    cfg->card_order[PAGE_POS_4] = PAGE_ID_PLAN;
    cfg->card_order[PAGE_POS_5] = PAGE_ID_GAS;
    cfg->card_order[PAGE_POS_6] = PAGE_ID_CUSTOM_GRID;
    cfg->card_order[PAGE_POS_7] = PAGE_ID_CUSTOM_GRID;
    cfg->card_order[PAGE_POS_SETUP] = PAGE_ID_SETUP;
    (void)memset(cfg->custom_card_slot, 0xFF, sizeof(cfg->custom_card_slot));
    cfg->custom_card_slot[PAGE_POS_6] = 0U;
    cfg->custom_card_slot[PAGE_POS_7] = 1U;
}

static void layout_set_default_fixed_widgets(sys_config_t *cfg, bool horizontal)
{
    static const grid_widget_t side_widgets[] =
    {
        { COMP_NDL_STOP_1606, 0, 0 },
        { COMP_DEPTH_1612, 0, 1 },
        { COMP_DIVE_TIME_1606, 0, 3 },
        { COMP_GAS_1606, 0, 4 },
        { COMP_EMPTY, 0, 5 },
        { COMP_EMPTY, 1, 5 },
        { COMP_SYS_1606, 0, 6 },
    };
    static const grid_widget_t top_widgets[] =
    {
        { COMP_NDL_STOP_1606, 0, 0 },
        { COMP_DEPTH_1612, 2, 0 },
        { COMP_DIVE_TIME_1606, 4, 0 },
        { COMP_GAS_1606, 4, 1 },
        { COMP_TEMP_0806, 6, 0 },
        { COMP_BATTERY_0806, 6, 1 },
    };
    const grid_widget_t *items = horizontal ? top_widgets : side_widgets;
    uint8_t count = horizontal ? (uint8_t)(sizeof(top_widgets) / sizeof(top_widgets[0])) : (uint8_t)(sizeof(side_widgets) / sizeof(side_widgets[0]));

    (void)memset(cfg->left_widgets, 0, sizeof(cfg->left_widgets));
    cfg->left_widget_count = count;
    for (uint8_t i = 0U; i < count; i++)
    {
        cfg->left_widgets[i] = items[i];
    }
}

static void layout_set_default_custom_cards(sys_config_t *cfg, bool horizontal)
{
    static const grid_widget_t side_custom[] =
    {
        { COMP_DEPTH_1606, 0, 0 },
        { COMP_PPO2_0806, 2, 0 },
        { COMP_BATTERY_0806, 3, 0 },
        { COMP_POD_0806, 4, 0 },
        { COMP_NDL_STOP_1606, 0, 1 },
        { COMP_CNS_0806, 2, 1 },
        { COMP_OTU_0806, 3, 1 },
        { COMP_HEADING_0806, 4, 1 },
        { COMP_GAS_1606, 0, 2 },
        { COMP_DIVE_TIME_1606, 2, 2 },
        { COMP_TTS_AT_5MIN_0806, 0, 3 },
        { COMP_TTS_DELTA_5MIN_0806, 1, 3 },
        { COMP_NDL_UP_3M_0806, 2, 3 },
        { COMP_NDL_DOWN_3M_0806, 3, 3 },
        { COMP_NDL_DELTA_3M_0806, 4, 3 },
        { COMP_GTR_0806, 0, 4 },
        { COMP_RMV_0806, 1, 4 },
        { COMP_SAC_0806, 2, 4 },
    };
    static const grid_widget_t side_sensor[] =
    {
        { COMP_ACCEL_2406, 0, 0 },
        { COMP_BATT_V_0806, 3, 0 },
        { COMP_PRESSURE_0806, 4, 0 },
        { COMP_GYRO_2406, 0, 1 },
        { COMP_CPU_0806, 3, 1 },
        { COMP_FPS_0806, 4, 1 },
        { COMP_MAG_2406, 0, 2 },
        { COMP_BLE_RSSI_0806, 3, 2 },
        { COMP_CHARGE_0806, 4, 2 },
        { COMP_MLX_2406, 0, 3 },
        { COMP_BATT_TEMP_0806, 3, 3 },
        { COMP_PRJ_TEMP_0806, 4, 3 },
        { COMP_TMAG_2406, 0, 4 },
        { COMP_NOFLY_0806, 3, 4 },
        { COMP_ATTITUDE_2406, 0, 5 },
        { COMP_SENSOR_STAT_1606, 3, 5 },
    };
    static const grid_widget_t top_custom[] =
    {
        { COMP_DEPTH_1606, 0, 0 },
        { COMP_PPO2_0806, 2, 0 },
        { COMP_BATTERY_0806, 3, 0 },
        { COMP_POD_0806, 4, 0 },
        { COMP_NDL_STOP_1606, 0, 1 },
        { COMP_CNS_0806, 2, 1 },
        { COMP_OTU_0806, 3, 1 },
        { COMP_HEADING_0806, 4, 1 },
        { COMP_GAS_1606, 0, 2 },
        { COMP_DIVE_TIME_1606, 2, 2 },
        { COMP_TTS_AT_5MIN_0806, 4, 2 },
        { COMP_TTS_DELTA_5MIN_0806, 5, 2 },
        { COMP_NDL_UP_3M_0806, 6, 2 },
        { COMP_NDL_DOWN_3M_0806, 0, 3 },
        { COMP_NDL_DELTA_3M_0806, 1, 3 },
        { COMP_GTR_0806, 2, 3 },
        { COMP_RMV_0806, 3, 3 },
        { COMP_SAC_0806, 4, 3 },
    };
    static const grid_widget_t top_sensor[] =
    {
        { COMP_ACCEL_2406, 0, 0 },
        { COMP_BATT_V_0806, 3, 0 },
        { COMP_PRESSURE_0806, 4, 0 },
        { COMP_GYRO_2406, 0, 1 },
        { COMP_CPU_0806, 3, 1 },
        { COMP_FPS_0806, 4, 1 },
        { COMP_MAG_2406, 0, 2 },
        { COMP_BLE_RSSI_0806, 3, 2 },
        { COMP_CHARGE_0806, 4, 2 },
        { COMP_TMAG_2406, 0, 3 },
        { COMP_BATT_TEMP_0806, 3, 3 },
        { COMP_PRJ_TEMP_0806, 4, 3 },
    };
    const grid_widget_t *custom = horizontal ? top_custom : side_custom;
    const grid_widget_t *sensor = horizontal ? top_sensor : side_sensor;
    uint8_t custom_count = horizontal ? (uint8_t)(sizeof(top_custom) / sizeof(top_custom[0])) : (uint8_t)(sizeof(side_custom) / sizeof(side_custom[0]));
    uint8_t sensor_count = horizontal ? (uint8_t)(sizeof(top_sensor) / sizeof(top_sensor[0])) : (uint8_t)(sizeof(side_sensor) / sizeof(side_sensor[0]));

    (void)memset(cfg->custom_cards, 0, sizeof(cfg->custom_cards));
    cfg->custom_card_count = 2U;
    cfg->custom_cards[0].widget_count = custom_count;
    cfg->custom_cards[1].widget_count = sensor_count;
    (void)snprintf(cfg->custom_cards[0].title, sizeof(cfg->custom_cards[0].title), "%s", "ALARM TARGETS");
    (void)snprintf(cfg->custom_cards[1].title, sizeof(cfg->custom_cards[1].title), "%s", "SENSOR PREVIEW");
    for (uint8_t i = 0U; i < custom_count; i++) cfg->custom_cards[0].widgets[i] = custom[i];
    for (uint8_t i = 0U; i < sensor_count; i++) cfg->custom_cards[1].widgets[i] = sensor[i];
}

static void layout_apply_direction_defaults(theme_t theme, order_t order)
{
    bool horizontal = (theme == THEME_CLASSIC);

    g_sys_config.safe_zone_w = horizontal ? 560U : 580U;
    g_sys_config.safe_zone_h = 420U;
    g_sys_config.offset_x = 0;
    g_sys_config.offset_y = -10;
    g_sys_config.theme_mode = (uint8_t)theme;
    g_sys_config.layout_order = (uint8_t)order;
    g_sys_config.panel_gap_u = 2U;
    layout_set_default_card_order(&g_sys_config);
    layout_set_default_fixed_widgets(&g_sys_config, horizontal);
    layout_set_default_custom_cards(&g_sys_config, horizontal);
}

/* =========================================================
 * Data Bus Setter 实现 — 硬件/模拟层专用
 * 铁律：仅更新数值 + 打脏标记，绝不碰 LVGL！
 * ========================================================= */

/* 显示层数据写入只负责数值同步和脏标记；告警由 alarm 模块的显式接口触发。 */
#define DEPTH_DISPLAY_DEBOUNCE_M      0.05f    /* 深度显示防抖：小于 0.05m 不刷新数字 */
#define ASCENT_RATE_UI_EPSILON        UI_ASCENT_RATE_DISPLAY_EPSILON_MPM

static void bus_apply_algo_gases(void)
{
    /* 仿真模式下把 UI 侧气体配置同步给减压算法，非仿真编译时不做任何事。 */
#ifdef PC_SIMULATOR
    deco_core_apply_gases_from_ui();
#endif
}

static void bus_apply_algo_gf(uint8_t gf_low, uint8_t gf_high)
{
    /* 让算法层始终读到与 UI 一致的 GF 设置。 */
#ifdef PC_SIMULATOR
    deco_core_set_gf(gf_low, gf_high);
#else
    (void)gf_low;
    (void)gf_high;
#endif
}

static void bus_apply_algo_salinity(uint8_t mode)
{
    /* 盐度模式只在仿真/调试侧需要显式同步。 */
#ifdef PC_SIMULATOR
    deco_core_set_salinity_mode(mode);
#else
    (void)mode;
#endif
}

static void bus_apply_algo_last_deco(uint8_t depth_m)
{
    /* 最后停留深度属于算法输入，保持与菜单设置同步。 */
#ifdef PC_SIMULATOR
    deco_core_set_final_stop_depth(depth_m);
#else
    (void)depth_m;
#endif
}

static void bus_apply_algo_safety_stop(uint8_t mode)
{
    /* 安全停留模式属于算法输入，PC 仿真侧同步到减压算法适配层。 */
#ifdef PC_SIMULATOR
    deco_core_set_safety_stop_mode(mode);
#else
    (void)mode;
#endif
}

float bus_calculate_gas_mod(uint8_t o2_pct, uint8_t he_pct, float max_ppo2)
{
#ifdef PC_SIMULATOR
    return deco_core_calculate_gas_mod(o2_pct, he_pct, max_ppo2);
#else
    return ui_calculate_gas_mod(o2_pct, he_pct, max_ppo2);
#endif
}

float bus_calculate_ppo2_bar(uint8_t o2_pct, float pressure_mbar)
{
    float pressure_bar = pressure_mbar / 1000.0f;
    if (pressure_bar < 0.0f) pressure_bar = 0.0f;
    return ((float)o2_pct / 100.0f) * pressure_bar;
}

static float bus_default_air_mod_m(void)
{
    float mod_m = bus_calculate_gas_mod(21U, 0U, 1.4f);
    return (mod_m > 0.0f) ? mod_m : 56.0f;
}

/* 温度统计累计值 */
static float    _temp_sum = 0.0f;        /* 温度累计和 */
static uint32_t _temp_sample_count = 0;  /* 温度采样次数 */
static uint8_t  s_gas_profile_batch_depth = 0U;
static bool     s_gas_profile_apply_pending = false;

static void bus_request_algo_gases_apply(void)
{
    if (s_gas_profile_batch_depth > 0U)
    {
        s_gas_profile_apply_pending = true;
        return;
    }

    bus_apply_algo_gases();
}

static float dive_log_triangle_area(const dive_pt_t *a,
                                    const dive_pt_t *b,
                                    const dive_pt_t *c)
{
    float ab_t = b->time_s - a->time_s;
    float ab_d = b->depth_m - a->depth_m;
    float ac_t = c->time_s - a->time_s;
    float ac_d = c->depth_m - a->depth_m;
    return fabsf(ab_t * ac_d - ab_d * ac_t);
}

static void dive_log_remove_at(uint16_t index)
{
    if (index >= s_dive_log_count)
    {
        return;
    }
    if (index + 1U < s_dive_log_count)
    {
        (void)memmove(&s_dive_log[index],
                      &s_dive_log[index + 1U],
                      (s_dive_log_count - index - 1U) * sizeof(s_dive_log[0]));
    }
    s_dive_log_count--;
}

static void dive_log_make_room(void)
{
    if (s_dive_log_count < MAX_DIVE_LOG)
    {
        return;
    }
    if (s_dive_log_count < 3U)
    {
        return;
    }

    {
        uint16_t drop_index = 1U;
        float drop_area = FLT_MAX;

        for (uint16_t i = 1U; i + 1U < s_dive_log_count; i++)
        {
            float area = dive_log_triangle_area(&s_dive_log[i - 1U],
                                                &s_dive_log[i],
                                                &s_dive_log[i + 1U]);
            if (area < drop_area)
            {
                drop_area = area;
                drop_index = i;
            }
        }

        dive_log_remove_at(drop_index);
    }
}

static bool deco_plan_equals_current(const deco_stop_t *stops, uint8_t count)
{
    if (s_deco_stop_count != count)
    {
        return false;
    }

    if (count == 0U)
    {
        return true;
    }

    if (stops == NULL)
    {
        return false;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        if ((fabsf(s_deco_stops[i].depth_m - stops[i].depth_m) > 0.01f) ||
                (fabsf(s_deco_stops[i].stay_min - stops[i].stay_min) > 0.01f))
        {
            return false;
        }
    }

    return true;
}

void data_init(void)
{
    uint8_t conservatism;

    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
    _temp_sum = 0.0f;
    _temp_sample_count = 0;
    s_gas_profile_batch_depth = 0U;
    s_gas_profile_apply_pending = false;

    g_sensor_data.ndl_bar_pct = 255U;
    g_sensor_data.gas_active_idx = 0;
    g_sensor_data.gas_recommended_idx = -1;
    g_sensor_data.gas_slot_count = 1;
    strncpy(g_sensor_data.gas_name, "AIR", sizeof(g_sensor_data.gas_name) - 1);

    strncpy(g_sensor_data.gas_slot_name[0], "AIR", sizeof(g_sensor_data.gas_slot_name[0]) - 1);
    g_sensor_data.gas_slot_o2_pct[0] = 21;
    g_sensor_data.gas_slot_he_pct[0] = 0;
    g_sensor_data.gas_slot_mod_m[0] = bus_default_air_mod_m();
    g_sensor_data.gas_slot_max_ppo2[0] = 1.4f;

    g_sensor_data.gas_slot_name[1][0] = '\0';
    g_sensor_data.gas_slot_o2_pct[1] = 0;
    g_sensor_data.gas_slot_he_pct[1] = 0;
    g_sensor_data.gas_slot_mod_m[1] = 0.0f;
    g_sensor_data.gas_slot_max_ppo2[1] = 0.0f;

    g_sensor_data.gas_slot_name[2][0] = '\0';
    g_sensor_data.gas_slot_o2_pct[2] = 0;
    g_sensor_data.gas_slot_he_pct[2] = 0;
    g_sensor_data.gas_slot_mod_m[2] = 0.0f;
    g_sensor_data.gas_slot_max_ppo2[2] = 0.0f;

    g_sensor_data.gas_slot_name[3][0] = '\0';
    g_sensor_data.gas_slot_o2_pct[3] = 0;
    g_sensor_data.gas_slot_he_pct[3] = 0;
    g_sensor_data.gas_slot_mod_m[3] = 0.0f;
    g_sensor_data.gas_slot_max_ppo2[3] = 0.0f;

    g_sensor_data.gas_slot_name[4][0] = '\0';
    g_sensor_data.gas_slot_o2_pct[4] = 0;
    g_sensor_data.gas_slot_he_pct[4] = 0;
    g_sensor_data.gas_slot_mod_m[4] = 0.0f;
    g_sensor_data.gas_slot_max_ppo2[4] = 0.0f;
    g_sensor_data.battery_voltage_v = 4.0f;
    g_sensor_data.charge_state = 0U;
    g_sensor_data.ambient_pressure_mbar = 1013.0f;
    g_sensor_data.nofly_time_min = 0U;
    g_sensor_data.fps = 0U;
    strncpy(g_sensor_data.sensor_status, "OK", sizeof(g_sensor_data.sensor_status) - 1);

    conservatism = g_sys_config.conservatism;
    if (conservatism >= UI_CONSERVATISM_PROFILE_COUNT)
    {
        conservatism = UI_CONSERVATISM_DEFAULT_LEVEL;
    }
    (void)ui_gf_from_conservatism_level(conservatism,
                                        &g_sensor_data.gf_low,
                                        &g_sensor_data.gf_high);

    /* 减压站预测数据初始化（仅初始化节数，数据本身由减压引擎填充） */
    s_deco_stop_count = 0U;
}

static void bus_set_depth_internal(float depth_m, bool force)
{
    /* 深度数值显示继续保留轻量防抖，避免数字末位来回跳 */
    /* 架构约束：
     * 1. 这里仅负责“当前深度显示值”；
     * 2. 潜次统计（MAX/AVG DEPTH）必须由上游业务层统一计算后回灌；
     * 3. 不能再把显示防抖后的深度变化误当成统计采样，否则串口直打 10m
     *    会被大量表面/噪声样本稀释，最终出现 AVG DEPTH=0.2m 这类假值。
     * 上升率仍由 bus_set_ascent_rate() 单独输入，避免不同采样周期下互相污染。 */
    if (force || fabsf(g_sensor_data.depth - depth_m) > DEPTH_DISPLAY_DEBOUNCE_M)
    {
        g_sensor_data.depth = depth_m;
        bus_mark_dirty(DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS);
    }
}

void bus_set_depth(float depth_m)
{
    bus_set_depth_internal(depth_m, false);
}

void bus_set_depth_force(float depth_m)
{
    bus_set_depth_internal(depth_m, true);
}

void bus_set_dive_profile_stats(float max_depth_m, float avg_depth_m)
{
    bool changed = false;

    if (max_depth_m < 0.0f)
    {
        max_depth_m = 0.0f;
    }
    if (avg_depth_m < 0.0f)
    {
        avg_depth_m = 0.0f;
    }

    /* 这份 summary 是“平均深度小组件”和 “InfoMenu -> LAST DIVE” 的共享真值，
     * 与某个 widget 是否存在、是否被删除无关。 */
    if (fabsf(g_sensor_data.max_depth - max_depth_m) > 0.001f)
    {
        g_sensor_data.max_depth = max_depth_m;
        changed = true;
    }
    if (fabsf(g_sensor_data.avg_depth - avg_depth_m) > 0.001f)
    {
        g_sensor_data.avg_depth = avg_depth_m;
        changed = true;
    }

    if (changed)
    {
        bus_mark_dirty(DIRTY_DIVE_PROFILE);
    }
}

void bus_set_ascent_rate(float rate_mpm)
{
    float prev_rate = g_sensor_data.ascent_rate;
    bool prev_is_moving = fabsf(prev_rate) > RATE_STILL_THRESHOLD;
    bool current_is_moving;

    if (fabsf(rate_mpm) < ASCENT_RATE_UI_EPSILON)
    {
        /* 很小的速度波动对用户没有意义，直接钳到 0，减少图标抖动。 */
        rate_mpm = 0.0f;
    }

    current_is_moving = fabsf(rate_mpm) > RATE_STILL_THRESHOLD;

    if ((fabsf(rate_mpm - prev_rate) >= ASCENT_RATE_UI_EPSILON) ||
            (current_is_moving != prev_is_moving))
    {
        /* 只有跨过显示阈值或“静止/运动”状态切换时才刷新 UI，
         * 这样能明显降低速率图标在临界值附近闪烁。 */
        g_sensor_data.ascent_rate = rate_mpm;
        bus_mark_dirty(DIRTY_DIVE_PROFILE);
    }

}

void bus_set_ndl(int16_t ndl_min)
{
    if (g_sensor_data.ndl != ndl_min)
    {
        g_sensor_data.ndl = ndl_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_tts(uint16_t tts_min)
{
    if (g_sensor_data.tts != tts_min)
    {
        g_sensor_data.tts = tts_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_tts_at_5min(uint16_t tts_min)
{
    if (!g_sensor_data.tts_at_5min_valid || g_sensor_data.tts_at_5min_min != tts_min)
    {
        g_sensor_data.tts_at_5min_valid = true;
        g_sensor_data.tts_at_5min_min = tts_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_tts_delta_5min(int16_t delta_min)
{
    if (!g_sensor_data.tts_delta_5min_valid || g_sensor_data.tts_delta_5min_min != delta_min)
    {
        g_sensor_data.tts_delta_5min_valid = true;
        g_sensor_data.tts_delta_5min_min = delta_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_ndl_up_3m(int16_t ndl_min)
{
    if (!g_sensor_data.ndl_up_3m_valid || g_sensor_data.ndl_up_3m_min != ndl_min)
    {
        g_sensor_data.ndl_up_3m_valid = true;
        g_sensor_data.ndl_up_3m_min = ndl_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_ndl_down_3m(int16_t ndl_min)
{
    if (!g_sensor_data.ndl_down_3m_valid || g_sensor_data.ndl_down_3m_min != ndl_min)
    {
        g_sensor_data.ndl_down_3m_valid = true;
        g_sensor_data.ndl_down_3m_min = ndl_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_set_ndl_delta_3m(int16_t ndl_min)
{
    if (!g_sensor_data.ndl_delta_3m_valid || g_sensor_data.ndl_delta_3m_min != ndl_min)
    {
        g_sensor_data.ndl_delta_3m_valid = true;
        g_sensor_data.ndl_delta_3m_min = ndl_min;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

void bus_clear_ndl_delta_3m(void)
{
    /* 方向不明确时不写入新值，也不清掉旧值；上电初始 invalid 时仍显示 "--"。 */
}

void bus_set_gtr(uint16_t gtr_min)
{
    if (!g_sensor_data.gtr_valid || g_sensor_data.gtr_min != gtr_min)
    {
        g_sensor_data.gtr_valid = true;
        g_sensor_data.gtr_min = gtr_min;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_rmv(float rmv_lpm)
{
    if (!g_sensor_data.rmv_valid || fabsf(g_sensor_data.rmv_lpm - rmv_lpm) > 0.05f)
    {
        g_sensor_data.rmv_valid = true;
        g_sensor_data.rmv_lpm = rmv_lpm;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_sac_rate(float sac_lpm)
{
    if (!g_sensor_data.sac_valid || fabsf(g_sensor_data.sac_rate - sac_lpm) > 0.05f)
    {
        g_sensor_data.sac_valid = true;
        g_sensor_data.sac_rate = sac_lpm;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_pod(uint8_t pod_idx, float bar)
{
    if (pod_idx == 0 && (!g_sensor_data.pod1_valid || g_sensor_data.pod1_bar != bar))
    {
        g_sensor_data.pod1_valid = true;
        g_sensor_data.pod1_bar = bar;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
    else if (pod_idx == 1 && (!g_sensor_data.pod2_valid || g_sensor_data.pod2_bar != bar))
    {
        g_sensor_data.pod2_valid = true;
        g_sensor_data.pod2_bar = bar;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_battery(float pct)
{
    static bool s_battery_initialized = false;

    if (pct < 0.0f)
    {
        pct = 0.0f;
    }
    else if (pct > 100.0f)
    {
        pct = 100.0f;
    }

    if (!s_battery_initialized || fabsf(g_sensor_data.battery_pct - pct) > 0.1f)
    {
        s_battery_initialized = true;
        g_sensor_data.battery_pct = pct;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_sys_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    hour = (hour > 23U) ? 0U : hour;
    minute = (minute > 59U) ? 0U : minute;
    second = (second > 59U) ? 0U : second;

    bool visible_time_changed = (g_sensor_data.sys_time_h != hour) || (g_sensor_data.sys_time_m != minute);

    if (visible_time_changed || (g_sensor_data.sys_time_s != second)) {
        g_sensor_data.sys_time_h = hour;
        g_sensor_data.sys_time_m = minute;
        g_sensor_data.sys_time_s = second;
        if (visible_time_changed)
        {
            bus_mark_dirty(DIRTY_SYSTEM);
        }
    }
}

void bus_set_heading(uint16_t heading_deg)
{
    if (g_sensor_data.heading != heading_deg)
    {
        g_sensor_data.heading = heading_deg;
        bus_mark_dirty(DIRTY_COMPASS);
    }
}

void bus_set_dive_time(uint32_t dive_s)
{
    if (g_sensor_data.dive_time_s != dive_s)
    {
        g_sensor_data.dive_time_s = dive_s;
        bus_mark_dirty(DIRTY_DIVE_PROFILE);
    }
}

void bus_set_surface_time(uint32_t surface_s)
{
    if (g_sensor_data.surface_time_s != surface_s)
    {
        g_sensor_data.surface_time_s = surface_s;
        bus_mark_dirty(DIRTY_DIVE_PROFILE);
    }
}

void bus_set_dive_lifecycle_phase(dive_lifecycle_phase_t phase)
{
    if (phase > DIVE_LIFECYCLE_SURFACING_PENDING)
    {
        phase = DIVE_LIFECYCLE_SURFACE_CONFIRMED;
    }

    if (g_sensor_data.dive_lifecycle_phase != phase)
    {
        g_sensor_data.dive_lifecycle_phase = phase;
        bus_mark_dirty(DIRTY_DIVE_PROFILE);
    }
}

void bus_set_ppo2(uint8_t sensor_idx, float ppo2_val)
{
    if (sensor_idx < GAS_COUNT && g_sensor_data.ppo2[sensor_idx] != ppo2_val)
    {
        g_sensor_data.ppo2[sensor_idx] = ppo2_val;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_gas(uint8_t gas_idx, const char *gas_name)
{
    bool changed = false;
    uint8_t gas_count = g_sensor_data.gas_slot_count;

    if (gas_count > GAS_COUNT)
    {
        gas_count = GAS_COUNT;
    }
    if (gas_count == 0U)
    {
        gas_idx = 0;
        gas_name = gas_name ? gas_name : "--";
    }
    if (gas_idx >= gas_count)
    {
        gas_idx = 0;
    }

    if (g_sensor_data.gas_active_idx != gas_idx)
    {
        g_sensor_data.gas_active_idx = gas_idx;
        if (g_sensor_data.gas_recommended_idx == (int8_t)gas_idx)
        {
            g_sensor_data.gas_recommended_idx = -1;
        }
        changed = true;
    }
    if (gas_name != NULL && strncmp(g_sensor_data.gas_name, gas_name, 15) != 0)
    {
        strncpy(g_sensor_data.gas_name, gas_name, 15);
        g_sensor_data.gas_name[15] = '\0';
        changed = true;
    }
    if (changed)
    {
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_set_recommended_gas_idx(int8_t gas_idx)
{
    uint8_t gas_count = bus_get_gas_slot_count();

    if (gas_idx < 0 || gas_count == 0U || gas_idx >= (int8_t)gas_count || gas_idx >= (int8_t)GAS_COUNT ||
        gas_idx == (int8_t)bus_get_gas_active_idx())
    {
        gas_idx = -1;
    }

    if (g_sensor_data.gas_recommended_idx != gas_idx)
    {
        g_sensor_data.gas_recommended_idx = gas_idx;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

void bus_begin_gas_profile_update(void)
{
    if (s_gas_profile_batch_depth < 255U) s_gas_profile_batch_depth++;
    s_gas_profile_apply_pending = true;
}

void bus_end_gas_profile_update(void)
{
    if (s_gas_profile_batch_depth == 0U) return;
    s_gas_profile_batch_depth--;
    if (s_gas_profile_batch_depth == 0U && s_gas_profile_apply_pending)
    {
        s_gas_profile_apply_pending = false;
        bus_apply_algo_gases();
    }
}

void bus_set_gas_slot_count(uint8_t count)
{
    if (count > GAS_COUNT)
    {
        count = GAS_COUNT;
    }

    if (g_sensor_data.gas_slot_count != count)
    {
        g_sensor_data.gas_slot_count = count;
        if (count == 0U)
        {
            g_sensor_data.gas_active_idx = 0;
            snprintf(g_sensor_data.gas_name, sizeof(g_sensor_data.gas_name), "--");
        }
        else if (g_sensor_data.gas_active_idx >= count)
        {
            g_sensor_data.gas_active_idx = 0;
            snprintf(g_sensor_data.gas_name,
                     sizeof(g_sensor_data.gas_name),
                     "%s",
                     g_sensor_data.gas_slot_name[0][0] ? g_sensor_data.gas_slot_name[0] : "AIR");
        }
        if (g_sensor_data.gas_recommended_idx >= (int8_t)count)
        {
            g_sensor_data.gas_recommended_idx = -1;
        }
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
    bus_request_algo_gases_apply();
}

void bus_set_gas_slot(uint8_t gas_idx, const char *gas_name,
                           uint8_t o2_pct, uint8_t he_pct, float mod_m, float max_ppo2)
{
    if (gas_idx >= GAS_COUNT)
    {
        return;
    }

    bool changed = false;

    if (gas_name != NULL && strncmp(g_sensor_data.gas_slot_name[gas_idx], gas_name, 15) != 0)
    {
        strncpy(g_sensor_data.gas_slot_name[gas_idx], gas_name, 15);
        g_sensor_data.gas_slot_name[gas_idx][15] = '\0';
        changed = true;
    }
    if (g_sensor_data.gas_slot_o2_pct[gas_idx] != o2_pct)
    {
        g_sensor_data.gas_slot_o2_pct[gas_idx] = o2_pct;
        changed = true;
    }
    if (g_sensor_data.gas_slot_he_pct[gas_idx] != he_pct)
    {
        g_sensor_data.gas_slot_he_pct[gas_idx] = he_pct;
        changed = true;
    }
    if (fabsf(g_sensor_data.gas_slot_mod_m[gas_idx] - mod_m) > 0.05f)
    {
        g_sensor_data.gas_slot_mod_m[gas_idx] = mod_m;
        changed = true;
    }
    if (fabsf(g_sensor_data.gas_slot_max_ppo2[gas_idx] - max_ppo2) > 0.005f)
    {
        g_sensor_data.gas_slot_max_ppo2[gas_idx] = max_ppo2;
        changed = true;
    }

    if (changed)
    {
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
    if (changed)
    {
        bus_request_algo_gases_apply();
    }
}

void bus_set_deco(int16_t stop_m, uint8_t stop_min)
{
    if (g_sensor_data.next_stop_m != stop_m || g_sensor_data.next_stop_min != stop_min)
    {
        g_sensor_data.next_stop_m = stop_m;
        g_sensor_data.next_stop_min = stop_min;
        bus_mark_dirty(DIRTY_PLAN);
    }
}

void bus_set_cns(uint8_t cns_pct)
{
    if (g_sensor_data.cns_pct != cns_pct)
    {
        g_sensor_data.cns_pct = cns_pct;
        bus_mark_dirty(DIRTY_TISSUE_TOX);
    }
}

void bus_set_otu(uint16_t otu_val)
{
    if (g_sensor_data.otu != otu_val)
    {
        g_sensor_data.otu = otu_val;
        bus_mark_dirty(DIRTY_TISSUE_TOX);
    }
}

void bus_set_gf99(float gf99)
{
    if (fabsf(g_sensor_data.gf99 - gf99) > 0.1f)
    {
        g_sensor_data.gf99 = gf99;
        bus_mark_dirty(DIRTY_TISSUE_TOX);
    }
}

void bus_set_surf_gf(float surf_gf)
{
    if (fabsf(g_sensor_data.surf_gf - surf_gf) > 0.1f)
    {
        g_sensor_data.surf_gf = surf_gf;
        bus_mark_dirty(DIRTY_TISSUE_TOX);
    }
}

/* =========================================================
 * 临界区保护的数组写入 — 防止多线程数据撕裂
 *
 * 铁律：> 32bit 的数据块拷贝必须包在关中断临界区里。
 *   - PC 仿真器: rt_hw_interrupt_disable/enable 替换为空操作
 *   - 真机 RT-Thread: 触发底层 cpsr 关中断，耗时 < 0.1us
 * ========================================================= */

/* 16 组织舱负荷数组写入（RAW 为有符号 absolute GF，必须包临界区） */
void bus_set_tissue_loads(const int16_t tissue_raw_pct[16],
                          const uint8_t tissue_gf_pct[16],
                          float tissue_target_gf_pct)
{
    if (tissue_raw_pct == NULL || tissue_gf_pct == NULL)
    {
        return;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    if ((memcmp(g_sensor_data.tissue_raw_pct, tissue_raw_pct, sizeof(g_sensor_data.tissue_raw_pct)) == 0) &&
        (memcmp(g_sensor_data.tissue_gf_pct, tissue_gf_pct, sizeof(g_sensor_data.tissue_gf_pct)) == 0) &&
        (fabsf(g_sensor_data.tissue_target_gf_pct - tissue_target_gf_pct) <= 0.1f))
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    memcpy(g_sensor_data.tissue_raw_pct, tissue_raw_pct, sizeof(g_sensor_data.tissue_raw_pct));
    memcpy(g_sensor_data.tissue_gf_pct, tissue_gf_pct, sizeof(g_sensor_data.tissue_gf_pct));
    g_sensor_data.tissue_target_gf_pct = tissue_target_gf_pct;
    g_sensor_data.dirty_mask |= DIRTY_TISSUE_TOX;
    rt_hw_interrupt_enable(level);
}

void bus_set_tissue_normalized_payload(const uint16_t tissue_bar_permille[16],
                                       uint16_t pi_permille,
                                       float ambient_pressure_bar,
                                       float inspired_n2_bar,
                                       float inspired_he_bar,
                                       const float tissue_n2_bar[16],
                                       const float tissue_he_bar[16],
                                       const float tissue_m_value_bar[16],
                                       const float tissue_m_gf_bar[16])
{
    if (tissue_bar_permille == NULL || tissue_n2_bar == NULL || tissue_he_bar == NULL || tissue_m_value_bar == NULL || tissue_m_gf_bar == NULL)
    {
        return;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    if ((memcmp(g_sensor_data.tissue_bar_permille, tissue_bar_permille, sizeof(g_sensor_data.tissue_bar_permille)) == 0) &&
        (memcmp(g_sensor_data.tissue_n2_bar, tissue_n2_bar, sizeof(g_sensor_data.tissue_n2_bar)) == 0) &&
        (memcmp(g_sensor_data.tissue_he_bar, tissue_he_bar, sizeof(g_sensor_data.tissue_he_bar)) == 0) &&
        (memcmp(g_sensor_data.tissue_m_value_bar, tissue_m_value_bar, sizeof(g_sensor_data.tissue_m_value_bar)) == 0) &&
        (memcmp(g_sensor_data.tissue_m_gf_bar, tissue_m_gf_bar, sizeof(g_sensor_data.tissue_m_gf_bar)) == 0) &&
        (g_sensor_data.tissue_pi_permille == pi_permille) &&
        (fabsf(g_sensor_data.tissue_ambient_pressure_bar - ambient_pressure_bar) <= 0.0001f) &&
        (fabsf(g_sensor_data.tissue_inspired_n2_bar - inspired_n2_bar) <= 0.0001f) &&
        (fabsf(g_sensor_data.tissue_inspired_he_bar - inspired_he_bar) <= 0.0001f) &&
        g_sensor_data.tissue_normalized_valid)
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    memcpy(g_sensor_data.tissue_bar_permille, tissue_bar_permille, sizeof(g_sensor_data.tissue_bar_permille));
    memcpy(g_sensor_data.tissue_n2_bar, tissue_n2_bar, sizeof(g_sensor_data.tissue_n2_bar));
    memcpy(g_sensor_data.tissue_he_bar, tissue_he_bar, sizeof(g_sensor_data.tissue_he_bar));
    memcpy(g_sensor_data.tissue_m_value_bar, tissue_m_value_bar, sizeof(g_sensor_data.tissue_m_value_bar));
    memcpy(g_sensor_data.tissue_m_gf_bar, tissue_m_gf_bar, sizeof(g_sensor_data.tissue_m_gf_bar));
    g_sensor_data.tissue_pi_permille = pi_permille;
    g_sensor_data.tissue_ambient_pressure_bar = ambient_pressure_bar;
    g_sensor_data.tissue_inspired_n2_bar = inspired_n2_bar;
    g_sensor_data.tissue_inspired_he_bar = inspired_he_bar;
    g_sensor_data.tissue_normalized_valid = true;
    g_sensor_data.dirty_mask |= DIRTY_TISSUE_TOX;
    rt_hw_interrupt_enable(level);
}

/* 完整减压站序列写入（可变长度，必须包临界区） */
void bus_set_deco_plan(const deco_stop_t *stops, uint8_t count)
{
    if (count > MAX_DECO_STOPS)
    {
        count = MAX_DECO_STOPS;
    }
    if (stops == NULL)
    {
        count = 0U;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    if (deco_plan_equals_current(stops, count))
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    s_deco_stop_count = count;
    if ((count > 0U) && (stops != NULL))
    {
        (void)memcpy(s_deco_stops, stops, count * sizeof(deco_stop_t));
    }
    g_sensor_data.dirty_mask |= DIRTY_PLAN;
    rt_hw_interrupt_enable(level);
}

dirty_mask_t bus_take_dirty(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    dirty_mask_t mask = g_sensor_data.dirty_mask;
    g_sensor_data.dirty_mask = DIRTY_NONE;
    rt_hw_interrupt_enable(level);
#if UI_DIRTY_THROTTLE_ENABLED
    mask = bus_throttle_dirty_mask(mask);
#endif
    return mask;
}

void bus_requeue_dirty(dirty_mask_t mask)
{
    bus_mark_dirty(mask);
}

void bus_requeue_dirty_immediate(dirty_mask_t mask)
{
#if UI_DIRTY_THROTTLE_ENABLED
    rt_base_t level = rt_hw_interrupt_disable();
    s_dirty_throttle_bypass_once |= mask;
    rt_hw_interrupt_enable(level);
#endif
    bus_mark_dirty(mask);
}

void bus_clear_all_dirty(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    g_sensor_data.dirty_mask = DIRTY_NONE;
    rt_hw_interrupt_enable(level);
}

void bus_set_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.temperature_c - temp_c) > 0.1f)
    {
        g_sensor_data.temperature_c = temp_c;
        bus_mark_dirty(DIRTY_SYSTEM);

        /* 统计计算：最低温度 + 平均温度 */
        if (_temp_sample_count == 0 || temp_c < g_sensor_data.min_temp)
        {
            g_sensor_data.min_temp = temp_c;
        }
        if (_temp_sample_count == 0 || temp_c > g_sensor_data.max_temp)
        {
            g_sensor_data.max_temp = temp_c;
        }
        _temp_sum += temp_c;
        _temp_sample_count++;
        g_sensor_data.avg_temp = (_temp_sample_count > 0) ? (_temp_sum / _temp_sample_count) : 0.0f;
    }
}

void bus_set_dive_temperature_stats(float min_temp_c, float avg_temp_c, float max_temp_c)
{
    if (!isfinite(min_temp_c)) min_temp_c = 0.0f;
    if (!isfinite(avg_temp_c)) avg_temp_c = min_temp_c;
    if (!isfinite(max_temp_c)) max_temp_c = min_temp_c;

    if (fabsf(g_sensor_data.min_temp - min_temp_c) > 0.1f ||
        fabsf(g_sensor_data.avg_temp - avg_temp_c) > 0.1f ||
        fabsf(g_sensor_data.max_temp - max_temp_c) > 0.1f)
    {
        g_sensor_data.min_temp = min_temp_c;
        g_sensor_data.avg_temp = avg_temp_c;
        g_sensor_data.max_temp = max_temp_c;
        _temp_sum = avg_temp_c;
        _temp_sample_count = (avg_temp_c == 0.0f && min_temp_c == 0.0f && max_temp_c == 0.0f) ? 0U : 1U;
        bus_mark_dirty(DIRTY_DIVE_PROFILE | DIRTY_SYSTEM);
    }
}

void bus_set_bat_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.bat_temperature_c - temp_c) > 0.1f)
    {
        g_sensor_data.bat_temperature_c = temp_c;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_prj_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.prj_temperature_c - temp_c) > 0.1f)
    {
        g_sensor_data.prj_temperature_c = temp_c;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_battery_voltage(float voltage_v)
{
    if (fabsf(g_sensor_data.battery_voltage_v - voltage_v) > 0.01f)
    {
        g_sensor_data.battery_voltage_v = voltage_v;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_charge_state(uint8_t state)
{
    if (state > 2U)
    {
        state = 0U;
    }
    if (g_sensor_data.charge_state != state)
    {
        g_sensor_data.charge_state = state;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_low_power_shutdown_state(bool active, uint8_t remaining_sec)
{
    if (!active)
    {
        remaining_sec = 0U;
    }

    if (g_sensor_data.low_power_shutdown_active != active ||
        g_sensor_data.low_power_shutdown_remaining_sec != remaining_sec)
    {
        g_sensor_data.low_power_shutdown_active = active;
        g_sensor_data.low_power_shutdown_remaining_sec = remaining_sec;
        bus_mark_dirty(DIRTY_ALARM);
    }
}

void bus_set_ambient_pressure(float pressure_mbar)
{
    if (fabsf(g_sensor_data.ambient_pressure_mbar - pressure_mbar) > 0.5f)
    {
        g_sensor_data.ambient_pressure_mbar = pressure_mbar;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_nofly_time(uint16_t minutes)
{
    if (g_sensor_data.nofly_time_min != minutes)
    {
        g_sensor_data.nofly_time_min = minutes;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_gyro(float x_dps, float y_dps, float z_dps)
{
    if (fabsf(g_sensor_data.gyro_x_dps - x_dps) > 0.1f ||
            fabsf(g_sensor_data.gyro_y_dps - y_dps) > 0.1f ||
            fabsf(g_sensor_data.gyro_z_dps - z_dps) > 0.1f)
    {
        g_sensor_data.gyro_x_dps = x_dps;
        g_sensor_data.gyro_y_dps = y_dps;
        g_sensor_data.gyro_z_dps = z_dps;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_accel(float x_g, float y_g, float z_g)
{
    if (fabsf(g_sensor_data.accel_x_g - x_g) > 0.01f ||
            fabsf(g_sensor_data.accel_y_g - y_g) > 0.01f ||
            fabsf(g_sensor_data.accel_z_g - z_g) > 0.01f)
    {
        g_sensor_data.accel_x_g = x_g;
        g_sensor_data.accel_y_g = y_g;
        g_sensor_data.accel_z_g = z_g;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_mag(float x_ut, float y_ut, float z_ut)
{
    if (fabsf(g_sensor_data.mag_x_ut - x_ut) > 0.1f ||
            fabsf(g_sensor_data.mag_y_ut - y_ut) > 0.1f ||
            fabsf(g_sensor_data.mag_z_ut - z_ut) > 0.1f)
    {
        g_sensor_data.mag_x_ut = x_ut;
        g_sensor_data.mag_y_ut = y_ut;
        g_sensor_data.mag_z_ut = z_ut;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_mlx(float x_ut, float y_ut, float z_ut)
{
    if (fabsf(g_sensor_data.mlx_x_ut - x_ut) > 0.1f ||
            fabsf(g_sensor_data.mlx_y_ut - y_ut) > 0.1f ||
            fabsf(g_sensor_data.mlx_z_ut - z_ut) > 0.1f)
    {
        g_sensor_data.mlx_x_ut = x_ut;
        g_sensor_data.mlx_y_ut = y_ut;
        g_sensor_data.mlx_z_ut = z_ut;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_tmag(float x_ut, float y_ut, float z_ut)
{
    float total_ut = sqrtf(x_ut * x_ut + y_ut * y_ut + z_ut * z_ut);
    if (fabsf(g_sensor_data.tmag_x_ut - x_ut) > 0.1f ||
            fabsf(g_sensor_data.tmag_y_ut - y_ut) > 0.1f ||
            fabsf(g_sensor_data.tmag_z_ut - z_ut) > 0.1f)
    {
        g_sensor_data.tmag_x_ut = x_ut;
        g_sensor_data.tmag_y_ut = y_ut;
        g_sensor_data.tmag_z_ut = z_ut;
        g_sensor_data.tmag_ut = total_ut;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_attitude(int16_t pitch_deg, int16_t roll_deg, uint16_t heading_deg)
{
    heading_deg %= 360U;
    if (g_sensor_data.pitch_deg != pitch_deg ||
            g_sensor_data.roll_deg != roll_deg ||
            g_sensor_data.attitude_heading_deg != heading_deg)
    {
        g_sensor_data.pitch_deg = pitch_deg;
        g_sensor_data.roll_deg = roll_deg;
        g_sensor_data.attitude_heading_deg = heading_deg;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_ble_rssi(int16_t rssi_dbm)
{
    if (g_sensor_data.ble_rssi_dbm != rssi_dbm)
    {
        g_sensor_data.ble_rssi_dbm = rssi_dbm;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_cpu_load(uint8_t pct)
{
    if (pct > 100U)
    {
        pct = 100U;
    }
    if (g_sensor_data.cpu_load_pct != pct)
    {
        g_sensor_data.cpu_load_pct = pct;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_fps(uint16_t fps)
{
    if (g_sensor_data.fps != fps)
    {
        g_sensor_data.fps = fps;
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_sensor_status(const char *status)
{
    const char *text = (status != NULL) ? status : "--";
    if (strncmp(g_sensor_data.sensor_status, text, sizeof(g_sensor_data.sensor_status) - 1U) != 0)
    {
        (void)snprintf(g_sensor_data.sensor_status, sizeof(g_sensor_data.sensor_status), "%s", text);
        bus_mark_dirty(DIRTY_SENSOR);
    }
}

void bus_set_ui_layout(const ble_ui_sync_payload_t *payload)
{
    UI_DATA_LAYOUT_TRACE("[BUS] bus_set_ui_layout called, version=0x%02X\r\n",
                         payload ? payload->version : 0);

    if (payload == NULL || payload->version != BLE_CFG_VERSION)
    {
        UI_DATA_LAYOUT_TRACE("[BUS] REJECTED: payload=%p, version=0x%02X\r\n",
                             payload, payload ? payload->version : 0);
        return;
    }

    /* 临界区保护，防止 UI 任务在中途读到撕裂的数据 */
#ifdef PC_SIMULATOR
    volatile rt_base_t level = 0;
#else
    rt_base_t level = rt_hw_interrupt_disable();
#endif

    /* 1. 兼容旧协议：旧 payload 只有 8 个 card_order 槽位，不能按新运行时数组长度整块 memcpy */
    for (size_t i = 0; i < sizeof(g_sys_config.card_order); i++)
    {
        g_sys_config.card_order[i] = PAGE_ID_UNUSED;
    }
    g_sys_config.card_order[PAGE_POS_INFO] = PAGE_ID_INFO;
    g_sys_config.card_order[PAGE_POS_SETUP] = PAGE_ID_SETUP;
    {
        uint8_t dynamic_pos = PAGE_POS_DYNAMIC_FIRST;

        for (int i = 0; i < 8 && dynamic_pos < PAGE_POS_SETUP; i++)
        {
            uint8_t page_id = payload->card_order[i];

            if (page_id == PAGE_ID_UNUSED)
            {
                continue;
            }

            g_sys_config.card_order[dynamic_pos++] = page_id;
        }
    }

    /* 2. 映射左侧 2x7 锚点配置到 g_sys_config */
    memset(g_sys_config.left_widgets, 0, sizeof(g_sys_config.left_widgets));
    g_sys_config.left_widget_count = (payload->left_count > LEFT_MAX_WIDGETS)
                                     ? LEFT_MAX_WIDGETS
                                     : payload->left_count;
    for (int i = 0; i < g_sys_config.left_widget_count; i++)
    {
        g_sys_config.left_widgets[i].widget_id = (comp_id_t)payload->left_widgets[i].widget_id;
        g_sys_config.left_widgets[i].x         = payload->left_widgets[i].x;
        g_sys_config.left_widgets[i].y         = payload->left_widgets[i].y;
    }

    /* 3. 兼容旧协议：单张 5F 配置映射到 custom_cards[0] */
    memset(g_sys_config.custom_cards, 0, sizeof(g_sys_config.custom_cards));
    memset(g_sys_config.custom_card_slot, 0xFF, sizeof(g_sys_config.custom_card_slot));
    g_sys_config.custom_card_count = 0;

    {
        uint8_t custom_idx = 0;

        /* 在 card_order 中查找 CUSTOM_GRID 卡片的位置，设置正确的 slot 映射。
         * 即使 custom_5f_count 为 0，也要保留自定义卡本身，让空自定义卡显示标题；
         * 真正不显示任何内容的页面由 PAGE_ID_BLANK 表达。 */
        for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < PAGE_POS_SETUP; pos++)
        {
            if (g_sys_config.card_order[pos] == PAGE_ID_CUSTOM_GRID)
            {
                g_sys_config.custom_card_slot[pos] = custom_idx;
                custom_idx++;
                if (custom_idx >= MAX_CUSTOM_CARDS)
                {
                    break;
                }
            }
        }

        g_sys_config.custom_card_count = custom_idx;
    }

    if (g_sys_config.custom_card_count > 0U)
    {
        g_sys_config.custom_cards[0].widget_count = (payload->custom_5f_count > MAX_5F_WIDGETS)
            ? MAX_5F_WIDGETS
            : payload->custom_5f_count;
        (void)snprintf(g_sys_config.custom_cards[0].title,
                       sizeof(g_sys_config.custom_cards[0].title),
                       "%s",
                       "CUSTOM WIDGETS");
    }
    for (int i = 0; i < g_sys_config.custom_cards[0].widget_count; i++)
    {
        g_sys_config.custom_cards[0].widgets[i].widget_id = (comp_id_t)payload->custom_5f_widgets[i].widget_id;
        g_sys_config.custom_cards[0].widgets[i].x = payload->custom_5f_widgets[i].c;  /* 列 -> x */
        g_sys_config.custom_cards[0].widgets[i].y = payload->custom_5f_widgets[i].r;  /* 行 -> y */
    }

    /* 4. 打上终极脏标记，通知 UI 推倒重建 */
    for (uint8_t page_idx = 1U; page_idx < g_sys_config.custom_card_count; page_idx++)
    {
        g_sys_config.custom_cards[page_idx] = g_sys_config.custom_cards[0];
    }

    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
    layout_archive_save_current();
    UI_DATA_LAYOUT_TRACE("[BUS] DIRTY_UI_LAYOUT set, dirty_mask=0x%08X\r\n",
                         g_sensor_data.dirty_mask);

#ifdef PC_SIMULATOR
    (void)level;
#else
    rt_hw_interrupt_enable(level);
#endif
}

// void bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count)
// {
//     if (g_sensor_data.strobe_on != strobe_on ||
//         g_sensor_data.flashlight_on != flashlight_on ||
//         g_sensor_data.cylinder_count != cylinder_count) {
//         g_sensor_data.strobe_on = strobe_on;
//         g_sensor_data.flashlight_on = flashlight_on;
//         g_sensor_data.cylinder_count = cylinder_count;
//         g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
//     }
// }

void bus_toggle_layout_order(void)
{
    g_sys_config.layout_order = (g_sys_config.layout_order == ORDER_NORMAL)
                                ? ORDER_REVERSE
                                : ORDER_NORMAL;
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_set_layout_mode(theme_t theme, order_t order)
{
    bool changed = false;

    if (theme != THEME_TECH && theme != THEME_CLASSIC)
    {
        theme = THEME_TECH;
    }
    if (order != ORDER_NORMAL && order != ORDER_REVERSE)
    {
        order = ORDER_NORMAL;
    }

    if (g_sys_config.theme_mode != (uint8_t)theme)
    {
        g_sys_config.theme_mode = (uint8_t)theme;
        changed = true;
    }
    if (g_sys_config.layout_order != (uint8_t)order)
    {
        g_sys_config.layout_order = (uint8_t)order;
        changed = true;
    }

    if (changed)
    {
        bus_mark_dirty(DIRTY_UI_LAYOUT);
    }
}

void bus_switch_layout_profile(theme_t theme, order_t order)
{
    layout_archive_id_t target_id;

    if (theme != THEME_TECH && theme != THEME_CLASSIC)
    {
        theme = THEME_TECH;
    }
    if (order != ORDER_NORMAL && order != ORDER_REVERSE)
    {
        order = ORDER_NORMAL;
    }

    layout_archive_save_current();
    target_id = layout_archive_id_for(theme, order);
    if (s_layout_archive_valid[target_id])
    {
        layout_copy_fields(&g_sys_config, &s_layout_archives[target_id]);
        g_sys_config.theme_mode = (uint8_t)theme;
        g_sys_config.layout_order = (uint8_t)order;
    }
    else
    {
        layout_apply_direction_defaults(theme, order);
        layout_archive_save_current();
    }
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_theme(void)
{
    g_sys_config.theme_mode = (g_sys_config.theme_mode == THEME_TECH)
                              ? THEME_CLASSIC
                              : THEME_TECH;
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_dots_position(void)
{
    static const uint8_t seq[] = { DOTS_RIGHT, DOTS_LEFT, DOTS_BOTTOM, DOTS_NONE };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.dots_position = seq[idx];
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_compass_style(void)
{
    static const uint8_t seq[] = { COMPASS_CLASSIC, COMPASS_AERO, COMPASS_SUB };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.compass_style = seq[idx];
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_sep_style(void)
{
    static const uint8_t seq[] = { SEP_NONE, SEP_SOLID, SEP_DASHED, SEP_DOTTED };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.sep_style = seq[idx];
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_flash_speed(void)
{
    g_sys_config.flash_speed = (g_sys_config.flash_speed + 1) % 3;
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_mask(void)
{
    g_sys_config.mask_enabled = !g_sys_config.mask_enabled;
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_toggle_split_outward(void)
{
    g_sys_config.split_outward = !g_sys_config.split_outward;
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

void bus_set_ui_offset_no_dirty(int16_t offset_x, int16_t offset_y)
{
    if (g_sys_config.offset_x == offset_x && g_sys_config.offset_y == offset_y)
    {
        return;
    }

    g_sys_config.offset_x = offset_x;
    g_sys_config.offset_y = offset_y;
}

void bus_set_ui_offset(int16_t offset_x, int16_t offset_y)
{
    int16_t old_offset_x = g_sys_config.offset_x;
    int16_t old_offset_y = g_sys_config.offset_y;

    bus_set_ui_offset_no_dirty(offset_x, offset_y);
    if (old_offset_x == g_sys_config.offset_x && old_offset_y == g_sys_config.offset_y)
    {
        return;
    }
    bus_mark_dirty(DIRTY_UI_LAYOUT);
}

/* =========================================================
 * 减压状态综合更新接口（原子操作）
 *
 * 算法层每个周期调用一次，一次性更新所有减压相关数据：
 *   - NDL 免减压时间
 *   - 停留状态机类型
 *   - 停留深度/时间参数
 *   - 停留区域标志
 *
 * 相比分离调用，本接口的优势：
 *   1. 原子更新，避免 UI 任务读到中间状态
 *   2. 一次临界区保护，减少中断关闭时间
 *   3. 合并减压状态刷新域脏标记
 * ========================================================= */
void bus_update_deco(int16_t ndl_min, stop_type_t stop_type,
                          float depth_m, uint16_t total_time_s,
                          uint16_t time_s, bool in_stop_zone)
{
    /* NDL 和停留快照共用同一个减压状态刷新域。 */
    dirty_mask_t new_dirty = DIRTY_DECO_STATUS;

    /* 计算是否需要更新 */
    bool ndl_changed  = (g_sensor_data.ndl != ndl_min);
    bool stop_changed = (g_sensor_data.stop_type != stop_type ||
                         g_sensor_data.stop_depth_m != depth_m ||
                         g_sensor_data.stop_time_total_s != total_time_s ||
                         g_sensor_data.stop_time_left_s != time_s ||
                         g_sensor_data.in_stop_zone != in_stop_zone);

    if (!ndl_changed && !stop_changed)
    {
        return;  /* 无变化，快速返回 */
    }

    /* 临界区保护：一次性更新所有字段 */
    /* 这里做原子批量更新，是因为 stop_type / stop_depth / stop_time / in_stop_zone
     * 在 UI 看来属于同一份“减压停留快照”。
     * 如果拆成多次写入，UI 定时任务可能会读到半新半旧的组合状态。 */
    rt_base_t level = rt_hw_interrupt_disable();

    if (ndl_changed)
    {
        g_sensor_data.ndl = ndl_min;
    }

    if (stop_changed)
    {
        g_sensor_data.stop_type = stop_type;
        g_sensor_data.stop_depth_m = depth_m;
        g_sensor_data.stop_time_total_s = total_time_s;
        g_sensor_data.stop_time_left_s = time_s;
        g_sensor_data.in_stop_zone = in_stop_zone;
        new_dirty |= DIRTY_DECO_STATUS;
    }

    g_sensor_data.dirty_mask |= new_dirty;

    rt_hw_interrupt_enable(level);

}

void bus_set_ndl_bar_pct(uint8_t pct)
{
    if (pct > 100U)
    {
        pct = 100U;
    }
    if (g_sensor_data.ndl_bar_pct != pct)
    {
        g_sensor_data.ndl_bar_pct = pct;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

/* GF Low/High 设定值同步接口 */
void bus_set_gf_setting(uint8_t gf_low, uint8_t gf_high)
{
    if (gf_low > 100U) gf_low = 100U;
    if (gf_high > 100U) gf_high = 100U;

    if (g_sensor_data.gf_low != gf_low || g_sensor_data.gf_high != gf_high)
    {
        g_sensor_data.gf_low = gf_low;
        g_sensor_data.gf_high = gf_high;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
    g_sys_config.conservatism = ui_conservatism_from_gf(gf_low, gf_high);
    bus_apply_algo_gf(gf_low, gf_high);
}

void bus_set_conservatism(uint8_t level)
{
    static const uint8_t gf_table[][2] =
    {
        { 40U, 95U },
        { 40U, 85U },
        { 30U, 70U },
        { 50U, 70U },
    };

    if (level >= (sizeof(gf_table) / sizeof(gf_table[0])))
    {
        level = 0U;
    }

    bus_set_gf_setting(gf_table[level][0], gf_table[level][1]);
}

void bus_set_mod_ppo2(float ppo2)
{
    if (g_sys_config.mod_ppo2 != ppo2)
    {
        g_sys_config.mod_ppo2 = ppo2;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
}

void bus_set_last_deco_stop(uint8_t depth_m)
{
    depth_m = (depth_m == 6U) ? 6U : 3U;
    if (g_sys_config.last_deco_stop_m != depth_m)
    {
        g_sys_config.last_deco_stop_m = depth_m;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
    bus_apply_algo_last_deco(depth_m);
}

void bus_set_brightness(uint8_t level)
{
    if (g_sys_config.brightness != level)
    {
        g_sys_config.brightness = level;
    }
}

void bus_set_log_rate(uint8_t seconds)
{
    if (ui_log_rate_is_valid(seconds))
    {
        g_sys_config.log_rate_s = seconds;
        return;
    }

    g_sys_config.log_rate_s = UI_LOG_RATE_DEFAULT_S;
}

void bus_set_time_24h_enabled(bool enabled)
{
    uint8_t value = enabled ? 1U : 0U;
    if (g_sys_config.time_24h_enabled != value)
    {
        g_sys_config.time_24h_enabled = value;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_units_mode(uint8_t units)
{
    uint8_t value = (units == UI_UNITS_IMPERIAL) ? UI_UNITS_IMPERIAL : UI_UNITS_METRIC;
    if (g_sys_config.units_mode != value)
    {
        g_sys_config.units_mode = value;
    }
    /* 睡眠恢复时单位可能未变化，但重建后的组件仍需要按 m/ft 重新格式化。 */
    bus_requeue_dirty_immediate(DIRTY_SYSTEM | DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS |
                                DIRTY_GAS_SUPPLY | DIRTY_PLAN | DIRTY_LOGBOOK);
}

void bus_set_date_format(uint8_t format)
{
    uint8_t value = (format == 0U) ? 0U : 1U;
    if (g_sys_config.date_format != value)
    {
        g_sys_config.date_format = value;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_temperature_unit(uint8_t unit)
{
    uint8_t value = (unit == UI_TEMP_UNIT_F) ? UI_TEMP_UNIT_F : UI_TEMP_UNIT_C;
    if (g_sys_config.temperature_unit != value)
    {
        g_sys_config.temperature_unit = value;
        bus_mark_dirty(DIRTY_SYSTEM);
    }
}

void bus_set_safety_stop_mode(uint8_t mode)
{
    if (g_sys_config.safety_stop_mode != mode)
    {
        g_sys_config.safety_stop_mode = mode;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
    bus_apply_algo_safety_stop(mode);
}

void bus_set_altitude_level(uint8_t level)
{
    if (g_sys_config.altitude_level != level)
    {
        g_sys_config.altitude_level = level;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
}

void bus_set_depth_alarm_m(uint16_t depth_m)
{
    if (g_sys_config.depth_alarm_m != depth_m)
    {
        g_sys_config.depth_alarm_m = depth_m;
    }
}

void bus_set_time_alarm_min(uint16_t minutes)
{
    if (g_sys_config.time_alarm_min != minutes)
    {
        g_sys_config.time_alarm_min = minutes;
    }
}

void bus_set_ndl_alarm_min(uint16_t minutes)
{
    if (g_sys_config.ndl_alarm_min != minutes)
    {
        g_sys_config.ndl_alarm_min = minutes;
    }
}

void bus_set_salinity_mode(uint8_t mode)
{
    if (mode > 2U) mode = 0U;
    if (g_sys_config.salinity_mode != mode)
    {
        g_sys_config.salinity_mode = mode;
        bus_mark_dirty(DIRTY_DIVE_CONFIG);
    }
    bus_apply_algo_salinity(mode);
}

/* MOD（最大操作深度）同步接口 */
void bus_set_mod(float mod_m)
{
    if (g_sensor_data.mod_m != mod_m)
    {
        g_sensor_data.mod_m = mod_m;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

/* Ceiling（减压上限）同步接口 */
void bus_set_ceiling(float ceiling_m)
{
    if (g_sensor_data.ceiling_m != ceiling_m)
    {
        g_sensor_data.ceiling_m = ceiling_m;
        bus_mark_dirty(DIRTY_DECO_STATUS);
    }
}

/* 气体混合比（O2/He）同步接口 */
void bus_set_gas_mix(uint8_t o2_pct, uint8_t he_pct)
{
    if (g_sensor_data.gas_o2_pct != o2_pct || g_sensor_data.gas_he_pct != he_pct)
    {
        g_sensor_data.gas_o2_pct = o2_pct;
        g_sensor_data.gas_he_pct = he_pct;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

/* 气体密度同步接口 */
void bus_set_gas_density(float density)
{
    if (g_sensor_data.gas_density != density)
    {
        g_sensor_data.gas_density = density;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

/* FiO2（实际吸入氧浓度）同步接口 */
void bus_set_fio2(float fio2_pct)
{
    if (g_sensor_data.fio2_pct != fio2_pct)
    {
        g_sensor_data.fio2_pct = fio2_pct;
        bus_mark_dirty(DIRTY_GAS_SUPPLY);
    }
}

uint16_t ui_safe_zone_w_get(void)
{
    return g_sys_config.safe_zone_w;
}

uint16_t ui_safe_zone_h_get(void)
{
    return g_sys_config.safe_zone_h;
}

int16_t ui_offset_x_get(void)
{
    return g_sys_config.offset_x;
}

int16_t ui_offset_y_get(void)
{
    return g_sys_config.offset_y;
}

bool ui_mask_enabled_get(void)
{
    return g_sys_config.mask_enabled;
}

uint16_t ui_block_gap_px_get(void)
{
    return (uint16_t)(g_sys_config.gap_u * BASE_U);
}

uint16_t ui_panel_gap_px_get(void)
{
    return (uint16_t)(g_sys_config.panel_gap_u * BASE_U);
}

uint16_t ui_menu_gap_px_get(void)
{
    return (uint16_t)(g_sys_config.gap_menu * BASE_U);
}

uint16_t ui_menu_item_h_px_get(void)
{
    return (uint16_t)(g_sys_config.h_menu_item * BASE_U);
}

uint16_t ui_tissues_chart_h_px_get(void)
{
    return (uint16_t)(g_sys_config.h_tissues_chart * BASE_U);
}

theme_t ui_theme_mode_get(void)
{
    return (g_sys_config.theme_mode == THEME_CLASSIC) ? THEME_CLASSIC : THEME_TECH;
}

order_t ui_layout_order_get(void)
{
    return g_sys_config.layout_order;
}

bool ui_layout_is_vertical_split(void)
{
    return ui_theme_mode_get() == THEME_TECH;
}

uint8_t ui_fixed_grid_cols_get(void)
{
    return ui_layout_is_vertical_split() ? FIXED_SIDE_COLS : FIXED_TOP_COLS;
}

uint8_t ui_fixed_grid_rows_get(void)
{
    return ui_layout_is_vertical_split() ? FIXED_SIDE_ROWS : FIXED_TOP_ROWS;
}

uint8_t ui_custom_grid_cols_get(void)
{
    return ui_layout_is_vertical_split() ? CUSTOM_SIDE_COLS : CUSTOM_TOP_COLS;
}

uint8_t ui_custom_grid_rows_get(void)
{
    return ui_layout_is_vertical_split() ? CUSTOM_SIDE_ROWS : CUSTOM_TOP_ROWS;
}

uint16_t ui_anchor_w_get(void)
{
    return ui_layout_is_vertical_split() ? LEFT_ANCHOR_W : ui_safe_zone_w_get();
}

uint16_t ui_anchor_h_get(void)
{
    return ui_layout_is_vertical_split() ? ui_safe_zone_h_get() : TOP_ANCHOR_H;
}

uint16_t ui_content_w_get(void)
{
    uint16_t gap = ui_panel_gap_px_get();

    if (ui_layout_is_vertical_split())
    {
        return (ui_safe_zone_w_get() > LEFT_ANCHOR_W + gap)
               ? (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W - gap)
               : 0U;
    }

    return ui_safe_zone_w_get();
}

uint16_t ui_content_h_get(void)
{
    if (ui_layout_is_vertical_split())
    {
        return ui_safe_zone_h_get();
    }

    return (ui_safe_zone_h_get() > TOP_ANCHOR_H)
           ? (uint16_t)(ui_safe_zone_h_get() - TOP_ANCHOR_H)
           : 0U;
}

uint8_t ui_dots_position_get(void)
{
    return g_sys_config.dots_position;
}

uint8_t ui_depth_h_u_get(void)
{
    return g_sys_config.h_depth;
}

uint8_t ui_ndl_h_u_get(void)
{
    return g_sys_config.h_ndl;
}

uint8_t ui_pod_h_u_get(void)
{
    return g_sys_config.h_pod;
}

uint8_t ui_batt_h_u_get(void)
{
    return g_sys_config.h_batt;
}

uint8_t ui_gas_h_u_get(void)
{
    return g_sys_config.h_gas;
}

uint8_t ui_time_h_u_get(void)
{
    return g_sys_config.h_time;
}

uint8_t ui_left_widget_count_get(void)
{
    return g_sys_config.left_widget_count;
}

const grid_widget_t *ui_left_widget_get(uint8_t index)
{
    return (index < LEFT_MAX_WIDGETS) ? &g_sys_config.left_widgets[index] : NULL;
}

uint8_t ui_custom_card_count_get(void)
{
    return g_sys_config.custom_card_count;
}

const char *ui_custom_card_title_get(uint8_t custom_card_idx)
{
    if (custom_card_idx >= MAX_CUSTOM_CARDS) {
        return "CUSTOM WIDGETS";
    }

    if (g_sys_config.custom_cards[custom_card_idx].title[0] == '\0') {
        return "CUSTOM WIDGETS";
    }

    return g_sys_config.custom_cards[custom_card_idx].title;
}

uint8_t ui_custom_card_widget_count_get(uint8_t custom_card_idx)
{
    if (custom_card_idx >= MAX_CUSTOM_CARDS)
    {
        return 0U;
    }
    return g_sys_config.custom_cards[custom_card_idx].widget_count;
}

const grid_widget_t *ui_custom_card_widget_get(uint8_t custom_card_idx, uint8_t widget_idx)
{
    if ((custom_card_idx >= MAX_CUSTOM_CARDS) || (widget_idx >= MAX_5F_WIDGETS))
    {
        return NULL;
    }
    return &g_sys_config.custom_cards[custom_card_idx].widgets[widget_idx];
}

uint8_t ui_custom_card_slot_get(uint8_t storage_pos)
{
    if (storage_pos >= PAGE_COUNT)
    {
        return 0xFFU;
    }
    return g_sys_config.custom_card_slot[storage_pos];
}

float bus_get_depth(void)
{
    return g_sensor_data.depth;
}

float bus_get_stop_depth_m(void)
{
    return g_sensor_data.stop_depth_m;
}

stop_type_t bus_get_stop_type(void)
{
    return g_sensor_data.stop_type;
}

uint8_t bus_get_ndl_bar_pct(void)
{
    return g_sensor_data.ndl_bar_pct;
}

uint16_t bus_get_stop_time_total_s(void)
{
    return g_sensor_data.stop_time_total_s;
}

uint16_t bus_get_stop_time_left_s(void)
{
    return g_sensor_data.stop_time_left_s;
}

bool bus_get_in_stop_zone(void)
{
    return g_sensor_data.in_stop_zone;
}

int16_t bus_get_ndl(void)
{
    return g_sensor_data.ndl;
}

int16_t bus_get_ndl_stop_value(void)
{
    return g_sensor_data.ndl_stop_value;
}

float bus_get_max_depth(void)
{
    return g_sensor_data.max_depth;
}

float bus_get_avg_depth(void)
{
    return g_sensor_data.avg_depth;
}

uint32_t bus_get_dive_time_s(void)
{
    return g_sensor_data.dive_time_s;
}

uint32_t bus_get_surface_time_s(void)
{
    return g_sensor_data.surface_time_s;
}

dive_lifecycle_phase_t bus_get_dive_lifecycle_phase(void)
{
    return g_sensor_data.dive_lifecycle_phase;
}

float bus_get_battery_pct(void)
{
    return g_sensor_data.battery_pct;
}

bool bus_get_low_power_shutdown_active(void)
{
    return g_sensor_data.low_power_shutdown_active;
}

uint8_t bus_get_low_power_shutdown_remaining_sec(void)
{
    return g_sensor_data.low_power_shutdown_remaining_sec;
}

float bus_get_pod1_bar(void)
{
    return g_sensor_data.pod1_bar;
}

float bus_get_pod2_bar(void)
{
    return g_sensor_data.pod2_bar;
}

bool bus_get_pod1_valid(void)
{
    return g_sensor_data.pod1_valid;
}

bool bus_get_pod2_valid(void)
{
    return g_sensor_data.pod2_valid;
}

float bus_get_temperature(void)
{
    return g_sensor_data.temperature_c;
}

float bus_get_min_temp(void)
{
    return g_sensor_data.min_temp;
}

float bus_get_avg_temp(void)
{
    return g_sensor_data.avg_temp;
}

float bus_get_max_temp(void)
{
    return g_sensor_data.max_temp;
}

float bus_get_bat_temperature(void)
{
    return g_sensor_data.bat_temperature_c;
}

float bus_get_prj_temperature(void)
{
    return g_sensor_data.prj_temperature_c;
}

float bus_get_battery_voltage(void)
{
    return g_sensor_data.battery_voltage_v;
}

uint8_t bus_get_charge_state(void)
{
    return g_sensor_data.charge_state;
}

float bus_get_ambient_pressure(void)
{
    return g_sensor_data.ambient_pressure_mbar;
}

uint16_t bus_get_nofly_time_min(void)
{
    return g_sensor_data.nofly_time_min;
}

float bus_get_gyro_x_dps(void)
{
    return g_sensor_data.gyro_x_dps;
}

float bus_get_gyro_y_dps(void)
{
    return g_sensor_data.gyro_y_dps;
}

float bus_get_gyro_z_dps(void)
{
    return g_sensor_data.gyro_z_dps;
}

float bus_get_accel_x_g(void)
{
    return g_sensor_data.accel_x_g;
}

float bus_get_accel_y_g(void)
{
    return g_sensor_data.accel_y_g;
}

float bus_get_accel_z_g(void)
{
    return g_sensor_data.accel_z_g;
}

float bus_get_mag_x_ut(void)
{
    return g_sensor_data.mag_x_ut;
}

float bus_get_mag_y_ut(void)
{
    return g_sensor_data.mag_y_ut;
}

float bus_get_mag_z_ut(void)
{
    return g_sensor_data.mag_z_ut;
}

float bus_get_mlx_x_ut(void)
{
    return g_sensor_data.mlx_x_ut;
}

float bus_get_mlx_y_ut(void)
{
    return g_sensor_data.mlx_y_ut;
}

float bus_get_mlx_z_ut(void)
{
    return g_sensor_data.mlx_z_ut;
}

float bus_get_tmag_x_ut(void)
{
    return g_sensor_data.tmag_x_ut;
}

float bus_get_tmag_y_ut(void)
{
    return g_sensor_data.tmag_y_ut;
}

float bus_get_tmag_z_ut(void)
{
    return g_sensor_data.tmag_z_ut;
}

float bus_get_tmag_ut(void)
{
    return g_sensor_data.tmag_ut;
}

int16_t bus_get_pitch_deg(void)
{
    return g_sensor_data.pitch_deg;
}

int16_t bus_get_roll_deg(void)
{
    return g_sensor_data.roll_deg;
}

uint16_t bus_get_attitude_heading_deg(void)
{
    return g_sensor_data.attitude_heading_deg;
}

int16_t bus_get_ble_rssi_dbm(void)
{
    return g_sensor_data.ble_rssi_dbm;
}

uint8_t bus_get_cpu_load_pct(void)
{
    return g_sensor_data.cpu_load_pct;
}

uint16_t bus_get_fps(void)
{
    return g_sensor_data.fps;
}

const char *bus_get_sensor_status(void)
{
    return g_sensor_data.sensor_status[0] ? g_sensor_data.sensor_status : "--";
}

float bus_get_ascent_rate(void)
{
    return g_sensor_data.ascent_rate;
}

uint16_t bus_get_sys_time_h(void)
{
    return g_sensor_data.sys_time_h;
}

uint16_t bus_get_sys_time_m(void)
{
    return g_sensor_data.sys_time_m;
}

uint16_t bus_get_sys_time_s(void)
{
    return g_sensor_data.sys_time_s;
}

uint8_t bus_get_gas_slot_count(void)
{
    uint8_t count = g_sensor_data.gas_slot_count;
    return (count > GAS_COUNT) ? GAS_COUNT : count;
}

uint8_t bus_get_gas_active_idx(void)
{
    uint8_t count = bus_get_gas_slot_count();
    uint8_t idx = g_sensor_data.gas_active_idx;

    if ((count == 0U) || (idx >= count))
    {
        return 0U;
    }

    return idx;
}

int8_t bus_get_recommended_gas_idx(void)
{
    int8_t idx = g_sensor_data.gas_recommended_idx;
    uint8_t count = bus_get_gas_slot_count();

    if (idx < 0 || count == 0U || idx >= (int8_t)count || idx >= (int8_t)GAS_COUNT ||
        idx == (int8_t)bus_get_gas_active_idx())
    {
        return -1;
    }

    return idx;
}

const char *bus_get_gas_slot_name(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return NULL;
    }

    if (g_sensor_data.gas_slot_name[gas_idx][0] != '\0')
    {
        return g_sensor_data.gas_slot_name[gas_idx];
    }

    return GAS_NAMES[gas_idx];
}

uint8_t bus_get_gas_slot_o2_pct(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return 0U;
    }

    return g_sensor_data.gas_slot_o2_pct[gas_idx];
}

uint8_t bus_get_gas_slot_he_pct(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return 0U;
    }

    return g_sensor_data.gas_slot_he_pct[gas_idx];
}

float bus_get_gas_slot_mod_m(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return 0.0f;
    }

    return g_sensor_data.gas_slot_mod_m[gas_idx];
}

float bus_get_gas_slot_max_ppo2(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return 0.0f;
    }

    return g_sensor_data.gas_slot_max_ppo2[gas_idx];
}

float bus_get_gas_slot_ppo2(uint8_t gas_idx)
{
    if (gas_idx >= GAS_COUNT)
    {
        return 0.0f;
    }

    return g_sensor_data.ppo2[gas_idx];
}

uint8_t bus_get_gas_mix_o2(void)
{
    return g_sensor_data.gas_o2_pct;
}

uint8_t bus_get_gas_mix_he(void)
{
    return g_sensor_data.gas_he_pct;
}

float bus_get_gas_density(void)
{
    return g_sensor_data.gas_density;
}

float bus_get_mod_m(void)
{
    return g_sensor_data.mod_m;
}

float bus_get_ceiling_m(void)
{
    return g_sensor_data.ceiling_m;
}

float bus_get_mod_ppo2(void)
{
    return g_sys_config.mod_ppo2;
}

float bus_get_fio2_pct(void)
{
    return g_sensor_data.fio2_pct;
}

uint8_t bus_get_gf_low(void)
{
    return g_sensor_data.gf_low;
}

uint8_t bus_get_gf_high(void)
{
    return g_sensor_data.gf_high;
}

float bus_get_gf99(void)
{
    return g_sensor_data.gf99;
}

float bus_get_surf_gf(void)
{
    return g_sensor_data.surf_gf;
}

uint8_t bus_get_cns_pct(void)
{
    return g_sensor_data.cns_pct;
}

uint16_t bus_get_otu(void)
{
    return g_sensor_data.otu;
}

int16_t bus_get_tissue_raw_pct(uint8_t index)
{
    if (index >= 16U)
    {
        return 0U;
    }

    return g_sensor_data.tissue_raw_pct[index];
}

uint8_t bus_get_tissue_gf_pct(uint8_t index)
{
    if (index >= 16U)
    {
        return 0U;
    }

    return g_sensor_data.tissue_gf_pct[index];
}

float bus_get_tissue_target_gf_pct(void)
{
    return g_sensor_data.tissue_target_gf_pct;
}

bool bus_get_tissue_normalized_valid(void)
{
    return g_sensor_data.tissue_normalized_valid;
}

uint16_t bus_get_tissue_bar_permille(uint8_t index)
{
    if (index >= 16U)
    {
        return 0U;
    }

    return g_sensor_data.tissue_bar_permille[index];
}

uint16_t bus_get_tissue_pi_permille(void)
{
    return g_sensor_data.tissue_pi_permille;
}

float bus_get_tissue_ambient_pressure_bar(void)
{
    return g_sensor_data.tissue_ambient_pressure_bar;
}

float bus_get_tissue_inspired_n2_bar(void)
{
    return g_sensor_data.tissue_inspired_n2_bar;
}

float bus_get_tissue_inspired_he_bar(void)
{
    return g_sensor_data.tissue_inspired_he_bar;
}

float bus_get_tissue_n2_bar(uint8_t index)
{
    if (index >= 16U)
    {
        return 0.0f;
    }

    return g_sensor_data.tissue_n2_bar[index];
}

float bus_get_tissue_he_bar(uint8_t index)
{
    if (index >= 16U)
    {
        return 0.0f;
    }

    return g_sensor_data.tissue_he_bar[index];
}

float bus_get_tissue_m_value_bar(uint8_t index)
{
    if (index >= 16U)
    {
        return 0.0f;
    }

    return g_sensor_data.tissue_m_value_bar[index];
}

float bus_get_tissue_m_gf_bar(uint8_t index)
{
    if (index >= 16U)
    {
        return 0.0f;
    }

    return g_sensor_data.tissue_m_gf_bar[index];
}

uint8_t bus_get_pod_count(void)
{
    return g_sensor_data.gas_slot_count;
}

bool bus_get_pod_valid(uint8_t pod_idx)
{
    return (pod_idx == 0U) ? g_sensor_data.pod1_valid : g_sensor_data.pod2_valid;
}

float bus_get_pod_bar(uint8_t pod_idx)
{
    return (pod_idx == 0U) ? g_sensor_data.pod1_bar : g_sensor_data.pod2_bar;
}

float bus_get_tts(void)
{
    return (float)g_sensor_data.tts;
}

bool bus_get_tts_at_5min(uint16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.tts_at_5min_min;
    return g_sensor_data.tts_at_5min_valid;
}

bool bus_get_tts_delta_5min(int16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.tts_delta_5min_min;
    return g_sensor_data.tts_delta_5min_valid;
}

bool bus_get_ndl_up_3m(int16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.ndl_up_3m_min;
    return g_sensor_data.ndl_up_3m_valid;
}

bool bus_get_ndl_down_3m(int16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.ndl_down_3m_min;
    return g_sensor_data.ndl_down_3m_valid;
}

bool bus_get_ndl_delta_3m(int16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.ndl_delta_3m_min;
    return g_sensor_data.ndl_delta_3m_valid;
}

bool bus_get_gtr(uint16_t *out_min)
{
    if (out_min != NULL) *out_min = g_sensor_data.gtr_min;
    return g_sensor_data.gtr_valid;
}

bool bus_get_rmv(float *out_lpm)
{
    if (out_lpm != NULL) *out_lpm = g_sensor_data.rmv_lpm;
    return g_sensor_data.rmv_valid;
}

float bus_get_sac_rate(void)
{
    return g_sensor_data.sac_rate;
}

bool bus_get_sac_rate_valid(void)
{
    return g_sensor_data.sac_valid;
}

uint8_t bus_get_last_deco_stop(void)
{
    return g_sys_config.last_deco_stop_m;
}

uint8_t bus_get_salinity_mode(void)
{
    return g_sys_config.salinity_mode;
}

uint8_t bus_get_conservatism(void)
{
    return g_sys_config.conservatism;
}

uint8_t bus_get_brightness(void)
{
    return g_sys_config.brightness;
}

uint8_t bus_get_log_rate(void)
{
    if (ui_log_rate_is_valid(g_sys_config.log_rate_s))
    {
        return g_sys_config.log_rate_s;
    }
    return UI_LOG_RATE_DEFAULT_S;
}

bool bus_get_time_24h_enabled(void)
{
    return g_sys_config.time_24h_enabled != 0U;
}

uint8_t bus_get_units_mode(void)
{
    return (g_sys_config.units_mode == UI_UNITS_IMPERIAL) ? UI_UNITS_IMPERIAL : UI_UNITS_METRIC;
}

const char *bus_get_depth_unit_label(void)
{
    return ui_depth_unit_label(bus_get_units_mode());
}

const char *bus_get_depth_units_label(void)
{
    return ui_depth_units_label(bus_get_units_mode());
}

float bus_get_depth_display(float depth_m)
{
    return ui_depth_display_from_m(depth_m, bus_get_units_mode());
}

uint8_t bus_get_date_format(void)
{
    return (g_sys_config.date_format == 0U) ? 0U : 1U;
}

uint8_t bus_get_temperature_unit(void)
{
    return (g_sys_config.temperature_unit == UI_TEMP_UNIT_F) ? UI_TEMP_UNIT_F : UI_TEMP_UNIT_C;
}

const char *bus_get_temperature_unit_label(void)
{
    return ui_temp_unit_label(bus_get_temperature_unit());
}

float bus_get_temperature_display(float temp_c)
{
    return ui_temp_display_from_c(temp_c, bus_get_temperature_unit());
}

uint8_t bus_get_safety_stop_mode(void)
{
    return g_sys_config.safety_stop_mode;
}

uint8_t bus_get_altitude_level(void)
{
    return g_sys_config.altitude_level;
}

uint16_t bus_get_depth_alarm_m(void)
{
    return g_sys_config.depth_alarm_m;
}

uint16_t bus_get_time_alarm_min(void)
{
    return g_sys_config.time_alarm_min;
}

uint16_t bus_get_ndl_alarm_min(void)
{
    return g_sys_config.ndl_alarm_min;
}

uint8_t bus_get_dive_log_count(void)
{
    if (s_dive_log_count > MAX_DIVE_LOG)
    {
        return (uint8_t)MAX_DIVE_LOG;
    }

    return (uint8_t)s_dive_log_count;
}

bool bus_get_dive_log_point(uint8_t index, dive_pt_t *out_point)
{
    if ((out_point == NULL) || (index >= s_dive_log_count))
    {
        return false;
    }

    *out_point = s_dive_log[index];
    return true;
}

uint8_t bus_get_deco_stop_count(void)
{
    if (s_deco_stop_count > MAX_DECO_STOPS)
    {
        return (uint8_t)MAX_DECO_STOPS;
    }

    return (uint8_t)s_deco_stop_count;
}

bool bus_get_deco_stop(uint8_t index, deco_stop_t *out_stop)
{
    if ((out_stop == NULL) || (index >= s_deco_stop_count))
    {
        return false;
    }

    *out_stop = s_deco_stops[index];
    return true;
}

bool bus_is_heading_locked(void)
{
    return g_sensor_data.heading_locked;
}

uint16_t bus_get_heading(void)
{
    return g_sensor_data.heading;
}

uint16_t bus_get_heading_target(void)
{
    return g_sensor_data.heading_target;
}

void bus_lock_heading_to_current(void)
{
    if (!g_sensor_data.heading_locked)
    {
        g_sensor_data.heading_locked = true;
        g_sensor_data.heading_target = g_sensor_data.heading;
        bus_mark_dirty(DIRTY_COMPASS);
    }
}

void bus_clear_heading_lock(void)
{
    if (g_sensor_data.heading_locked)
    {
        g_sensor_data.heading_locked = false;
        bus_mark_dirty(DIRTY_COMPASS);
    }
}

void dive_log_append(float current_time_s, float current_depth_m)
{
    if (current_time_s < 0.0f)
    {
        return;
    }

    if (s_dive_log_count > 0U)
    {
        dive_pt_t *last = &s_dive_log[s_dive_log_count - 1U];

        if (current_time_s < last->time_s)
        {
            return;
        }

        if (fabsf(current_time_s - last->time_s) < 0.001f)
        {
            if (fabsf(last->depth_m - current_depth_m) < 0.001f)
            {
                last->depth_m = current_depth_m;
                return;
            }

            if (s_dive_log_count >= MAX_DIVE_LOG)
            {
                dive_log_make_room();
            }

            if (s_dive_log_count < MAX_DIVE_LOG)
            {
                s_dive_log[s_dive_log_count].time_s  = current_time_s;
                s_dive_log[s_dive_log_count].depth_m = current_depth_m;
                s_dive_log_count++;
                bus_mark_dirty(DIRTY_PLAN);
            }
            return;
        }
    }

    if (s_dive_log_count >= MAX_DIVE_LOG)
    {
        dive_log_make_room();
    }

    if (s_dive_log_count < MAX_DIVE_LOG)
    {
        s_dive_log[s_dive_log_count].time_s  = current_time_s;
        s_dive_log[s_dive_log_count].depth_m = current_depth_m;
        s_dive_log_count++;
        bus_mark_dirty(DIRTY_PLAN);
    }
}

void dive_log_append_sampled(float current_time_s, float current_depth_m)
{
    float log_rate_s = (float)bus_get_log_rate();

    if (current_time_s < 0.0f)
    {
        return;
    }

    if (s_dive_log_sample_valid)
    {
        if (current_time_s < s_dive_log_last_sample_time_s)
        {
            return;
        }

        if ((current_time_s - s_dive_log_last_sample_time_s) < log_rate_s)
        {
            return;
        }
    }

    dive_log_append(current_time_s, current_depth_m);
    s_dive_log_last_sample_time_s = current_time_s;
    s_dive_log_sample_valid = true;
}

void dive_log_reset(void)
{
    s_dive_log_count = 0U;
    s_dive_log_sample_valid = false;
    s_dive_log_last_sample_time_s = 0.0f;
    s_deco_stop_count = 0U;
}

static bool last_dive_snapshot_load_latest(logbook_entry_t *out_entry)
{
    uint16_t count;
    logbook_entry_t latest;

    if (out_entry == NULL)
    {
        return false;
    }

    count = logbook_backend_count();
    if (count == 0U)
    {
        if (s_last_dive_snapshot.valid && s_last_dive_snapshot_source_count == 0U)
        {
            *out_entry = s_last_dive_snapshot;
            return true;
        }

        (void)memset(&s_last_dive_snapshot, 0, sizeof(s_last_dive_snapshot));
        s_last_dive_snapshot_source_count = 0U;
        return false;
    }

    if (s_last_dive_snapshot.valid && s_last_dive_snapshot_source_count == count)
    {
        *out_entry = s_last_dive_snapshot;
        return true;
    }

    (void)memset(&latest, 0, sizeof(latest));
    if (!logbook_backend_get_summary((uint16_t)(count - 1U), &latest) || !latest.valid)
    {
        if (s_last_dive_snapshot.valid)
        {
            *out_entry = s_last_dive_snapshot;
            return true;
        }
        return false;
    }

    s_last_dive_snapshot = latest;
    s_last_dive_snapshot.valid = true;
    s_last_dive_snapshot_source_count = count;
    *out_entry = s_last_dive_snapshot;
    return true;
}

#ifdef PC_SIMULATOR
uint16_t logbook_backend_count(void)
{
    return s_logbook_count;
}

bool logbook_backend_get_summary(uint16_t index, logbook_entry_t *out_entry)
{
    if ((out_entry == NULL) || (index >= s_logbook_count))
    {
        return false;
    }

    *out_entry = s_logbook_entries[index];
    return out_entry->valid;
}

bool logbook_backend_get_detail(uint16_t index, logbook_entry_t *out_entry)
{
    return logbook_backend_get_summary(index, out_entry);
}

bool logbook_backend_get_samples(uint16_t index, dive_pt_t *out_points, uint16_t max_points, uint16_t *out_count)
{
    uint16_t count;

    if (out_count)
    {
        *out_count = 0U;
    }
    if (index >= s_logbook_count)
    {
        return false;
    }

    count = s_logbook_sample_counts[index];
    if (count > max_points)
    {
        count = max_points;
    }
    if ((out_points != NULL) && (count > 0U))
    {
        (void)memcpy(out_points, s_logbook_samples[index], count * sizeof(out_points[0]));
    }
    if (out_count)
    {
        *out_count = count;
    }
    return true;
}

bool logbook_backend_acquire_samples(uint16_t index, const dive_pt_t **out_points, uint16_t *out_count)
{
    dive_pt_t *points;

    if (out_points == NULL || out_count == NULL)
    {
        return false;
    }

    *out_points = NULL;
    *out_count = 0U;
    points = (dive_pt_t *)malloc(LOGBOOK_SAMPLE_BUFFER_BYTES);
    if (points == NULL)
    {
        return false;
    }

    if (!logbook_backend_get_samples(index, points, MAX_DIVE_LOG, out_count))
    {
        free(points);
        return false;
    }

    *out_points = points;
    return true;
}

void logbook_backend_release_samples(const dive_pt_t *points)
{
    free((void *)points);
}

bool logbook_backend_update_meta(uint16_t index, const logbook_meta_t *meta)
{
    if ((meta == NULL) || (index >= s_logbook_count))
    {
        return false;
    }

    s_logbook_entries[index].meta = *meta;
    if (s_last_dive_snapshot.valid && (index + 1U == s_logbook_count))
    {
        s_last_dive_snapshot.meta = *meta;
        s_last_dive_snapshot_source_count = s_logbook_count;
    }
    bus_mark_dirty(DIRTY_LOGBOOK);
    return true;
}

bool logbook_backend_delete(uint16_t index)
{
    if (index >= s_logbook_count)
    {
        return false;
    }

    if (index + 1U < s_logbook_count)
    {
        uint16_t tail_count = (uint16_t)(s_logbook_count - index - 1U);
        (void)memmove(&s_logbook_entries[index], &s_logbook_entries[index + 1U], tail_count * sizeof(s_logbook_entries[0]));
        (void)memmove(&s_logbook_sample_counts[index], &s_logbook_sample_counts[index + 1U], tail_count * sizeof(s_logbook_sample_counts[0]));
        (void)memmove(&s_logbook_samples[index], &s_logbook_samples[index + 1U], tail_count * sizeof(s_logbook_samples[0]));
    }
    s_logbook_count--;

    if (s_logbook_count > 0U)
    {
        s_last_dive_snapshot = s_logbook_entries[s_logbook_count - 1U];
        s_last_dive_snapshot_source_count = s_logbook_count;
    }
    else
    {
        (void)memset(&s_last_dive_snapshot, 0, sizeof(s_last_dive_snapshot));
        s_last_dive_snapshot_source_count = 0U;
    }
    bus_mark_dirty(DIRTY_LOGBOOK);
    return true;
}

bool logbook_backend_append_finalized_dive(const logbook_entry_t *entry, const dive_pt_t *points, uint16_t point_count)
{
    uint16_t index;

    if (entry == NULL)
    {
        return false;
    }

    if (s_logbook_count >= MAX_LOGBOOK_ENTRIES)
    {
        (void)logbook_backend_delete(0U);
    }

    if (s_logbook_count >= MAX_LOGBOOK_ENTRIES)
    {
        return false;
    }

    index = s_logbook_count++;
    s_logbook_entries[index] = *entry;
    s_logbook_entries[index].valid = true;

    if (point_count > MAX_DIVE_LOG)
    {
        point_count = MAX_DIVE_LOG;
    }
    s_logbook_sample_counts[index] = point_count;
    if ((points != NULL) && (point_count > 0U))
    {
        (void)memcpy(s_logbook_samples[index], points, point_count * sizeof(points[0]));
    }

    s_last_dive_snapshot = s_logbook_entries[index];
    s_last_dive_snapshot_source_count = s_logbook_count;
    bus_mark_dirty(DIRTY_LOGBOOK);
    return true;
}

bool bus_get_last_dive_snapshot(logbook_entry_t *out_entry)
{
    return last_dive_snapshot_load_latest(out_entry);
}
#else
__attribute__((weak))
uint16_t logbook_backend_count(void)
{
    return 0U;
}

__attribute__((weak))
bool logbook_backend_get_summary(uint16_t index, logbook_entry_t *out_entry)
{
    (void)index;
    (void)out_entry;
    return false;
}

__attribute__((weak))
bool logbook_backend_get_detail(uint16_t index, logbook_entry_t *out_entry)
{
    return logbook_backend_get_summary(index, out_entry);
}

__attribute__((weak))
bool logbook_backend_get_samples(uint16_t index, dive_pt_t *out_points, uint16_t max_points, uint16_t *out_count)
{
    (void)index;
    (void)out_points;
    (void)max_points;
    if (out_count)
    {
        *out_count = 0U;
    }
    return false;
}

static bool logbook_backend_heap_ensure(void)
{
    if (s_logbook_backend_heap_ready)
    {
        return true;
    }

    if (rt_memheap_init(&s_logbook_backend_heap,
                        "logbook_backend",
                        (void *)s_logbook_backend_heap_pool,
                        sizeof(s_logbook_backend_heap_pool)) != RT_EOK)
    {
        rt_kprintf("[Logbook] backend heap init failed\n");
        return false;
    }

    s_logbook_backend_heap_ready = true;
    return true;
}

__attribute__((weak))
bool logbook_backend_acquire_samples(uint16_t index, const dive_pt_t **out_points, uint16_t *out_count)
{
    dive_pt_t *points;

    if (out_points == NULL || out_count == NULL)
    {
        return false;
    }

    *out_points = NULL;
    *out_count = 0U;
    if (!logbook_backend_heap_ensure())
    {
        return false;
    }

    points = (dive_pt_t *)rt_memheap_alloc(&s_logbook_backend_heap, LOGBOOK_SAMPLE_BUFFER_BYTES);
    if (points == NULL)
    {
        rt_kprintf("[Logbook] sample buffer alloc failed\n");
        return false;
    }

    if (!logbook_backend_get_samples(index, points, MAX_DIVE_LOG, out_count))
    {
        rt_memheap_free(points);
        return false;
    }

    *out_points = points;
    return true;
}

__attribute__((weak))
void logbook_backend_release_samples(const dive_pt_t *points)
{
    if (points != NULL)
    {
        rt_memheap_free((void *)points);
    }
}

__attribute__((weak))
bool logbook_backend_update_meta(uint16_t index, const logbook_meta_t *meta)
{
    (void)index;
    (void)meta;
    return false;
}

__attribute__((weak))
bool logbook_backend_delete(uint16_t index)
{
    (void)index;
    return false;
}

__attribute__((weak))
bool logbook_backend_append_finalized_dive(const logbook_entry_t *entry, const dive_pt_t *points, uint16_t point_count)
{
    (void)points;
    (void)point_count;
    if (entry == NULL)
    {
        return false;
    }

    s_last_dive_snapshot = *entry;
    s_last_dive_snapshot.valid = true;
    s_last_dive_snapshot_source_count = 0U;
    bus_mark_dirty(DIRTY_LOGBOOK);
    return false;
}

__attribute__((weak))
bool bus_get_last_dive_snapshot(logbook_entry_t *out_entry)
{
    return last_dive_snapshot_load_latest(out_entry);
}
#endif

/* =========================================================
 * Legacy 配置接口兼容占位
 *
 * 最新架构下，UI 配置与用户参数持久化已经收口到：
 * - system bootstrap
 * - ui_layout_runtime_restore / ui_runtime_persistence
 * - user_settings_service / alert_config_service
 *
 * 因此 UI core 不再负责直接读写配置。
 * 这里保留空实现，仅避免历史模拟器/旧代码链接失败。
 * ========================================================= */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //真机需打开，用于覆盖此默认实现
#endif
bool config_load(sys_config_t *cfg)
{
    (void)cfg;
    return false;
}

#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //真机需打开，用于覆盖此默认实现
#endif
bool config_save(const sys_config_t *cfg)
{
    (void)cfg;
    return false;
}
