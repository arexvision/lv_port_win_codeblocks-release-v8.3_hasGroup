#ifndef ALGO_SIM_DECO_CORE_H
#define ALGO_SIM_DECO_CORE_H

#include "../ui/views/submenu_dive_plan_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void deco_core_init(void);
void deco_core_reset(void);
void deco_core_tick(float depth_m, float temperature_c, uint32_t delta_time_s);
void deco_core_set_gf(uint8_t gf_low_pct, uint8_t gf_high_pct);
void deco_core_set_final_stop_depth(uint8_t depth_m);
void deco_core_set_salinity_mode(uint8_t mode);
void deco_core_set_safety_stop_mode(uint8_t mode);
void deco_core_apply_gases_from_ui(void);
float deco_core_calculate_gas_mod(uint8_t o2_pct, uint8_t he_pct, float max_ppo2);
bool deco_core_rtc_offline(uint32_t seconds);
bool deco_core_plan_calculate(float depth_m, uint16_t bottom_time_min, float rmv_lpm, dive_plan_result_snapshot_t *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* ALGO_SIM_DECO_CORE_H */
