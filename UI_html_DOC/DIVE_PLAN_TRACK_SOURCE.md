# DIVE PLAN TRACK source bundle

This document mechanically collects the direct implementation of the DIVE PLAN TRACK chart. Code blocks are copied verbatim from the current source files.

Scope: PLAN card drawing, shared depth profile drawing, PLAN VM, display configuration, fonts and title rendering, data structures, track sampling and thinning, deco stop synchronization, dirty routing, page registration, and current algorithm/PC simulator data entry points. Generic LVGL, global menus, and unrelated code are excluded.

Generation baseline: `7d786f8`.

---

# 1. Core drawing files

## src/ui/cards/card_plan.c (full file)

```c
#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_defs.h"
#include "../core/ui_engine.h"
#include "../core/vm/ui_vm_plan_chart_types.h"
#include "../comp/depth_chart_renderer.h"
#include "../screen/layout_view.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <math.h>

#define CHART_PAD 10

static ui_vm_plan_chart_t s_plan_chart_vm __attribute__((section(".psram_bss")));
static lv_obj_t *s_chart_obj;

static void draw_diagonal_dashed_line(lv_draw_ctx_t *draw_ctx,
                                      lv_draw_line_dsc_t *dsc,
                                      lv_point_t p1,
                                      lv_point_t p2)
{
    int dx = p2.x - p1.x;
    int dy = p2.y - p1.y;
    float dist = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);

    if (dist < 1.0f)
    {
        return;
    }

    float dash_len = 6.0f;
    float gap_len = 5.0f;
    float step = dash_len + gap_len;
    int num_steps = (int)ceilf(dist / step);

    lv_draw_line_dsc_t solid_dsc = *dsc;
    solid_dsc.dash_width = 0;
    solid_dsc.dash_gap = 0;

    for (int i = 0; i < num_steps; i++)
    {
        float t1 = (i * step) / dist;
        float t2 = (i * step + dash_len) / dist;

        if (t1 > 1.0f)
        {
            break;
        }
        if (t2 > 1.0f)
        {
            t2 = 1.0f;
        }

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

static uint16_t plan_track_stop_label_minutes(float stay_min)
{
    if (stay_min <= 0.0f) return 0U;
    uint16_t minutes = (uint16_t)ceilf(stay_min);
    return (minutes == 0U) ? 1U : minutes;
}

static void plan_track_format_stop_label(char *buf, size_t size, const deco_stop_t *stop, bool show_stop_time)
{
    if (show_stop_time) snprintf(buf, size, "%.0f%s %u'", (double)bus_get_depth_display(stop->depth_m), bus_get_depth_unit_label(), (unsigned)plan_track_stop_label_minutes(stop->stay_min));
    else snprintf(buf, size, "%.0f%s", (double)bus_get_depth_display(stop->depth_m), bus_get_depth_unit_label());
}

static void plan_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    const ui_vm_plan_chart_t *vm = &s_plan_chart_vm;

    if (vm->draw_enabled == 0U)
    {
        return;
    }

    float pad_x = 45.0f;
    float pad_y_top = 15.0f;
    float pad_y_bot = 34.0f;
    float pad_right = 24.0f;
    float chart_w = (float)(area->x2 - area->x1);
    float chart_h = (float)(area->y2 - area->y1);
    float w = chart_w - pad_x - pad_right;
    float h = chart_h - pad_y_top - pad_y_bot;
    float x_axis_left = (float)area->x1 + pad_x;
    float x_axis_right = (float)area->x1 + chart_w - pad_right;

    float current_t_sec = vm->current_time_s;
    float current_d = vm->current_depth_m;
    float max_d_axis = vm->max_depth_axis_m;
    float max_t_axis_sec = vm->max_time_axis_s;
    int x_step = (int)vm->x_step_s;

    enum
    {
        X_AXIS_SECONDS,
        X_AXIS_MINUTES,
        X_AXIS_HOURS
    } x_axis_mode = X_AXIS_SECONDS;

    if (max_d_axis <= 0.0f)
    {
        max_d_axis = 60.0f;
    }
    if (max_t_axis_sec <= 0.0f)
    {
        max_t_axis_sec = 20.0f;
    }
    if (max_t_axis_sec > (float)PLAN_TRACK_HOUR_MODE_THRESHOLD_S)
    {
        x_axis_mode = X_AXIS_HOURS;
    }
    else if (max_t_axis_sec > 60.0f)
    {
        x_axis_mode = X_AXIS_MINUTES;
    }
    if (x_step <= 0)
    {
        x_step = 10;
    }

#define MAP_X(t_sec) ((lv_coord_t)(x_axis_left + ((t_sec) / max_t_axis_sec) * w))
#define MAP_Y(d)     (area->y1 + (lv_coord_t)(pad_y_top + ((d) / max_d_axis) * h))

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_t txt_dsc;
    lv_draw_label_dsc_init(&txt_dsc);
    txt_dsc.font = get_font(FONT_ID_SMALL);
    txt_dsc.color = GREEN;

    line_dsc.color = GREEN;
    line_dsc.width = 1;
    line_dsc.opa = 38;
    line_dsc.dash_width = 4;
    line_dsc.dash_gap = 4;

    int y_step = (max_d_axis >= 200.0f) ? 50 : ((max_d_axis >= 100.0f) ? 20 : 10);
    for (int d = 0; d <= (int)max_d_axis; d += y_step)
    {
        lv_coord_t y = MAP_Y((float)d);
        lv_point_t pts[2] =
        {
            {(lv_coord_t)x_axis_left, y},
            {(lv_coord_t)x_axis_right, y}
        };
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", d);
        txt_dsc.opa = 191;
        lv_area_t t_area = {area->x1, y - 10, area->x1 + (lv_coord_t)pad_x - 5, y + 10};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }

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
        if (x_axis_mode == X_AXIS_HOURS)
        {
            snprintf(buf, sizeof(buf), "%d", t / 3600);
        }
        else if (x_axis_mode == X_AXIS_MINUTES)
        {
            snprintf(buf, sizeof(buf), "%d", t / 60);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", t);
        }

        lv_point_t tick_text_size;
        lv_txt_get_size(&tick_text_size, buf, txt_dsc.font, txt_dsc.letter_space,
                        txt_dsc.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t tick_pad_x = 4;
        lv_coord_t lbl_left = x - (tick_text_size.x / 2) - tick_pad_x;
        lv_coord_t lbl_right = x + ((tick_text_size.x + 1) / 2) + tick_pad_x;

        if (lbl_left < area->x1)
        {
            lv_coord_t shift = area->x1 - lbl_left;
            lbl_left += shift;
            lbl_right += shift;
        }
        if (lbl_right > area->x2)
        {
            lv_coord_t shift = lbl_right - area->x2;
            lbl_left -= shift;
            lbl_right -= shift;
        }

        lv_area_t t_area = {lbl_left, area->y2 - 24, lbl_right, area->y2 - 6};
        lv_draw_label(draw_ctx, &txt_dsc, &t_area, buf, NULL);
    }
    txt_dsc.align = LV_TEXT_ALIGN_LEFT;

    lv_draw_label_dsc_t unit_dsc;
    lv_draw_label_dsc_init(&unit_dsc);
    unit_dsc.font = FONT_14;
    unit_dsc.color = LIGHT;
    unit_dsc.opa = 191;

    lv_area_t unit_area =
    {
        area->x1 + 1,
        area->y2 - 22,
        area->x1 + 42,
        area->y2 - 4
    };
    const char *x_unit = "m/s";
    if (x_axis_mode == X_AXIS_HOURS)
    {
        x_unit = "m/h";
    }
    else if (x_axis_mode == X_AXIS_MINUTES)
    {
        x_unit = "m/min";
    }
    lv_draw_label(draw_ctx, &unit_dsc, &unit_area, x_unit, NULL);

    line_dsc.opa = LV_OPA_COVER;
    line_dsc.width = 3;
    line_dsc.dash_width = 0;
    line_dsc.dash_gap = 0;

    lv_point_t last_p;
    if (!depth_chart_draw_profile(draw_ctx, vm->dive_log, vm->dive_log_count, (lv_coord_t)x_axis_left, (lv_coord_t)((float)area->y1 + pad_y_top), (lv_coord_t)w, (lv_coord_t)h, max_t_axis_sec, max_d_axis, line_dsc.color, line_dsc.width, line_dsc.opa, &last_p))
    {
        last_p.x = MAP_X(0.0f);
        last_p.y = MAP_Y(0.0f);
    }

    lv_point_t now_p = {MAP_X(current_t_sec), MAP_Y(current_d)};
    lv_draw_line(draw_ctx, &line_dsc, &last_p, &now_p);

    lv_draw_rect_dsc_t now_dsc;
    lv_draw_rect_dsc_init(&now_dsc);
    now_dsc.radius = LV_RADIUS_CIRCLE;
    now_dsc.bg_color = GREEN;
    now_dsc.bg_opa = LV_OPA_COVER;
    lv_area_t now_area = {now_p.x - PLAN_TRACK_NOW_DOT_RADIUS_PX, now_p.y - PLAN_TRACK_NOW_DOT_RADIUS_PX, now_p.x + PLAN_TRACK_NOW_DOT_RADIUS_PX, now_p.y + PLAN_TRACK_NOW_DOT_RADIUS_PX};
    lv_draw_rect(draw_ctx, &now_dsc, &now_area);

    lv_point_t now_text_size;
    lv_txt_get_size(&now_text_size, "NOW", txt_dsc.font, txt_dsc.letter_space, txt_dsc.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_draw_rect_dsc_t now_bg;
    lv_draw_rect_dsc_init(&now_bg);
    now_bg.bg_color = GREEN;
    now_bg.bg_opa = LV_OPA_COVER;
    lv_coord_t now_bg_x1 = now_p.x + PLAN_TRACK_NOW_DOT_RADIUS_PX + PLAN_TRACK_NOW_LABEL_GAP_PX;
    lv_coord_t now_bg_w = now_text_size.x + PLAN_TRACK_NOW_LABEL_PAD_X_PX * 2;
    lv_coord_t now_bg_h = now_text_size.y + PLAN_TRACK_NOW_LABEL_PAD_Y_PX * 2;
    lv_coord_t now_bg_y1 = now_p.y - now_bg_h / 2 + PLAN_TRACK_NOW_LABEL_OFFSET_Y_PX;
    lv_area_t now_bg_area = {now_bg_x1, now_bg_y1, now_bg_x1 + now_bg_w, now_bg_y1 + now_bg_h};
    lv_draw_rect(draw_ctx, &now_bg, &now_bg_area);

    txt_dsc.color = BLACK;
    txt_dsc.opa = LV_OPA_COVER;
    txt_dsc.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t now_txt_area = {now_bg_area.x1 + PLAN_TRACK_NOW_LABEL_PAD_X_PX, now_bg_area.y1 + (now_bg_h - now_text_size.y) / 2, now_bg_area.x2 - PLAN_TRACK_NOW_LABEL_PAD_X_PX, now_bg_area.y1 + (now_bg_h - now_text_size.y) / 2 + now_text_size.y};
    lv_draw_label(draw_ctx, &txt_dsc, &now_txt_area, "NOW", NULL);
    txt_dsc.color = GREEN;
    txt_dsc.align = LV_TEXT_ALIGN_LEFT;

    if (vm->deco_stop_count > 0U)
    {
        line_dsc.opa = LV_OPA_COVER;
        line_dsc.width = 3;

        float cur_t_sec = current_t_sec;
        float draw_d = current_d;
        lv_point_t p1 = now_p;
        bool safety_stop_label = bus_get_stop_type() == STOP_SAFETY;
        bool show_stop_time = safety_stop_label || (PLAN_TRACK_DECO_STOP_TIME_LABELS_ENABLED != 0);
        const lv_font_t *normal_stop_font = txt_dsc.font;
        txt_dsc.font = FONT_TRACK;

        for (uint8_t i = 0U; i < vm->deco_stop_count; i++)
        {
            float asc_t = (draw_d > vm->deco_stops[i].depth_m)
                          ? (draw_d - vm->deco_stops[i].depth_m) * 6.0f : 0.0f;
            cur_t_sec += asc_t;
            lv_point_t p2 = {MAP_X(cur_t_sec), MAP_Y(vm->deco_stops[i].depth_m)};
            draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p2);

            float hold_t_sec = vm->deco_stops[i].stay_min * 60.0f;
            cur_t_sec += hold_t_sec;
            lv_point_t p3 = {MAP_X(cur_t_sec), MAP_Y(vm->deco_stops[i].depth_m)};
            draw_diagonal_dashed_line(draw_ctx, &line_dsc, p2, p3);

            lv_coord_t label_anchor_x = MAP_X(cur_t_sec - hold_t_sec / 2.0f);

            char d_buf[16];
            plan_track_format_stop_label(d_buf, sizeof(d_buf), &vm->deco_stops[i], show_stop_time);
            lv_coord_t label_x1;
            lv_coord_t label_x2;
            lv_area_t d_txt;
            txt_dsc.opa = LV_OPA_COVER;
            if (safety_stop_label)
            {
                label_x1 = label_anchor_x - PLAN_TRACK_STOP_LABEL_W_PX / 2;
                label_x2 = label_anchor_x + PLAN_TRACK_STOP_LABEL_W_PX / 2;
                if (label_x1 < (lv_coord_t)x_axis_left)
                {
                    label_x1 = (lv_coord_t)x_axis_left;
                    label_x2 = label_x1 + PLAN_TRACK_STOP_LABEL_W_PX;
                }
                if (label_x2 > (lv_coord_t)x_axis_right)
                {
                    label_x2 = (lv_coord_t)x_axis_right;
                    label_x1 = label_x2 - PLAN_TRACK_STOP_LABEL_W_PX;
                }
                d_txt = (lv_area_t){label_x1, p2.y - 30 + PLAN_TRACK_STOP_LABEL_OFFSET_Y_PX, label_x2, p2.y - 12 + PLAN_TRACK_STOP_LABEL_OFFSET_Y_PX};
                txt_dsc.align = LV_TEXT_ALIGN_CENTER;
            }
            else
            {
                label_x2 = label_anchor_x - PLAN_TRACK_STOP_LABEL_GAP_PX;
                label_x1 = label_x2 - PLAN_TRACK_STOP_LABEL_W_PX;
                if (label_x2 <= (lv_coord_t)x_axis_left) label_x2 = (lv_coord_t)x_axis_left + PLAN_TRACK_STOP_LABEL_W_PX;
                if (label_x1 < (lv_coord_t)x_axis_left) label_x1 = (lv_coord_t)x_axis_left;
                d_txt = (lv_area_t){label_x1, p2.y - 20 + PLAN_TRACK_STOP_LABEL_OFFSET_Y_PX, label_x2, p2.y - 4 + PLAN_TRACK_STOP_LABEL_OFFSET_Y_PX};
                txt_dsc.align = LV_TEXT_ALIGN_RIGHT;
            }
            lv_draw_label(draw_ctx, &txt_dsc, &d_txt, d_buf, NULL);
            txt_dsc.align = LV_TEXT_ALIGN_LEFT;
            txt_dsc.opa = 191;

            p1 = p3;
            draw_d = vm->deco_stops[i].depth_m;
        }
        txt_dsc.font = normal_stop_font;

        cur_t_sec += (draw_d > 0.0f) ? draw_d * 6.0f : 0.0f;
        lv_point_t p_end = {MAP_X(cur_t_sec), MAP_Y(0.0f)};
        draw_diagonal_dashed_line(draw_ctx, &line_dsc, p1, p_end);
    }

#undef MAP_X
#undef MAP_Y
}

void card_plan_create(lv_obj_t *parent)
{
    render_card_title(parent, "DIVE PLAN TRACK");

    int right_w = (int)ui_content_w_get();
    int tile_h = (int)ui_content_h_get();
    int chart_x = CHART_PAD;
    int chart_y = CARD_TITLE_H;
    int chart_w = right_w - CHART_PAD * 2;
    int chart_h = tile_h - CARD_TITLE_H - CHART_PAD;

    if (chart_h < 60)
    {
        chart_h = 60;
    }

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

    s_chart_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_chart_obj);
    lv_obj_set_size(s_chart_obj, chart_w, chart_h);
    lv_obj_set_pos(s_chart_obj, chart_x, chart_y);
    lv_obj_add_event_cb(s_chart_obj, plan_chart_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

void card_plan_update(const ui_vm_plan_chart_t *vm)
{
    if (vm != NULL)
    {
        s_plan_chart_vm = *vm;
    }

    if (!screen_page_id_refresh_visible(PAGE_ID_PLAN))
    {
        return;
    }

    if (s_chart_obj != NULL && lv_obj_is_valid(s_chart_obj))
    {
        lv_obj_invalidate(s_chart_obj);
    }
}
```
## src/ui/comp/depth_chart_renderer.h (full file)

