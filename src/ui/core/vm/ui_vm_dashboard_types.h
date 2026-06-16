/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_dashboard_types.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_DASHBOARD_TYPES_H
#define UI_VM_DASHBOARD_TYPES_H

#include <stdint.h>

#include "../ui_defs.h"
#include "../ui_types.h"
#include "../ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t heading;
    uint16_t heading_target;
    uint16_t right_canvas_w;
    uint8_t reserved;
    unsigned locked : 1;
} ui_vm_compass_t;

typedef struct
{
    uint8_t gas_count;
    uint8_t active_idx;
    uint8_t cursor_idx;
    uint8_t edit_mode;
    uint16_t right_canvas_w;
    uint8_t gap_y;
    char hint[48];
    char names[GAS_COUNT][20];
    char mod_text[GAS_COUNT][20];
    char ppo2_text[GAS_COUNT][20];
    uint8_t visible[GAS_COUNT];
    uint8_t highlighted[GAS_COUNT];
} ui_vm_gas_t;

typedef struct
{
    char gf_setting[16];
    char gf99[16];
    char surf_gf[16];
    char cns[16];
    char otu[16];
    int16_t tissue_raw_pct[16];
    uint8_t tissue_gf_pct[16];
    float tissue_target_gf_pct;
    uint8_t gf_low;
    uint8_t gf_high;
    uint8_t chart_active;
    uint8_t surf_gf_alert;
    uint16_t right_canvas_w;
} ui_vm_deco_t;

typedef struct
{
    int16_t int_part;
    uint8_t dec_part;
    char text[16];
} ui_vm_depth_t;

typedef struct
{
    stop_type_t stop_type;
    int16_t ndl;
    int16_t ndl_stop_value;
    float stop_depth_m;
    uint16_t stop_time_left_s;
    uint16_t stop_time_total_s;
    uint8_t ndl_bar_pct;
    unsigned in_stop_zone : 1;
} ui_vm_ndl_stop_t;

typedef struct
{
    float rate;
    unsigned is_moving : 1;
    unsigned flash_on : 1;
    int8_t direction;
} ui_vm_ascent_t;

typedef struct
{
    char text[48];
} ui_vm_value_text_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_DASHBOARD_TYPES_H */
