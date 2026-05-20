#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "../arex_ui_state.h"
#include "../arex_layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

/* GAS 行高按规范锁死 49px（不可改），行间距从 gap_menu 配置推算 */
#define GAS_ROW_H   49

static lv_obj_t *s_items[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_name[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_ppo2[AREX_GAS_COUNT];
static lv_obj_t *s_lbl_mod[AREX_GAS_COUNT];
static lv_obj_t *s_hint;

void card_gas_update(void); /* forward declaration */

void card_gas_create(lv_obj_t *parent)
{
    arex_render_card_title(parent, "GAS SWITCH");

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - ((int)g_sys_config.gap_u * AREX_BASE_U);
    int row_w = right_canvas_w - 15;   /* 右侧 15px 呼吸间隙 */
    int gap_y = (int)g_sys_config.gap_menu * AREX_BASE_U;  /* 从配置推算 */

    for (int i = 0; i < AREX_GAS_COUNT; i++)
    {
        /* 气体行：Y 起点紧贴标题区下方，内容区自适应 */
        lv_coord_t row_y = AREX_CARD_TITLE_H + i * (GAS_ROW_H + gap_y);

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, row_w, GAS_ROW_H);
        lv_obj_set_pos(row, 0, row_y);
        lv_obj_set_style_bg_color(row, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, AREX_DARK, 0);
        lv_obj_set_style_border_width(row, AREX_GAS_BORDER_W, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);   /* 零边距，子元素绝对定位 */
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* Gas name — 左侧 12px 呼吸，垂直居中 */
        s_lbl_name[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_name[i], AREX_GREEN, 0);
        lv_obj_set_style_text_font(s_lbl_name[i], arex_get_font(AREX_FONT_ID_TITLE), 0);
        lv_label_set_text(s_lbl_name[i], AREX_GAS_NAMES[i]);
        lv_obj_set_size(s_lbl_name[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(s_lbl_name[i], LV_ALIGN_LEFT_MID, 12, 0);

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
    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count == 0U || gas_count > AREX_GAS_COUNT)
    {
        gas_count = 1U;
    }

    for (int i = 0; i < AREX_GAS_COUNT; i++)
    {
        if (i >= gas_count)
        {
            lv_obj_add_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);

        bool is_active  = (g_sensor_data.gas_active_idx == (uint8_t)i);
        bool is_cursor  = (g_ui.state == UI_EDIT_GAS && g_ui.gas_cursor == (uint8_t)i);

        /* 高亮条件：光标所在行始终高亮；非编辑模式下，激活气体高亮 */
        bool highlight = is_cursor || (is_active && g_ui.state != UI_EDIT_GAS);

        lv_color_t fg = AREX_GREEN;

        lv_obj_set_style_bg_color(s_items[i], AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(s_items[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_items[i], highlight ? AREX_GREEN : AREX_DARK, 0);
        lv_obj_set_style_border_width(s_items[i], highlight ? (AREX_GAS_BORDER_W + 2) : AREX_GAS_BORDER_W, 0);
        if (highlight)
        {
            lv_obj_add_state(s_items[i], LV_STATE_FOCUSED);
        }
        else
        {
            lv_obj_clear_state(s_items[i], LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);
        }

        const char *slot_name = g_sensor_data.gas_slot_name[i][0] ? g_sensor_data.gas_slot_name[i]
                                : AREX_GAS_NAMES[i];
        float mod_m = g_sensor_data.gas_slot_mod_m[i];
        float ppo2 = g_sensor_data.ppo2[i];

        char buf[32];
        lv_label_set_text_fmt(s_lbl_name[i], "%s", slot_name);

        if (mod_m > 0.0f)
        {
            snprintf(buf, sizeof(buf), "MOD %.0fm", (double)mod_m);
        }
        else
        {
            snprintf(buf, sizeof(buf), "MOD --");
        }
        lv_label_set_text(s_lbl_mod[i], buf);

        snprintf(buf, sizeof(buf), "PO2 %.2f", (double)ppo2);
        lv_label_set_text(s_lbl_ppo2[i], buf);

        /* Recolor children */
        if (s_lbl_name[i])
        {
            lv_obj_set_style_text_color(s_lbl_name[i], highlight ? AREX_LIGHT : fg, 0);
            lv_obj_set_style_text_font(s_lbl_name[i],
                                       arex_get_font(highlight ? AREX_FONT_ID_MEDIUM : AREX_FONT_ID_TITLE),
                                       0);
        }
        if (s_lbl_mod[i])
        {
            lv_obj_set_style_text_color(s_lbl_mod[i], AREX_LIGHT, 0);
        }
        if (s_lbl_ppo2[i])
        {
            lv_obj_set_style_text_color(s_lbl_ppo2[i], AREX_LIGHT, 0);
        }
    }

    /* Update hint text based on edit state */
    if (s_hint)
    {
        lv_label_set_text(s_hint,
                          (g_ui.state == UI_EDIT_GAS)
                          ? "[ SCROLL TO SELECT / PRESS TO CONFIRM ]"
                          : "[ PRESS TO SWITCH GAS ]");
    }
}