```c
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
```

# 2. PLAN ViewModel

## src/ui/core/vm/ui_vm_plan_chart_types.h (full file)

```c
/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_chart_types.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_PLAN_CHART_TYPES_H
#define UI_VM_PLAN_CHART_TYPES_H

#include <stdint.h>
#include "../ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_vm_plan_chart
{
    uint8_t draw_enabled;
    uint8_t dive_log_count;
    uint8_t deco_stop_count;
    uint16_t x_step_s;
    float current_time_s;
    float current_depth_m;
    float predicted_total_time_s;
    float max_depth_axis_m;
    float max_time_axis_s;
    dive_pt_t dive_log[MAX_DIVE_LOG];
    deco_stop_t deco_stops[MAX_DECO_STOPS];
} ui_vm_plan_chart_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_PLAN_CHART_TYPES_H */
```

## src/ui/core/vm/ui_vm_plan_chart.h (full file)

```c
/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_chart.h
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_VM_PLAN_CHART_H
#define UI_VM_PLAN_CHART_H

#include "ui_vm_plan_chart_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_vm_plan_chart_update(ui_vm_plan_chart_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_PLAN_CHART_H */
```

## src/ui/core/vm/ui_vm_plan_chart.c (full file)

```c
/*
 * 文件: src/app_ui/ui/core/vm/ui_vm_plan_chart.c
 * 作用: 该文件属于 ViewModel 子模块，负责把底层运行数据整理为界面可直接消费的展示数据结构。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_vm_plan_chart.h"

#include "../data.h"
#include "../ui_defs.h"

#include <math.h>
#include <string.h>

static float vm_plan_max_log_depth(uint8_t count, const dive_pt_t *points, float current_depth_m)
{
    float max_depth = current_depth_m;

    for (uint8_t i = 0U; i < count; i++)
    {
        if (points[i].depth_m > max_depth)
        {
            max_depth = points[i].depth_m;
        }
    }

    return max_depth;
}

static float vm_plan_predicted_total_time(float current_time_s,
                                          float current_depth_m,
                                          uint8_t stop_count,
                                          const deco_stop_t *stops)
{
    float predicted_t_sec = current_time_s;
    float sim_d = current_depth_m;

    for (uint8_t i = 0U; i < stop_count; i++)
    {
        float asc_t = (sim_d > stops[i].depth_m) ? (sim_d - stops[i].depth_m) * 6.0f : 0.0f;
        predicted_t_sec += asc_t;
        predicted_t_sec += stops[i].stay_min * 60.0f;
        sim_d = stops[i].depth_m;
    }

    if (sim_d > 0.0f)
    {
        predicted_t_sec += sim_d * 6.0f;
    }

    return predicted_t_sec;
}

static float vm_plan_depth_axis(float max_log_d)
{
    float axis = 60.0f;

    if (max_log_d >= axis * 0.9f)
    {
        axis = ceilf((max_log_d + 15.0f) / 20.0f) * 20.0f;
    }

    return axis;
}

static float vm_plan_time_axis(float current_time_s, float predicted_t_sec)
{
    float target_max_t_sec = fmaxf(current_time_s, predicted_t_sec) * (1.0f + PLAN_TRACK_TIME_AXIS_HEADROOM_PCT / 100.0f);
    float axis = 20.0f;

    if (target_max_t_sec < 20.0f)
    {
        target_max_t_sec = 20.0f;
    }

    if (target_max_t_sec > (float)PLAN_TRACK_HOUR_MODE_THRESHOLD_S)
    {
        axis = ceilf(target_max_t_sec / 3600.0f) * 3600.0f;
    }
    else if (target_max_t_sec > 60.0f)
    {
        axis = ceilf(target_max_t_sec / 60.0f) * 60.0f;
    }
    else
    {
        axis = ceilf(target_max_t_sec / 10.0f) * 10.0f;
    }

    return axis;
}

static uint16_t vm_plan_time_step(float max_t_axis_sec)
{
    uint16_t x_step = 10U;

    if (max_t_axis_sec > 28800.0f)
    {
        x_step = 14400U;
    }
    else if (max_t_axis_sec > 14400.0f)
    {
        x_step = 7200U;
    }
    else if (max_t_axis_sec > (float)PLAN_TRACK_HOUR_MODE_THRESHOLD_S)
    {
        x_step = 3600U;
    }
    else if (max_t_axis_sec > 3600.0f)
    {
        x_step = PLAN_TRACK_LONG_MINUTE_STEP_S;
    }
    else if (max_t_axis_sec > 1200.0f)
    {
        x_step = 600U;
    }
    else if (max_t_axis_sec > 600.0f)
    {
        x_step = 300U;
    }
    else if (max_t_axis_sec > 300.0f)
    {
        x_step = 120U;
    }
    else if (max_t_axis_sec > 60.0f)
    {
        x_step = 60U;
    }
    else
    {
    }

    return x_step;
}

void ui_vm_plan_chart_update(ui_vm_plan_chart_t *vm)
{
    uint8_t dive_log_count;
    uint8_t deco_stop_count;

    if (vm == NULL)
    {
        return;
    }

    (void)memset(vm, 0, sizeof(*vm));

    vm->current_time_s = (float)bus_get_dive_time_s();
    vm->current_depth_m = bus_get_depth();

    dive_log_count = bus_get_dive_log_count();
    deco_stop_count = bus_get_deco_stop_count();

    vm->dive_log_count = dive_log_count;
    vm->deco_stop_count = deco_stop_count;
    vm->draw_enabled = ((vm->current_depth_m > 0.5f) || (dive_log_count > 0U)) ? 1U : 0U;

    for (uint8_t i = 0U; i < dive_log_count; i++)
    {
        (void)bus_get_dive_log_point(i, &vm->dive_log[i]);
    }

    if (bus_get_stop_type() == STOP_SAFETY && bus_get_stop_depth_m() > 0.0f)
    {
        uint16_t safety_s = bus_get_in_stop_zone() ? bus_get_stop_time_left_s() : bus_get_stop_time_total_s();
        if (safety_s == 0U)
        {
            safety_s = bus_get_stop_time_total_s();
        }
        if (safety_s > 0U)
        {
            vm->deco_stop_count = 1U;
            vm->deco_stops[0].depth_m = bus_get_stop_depth_m();
            vm->deco_stops[0].stay_min = (float)safety_s / 60.0f;
            deco_stop_count = 1U;
        }
        else
        {
            vm->deco_stop_count = 0U;
            deco_stop_count = 0U;
        }
    }
    else
    {
        for (uint8_t i = 0U; i < deco_stop_count; i++)
        {
            (void)bus_get_deco_stop(i, &vm->deco_stops[i]);
        }
    }

    vm->predicted_total_time_s = vm_plan_predicted_total_time(vm->current_time_s,
                                                              vm->current_depth_m,
                                                              deco_stop_count,
                                                              vm->deco_stops);
    vm->max_depth_axis_m = vm_plan_depth_axis(vm_plan_max_log_depth(dive_log_count,
                                                                    vm->dive_log,
                                                                    vm->current_depth_m));
    vm->max_time_axis_s = vm_plan_time_axis(vm->current_time_s, vm->predicted_total_time_s);
    vm->x_step_s = vm_plan_time_step(vm->max_time_axis_s);
}
```

