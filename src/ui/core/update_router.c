/*
 * 文件: src/app_ui/ui/core/update_router.c
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "update_router.h"

#include "../screen/page_registry.h"
#include "data.h"
#include "vm/ui_vm_dashboard.h"
#include "vm/ui_vm_plan_chart.h"
#include "../screen/layout_view.h"
#include "../screen/screen.h"
#include "ui_engine.h"
#include "ui_state.h"
#include "../comp/comp_update.h"
#include "../comp/comp_view.h"
#include "../cards/card_compass.h"

static void ui_router_refresh_text_widget(comp_id_t widget_id, uint8_t pod_index)
{
    /* 文本型组件的刷新统一走 VM 更新 -> comp_set_text，避免每个调用点重复组装字符串。 */
    ui_vm_value_text_t value_vm;

    ui_vm_value_text_update(&value_vm, widget_id, pod_index);
    comp_set_text(widget_id, value_vm.text);
}

static void ui_router_refresh_sensor_preview_widgets(void)
{
    static const comp_id_t widgets[] =
    {
        COMP_GYRO_1606,
        COMP_BATT_V_0806,
        COMP_BATT_TEMP_0806,
        COMP_PRJ_TEMP_0806,
        COMP_CHARGE_0806,
        COMP_PRESSURE_0806,
        COMP_NOFLY_0806,
        COMP_ACCEL_1606,
        COMP_MAG_1606,
        COMP_TMAG_1606,
        COMP_ATTITUDE_1606,
        COMP_BLE_RSSI_0806,
        COMP_CPU_0806,
        COMP_FPS_0806,
        COMP_SENSOR_STAT_0806,
    };

    for (uint8_t i = 0; i < (uint8_t)(sizeof(widgets) / sizeof(widgets[0])); i++)
    {
        ui_router_refresh_text_widget(widgets[i], 0U);
    }
}

