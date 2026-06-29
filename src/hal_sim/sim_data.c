#include "sim_data.h"

#include "../ui/core/data.h"
#include "../ui/core/ui_dirty.h"
#include "../algo_sim/deco_core.h"
#ifndef PC_SIMULATOR
#define PC_SIMULATOR
#endif
#include "sim_policy.h"
#include "sim_alert_policy.h"
#define DEBUG_LINK_PC_IMPLEMENTATION
#include "debug_link_pc.h"
#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef TCP_ALGO_DEBUG
#define TCP_ALGO_DEBUG 1
#endif

static lv_timer_t *s_sim_timer;
static lv_timer_t *s_heading_timer;
static uint32_t s_heading_accum_mdeg;

static void sim_fill_logbook_tanks(logbook_entry_t *entry);
static void sim_raise_next_log_no_from_logbook(void);

static uint32_t sim_surface_confirm_s(void)
{
    return (uint32_t)bus_get_surface_confirm_min() * 60U;
}

static float sim_dive_entry_depth_m(void)
{
    return bus_get_dive_start_depth_m();
}

static float sim_default_air_mod_m(void)
{
    float mod_m = bus_calculate_gas_mod(21U, 0U, 1.4f);
    return (mod_m > 0.0f) ? mod_m : 56.0f;
}

typedef enum
{
    SIM_LIFE_SURFACE_CONFIRMED = 0,
    SIM_LIFE_ENTRY_PENDING,
    SIM_LIFE_DIVING,
    SIM_LIFE_SURFACING_PENDING
} sim_lifecycle_state_t;

typedef struct
{
    uint16_t heading_deg;
    uint32_t dive_time_s;
    uint32_t surface_time_s;
    uint32_t runtime_tick_s;
    uint32_t entry_pending_s;
    uint32_t surface_pending_s;
    uint32_t last_surface_interval_s;
    float depth_m;
    float max_depth_m;
    float depth_sum_m;
    uint16_t tts_min;
    uint16_t next_log_no;
    uint32_t depth_sample_count;
    uint8_t cns_pct;
    uint8_t start_h;
    uint8_t start_m;
    uint8_t start_cns_pct;
    uint16_t otu;
    float battery_pct;
    float temperature_c;
    uint16_t layout_tick;
    uint8_t layout_phase;
    uint16_t phase_tick;
    uint8_t depth_phase;
    float rate_sample_depth_m;
    bool rate_sample_valid;
    sim_lifecycle_state_t lifecycle_state;
    bool in_dive;
    bool surfacing_pending;
} sim_state_t;

static sim_state_t s_sim = {
    .heading_deg = 0,
    .dive_time_s = 0,
    .surface_time_s = 0,
    .runtime_tick_s = 0,
    .entry_pending_s = 0,
    .surface_pending_s = 0,
    .last_surface_interval_s = 0,
    .depth_m = 0.0f,
    .max_depth_m = 0.0f,
    .depth_sum_m = 0.0f,
    .tts_min = 0,
    .next_log_no = 1,
    .depth_sample_count = 0,
    .cns_pct = 0,
    .start_h = 10,
    .start_m = 55,
    .start_cns_pct = 0,
    .otu = 0,
    .battery_pct = 85.0f,
    .temperature_c = SIM_TEMP_C,
    .layout_tick = 0,
    .layout_phase = 0,
    .phase_tick = 0,
    .depth_phase = 0,
    .rate_sample_depth_m = 0.0f,
    .rate_sample_valid = false,
    .lifecycle_state = SIM_LIFE_SURFACE_CONFIRMED,
    .in_dive = false,
    .surfacing_pending = false,
};

static float sim_calc_ppo2(uint8_t o2_pct, float depth_m)
{
    float pressure_depth_m = (depth_m > 0.0f) ? depth_m : 0.0f;
    float pressure_mbar = SIM_SURFACE_PRESSURE_MBAR + (pressure_depth_m / SIM_WATER_METERS_PER_BAR) * 1000.0f;
    return bus_calculate_ppo2_bar(o2_pct, pressure_mbar);
}

static void sim_update_heading_auto(uint16_t step_deg)
{
    if (step_deg == 0U)
    {
        return;
    }
    s_sim.heading_deg = (uint16_t)((bus_get_heading() + step_deg) % 360U);
    bus_set_heading(s_sim.heading_deg);
}

static uint16_t sim_heading_speed_dps(void)
{
#if TCP_ALGO_DEBUG
    if (debug_link_pc_manual_mode())
    {
        return debug_link_pc_heading_speed_dps();
    }
#endif
    return 1U;
}

