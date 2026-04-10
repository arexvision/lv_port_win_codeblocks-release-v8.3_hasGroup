#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

#define GAS_ROW_H   49   /* 规范：约 49px（padding上下12px） */
#define GAS_ROW_GAP  8

static lv_obj_t *s_items[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_ppo2[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_mod[AREX_GAS_COUNT];
static lv_obj_t *s_hint;

void card_gas_update(void); /* forward declaration */

void card_gas_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "3F: GAS SWITCH");

    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        lv_coord_t row_y = 50 + i * (GAS_ROW_H + GAS_ROW_GAP);

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 428, GAS_ROW_H);
        lv_obj_set_pos(row, 16, row_y);
        lv_obj_set_style_bg_color(row, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_make(0x00,0x33,0x00), 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_ver(row, 12, 0); /* 规范：padding 上下 12px */
        lv_obj_set_style_pad_hor(row, 15, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* Gas name — left side */
        lv_obj_t *lbl_name = lv_label_create(row);
        lv_obj_set_style_text_color(lbl_name, lv_color_make(0x00,0xFF,0x00), 0);
        lv_obj_set_style_text_font(lbl_name, AREX_FONT_TITLE, 0);
        lv_label_set_text(lbl_name, AREX_GAS_TABLE[i].name);
        /* 规范：pad_ver=12px(上下)，行高49px → 内容区25px；字体20px垂直居中 */
        lv_obj_set_pos(lbl_name, 0, 2);

        /* MOD — right side, top */
        s_lbl_mod[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_mod[i], lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(s_lbl_mod[i], AREX_FONT_SMALL, 0);
        char buf[20];
        snprintf(buf, sizeof(buf), "MOD %dm", AREX_GAS_TABLE[i].mod_m);
        lv_label_set_text(s_lbl_mod[i], buf);
        lv_obj_set_pos(s_lbl_mod[i], 220, 0);

        /* PPO2 — right side, bottom */
        s_lbl_ppo2[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_ppo2[i], lv_color_make(0x55,0xFF,0x55), 0);
        lv_obj_set_style_text_font(s_lbl_ppo2[i], AREX_FONT_SMALL, 0);
        lv_label_set_text(s_lbl_ppo2[i], "PO2 -.-");
        lv_obj_set_pos(s_lbl_ppo2[i], 220, 18);
    }

    /* Hint text at bottom */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_color(s_hint, lv_color_make(0x55,0xFF,0x55), 0);
    lv_obj_set_style_text_font(s_hint, AREX_FONT_SMALL, 0);
    lv_label_set_text(s_hint, "[ PRESS TO SWITCH GAS ]");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    card_gas_update();
}

void card_gas_update(void)
{
    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        bool is_active  = (g_arex.gas.active_idx == (uint8_t)i);
        bool is_cursor  = (g_ui.state == UI_EDIT_GAS && g_ui.gas_cursor == (uint8_t)i);

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
        lv_obj_set_style_border_color(s_items[i], lv_color_make(0x00,0x33,0x00), 0);

        /* Update PPO2 at current depth */
        char buf[20];
        float ppo2 = g_arex.dive.depth / 10.0f * 0.21f;
        snprintf(buf, sizeof(buf), "PO2 %.2f", ppo2);
        lv_label_set_text(s_lbl_ppo2[i], buf);

        /* Recolor children */
        lv_obj_t *name_lbl = lv_obj_get_child(s_items[i], 0);
        if (name_lbl) lv_obj_set_style_text_color(name_lbl, fg, 0);
    }

    /* Update hint text based on edit state */
    if (s_hint) {
        lv_label_set_text(s_hint,
            (g_ui.state == UI_EDIT_GAS)
                ? "[ SCROLL TO SELECT / PRESS TO CONFIRM ]"
                : "[ PRESS TO SWITCH GAS ]");
    }
}
