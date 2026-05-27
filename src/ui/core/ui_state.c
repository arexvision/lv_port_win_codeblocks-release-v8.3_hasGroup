#include "ui_state.h"
#include "data.h"
#include "ui_engine.h"
#include "../screen/page_registry.h"
#include "../screen/screen.h"

#include <string.h>

/* =========================================
   Global UI context
   ========================================= */
static ui_ctx_t s_ui;

/* =========================================
   气体切换命令队列（单向数据流：UI → Algorithm）
   ========================================= */
static gas_switch_cmd_t g_gas_switch_cmd = {false, 0};
static compass_cal_cmd_t g_compass_cal_cmd = {false, COMPASS_CAL_CMD_NONE};
static compass_cal_ui_state_t g_compass_cal_ui_state = COMPASS_CAL_IDLE;
static bool g_heading_lock_pending = false;
static bool g_heading_lock_active = false;

/* =========================================
   Init
   ========================================= */
void ui_state_init(void)
{
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.state         = UI_DASH;
    s_ui.dash_page     = PAGE_POS_DYNAMIC_FIRST;
    s_ui.menu_info_idx = 0;
    s_ui.wall_charge   = 0;
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
    s_ui.dash_page = tile_pos;
    screen_scroll_to_page(tile_pos);
}

/* =========================================
   Rotate handler (+1 = down, -1 = up)
   ========================================= */