static void sim_lifecycle_set_state(sim_lifecycle_state_t state)
{
    static const dive_lifecycle_phase_t phase_map[] = {
        DIVE_LIFECYCLE_SURFACE_CONFIRMED,
        DIVE_LIFECYCLE_ENTRY_PENDING,
        DIVE_LIFECYCLE_ACTIVE,
        DIVE_LIFECYCLE_SURFACING_PENDING
    };

    s_sim.lifecycle_state = state;
    s_sim.in_dive = (state == SIM_LIFE_DIVING) || (state == SIM_LIFE_SURFACING_PENDING);
    s_sim.surfacing_pending = (state == SIM_LIFE_SURFACING_PENDING);
    bus_set_dive_lifecycle_phase(phase_map[state]);
    deco_core_set_surface_confirmed((state == SIM_LIFE_SURFACE_CONFIRMED) || (state == SIM_LIFE_ENTRY_PENDING));
}

static void sim_heading_tick_cb(lv_timer_t *t)
{
    uint16_t speed_dps;
    uint32_t step_deg;

    (void)t;

    speed_dps = sim_heading_speed_dps();
    if (speed_dps == 0U)
    {
        s_heading_accum_mdeg = 0U;
        return;
    }

    s_heading_accum_mdeg += (uint32_t)speed_dps * SIM_HEADING_TIMER_MS;
    step_deg = s_heading_accum_mdeg / 1000U;
    s_heading_accum_mdeg %= 1000U;
    if (step_deg > 0U)
    {
        sim_update_heading_auto((uint16_t)(step_deg % 360U));
    }
}

static void sim_update_gas_derived(float depth_m)
{
    uint8_t active_idx = bus_get_gas_active_idx();
    float fio2 = (float)bus_get_gas_slot_o2_pct(active_idx) / 100.0f;
    float fihe = (float)bus_get_gas_slot_he_pct(active_idx) / 100.0f;
    float fin2 = 1.0f - fio2 - fihe;
    float ambient_ata = 1.0f + depth_m / 10.0f;
    float surface_density = fio2 * 1.428f + fihe * 0.179f + fin2 * 1.251f;

    bus_set_fio2(fio2 * 100.0f);
    bus_set_gas_density(surface_density * ambient_ata);
}

static void sim_update_mlx_diagnostics(uint32_t tick_s)
{
    float phase = (float)(tick_s % 240U);

    bus_set_mlx(120.0f + phase,
                -80.0f + (phase * 0.5f),
                35.0f - (phase * 0.25f));
}

static void sim_update_temperature(void)
{
    s_sim.temperature_c = SIM_TEMP_C;
    bus_set_temperature(s_sim.temperature_c);
    bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
    bus_set_prj_temperature(s_sim.temperature_c - 1.0f);
}

static void sim_update_runtime_metrics(uint16_t time_scale)
{
    uint16_t scale_load = (time_scale > 1U) ? (time_scale / 4U) : 0U;
    uint8_t cpu_pct = (uint8_t)(12U + ((s_sim.runtime_tick_s * 7U + (s_sim.runtime_tick_s / 3U) * 5U) % 23U));
    uint16_t fps = (time_scale > 20U) ? 30U : 33U;

    if (scale_load > 38U)
    {
        scale_load = 38U;
    }
    bus_set_cpu_load((uint8_t)(cpu_pct + scale_load));
    bus_set_fps((uint16_t)(fps + (s_sim.runtime_tick_s % 3U)));
}

static void sim_seed_original_defaults(void)
{
    float air_mod_m = sim_default_air_mod_m();
    bus_set_gas_slot(0, "AIR", 21, 0, air_mod_m, 1.4f);
    bus_set_gas_slot(1, "", 0, 0, 0.0f, 0.0f);
    bus_set_gas_slot(2, "", 0, 0, 0.0f, 0.0f);
    bus_set_gas_slot_count(1);
    bus_set_gas(0, "AIR");
    bus_set_pod(0, 200.0f);
    bus_set_pod(1, 185.0f);
    bus_set_gf_setting(30, 70);
    bus_set_surf_gf(85.0f);
    bus_set_gf99(42.0f);
    bus_set_mod(air_mod_m);
    bus_set_ceiling(0.0f);
    bus_set_gas_mix(21, 0);
    sim_update_gas_derived(0.0f);
}

