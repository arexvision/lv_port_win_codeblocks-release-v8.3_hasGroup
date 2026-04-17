/*
 * card_plan.c  —  4F: DIVE PLAN TRACK
 *
 * 架构规则：
 *   - canvas 尺寸 = (rc_w - PAD_X*2 - SHELL_PAD*2) × (rc_h - TITLE_H - PAD_Y*2 - SHELL_PAD*2)
 *   - 从 g_sensor 读取当前深度和潜水时间
 *   - X 轴最大刻度 = 实际潜水时间 * 1.05（动态贴满）最低保底 PLOT_T_MIN
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <math.h>

#define TITLE_H     44
#define PAD_X       16
#define PAD_Y        8
#define SHELL_PAD   10
#define PLOT_D_MAX  50    /* 深度轴最大 50m */
#define PLOT_T_MIN  20    /* 时间轴最小保底 20min */

static lv_obj_t   *s_canvas;
static int16_t     s_cw, s_ch; /* canvas 实际尺寸 */
static uint16_t    s_last_time_sec = 0xFFFF;  /* 脏标志：避免无变化时重绘 */

/* Demo 潜水剖面 (time_min, depth_m) */
static const int16_t s_profile[][2] = {
    {0, 0}, {2, 15}, {5, 30}, {10, 45}, {30, 45},
    {33, 30}, {36, 21}, {40, 21}, {43, 9}, {48, 9},
    {50, 6},  {53, 6},  {55, 0},
};
#define PROFILE_LEN 13

static lv_color_t s_static_cbuf[640 * 380]; /* 静态缓冲，足够大 */

static void draw_plan(void)
{
    if (!s_canvas) return;

    int16_t cw = s_cw, ch = s_ch;

    /* 动态 X 轴：当前时间 * 1.05，保底 PLOT_T_MIN */
    float dive_min = g_sensor.dive_time_sec / 60.0f;
    int16_t t_max  = (int16_t)(dive_min * 1.05f);
    if (t_max < PLOT_T_MIN) t_max = PLOT_T_MIN;
    /* 向上取整到 5 的倍数，使网格对齐 */
    t_max = ((t_max + 4) / 5) * 5;

#define PX(t)  ((lv_coord_t)((t) * cw / t_max))
#define PY(d)  ((lv_coord_t)(ch - (d) * ch / PLOT_D_MAX))

    lv_canvas_fill_bg(s_canvas, AREX_BLACK, LV_OPA_COVER);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = AREX_GREEN;
    l.opa   = 51;  /* 20% */
    l.width = 1;

    lv_draw_label_dsc_t t;
    lv_draw_label_dsc_init(&t);
    t.font  = AREX_FONT_SMALL;
    t.color = AREX_GREEN;
    t.opa   = 191; /* 75% */

    /* 深度网格（横线）*/
    for (int d = 0; d <= PLOT_D_MAX; d += 10) {
        lv_coord_t y = PY(d);
        lv_canvas_draw_line(s_canvas, (lv_point_t[]){{0,y},{cw,y}}, 2, &l);
        char buf[8]; snprintf(buf, sizeof(buf), "%dm", d);
        lv_canvas_draw_text(s_canvas, 2, y + 2, 36, &t, buf);
    }
    /* 时间网格（纵线）*/
    for (int m = 0; m <= t_max; m += 5) {
        lv_coord_t x = PX(m);
        if (x > cw) x = cw;
        lv_canvas_draw_line(s_canvas, (lv_point_t[]){{x,0},{x,ch}}, 2, &l);
        char buf[8]; snprintf(buf, sizeof(buf), "%d'", m);
        lv_canvas_draw_text(s_canvas, x + 2, ch - 18, 30, &t, buf);
    }

    /* 走势线（实线 4px）*/
    l.color = AREX_GREEN;
    l.width = 4;
    l.opa   = 255;
    for (int i = 1; i < PROFILE_LEN; i++) {
        lv_coord_t x1 = PX(s_profile[i-1][0]);
        lv_coord_t y1 = PY(s_profile[i-1][1]);
        lv_coord_t x2 = PX(s_profile[i  ][0]);
        lv_coord_t y2 = PY(s_profile[i  ][1]);
        lv_canvas_draw_line(s_canvas, (lv_point_t[]){{x1,y1},{x2,y2}}, 2, &l);
    }

    /* 停留点（黑填充，绿边框圆形）*/
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color     = AREX_BLACK;
    r.border_color = AREX_GREEN;
    r.border_width = 2;
    r.radius       = LV_RADIUS_CIRCLE;
    for (int i = 1; i < PROFILE_LEN; i++) {
        int t0 = s_profile[i-1][0], t1 = s_profile[i][0];
        if (s_profile[i][1] != 0 && (t1 - t0) >= 3) {
            int tm = (t0 + t1) / 2;
            lv_coord_t cx = PX(tm);
            lv_coord_t cy = PY(s_profile[i][1]);
            lv_canvas_draw_rect(s_canvas, cx - 6, cy - 6, 12, 12, &r);
        }
    }

    /* 当前位置（黄点 + NOW 标签）*/
    lv_coord_t cx = PX((int)dive_min);
    lv_coord_t cy = PY((int)g_sensor.depth_m);
    lv_draw_rect_dsc_t nd;
    lv_draw_rect_dsc_init(&nd);
    nd.bg_color = lv_color_make(0xFF, 0xFF, 0x00);
    nd.radius   = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_canvas, cx - 5, cy - 5, 10, 10, &nd);

    lv_draw_label_dsc_t nl;
    lv_draw_label_dsc_init(&nl);
    nl.font  = AREX_FONT_SMALL;
    nl.color = AREX_GREEN;
    nl.opa   = 255;
    lv_canvas_draw_text(s_canvas, cx + 8, cy - 8, 40, &nl, "NOW");

#undef PX
#undef PY
}

void card_plan_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "4F: DIVE PLAN TRACK");

    int16_t shell_x = PAD_X - SHELL_PAD;
    int16_t shell_y = TITLE_H;
    int16_t shell_w = g_layout.rc_w - PAD_X * 2 + SHELL_PAD * 2;
    int16_t shell_h = g_layout.rc_h - TITLE_H - PAD_Y;

    /* 外壳 */
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_set_pos(shell,  shell_x, shell_y);
    lv_obj_set_size(shell, shell_w, shell_h);
    lv_obj_set_style_bg_color(shell,   AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(shell,     LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, AREX_DARK, 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_radius(shell,     0, 0);
    lv_obj_set_style_pad_all(shell,    0, 0);
    lv_obj_set_scrollbar_mode(shell,   LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(shell, LV_OBJ_FLAG_SCROLLABLE);

    /* canvas 尺寸 */
    s_cw = shell_w - SHELL_PAD * 2;
    s_ch = shell_h - SHELL_PAD * 2;
    if (s_cw < 10) s_cw = 10;
    if (s_ch < 10) s_ch = 10;

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_static_cbuf, s_cw, s_ch, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, PAD_X, TITLE_H + SHELL_PAD);

    draw_plan();
}

void card_plan_update(void)
{
    /* 只在时间变化时重绘（每秒至多一次，避免无谓的全图软渲染） */
    if (g_sensor.dive_time_sec == s_last_time_sec) return;
    s_last_time_sec = g_sensor.dive_time_sec;
    draw_plan();
}
