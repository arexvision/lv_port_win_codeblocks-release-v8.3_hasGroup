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

#include <string.h>

#define UI_ROUTER_COMP_ID_MAX  64U

typedef struct
{
    uint8_t dash_page;
    uint8_t page_id;
    uint8_t storage_pos;
    uint8_t custom_card_idx;
    dirty_mask_t visible_widget_mask;
} ui_router_visible_ctx_t;

__attribute__((weak)) void app_ui_perf_note_router_cost(uint32_t total_ms,
                                                        uint32_t deco_ms,
                                                        uint32_t ndl_ms,
                                                        uint32_t plan_ms,
                                                        uint32_t widget_ms,
                                                        uint32_t info_ms,
                                                        uint32_t mask)
{
    (void)total_ms;
    (void)deco_ms;
    (void)ndl_ms;
    (void)plan_ms;
    (void)widget_ms;
    (void)info_ms;
    (void)mask;
}

static dirty_mask_t ui_router_widget_dirty_mask(comp_id_t widget_id)
{
    switch (widget_id)
    {
    case COMP_NDL_STOP_1606:
        return DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS;
    case COMP_DEPTH_1612:
    case COMP_DEPTH_1606:
        return DIRTY_DIVE_PROFILE | DIRTY_SYSTEM;
    case COMP_DIVE_TIME_1606:
    case COMP_SURFACE_TIME_1606:
    case COMP_ASCENT_0806:
    case COMP_ASCENT_0812:
    case COMP_DEPTH_MAX_0806:
    case COMP_DEPTH_AVG_0806:
        return DIRTY_DIVE_PROFILE | DIRTY_SYSTEM;
    case COMP_TTS_0806:
    case COMP_STOP_DEPTH_0806:
    case COMP_STOP_TIME_1606:
    case COMP_CEILING_0806:
    case COMP_TTS_AT_5MIN_0806:
    case COMP_TTS_DELTA_5MIN_0806:
    case COMP_NDL_UP_3M_0806:
    case COMP_NDL_DOWN_3M_0806:
    case COMP_NDL_DELTA_3M_0806:
        return DIRTY_DECO_STATUS | DIRTY_SYSTEM;
    case COMP_GAS_1606:
    case COMP_PPO2_0806:
    case COMP_MOD_0806:
    case COMP_GAS_MIX_1606:
    case COMP_GAS_DENS_0806:
    case COMP_FIO2_0806:
    case COMP_POD_0806:
    case COMP_GTR_0806:
    case COMP_RMV_0806:
    case COMP_SAC_0806:
        return DIRTY_GAS_SUPPLY | DIRTY_SYSTEM;
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
    case COMP_MLX_2406:
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

static void ui_router_refresh_widget_if_dirty(const grid_widget_t *widget,
                                              dirty_mask_t mask,
                                              uint8_t refreshed[UI_ROUTER_COMP_ID_MAX])
{
    comp_id_t widget_id;

    if (widget == NULL || widget->widget_id == COMP_EMPTY)
    {
        return;
    }

    widget_id = (comp_id_t)widget->widget_id;
    if ((uint8_t)widget_id < UI_ROUTER_COMP_ID_MAX && refreshed[(uint8_t)widget_id] != 0U)
    {
        return;
    }

    if (ui_router_widget_dirty_mask(widget_id) & mask)
    {
        if ((uint8_t)widget_id < UI_ROUTER_COMP_ID_MAX)
        {
            refreshed[(uint8_t)widget_id] = 1U;
        }
        comp_sync_data(widget_id);
    }
}

static void ui_router_visible_ctx_update(ui_router_visible_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->dash_page = ui_state_get_dash_page();
    ctx->storage_pos = page_storage_pos(ctx->dash_page);
    ctx->page_id = (ctx->storage_pos == 0xFFU) ? PAGE_ID_UNUSED : g_sys_page_order(ctx->storage_pos);
    ctx->custom_card_idx = 0xFFU;
    ctx->visible_widget_mask = DIRTY_NONE;

    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget && widget->widget_id != COMP_EMPTY)
        {
            ctx->visible_widget_mask |= ui_router_widget_dirty_mask(widget->widget_id);
        }
    }

    if (ctx->page_id == PAGE_ID_CUSTOM_GRID && ctx->storage_pos != 0xFFU)
    {
        ctx->custom_card_idx = ui_custom_card_slot_get(ctx->storage_pos);
        if (ctx->custom_card_idx < ui_custom_card_count_get() &&
            ctx->custom_card_idx < MAX_CUSTOM_CARDS)
        {
            for (uint8_t i = 0; i < ui_custom_card_widget_count_get(ctx->custom_card_idx); i++)
            {
                const grid_widget_t *widget = ui_custom_card_widget_get(ctx->custom_card_idx, i);
                if (widget && widget->widget_id != COMP_EMPTY)
                {
                    ctx->visible_widget_mask |= ui_router_widget_dirty_mask(widget->widget_id);
                }
            }
        }
    }
}

