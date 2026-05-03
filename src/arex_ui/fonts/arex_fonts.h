#ifndef AREX_FONTS_H
#define AREX_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* ========================
 * Font Family Selection
 * ========================
 * Uncomment ONE of the following to switch font family:
 *   #define AREX_USE_FONT_CONSOLA   // Consolas (code/terminal style)
 *   #define AREX_USE_FONT_COURIER   // Courier New Bold (classic monospace)
 *   #define AREX_USE_FONT_ORDINAR   // Linotype Ordinar (dive computer style)
 * Comment out all to use built-in LVGL fonts
 */
#define AREX_USE_FONT_COURIER

/* ================================================================
 * Consolas Font — generated from E:/UI/Consolas/consola-1.ttf
 * Font files must be added to project as separate compilation units
 * ================================================================ */
#ifdef AREX_USE_FONT_CONSOLA

/* Extern declarations for Consolas fonts */
extern const lv_font_t lv_font_consola_14;
extern const lv_font_t lv_font_consola_20;
extern const lv_font_t lv_font_consola_24;
extern const lv_font_t lv_font_consola_28;
extern const lv_font_t lv_font_consola_48;
extern const lv_font_t lv_font_consola_58;

/* ================================================================
 * Courier New Bold Font — generated from C:/Windows/Fonts/courbd.ttf
 * ================================================================ */
#elif defined(AREX_USE_FONT_COURIER)

extern const lv_font_t lv_font_courier_14;
extern const lv_font_t lv_font_courier_20;
extern const lv_font_t lv_font_courier_28;
extern const lv_font_t lv_font_courier_48;
extern const lv_font_t lv_font_courier_58;

/* ================================================================
 * Linotype Ordinar Font — generated from E:/UI/111/Linotype Ordinar W01 Regular.ttf
 * ================================================================ */
#elif defined(AREX_USE_FONT_ORDINAR)

extern const lv_font_t lv_font_ordinar_14;
extern const lv_font_t lv_font_ordinar_20;
extern const lv_font_t lv_font_ordinar_24;
extern const lv_font_t lv_font_ordinar_28;
extern const lv_font_t lv_font_ordinar_32;
extern const lv_font_t lv_font_ordinar_48;
extern const lv_font_t lv_font_ordinar_58;

#endif /* AREX_USE_FONT_ORDINAR */

/* ================================================================
 * Font Role Aliases — switch between Consolas and Courier by
 * commenting/uncommenting AREX_USE_FONT_CONSOLA above
 * ================================================================ */
#ifdef AREX_USE_FONT_CONSOLA
#define AREX_FONT_SMALL    (&lv_font_consola_14)  /* 14px  标签/单位/Badge */
#define AREX_FONT_TITLE    (&lv_font_consola_20)  /* 20px  菜单项/卡片标题(规范21px) */
#define AREX_FONT_MEDIUM   (&lv_font_consola_28)  /* 28px  数据值 */
#define AREX_FONT_48       (&lv_font_consola_48)   /* 48px  NDL减压时间 */
#define AREX_FONT_HUGE     (&lv_font_consola_58)  /* 58px  深度大数字(与HTML规范一致) */
#define AREX_FONT_DERIVED  (&lv_font_consola_20)  /* 21px  派生(≈Title); 规范0.75x≈21px */
#define AREX_FONT_24       (&lv_font_consola_24)  /* 24px  中等标题 */
#define AREX_FONT_32       (&lv_font_consola_32)  /* 32px  中大标题 */
#elif defined(AREX_USE_FONT_COURIER)
#define AREX_FONT_SMALL    (&lv_font_courier_14)  /* 14px  标签/单位/Badge */
#define AREX_FONT_TITLE    (&lv_font_courier_20)  /* 20px  菜单项/卡片标题(规范21px) */
#define AREX_FONT_MEDIUM   (&lv_font_courier_28)  /* 28px  数据值 */
#define AREX_FONT_48       (&lv_font_courier_48)   /* 48px  NDL减压时间 */
#define AREX_FONT_HUGE     (&lv_font_courier_58)  /* 58px  深度大数字(与HTML规范一致) */
#define AREX_FONT_DERIVED  (&lv_font_courier_20)  /* 21px  派生(≈Title); 规范0.75x≈21px */
#define AREX_FONT_24       (&lv_font_courier_24)  /* 24px  中等标题 */
#elif defined(AREX_USE_FONT_ORDINAR)
#define AREX_FONT_SMALL    (&lv_font_ordinar_14)  /* 14px  标签/单位/Badge */
#define AREX_FONT_TITLE    (&lv_font_ordinar_20)  /* 20px  菜单项/卡片标题 */
#define AREX_FONT_MEDIUM   (&lv_font_ordinar_28)  /* 28px  数据值 */
#define AREX_FONT_HUGE     (&lv_font_ordinar_58)  /* 58px  深度大数字 */
#define AREX_FONT_48       (&lv_font_ordinar_48)   /* 48px  NDL减压时间 */
#define AREX_FONT_DERIVED  (&lv_font_ordinar_20)  /* 20px  派生 */
#define AREX_FONT_24       (&lv_font_ordinar_24)  /* 24px  中等标题 */
#define AREX_FONT_32       (&lv_font_ordinar_32)  /* 32px  中大标题 */
#else
#define AREX_FONT_SMALL    (lv_font_montserrat_14)
#define AREX_FONT_TITLE    (lv_font_montserrat_20)
#define AREX_FONT_MEDIUM   (lv_font_montserrat_28)
#define AREX_FONT_48       (lv_font_montserrat_48)
#define AREX_FONT_HUGE     (lv_font_montserrat_58)
#define AREX_FONT_DERIVED  (lv_font_montserrat_20)
#define AREX_FONT_24       (lv_font_montserrat_24)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AREX_FONTS_H */
