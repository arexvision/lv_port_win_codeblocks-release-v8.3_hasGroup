#include "arex_ui_state.h"
#include "arex_data.h"
#include "arex_card_registry.h"
#include "arex_screen.h"

#include <string.h>

/* =========================================
   Global UI context
   ========================================= */
arex_ui_ctx_t g_ui;

/* =========================================
   Init
   ========================================= */
void arex_ui_state_init(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.state         = UI_INFO;  /* 启动直接进 INFO 菜单 */
    g_ui.dash_card     = 1;        /* DASH 默认卡（COMPASS），wall-charge 退出 INFO 后跳这里 */
    g_ui.menu_info_idx = 0;
    g_ui.wall_charge   = 0;
}

/* =========================================
   Internal: notify registered cards
   ========================================= */
void arex_ui_refresh_all(void)
{
    for (uint8_t i = 0; i < arex_card_count(); i++) {
        arex_card_reg_t *c = arex_card_get(i);
        if (c && c->update_cb) c->update_cb();
    }
}

/* =========================================
   Internal: tileview navigation
   ========================================= */
void arex_ui_go_to_card(uint8_t idx)
{
    g_ui.dash_card = idx;
    arex_screen_scroll_to_card(idx);
}

/* =========================================
   Rotate handler  (+1 = down,  -1 = up)
   ========================================= */
