#include "buhlmann_debug.h"

#include "Buhlmann.h"
#include "rtthread.h"

#ifdef MAX_DECO_STOPS
#undef MAX_DECO_STOPS
#endif

extern "C" {
#include "../arex_ui/core/data.h"
#include "../arex_ui/core/ui_engine.h"
#include "../arex_ui/core/ui_state.h"
}

#include <math.h>
#include <stdio.h>
#include <string.h>

static Buhlmann s_buhlmann(62.7f);
static bool s_initialized = false;
static bool s_diving = false;
static uint8_t s_gf_low_pct = 40U;
static uint8_t s_gf_high_pct = 85U;
static uint8_t s_final_deco_stop_depth_m = (uint8_t)DECO_DEFAULT_FINAL_STOP_METERS;
static bool s_deco_plan_dirty = true;
static uint32_t s_deco_plan_elapsed_s = 0U;

#define AREX_DECO_PLAN_REFRESH_S 5U

static uint16_t clamp_u16_non_negative(int value)
{
    if (value <= 0) return 0;
    if (value >= 65535) return 65535;
    return (uint16_t)value;
}

static int16_t clamp_ndl_for_display(int minutes_to_deco)
{
    if (minutes_to_deco < 0) return 0;
    if (minutes_to_deco > 99) return 99;
    return (int16_t)minutes_to_deco;
}

static uint8_t ndl_bar_pct_for_display(const DiveInfo &dive_info)
{
    int ndl = dive_info.minutesToDeco;
    if (ndl < 0) ndl = 0;
    if (ndl > 99) ndl = 99;
    return (uint8_t)((ndl * 100) / 99);
}

static uint16_t display_minutes_from_seconds(int seconds)
{
    if (seconds <= 0) return 0U;
    if (seconds > 3932100) return 65535U;
    return (uint16_t)((seconds + 59) / 60);
}

static void sync_deco_plan_data(float depth_m, uint32_t delta_time_s)
{
    if (!s_deco_plan_dirty)
    {
        s_deco_plan_elapsed_s += delta_time_s;
        if (s_deco_plan_elapsed_s < AREX_DECO_PLAN_REFRESH_S)
        {
            return;
        }
    }

    s_deco_plan_dirty = false;
    s_deco_plan_elapsed_s = 0U;

    float plan_depth_m = g_sensor_data.max_depth;
    if (depth_m > plan_depth_m)
    {
        plan_depth_m = depth_m;
    }

    if (plan_depth_m < 3.0f || g_sensor_data.dive_time_s == 0U)
    {
        arex_bus_set_deco_plan(NULL, 0U);
        return;
    }

    DecoPlanConfig config;
    config.bottomDepthMeters = plan_depth_m;
    config.bottomTimeSeconds = (int)g_sensor_data.dive_time_s;
    if (config.bottomTimeSeconds < 60)
    {
        config.bottomTimeSeconds = 60;
    }
    config.descentRateMpm = 18.0f;
    config.ascentRateMpm = 10.0f;
    config.rmvLitersPerMinute = 14.0f;
    config.gfLow = (float)s_gf_low_pct / 100.0f;
    config.gfHigh = (float)s_gf_high_pct / 100.0f;
    config.finalStopDepthMeters = (float)s_final_deco_stop_depth_m;
    config.bottomPPO2 = 1.4f;
    config.decoPPO2 = 1.6f;

    for (int i = 0; i < MAX_GASES; i++)
    {
        config.gases[i] = s_buhlmann.getGas(i);
    }
    config.gases[0].enabled = true;

    DecoPlanResult result;
    arex_deco_stop_t plan_stops[MAX_DECO_STOPS];
    uint8_t stop_count = 0U;

    if (s_buhlmann.planDive(config, result))
    {
        for (int i = 0; i < result.entryCount && stop_count < MAX_DECO_STOPS; i++)
        {
            const DecoPlanEntry &entry = result.entries[i];
            if (entry.entryType != DECO_PLAN_ENTRY_DECO_STOP)
            {
                continue;
            }

            plan_stops[stop_count].depth_m = entry.depthMeters;
            plan_stops[stop_count].stay_min = (float)display_minutes_from_seconds(entry.timeSeconds);
            stop_count++;
        }
    }

    arex_bus_set_deco_plan(plan_stops, stop_count);
}

