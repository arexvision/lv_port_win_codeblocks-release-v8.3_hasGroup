#ifndef SIM_ALERT_POLICY_H
#define SIM_ALERT_POLICY_H

#include "../ui/alarm/alarm.h"
#include "../ui/core/data.h"
#include "lvgl/lvgl.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t depth_alarm_m;
    uint16_t time_alarm_min;
    uint16_t ndl_alarm_min;
    uint8_t battery_low_pct;
    uint8_t battery_dead_pct;
    float ppo2_warn;
    float ppo2_critical;
    uint8_t cns_warn_pct;
    uint16_t otu_warn;
    float ceiling_tolerance_m;
    float safety_stop_shallow_m;
    float safety_stop_deep_m;
    float ascent_rate_critical_mpm;
    uint32_t ascent_rate_critical_duration_ms;
    uint32_t algo_lock_duration_ms;
    uint32_t info_event_display_ms;
} sim_alert_config_t;

static sim_alert_config_t s_sim_alert_config =
{
    40U,
    60U,
    5U,
    20U,
    5U,
    1.40f,
    1.60f,
    80U,
    250U,
    0.6f,
    2.4f,
    6.0f,
    10.0f,
    5000U,
    180000U,
    3000U,
};

static bool s_sim_alert_auto_enabled = true;
static bool s_sim_alert_forced[ALARM_ID_COUNT];
static bool s_sim_alert_safety_seen_zone;
static uint32_t s_sim_alert_ceiling_broken_since_ms;
static bool s_sim_alert_algo_lock_active;
static bool s_sim_alert_stop_was_active;
static stop_type_t s_sim_alert_last_stop_type = STOP_NONE;
static float s_sim_alert_last_stop_depth_m;
static uint32_t s_sim_alert_stop_done_until_ms;
static bool s_sim_alert_safety_info_was_in_zone;
static uint32_t s_sim_alert_safety_info_until_ms;

static bool sim_alert_finite(float value)
{
    return !isnan(value) && !isinf(value);
}

static bool sim_alert_streq(const char *a, const char *b)
{
    if (!a || !b)
    {
        return false;
    }
    while (*a && *b)
    {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb)
        {
            return false;
        }
    }
    return *a == '\0' && *b == '\0';
}

static uint8_t sim_alert_clamp_pct(float value)
{
    if (!sim_alert_finite(value) || value <= 0.0f)
    {
        return 0U;
    }
    if (value >= 100.0f)
    {
        return 100U;
    }
    return (uint8_t)(value + 0.5f);
}

static void sim_alert_reset_runtime(void)
{
    s_sim_alert_safety_seen_zone = false;
    s_sim_alert_ceiling_broken_since_ms = 0U;
    s_sim_alert_algo_lock_active = false;
    s_sim_alert_stop_was_active = false;
    s_sim_alert_last_stop_type = STOP_NONE;
    s_sim_alert_last_stop_depth_m = 0.0f;
    s_sim_alert_stop_done_until_ms = 0U;
    s_sim_alert_safety_info_was_in_zone = false;
    s_sim_alert_safety_info_until_ms = 0U;
}

static void sim_alert_init(void)
{
    memset(s_sim_alert_forced, 0, sizeof(s_sim_alert_forced));
    s_sim_alert_auto_enabled = true;
    sim_alert_reset_runtime();
}

static void sim_alert_clear_all(void)
{
    memset(s_sim_alert_forced, 0, sizeof(s_sim_alert_forced));
    for (uint8_t id = 0U; id < ALARM_ID_COUNT; ++id)
    {
        (void)alarm_set_active((alarm_id_t)id, false);
    }
    alarm_clear_custom();
    sim_alert_reset_runtime();
}

static void sim_alert_clear_auto_active(void)
{
    for (uint8_t id = 0U; id < ALARM_ID_COUNT; ++id)
    {
        if (!s_sim_alert_forced[id])
        {
            (void)alarm_set_active((alarm_id_t)id, false);
        }
    }
    sim_alert_reset_runtime();
}

static void sim_alert_set_forced(alarm_id_t id, bool active)
{
    if (id >= ALARM_ID_COUNT)
    {
        return;
    }
    s_sim_alert_forced[id] = active;
    (void)alarm_set_active(id, active);
}