# 3. Drawing configuration, fonts, and shared layout

## Chart, title, and color macros

Source: `src/ui/core/ui_defs.h`, current lines 28-52.

```c
#define CARD_TITLE_H              60      /* 卡片/菜单标题区高度 */
#define CARD_TITLE_TEXT_Y         8       /* 卡片/菜单标题文字Y */
#define CARD_TITLE_TEXT_H         40      /* 卡片/菜单标题文字高度 */
#define CARD_TITLE_LINE_Y         48      /* 卡片/菜单标题下划线Y */
#define CARD_TITLE_LINE_H         2       /* 卡片/菜单标题下划线高度 */
#define DECO_REFRESH_MS           1000    /* DECO 卡片刷新周期，单位 ms */
#define UI_NDL_STOP_TIME_MINUTE_ONLY  1U  /* NDL/停留时间只显示向上取整分钟 */
#define PLAN_TRACK_DECO_STOP_TIME_LABELS_ENABLED  1  /* 减压站标签停留时间开关 */
#define PLAN_TRACK_STOP_LABEL_W_PX                72  /* 停站标签最大宽度 */
#define PLAN_TRACK_STOP_LABEL_GAP_PX              6  /* 停站标签距节点距离 */
#define PLAN_TRACK_STOP_LABEL_OFFSET_Y_PX         4  /* 停站标签整体纵向微调 */
#define PLAN_TRACK_NOW_DOT_RADIUS_PX              5  /* NOW 当前点半径 */
#define PLAN_TRACK_HOUR_MODE_THRESHOLD_S       10800U /* PLAN图超过该秒数才切小时刻度 */
#define PLAN_TRACK_LONG_MINUTE_STEP_S           1800U /* PLAN图1~3h分钟刻度步进 */
#define PLAN_TRACK_TIME_AXIS_HEADROOM_PCT        15.0f /* PLAN图X轴右侧时间余量百分比 */
#define UI_SCROLL_DOTS_AUTO_HIDE_MS            3000U /* 楼层指示点无操作自动隐藏时间 */
#define PLAN_TRACK_NOW_LABEL_GAP_PX               8  /* NOW 当前点到文字框距离 */
#define PLAN_TRACK_NOW_LABEL_OFFSET_Y_PX          16  /* NOW 文字框纵向偏移 */
#define PLAN_TRACK_NOW_LABEL_PAD_X_PX             4  /* NOW 文字框横向内边距 */
#define PLAN_TRACK_NOW_LABEL_PAD_Y_PX             2  /* NOW 文字框纵向内边距 */

#define GREEN  lv_color_make(0x00, 0xFF, 0x00)  /* 主荧光绿 */
#define LIGHT  lv_color_make(0x55, 0xFF, 0x55)  /* 高亮文字色 */
#define DARK   lv_color_make(0x00, 0x33, 0x00)  /* 暗边框色 */
#define BLACK  lv_color_make(0x00, 0x00, 0x00)  /* 黑色底色 */
```