void ui_handle_rotate(int8_t dir)
{
    uint8_t total_cards = arex_card_count();

    switch (g_ui.state) {

        /* --- DASH: scroll between cards with wall-charge at edges ---
           card_order layout:  [0]=INFO  [1]=COMPASS  [2]=DECO  [3]=GAS  [4]=PLAN  [5]=SETUP
           DASH 可滚动范围: index 1~(total_cards-2)
           index 0 (INFO) 和 index (total_cards-1) (SETUP) 只能通过 wall-charge 进入
        --- */
        case UI_DASH: {
            uint8_t dash_min = 1;
            uint8_t dash_max = total_cards - 2;
            bool at_top    = (g_ui.dash_card == dash_min);
            bool at_bottom = (g_ui.dash_card == dash_max);

            if (at_top && dir == -1) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_TOP, g_ui.wall_charge, ">>> ENTER INFO MENU >>>");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls();
                    g_ui.state = UI_INFO;
                    g_ui.menu_info_idx = 0;
                    arex_screen_set_info_selection(0);
                    arex_ui_go_to_card(0);   /* INFO card is always index 0 in order */
                }
            } else if (at_bottom && dir == 1) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_BOTTOM, g_ui.wall_charge, "<<< ENTER DIVE SETUP <<<");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls();
                    g_ui.state = UI_SETUP;
                    g_ui.menu_setup_idx = 0;
                    arex_screen_set_setup_selection(0);
                    arex_ui_go_to_card(total_cards - 1);
                }
            } else {
                g_ui.wall_charge = 0;
                arex_screen_hide_walls();
                int8_t next = (int8_t)g_ui.dash_card + dir;
                if (next < (int8_t)dash_min) next = (int8_t)dash_min;
                if (next > (int8_t)dash_max) next = (int8_t)dash_max;
                arex_ui_go_to_card((uint8_t)next);
            }
            break;
        }

        /* --- EDIT_GAS: move cursor through gas list --- */
        case UI_EDIT_GAS: {
            int8_t next = ((int8_t)g_ui.gas_cursor + dir + AREX_GAS_COUNT) % AREX_GAS_COUNT;
            g_ui.gas_cursor = (uint8_t)next;
            arex_screen_refresh_gas_menu();
            break;
        }

        /* --- INFO menu: scroll list with bottom-wall exit --- */
        case UI_INFO: {
            uint8_t len = arex_screen_info_item_count();
            if (dir == 1 && g_ui.menu_info_idx == len - 1) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_BOTTOM, g_ui.wall_charge, "<<< RETURN TO DASH <<<");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls();
                    g_ui.state = UI_DASH;
                    g_ui.dash_card = 1;   /* 返回 DASH 第一张卡（COMPASS） */
                    arex_ui_go_to_card(1);
                }
            } else {
                g_ui.wall_charge = 0;
                arex_screen_hide_walls();
                int8_t next = (int8_t)g_ui.menu_info_idx + dir;
                if (next < 0) next = 0;
                if (next >= (int8_t)len) next = len - 1;
                g_ui.menu_info_idx = (uint8_t)next;
                arex_screen_set_info_selection(g_ui.menu_info_idx);
            }
            break;
        }

        /* --- SETUP menu: scroll list with top-wall exit --- */
        case UI_SETUP: {
            uint8_t len = arex_screen_setup_item_count();
            if (dir == -1 && g_ui.menu_setup_idx == 0) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_TOP, g_ui.wall_charge, ">>> RETURN TO DASH >>>");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls();
                    g_ui.state = UI_DASH;
                    g_ui.dash_card = total_cards - 2;   /* 返回 DASH 最后一张卡（PLAN） */
                    arex_ui_go_to_card(total_cards - 2);
                }
            } else {
                g_ui.wall_charge = 0;
                arex_screen_hide_walls();
                int8_t next = (int8_t)g_ui.menu_setup_idx + dir;
                if (next < 0) next = 0;
                if (next >= (int8_t)len) next = len - 1;
                g_ui.menu_setup_idx = (uint8_t)next;
                arex_screen_set_setup_selection(g_ui.menu_setup_idx);
            }
            break;
        }

        /* --- SUB_MENU: scroll sub-menu list --- */
        case UI_SUB_MENU: {
            int8_t next = (int8_t)g_ui.sub_menu_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)g_ui.sub_item_count) next = g_ui.sub_item_count - 1;
            g_ui.sub_menu_idx = (uint8_t)next;
            arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
            break;
        }

        /* --- EDIT_VALUE: adjust numeric value --- */
        case UI_EDIT_VALUE: {
            if (!g_ui.edit_ctx.active) break;
            float next = g_ui.edit_ctx.value + dir * g_ui.edit_ctx.step;
            if (next < g_ui.edit_ctx.min) next = g_ui.edit_ctx.min;
            if (next > g_ui.edit_ctx.max) next = g_ui.edit_ctx.max;
            /* round to avoid float drift */
            int steps = (int)((next - g_ui.edit_ctx.min) / g_ui.edit_ctx.step + 0.5f);
            g_ui.edit_ctx.value = g_ui.edit_ctx.min + steps * g_ui.edit_ctx.step;
            arex_screen_refresh_edit_value();
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
    switch (g_ui.state) {

        case UI_DASH: {
            uint8_t physical_card = g_arex.settings.card_order[g_ui.dash_card];
            if (physical_card == 1 /* COMPASS */) {
                if (!g_arex.compass.marked) {
                    g_arex.compass.marked = true;
                    g_arex.compass.target = g_arex.compass.heading;
                    arex_screen_refresh_compass_target();
                } else {
                    g_ui.state = UI_MODAL_COMPASS;
                    arex_screen_show_modal_compass();
                }
            } else if (physical_card == 3 /* GAS */) {
                g_ui.state = UI_EDIT_GAS;
                g_ui.gas_cursor = g_arex.gas.active_idx;
                arex_screen_refresh_gas_menu();
            }
            break;
        }

        case UI_EDIT_GAS:
            g_ui.state = UI_MODAL_GAS;
            arex_screen_show_modal_gas();
            break;

        case UI_MODAL_GAS: {
            uint8_t ci = g_ui.gas_cursor;
            if (g_arex.dive.depth <= AREX_GAS_TABLE[ci].mod_m) {
                g_arex.gas.active_idx = ci;
                arex_screen_hide_modal();
                g_ui.state = UI_DASH;
                arex_screen_refresh_gas_menu();
                arex_screen_refresh_left_panel();
            } else {
                arex_screen_pulse_modal(); /* shake feedback */
            }
            break;
        }

        case UI_MODAL_COMPASS:
            g_arex.compass.marked = false;
            arex_screen_refresh_compass_target();
            arex_screen_hide_modal();
            g_ui.state = UI_DASH;
            break;

        case UI_EDIT_VALUE:
            /* Commit the edited value */
            g_arex.settings.mod_ppo2 = g_ui.edit_ctx.value;
            g_ui.edit_ctx.active = false;
            g_ui.state = UI_SUB_MENU;
            arex_screen_commit_edit_value();
            break;

        case UI_INFO:
            arex_screen_open_info_submenu(g_ui.menu_info_idx);
            break;

        case UI_SETUP:
            arex_screen_open_setup_submenu(g_ui.menu_setup_idx);
            break;

        case UI_SUB_MENU:
            arex_screen_handle_submenu_select(g_ui.sub_menu_idx);
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
    switch (g_ui.state) {
        case UI_EDIT_GAS:
            g_ui.state = UI_DASH;
            arex_screen_refresh_gas_menu();
            break;

        case UI_MODAL_GAS:
            arex_screen_hide_modal();
            g_ui.state = UI_EDIT_GAS;
            break;

        case UI_MODAL_COMPASS:
        case UI_MODAL_ACT:
            arex_screen_hide_modal();
            g_ui.state = UI_DASH;
            break;

        case UI_EDIT_VALUE:
            /* Cancel — restore original */
            g_ui.edit_ctx.value = g_ui.edit_ctx.original;
            g_ui.edit_ctx.active = false;
            g_ui.state = UI_SUB_MENU;
            arex_screen_cancel_edit_value();
            break;

        case UI_SUB_MENU:
            arex_screen_close_submenu();
            break;

        case UI_INFO:
            g_ui.state = UI_DASH;
            g_ui.dash_card = 1;
            arex_ui_go_to_card(1);
            break;

        case UI_SETUP:
            g_ui.state = UI_DASH;
            g_ui.dash_card = arex_card_count() - 2;
            arex_ui_go_to_card(arex_card_count() - 2);
            break;

        default:
            break;
    }
}
