#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../screen/layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* 图表布局常量（完全动态化，以标题区为绝对 Y=0 起点）
 * 宽高由 tile 尺寸动态推算，绝不使用魔法数字。
 * 图表 Y 起点 = CARD_TITLE_H（标题下方）
 * 图表高度 = tile_h - CARD_TITLE_H - 底部预留
 * 图表宽度 = tile_w - 左右 padding */
#define CHART_PAD   10

/* ============================================================
 * 潜水轨迹与减压停留（定义，共享给 dive_log_append 追加点）
 * ============================================================ */
dive_pt_t   g_dive_log[MAX_DIVE_LOG];
uint16_t         g_dive_log_count;

/* 满 200 点后压掉最接近直线的内部点，保留明显转折，避免轨迹失真。 */
static float dive_log_triangle_area(const dive_pt_t *a,
                                    const dive_pt_t *b,
                                    const dive_pt_t *c)
{
    float ab_t = b->time_s - a->time_s;
    float ab_d = b->depth_m - a->depth_m;
    float ac_t = c->time_s - a->time_s;
    float ac_d = c->depth_m - a->depth_m;
    return fabsf(ab_t * ac_d - ab_d * ac_t);
}

static void dive_log_remove_at(uint16_t index)
{
    if (index >= g_dive_log_count)
    {
        return;
    }
    if (index + 1U < g_dive_log_count)
    {
        memmove(&g_dive_log[index],
                &g_dive_log[index + 1U],
                (g_dive_log_count - index - 1U) * sizeof(g_dive_log[0]));
    }
    g_dive_log_count--;
}

static bool dive_log_make_room_for(float current_time_s, float current_depth_m)
{
    if (g_dive_log_count < MAX_DIVE_LOG)
    {
        return false;
    }
    if (g_dive_log_count < 3U)
    {
        g_dive_log[g_dive_log_count - 1U].time_s = current_time_s;
        g_dive_log[g_dive_log_count - 1U].depth_m = current_depth_m;
        return true;
    }

    uint16_t drop_index = 1U;
    float drop_area = FLT_MAX;
    for (uint16_t i = 1U; i + 1U < g_dive_log_count; i++)
    {
        float area = dive_log_triangle_area(&g_dive_log[i - 1U],
                                            &g_dive_log[i],
                                            &g_dive_log[i + 1U]);
        if (area < drop_area)
        {
            drop_area = area;
            drop_index = i;
        }
    }

    dive_pt_t next =
    {
        .time_s = current_time_s,
        .depth_m = current_depth_m
    };
    float tail_area = dive_log_triangle_area(&g_dive_log[g_dive_log_count - 2U],
                                             &g_dive_log[g_dive_log_count - 1U],
                                             &next);
    if (tail_area <= drop_area)
    {
        g_dive_log[g_dive_log_count - 1U] = next;
        return true;
    }

    dive_log_remove_at(drop_index);
    return false;
}

/* ============================================================
 * 历史轨迹推流接口（供 data.h 导出，外部 1Hz 定时器调用）
 * ============================================================ */
void dive_log_append(float current_time_s, float current_depth_m)
{
    if (current_time_s < 0.0f)
    {
        return;
    }

    if (g_dive_log_count > 0)
    {
        dive_pt_t *last = &g_dive_log[g_dive_log_count - 1];

        /* 丢弃回退时间点，避免旧缓存/跨线程时序异常污染轨迹 */
        if (current_time_s < last->time_s)
        {
            return;
        }

        /* 同一秒内的重复采样只更新最后一个点，避免轨迹堆积和折返 */
        if (fabsf(current_time_s - last->time_s) < 0.001f)
        {
            last->depth_m = current_depth_m;
            return;
        }
    }

    if (g_dive_log_count >= MAX_DIVE_LOG &&
        dive_log_make_room_for(current_time_s, current_depth_m))
    {
        return;
    }

    if (g_dive_log_count < MAX_DIVE_LOG)
    {
        g_dive_log[g_dive_log_count].time_s  = current_time_s;
        g_dive_log[g_dive_log_count].depth_m = current_depth_m;
        g_dive_log_count++;
    }
}

