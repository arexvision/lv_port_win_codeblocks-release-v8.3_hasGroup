#ifndef AREX_CARD_REGISTRY_H
#define AREX_CARD_REGISTRY_H

#include "lvgl/lvgl.h"
#include <stdint.h>

/* =========================================
   Card IDs — stable identifiers
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
   ========================================= */
typedef enum {
    CARD_POS_INFO  = 0,
    CARD_POS_1     = 1,
    CARD_POS_2     = 2,
    CARD_POS_3     = 3,
    CARD_POS_4     = 4,
    CARD_POS_SETUP = 5,
    CARD_POS_COUNT
} arex_card_pos_t;

#define AREX_CARD_COUNT      CARD_ID_COUNT
#define AREX_DASH_CARD_COUNT (AREX_CARD_COUNT - 2)

/* =========================================
   Card engine type
   ========================================= */
typedef enum {
    CARD_ENGINE_MENU   = 0,   /* arex_render_dynamic_menu()   */
    CARD_ENGINE_GRID   = 1,   /* arex_render_5f_custom_grid() */
    CARD_ENGINE_CHART  = 2,   /* reserved */
    CARD_ENGINE_CUSTOM = 3,   /* create_cb() full control     */
} arex_card_engine_t;

/* =========================================
   Unified card descriptor — one entry per card.
   ROM fields (engine_type, config_data) are set at init and never changed.
   RAM field (tile_obj) is filled by right_panel_create().
   ========================================= */
typedef struct {
    arex_card_id_t      id;
    const char         *title;
    arex_card_engine_t  engine_type;
    const void         *config_data;         /* arex_menu_list_cfg_t* for MENU engine */
    lv_obj_t           *tile_obj;            /* filled at runtime */
    void (*create_cb)(lv_obj_t *parent);
    void (*update_cb)(void);
    void (*on_enter_cb)(void);
} arex_card_t;

/* =========================================
   Registry API
   ========================================= */
uint8_t      arex_card_count(void);
arex_card_t *arex_card_get(uint8_t order_pos);
arex_card_t *arex_card_get_by_id(arex_card_id_t id);

#endif /* AREX_CARD_REGISTRY_H */
