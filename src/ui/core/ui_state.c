#include "ui_state.h"
#include "ui_engine.h"
#include "../screen/page_registry.h"
#include "../screen/screen.h"

#include <string.h>

/* =========================================
   Global UI context
   ========================================= */
ui_ctx_t g_ui;

/* =========================================
   气体切换命令队列（单向数据流：UI → Algorithm）
   ========================================= */
static gas_switch_cmd_t g_gas_switch_cmd = {false, 0};
static compass_cal_cmd_t g_compass_cal_cmd = {false, COMPASS_CAL_CMD_NONE};
static compass_cal_ui_state_t g_compass_cal_ui_state = COMPASS_CAL_IDLE;

/* =========================================
   Init
   ========================================= */
void ui_state_init(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.state         = UI_DASH;
    g_ui.dash_page     = PAGE_POS_DYNAMIC_FIRST;
    g_ui.menu_info_idx = 0;
    g_ui.wall_charge   = 0;
}

/* =========================================
   Internal: notify registered pages
   ========================================= */
void ui_refresh_all(void)
{
    for (uint8_t i = 0; i < page_count(); i++)
    {
        page_t *c = page_get(i);
        if (c && c->update_cb) c->update_cb();
    }
}

/* =========================================
   Internal: tileview navigation
   ========================================= */
void ui_go_to_page(uint8_t tile_pos)
{
    g_ui.dash_page = tile_pos;
    screen_scroll_to_page(tile_pos);
}

/* =========================================
   Rotate handler (+1 = down, -1 = up)
   ========================================= */
void ui_handle_rotate(int8_t dir)
{
    switch (g_ui.state)
    {

    /* --- DASH: scroll between pages with wall-charge at edges --- */
    case UI_DASH:
    {
        uint8_t dash_min = PAGE_POS_DYNAMIC_FIRST;
        uint8_t dash_max = page_setup_display_pos() - 1;

#if ENABLE_INFO_MENU
        if (g_ui.dash_page == dash_min && dir == -1)
        {
            g_ui.wall_charge++;
            screen_show_wall(WALL_TOP, g_ui.wall_charge,
                                  ">>> ENTER INFO MENU >>>");
            if (g_ui.wall_charge >= 3)
            {
                g_ui.wall_charge = 0;
                screen_hide_walls_snap();
                g_ui.state = UI_INFO;
                g_ui.menu_info_idx = 0;
                screen_set_info_selection(0);
                ui_go_to_page(0);
            }
        }
        else
#endif
            if (g_ui.dash_page == dash_max && dir == 1)
            {
                g_ui.wall_charge++;
                screen_show_wall(WALL_BOTTOM, g_ui.wall_charge,
                                      "<<< ENTER DIVE MENU <<<");
                if (g_ui.wall_charge >= 3)
                {
                    g_ui.wall_charge = 0;
                    screen_hide_walls_snap();
                    g_ui.state = UI_SETUP;
                    g_ui.menu_setup_idx = 0;
                    screen_set_setup_selection(0);
                    ui_go_to_page(page_setup_display_pos());
                }
            }
            else
            {
                g_ui.wall_charge = 0;
                screen_hide_walls();
                int8_t next = (int8_t)g_ui.dash_page + dir;
                if (next < (int8_t)dash_min) next = (int8_t)dash_min;
                if (next > (int8_t)dash_max) next = (int8_t)dash_max;
                ui_go_to_page((uint8_t)next);
            }
        break;
    }

    /* --- EDIT_GAS --- */
    case UI_EDIT_GAS:
    {
        uint8_t gas_count = g_sensor_data.gas_slot_count;
        if (gas_count > GAS_COUNT)
        {
            gas_count = GAS_COUNT;
        }
        if (gas_count == 0U)
        {
            break;
        }
        int8_t next = ((int8_t)g_ui.gas_cursor + dir + gas_count) % gas_count;
        g_ui.gas_cursor = (uint8_t)next;
        screen_refresh_gas_menu();
        break;
    }

    /* --- INFO menu --- */
    case UI_INFO:
    {
        uint8_t len = screen_info_item_count();
        if (dir == 1 && g_ui.menu_info_idx == len - 1)
        {
            g_ui.wall_charge++;
            screen_show_wall(WALL_BOTTOM, g_ui.wall_charge,
                                  "<<< RETURN TO DASH <<<");
            if (g_ui.wall_charge >= 3)
            {
                g_ui.wall_charge = 0;
                screen_hide_walls_snap();
                g_ui.state = UI_DASH;
                g_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
                ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
            }
        }
        else
        {
            g_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)g_ui.menu_info_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            g_ui.menu_info_idx = (uint8_t)next;
            screen_set_info_selection(g_ui.menu_info_idx);
        }
        break;
    }

    /* --- SETUP menu --- */
    case UI_SETUP:
    {
        uint8_t len = screen_setup_item_count();
        if (dir == -1 && g_ui.menu_setup_idx == 0)
        {
            g_ui.wall_charge++;
            screen_show_wall(WALL_TOP, g_ui.wall_charge,
                                  ">>> RETURN TO DASH >>>");
            if (g_ui.wall_charge >= 3)
            {
                g_ui.wall_charge = 0;
                screen_hide_walls_snap();
                g_ui.state = UI_DASH;
                g_ui.dash_page = page_setup_display_pos() - 1;
                ui_go_to_page(page_setup_display_pos() - 1);
            }
        }
        else
        {
            g_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)g_ui.menu_setup_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            g_ui.menu_setup_idx = (uint8_t)next;
            screen_set_setup_selection(g_ui.menu_setup_idx);
        }
        break;
    }

    /* --- SUB_MENU --- */
    case UI_SUB_MENU:
    {
        if (screen_handle_dive_plan_rotate(dir))
        {
            break;
        }
        int8_t next = (int8_t)g_ui.sub_menu_idx + dir;
        if (next < 0) next = 0;
        if (next >= (int8_t)g_ui.sub_item_count) next = g_ui.sub_item_count - 1;
        g_ui.sub_menu_idx = (uint8_t)next;
        screen_set_submenu_selection(g_ui.sub_menu_idx);
        break;
    }

    /* --- EDIT_VALUE --- */
    case UI_EDIT_VALUE:
    {
        if (!g_ui.edit_ctx.active) break;
        float next = g_ui.edit_ctx.value + dir * g_ui.edit_ctx.step;
        if (next < g_ui.edit_ctx.min) next = g_ui.edit_ctx.min;
        if (next > g_ui.edit_ctx.max) next = g_ui.edit_ctx.max;
        int steps = (int)((next - g_ui.edit_ctx.min) / g_ui.edit_ctx.step + 0.5f);
        g_ui.edit_ctx.value = g_ui.edit_ctx.min + steps * g_ui.edit_ctx.step;
        screen_refresh_edit_value();
        break;
    }

    default:
        break;
    }
}

