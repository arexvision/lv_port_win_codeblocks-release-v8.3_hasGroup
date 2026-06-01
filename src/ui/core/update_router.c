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
        COMP_GYRO_2406,
        COMP_BATT_V_0806,
        COMP_BATT_TEMP_0806,
        COMP_PRJ_TEMP_0806,
        COMP_CHARGE_0806,
        COMP_PRESSURE_0806,
        COMP_NOFLY_0806,
        COMP_ACCEL_2406,
        COMP_MAG_2406,
        COMP_TMAG_2406,
        COMP_ATTITUDE_2406,
        COMP_BLE_RSSI_0806,
        COMP_CPU_0806,
        COMP_FPS_0806,
        COMP_SENSOR_STAT_1606,
    };

    for (uint8_t i = 0; i < (uint8_t)(sizeof(widgets) / sizeof(widgets[0])); i++)
    {
        ui_router_refresh_text_widget(widgets[i], 0U);
    }
}

static dirty_mask_t ui_router_widget_dirty_mask(comp_id_t widget_id)
{
    switch (widget_id)
    {
    case COMP_NDL_STOP_1606:
        return DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS;
    case COMP_DEPTH_1612:
    case COMP_DEPTH_1606:
    case COMP_DIVE_TIME_1606:
    case COMP_ASCENT_0806:
    case COMP_ASCENT_0812:
    case COMP_DEPTH_MAX_0806:
    case COMP_DEPTH_AVG_0806:
        return DIRTY_DIVE_PROFILE;
    case COMP_TTS_0806:
    case COMP_STOP_DEPTH_0806:
    case COMP_STOP_TIME_1606:
    case COMP_CEILING_0806:
        return DIRTY_DECO_STATUS;
    case COMP_GAS_1606:
    case COMP_PPO2_0806:
    case COMP_MOD_0806:
    case COMP_GAS_MIX_1606:
    case COMP_GAS_DENS_0806:
    case COMP_FIO2_0806:
    case COMP_POD_0806:
        return DIRTY_GAS_SUPPLY;
    case COMP_SYS_1606:
    case COMP_TEMP_0806:
    case COMP_TIME_1606:
    case COMP_BATTERY_0806:
    case COMP_BATT_TEMP_0806:
    case COMP_PRJ_TEMP_0806:
    case COMP_TEMP_MIN_0806:
    case COMP_TEMP_AVG_0806:
        return DIRTY_SYSTEM;
    case COMP_COMPASS_1612:
    case COMP_HEADING_0806:
        return DIRTY_COMPASS;
    case COMP_TISSUE_GF_4012:
    case COMP_TISSUE_RAW_4012:
    case COMP_SURF_GF_0806:
    case COMP_GF99_0806:
    case COMP_CNS_0806:
    case COMP_OTU_0806:
        return DIRTY_TISSUE_TOX;
    case COMP_GF_0806:
        return DIRTY_DIVE_CONFIG;
    case COMP_GYRO_2406:
    case COMP_BATT_V_0806:
    case COMP_CHARGE_0806:
    case COMP_PRESSURE_0806:
    case COMP_NOFLY_0806:
    case COMP_ACCEL_2406:
    case COMP_MAG_2406:
    case COMP_TMAG_2406:
    case COMP_ATTITUDE_2406:
    case COMP_BLE_RSSI_0806:
    case COMP_CPU_0806:
    case COMP_FPS_0806:
    case COMP_SENSOR_STAT_1606:
        return DIRTY_SENSOR;
    default:
        return DIRTY_NONE;
    }
}

static dirty_mask_t ui_router_custom_card_subscription_mask(uint8_t custom_card_idx)
{
    dirty_mask_t mask = DIRTY_NONE;

    if (custom_card_idx >= ui_custom_card_count_get() || custom_card_idx >= MAX_CUSTOM_CARDS)
    {
        return DIRTY_NONE;
    }

    for (uint8_t i = 0; i < ui_custom_card_widget_count_get(custom_card_idx); i++)
    {
        const grid_widget_t *widget = ui_custom_card_widget_get(custom_card_idx, i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            mask |= ui_router_widget_dirty_mask(widget->widget_id);
        }
    }

    return mask;
}

static dirty_mask_t ui_router_layout_subscription_mask(void)
{
    dirty_mask_t mask = DIRTY_NONE;

    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            mask |= ui_router_widget_dirty_mask(widget->widget_id);
        }
    }

    for (uint8_t storage_pos = PAGE_POS_DYNAMIC_FIRST; storage_pos < PAGE_POS_SETUP; storage_pos++)
    {
        uint8_t page_id = g_sys_page_order(storage_pos);

        switch (page_id)
        {
        case PAGE_ID_COMPASS:
            mask |= DIRTY_COMPASS;
            break;
        case PAGE_ID_DECO:
            mask |= DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG;
            break;
        case PAGE_ID_GAS:
            mask |= DIRTY_GAS_SUPPLY;
            break;
        case PAGE_ID_PLAN:
            mask |= DIRTY_PLAN;
            break;
        case PAGE_ID_CUSTOM_GRID:
        {
            uint8_t custom_card_idx = ui_custom_card_slot_get(storage_pos);
            mask |= ui_router_custom_card_subscription_mask(custom_card_idx);
            break;
        }
        default:
            break;
        }
    }

    return mask;
}

