#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <math.h>

/* ============================================================
   4F: DIVE PLAN TRACK — Zero-RAM Renderer
   100% 像素级复刻 HTML 原型逻辑：
     外壳：2px 实线 AREX_DARK，Padding 10px
     画布：400×320px（实际绘图区：X轴45~385，Y轴15~295）
     背景网格：虚线，透明度 15%（opa=38）
     坐标轴文字：AREX_FONT_ID_SMALL，透明度 75%（opa=191）
     历史轨迹（Past）：实线，粗细 3px，AREX_GREEN
     计划轨迹（Future）：自定义插值斜虚线，粗细 3px，AREX_GREEN
     NOW 标记：绿色实心圆 + 黑字绿底背景
     减压停留点：空心圆 + 深度停留文字
   ============================================================ */

#define CHART_PAD   10
#define CHART_W     400
#define CHART_H     320
#define CHART_X     16
#define CHART_Y     50

/* ============================================================
 * 数据总线（与 HTML diveLog / mockStops 完全对应）
 * ============================================================ */
typedef struct { float time_min; float depth_m; } arex_dive_pt_t;
typedef struct { float depth_m; float stay_min; } arex_deco_stop_t;

#define MAX_DIVE_LOG   100
#define MAX_DECO_STOPS 10

arex_dive_pt_t   g_dive_log[MAX_DIVE_LOG];
uint16_t         g_dive_log_count;

arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
uint16_t         g_deco_stop_count;

/* 初始化模拟数据：刚下水不久，在 13m 处 */
static void init_test_data(void)
{
    /* g_sensor_data.dive_time_s 和 g_sensor_data.depth
     * 由 UI_main.c 的 sim_tick_cb 实时模拟，此处仅初始化曲线数据 */

    /* 过去的日志：只存了 2 个点，说明刚下水 */
    g_dive_log_count = 2;
    g_dive_log[0] = (arex_dive_pt_t){0.0f,  0.0f};
    g_dive_log[1] = (arex_dive_pt_t){0.5f, 10.0f}; /* 0.5 分钟时在 10m */

    /* 算法预测的未来减压路线：需要在 3m 停留 3 分钟 */
    g_deco_stop_count = 1;
    g_deco_stops[0] = (arex_deco_stop_t){.depth_m = 3.0f, .stay_min = 3.0f};
}

/* ============================================================
 * 自定义斜虚线绘制（绕过 LVGL 软件渲染不支持斜虚线的限制）
 * 通过数学插值把一条长斜线切成无数段小实线
 * ============================================================ */
static void draw_diagonal_dashed_line(lv_draw_ctx_t *draw_ctx,
                                      lv_draw_line_dsc_t *dsc,
                                      lv_point_t p1, lv_point_t p2)
{
    int dx = p2.x - p1.x;
    int dy = p2.y - p1.y;
    float dist = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);
    if (dist < 1.0f) return;

    float dash_len = 6.0f;  /* 虚线实部长 */
    float gap_len  = 5.0f;  /* 虚线空白部长 */
    float step     = dash_len + gap_len;
    int num_steps  = (int)ceilf(dist / step);

    /* 复制原画笔，关闭原生虚线属性，用纯实线一截一截画 */
    lv_draw_line_dsc_t solid_dsc = *dsc;
    solid_dsc.dash_width = 0;
    solid_dsc.dash_gap   = 0;

    for (int i = 0; i < num_steps; i++) {
        float t1 = (i * step) / dist;
        float t2 = (i * step + dash_len) / dist;
        if (t1 > 1.0f) break;
        if (t2 > 1.0f) t2 = 1.0f;

        lv_point_t seg_p1 = {
            p1.x + (lv_coord_t)(dx * t1),
            p1.y + (lv_coord_t)(dy * t1)
        };
        lv_point_t seg_p2 = {
            p1.x + (lv_coord_t)(dx * t2),
            p1.y + (lv_coord_t)(dy * t2)
        };

        lv_draw_line(draw_ctx, &solid_dsc, &seg_p1, &seg_p2);
    }
}

/* ============================================================
 * 零内存高阶渲染引擎
 * ============================================================ */
