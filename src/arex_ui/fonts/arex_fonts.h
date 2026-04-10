#ifndef AREX_FONTS_H
#define AREX_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* Courier New Bold — matches HTML --arex-font: 'Courier New', Courier, monospace
   Generated with lv_font_conv from C:/Windows/Fonts/courbd.ttf
   Sizes mirror HTML: --font-small=14, --font-medium*0.75=20, --font-medium=28, --font-huge=48 */

LV_FONT_DECLARE(lv_font_courier_14)
LV_FONT_DECLARE(lv_font_courier_20)
LV_FONT_DECLARE(lv_font_courier_28)
LV_FONT_DECLARE(lv_font_courier_48)

/* Convenience aliases matching the role names used in arex_screen.c */
#define AREX_FONT_SMALL   (&lv_font_courier_14)
#define AREX_FONT_MEDIUM  (&lv_font_courier_28)
#define AREX_FONT_TITLE   (&lv_font_courier_20)
#define AREX_FONT_HUGE    (&lv_font_courier_48)

#ifdef __cplusplus
}
#endif

#endif /* AREX_FONTS_H */