## Chart shell border macro

Source: `src/ui/core/ui_defs.h`, current lines 64-64.

```c
#define GRID_BORDER_W             0       /* 自定义网格辅助边框宽度 */
```

## Font ID definition

Source: `src/ui/core/ui_defs.h`, current lines 99-108.

```c
typedef enum
{
    FONT_ID_SMALL = 0,
    FONT_ID_TITLE,
    FONT_ID_MEDIUM,
    FONT_ID_BIG_TITLE,
    FONT_ID_LARGE,
    FONT_ID_HUGE,
    FONT_ID_NDL,
} font_id_t;
```

## PLAN font definition in the active font branch

Source: `src/ui/fonts/fonts.h`, current lines 107-125.

```c
#elif defined(USE_FONT_ORDINAR)
#define FONT_SMALL    (&lv_font_ordinar_20)  /* 20px  小字体 - 标签/单位/Badge/标题 */
#define FONT_14       (&lv_font_ordinar_14)  /* 14px  极小辅助文字 */
#define FONT_TITLE    (&lv_font_ordinar_20)  /* 20px  菜单项/卡片标题 */
#define FONT_MEDIUM   (&lv_font_ordinar_32)  /* 32px  中字体 - 数据值 */
#define FONT_40       (&lv_font_ordinar_40)  /* 40px  大模块标题 */
#define FONT_LARGE    (&lv_font_ordinar_64)  /* 64px  大字体 - 深度大数字 */
#define FONT_HUGE     (&lv_font_ordinar_64)  /* 64px  大字体 */
#define FONT_NDL      (&lv_font_ordinar_58)  /* 56px  NDL减压时间专用 */
#define FONT_DERIVED  (&lv_font_ordinar_20)  /* 20px  派生 */
#define FONT_24       (&lv_font_ordinar_24)  /* 24px  中等标题 */
#define FONT_TRACK    (&lv_font_ordinar_14)  /* PLAN图减压站标签专用14px适配字体 */
#else
#define FONT_SMALL    (lv_font_montserrat_20)
#define FONT_14       (lv_font_montserrat_14)
#define FONT_TITLE    (lv_font_montserrat_20)
#define FONT_MEDIUM   (lv_font_montserrat_32)
#define FONT_40       (lv_font_montserrat_40)
#define FONT_LARGE    (lv_font_montserrat_64)
```