static bool sim_alert_alarm_id_from_text(const char *text, alarm_id_t *out_id)
{
    if (!text || !out_id)
    {
        return false;
    }

    if (sim_alert_streq(text, "asc") || sim_alert_streq(text, "ascent")) *out_id = ALARM_ID_CRIT_ASCENT_RATE;
    else if (sim_alert_streq(text, "po2") || sim_alert_streq(text, "ppo2")) *out_id = ALARM_ID_CRIT_PO2_MAX;
    else if (sim_alert_streq(text, "po2min") || sim_alert_streq(text, "ppo2min")) *out_id = ALARM_ID_CRIT_PO2_MIN;
    else if (sim_alert_streq(text, "po2w") || sim_alert_streq(text, "ppo2w")) *out_id = ALARM_ID_WARN_PO2_ELEVATED;
    else if (sim_alert_streq(text, "ceil") || sim_alert_streq(text, "ceiling")) *out_id = ALARM_ID_CRIT_CEIL_BROKEN;
    else if (sim_alert_streq(text, "lock") || sim_alert_streq(text, "algo")) *out_id = ALARM_ID_CRIT_ALGO_LOCK;
    else if (sim_alert_streq(text, "batt") || sim_alert_streq(text, "battery")) *out_id = ALARM_ID_WARN_BATTERY_LOW;
    else if (sim_alert_streq(text, "dead") || sim_alert_streq(text, "battdead")) *out_id = ALARM_ID_CRIT_BATTERY_DEAD;
    else if (sim_alert_streq(text, "ndl")) *out_id = ALARM_ID_WARN_NDL_LOW;
    else if (sim_alert_streq(text, "cns")) *out_id = ALARM_ID_WARN_CNS_HIGH;
    else if (sim_alert_streq(text, "otu")) *out_id = ALARM_ID_WARN_OTU_HIGH;
    else if (sim_alert_streq(text, "safety") || sim_alert_streq(text, "safe")) *out_id = ALARM_ID_WARN_SAFETY_BROKEN;
    else if (sim_alert_streq(text, "depth")) *out_id = ALARM_ID_WARN_DEPTH_LIMIT;
    else if (sim_alert_streq(text, "time")) *out_id = ALARM_ID_WARN_TIME_LIMIT;
    else if (sim_alert_streq(text, "ss") || sim_alert_streq(text, "sstop")) *out_id = ALARM_ID_INFO_SAFETY_STOP;
    else if (sim_alert_streq(text, "done") || sim_alert_streq(text, "stopdone")) *out_id = ALARM_ID_INFO_STOP_DONE;
    else return false;

    return true;
}

static const char *sim_alert_alarm_id_name(alarm_id_t id)
{
    switch (id)
    {
    case ALARM_ID_CRIT_ASCENT_RATE: return "asc";
    case ALARM_ID_CRIT_PO2_MAX: return "po2";
    case ALARM_ID_CRIT_PO2_MIN: return "po2min";
    case ALARM_ID_CRIT_CEIL_BROKEN: return "ceil";
    case ALARM_ID_CRIT_ALGO_LOCK: return "lock";
    case ALARM_ID_CRIT_BATTERY_DEAD: return "dead";
    case ALARM_ID_WARN_PO2_ELEVATED: return "po2w";
    case ALARM_ID_WARN_NDL_LOW: return "ndl";
    case ALARM_ID_WARN_CNS_HIGH: return "cns";
    case ALARM_ID_WARN_OTU_HIGH: return "otu";
    case ALARM_ID_WARN_SAFETY_BROKEN: return "safety";
    case ALARM_ID_WARN_DEPTH_LIMIT: return "depth";
    case ALARM_ID_WARN_TIME_LIMIT: return "time";
    case ALARM_ID_WARN_BATTERY_LOW: return "batt";
    case ALARM_ID_INFO_SAFETY_STOP: return "ss";
    case ALARM_ID_INFO_STOP_DONE: return "done";
    default: return NULL;
    }
}