void ui_update_router_dispatch(uint32_t mask)
{
    /* dirty mask 是 UI 更新路由的核心入口：按位决定本轮要刷新哪些数据和页面。 */
    /* 这里刻意把“数据变更”与“UI 如何刷新”隔离开：
     * 上游 Data Bus 只负责打脏位，不关心页面细节；
     * router 负责把脏位翻译成 VM 更新、组件刷新或页面重建。
     * 这样可以避免传感器/算法代码直接依赖 LVGL 对象。 */
    ui_vm_compass_t compass_vm;
    ui_vm_deco_t deco_vm;
    ui_vm_gas_t gas_vm;
    ui_vm_ndl_stop_t ndl_stop_vm;
    ui_vm_ascent_t ascent_vm;
    ui_vm_plan_chart_t plan_chart_vm;
    bool deco_vm_ready = false;
    bool refresh_all_widgets = false;

    if (mask & DIRTY_UI_LAYOUT)
    {
        /* 布局相关脏标记优先处理，因为后续所有对象位置都依赖重建后的坐标系。 */
        /* 注意这里先关闭 invalidation，再整页重建，最后把剩余脏位重新排队。
         * 目的有两个：
         * 1. 防止重建过程中出现半成品闪屏；
         * 2. 避免旧对象已经销毁时，后续刷新逻辑还在访问旧引用。 */
        lv_disp_t *disp = lv_disp_get_default();
        if (disp)
        {
            lv_disp_enable_invalidation(disp, false);
        }
        screen_rebuild_full();
        if (disp)
        {
            lv_disp_enable_invalidation(disp, true);
        }
        bus_requeue_dirty(mask & ~DIRTY_UI_LAYOUT);
        return;
    }

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES | DIRTY_CNS | DIRTY_OTU))
    {
        ui_vm_deco_update(&deco_vm, NULL, NULL);
        deco_vm_ready = true;
        page_registry_update_deco_vm(&deco_vm);
    }

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES | DIRTY_ASCENT))
    {
        /* 深度、NDL、TTS、组织和上升率通常联动刷新，所以归并为一组处理。 */
        /* 这一组数据本质上都属于“减压态势”：
         * 任意一个变化，往往都要求左侧主卡、DECO 卡和上升率图标同步更新。 */
        refresh_all_widgets = true;
        if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
        {
            if (deco_vm_ready)
            {
                comp_refresh_tissue_widgets(&deco_vm, mask);
            }
        }
        ui_vm_ascent_update(&ascent_vm, bus_get_ascent_rate());
        comp_refresh_ascent_icons(&ascent_vm);
    }

    ui_vm_ndl_stop_update(&ndl_stop_vm, NULL);
    comp_refresh_ndl_stop_vm(&ndl_stop_vm, mask);

    if (mask & DIRTY_POD)
    {
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_BATT)
    {
        /* 电量刷新只影响部分文本组件，不必触发整页重绘。 */
        ui_router_refresh_text_widget(COMP_BATTERY_0806, 0U);
    }

    if (mask & DIRTY_HEADING)
    {
        ui_vm_compass_update(&compass_vm, NULL, NULL);
        card_compass_refresh_heading_vm(&compass_vm, false);
        ui_router_refresh_sensor_preview_widgets();
    }

    if (mask & DIRTY_DIVE_TIME)
    {
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_PPO2)
    {
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_GAS)
    {
        /* 气体变化既影响菜单也影响左侧显示，因此要同时刷新两条路径。 */
        /* 一次切气会同时波及：
         * - 当前气体名称/高亮
         * - PPO2/MOD 等派生数据显示
         * - GAS 菜单页自身的选中态
         * 所以这里不能只刷某一个 label。 */
        ui_vm_gas_update(&gas_vm,
                         NULL,
                         NULL,
                         ui_state_get_state(),
                         ui_state_get_gas_cursor());
        page_registry_update_gas_vm(&gas_vm);
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_TRAJECTORY)
    {
        ui_vm_plan_chart_update(&plan_chart_vm);
        page_registry_update_plan_vm(&plan_chart_vm);
    }

    if (mask & DIRTY_CNS)
    {
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_OTU)
    {
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_TEMP)
    {
        ui_router_refresh_text_widget(COMP_TEMP_0806, 0U);
        refresh_left_aux_slots();
    }

    if (mask & DIRTY_DEPTH)
    {
        ui_router_refresh_text_widget(COMP_DEPTH_MAX_0806, 0U);
        ui_router_refresh_text_widget(COMP_DEPTH_AVG_0806, 0U);
    }

    if (mask & DIRTY_TEMP)
    {
        ui_router_refresh_text_widget(COMP_TEMP_MIN_0806, 0U);
        ui_router_refresh_text_widget(COMP_TEMP_AVG_0806, 0U);
    }

    if (mask & (DIRTY_GF_SETTING | DIRTY_MOD | DIRTY_CEILING |
                DIRTY_GAS_MIX | DIRTY_GAS_DENS | DIRTY_FIO2))
    {
        refresh_all_widgets = true;
    }

    if (mask & (DIRTY_BATT | DIRTY_TEMP))
    {
        comp_refresh_sys(mask);
    }

    if (mask & DIRTY_ALARM)
    {
        /* 告警动画与样式节拍在 ui_update_task() 统一推进，router 只负责消费脏标记。 */
    }

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_NDL_STOP | DIRTY_TTS |
                DIRTY_DIVE_TIME | DIRTY_GAS | DIRTY_TEMP | DIRTY_BATT |
                DIRTY_POD | DIRTY_PPO2 | DIRTY_CNS | DIRTY_OTU |
                DIRTY_TISSUES | DIRTY_ASCENT | DIRTY_GF_SETTING | DIRTY_MOD |
                DIRTY_CEILING | DIRTY_GAS_MIX | DIRTY_GAS_DENS |
                DIRTY_FIO2))
    {
        screen_refresh_info_submenu_if_open();
    }

    if (refresh_all_widgets)
    {
        /* 需要整组刷新时，统一下发到 screen 层，由它再分发到各 widget。 */
        /* 这里故意不在 router 内部逐个处理所有 widget，
         * 因为 screen 层掌握当前布局配置，知道左侧和 5F 卡片到底摆了哪些组件。 */
        screen_refresh_all_widgets();
    }
}