/* =========================================
   Click handler
   ========================================= */
void ui_handle_click(void)
{
    /* 告警锁：触发后必须先 click/rotate 一次才可清除 */
    if (g_ui.alarm_pending_click)
    {
        extern bool alarm_mark_clear_requested(void);
        g_ui.alarm_pending_click = false;
        alarm_mark_clear_requested();
    }

    switch (g_ui.state)
    {

    case UI_DASH:
    {
        /* page_id 从 card_order[] 映射 */
        uint8_t page_id = page_id_at(g_ui.dash_page);

        if (page_id == PAGE_ID_COMPASS)
        {
            if (!g_sensor_data.heading_locked)
            {
                g_sensor_data.heading_locked = true;
                g_sensor_data.heading_target = g_sensor_data.heading;
                screen_refresh_compass_target();
            }
            else
            {
                g_ui.state = UI_MODAL_COMPASS;
                screen_show_modal_compass();
            }
        }
        else if (page_id == PAGE_ID_GAS)
        {
            g_ui.state = UI_EDIT_GAS;
            g_ui.gas_cursor = g_sensor_data.gas_active_idx;
            screen_refresh_gas_menu();
        }
        break;
    }

    case UI_EDIT_GAS:
        g_ui.state = UI_MODAL_GAS;
        g_ui.gas_modal_from_submenu = false;  // HOTFIX: Route GAS modal exit based on context.
        screen_show_modal_gas();
        break;

    case UI_MODAL_GAS:
    {
        uint8_t ci = g_ui.gas_cursor;
        uint8_t gas_count = g_sensor_data.gas_slot_count;
        if (gas_count > GAS_COUNT)
        {
            gas_count = GAS_COUNT;
        }
        if (gas_count == 0U)
        {
            screen_pulse_modal();
            break;
        }
        if (ci >= gas_count)
        {
            ci = 0;
        }
        float mod_m = g_sensor_data.gas_slot_mod_m[ci] > 0.0f
                      ? g_sensor_data.gas_slot_mod_m[ci]
                      : (float)GAS_MOD_M[ci];
        if (g_sensor_data.depth <= mod_m)
        {
            /* 修复：不直接修改数据源，发送命令到队列 */
            request_gas_switch(ci);
            screen_hide_modal();
            /* 注意：gas_name 和 gas_active_idx 由 buhlmann_task 更新 */
            screen_refresh_gas_menu();
            screen_refresh_left_panel();
            // HOTFIX: Route GAS modal exit based on context.
            if (g_ui.gas_modal_from_submenu)
            {
                g_ui.gas_modal_from_submenu = false;
                screen_close_submenu();
            }
            else
            {
                g_ui.state = UI_DASH;
            }
        }
        else
        {
            screen_pulse_modal();
        }
        break;
    }

    case UI_MODAL_COMPASS:
        g_sensor_data.heading_locked = false;
        screen_refresh_compass_target();
        screen_hide_modal();
        g_ui.state = UI_DASH;
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_confirm_submenu_setting();
        break;

    case UI_EDIT_VALUE:
        g_ui.edit_ctx.active = false;
        g_ui.state = UI_SUB_MENU;
        screen_commit_edit_value();
        break;

    case UI_INFO:
#if ENABLE_INFO_MENU
        screen_open_info_submenu(g_ui.menu_info_idx);
#else
        g_ui.state = UI_DASH;
        g_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#endif
        break;

    case UI_SETUP:
        screen_open_setup_submenu(g_ui.menu_setup_idx);
        break;

    case UI_SUB_MENU:
        screen_handle_submenu_select(g_ui.sub_menu_idx);
        break;

    default:
        break;
    }
}