static bool ui_router_visible_page_id(const ui_router_visible_ctx_t *ctx, uint8_t page_id)
{
    return (ctx != NULL && ctx->page_id == page_id);
}

static bool ui_router_visible_custom_card(const ui_router_visible_ctx_t *ctx, uint8_t custom_card_idx)
{
    return (ctx != NULL &&
            ctx->page_id == PAGE_ID_CUSTOM_GRID &&
            ctx->custom_card_idx == custom_card_idx);
}

static void ui_router_refresh_layout_widgets(dirty_mask_t mask,
                                             const ui_router_visible_ctx_t *ctx)
{
    uint8_t refreshed[UI_ROUTER_COMP_ID_MAX];

    memset(refreshed, 0, sizeof(refreshed));

    for (uint8_t i = 0; i < ui_left_widget_count_get(); i++)
    {
        ui_router_refresh_widget_if_dirty(ui_left_widget_get(i), mask, refreshed);
    }

    if (ctx == NULL ||
        ctx->page_id != PAGE_ID_CUSTOM_GRID ||
        ctx->custom_card_idx >= ui_custom_card_count_get() ||
        ctx->custom_card_idx >= MAX_CUSTOM_CARDS)
    {
        return;
    }

    for (uint8_t i = 0; i < ui_custom_card_widget_count_get(ctx->custom_card_idx); i++)
    {
        ui_router_refresh_widget_if_dirty(ui_custom_card_widget_get(ctx->custom_card_idx, i), mask, refreshed);
    }
}

static dirty_mask_t ui_router_layout_subscription_mask(const ui_router_visible_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return DIRTY_NONE;
    }

    switch (ctx->page_id)
    {
    case PAGE_ID_COMPASS:
        return ctx->visible_widget_mask | DIRTY_COMPASS;
    case PAGE_ID_DECO:
        return ctx->visible_widget_mask |
               DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG;
    case PAGE_ID_GAS:
        return ctx->visible_widget_mask | DIRTY_GAS_SUPPLY;
    case PAGE_ID_PLAN:
        return ctx->visible_widget_mask | DIRTY_PLAN;
    default:
        return ctx->visible_widget_mask;
    }
}

static dirty_mask_t ui_router_state_subscription_mask(void)
{
    ui_state_t state = ui_state_get_state();

    if (state == UI_SUB_MENU && ui_state_get_sub_parent() == UI_INFO)
    {
        return DIRTY_INFO_REFRESH_MASK;
    }

    return DIRTY_NONE;
}

