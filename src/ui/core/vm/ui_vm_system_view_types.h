/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_system_view_types.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_SYSTEM_VIEW_TYPES_H
#define UI_VM_SYSTEM_VIEW_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t light_power_on;
    uint8_t light_mode;
    uint8_t light_color;
    uint8_t light_level;
} ui_vm_submenu_view_t;

typedef struct
{
    uint8_t brightness_level;
} ui_vm_brightness_t;

typedef struct
{
    char battery_temp_text[16];
    char project_temp_text[16];
} ui_vm_left_aux_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_SYSTEM_VIEW_TYPES_H */
