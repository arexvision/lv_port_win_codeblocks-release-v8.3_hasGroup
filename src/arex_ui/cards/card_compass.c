#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_engine.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <math.h>

/* ============================================================
   1F: NAV COMPASS — 三种风格（Classic / Aero / Sub）
   规范参数（用户定义）：
     容器总高：140px
     滑动框(Box)：宽 100%，高 60px，边框 2px AREX_DARK
     航向数字：46.4px（规范）；字库最接近 lv_font_courier_48 (48px)
     中心游标：宽 4px，高 60px，颜色 AREX_GREEN
   ============================================================ */

/* 运行时动态宽度，由 card_compass_create() 初始化 */
static int s_comp_w;
#define COMP_H  140           /* 规范：容器总高 140px */
#define TAPE_H  60            /* 规范：滑动框高 60px */
#define HEADING_Y (TAPE_H + 5) /* 航向数字起点 y = 65 */
#define TARGET_Y   (HEADING_Y + 55) /* TARGET 标签 y = 120 */
#define HINT_Y     (COMP_H - 25)   /* 底部 hint y = 115 */

/* ---- helpers ---- */
static lv_obj_t *s_canvas;   /* 画布对象，必须在 draw_tape 之前声明 */
static void draw_tape(lv_coord_t heading)
{
    /* 清画布 */
    lv_canvas_fill_bg(s_canvas, lv_color_make(0,0,0), LV_OPA_COVER);

    int cx = s_comp_w / 2;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_make(0x00, 0x33, 0x00);
    line_dsc.width = 1;

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = lv_color_make(0x00, 0xFF, 0x00);
    lbl_dsc.font  = AREX_FONT_SMALL;

    /* 滑动刻度带（-60° ~ +60°，每度 3px → 总宽 360px 覆盖动态宽度画布） */
    for (int deg = -60; deg <= 60; deg++) {
        int d = ((int)heading + deg + 360) % 360;
        lv_coord_t x = cx + deg * 3;
        if (x < 0 || x >= s_comp_w) continue;

        bool major = (d % 45 == 0);
        bool minor = (d % 15 == 0);
        lv_coord_t tick_h = major ? 24 : (minor ? 16 : 8);

        line_dsc.color = major ? lv_color_make(0x00,0xFF,0x00) : lv_color_make(0x00,0x66,0x00);
        line_dsc.width = major ? 2 : 1;
        lv_canvas_draw_line(s_canvas, (lv_point_t[]){{x,0},{x,tick_h}}, 2, &line_dsc);

        if (major) {
            static const char *dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
            const char *label = dirs[d / 45];
            lv_canvas_draw_text(s_canvas, x - 6, tick_h + 2, 20, &lbl_dsc, label);
        }
    }

    /* 规范：中心游标，宽 4px，高 60px，颜色 AREX_GREEN */
    line_dsc.color = lv_color_make(0x00,0xFF,0x00);
    line_dsc.width = 4;
    lv_canvas_draw_line(s_canvas, (lv_point_t[]){{cx, 0},{cx, TAPE_H}}, 2, &line_dsc);

    /* 滑动框边框（规范：2px AREX_DARK） */
    line_dsc.color = lv_color_make(0x00,0x33,0x00);
    line_dsc.width = 2;
    lv_canvas_draw_line(s_canvas, (lv_point_t[]){{0, 0},{s_comp_w, 0}}, 2, &line_dsc);
    lv_canvas_draw_line(s_canvas, (lv_point_t[]){{0, TAPE_H},{s_comp_w, TAPE_H}}, 2, &line_dsc);

    /* 航向数字 — 规范 46.4px，字库最接近 AREX_FONT_HUGE(48px)，居中显示 */
    char buf[12];
    snprintf(buf, sizeof(buf), "%03d\xC2\xB0", (int)heading);
    lbl_dsc.font  = AREX_FONT_HUGE; /* 48px（规范 46.4px） */
    lbl_dsc.color = lv_color_make(0x00,0xFF,0x00);
    lv_canvas_draw_text(s_canvas, cx - 64, HEADING_Y, 128, &lbl_dsc, buf);

    /* 目标航向标记 */
    if (g_sensor_data.heading_locked) {
        lv_coord_t diff = (int)(g_sensor_data.heading_target - heading + 360) % 360;
        if (diff > 180) diff -= 360;
        lv_coord_t tx = cx + diff * 3;
        if (tx >= 0 && tx < s_comp_w) {
            line_dsc.color = lv_color_make(0xFF,0xFF,0x00);
            line_dsc.width = 2;
            lv_canvas_draw_line(s_canvas, (lv_point_t[]){{tx,0},{tx,TAPE_H}}, 2, &line_dsc);
        }

        lbl_dsc.font  = AREX_FONT_SMALL;
        lbl_dsc.color = lv_color_make(0xFF,0xFF,0x00);
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "TARGET %03d\xC2\xB0", (int)g_sensor_data.heading_target);
        lv_canvas_draw_text(s_canvas, 10, TARGET_Y, 200, &lbl_dsc, tbuf);
    }

    /* 底部 hint */
    lbl_dsc.font  = AREX_FONT_SMALL;
    lbl_dsc.color = lv_color_make(0x55,0xFF,0x55);
    lv_canvas_draw_text(s_canvas, 10, HINT_Y, 400, &lbl_dsc,
        g_sensor_data.heading_locked
            ? "[ ENTER ] clear target"
            : "[ ENTER ] mark heading");
}

static uint8_t   s_last_style = 0xFF;
/* canvas buffer 声明为最大可能尺寸 (640px 宽 * 140px 高)，运行时按 s_comp_w 裁剪 */
static lv_color_t s_cbuf[640 * COMP_H];

void card_compass_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "1F: NAV COMPASS");

    /* 动态计算 COMP_W = safe_zone_w - LEFT_ANCHOR - gap */
    s_comp_w = (int)g_sys_config.safe_zone_w - (int)AREX_LEFT_ANCHOR_W
               - (int)(g_sys_config.gap_u * AREX_BASE_U);

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_cbuf, s_comp_w, COMP_H, LV_IMG_CF_TRUE_COLOR);
    /* 规范：canvas y=50（标题下方），高 140px */
    lv_obj_set_pos(s_canvas, 0, 50);

    draw_tape((lv_coord_t)g_sensor_data.heading);
    s_last_style = g_sys_config.compass_style;
}

void card_compass_update(void)
{
    draw_tape((lv_coord_t)g_sensor_data.heading);
    (void)s_last_style; /* suppress unused warning */
}
