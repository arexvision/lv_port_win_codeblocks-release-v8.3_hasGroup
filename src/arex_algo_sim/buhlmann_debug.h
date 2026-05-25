#ifndef AREX_ALGO_SIM_BUHLMANN_DEBUG_H
#define AREX_ALGO_SIM_BUHLMANN_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#define BUHLMANN_DEBUG_PLAN_MAX_ENTRIES 32U

typedef enum
{
    BUHLMANN_DEBUG_PLAN_ROW_BOTTOM = 0,
    BUHLMANN_DEBUG_PLAN_ROW_ASCENT = 1,
    BUHLMANN_DEBUG_PLAN_ROW_DECO_STOP = 2,
} buhlmann_debug_plan_row_type_t;

typedef struct
{
    buhlmann_debug_plan_row_type_t type;
    int16_t depth_m;
    uint16_t time_min;
    uint16_t run_min;
    uint8_t o2_pct;
    uint8_t he_pct;
    uint16_t gas_l;
} buhlmann_debug_plan_row_t;

typedef struct
{
    bool ok;
    bool truncated;
    uint8_t entry_count;
    uint16_t total_runtime_min;
    uint16_t total_deco_min;
    uint16_t total_gas_l;
    uint16_t cns_pct;
    uint16_t otu;
    buhlmann_debug_plan_row_t entries[BUHLMANN_DEBUG_PLAN_MAX_ENTRIES];
} buhlmann_debug_plan_result_t;

#ifdef __cplusplus
extern "C" {
#endif

void buhlmann_debug_init(void);
void buhlmann_debug_reset(void);
void buhlmann_debug_tick(float depth_m, float temperature_c, uint32_t delta_time_s);
void buhlmann_debug_set_gf(uint8_t gf_low_pct, uint8_t gf_high_pct);
void buhlmann_debug_set_final_stop_depth(uint8_t depth_m);
void buhlmann_debug_set_salinity_mode(uint8_t mode);
void buhlmann_debug_apply_gases_from_ui(void);
bool buhlmann_debug_plan_calculate(float depth_m,
                                   uint16_t bottom_time_min,
                                   float rmv_lpm,
                                   buhlmann_debug_plan_result_t *out_result);

#ifdef __cplusplus
}
#endif

#endif /* AREX_ALGO_SIM_BUHLMANN_DEBUG_H */
