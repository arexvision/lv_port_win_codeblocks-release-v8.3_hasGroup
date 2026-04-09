#ifndef AREX_CARD_REGISTRY_H
#define AREX_CARD_REGISTRY_H

#include "lvgl/lvgl.h"
#include <stdint.h>

/* =========================================
   Card IDs — stable identifiers
   card_order[] in settings uses these values
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

#define AREX_CARD_COUNT CARD_ID_COUNT

/* =========================================
   Card descriptor
   ========================================= */
typedef struct {
    arex_card_id_t  id;
    const char     *title;        /* shown in card header, English */
    lv_obj_t       *tile_obj;     /* filled after create_cb() */

    /* create_cb: build all widgets inside parent */
    void (*create_cb)(lv_obj_t *parent);

    /* update_cb: refresh widget values from g_arex — called every sim tick */
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
