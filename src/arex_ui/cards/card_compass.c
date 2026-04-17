/*
 * card_compass.c  —  1F: NAV COMPASS
 *
 * 架构规则：
 *   - canvas 宽度由 g_layout.rc_w 决定，高度固定 COMP_H
 *   - draw_tape() 从 g_sensor.heading_deg 读取，同时保留与 g_arex 的桥接
 *   - create_cb 只建控件；update_cb 只重绘 canvas
 */

#include "../arex_ui_engine.h"
#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "../fonts/arex_fonts.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <math.h>

#define COMP_H    140
#define TAPE_H     60
#define TITLE_H    44
#define PAD_X       0

/* canvas 宽度在 create 时由 g_layout.rc_w 决定 */
static int16_t   s_comp_w;
static lv_obj_t *s_canvas;
static lv_obj_t *s_hint_lbl;

/* canvas 像素缓冲：最大 640×140 */
#define CBUF_MAX (640 * COMP_H)
static lv_color_t s_cbuf[CBUF_MAX];
static int16_t    s_last_heading = -1;  /* 脏标志 */

static void draw_tape(int16_t heading)
{
    if (!s_canvas) return;
    int16_t cw = s_comp_w;
    int16_t cx = cw / 2;

    lv_canvas_fill_bg(s_canvas, AREX_BLACK, LV_OPA_COVER);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = AREX_GREEN;
    lbl_dsc.font  = AREX_FONT_SMALL;

    /* 刻度带：-60° ~ +60°，每度 3px */
    for (int deg = -60; deg <= 60; deg++) {
        int d = ((int)heading + deg + 360) % 360;
        lv_coord_t x = cx + deg * 3;
        if (x < 0 || x >= cw) continue;

        bool major = (d % 45 == 0);
        bool minor = (d % 15 == 0);
        lv_coord_t tick_h = major ? 24 : (minor ? 16 : 8);

        line_dsc.color = major ? AREX_GREEN : AREX_DARK;
        line_dsc.color = major ? AREX_GREEN : lv_color_make(0x00, 0x66, 0x00);
        line_dsc.width = major ? 2 : 1;
        lv_canvas_draw_line(s_canvas,
            (lv_point_t[]){{x, 0}, {x, tick_h}}, 2, &line_dsc);

        if (major) {
            static const char *dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
            lv_canvas_draw_text(s_canvas, x - 6, tick_h + 2, 20, &lbl_dsc, dirs[d / 45]);
        }
    }

    /* 中心游标：4px 宽，覆盖整个 TAPE_H */
    line_dsc.color = AREX_GREEN;
    line_dsc.width = 4;
    lv_canvas_draw_line(s_canvas,
        (lv_point_t[]){{cx, 0}, {cx, TAPE_H}}, 2, &line_dsc);

    /* 滑动框上下边框 */
    line_dsc.color = AREX_DARK;
    line_dsc.width = 2;
    lv_canvas_draw_line(s_canvas,
        (lv_point_t[]){{0, 0}, {cw, 0}}, 2, &line_dsc);
    lv_canvas_draw_line(s_canvas,
        (lv_point_t[]){{0, TAPE_H}, {cw, TAPE_H}}, 2, &line_dsc);

    /* 航向大数字（HUGE 48px，居中） */
    char buf[12];
    snprintf(buf, sizeof(buf), "%03d", heading % 360);
    lbl_dsc.font  = AREX_FONT_HUGE;
    lbl_dsc.color = AREX_GREEN;
    lv_canvas_draw_text(s_canvas, cx - 64, TAPE_H + 5, 128, &lbl_dsc, buf);

    /* 目标航向标记（黄线 + TARGET 标签） */
    if (g_arex.compass.marked) {
        int diff = ((int)g_arex.compass.target - heading + 360) % 360;
        if (diff > 180) diff -= 360;
        lv_coord_t tx = cx + diff * 3;
        if (tx >= 0 && tx < cw) {
            line_dsc.color = lv_color_make(0xFF, 0xFF, 0x00);
            line_dsc.width = 2;
            lv_canvas_draw_line(s_canvas,
                (lv_point_t[]){{tx, 0}, {tx, TAPE_H}}, 2, &line_dsc);
        }
        lbl_dsc.font  = AREX_FONT_SMALL;
        lbl_dsc.color = lv_color_make(0xFF, 0xFF, 0x00);
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "TARGET %03d", (int)g_arex.compass.target);
        lv_canvas_draw_text(s_canvas, 10, TAPE_H + 60, 200, &lbl_dsc, tbuf);
    }
}

void card_compass_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "1F: NAV COMPASS");

    s_comp_w = g_layout.rc_w;
    if (s_comp_w > 640) s_comp_w = 640;

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_cbuf, s_comp_w, COMP_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, 0, TITLE_H);

    /* hint 文字（canvas 外，用 label 覆盖在底部）*/
    s_hint_lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(s_hint_lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_hint_lbl,  AREX_FONT_SMALL, 0);
    lv_obj_set_style_bg_opa(s_hint_lbl,     LV_OPA_TRANSP, 0);
    lv_label_set_text(s_hint_lbl, "[ ENTER ] mark heading");
    lv_obj_set_pos(s_hint_lbl, 10, TITLE_H + COMP_H + 5);

    draw_tape((int16_t)g_arex.compass.heading);
}

void card_compass_update(void)
{
    int16_t hdg = (int16_t)g_sensor.heading_deg;
    if (hdg != s_last_heading) {
        s_last_heading = hdg;
        draw_tape(hdg);
    }

    /* 同步 hint 文字 */
    if (s_hint_lbl) {
        lv_label_set_text(s_hint_lbl,
            g_arex.compass.marked
                ? "[ ENTER ] clear target"
                : "[ ENTER ] mark heading");
    }
}
