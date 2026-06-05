#ifndef AREX_DECO_API_H
#define AREX_DECO_API_H

#include "arex_deco/status.h"
#include "arex_deco/types.h"

#ifdef __cplusplus
extern "C" {
#endif

ArexDecoStatus arex_deco_make_default_config(ArexDecoConfig* config);
ArexDecoStatus arex_deco_make_default_air_gas(const ArexDecoConfig* config, ArexDecoGas* gas);
ArexDecoStatus arex_deco_make_default_gas_plan(const ArexDecoConfig* config, ArexDecoGasPlan* gas_plan);
ArexDecoStatus arex_deco_reset_tissue_to_surface(
    const ArexDecoConfig* config,
    const ArexDecoGas* surface_gas,
    ArexDecoTissueState* tissue);
ArexDecoStatus arex_deco_make_initial_dive_state(ArexDecoDiveState* state);
ArexDecoStatus arex_deco_validate_gas(const ArexDecoConfig* config, const ArexDecoGas* gas);
ArexDecoStatus arex_deco_validate_config(const ArexDecoConfig* config);

ArexDecoStatus arex_deco_step(
    const ArexDecoDiveState* state,
    const ArexDecoStepInput* input,
    ArexDecoDiveState* next_state,
    ArexDecoRuntimeMetrics* metrics);

ArexDecoStatus arex_deco_step_pressure(
    const ArexDecoDiveState* state,
    const ArexDecoPressureStepInput* input,
    ArexDecoDiveState* next_state,
    ArexDecoRuntimeMetrics* metrics);

ArexDecoStatus arex_deco_plan(
    const ArexDecoDiveState* state,
    ArexDecoSchedule* schedule,
    ArexDecoGasRecommendation* gas_rec);

ArexDecoStatus arex_deco_nofly(
    const ArexDecoDiveState* state,
    uint32_t* nofly_seconds);

ArexDecoStatus arex_deco_calculate_tissue_margin(
    const ArexDecoDiveState* state,
    float reference_depth_m,
    float reference_limit_gf,
    ArexDecoTissueMarginMetrics* metrics);

#ifdef __cplusplus
}
#endif

#endif
