#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
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

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - (g_sys_config.gap_u * AREX_BASE_U);
    int row_w = right_canvas_w - 15;   /* 右侧 15px 呼吸间隙 */

    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        lv_coord_t row_y = 50 + i * (GAS_ROW_H + GAS_ROW_GAP);

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, row_w, GAS_ROW_H);
        lv_obj_set_pos(row, 0, row_y);
        lv_obj_set_style_bg_color(row, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, AREX_DARK, 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);   /* 零边距，子元素绝对定位 */
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* Gas name — 左侧 12px 呼吸，垂直居中 */
        lv_obj_t *lbl_name = lv_label_create(row);
        lv_obj_set_style_text_color(lbl_name, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl_name, arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_label_set_text(lbl_name, AREX_GAS_NAMES[i]);
        lv_obj_set_size(lbl_name, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 12, 0);

        /* MOD — 右侧，垂直偏上 */
        s_lbl_mod[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_mod[i], AREX_LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_mod[i], arex_get_font(AREX_FONT_ID_SMALL), 0);
        char buf[20];
        snprintf(buf, sizeof(buf), "MOD %dm", AREX_GAS_MOD_M[i]);
        lv_label_set_text(s_lbl_mod[i], buf);
        lv_obj_set_size(s_lbl_mod[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(s_lbl_mod[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(s_lbl_mod[i], LV_ALIGN_RIGHT_MID, -25, -9);

        /* PPO2 — 右侧，垂直偏下 */
        s_lbl_ppo2[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_ppo2[i], AREX_LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_ppo2[i], arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_label_set_text(s_lbl_ppo2[i], "PO2 -.-");
        lv_obj_set_size(s_lbl_ppo2[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(s_lbl_ppo2[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(s_lbl_ppo2[i], LV_ALIGN_RIGHT_MID, -25, 9);
    }

    /* Hint text at bottom */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_color(s_hint, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_hint, arex_get_font(AREX_FONT_ID_SMALL), 0);
    lv_label_set_text(s_hint, "[ PRESS TO SWITCH GAS ]");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    card_gas_update();
}

void card_gas_update(void)
{
    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        bool is_active  = (g_sensor_data.gas_active_idx == (uint8_t)i);
        bool is_cursor  = (g_ui.state == UI_EDIT_GAS && g_ui.gas_cursor == (uint8_t)i);

        lv_color_t bg, fg;
        if (is_cursor) {
            bg = lv_color_make(0x00,0xFF,0x00);
            fg = lv_color_make(0x00,0x00,0x00);
        } else if (is_active) {
            bg = lv_color_make(0x00,0x00,0x00);
            fg = lv_color_make(0x00,0xFF,0x00);
        } else {
            bg = lv_color_make(0x00,0x00,0x00);
            fg = lv_color_make(0x00,0xFF,0x00);
        }

        lv_obj_set_style_bg_color(s_items[i], bg, 0);

        /* 被选中的活动气体：边框变绿色 #00FF00 */
        if (is_active) {
            lv_obj_set_style_border_color(s_items[i], AREX_GREEN, 0);
        } else {
            lv_obj_set_style_border_color(s_items[i], AREX_DARK, 0);
        }

        char buf[20];
        float ppo2 = g_sensor_data.depth / 10.0f * 0.21f;
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
