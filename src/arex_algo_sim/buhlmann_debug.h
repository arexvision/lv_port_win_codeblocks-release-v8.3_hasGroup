#ifndef AREX_ALGO_SIM_BUHLMANN_DEBUG_H
#define AREX_ALGO_SIM_BUHLMANN_DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void buhlmann_debug_init(void);
void buhlmann_debug_reset(void);
void buhlmann_debug_tick(float depth_m, float temperature_c, uint32_t delta_time_s);
void buhlmann_debug_set_gf(uint8_t gf_low_pct, uint8_t gf_high_pct);
void buhlmann_debug_set_final_stop_depth(uint8_t depth_m);

#ifdef __cplusplus
}
#endif

#endif /* AREX_ALGO_SIM_BUHLMANN_DEBUG_H */
