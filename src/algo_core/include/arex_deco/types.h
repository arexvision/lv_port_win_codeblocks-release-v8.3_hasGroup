#ifndef AREX_DECO_TYPES_H
#define AREX_DECO_TYPES_H

#include "arex_deco/constants.h"
#include "arex_deco/version.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ArexDecoWaterType {
    AREX_DECO_WATER_SALT = 0,
    AREX_DECO_WATER_FRESH = 1
    // TODO: EN13319
} ArexDecoWaterType;

typedef enum ArexDecoGasRole {
    AREX_DECO_GAS_ROLE_BOTTOM = 0,
    AREX_DECO_GAS_ROLE_TRAVEL = 1,
    AREX_DECO_GAS_ROLE_DECO = 2,
    AREX_DECO_GAS_ROLE_UNKNOWN = 255
} ArexDecoGasRole;

typedef struct ArexDecoAscentRate {
    float rate_75_percent_m_per_min;
    float rate_50_percent_m_per_min;
    float rate_stops_m_per_min;
    float rate_last_6m_m_per_min;
} ArexDecoAscentRate;

typedef struct ArexDecoConfig {
    ArexDecoVersion api_version;
    float surface_pressure_bar;
    float water_vapor_pressure_bar;
    float water_meters_per_bar;
    float gf_low;
    float gf_high;
    ArexDecoAscentRate ascent_rate;
    float deco_step_m;
    float last_stop_m;
    uint32_t safety_stop_seconds;
    uint32_t gas_switch_penalty_seconds;
    ArexDecoWaterType water_type;
    uint8_t safety_stop_enabled;
    uint8_t reserved[3];
} ArexDecoConfig;

typedef struct ArexDecoGas {
    float oxygen_fraction;
    float helium_fraction;
    float nitrogen_fraction;
    float min_depth_m;
    float max_depth_m;
    float max_ppo2_bar;
    uint8_t enabled;
    ArexDecoGasRole role;
    uint8_t reserved[14];
} ArexDecoGas;

typedef struct ArexDecoGasPlan {
    ArexDecoVersion api_version;
    uint8_t gas_count;
    int8_t active_gas_index;
    uint8_t reserved[14];
    ArexDecoGas gases[AREX_DECO_MAX_GAS_COUNT];
} ArexDecoGasPlan;

typedef struct ArexDecoTissueState {
    ArexDecoVersion api_version;
    float nitrogen_bar[AREX_DECO_COMPARTMENT_COUNT];
    float helium_bar[AREX_DECO_COMPARTMENT_COUNT];
} ArexDecoTissueState;

typedef struct ArexDecoOxygenExposure {
    float cns_percent;
    float otu;
    uint8_t reserved[24];
} ArexDecoOxygenExposure;

typedef struct ArexDecoDiveState {
    ArexDecoVersion api_version;
    ArexDecoConfig config;
    ArexDecoGasPlan gas_plan;
    ArexDecoTissueState tissue;
    ArexDecoOxygenExposure oxygen_exposure;
    float current_depth_m;
    float max_depth_m;
    float depth_time_m_seconds;
    uint32_t elapsed_seconds;
    // 本次潜水是否曾产生减压义务（latched bit）。一旦在某个 step 中
    // ceiling > 0 即置 1，至 arex_deco_reset_tissue_to_surface 才清零。
    // 此字段服务的是过去式语义（影响 nofly 等下限），不要用作 UI 实时
    // "DECO NOW" 指示——实时义务请读 ArexDecoRuntimeMetrics.ceiling_depth_m。
    uint8_t was_deco_dive;
    uint8_t reserved[27];
} ArexDecoDiveState;

typedef struct ArexDecoStepInput {
    ArexDecoVersion api_version;
    float start_depth_m;
    float end_depth_m;
    uint32_t duration_seconds;
    int8_t gas_index;
    uint8_t reserved[15];
} ArexDecoStepInput;

typedef struct ArexDecoPressureStepInput {
    ArexDecoVersion api_version;
    float start_pressure_bar;
    float end_pressure_bar;
    uint32_t duration_seconds;
    int8_t gas_index;
    uint8_t reserved[15];
} ArexDecoPressureStepInput;

typedef struct ArexDecoRuntimeMetrics {
    ArexDecoVersion api_version;
    float gf99_percent;
    float surface_gf_percent;
    float ceiling_depth_m;
    int32_t ndl_seconds;
    int8_t leading_compartment;
    uint8_t reserved[27];
} ArexDecoRuntimeMetrics;

typedef struct ArexDecoTissueGradientMetrics {
    // Absolute GF percentages at the current ambient pressure. Values are
    // already in percent units, e.g. 70.0f means 70%.
    float absolute_gf_percent[AREX_DECO_COMPARTMENT_COUNT];
    // Percentages relative to the current target GF limit. Values are already
    // in percent units and must not be multiplied by 100 by callers.
    float relative_gf_percent[AREX_DECO_COMPARTMENT_COUNT];
    float current_target_gf;
} ArexDecoTissueGradientMetrics;

typedef struct ArexDecoGasRecommendation {
    ArexDecoVersion api_version;
    uint8_t available;
    int8_t recommended_gas_index;
    int8_t active_gas_index;
    uint8_t is_emergency_no_safe_gas;
    float depth_m;
    float ppo2_bar;
    uint8_t reserved[24];
} ArexDecoGasRecommendation;

typedef struct ArexDecoStop {
    float depth_m;
    // Total predicted time at this stop. This includes the physical hold and
    // any same-depth gas-switch penalty used by the planner.
    uint32_t duration_seconds;
    int8_t gas_index;
    float target_gf;
    // Physical stop hold only. Does not include gas-switch penalty.
    uint32_t hold_seconds;
    // Planner-only same-depth gas-switch delay.
    uint32_t switch_penalty_seconds;
    uint8_t reserved[12];
} ArexDecoStop;

typedef struct ArexDecoSchedule {
    ArexDecoVersion api_version;
    uint8_t stop_count;
    uint8_t truncated;
    // 1 if current_depth_m is shallower than the GF-high ceiling at plan time.
    // Product layers must treat this as a warning even when stops/tts are zero.
    uint8_t ceiling_violated;
    uint8_t reserved[13];
    uint32_t tts_seconds;
    ArexDecoOxygenExposure end_of_dive_exposure;
    ArexDecoStop stops[AREX_DECO_MAX_DECO_STOP_COUNT];
} ArexDecoSchedule;

#ifdef __cplusplus
}
#endif

#endif
