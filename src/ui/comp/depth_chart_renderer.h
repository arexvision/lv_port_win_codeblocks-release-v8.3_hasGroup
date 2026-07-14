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

static inline bool depth_chart_line_intersects_clip(const lv_draw_ctx_t *draw_ctx,
                                                     const lv_point_t *p1,
                                                     const lv_point_t *p2,
                                                     uint8_t line_width)
{
    if ((p1 == NULL) || (p2 == NULL)) return false;
    if ((draw_ctx == NULL) || (draw_ctx->clip_area == NULL)) return true;

    const lv_area_t *clip = draw_ctx->clip_area;
    lv_coord_t pad = (lv_coord_t)line_width + 1;
    lv_coord_t min_x = LV_MIN(p1->x, p2->x) - pad;
    lv_coord_t max_x = LV_MAX(p1->x, p2->x) + pad;
    lv_coord_t min_y = LV_MIN(p1->y, p2->y) - pad;
    lv_coord_t max_y = LV_MAX(p1->y, p2->y) + pad;

    return !(max_x < clip->x1 || min_x > clip->x2 || max_y < clip->y1 || min_y > clip->y2);
}

static inline bool depth_chart_points_continue_line(const lv_point_t *start,
                                                     const lv_point_t *middle,
                                                     const lv_point_t *end)
{
    int32_t dx1 = (int32_t)middle->x - (int32_t)start->x;
    int32_t dy1 = (int32_t)middle->y - (int32_t)start->y;
    int32_t dx2 = (int32_t)end->x - (int32_t)middle->x;
    int32_t dy2 = (int32_t)end->y - (int32_t)middle->y;

    if ((dx1 * dy2) != (dy1 * dx2)) return false;
    return (dx1 * dx2 >= 0) && (dy1 * dy2 >= 0);
}

static inline bool depth_chart_build_profile_points(const dive_pt_t *points,
                                                     uint16_t point_count,
                                                     lv_coord_t x,
                                                     lv_coord_t y,
                                                     lv_coord_t w,
                                                     lv_coord_t h,
                                                     float max_time_s,
                                                     float max_depth_m,
                                                     lv_point_t *out_points,
                                                     uint16_t out_capacity,
                                                     uint16_t *out_point_count)
{
    uint16_t draw_count = 0U;

    if (out_point_count != NULL) *out_point_count = 0U;
    if ((points == NULL) || (point_count == 0U) || (out_points == NULL) || (out_capacity == 0U) || (out_point_count == NULL)) return false;

    out_points[draw_count++] = depth_chart_map_point(&points[0], x, y, w, h, max_time_s, max_depth_m);
    for (uint16_t i = 1U; i < point_count; i++)
    {
        lv_point_t next = depth_chart_map_point(&points[i], x, y, w, h, max_time_s, max_depth_m);
        lv_point_t *last = &out_points[draw_count - 1U];

        if ((next.x == last->x) && (next.y == last->y)) continue;
        if ((draw_count >= 2U) && depth_chart_points_continue_line(&out_points[draw_count - 2U], last, &next))
        {
            *last = next;
            continue;
        }
        if (draw_count >= out_capacity) return false;
        out_points[draw_count++] = next;
    }

    *out_point_count = draw_count;
    return true;
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
    lv_point_t segment_start;
    lv_point_t segment_end;
    lv_point_t last;

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

    segment_start = depth_chart_map_point(&points[0], x, y, w, h, max_time_s, max_depth_m);
    segment_end = segment_start;
    last = segment_start;
    for (uint16_t i = 1U; i < point_count; i++)
    {
        lv_point_t next = depth_chart_map_point(&points[i], x, y, w, h, max_time_s, max_depth_m);
        last = next;

        if ((next.x == segment_end.x) && (next.y == segment_end.y)) continue;
        if (((segment_start.x != segment_end.x) || (segment_start.y != segment_end.y)) &&
            depth_chart_points_continue_line(&segment_start, &segment_end, &next))
        {
            segment_end = next;
            continue;
        }

        if (((segment_start.x != segment_end.x) || (segment_start.y != segment_end.y)) &&
            depth_chart_line_intersects_clip(draw_ctx, &segment_start, &segment_end, line_width))
        {
            lv_draw_line(draw_ctx, &line_dsc, &segment_start, &segment_end);
        }
        segment_start = segment_end;
        segment_end = next;
    }

    if (((segment_start.x != segment_end.x) || (segment_start.y != segment_end.y)) &&
        depth_chart_line_intersects_clip(draw_ctx, &segment_start, &segment_end, line_width))
    {
        lv_draw_line(draw_ctx, &line_dsc, &segment_start, &segment_end);
    }

    if (out_last != NULL)
    {
        *out_last = last;
    }
    return true;
}

#endif /* DEPTH_CHART_RENDERER_H */