## Font ID to LVGL font mapping

Source: `src/ui/core/ui_engine.c`, current lines 377-413.

```c
/* =========================================================
 * 字体映射(Font Mapper)
 *
 * 全系统唯一允许将字ID 转换为真lvgl 字体指针的地方
 * 所有配置结构体中保存的 title_font / val_font 均应font_id_t 值
 *
 * ID 映射表：
 *   FONT_ID_SMALL  (0) 20px  标签/单位/Badge
 *   FONT_ID_TITLE  (1) 20px  菜单卡片标题
 *   FONT_ID_MEDIUM (2) 32px  数据
 *   FONT_ID_BIG_TITLE (3) 40px  大模块标题
 *   FONT_ID_LARGE  (4) 64px  深度大数
 *   FONT_ID_HUGE   (5) 64px  大字
 *   FONT_ID_NDL    (6) 48px  NDL减压时间
 * ========================================================= */
const lv_font_t *get_font(uint8_t font_id)
{
    switch (font_id)
    {
    case FONT_ID_SMALL:
        return FONT_SMALL;   /* 20px */
    case FONT_ID_TITLE:
        return FONT_TITLE;   /* 20px */
    case FONT_ID_MEDIUM:
        return FONT_MEDIUM;  /* 32px */
    case FONT_ID_BIG_TITLE:
        return FONT_40;      /* 40px */
    case FONT_ID_LARGE:
        return FONT_LARGE;   /* 64px */
    case FONT_ID_HUGE:
        return FONT_HUGE;    /* 64px */
    case FONT_ID_NDL:
        return FONT_NDL;     /* 48px */
    default:
        return FONT_SMALL;   /* 兜底：永不为 NULL */
    }
}
```

## Card title drawing function

Source: `src/ui/screen/layout_view.c`, current lines 352-372.

```c
void render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    /* 卡片标题统一用同一套位置、尺寸和省略规则，避免各卡片样式漂移。 */
    uint16_t right_w = ui_content_w_get();

    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, CARD_TITLE_TEXT_Y);
    lv_obj_set_size(lbl, right_w - 32, CARD_TITLE_TEXT_H);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, LIGHT, 0);

    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, CARD_TITLE_LINE_H);
    lv_obj_set_pos(line, 16, CARD_TITLE_LINE_Y);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, DARK, 0);
}
```

## PLAN card content dimensions

Source: `src/ui/core/data.c`, current lines 2281-2305.

```c
uint16_t ui_content_w_get(void)
{
    uint16_t gap = ui_panel_gap_px_get();

    if (ui_layout_is_vertical_split())
    {
        return (ui_safe_zone_w_get() > LEFT_ANCHOR_W + gap)
               ? (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W - gap)
               : 0U;
    }

    return ui_safe_zone_w_get();
}

uint16_t ui_content_h_get(void)
{
    if (ui_layout_is_vertical_split())
    {
        return ui_safe_zone_h_get();
    }

    return (ui_safe_zone_h_get() > TOP_ANCHOR_H)
           ? (uint16_t)(ui_safe_zone_h_get() - TOP_ANCHOR_H)
           : 0U;
}
```

## PLAN page visibility check

Source: `src/ui/screen/screen.c`, current lines 813-816.

```c
bool screen_page_id_refresh_visible(page_id_t page_id)
{
    return page_id_at(s_visible_tile_pos) == page_id;
}
```

# 4. Track and deco stop data structures

## Stop type definition

Source: `src/ui/core/ui_types.h`, current lines 96-101.

```c
typedef enum
{
    STOP_NONE = 0,
    STOP_SAFETY,
    STOP_DECO
} stop_type_t;
```

## Dive track point definition

Source: `src/ui/core/ui_types.h`, current lines 238-242.

```c
typedef struct
{
    float time_s;
    float depth_m;
} dive_pt_t;
```

## Deco stop definition and point limits

Source: `src/ui/core/ui_types.h`, current lines 280-289.

```c
typedef struct
{
    float depth_m;
    float stay_min;
} deco_stop_t;

#define MAX_DIVE_LOG   200
#ifndef MAX_DECO_STOPS
#define MAX_DECO_STOPS 10
#endif
```

## Track and deco stop storage

Source: `src/ui/core/data.c`, current lines 39-44.

```c
static dive_pt_t s_dive_log[MAX_DIVE_LOG] __attribute__((section(".psram_bss")));
static uint16_t s_dive_log_count;
static bool s_dive_log_sample_valid;
static float s_dive_log_last_sample_time_s;
static deco_stop_t s_deco_stops[MAX_DECO_STOPS];
static uint16_t s_deco_stop_count;
```

## Default sampling period and option count

Source: `src/ui/core/ui_settings.h`, current lines 20-21.

```c
#define UI_LOG_RATE_DEFAULT_S      2U
#define UI_LOG_RATE_OPTION_COUNT   4U
```

## Track sampling period options

Source: `src/ui/core/ui_settings.h`, current lines 100-124.