static bool sim_alert_update_ascent_critical(uint32_t now_ms)
{
    const bool is_diving = g_sensor_data.depth > 0.8f || g_sensor_data.dive_time_s > 0U;
    float rate_mpm = g_sensor_data.ascent_rate;
    (void)now_ms;

    if (!is_diving || !sim_alert_finite(g_sensor_data.depth) || !sim_alert_finite(rate_mpm))
    {
        return false;
    }

    return rate_mpm > s_sim_alert_config.ascent_rate_critical_mpm;
}

static bool sim_alert_ceiling_broken(void)
{
    return sim_alert_finite(g_sensor_data.depth) &&
           sim_alert_finite(g_sensor_data.ceiling_m) &&
           g_sensor_data.ceiling_m > 0.0f &&
           (g_sensor_data.depth + s_sim_alert_config.ceiling_tolerance_m) < g_sensor_data.ceiling_m;
}

static bool sim_alert_update_algo_lock(bool ceiling_broken, uint32_t now_ms)
{
    if (!ceiling_broken)
    {
        s_sim_alert_ceiling_broken_since_ms = 0U;
        s_sim_alert_algo_lock_active = false;
        return false;
    }
    if (s_sim_alert_ceiling_broken_since_ms == 0U)
    {
        s_sim_alert_ceiling_broken_since_ms = now_ms;
    }
    s_sim_alert_algo_lock_active =
        (now_ms - s_sim_alert_ceiling_broken_since_ms) >=
        s_sim_alert_config.algo_lock_duration_ms;
    return s_sim_alert_algo_lock_active;
}

static bool sim_alert_update_safety_info(bool in_zone, uint32_t now_ms)
{
    if (in_zone && !s_sim_alert_safety_info_was_in_zone)
    {
        s_sim_alert_safety_info_until_ms = now_ms + s_sim_alert_config.info_event_display_ms;
    }
    if (!in_zone)
    {
        s_sim_alert_safety_info_was_in_zone = false;
        s_sim_alert_safety_info_until_ms = 0U;
        return false;
    }
    s_sim_alert_safety_info_was_in_zone = true;
    if (s_sim_alert_safety_info_until_ms == 0U)
    {
        return false;
    }
    if ((int32_t)(now_ms - s_sim_alert_safety_info_until_ms) >= 0)
    {
        s_sim_alert_safety_info_until_ms = 0U;
        return false;
    }
    return true;
}

static bool sim_alert_update_stop_done(uint32_t now_ms)
{
    const bool stop_active = g_sensor_data.stop_type != STOP_NONE &&
                             g_sensor_data.stop_time_left_s > 0U;
    const bool stop_finished =
        s_sim_alert_stop_was_active &&
        !stop_active;
    const bool deco_station_done =
        s_sim_alert_stop_was_active &&
        stop_active &&
        s_sim_alert_last_stop_type == STOP_DECO &&
        g_sensor_data.stop_type == STOP_DECO &&
        g_sensor_data.stop_depth_m < (s_sim_alert_last_stop_depth_m - 0.1f);

    if (stop_finished || deco_station_done)
    {
        s_sim_alert_stop_done_until_ms = now_ms + s_sim_alert_config.info_event_display_ms;
    }

    if (stop_active)
    {
        s_sim_alert_last_stop_type = g_sensor_data.stop_type;
        s_sim_alert_last_stop_depth_m = g_sensor_data.stop_depth_m;
    }
    else
    {
        s_sim_alert_last_stop_type = STOP_NONE;
        s_sim_alert_last_stop_depth_m = 0.0f;
    }
    s_sim_alert_stop_was_active = stop_active;

    if (s_sim_alert_stop_done_until_ms == 0U)
    {
        return false;
    }
    if ((int32_t)(now_ms - s_sim_alert_stop_done_until_ms) >= 0)
    {
        s_sim_alert_stop_done_until_ms = 0U;
        return false;
    }
    return true;
}

static void sim_alert_apply_auto(alarm_id_t id, bool active)
{
    if (id >= ALARM_ID_COUNT || s_sim_alert_forced[id])
    {
        return;
    }
    (void)alarm_set_active(id, active);
}