static void sim_seed_logbook_demo_if_empty(void)
{
    logbook_entry_t entry;

    if (logbook_backend_count() > 0U)
    {
        sim_raise_next_log_no_from_logbook();
        return;
    }

    for (uint8_t i = 0U; i < 11U; i++)
    {
        uint8_t age = (uint8_t)(10U - i);
        float max_depth = 18.0f + (float)((age * 3U) % 18U);
        uint32_t dive_time_s = 1800U + (uint32_t)age * 180U;
        dive_pt_t points[] =
        {
            {0.0f, 0.0f},
            {60.0f, max_depth * 0.35f},
            {180.0f, max_depth * 0.75f},
            {420.0f, max_depth},
            {900.0f, max_depth * 0.90f},
            {1500.0f, max_depth * 0.58f},
            {2100.0f, 6.0f},
            {(float)dive_time_s, 0.0f},
        };

        (void)memset(&entry, 0, sizeof(entry));
        entry.valid = true;
        entry.meta.log_no = (uint16_t)(i + 1U);
        entry.meta.year = 2025U;
        entry.meta.month = (uint8_t)(12U - (age / 4U));
        entry.meta.day = (uint8_t)(31U - age);
        entry.meta.start_h = (uint8_t)(10U + (age % 5U));
        entry.meta.start_m = (uint8_t)((55U + age * 7U) % 60U);
        uint16_t start_total_min = (uint16_t)(entry.meta.start_h * 60U + entry.meta.start_m);
        uint16_t end_total_min = (uint16_t)(start_total_min + dive_time_s / 60U);
        entry.meta.end_h = (uint8_t)((end_total_min / 60U) % 24U);
        entry.meta.end_m = (uint8_t)(end_total_min % 60U);
        entry.dive_time_s = dive_time_s;
        entry.surface_interval_s = (42U * 3600U + 24U * 60U) + (uint32_t)age * 1800U;
        entry.max_depth_m = max_depth;
        entry.avg_depth_m = max_depth * 0.65f;
        entry.surface_mbar = 1013.0f;
        entry.start_cns_pct = (uint8_t)(age % 4U);
        entry.end_cns_pct = (uint8_t)(8U + age);
        entry.avg_sac_l_min = 11.5f + (float)(age % 4U) * 0.7f;
        (void)snprintf(entry.mode, sizeof(entry.mode), "%s", (age % 3U == 0U) ? "Nitrox" : "Air");
        (void)snprintf(entry.deco_model, sizeof(entry.deco_model), "%s", "GF 30/70");
        sim_fill_logbook_tanks(&entry);
        (void)snprintf(entry.tank_start[0], sizeof(entry.tank_start[0]), "%u", (unsigned)(200U - age));
        (void)snprintf(entry.tank_end[0], sizeof(entry.tank_end[0]), "%u", (unsigned)(82U - (age % 6U) * 3U));
        (void)logbook_backend_append_finalized_dive(&entry, points, (uint16_t)(sizeof(points) / sizeof(points[0])));
    }
    sim_raise_next_log_no_from_logbook();
}

#if TCP_ALGO_DEBUG
static void sim_seed_tcp_algo_defaults(void)
{
    float air_mod_m = sim_default_air_mod_m();
    bus_set_gas_slot(0, "AIR", 21, 0, air_mod_m, 1.4f);
    bus_set_gas_slot(1, "", 0, 0, 0.0f, 0.0f);
    bus_set_gas_slot(2, "", 0, 0, 0.0f, 0.0f);
    bus_set_gas_slot_count(1);
    bus_set_gas(0, "AIR");
    bus_set_pod(0, 200.0f);
    bus_set_pod(1, 185.0f);
    bus_set_gf_setting(30, 70);
    bus_set_surf_gf(0.0f);
    bus_set_gf99(0.0f);
    bus_set_mod(air_mod_m);
    bus_set_ceiling(0.0f);
    bus_set_gas_mix(21, 0);
    sim_update_gas_derived(0.0f);
    bus_update_deco(99, STOP_NONE, 0.0f, 0U, 0U, false);
}

static void sim_reset_for_tcp_debug(void)
{
    memset(&s_sim, 0, sizeof(s_sim));
    s_sim.battery_pct = 86.0f;
    s_sim.temperature_c = SIM_TEMP_C;
    s_sim.next_log_no = 1U;
    s_sim.start_h = 10U;
    s_sim.start_m = 55U;

    data_init();
    sim_alert_init();
    dive_log_reset();
    bus_set_depth(0.0f);
    bus_set_ascent_rate(0.0f);
    bus_set_dive_time(0U);
    bus_set_surface_time(0U);
    bus_set_battery(s_sim.battery_pct);
    bus_set_temperature(s_sim.temperature_c);
    bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
    bus_set_prj_temperature(s_sim.temperature_c - 1.0f);
    bus_set_cpu_load(12U);
    bus_set_fps(33U);
    sim_seed_tcp_algo_defaults();
    sim_seed_logbook_demo_if_empty();
    deco_core_reset();
    sim_lifecycle_set_state(SIM_LIFE_SURFACE_CONFIRMED);

    bus_requeue_dirty(DIRTY_DATA_ALL);
}