```c
static inline uint8_t ui_log_rate_option(uint8_t index)
{
    static const uint8_t rate_table[UI_LOG_RATE_OPTION_COUNT] =
    {
        2U, 5U, 10U, 30U
    };

    if (index >= UI_LOG_RATE_OPTION_COUNT)
    {
        return UI_LOG_RATE_DEFAULT_S;
    }
    return rate_table[index];
}

static inline bool ui_log_rate_is_valid(uint8_t seconds)
{
    for (uint8_t i = 0U; i < UI_LOG_RATE_OPTION_COUNT; i++)
    {
        if (seconds == ui_log_rate_option(i))
        {
            return true;
        }
    }
    return false;
}
```

# 5. Data Bus declarations and implementation

## Deco stop write declaration

Source: `src/ui/core/data.h`, current lines 168-169.

```c
/* 完整减压站序列（>32bit，必须包临界区） */
void bus_set_deco_plan(const deco_stop_t *stops, uint8_t count);
```

## Layout dimensions and current PLAN state declarations

Source: `src/ui/core/data.h`, current lines 254-281.

```c
uint16_t ui_content_w_get(void);
uint16_t ui_content_h_get(void);
uint8_t ui_dots_position_get(void);
uint8_t ui_depth_h_u_get(void);
uint8_t ui_ndl_h_u_get(void);
uint8_t ui_pod_h_u_get(void);
uint8_t ui_batt_h_u_get(void);
uint8_t ui_gas_h_u_get(void);
uint8_t ui_time_h_u_get(void);
uint8_t ui_left_widget_count_get(void);
const grid_widget_t *ui_left_widget_get(uint8_t index);
uint8_t ui_custom_card_count_get(void);
const char *ui_custom_card_title_get(uint8_t custom_card_idx);
uint8_t ui_custom_card_widget_count_get(uint8_t custom_card_idx);
const grid_widget_t *ui_custom_card_widget_get(uint8_t custom_card_idx, uint8_t widget_idx);
uint8_t ui_custom_card_slot_get(uint8_t storage_pos);
float bus_get_depth(void);
float bus_get_stop_depth_m(void);
stop_type_t bus_get_stop_type(void);
uint8_t bus_get_ndl_bar_pct(void);
uint16_t bus_get_stop_time_total_s(void);
uint16_t bus_get_stop_time_left_s(void);
bool bus_get_in_stop_zone(void);
int16_t bus_get_ndl(void);
int16_t bus_get_ndl_stop_value(void);
float bus_get_max_depth(void);
float bus_get_avg_depth(void);
uint32_t bus_get_dive_time_s(void);
```

## Units, track, and deco stop read declarations

Source: `src/ui/core/data.h`, current lines 380-410.

```c
uint8_t bus_get_log_rate(void);
bool bus_get_time_24h_enabled(void);
uint8_t bus_get_units_mode(void);
const char *bus_get_depth_unit_label(void);
const char *bus_get_depth_units_label(void);
float bus_get_depth_display(float depth_m);
uint8_t bus_get_date_format(void);
uint8_t bus_get_temperature_unit(void);
const char *bus_get_temperature_unit_label(void);
float bus_get_temperature_display(float temp_c);
uint8_t bus_get_safety_stop_mode(void);
uint8_t bus_get_surface_confirm_min(void);
float bus_get_dive_start_depth_m(void);
bool bus_get_depth_comp_enabled(void);
float bus_get_depth_comp_m(void);
float bus_get_deco_input_depth_m(float raw_depth_m);
uint8_t bus_get_altitude_level(void);
uint16_t bus_get_depth_alarm_m(void);
uint16_t bus_get_time_alarm_min(void);
uint16_t bus_get_ndl_alarm_min(void);
bool bus_is_heading_locked(void);
uint16_t bus_get_heading(void);
uint16_t bus_get_heading_target(void);
void bus_lock_heading_to_current(void);
void bus_clear_heading_lock(void);

/* --- 历史轨迹 / 减压停留原始数据只读接口 --- */
uint8_t bus_get_dive_log_count(void);
bool bus_get_dive_log_point(uint8_t index, dive_pt_t *out_point);
uint8_t bus_get_deco_stop_count(void);
bool bus_get_deco_stop(uint8_t index, deco_stop_t *out_stop);
```

## Dive track write declarations

Source: `src/ui/core/data.h`, current lines 433-436.

```c
/* --- 历史轨迹推流 --- */
void dive_log_append(float current_time_s, float current_depth_m);
void dive_log_append_sampled(float current_time_s, float current_depth_m);
void dive_log_reset(void);
```

## Dive track thinning algorithm

Source: `src/ui/core/data.c`, current lines 563-618.

```c
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
    if (index >= s_dive_log_count)
    {
        return;
    }
    if (index + 1U < s_dive_log_count)
    {
        (void)memmove(&s_dive_log[index],
                      &s_dive_log[index + 1U],
                      (s_dive_log_count - index - 1U) * sizeof(s_dive_log[0]));
    }
    s_dive_log_count--;
}

static void dive_log_make_room(void)
{
    if (s_dive_log_count < MAX_DIVE_LOG)
    {
        return;
    }
    if (s_dive_log_count < 3U)
    {
        return;
    }

    {
        uint16_t drop_index = 1U;
        float drop_area = FLT_MAX;

        for (uint16_t i = 1U; i + 1U < s_dive_log_count; i++)
        {
            float area = dive_log_triangle_area(&s_dive_log[i - 1U],
                                                &s_dive_log[i],
                                                &s_dive_log[i + 1U]);
            if (area < drop_area)
            {
                drop_area = area;
                drop_index = i;
            }
        }

        dive_log_remove_at(drop_index);
    }
}
```

## Deco plan equality check

Source: `src/ui/core/data.c`, current lines 620-647.

```c
static bool deco_plan_equals_current(const deco_stop_t *stops, uint8_t count)
{
    if (s_deco_stop_count != count)
    {
        return false;
    }

    if (count == 0U)
    {
        return true;
    }

    if (stops == NULL)
    {
        return false;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        if ((fabsf(s_deco_stops[i].depth_m - stops[i].depth_m) > 0.01f) ||
                (fabsf(s_deco_stops[i].stay_min - stops[i].stay_min) > 0.01f))
        {
            return false;
        }
    }

    return true;
}
```

## Deco stop sequence write

Source: `src/ui/core/data.c`, current lines 1284-1310.

```c
/* 完整减压站序列写入（可变长度，必须包临界区） */
void bus_set_deco_plan(const deco_stop_t *stops, uint8_t count)
{
    if (count > MAX_DECO_STOPS)
    {
        count = MAX_DECO_STOPS;
    }
    if (stops == NULL)
    {
        count = 0U;
    }

    rt_base_t level = rt_hw_interrupt_disable();
    if (deco_plan_equals_current(stops, count))
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    s_deco_stop_count = count;
    if ((count > 0U) && (stops != NULL))
    {
        (void)memcpy(s_deco_stops, stops, count * sizeof(deco_stop_t));
    }
    g_sensor_data.dirty_mask |= DIRTY_PLAN;
    rt_hw_interrupt_enable(level);
}
```

## Current depth, stop state, and dive time reads

Source: `src/ui/core/data.c`, current lines 2397-2455.

