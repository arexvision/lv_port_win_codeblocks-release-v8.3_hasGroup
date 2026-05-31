/*
 * 文件: src/app_ui/ui/cards/card_gas.c
 * 作用: 该文件属于仪表卡片模块，负责某一类卡片页面的创建、布局、刷新或与页面注册表之间的装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "../core/ui_vm.h"
#include "../core/vm/ui_vm_dashboard_types.h"
#include "../screen/layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>

/* GAS 行高按规范锁死 49px（不可改），行间距从 gap_menu 配置推算 */
#define GAS_ROW_H   49

static lv_obj_t *s_items[GAS_COUNT];
static lv_obj_t *s_lbl_name[GAS_COUNT];
static lv_obj_t *s_lbl_ppo2[GAS_COUNT];
static lv_obj_t *s_lbl_mod[GAS_COUNT];
static lv_obj_t *s_hint;
static ui_vm_gas_t s_gas_vm_cache;

static bool gas_obj_is_valid(lv_obj_t **obj_ref)
{
    if (obj_ref == NULL || *obj_ref == NULL)
    {
        return false;
    }

    if (!lv_obj_is_valid(*obj_ref))
    {
        *obj_ref = NULL;
        return false;
    }

    return true;
}

void card_gas_update(void); /* forward declaration */
void card_gas_update_vm(const ui_vm_gas_t *vm);

void card_gas_create(lv_obj_t *parent)
{
    render_card_title(parent, "GAS SWITCH");

    ui_vm_gas_update(&s_gas_vm_cache,
                     NULL,
                     NULL,
                     ui_state_get_state(),
                     ui_state_get_gas_cursor());
    int right_canvas_w = (int)s_gas_vm_cache.right_canvas_w;
    int row_w = right_canvas_w - 15;   /* 右侧 15px 呼吸间隙 */
    int gap_y = (int)s_gas_vm_cache.gap_y;
    int row_h = GAS_ROW_H;
    int content_h = (int)ui_content_h_get();
    if (!ui_layout_is_vertical_split())
    {
        gap_y = 4;
        row_h = (content_h - CARD_TITLE_H - 30 - (GAS_COUNT - 1) * gap_y) / GAS_COUNT;
        if (row_h < 34)
        {
            row_h = 34;
        }
        if (row_h > GAS_ROW_H)
        {
            row_h = GAS_ROW_H;
        }
    }

    for (int i = 0; i < GAS_COUNT; i++)
    {
        /* 气体行：Y 起点紧贴标题区下方，内容区自适应 */
        lv_coord_t row_y = CARD_TITLE_H + i * (row_h + gap_y);

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, row_w, row_h);
        lv_obj_set_pos(row, 0, row_y);
        lv_obj_set_style_bg_color(row, lv_color_make(0,0,0), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, DARK, 0);
        lv_obj_set_style_border_width(row, GAS_BORDER_W, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);   /* 零边距，子元素绝对定位 */
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_items[i] = row;

        /* Gas name — 左侧 12px 呼吸，垂直居中 */
        s_lbl_name[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_name[i], GREEN, 0);
        lv_obj_set_style_text_font(s_lbl_name[i], get_font(FONT_ID_TITLE), 0);
        lv_label_set_text(s_lbl_name[i], GAS_NAMES[i]);
        lv_obj_set_size(s_lbl_name[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(s_lbl_name[i], LV_ALIGN_LEFT_MID, 12, 0);

        /* MOD — 右侧，垂直偏上 */
        s_lbl_mod[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_mod[i], LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_mod[i], get_font(FONT_ID_SMALL), 0);
        char buf[20];
        snprintf(buf, sizeof(buf), "MOD %dm", GAS_MOD_M[i]);
        lv_label_set_text(s_lbl_mod[i], buf);
        lv_obj_set_size(s_lbl_mod[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(s_lbl_mod[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(s_lbl_mod[i], LV_ALIGN_RIGHT_MID, -25, -9);

        /* PPO2 — 右侧，垂直偏下 */
        s_lbl_ppo2[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_lbl_ppo2[i], LIGHT, 0);
        lv_obj_set_style_text_font(s_lbl_ppo2[i], get_font(FONT_ID_SMALL), 0);
        lv_label_set_text(s_lbl_ppo2[i], "PO2 -.-");
        lv_obj_set_size(s_lbl_ppo2[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(s_lbl_ppo2[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(s_lbl_ppo2[i], LV_ALIGN_RIGHT_MID, -25, 9);
    }

    /* Hint text at bottom */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_color(s_hint, LIGHT, 0);
    lv_obj_set_style_text_font(s_hint, get_font(FONT_ID_SMALL), 0);
    lv_label_set_text(s_hint, "[ PRESS TO SWITCH GAS ]");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    card_gas_update_vm(&s_gas_vm_cache);
}

void card_gas_update(void)
{
    ui_vm_gas_t vm;

    ui_vm_gas_update(&vm, NULL, NULL, ui_state_get_state(), ui_state_get_gas_cursor());
    card_gas_update_vm(&vm);
}

void card_gas_update_vm(const ui_vm_gas_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    s_gas_vm_cache = *vm;

    for (int i = 0; i < GAS_COUNT; i++)
    {
        if (!gas_obj_is_valid(&s_items[i]) ||
            !gas_obj_is_valid(&s_lbl_name[i]) ||
            !gas_obj_is_valid(&s_lbl_mod[i]) ||
            !gas_obj_is_valid(&s_lbl_ppo2[i]))
        {
            continue;
        }

        if (s_gas_vm_cache.visible[i] == 0U)
        {
            lv_obj_add_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
        bool highlight = (s_gas_vm_cache.highlighted[i] != 0U);

        lv_color_t fg = GREEN;

        lv_obj_set_style_bg_color(s_items[i], BLACK, 0);
        lv_obj_set_style_bg_opa(s_items[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_items[i], highlight ? GREEN : DARK, 0);
        lv_obj_set_style_border_width(s_items[i], highlight ? (GAS_BORDER_W + 2) : GAS_BORDER_W, 0);
        if (highlight)
        {
            lv_obj_add_state(s_items[i], LV_STATE_FOCUSED);
        }
        else
        {
            lv_obj_clear_state(s_items[i], LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);
        }

        lv_label_set_text(s_lbl_name[i], s_gas_vm_cache.names[i]);
        lv_label_set_text(s_lbl_mod[i], s_gas_vm_cache.mod_text[i]);
        lv_label_set_text(s_lbl_ppo2[i], s_gas_vm_cache.ppo2_text[i]);

        /* Recolor children */
        if (s_lbl_name[i])
        {
            lv_obj_set_style_text_color(s_lbl_name[i], highlight ? LIGHT : fg, 0);
            lv_obj_set_style_text_font(s_lbl_name[i],
                                       get_font(highlight ? FONT_ID_MEDIUM : FONT_ID_TITLE),
                                       0);
        }
        if (s_lbl_mod[i])
        {
            lv_obj_set_style_text_color(s_lbl_mod[i], LIGHT, 0);
        }
        if (s_lbl_ppo2[i])
        {
            lv_obj_set_style_text_color(s_lbl_ppo2[i], LIGHT, 0);
        }
    }

    /* Update hint text based on edit state */
    if (gas_obj_is_valid(&s_hint))
    {
        lv_label_set_text(s_hint, s_gas_vm_cache.hint);
    }
}
