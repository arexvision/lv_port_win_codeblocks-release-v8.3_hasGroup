#include "buhlmann_debug.h"

#include "Buhlmann.h"
#include "rtthread.h"

#ifdef MAX_DECO_STOPS
#undef MAX_DECO_STOPS
#endif

extern "C" {
#include "../ui/core/data.h"
#include "../ui/core/ui_engine.h"
#include "../ui/core/ui_state.h"
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

static WaterType water_type_from_salinity_mode(uint8_t mode)
{
    if (mode == 1U) return WATER_SALT;
    if (mode == 2U) return WATER_EN13319;
    return WATER_FRESH;
}

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

static uint16_t round_u16_non_negative(float value)
{
    if (value <= 0.0f) return 0U;
    if (value >= 65535.0f) return 65535U;
    return (uint16_t)(value + 0.5f);
}

static void fill_plan_config_from_ui(float depth_m,
                                     uint16_t bottom_time_min,
                                     float rmv_lpm,
                                     DecoPlanConfig &config)
{
    config = DecoPlanConfig();

    if (depth_m < 3.0f) depth_m = 3.0f;
    if (depth_m > 120.0f) depth_m = 120.0f;
    if (bottom_time_min < 1U) bottom_time_min = 1U;
    if (bottom_time_min > 300U) bottom_time_min = 300U;
    if (rmv_lpm < 5.0f) rmv_lpm = 5.0f;
    if (rmv_lpm > 50.0f) rmv_lpm = 50.0f;

    uint8_t gf_low = g_sensor_data.gf_low ? g_sensor_data.gf_low : s_gf_low_pct;
    uint8_t gf_high = g_sensor_data.gf_high ? g_sensor_data.gf_high : s_gf_high_pct;
    if (gf_low > 100U) gf_low = 100U;
    if (gf_high > 100U) gf_high = 100U;

    config.bottomDepthMeters = depth_m;
    config.bottomTimeSeconds = (int)bottom_time_min * 60;
    config.descentRateMpm = 18.0f;
    config.ascentRateMpm = 10.0f;
    config.rmvLitersPerMinute = rmv_lpm;
    config.gfLow = (float)gf_low / 100.0f;
    config.gfHigh = (float)gf_high / 100.0f;
    config.finalStopDepthMeters = (float)s_final_deco_stop_depth_m;
    config.bottomPPO2 = 1.4f;
    config.decoPPO2 = 1.6f;

    for (int i = 0; i < MAX_GASES; i++)
    {
        config.gases[i] = Gas(0.21f, 0.0f);
        config.gases[i].enabled = false;
    }

    uint8_t gas_count = g_sensor_data.gas_slot_count;
    uint8_t valid_count = 0U;
    if (gas_count > GAS_COUNT) gas_count = GAS_COUNT;
    for (uint8_t i = 0; i < gas_count && valid_count < MAX_GASES; i++)
    {
        uint8_t o2 = g_sensor_data.gas_slot_o2_pct[i];
        uint8_t he = g_sensor_data.gas_slot_he_pct[i];
        if (o2 == 0U || o2 > 100U || he > 100U || (uint16_t)o2 + (uint16_t)he > 100U)
        {
            continue;
        }
        config.gases[valid_count] = Gas((float)o2 / 100.0f, (float)he / 100.0f);
        config.gases[valid_count].enabled = true;
        valid_count++;
    }

    if (valid_count == 0U)
    {
        config.gases[0] = Gas(0.21f, 0.0f);
        config.gases[0].enabled = true;
    }
}

static buhlmann_debug_plan_row_type_t map_plan_row_type(DecoPlanEntryType type)
{
    switch (type)
    {
    case DECO_PLAN_ENTRY_ASCENT:
        return BUHLMANN_DEBUG_PLAN_ROW_ASCENT;
    case DECO_PLAN_ENTRY_DECO_STOP:
        return BUHLMANN_DEBUG_PLAN_ROW_DECO_STOP;
    case DECO_PLAN_ENTRY_BOTTOM:
    default:
        return BUHLMANN_DEBUG_PLAN_ROW_BOTTOM;
    }
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

    bus_set_tissues(tissue_load);
    bus_set_cns((uint8_t)dive_info.cns);
    bus_set_otu((uint16_t)dive_info.otu);
    bus_set_gf99(dive_info.gf99);
    bus_set_surf_gf(dive_info.surfGF);

    float mod_m = s_buhlmann.calculateMOD(g_sys_config.mod_ppo2);
    bus_set_mod(mod_m);

    float ceiling_m = s_buhlmann.calculateDepthFromPressure(s_buhlmann.getLastCeilingPressure());
    bus_set_ceiling(ceiling_m);

    Gas active_gas = s_buhlmann.getGas(s_buhlmann.getActiveGas());
    uint8_t o2_pct = (uint8_t)(active_gas.oxygenFraction * 100.0f + 0.5f);
    uint8_t he_pct = (uint8_t)(active_gas.heliumFraction * 100.0f + 0.5f);
    bus_set_gas_mix(o2_pct, he_pct);

    float surface_pressure = s_buhlmann.getSurfacePressure();
    float n2_fraction = 1.0f - active_gas.oxygenFraction - active_gas.heliumFraction;
    float gas_density = (active_gas.oxygenFraction * 1.429f +
                         n2_fraction * 1.251f +
                         active_gas.heliumFraction * 0.179f) *
                        (current_pressure / surface_pressure);
    bus_set_gas_density(gas_density);

    float fio2_pct = (active_gas.oxygenFraction * current_pressure / surface_pressure) * 100.0f;
    bus_set_fio2(fio2_pct);

    uint16_t tts_val = (uint16_t)(dive_info.ttsSeconds / 60);
    if (tts_val > 9999U) tts_val = 9999U;
    bus_set_tts(tts_val);
}

static void sync_stop_data(const DiveInfo &dive_info)
{
    static bool s_deco_bar_active = false;
    static float s_deco_bar_depth_m = 0.0f;
    static uint16_t s_deco_bar_total_s = 0U;

    stop_type_t stop_type = STOP_NONE;
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

    bus_update_deco(ndl_display_min,
                         stop_type,
                         dive_info.stopDepthMeters,
                         stop_total_s,
                         stop_left_s,
                         dive_info.inStopZone);
    if (stop_type == STOP_NONE) {
        bus_set_ndl_bar_pct(ndl_bar_pct_for_display(dive_info));
    }

}

static void handle_pending_gas_switch(float depth_m)
{
    uint8_t target_gas_idx = 0;
    if (!has_pending_gas_switch(&target_gas_idx)) {
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

    clear_gas_switch_cmd();
}

static void sync_gas_data(float current_pressure)
{
    int active_gas_idx = s_buhlmann.getActiveGas();
    Gas active_gas = s_buhlmann.getGas(active_gas_idx);
    char gas_name_buf[16];

    if (active_gas.enabled) {
        format_gas_name(active_gas, gas_name_buf, sizeof(gas_name_buf));
        bus_set_gas((uint8_t)active_gas_idx, gas_name_buf);
    }

    for (int i = 0; i < MAX_GASES; i++) {
        Gas gas = s_buhlmann.getGas(i);
        if (gas.enabled) {
            float ppo2 = gas.oxygenFraction * (current_pressure / 1013.25f);
            bus_set_ppo2((uint8_t)i, ppo2);
        }
    }
}

void buhlmann_debug_init(void)
{
    if (s_initialized) {
        return;
    }

    rt_kprintf("Initializing Buhlmann algorithm...\n");

    s_buhlmann.setSeaLevelAtmosphericPressure(1000.0f);
    s_buhlmann.setWaterType(water_type_from_salinity_mode(g_sys_config.salinity_mode));
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
    buhlmann_debug_apply_gases_from_ui();
}

void buhlmann_debug_set_final_stop_depth(uint8_t depth_m)
{
    s_final_deco_stop_depth_m = (depth_m == 6U) ? 6U : 3U;
    s_buhlmann.setFinalStopDepth((float)s_final_deco_stop_depth_m);
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
    rt_kprintf("[DIVE_SETUP] GF: %u/%u\n", (unsigned)s_gf_low_pct, (unsigned)s_gf_high_pct);
}

void buhlmann_debug_set_salinity_mode(uint8_t mode)
{
    if (mode > 2U) mode = 0U;

    if (!s_initialized) {
        buhlmann_debug_init();
    }

    s_buhlmann.setWaterType(water_type_from_salinity_mode(mode));
    rt_kprintf("[DIVE_SETUP] Salinity mode: %u\n", (unsigned)mode);
}

void buhlmann_debug_apply_gases_from_ui(void)
{
    if (!s_initialized) {
        buhlmann_debug_init();
    }

    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count > MAX_GASES) gas_count = MAX_GASES;

    bool any_enabled = false;
    bool enabled_slots[MAX_GASES] = { false, false, false };
    uint8_t first_enabled_idx = 0U;
    for (uint8_t i = 0; i < MAX_GASES; i++) {
        bool enabled = i < gas_count;
        uint8_t o2 = enabled ? g_sensor_data.gas_slot_o2_pct[i] : 0U;
        uint8_t he = enabled ? g_sensor_data.gas_slot_he_pct[i] : 0U;

        if (o2 == 0U || o2 > 100U || he > 100U || (uint16_t)o2 + (uint16_t)he > 100U) {
            enabled = false;
            o2 = 21U;
            he = 0U;
        }

        float ppo2 = (i == 0U) ? 1.4f : 1.6f;
        s_buhlmann.setGas(i, (float)o2 / 100.0f, (float)he / 100.0f, enabled, ppo2);
        if (enabled) {
            if (!any_enabled) {
                first_enabled_idx = i;
            }
            enabled_slots[i] = true;
            any_enabled = true;
        }
    }

    if (!any_enabled) {
        s_buhlmann.setGas(0, 0.21f, 0.0f, true, 1.4f);
        gas_count = 1U;
        enabled_slots[0] = true;
        first_enabled_idx = 0U;
    }

    uint8_t active_idx = g_sensor_data.gas_active_idx;
    if (active_idx >= gas_count || active_idx >= MAX_GASES || !enabled_slots[active_idx]) {
        active_idx = first_enabled_idx;
    }
    s_buhlmann.setActiveGas(active_idx);

    Gas active_gas = s_buhlmann.getGas(s_buhlmann.getActiveGas());
    s_buhlmann.setOxygenRateInGas(active_gas.oxygenFraction);
}

bool buhlmann_debug_plan_calculate(float depth_m,
                                   uint16_t bottom_time_min,
                                   float rmv_lpm,
                                   buhlmann_debug_plan_result_t *out_result)
{
    if (out_result == NULL)
    {
        return false;
    }

    memset(out_result, 0, sizeof(*out_result));

    if (!s_initialized)
    {
        buhlmann_debug_init();
    }

    DecoPlanConfig config;
    fill_plan_config_from_ui(depth_m, bottom_time_min, rmv_lpm, config);

    DecoPlanResult result;
    bool ok = s_buhlmann.planDive(config, result);
    out_result->ok = ok && !result.truncated && result.entryCount > 0;
    out_result->truncated = result.truncated;
    out_result->total_runtime_min = display_minutes_from_seconds(result.totalRuntimeSeconds);
    out_result->total_deco_min = display_minutes_from_seconds(result.totalDecoSeconds);
    out_result->total_gas_l = round_u16_non_negative(result.totalGasLiters);
    out_result->cns_pct = round_u16_non_negative(result.cns);
    out_result->otu = round_u16_non_negative(result.otu);

    uint8_t entry_count = 0U;
    bool deco_started = false;
    for (int i = 0; i < result.entryCount; i++)
    {
        const DecoPlanEntry &entry = result.entries[i];
        if (entry.entryType == DECO_PLAN_ENTRY_ASCENT && deco_started)
        {
            continue;
        }

        if (entry_count >= BUHLMANN_DEBUG_PLAN_MAX_ENTRIES)
        {
            out_result->truncated = true;
            break;
        }

        buhlmann_debug_plan_row_t &row = out_result->entries[entry_count];
        row.type = map_plan_row_type(entry.entryType);
        row.depth_m = (int16_t)roundf(entry.depthMeters);
        row.time_min = display_minutes_from_seconds(entry.timeSeconds);
        row.run_min = display_minutes_from_seconds(entry.runtimeSeconds);
        row.o2_pct = (uint8_t)roundf(entry.oxygenFraction * 100.0f);
        row.he_pct = (uint8_t)roundf(entry.heliumFraction * 100.0f);
        row.gas_l = round_u16_non_negative(entry.gasQtyLiters);
        entry_count++;

        if (entry.entryType == DECO_PLAN_ENTRY_DECO_STOP)
        {
            deco_started = true;
        }
    }
    out_result->entry_count = entry_count;

    return out_result->ok;
}

void buhlmann_debug_reset(void)
{
    s_buhlmann = Buhlmann(62.7f);
    s_initialized = false;
    s_diving = false;
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
    sync_gas_data(current_pressure);
}
