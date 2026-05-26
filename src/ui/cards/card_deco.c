#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
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

/* Tissue bars use per-compartment GF against the raw M-value:
 * - 0..100 maps directly to bar height
 * - GF High is drawn as the stop/reference line
 * - >=100 means the raw M-value line is reached
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

void card_deco_update(void);

static bool any_tissue_danger(void)
{
    for (int i = 0; i < 16; i++)
    {
        if (g_sensor_data.tissue_pct[i] >= TISSUE_DANGER_PCT) return true;
    }
    return false;
}

static void tissue_danger_flash_cb(lv_timer_t *t)
{
    (void)t;
    s_tissue_flash_phase = !s_tissue_flash_phase;
    for (int i = 0; i < 16; i++)
    {
        if (g_sensor_data.tissue_pct[i] >= TISSUE_DANGER_PCT)
        {
            // 危险时在 亮绿 和 暗绿空槽 之间闪烁
            lv_color_t c = s_tissue_flash_phase ? GREEN : DARK;
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

static uint8_t card_deco_get_gf_high_pct(void)
{
    uint8_t gf_high = g_sensor_data.gf_high;
    if (gf_high == 0)
    {
        gf_high = 70;
    }
    if (gf_high > 100)
    {
        gf_high = 100;
    }
    return gf_high;
}

static uint8_t card_deco_get_gf_low_pct(void)
{
    uint8_t gf_low = g_sensor_data.gf_low;
    if (gf_low == 0)
    {
        gf_low = 30;
    }
    if (gf_low > 100)
    {
        gf_low = 100;
    }
    return gf_low;
}

static bool card_deco_tissue_chart_active(void)
{
    return (g_sensor_data.depth > 0.3f) || (g_sensor_data.dive_time_s > 0);
}

static void card_deco_update_mvalue_line(void)
{
    if (!s_mvalue_line || !s_mvalue_label)
    {
        return;
    }

    int bar_max_h = (int)lv_obj_get_height(s_bars[0]);
    int line_y = bar_max_h;
    uint8_t gf_high = card_deco_get_gf_high_pct();

    line_y -= ((int)gf_high * bar_max_h) / 100;
    if (line_y < 0)
    {
        line_y = 0;
    }
    if (line_y > bar_max_h - 2)
    {
        line_y = bar_max_h - 2;
    }

    lv_obj_set_y(s_mvalue_line, line_y);
    lv_label_set_text_fmt(s_mvalue_label, "M-VALUE %u%%", gf_high);
    lv_obj_align_to(s_mvalue_label, s_mvalue_line, LV_ALIGN_OUT_TOP_RIGHT, -4, -2);
}

static void surf_gf_apply_style(void)
{
    if (g_sensor_data.surf_gf > 100.0f)
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

    int right_canvas_w = (int)g_sys_config.safe_zone_w - 160 - (int)(g_sys_config.gap_u * 10);

    make_grid_row(parent, DECO_CONTENT_Y,
                  "ALGORITHM", "ZHL-16C", NULL,
                  "GF LOW / HIGH", "-- / --", &s_lbl_gf_setting,
                  true, right_canvas_w - 15);

    make_grid_row(parent, DECO_ROW2_Y,
                  "GF99", "--", &s_lbl_gf99,
                  "SurfGF", "--", &s_lbl_surf_gf,
                  true, right_canvas_w - 15);

    make_grid_row(parent, DECO_ROW3_Y,
                  "CNS O2", "--%", &s_lbl_cns,
                  "OTU", "--", &s_lbl_otu,
                  false, right_canvas_w - 15);

    int chart_w = right_canvas_w - 40;     /* 增加左右安全边距防截断 */
    int chart_h = 120;                     /* 固定 120px */

    lv_obj_t *chart_container = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_size(chart_container, chart_w, chart_h);
    lv_obj_align(chart_container, LV_ALIGN_BOTTOM_MID, 0, -15);

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
    char buf[16];
    uint8_t gf_low = card_deco_get_gf_low_pct();
    uint8_t gf_high = card_deco_get_gf_high_pct();
    bool chart_active = card_deco_tissue_chart_active();

    if (s_lbl_gf_setting)
    {
        lv_label_set_text_fmt(s_lbl_gf_setting, "%u / %u", gf_low, gf_high);
    }

    snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.gf99);
    lv_label_set_text(s_lbl_gf99, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.surf_gf);
    lv_label_set_text(s_lbl_surf_gf, buf);
    surf_gf_apply_style();

    snprintf(buf, sizeof(buf), "%d%%", g_sensor_data.cns_pct);
    lv_label_set_text(s_lbl_cns, buf);

    snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu);
    lv_label_set_text(s_lbl_otu, buf);

    card_deco_update_mvalue_line();

    tissue_flash_ensure();

    for (int i = 0; i < 16; i++)
    {
        uint8_t pct = g_sensor_data.tissue_pct[i];

        lv_obj_t *bar_fill = lv_obj_get_child(s_bars[i], 0);
        if (bar_fill)
        {
            int bar_max_h = (int)lv_obj_get_height(s_bars[i]);
            int fill_h = 0;
            int fill_pct = (int)pct;

            if (!chart_active)
            {
                fill_pct = 0;
            }
            else if (fill_pct < 0)
            {
                fill_pct = 0;
            }
            if (fill_pct > 100)
            {
                fill_pct = 100;
            }

            if (fill_pct > 0)
            {
                fill_h = (fill_pct * bar_max_h) / 100;
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
            lv_obj_set_style_bg_color(bar_fill, tissue_fill_color(pct, gf_high), 0);
        }
    }
}
