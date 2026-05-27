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
    ui_vm_value_text_t value_vm;

    ui_vm_value_text_update(&value_vm, widget_id, pod_index);
    comp_set_text(widget_id, value_vm.text);
}

void ui_update_router_dispatch(uint32_t mask)
{
    ui_vm_compass_t compass_vm;
    ui_vm_deco_t deco_vm;
    ui_vm_gas_t gas_vm;
    ui_vm_sys_t sys_vm;
    ui_vm_ndl_stop_t ndl_stop_vm;
    ui_vm_ascent_t ascent_vm;
    ui_vm_plan_chart_t plan_chart_vm;
    bool refresh_all_widgets = false;

    if (mask & DIRTY_UI_LAYOUT)
    {
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

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES | DIRTY_ASCENT))
    {
        refresh_all_widgets = true;
        if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
        {
            ui_vm_deco_update(&deco_vm, NULL, NULL);
            card_deco_update();
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
        ui_router_refresh_text_widget(COMP_BATTERY_0806, 0U);
    }

    if (mask & DIRTY_HEADING)
    {
        ui_vm_compass_update(&compass_vm, NULL, NULL);
        card_compass_refresh_heading_vm(&compass_vm, false);
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
        ui_vm_gas_update(&gas_vm,
                         NULL,
                         NULL,
                         ui_state_get_state(),
                         ui_state_get_gas_cursor());
        screen_refresh_gas_menu();
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_TRAJECTORY)
    {
        ui_vm_plan_chart_update(&plan_chart_vm);
        page_registry_update_plan_vm(&plan_chart_vm);
    }

    if (mask & DIRTY_CNS)
    {
        ui_vm_deco_update(&deco_vm, NULL, NULL);
        card_deco_update();
        refresh_all_widgets = true;
    }

    if (mask & DIRTY_OTU)
    {
        ui_vm_deco_update(&deco_vm, NULL, NULL);
        card_deco_update();
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
        ui_vm_sys_update(&sys_vm, NULL);
        comp_refresh_sys_vm(&sys_vm, mask);
    }

    if (mask & DIRTY_ALARM)
    {
        /* 告警动画与样式节拍在 ui_update_task() 统一推进。
         * Router 仅消费 dirty，不承载周期性业务。 */
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
        screen_refresh_all_widgets();
    }
}
