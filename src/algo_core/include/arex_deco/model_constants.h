#ifndef AREX_DECO_MODEL_CONSTANTS_H
#define AREX_DECO_MODEL_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Planner fixed policy constants, not ArexDecoConfig fields. The safety stop
// switch and duration are config-controlled; trigger threshold and stop depth
// remain core rules expressed in meters.
#define AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M 10.0f
#define AREX_DECO_SAFETY_STOP_DEPTH_M 5.0f
#define AREX_DECO_SAFETY_STOP_ZONE_MIN_DEPTH_M 2.9f
#define AREX_DECO_SAFETY_STOP_ZONE_MAX_DEPTH_M 6.0f
#define AREX_DECO_SAFETY_STOP_TOO_SHALLOW_DEPTH_M 2.0f

// Decompression schedule output constraints. The planner emits second-level
// durations and does not round positive stops to whole minutes.
#define AREX_DECO_MAX_STOP_SECONDS 21600.0f
#define AREX_DECO_STOP_TIME_GRANULARITY_SECONDS 1u

// 0.0.35 field experiment, retained by 0.0.36: once mandatory decompression has established a GF
// anchor, the final stop remains active until the surface endpoint satisfies
// GF High minus this absolute GF fraction. Intermediate stops and the GF path
// are unchanged. 0.018 means 1.8 GF percentage points.
#define AREX_DECO_FINAL_STOP_RELEASE_HEADROOM_GF_FRACTION 0.018f

// 0.0.36 pressure-reference compatibility policy. The configurable offset is
// expressed in vertical meters and ramps in over the first meter of raw depth
// so a surface pressure sample remains exactly at surface pressure.
#define AREX_DECO_PRESSURE_REFERENCE_OFFSET_RAMP_DEPTH_M 1.0f
#define AREX_DECO_MAX_PRESSURE_REFERENCE_OFFSET_M 2.0f

// Physical gas constants.
#define AREX_DECO_AIR_OXYGEN_FRACTION 0.21f
#define AREX_DECO_AIR_NITROGEN_FRACTION 0.79f
#define AREX_DECO_MOLAR_MASS_O2_G_PER_MOL 31.9988f
#define AREX_DECO_MOLAR_MASS_HE_G_PER_MOL 4.0026f
#define AREX_DECO_MOLAR_MASS_N2_G_PER_MOL 28.0134f
#define AREX_DECO_UNIVERSAL_GAS_CONSTANT_J_PER_MOL_K 8.314462f
#define AREX_DECO_GRAMS_PER_KILOGRAM 1000.0f
#define AREX_DECO_CELSIUS_TO_KELVIN_OFFSET 273.15f
#define AREX_DECO_IDEAL_GAS_COMPRESSIBILITY_Z 1.0f

// Mechanism-level ppO2 hard ceiling. validate_gas() rejects values above this.
#define AREX_DECO_MAX_ALLOWABLE_PPO2_BAR 2.0f

// Numeric tolerances used by validation, planning, and rounding.
#define AREX_DECO_MATH_EPSILON 1.0e-6f
#define AREX_DECO_UNIT_FRACTION_TOLERANCE 0.001f
#define AREX_DECO_GAS_FRACTION_TOLERANCE 0.001f
#define AREX_DECO_DEPTH_TOLERANCE_M 0.01f
#define AREX_DECO_PRESSURE_TOLERANCE_BAR 0.0001f

// No-fly policy constants.
#define AREX_DECO_NOFLY_MIN_NO_DECO_SECONDS (12u * 3600u)
#define AREX_DECO_NOFLY_MIN_DECO_SECONDS (24u * 3600u)
#define AREX_DECO_NOFLY_DESAT_THRESHOLD 1.02
#define AREX_DECO_NOFLY_DESAT_MAX_SECONDS (48.0 * 3600.0)

// CNS model constants.
#define AREX_DECO_LN2_F 0.69314718055994530942f
#define AREX_DECO_CNS_HALFLIFE_SECONDS 5400.0f
#define AREX_DECO_CNS_DECAY_PPO2_THRESHOLD_BAR 0.5f
#define AREX_DECO_OTU_SURFACE_RESET_SECONDS (24u * 3600u)

#ifdef __cplusplus
}
#endif

#endif
