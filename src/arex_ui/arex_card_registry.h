#ifndef AREX_CARD_REGISTRY_H
#define AREX_CARD_REGISTRY_H

#include "lvgl/lvgl.h"
#include <stdint.h>

/* =========================================
   Card IDs — stable identifiers (卡片固有身份，不随顺序变化)
   ========================================= */
typedef enum {
    CARD_ID_INFO    = 0,
    CARD_ID_COMPASS = 1,
    CARD_ID_DECO    = 2,
    CARD_ID_GAS     = 3,
    CARD_ID_PLAN    = 4,
    CARD_ID_SETUP   = 5,
    CARD_ID_COUNT
} arex_card_id_t;

/* =========================================
   Card Positions — tileview 中的显示位置
   card_order[pos] = card_id

   INFO 固定在 tile 0，SETUP 固定在 tile 5。
   只有 CARD_POS_1 ~ CARD_POS_4 这 4 个位置可重排。

   默认顺序：
     card_order[CARD_POS_INFO]  = CARD_ID_INFO;      // 固定 tile 0
     card_order[CARD_POS_1]     = CARD_ID_COMPASS;
     card_order[CARD_POS_2]     = CARD_ID_DECO;
     card_order[CARD_POS_3]     = CARD_ID_GAS;
     card_order[CARD_POS_4]     = CARD_ID_PLAN;
     card_order[CARD_POS_SETUP] = CARD_ID_SETUP;     // 固定 tile 5
   ========================================= */
typedef enum {
    CARD_POS_INFO  = 0,   /* INFO 固定 tile 0 */
    CARD_POS_1     = 1,   /* 可重排 */
    CARD_POS_2     = 2,   /* 可重排 */
    CARD_POS_3     = 3,   /* 可重排 */
    CARD_POS_4     = 4,   /* 可重排 */
    CARD_POS_SETUP = 5,   /* SETUP 固定 tile 5 */
    CARD_POS_COUNT
} arex_card_pos_t;

/* 物理卡片总数（INFO + COMPASS + DECO + GAS + PLAN + SETUP = 6） */
#define AREX_CARD_COUNT CARD_ID_COUNT

/* DASH 状态下可滑动的卡片数（排除首尾 INFO/SETUP） */
#define AREX_DASH_CARD_COUNT (AREX_CARD_COUNT - 2)

/* =========================================
   Card descriptor
   ========================================= */
typedef struct {
    arex_card_id_t  id;
    const char     *title;        /* shown in card header, English */
    lv_obj_t       *tile_obj;     /* filled after create_cb() */

    /* create_cb: build all widgets inside parent */
    void (*create_cb)(lv_obj_t *parent);

    /* update_cb: refresh widget values from g_sensor_data — called every sim tick */
    void (*update_cb)(void);

    /* on_enter_cb: called when user scrolls to this card — optional */
    void (*on_enter_cb)(void);
} arex_card_reg_t;

/* =========================================
   Registry API
   ========================================= */

/* Returns pointer to the full registry array */
arex_card_reg_t *arex_card_registry(void);

/* Number of registered cards (always AREX_CARD_COUNT) */
uint8_t arex_card_count(void);

/* Get card descriptor by position in card_order[] */
arex_card_reg_t *arex_card_get(uint8_t order_pos);

/* Get card descriptor by stable card ID */
arex_card_reg_t *arex_card_get_by_id(arex_card_id_t id);

#endif /* AREX_CARD_REGISTRY_H */
