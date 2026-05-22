#ifndef AREX_ALGO_SIM_BUHLMANN_DEBUG_H
#define AREX_ALGO_SIM_BUHLMANN_DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void buhlmann_debug_init(void);
void buhlmann_debug_tick(float depth_m, float temperature_c, uint32_t delta_time_s);

#ifdef __cplusplus
}
#endif

#endif /* AREX_ALGO_SIM_BUHLMANN_DEBUG_H */