static void format_gas_name(const Gas &gas, char *name_buf, size_t name_buf_size)
{
    uint8_t o2_pct = (uint8_t)(gas.oxygenFraction * 100.0f + 0.5f);
    uint8_t he_pct = (uint8_t)(gas.heliumFraction * 100.0f + 0.5f);

    if (he_pct > 0U) {
        snprintf(name_buf, name_buf_size, "TX %u/%u", (unsigned)o2_pct, (unsigned)he_pct);
    } else if (o2_pct == 21U) {
        snprintf(name_buf, name_buf_size, "AIR");
    } else if (o2_pct == 100U) {
        snprintf(name_buf, name_buf_size, "O2 100%%");
    } else {
        snprintf(name_buf, name_buf_size, "NX %u", (unsigned)o2_pct);
    }
}

static void sync_core_data(const DiveInfo &dive_info, float depth_m)
{
    uint8_t tissue_load[16];
    float current_pressure = s_buhlmann.calculateHydrostaticPressureFromDepth(depth_m);
    float surface_pressure_bar = s_buhlmann.getSurfacePressure() / 1000.0f;

    for (int i = 0; i < 16; i++) {
        /* Per-compartment SurfGF: reaches GF High when this tissue drives NDL to zero. */
        float tissue_pressure_bar = s_buhlmann.getCompartmentTotalInertLoad(i) / 1000.0f;
        float m_value_bar = s_buhlmann.getCompartmentCombinedA(i) +
                            surface_pressure_bar / s_buhlmann.getCompartmentCombinedB(i);
        float denominator = m_value_bar - surface_pressure_bar;
        float load_percent = 0.0f;

        if (denominator > 0.0001f) {
            load_percent = ((tissue_pressure_bar - surface_pressure_bar) / denominator) * 100.0f;
        }
        if (load_percent > 200.0f) load_percent = 200.0f;
        if (load_percent < 0.0f) load_percent = 0.0f;
        tissue_load[i] = (uint8_t)(load_percent + 0.5f);
    }

    arex_bus_set_tissues(tissue_load);
    arex_bus_set_cns((uint8_t)dive_info.cns);
    arex_bus_set_otu((uint16_t)dive_info.otu);
    arex_bus_set_gf99(dive_info.gf99);
    arex_bus_set_surf_gf(dive_info.surfGF);
    arex_bus_set_gf_setting((uint8_t)(s_buhlmann.getGFLow() * 100.0f),
                            (uint8_t)(s_buhlmann.getGFHigh() * 100.0f));

    float mod_m = s_buhlmann.calculateMOD(g_sys_config.mod_ppo2);
    arex_bus_set_mod(mod_m);

    float ceiling_m = s_buhlmann.calculateDepthFromPressure(s_buhlmann.getLastCeilingPressure());
    arex_bus_set_ceiling(ceiling_m);

    Gas active_gas = s_buhlmann.getGas(s_buhlmann.getActiveGas());
    uint8_t o2_pct = (uint8_t)(active_gas.oxygenFraction * 100.0f + 0.5f);
    uint8_t he_pct = (uint8_t)(active_gas.heliumFraction * 100.0f + 0.5f);
    arex_bus_set_gas_mix(o2_pct, he_pct);

    float surface_pressure = s_buhlmann.getSurfacePressure();
    float n2_fraction = 1.0f - active_gas.oxygenFraction - active_gas.heliumFraction;
    float gas_density = (active_gas.oxygenFraction * 1.429f +
                         n2_fraction * 1.251f +
                         active_gas.heliumFraction * 0.179f) *
                        (current_pressure / surface_pressure);
    arex_bus_set_gas_density(gas_density);

    float fio2_pct = (active_gas.oxygenFraction * current_pressure / surface_pressure) * 100.0f;
    arex_bus_set_fio2(fio2_pct);

    uint16_t tts_val = (uint16_t)(dive_info.ttsSeconds / 60);
    if (tts_val > 9999U) tts_val = 9999U;
    arex_bus_set_tts(tts_val);
}

