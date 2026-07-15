/*
 * 文件: src/app_ui/ui/comp/depth_chart_renderer.h
 * 作用: 通用深度轨迹绘制 helper，供实时 PLAN 和历史 LOGBOOK 复用。
 */

#ifndef DEPTH_CHART_RENDERER_H
#define DEPTH_CHART_RENDERER_H

#include "../core/ui_types.h"
#include "lvgl/lvgl.h"

static inline lv_point_t depth_chart_map_point(const dive_pt_t *point,
                                               lv_coord_t x,
                                               lv_coord_t y,
                                               lv_coord_t w,
                                               lv_coord_t h,
                                               float max_time_s,
                                               float max_depth_m)
{
    lv_point_t out;
    float t = (max_time_s > 0.0f) ? point->time_s / max_time_s : 0.0f;
    float d = (max_depth_m > 0.0f) ? point->depth_m / max_depth_m : 0.0f;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;

    out.x = (lv_coord_t)(x + t * (float)w);
    out.y = (lv_coord_t)(y + d * (float)h);
    return out;
}

static inline lv_coord_t depth_chart_coord_abs_diff(lv_coord_t a, lv_coord_t b)
{
    return (a >= b) ? (lv_coord_t)(a - b) : (lv_coord_t)(b - a);
}

static inline int depth_chart_delta_sign(int32_t value)
{
    if (value > 0)
    {
        return 1;
    }
    if (value < 0)
    {
        return -1;
    }
    return 0;
}

static inline void depth_chart_draw_line_if_needed(lv_draw_ctx_t *draw_ctx,
                                                   const lv_draw_line_dsc_t *line_dsc,
                                                   const lv_point_t *p1,
                                                   const lv_point_t *p2)
{
    if ((p1->x == p2->x) && (p1->y == p2->y))
    {
        return;
    }
    lv_draw_line(draw_ctx, line_dsc, p1, p2);
}

static inline void depth_chart_flush_lod_column(lv_draw_ctx_t *draw_ctx,
                                                const lv_draw_line_dsc_t *line_dsc,
                                                lv_point_t *prev,
                                                lv_coord_t x,
                                                lv_coord_t min_y,
                                                lv_coord_t max_y,
                                                lv_coord_t last_y)
{
    lv_point_t last = {x, last_y};

    if (depth_chart_coord_abs_diff(min_y, max_y) <= (lv_coord_t)line_dsc->width)
    {
        depth_chart_draw_line_if_needed(draw_ctx, line_dsc, prev, &last);
        *prev = last;
        return;
    }

    lv_point_t near_p;
    lv_point_t far_p;
    if (depth_chart_coord_abs_diff(prev->y, min_y) <= depth_chart_coord_abs_diff(prev->y, max_y))
    {
        near_p.x = x;
        near_p.y = min_y;
        far_p.x = x;
        far_p.y = max_y;
    }
    else
    {
        near_p.x = x;
        near_p.y = max_y;
        far_p.x = x;
        far_p.y = min_y;
    }

    depth_chart_draw_line_if_needed(draw_ctx, line_dsc, prev, &near_p);
    depth_chart_draw_line_if_needed(draw_ctx, line_dsc, &near_p, &far_p);
    depth_chart_draw_line_if_needed(draw_ctx, line_dsc, &far_p, &last);
    *prev = last;
}

static inline lv_coord_t depth_chart_lod_bucket_x(lv_coord_t mapped_x,
                                                  lv_coord_t x,
                                                  lv_coord_t w,
                                                  uint16_t bucket_count)
{
    if ((w == 0) || (bucket_count <= 1U))
    {
        return mapped_x;
    }

    int32_t span = (w > 0) ? (int32_t)w : -(int32_t)w;
    int32_t rel = (w > 0) ? ((int32_t)mapped_x - (int32_t)x) :
                             ((int32_t)x - (int32_t)mapped_x);
    if (rel < 0)
    {
        rel = 0;
    }
    if (rel > span)
    {
        rel = span;
    }

    uint16_t bucket = (uint16_t)((rel * (int32_t)(bucket_count - 1U)) / span);
    lv_coord_t bucket_x = (lv_coord_t)(x + ((int32_t)w * (int32_t)bucket) / (int32_t)(bucket_count - 1U));
    return bucket_x;
}