static void sim_alert_tick(void)
{
    const uint32_t now_ms = lv_tick_get();
    const uint8_t battery_pct = sim_alert_clamp_pct(g_sensor_data.battery_pct);
    const uint8_t active_gas = g_sensor_data.gas_active_idx < GAS_COUNT ? g_sensor_data.gas_active_idx : 0U;
    float active_ppo2 = g_sensor_data.ppo2[active_gas];
    const bool has_ppo2 = sim_alert_finite(active_ppo2) && active_ppo2 > 0.0f;
    const bool ceiling_broken = sim_alert_ceiling_broken();
    const bool safety_active = g_sensor_data.stop_type == STOP_SAFETY &&
                               g_sensor_data.stop_time_left_s > 0U &&
                               sim_alert_finite(g_sensor_data.depth);
    bool safety_in_zone = false;
    bool safety_broken = false;

    if (!s_sim_alert_auto_enabled)
    {
        return;
    }

    sim_alert_apply_auto(ALARM_ID_CRIT_BATTERY_DEAD,
                         battery_pct <= s_sim_alert_config.battery_dead_pct);
    sim_alert_apply_auto(ALARM_ID_WARN_BATTERY_LOW,
                         battery_pct <= s_sim_alert_config.battery_low_pct &&
                         battery_pct > s_sim_alert_config.battery_dead_pct);
    sim_alert_apply_auto(ALARM_ID_CRIT_ASCENT_RATE,
                         sim_alert_update_ascent_critical(now_ms));
    sim_alert_apply_auto(ALARM_ID_WARN_DEPTH_LIMIT,
                         sim_alert_finite(g_sensor_data.depth) &&
                         g_sensor_data.depth >= (float)bus_get_depth_alarm_m());
    sim_alert_apply_auto(ALARM_ID_WARN_TIME_LIMIT,
                         g_sensor_data.dive_time_s >= ((uint32_t)bus_get_time_alarm_min() * 60U));
    sim_alert_apply_auto(ALARM_ID_CRIT_PO2_MAX,
                         has_ppo2 && active_ppo2 >= s_sim_alert_config.ppo2_critical);
    sim_alert_apply_auto(ALARM_ID_WARN_PO2_ELEVATED,
                         has_ppo2 &&
                         active_ppo2 >= s_sim_alert_config.ppo2_warn &&
                         active_ppo2 < s_sim_alert_config.ppo2_critical);
    {
        uint16_t ndl_alarm_min = bus_get_ndl_alarm_min();
        sim_alert_apply_auto(ALARM_ID_WARN_NDL_LOW,
                             ndl_alarm_min > 0U &&
                             g_sensor_data.ndl >= 0 &&
                             g_sensor_data.ndl <= (int16_t)ndl_alarm_min &&
                             g_sensor_data.stop_type == STOP_NONE);
    }
    sim_alert_apply_auto(ALARM_ID_WARN_CNS_HIGH,
                         g_sensor_data.cns_pct >= s_sim_alert_config.cns_warn_pct);
    sim_alert_apply_auto(ALARM_ID_WARN_OTU_HIGH,
                         g_sensor_data.otu >= s_sim_alert_config.otu_warn);
    sim_alert_apply_auto(ALARM_ID_CRIT_CEIL_BROKEN, ceiling_broken);
    sim_alert_apply_auto(ALARM_ID_CRIT_ALGO_LOCK,
                         sim_alert_update_algo_lock(ceiling_broken, now_ms));

    if (safety_active)
    {
        safety_in_zone = g_sensor_data.depth >= s_sim_alert_config.safety_stop_shallow_m &&
                         g_sensor_data.depth <= s_sim_alert_config.safety_stop_deep_m;
        if (safety_in_zone)
        {
            s_sim_alert_safety_seen_zone = true;
        }
        safety_broken = s_sim_alert_safety_seen_zone &&
                        g_sensor_data.depth < s_sim_alert_config.safety_stop_shallow_m;
    }
    else
    {
        s_sim_alert_safety_seen_zone = false;
    }

    sim_alert_apply_auto(ALARM_ID_WARN_SAFETY_BROKEN, safety_broken);
    sim_alert_apply_auto(ALARM_ID_INFO_SAFETY_STOP,
                         sim_alert_update_safety_info(safety_in_zone, now_ms));
    sim_alert_apply_auto(ALARM_ID_INFO_STOP_DONE,
                         sim_alert_update_stop_done(now_ms));
}

#endif /* SIM_ALERT_POLICY_H */