static void plan_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;

    /* 等待下水 */
    if (g_sensor_data.depth <= 0.5f && g_dive_log_count == 0) {
        return;
    }

    /* 1. 同步 HTML 的边距参数 */
    float pad_x      = 45.0f;
    float pad_y_top  = 15.0f;
    float pad_y_bot  = 25.0f;
    float pad_right  = 15.0f;
    float chart_w    = 400.0f;
    float chart_h    = 320.0f;
    float w           = chart_w - pad_x - pad_right;
    float h           = chart_h - pad_y_top - pad_y_bot;

    /* 当前时间和深度 */
    float current_t = (float)g_sensor_data.dive_time_s / 60.0f;
    float current_d  = g_sensor_data.depth;

    /* 2. 预测未来总时间 (HTML 逻辑复刻: 每 10m/min 升水速度) */
    float predicted_t = current_t;
    float sim_d = current_d;
    for (int i = 0; i < g_deco_stop_count; i++) {
        float asc_t = (sim_d > g_deco_stops[i].depth_m)
                      ? (sim_d - g_deco_stops[i].depth_m) * 0.1f : 0;
        predicted_t += asc_t;
        predicted_t += g_deco_stops[i].stay_min;
        sim_d = g_deco_stops[i].depth_m;
    }
    predicted_t += (sim_d > 0) ? sim_d * 0.1f : 0;

    /* 3. 动态扩展 Y 轴 (HTML 逻辑复刻) */
    float max_log_d = current_d;
    for (int i = 0; i < g_dive_log_count; i++) {
        if (g_dive_log[i].depth_m > max_log_d) max_log_d = g_dive_log[i].depth_m;
    }
    float max_d_axis = 60.0f; /* 初始 60m */
    if (max_log_d >= max_d_axis * 0.9f) {
        max_d_axis = ceilf((max_log_d + 15.0f) / 20.0f) * 20.0f;
    }

    /* 4. 动态扩展 X 轴（极度弹性：早期放大视图，最小 5 分钟跨度） */
    float target_max_t = (current_t > predicted_t ? current_t : predicted_t) * 1.1f;

    float max_t_axis;
    int x_step;
    if (target_max_t <= 5.0f) {
        max_t_axis = 5.0f;  x_step = 1;        /* 0, 1', 2'... */
    } else if (target_max_t <= 10.0f) {
        max_t_axis = 10.0f; x_step = 2;        /* 0, 2', 4'... */
    } else if (target_max_t <= 20.0f) {
        max_t_axis = ceilf(target_max_t / 10.0f) * 10.0f;
        x_step = 5;                            /* 0, 5', 10'... */
    } else if (target_max_t <= 60.0f) {
        max_t_axis = ceilf(target_max_t / 10.0f) * 10.0f;
        x_step = 10;
    } else {
        max_t_axis = ceilf(target_max_t / 30.0f) * 30.0f;
        x_step = 30;
    }

    /* 5. 映射宏 */
    #define MAP_X(t) (area->x1 + (lv_coord_t)(pad_x + ((t) / max_t_axis) * w))
    #define MAP_Y(d) (area->y1 + (lv_coord_t)(pad_y_top + ((d) / max_d_axis) * h))

    /* 初始化通用画笔 */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_t txt_dsc;
    lv_draw_label_dsc_init(&txt_dsc);
    txt_dsc.font = arex_get_font(AREX_FONT_ID_SMALL);
    txt_dsc.color = AREX_GREEN;

    /* ==========================================
     * 绘制 Y 轴网格 (深度)
     * ========================================== */
    int y_step = (max_d_axis >= 200.0f) ? 50 : ((max_d_axis >= 100.0f) ? 20 : 10);
    line_dsc.color = AREX_GREEN;
    line_dsc.width = 1;
    line_dsc.opa = 38;   /* 15% 透明度 (HTML 0.15) */
    line_dsc.dash_width = 4;
    line_dsc.dash_gap   = 4;

    for (int d = 0; d <= (int)max_d_axis; d += y_step) {
        lv_coord_t y = MAP_Y((float)d);
        lv_point_t pts[2] = {
            {area->x1 + (lv_coord_t)pad_x,             y},
            {area->x1 + (lv_coord_t)(chart_w - pad_right), y}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%dm", d);
        txt_dsc.opa = 191; /* 75% 文本透明度 */
        lv_area_t t_area = {area->x1, y - 10, area->x1 + (lv_coord_t)pad_x - 5, y + 10};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* ==========================================
     * 绘制 X 轴网格 (时间)
     * ========================================== */
    for (int t = 0; t <= (int)max_t_axis; t += x_step) {
        lv_coord_t x = MAP_X((float)t);
        lv_point_t pts[2] = {
            {x, area->y1 + (lv_coord_t)pad_y_top},
            {x, area->y1 + (lv_coord_t)(chart_h - pad_y_bot)}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[12];
        if (max_t_axis >= 60.0f) snprintf(buf, sizeof(buf), "%d'", t);
        else                      snprintf(buf, sizeof(buf), "%ds", t);
        lv_area_t t_area = {x - 20, area->y2 - 18, x + 20, area->y2};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* ==========================================
     * 绘制历史真实轨迹 (实线)
     * ========================================== */
    line_dsc.opa       = LV_OPA_COVER;
    line_dsc.width     = 3;
    line_dsc.dash_width = 0;
    line_dsc.dash_gap   = 0;  /* 恢复实线 */

    if (g_dive_log_count > 0) {
        lv_point_t last_p = {
            MAP_X(g_dive_log[0].time_min),
            MAP_Y(g_dive_log[0].depth_m)
        };
        for (int i = 1; i < g_dive_log_count; i++) {
            lv_point_t next_p = {
                MAP_X(g_dive_log[i].time_min),
                MAP_Y(g_dive_log[i].depth_m)
            };
            lv_draw_line(draw_ctx, &line_dsc, &last_p, &next_p);
            last_p = next_p;
        }

        /* 连到当前 NOW 点 */
        lv_point_t now_p = {MAP_X(current_t), MAP_Y(current_d)};
        lv_draw_line(draw_ctx, &line_dsc, &last_p, &now_p);

        /* 绘制 NOW 点 (绿色实心圆) */
        lv_draw_rect_dsc_t now_dsc;
        lv_draw_rect_dsc_init(&now_dsc);
        now_dsc.radius   = LV_RADIUS_CIRCLE;
        now_dsc.bg_color = AREX_GREEN;
        now_dsc.bg_opa   = LV_OPA_COVER;
        lv_area_t now_area = {now_p.x - 6, now_p.y - 6, now_p.x + 6, now_p.y + 6};
        lv_draw_rect(draw_ctx, &now_dsc, &now_area);

        /* NOW 文字背景 (绿色矩形) */
        lv_draw_rect_dsc_t now_bg;
        lv_draw_rect_dsc_init(&now_bg);
        now_bg.bg_color = AREX_GREEN;
        now_bg.bg_opa  = LV_OPA_COVER;
        lv_area_t now_bg_area = {now_p.x + 10, now_p.y - 8, now_p.x + 46, now_p.y + 8};
        lv_draw_rect(draw_ctx, &now_bg, &now_bg_area);

        /* NOW 文字 (黑字) */
        txt_dsc.color = AREX_BLACK;
        txt_dsc.opa   = LV_OPA_COVER;
        lv_area_t now_txt_area = {now_p.x + 10, now_p.y - 8, now_p.x + 46, now_p.y + 8};
        lv_draw_label(draw_ctx, &txt_dsc, &now_txt_area, "NOW", NULL);
        txt_dsc.color = AREX_GREEN; /* 恢复绿字 */

        /* ==========================================
         * 绘制预测减压轨迹 (斜虚线插值)
         * ========================================== */
        if (g_deco_stop_count > 0) {
            line_dsc.opa = LV_OPA_COVER;
            line_dsc.width = 3;

            float cur_t = current_t;
            float draw_d = current_d;
            lv_point_t p1 = now_p;

            for (int i = 0; i < g_deco_stop_count; i++) {
                /* 上升段（斜线）— 使用自定义斜虚线插值 */
                float asc_t = (draw_d > g_deco_stops[i].depth_m)
                              ? (draw_d - g_deco_stops[i].depth_m) * 0.1f : 0;
                cur_t += asc_t;
                lv_point_t p2 = {MAP_X(cur_t), MAP_Y(g_deco_stops[i].depth_m)};
                draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p2);

                /* 停留段（水平线）— 同样使用斜虚线函数 */
                float hold_t = g_deco_stops[i].stay_min;
                cur_t += hold_t;
                lv_point_t p3 = {MAP_X(cur_t), MAP_Y(g_deco_stops[i].depth_m)};
                draw_diagonal_dashed_line(draw_ctx, &line_dsc, p2, p3);

                /* 绘制停留点空心圆 */
                lv_coord_t circle_x = MAP_X(cur_t - hold_t / 2.0f);
                lv_draw_rect_dsc_t deco_node;
                lv_draw_rect_dsc_init(&deco_node);
                deco_node.radius        = LV_RADIUS_CIRCLE;
                deco_node.bg_color     = AREX_BLACK;
                deco_node.bg_opa      = LV_OPA_COVER;
                deco_node.border_color = AREX_GREEN;
                deco_node.border_width = 2;
                deco_node.border_opa  = 128;
                lv_area_t c_area = {circle_x - 4, p2.y - 4, circle_x + 4, p2.y + 4};
                lv_draw_rect(draw_ctx, &deco_node, &c_area);

                /* 停留点文字 */
                char d_buf[16];
                snprintf(d_buf, sizeof(d_buf), "%dm %d'",
                         (int)g_deco_stops[i].depth_m, (int)g_deco_stops[i].stay_min);
                lv_area_t d_txt = {circle_x - 20, p2.y - 20, circle_x + 20, p2.y - 4};
                txt_dsc.opa = LV_OPA_COVER;
                lv_draw_label(draw_ctx, &txt_dsc, &d_txt, d_buf, NULL);
                txt_dsc.opa = 191; /* 恢复透明度 */

                p1 = p3;
                draw_d = g_deco_stops[i].depth_m;
            }
            /* 升水面段（斜线）— 使用自定义斜虚线插值 */
            cur_t += (draw_d > 0) ? draw_d * 0.1f : 0;
            lv_point_t p_end = {MAP_X(cur_t), MAP_Y(0.0f)};
            draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p_end);
        }
    }

    #undef MAP_X
    #undef MAP_Y
}

/* ============================================================
 * 卡片创建
 * ============================================================ */
static lv_obj_t *s_chart_obj;

void card_plan_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "4F: DIVE PLAN TRACK");

    /* 初始化测试数据 */
    init_test_data();

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