static bool sim_apply_rtc_offline(uint32_t seconds, char *err_buf, uint16_t err_buf_size)
{
    if (seconds == 0U)
    {
        if (err_buf && err_buf_size > 0U) (void)snprintf(err_buf, err_buf_size, "%s", "usage: rtc_offline <seconds>");
        return false;
    }
    if (s_sim.lifecycle_state != SIM_LIFE_SURFACE_CONFIRMED)
    {
        if (err_buf && err_buf_size > 0U) (void)snprintf(err_buf, err_buf_size, "%s", "rtc_offline requires confirmed surface state");
        return false;
    }
    if (!deco_core_rtc_offline(seconds))
    {
        if (err_buf && err_buf_size > 0U) (void)snprintf(err_buf, err_buf_size, "%s", "deco core rtc offline failed");
        return false;
    }

    s_sim.depth_m = 0.0f;
    s_sim.entry_pending_s = 0U;
    s_sim.surface_pending_s = 0U;
    s_sim.surface_time_s += seconds;
    s_sim.rate_sample_valid = false;
    bus_set_depth(0.0f);
    bus_set_ascent_rate(0.0f);
    bus_set_surface_time(s_sim.surface_time_s);
    bus_requeue_dirty(DIRTY_DATA_ALL);
    return true;
}
#endif

