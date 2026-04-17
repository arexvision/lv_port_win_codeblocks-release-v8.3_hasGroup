/*
 * card_gas.c  —  3F: GAS SWITCH
 *
 * 架构规则：
 *   - 行宽由 g_layout.rc_w 决定；行 Y 坐标用 current_y 累加
 *   - hint label 绝对定位于卡片底部（固定 Y = card_h - HINT_MARGIN）
 *   - 从 g_sensor.depth_m 计算 PPO2
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

#define GAS_ROW_H    49
#define GAS_ROW_GAP   8
#define TITLE_H      44
#define PAD_X        16
#define HINT_MARGIN  24

static lv_obj_t *s_items[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_mod[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_ppo2[AREX_GAS_COUNT];
static lv_obj_t *s_hint;
static int16_t   s_row_w;

void card_gas_update(void);

void card_gas_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "3F: GAS SWITCH");

    s_row_w = g_layout.rc_w - PAD_X * 2;
    int16_t cy = TITLE_H;

    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_pos(row,  PAD_X, cy);
        lv_obj_set_size(row, s_row_w, GAS_ROW_H);
        lv_obj_set_style_bg_color(row, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(row,   LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, AREX_DARK, 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row,   0, 0);
        lv_obj_set_style_pad_ver(row,  12, 0);
        lv_obj_set_style_pad_hor(row,  15, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* 气体名：左上，child 0 */
        lv_obj_t *name = lv_label_create(row);
        lv_obj_set_style_text_color(name, AREX_GREEN, 0);
        lv_obj_set_style_text_font(name,  AREX_FONT_TITLE, 0);
        lv_obj_set_style_bg_opa(name,     LV_OPA_TRANSP, 0);
        lv_label_set_text(name, AREX_GAS_TABLE[i].name);
        lv_obj_set_pos(name, 0, 2);

        /* MOD：右上，绝对 X = row_inner_w - 右侧内容占位 */
        int16_t right_x = s_row_w / 2;
        s_lbl_mod[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_mod[i], AREX_LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_mod[i],  AREX_FONT_SMALL, 0);
        lv_obj_set_style_bg_opa(s_lbl_mod[i],     LV_OPA_TRANSP, 0);
        char mbuf[20];
        snprintf(mbuf, sizeof(mbuf), "MOD %dm", AREX_GAS_TABLE[i].mod_m);
        lv_label_set_text(s_lbl_mod[i], mbuf);
        lv_obj_set_pos(s_lbl_mod[i], right_x, 0);

        /* PPO2：右下 */
        s_lbl_ppo2[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_ppo2[i], AREX_LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_ppo2[i],  AREX_FONT_SMALL, 0);
        lv_obj_set_style_bg_opa(s_lbl_ppo2[i],     LV_OPA_TRANSP, 0);
        lv_label_set_text(s_lbl_ppo2[i], "PO2 -.-");
        lv_obj_set_pos(s_lbl_ppo2[i], right_x, 18);

        cy += GAS_ROW_H + GAS_ROW_GAP;
    }

    /* hint label：绝对定位于底部 */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_color(s_hint, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_hint,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(s_hint,     LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_opa(s_hint,   LV_OPA_60, 0);
    lv_label_set_text(s_hint, "[ PRESS TO SWITCH GAS ]");
    lv_obj_set_pos(s_hint, PAD_X, g_layout.rc_h - HINT_MARGIN - 14);

    card_gas_update();
}

void card_gas_update(void)
{
    float depth = g_sensor.depth_m;

    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        bool is_active = (g_arex.gas.active_idx == (uint8_t)i);
        bool is_cursor = (g_ui.state == UI_EDIT_GAS && g_ui.gas_cursor == (uint8_t)i);
        bool over_mod  = (depth > (float)AREX_GAS_TABLE[i].mod_m);

        /* 背景 / 边框 / 文字颜色三态 */
        lv_color_t bg = AREX_BLACK;
        lv_color_t fg = AREX_GREEN;
        lv_color_t border;

        if (is_cursor) {
            bg = AREX_GREEN; fg = AREX_BLACK;
            border = AREX_GREEN;
        } else if (is_active) {
            border = AREX_GREEN;
        } else if (over_mod) {
            border = lv_color_make(0xFF, 0x00, 0x00);
        } else {
            border = AREX_DARK;
        }

        lv_obj_set_style_bg_color(s_items[i], bg, 0);
        lv_obj_set_style_border_color(s_items[i], border, 0);

        lv_obj_t *name_lbl = lv_obj_get_child(s_items[i], 0);
        if (name_lbl) lv_obj_set_style_text_color(name_lbl, fg, 0);

        /* 实时 PPO2（简化：depth/10 * 0.21 近似） */
        char pbuf[20];
        float ppo2 = depth / 10.0f * 0.21f;
        snprintf(pbuf, sizeof(pbuf), "PO2 %.2f", ppo2);
        lv_label_set_text(s_lbl_ppo2[i], pbuf);
    }

    if (s_hint) {
        lv_label_set_text(s_hint,
            (g_ui.state == UI_EDIT_GAS)
                ? "[ SCROLL TO SELECT / PRESS TO CONFIRM ]"
                : "[ PRESS TO SWITCH GAS ]");
    }
}
