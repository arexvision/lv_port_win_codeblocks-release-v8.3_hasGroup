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
        txt_dsc.font = PLAN_TRACK_DECO_STOP_LABEL_FONT;

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