static void test_set_ui_layout(uint8_t phase)
{
    static ble_ui_sync_payload_t s_payload;
    static const uint8_t s_default_card_order[8] = {
        PAGE_ID_BLANK,
        PAGE_ID_COMPASS,
        PAGE_ID_DECO,
        PAGE_ID_PLAN,
        PAGE_ID_GAS,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_UNUSED,
        PAGE_ID_UNUSED,
    };
    static const uint8_t s_multi_custom_card_order[8] = {
        PAGE_ID_COMPASS,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_DECO,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_PLAN,
        PAGE_ID_GAS,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_BLANK,
    };

    memset(&s_payload, 0, sizeof(s_payload));
    s_payload.version = BLE_CFG_VERSION;
    memcpy(s_payload.card_order,
           (phase == 2U) ? s_multi_custom_card_order : s_default_card_order,
           sizeof(s_payload.card_order));

    if (phase == 3U) {
        uint8_t side_empty[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };
        uint8_t top_empty[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     2, 0 },
            { COMP_DIVE_TIME_1606, 4, 0 },
            { COMP_GAS_1606,       4, 1 },
            { COMP_TEMP_0806,      6, 0 },
            { COMP_BATTERY_0806,   6, 1 },
        };
        const uint8_t (*fixed)[3] = ui_layout_is_vertical_split() ? side_empty : top_empty;

        s_payload.left_count = (uint8_t)(ui_layout_is_vertical_split() ? (sizeof(side_empty) / sizeof(side_empty[0])) : (sizeof(top_empty) / sizeof(top_empty[0])));
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = fixed[i][0];
            s_payload.left_widgets[i].x = fixed[i][1];
            s_payload.left_widgets[i].y = fixed[i][2];
        }
        s_payload.custom_5f_count = 0U;
    } else if (phase == 0U || phase == 2U) {
        uint8_t side_def[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };
        uint8_t top_def[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     2, 0 },
            { COMP_DIVE_TIME_1606, 4, 0 },
            { COMP_GAS_1606,       4, 1 },
            { COMP_TEMP_0806,      6, 0 },
            { COMP_BATTERY_0806,   6, 1 },
        };
        uint8_t side_custom[][3] = {
            { COMP_DEPTH_1606,     0, 0 },
            { COMP_PPO2_0806,      0, 2 },
            { COMP_BATTERY_0806,   0, 3 },
            { COMP_POD_0806,       0, 4 },
            { COMP_NDL_STOP_1606,  1, 0 },
            { COMP_CNS_0806,       1, 2 },
            { COMP_OTU_0806,       1, 3 },
            { COMP_HEADING_0806,   1, 4 },
            { COMP_GAS_1606,       2, 0 },
            { COMP_DIVE_TIME_1606, 2, 2 },
            { COMP_TTS_AT_5MIN_0806, 3, 0 },
            { COMP_TTS_DELTA_5MIN_0806, 3, 1 },
            { COMP_NDL_UP_3M_0806, 3, 2 },
            { COMP_NDL_DOWN_3M_0806, 3, 3 },
            { COMP_NDL_DELTA_3M_0806, 3, 4 },
            { COMP_GTR_0806,       4, 0 },
            { COMP_RMV_0806,       4, 1 },
            { COMP_SAC_0806,       4, 2 },
        };
        uint8_t top_custom[][3] = {
            { COMP_DEPTH_1606,     0, 0 },
            { COMP_PPO2_0806,      0, 2 },
            { COMP_BATTERY_0806,   0, 3 },
            { COMP_POD_0806,       0, 4 },
            { COMP_NDL_STOP_1606,  1, 0 },
            { COMP_CNS_0806,       1, 2 },
            { COMP_OTU_0806,       1, 3 },
            { COMP_HEADING_0806,   1, 4 },
            { COMP_GAS_1606,       2, 0 },
            { COMP_DIVE_TIME_1606, 2, 2 },
            { COMP_TTS_AT_5MIN_0806, 2, 4 },
            { COMP_TTS_DELTA_5MIN_0806, 2, 5 },
            { COMP_NDL_UP_3M_0806, 2, 6 },
            { COMP_NDL_DOWN_3M_0806, 3, 0 },
            { COMP_NDL_DELTA_3M_0806, 3, 1 },
            { COMP_GTR_0806,       3, 2 },
            { COMP_RMV_0806,       3, 3 },
            { COMP_SAC_0806,       3, 4 },
        };
        const bool horizontal = !ui_layout_is_vertical_split();
        const uint8_t (*fixed)[3] = horizontal ? top_def : side_def;
        const uint8_t (*custom)[3] = horizontal ? top_custom : side_custom;

        s_payload.left_count = (uint8_t)(horizontal ? (sizeof(top_def) / sizeof(top_def[0])) : (sizeof(side_def) / sizeof(side_def[0])));
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = fixed[i][0];
            s_payload.left_widgets[i].x = fixed[i][1];
            s_payload.left_widgets[i].y = fixed[i][2];
        }

        s_payload.custom_5f_count = (uint8_t)(horizontal ? (sizeof(top_custom) / sizeof(top_custom[0])) : (sizeof(side_custom) / sizeof(side_custom[0])));
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom[i][0];
            s_payload.custom_5f_widgets[i].r = custom[i][1];
            s_payload.custom_5f_widgets[i].c = custom[i][2];
        }
    } else {
        uint8_t side_min[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };
        uint8_t top_min[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     2, 0 },
            { COMP_DIVE_TIME_1606, 4, 0 },
            { COMP_GAS_1606,       4, 1 },
            { COMP_TEMP_0806,      6, 0 },
            { COMP_BATTERY_0806,   6, 1 },
        };
        uint8_t side_custom_min[][3] = {
            { COMP_TEMP_0806,      0, 0 },
            { COMP_TEMP_0806,      0, 2 },
            { COMP_BATTERY_0806,   2, 0 },
            { COMP_PPO2_0806,      2, 2 },
            { COMP_NDL_STOP_1606,  3, 0 },
            { COMP_GYRO_2406,      4, 3 },
            { COMP_EMPTY,          4, 0 },
        };
        uint8_t top_custom_min[][3] = {
            { COMP_TEMP_0806,      0, 0 },
            { COMP_TEMP_0806,      2, 0 },
            { COMP_BATTERY_0806,   4, 0 },
            { COMP_PPO2_0806,      5, 0 },
            { COMP_NDL_STOP_1606,  0, 2 },
            { COMP_GYRO_2406,      3, 2 },
        };
        const bool horizontal = !ui_layout_is_vertical_split();
        const uint8_t (*fixed)[3] = horizontal ? top_min : side_min;
        const uint8_t (*custom)[3] = horizontal ? top_custom_min : side_custom_min;

        s_payload.left_count = (uint8_t)(horizontal ? (sizeof(top_min) / sizeof(top_min[0])) : (sizeof(side_min) / sizeof(side_min[0])));
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = fixed[i][0];
            s_payload.left_widgets[i].x = fixed[i][1];
            s_payload.left_widgets[i].y = fixed[i][2];
        }

        s_payload.custom_5f_count = (uint8_t)(horizontal ? (sizeof(top_custom_min) / sizeof(top_custom_min[0])) : (sizeof(side_custom_min) / sizeof(side_custom_min[0])));
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom[i][0];
            s_payload.custom_5f_widgets[i].r = custom[i][1];
            s_payload.custom_5f_widgets[i].c = custom[i][2];
        }
    }

    bus_set_ui_layout(&s_payload);
}

static void sim_update_depth_script(void)
{
    switch (s_sim.depth_phase) {
        case 0:
            if (s_sim.phase_tick < 6) {
                s_sim.phase_tick++;
                s_sim.depth_m += 2.12f;
            } else {
                s_sim.depth_phase = 1;
                s_sim.phase_tick = 0;
            }
            break;
        case 1:
            if (s_sim.phase_tick < 5) {
                s_sim.phase_tick++;
            } else {
                s_sim.depth_phase = 2;
                s_sim.phase_tick = 0;
            }
            break;
        default:
            if (s_sim.phase_tick < 5) {
                s_sim.phase_tick++;
                s_sim.depth_m -= 2.12f;
                if (s_sim.depth_m < 0.0f) {
                    s_sim.depth_m = 0.0f;
                }
            } else {
                s_sim.depth_phase = 0;
                s_sim.phase_tick = 0;
                s_sim.depth_m = 0.0f;
            }
            break;
    }
}