static void sync_stop_data(const DiveInfo &dive_info)
{
    static bool s_deco_bar_active = false;
    static float s_deco_bar_depth_m = 0.0f;
    static uint16_t s_deco_bar_total_s = 0U;

    arex_stop_type_t stop_type = STOP_NONE;
    if (dive_info.stopType == BUHLMANN_STOP_SAFETY) {
        stop_type = STOP_SAFETY;
    } else if (dive_info.stopType == BUHLMANN_STOP_DECO) {
        stop_type = STOP_DECO;
    }

    int16_t ndl_display_min = (stop_type == STOP_DECO) ? 0 : clamp_ndl_for_display(dive_info.minutesToDeco);
    uint16_t stop_total_s = clamp_u16_non_negative(dive_info.stopTimeTotalSeconds);
    uint16_t stop_left_s = clamp_u16_non_negative(dive_info.stopTimeRemainingSeconds);

    if (stop_type == STOP_DECO) {
        if (!dive_info.inStopZone) {
            s_deco_bar_active = false;
            s_deco_bar_total_s = stop_left_s;
            s_deco_bar_depth_m = dive_info.stopDepthMeters;
            stop_total_s = stop_left_s;
        } else if (!s_deco_bar_active || fabsf(s_deco_bar_depth_m - dive_info.stopDepthMeters) > 0.1f) {
            s_deco_bar_active = true;
            s_deco_bar_depth_m = dive_info.stopDepthMeters;
            s_deco_bar_total_s = (stop_left_s > 0U) ? stop_left_s : 1U;
            stop_total_s = s_deco_bar_total_s;
        } else {
            if (stop_left_s > s_deco_bar_total_s) {
                s_deco_bar_total_s = stop_left_s;
            }
            stop_total_s = s_deco_bar_total_s;
        }
    } else {
        s_deco_bar_active = false;
        s_deco_bar_total_s = 0U;
        s_deco_bar_depth_m = 0.0f;
    }

    arex_bus_update_deco(ndl_display_min,
                         stop_type,
                         dive_info.stopDepthMeters,
                         stop_total_s,
                         stop_left_s,
                         dive_info.inStopZone);
    if (stop_type == STOP_NONE) {
        arex_bus_set_ndl_bar_pct(ndl_bar_pct_for_display(dive_info));
    }

}

static void handle_pending_gas_switch(float depth_m)
{
    uint8_t target_gas_idx = 0;
    if (!arex_has_pending_gas_switch(&target_gas_idx)) {
        return;
    }

    if (target_gas_idx < MAX_GASES) {
        Gas target_gas = s_buhlmann.getGas(target_gas_idx);
        bool mod_ok = target_gas.enabled && depth_m <= target_gas.modDepth;
        bool icd_risk = s_buhlmann.checkICDRisk(target_gas_idx, depth_m);
        if (mod_ok && !icd_risk) {
            s_buhlmann.setActiveGas(target_gas_idx);
        }
    }

    arex_clear_gas_switch_cmd();
}

static void sync_gas_data(float current_pressure)
{
    int active_gas_idx = s_buhlmann.getActiveGas();
    Gas active_gas = s_buhlmann.getGas(active_gas_idx);
    char gas_name_buf[16];

    if (active_gas.enabled) {
        format_gas_name(active_gas, gas_name_buf, sizeof(gas_name_buf));
        arex_bus_set_gas((uint8_t)active_gas_idx, gas_name_buf);
    }

    for (int i = 0; i < MAX_GASES; i++) {
        Gas gas = s_buhlmann.getGas(i);
        if (gas.enabled) {
            float ppo2 = gas.oxygenFraction * (current_pressure / 1013.25f);
            arex_bus_set_ppo2((uint8_t)i, ppo2);
        }

        uint8_t o2_pct = (uint8_t)(gas.oxygenFraction * 100.0f + 0.5f);
        uint8_t he_pct = (uint8_t)(gas.heliumFraction * 100.0f + 0.5f);
        format_gas_name(gas, gas_name_buf, sizeof(gas_name_buf));
        arex_bus_set_gas_slot((uint8_t)i, gas_name_buf, o2_pct, he_pct, gas.modDepth);
    }
    arex_bus_set_gas_slot_count(MAX_GASES);
}

