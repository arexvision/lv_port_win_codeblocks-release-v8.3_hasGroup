#ifndef UI_VM_PLAN_CHART_TYPES_H
#define UI_VM_PLAN_CHART_TYPES_H

#include <stdint.h>
#include "../ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_vm_plan_chart
{
    uint8_t draw_enabled;
    uint8_t dive_log_count;
    uint8_t deco_stop_count;
    uint16_t x_step_s;
    float current_time_s;
    float current_depth_m;
    float predicted_total_time_s;
    float max_depth_axis_m;
    float max_time_axis_s;
    dive_pt_t dive_log[MAX_DIVE_LOG];
    deco_stop_t deco_stops[MAX_DECO_STOPS];
} ui_vm_plan_chart_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_PLAN_CHART_TYPES_H */
