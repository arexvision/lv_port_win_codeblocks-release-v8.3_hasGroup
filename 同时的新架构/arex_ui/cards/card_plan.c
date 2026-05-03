#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <math.h>

/* 图表布局常量（完全动态化，以标题区为绝对 Y=0 起点）
 * 宽高由 tile 尺寸动态推算，绝不使用魔法数字。
 * 图表 Y 起点 = AREX_CARD_TITLE_H（标题下方）
 * 图表高度 = tile_h - AREX_CARD_TITLE_H - 底部预留
 * 图表宽度 = tile_w - 左右 padding */
#define CHART_PAD   10

/* ============================================================
 * 潜水轨迹与减压停留（定义，共享给 arex_dive_log_append 追加点）
 * ============================================================ */
arex_dive_pt_t   g_dive_log[MAX_DIVE_LOG];
uint16_t         g_dive_log_count;
arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
uint16_t         g_deco_stop_count;

/* ============================================================
 * 历史轨迹推流接口（供 arex_data.h 导出，外部 1Hz 定时器调用）
 * ============================================================ */
void arex_dive_log_append(float current_time_s, float current_depth_m)
{
    if (g_dive_log_count < MAX_DIVE_LOG) {
        g_dive_log[g_dive_log_count].time_s   = current_time_s;
        g_dive_log[g_dive_log_count].depth_m  = current_depth_m;
        g_dive_log_count++;
    }
}

/* ============================================================
 * 初始化模拟数据：清空历史轨迹，让它从零开始生长
 * ============================================================ */
