#include "../arex_screen.h"
#include "../arex_data.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>

#define CHART_W  380
#define CHART_H  280
#define CHART_X  40
#define CHART_Y  80

static lv_color_t s_cbuf[CHART_W * CHART_H];
static lv_obj_t  *s_canvas;

/* Demo dive profile points (time_min, depth_m) */
static const int16_t s_profile[][2] = {
    {0,  0}, {2, 15}, {5, 30}, {10, 45}, {30, 45},
    {33, 30}, {36, 21}, {40, 21}, {43, 9}, {48, 9},
    {50, 6},  {53, 6},  {55, 0},
};
#define PROFILE_LEN 13

static void draw_plan(void)
{
    lv_canvas_fill_bg(s_canvas, lv_color_make(0,0,0), LV_OPA_COVER);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    lv_draw_label_dsc_t t;
    lv_draw_label_dsc_init(&t);
    t.font  = &lv_font_montserrat_14;
    t.color = lv_color_make(0x55,0xFF,0x55);

    /* Grid */
    l.color = lv_color_make(0x00,0x22,0x00);
    l.width = 1;
    for (int d = 0; d <= 50; d += 10) {
        lv_coord_t y = (lv_coord_t)(d * CHART_H / 55);
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{0,y},{CHART_W,y}}, 2, &l);
        char buf[8]; snprintf(buf,sizeof(buf),"%dm",d);
        lv_canvas_draw_text(s_canvas, 0, y-8, 30, &t, buf);
    }
    for (int m = 0; m <= 55; m += 10) {
        lv_coord_t x = (lv_coord_t)(m * CHART_W / 55);
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{x,0},{x,CHART_H}}, 2, &l);
        char buf[8]; snprintf(buf,sizeof(buf),"%d'",m);
        lv_canvas_draw_text(s_canvas, x+2, CHART_H-18, 30, &t, buf);
    }

    /* Profile line */
    l.color = lv_color_make(0x00,0xFF,0x00);
    l.width = 2;
    for (int i = 1; i < PROFILE_LEN; i++) {
        lv_coord_t x1 = (lv_coord_t)(s_profile[i-1][0] * CHART_W / 55);
        lv_coord_t y1 = (lv_coord_t)(s_profile[i-1][1] * CHART_H / 55);
        lv_coord_t x2 = (lv_coord_t)(s_profile[i  ][0] * CHART_W / 55);
        lv_coord_t y2 = (lv_coord_t)(s_profile[i  ][1] * CHART_H / 55);
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{x1,y1},{x2,y2}}, 2, &l);
    }

    /* Current position dot */
    float dive_min = g_arex.dive.dive_time_s / 60.0f;
    lv_coord_t cx = (lv_coord_t)(dive_min * CHART_W / 55.0f);
    lv_coord_t cy = (lv_coord_t)(g_arex.dive.depth * CHART_H / 55.0f);
    lv_draw_rect_dsc_t r; lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_color_make(0xFF,0xFF,0x00);
    r.radius   = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_canvas, cx-4, cy-4, 8, 8, &r);
}

void card_plan_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_color(title, lv_color_make(0x00,0xFF,0x00), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "4F  DIVE PLAN TRACK");
    lv_obj_set_pos(title, 16, 12);

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_cbuf, CHART_W, CHART_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, 30, 60);

    draw_plan();
}

void card_plan_update(void)
{
    draw_plan();
}