static dirty_mask_t ui_router_state_subscription_mask(void)
{
    ui_state_t state = ui_state_get_state();

    if (state == UI_INFO || (state == UI_SUB_MENU && ui_state_get_sub_parent() == UI_INFO))
    {
        return DIRTY_INFO_REFRESH_MASK;
    }

    return DIRTY_NONE;
}

static dirty_mask_t ui_router_subscription_mask(void)
{
    return DIRTY_UI_LAYOUT | DIRTY_ALARM |
           ui_router_layout_subscription_mask() |
           ui_router_state_subscription_mask();
}

void ui_update_router_dispatch(dirty_mask_t mask)
{
    /* dirty mask 是 UI 更新路由的核心入口：按刷新域决定本轮要刷新哪些数据和页面。 */
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

    mask &= ui_router_subscription_mask();
    if (mask == DIRTY_NONE)
    {
        return;
    }

    if (mask & (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG))
    {
        ui_vm_deco_update(&deco_vm, NULL, NULL);
        deco_vm_ready = true;
        page_registry_update_deco_vm(&deco_vm);
    }

    if (mask & (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX))
    {
        /* 减压态势相关域共享 deco VM，但组织柱只在组织/毒性域变化时重绘。 */
        if (mask & DIRTY_TISSUE_TOX)
        {
            if (deco_vm_ready)
            {
                comp_refresh_tissue_widgets(&deco_vm, mask);
            }
        }
    }

    if (mask & DIRTY_DIVE_PROFILE)
    {
        ui_vm_ascent_update(&ascent_vm, bus_get_ascent_rate());
        comp_refresh_ascent_icons(&ascent_vm);
    }

    ui_vm_ndl_stop_update(&ndl_stop_vm, NULL);
    comp_refresh_ndl_stop_vm(&ndl_stop_vm, mask);

    if (mask & DIRTY_SYSTEM)
    {
        /* 系统域统一覆盖电量、主温和板级温度，SYS 复合组件一次刷新两格。 */
        ui_router_refresh_text_widget(COMP_BATTERY_0806, 0U);
        ui_router_refresh_text_widget(COMP_TEMP_0806, 0U);
        ui_router_refresh_text_widget(COMP_TEMP_MIN_0806, 0U);
        ui_router_refresh_text_widget(COMP_TEMP_AVG_0806, 0U);
        refresh_left_aux_slots();
        comp_refresh_sys(mask);
    }

    if (mask & DIRTY_COMPASS)
    {
        ui_vm_compass_update(&compass_vm, NULL, NULL);
        card_compass_refresh_heading_vm(&compass_vm, false);
    }

    if (mask & DIRTY_SENSOR)
    {
        ui_router_refresh_sensor_preview_widgets();
    }

    if (mask & DIRTY_GAS_SUPPLY)
    {
        /* 气体供应域既影响菜单也影响左侧显示，因此要同时刷新两条路径。 */
        /* 一次切气或气体参数变化会同时波及：
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
    }

    if (mask & DIRTY_PLAN)
    {
        ui_vm_plan_chart_update(&plan_chart_vm);
        page_registry_update_plan_vm(&plan_chart_vm);
    }

    if (mask & DIRTY_DIVE_PROFILE)
    {
        ui_router_refresh_text_widget(COMP_DEPTH_MAX_0806, 0U);
        ui_router_refresh_text_widget(COMP_DEPTH_AVG_0806, 0U);
    }

    if (mask & DIRTY_ALARM)
    {
        /* 告警动画与样式节拍在 ui_update_task() 统一推进，router 只负责消费脏标记。 */
    }

    if (mask & DIRTY_INFO_REFRESH_MASK)
    {
        screen_refresh_info_submenu_if_open();
    }

    if (mask & DIRTY_WIDGET_REFRESH_MASK)
    {
        refresh_all_widgets = true;
    }

    if (refresh_all_widgets)
    {
        /* 需要整组刷新时，统一下发到 screen 层，由它再分发到各 widget。 */
        /* 这里故意不在 router 内部逐个处理所有 widget，
         * 因为 screen 层掌握当前布局配置，知道左侧和 5F 卡片到底摆了哪些组件。 */
        screen_refresh_all_widgets();
    }
}
