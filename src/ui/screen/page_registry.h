#ifndef PAGE_REGISTRY_H
#define PAGE_REGISTRY_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 右侧 tileview 的页面类型 ID。
 * 数值必须保持稳定：BLE 旧协议通过 card_order[8] 下发这些数字。
 * 可以改源码命名，不能随意改这里的取值。
 */
typedef enum
{
    PAGE_ID_INFO        = 0,
    PAGE_ID_COMPASS     = 1,
    PAGE_ID_DECO        = 2,
    PAGE_ID_GAS         = 3,
    PAGE_ID_PLAN        = 4,
    PAGE_ID_CUSTOM_GRID = 5,
    PAGE_ID_BLANK       = 6,
    PAGE_ID_SETUP       = 7,
    PAGE_ID_COUNT
} page_id_t;

#define PAGE_ID_UNUSED 0xFF

/* 右侧 tileview 的存储位置。
 * INFO 固定在 0，SETUP 固定在最后一个存储槽；中间是 APP/BLE 可重排的动态页。
 */
#define MAX_DYNAMIC_SLOTS 12

typedef enum
{
    PAGE_POS_INFO          = 0,
    PAGE_POS_DYNAMIC_FIRST = 1,
    PAGE_POS_SETUP         = PAGE_POS_DYNAMIC_FIRST + MAX_DYNAMIC_SLOTS,
    PAGE_POS_COUNT
} page_pos_t;

#define PAGE_POS_1   (PAGE_POS_DYNAMIC_FIRST + 0)
#define PAGE_POS_2   (PAGE_POS_DYNAMIC_FIRST + 1)
#define PAGE_POS_3   (PAGE_POS_DYNAMIC_FIRST + 2)
#define PAGE_POS_4   (PAGE_POS_DYNAMIC_FIRST + 3)
#define PAGE_POS_5   (PAGE_POS_DYNAMIC_FIRST + 4)
#define PAGE_POS_6   (PAGE_POS_DYNAMIC_FIRST + 5)
#define PAGE_POS_7   (PAGE_POS_DYNAMIC_FIRST + 6)
#define PAGE_POS_8   (PAGE_POS_DYNAMIC_FIRST + 7)
#define PAGE_POS_9   (PAGE_POS_DYNAMIC_FIRST + 8)
#define PAGE_POS_10  (PAGE_POS_DYNAMIC_FIRST + 9)
#define PAGE_POS_11  (PAGE_POS_DYNAMIC_FIRST + 10)
#define PAGE_POS_12  (PAGE_POS_DYNAMIC_FIRST + 11)

#define PAGE_COUNT      PAGE_POS_COUNT
#define DASH_PAGE_COUNT MAX_DYNAMIC_SLOTS

typedef enum
{
    PAGE_ENGINE_MENU   = 0,   /* render_dynamic_menu() */
    PAGE_ENGINE_GRID   = 1,   /* render_5f_custom_grid() */
    PAGE_ENGINE_CHART  = 2,   /* reserved */
    PAGE_ENGINE_CUSTOM = 3,   /* create_cb() full control */
} page_engine_t;

typedef struct
{
    page_id_t     id;
    const char   *title;
    page_engine_t engine_type;
    const void   *config_data;         /* menu_list_cfg_t* for MENU engine */
    lv_obj_t     *tile_obj;            /* filled at runtime */
    void (*create_cb)(lv_obj_t *parent);
    void (*update_cb)(void);
    void (*on_enter_cb)(void);
} page_t;

uint8_t page_count(void);
uint8_t page_visible_dash_count(void);
uint8_t page_setup_display_pos(void);
uint8_t page_storage_pos(uint8_t display_pos);
uint8_t page_id_at(uint8_t display_pos);
page_t *page_get(uint8_t display_pos);
page_t *page_get_by_id(page_id_t id);

/* 兼容旧源码命名：仅做编译期别名，不改变协议数值。 */
typedef page_id_t card_id_t;
typedef page_pos_t card_pos_t;
typedef page_engine_t card_engine_t;
typedef page_t card_t;

#define CARD_ID_INFO        PAGE_ID_INFO
#define CARD_ID_COMPASS     PAGE_ID_COMPASS
#define CARD_ID_DECO        PAGE_ID_DECO
#define CARD_ID_GAS         PAGE_ID_GAS
#define CARD_ID_PLAN        PAGE_ID_PLAN
#define CARD_ID_CUSTOM_GRID PAGE_ID_CUSTOM_GRID
#define CARD_ID_BLANK       PAGE_ID_BLANK
#define CARD_ID_SETUP       PAGE_ID_SETUP
#define CARD_ID_COUNT       PAGE_ID_COUNT
#define CARD_ID_UNUSED      PAGE_ID_UNUSED

#define CARD_POS_INFO          PAGE_POS_INFO
#define CARD_POS_DYNAMIC_FIRST PAGE_POS_DYNAMIC_FIRST
#define CARD_POS_SETUP         PAGE_POS_SETUP
#define CARD_POS_COUNT         PAGE_POS_COUNT
#define CARD_POS_1             PAGE_POS_1
#define CARD_POS_2             PAGE_POS_2
#define CARD_POS_3             PAGE_POS_3
#define CARD_POS_4             PAGE_POS_4
#define CARD_POS_5             PAGE_POS_5
#define CARD_POS_6             PAGE_POS_6
#define CARD_POS_7             PAGE_POS_7
#define CARD_POS_8             PAGE_POS_8
#define CARD_POS_9             PAGE_POS_9
#define CARD_POS_10            PAGE_POS_10
#define CARD_POS_11            PAGE_POS_11
#define CARD_POS_12            PAGE_POS_12

#define CARD_COUNT      PAGE_COUNT
#define DASH_CARD_COUNT DASH_PAGE_COUNT

#define CARD_ENGINE_MENU   PAGE_ENGINE_MENU
#define CARD_ENGINE_GRID   PAGE_ENGINE_GRID
#define CARD_ENGINE_CHART  PAGE_ENGINE_CHART
#define CARD_ENGINE_CUSTOM PAGE_ENGINE_CUSTOM

#define card_count          page_count
#define visible_dash_count  page_visible_dash_count
#define setup_display_pos   page_setup_display_pos
#define card_storage_pos    page_storage_pos
#define card_id_at          page_id_at
#define card_get            page_get
#define card_get_by_id      page_get_by_id

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

#endif /* PAGE_REGISTRY_H */