static void sim_update_deco_state(void)
{
    if (s_sim.depth_m < 10.0f) {
        bus_update_deco(45, STOP_NONE, 0.0f, 0, 0, false);
        return;
    }

    if (s_sim.depth_phase == 1U) {
        bus_update_deco(12, STOP_SAFETY, 5.0f, 180, 180, false);
        return;
    }

    if (s_sim.depth_phase == 2U && s_sim.depth_m <= 6.5f) {
        uint16_t left_s = (s_sim.phase_tick < 5U) ? (uint16_t)(180U - (s_sim.phase_tick * 30U)) : 0U;
        bus_update_deco(12, STOP_SAFETY, 5.0f, 180, left_s, true);
        return;
    }

    bus_update_deco(0, STOP_DECO, 6.0f, 300, 120, false);
}

static void sim_start_dive(float depth_m)
{
    uint16_t h = bus_get_sys_time_h();
    uint16_t m = bus_get_sys_time_m();

    sim_lifecycle_set_state(SIM_LIFE_DIVING);
    s_sim.entry_pending_s = 0U;
    s_sim.surface_pending_s = 0U;
    s_sim.last_surface_interval_s = s_sim.surface_time_s;
    s_sim.dive_time_s = 0U;
    s_sim.surface_time_s = 0U;
    s_sim.max_depth_m = depth_m;
    s_sim.depth_sum_m = 0.0f;
    s_sim.depth_sample_count = 0U;
    s_sim.start_h = (h <= 23U) ? (uint8_t)h : 10U;
    s_sim.start_m = (m <= 59U) ? (uint8_t)m : 55U;
    s_sim.start_cns_pct = bus_get_cns_pct();
    dive_log_reset();
    dive_log_append(0.0f, 0.0f);
    bus_set_dive_time(0U);
    bus_set_surface_time(0U);
}

static void sim_fill_logbook_tanks(logbook_entry_t *entry)
{
    for (uint8_t i = 0U; i < LOGBOOK_TANK_COUNT; i++)
    {
        (void)snprintf(entry->tank_start[i], sizeof(entry->tank_start[i]), "N/A");
        (void)snprintf(entry->tank_end[i], sizeof(entry->tank_end[i]), "N/A");
    }
}

static void sim_raise_next_log_no_from_logbook(void)
{
    uint16_t count = logbook_backend_count();
    uint16_t max_log_no = 0U;

    for (uint16_t i = 0U; i < count; i++)
    {
        logbook_entry_t entry;
        if (logbook_backend_get_summary(i, &entry) && entry.valid && entry.meta.log_no > max_log_no)
        {
            max_log_no = entry.meta.log_no;
        }
    }

    if (s_sim.next_log_no == 0U)
    {
        s_sim.next_log_no = 1U;
    }
    if (s_sim.next_log_no <= max_log_no)
    {
        s_sim.next_log_no = (uint16_t)(max_log_no + 1U);
    }
}

static void sim_finalize_dive(void)
{
    logbook_entry_t entry;
    dive_pt_t points[MAX_DIVE_LOG];
    uint8_t count = bus_get_dive_log_count();
    uint8_t active_gas = bus_get_gas_active_idx();
    uint8_t active_o2 = bus_get_gas_slot_o2_pct(active_gas);
    uint8_t active_he = bus_get_gas_slot_he_pct(active_gas);
    const char *mode = active_he > 0U ? "TX" : ((active_o2 == 21U) ? "Air" : "Nitrox");
    uint32_t start_min;
    uint32_t end_min;

    (void)memset(&entry, 0, sizeof(entry));
    for (uint8_t i = 0U; i < count; i++)
    {
        (void)bus_get_dive_log_point(i, &points[i]);
    }

    sim_raise_next_log_no_from_logbook();
    entry.valid = true;
    entry.meta.log_no = s_sim.next_log_no++;
    entry.meta.year = 2025U;
    entry.meta.month = 12U;
    entry.meta.day = 31U;
    entry.meta.start_h = s_sim.start_h;
    entry.meta.start_m = s_sim.start_m;
    start_min = ((uint32_t)entry.meta.start_h * 60U) + entry.meta.start_m;
    end_min = (start_min + ((s_sim.dive_time_s + 59U) / 60U)) % 1440U;
    entry.meta.end_h = (uint8_t)(end_min / 60U);
    entry.meta.end_m = (uint8_t)(end_min % 60U);
    entry.dive_time_s = s_sim.dive_time_s;
    entry.surface_interval_s = s_sim.last_surface_interval_s;
    entry.max_depth_m = s_sim.max_depth_m;
    entry.avg_depth_m = (s_sim.depth_sample_count > 0U) ? (s_sim.depth_sum_m / (float)s_sim.depth_sample_count) : 0.0f;
    entry.surface_mbar = bus_get_ambient_pressure();
    entry.start_cns_pct = s_sim.start_cns_pct;
    entry.end_cns_pct = bus_get_cns_pct();
    entry.avg_sac_l_min = bus_get_sac_rate();
    (void)snprintf(entry.mode, sizeof(entry.mode), "%s", mode);
    (void)snprintf(entry.deco_model, sizeof(entry.deco_model), "GF %u/%u", (unsigned)bus_get_gf_low(), (unsigned)bus_get_gf_high());
    sim_fill_logbook_tanks(&entry);
    (void)logbook_backend_append_finalized_dive(&entry, points, count);

    sim_lifecycle_set_state(SIM_LIFE_SURFACE_CONFIRMED);
    s_sim.entry_pending_s = 0U;
    s_sim.surface_pending_s = 0U;
    s_sim.dive_time_s = 0U;
    s_sim.surface_time_s = 0U;
    s_sim.max_depth_m = 0.0f;
    s_sim.depth_sum_m = 0.0f;
    s_sim.depth_sample_count = 0U;
    dive_log_reset();
    bus_set_dive_time(0U);
    bus_set_surface_time(0U);
}

