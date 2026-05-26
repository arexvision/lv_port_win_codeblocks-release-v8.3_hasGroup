#include "update_router.h"

#include "../alarm/alarm_view.h"
#include "../screen/card_registry.h"
#include "data.h"
#include "../screen/layout_view.h"
#include "../screen/screen.h"
#include "ui_engine.h"
#include "ui_state.h"
#include "../comp/comp_update.h"
#include "../comp/comp_view.h"
#include "../cards/card_compass.h"

#include <math.h>

static uint32_t s_deco_last_refresh_ms = 0;

static void ui_update_router_alarm_tick(void)
{
    alarm_view_context_t ctx;
    ctx.safe_zone = get_safe_zone();
    ctx.left_anchor = g_left_anchor_obj;
    ctx.custom_cards = g_card_custom_objs;
    ctx.custom_card_count = g_card_custom_obj_count;
    ctx.max_custom_cards = MAX_CUSTOM_CARDS;
    ctx.layout_order = g_sys_config.layout_order;
    ctx.safe_zone_w = g_sys_config.safe_zone_w;
    ctx.left_anchor_w = LEFT_ANCHOR_W;
    ctx.panel_gap_px = (uint16_t)(g_sys_config.panel_gap_u * BASE_U);
    ctx.alarm_pending_click = &g_ui.alarm_pending_click;
    alarm_view_tick(&ctx);
}

static void ui_update_router_setup_tick(void)
{
    static compass_cal_ui_state_t s_last_compass_cal_state = COMPASS_CAL_IDLE;
    compass_cal_ui_state_t cal_state = get_compass_calibration_ui_state();

    if (cal_state != s_last_compass_cal_state)
    {
        s_last_compass_cal_state = cal_state;
        screen_refresh_setup_menu();
    }
}

static void ui_update_router_ascent_heartbeat(void)
{
    static bool s_last_flash_state = false;
    bool current_flash_state = (lv_tick_get() / 500U) % 2U == 0U;

    if (current_flash_state == s_last_flash_state)
    {
        return;
    }

    s_last_flash_state = current_flash_state;

    if (fabsf(g_sensor_data.ascent_rate) >= RATE_STILL_THRESHOLD)
    {
        g_sensor_data.dirty_mask |= DIRTY_ASCENT;
    }
}

static void ui_update_router_refresh_plan(void)
{
    uint32_t now = lv_tick_get();

#if DECO_REFRESH_MS > 0
    if (now - s_deco_last_refresh_ms >= DECO_REFRESH_MS)
    {
        s_deco_last_refresh_ms = now;
        card_plan_update();
    }
#else
    (void)s_deco_last_refresh_ms;
    card_plan_update();
#endif
}

void ui_update_router_tick(void)
{
    ui_update_router_alarm_tick();
    ui_update_router_setup_tick();
    ui_update_router_ascent_heartbeat();
}

void ui_update_router_dispatch(uint32_t mask)
{
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
        screen_refresh_all_widgets();
        if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
        {
            card_deco_update();
        }
        comp_refresh_ascent_icons(g_sensor_data.ascent_rate);
    }

    comp_refresh_ndl_stop(mask);

    if (mask & DIRTY_POD)
    {
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_BATT)
    {
        comp_set_value(COMP_BATTERY_0806, g_sensor_data.battery_pct);
    }

    if (mask & DIRTY_HEADING)
    {
        card_compass_refresh_heading(false);
    }

    if (mask & DIRTY_DIVE_TIME)
    {
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_PPO2)
    {
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_GAS)
    {
        screen_refresh_gas_menu();
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_TRAJECTORY)
    {
        ui_update_router_refresh_plan();
    }

    if (mask & DIRTY_CNS)
    {
        card_deco_update();
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_OTU)
    {
        card_deco_update();
        screen_refresh_all_widgets();
    }

    if (mask & DIRTY_TEMP)
    {
        comp_set_value(COMP_TEMP_0806, g_sensor_data.temperature_c);
        refresh_left_aux_slots();
    }

    if (mask & DIRTY_DEPTH)
    {
        comp_set_value(COMP_DEPTH_MAX_0806, g_sensor_data.max_depth);
        comp_set_value(COMP_DEPTH_AVG_0806, g_sensor_data.avg_depth);
    }

    if (mask & DIRTY_TEMP)
    {
        comp_set_value(COMP_TEMP_MIN_0806, g_sensor_data.min_temp);
        comp_set_value(COMP_TEMP_AVG_0806, g_sensor_data.avg_temp);
    }

    if (mask & (DIRTY_GF_SETTING | DIRTY_MOD | DIRTY_CEILING |
                DIRTY_GAS_MIX | DIRTY_GAS_DENS | DIRTY_FIO2))
    {
        screen_refresh_all_widgets();
    }

    if (mask & (DIRTY_BATT | DIRTY_TEMP))
    {
        comp_refresh_sys(mask);
    }

    if (mask & DIRTY_ALARM)
    {
        ui_update_router_alarm_tick();
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
}
