#include "arex_ui_update_router.h"

#include "arex_alarm_view.h"
#include "arex_card_registry.h"
#include "arex_data.h"
#include "arex_layout_view.h"
#include "arex_screen.h"
#include "arex_ui_engine.h"
#include "arex_ui_state.h"
#include "arex_widget_update.h"
#include "arex_widget_view.h"
#include "cards/card_compass.h"

#include <math.h>

static uint32_t s_deco_last_refresh_ms = 0;

static void arex_ui_update_router_alarm_tick(void)
{
    arex_alarm_view_context_t ctx;
    ctx.safe_zone = arex_get_safe_zone();
    ctx.left_anchor = g_left_anchor_obj;
    ctx.custom_cards = g_card_custom_objs;
    ctx.custom_card_count = g_card_custom_obj_count;
    ctx.max_custom_cards = AREX_MAX_CUSTOM_CARDS;
    ctx.layout_order = g_sys_config.layout_order;
    ctx.safe_zone_w = g_sys_config.safe_zone_w;
    ctx.left_anchor_w = AREX_LEFT_ANCHOR_W;
    ctx.panel_gap_px = (uint16_t)(g_sys_config.gap_u * AREX_BASE_U);
    ctx.alarm_pending_click = &g_ui.alarm_pending_click;
    arex_alarm_view_tick(&ctx);
}

static void arex_ui_update_router_setup_tick(void)
{
    static arex_compass_cal_ui_state_t s_last_compass_cal_state = AREX_COMPASS_CAL_IDLE;
    arex_compass_cal_ui_state_t cal_state = arex_get_compass_calibration_ui_state();

    if (cal_state != s_last_compass_cal_state)
    {
        s_last_compass_cal_state = cal_state;
        arex_screen_refresh_setup_menu();
    }
}

static void arex_ui_update_router_ascent_heartbeat(void)
{
    static bool s_last_flash_state = false;
    bool current_flash_state = (lv_tick_get() / 500U) % 2U == 0U;

    if (current_flash_state == s_last_flash_state)
    {
        return;
    }

    s_last_flash_state = current_flash_state;

    if (fabsf(g_sensor_data.ascent_rate) >= AREX_RATE_STILL_THRESHOLD)
    {
        g_sensor_data.dirty_mask |= DIRTY_DEPTH;
    }
}

static void arex_ui_update_router_refresh_plan(void)
{
    uint32_t now = lv_tick_get();

#if AREX_DECO_REFRESH_MS > 0
    if (now - s_deco_last_refresh_ms >= AREX_DECO_REFRESH_MS)
    {
        s_deco_last_refresh_ms = now;
        card_plan_update();
    }
#else
    (void)s_deco_last_refresh_ms;
    card_plan_update();
#endif
}

void arex_ui_update_router_tick(void)
{
    arex_ui_update_router_alarm_tick();
    arex_ui_update_router_setup_tick();
    arex_ui_update_router_ascent_heartbeat();
}

void arex_ui_update_router_dispatch(uint32_t mask)
{
    if (mask & DIRTY_UI_LAYOUT)
    {
        lv_disp_t *disp = lv_disp_get_default();
        if (disp)
        {
            lv_disp_enable_invalidation(disp, false);
        }
        arex_screen_rebuild_full();
        if (disp)
        {
            lv_disp_enable_invalidation(disp, true);
        }
        arex_bus_requeue_dirty(mask & ~DIRTY_UI_LAYOUT);
        return;
    }

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
    {
        arex_screen_refresh_all_widgets();
        card_deco_update();
        arex_widget_refresh_ascent_icons(g_sensor_data.ascent_rate);
    }

    arex_widget_refresh_ndl_stop(mask);

    if (mask & DIRTY_POD)
    {
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_BATT)
    {
        arex_widget_set_value(WIDGET_BATTERY_0806, g_sensor_data.battery_pct);
    }

    if (mask & DIRTY_HEADING)
    {
        card_compass_refresh_heading(false);
    }

    if (mask & DIRTY_DIVE_TIME)
    {
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_PPO2)
    {
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_GAS)
    {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_TRAJECTORY)
    {
        arex_ui_update_router_refresh_plan();
    }

    if (mask & DIRTY_CNS)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_OTU)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    if (mask & DIRTY_TEMP)
    {
        arex_widget_set_value(WIDGET_TEMP_0806, g_sensor_data.temperature_c);
        arex_refresh_left_aux_slots();
    }

    if (mask & DIRTY_DEPTH)
    {
        arex_widget_set_value(WIDGET_DEPTH_MAX_0806, g_sensor_data.max_depth);
        arex_widget_set_value(WIDGET_DEPTH_AVG_0806, g_sensor_data.avg_depth);
    }

    if (mask & DIRTY_TEMP)
    {
        arex_widget_set_value(WIDGET_TEMP_MIN_0806, g_sensor_data.min_temp);
        arex_widget_set_value(WIDGET_TEMP_AVG_0806, g_sensor_data.avg_temp);
    }

    if (mask & (DIRTY_GF_SETTING | DIRTY_MOD | DIRTY_CEILING |
                DIRTY_GAS_MIX | DIRTY_GAS_DENS | DIRTY_FIO2))
    {
        arex_screen_refresh_all_widgets();
    }

    if (mask & (DIRTY_BATT | DIRTY_TEMP))
    {
        arex_widget_refresh_sys(mask);
    }

    if (mask & DIRTY_ALARM)
    {
        arex_ui_update_router_alarm_tick();
    }
}