void ui_handle_rotate(int8_t dir)
{
    switch (s_ui.state)
    {

    /* --- DASH: scroll between pages with wall-charge at edges --- */
    case UI_DASH:
    {
        uint8_t dash_min = PAGE_POS_DYNAMIC_FIRST;
        uint8_t dash_max = page_setup_display_pos() - 1;

#if ENABLE_INFO_MENU
        if (s_ui.dash_page == dash_min && dir == -1)
        {
            s_ui.wall_charge++;
            screen_show_wall(WALL_TOP, s_ui.wall_charge,
                                  ">>> ENTER INFO MENU >>>");
            if (s_ui.wall_charge >= 3)
            {
                s_ui.wall_charge = 0;
                screen_hide_walls_snap();
                s_ui.state = UI_INFO;
                s_ui.menu_info_idx = 0;
                screen_set_info_selection(0);
                ui_go_to_page(0);
            }
        }
        else
#endif
            if (s_ui.dash_page == dash_max && dir == 1)
            {
                s_ui.wall_charge++;
                screen_show_wall(WALL_BOTTOM, s_ui.wall_charge,
                                      "<<< ENTER DIVE MENU <<<");
                if (s_ui.wall_charge >= 3)
                {
                    s_ui.wall_charge = 0;
                    screen_hide_walls_snap();
                    s_ui.state = UI_SETUP;
                    s_ui.menu_setup_idx = 0;
                    screen_set_setup_selection(0);
                    ui_go_to_page(page_setup_display_pos());
                }
            }
            else
            {
                s_ui.wall_charge = 0;
                screen_hide_walls();
                int8_t next = (int8_t)s_ui.dash_page + dir;
                if (next < (int8_t)dash_min) next = (int8_t)dash_min;
                if (next > (int8_t)dash_max) next = (int8_t)dash_max;
                ui_go_to_page((uint8_t)next);
            }
        break;
    }

    /* --- EDIT_GAS --- */
    case UI_EDIT_GAS:
    {
        uint8_t gas_count = bus_get_gas_slot_count();
        if (gas_count == 0U)
        {
            break;
        }
        int8_t next = ((int8_t)s_ui.gas_cursor + dir + gas_count) % gas_count;
        s_ui.gas_cursor = (uint8_t)next;
        screen_refresh_gas_menu();
        break;
    }

    /* --- INFO menu --- */
    case UI_INFO:
    {
        uint8_t len = screen_info_item_count();
        if (dir == 1 && s_ui.menu_info_idx == len - 1)
        {
            s_ui.wall_charge++;
            screen_show_wall(WALL_BOTTOM, s_ui.wall_charge,
                                  "<<< RETURN TO DASH <<<");
            if (s_ui.wall_charge >= 3)
            {
                s_ui.wall_charge = 0;
                screen_hide_walls_snap();
                s_ui.state = UI_DASH;
                s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
                ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
            }
        }
        else
        {
            s_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)s_ui.menu_info_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            s_ui.menu_info_idx = (uint8_t)next;
            screen_set_info_selection(s_ui.menu_info_idx);
        }
        break;
    }

    /* --- SETUP menu --- */
    case UI_SETUP:
    {
        uint8_t len = screen_setup_item_count();
        if (dir == -1 && s_ui.menu_setup_idx == 0)
        {
            s_ui.wall_charge++;
            screen_show_wall(WALL_TOP, s_ui.wall_charge,
                                  ">>> RETURN TO DASH >>>");
            if (s_ui.wall_charge >= 3)
            {
                s_ui.wall_charge = 0;
                screen_hide_walls_snap();
                s_ui.state = UI_DASH;
                s_ui.dash_page = page_setup_display_pos() - 1;
                ui_go_to_page(page_setup_display_pos() - 1);
            }
        }
        else
        {
            s_ui.wall_charge = 0;
            screen_hide_walls();
            int8_t next = (int8_t)s_ui.menu_setup_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)len) next = len - 1;
            s_ui.menu_setup_idx = (uint8_t)next;
            screen_set_setup_selection(s_ui.menu_setup_idx);
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
        int8_t next = (int8_t)s_ui.sub_menu_idx + dir;
        if (next < 0) next = 0;
        if (next >= (int8_t)s_ui.sub_item_count) next = s_ui.sub_item_count - 1;
        s_ui.sub_menu_idx = (uint8_t)next;
        screen_set_submenu_selection(s_ui.sub_menu_idx);
        break;
    }

    /* --- EDIT_VALUE --- */
    case UI_EDIT_VALUE:
    {
        if (!s_ui.edit_ctx.active) break;
        float next = s_ui.edit_ctx.value + dir * s_ui.edit_ctx.step;
        if (next < s_ui.edit_ctx.min) next = s_ui.edit_ctx.min;
        if (next > s_ui.edit_ctx.max) next = s_ui.edit_ctx.max;
        int steps = (int)((next - s_ui.edit_ctx.min) / s_ui.edit_ctx.step + 0.5f);
        s_ui.edit_ctx.value = s_ui.edit_ctx.min + steps * s_ui.edit_ctx.step;
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
    if (s_ui.alarm_pending_click)
    {
        extern bool alarm_mark_clear_requested(void);
        s_ui.alarm_pending_click = false;
        alarm_mark_clear_requested();
    }

    switch (s_ui.state)
    {

    case UI_DASH:
    {
        /* page_id 从 card_order[] 映射 */
        uint8_t page_id = page_id_at(s_ui.dash_page);

        if (page_id == PAGE_ID_COMPASS)
        {
            if (!bus_is_heading_locked())
            {
                g_heading_lock_pending = true;
                bus_lock_heading_to_current();
                g_heading_lock_active = true;
                screen_refresh_compass_target();
            }
            else
            {
                s_ui.state = UI_MODAL_COMPASS;
                screen_show_modal_compass();
            }
        }
        else if (page_id == PAGE_ID_GAS)
        {
            s_ui.state = UI_EDIT_GAS;
            s_ui.gas_cursor = bus_get_gas_active_idx();
            screen_refresh_gas_menu();
        }
        break;
    }

    case UI_EDIT_GAS:
        s_ui.state = UI_MODAL_GAS;
        s_ui.gas_modal_from_submenu = false;  // HOTFIX: Route GAS modal exit based on context.
        screen_show_modal_gas();
        break;

    case UI_MODAL_GAS:
    {
        uint8_t ci = s_ui.gas_cursor;
        uint8_t gas_count = bus_get_gas_slot_count();
        if (gas_count == 0U)
        {
            screen_pulse_modal();
            break;
        }
        if (ci >= gas_count)
        {
            ci = 0;
        }
        float mod_m = bus_get_gas_slot_mod_m(ci);
        if (mod_m <= 0.0f)
        {
            mod_m = (float)GAS_MOD_M[ci];
        }
        if (bus_get_depth() <= mod_m)
        {
            /* 修复：不直接修改数据源，发送命令到队列 */
            request_gas_switch(ci);
            screen_hide_modal();
            /* 注意：gas_name 和 gas_active_idx 由 buhlmann_task 更新 */
            screen_refresh_gas_menu();
            screen_refresh_left_panel();
            // HOTFIX: Route GAS modal exit based on context.
            if (s_ui.gas_modal_from_submenu)
            {
                s_ui.gas_modal_from_submenu = false;
                screen_close_submenu();
            }
            else
            {
                s_ui.state = UI_DASH;
            }
        }
        else
        {
            screen_pulse_modal();
        }
        break;
    }

    case UI_MODAL_COMPASS:
        bus_clear_heading_lock();
        g_heading_lock_active = false;
        screen_refresh_compass_target();
        screen_hide_modal();
        s_ui.state = UI_DASH;
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_confirm_submenu_setting();
        break;

    case UI_EDIT_VALUE:
        s_ui.edit_ctx.active = false;
        s_ui.state = UI_SUB_MENU;
        screen_commit_edit_value();
        break;

    case UI_INFO:
#if ENABLE_INFO_MENU
        screen_open_info_submenu(s_ui.menu_info_idx);
#else
        s_ui.state = UI_DASH;
        s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#endif
        break;

    case UI_SETUP:
        screen_open_setup_submenu(s_ui.menu_setup_idx);
        break;

    case UI_SUB_MENU:
        screen_handle_submenu_select(s_ui.sub_menu_idx);
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
    switch (s_ui.state)
    {
    case UI_EDIT_GAS:
        s_ui.state = UI_DASH;
        screen_refresh_gas_menu();
        break;

    case UI_MODAL_GAS:
        screen_hide_modal();
        // HOTFIX: Route GAS modal exit based on context.
        if (s_ui.gas_modal_from_submenu)
        {
            s_ui.gas_modal_from_submenu = false;
            screen_close_submenu();
        }
        else
        {
            s_ui.state = UI_EDIT_GAS;
        }
        break;

    case UI_MODAL_COMPASS:
    case UI_MODAL_ACT:
        screen_hide_modal();
        if (s_ui.sub_item_count > 0)
        {
            s_ui.state = UI_SUB_MENU;
        }
        else
        {
            s_ui.state = UI_DASH;
        }
        break;

    case UI_MODAL_SETUP_CONFIRM:
        screen_cancel_submenu_setting();
        break;

    case UI_EDIT_VALUE:
        s_ui.edit_ctx.value = s_ui.edit_ctx.original;
        s_ui.edit_ctx.active = false;
        s_ui.state = UI_SUB_MENU;
        screen_cancel_edit_value();
        break;

    case UI_SUB_MENU:
        screen_close_submenu();
        break;

    case UI_INFO:
#if ENABLE_INFO_MENU
        s_ui.state = UI_DASH;
        s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#else
        s_ui.state = UI_DASH;
        s_ui.dash_page = PAGE_POS_DYNAMIC_FIRST;
        ui_go_to_page(PAGE_POS_DYNAMIC_FIRST);
#endif
        break;

    case UI_SETUP:
        s_ui.state = UI_DASH;
        s_ui.dash_page = page_setup_display_pos() - 1;
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

ui_state_t ui_state_get_state(void)
{
    return s_ui.state;
}

void ui_state_set_state(ui_state_t state)
{
    s_ui.state = state;
}

uint8_t ui_state_get_dash_page(void)
{
    return s_ui.dash_page;
}

void ui_state_set_dash_page(uint8_t page)
{
    s_ui.dash_page = page;
}

uint8_t ui_state_get_menu_info_idx(void)
{
    return s_ui.menu_info_idx;
}

void ui_state_set_menu_info_idx(uint8_t idx)
{
    s_ui.menu_info_idx = idx;
}

uint8_t ui_state_get_menu_setup_idx(void)
{
    return s_ui.menu_setup_idx;
}

void ui_state_set_menu_setup_idx(uint8_t idx)
{
    s_ui.menu_setup_idx = idx;
}

uint8_t ui_state_get_gas_cursor(void)
{
    return s_ui.gas_cursor;
}

void ui_state_set_gas_cursor(uint8_t cursor)
{
    s_ui.gas_cursor = cursor;
}

uint8_t ui_state_get_sub_menu_idx(void)
{
    return s_ui.sub_menu_idx;
}

void ui_state_set_sub_menu_idx(uint8_t idx)
{
    s_ui.sub_menu_idx = idx;
}

uint8_t ui_state_get_sub_item_count(void)
{
    return s_ui.sub_item_count;
}

void ui_state_set_sub_item_count(uint8_t count)
{
    s_ui.sub_item_count = count;
}

ui_state_t ui_state_get_sub_parent(void)
{
    return s_ui.sub_parent;
}

void ui_state_set_sub_parent(ui_state_t state)
{
    s_ui.sub_parent = state;
}

uint8_t ui_state_get_sub_history_depth(void)
{
    return s_ui.sub_history_depth;
}

void ui_state_set_sub_history_depth(uint8_t depth)
{
    s_ui.sub_history_depth = depth;
}

bool ui_state_get_gas_modal_from_submenu(void)
{
    return s_ui.gas_modal_from_submenu;
}

void ui_state_set_gas_modal_from_submenu(bool enabled)
{
    s_ui.gas_modal_from_submenu = enabled;
}

bool ui_state_get_alarm_pending_click(void)
{
    return s_ui.alarm_pending_click;
}

void ui_state_set_alarm_pending_click(bool pending)
{
    s_ui.alarm_pending_click = pending;
}

bool ui_state_get_edit_active(void)
{
    return s_ui.edit_ctx.active;
}

void ui_state_set_edit_active(bool active)
{
    s_ui.edit_ctx.active = active;
}

uint8_t ui_state_get_edit_item_index(void)
{
    return s_ui.edit_ctx.item_index;
}

void ui_state_set_edit_item_index(uint8_t index)
{
    s_ui.edit_ctx.item_index = index;
}

bool ui_state_get_edit_value_active(void)
{
    return s_ui.edit_ctx.active;
}

float ui_state_get_edit_value(void)
{
    return s_ui.edit_ctx.value;
}

void ui_state_set_edit_value(float value)
{
    s_ui.edit_ctx.value = value;
}

float ui_state_get_edit_original(void)
{
    return s_ui.edit_ctx.original;
}

void ui_state_set_edit_original(float value)
{
    s_ui.edit_ctx.original = value;
}

float ui_state_get_edit_min(void)
{
    return s_ui.edit_ctx.min;
}

void ui_state_set_edit_min(float value)
{
    s_ui.edit_ctx.min = value;
}

float ui_state_get_edit_max(void)
{
    return s_ui.edit_ctx.max;
}

void ui_state_set_edit_max(float value)
{
    s_ui.edit_ctx.max = value;
}

float ui_state_get_edit_step(void)
{
    return s_ui.edit_ctx.step;
}

void ui_state_set_edit_step(float value)
{
    s_ui.edit_ctx.step = value;
}

submenu_setting_kind_t ui_state_get_edit_setting_kind(void)
{
    return s_ui.edit_ctx.setting_kind;
}

void ui_state_set_edit_setting_kind(submenu_setting_kind_t kind)
{
    s_ui.edit_ctx.setting_kind = kind;
}

uint8_t ui_state_get_edit_setting_arg(void)
{
    return s_ui.edit_ctx.setting_arg;
}

void ui_state_set_edit_setting_arg(uint8_t arg)
{
    s_ui.edit_ctx.setting_arg = arg;
}

uint8_t ui_state_get_edit_decimals(void)
{
    return s_ui.edit_ctx.decimals;
}

void ui_state_set_edit_decimals(uint8_t decimals)
{
    s_ui.edit_ctx.decimals = decimals;
}

const char *ui_state_get_edit_label(void)
{
    return s_ui.edit_ctx.label;
}

void ui_state_set_edit_label(const char *label)
{
    if (label == NULL)
    {
        s_ui.edit_ctx.label[0] = '\0';
    }
    else
    {
        lv_snprintf(s_ui.edit_ctx.label, sizeof(s_ui.edit_ctx.label), "%s", label);
    }
}

bool ui_state_get_heading_lock_pending(void)
{
    return g_heading_lock_pending;
}

void ui_state_set_heading_lock_pending(bool pending)
{
    g_heading_lock_pending = pending;
}

bool ui_state_get_heading_lock_active(void)
{
    return g_heading_lock_active;
}

void ui_state_set_heading_lock_active(bool active)
{
    g_heading_lock_active = active;
}
