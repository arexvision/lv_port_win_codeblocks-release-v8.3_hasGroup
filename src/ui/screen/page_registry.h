/*
 * 文件: src/app_ui/ui/screen/page_registry.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef PAGE_REGISTRY_H
#define PAGE_REGISTRY_H

#include "page_registry_types.h"
#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_vm_plan_chart ui_vm_plan_chart_t;

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
    void (*update_vm_cb)(const void *vm);
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
void card_plan_update(const ui_vm_plan_chart_t *vm);
void card_blank_update(void);
void menu_setup_update(void);
void page_registry_update_plan_vm(const ui_vm_plan_chart_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* PAGE_REGISTRY_H */
