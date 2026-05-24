#include "sim_data.h"

#include "../arex_ui/alarm/alarm.h"
#include "../arex_ui/core/data.h"
#include "../arex_algo_sim/buhlmann_debug.h"
#ifndef PC_SIMULATOR
#define PC_SIMULATOR
#endif
#define AREX_DEBUG_LINK_PC_IMPLEMENTATION
#include "debug_link_pc.h"
#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <string.h>

#ifndef AREX_TCP_ALGO_DEBUG
#define AREX_TCP_ALGO_DEBUG 1
#endif

static lv_timer_t *s_sim_timer;
static lv_timer_t *s_l1_alarm_timer;

typedef struct
{
    uint16_t heading_deg;
    uint32_t dive_time_s;
    uint32_t surface_time_s;
    float depth_m;
    uint16_t tts_min;
    uint8_t cns_pct;
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
} arex_sim_state_t;

static arex_sim_state_t s_sim = {
    .heading_deg = 0,
    .dive_time_s = 0,
    .surface_time_s = 0,
    .depth_m = 0.0f,
    .tts_min = 0,
    .cns_pct = 0,
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
};

static float arex_sim_calc_ppo2(uint8_t o2_pct, float depth_m)
{
    float ambient_bar = 1.0f + (depth_m / 10.0f);
    if (ambient_bar < 1.0f) {
        ambient_bar = 1.0f;
    }
    return ((float)o2_pct / 100.0f) * ambient_bar;
}

static void arex_sim_seed_original_defaults(void)
{
    arex_bus_set_gas_slot(0, "AIR", 21, 0, 56.0f);
    arex_bus_set_gas_slot(1, "NX 32", 32, 0, 33.0f);
    arex_bus_set_gas_slot(2, "O2 100%", 100, 0, 6.0f);
    arex_bus_set_gas_slot_count(3);
    arex_bus_set_gas(0, "AIR");
    arex_bus_set_pod(0, 200.0f);
    arex_bus_set_pod(1, 185.0f);
    arex_bus_set_gf_setting(30, 70);
    arex_bus_set_surf_gf(85.0f);
    arex_bus_set_gf99(42.0f);
    arex_bus_set_mod(33.0f);
    arex_bus_set_ceiling(0.0f);
    arex_bus_set_gas_mix(21, 0);
    arex_bus_set_gas_density(5.2f);
    arex_bus_set_fio2(21.0f);
}

#if AREX_TCP_ALGO_DEBUG
static void arex_sim_seed_tcp_algo_defaults(void)
{
    arex_bus_set_gas_slot(0, "AIR", 21, 0, 56.0f);
    arex_bus_set_gas_slot(1, "NX 32", 32, 0, 33.0f);
    arex_bus_set_gas_slot(2, "O2 100%", 100, 0, 6.0f);
    arex_bus_set_gas_slot_count(3);
    arex_bus_set_gas(0, "AIR");
    arex_bus_set_pod(0, 200.0f);
    arex_bus_set_pod(1, 185.0f);
    arex_bus_set_gf_setting(30, 70);
    arex_bus_set_surf_gf(0.0f);
    arex_bus_set_gf99(0.0f);
    arex_bus_set_mod(56.0f);
    arex_bus_set_ceiling(0.0f);
    arex_bus_set_gas_mix(21, 0);
    arex_bus_set_gas_density(1.2f);
    arex_bus_set_fio2(21.0f);
    arex_bus_update_deco(99, STOP_NONE, 0.0f, 0U, 0U, false);
}

static void arex_sim_reset_for_tcp_debug(void)
{
    memset(&s_sim, 0, sizeof(s_sim));
    s_sim.battery_pct = 86.0f;
    s_sim.temperature_c = 25.0f;

    arex_data_init();
    arex_dive_log_reset();
    arex_bus_set_depth(0.0f);
    arex_bus_set_ascent_rate(0.0f);
    arex_bus_set_dive_time(0U);
    arex_bus_set_surface_time(0U);
    arex_bus_set_battery(s_sim.battery_pct);
    arex_bus_set_temperature(s_sim.temperature_c);
    arex_bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
    arex_bus_set_prj_temperature(s_sim.temperature_c - 1.0f);
    arex_sim_seed_tcp_algo_defaults();
    buhlmann_debug_reset();

    arex_bus_requeue_dirty(0xFFFFFFFFU);
}
#endif

