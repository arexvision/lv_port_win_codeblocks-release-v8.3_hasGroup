/*
 * 文件: src/app_ui/ui/cards/card_deco.c
 * 作用: 该文件属于仪表卡片模块，负责某一类卡片页面的创建、布局、刷新或与页面注册表之间的装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_vm.h"
#include "../core/vm/ui_vm_dashboard_types.h"
#include "../screen/layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <stdbool.h>

/* deco 内容区 Y 起点 = 标题区下方 */
#define DECO_CONTENT_Y  (CARD_TITLE_H + 20)
#define DECO_ROW2_Y     (CARD_TITLE_H + 67)
#define DECO_ROW3_Y     (CARD_TITLE_H + 114)
#define GRID_X              16

/* Tissue bars use ambient-relative per-compartment load against the raw M-value:
 * - 0..100 maps directly to bar height
 * - GF High is drawn as the stop/reference line
 * - >=100 means the current-depth raw M-value line is reached
 */
#define TISSUE_DANGER_PCT   100

/* HTML --flash-speed default 0.3s → 300ms half-period for flashInvert */
#define TISSUE_FLASH_MS     300

static lv_obj_t *s_bars[16];
static lv_obj_t *s_lbl_gf99;
static lv_obj_t *s_lbl_surf_gf;
static lv_obj_t *s_lbl_cns;
static lv_obj_t *s_lbl_otu;
static lv_obj_t *s_lbl_gf_setting;
static lv_obj_t *s_mvalue_line;
static lv_obj_t *s_mvalue_label;

static lv_timer_t *s_tissue_flash_timer;
static bool        s_tissue_flash_phase;
static ui_vm_deco_t s_deco_vm_cache;

void card_deco_update(void);
void card_deco_update_vm(const ui_vm_deco_t *vm);

static bool deco_obj_is_valid(lv_obj_t **obj_ref)
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

static bool any_tissue_danger(void)
{
    for (int i = 0; i < 16; i++)
    {
        if (s_deco_vm_cache.tissue_raw_pct[i] >= TISSUE_DANGER_PCT) return true;
    }
    return false;
}

static void tissue_danger_flash_cb(lv_timer_t *t)
{
    (void)t;
    s_tissue_flash_phase = !s_tissue_flash_phase;
    for (int i = 0; i < 16; i++)
    {
        if (s_deco_vm_cache.tissue_raw_pct[i] >= TISSUE_DANGER_PCT)
        {
            // 危险时在 亮绿 和 暗绿空槽 之间闪烁
            lv_color_t c = s_tissue_flash_phase ? GREEN : DARK;
            if (!deco_obj_is_valid(&s_bars[i]))
            {
                continue;
            }

            lv_obj_t *bar_fill = lv_obj_get_child(s_bars[i], 0);
            if (bar_fill)
            {
                lv_obj_set_style_bg_color(bar_fill, c, 0);
            }
        }
    }
}

static void tissue_flash_ensure(void)
{
    if (any_tissue_danger())
    {
        if (!s_tissue_flash_timer)
        {
            s_tissue_flash_phase = false;
            s_tissue_flash_timer = lv_timer_create(tissue_danger_flash_cb, TISSUE_FLASH_MS, NULL);
        }
    }
    else
    {
        if (s_tissue_flash_timer)
        {
            lv_timer_del(s_tissue_flash_timer);
            s_tissue_flash_timer = NULL;
        }
    }
}

static lv_color_t tissue_fill_color(uint8_t pct, uint8_t gf_high)
{
    if (pct >= TISSUE_DANGER_PCT)
        return s_tissue_flash_phase ? GREEN : DARK;
    if (pct >= gf_high)
        return LIGHT;
    return GREEN;
}

static uint8_t card_deco_mvalue_line_pct(void)
{
    uint8_t line_pct = s_deco_vm_cache.gf_high;
    if (line_pct > 100U)
    {
        line_pct = 100U;
    }
    return line_pct;
}

static bool card_deco_tissue_chart_active(void)
{
    return s_deco_vm_cache.chart_active != 0U;
}

