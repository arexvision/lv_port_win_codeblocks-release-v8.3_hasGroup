#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================
   4F: DIVE PLAN TRACK
   规范参数：
     外壳：2px 实线 AREX_DARK，Padding 10px
     画布：400×320px
     背景网格：横间距 53px，纵间距 64px，线宽 1px，透明度 20%（opa≈51）
     坐标轴文字：12px，规范无此字号→用 AREX_FONT_SMALL(14px)，透明度 75%（opa≈191）
     走势线（Past）：实线，粗细 4px，AREX_GREEN
     计划线（Future）：虚线，粗细 4px，AREX_GREEN
     停留点：R=6px，填充黑，描边 2px AREX_GREEN
   ============================================================ */

#define CHART_PAD   10
#define CHART_W     400    /* 规范 */
#define CHART_H     320    /* 规范 */
#define CHART_X     16     /* tile 内 x = 16 */
#define CHART_Y     50     /* tile 内 y = 50（标题下方） */

/* 画布原点（左下）对应：时间 0min，深度 0m
   X轴：0~55min → 映射到 0~CHART_W
   Y轴：0~50m   → 映射到 0~CHART_H */
#define PLOT_T_MAX  55     /* 分钟 */
#define PLOT_D_MAX  50     /* 米 */

static lv_color_t s_cbuf[CHART_W * CHART_H];
static lv_obj_t  *s_canvas;

/* Demo dive profile points (time_min, depth_m) */
static const int16_t s_profile[][2] = {
    {0,  0}, {2, 15}, {5, 30}, {10, 45}, {30, 45},
    {33, 30}, {36, 21}, {40, 21}, {43, 9}, {48, 9},
    {50, 6},  {53, 6},  {55, 0},
};
#define PROFILE_LEN 13

/* 原点 → 画布像素坐标（Y轴翻转：深度大为上方） */
static inline lv_coord_t plot_x(int t) { return (lv_coord_t)(t * CHART_W / PLOT_T_MAX); }
static inline lv_coord_t plot_y(int d) { return (lv_coord_t)(d * CHART_H / PLOT_D_MAX); }

static void draw_plan(void)
{
    lv_canvas_fill_bg(s_canvas, lv_color_make(0,0,0), LV_OPA_COVER);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    lv_draw_label_dsc_t t;
    lv_draw_label_dsc_init(&t);
    t.font  = AREX_FONT_SMALL;
    t.color = lv_color_make(0x00,0xFF,0x00);
    t.opa   = 191; /* 规范：透明度 75% */

    /* 背景网格 — 规范：横间距 53px，纵间距 64px，透明度 20%（opa≈51） */
    l.color = lv_color_make(0x00,0xFF,0x00);
    l.width = 1;
    l.opa   = 51; /* 20% opacity */

    /* 横向网格线（深度刻度） */
    for (int d = 0; d <= PLOT_D_MAX; d += 10) {
        lv_coord_t y = (lv_coord_t)(d * CHART_H / PLOT_D_MAX);
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{0,y},{CHART_W,y}}, 2, &l);
        char buf[8]; snprintf(buf,sizeof(buf),"%dm",d);
        lv_canvas_draw_text(s_canvas, 2, y+2, 36, &t, buf);
    }
    /* 纵向网格线（时间刻度）— 规范：间距 53px */
    for (int m = 0; m <= PLOT_T_MAX; m += 5) {
        lv_coord_t x = (lv_coord_t)(m * CHART_W / PLOT_T_MAX);
        if (x > CHART_W) x = CHART_W;
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{x,0},{x,CHART_H}}, 2, &l);
        char buf[8]; snprintf(buf,sizeof(buf),"%d'",m);
        lv_canvas_draw_text(s_canvas, x+2, CHART_H-18, 30, &t, buf);
    }

    /* 潜水走势线 — 规范：实线，粗细 4px，AREX_GREEN */
    l.color = lv_color_make(0x00,0xFF,0x00);
    l.width = 4;
    l.opa   = 255;
    for (int i = 1; i < PROFILE_LEN; i++) {
        lv_coord_t x1 = plot_x(s_profile[i-1][0]);
        lv_coord_t y1 = CHART_H - plot_y(s_profile[i-1][1]); /* 翻转Y */
        lv_coord_t x2 = plot_x(s_profile[i  ][0]);
        lv_coord_t y2 = CHART_H - plot_y(s_profile[i  ][1]);
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{x1,y1},{x2,y2}}, 2, &l);
    }

    /* 停留点标记 — 规范：R=6px，填充黑，描边 2px AREX_GREEN */
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color  = lv_color_make(0x00,0x00,0x00);
    r.border_color = lv_color_make(0x00,0xFF,0x00);
    r.border_width = 2;
    r.radius   = LV_RADIUS_CIRCLE;

    for (int i = 1; i < PROFILE_LEN; i++) {
        int t0 = s_profile[i-1][0];
        int t1 = s_profile[i  ][0];
        /* 停留点：在水平段中点（仅当 t1 - t0 >= 3） */
        if (s_profile[i][1] != 0 && (t1 - t0) >= 3) {
            int tm = (t0 + t1) / 2;
            lv_coord_t cx = plot_x(tm);
            lv_coord_t cy = CHART_H - plot_y(s_profile[i][1]);
            lv_canvas_draw_rect(s_canvas, cx - 6, cy - 6, 12, 12, &r);
        }
    }

    /* 当前潜水位置 — 黄色圆点（动态计算） */
    float dive_min = g_sensor_data.dive_time_s / 60.0f;
    if (dive_min > PLOT_T_MAX) dive_min = PLOT_T_MAX;
    lv_coord_t cx = (lv_coord_t)(dive_min * CHART_W / PLOT_T_MAX);
    lv_coord_t cy = CHART_H - (lv_coord_t)(g_sensor_data.depth * CHART_H / PLOT_D_MAX);
    lv_draw_rect_dsc_t nd; lv_draw_rect_dsc_init(&nd);
    nd.bg_color = lv_color_make(0xFF,0xFF,0x00);
    nd.radius   = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_canvas, cx - 5, cy - 5, 10, 10, &nd);

    /* "NOW" 标签 */
    lv_draw_label_dsc_t nl; lv_draw_label_dsc_init(&nl);
    nl.font  = AREX_FONT_SMALL;
    nl.color = lv_color_make(0x00,0xFF,0x00);
    nl.opa   = 255;
    lv_canvas_draw_text(s_canvas, cx+8, cy-8, 40, &nl, "NOW");
}

void card_plan_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "4F: DIVE PLAN TRACK");

    /* 外壳边框 — 规范：2px 实线 AREX_DARK */
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_set_size(shell, CHART_W + CHART_PAD*2, CHART_H + CHART_PAD*2);
    lv_obj_set_pos(shell, CHART_X - CHART_PAD, CHART_Y - CHART_PAD);
    lv_obj_set_style_bg_color(shell, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, AREX_DARK, 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_radius(shell, 0, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_cbuf, CHART_W, CHART_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, CHART_X, CHART_Y);

    draw_plan();
}

void card_plan_update(void)
{
    draw_plan();
}
