/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_chart_types.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

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
