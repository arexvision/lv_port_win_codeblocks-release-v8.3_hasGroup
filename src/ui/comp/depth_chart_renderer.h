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

    last = depth_chart_map_point(&points[0], x, y, w, h, max_time_s, max_depth_m);
    for (uint16_t i = 1U; i < point_count; i++)
    {
        lv_point_t next = depth_chart_map_point(&points[i], x, y, w, h, max_time_s, max_depth_m);
        lv_draw_line(draw_ctx, &line_dsc, &last, &next);
        last = next;
    }

    if (out_last != NULL)
    {
        *out_last = last;
    }
    return true;
}

#endif /* DEPTH_CHART_RENDERER_H */