static inline bool depth_chart_draw_profile(lv_draw_ctx_t *draw_ctx,
                                            const dive_pt_t *points,
                                            uint16_t point_count,
                                            lv_coord_t x,
                                            lv_coord_t y,
                                            lv_coord_t w,
                                            lv_coord_t h,
                                            float max_time_s,
                                            float max_depth_m,
                                            lv_color_t color,
                                            uint8_t line_width,
                                            lv_opa_t opa,
                                            lv_point_t *out_last)
{
    lv_draw_line_dsc_t line_dsc;
    lv_point_t anchor;
    lv_point_t pending;
    lv_point_t raw_last;

    if ((draw_ctx == NULL) || (points == NULL) || (point_count == 0U))
    {
        return false;
    }

    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = line_width;
    line_dsc.opa = opa;
    line_dsc.dash_width = 0;
    line_dsc.dash_gap = 0;

    uint16_t pixel_budget = (w > 0) ? (uint16_t)w : (uint16_t)(0 - w);
    if (pixel_budget < 16U)
    {
        pixel_budget = 16U;
    }
    if (pixel_budget > 96U)
    {
        pixel_budget = 96U;
    }
    if (point_count > pixel_budget)
    {
        lv_point_t prev_drawn;
        lv_coord_t group_x;
        lv_coord_t min_y;
        lv_coord_t max_y;
        lv_coord_t last_y;

        anchor = depth_chart_map_point(&points[0], x, y, w, h, max_time_s, max_depth_m);
        prev_drawn = anchor;
        raw_last = anchor;
        group_x = depth_chart_lod_bucket_x(anchor.x, x, w, pixel_budget);
        min_y = anchor.y;
        max_y = anchor.y;
        last_y = anchor.y;

        for (uint16_t i = 1U; i < point_count; i++)
        {
            lv_point_t next = depth_chart_map_point(&points[i], x, y, w, h, max_time_s, max_depth_m);
            lv_coord_t next_group_x = depth_chart_lod_bucket_x(next.x, x, w, pixel_budget);
            raw_last = next;
            if (next_group_x == group_x)
            {
                if (next.y < min_y) min_y = next.y;
                if (next.y > max_y) max_y = next.y;
                last_y = next.y;
                continue;
            }

            depth_chart_flush_lod_column(draw_ctx, &line_dsc, &prev_drawn, group_x, min_y, max_y, last_y);
            group_x = next_group_x;
            min_y = next.y;
            max_y = next.y;
            last_y = next.y;
        }

        depth_chart_flush_lod_column(draw_ctx, &line_dsc, &prev_drawn, group_x, min_y, max_y, last_y);
        if (out_last != NULL)
        {
            *out_last = raw_last;
        }
        return true;
    }

    anchor = depth_chart_map_point(&points[0], x, y, w, h, max_time_s, max_depth_m);
    pending = anchor;
    raw_last = anchor;
    for (uint16_t i = 1U; i < point_count; i++)
    {
        lv_point_t next = depth_chart_map_point(&points[i], x, y, w, h, max_time_s, max_depth_m);
        int32_t ax;
        int32_t ay;
        int32_t bx;
        int32_t by;
        int32_t area2;
        int32_t span;
        int32_t seg1_dx;
        int32_t seg1_dy;
        int32_t seg2_dx;
        int32_t seg2_dy;
        bool same_direction;

        raw_last = next;
        if ((next.x == pending.x) && (next.y == pending.y))
        {
            continue;
        }
        if ((pending.x == anchor.x) && (pending.y == anchor.y))
        {
            pending = next;
            continue;
        }

        ax = (int32_t)pending.x - (int32_t)anchor.x;
        ay = (int32_t)pending.y - (int32_t)anchor.y;
        bx = (int32_t)next.x - (int32_t)anchor.x;
        by = (int32_t)next.y - (int32_t)anchor.y;
        area2 = (ax * by) - (ay * bx);
        if (area2 < 0)
        {
            area2 = -area2;
        }
        span = (bx < 0 ? -bx : bx) + (by < 0 ? -by : by);
        seg1_dx = (int32_t)pending.x - (int32_t)anchor.x;
        seg1_dy = (int32_t)pending.y - (int32_t)anchor.y;
        seg2_dx = (int32_t)next.x - (int32_t)pending.x;
        seg2_dy = (int32_t)next.y - (int32_t)pending.y;
        same_direction =
            (depth_chart_delta_sign(seg1_dx) == depth_chart_delta_sign(seg2_dx)) &&
            (depth_chart_delta_sign(seg1_dy) == depth_chart_delta_sign(seg2_dy));

        /*
         * 只合并同方向的近似直线段。下潜/上升接恒深平台时 dy 方向会变化，
         * 必须保留拐点，否则 PLAN 图会把平台段拉成“2点一线”的斜线。
         */
        if (same_direction &&
            (span > 0) &&
            (area2 <= (span * 2)))
        {
            pending = next;
            continue;
        }

        lv_draw_line(draw_ctx, &line_dsc, &anchor, &pending);
        anchor = pending;
        pending = next;
    }

    if ((pending.x != anchor.x) || (pending.y != anchor.y))
    {
        lv_draw_line(draw_ctx, &line_dsc, &anchor, &pending);
    }

    if (out_last != NULL)
    {
        *out_last = raw_last;
    }
    return true;
}

#endif /* DEPTH_CHART_RENDERER_H */
