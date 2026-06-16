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
#include <string.h>

/* deco 内容区 Y 起点 = 标题区下方 */
#define DECO_CONTENT_Y  (CARD_TITLE_H + 20)
#define DECO_ROW2_Y     (CARD_TITLE_H + 67)
#define DECO_ROW3_Y     (CARD_TITLE_H + 114)
#define GRID_X              16

#define TISSUE_DRAW_MIN_PCT      (-100)  /* 绘制下界：欠饱和 */
#define TISSUE_DRAW_MAX_PCT      120     /* 绘制上界：过 M 值封顶 */
#define TISSUE_BASELINE_PCT      0       /* 环境水压基准线 */
#define TISSUE_MVALUE_PCT        100     /* 绝对 M 值线 */
#define TISSUE_TARGET_FALLBACK   85      /* target GF 缺省绘制线 */

/* HTML --flash-speed default 0.3s → 300ms half-period for flashInvert */
#define TISSUE_FLASH_MS     300

static lv_obj_t *s_tissue_chart;
static lv_obj_t *s_lbl_gf99;
static lv_obj_t *s_lbl_surf_gf;
static lv_obj_t *s_lbl_cns;
static lv_obj_t *s_lbl_otu;
static lv_obj_t *s_lbl_gf_setting;

static lv_timer_t *s_tissue_flash_timer;
static bool        s_tissue_flash_phase;
static ui_vm_deco_t s_deco_vm_cache;
static bool        s_surf_gf_alert_cache;
static bool        s_surf_gf_style_cache_valid;

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

static bool deco_page_refresh_visible(void)
{
    return screen_page_id_refresh_visible(PAGE_ID_DECO);
}

static void deco_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *old_text;

    if (label == NULL || text == NULL)
    {
        return;
    }

    old_text = lv_label_get_text(label);
    if (old_text != NULL && strcmp(old_text, text) == 0)
    {
        return;
    }

    lv_label_set_text(label, text);
}

static bool any_tissue_danger(void)
{
    for (int i = 0; i < 16; i++)
    {
        if (s_deco_vm_cache.tissue_raw_pct[i] > TISSUE_MVALUE_PCT) return true;
    }
    return false;
}

static void tissue_danger_flash_cb(lv_timer_t *t)
{
    (void)t;
    if (!deco_page_refresh_visible() || !deco_obj_is_valid(&s_tissue_chart))
    {
        return;
    }

    s_tissue_flash_phase = !s_tissue_flash_phase;
    lv_obj_invalidate(s_tissue_chart);
}