static void sim_lifecycle_tick(float depth_m)
{
    bool dive_tick = false;

    if (s_sim.lifecycle_state == SIM_LIFE_SURFACE_CONFIRMED)
    {
        if (depth_m >= sim_dive_entry_depth_m())
        {
            s_sim.entry_pending_s = 0U;
            sim_lifecycle_set_state(SIM_LIFE_ENTRY_PENDING);
        }
        else
        {
            s_sim.surface_time_s++;
            bus_set_surface_time(s_sim.surface_time_s);
            bus_set_dive_time(0U);
            return;
        }
    }

    if (s_sim.lifecycle_state == SIM_LIFE_ENTRY_PENDING)
    {
        if (depth_m >= sim_dive_entry_depth_m())
        {
            if (s_sim.entry_pending_s < SIM_DIVE_ENTRY_CONFIRM_S) s_sim.entry_pending_s++;
            if (s_sim.entry_pending_s >= SIM_DIVE_ENTRY_CONFIRM_S)
            {
                sim_start_dive(depth_m);
                dive_tick = true;
            }
            else
            {
                s_sim.surface_time_s++;
                bus_set_surface_time(s_sim.surface_time_s);
                bus_set_dive_time(0U);
                return;
            }
        }
        else
        {
            s_sim.entry_pending_s = 0U;
            sim_lifecycle_set_state(SIM_LIFE_SURFACE_CONFIRMED);
            s_sim.surface_time_s++;
            bus_set_surface_time(s_sim.surface_time_s);
            bus_set_dive_time(0U);
            return;
        }
    }

    if ((s_sim.lifecycle_state == SIM_LIFE_DIVING) || (s_sim.lifecycle_state == SIM_LIFE_SURFACING_PENDING)) dive_tick = true;
    if (!dive_tick) return;

    s_sim.dive_time_s++;
    bus_set_dive_time(s_sim.dive_time_s);
    if (depth_m > s_sim.max_depth_m)
    {
        s_sim.max_depth_m = depth_m;
    }
    s_sim.depth_sum_m += depth_m;
    s_sim.depth_sample_count++;
    bus_set_dive_profile_stats(s_sim.max_depth_m, s_sim.depth_sum_m / (float)s_sim.depth_sample_count);
    dive_log_append_sampled((float)s_sim.dive_time_s, depth_m);

    if (depth_m <= SIM_SURFACE_DEPTH_M)
    {
        if (s_sim.lifecycle_state != SIM_LIFE_SURFACING_PENDING)
        {
            s_sim.surface_pending_s = 0U;
            sim_lifecycle_set_state(SIM_LIFE_SURFACING_PENDING);
        }
        s_sim.surface_pending_s++;
        if (s_sim.surface_pending_s >= sim_surface_confirm_s())
        {
            sim_finalize_dive();
        }
    }
    else
    {
        if (s_sim.lifecycle_state == SIM_LIFE_SURFACING_PENDING) sim_lifecycle_set_state(SIM_LIFE_DIVING);
        s_sim.surface_pending_s = 0U;
    }
}