static void card_deco_update_mvalue_line(bool chart_active)
{
    if (!deco_obj_is_valid(&s_mvalue_line) || !deco_obj_is_valid(&s_mvalue_label))
    {
        return;
    }

    if (!chart_active)
    {
        lv_obj_add_flag(s_mvalue_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mvalue_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_mvalue_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_mvalue_label, LV_OBJ_FLAG_HIDDEN);

    if (!deco_obj_is_valid(&s_bars[0]))
    {
        return;
    }

    int bar_max_h = (int)lv_obj_get_height(s_bars[0]);
    int line_y = bar_max_h;
    uint8_t line_pct = card_deco_mvalue_line_pct();

    line_y -= ((int)line_pct * bar_max_h) / 100;
    if (line_y < 0)
    {
        line_y = 0;
    }
    if (line_y > bar_max_h - 2)
    {
        line_y = bar_max_h - 2;
    }

    lv_obj_set_y(s_mvalue_line, line_y);
    lv_label_set_text_fmt(s_mvalue_label, "M-VALUE %u%%", line_pct);
    lv_obj_align_to(s_mvalue_label, s_mvalue_line, LV_ALIGN_OUT_TOP_RIGHT, -4, -2);
}

static void surf_gf_apply_style(void)
{
    if (!deco_obj_is_valid(&s_lbl_surf_gf))
    {
        return;
    }

    if (s_deco_vm_cache.surf_gf_alert != 0U)
    {
        lv_obj_set_style_bg_color(s_lbl_surf_gf, BLACK, 0);
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, LIGHT, 0);
        lv_obj_set_style_border_color(s_lbl_surf_gf, GREEN, 0);
        lv_obj_set_style_border_width(s_lbl_surf_gf, 2, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 4, 0);
        lv_obj_set_style_pad_ver(s_lbl_surf_gf, 0, 0);
    }
    else
    {
        lv_obj_set_style_bg_opa(s_lbl_surf_gf, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_lbl_surf_gf, GREEN, 0);
        lv_obj_set_style_border_width(s_lbl_surf_gf, 0, 0);
        lv_obj_set_style_pad_hor(s_lbl_surf_gf, 0, 0);
        lv_obj_set_style_pad_ver(s_lbl_surf_gf, 0, 0);
    }
}

static void make_grid_row(lv_obj_t *parent, lv_coord_t y,
                          const char *left_cap, const char *left_val, lv_obj_t **left_ref,
                          const char *right_cap, const char *right_val, lv_obj_t **right_ref,
                          bool dashed_bottom, int tissue_area_w)
{
    lv_obj_t *lc = lv_label_create(parent);
    lv_obj_set_style_text_color(lc, LIGHT, 0);
    lv_obj_set_style_text_font(lc, get_font(FONT_ID_SMALL), 0);
    lv_label_set_text(lc, left_cap);
    lv_obj_set_pos(lc, GRID_X, y);

    lv_obj_t *lv_ = lv_label_create(parent);
    lv_obj_set_style_text_color(lv_, GREEN, 0);
    lv_obj_set_style_text_font(lv_, get_font(FONT_ID_TITLE), 0);
    lv_label_set_text(lv_, left_val);
    lv_obj_set_pos(lv_, GRID_X, y + 16);
    if (left_ref) *left_ref = lv_;

    lv_obj_t *rc = lv_label_create(parent);
    lv_obj_set_style_text_color(rc, LIGHT, 0);
    lv_obj_set_style_text_font(rc, get_font(FONT_ID_SMALL), 0);
    lv_label_set_text(rc, right_cap);
    lv_obj_set_pos(rc, GRID_X + tissue_area_w / 2 + 4, y);

    lv_obj_t *rv = lv_label_create(parent);
    lv_obj_set_style_text_color(rv, GREEN, 0);
    lv_obj_set_style_text_font(rv, get_font(FONT_ID_TITLE), 0);
    lv_label_set_text(rv, right_val);
    lv_obj_set_pos(rv, GRID_X + tissue_area_w / 2 + 4, y + 16);
    if (right_ref) *right_ref = rv;

    if (dashed_bottom)
    {
        lv_obj_t *line = lv_obj_create(parent);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, tissue_area_w, 1);
        lv_obj_set_pos(line, GRID_X, y + 40);
        lv_obj_set_style_bg_color(line, DARK, 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    }
}