static void tissue_flash_ensure(void)
{
    if (any_tissue_danger())
    {
        if (!s_tissue_flash_timer)
        {
            s_tissue_flash_phase = true;
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

static bool card_deco_tissue_chart_active(void)
{
    if (s_deco_vm_cache.chart_active != 0U)
    {
        return true;
    }

    for (uint8_t i = 0U; i < 16U; i++)
    {
        if (s_deco_vm_cache.tissue_raw_pct[i] != 0 ||
            s_deco_vm_cache.tissue_gf_pct[i] > 0U)
        {
            return true;
        }
    }

    return false;
}

static int tissue_draw_pct_for_range(int pct)
{
    if (pct < TISSUE_DRAW_MIN_PCT)
    {
        return TISSUE_DRAW_MIN_PCT;
    }
    if (pct > TISSUE_DRAW_MAX_PCT)
    {
        return TISSUE_DRAW_MAX_PCT;
    }
    return pct;
}

static lv_coord_t tissue_y_for_pct(const lv_area_t *plot, int pct)
{
    int draw_pct = tissue_draw_pct_for_range(pct);
    int range = TISSUE_DRAW_MAX_PCT - TISSUE_DRAW_MIN_PCT;
    int plot_h = lv_area_get_height(plot) - 1;
    return plot->y2 - (lv_coord_t)(((draw_pct - TISSUE_DRAW_MIN_PCT) * plot_h) / range);
}

static int tissue_target_gf_draw_pct(void)
{
    float target = s_deco_vm_cache.tissue_target_gf_pct;
    int pct;

    if (target <= 0.0f)
    {
        return TISSUE_TARGET_FALLBACK;
    }

    pct = (int)(target + 0.5f);
    if (pct < TISSUE_BASELINE_PCT)
    {
        pct = TISSUE_BASELINE_PCT;
    }
    if (pct > TISSUE_DRAW_MAX_PCT)
    {
        pct = TISSUE_DRAW_MAX_PCT;
    }
    return pct;
}

static void tissue_draw_line(lv_draw_ctx_t *draw_ctx, const lv_area_t *plot, int pct,
                             lv_color_t color, lv_opa_t opa, lv_coord_t width,
                             lv_coord_t dash_width, lv_coord_t dash_gap)
{
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.opa = opa;
    line_dsc.width = width;
    line_dsc.dash_width = dash_width;
    line_dsc.dash_gap = dash_gap;

    lv_coord_t y = tissue_y_for_pct(plot, pct);
    lv_point_t pts[2] = {{plot->x1, y}, {plot->x2, y}};
    lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);
}

static void tissue_draw_bar_segment(lv_draw_ctx_t *draw_ctx, lv_draw_rect_dsc_t *rect_dsc,
                                    lv_coord_t x1, lv_coord_t x2, const lv_area_t *plot,
                                    int low_pct, int high_pct, lv_color_t color)
{
    int draw_low = tissue_draw_pct_for_range(low_pct);
    int draw_high = tissue_draw_pct_for_range(high_pct);
    lv_coord_t y_top;
    lv_coord_t y_bot;
    lv_area_t fill;

    if (draw_high <= draw_low)
    {
        return;
    }

    y_top = tissue_y_for_pct(plot, draw_high);
    y_bot = tissue_y_for_pct(plot, draw_low);
    if (y_bot < y_top)
    {
        return;
    }

    fill.x1 = x1;
    fill.x2 = x2;
    fill.y1 = y_top;
    fill.y2 = y_bot;
    rect_dsc->bg_color = color;
    rect_dsc->bg_opa = LV_OPA_COVER;
    lv_draw_rect(draw_ctx, rect_dsc, &fill);
}

static void tissue_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    bool chart_active = card_deco_tissue_chart_active();
    int text_h = 16;
    int exact_col_w = lv_area_get_width(area) / 16;
    int tissue_grid_w = exact_col_w * 16;
    int target_pct = tissue_target_gf_draw_pct();
    lv_area_t plot = {area->x1, area->y1, area->x1 + tissue_grid_w - 1, area->y2 - text_h};

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0;

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = get_font(FONT_ID_SMALL);
    label_dsc.color = LIGHT;
    label_dsc.align = LV_TEXT_ALIGN_CENTER;

    for (int i = 0; i < 16; i++)
    {
        int exact_x = (int)area->x1 + i * exact_col_w;
        int bar_w = exact_col_w - 4;
        lv_coord_t x1 = (lv_coord_t)(exact_x + 2);
        lv_coord_t x2 = (lv_coord_t)(x1 + bar_w - 1);
        lv_area_t bg = {x1, plot.y1, x2, plot.y2};
        int value_pct = chart_active ? (int)s_deco_vm_cache.tissue_raw_pct[i] : 0;
        int draw_pct = tissue_draw_pct_for_range(value_pct);

        rect_dsc.bg_color = DARK;
        rect_dsc.bg_opa = LV_OPA_COVER;
        lv_draw_rect(draw_ctx, &rect_dsc, &bg);

        if (draw_pct < TISSUE_BASELINE_PCT)
        {
            tissue_draw_bar_segment(draw_ctx, &rect_dsc, x1, x2, &plot, draw_pct, TISSUE_BASELINE_PCT, lv_color_make(0x00, 0x99, 0x88));
        }
        else if (draw_pct > TISSUE_BASELINE_PCT)
        {
            int green_top = (draw_pct < target_pct) ? draw_pct : target_pct;
            int yellow_top = (draw_pct < TISSUE_MVALUE_PCT) ? draw_pct : TISSUE_MVALUE_PCT;
            if (green_top > TISSUE_BASELINE_PCT) tissue_draw_bar_segment(draw_ctx, &rect_dsc, x1, x2, &plot, TISSUE_BASELINE_PCT, green_top, GREEN);
            if (yellow_top > target_pct) tissue_draw_bar_segment(draw_ctx, &rect_dsc, x1, x2, &plot, target_pct, yellow_top, lv_color_make(0xFF, 0xD0, 0x00));
            if (draw_pct > TISSUE_MVALUE_PCT && s_tissue_flash_phase) tissue_draw_bar_segment(draw_ctx, &rect_dsc, x1, x2, &plot, TISSUE_MVALUE_PCT, draw_pct, lv_color_make(0xFF, 0x20, 0x20));
        }

        char buf[4];
        (void)snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_area_t t_area = {(lv_coord_t)exact_x, (lv_coord_t)(plot.y2 + 1), (lv_coord_t)(exact_x + exact_col_w - 1), area->y2};
        lv_draw_label(draw_ctx, &label_dsc, &t_area, buf, NULL);
    }

    tissue_draw_line(draw_ctx, &plot, TISSUE_BASELINE_PCT, GREEN, LV_OPA_COVER, 2, 0, 0);
    tissue_draw_line(draw_ctx, &plot, target_pct, lv_color_make(0xFF, 0xD0, 0x00), LV_OPA_COVER, 1, 4, 4);
    tissue_draw_line(draw_ctx, &plot, TISSUE_MVALUE_PCT, lv_color_make(0xFF, 0x20, 0x20), LV_OPA_COVER, 2, 0, 0);
}

static void surf_gf_apply_style(void)
{
    bool alert;

    if (!deco_obj_is_valid(&s_lbl_surf_gf))
    {
        return;
    }

    alert = (s_deco_vm_cache.surf_gf_alert != 0U);
    if (s_surf_gf_style_cache_valid && s_surf_gf_alert_cache == alert)
    {
        return;
    }

    s_surf_gf_alert_cache = alert;
    s_surf_gf_style_cache_valid = true;

    if (alert)
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
    s_tissue_chart = NULL;
    s_surf_gf_alert_cache = false;
    s_surf_gf_style_cache_valid = false;

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

    s_tissue_chart = lv_obj_create(parent);
    lv_obj_remove_style_all(s_tissue_chart);
    lv_obj_set_size(s_tissue_chart, chart_w, chart_h);
    lv_obj_align(s_tissue_chart, LV_ALIGN_BOTTOM_MID, 0, chart_bottom);
    lv_obj_add_event_cb(s_tissue_chart, tissue_chart_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* 缩短标题，给 16 组织仓图留出完整宽度。 */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(sec_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(sec_lbl, LIGHT, 0);
    lv_label_set_text(sec_lbl, "TISSUE SATURATION"); // 删掉了冗长的 (16 COMPARTMENTS)
    lv_obj_align_to(sec_lbl, s_tissue_chart, LV_ALIGN_OUT_TOP_LEFT, 0, -10);

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
    if (!deco_page_refresh_visible())
    {
        return;
    }

    if (deco_obj_is_valid(&s_lbl_gf_setting))
    {
        deco_label_set_text_if_changed(s_lbl_gf_setting, s_deco_vm_cache.gf_setting);
    }

    if (deco_obj_is_valid(&s_lbl_gf99))
    {
        deco_label_set_text_if_changed(s_lbl_gf99, s_deco_vm_cache.gf99);
    }

    if (deco_obj_is_valid(&s_lbl_surf_gf))
    {
        deco_label_set_text_if_changed(s_lbl_surf_gf, s_deco_vm_cache.surf_gf);
    }
    surf_gf_apply_style();

    if (deco_obj_is_valid(&s_lbl_cns))
    {
        deco_label_set_text_if_changed(s_lbl_cns, s_deco_vm_cache.cns);
    }

    if (deco_obj_is_valid(&s_lbl_otu))
    {
        deco_label_set_text_if_changed(s_lbl_otu, s_deco_vm_cache.otu);
    }

    tissue_flash_ensure();
    if (deco_obj_is_valid(&s_tissue_chart))
    {
        lv_obj_invalidate(s_tissue_chart);
    }
}