static void sim_tick_cb(lv_timer_t *t)
{
    float current_depth_m;

    (void)t;

#if TCP_ALGO_DEBUG
    if (debug_link_pc_consume_connect_event()) {
        sim_reset_for_tcp_debug();
    }

    if (!debug_link_pc_manual_mode()) {
        return;
    }

    {
        uint16_t time_scale = debug_link_pc_time_scale();
        for (uint16_t tick = 0; tick < time_scale; tick++) {
            bool goto_reached = false;
            s_sim.runtime_tick_s++;
            current_depth_m = g_sensor_data.depth;
            if (debug_link_pc_depth_goto_step(current_depth_m, &current_depth_m, &goto_reached)) {
                if (goto_reached) {
                    bus_set_depth_force(current_depth_m);
                    bus_set_ascent_rate(0.0f);
                    s_sim.rate_sample_valid = false;
                } else {
                    bus_set_depth(current_depth_m);
                }
            }
            s_sim.depth_m = current_depth_m;
            sim_lifecycle_tick(current_depth_m);

            if (s_sim.rate_sample_valid) {
                bus_set_ascent_rate((s_sim.rate_sample_depth_m - current_depth_m) * 60.0f);
            } else {
                bus_set_ascent_rate(0.0f);
                s_sim.rate_sample_valid = true;
            }
            s_sim.rate_sample_depth_m = current_depth_m;

            s_sim.battery_pct += 1.2f;
            bus_set_battery(s_sim.battery_pct);

            sim_update_temperature();
            sim_update_mlx_diagnostics(s_sim.dive_time_s);
            sim_update_runtime_metrics(time_scale);

            deco_core_tick(current_depth_m, s_sim.temperature_c, 1U);
            sim_alert_tick();
        }
    }
    return;
#endif

    if (s_sim.layout_tick == 0U) {
        test_set_ui_layout(s_sim.layout_phase);
        s_sim.layout_phase = (uint8_t)((s_sim.layout_phase + 1U) % SIM_LAYOUT_PHASE_COUNT);
    }
    s_sim.layout_tick = (uint16_t)((s_sim.layout_tick + 1U) % SIM_LAYOUT_SWITCH_TICKS);
    s_sim.runtime_tick_s++;
    sim_update_runtime_metrics(1U);

    sim_update_depth_script();
    sim_update_deco_state();
    current_depth_m = s_sim.depth_m;
    bus_set_depth(current_depth_m);
    sim_lifecycle_tick(current_depth_m);

    if (s_sim.rate_sample_valid) {
        bus_set_ascent_rate((s_sim.rate_sample_depth_m - current_depth_m) * 60.0f);
    } else {
        bus_set_ascent_rate(0.0f);
        s_sim.rate_sample_valid = true;
    }
    s_sim.rate_sample_depth_m = current_depth_m;

    s_sim.battery_pct += 1.2f;
    bus_set_battery(s_sim.battery_pct);

    sim_update_temperature();
    sim_update_mlx_diagnostics(s_sim.dive_time_s);

    if (current_depth_m > 12.0f) {
        deco_stop_t sim_stops[] = {
            { .depth_m = 9.0f, .stay_min = 2.0f },
            { .depth_m = 6.0f, .stay_min = 3.0f },
            { .depth_m = 3.0f, .stay_min = 1.0f },
        };
        bus_set_deco_plan(sim_stops, 3);
    }

    s_sim.tts_min++;
    bus_set_tts(s_sim.tts_min);

    if (s_sim.cns_pct < 100U) {
        s_sim.cns_pct++;
        bus_set_cns(s_sim.cns_pct);
    }

    s_sim.otu++;
    bus_set_otu(s_sim.otu);

    for (uint8_t i = 0; i < GAS_COUNT; i++) {
        bus_set_ppo2(i, sim_calc_ppo2(g_sensor_data.gas_slot_o2_pct[i], current_depth_m));
    }
    sim_update_gas_derived(current_depth_m);

    {
        static const int16_t s_tissue_raw[16] = {
            20, 30, 40, 50, 60, 65, 70, 72,
            74, 76, 78, 80, 82, 85, 88, 90
        };
        uint8_t tissue_gf[16];
        uint8_t gf_high = bus_get_gf_high();
        if (gf_high == 0U)
        {
            gf_high = 85U;
        }
        for (uint8_t i = 0U; i < 16U; i++)
        {
            uint16_t gf_pct = (s_tissue_raw[i] > 0) ? ((uint16_t)s_tissue_raw[i] * 100U / gf_high) : 0U;
            tissue_gf[i] = (gf_pct > 200U) ? 200U : (uint8_t)gf_pct;
        }
        bus_set_tissue_loads(s_tissue_raw, tissue_gf, (float)gf_high);
    }

    sim_alert_tick();
}

void sim_data_start(void)
{
    if (s_sim_timer != NULL) {
        return;
    }

#if TCP_ALGO_DEBUG
    debug_link_pc_set_rtc_offline_handler(sim_apply_rtc_offline);
    debug_link_pc_start();
    sim_alert_init();
    sim_seed_logbook_demo_if_empty();
#else
    sim_seed_original_defaults();
    sim_seed_logbook_demo_if_empty();
    sim_alert_init();
#endif

    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);
    s_heading_timer = lv_timer_create(sim_heading_tick_cb, SIM_HEADING_TIMER_MS, NULL);

}
