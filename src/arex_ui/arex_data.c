#include "arex_data.h"
#include <math.h>
#include <string.h>

/* =========================================================
 * Data Bus Setter 实现 — 硬件/模拟层专用
 * 铁律：仅更新数值 + 打脏标记，绝不碰 LVGL！
 * ========================================================= */

void arex_bus_set_depth(float depth_m)
{
    /* 防抖：只有变化超过 0.05m 才触发 UI 刷新，极大节省 CPU */
    if (fabsf(g_sensor_data.depth - depth_m) > 0.05f) {
        g_sensor_data.depth = depth_m;
        g_sensor_data.dirty_mask |= DIRTY_DEPTH;
    }
}

void arex_bus_set_ndl(int16_t ndl_min)
{
    if (g_sensor_data.ndl != ndl_min) {
        g_sensor_data.ndl = ndl_min;
        g_sensor_data.dirty_mask |= DIRTY_NDL | DIRTY_DECO;
    }
}

void arex_bus_set_tts(uint16_t tts_min)
{
    if (g_sensor_data.tts != tts_min) {
        g_sensor_data.tts = tts_min;
        g_sensor_data.dirty_mask |= DIRTY_TTS | DIRTY_DECO;
    }
}

void arex_bus_set_pod(uint8_t pod_idx, float bar)
{
    if (pod_idx == 0 && g_sensor_data.pod1_bar != bar) {
        g_sensor_data.pod1_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    } else if (pod_idx == 1 && g_sensor_data.pod2_bar != bar) {
        g_sensor_data.pod2_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    }
}

void arex_bus_set_battery(float pct)
{
    if (fabsf(g_sensor_data.battery_pct - pct) > 0.1f) {
        g_sensor_data.battery_pct = pct;
        g_sensor_data.dirty_mask |= DIRTY_BATT;
    }
}

void arex_bus_set_heading(uint16_t heading_deg)
{
    if (g_sensor_data.heading != heading_deg) {
        g_sensor_data.heading = heading_deg;
        g_sensor_data.dirty_mask |= DIRTY_HEADING;
    }
}

void arex_bus_set_dive_time(uint32_t dive_s)
{
    if (g_sensor_data.dive_time_s != dive_s) {
        g_sensor_data.dive_time_s = dive_s;
        g_sensor_data.dirty_mask |= DIRTY_TIME | DIRTY_CHART;
    }
}

void arex_bus_set_surface_time(uint32_t surface_s)
{
    if (g_sensor_data.surface_time_s != surface_s) {
        g_sensor_data.surface_time_s = surface_s;
        g_sensor_data.dirty_mask |= DIRTY_TIME;
    }
}

void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val)
{
    if (sensor_idx < 3 && g_sensor_data.ppo2[sensor_idx] != ppo2_val) {
        g_sensor_data.ppo2[sensor_idx] = ppo2_val;
        g_sensor_data.dirty_mask |= DIRTY_PPO2;
    }
}

void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name)
{
    if (g_sensor_data.gas_active_idx != gas_idx) {
        g_sensor_data.gas_active_idx = gas_idx;
    }
    if (gas_name != NULL && strncmp(g_sensor_data.gas_name, gas_name, 15) != 0) {
        strncpy(g_sensor_data.gas_name, gas_name, 15);
        g_sensor_data.gas_name[15] = '\0';
    }
    g_sensor_data.dirty_mask |= DIRTY_GAS;
}

void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min)
{
    if (g_sensor_data.next_stop_m != stop_m || g_sensor_data.next_stop_min != stop_min) {
        g_sensor_data.next_stop_m = stop_m;
        g_sensor_data.next_stop_min = stop_min;
        g_sensor_data.dirty_mask |= DIRTY_DECO;
    }
}

void arex_bus_set_cns(uint8_t cns_pct)
{
    if (g_sensor_data.cns_pct != cns_pct) {
        g_sensor_data.cns_pct = cns_pct;
        g_sensor_data.dirty_mask |= DIRTY_DECO;
    }
}

void arex_bus_set_otu(uint16_t otu_val)
{
    if (g_sensor_data.otu != otu_val) {
        g_sensor_data.otu = otu_val;
        g_sensor_data.dirty_mask |= DIRTY_DECO;
    }
}

void arex_bus_set_chart_refresh(void)
{
    g_sensor_data.dirty_mask |= DIRTY_CHART;
}

void arex_bus_clear_all_dirty(void)
{
    g_sensor_data.dirty_mask = DIRTY_NONE;
}

void arex_bus_set_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.temperature_c - temp_c) > 0.1f) {
        g_sensor_data.temperature_c = temp_c;
        g_sensor_data.dirty_mask |= DIRTY_TEMP;
    }
}

void arex_bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count)
{
    if (g_sensor_data.strobe_on != strobe_on ||
        g_sensor_data.flashlight_on != flashlight_on ||
        g_sensor_data.cylinder_count != cylinder_count) {
        g_sensor_data.strobe_on = strobe_on;
        g_sensor_data.flashlight_on = flashlight_on;
        g_sensor_data.cylinder_count = cylinder_count;
        g_sensor_data.dirty_mask |= DIRTY_DEVICES;
    }
}
