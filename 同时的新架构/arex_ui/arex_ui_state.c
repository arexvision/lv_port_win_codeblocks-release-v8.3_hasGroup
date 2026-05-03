#include "arex_ui_state.h"
#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#define LOG_TAG "arex_ui_state"
#include "log.h"

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
    g_ui.state         = UI_INFO;
    g_ui.dash_card     = CARD_POS_DYNAMIC_FIRST;
    g_ui.menu_info_idx = 0;
    g_ui.wall_charge   = 0;
}

/* =========================================
   Internal: notify registered cards
   ========================================= */
void arex_ui_refresh_all(void)
{
    for (uint8_t i = 0; i < arex_card_count(); i++) {
        arex_card_t *c = arex_card_get(i);
        if (c && c->update_cb) c->update_cb();
    }
}

/* =========================================
   Internal: tileview navigation
   ========================================= */
void arex_ui_go_to_card(uint8_t tile_pos)
{
    uint8_t card_id = arex_card_id_at(tile_pos);
    LOG_I("[NAV] go_to_card state=%u dash_card:%u->%u tile_pos=%u card_id=%u",
          g_ui.state,
          g_ui.dash_card,
          tile_pos,
          tile_pos,
          card_id);
    g_ui.dash_card = tile_pos;
    arex_screen_scroll_to_card(tile_pos);
}

/* =========================================
   Rotate handler (+1 = down, -1 = up)
   ========================================= */
