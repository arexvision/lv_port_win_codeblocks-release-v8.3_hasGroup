#include "../arex_screen.h"
#include "../arex_data.h"
#include "../arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <math.h>

#define COMP_W  420
#define COMP_H  380
#define CX      (COMP_W / 2)
#define CY      (COMP_H / 2)
#define TAPE_H  80
#define TAPE_W  COMP_W

static lv_obj_t *s_canvas;
static uint8_t   s_last_style = 0xFF;

static lv_color_t s_cbuf[COMP_W * COMP_H];

/* ---- helpers ---- */
static void draw_tape(lv_coord_t heading)
{
    /* Clear canvas */
    lv_canvas_fill_bg(s_canvas, lv_color_make(0,0,0), LV_OPA_COVER);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_make(0x00, 0x33, 0x00);
    line_dsc.width = 1;

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = lv_color_make(0x00, 0xFF, 0x00);
    lbl_dsc.font  = AREX_FONT_SMALL;

    /* Tape: draw tick marks centered on heading */
    for (int deg = -60; deg <= 60; deg++) {
        int d = ((int)heading + deg + 360) % 360;
        lv_coord_t x = CX + deg * 3;
        if (x < 0 || x >= TAPE_W) continue;

        bool major = (d % 45 == 0);
        bool minor = (d % 15 == 0);
        lv_coord_t tick_h = major ? 24 : (minor ? 16 : 8);

        line_dsc.color = major ? lv_color_make(0x00,0xFF,0x00) : lv_color_make(0x00,0x66,0x00);
        line_dsc.width = major ? 2 : 1;
        lv_canvas_draw_line(s_canvas, (lv_point_t[]){ {x,0},{x,tick_h} }, 2, &line_dsc);

        if (major) {
            static const char *dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
            char buf[4];
            const char *label = dirs[d / 45];
            lv_canvas_draw_text(s_canvas, x - 6, tick_h + 2, 20, &lbl_dsc, label);
            (void)buf;
        }
    }

    /* Center pointer */
    line_dsc.color = lv_color_make(0x00,0xFF,0x00);
    line_dsc.width = 3;
    lv_canvas_draw_line(s_canvas, (lv_point_t[]){ {CX, 0},{CX, 36} }, 2, &line_dsc);

    /* Heading value */
    char buf[8];
    snprintf(buf, sizeof(buf), "%03d°", (int)heading);
    lbl_dsc.font  = AREX_FONT_HUGE;
    lbl_dsc.color = lv_color_make(0x00,0xFF,0x00);
    lv_canvas_draw_text(s_canvas, CX - 64, TAPE_H + 10, 128, &lbl_dsc, buf);

    /* Target marker */
    if (g_arex.compass.marked) {
        lv_coord_t diff = (int)(g_arex.compass.target - heading + 360) % 360;
        if (diff > 180) diff -= 360;
        lv_coord_t tx = CX + diff * 3;
        if (tx >= 0 && tx < TAPE_W) {
            line_dsc.color = lv_color_make(0xFF,0xFF,0x00);
            line_dsc.width = 2;
            lv_canvas_draw_line(s_canvas, (lv_point_t[]){ {tx,0},{tx,TAPE_H} }, 2, &line_dsc);
        }

        lbl_dsc.font  = AREX_FONT_SMALL;
        lbl_dsc.color = lv_color_make(0xFF,0xFF,0x00);
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "TARGET %03d°", (int)g_arex.compass.target);
        lv_canvas_draw_text(s_canvas, 10, TAPE_H + 72, 200, &lbl_dsc, tbuf);
    }

    /* Hint */
    lbl_dsc.font  = AREX_FONT_SMALL;
    lbl_dsc.color = lv_color_make(0x55,0xFF,0x55);
    lv_canvas_draw_text(s_canvas, 10, COMP_H - 40, 400, &lbl_dsc,
        g_arex.compass.marked
            ? "[ ENTER ] clear target"
            : "[ ENTER ] mark heading");
}

void card_compass_create(lv_obj_t *parent)
{
    arex_screen_make_card_title(parent, "1F: NAV COMPASS");

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_cbuf, COMP_W, COMP_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(s_canvas, LV_ALIGN_BOTTOM_MID, 0, -20);

    draw_tape((lv_coord_t)g_arex.compass.heading);
    s_last_style = g_arex.compass.style;
}

void card_compass_update(void)
{
    draw_tape((lv_coord_t)g_arex.compass.heading);
}
