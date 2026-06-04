#include "sim_data.h"

#include "../ui/core/data.h"
#include "../ui/core/ui_dirty.h"
#include "../algo_sim/buhlmann_debug.h"
#include "sim_alert_policy.h"
#ifndef PC_SIMULATOR
#define PC_SIMULATOR
#endif
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

static void sim_fill_logbook_tanks(logbook_entry_t *entry);

#define SIM_LAYOUT_PHASE_COUNT 4U
#define SIM_LAYOUT_SWITCH_TICKS 5U
#define SIM_DIVE_ENTRY_DEPTH_M 1.2f
#define SIM_SURFACE_DEPTH_M 0.8f
#ifndef SIM_SURFACE_CONFIRM_S
#define SIM_SURFACE_CONFIRM_S 5U
#endif

typedef struct
{
    uint16_t heading_deg;
    uint32_t dive_time_s;
    uint32_t surface_time_s;
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
    float temp_offset;
    uint16_t layout_tick;
    uint8_t layout_phase;
    uint16_t phase_tick;
    uint8_t depth_phase;
    float rate_sample_depth_m;
    bool rate_sample_valid;
    bool in_dive;
    bool surfacing_pending;
} sim_state_t;

static sim_state_t s_sim = {
    .heading_deg = 0,
    .dive_time_s = 0,
    .surface_time_s = 0,
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
    .temperature_c = 25.0f,
    .temp_offset = 0.0f,
    .layout_tick = 0,
    .layout_phase = 0,
    .phase_tick = 0,
    .depth_phase = 0,
    .rate_sample_depth_m = 0.0f,
    .rate_sample_valid = false,
    .in_dive = false,
    .surfacing_pending = false,
};

