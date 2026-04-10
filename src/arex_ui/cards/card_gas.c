#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

static lv_obj_t *s_items[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_ppo2[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_mod[AREX_GAS_COUNT];

void card_gas_update(void); /* forward declaration */

void card_gas_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_color(title, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_text_font(title, AREX_FONT_SMALL, 0);
    lv_label_set_text(title, "3F  GAS SWITCH");
    lv_obj_set_pos(title, 16, 12);

    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 400, 72);
        lv_obj_set_pos(row, 30, 48 + i * 86);
        lv_obj_set_style_bg_color(row, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_make(0x00,0x33,0x00), 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* Gas name */
        lv_obj_t *lbl_name = lv_label_create(row);
        lv_obj_set_style_text_color(lbl_name, lv_color_make(0x00,0xFF,0x00), 0);
        lv_obj_set_style_text_font(lbl_name, AREX_FONT_MEDIUM, 0);
        lv_label_set_text(lbl_name, AREX_GAS_TABLE[i].name);
        lv_obj_set_pos(lbl_name, 0, 0);

        /* MOD */
        s_lbl_mod[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_mod[i], lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(s_lbl_mod[i], AREX_FONT_SMALL, 0);
        char buf[20];
        snprintf(buf, sizeof(buf), "MOD %dm", AREX_GAS_TABLE[i].mod_m);
        lv_label_set_text(s_lbl_mod[i], buf);
        lv_obj_set_pos(s_lbl_mod[i], 200, 8);

        /* PPO2 (placeholder, updated live) */
        s_lbl_ppo2[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_ppo2[i], lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(s_lbl_ppo2[i], AREX_FONT_SMALL, 0);
        lv_label_set_text(s_lbl_ppo2[i], "PO2 -.-");
        lv_obj_set_pos(s_lbl_ppo2[i], 200, 28);
    }

    card_gas_update();
}

void card_gas_update(void)
{
    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        bool is_active  = (g_arex.gas.active_idx == (uint8_t)i);
        bool is_cursor  = (g_ui.state == UI_EDIT_GAS && g_ui.gas_cursor == (uint8_t)i);
        bool over_mod   = (g_arex.dive.depth > AREX_GAS_TABLE[i].mod_m);

        lv_color_t bg, fg;
        if (is_cursor) {
            bg = lv_color_make(0x00,0xFF,0x00);
            fg = lv_color_make(0x00,0x00,0x00);
        } else if (is_active) {
            bg = lv_color_make(0x00,0x33,0x00);
            fg = lv_color_make(0x00,0xFF,0x00);
        } else {
            bg = lv_color_make(0x00,0x00,0x00);
            fg = lv_color_make(0x00,0xFF,0x00);
        }

        lv_obj_set_style_bg_color(s_items[i], bg, 0);

        /* Danger tint if over MOD */
        if (over_mod) {
            lv_obj_set_style_border_color(s_items[i], lv_color_make(0xFF,0x00,0x00), 0);
        } else {
            lv_obj_set_style_border_color(s_items[i], lv_color_make(0x00,0x33,0x00), 0);
        }

        /* Update PPO2 at current depth */
        char buf[20];
        float ppo2 = g_arex.dive.depth / 10.0f * 0.21f; /* simplified */
        snprintf(buf, sizeof(buf), "PO2 %.2f", ppo2);
        lv_label_set_text(s_lbl_ppo2[i], buf);

        /* Recolor children */
        lv_obj_t *name_lbl = lv_obj_get_child(s_items[i], 0);
        if (name_lbl) lv_obj_set_style_text_color(name_lbl, fg, 0);
    }
}
