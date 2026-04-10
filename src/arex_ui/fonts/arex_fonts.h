#ifndef AREX_FONTS_H
#define AREX_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* Enable the font data sections inside each lv_font_courier_*.c file */
#ifndef LV_FONT_COURIER_14
#define LV_FONT_COURIER_14 1
#endif
#ifndef LV_FONT_COURIER_20
#define LV_FONT_COURIER_20 1
#endif
#ifndef LV_FONT_COURIER_28
#define LV_FONT_COURIER_28 1
#endif
#ifndef LV_FONT_COURIER_48
#define LV_FONT_COURIER_48 1
#endif

/* Courier New Bold — matches HTML --arex-font: 'Courier New', Courier, monospace
   Generated with lv_font_conv from C:/Windows/Fonts/courbd.ttf */
LV_FONT_DECLARE(lv_font_courier_14)
LV_FONT_DECLARE(lv_font_courier_20)
LV_FONT_DECLARE(lv_font_courier_28)
LV_FONT_DECLARE(lv_font_courier_48)

/* Role aliases */
#define AREX_FONT_SMALL   (&lv_font_courier_14)
#define AREX_FONT_TITLE   (&lv_font_courier_20)
#define AREX_FONT_MEDIUM  (&lv_font_courier_28)
#define AREX_FONT_HUGE    (&lv_font_courier_48)

#ifdef __cplusplus
}
#endif

#endif /* AREX_FONTS_H */