static bool ui_router_widget_visible(comp_id_t widget_id, const ui_router_visible_ctx_t *ctx)
{
    for (uint8_t i = 0U; i < ui_left_widget_count_get(); i++)
    {
        const grid_widget_t *widget = ui_left_widget_get(i);
        if (widget != NULL && (comp_id_t)widget->widget_id == widget_id)
        {
            return true;
        }
    }

    if (ctx != NULL &&
        ctx->page_id == PAGE_ID_CUSTOM_GRID &&
        ctx->custom_card_idx < ui_custom_card_count_get() &&
        ctx->custom_card_idx < MAX_CUSTOM_CARDS)
    {
        for (uint8_t i = 0U; i < ui_custom_card_widget_count_get(ctx->custom_card_idx); i++)
        {
            const grid_widget_t *widget = ui_custom_card_widget_get(ctx->custom_card_idx, i);
            if (widget != NULL && (comp_id_t)widget->widget_id == widget_id)
            {
                return true;
            }
        }
    }

    return false;
}

static bool ui_router_any_widget_visible(dirty_mask_t widget_mask, const ui_router_visible_ctx_t *ctx)
{
    return (ctx != NULL && (ctx->visible_widget_mask & widget_mask) != 0U);
}

static bool ui_router_deco_vm_needed(dirty_mask_t mask, const ui_router_visible_ctx_t *ctx)
{
    if (ui_router_visible_page_id(ctx, PAGE_ID_DECO))
    {
        return true;
    }

    if ((mask & DIRTY_TISSUE_TOX) != 0U &&
        ui_router_any_widget_visible(DIRTY_TISSUE_TOX, ctx))
    {
        return true;
    }

    return ui_router_widget_visible(COMP_NDL_STOP_1606, ctx);
}