static float sim_calc_ppo2(uint8_t o2_pct, float depth_m)
{
    float ambient_bar = 1.0f + (depth_m / 10.0f);
    if (ambient_bar < 1.0f) {
        ambient_bar = 1.0f;
    }
    return ((float)o2_pct / 100.0f) * ambient_bar;
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

static void sim_seed_original_defaults(void)
{
    bus_set_gas_slot(0, "AIR", 21, 0, 56.0f);
    bus_set_gas_slot(1, "", 0, 0, 0.0f);
    bus_set_gas_slot(2, "", 0, 0, 0.0f);
    bus_set_gas_slot_count(1);
    bus_set_gas(0, "AIR");
    bus_set_pod(0, 200.0f);
    bus_set_pod(1, 185.0f);
    bus_set_gf_setting(30, 70);
    bus_set_surf_gf(85.0f);
    bus_set_gf99(42.0f);
    bus_set_mod(33.0f);
    bus_set_ceiling(0.0f);
    bus_set_gas_mix(21, 0);
    sim_update_gas_derived(0.0f);
}

static void sim_seed_logbook_demo_if_empty(void)
{
    logbook_entry_t entry;
    static const dive_pt_t points[] =
    {
        {0.0f, 0.0f},
        {60.0f, 8.0f},
        {180.0f, 18.0f},
        {420.0f, 24.0f},
        {900.0f, 22.0f},
        {1500.0f, 14.0f},
        {2100.0f, 6.0f},
        {2400.0f, 0.0f},
    };

    if (logbook_backend_count() > 0U)
    {
        return;
    }

    (void)memset(&entry, 0, sizeof(entry));
    entry.valid = true;
    entry.meta.log_no = 1U;
    entry.meta.year = 2025U;
    entry.meta.month = 12U;
    entry.meta.day = 31U;
    entry.meta.start_h = 10U;
    entry.meta.start_m = 55U;
    entry.meta.end_h = 11U;
    entry.meta.end_m = 35U;
    entry.dive_time_s = 2400U;
    entry.surface_interval_s = 42U * 3600U + 24U * 60U;
    entry.max_depth_m = 24.0f;
    entry.avg_depth_m = 15.6f;
    entry.surface_mbar = 1013.0f;
    entry.start_cns_pct = 0U;
    entry.end_cns_pct = 8U;
    entry.avg_sac_l_min = 12.4f;
    (void)snprintf(entry.mode, sizeof(entry.mode), "Air");
    (void)snprintf(entry.deco_model, sizeof(entry.deco_model), "GF 30/70");
    sim_fill_logbook_tanks(&entry);
    (void)snprintf(entry.tank_start[0], sizeof(entry.tank_start[0]), "200");
    (void)snprintf(entry.tank_end[0], sizeof(entry.tank_end[0]), "82");
    (void)logbook_backend_append_finalized_dive(&entry, points, (uint16_t)(sizeof(points) / sizeof(points[0])));
}

#if TCP_ALGO_DEBUG
static void sim_seed_tcp_algo_defaults(void)
{
    bus_set_gas_slot(0, "AIR", 21, 0, 56.0f);
    bus_set_gas_slot(1, "", 0, 0, 0.0f);
    bus_set_gas_slot(2, "", 0, 0, 0.0f);
    bus_set_gas_slot_count(1);
    bus_set_gas(0, "AIR");
    bus_set_pod(0, 200.0f);
    bus_set_pod(1, 185.0f);
    bus_set_gf_setting(30, 70);
    bus_set_surf_gf(0.0f);
    bus_set_gf99(0.0f);
    bus_set_mod(56.0f);
    bus_set_ceiling(0.0f);
    bus_set_gas_mix(21, 0);
    sim_update_gas_derived(0.0f);
    bus_update_deco(99, STOP_NONE, 0.0f, 0U, 0U, false);
}

static void sim_reset_for_tcp_debug(void)
{
    memset(&s_sim, 0, sizeof(s_sim));
    s_sim.battery_pct = 86.0f;
    s_sim.temperature_c = 25.0f;
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
    sim_seed_tcp_algo_defaults();
    sim_seed_logbook_demo_if_empty();
    buhlmann_debug_reset();

    bus_requeue_dirty(DIRTY_DATA_ALL);
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
        uint8_t left_empty[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };

        s_payload.left_count = sizeof(left_empty) / sizeof(left_empty[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_empty[i][0];
            s_payload.left_widgets[i].x = left_empty[i][1];
            s_payload.left_widgets[i].y = left_empty[i][2];
        }
        s_payload.custom_5f_count = 0U;
    } else if (phase == 0U || phase == 2U) {
        uint8_t left_def[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };
        uint8_t custom_5f[][3] = {
            { COMP_TEMP_0806,      0, 0 },
            { COMP_TEMP_0806,      0, 2 },
            { COMP_HEADING_0806,   0, 3 },
            { COMP_EMPTY,          2, 0 },
            { COMP_BATTERY_0806,   2, 2 },
            { COMP_PPO2_0806,      2, 4 },
            { COMP_NDL_STOP_1606,  3, 0 },
            { COMP_TTS_0806,       3, 2 },
            { COMP_CNS_0806,       3, 4 },
            { COMP_POD_0806,       4, 0 },
            { COMP_POD_0806,       4, 2 },
            { COMP_GYRO_2406,      4, 3 },
            { COMP_EMPTY,          4, 4 },
        };

        s_payload.left_count = sizeof(left_def) / sizeof(left_def[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_def[i][0];
            s_payload.left_widgets[i].x = left_def[i][1];
            s_payload.left_widgets[i].y = left_def[i][2];
        }

        s_payload.custom_5f_count = sizeof(custom_5f) / sizeof(custom_5f[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_5f[i][0];
            s_payload.custom_5f_widgets[i].r = custom_5f[i][1];
            s_payload.custom_5f_widgets[i].c = custom_5f[i][2];
        }
    } else {
        uint8_t left_min[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1612,     0, 1 },
            { COMP_DIVE_TIME_1606, 0, 3 },
            { COMP_GAS_1606,       0, 4 },
            { COMP_POD_0806,       0, 5 },
            { COMP_POD_0806,       1, 5 },
            { COMP_SYS_1606,       0, 6 },
        };
        uint8_t custom_min[][3] = {
            { COMP_TEMP_0806,      0, 0 },
            { COMP_TEMP_0806,      0, 2 },
            { COMP_BATTERY_0806,   2, 0 },
            { COMP_PPO2_0806,      2, 2 },
            { COMP_NDL_STOP_1606,  3, 0 },
            { COMP_GYRO_2406,      4, 3 },
            { COMP_EMPTY,          4, 0 },
        };

        s_payload.left_count = sizeof(left_min) / sizeof(left_min[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_min[i][0];
            s_payload.left_widgets[i].x = left_min[i][1];
            s_payload.left_widgets[i].y = left_min[i][2];
        }

        s_payload.custom_5f_count = sizeof(custom_min) / sizeof(custom_min[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_min[i][0];
            s_payload.custom_5f_widgets[i].r = custom_min[i][1];
            s_payload.custom_5f_widgets[i].c = custom_min[i][2];
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

    s_sim.in_dive = true;
    s_sim.surfacing_pending = false;
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

static void sim_finalize_dive(void)
{
    logbook_entry_t entry;
    dive_pt_t points[MAX_DIVE_LOG];
    uint8_t count = bus_get_dive_log_count();
    uint8_t active_gas = bus_get_gas_active_idx();
    uint8_t active_o2 = bus_get_gas_slot_o2_pct(active_gas);
    uint8_t active_he = bus_get_gas_slot_he_pct(active_gas);
    const char *mode = active_he > 0U ? "Trimix" : ((active_o2 == 21U) ? "Air" : "Nitrox");
    uint32_t start_min;
    uint32_t end_min;

    (void)memset(&entry, 0, sizeof(entry));
    for (uint8_t i = 0U; i < count; i++)
    {
        (void)bus_get_dive_log_point(i, &points[i]);
    }

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

    s_sim.in_dive = false;
    s_sim.surfacing_pending = false;
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
    if (!s_sim.in_dive)
    {
        if (depth_m >= SIM_DIVE_ENTRY_DEPTH_M)
        {
            sim_start_dive(depth_m);
        }
        else
        {
            s_sim.surface_time_s++;
            bus_set_surface_time(s_sim.surface_time_s);
            bus_set_dive_time(0U);
            return;
        }
    }

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
        s_sim.surfacing_pending = true;
        s_sim.surface_pending_s++;
        if (s_sim.surface_pending_s >= SIM_SURFACE_CONFIRM_S)
        {
            sim_finalize_dive();
        }
    }
    else
    {
        s_sim.surfacing_pending = false;
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
            current_depth_m = g_sensor_data.depth;
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

            s_sim.temp_offset += 1.0f;
            if (s_sim.temp_offset > 5.0f) {
                s_sim.temp_offset = -5.0f;
            }
            s_sim.temperature_c = 25.0f + s_sim.temp_offset;
            bus_set_temperature(s_sim.temperature_c);
            bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
            bus_set_prj_temperature(s_sim.temperature_c - 1.0f);
            sim_update_mlx_diagnostics(s_sim.dive_time_s);

            buhlmann_debug_tick(current_depth_m, s_sim.temperature_c, 1U);
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

    s_sim.heading_deg = (uint16_t)((s_sim.heading_deg + 1U) % 360U);
    bus_set_heading(s_sim.heading_deg);

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

    s_sim.temp_offset += 1.0f;
    if (s_sim.temp_offset > 5.0f) {
        s_sim.temp_offset = -5.0f;
    }
    s_sim.temperature_c = 25.0f + s_sim.temp_offset;
    bus_set_temperature(s_sim.temperature_c);
    bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
    bus_set_prj_temperature(s_sim.temperature_c - 1.0f);
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
        static const uint8_t s_tissue_raw[16] = {
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
            uint16_t gf_pct = (uint16_t)s_tissue_raw[i] * 100U / gf_high;
            tissue_gf[i] = (gf_pct > 200U) ? 200U : (uint8_t)gf_pct;
        }
        bus_set_tissue_loads(s_tissue_raw, tissue_gf);
    }

    sim_alert_tick();
}

void sim_data_start(void)
{
    if (s_sim_timer != NULL) {
        return;
    }

#if TCP_ALGO_DEBUG
    debug_link_pc_start();
    sim_alert_init();
    sim_seed_logbook_demo_if_empty();
#else
    sim_seed_original_defaults();
    sim_seed_logbook_demo_if_empty();
    sim_alert_init();
#endif

    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);

}