```c
float bus_get_depth(void)
{
    return g_sensor_data.depth;
}

float bus_get_stop_depth_m(void)
{
    return g_sensor_data.stop_depth_m;
}

stop_type_t bus_get_stop_type(void)
{
    return g_sensor_data.stop_type;
}

uint8_t bus_get_ndl_bar_pct(void)
{
    return g_sensor_data.ndl_bar_pct;
}

uint16_t bus_get_stop_time_total_s(void)
{
    return g_sensor_data.stop_time_total_s;
}

uint16_t bus_get_stop_time_left_s(void)
{
    return g_sensor_data.stop_time_left_s;
}

bool bus_get_in_stop_zone(void)
{
    return g_sensor_data.in_stop_zone;
}

int16_t bus_get_ndl(void)
{
    return g_sensor_data.ndl;
}

int16_t bus_get_ndl_stop_value(void)
{
    return g_sensor_data.ndl_stop_value;
}

float bus_get_max_depth(void)
{
    return g_sensor_data.max_depth;
}

float bus_get_avg_depth(void)
{
    return g_sensor_data.avg_depth;
}

uint32_t bus_get_dive_time_s(void)
{
    return g_sensor_data.dive_time_s;
}
```

## Depth unit and display conversion

Source: `src/ui/core/data.c`, current lines 3061-3074.

```c
const char *bus_get_depth_unit_label(void)
{
    return ui_depth_unit_label(bus_get_units_mode());
}

const char *bus_get_depth_units_label(void)
{
    return ui_depth_units_label(bus_get_units_mode());
}

float bus_get_depth_display(float depth_m)
{
    return ui_depth_display_from_m(depth_m, bus_get_units_mode());
}
```

## Dive track and deco stop reads

Source: `src/ui/core/data.c`, current lines 3153-3193.

```c
uint8_t bus_get_dive_log_count(void)
{
    if (s_dive_log_count > MAX_DIVE_LOG)
    {
        return (uint8_t)MAX_DIVE_LOG;
    }

    return (uint8_t)s_dive_log_count;
}

bool bus_get_dive_log_point(uint8_t index, dive_pt_t *out_point)
{
    if ((out_point == NULL) || (index >= s_dive_log_count))
    {
        return false;
    }

    *out_point = s_dive_log[index];
    return true;
}

uint8_t bus_get_deco_stop_count(void)
{
    if (s_deco_stop_count > MAX_DECO_STOPS)
    {
        return (uint8_t)MAX_DECO_STOPS;
    }

    return (uint8_t)s_deco_stop_count;
}

bool bus_get_deco_stop(uint8_t index, deco_stop_t *out_stop)
{
    if ((out_stop == NULL) || (index >= s_deco_stop_count))
    {
        return false;
    }

    *out_stop = s_deco_stops[index];
    return true;
}
```

## Dive track append, sampling, and reset

Source: `src/ui/core/data.c`, current lines 3229-3316.

```c
void dive_log_append(float current_time_s, float current_depth_m)
{
    if (current_time_s < 0.0f)
    {
        return;
    }

    if (s_dive_log_count > 0U)
    {
        dive_pt_t *last = &s_dive_log[s_dive_log_count - 1U];

        if (current_time_s < last->time_s)
        {
            return;
        }

        if (fabsf(current_time_s - last->time_s) < 0.001f)
        {
            if (fabsf(last->depth_m - current_depth_m) < 0.001f)
            {
                last->depth_m = current_depth_m;
                return;
            }

            if (s_dive_log_count >= MAX_DIVE_LOG)
            {
                dive_log_make_room();
            }

            if (s_dive_log_count < MAX_DIVE_LOG)
            {
                s_dive_log[s_dive_log_count].time_s  = current_time_s;
                s_dive_log[s_dive_log_count].depth_m = current_depth_m;
                s_dive_log_count++;
                bus_mark_dirty(DIRTY_PLAN);
            }
            return;
        }
    }

    if (s_dive_log_count >= MAX_DIVE_LOG)
    {
        dive_log_make_room();
    }

    if (s_dive_log_count < MAX_DIVE_LOG)
    {
        s_dive_log[s_dive_log_count].time_s  = current_time_s;
        s_dive_log[s_dive_log_count].depth_m = current_depth_m;
        s_dive_log_count++;
        bus_mark_dirty(DIRTY_PLAN);
    }
}

void dive_log_append_sampled(float current_time_s, float current_depth_m)
{
    float log_rate_s = (float)bus_get_log_rate();

    if (current_time_s < 0.0f)
    {
        return;
    }

    if (s_dive_log_sample_valid)
    {
        if (current_time_s < s_dive_log_last_sample_time_s)
        {
            return;
        }

        if ((current_time_s - s_dive_log_last_sample_time_s) < log_rate_s)
        {
            return;
        }
    }

    dive_log_append(current_time_s, current_depth_m);
    s_dive_log_last_sample_time_s = current_time_s;
    s_dive_log_sample_valid = true;
}

void dive_log_reset(void)
{
    s_dive_log_count = 0U;
    s_dive_log_sample_valid = false;
    s_dive_log_last_sample_time_s = 0.0f;
    s_deco_stop_count = 0U;
}
```

# 6. Dirty refresh routing and page registration

## PLAN dirty bit definition

Source: `src/ui/core/ui_dirty.h`, current lines 17-43.

```c
typedef uint32_t dirty_mask_t;

typedef enum
{
    DIRTY_NONE         = 0,
    DIRTY_DIVE_PROFILE = (1U << 0),
    DIRTY_DECO_STATUS  = (1U << 1),
    DIRTY_TISSUE_TOX   = (1U << 2),
    DIRTY_GAS_SUPPLY   = (1U << 3),
    DIRTY_SYSTEM       = (1U << 4),
    DIRTY_COMPASS      = (1U << 5),
    DIRTY_SENSOR       = (1U << 6),
    DIRTY_PLAN         = (1U << 7),
    DIRTY_DIVE_CONFIG  = (1U << 8),
    DIRTY_ALARM        = (1U << 9),
    DIRTY_LOGBOOK      = (1U << 10),
    DIRTY_UI_LAYOUT    = (1U << 11),
} dirty_bit_t;

#define DIRTY_DATA_ALL  (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | \
                         DIRTY_GAS_SUPPLY | DIRTY_SYSTEM | DIRTY_COMPASS | \
                         DIRTY_SENSOR | DIRTY_PLAN | DIRTY_DIVE_CONFIG | \
                         DIRTY_LOGBOOK | DIRTY_ALARM)

#define DIRTY_INFO_REFRESH_MASK  (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | \
                                  DIRTY_GAS_SUPPLY | DIRTY_SYSTEM | DIRTY_COMPASS | \
                                  DIRTY_SENSOR | DIRTY_PLAN | DIRTY_DIVE_CONFIG | DIRTY_LOGBOOK)
```

## PLAN page ID definition

Source: `src/ui/screen/page_registry_types.h`, current lines 33-45.