void buhlmann_debug_init(void)
{
    if (s_initialized) {
        return;
    }

    rt_kprintf("Initializing Buhlmann algorithm...\n");

    s_buhlmann.setSeaLevelAtmosphericPressure(1000.0f);
    s_buhlmann.setNitrogenRateInGas(0.79f);
    s_buhlmann.setGFLow((float)s_gf_low_pct / 100.0f);
    s_buhlmann.setGFHigh((float)s_gf_high_pct / 100.0f);

    s_buhlmann.setGas(0, 0.21f, 0.0f, true, 1.4f);
    s_buhlmann.setGas(1, 0.50f, 0.0f, true, 1.6f);
    s_buhlmann.setGas(2, 1.00f, 0.0f, true, 1.6f);
    s_buhlmann.setActiveGas(0);
    s_buhlmann.setOxygenRateInGas(0.21f);
    s_buhlmann.setFinalStopDepth((float)s_final_deco_stop_depth_m);

    DiveResult *initial_result = s_buhlmann.initializeCompartments();
    s_buhlmann.startDive(initial_result, 0U);
    s_buhlmann.resetCNS();
    s_buhlmann.resetOTU();

    rt_kprintf("Buhlmann algorithm initialized (GF: %d/%d)\n",
               (int)(s_buhlmann.getGFLow() * 100),
               (int)(s_buhlmann.getGFHigh() * 100));

    s_initialized = true;
}

void buhlmann_debug_set_final_stop_depth(uint8_t depth_m)
{
    s_final_deco_stop_depth_m = (depth_m == 6U) ? 6U : 3U;
    s_buhlmann.setFinalStopDepth((float)s_final_deco_stop_depth_m);
    s_deco_plan_dirty = true;
    rt_kprintf("[DIVE_SETUP] Last deco stop: %um\n", (unsigned)s_final_deco_stop_depth_m);
}

void buhlmann_debug_set_gf(uint8_t gf_low_pct, uint8_t gf_high_pct)
{
    if (gf_low_pct > 100U) gf_low_pct = 100U;
    if (gf_high_pct > 100U) gf_high_pct = 100U;

    s_gf_low_pct = gf_low_pct;
    s_gf_high_pct = gf_high_pct;

    if (!s_initialized) {
        buhlmann_debug_init();
    }

    s_buhlmann.setGFLow((float)s_gf_low_pct / 100.0f);
    s_buhlmann.setGFHigh((float)s_gf_high_pct / 100.0f);
    arex_bus_set_gf_setting(s_gf_low_pct, s_gf_high_pct);
    s_deco_plan_dirty = true;
    rt_kprintf("[DIVE_SETUP] GF: %u/%u\n", (unsigned)s_gf_low_pct, (unsigned)s_gf_high_pct);
}

void buhlmann_debug_reset(void)
{
    s_buhlmann = Buhlmann(62.7f);
    s_initialized = false;
    s_diving = false;
    s_deco_plan_dirty = true;
    s_deco_plan_elapsed_s = 0U;
    buhlmann_debug_init();
}

void buhlmann_debug_tick(float depth_m, float temperature_c, uint32_t delta_time_s)
{
    (void)temperature_c;

    if (!s_initialized) {
        buhlmann_debug_init();
    }

    if (delta_time_s == 0U) {
        delta_time_s = 1U;
    }
    if (depth_m < 0.0f) {
        depth_m = 0.0f;
    }

    if (depth_m > 1.2f) {
        s_diving = true;
    } else if (depth_m < 0.8f) {
        s_diving = false;
    }

    float current_pressure = s_buhlmann.calculateHydrostaticPressureFromDepth(depth_m);
    DiveInfo dive_info = s_buhlmann.progressDive(current_pressure, delta_time_s);

    dive_info.ppo2 = s_buhlmann.calculateOxygenPartialPressure(current_pressure);
    if (s_diving) {
        float time_in_minutes = (float)delta_time_s / 60.0f;
        s_buhlmann.updateCNS(dive_info.ppo2, time_in_minutes);
        s_buhlmann.updateOTU(dive_info.ppo2, time_in_minutes);
    }
    dive_info.cns = s_buhlmann.getCumulativeCNS();
    dive_info.otu = s_buhlmann.getCumulativeOTU();
    dive_info.gf99 = s_buhlmann.calculateGF99();
    dive_info.surfGF = s_buhlmann.calculateSurfaceGF();
    s_buhlmann.getCurrentCompartmentPressures(dive_info.compartmentPressures);

    handle_pending_gas_switch(depth_m);
    sync_core_data(dive_info, depth_m);
    sync_stop_data(dive_info);
    sync_deco_plan_data(depth_m, delta_time_s);
    sync_gas_data(current_pressure);
}