static dirty_mask_t ui_router_subscription_mask(const ui_router_visible_ctx_t *ctx)
{
    return DIRTY_UI_LAYOUT | DIRTY_ALARM |
           ui_router_layout_subscription_mask(ctx) |
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
    uint32_t route_start_ms = lv_tick_get();
    uint32_t deco_ms = 0U;
    uint32_t ndl_ms = 0U;
    uint32_t plan_ms = 0U;
    uint32_t widget_ms = 0U;
    uint32_t info_ms = 0U;
    dirty_mask_t original_mask = mask;
    ui_router_visible_ctx_t visible_ctx;

    ui_router_visible_ctx_update(&visible_ctx);

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
        bus_requeue_dirty_immediate(mask & ~DIRTY_UI_LAYOUT);
        return;
    }

    mask &= ui_router_subscription_mask(&visible_ctx);
    if (mask == DIRTY_NONE)
    {
        return;
    }

    if ((mask & (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG)) &&
        ui_router_deco_vm_needed(mask, &visible_ctx))
    {
        uint32_t start_ms = lv_tick_get();
        ui_vm_deco_update(&deco_vm, NULL, NULL);
        deco_vm_ready = true;
        if (ui_router_visible_page_id(&visible_ctx, PAGE_ID_DECO))
        {
            page_registry_update_deco_vm(&deco_vm);
        }
        deco_ms += lv_tick_get() - start_ms;
    }

    if (mask & (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX))
    {
        uint32_t start_ms = lv_tick_get();
        /* 减压态势相关域共享 deco VM，但组织柱只在组织/毒性域变化时重绘。 */
        if (mask & DIRTY_TISSUE_TOX)
        {
            if (deco_vm_ready)
            {
                comp_refresh_tissue_widgets(&deco_vm, mask);
            }
        }
        deco_ms += lv_tick_get() - start_ms;
    }

    if ((mask & DIRTY_DIVE_PROFILE) &&
        (ui_router_widget_visible(COMP_DEPTH_1612, &visible_ctx) ||
         ui_router_widget_visible(COMP_ASCENT_0806, &visible_ctx) ||
         ui_router_widget_visible(COMP_ASCENT_0812, &visible_ctx)))
    {
        uint32_t start_ms = lv_tick_get();
        ui_vm_ascent_update(&ascent_vm, bus_get_ascent_rate());
        comp_refresh_ascent_icons(&ascent_vm);
        deco_ms += lv_tick_get() - start_ms;
    }

    if ((mask & (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS)) &&
        ui_router_widget_visible(COMP_NDL_STOP_1606, &visible_ctx))
    {
        uint32_t start_ms = lv_tick_get();
        ui_vm_ndl_stop_update(&ndl_stop_vm, NULL);
        comp_refresh_ndl_stop_vm(&ndl_stop_vm, mask);
        ndl_ms += lv_tick_get() - start_ms;
    }

    if (mask & DIRTY_SYSTEM)
    {
        if (ui_router_widget_visible(COMP_BATT_TEMP_0806, &visible_ctx) ||
            ui_router_widget_visible(COMP_PRJ_TEMP_0806, &visible_ctx))
        {
            /* 左侧固定栏的辅助温度标签不是通用 widget 子树，不能依赖后续
             * ui_router_refresh_layout_widgets()；其它系统类 widget 交给统一
             * layout widget 路由，避免同一帧重复写 LVGL label。 */
            refresh_left_aux_slots();
        }
    }

    if ((mask & DIRTY_COMPASS) && ui_router_visible_page_id(&visible_ctx, PAGE_ID_COMPASS))
    {
        ui_vm_compass_update(&compass_vm, NULL, NULL);
        card_compass_refresh_heading_vm(&compass_vm, false);
    }

    if (mask & DIRTY_GAS_SUPPLY)
    {
        /* 气体供应域既影响菜单也影响左侧显示，因此要同时刷新两条路径。 */
        /* 一次切气或气体参数变化会同时波及：
         * - 当前气体名称/高亮
         * - PPO2/MOD 等派生数据显示
         * - GAS 菜单页自身的选中态
         * 所以这里不能只刷某一个 label。 */
        if (ui_router_visible_page_id(&visible_ctx, PAGE_ID_GAS))
        {
            ui_vm_gas_update(&gas_vm,
                             NULL,
                             NULL,
                             ui_state_get_state(),
                             ui_state_get_gas_cursor());
            page_registry_update_gas_vm(&gas_vm);
        }
    }

    if (mask & DIRTY_PLAN)
    {
        uint32_t start_ms = lv_tick_get();
        ui_vm_plan_chart_update(&plan_chart_vm);
        if (ui_router_visible_page_id(&visible_ctx, PAGE_ID_PLAN))
        {
            page_registry_update_plan_vm(&plan_chart_vm);
        }
        plan_ms += lv_tick_get() - start_ms;
    }

    if (mask & DIRTY_ALARM)
    {
        /* 告警动画与样式节拍在 ui_update_task() 统一推进，router 只负责消费脏标记。 */
    }

    if (mask & DIRTY_LOGBOOK)
    {
        screen_refresh_logbook_if_open();
    }

    if (mask & DIRTY_INFO_REFRESH_MASK)
    {
        uint32_t start_ms = lv_tick_get();
        if (screen_page_id_refresh_visible(PAGE_ID_MENU))
        {
            menu_entry_update();
        }
        screen_refresh_info_submenu_if_open();
        if (mask & DIRTY_SYSTEM)
        {
            screen_refresh_setup_menu();
            screen_refresh_settings_submenu_if_open();
        }
        info_ms += lv_tick_get() - start_ms;
    }

    if (mask & DIRTY_WIDGET_REFRESH_MASK)
    {
        uint32_t start_ms = lv_tick_get();
        /* 只刷新当前布局里订阅本轮 dirty 的组件，避免新模块再维护临时组件清单。 */
        ui_router_refresh_layout_widgets(mask & DIRTY_WIDGET_REFRESH_MASK, &visible_ctx);
        widget_ms += lv_tick_get() - start_ms;
    }

    app_ui_perf_note_router_cost(lv_tick_get() - route_start_ms,
                                 deco_ms,
                                 ndl_ms,
                                 plan_ms,
                                 widget_ms,
                                 info_ms,
                                 (uint32_t)original_mask);
}
