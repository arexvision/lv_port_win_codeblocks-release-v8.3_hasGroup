#ifndef AREX_CARD_REGISTRY_H
#define AREX_CARD_REGISTRY_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif


/* =========================================
   Card IDs — stable identifiers
   ========================================= */
typedef enum
{
    CARD_ID_INFO         = 0,
    CARD_ID_COMPASS      = 1,
    CARD_ID_DECO         = 2,
    CARD_ID_GAS          = 3,
    CARD_ID_PLAN         = 4,
    CARD_ID_CUSTOM_GRID  = 5,   /* 5F 自定义网格卡片 */
    CARD_ID_BLANK        = 6,   /* 空白卡片 */
    CARD_ID_SETUP        = 7,
    CARD_ID_COUNT
} arex_card_id_t;


/* 未使用的槽位标记（不等于任何有效卡片ID） */
#define CARD_ID_UNUSED  0xFF
/* =========================================
   Card Positions — tileview 中的显示位置
   card_order[pos] = card_id

   INFO 固定在 tile 0，SETUP 固定在最后一页。
   中间动态槽位数量由 AREX_MAX_DYNAMIC_SLOTS 决定。
   ========================================= */
#define AREX_MAX_DYNAMIC_SLOTS  12

typedef enum
{
    CARD_POS_INFO          = 0,
    CARD_POS_DYNAMIC_FIRST = 1,
    CARD_POS_SETUP         = CARD_POS_DYNAMIC_FIRST + AREX_MAX_DYNAMIC_SLOTS,
    CARD_POS_COUNT
} arex_card_pos_t;

#define CARD_POS_1   (CARD_POS_DYNAMIC_FIRST + 0)
#define CARD_POS_2   (CARD_POS_DYNAMIC_FIRST + 1)
#define CARD_POS_3   (CARD_POS_DYNAMIC_FIRST + 2)
#define CARD_POS_4   (CARD_POS_DYNAMIC_FIRST + 3)
#define CARD_POS_5   (CARD_POS_DYNAMIC_FIRST + 4)
#define CARD_POS_6   (CARD_POS_DYNAMIC_FIRST + 5)
#define CARD_POS_7   (CARD_POS_DYNAMIC_FIRST + 6)
#define CARD_POS_8   (CARD_POS_DYNAMIC_FIRST + 7)
#define CARD_POS_9   (CARD_POS_DYNAMIC_FIRST + 8)
#define CARD_POS_10  (CARD_POS_DYNAMIC_FIRST + 9)
#define CARD_POS_11  (CARD_POS_DYNAMIC_FIRST + 10)
#define CARD_POS_12  (CARD_POS_DYNAMIC_FIRST + 11)

#define AREX_CARD_ID_COUNT   CARD_ID_COUNT
#define AREX_CARD_COUNT      CARD_POS_COUNT
#define AREX_DASH_CARD_COUNT AREX_MAX_DYNAMIC_SLOTS

/* =========================================
   Card engine type
   ========================================= */
typedef enum
{
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
typedef struct
{
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
uint8_t      arex_visible_dash_count(void);
uint8_t      arex_setup_display_pos(void);
uint8_t      arex_card_storage_pos(uint8_t display_pos);
uint8_t      arex_card_id_at(uint8_t display_pos);
arex_card_t *arex_card_get(uint8_t order_pos);
arex_card_t *arex_card_get_by_id(arex_card_id_t id);

/* Card update forward declarations (defined in respective card_*.c) */
void card_info_update(void);
void card_compass_update(void);
void card_deco_update(void);
void card_gas_update(void);
void card_plan_update(void);
void card_blank_update(void);
void card_setup_update(void);

#ifdef __cplusplus
}
#endif


#endif /* AREX_CARD_REGISTRY_H */
