/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_system_view.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_system_view.h"

#include "../data.h"
#include "../callbacks.h"

#include <stdio.h>
#include <string.h>

void ui_vm_submenu_view_update(ui_vm_submenu_view_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->light_power_on = bus_get_light_power() ? 1U : 0U;
    vm->light_mode = (uint8_t)bus_get_light_mode();
    vm->light_color = (uint8_t)bus_get_light_color();
    vm->light_level = (uint8_t)bus_get_light_level();
}

void ui_vm_brightness_update(ui_vm_brightness_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));
    vm->brightness_level = bus_get_brightness();
}

void ui_vm_left_aux_update(ui_vm_left_aux_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    (void)snprintf(vm->battery_temp_text, sizeof(vm->battery_temp_text), "%.1f%s", (double)bus_get_temperature_display(bus_get_bat_temperature()), bus_get_temperature_unit_label());
    (void)snprintf(vm->project_temp_text, sizeof(vm->project_temp_text), "%.1f%s", (double)bus_get_temperature_display(bus_get_prj_temperature()), bus_get_temperature_unit_label());
}