```c
typedef enum
{
    PAGE_ID_INFO        = 0,
    PAGE_ID_COMPASS     = 1,
    PAGE_ID_DECO        = 2,
    PAGE_ID_GAS         = 3,
    PAGE_ID_PLAN        = 4,
    PAGE_ID_CUSTOM_GRID = 5,
    PAGE_ID_BLANK       = 6,
    PAGE_ID_SETUP       = 7,
    PAGE_ID_MENU        = 8,
    PAGE_ID_COUNT
} page_id_t;
```

## Visible PLAN page dirty subscription

Source: `src/ui/core/update_router.c`, current lines 250-271.

```c
static dirty_mask_t ui_router_layout_subscription_mask(const ui_router_visible_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return DIRTY_NONE;
    }

    switch (ctx->page_id)
    {
    case PAGE_ID_COMPASS:
        return ctx->visible_widget_mask | DIRTY_COMPASS;
    case PAGE_ID_DECO:
        return ctx->visible_widget_mask |
               DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | DIRTY_DIVE_CONFIG;
    case PAGE_ID_GAS:
        return ctx->visible_widget_mask | DIRTY_GAS_SUPPLY;
    case PAGE_ID_PLAN:
        return ctx->visible_widget_mask | DIRTY_PLAN;
    default:
        return ctx->visible_widget_mask;
    }
}
```

## DIRTY_PLAN to PLAN VM refresh branch

Source: `src/ui/core/update_router.c`, current lines 487-496.

```c
    if (mask & DIRTY_PLAN)
    {
        uint32_t start_ms = lv_tick_get();
        ui_vm_plan_chart_update(&plan_chart_vm);
        if (ui_router_visible_page_id(&visible_ctx, PAGE_ID_PLAN))
        {
            page_registry_update_plan_vm(&plan_chart_vm);
        }
        plan_ms += lv_tick_get() - start_ms;
    }
```

## PLAN VM page bridge

Source: `src/ui/screen/page_registry.c`, current lines 42-46.

```c
static void page_plan_update_vm_bridge(const void *vm)
{
    /* 计划页的 VM 数据需要先转回具体类型，再交给卡片更新函数。 */
    card_plan_update((const ui_vm_plan_chart_t *)vm);
}
```

## PLAN page registration entry

Source: `src/ui/screen/page_registry.c`, current lines 102-112.

```c
    [PAGE_ID_PLAN] = {
        .id          = PAGE_ID_PLAN,
        .title       = "DIVE PLAN TRACK",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_plan_create,
        .update_cb   = NULL,
        .on_enter_cb = NULL,
        .update_vm_cb = page_plan_update_vm_bridge,
    },
```

## PLAN VM page forwarding function

Source: `src/ui/screen/page_registry.c`, current lines 240-251.

```c
void page_registry_update_plan_vm(const ui_vm_plan_chart_t *vm)
{
    /* 仅当计划页存在且支持 VM 刷新时才转发数据。 */
    page_t *page = page_get_by_id(PAGE_ID_PLAN);

    if (page == NULL || page->update_vm_cb == NULL)
    {
        return;
    }

    page->update_vm_cb(vm);
}
```

# 7. Algorithm and PC simulator data entry points

## Algorithm deco plan synchronization to the UI Data Bus

Source: `src/algo_sim/deco_core.cpp`, current lines 981-1015.

```cpp
static void sync_deco_plan_data(const ArexDecoSchedule *schedule, const ArexDecoRuntimeStop *runtime_stop)
{
    deco_stop_t stops[MAX_DECO_STOPS];
    uint8_t count = 0U;
    uint8_t first_display_index = 0U;

    if (schedule == NULL || schedule->stop_count == 0U || runtime_stop == NULL || runtime_stop->available == 0U)
    {
        bus_set_deco_plan(NULL, 0U);
        return;
    }

    first_display_index = plan_display_first_index(schedule, runtime_stop);

    for (uint8_t i = 0U; i < schedule->stop_count && count < MAX_DECO_STOPS; i++)
    {
        uint32_t runtime_s = stop_runtime_seconds(&schedule->stops[i]);
        if (i < first_display_index)
        {
            continue;
        }
        if (!stop_is_plan_display_stop(&schedule->stops[i]))
        {
            continue;
        }
        if (schedule->stops[i].depth_m <= 0.0f || runtime_s == 0U)
        {
            continue;
        }
        stops[count].depth_m = schedule->stops[i].depth_m;
        stops[count].stay_min = (float)runtime_s / 60.0f;
        count++;
    }
    bus_set_deco_plan((count > 0U) ? stops : NULL, count);
}
```

## PC simulated dive track append

Source: `src/hal_sim/sim_data.c`, current lines 808-820.

```c
    if ((s_sim.lifecycle_state == SIM_LIFE_DIVING) || (s_sim.lifecycle_state == SIM_LIFE_SURFACING_PENDING)) dive_tick = true;
    if (!dive_tick) return;

    s_sim.dive_time_s++;
    bus_set_dive_time(s_sim.dive_time_s);
    if (depth_m > s_sim.max_depth_m)
    {
        s_sim.max_depth_m = depth_m;
    }
    s_sim.depth_sum_m += depth_m;
    s_sim.depth_sample_count++;
    bus_set_dive_profile_stats(s_sim.max_depth_m, s_sim.depth_sum_m / (float)s_sim.depth_sample_count);
    dive_log_append_sampled((float)s_sim.dive_time_s, depth_m);
```

## PC simulator test deco stop write

Source: `src/hal_sim/sim_data.c`, current lines 927-934.

```c
    if (current_depth_m > 12.0f) {
        deco_stop_t sim_stops[] = {
            { .depth_m = 9.0f, .stay_min = 2.0f },
            { .depth_m = 6.0f, .stay_min = 3.0f },
            { .depth_m = 3.0f, .stay_min = 1.0f },
        };
        bus_set_deco_plan(sim_stops, 3);
    }
```

## PC debug-link depth data appended to the track

Source: `src/hal_sim/debug_link_pc.h`, current lines 591-616.

```c
    sample_time_s = g_sensor_data.dive_time_s;
    s_debug_link.sample_time_s = sample_time_s;
    now_ms = lv_tick_get();

    if (s_debug_link.depth_rate_valid)
    {
        uint32_t delta_ms = now_ms - s_debug_link.depth_rate_last_tick_ms;
        if (delta_ms > 0U)
        {
            float delta_min = (float)delta_ms / 60000.0f;
            bus_set_ascent_rate((s_debug_link.depth_rate_last_m - depth) / delta_min);
        }
    }
    else
    {
        bus_set_ascent_rate(0.0f);
    }
    s_debug_link.depth_rate_valid = true;
    s_debug_link.depth_rate_last_m = depth;
    s_debug_link.depth_rate_last_tick_ms = now_ms;

    dive_log_append_sampled((float)sample_time_s, depth);
    bus_set_depth(depth);
    debug_update_gas_derived();
    sim_alert_tick();
}
```
