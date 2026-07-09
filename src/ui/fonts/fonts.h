/*
 * 文件: src/app_ui/ui/fonts/fonts.h
 * 作用: 该文件为字体资源或字体声明文件，主要承载 LVGL 字库数据与对外字体入口，通常由工具生成或由资源配置统一维护。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应优先确认其是否为生成文件；若资源由工具链产出，应通过资源源文件或生成脚本更新，避免手改数据区导致后续重新生成时被覆盖。
 */

#ifndef FONTS_H
#define FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* Wall blocks 需要固定使用 Courier 方块字形（U+25A0），
 * 因此无论当前主字体家族是什么，都要保证该字体声明可见。 */
extern const lv_font_t lv_font_courier_28;

/* ========================
 * Font Family Selection
 * ========================
 * Uncomment ONE of the following to switch font family:
 *   #define USE_FONT_CONSOLA   // Consolas (code/terminal style)
 *   #define USE_FONT_COURIER   // Courier New Bold (classic monospace)
 *   #define USE_FONT_ORDINAR   // Linotype Ordinar (dive computer style)
 * Comment out all to use built-in LVGL fonts
 */
#define USE_FONT_ORDINAR

/* ================================================================
 * Consolas Font — generated from E:/UI/Consolas/consola-1.ttf
 * Font files must be added to project as separate compilation units
 * ================================================================ */
#ifdef USE_FONT_CONSOLA

extern const lv_font_t lv_font_consola_14;
extern const lv_font_t lv_font_consola_20;
extern const lv_font_t lv_font_consola_24;
extern const lv_font_t lv_font_consola_28;
extern const lv_font_t lv_font_consola_32;
extern const lv_font_t lv_font_consola_48;
extern const lv_font_t lv_font_consola_56;
extern const lv_font_t lv_font_consola_58;
extern const lv_font_t lv_font_consola_64;

/* ================================================================
 * Courier New Bold Font — generated from C:/Windows/Fonts/courbd.ttf
 * ================================================================ */
#elif defined(USE_FONT_COURIER)

extern const lv_font_t lv_font_courier_14;
extern const lv_font_t lv_font_courier_20;
extern const lv_font_t lv_font_courier_28;
extern const lv_font_t lv_font_courier_32;
extern const lv_font_t lv_font_courier_48;
extern const lv_font_t lv_font_courier_56;
extern const lv_font_t lv_font_courier_58;
extern const lv_font_t lv_font_courier_64;

/* ================================================================
 * Linotype Ordinar Font — generated from E:/UI/111/Linotype Ordinar W01 Regular.ttf
 * ================================================================ */
#elif defined(USE_FONT_ORDINAR)

extern const lv_font_t lv_font_ordinar_14;
extern const lv_font_t lv_font_ordinar_20;
extern const lv_font_t lv_font_ordinar_24;
extern const lv_font_t lv_font_ordinar_28;
extern const lv_font_t lv_font_ordinar_32;
extern const lv_font_t lv_font_ordinar_48;
extern const lv_font_t lv_font_ordinar_56;
extern const lv_font_t lv_font_ordinar_58;
extern const lv_font_t lv_font_ordinar_64;

#endif /* USE_FONT_ORDINAR */

/* ================================================================
 * Font Role Aliases — 小/中/大 + NDL专用
 * ================================================================ */
#ifdef USE_FONT_CONSOLA
#define FONT_SMALL    (&lv_font_consola_20)  /* 20px  小字体 - 标签/单位/Badge/标题 */
#define FONT_14       (&lv_font_consola_14)  /* 14px  极小辅助文字 */
#define FONT_TITLE    (&lv_font_consola_20)  /* 20px  菜单项/卡片标题(规范21px) */
#define FONT_MEDIUM   (&lv_font_consola_32)  /* 32px  中字体 - 数据值 */
#define FONT_LARGE    (&lv_font_consola_64)  /* 64px  大字体 - 深度大数字 */
#define FONT_HUGE     (&lv_font_consola_64)  /* 64px  大字体 */
#define FONT_NDL      (&lv_font_consola_56)  /* 56px  NDL减压时间专用 */
#define FONT_DERIVED  (&lv_font_consola_20)  /* 20px  派生(≈Title); 规范0.75x≈21px */
#define FONT_24       (&lv_font_consola_24)  /* 24px  中等标题 */
#elif defined(USE_FONT_COURIER)
#define FONT_SMALL    (&lv_font_courier_20)  /* 20px  小字体 - 标签/单位/Badge/标题 */
#define FONT_14       (&lv_font_courier_14)  /* 14px  极小辅助文字 */
#define FONT_TITLE    (&lv_font_courier_20)  /* 20px  菜单项/卡片标题(规范21px) */
#define FONT_MEDIUM   (&lv_font_courier_32)  /* 32px  中字体 - 数据值 */
#define FONT_LARGE    (&lv_font_courier_64)  /* 64px  大字体 - 深度大数字 */
#define FONT_HUGE     (&lv_font_courier_64)  /* 64px  大字体 */
#define FONT_NDL      (&lv_font_courier_56)  /* 56px  NDL减压时间专用 */
#define FONT_DERIVED  (&lv_font_courier_20)  /* 20px  派生(≈Title); 规范0.75x≈21px */
#define FONT_24       (&lv_font_courier_24)  /* 24px  中等标题 */
#elif defined(USE_FONT_ORDINAR)
#define FONT_SMALL    (&lv_font_ordinar_20)  /* 20px  小字体 - 标签/单位/Badge/标题 */
#define FONT_14       (&lv_font_ordinar_14)  /* 14px  极小辅助文字 */
#define FONT_TITLE    (&lv_font_ordinar_20)  /* 20px  菜单项/卡片标题 */
#define FONT_MEDIUM   (&lv_font_ordinar_32)  /* 32px  中字体 - 数据值 */
#define FONT_LARGE    (&lv_font_ordinar_64)  /* 64px  大字体 - 深度大数字 */
#define FONT_HUGE     (&lv_font_ordinar_64)  /* 64px  大字体 */
#define FONT_NDL      (&lv_font_ordinar_58)  /* 56px  NDL减压时间专用 */
#define FONT_DERIVED  (&lv_font_ordinar_20)  /* 20px  派生 */
#define FONT_24       (&lv_font_ordinar_24)  /* 24px  中等标题 */
#define FONT_TRACK    (&lv_font_ordinar_14)  /* PLAN图减压站标签专用14px适配字体 */
#else
#define FONT_SMALL    (lv_font_montserrat_20)
#define FONT_14       (lv_font_montserrat_14)
#define FONT_TITLE    (lv_font_montserrat_20)
#define FONT_MEDIUM   (lv_font_montserrat_32)
#define FONT_LARGE    (lv_font_montserrat_64)
#define FONT_HUGE     (lv_font_montserrat_64)
#define FONT_NDL      (lv_font_montserrat_56)
#define FONT_DERIVED  (lv_font_montserrat_20)
#define FONT_24       (lv_font_montserrat_24)
#endif

#ifdef __cplusplus
}
#endif

#endif /* FONTS_H */