void card_deco_create(lv_obj_t *parent)
{
    render_card_title(parent, "TISSUES & DECO");

    int right_canvas_w = (int)ui_content_w_get();
    int content_h = (int)ui_content_h_get();
    int row1_y = DECO_CONTENT_Y;
    int row2_y = DECO_ROW2_Y;
    int row3_y = DECO_ROW3_Y;
    int chart_h = 120;
    int chart_bottom = -15;

    if (!ui_layout_is_vertical_split())
    {
        row1_y = CARD_TITLE_H + 8;
        row2_y = CARD_TITLE_H + 45;
        row3_y = CARD_TITLE_H + 82;
        chart_h = (content_h > 250) ? 82 : 70;
        chart_bottom = -8;
    }

    make_grid_row(parent, row1_y,
                  "ALGORITHM", "ZHL-16C", NULL,
                  "GF LOW / HIGH", "-- / --", &s_lbl_gf_setting,
                  true, right_canvas_w - 15);

    make_grid_row(parent, row2_y,
                  "GF99", "--", &s_lbl_gf99,
                  "SurfGF", "--", &s_lbl_surf_gf,
                  true, right_canvas_w - 15);

    make_grid_row(parent, row3_y,
                  "CNS O2", "--%", &s_lbl_cns,
                  "OTU", "--", &s_lbl_otu,
                  false, right_canvas_w - 15);

    int chart_w = right_canvas_w - 40;     /* 增加左右安全边距防截断 */

    lv_obj_t *chart_container = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_size(chart_container, chart_w, chart_h);
    lv_obj_align(chart_container, LV_ALIGN_BOTTOM_MID, 0, chart_bottom);

    /* 🚨 核心修复 1: 缩短标题，防止和右侧的 M-VALUE 撞车！ */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(sec_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(sec_lbl, LIGHT, 0);
    lv_label_set_text(sec_lbl, "TISSUE SATURATION"); // 删掉了冗长的 (16 COMPARTMENTS)
    lv_obj_align_to(sec_lbl, chart_container, LV_ALIGN_OUT_TOP_LEFT, 0, -10);

    int text_h    = 16;
    int bar_max_h = chart_h - text_h;
    int exact_col_w = chart_w / 16;

    for (int i = 0; i < 16; i++)
    {
        int exact_x = i * exact_col_w;
        int bar_w   = exact_col_w - 4;

        lv_obj_t *bar_bg = lv_obj_create(chart_container);
        lv_obj_remove_style_all(bar_bg);
        lv_obj_set_style_bg_color(bar_bg, DARK, 0);
        lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
        lv_obj_set_size(bar_bg, bar_w, bar_max_h);
        lv_obj_set_pos(bar_bg, exact_x + 2, 0);

        lv_obj_t *bar_fill = lv_obj_create(bar_bg);
        lv_obj_remove_style_all(bar_fill);
        lv_obj_set_style_bg_color(bar_fill, GREEN, 0);
        lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
        // 初始化时给最小高度，具体数据由 update 注入
        lv_obj_set_size(bar_fill, LV_PCT(100), 2);
        lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);

        lv_obj_t *num_lbl = lv_label_create(chart_container);
        lv_label_set_text_fmt(num_lbl, "%d", i + 1);
        lv_obj_set_style_text_font(num_lbl, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(num_lbl, LIGHT, 0);
        lv_obj_set_size(num_lbl, exact_col_w, text_h);
        lv_obj_set_pos(num_lbl, exact_x, bar_max_h);
        lv_obj_set_style_text_align(num_lbl, LV_TEXT_ALIGN_CENTER, 0);

        s_bars[i] = bar_bg;
    }

    s_mvalue_line = lv_obj_create(chart_container);
    lv_obj_remove_style_all(s_mvalue_line);
    lv_obj_set_size(s_mvalue_line, chart_w, 2);
    lv_obj_set_pos(s_mvalue_line, 0, bar_max_h / 2);
    lv_obj_set_style_bg_color(s_mvalue_line, GREEN, 0);
    lv_obj_set_style_bg_opa(s_mvalue_line, LV_OPA_COVER, 0);

    s_mvalue_label = lv_label_create(chart_container);
    lv_obj_set_style_text_font(s_mvalue_label, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_mvalue_label, GREEN, 0);
    lv_obj_set_style_bg_opa(s_mvalue_label, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_mvalue_label, BLACK, 0);

    card_deco_update();
}

void card_deco_update(void)
{
    ui_vm_deco_t vm;

    ui_vm_deco_update(&vm, NULL, NULL);
    card_deco_update_vm(&vm);
}

void card_deco_update_vm(const ui_vm_deco_t *vm)
{
    if (vm == NULL)
    {
        return;
    }

    s_deco_vm_cache = *vm;

    uint8_t line_pct = card_deco_mvalue_line_pct();
    bool chart_active = card_deco_tissue_chart_active();

    if (deco_obj_is_valid(&s_lbl_gf_setting))
    {
        lv_label_set_text(s_lbl_gf_setting, s_deco_vm_cache.gf_setting);
    }

    if (deco_obj_is_valid(&s_lbl_gf99))
    {
        lv_label_set_text(s_lbl_gf99, s_deco_vm_cache.gf99);
    }

    if (deco_obj_is_valid(&s_lbl_surf_gf))
    {
        lv_label_set_text(s_lbl_surf_gf, s_deco_vm_cache.surf_gf);
    }
    surf_gf_apply_style();

    if (deco_obj_is_valid(&s_lbl_cns))
    {
        lv_label_set_text(s_lbl_cns, s_deco_vm_cache.cns);
    }

    if (deco_obj_is_valid(&s_lbl_otu))
    {
        lv_label_set_text(s_lbl_otu, s_deco_vm_cache.otu);
    }

    card_deco_update_mvalue_line(chart_active);

    tissue_flash_ensure();

    for (int i = 0; i < 16; i++)
    {
        uint8_t pct = s_deco_vm_cache.tissue_raw_pct[i];
        if (!deco_obj_is_valid(&s_bars[i]))
        {
            continue;
        }

        lv_obj_t *bar_fill = lv_obj_get_child(s_bars[i], 0);
        if (bar_fill)
        {
            int bar_max_h = (int)lv_obj_get_height(s_bars[i]);
            int fill_h = 0;
            uint8_t draw_pct = pct;

            if (!chart_active)
            {
                draw_pct = 0U;
            }
            if (draw_pct > 100U)
            {
                draw_pct = 100U;
            }

            if (draw_pct > 0U)
            {
                fill_h = ((int)draw_pct * bar_max_h) / 100;
                if (fill_h < 2)
                {
                    fill_h = 2;
                }
                if (fill_h > bar_max_h)
                {
                    fill_h = bar_max_h;
                }
            }

            lv_obj_set_size(bar_fill, LV_PCT(100), fill_h);
            lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(bar_fill, tissue_fill_color(pct, line_pct), 0);
        }
    }
}