void ui_handle_rotate(int8_t dir)
{
    switch (g_ui.state) {

        /* --- DASH: scroll between cards with wall-charge at edges --- */
        case UI_DASH: {
            uint8_t dash_min = CARD_POS_DYNAMIC_FIRST;
            uint8_t dash_max = arex_setup_display_pos() - 1;
            LOG_I("[ROTATE] state=DASH dir=%d current=%u range=[%u,%u] wall_charge=%u",
                  dir,
                  g_ui.dash_card,
                  dash_min,
                  dash_max,
                  g_ui.wall_charge);

            if (g_ui.dash_card == dash_min && dir == -1) {
                g_ui.wall_charge++;
                LOG_I("[ROTATE] hit_top_wall charge=%u", g_ui.wall_charge);
                arex_screen_show_wall(WALL_TOP, g_ui.wall_charge,
                                     ">>> ENTER INFO MENU >>>");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls_snap();
                    g_ui.state = UI_INFO;
                    g_ui.menu_info_idx = 0;
                    arex_screen_set_info_selection(0);
                    arex_ui_go_to_card(0);
                }
            } else if (g_ui.dash_card == dash_max && dir == 1) {
                g_ui.wall_charge++;
                LOG_I("[ROTATE] hit_bottom_wall charge=%u", g_ui.wall_charge);
                arex_screen_show_wall(WALL_BOTTOM, g_ui.wall_charge,
                                     "<<< ENTER DIVE SETUP <<<");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls_snap();
                    g_ui.state = UI_SETUP;
                    g_ui.menu_setup_idx = 0;
                    arex_screen_set_setup_selection(0);
                    arex_ui_go_to_card(arex_setup_display_pos());
                }
            } else {
                g_ui.wall_charge = 0;
                arex_screen_hide_walls();
                int8_t next = (int8_t)g_ui.dash_card + dir;
                if (next < (int8_t)dash_min) next = (int8_t)dash_min;
                if (next > (int8_t)dash_max) next = (int8_t)dash_max;
                LOG_I("[ROTATE] advance dir=%d current=%u next=%d",
                      dir,
                      g_ui.dash_card,
                      next);
                arex_ui_go_to_card((uint8_t)next);
            }
            break;
        }

        /* --- EDIT_GAS --- */
        case UI_EDIT_GAS: {
            int8_t next = ((int8_t)g_ui.gas_cursor + dir + AREX_GAS_COUNT) % AREX_GAS_COUNT;
            g_ui.gas_cursor = (uint8_t)next;
            arex_screen_refresh_gas_menu();
            break;
        }

        /* --- INFO menu --- */
        case UI_INFO: {
            uint8_t len = arex_screen_info_item_count();
            if (dir == 1 && g_ui.menu_info_idx == len - 1) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_BOTTOM, g_ui.wall_charge,
                                     "<<< RETURN TO DASH <<<");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls_snap();
                    g_ui.state = UI_DASH;
                    g_ui.dash_card = CARD_POS_DYNAMIC_FIRST;
                    arex_ui_go_to_card(CARD_POS_DYNAMIC_FIRST);
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

        /* --- SETUP menu --- */
        case UI_SETUP: {
            uint8_t len = arex_screen_setup_item_count();
            if (dir == -1 && g_ui.menu_setup_idx == 0) {
                g_ui.wall_charge++;
                arex_screen_show_wall(WALL_TOP, g_ui.wall_charge,
                                     ">>> RETURN TO DASH >>>");
                if (g_ui.wall_charge >= 3) {
                    g_ui.wall_charge = 0;
                    arex_screen_hide_walls_snap();
                    g_ui.state = UI_DASH;
                    g_ui.dash_card = arex_setup_display_pos() - 1;
                    arex_ui_go_to_card(arex_setup_display_pos() - 1);
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

        /* --- SUB_MENU --- */
        case UI_SUB_MENU: {
            int8_t next = (int8_t)g_ui.sub_menu_idx + dir;
            if (next < 0) next = 0;
            if (next >= (int8_t)g_ui.sub_item_count) next = g_ui.sub_item_count - 1;
            g_ui.sub_menu_idx = (uint8_t)next;
            arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
            break;
        }

        /* --- EDIT_VALUE --- */
        case UI_EDIT_VALUE: {
            if (!g_ui.edit_ctx.active) break;
            float next = g_ui.edit_ctx.value + dir * g_ui.edit_ctx.step;
            if (next < g_ui.edit_ctx.min) next = g_ui.edit_ctx.min;
            if (next > g_ui.edit_ctx.max) next = g_ui.edit_ctx.max;
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
            /* card_id 从 card_order[] 映射 */
            uint8_t card_id = arex_card_id_at(g_ui.dash_card);

            if (card_id == CARD_ID_COMPASS) {
                if (!g_sensor_data.heading_locked) {
                    g_sensor_data.heading_locked = true;
                    g_sensor_data.heading_target = g_sensor_data.heading;
                    arex_screen_refresh_compass_target();
                } else {
                    g_ui.state = UI_MODAL_COMPASS;
                    arex_screen_show_modal_compass();
                }
            } else if (card_id == CARD_ID_GAS) {
                g_ui.state = UI_EDIT_GAS;
                g_ui.gas_cursor = g_sensor_data.gas_active_idx;
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
            if (g_sensor_data.depth <= AREX_GAS_MOD_M[ci]) {
                g_sensor_data.gas_active_idx = ci;
                arex_screen_hide_modal();
                g_ui.state = UI_DASH;
                strncpy(g_sensor_data.gas_name,
                        AREX_GAS_NAMES[ci], 15);
                g_sensor_data.gas_name[15] = '\0';
                arex_screen_refresh_gas_menu();
                arex_screen_refresh_left_panel();
            } else {
                arex_screen_pulse_modal();
            }
            break;
        }

        case UI_MODAL_COMPASS:
            g_sensor_data.heading_locked = false;
            arex_screen_refresh_compass_target();
            arex_screen_hide_modal();
            g_ui.state = UI_DASH;
            break;

        case UI_EDIT_VALUE:
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
            g_ui.dash_card = CARD_POS_DYNAMIC_FIRST;
            arex_ui_go_to_card(CARD_POS_DYNAMIC_FIRST);
            break;

        case UI_SETUP:
            g_ui.state = UI_DASH;
            g_ui.dash_card = arex_setup_display_pos() - 1;
            arex_ui_go_to_card(arex_setup_display_pos() - 1);
            break;

        default:
            break;
    }
}