void dive_log_reset(void)
{
    g_dive_log_count = 0;
    bus_set_deco_plan(NULL, 0);
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

    for (int i = 0; i < num_steps; i++)
    {
        float t1 = (i * step) / dist;
        float t2 = (i * step + dash_len) / dist;
        if (t1 > 1.0f) break;
        if (t2 > 1.0f) t2 = 1.0f;

        lv_point_t seg_p1 =
        {
            p1.x + (lv_coord_t)(dx * t1),
            p1.y + (lv_coord_t)(dy * t1)
        };
        lv_point_t seg_p2 =
        {
            p1.x + (lv_coord_t)(dx * t2),
            p1.y + (lv_coord_t)(dy * t2)
        };

        lv_draw_line(draw_ctx, &solid_dsc, &seg_p1, &seg_p2);
    }
}

/* ============================================================
 * 4F: DIVE PLAN TRACK — 秒级坐标引擎
 * 100% 秒 (Seconds) 为底层单位，重现 HTML "0s, 10s, 20s" 放大效果
 *   外壳：2px 实线 DARK，Padding 10px
 *   画布：400×320px（实际绘图区：X轴45~385，Y轴15~295）
 *   背景网格：虚线，透明度 15%（opa=38）
 *   坐标轴文字：FONT_ID_SMALL，透明度 75%（opa=191）
 *   历史轨迹（Past）：实线，粗细 3px，GREEN
 *   计划轨迹（Future）：自定义插值斜虚线，粗细 3px，GREEN
 *   NOW 标记：绿色实心圆 + 黑字绿底背景
 *   减压停留点：空心圆 + 深度停留文字
 * ============================================================ */