static void init_test_data(void)
{
    /* 清空历史轨迹，每次启动都是从零生长 */
    g_dive_log_count = 0;

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

    float dash_len = 6.0f;
    float gap_len  = 5.0f;
    float step     = dash_len + gap_len;
    int num_steps  = (int)ceilf(dist / step);

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
 * 4F: DIVE PLAN TRACK — 秒级坐标引擎
 * 100% 秒 (Seconds) 为底层单位，重现 HTML "0s, 10s, 20s" 放大效果
 *   外壳：2px 实线 AREX_DARK，Padding 10px
 *   画布：400×320px（实际绘图区：X轴45~385，Y轴15~295）
 *   背景网格：虚线，透明度 15%（opa=38）
 *   坐标轴文字：AREX_FONT_ID_SMALL，透明度 75%（opa=191）
 *   历史轨迹（Past）：实线，粗细 3px，AREX_GREEN
 *   计划轨迹（Future）：自定义插值斜虚线，粗细 3px，AREX_GREEN
 *   NOW 标记：绿色实心圆 + 黑字绿底背景
 *   减压停留点：空心圆 + 深度停留文字
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

    /* 1. 同步 HTML 边距参数（基于实际画布尺寸动态计算） */
    float pad_x      = 45.0f;
    float pad_y_top  = 15.0f;
    float pad_y_bot  = 25.0f;
    float pad_right  = 15.0f;
    float chart_w    = (float)(area->x2 - area->x1);
    float chart_h    = (float)(area->y2 - area->y1);
    float w          = chart_w - pad_x - pad_right;
    float h          = chart_h - pad_y_top - pad_y_bot;

    /* 2. 核心：全量提取为"秒"进行推演 */
    float current_t_sec = (float)g_sensor_data.dive_time_s;
    float current_d     = g_sensor_data.depth;

    /* 预测所需总时间（基于 6秒/米 的升水速度，即 10m/min） */
    float predicted_t_sec = current_t_sec;
    float sim_d = current_d;
    for (int i = 0; i < g_deco_stop_count; i++) {
        float asc_t = (sim_d > g_deco_stops[i].depth_m)
                      ? (sim_d - g_deco_stops[i].depth_m) * 6.0f : 0;
        predicted_t_sec += asc_t;
        predicted_t_sec += g_deco_stops[i].stay_min * 60.0f;
        sim_d = g_deco_stops[i].depth_m;
    }
    predicted_t_sec += (sim_d > 0) ? sim_d * 6.0f : 0;

    /* 3. 动态扩展 Y 轴 */
    float max_log_d = current_d;
    for (int i = 0; i < g_dive_log_count; i++) {
        if (g_dive_log[i].depth_m > max_log_d) max_log_d = g_dive_log[i].depth_m;
    }
    float max_d_axis = 60.0f;
    if (max_log_d >= max_d_axis * 0.9f) {
        max_d_axis = ceilf((max_log_d + 15.0f) / 20.0f) * 20.0f;
    }

    /* 4. 动态扩展 X 轴（单位：秒） */
    float target_max_t_sec = fmaxf(current_t_sec, predicted_t_sec) * 1.05f;
    if (target_max_t_sec < 20.0f) target_max_t_sec = 20.0f;

    float max_t_axis_sec = 20.0f;
    if (target_max_t_sec > 120.0f)
        max_t_axis_sec = ceilf(target_max_t_sec / 60.0f) * 60.0f;
    else
        max_t_axis_sec = ceilf(target_max_t_sec / 10.0f) * 10.0f;

    /* X轴步长动态分配（秒） */
    int x_step = 10;
    if (max_t_axis_sec > 60)    x_step = 15;
    if (max_t_axis_sec > 120)   x_step = 30;
    if (max_t_axis_sec > 300)   x_step = 60;
    if (max_t_axis_sec > 600)   x_step = 120;
    if (max_t_axis_sec > 1200)  x_step = 300;
    if (max_t_axis_sec > 3600)  x_step = 600;

    /* 5. 秒数映射宏 */
    #define MAP_X(t_sec) (area->x1 + (lv_coord_t)(pad_x + ((t_sec) / max_t_axis_sec) * w))
    #define MAP_Y(d)     (area->y1 + (lv_coord_t)(pad_y_top + ((d) / max_d_axis) * h))

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_t txt_dsc;
    lv_draw_label_dsc_init(&txt_dsc);
    txt_dsc.font = arex_get_font(AREX_FONT_ID_SMALL);
    txt_dsc.color = AREX_GREEN;

    /* ==========================================
     * 绘制 Y 轴网格（深度）
     * ========================================== */
    line_dsc.color = AREX_GREEN;
    line_dsc.width = 1;
    line_dsc.opa   = 38;
    line_dsc.dash_width = 4;
    line_dsc.dash_gap   = 4;

    int y_step = (max_d_axis >= 200.0f) ? 50 : ((max_d_axis >= 100.0f) ? 20 : 10);
    for (int d = 0; d <= (int)max_d_axis; d += y_step) {
        lv_coord_t y = MAP_Y((float)d);
        lv_point_t pts[2] = {
            {area->x1 + (lv_coord_t)pad_x,               y},
            {area->x1 + (lv_coord_t)(chart_w - pad_right), y}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", d);
        txt_dsc.opa = 191;
        lv_area_t t_area = {area->x1, y - 10, area->x1 + (lv_coord_t)pad_x - 5, y + 10};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* ==========================================
     * 绘制 X 轴网格（时间：秒，居中对齐刻度数字）
     * ========================================== */
    txt_dsc.align = LV_TEXT_ALIGN_CENTER;
    for (int t = 0; t <= (int)max_t_axis_sec; t += x_step) {
        lv_coord_t x = MAP_X((float)t);
        lv_point_t pts[2] = {
            {x, area->y1 + (lv_coord_t)pad_y_top},
            {x, area->y1 + (lv_coord_t)(chart_h - pad_y_bot)}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[16];
        if (max_t_axis_sec >= 120.0f) {
            if (t % 60 == 0)
                snprintf(buf, sizeof(buf), "%d", t / 60);
            else
                snprintf(buf, sizeof(buf), "%.1f", (float)t / 60.0f);
        } else {
            snprintf(buf, sizeof(buf), "%d", t);
        }
        lv_area_t t_area = {x - 20, area->y2 - 18, x + 20, area->y2};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }
    txt_dsc.align = LV_TEXT_ALIGN_LEFT;

    /* ==========================================
     * 绘制左下角坐标系单位 (m/min)
     * ========================================== */
    lv_draw_label_dsc_t unit_dsc;
    lv_draw_label_dsc_init(&unit_dsc);
    unit_dsc.font = arex_get_font(AREX_FONT_ID_SMALL);
    unit_dsc.color = AREX_LIGHT;
    unit_dsc.opa = 191;

    lv_area_t unit_area = {
        area->x1 + 2,
        area->y2 - 14,
        area->x1 + 60,
        area->y2 + 10
    };
    lv_draw_label(draw_ctx, &unit_dsc, &unit_area, "m/min", NULL);

    /* ==========================================
     * 绘制历史真实轨迹（实线）
     * ========================================== */
    line_dsc.opa       = LV_OPA_COVER;
    line_dsc.width     = 3;
    line_dsc.dash_width = 0;
    line_dsc.dash_gap   = 0;

    lv_point_t last_p;
    if (g_dive_log_count > 0) {
        last_p.x = MAP_X(g_dive_log[0].time_s);
        last_p.y = MAP_Y(g_dive_log[0].depth_m);

        for (int i = 1; i < g_dive_log_count; i++) {
            lv_point_t next_p = {
                MAP_X(g_dive_log[i].time_s),
                MAP_Y(g_dive_log[i].depth_m)
            };
            lv_draw_line(draw_ctx, &line_dsc, &last_p, &next_p);
            last_p = next_p;
        }
    } else {
        last_p.x = MAP_X(0.0f);
        last_p.y = MAP_Y(0.0f);
    }

    /* 连到当前 NOW 点 */
    lv_point_t now_p = {MAP_X(current_t_sec), MAP_Y(current_d)};
    lv_draw_line(draw_ctx, &line_dsc, &last_p, &now_p);

    /* NOW 点（纯 AREX_GREEN 实心圆） */
    lv_draw_rect_dsc_t now_dsc;
    lv_draw_rect_dsc_init(&now_dsc);
    now_dsc.radius   = LV_RADIUS_CIRCLE;
    now_dsc.bg_color = AREX_GREEN;
    now_dsc.bg_opa   = LV_OPA_COVER;
    lv_area_t now_area = {now_p.x - 6, now_p.y - 6, now_p.x + 6, now_p.y + 6};
    lv_draw_rect(draw_ctx, &now_dsc, &now_area);

    /* NOW 文字背景（绿底） */
    lv_draw_rect_dsc_t now_bg;
    lv_draw_rect_dsc_init(&now_bg);
    now_bg.bg_color = AREX_GREEN;
    now_bg.bg_opa   = LV_OPA_COVER;
    lv_area_t now_bg_area = {now_p.x + 10, now_p.y - 8, now_p.x + 46, now_p.y + 8};
    lv_draw_rect(draw_ctx, &now_bg, &now_bg_area);

    /* NOW 文字（黑字） */
    txt_dsc.color = AREX_BLACK;
    txt_dsc.opa   = LV_OPA_COVER;
    lv_draw_label(draw_ctx, &txt_dsc, &now_bg_area, "NOW", NULL);
    txt_dsc.color = AREX_GREEN;

    /* ==========================================
     * 绘制预测减压轨迹（斜虚线）
     * ========================================== */
    if (g_deco_stop_count > 0) {
        line_dsc.opa   = LV_OPA_COVER;
        line_dsc.width = 3;

        float cur_t_sec = current_t_sec;
        float draw_d   = current_d;
        lv_point_t p1  = now_p;

        for (int i = 0; i < g_deco_stop_count; i++) {
            /* 上升段（斜虚线，6秒/米） */
            float asc_t = (draw_d > g_deco_stops[i].depth_m)
                          ? (draw_d - g_deco_stops[i].depth_m) * 6.0f : 0;
            cur_t_sec += asc_t;
            lv_point_t p2 = {MAP_X(cur_t_sec), MAP_Y(g_deco_stops[i].depth_m)};
            draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p2);

            /* 停留段（平移虚线，分钟转秒） */
            float hold_t_sec = g_deco_stops[i].stay_min * 60.0f;
            cur_t_sec += hold_t_sec;
            lv_point_t p3 = {MAP_X(cur_t_sec), MAP_Y(g_deco_stops[i].depth_m)};
            draw_diagonal_dashed_line(draw_ctx, &line_dsc, p2, p3);

            /* 停留点空心圆 */
            lv_coord_t circle_x = MAP_X(cur_t_sec - hold_t_sec / 2.0f);
            lv_draw_rect_dsc_t deco_node;
            lv_draw_rect_dsc_init(&deco_node);
            deco_node.radius        = LV_RADIUS_CIRCLE;
            deco_node.bg_color      = AREX_BLACK;
            deco_node.bg_opa        = LV_OPA_COVER;
            deco_node.border_color  = AREX_GREEN;
            deco_node.border_width  = 2;
            lv_area_t c_area = {circle_x - 4, p2.y - 4, circle_x + 4, p2.y + 4};
            lv_draw_rect(draw_ctx, &deco_node, &c_area);

            /* 停留点文字 */
            char d_buf[16];
            snprintf(d_buf, sizeof(d_buf), "%dm %d'",
                     (int)g_deco_stops[i].depth_m, (int)g_deco_stops[i].stay_min);
            lv_area_t d_txt = {circle_x - 20, p2.y - 20, circle_x + 20, p2.y - 4};
            txt_dsc.opa = LV_OPA_COVER;
            lv_draw_label(draw_ctx, &txt_dsc, &d_txt, d_buf, NULL);
            txt_dsc.opa = 191;

            p1 = p3;
            draw_d = g_deco_stops[i].depth_m;
        }

        /* 最后升水段 */
        cur_t_sec += (draw_d > 0) ? draw_d * 6.0f : 0;
        lv_point_t p_end = {MAP_X(cur_t_sec), MAP_Y(0.0f)};
        draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p_end);
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
    arex_render_card_title(parent, "4F: DIVE PLAN TRACK");

    init_test_data();

    /* 动态推算图表尺寸：以标题区下方为 Y=0 起点 */
    int right_w = (int)g_sys_config.safe_zone_w - (int)AREX_LEFT_ANCHOR_W
                - (int)(g_sys_config.gap_u * AREX_BASE_U);
    int tile_h  = (int)g_sys_config.safe_zone_h;
    int chart_x = CHART_PAD;
    int chart_y = AREX_CARD_TITLE_H;
    int chart_w = right_w - CHART_PAD * 2;
    int chart_h = tile_h - AREX_CARD_TITLE_H - CHART_PAD;  /* 剩余空间自适应 */
    if (chart_h < 60) chart_h = 60;
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_remove_style_all(shell);
    lv_obj_set_size(shell, chart_w + CHART_PAD * 2, chart_h + CHART_PAD * 2);
    lv_obj_set_pos(shell, chart_x - CHART_PAD, chart_y - CHART_PAD);
    lv_obj_set_style_bg_color(shell, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, AREX_DARK, 0);
        lv_obj_set_style_border_width(shell, AREX_GRID_BORDER_W, 0);
    lv_obj_set_style_radius(shell, 0, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);

    /* 零 RAM 画板对象 */
    s_chart_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_chart_obj);
    lv_obj_set_size(s_chart_obj, chart_w, chart_h);
    lv_obj_set_pos(s_chart_obj, chart_x, chart_y);
    lv_obj_add_event_cb(s_chart_obj, plan_chart_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

void card_plan_update(void)
{
    if (s_chart_obj) {
        lv_obj_invalidate(s_chart_obj);
    }
}
