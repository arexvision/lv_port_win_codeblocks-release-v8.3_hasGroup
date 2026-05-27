#ifndef FONTS_H
#define FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* Wall blocks need the Courier square glyph (U+25A0). */
extern const lv_font_t lv_font_courier_28;

#define USE_FONT_ORDINAR

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

#elif defined(USE_FONT_COURIER)

extern const lv_font_t lv_font_courier_14;
extern const lv_font_t lv_font_courier_20;
extern const lv_font_t lv_font_courier_28;
extern const lv_font_t lv_font_courier_32;
extern const lv_font_t lv_font_courier_48;
extern const lv_font_t lv_font_courier_56;
extern const lv_font_t lv_font_courier_58;
extern const lv_font_t lv_font_courier_64;

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

#endif

#ifdef USE_FONT_CONSOLA
#define FONT_SMALL    (&lv_font_consola_20)
#define FONT_TITLE    (&lv_font_consola_20)
#define FONT_MEDIUM   (&lv_font_consola_32)
#define FONT_LARGE    (&lv_font_consola_64)
#define FONT_HUGE     (&lv_font_consola_64)
#define FONT_NDL      (&lv_font_consola_56)
#define FONT_DERIVED  (&lv_font_consola_20)
#define FONT_14       (&lv_font_consola_14)
#define FONT_24       (&lv_font_consola_24)
#elif defined(USE_FONT_COURIER)
#define FONT_SMALL    (&lv_font_courier_20)
#define FONT_TITLE    (&lv_font_courier_20)
#define FONT_MEDIUM   (&lv_font_courier_32)
#define FONT_LARGE    (&lv_font_courier_64)
#define FONT_HUGE     (&lv_font_courier_64)
#define FONT_NDL      (&lv_font_courier_56)
#define FONT_DERIVED  (&lv_font_courier_20)
#define FONT_14       (&lv_font_courier_14)
#define FONT_24       (&lv_font_courier_28)
#elif defined(USE_FONT_ORDINAR)
#define FONT_SMALL    (&lv_font_ordinar_20)
#define FONT_TITLE    (&lv_font_ordinar_20)
#define FONT_MEDIUM   (&lv_font_ordinar_32)
#define FONT_LARGE    (&lv_font_ordinar_64)
#define FONT_HUGE     (&lv_font_ordinar_64)
#define FONT_NDL      (&lv_font_ordinar_58)
#define FONT_DERIVED  (&lv_font_ordinar_20)
#define FONT_14       (&lv_font_ordinar_14)
#define FONT_24       (&lv_font_ordinar_24)
#else
#define FONT_SMALL    (lv_font_montserrat_20)
#define FONT_TITLE    (lv_font_montserrat_20)
#define FONT_MEDIUM   (lv_font_montserrat_32)
#define FONT_LARGE    (lv_font_montserrat_64)
#define FONT_HUGE     (lv_font_montserrat_64)
#define FONT_NDL      (lv_font_montserrat_56)
#define FONT_DERIVED  (lv_font_montserrat_20)
#define FONT_14       (lv_font_montserrat_14)
#define FONT_24       (lv_font_montserrat_24)
#endif

#ifdef __cplusplus
}
#endif

#endif /* FONTS_H */