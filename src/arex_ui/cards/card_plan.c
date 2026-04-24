#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>

/* ============================================================
   4F: DIVE PLAN TRACK — Zero-RAM Renderer
   规范参数：
     外壳：2px 实线 AREX_DARK，Padding 10px
     画布：400×320px
     背景网格：横间距 53px，纵间距 64px，线宽 1px，透明度 20%（opa≈51）
     坐标轴文字：AREX_FONT_ID_SMALL(14px)，透明度 75%（opa≈191）
     走势线（Past）：实线，粗细 4px，AREX_GREEN
     计划线（Future）：虚线，粗细 4px，AREX_GREEN
     停留点：R=6px，填充黑，描边 2px AREX_GREEN
   ============================================================ */

#define CHART_PAD   10
#define CHART_W     400
#define CHART_H     320
#define CHART_X     16
#define CHART_Y     50

#define PLOT_T_MAX  55
#define PLOT_D_MAX  50

static lv_obj_t *s_chart_obj;

/* Demo dive profile — 后台算法通过 g_sensor_data.plan_points 写入，测试用硬编码 */
static const int16_t s_profile[][2] = {
    {0,  0}, {2, 15}, {5, 30}, {10, 45}, {30, 45},
    {33, 30}, {36, 21}, {40, 21}, {43, 9}, {48, 9},
    {50, 6},  {53, 6},  {55, 0},
};
#define PROFILE_LEN 13

#define PLOT_X(area, t) ((area)->x1 + (lv_coord_t)((t) * CHART_W / PLOT_T_MAX))
#define PLOT_Y(area, d) ((area)->y1 + (lv_coord_t)((d) * CHART_H / PLOT_D_MAX))