static void arex_test_set_ui_layout(uint8_t phase)
{
    static arex_ble_ui_sync_payload_t s_payload;
    static const uint8_t s_default_card_order[8] = {
        CARD_ID_COMPASS,
        CARD_ID_DECO,
        CARD_ID_PLAN,
        CARD_ID_GAS,
        CARD_ID_CUSTOM_GRID,
        CARD_ID_BLANK,
        CARD_ID_UNUSED,
        CARD_ID_UNUSED,
    };

    memset(&s_payload, 0, sizeof(s_payload));
    s_payload.version = AREX_BLE_CFG_VERSION;
    memcpy(s_payload.card_order, s_default_card_order, sizeof(s_default_card_order));

    if (phase == 0) {
        uint8_t left_def[][3] = {
            { COMP_NDL_STOP_1606,  0, 0 },
            { COMP_DEPTH_1606,     0, 1 },
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

    arex_bus_set_ui_layout(&s_payload);
}

static void arex_sim_update_depth_script(void)
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

static void arex_sim_update_deco_state(void)
{
    if (s_sim.depth_m < 10.0f) {
        arex_bus_update_deco(45, STOP_NONE, 0.0f, 0, 0, false);
        return;
    }

    if (s_sim.depth_phase == 1U) {
        arex_bus_update_deco(12, STOP_SAFETY, 5.0f, 180, 180, false);
        return;
    }

    if (s_sim.depth_phase == 2U && s_sim.depth_m <= 6.5f) {
        uint16_t left_s = (s_sim.phase_tick < 5U) ? (uint16_t)(180U - (s_sim.phase_tick * 30U)) : 0U;
        arex_bus_update_deco(12, STOP_SAFETY, 5.0f, 180, left_s, true);
        return;
    }

    arex_bus_update_deco(0, STOP_DECO, 6.0f, 300, 120, false);
}

static void sim_tick_cb(lv_timer_t *t)
{
    float current_depth_m;

    (void)t;

#if AREX_TCP_ALGO_DEBUG
    if (arex_debug_link_pc_consume_connect_event()) {
        arex_sim_reset_for_tcp_debug();
    }

    if (!arex_debug_link_pc_manual_mode()) {
        return;
    }

    {
        uint16_t time_scale = arex_debug_link_pc_time_scale();
        for (uint16_t tick = 0; tick < time_scale; tick++) {
            if (s_sim.dive_time_s < g_sensor_data.dive_time_s) {
                s_sim.dive_time_s = g_sensor_data.dive_time_s;
            }
            if (s_sim.surface_time_s < g_sensor_data.surface_time_s) {
                s_sim.surface_time_s = g_sensor_data.surface_time_s;
            }

            s_sim.dive_time_s++;
            arex_bus_set_dive_time(s_sim.dive_time_s);

            s_sim.surface_time_s++;
            arex_bus_set_surface_time(s_sim.surface_time_s);

            current_depth_m = g_sensor_data.depth;
            s_sim.depth_m = current_depth_m;
            arex_dive_log_append((float)s_sim.dive_time_s, current_depth_m);

            if (s_sim.rate_sample_valid) {
                arex_bus_set_ascent_rate((s_sim.rate_sample_depth_m - current_depth_m) * 60.0f);
            } else {
                arex_bus_set_ascent_rate(0.0f);
                s_sim.rate_sample_valid = true;
            }
            s_sim.rate_sample_depth_m = current_depth_m;

            s_sim.battery_pct += 1.2f;
            arex_bus_set_battery(s_sim.battery_pct);

            s_sim.temp_offset += 1.0f;
            if (s_sim.temp_offset > 5.0f) {
                s_sim.temp_offset = -5.0f;
            }
            s_sim.temperature_c = 25.0f + s_sim.temp_offset;
            arex_bus_set_temperature(s_sim.temperature_c);
            arex_bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
            arex_bus_set_prj_temperature(s_sim.temperature_c - 1.0f);

            buhlmann_debug_tick(current_depth_m, s_sim.temperature_c, 1U);
        }
    }
    return;
#endif

    s_sim.layout_tick++;
    // arex_test_set_ui_layout(s_sim.layout_phase);
    s_sim.layout_phase = (uint8_t)(1U - s_sim.layout_phase);

    s_sim.heading_deg = (uint16_t)((s_sim.heading_deg + 1U) % 360U);
    arex_bus_set_heading(s_sim.heading_deg);

    if (s_sim.dive_time_s < g_sensor_data.dive_time_s) {
        s_sim.dive_time_s = g_sensor_data.dive_time_s;
    }
    if (s_sim.surface_time_s < g_sensor_data.surface_time_s) {
        s_sim.surface_time_s = g_sensor_data.surface_time_s;
    }

    s_sim.dive_time_s++;
    arex_bus_set_dive_time(s_sim.dive_time_s);

    s_sim.surface_time_s++;
    arex_bus_set_surface_time(s_sim.surface_time_s);

    arex_sim_update_depth_script();
    arex_sim_update_deco_state();
    current_depth_m = s_sim.depth_m;
    arex_dive_log_append((float)s_sim.dive_time_s, current_depth_m);
    arex_bus_set_depth(current_depth_m);

    if (s_sim.rate_sample_valid) {
        arex_bus_set_ascent_rate((s_sim.rate_sample_depth_m - current_depth_m) * 60.0f);
    } else {
        arex_bus_set_ascent_rate(0.0f);
        s_sim.rate_sample_valid = true;
    }
    s_sim.rate_sample_depth_m = current_depth_m;

    s_sim.battery_pct += 1.2f;
    arex_bus_set_battery(s_sim.battery_pct);

    s_sim.temp_offset += 1.0f;
    if (s_sim.temp_offset > 5.0f) {
        s_sim.temp_offset = -5.0f;
    }
    s_sim.temperature_c = 25.0f + s_sim.temp_offset;
    arex_bus_set_temperature(s_sim.temperature_c);
    arex_bus_set_bat_temperature(s_sim.temperature_c + 1.0f);
    arex_bus_set_prj_temperature(s_sim.temperature_c - 1.0f);

    if (current_depth_m > 12.0f) {
        arex_deco_stop_t sim_stops[] = {
            { .depth_m = 9.0f, .stay_min = 2.0f },
            { .depth_m = 6.0f, .stay_min = 3.0f },
            { .depth_m = 3.0f, .stay_min = 1.0f },
        };
        arex_bus_set_deco_plan(sim_stops, 3);
    }

    s_sim.tts_min++;
    arex_bus_set_tts(s_sim.tts_min);

    if (s_sim.cns_pct < 100U) {
        s_sim.cns_pct++;
        arex_bus_set_cns(s_sim.cns_pct);
    }

    s_sim.otu++;
    arex_bus_set_otu(s_sim.otu);

    for (uint8_t i = 0; i < GAS_COUNT; i++) {
        arex_bus_set_ppo2(i, arex_sim_calc_ppo2(g_sensor_data.gas_slot_o2_pct[i], current_depth_m));
    }

    {
        static const uint8_t s_tissue[16] = {
            20, 30, 40, 50, 60, 65, 70, 72,
            74, 76, 78, 80, 82, 85, 88, 90
        };
        arex_bus_set_tissues(s_tissue);
    }
}

static void sim_l1_alarm_timer_cb(lv_timer_t *t)
{
    arex_alarm_set_active(AREX_ALARM_ID_INFO_SAFETY_STOP, true);
    s_l1_alarm_timer = NULL;
    lv_timer_del(t);
}

void arex_sim_data_start(void)
{
    if (s_sim_timer != NULL) {
        return;
    }

#if AREX_TCP_ALGO_DEBUG
    arex_debug_link_pc_start();
#else
    arex_sim_seed_original_defaults();
#endif

    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);


    // s_l1_alarm_timer = lv_timer_create(sim_l1_alarm_timer_cb, 1000, NULL);

}