/* =========================================
   Back / ESC handler
   ========================================= */
void ui_handle_back(void)
{
    switch (g_ui.state)
    {
    case UI_EDIT_GAS:
        g_ui.state = UI_DASH;
        screen_refresh_gas_menu();
        break;

    case UI_MODAL_GAS:
        screen_hide_modal();
        // HOTFIX: Route GAS modal exit based on context.
        if (g_ui.gas_modal_from_submenu)
        {
            g_ui.gas_modal_from_submenu = false;
            screen_close_submenu();
        }
        else
        {
            g_ui.state = UI_EDIT_GAS;
        }
        break;

    case UI_MODAL_COMPASS:
    case UI_MODAL_ACT:
        screen_hide_modal();
        if (g_ui.sub_item_count > 0)
        {
            g_ui.state = UI_SUB_MENU;
        }
        else
        {
            g_ui.state = UI_DASH;
        }
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_cancel_submenu_setting();
        break;

    case UI_EDIT_VALUE:
        g_ui.edit_ctx.value = g_ui.edit_ctx.original;
        g_ui.edit_ctx.active = false;
        g_ui.state = UI_SUB_MENU;
        screen_cancel_edit_value();
        break;

    case UI_SUB_MENU:
        screen_close_submenu();
        break;

    case UI_INFO:
#if ENABLE_INFO_MENU
        g_ui.state = UI_DASH;
        g_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#else
        g_ui.state = UI_DASH;
        g_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#endif
        break;

    case UI_SETUP:
        g_ui.state = UI_DASH;
        g_ui.dash_page = page_setup_display_pos() - 1;
        ui_go_to_page(page_setup_display_pos() - 1);
        break;

    default:
        break;
    }
}

/* =========================================
   气体切换命令队列接口实现
   ========================================= */
void request_gas_switch(uint8_t gas_idx)
{
    g_gas_switch_cmd.pending = true;
    g_gas_switch_cmd.gas_idx = gas_idx;
}

bool has_pending_gas_switch(uint8_t *out_gas_idx)
{
    if (g_gas_switch_cmd.pending && out_gas_idx != NULL)
    {
        *out_gas_idx = g_gas_switch_cmd.gas_idx;
        return true;
    }
    return false;
}

void clear_gas_switch_cmd(void)
{
    g_gas_switch_cmd.pending = false;
}

void request_compass_calibration_start(void)
{
    g_compass_cal_cmd.pending = true;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_START;
}

void request_compass_calibration_reset(void)
{
    g_compass_cal_cmd.pending = true;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_RESET;
}

bool has_pending_compass_calibration(compass_cal_cmd_action_t *out_action)
{
    if (g_compass_cal_cmd.pending && out_action != NULL)
    {
        *out_action = g_compass_cal_cmd.action;
        return true;
    }
    return false;
}

void clear_compass_calibration_cmd(void)
{
    g_compass_cal_cmd.pending = false;
    g_compass_cal_cmd.action = COMPASS_CAL_CMD_NONE;
}

void set_compass_calibration_ui_state(compass_cal_ui_state_t state)
{
    g_compass_cal_ui_state = state;
}

compass_cal_ui_state_t get_compass_calibration_ui_state(void)
{
    return g_compass_cal_ui_state;
}
