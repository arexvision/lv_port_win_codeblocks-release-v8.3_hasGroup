#ifndef AREX_DECO_DEFAULTS_H
#define AREX_DECO_DEFAULTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Default templates written by arex_deco_make_default_config() and related
// default constructors. Callers override the config/gas fields themselves; the
// macros are not live mutable settings.
#define AREX_DECO_DEFAULT_SURFACE_PRESSURE_BAR 1.01325f
#define AREX_DECO_DEFAULT_WATER_VAPOR_PRESSURE_BAR 0.0627f
#define AREX_DECO_DEFAULT_SALT_WATER_METERS_PER_BAR 10.0f
#define AREX_DECO_DEFAULT_FRESH_WATER_METERS_PER_BAR 10.3f
#define AREX_DECO_DEFAULT_GF_LOW 0.50f
#define AREX_DECO_DEFAULT_GF_HIGH 0.70f
#define AREX_DECO_DEFAULT_ASCENT_RATE_75_PERCENT_M_PER_MIN 9.0f
#define AREX_DECO_DEFAULT_ASCENT_RATE_50_PERCENT_M_PER_MIN 9.0f
#define AREX_DECO_DEFAULT_ASCENT_RATE_STOPS_M_PER_MIN 9.0f
#define AREX_DECO_DEFAULT_ASCENT_RATE_LAST_6M_M_PER_MIN 9.0f
#define AREX_DECO_DEFAULT_SAFETY_STOP_ENABLED 1u
#define AREX_DECO_DEFAULT_SAFETY_STOP_SECONDS (3u * 60u)
#define AREX_DECO_DEFAULT_GAS_SWITCH_PENALTY_SECONDS 60u
#define AREX_DECO_DEFAULT_DECO_STEP_M 3.0f
#define AREX_DECO_DEFAULT_LAST_STOP_M 3.0f

// Default gas ppO2 policies. These are commonly written into gas defaults and
// then checked by validate_gas().
#define AREX_DECO_DEFAULT_BOTTOM_PPO2_BAR 1.4f
#define AREX_DECO_DEFAULT_DECO_PPO2_BAR 1.6f

// Default runtime current-stop selector policy. These values are used when
// ArexDecoRuntimeStopSelectorInput passes 0 for the corresponding override.
#define AREX_DECO_DEFAULT_RUNTIME_STOP_ZONE_HALF_WIDTH_M 1.5f
#define AREX_DECO_DEFAULT_RUNTIME_STOP_PROMOTE_MIN_SECONDS 30u
#define AREX_DECO_DEFAULT_RUNTIME_STOP_STABLE_SECONDS 10u

#ifdef __cplusplus
}
#endif

#endif
