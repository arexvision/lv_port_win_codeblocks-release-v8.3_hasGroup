#ifndef AREX_DECO_ENVIRONMENT_H
#define AREX_DECO_ENVIRONMENT_H

#ifdef __cplusplus
extern "C" {
#endif

// Version of the public physical-environment contract. Increment this when a
// named water model, physical constant, or conversion semantic changes.
#define AREX_DECO_ENVIRONMENT_CONTRACT_VERSION 1

// Pressure and hydrostatic reference constants.
#define AREX_DECO_PRESSURE_PA_PER_BAR 100000.0f
#define AREX_DECO_PRESSURE_MBAR_PER_BAR 1000.0f
#define AREX_DECO_STANDARD_ATMOSPHERE_BAR 1.01325f
#define AREX_DECO_STANDARD_GRAVITY_M_PER_S2 9.80665f

// Canonical water densities used by product water-mode selections.
#define AREX_DECO_WATER_FRESH_DENSITY_KG_PER_M3 1000.0f
#define AREX_DECO_WATER_EN13319_DENSITY_KG_PER_M3 1020.0f
#define AREX_DECO_WATER_SALT_DENSITY_KG_PER_M3 1030.0f
#define AREX_DECO_WATER_METERS_PER_BAR_MATCH_TOLERANCE 0.0001f

typedef enum ArexDecoWaterType {
    AREX_DECO_WATER_FRESH = 0,
    AREX_DECO_WATER_EN13319 = 1,
    AREX_DECO_WATER_SALT = 2,
    AREX_DECO_WATER_UNKNOWN = 255
} ArexDecoWaterType;

typedef struct ArexDecoWaterProperties {
    float density_kg_per_m3;
    float meters_per_bar;
} ArexDecoWaterProperties;

#ifdef __cplusplus
}
#endif

#endif
