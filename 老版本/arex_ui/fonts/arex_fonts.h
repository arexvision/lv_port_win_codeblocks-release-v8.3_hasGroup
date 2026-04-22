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
   Generated with lv_font_conv from C:/Windows/Fonts/courbd.ttf
   可用尺寸: 14px(Small) / 20px(Title) / 28px(Medium) / 48px(Huge)
   规范中的派生字体 21px(0.75x) 最接近 20px，用 lv_font_courier_20 代用 */
LV_FONT_DECLARE(lv_font_courier_14)
LV_FONT_DECLARE(lv_font_courier_20)
LV_FONT_DECLARE(lv_font_courier_28)
LV_FONT_DECLARE(lv_font_courier_48)

/* Role aliases */
#define AREX_FONT_SMALL    (&lv_font_courier_14)  /* 14px  标签/单位/Badge */
#define AREX_FONT_TITLE    (&lv_font_courier_20)  /* 20px  菜单项/卡片标题(规范21px) */
#define AREX_FONT_MEDIUM   (&lv_font_courier_28)  /* 28px  数据值 */
#define AREX_FONT_HUGE     (&lv_font_courier_48)  /* 48px  深度大数字 */
#define AREX_FONT_DERIVED  (&lv_font_courier_20)  /* 21px  派生(≈Title); 规范0.75x≈21px */

#ifdef __cplusplus
}
#endif

#endif /* AREX_FONTS_H */
