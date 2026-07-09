#ifndef AREX_DECO_TYPES_H
#define AREX_DECO_TYPES_H

#include "arex_deco/constants.h"
#include "arex_deco/version.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ArexDecoGasRole {
    AREX_DECO_GAS_ROLE_BOTTOM = 0,
    AREX_DECO_GAS_ROLE_TRAVEL = 1,
    AREX_DECO_GAS_ROLE_DECO = 2,
    AREX_DECO_GAS_ROLE_UNKNOWN = 255
} ArexDecoGasRole;

typedef enum ArexDecoSafetyStopPhase {
    AREX_DECO_SAFETY_STOP_PHASE_NOT_REQUIRED = 0,
    AREX_DECO_SAFETY_STOP_PHASE_PENDING = 1,
    AREX_DECO_SAFETY_STOP_PHASE_COUNTING = 2,
    AREX_DECO_SAFETY_STOP_PHASE_PAUSED_TOO_DEEP = 3,
    AREX_DECO_SAFETY_STOP_PHASE_PAUSED_TOO_SHALLOW = 4,
    AREX_DECO_SAFETY_STOP_PHASE_MISSED_TOO_SHALLOW = 5,
    AREX_DECO_SAFETY_STOP_PHASE_COMPLETE = 6,
    AREX_DECO_SAFETY_STOP_PHASE_SUPPRESSED_BY_DECO = 7
} ArexDecoSafetyStopPhase;

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
    float meters_per_bar;
    float gf_low;
    float gf_high;
    ArexDecoAscentRate ascent_rate;
    float deco_step_m;
    float last_stop_m;
    uint32_t safety_stop_seconds;
    uint32_t gas_switch_penalty_seconds;
    uint8_t safety_stop_enabled;
    uint8_t reserved[7];
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
    // Consecutive near-surface air/offline interval used for 24 h OTU expiry.
    // Reset to 0 whenever OTU is loaded by elevated PPO2 or the diver leaves
    // the near-surface air recovery condition.
    uint32_t otu_surface_interval_seconds;
    uint8_t reserved[20];
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
    // ceiling > 0 即置 1，至新潜水状态初始化才清零。
    // 此字段服务的是过去式语义（影响 nofly 等下限），不要用作 UI 实时
    // "DECO NOW" 指示——实时义务请读 ArexDecoRuntimeMetrics.ceiling_depth_m。
    uint8_t was_deco_dive;
    uint8_t safety_stop_required;
    uint8_t safety_stop_completed;
    uint8_t safety_stop_missed;
    uint32_t safety_stop_elapsed_seconds;
    // GF Low 深端锚点：本次潜水进入强制减压后，历史上出现过的
    // 最深有效 GF-low first-stop grid depth。用于稳定 GF Low -> GF High
    // 插值斜率；不是 current stop / current ceiling / max depth / safety stop。
    float gf_anchor_depth_m;
    uint8_t gf_anchor_valid;
    uint8_t reserved[15];
} ArexDecoDiveState;

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

typedef struct ArexDecoTissuePressureMetrics {
    ArexDecoVersion api_version;
    float ambient_pressure_bar;
    float inspired_n2_bar;
    float inspired_he_bar;
    float tissue_n2_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_he_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_m_value_bar[AREX_DECO_COMPARTMENT_COUNT];
    float tissue_m_gf_bar[AREX_DECO_COMPARTMENT_COUNT];
    float current_gf_target;
    uint8_t reserved[24];
} ArexDecoTissuePressureMetrics;

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

enum {
    AREX_DECO_STOP_KIND_MANDATORY = 0,
    AREX_DECO_STOP_KIND_ROUTE_WAYPOINT = 1,
    AREX_DECO_STOP_KIND_GAS_SWITCH = 2,
    AREX_DECO_STOP_KIND_SAFETY = 3,
};

enum {
    AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED = 1u << 0,
};

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
    // AREX_DECO_STOP_KIND_*. The field is uint8_t to keep the C ABI compact
    // and stable across compilers.
    uint8_t kind;
    // AREX_DECO_STOP_FLAG_* bitmask for display/runtime interpretation.
    uint8_t flags;
    uint8_t reserved[10];
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

enum {
    AREX_DECO_RUNTIME_STOP_REASON_NONE = 0,
    AREX_DECO_RUNTIME_STOP_REASON_STABLE_CANDIDATE = 1,
    AREX_DECO_RUNTIME_STOP_REASON_UNIQUE_SHORT = 2,
    AREX_DECO_RUNTIME_STOP_REASON_HELD_PREVIOUS = 3,
    AREX_DECO_RUNTIME_STOP_REASON_DEBOUNCING = 4,
    AREX_DECO_RUNTIME_STOP_REASON_CLEARED = 5,
};

typedef struct ArexDecoRuntimeStopSelectorState {
    ArexDecoVersion api_version;
    uint8_t active;
    uint8_t candidate_active;
    uint8_t displayed_source_raw_index;
    uint8_t candidate_source_raw_index;
    float displayed_depth_m;
    uint32_t displayed_remaining_seconds;
    uint32_t displayed_total_seconds;
    int8_t displayed_gas_index;
    uint8_t displayed_is_short;
    int8_t candidate_gas_index;
    uint8_t reserved_flags[1];
    float candidate_depth_m;
    uint32_t candidate_seen_seconds;
    uint32_t last_elapsed_seconds;
    uint8_t reserved[24];
} ArexDecoRuntimeStopSelectorState;

typedef struct ArexDecoRuntimeStopSelectorInput {
    ArexDecoVersion api_version;
    float current_depth_m;
    uint32_t elapsed_seconds;
    float stop_zone_half_width_m;
    uint32_t promote_min_seconds;
    uint32_t stable_seconds;
    uint8_t reserved[24];
} ArexDecoRuntimeStopSelectorInput;

typedef struct ArexDecoRuntimeStop {
    ArexDecoVersion api_version;
    uint8_t available;
    uint8_t source_raw_index;
    uint8_t reason;
    uint8_t is_short;
    float depth_m;
    uint32_t remaining_seconds;
    uint32_t total_seconds;
    int8_t gas_index;
    uint8_t reserved_flags[3];
    uint8_t reserved[24];
} ArexDecoRuntimeStop;

typedef struct ArexDecoTtsForecast {
    ArexDecoVersion api_version;
    uint32_t hold_seconds;
    uint32_t current_tts_seconds;
    uint32_t tts_at_hold_seconds;
    int32_t tts_delta_hold_seconds;
    uint8_t reserved[24];
} ArexDecoTtsForecast;

typedef struct ArexDecoNdlExcursionForecast {
    ArexDecoVersion api_version;
    float delta_depth_m;
    int32_t current_ndl_seconds;
    int32_t ndl_up_seconds;
    int32_t ndl_down_seconds;
    uint8_t reserved[24];
} ArexDecoNdlExcursionForecast;

typedef struct ArexDecoSafetyStopStatus {
    ArexDecoVersion api_version;
    uint8_t required;
    uint8_t counting;
    ArexDecoSafetyStopPhase phase;
    uint8_t completed;
    uint8_t missed;
    uint8_t reserved_flags[2];
    float target_depth_m;
    float zone_min_depth_m;
    float zone_max_depth_m;
    float too_shallow_depth_m;
    float trigger_depth_m;
    uint32_t required_seconds;
    uint32_t elapsed_seconds;
    uint32_t remaining_seconds;
    uint8_t reserved[16];
} ArexDecoSafetyStopStatus;

#ifdef __cplusplus
}
#endif

#endif