static void plan_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;

    /* 状态机：深度 <= 0.5m，显示等待文本 */
    if (g_sensor_data.depth <= 0.5f) {
        lv_draw_label_dsc_t wait_dsc;
        lv_draw_label_dsc_init(&wait_dsc);
        wait_dsc.color = AREX_LIGHT;
        wait_dsc.font  = arex_get_font(AREX_FONT_ID_MEDIUM);
        lv_draw_label(draw_ctx, &wait_dsc, area, "WAITING FOR DIVE > 0.5M...", NULL);
        return;
    }

    /* 初始化画笔 */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = AREX_GREEN;
    line_dsc.width = 1;
    line_dsc.opa   = 51;

    lv_draw_rect_dsc_t node_dsc;
    lv_draw_rect_dsc_init(&node_dsc);
    node_dsc.radius       = LV_RADIUS_CIRCLE;
    node_dsc.border_color  = AREX_GREEN;
    node_dsc.border_width  = 2;
    node_dsc.bg_color     = AREX_BLACK;
    node_dsc.bg_opa       = LV_OPA_COVER;

    lv_draw_label_dsc_t txt_dsc;
    lv_draw_label_dsc_init(&txt_dsc);
    txt_dsc.font  = arex_get_font(AREX_FONT_ID_SMALL);
    txt_dsc.color = AREX_GREEN;
    txt_dsc.opa   = 191;

    /* 横向网格（深度刻度，每 10m） */
    for (int d = 0; d <= PLOT_D_MAX; d += 10) {
        lv_coord_t y = PLOT_Y(area, d);
        lv_point_t pts[2] = {{area->x1, y}, {area->x2, y}};
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%dm", d);
        lv_area_t t_area = {area->x1 + 2, y - 16, area->x1 + 42, y};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* 纵向网格（时间刻度，每 5min） */
    for (int m = 0; m <= PLOT_T_MAX; m += 5) {
        lv_coord_t x = PLOT_X(area, m);
        lv_point_t pts[2] = {{x, area->y1}, {x, area->y2}};
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d'", m);
        lv_area_t t_area = {x + 2, area->y2 - 18, x + 30, area->y2};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* 潜水走势线 — 动态切分实线/虚线 */
    line_dsc.width = 4;
    line_dsc.opa   = LV_OPA_COVER;

    float dive_min = g_sensor_data.dive_time_s / 60.0f;
    if (dive_min > PLOT_T_MAX) dive_min = PLOT_T_MAX;

    for (int i = 1; i < PROFILE_LEN; i++) {
        lv_coord_t x1 = PLOT_X(area, s_profile[i - 1][0]);
        lv_coord_t y1 = PLOT_Y(area, s_profile[i - 1][1]);
        lv_coord_t x2 = PLOT_X(area, s_profile[i][0]);
        lv_coord_t y2 = PLOT_Y(area, s_profile[i][1]);

        /* 当前时刻之后的段画虚线 */
        if ((float)s_profile[i - 1][0] >= dive_min) {
            /* 未来计划线：尝试虚线，若渲染器不支持斜线虚线则降级为半透明 */
            line_dsc.dash_width = 8;
            line_dsc.dash_gap   = 6;
            line_dsc.opa        = LV_OPA_COVER;
            if (x2 <= x1 || y2 <= y1) {
                line_dsc.dash_width = 0;
                line_dsc.dash_gap   = 0;
                line_dsc.opa        = LV_OPA_50;
            }
        } else {
            line_dsc.dash_width = 0;
            line_dsc.dash_gap   = 0;
            line_dsc.opa        = LV_OPA_COVER;
        }

        lv_point_t pts[2] = {{x1, y1}, {x2, y2}};
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);
    }

    /* 停留点（水平段中点，R=6px 空心圆） */
    for (int i = 1; i < PROFILE_LEN; i++) {
        int t0 = s_profile[i - 1][0];
        int t1 = s_profile[i][0];
        if (s_profile[i][1] != 0 && (t1 - t0) >= 3) {
            int tm = (t0 + t1) / 2;
            lv_coord_t cx = PLOT_X(area, tm);
            lv_coord_t cy = PLOT_Y(area, s_profile[i][1]);
            lv_area_t node_area = {cx - 6, cy - 6, cx + 6, cy + 6};
            lv_draw_rect(draw_ctx, &node_dsc, &node_area);
        }
    }

    /* NOW 当前点 — 黄色实心圆 */
    if (dive_min > 0) {
        lv_coord_t nx = PLOT_X(area, (int)dive_min);
        lv_coord_t ny = PLOT_Y(area, (int)g_sensor_data.depth);
        node_dsc.bg_color    = lv_color_hex(0xFFFF00);
        node_dsc.border_width = 0;
        lv_area_t now_area = {nx - 5, ny - 5, nx + 5, ny + 5};
        lv_draw_rect(draw_ctx, &node_dsc, &now_area);
        node_dsc.border_width = 2;

        txt_dsc.color = lv_color_hex(0xFFFF00);
        txt_dsc.opa   = LV_OPA_COVER;
        lv_area_t now_txt = {nx + 10, ny - 8, nx + 52, ny + 10};
        lv_draw_label(draw_ctx, &txt_dsc, &now_txt, "NOW", NULL);
    }
}

void card_plan_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "4F: DIVE PLAN TRACK");

    /* 外壳边框 — 规范：2px 实线 AREX_DARK */
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_set_size(shell, CHART_W + CHART_PAD * 2, CHART_H + CHART_PAD * 2);
    lv_obj_set_pos(shell, CHART_X - CHART_PAD, CHART_Y - CHART_PAD);
    lv_obj_set_style_bg_color(shell, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, AREX_DARK, 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_radius(shell, 0, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);

    /* 零 RAM 画板对象 */
    s_chart_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_chart_obj);
    lv_obj_set_size(s_chart_obj, CHART_W, CHART_H);
    lv_obj_set_pos(s_chart_obj, CHART_X, CHART_Y);
    lv_obj_add_event_cb(s_chart_obj, plan_chart_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

void card_plan_update(void)
{
    if (s_chart_obj) {
        lv_obj_invalidate(s_chart_obj);
    }
}

