#ifndef CARD_REGISTRY_H
#define CARD_REGISTRY_H

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
} card_id_t;

/* INFO 和 SETUP 仍保留 CARD_ID_*，因为右侧 tileview 统一按页面 ID 排序。
 * 它们的实现已经放到 menus/，不再属于 cards/ 业务卡片目录。
 */


/* 未使用的槽位标记（不等于任何有效卡片ID） */
#define CARD_ID_UNUSED  0xFF
/* =========================================
   Card Positions — tileview 中的显示位置
   card_order[pos] = card_id

   INFO 固定在 tile 0，SETUP 固定在最后一页。
   中间动态槽位数量由 MAX_DYNAMIC_SLOTS 决定。
   ========================================= */
#define MAX_DYNAMIC_SLOTS  12

typedef enum
{
    CARD_POS_INFO          = 0,
    CARD_POS_DYNAMIC_FIRST = 1,
    CARD_POS_SETUP         = CARD_POS_DYNAMIC_FIRST + MAX_DYNAMIC_SLOTS,
    CARD_POS_COUNT
} card_pos_t;

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
#define CARD_COUNT      CARD_POS_COUNT
#define DASH_CARD_COUNT MAX_DYNAMIC_SLOTS

/* =========================================
   Card engine type
   ========================================= */
typedef enum
{
    CARD_ENGINE_MENU   = 0,   /* render_dynamic_menu()   */
    CARD_ENGINE_GRID   = 1,   /* render_5f_custom_grid() */
    CARD_ENGINE_CHART  = 2,   /* reserved */
    CARD_ENGINE_CUSTOM = 3,   /* create_cb() full control     */
} card_engine_t;

/* =========================================
   Unified card descriptor — one entry per card.
   ROM fields (engine_type, config_data) are set at init and never changed.
   RAM field (tile_obj) is filled by right_panel_create().
   ========================================= */
typedef struct
{
    card_id_t      id;
    const char         *title;
    card_engine_t  engine_type;
    const void         *config_data;         /* menu_list_cfg_t* for MENU engine */
    lv_obj_t           *tile_obj;            /* filled at runtime */
    void (*create_cb)(lv_obj_t *parent);
    void (*update_cb)(void);
    void (*on_enter_cb)(void);
} card_t;

/* =========================================
   Registry API
   ========================================= */
uint8_t      card_count(void);
uint8_t      visible_dash_count(void);
uint8_t      setup_display_pos(void);
uint8_t      card_storage_pos(uint8_t display_pos);
uint8_t      card_id_at(uint8_t display_pos);
card_t *card_get(uint8_t order_pos);
card_t *card_get_by_id(card_id_t id);

/* Right-side page update declarations. Top-level menu pages live in menus/. */
void menu_info_update(void);
void card_compass_update(void);
void card_deco_update(void);
void card_gas_update(void);
void card_plan_update(void);
void card_blank_update(void);
void menu_setup_update(void);

#ifdef __cplusplus
}
#endif


#endif /* CARD_REGISTRY_H */