static void plan_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;

    /* 等待下水 */
    if (g_sensor_data.depth <= 0.5f && g_dive_log_count == 0)
    {
        return;
    }

    /* 1. 同步 HTML 边距参数（基于实际画布尺寸动态计算） */
    float pad_x      = 45.0f;
    float pad_y_top  = 15.0f;
    float pad_y_bot  = 25.0f;
    float pad_right  = 24.0f;
    float chart_w    = (float)(area->x2 - area->x1);
    float chart_h    = (float)(area->y2 - area->y1);
    float w          = chart_w - pad_x - pad_right;
    float h          = chart_h - pad_y_top - pad_y_bot;
    float x_axis_left = (float)area->x1 + pad_x;
    float x_axis_right = (float)area->x1 + chart_w - pad_right;

    /* 2. 核心：全量提取为"秒"进行推演 */
    float current_t_sec = (float)g_sensor_data.dive_time_s;
    float current_d     = g_sensor_data.depth;

    /* 预测所需总时间（基于 6秒/米 的升水速度，即 10m/min） */
    float predicted_t_sec = current_t_sec;
    float sim_d = current_d;
    for (int i = 0; i < g_deco_stop_count; i++)
    {
        float asc_t = (sim_d > g_deco_stops[i].depth_m)
                      ? (sim_d - g_deco_stops[i].depth_m) * 6.0f : 0;
        predicted_t_sec += asc_t;
        predicted_t_sec += g_deco_stops[i].stay_min * 60.0f;
        sim_d = g_deco_stops[i].depth_m;
    }
    predicted_t_sec += (sim_d > 0) ? sim_d * 6.0f : 0;

    /* 3. 动态扩展 Y 轴 */
    float max_log_d = current_d;
    for (int i = 0; i < g_dive_log_count; i++)
    {
        if (g_dive_log[i].depth_m > max_log_d) max_log_d = g_dive_log[i].depth_m;
    }
    float max_d_axis = 60.0f;
    if (max_log_d >= max_d_axis * 0.9f)
    {
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

    /* X轴显示分两种模式：
     * 1. 整个时间轴还在 1 分钟内：用秒，左下角显示 m/s。
     * 2. 整个时间轴超过 1 分钟：直接切到分钟，刻度只显示整数分钟。
     */
    bool x_axis_in_minutes = (max_t_axis_sec > 60.0f);
    int x_step = 10;
    if (x_axis_in_minutes)
    {
        float max_t_axis_min = max_t_axis_sec / 60.0f;
        if (max_t_axis_min <= 10.0f)      x_step = 60;
        else if (max_t_axis_min <= 20.0f) x_step = 120;
        else if (max_t_axis_min <= 40.0f) x_step = 300;
        else if (max_t_axis_min <= 80.0f) x_step = 600;
        else                               x_step = 900;
    }

    /* 5. 秒数映射宏 */
#define MAP_X(t_sec) ((lv_coord_t)(x_axis_left + ((t_sec) / max_t_axis_sec) * w))
#define MAP_Y(d)     (area->y1 + (lv_coord_t)(pad_y_top + ((d) / max_d_axis) * h))

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_t txt_dsc;
    lv_draw_label_dsc_init(&txt_dsc);
    txt_dsc.font = get_font(FONT_ID_SMALL);
    txt_dsc.color = GREEN;

    /* ==========================================
     * 绘制 Y 轴网格（深度）
     * ========================================== */
    line_dsc.color = GREEN;
    line_dsc.width = 1;
    line_dsc.opa   = 38;
    line_dsc.dash_width = 4;
    line_dsc.dash_gap   = 4;

    int y_step = (max_d_axis >= 200.0f) ? 50 : ((max_d_axis >= 100.0f) ? 20 : 10);
    for (int d = 0; d <= (int)max_d_axis; d += y_step)
    {
        lv_coord_t y = MAP_Y((float)d);
        lv_point_t pts[2] =
        {
            {(lv_coord_t)x_axis_left,  y},
            {(lv_coord_t)x_axis_right, y}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", d);
        txt_dsc.opa = 191;
        lv_area_t t_area = {area->x1, y - 10, area->x1 + (lv_coord_t)pad_x - 5, y + 10};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

    /* ==========================================
     * 绘制 X 轴网格（时间，居中对齐刻度数字）
     * ========================================== */
    txt_dsc.align = LV_TEXT_ALIGN_CENTER;
    for (int t = 0; t <= (int)max_t_axis_sec; t += x_step)
    {
        lv_coord_t x = MAP_X((float)t);
        lv_point_t pts[2] =
        {
            {x, area->y1 + (lv_coord_t)pad_y_top},
            {x, area->y1 + (lv_coord_t)(chart_h - pad_y_bot)}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[16];
        if (x_axis_in_minutes)
        {
            snprintf(buf, sizeof(buf), "%d", t / 60);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", t);
        }
        lv_coord_t lbl_left = x - 20;
        lv_coord_t lbl_right = x + 20;
        if (lbl_left < (lv_coord_t)x_axis_left)
        {
            lbl_left = (lv_coord_t)x_axis_left;
            lbl_right = lbl_left + 40;
        }
        if (lbl_right > (lv_coord_t)x_axis_right)
        {
            lbl_right = (lv_coord_t)x_axis_right;
            lbl_left = lbl_right - 40;
        }
        lv_area_t t_area = {lbl_left, area->y2 - 18, lbl_right, area->y2};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }
    txt_dsc.align = LV_TEXT_ALIGN_LEFT;

    /* ==========================================
     * 绘制左下角坐标系单位：秒模式 m/s，分钟模式 m/min
     * ========================================== */
    lv_draw_label_dsc_t unit_dsc;
    lv_draw_label_dsc_init(&unit_dsc);
    unit_dsc.font = get_font(FONT_ID_SMALL);
    unit_dsc.color = LIGHT;
    unit_dsc.opa = 191;

    lv_area_t unit_area =
    {
        area->x1 + 2,
        area->y2 - 14,
        area->x1 + 60,
        area->y2 + 10
    };
    lv_draw_label(draw_ctx, &unit_dsc, &unit_area,
                  x_axis_in_minutes ? "m/min" : "m/s", NULL);

    /* ==========================================
     * 绘制历史真实轨迹（实线）
     * ========================================== */
    line_dsc.opa       = LV_OPA_COVER;
    line_dsc.width     = 3;
    line_dsc.dash_width = 0;
    line_dsc.dash_gap   = 0;

    lv_point_t last_p;
    if (g_dive_log_count > 0)
    {
        last_p.x = MAP_X(g_dive_log[0].time_s);
        last_p.y = MAP_Y(g_dive_log[0].depth_m);

        for (int i = 1; i < g_dive_log_count; i++)
        {
            lv_point_t next_p =
            {
                MAP_X(g_dive_log[i].time_s),
                MAP_Y(g_dive_log[i].depth_m)
            };
            lv_draw_line(draw_ctx, &line_dsc, &last_p, &next_p);
            last_p = next_p;
        }
    }
    else
    {
        last_p.x = MAP_X(0.0f);
        last_p.y = MAP_Y(0.0f);
    }

    /* 连到当前 NOW 点 */
    lv_point_t now_p = {MAP_X(current_t_sec), MAP_Y(current_d)};
    lv_draw_line(draw_ctx, &line_dsc, &last_p, &now_p);

    /* NOW 点（纯 GREEN 实心圆） */
    lv_draw_rect_dsc_t now_dsc;
    lv_draw_rect_dsc_init(&now_dsc);
    now_dsc.radius   = LV_RADIUS_CIRCLE;
    now_dsc.bg_color = GREEN;
    now_dsc.bg_opa   = LV_OPA_COVER;
    lv_area_t now_area = {now_p.x - 6, now_p.y - 6, now_p.x + 6, now_p.y + 6};
    lv_draw_rect(draw_ctx, &now_dsc, &now_area);

    /* NOW 文字背景（绿底）
     * 按真实字宽留白，避免不同字库/字重下右侧字符被裁掉。 */
    lv_point_t now_text_size;
    lv_txt_get_size(&now_text_size, "NOW", txt_dsc.font, txt_dsc.letter_space,
                    txt_dsc.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_draw_rect_dsc_t now_bg;
    lv_draw_rect_dsc_init(&now_bg);
    now_bg.bg_color = GREEN;
    now_bg.bg_opa   = LV_OPA_COVER;
    lv_coord_t now_bg_pad_x = 6;
    lv_coord_t now_bg_pad_y = 4;
    lv_area_t now_bg_area =
    {
        now_p.x + 10,
        now_p.y - (now_text_size.y / 2) - now_bg_pad_y,
        now_p.x + 10 + now_text_size.x + now_bg_pad_x * 2,
        now_p.y + (now_text_size.y / 2) + now_bg_pad_y
    };
    lv_draw_rect(draw_ctx, &now_bg, &now_bg_area);

    /* NOW 文字（黑字） */
    txt_dsc.color = BLACK;
    txt_dsc.opa   = LV_OPA_COVER;
    lv_draw_label(draw_ctx, &txt_dsc, &now_bg_area, "NOW", NULL);
    txt_dsc.color = GREEN;

    /* ==========================================
     * 绘制预测减压轨迹（斜虚线）
     * ========================================== */
    if (g_deco_stop_count > 0)
    {
        line_dsc.opa   = LV_OPA_COVER;
        line_dsc.width = 3;

        float cur_t_sec = current_t_sec;
        float draw_d   = current_d;
        lv_point_t p1  = now_p;

        for (int i = 0; i < g_deco_stop_count; i++)
        {
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
            deco_node.bg_color      = BLACK;
            deco_node.bg_opa        = LV_OPA_COVER;
            deco_node.border_color  = GREEN;
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
    render_card_title(parent, "DIVE PLAN TRACK");

    /* 动态推算图表尺寸：以标题区下方为 Y=0 起点 */
    int right_w = (int)g_sys_config.safe_zone_w - (int)LEFT_ANCHOR_W
                  - (int)(g_sys_config.gap_u * BASE_U);
    int tile_h  = (int)g_sys_config.safe_zone_h;
    int chart_x = CHART_PAD;
    int chart_y = CARD_TITLE_H;
    int chart_w = right_w - CHART_PAD * 2;
    int chart_h = tile_h - CARD_TITLE_H - CHART_PAD;  /* 剩余空间自适应 */
    if (chart_h < 60) chart_h = 60;
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_remove_style_all(shell);
    lv_obj_set_size(shell, chart_w + CHART_PAD * 2, chart_h + CHART_PAD * 2);
    lv_obj_set_pos(shell, chart_x - CHART_PAD, chart_y - CHART_PAD);
    lv_obj_set_style_bg_color(shell, BLACK, 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, DARK, 0);
    lv_obj_set_style_border_width(shell, GRID_BORDER_W, 0);
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
    if (s_chart_obj)
    {
        lv_obj_invalidate(s_chart_obj);
    }
}
